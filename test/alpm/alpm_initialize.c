#include <alpm.h>
#include "tap.c"
#include "pactest.c"

#define DBVERPATH "var/lib/pacman/local/ALPM_DB_VERSION"

#define DBVER "9"

#define IS_REALDIR(got, expected) do { \
	char c[PATH_MAX]; \
	if(realpath(expected, c)) { \
		strcat(c, "/"); \
		tap_is_str(got, c, #got " == " #expected); \
	} else { \
		tap_bail("realdir"); \
		exit(1); \
	} \
} while(0)

#define CHECK_DB_VERSION(pt) do { \
	struct stat sbuf; \
	int sret = fstatat(pt->rootfd, DBVERPATH, &sbuf, AT_SYMLINK_NOFOLLOW); \
	tap_ok(sret == 0 && pt_grepat(pt->rootfd, DBVERPATH, DBVER), "DB version"); \
} while(0)

int main(int argc, char **argv)
{
	alpm_errno_t err;
	pt_env_t *pt;
	char *c;
	
	tap_plan(29);

	pt = pt_new(NULL);
	c = pt_path(pt, "this-path-does-not-exist");
	tap_ok(alpm_initialize(c, c, &err) == NULL, "non-existent path");
	tap_is_int(err, ALPM_ERR_NOT_A_DIR, "non-existent path error");
	pt_cleanup(pt);

	pt = pt_new(NULL);
	tap_ok(pt_initialize(pt, &err) != NULL, "empty dbpath");
	tap_is_int(err, 0, "no error");
	CHECK_DB_VERSION(pt);
	IS_REALDIR(alpm_option_get_root(pt->handle), pt->root);
	IS_REALDIR(alpm_option_get_dbpath(pt->handle), pt->dbpath);
	pt_cleanup(pt);

	pt = pt_new("var2/lib/pacman/");
	pt_mkdirat(pt->rootfd, 0700, "var2");
	if(symlinkat("var2", pt->rootfd, "var") != 0) {
		tap_bail("symlinkat (%s)", strerror(errno));
		exit(1);
	}
	tap_ok(pt_initialize(pt, &err) != NULL, "symlink in dbpath");
	tap_is_int(err, 0, "no error");
	CHECK_DB_VERSION(pt);
	IS_REALDIR(alpm_option_get_root(pt->handle), pt->root);
	IS_REALDIR(alpm_option_get_dbpath(pt->handle), pt->dbpath);
	pt_cleanup(pt);

	pt = pt_new(NULL);
	pt_mkdirat(pt->rootfd, 0700, "var/lib/pacman/local/");
	tap_ok(pt_initialize(pt, &err) != NULL, "empty localdb, no version file");
	tap_is_int(err, 0, "no error");
	CHECK_DB_VERSION(pt);
	pt_cleanup(pt);

	pt = pt_new(NULL);
	unlinkat(pt->rootfd, DBVERPATH, 0);
	pt_mkdirat(pt->rootfd, 0700, "var/lib/pacman/local/pacman-4.2.0/");
	tap_ok(pt_initialize(pt, &err) == NULL, "non-empty localdb, no version file");
	tap_is_int(err, ALPM_ERR_DB_VERSION, "error");
	tap_ok(faccessat(pt->rootfd, DBVERPATH, F_OK, 0) == -1 && errno == ENOENT,
			"version file not created");
	pt_cleanup(pt);

	pt = pt_new(NULL);
	pt_writeat(pt->rootfd, DBVERPATH, "0\n");
	tap_ok(pt_initialize(pt, &err) == NULL, "outdated version file");
	tap_is_int(err, ALPM_ERR_DB_VERSION, "error");
	tap_ok(pt_grepat(pt->rootfd, DBVERPATH, "0\n"), "DB version unaltered");
	pt_cleanup(pt);

	pt = pt_new(NULL);
	pt_writeat(pt->rootfd, DBVERPATH, DBVER "\n");
	tap_ok(pt_initialize(pt, &err) != NULL, "up-to-date version file");
	tap_is_int(err, 0, "error");
	tap_ok(pt_grepat(pt->rootfd, DBVERPATH, DBVER "\n"), "DB version unaltered");
	pt_cleanup(pt);

	pt = pt_new(NULL);
	pt_writeat(pt->rootfd, DBVERPATH, "99\n");
	tap_ok(pt_initialize(pt, &err) == NULL, "newer version file");
	tap_is_int(err, ALPM_ERR_DB_VERSION, "error");
	tap_ok(pt_grepat(pt->rootfd, DBVERPATH, "99\n"), "DB version unaltered");
	pt_cleanup(pt);

	pt = pt_new(NULL);
	pt_rmrfat(pt->rootfd, "var/lib/pacman/local");
	pt_writeat(pt->rootfd, "var/lib/pacman/local", "");
	tap_ok(pt_initialize(pt, &err) == NULL, "localdb exists as file");
	tap_is_int(err, ALPM_ERR_DB_OPEN, "error");
	pt_cleanup(pt);

	return tap_finish();
}

/* vim: set noet: */
