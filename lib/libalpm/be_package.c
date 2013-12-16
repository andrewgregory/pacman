/*
 *  be_package.c : backend for packages
 *
 *  Copyright (c) 2006-2013 Pacman Development Team <pacman-dev@archlinux.org>
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "alpm_list.h"
#include "alpm.h"
#include "libarchive-compat.h"
#include "util.h"
#include "log.h"
#include "handle.h"
#include "package.h"
#include "deps.h"
#include "filelist.h"

struct _alpm_package_changelog_t {
	struct archive *archive;
	int fd;
};

/**
 * Open a package changelog for reading. Similar to fopen in functionality,
 * except that the returned 'file stream' is from an archive.
 * @param pkg the package (file) to read the changelog
 * @return a 'file stream' to the package changelog
 */
static void *_package_changelog_open(alpm_pkg_t *pkg)
{
	ASSERT(pkg != NULL, return NULL);

	struct _alpm_package_changelog_t *changelog;
	struct archive *archive;
	struct archive_entry *entry;
	const char *pkgfile = pkg->origin_data.file;
	struct stat buf;
	int fd;

	fd = _alpm_open_archive(pkg->handle, pkgfile, &buf,
			&archive, ALPM_ERR_PKG_OPEN);
	if(fd < 0) {
		return NULL;
	}

	while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".CHANGELOG") == 0) {
			changelog = malloc(sizeof(struct _alpm_package_changelog_t));
			if(!changelog) {
				pkg->handle->pm_errno = ALPM_ERR_MEMORY;
				_alpm_archive_read_free(archive);
				close(fd);
				return NULL;
			}
			changelog->archive = archive;
			changelog->fd = fd;
			return changelog;
		}
	}
	/* we didn't find a changelog */
	_alpm_archive_read_free(archive);
	close(fd);
	errno = ENOENT;

	return NULL;
}

/**
 * Read data from an open changelog 'file stream'. Similar to fread in
 * functionality, this function takes a buffer and amount of data to read.
 * @param ptr a buffer to fill with raw changelog data
 * @param size the size of the buffer
 * @param pkg the package that the changelog is being read from
 * @param fp a 'file stream' to the package changelog
 * @return the number of characters read, or 0 if there is no more data
 */
static size_t _package_changelog_read(void *ptr, size_t size,
		const alpm_pkg_t UNUSED *pkg, void *fp)
{
	struct _alpm_package_changelog_t *changelog = fp;
	ssize_t sret = archive_read_data(changelog->archive, ptr, size);
	/* Report error (negative values) */
	if(sret < 0) {
		RET_ERR(pkg->handle, ALPM_ERR_LIBARCHIVE, 0);
	} else {
		return (size_t)sret;
	}
}

/**
 * Close a package changelog for reading. Similar to fclose in functionality,
 * except that the 'file stream' is from an archive.
 * @param pkg the package (file) that the changelog was read from
 * @param fp a 'file stream' to the package changelog
 * @return whether closing the package changelog stream was successful
 */
static int _package_changelog_close(const alpm_pkg_t UNUSED *pkg, void *fp)
{
	int ret;
	struct _alpm_package_changelog_t *changelog = fp;
	ret = _alpm_archive_read_free(changelog->archive);
	close(changelog->fd);
	free(changelog);
	return ret;
}

/** Package file operations struct accessor. We implement this as a method
 * rather than a static struct as in be_files because we want to reuse the
 * majority of the _alpm_default_pkg_ops struct and add only a few operations
 * of our own on top.
 */
static struct _alpm_pkg_operations_t *get_file_pkg_ops(void)
{
	static struct _alpm_pkg_operations_t file_pkg_ops;
	static int file_pkg_ops_initialized = 0;
	if(!file_pkg_ops_initialized) {
		file_pkg_ops = _alpm_default_pkg_ops;
		file_pkg_ops.changelog_open  = _package_changelog_open;
		file_pkg_ops.changelog_read  = _package_changelog_read;
		file_pkg_ops.changelog_close = _package_changelog_close;
		file_pkg_ops_initialized = 1;
	}
	return &file_pkg_ops;
}

/**
 * Parses the package description file for a package into a alpm_pkg_t struct.
 * @param archive the archive to read from, pointed at the .PKGINFO entry
 * @param newpkg an empty alpm_pkg_t struct to fill with package info
 *
 * @return 0 on success, -1 on error
 */
static int parse_descfile(alpm_handle_t *handle, struct archive *a, alpm_pkg_t *newpkg)
{
	char *ptr = NULL;
	char *key = NULL;
	int ret, linenum = 0;
	struct _alpm_archive_read_buffer_t buf;

	memset(&buf, 0, sizeof(buf));
	/* 512K for a line length seems reasonable */
	buf.max_line_size = 512 * 1024;

	/* loop until we reach EOF or other error */
	while((ret = _alpm_archive_fgets(a, &buf)) == ARCHIVE_OK) {
		size_t len = _alpm_strip_newline(buf.line, buf.real_line_size);

		linenum++;
		key = buf.line;
		if(len == 0 || key[0] == '#') {
			continue;
		}
		/* line is always in this format: "key = value"
		 * we can be sure the " = " exists, so look for that */
		ptr = memchr(key, ' ', len);
		if(!ptr || (size_t)(ptr - key + 2) > len || memcmp(ptr, " = ", 3) != 0) {
			_alpm_log(handle, ALPM_LOG_DEBUG,
					"%s: syntax error in description file line %d\n",
					newpkg->name ? newpkg->name : "error", linenum);
		} else {
			/* NULL the end of the key portion, move ptr to start of value */
			*ptr = '\0';
			ptr += 3;
			if(strcmp(key, "pkgname") == 0) {
				STRDUP(newpkg->name, ptr, return -1);
				newpkg->name_hash = _alpm_hash_sdbm(newpkg->name);
			} else if(strcmp(key, "pkgbase") == 0) {
				/* not used atm */
			} else if(strcmp(key, "pkgver") == 0) {
				STRDUP(newpkg->version, ptr, return -1);
			} else if(strcmp(key, "pkgdesc") == 0) {
				STRDUP(newpkg->desc, ptr, return -1);
			} else if(strcmp(key, "group") == 0) {
				newpkg->groups = alpm_list_add(newpkg->groups, strdup(ptr));
			} else if(strcmp(key, "url") == 0) {
				STRDUP(newpkg->url, ptr, return -1);
			} else if(strcmp(key, "license") == 0) {
				newpkg->licenses = alpm_list_add(newpkg->licenses, strdup(ptr));
			} else if(strcmp(key, "builddate") == 0) {
				newpkg->builddate = _alpm_parsedate(ptr);
			} else if(strcmp(key, "packager") == 0) {
				STRDUP(newpkg->packager, ptr, return -1);
			} else if(strcmp(key, "arch") == 0) {
				STRDUP(newpkg->arch, ptr, return -1);
			} else if(strcmp(key, "size") == 0) {
				/* size in the raw package is uncompressed (installed) size */
				newpkg->isize = _alpm_strtoofft(ptr);
			} else if(strcmp(key, "depend") == 0) {
				alpm_depend_t *dep = _alpm_splitdep(ptr);
				newpkg->depends = alpm_list_add(newpkg->depends, dep);
			} else if(strcmp(key, "optdepend") == 0) {
				alpm_depend_t *optdep = _alpm_splitdep(ptr);
				newpkg->optdepends = alpm_list_add(newpkg->optdepends, optdep);
			} else if(strcmp(key, "makedepend") == 0) {
				/* not used atm */
			} else if(strcmp(key, "checkdepend") == 0) {
				/* not used atm */
			} else if(strcmp(key, "conflict") == 0) {
				alpm_depend_t *conflict = _alpm_splitdep(ptr);
				newpkg->conflicts = alpm_list_add(newpkg->conflicts, conflict);
			} else if(strcmp(key, "replaces") == 0) {
				alpm_depend_t *replace = _alpm_splitdep(ptr);
				newpkg->replaces = alpm_list_add(newpkg->replaces, replace);
			} else if(strcmp(key, "provides") == 0) {
				alpm_depend_t *provide = _alpm_splitdep(ptr);
				newpkg->provides = alpm_list_add(newpkg->provides, provide);
			} else if(strcmp(key, "backup") == 0) {
				alpm_backup_t *backup;
				CALLOC(backup, 1, sizeof(alpm_backup_t), return -1);
				STRDUP(backup->name, ptr, return -1);
				newpkg->backup = alpm_list_add(newpkg->backup, backup);
			} else if(strcmp(key, "force") == 0) {
				/* deprecated, skip it */
			} else if(strcmp(key, "makepkgopt") == 0) {
				/* not used atm */
			} else {
				_alpm_log(handle, ALPM_LOG_DEBUG, "%s: unknown key '%s' in description file line %d\n",
									newpkg->name ? newpkg->name : "error", key, linenum);
			}
		}
	}
	if(ret != ARCHIVE_EOF) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "error parsing package descfile\n");
		return -1;
	}

	return 0;
}

/**
 * Validate a package.
 * @param handle the context handle
 * @param pkgfile path to the package file
 * @param syncpkg package object to load verification data from (md5sum,
 * sha256sum, and/or base64 signature)
 * @param level the required level of signature verification
 * @param sigdata signature data from the package to pass back
 * @param validation successful validations performed on the package file
 * @return 0 if package is fully valid, -1 and pm_errno otherwise
 */
int _alpm_pkg_validate_internal(alpm_handle_t *handle,
		const char *pkgfile, alpm_pkg_t *syncpkg, alpm_siglevel_t level,
		alpm_siglist_t **sigdata, alpm_pkgvalidation_t *validation)
{
	int has_sig;
	handle->pm_errno = 0;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1);
	}

	/* attempt to access the package file, ensure it exists */
	if(_alpm_access(handle, NULL, pkgfile, R_OK) != 0) {
		if(errno == ENOENT) {
			handle->pm_errno = ALPM_ERR_PKG_NOT_FOUND;
		} else if(errno == EACCES) {
			handle->pm_errno = ALPM_ERR_BADPERMS;
		} else {
			handle->pm_errno = ALPM_ERR_PKG_OPEN;
		}
		return -1;
	}

	/* can we get away with skipping checksums? */
	has_sig = 0;
	if(level & ALPM_SIG_PACKAGE) {
		if(syncpkg && syncpkg->base64_sig) {
			has_sig = 1;
		} else {
			char *sigpath = _alpm_sigpath(handle, pkgfile);
			if(sigpath && !_alpm_access(handle, NULL, sigpath, R_OK)) {
				has_sig = 1;
			}
			free(sigpath);
		}
	}

	if(syncpkg && !has_sig) {
		if(syncpkg->md5sum && !syncpkg->sha256sum) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "md5sum: %s\n", syncpkg->md5sum);
			_alpm_log(handle, ALPM_LOG_DEBUG, "checking md5sum for %s\n", pkgfile);
			if(_alpm_test_checksum(pkgfile, syncpkg->md5sum, ALPM_PKG_VALIDATION_MD5SUM) != 0) {
				RET_ERR(handle, ALPM_ERR_PKG_INVALID_CHECKSUM, -1);
			}
			if(validation) {
				*validation |= ALPM_PKG_VALIDATION_MD5SUM;
			}
		}

		if(syncpkg->sha256sum) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "sha256sum: %s\n", syncpkg->sha256sum);
			_alpm_log(handle, ALPM_LOG_DEBUG, "checking sha256sum for %s\n", pkgfile);
			if(_alpm_test_checksum(pkgfile, syncpkg->sha256sum, ALPM_PKG_VALIDATION_SHA256SUM) != 0) {
				RET_ERR(handle, ALPM_ERR_PKG_INVALID_CHECKSUM, -1);
			}
			if(validation) {
				*validation |= ALPM_PKG_VALIDATION_SHA256SUM;
			}
		}
	}

	/* even if we don't have a sig, run the check code if level tells us to */
	if(level & ALPM_SIG_PACKAGE) {
		const char *sig = syncpkg ? syncpkg->base64_sig : NULL;
		_alpm_log(handle, ALPM_LOG_DEBUG, "sig data: %s\n", sig ? sig : "<from .sig>");
		if(!has_sig && !(level & ALPM_SIG_PACKAGE_OPTIONAL)) {
			handle->pm_errno = ALPM_ERR_PKG_MISSING_SIG;
			return -1;
		}
		if(_alpm_check_pgp_helper(handle, pkgfile, sig,
					level & ALPM_SIG_PACKAGE_OPTIONAL, level & ALPM_SIG_PACKAGE_MARGINAL_OK,
					level & ALPM_SIG_PACKAGE_UNKNOWN_OK, sigdata)) {
			handle->pm_errno = ALPM_ERR_PKG_INVALID_SIG;
			return -1;
		}
		if(validation && has_sig) {
			*validation |= ALPM_PKG_VALIDATION_SIGNATURE;
		}
	}

	if(validation && !*validation) {
		*validation = ALPM_PKG_VALIDATION_NONE;
	}

	return 0;
}

/**
 * Load a package and create the corresponding alpm_pkg_t struct.
 * @param handle the context handle
 * @param pkgfile path to the package file
 * @param full whether to stop the load after metadata is read or continue
 * through the full archive
 */
alpm_pkg_t *_alpm_pkg_load_internal(alpm_handle_t *handle,
		const char *pkgfile, int full)
{
	int ret, fd, config = 0;
	struct archive *archive;
	struct archive_entry *entry;
	alpm_pkg_t *newpkg;
	struct stat st;
	size_t files_size = 0;

	if(pkgfile == NULL || strlen(pkgfile) == 0) {
		RET_ERR(handle, ALPM_ERR_WRONG_ARGS, NULL);
	}

	fd = _alpm_open_archive(handle, pkgfile, &st, &archive, ALPM_ERR_PKG_OPEN);
	if(fd < 0) {
		if(errno == ENOENT) {
			handle->pm_errno = ALPM_ERR_PKG_NOT_FOUND;
		} else if(errno == EACCES) {
			handle->pm_errno = ALPM_ERR_BADPERMS;
		} else {
			handle->pm_errno = ALPM_ERR_PKG_OPEN;
		}
		return NULL;
	}

	newpkg = _alpm_pkg_new();
	if(newpkg == NULL) {
		handle->pm_errno = ALPM_ERR_MEMORY;
		goto error;
	}
	STRDUP(newpkg->filename, pkgfile,
			handle->pm_errno = ALPM_ERR_MEMORY; goto error);
	newpkg->size = st.st_size;

	_alpm_log(handle, ALPM_LOG_DEBUG, "starting package load for %s\n", pkgfile);

	/* If full is false, only read through the archive until we find our needed
	 * metadata. If it is true, read through the entire archive, which serves
	 * as a verification of integrity and allows us to create the filelist. */
	while((ret = archive_read_next_header(archive, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);

		if(strcmp(entry_name, ".PKGINFO") == 0) {
			/* parse the info file */
			if(parse_descfile(handle, archive, newpkg) != 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("could not parse package description file in %s\n"),
						pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->name == NULL || strlen(newpkg->name) == 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("missing package name in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			if(newpkg->version == NULL || strlen(newpkg->version) == 0) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("missing package version in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			if(strchr(newpkg->version, '-') == NULL) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("invalid package version in %s\n"), pkgfile);
				goto pkg_invalid;
			}
			config = 1;
			continue;
		} else if(strcmp(entry_name, ".INSTALL") == 0) {
			newpkg->scriptlet = 1;
		} else if(*entry_name == '.') {
			/* for now, ignore all files starting with '.' that haven't
			 * already been handled (for future possibilities) */
		} else if(full) {
			const size_t files_count = newpkg->files.count;
			alpm_file_t *current_file;
			/* Keep track of all files for filelist generation */
			if(files_count >= files_size) {
				size_t old_size = files_size;
				alpm_file_t *newfiles;
				if(files_size == 0) {
					files_size = 4;
				} else {
					files_size *= 2;
				}
				newfiles = realloc(newpkg->files.files,
						sizeof(alpm_file_t) * files_size);
				if(!newfiles) {
					_alpm_alloc_fail(sizeof(alpm_file_t) * files_size);
					goto error;
				}
				/* ensure all new memory is zeroed out, in both the initial
				 * allocation and later reallocs */
				memset(newfiles + old_size, 0,
						sizeof(alpm_file_t) * (files_size - old_size));
				newpkg->files.files = newfiles;
			}
			current_file = newpkg->files.files + files_count;
			STRDUP(current_file->name, entry_name, goto error);
			current_file->size = archive_entry_size(entry);
			current_file->mode = archive_entry_mode(entry);
			newpkg->files.count++;
		}

		if(archive_read_data_skip(archive)) {
			_alpm_log(handle, ALPM_LOG_ERROR, _("error while reading package %s: %s\n"),
					pkgfile, archive_error_string(archive));
			handle->pm_errno = ALPM_ERR_LIBARCHIVE;
			goto error;
		}

		/* if we are not doing a full read, see if we have all we need */
		if(!full && config) {
			break;
		}
	}

	if(ret != ARCHIVE_EOF && ret != ARCHIVE_OK) { /* An error occurred */
		_alpm_log(handle, ALPM_LOG_ERROR, _("error while reading package %s: %s\n"),
				pkgfile, archive_error_string(archive));
		handle->pm_errno = ALPM_ERR_LIBARCHIVE;
		goto error;
	}

	if(!config) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("missing package metadata in %s\n"), pkgfile);
		goto pkg_invalid;
	}

	_alpm_archive_read_free(archive);
	close(fd);

	/* internal fields for package struct */
	newpkg->origin = ALPM_PKG_FROM_FILE;
	newpkg->origin_data.file = strdup(pkgfile);
	newpkg->ops = get_file_pkg_ops();
	newpkg->handle = handle;
	newpkg->infolevel = INFRQ_BASE | INFRQ_DESC | INFRQ_SCRIPTLET;
	newpkg->validation = ALPM_PKG_VALIDATION_NONE;

	if(full) {
		if(newpkg->files.files) {
			/* attempt to hand back any memory we don't need */
			newpkg->files.files = realloc(newpkg->files.files,
					sizeof(alpm_file_t) * newpkg->files.count);
			/* "checking for conflicts" requires a sorted list, ensure that here */
			_alpm_log(handle, ALPM_LOG_DEBUG,
					"sorting package filelist for %s\n", pkgfile);

			qsort(newpkg->files.files, newpkg->files.count,
					sizeof(alpm_file_t), _alpm_files_cmp);
		}
		newpkg->infolevel |= INFRQ_FILES;
	}

	return newpkg;

pkg_invalid:
	handle->pm_errno = ALPM_ERR_PKG_INVALID;
error:
	_alpm_pkg_free(newpkg);
	_alpm_archive_read_free(archive);
	if(fd >= 0) {
		close(fd);
	}

	return NULL;
}

static int read_sigfile(const char *sigpath, unsigned char **sig)
{
	struct stat st;
	FILE *fp;

	if(stat(sigpath, &st) != 0) {
		return -1;
	}

	MALLOC(*sig, st.st_size, return -1);

	if((fp = fopen(sigpath, "rb")) == NULL) {
		free(*sig);
		return -1;
	}

	if(fread(*sig, st.st_size, 1, fp) != 1) {
		free(*sig);
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return st.st_size;
}

/** Create a package from a file.
 * If full is false, the archive is read only until all necessary
 * metadata is found. If it is true, the entire archive is read, which
 * serves as a verification of integrity and the filelist can be created.
 * The allocated structure should be freed using alpm_pkg_free().
 * @param handle the context handle
 * @param filename location of the package tarball
 * @param full whether to stop the load after metadata is read or continue
 * through the full archive
 * @param level what level of package signature checking to perform on the
 * package; note that this must be a '.sig' file type verification
 * @param pkg address of the package pointer
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_pkg_load(alpm_handle_t *handle, const char *filename, int full,
		alpm_siglevel_t level, alpm_pkg_t **pkg)
{
	alpm_pkgvalidation_t validation = 0;
	char *sigpath;

	CHECK_HANDLE(handle, return -1);
	ASSERT(pkg != NULL, RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1));

	sigpath = _alpm_sigpath(handle, filename);
	if(sigpath && !_alpm_access(handle, NULL, sigpath, R_OK)) {
		if(level & ALPM_SIG_PACKAGE) {
			alpm_list_t *keys = NULL;
			int fail = 0;
			unsigned char *sig = NULL;
			int len = read_sigfile(sigpath, &sig);

			if(len == -1) {
				_alpm_log(handle, ALPM_LOG_ERROR,
					_("failed to read signature file: %s\n"), sigpath);
				free(sigpath);
				return -1;
			}

			if(alpm_extract_keyid(handle, filename, sig, len, &keys) == 0) {
				alpm_list_t *k;
				for(k = keys; k; k = k->next) {
					char *key = k->data;
					if(_alpm_key_in_keychain(handle, key) == 0) {
						if(_alpm_key_import(handle, key) == -1) {
							fail = 1;
						}
					}
				}
				FREELIST(keys);
			}

			free(sig);

			if(fail) {
				_alpm_log(handle, ALPM_LOG_ERROR, _("required key missing from keyring\n"));
				return -1;
			}
		}
	}
	free(sigpath);

	if(_alpm_pkg_validate_internal(handle, filename, NULL, level, NULL,
				&validation) == -1) {
		/* pm_errno is set by pkg_validate */
		return -1;
	}
	*pkg = _alpm_pkg_load_internal(handle, filename, full);
	if(*pkg == NULL) {
		/* pm_errno is set by pkg_load */
		return -1;
	}
	(*pkg)->validation = validation;

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
