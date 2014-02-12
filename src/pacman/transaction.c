/*
 *  transaction.c
 *
 *  Copyright (c) 2006-2014 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "util.h"
#include "package.h"
#include "conf.h"

#define ret_err(retval) ret = retval; goto cleanup;

static int fnmatch_cmp(const void *pattern, const void *string)
{
	return fnmatch(pattern, string, 0);
}

static int remove_target(const char *target)
{
	alpm_pkg_t *pkg;
	alpm_group_t *grp;
	alpm_db_t *db_local = alpm_get_localdb(config->handle);
	alpm_list_t *i;

	if(strncmp(target, "local/", 6) == 0) {
		target += 6;
	}

	if((pkg = alpm_db_get_pkg(db_local, target)) != NULL) {
		config->explicit_removes = alpm_list_add(config->explicit_removes, pkg);
		return 0;
	}

	/* fallback to group */
	grp = alpm_db_get_group(db_local, target);
	if(grp == NULL) {
		pm_printf(ALPM_LOG_ERROR, _("target not found: %s\n"), target);
		return -1;
	}
	for(i = grp->packages; i; i = alpm_list_next(i)) {
		config->explicit_removes = alpm_list_add(config->explicit_removes, i->data);
	}
	return 0;
}

static int select_grp_pkgs(const char *group,
		alpm_list_t *pkgs, alpm_list_t **dest)
{
	alpm_list_t *i;
	int count = alpm_list_count(pkgs);

	if(config->print == 0) {
		char *array = malloc(count);
		int n;
		if(!array) {
			return 1;
		}

		colon_printf(_n("There is %d member in group %s:\n",
					"There are %d members in group %s:\n", count),
				count, group);
		select_display(pkgs);
		if(multiselect_question(array, count)) {
			free(array);
			return 1;
		}
		for(i = pkgs, n = 0; i; i = alpm_list_next(i), ++n) {
			if(array[n]) {
				*dest = alpm_list_add(*dest, i->data);
			}
		}
		free(array);
	} else {
		for(i = pkgs; i; i = alpm_list_next(i)) {
			*dest = alpm_list_add(*dest, i->data);
		}
	}

	return 0;
}

static int get_pkg(const char *pkgname, alpm_list_t *dbs, alpm_list_t **target)
{
	alpm_pkg_t *pkg = NULL;
	alpm_list_t *i, *group_pkgs = NULL;

	const char *c;
	if((c = strchr(pkgname, '/'))) {
		const char *dbname = pkgname;
		size_t dblen = c - pkgname;
		int dbfound = 0;

		pkgname = c + 1;
		for(i = dbs; i && !dbfound; i = i->next) {
			alpm_list_t *d;
			if(strncmp(dbname, alpm_db_get_name(i->data), dblen) != 0) {
				continue;
			}
			dbfound = 1;
			d = alpm_list_add(NULL, i->data);
			if(!(pkg = alpm_find_dbs_satisfier(config->handle, d, pkgname))) {
				/* skip ignored packages when user says no */
				if(alpm_errno(config->handle) == ALPM_ERR_PKG_IGNORED) {
					pm_printf(ALPM_LOG_WARNING, _("skipping target: %s\n"), pkgname);
					alpm_list_free(d);
					return 0;
				}
				group_pkgs = alpm_find_group_pkgs(d, pkgname);
			}
			alpm_list_free(d);
		}
		if(!dbfound) {
			pm_printf(ALPM_LOG_ERROR, _("database not found: %s\n"), dbname);
			return 0;
		}
	} else {
		if(!(pkg = alpm_find_dbs_satisfier(config->handle, dbs, pkgname))) {
			/* skip ignored packages when user says no */
			if(alpm_errno(config->handle) == ALPM_ERR_PKG_IGNORED) {
				pm_printf(ALPM_LOG_WARNING, _("skipping target: %s\n"), pkgname);
				return 0;
			}
			group_pkgs = alpm_find_group_pkgs(dbs, pkgname);
		}
	}

	if(group_pkgs) {
		alpm_list_t *selected = NULL;
		int ret = select_grp_pkgs(pkgname, group_pkgs, &selected);
		if(ret == 0) {
			for(i = selected; i; i = alpm_list_next(i)) {
				if(!alpm_list_find_ptr(*target, i->data)) {
					*target = alpm_list_add(*target, i->data);
				}
			}
		}
		alpm_list_free(group_pkgs);
		alpm_list_free(selected);
		return ret;
	} else if(pkg) {
		if(!alpm_list_find_ptr(*target, pkg)) {
			*target = alpm_list_add(*target, pkg);
		}
		return 0;
	} else {
		if(access(pkgname, R_OK) == 0) {
			pm_printf(ALPM_LOG_WARNING,
					_("'%s' is a file, did you mean %s instead of %s?\n"),
					pkgname, "-U/--upgrade", "-S/--sync");
		}
		return 1;
	}
}

static int load_target(const char *pkgpath)
{
	alpm_pkg_t *pkg;
	alpm_siglevel_t siglevel;
	const char *loadpath = NULL;
	char *dlpath = NULL;
	int ret = 0;

	if(strstr(pkgpath, "://")) {
		dlpath = alpm_fetch_pkgurl(config->handle, pkgpath);
		if(!dlpath) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
					pkgpath, alpm_strerror(alpm_errno(config->handle)));
			ret = 1;
			goto cleanup;
		}
		siglevel = alpm_option_get_remote_file_siglevel(config->handle);
	} else {
		siglevel = alpm_option_get_local_file_siglevel(config->handle);
	}

	loadpath = dlpath ? dlpath : pkgpath;
	if(alpm_pkg_load(config->handle, loadpath, 1, siglevel, &pkg) != 0) {
		pm_printf(ALPM_LOG_ERROR, "'%s': %s\n",
				pkgpath, alpm_strerror(alpm_errno(config->handle)));
		ret_err(1);
	}

	config->explicit_adds = alpm_list_add(config->explicit_adds, pkg);

cleanup:
	free(dlpath);
	return ret;
}

static void print_trans_error(alpm_errno_t err, alpm_list_t *data)
{
	alpm_list_t *i;
	switch(err) {
		case ALPM_ERR_PKG_INVALID_ARCH:
			for(i = data; i; i = alpm_list_next(i)) {
				char *pkg = i->data;
				colon_printf(_("package %s does not have a valid architecture\n"), pkg);
				free(pkg);
			}
			break;
		case ALPM_ERR_UNSATISFIED_DEPS:
			for(i = data; i; i = alpm_list_next(i)) {
				alpm_depmissing_t *miss = i->data;
				char *depstring = alpm_dep_compute_string(miss->depend);
				colon_printf(_("%s: requires %s\n"), miss->target, depstring);
				free(depstring);
				alpm_depmissing_free(miss);
			}
			break;
		case ALPM_ERR_CONFLICTING_DEPS:
			for(i = data; i; i = alpm_list_next(i)) {
				alpm_conflict_t *conflict = i->data;
				/* only print reason if it contains new information */
				if(conflict->reason->mod == ALPM_DEP_MOD_ANY) {
					colon_printf(_("%s and %s are in conflict\n"),
							conflict->package1, conflict->package2);
				} else {
					char *reason = alpm_dep_compute_string(conflict->reason);
					colon_printf(_("%s and %s are in conflict (%s)\n"),
							conflict->package1, conflict->package2, reason);
					free(reason);
				}
				alpm_conflict_free(conflict);
			}
			break;
		case ALPM_ERR_FILE_CONFLICTS:
			if(config->flags & ALPM_TRANS_FLAG_FORCE) {
				printf(_("unable to %s directory-file conflicts\n"), "--force");
			}
			for(i = data; i; i = alpm_list_next(i)) {
				alpm_fileconflict_t *conflict = i->data;
				switch(conflict->type) {
					case ALPM_FILECONFLICT_TARGET:
						printf(_("%s exists in both '%s' and '%s'\n"),
								conflict->file, conflict->target, conflict->ctarget);
						break;
					case ALPM_FILECONFLICT_FILESYSTEM:
						printf(_("%s: %s exists in filesystem\n"),
								conflict->target, conflict->file);
						break;
				}
				alpm_fileconflict_free(conflict);
			}
			break;
		case ALPM_ERR_PKG_INVALID:
		case ALPM_ERR_PKG_INVALID_CHECKSUM:
		case ALPM_ERR_PKG_INVALID_SIG:
		case ALPM_ERR_DLT_INVALID:
			for(i = data; i; i = alpm_list_next(i)) {
				char *filename = i->data;
				printf(_("%s is invalid or corrupted\n"), filename);
				free(filename);
			}
			break;
		default:
			break;
	}
}

int pacman_transaction(alpm_list_t *targets, alpm_list_t *add,
		alpm_list_t *pkgfile, alpm_list_t *rem)
{
	int ret = 0;
	int confirm;
	alpm_list_t *i, *trans_packages = NULL, *data = NULL;
	alpm_list_t *syncdbs = alpm_get_syncdbs(config->handle);

	if(targets) {
		pm_printf(ALPM_LOG_ERROR, _("arguments given before action\n"));
		ret_err(1);
	}

	/* Step 1: load the specified packages */
	if(add || pkgfile) {
		int need_dbs = add ? 1 : 0;
		if(check_syncdbs(need_dbs, 1) != 0) {
			ret_err(1);
		}
	}
	for(i = add; i; i = i->next) {
		if(get_pkg(i->data, syncdbs, &config->explicit_adds) != 0) {
			ret = 1;
		}
	}
	for(i = pkgfile; i; i = i->next) {
		if(load_target(i->data) != 0) {
			ret = 1;
		}
	}
	for(i = rem; i; i = i->next) {
		if(remove_target(i->data) != 0) {
			ret = 1;
		}
	}
	if(ret) {
		ret_err(1);
	}

	/* Step 2: initialize the transaction */
	if(trans_init(config->flags, 1) != 0) {
		ret_err(1);
	}

	for(i = config->explicit_adds; i; i = i->next) {
		if(alpm_add_pkg(config->handle, i->data) != 0) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", alpm_pkg_get_name(i->data),
					alpm_strerror(alpm_errno(config->handle)));
			ret = 1;
			/* packages loaded from files need to be freed manually */
			alpm_pkg_free(i->data);
		}
	}
	for(i = config->explicit_removes; i; i = i->next) {
		if(alpm_remove_pkg(config->handle, i->data) != 0) {
			pm_printf(ALPM_LOG_ERROR, "'%s': %s\n", alpm_pkg_get_name(i->data),
					alpm_strerror(alpm_errno(config->handle)));
			ret = 1;
		}
	}
	if(ret) {
		ret_err(1);
	}
	if(config->op_s_upgrade) {
		colon_printf(_("Starting full system upgrade...\n"));
		alpm_logaction(config->handle, PACMAN_CALLER_PREFIX,
				"starting full system upgrade\n");
		if(alpm_sync_sysupgrade(config->handle, config->op_s_upgrade >= 2) == -1) {
			pm_printf(ALPM_LOG_ERROR, "%s\n", alpm_strerror(alpm_errno(config->handle)));
			ret_err(1);
		}
	}

	/* Step 3: prepare the transaction */
	if(alpm_trans_prepare(config->handle, &data) != 0) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to prepare transaction (%s)\n"),
				alpm_strerror(err));
		print_trans_error(alpm_errno(config->handle), data);
		ret_err(1);
	}

	trans_packages = alpm_list_copy(alpm_trans_get_add(config->handle));
	trans_packages = alpm_list_join(trans_packages,
			alpm_trans_get_remove(config->handle));
	if(!trans_packages) {
		/* nothing to do: just exit without complaining */
		if(!config->print) {
			printf(_(" there is nothing to do\n"));
		}
		goto cleanup;
	}

	int holdpkg = 0;
	for(i = alpm_trans_get_remove(config->handle); i; i = alpm_list_next(i)) {
		alpm_pkg_t *pkg = i->data;
		if(alpm_list_find(config->holdpkg, alpm_pkg_get_name(pkg), fnmatch_cmp)) {
			pm_printf(ALPM_LOG_WARNING, _("%s is designated as a HoldPkg.\n"),
					alpm_pkg_get_name(pkg));
			holdpkg = 1;
		}
	}
	if(holdpkg && (noyes(_("HoldPkg was found in target list. Do you want to continue?")) == 0)) {
		ret_err(1);
	}

	if(config->print) {
		print_packages(trans_packages);
		goto cleanup;
	}

	display_targets();
	putchar('\n');

	if(config->op_s_downloadonly) {
		confirm = yesno(_("Proceed with download?"));
	} else {
		confirm = yesno(_("Proceed with installation?"));
	}
	if(!confirm) {
		ret_err(1);
	}

	/* Step 4: perform the operation */
	if(alpm_trans_commit(config->handle, &data) == -1) {
		alpm_errno_t err = alpm_errno(config->handle);
		pm_printf(ALPM_LOG_ERROR, _("failed to commit transaction (%s)\n"),
				alpm_strerror(err));
		print_trans_error(err, data);
		ret_err(1);
	}

	/* Step 4: release transaction resources */
cleanup:
	alpm_list_free(data);
	if(trans_release() == -1) {
		ret = 1;
	}

	return ret;
}

#undef ret_err

/* vim: set noet: */
