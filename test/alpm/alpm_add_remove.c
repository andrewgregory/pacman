#include <alpm.h>
#include "pactest.c"
#include "tap.h"

/* install and them remove a package with a single handle */

#define ASSERT(x) if(!(x)) { tap_bail("ASSERT FAILED: '%s'", #x); pt_cleanup(pt); exit(1); }

int main(void) {
    pt_env_t *pt;
    pt_pkg_t *pkg;
    alpm_pkg_t *lpkg;
    alpm_list_t *data = NULL;

    ASSERT(pt = pt_new(NULL));
    ASSERT(pkg = pt_pkg_new(pt, "foo", "1-1"));
    ASSERT(pt_pkg_writeat(pt->rootfd, "foo.pkg.tar", pkg) == 0);

    ASSERT(pt_initialize(pt, NULL));
    ASSERT(alpm_pkg_load(pt->handle, pt_path(pt, "foo.pkg.tar"), 1, 0, &lpkg) == 0);

    ASSERT(alpm_trans_init(pt->handle, 0) == 0);
    ASSERT(alpm_add_pkg(pt->handle, lpkg) == 0);
    ASSERT(alpm_trans_prepare(pt->handle, &data) == 0);
    ASSERT(alpm_trans_commit(pt->handle, &data) == 0);
    ASSERT(alpm_trans_release(pt->handle) == 0);
    ASSERT(lpkg = pt_alpm_get_pkg(pt->handle, "local/foo"));

    tap_plan(6);
    tap_is_int(alpm_trans_init(pt->handle, 0), 0, "alpm_trans_init");
    tap_is_int(alpm_remove_pkg(pt->handle, lpkg), 0, "alpm_remove_pkg");
    tap_is_int(alpm_trans_prepare(pt->handle, &data), 0, "alpm_trans_prepare");
    tap_is_int(alpm_trans_commit(pt->handle, &data), 0, "alpm_trans_commit");
    tap_is_int(alpm_trans_release(pt->handle), 0, "alpm_trans_release");

    tap_is_str(pt_test_pkg_version(pt, "foo"), NULL, "foo removed");

    pt_cleanup(pt);
    return tap_finish();
}
