#include "../alpmtest.h"

#ifndef HAVE_PTHREAD

#include "../ptserve.c"

/* sync a package from a "remote" repository */

ptserve_t *ptserve = NULL;
pt_env_t *pt = NULL;
pt_pkg_t *pkg;
pt_db_t *db;
alpm_pkg_t *lpkg;
alpm_db_t *adb;
alpm_list_t *data = NULL;

void cleanup(void) {
    pt_cleanup(pt);
    ptserve_free(ptserve);
}

int pt_can_chroot(void) {
    return chroot("/") == 0;
}

int main(void) {
    ASSERTC(pt = pt_new(NULL));
    ASSERTC(pt_mkdirat(pt->rootfd, 0755, "srv") == 0);

    ASSERTC(pkg = pt_pkg_new(pt, "foo", "1-1"));
    ASSERTC((free(pkg->filename), pkg->filename = strdup("foo.pkg.tar")));
    ASSERTC(pt_pkg_writeat(pt->rootfd, "srv/foo.pkg.tar", pkg) == 0);

    ASSERTC(db = pt_db_new(pt, "sync"));
    ASSERTC(pt_db_add_pkg(db, pkg));
    ASSERTC(pt_install_db(pt, db) == 0);

    ASSERTC(ptserve = ptserve_serve_dirat(pt->rootfd, "srv/"));

    ASSERTC(pt_initialize(pt, NULL));
    ASSERTC(alpm_option_add_cachedir(pt->handle, pt_path(pt, "tmp")) == 0);
    ASSERTC(adb = alpm_register_syncdb(pt->handle, "sync", 0));
    ASSERTC(alpm_db_add_server(adb, ptserve->url) == 0);
    ASSERTC(lpkg = pt_alpm_get_pkg(pt->handle, "sync/foo"));

    tap_plan(6);
    tap_is_int(alpm_trans_init(pt->handle, 0), 0, "alpm_trans_init");
    tap_is_int(alpm_add_pkg(pt->handle, lpkg), 0, "alpm_add_pkg");
    tap_is_int(alpm_trans_prepare(pt->handle, &data), 0, "alpm_trans_prepare");
    tap_is_int(alpm_trans_commit(pt->handle, &data), 0, "alpm_trans_commit");
    tap_is_int(alpm_trans_release(pt->handle), 0, "alpm_trans_release");
    tap_is_str(pt_test_pkg_version(pt, "foo"), "1-1", "foo installed");

    cleanup();
    return tap_finish();
}

#else

int main(void) {
    tap_skip_all("pthread is required for ptserve");
    return 0;
}

#endif
