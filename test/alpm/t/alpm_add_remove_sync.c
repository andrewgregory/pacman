#include "../alpmtest.h"

/* install and them remove a package with a single handle */

int main(void) {
    pt_env_t *pt;
    pt_pkg_t *pkg;
    pt_db_t *db;
    alpm_handle_t *h;
    alpm_pkg_t *lpkg;
    alpm_list_t *data = NULL;

    ASSERT(pt = pt_new(NULL));
    ASSERT(pkg = pt_pkg_new(pt, "foo", "1-1"));
    ASSERT(pt_pkg_writeat(pt->rootfd, "foo.pkg.tar", pkg) == 0);

    pt_serve_dir(pt->rootfd, "srv/");

    ASSERT(h = alpm_initialize(pt->root, pt->dbpath, NULL));
    ASSERT(alpm_pkg_load(h, pt_path(pt, "foo.pkg.tar"), 1, 0, &lpkg) == 0);

    ASSERT(alpm_trans_init(h, 0) == 0);
    ASSERT(alpm_add_pkg(h, lpkg) == 0);
    ASSERT(alpm_trans_prepare(h, &data) == 0);
    ASSERT(alpm_trans_commit(h, &data) == 0);
    ASSERT(alpm_trans_release(h) == 0);
    ASSERT(lpkg = pt_alpm_get_pkg(h, "local/foo"));

    tap_plan(6);
    tap_is_int(alpm_trans_init(h, 0), 0, "alpm_trans_init");
    tap_is_int(alpm_remove_pkg(h, lpkg), 0, "alpm_remove_pkg");
    tap_is_int(alpm_trans_prepare(h, &data), 0, "alpm_trans_prepare");
    tap_is_int(alpm_trans_commit(h, &data), 0, "alpm_trans_commit");
    tap_is_int(alpm_trans_release(h), 0, "alpm_trans_release");

    tap_is_str(pt_test_pkg_version(pt, "foo"), NULL, "foo removed");

    pt_cleanup(pt);
    return tap_finish();
}
