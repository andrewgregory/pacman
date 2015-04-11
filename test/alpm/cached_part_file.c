#include <alpm.h>
#include "pactest.c"
#include "tap.h"

/* install and them remove a package with a single handle */

#define ASSERT(x) if(!(x)) { tap_bail("ASSERT FAILED: '%s'", #x); pt_cleanup(pt); exit(1); }

int main(void) {
    pt_env_t *pt;
    pt_pkg_t *pkg;
    pt_db_t *db;
    alpm_handle_t *h;
    alpm_pkg_t *lpkg;
    alpm_db_t *adb;
    alpm_list_t *data = NULL;

    ASSERT(pt = pt_new(NULL));

    ASSERT(pkg = pt_pkg_new(pt, "foo", "1-1"));
    ASSERT(pkg->filename = strdup("foo.pkg.tar"));
    ASSERT(pt_pkg_writeat(pt->rootfd, "tmp/foo.pkg.tar.part", pkg) == 0);

    ASSERT(db = pt_db_new(pt, "sync"));
    ASSERT(pt_db_add_pkg(db, pkg));
    ASSERT(pt_install_db(pt, db) == 0);

    ASSERT(h = alpm_initialize(pt->root, pt->dbpath, NULL));
    ASSERT(alpm_option_add_cachedir(h, pt_path(pt, "tmp")) == 0);
    ASSERT(adb = alpm_register_syncdb(h, "sync", 0));
    ASSERT(alpm_db_add_server(adb, "http://foo") == 0);
    ASSERT(lpkg = pt_alpm_get_pkg(h, "sync/foo"));

    tap_plan(6);
    tap_is_int(alpm_trans_init(h, 0), 0, "alpm_trans_init");
    tap_is_int(alpm_add_pkg(h, lpkg), 0, "alpm_add_pkg");
    tap_is_int(alpm_trans_prepare(h, &data), 0, "alpm_trans_prepare");

    tap_todo("don't fail on .part files");
    tap_is_int(alpm_trans_commit(h, &data), 0, "alpm_trans_commit");

    tap_todo(NULL);
    tap_is_int(alpm_trans_release(h), 0, "alpm_trans_release");

    tap_todo("don't fail on .part files");
    tap_is_str(pt_test_pkg_version(pt, "foo"), "1-1", "foo installed");

    pt_cleanup(pt);
    return tap_finish();
}
