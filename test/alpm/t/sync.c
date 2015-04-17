#include "../alpmtest.h"

/* install and them remove a package with a single handle */

int main(void) {
    pt_env_t *pt;
    pt_pkg_t *pkg;
    pt_db_t *db;
    alpm_handle_t *h;
    alpm_pkg_t *lpkg;
    alpm_db_t *adb;
    alpm_list_t *data = NULL;

    ASSERT(pt = pt_new(NULL));
    ASSERT(db = pt_db_new(pt, "sync"));
    ASSERT(pt_mkdirat(pt->rootfd, 0755, "srv/") == 0);

    ASSERT(pkg = pt_pkg_new(pt, "foo", "1-1"));
    ASSERT(pt_db_add_pkg(db, pkg));
    ASSERT(pt_pkg_writeat(pt->rootfd, "srv/foo.pkg.tar", pkg) == 0);
    ASSERT(pt_write_db(pt->rootfd, "srv/sync.db", db) == 0);

    ASSERT(ptserve = ptserve_serve_dir(pt->rootfd, "srv/"));

    ASSERT(h = alpm_initialize(pt->root, pt->dbpath, NULL));
    ASSERT(adb = alpm_register_syncdb(h, "sync", 0));
    ASSERT(alpm_db_add_server(adb, ptserve->url) == 0);
    ASSERT(alpm_synctree(h));
    ASSERT(lpkg = pt_alpm_get_pkg(h, "sync/foo"));

    tap_plan(6);
    tap_is_int(alpm_trans_init(h, 0), 0, "alpm_trans_init");
    tap_is_int(alpm_add_pkg(h, lpkg), 0, "alpm_add_pkg");
    tap_is_int(alpm_trans_prepare(h, &data), 0, "alpm_trans_prepare");
    tap_is_int(alpm_trans_commit(h, &data), 0, "alpm_trans_commit");
    tap_is_int(alpm_trans_release(h), 0, "alpm_trans_release");
    tap_is_str(pt_test_pkg_version(pt, "foo"), "1-1", "foo installed");

    pt_cleanup(pt);
    return tap_finish();
}
