#include "../alpmtest.h"
#include "../ptserve.c"

/* install a package with a .part file in a secondary cache */

pt_env_t *pt = NULL;
ptserve_t *ptserve = NULL;
off_t total_download = 0;
off_t actual_download = 0;

void cleanup(void)
{
	pt_cleanup(pt);
	ptserve_free(ptserve);
}

off_t getsize(int dirfd, const char *path)
{
	struct stat s;
	return fstatat(dirfd, path, &s, AT_SYMLINK_NOFOLLOW) == 0 ? s.st_size : -1;
}

void cb_dl_progress(const char *filename, off_t file_xfered, off_t file_total)
{
	if(file_xfered == file_total) {
		actual_download += file_xfered;
	}
}

void cb_dl_total(off_t total)
{
	total_download += total;
}

int main(void)
{
	int fd;
	const char *cpath = "secondary/foo.pkg.tar.part";
	off_t csize;
	pt_pkg_t *pkg;
	pt_db_t *db;
	alpm_pkg_t *lpkg;
	alpm_db_t *adb;
	alpm_list_t *data = NULL;

	ASSERTC(pt = pt_new(NULL));

	ASSERTC(pkg = pt_pkg_new(pt, "foo", "1-1"));
	ASSERTC((free(pkg->filename), pkg->filename = strdup("foo.pkg.tar")));

	/* write partial copy to our cache */
	ASSERTC(pt_pkg_writeat(pt->rootfd, cpath, pkg) == 0);
	ASSERTC((csize = getsize(pt->rootfd, cpath)) > 0);
	ASSERTC((fd = openat(pt->rootfd, cpath, O_WRONLY)) >= 0);
	ASSERTC(ftruncate(fd, csize / 2) == 0);
	ASSERTC(close(fd) == 0);

	/* write full copy to server */
	ASSERTC(pt_pkg_writeat(pt->rootfd, "srv/foo.pkg.tar", pkg) == 0);
	ASSERTC(ptserve = ptserve_serve_dirat(pt->rootfd, "srv/"));

	ASSERTC(db = pt_db_new(pt, "sync"));
	ASSERTC(pt_db_add_pkg(db, pkg));
	ASSERTC(pt_install_db(pt, db) == 0);

	ASSERTC(pt_initialize(pt, NULL));
	ASSERTC(alpm_option_add_cachedir(pt->handle, pt_path(pt, "primary")) == 0);
	ASSERTC(alpm_option_add_cachedir(pt->handle, pt_path(pt, "secondary")) == 0);
	ASSERTC(alpm_option_set_totaldlcb(pt->handle, cb_dl_total) == 0);
	ASSERTC(alpm_option_set_dlcb(pt->handle, cb_dl_progress) == 0);

	ASSERTC(adb = alpm_register_syncdb(pt->handle, "sync", 0));
	ASSERTC(alpm_db_add_server(adb, ptserve->url) == 0);
	ASSERTC(lpkg = pt_alpm_get_pkg(pt->handle, "sync/foo"));

	tap_plan(9);
	tap_is_int(alpm_trans_init(pt->handle, 0), 0, "alpm_trans_init");
	tap_is_int(alpm_add_pkg(pt->handle, lpkg), 0, "alpm_add_pkg");
	tap_is_int(alpm_trans_prepare(pt->handle, &data), 0, "alpm_trans_prepare");

	tap_is_int(alpm_trans_commit(pt->handle, &data), 0, "alpm_trans_commit");
	tap_is_int(alpm_trans_release(pt->handle), 0, "alpm_trans_release");
	tap_is_str(pt_test_pkg_version(pt, "foo"), "1-1", "foo installed");

	tap_is_int(total_download, csize - csize / 2, "predicted download size");
	tap_todo("use .part files in secondary caches");
	tap_is_int(actual_download, csize - csize / 2, "actual download size");
	tap_ok(pt_test_file_no_exist(pt->rootfd, cpath), ".part file removed");

	cleanup();
	return tap_finish();
}

/* vim: set noet: */
