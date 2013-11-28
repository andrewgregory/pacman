/*
 * alpm.h
 *
 *  Copyright (c) 2006-2013 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALPM_H
#define _ALPM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>    /* int64_t */
#include <sys/types.h> /* off_t */
#include <stdarg.h>    /* va_list */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

#include <alpm_list.h>

/*
 * Arch Linux Package Management library
 */

/** @addtogroup libalpm Public API
 * The libalpm Public API
 * @{
 */

typedef int64_t alpm_time_t;

/*
 * Enumerations
 * These ones are used in multiple contexts, so are forward-declared.
 */

/** Package install reasons. */
typedef enum _alpm_pkgreason_t {
	/** Explicitly requested by the user. */
	ALPM_PKG_REASON_EXPLICIT = 0,
	/** Installed as a dependency for another package. */
	ALPM_PKG_REASON_DEPEND = 1
} alpm_pkgreason_t;

/** Location a package object was loaded from. */
typedef enum _alpm_pkgfrom_t {
	ALPM_PKG_FROM_FILE = 1,
	ALPM_PKG_FROM_LOCALDB,
	ALPM_PKG_FROM_SYNCDB
} alpm_pkgfrom_t;

/** Method used to validate a package. */
typedef enum _alpm_pkgvalidation_t {
	ALPM_PKG_VALIDATION_UNKNOWN = 0,
	ALPM_PKG_VALIDATION_NONE = (1 << 0),
	ALPM_PKG_VALIDATION_MD5SUM = (1 << 1),
	ALPM_PKG_VALIDATION_SHA256SUM = (1 << 2),
	ALPM_PKG_VALIDATION_SIGNATURE = (1 << 3)
} alpm_pkgvalidation_t;

/** Types of version constraints in dependency specs. */
typedef enum _alpm_depmod_t {
	/** No version constraint */
	ALPM_DEP_MOD_ANY = 1,
	/** Test version equality (package=x.y.z) */
	ALPM_DEP_MOD_EQ,
	/** Test for at least a version (package>=x.y.z) */
	ALPM_DEP_MOD_GE,
	/** Test for at most a version (package<=x.y.z) */
	ALPM_DEP_MOD_LE,
	/** Test for greater than some version (package>x.y.z) */
	ALPM_DEP_MOD_GT,
	/** Test for less than some version (package<x.y.z) */
	ALPM_DEP_MOD_LT
} alpm_depmod_t;

/**
 * File conflict type.
 * Whether the conflict results from a file existing on the filesystem, or with
 * another target in the transaction.
 */
typedef enum _alpm_fileconflicttype_t {
	ALPM_FILECONFLICT_TARGET = 1,
	ALPM_FILECONFLICT_FILESYSTEM
} alpm_fileconflicttype_t;

/** PGP signature verification options */
typedef enum _alpm_siglevel_t {
	ALPM_SIG_PACKAGE = (1 << 0),
	ALPM_SIG_PACKAGE_OPTIONAL = (1 << 1),
	ALPM_SIG_PACKAGE_MARGINAL_OK = (1 << 2),
	ALPM_SIG_PACKAGE_UNKNOWN_OK = (1 << 3),

	ALPM_SIG_DATABASE = (1 << 10),
	ALPM_SIG_DATABASE_OPTIONAL = (1 << 11),
	ALPM_SIG_DATABASE_MARGINAL_OK = (1 << 12),
	ALPM_SIG_DATABASE_UNKNOWN_OK = (1 << 13),

	ALPM_SIG_PACKAGE_SET = (1 << 27),
	ALPM_SIG_PACKAGE_TRUST_SET = (1 << 28),

	ALPM_SIG_USE_DEFAULT = (1 << 31)
} alpm_siglevel_t;

/** PGP signature verification status return codes */
typedef enum _alpm_sigstatus_t {
	ALPM_SIGSTATUS_VALID,
	ALPM_SIGSTATUS_KEY_EXPIRED,
	ALPM_SIGSTATUS_SIG_EXPIRED,
	ALPM_SIGSTATUS_KEY_UNKNOWN,
	ALPM_SIGSTATUS_KEY_DISABLED,
	ALPM_SIGSTATUS_INVALID
} alpm_sigstatus_t;

/** PGP signature verification status return codes */
typedef enum _alpm_sigvalidity_t {
	ALPM_SIGVALIDITY_FULL,
	ALPM_SIGVALIDITY_MARGINAL,
	ALPM_SIGVALIDITY_NEVER,
	ALPM_SIGVALIDITY_UNKNOWN
} alpm_sigvalidity_t;

/*
 * Structures
 */

typedef struct __alpm_handle_t alpm_handle_t;
typedef struct __alpm_db_t alpm_db_t;
typedef struct __alpm_pkg_t alpm_pkg_t;
typedef struct __alpm_trans_t alpm_trans_t;

/** Dependency */
typedef struct _alpm_depend_t {
	char *name;
	char *version;
	char *desc;
	unsigned long name_hash;
	alpm_depmod_t mod;
} alpm_depend_t;

/** Missing dependency */
typedef struct _alpm_depmissing_t {
	char *target;
	alpm_depend_t *depend;
	/* this is used only in the case of a remove dependency error */
	char *causingpkg;
} alpm_depmissing_t;

/** Conflict */
typedef struct _alpm_conflict_t {
	unsigned long package1_hash;
	unsigned long package2_hash;
	char *package1;
	char *package2;
	alpm_depend_t *reason;
} alpm_conflict_t;

/** File conflict */
typedef struct _alpm_fileconflict_t {
	char *target;
	alpm_fileconflicttype_t type;
	char *file;
	char *ctarget;
} alpm_fileconflict_t;

/** Package group */
typedef struct _alpm_group_t {
	/** group name */
	char *name;
	/** list of alpm_pkg_t packages */
	alpm_list_t *packages;
} alpm_group_t;

/** Package upgrade delta */
typedef struct _alpm_delta_t {
	/** filename of the delta patch */
	char *delta;
	/** md5sum of the delta file */
	char *delta_md5;
	/** filename of the 'before' file */
	char *from;
	/** filename of the 'after' file */
	char *to;
	/** filesize of the delta file */
	off_t delta_size;
	/** download filesize of the delta file */
	off_t download_size;
} alpm_delta_t;

/** File in a package */
typedef struct _alpm_file_t {
	char *name;
	off_t size;
	mode_t mode;
} alpm_file_t;

/** Package filelist container */
typedef struct _alpm_filelist_t {
	size_t count;
	alpm_file_t *files;
} alpm_filelist_t;

/** Local package or package file backup entry */
typedef struct _alpm_backup_t {
	char *name;
	char *hash;
} alpm_backup_t;

typedef struct _alpm_pgpkey_t {
	void *data;
	char *fingerprint;
	char *uid;
	char *name;
	char *email;
	alpm_time_t created;
	alpm_time_t expires;
	unsigned int length;
	unsigned int revoked;
	char pubkey_algo;
} alpm_pgpkey_t;

/**
 * Signature result. Contains the key, status, and validity of a given
 * signature.
 */
typedef struct _alpm_sigresult_t {
	alpm_pgpkey_t key;
	alpm_sigstatus_t status;
	alpm_sigvalidity_t validity;
} alpm_sigresult_t;

/**
 * Signature list. Contains the number of signatures found and a pointer to an
 * array of results. The array is of size count.
 */
typedef struct _alpm_siglist_t {
	size_t count;
	alpm_sigresult_t *results;
} alpm_siglist_t;

/*
 * Logging facilities
 */

/** Logging Levels */
typedef enum _alpm_loglevel_t {
	ALPM_LOG_ERROR    = 1,
	ALPM_LOG_WARNING  = (1 << 1),
	ALPM_LOG_DEBUG    = (1 << 2),
	ALPM_LOG_FUNCTION = (1 << 3)
} alpm_loglevel_t;

typedef void (*alpm_cb_log)(alpm_loglevel_t, const char *, va_list);

int alpm_logaction(alpm_handle_t *handle, const char *prefix,
		const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/**
 * Events.
 * NULL parameters are passed to in all events unless specified otherwise.
 */
typedef enum _alpm_event_t {
	/** Dependencies will be computed for a package. */
	ALPM_EVENT_CHECKDEPS_START = 1,
	/** Dependencies were computed for a package. */
	ALPM_EVENT_CHECKDEPS_DONE,
	/** File conflicts will be computed for a package. */
	ALPM_EVENT_FILECONFLICTS_START,
	/** File conflicts were computed for a package. */
	ALPM_EVENT_FILECONFLICTS_DONE,
	/** Dependencies will be resolved for target package. */
	ALPM_EVENT_RESOLVEDEPS_START,
	/** Dependencies were resolved for target package. */
	ALPM_EVENT_RESOLVEDEPS_DONE,
	/** Inter-conflicts will be checked for target package. */
	ALPM_EVENT_INTERCONFLICTS_START,
	/** Inter-conflicts were checked for target package. */
	ALPM_EVENT_INTERCONFLICTS_DONE,
	/** Package will be installed.
	 * A pointer to the target package is passed to the callback.
	 */
	ALPM_EVENT_ADD_START,
	/** Package was installed.
	 * A pointer to the new package is passed to the callback.
	 */
	ALPM_EVENT_ADD_DONE,
	/** Package will be removed.
	 * A pointer to the target package is passed to the callback.
	 */
	ALPM_EVENT_REMOVE_START,
	/** Package was removed.
	 * A pointer to the removed package is passed to the callback.
	 */
	ALPM_EVENT_REMOVE_DONE,
	/** Package will be upgraded.
	 * A pointer to the upgraded package is passed to the callback.
	 */
	ALPM_EVENT_UPGRADE_START,
	/** Package was upgraded.
	 * A pointer to the new package, and a pointer to the old package is passed
	 * to the callback, respectively.
	 */
	ALPM_EVENT_UPGRADE_DONE,
	/** Package will be downgraded.
	 * A pointer to the downgraded package is passed to the callback.
	 */
	ALPM_EVENT_DOWNGRADE_START,
	/** Package was downgraded.
	 * A pointer to the new package, and a pointer to the old package is passed
	 * to the callback, respectively.
	 */
	ALPM_EVENT_DOWNGRADE_DONE,
	/** Package will be reinstalled.
	 * A pointer to the reinstalled package is passed to the callback.
	 */
	ALPM_EVENT_REINSTALL_START,
	/** Package was reinstalled.
	 * A pointer to the new package, and a pointer to the old package is passed
	 * to the callback, respectively.
	 */
	ALPM_EVENT_REINSTALL_DONE,
	/** Target package's integrity will be checked. */
	ALPM_EVENT_INTEGRITY_START,
	/** Target package's integrity was checked. */
	ALPM_EVENT_INTEGRITY_DONE,
	/** Target package will be loaded. */
	ALPM_EVENT_LOAD_START,
	/** Target package is finished loading. */
	ALPM_EVENT_LOAD_DONE,
	/** Target delta's integrity will be checked. */
	ALPM_EVENT_DELTA_INTEGRITY_START,
	/** Target delta's integrity was checked. */
	ALPM_EVENT_DELTA_INTEGRITY_DONE,
	/** Deltas will be applied to packages. */
	ALPM_EVENT_DELTA_PATCHES_START,
	/** Deltas were applied to packages. */
	ALPM_EVENT_DELTA_PATCHES_DONE,
	/** Delta patch will be applied to target package.
	 * The filename of the package and the filename of the patch is passed to the
	 * callback.
	 */
	ALPM_EVENT_DELTA_PATCH_START,
	/** Delta patch was applied to target package. */
	ALPM_EVENT_DELTA_PATCH_DONE,
	/** Delta patch failed to apply to target package. */
	ALPM_EVENT_DELTA_PATCH_FAILED,
	/** Scriptlet has printed information.
	 * A line of text is passed to the callback.
	 */
	ALPM_EVENT_SCRIPTLET_INFO,
	/** Files will be downloaded from a repository.
	 * The repository's tree name is passed to the callback.
	 */
	ALPM_EVENT_RETRIEVE_START,
	/** Disk space usage will be computed for a package */
	ALPM_EVENT_DISKSPACE_START,
	/** Disk space usage was computed for a package */
	ALPM_EVENT_DISKSPACE_DONE,
	/** An optdepend for another package is being removed
	 * The requiring package and its dependency are passed to the callback */
	ALPM_EVENT_OPTDEP_REQUIRED,
	/** A configured repository database is missing */
	ALPM_EVENT_DATABASE_MISSING,
	/** Checking keys used to create signatures are in keyring. */
	ALPM_EVENT_KEYRING_START,
	/** Keyring checking is finished. */
	ALPM_EVENT_KEYRING_DONE,
	/** Downloading missing keys into keyring. */
	ALPM_EVENT_KEY_DOWNLOAD_START,
	/** Key downloading is finished. */
	ALPM_EVENT_KEY_DOWNLOAD_DONE
} alpm_event_t;

/** Event callback */
typedef void (*alpm_cb_event)(alpm_event_t, void *, void *);

/**
 * Questions.
 * Unlike the events or progress enumerations, this enum has bitmask values
 * so a frontend can use a bitmask map to supply preselected answers to the
 * different types of questions.
 */
typedef enum _alpm_question_t {
	ALPM_QUESTION_INSTALL_IGNOREPKG = 1,
	ALPM_QUESTION_REPLACE_PKG = (1 << 1),
	ALPM_QUESTION_CONFLICT_PKG = (1 << 2),
	ALPM_QUESTION_CORRUPTED_PKG = (1 << 3),
	ALPM_QUESTION_REMOVE_PKGS = (1 << 4),
	ALPM_QUESTION_SELECT_PROVIDER = (1 << 5),
	ALPM_QUESTION_IMPORT_KEY = (1 << 6)
} alpm_question_t;

/** Question callback */
typedef void (*alpm_cb_question)(alpm_question_t, void *, void *, void *, int *);

/** Progress */
typedef enum _alpm_progress_t {
	ALPM_PROGRESS_ADD_START,
	ALPM_PROGRESS_UPGRADE_START,
	ALPM_PROGRESS_DOWNGRADE_START,
	ALPM_PROGRESS_REINSTALL_START,
	ALPM_PROGRESS_REMOVE_START,
	ALPM_PROGRESS_CONFLICTS_START,
	ALPM_PROGRESS_DISKSPACE_START,
	ALPM_PROGRESS_INTEGRITY_START,
	ALPM_PROGRESS_LOAD_START,
	ALPM_PROGRESS_KEYRING_START
} alpm_progress_t;

/** Progress callback */
typedef void (*alpm_cb_progress)(alpm_progress_t, const char *, int, size_t, size_t);

/*
 * Downloading
 */

/** Type of download progress callbacks.
 * @param filename the name of the file being downloaded
 * @param xfered the number of transferred bytes
 * @param total the total number of bytes to transfer
 */
typedef void (*alpm_cb_download)(const char *filename,
		off_t xfered, off_t total);

typedef void (*alpm_cb_totaldl)(off_t total);

/** A callback for downloading files
 * @param url the URL of the file to be downloaded
 * @param localpath the directory to which the file should be downloaded
 * @param force whether to force an update, even if the file is the same
 * @return 0 on success, 1 if the file exists and is identical, -1 on
 * error.
 */
typedef int (*alpm_cb_fetch)(const char *url, const char *localpath,
		int force);

char *alpm_fetch_pkgurl(alpm_handle_t *handle, const char *url);

/** @addtogroup libalpm_options Options
 * Libalpm option getters and setters
 * @{
 */

alpm_cb_log alpm_option_get_logcb(alpm_handle_t *handle);
int alpm_option_set_logcb(alpm_handle_t *handle, alpm_cb_log cb);

alpm_cb_download alpm_option_get_dlcb(alpm_handle_t *handle);
int alpm_option_set_dlcb(alpm_handle_t *handle, alpm_cb_download cb);

alpm_cb_fetch alpm_option_get_fetchcb(alpm_handle_t *handle);
int alpm_option_set_fetchcb(alpm_handle_t *handle, alpm_cb_fetch cb);

alpm_cb_totaldl alpm_option_get_totaldlcb(alpm_handle_t *handle);
int alpm_option_set_totaldlcb(alpm_handle_t *handle, alpm_cb_totaldl cb);

alpm_cb_event alpm_option_get_eventcb(alpm_handle_t *handle);
int alpm_option_set_eventcb(alpm_handle_t *handle, alpm_cb_event cb);

alpm_cb_question alpm_option_get_questioncb(alpm_handle_t *handle);
int alpm_option_set_questioncb(alpm_handle_t *handle, alpm_cb_question cb);

alpm_cb_progress alpm_option_get_progresscb(alpm_handle_t *handle);
int alpm_option_set_progresscb(alpm_handle_t *handle, alpm_cb_progress cb);

const char *alpm_option_get_root(alpm_handle_t *handle);

const char *alpm_option_get_dbpath(alpm_handle_t *handle);

const char *alpm_option_get_lockfile(alpm_handle_t *handle);

/** @name Accessors to the list of package cache directories.
 * @{
 */
alpm_list_t *alpm_option_get_cachedirs(alpm_handle_t *handle);
int alpm_option_set_cachedirs(alpm_handle_t *handle, alpm_list_t *cachedirs);
int alpm_option_add_cachedir(alpm_handle_t *handle, const char *cachedir);
int alpm_option_remove_cachedir(alpm_handle_t *handle, const char *cachedir);
/** @} */

const char *alpm_option_get_logfile(alpm_handle_t *handle);
int alpm_option_set_logfile(alpm_handle_t *handle, const char *logfile);

const char *alpm_option_get_gpgdir(alpm_handle_t *handle);
int alpm_option_set_gpgdir(alpm_handle_t *handle, const char *gpgdir);

int alpm_option_get_usesyslog(alpm_handle_t *handle);
int alpm_option_set_usesyslog(alpm_handle_t *handle, int usesyslog);

/** @name Accessors to the list of no-upgrade files.
 * These functions modify the list of files which should
 * not be updated by package installation.
 * @{
 */
alpm_list_t *alpm_option_get_noupgrades(alpm_handle_t *handle);
int alpm_option_add_noupgrade(alpm_handle_t *handle, const char *pkg);
int alpm_option_set_noupgrades(alpm_handle_t *handle, alpm_list_t *noupgrade);
int alpm_option_remove_noupgrade(alpm_handle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of no-extract files.
 * These functions modify the list of filenames which should
 * be skipped packages which should
 * not be upgraded by a sysupgrade operation.
 * @{
 */
alpm_list_t *alpm_option_get_noextracts(alpm_handle_t *handle);
int alpm_option_add_noextract(alpm_handle_t *handle, const char *pkg);
int alpm_option_set_noextracts(alpm_handle_t *handle, alpm_list_t *noextract);
int alpm_option_remove_noextract(alpm_handle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of ignored packages.
 * These functions modify the list of packages that
 * should be ignored by a sysupgrade.
 * @{
 */
alpm_list_t *alpm_option_get_ignorepkgs(alpm_handle_t *handle);
int alpm_option_add_ignorepkg(alpm_handle_t *handle, const char *pkg);
int alpm_option_set_ignorepkgs(alpm_handle_t *handle, alpm_list_t *ignorepkgs);
int alpm_option_remove_ignorepkg(alpm_handle_t *handle, const char *pkg);
/** @} */

/** @name Accessors to the list of ignored groups.
 * These functions modify the list of groups whose packages
 * should be ignored by a sysupgrade.
 * @{
 */
alpm_list_t *alpm_option_get_ignoregroups(alpm_handle_t *handle);
int alpm_option_add_ignoregroup(alpm_handle_t *handle, const char *grp);
int alpm_option_set_ignoregroups(alpm_handle_t *handle, alpm_list_t *ignoregrps);
int alpm_option_remove_ignoregroup(alpm_handle_t *handle, const char *grp);
/** @} */

const char *alpm_option_get_arch(alpm_handle_t *handle);
int alpm_option_set_arch(alpm_handle_t *handle, const char *arch);

double alpm_option_get_deltaratio(alpm_handle_t *handle);
int alpm_option_set_deltaratio(alpm_handle_t *handle, double ratio);

int alpm_option_get_checkspace(alpm_handle_t *handle);
int alpm_option_set_checkspace(alpm_handle_t *handle, int checkspace);

alpm_siglevel_t alpm_option_get_default_siglevel(alpm_handle_t *handle);
int alpm_option_set_default_siglevel(alpm_handle_t *handle, alpm_siglevel_t level);

alpm_siglevel_t alpm_option_get_local_file_siglevel(alpm_handle_t *handle);
int alpm_option_set_local_file_siglevel(alpm_handle_t *handle, alpm_siglevel_t level);

alpm_siglevel_t alpm_option_get_remote_file_siglevel(alpm_handle_t *handle);
int alpm_option_set_remote_file_siglevel(alpm_handle_t *handle, alpm_siglevel_t level);

/** @} */

/** @addtogroup libalpm_databases Database Functions
 * Functions to query and manipulate the database of libalpm.
 * @{
 */

alpm_db_t *alpm_get_localdb(alpm_handle_t *handle);
alpm_list_t *alpm_get_syncdbs(alpm_handle_t *handle);

alpm_db_t *alpm_register_syncdb(alpm_handle_t *handle, const char *treename,
		alpm_siglevel_t level);
int alpm_unregister_all_syncdbs(alpm_handle_t *handle);
int alpm_db_unregister(alpm_db_t *db);

const char *alpm_db_get_name(const alpm_db_t *db);

alpm_siglevel_t alpm_db_get_siglevel(alpm_db_t *db);

int alpm_db_get_valid(alpm_db_t *db);

/** @name Accessors to the list of servers for a database.
 * @{
 */
alpm_list_t *alpm_db_get_servers(const alpm_db_t *db);
int alpm_db_set_servers(alpm_db_t *db, alpm_list_t *servers);
int alpm_db_add_server(alpm_db_t *db, const char *url);
int alpm_db_remove_server(alpm_db_t *db, const char *url);
/** @} */

int alpm_db_update(int force, alpm_db_t *db);
alpm_pkg_t *alpm_db_get_pkg(alpm_db_t *db, const char *name);
alpm_list_t *alpm_db_get_pkgcache(alpm_db_t *db);
alpm_group_t *alpm_db_get_group(alpm_db_t *db, const char *name);
alpm_list_t *alpm_db_get_groupcache(alpm_db_t *db);
alpm_list_t *alpm_db_search(alpm_db_t *db, const alpm_list_t *needles);

typedef enum _alpm_db_usage_ {
	ALPM_DB_USAGE_SYNC = 1,
	ALPM_DB_USAGE_SEARCH = (1 << 1),
	ALPM_DB_USAGE_INSTALL = (1 << 2),
	ALPM_DB_USAGE_UPGRADE = (1 << 3),
	ALPM_DB_USAGE_ALL = (1 << 4) - 1,
} alpm_db_usage_t;

int alpm_db_set_usage(alpm_db_t *db, alpm_db_usage_t usage);

int alpm_db_get_usage(alpm_db_t *db, alpm_db_usage_t *usage);

/** @} */

/** @addtogroup libalpm_packages Package Functions
 * Functions to manipulate libalpm packages
 * @{
 */

int alpm_pkg_load(alpm_handle_t *handle, const char *filename, int full,
		alpm_siglevel_t level, alpm_pkg_t **pkg);
alpm_pkg_t *alpm_pkg_find(alpm_list_t *haystack, const char *needle);
int alpm_pkg_free(alpm_pkg_t *pkg);
int alpm_pkg_checkmd5sum(alpm_pkg_t *pkg);
int alpm_pkg_vercmp(const char *a, const char *b);
alpm_list_t *alpm_pkg_compute_requiredby(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_compute_optionalfor(alpm_pkg_t *pkg);
int alpm_pkg_should_ignore(alpm_handle_t *handle, alpm_pkg_t *pkg);

/** @name Package Property Accessors
 * Any pointer returned by these functions points to internal structures
 * allocated by libalpm. They should not be freed nor modified in any
 * way.
 * @{
 */

const char *alpm_pkg_get_filename(alpm_pkg_t *pkg);
const char *alpm_pkg_get_name(alpm_pkg_t *pkg);
const char *alpm_pkg_get_version(alpm_pkg_t *pkg);
alpm_pkgfrom_t alpm_pkg_get_origin(alpm_pkg_t *pkg);
const char *alpm_pkg_get_desc(alpm_pkg_t *pkg);
const char *alpm_pkg_get_url(alpm_pkg_t *pkg);
alpm_time_t alpm_pkg_get_builddate(alpm_pkg_t *pkg);
alpm_time_t alpm_pkg_get_installdate(alpm_pkg_t *pkg);
const char *alpm_pkg_get_packager(alpm_pkg_t *pkg);
const char *alpm_pkg_get_md5sum(alpm_pkg_t *pkg);
const char *alpm_pkg_get_sha256sum(alpm_pkg_t *pkg);
const char *alpm_pkg_get_arch(alpm_pkg_t *pkg);
off_t alpm_pkg_get_size(alpm_pkg_t *pkg);
off_t alpm_pkg_get_isize(alpm_pkg_t *pkg);
alpm_pkgreason_t alpm_pkg_get_reason(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_licenses(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_groups(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_depends(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_optdepends(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_conflicts(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_provides(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_deltas(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_replaces(alpm_pkg_t *pkg);
alpm_filelist_t *alpm_pkg_get_files(alpm_pkg_t *pkg);
alpm_list_t *alpm_pkg_get_backup(alpm_pkg_t *pkg);
alpm_db_t *alpm_pkg_get_db(alpm_pkg_t *pkg);
const char *alpm_pkg_get_base64_sig(alpm_pkg_t *pkg);
alpm_pkgvalidation_t alpm_pkg_get_validation(alpm_pkg_t *pkg);

/* End of alpm_pkg_t accessors */
/* @} */

void *alpm_pkg_changelog_open(alpm_pkg_t *pkg);
size_t alpm_pkg_changelog_read(void *ptr, size_t size,
		const alpm_pkg_t *pkg, void *fp);
int alpm_pkg_changelog_close(const alpm_pkg_t *pkg, void *fp);

struct archive *alpm_pkg_mtree_open(alpm_pkg_t *pkg);
int alpm_pkg_mtree_next(const alpm_pkg_t *pkg, struct archive *archive,
		struct archive_entry **entry);
int alpm_pkg_mtree_close(const alpm_pkg_t *pkg, struct archive *archive);

int alpm_pkg_has_scriptlet(alpm_pkg_t *pkg);

off_t alpm_pkg_download_size(alpm_pkg_t *newpkg);

alpm_list_t *alpm_pkg_unused_deltas(alpm_pkg_t *pkg);

int alpm_pkg_set_reason(alpm_pkg_t *pkg, alpm_pkgreason_t reason);


/* End of alpm_pkg */
/** @} */

alpm_file_t *alpm_filelist_contains(alpm_filelist_t *filelist, const char *path);

int alpm_pkg_check_pgp_signature(alpm_pkg_t *pkg, alpm_siglist_t *siglist);
int alpm_db_check_pgp_signature(alpm_db_t *db, alpm_siglist_t *siglist);
int alpm_siglist_cleanup(alpm_siglist_t *siglist);
int alpm_decode_signature(const char *base64_data,
		unsigned char **data, size_t *data_len);
int alpm_extract_keyid(alpm_handle_t *handle, const char *identifier,
		const unsigned char *sig, const size_t len, alpm_list_t **keys);

alpm_list_t *alpm_find_group_pkgs(alpm_list_t *dbs, const char *name);

alpm_pkg_t *alpm_sync_newversion(alpm_pkg_t *pkg, alpm_list_t *dbs_sync);

/** @addtogroup libalpm_trans Transaction Functions
 * Functions to manipulate libalpm transactions
 * @{
 */

/** Transaction flags */
typedef enum _alpm_transflag_t {
	/** Ignore dependency checks. */
	ALPM_TRANS_FLAG_NODEPS = 1,
	/** Ignore file conflicts and overwrite files. */
	ALPM_TRANS_FLAG_FORCE = (1 << 1),
	/** Delete files even if they are tagged as backup. */
	ALPM_TRANS_FLAG_NOSAVE = (1 << 2),
	/** Ignore version numbers when checking dependencies. */
	ALPM_TRANS_FLAG_NODEPVERSION = (1 << 3),
	/** Remove also any packages depending on a package being removed. */
	ALPM_TRANS_FLAG_CASCADE = (1 << 4),
	/** Remove packages and their unneeded deps (not explicitly installed). */
	ALPM_TRANS_FLAG_RECURSE = (1 << 5),
	/** Modify database but do not commit changes to the filesystem. */
	ALPM_TRANS_FLAG_DBONLY = (1 << 6),
	/* (1 << 7) flag can go here */
	/** Use ALPM_PKG_REASON_DEPEND when installing packages. */
	ALPM_TRANS_FLAG_ALLDEPS = (1 << 8),
	/** Only download packages and do not actually install. */
	ALPM_TRANS_FLAG_DOWNLOADONLY = (1 << 9),
	/** Do not execute install scriptlets after installing. */
	ALPM_TRANS_FLAG_NOSCRIPTLET = (1 << 10),
	/** Ignore dependency conflicts. */
	ALPM_TRANS_FLAG_NOCONFLICTS = (1 << 11),
	/* (1 << 12) flag can go here */
	/** Do not install a package if it is already installed and up to date. */
	ALPM_TRANS_FLAG_NEEDED = (1 << 13),
	/** Use ALPM_PKG_REASON_EXPLICIT when installing packages. */
	ALPM_TRANS_FLAG_ALLEXPLICIT = (1 << 14),
	/** Do not remove a package if it is needed by another one. */
	ALPM_TRANS_FLAG_UNNEEDED = (1 << 15),
	/** Remove also explicitly installed unneeded deps (use with ALPM_TRANS_FLAG_RECURSE). */
	ALPM_TRANS_FLAG_RECURSEALL = (1 << 16),
	/** Do not lock the database during the operation. */
	ALPM_TRANS_FLAG_NOLOCK = (1 << 17)
} alpm_transflag_t;

alpm_transflag_t alpm_trans_get_flags(alpm_handle_t *handle);

alpm_list_t *alpm_trans_get_add(alpm_handle_t *handle);
alpm_list_t *alpm_trans_get_remove(alpm_handle_t *handle);

int alpm_trans_init(alpm_handle_t *handle, alpm_transflag_t flags);
int alpm_trans_prepare(alpm_handle_t *handle, alpm_list_t **data);
int alpm_trans_commit(alpm_handle_t *handle, alpm_list_t **data);
int alpm_trans_interrupt(alpm_handle_t *handle);
int alpm_trans_release(alpm_handle_t *handle);
/** @} */

/** @name Common Transactions */
/** @{ */

int alpm_sync_sysupgrade(alpm_handle_t *handle, int enable_downgrade);
int alpm_add_pkg(alpm_handle_t *handle, alpm_pkg_t *pkg);
int alpm_remove_pkg(alpm_handle_t *handle, alpm_pkg_t *pkg);

/** @} */

/** @addtogroup libalpm_depends Dependency Functions
 * Functions dealing with libalpm representation of dependency
 * information.
 * @{
 */

alpm_list_t *alpm_checkdeps(alpm_handle_t *handle, alpm_list_t *pkglist,
		alpm_list_t *remove, alpm_list_t *upgrade, int reversedeps);
alpm_pkg_t *alpm_find_satisfier(alpm_list_t *pkgs, const char *depstring);
alpm_pkg_t *alpm_find_dbs_satisfier(alpm_handle_t *handle,
		alpm_list_t *dbs, const char *depstring);

alpm_list_t *alpm_checkconflicts(alpm_handle_t *handle, alpm_list_t *pkglist);

char *alpm_dep_compute_string(const alpm_depend_t *dep);

/** @} */

/** @} */

/*
 * Helpers
 */

/** @addtogroup libalpm_misc
 * @{
 */

/* checksums */
char *alpm_compute_md5sum(const char *filename);
char *alpm_compute_sha256sum(const char *filename);

/** @} */

/** @addtogroup libalpm_errors Error Codes
 * @{
 */
typedef enum _alpm_errno_t {
	ALPM_ERR_MEMORY = 1,
	ALPM_ERR_SYSTEM,
	ALPM_ERR_BADPERMS,
	ALPM_ERR_NOT_A_FILE,
	ALPM_ERR_NOT_A_DIR,
	ALPM_ERR_WRONG_ARGS,
	ALPM_ERR_DISK_SPACE,
	/* Interface */
	ALPM_ERR_HANDLE_NULL,
	ALPM_ERR_HANDLE_NOT_NULL,
	ALPM_ERR_HANDLE_LOCK,
	/* Databases */
	ALPM_ERR_DB_OPEN,
	ALPM_ERR_DB_CREATE,
	ALPM_ERR_DB_NULL,
	ALPM_ERR_DB_NOT_NULL,
	ALPM_ERR_DB_NOT_FOUND,
	ALPM_ERR_DB_INVALID,
	ALPM_ERR_DB_INVALID_SIG,
	ALPM_ERR_DB_VERSION,
	ALPM_ERR_DB_WRITE,
	ALPM_ERR_DB_REMOVE,
	/* Servers */
	ALPM_ERR_SERVER_BAD_URL,
	ALPM_ERR_SERVER_NONE,
	/* Transactions */
	ALPM_ERR_TRANS_NOT_NULL,
	ALPM_ERR_TRANS_NULL,
	ALPM_ERR_TRANS_DUP_TARGET,
	ALPM_ERR_TRANS_NOT_INITIALIZED,
	ALPM_ERR_TRANS_NOT_PREPARED,
	ALPM_ERR_TRANS_ABORT,
	ALPM_ERR_TRANS_TYPE,
	ALPM_ERR_TRANS_NOT_LOCKED,
	/* Packages */
	ALPM_ERR_PKG_NOT_FOUND,
	ALPM_ERR_PKG_IGNORED,
	ALPM_ERR_PKG_INVALID,
	ALPM_ERR_PKG_INVALID_CHECKSUM,
	ALPM_ERR_PKG_INVALID_SIG,
	ALPM_ERR_PKG_MISSING_SIG,
	ALPM_ERR_PKG_OPEN,
	ALPM_ERR_PKG_CANT_REMOVE,
	ALPM_ERR_PKG_INVALID_NAME,
	ALPM_ERR_PKG_INVALID_ARCH,
	ALPM_ERR_PKG_REPO_NOT_FOUND,
	/* Signatures */
	ALPM_ERR_SIG_MISSING,
	ALPM_ERR_SIG_INVALID,
	/* Deltas */
	ALPM_ERR_DLT_INVALID,
	ALPM_ERR_DLT_PATCHFAILED,
	/* Dependencies */
	ALPM_ERR_UNSATISFIED_DEPS,
	ALPM_ERR_CONFLICTING_DEPS,
	ALPM_ERR_FILE_CONFLICTS,
	/* Misc */
	ALPM_ERR_RETRIEVE,
	ALPM_ERR_INVALID_REGEX,
	/* External library errors */
	ALPM_ERR_LIBARCHIVE,
	ALPM_ERR_LIBCURL,
	ALPM_ERR_EXTERNAL_DOWNLOAD,
	ALPM_ERR_GPGME
} alpm_errno_t;

alpm_errno_t alpm_errno(alpm_handle_t *handle);
const char *alpm_strerror(alpm_errno_t err);

/* End of alpm_errors */
/** @} */

/** @addtogroup libalpm_interface Interface Functions
 * @brief Functions to initialize and release libalpm
 * @{
 */

alpm_handle_t *alpm_initialize(const char *root, const char *dbpath,
		alpm_errno_t *err);
int alpm_release(alpm_handle_t *handle);

/** @} */

/** @addtogroup libalpm_misc Miscellaneous Functions
 * @brief Various libalpm functions
 * @{
 */

enum alpm_caps {
	ALPM_CAPABILITY_NLS = (1 << 0),
	ALPM_CAPABILITY_DOWNLOADER = (1 << 1),
	ALPM_CAPABILITY_SIGNATURES = (1 << 2)
};

const char *alpm_version(void);
enum alpm_caps alpm_capabilities(void);

/** @} */

/* End of alpm */
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _ALPM_H */

/* vim: set ts=2 sw=2 noet: */
