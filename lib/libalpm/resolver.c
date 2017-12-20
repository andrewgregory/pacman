#include "alpm_list.h"
#include "package.h"
#include "deps.h"
#include "resolver.h"
#include "db.h"
#include "handle.h"

#include <limits.h>
#include <string.h>

#define debug(...) printf(__VA_ARGS__)

static size_t _alpm_hash_pkg(void *pkg) {
	return (size_t)pkg;
}

static int _alpm_cmp_pkg(void *i1, void *i2) {
	return i1 < i2 ? -1 : i1 > i2 ? 1 : 0;
}

static size_t _alpm_hash_dep(void *dep) {
	return ((alpm_depend_t *) dep)->name_hash;
}

static int _alpm_cmp_dep(void *i1, void *i2) {
	alpm_depend_t *dep = i1;
	alpm_depend_t *dep2 = i2;
	return (dep->name_hash == dep2->name_hash && dep->mod == dep2->mod
		&& ((dep->version == NULL && dep2->version == NULL)
				|| (dep->version && dep2->version && strcmp(dep->version, dep2->version) == 0))
		&& strcmp(dep->name, dep2->name) == 0) ? 0 : 1;
}

#define MHT_KEY_TYPE void*
#define MHT_DEFAULT_CMPFN _alpm_cmp_pkg
#define MHT_DEFAULT_HASHFN _alpm_hash_pkg
#include "mhashtable.c"

struct _alpm_dep_graph {
	alpm_handle_t *handle;
	alpm_list_t *pool;
	int flags;

	alpm_list_t *pkg_nodes;
	alpm_list_t *dep_nodes;
	mht_t *pkg_hash;
	mht_t *dep_hash;
};

struct _alpm_resolver_pkg {
	alpm_pkg_t *pkg;
	alpm_list_t *rdeps;
	alpm_list_t *owners;
	int disabled;
	int picked;
};

struct _alpm_resolver_dep {
	alpm_depend_t *dep;
	alpm_list_t *satisfiers;
	alpm_list_t *emark, *pmark;
};

struct _alpm_resolver_conflict {
	struct _alpm_resolver_pkg *rpkg1;
	struct _alpm_resolver_pkg *rpkg2;
};

enum _alpm_resolver_action_type {
	_ALPM_RESOLVER_ACTION_INSTALL = 1,
	_ALPM_RESOLVER_ACTION_UNINSTALL,
};

struct _alpm_resolver_action {
	enum _alpm_resolver_action_type type;
	alpm_pkg_t *target;
	alpm_list_t *causing_pkgs;
};

typedef struct _alpm_resolver_pkg rpkg_t;
typedef struct _alpm_resolver_dep rdep_t;
typedef struct _alpm_resolver_conflict rconflict_t;

static struct _alpm_resolver_pkg *_alpm_resolver_extend_graph(
		struct _alpm_dep_graph *graph, alpm_pkg_t *pkg);

static alpm_list_t *_alpm_rdep_find_satisfier(struct _alpm_dep_graph *graph, rdep_t *rdep)
{
	alpm_depend_t *d = rdep->dep;
	alpm_depmod_t mod = d->mod;
	alpm_pkg_t *satisfier = NULL;

	if(graph->flags & ALPM_RESOLVER_IGNORE_DEPENDENCY_VERSION) {
		d->mod = ALPM_DEP_MOD_ANY;
	}

	while(rdep->emark && !satisfier) {
		alpm_pkg_t *p = rdep->emark->data;
		if(p->name_hash == d->name_hash && _alpm_depcmp_literal(p, d)) {
			satisfier = p;
		}
		rdep->emark = rdep->emark->next;
	}
	while(rdep->pmark && !satisfier) {
		alpm_pkg_t *p = rdep->pmark->data;
		if(_alpm_depcmp_provides(d, alpm_pkg_get_provides(p))) {
			satisfier = p;
		}
		rdep->pmark = rdep->pmark->next;
	}

	d->mod = mod;

	if(satisfier) {
		rpkg_t *rpkg = _alpm_resolver_extend_graph(graph, satisfier);
		alpm_list_append(&(rpkg->owners), rdep);
		return alpm_list_append(&(rdep->satisfiers), rpkg);
	}

	return NULL;
}

static alpm_list_t *_alpm_rdep_first_satisfier(struct _alpm_dep_graph *graph, rdep_t *rdep)
{
	return rdep->satisfiers ? rdep->satisfiers : _alpm_rdep_find_satisfier(graph, rdep);
}

static alpm_list_t *_alpm_rdep_next_satisfier(struct _alpm_dep_graph *graph, rdep_t *rdep, alpm_list_t *i)
{
	return i->next ? i->next : _alpm_rdep_find_satisfier(graph, rdep);
}

static rpkg_t *_alpm_graph_find_pkg(struct _alpm_dep_graph *graph, alpm_pkg_t *pkg)
{
#if 0
	alpm_list_t *i;
	for(i = graph->pkg_nodes; i; i = i->next) {
		rpkg_t *rpkg = i->data;
		if(pkg == rpkg->pkg) {
			return rpkg;
		}
	}
#endif
	return mht_get_value(graph->pkg_hash, pkg);
}

static struct _alpm_resolver_pkg *_alpm_resolver_extend_graph(
		struct _alpm_dep_graph *graph, alpm_pkg_t *pkg)
{
	alpm_list_t *j;
	rpkg_t *rpkg = NULL;

	if((rpkg = _alpm_graph_find_pkg(graph, pkg))) {
		return rpkg;
	}

	rpkg = malloc(sizeof(struct _alpm_resolver_pkg));
	alpm_list_append(&(graph->pkg_nodes), rpkg);
	mht_set_value(graph->pkg_hash, pkg, rpkg);

	rpkg->pkg = pkg;
	rpkg->owners = NULL;
	rpkg->rdeps = NULL;
	rpkg->disabled = 0;
	rpkg->picked = 0;

	for(j = alpm_pkg_get_depends(pkg); j; j = j->next) {
		alpm_depend_t *dep = j->data;
		struct _alpm_resolver_dep *rdep = NULL;
		if(_alpm_depcmp_provides(j->data, graph->handle->assumeinstalled)) {
			continue;
		}
		rdep = mht_get_value(graph->dep_hash, dep);
		if(rdep == NULL) {
			rdep = malloc(sizeof(struct _alpm_resolver_dep));
			rdep->satisfiers = NULL;
			rdep->dep = dep;
			rdep->emark = graph->pool;
			rdep->pmark = graph->pool;

			if(!_alpm_rdep_first_satisfier(graph, rdep)) {
				return NULL;
			}

			alpm_list_append(&(graph->dep_nodes), rdep);
			mht_set_value(graph->dep_hash, dep, rdep);
		}

		alpm_list_append(&(rpkg->rdeps), rdep);
	}
	return rpkg;
}

static void _alpm_resolver_reduce(rpkg_t *rpkg, alpm_list_t **solution)
{
	alpm_list_t *i, *j;
	if(rpkg->disabled || rpkg->picked) {
		return;
	}
	rpkg->picked = 1;
	for(i = rpkg->rdeps; i; i = i->next) {
		rdep_t *rdep = i->data;
		for(j = rdep->satisfiers; j; j = j->next) {
			rpkg_t *rsatisfier = j->data;
			if(!rsatisfier->disabled) {
				_alpm_resolver_reduce(rsatisfier, solution);
				break;
			}
		}
	}
	if(rpkg->pkg->origin != ALPM_PKG_FROM_LOCALDB) {
		alpm_list_append(solution, rpkg->pkg);
	}
}

static int _alpm_resolver_solve_conflicts(struct _alpm_dep_graph *graph, alpm_list_t *conflicts, alpm_list_t *roots)
{
	/* grab the first conflict */
	rconflict_t *conflict;
	int pkg1_can_be_disabled = 1;
	int pkg2_can_be_disabled = 1;
	alpm_list_t *i;

	if(conflicts == NULL) {
		return 0;
	}

	conflict = conflicts->data;

	/* check if conflict has already been resolved */
	if(conflict->rpkg1->disabled || conflict->rpkg2->disabled) {
		return _alpm_resolver_solve_conflicts(graph, conflicts->next, roots);
	}

	/* rpkg1 is the preferred package, try disabling rpkg2 first */
	/* check if rpkg2 can be disabled */
	for(i = roots; i && pkg2_can_be_disabled; i = i->next) {
		if(conflict->rpkg2 == i->data && conflict->rpkg2->pkg->origin != ALPM_PKG_FROM_LOCALDB) {
			pkg2_can_be_disabled = 0;
		}
	}
	for(i = conflict->rpkg2->owners; i && pkg2_can_be_disabled; i = i->next) {
			rdep_t *rdep = i->data;
			int dep_has_alt_satisfier = 0;
			alpm_list_t *k;
			for(k = _alpm_rdep_first_satisfier(graph, rdep); k && !dep_has_alt_satisfier; k = _alpm_rdep_next_satisfier(graph, rdep, k)) {
				rpkg_t *satisfier = k->data;
				if(satisfier != conflict->rpkg2 && !satisfier->disabled) {
					dep_has_alt_satisfier = 1;
				}
			}
			if(!dep_has_alt_satisfier) {
				pkg2_can_be_disabled = 0;
			}
	}
	if(pkg2_can_be_disabled) {
		conflict->rpkg2->disabled = 1;
		if(_alpm_resolver_solve_conflicts(graph, conflicts->next, roots) == 0) {
			return 0;
		}
		conflict->rpkg2->disabled = 0;
	}

	/* check if rpkg1 can be disabled */
	for(i = roots; i && pkg1_can_be_disabled; i = i->next) {
		if(conflict->rpkg1 == i->data && conflict->rpkg1->pkg->origin != ALPM_PKG_FROM_LOCALDB) {
			pkg1_can_be_disabled = 0;
		}
	}
	for(i = conflict->rpkg1->owners; i && pkg1_can_be_disabled; i = i->next) {
			rdep_t *rdep = i->data;
			int dep_has_alt_satisfier = 0;
			alpm_list_t *k;
			for(k = _alpm_rdep_first_satisfier(graph, rdep); k && !dep_has_alt_satisfier; k = _alpm_rdep_next_satisfier(graph, rdep, k)) {
				rpkg_t *satisfier = k->data;
				if(satisfier != conflict->rpkg1 && !satisfier->disabled) {
					dep_has_alt_satisfier = 1;
				}
			}
			if(!dep_has_alt_satisfier) {
				pkg1_can_be_disabled = 0;
			}
	}
	if(pkg1_can_be_disabled) {
		conflict->rpkg1->disabled = 1;
		if(_alpm_resolver_solve_conflicts(graph, conflicts->next, roots) == 0) {
			return 0;
		}
		conflict->rpkg1->disabled = 0;
	}

	return -1;
}

static int _alpm_resolver_pkgs_conflict(alpm_pkg_t *pkg1, alpm_pkg_t *pkg2) {
	alpm_list_t *i;
	/* all packages conflict with alternate versions of themselves */
	if(strcmp(pkg1->name, pkg2->name) == 0) {
		return 1;
	}
	/* check conflicts in one direction */
	for(i = alpm_pkg_get_conflicts(pkg1); i; i = i->next) {
		alpm_depend_t *conflict = i->data;
		if(_alpm_depcmp(pkg2, conflict)) {
			return 1;
		}
	}
	/* check conflicts in the other direction */
	for(i = alpm_pkg_get_conflicts(pkg2); i; i = i->next) {
		alpm_depend_t *conflict = i->data;
		if(_alpm_depcmp(pkg1, conflict)) {
			return 1;
		}
	}
	return 0;
}

static alpm_list_t *_alpm_resolver_find_conflicts(struct _alpm_dep_graph *graph)
{
	alpm_list_t *i, *j, *conflicts = NULL;
	for(i = graph->pkg_nodes; i; i = i->next) {
		rpkg_t *rpkg1 = i->data;
		for(j = i->next; j; j = j->next) {
			rpkg_t *rpkg2 = j->data;
			if(_alpm_resolver_pkgs_conflict(rpkg1->pkg, rpkg2->pkg)) {
				rconflict_t *rconflict = malloc(sizeof(rconflict_t));
				rconflict->rpkg1 = rpkg1;
				rconflict->rpkg2 = rpkg2;
				alpm_list_append(&conflicts, rconflict);
			}
		}
	}
	return conflicts;
}

static alpm_list_t *_alpm_resolver_solve(struct _alpm_dep_graph *graph, alpm_list_t *roots)
{
	alpm_list_t *i;
	alpm_list_t *conflicts = _alpm_resolver_find_conflicts(graph);
	alpm_list_t *solution = NULL;
	if(_alpm_resolver_solve_conflicts(graph, conflicts, roots) != 0) {
		FREELIST(conflicts);
		return NULL;
	}
	for(i = roots; i; i = i->next) {
		_alpm_resolver_reduce(i->data, &solution);
	}
	FREELIST(conflicts);
	return solution;
}

alpm_list_t *_alpm_resolvedeps_thorough(alpm_handle_t *handle, alpm_list_t *add, alpm_list_t *remove, int flags)
{
	struct _alpm_dep_graph graph;
	alpm_list_t *i, *roots = NULL;
	alpm_list_t *solution = NULL;

	graph.pkg_nodes = NULL;
	graph.dep_nodes = NULL;
	graph.handle = handle;
	graph.flags = flags;
	graph.pool = NULL;

	graph.pkg_hash = mht_new(0);
	graph.pkg_hash->hashfn = _alpm_hash_pkg;
	graph.pkg_hash->cmpfn = _alpm_cmp_pkg;

	graph.dep_hash = mht_new(0);
	graph.dep_hash->hashfn = _alpm_hash_dep;
	graph.dep_hash->cmpfn = _alpm_cmp_dep;

	for(i = add; i; i = i->next) {
		alpm_pkg_t *pkg = i->data;
		alpm_list_append(&graph.pool, pkg);
	}
	for(i = _alpm_db_get_pkgcache(handle->db_local); i; i = i->next) {
		alpm_pkg_t *pkg = i->data;
		if(!alpm_pkg_find(add, pkg->name) && !alpm_pkg_find(remove, pkg->name)) {
			alpm_list_append(&graph.pool, pkg);
		}
	}
	for(i = handle->dbs_sync; i; i = i->next) {
		alpm_list_t *j;
		for(j = _alpm_db_get_pkgcache(i->data); j; j = j->next) {
			alpm_pkg_t *pkg = j->data;
			if(!alpm_pkg_find(add, pkg->name) && !alpm_pkg_find(remove, pkg->name) && !alpm_pkg_should_ignore(handle, pkg)) {
				alpm_list_append(&graph.pool, pkg);
			}
		}
	}

	for(i = add; i; i = i->next) {
		/* seed the graph with packages we know we need */
		rpkg_t *rpkg = _alpm_resolver_extend_graph(&graph, i->data);
		if(rpkg == NULL) {
			goto cleanup;
		}
		alpm_list_append(&roots, rpkg);
	}
	for(i = _alpm_db_get_pkgcache(handle->db_local); i; i = i->next) {
		/* seed the pool with currently installed packages to make sure we don't
		 * break their dependencies */
		alpm_pkg_t *pkg = i->data;
		if(!alpm_pkg_find(add, pkg->name) && !alpm_pkg_find(remove, pkg->name)) {
			rpkg_t *rpkg = _alpm_resolver_extend_graph(&graph, pkg);
			if(rpkg == NULL) {
				goto cleanup;
			}
			alpm_list_append(&roots, rpkg);
		}
	}
	solution = _alpm_resolver_solve(&graph, roots);

cleanup:
	FREELIST(graph.pkg_nodes);
	FREELIST(graph.dep_nodes);
	alpm_list_free(roots);

	return solution;
}

/*
 input: list of packages to add/remove
 graph - all packages to install and all local packages
 pool - all packages available to satisfy dependencies,
	 all packages being installed, sync packages, and local packages not being removed
 roots - packages whose dependencies must be satisfied,
   packages being installed and local packages
*/
