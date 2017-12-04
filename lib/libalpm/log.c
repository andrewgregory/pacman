/*
 *  log.c
 *
 *  Copyright (c) 2006-2017 Pacman Development Team <pacman-dev@archlinux.org>
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
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>

/* libalpm */
#include "log.h"
#include "handle.h"
#include "util.h"
#include "alpm.h"
#include "thread.h"

/** \addtogroup alpm_log Logging Functions
 * @brief Functions to log using libalpm
 * @{
 */

static int _alpm_log_leader(FILE *f, const char *prefix)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	/* Use ISO-8601 date format */
	return fprintf(f, "[%04d-%02d-%02d %02d:%02d] [%s] ",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, prefix);
}

/** A printf-like function for logging.
 * @param handle the context handle
 * @param prefix caller-specific prefix for the log
 * @param fmt output format
 * @return 0 on success, -1 on error (pm_errno is set accordingly)
 */
int SYMEXPORT alpm_logaction(alpm_handle_t *handle, const char *prefix,
		const char *fmt, ...)
{
	int ret = 0;
	va_list args;

	ASSERT(handle != NULL, return -1);

	if(!(prefix && *prefix)) {
		prefix = "UNKNOWN";
	}
	_ALPM_TLOCK_LOG(handle);

	/* check if the logstream is open already, opening it if needed */
	if(handle->logstream == NULL && handle->logfile != NULL) {
		int fd;
		do {
			fd = open(handle->logfile, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
					0644);
		} while(fd == -1 && errno == EINTR);
		/* if we couldn't open it, we have an issue */
		if(fd < 0 || (handle->logstream = fdopen(fd, "a")) == NULL) {
			if(errno == EACCES) {
				_alpm_set_errno(handle, ALPM_ERR_BADPERMS);
			} else if(errno == ENOENT) {
				_alpm_set_errno(handle, ALPM_ERR_NOT_A_DIR);
			} else {
				_alpm_set_errno(handle, ALPM_ERR_SYSTEM);
			}
			ret = -1;
		}
	}

	_ALPM_TUNLOCK_LOG(handle);

	va_start(args, fmt);

	if(handle->usesyslog) {
		/* we can't use a va_list more than once, so we need to copy it
		 * so we can use the original when calling vfprintf below. */
		va_list args_syslog;
		va_copy(args_syslog, args);
		vsyslog(LOG_WARNING, fmt, args_syslog);
		va_end(args_syslog);
	}

	if(handle->logstream) {
		if(_alpm_log_leader(handle->logstream, prefix) < 0
				|| vfprintf(handle->logstream, fmt, args) < 0) {
			ret = -1;
			handle->pm_errno = ALPM_ERR_SYSTEM;
		}
		fflush(handle->logstream);
	}

	va_end(args);
	return ret;
}

/** @} */

void _alpm_log(alpm_handle_t *handle, alpm_loglevel_t flag, const char *fmt, ...)
{
	va_list args;

	if(handle == NULL || handle->logcb == NULL) {
		return;
	}

	va_start(args, fmt);
	_ALPM_TLOCK_CB(handle);
	handle->logcb(flag, fmt, args);
	_ALPM_TUNLOCK_CB(handle);
	va_end(args);
}

/** Write formatted message to log.
 * @param handle the context handle
 * @param format formatted string to write out
 * @param args formatting arguments
 * @return 0 or number of characters written on success, vfprintf return value
 * on error
 */
int _alpm_logaction(alpm_handle_t *handle, const char *prefix,
		const char *fmt, va_list args)
{
	int ret = 0;

	if(!(prefix && *prefix)) {
		prefix = "UNKNOWN";
	}

	_ALPM_TLOCK_LOG(handle);

	if(handle->usesyslog) {
		/* we can't use a va_list more than once, so we need to copy it
		 * so we can use the original when calling vfprintf below. */
		va_list args_syslog;
		va_copy(args_syslog, args);
		vsyslog(LOG_WARNING, fmt, args_syslog);
		va_end(args_syslog);
	}

	if(handle->logstream) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);

		/* Use ISO-8601 date format */
		fprintf(handle->logstream, "[%04d-%02d-%02d %02d:%02d] [%s] ",
						tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
						tm->tm_hour, tm->tm_min, prefix);
		ret = vfprintf(handle->logstream, fmt, args);
		fflush(handle->logstream);
	}

	_ALPM_TUNLOCK_LOG(handle);

	return ret;
}

/* vim: set noet: */
