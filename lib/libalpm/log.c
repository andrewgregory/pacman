/*
 *  log.c
 *
 *  Copyright (c) 2006-2015 Pacman Development Team <pacman-dev@archlinux.org>
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

/* libalpm */
#include "log.h"
#include "handle.h"
#include "util.h"
#include "alpm.h"

/** \addtogroup alpm_log Logging Functions
 * @brief Functions to log using libalpm
 * @{
 */

static int _alpm_log_open(alpm_handle_t *handle)
{
	int fd;
	do {
		fd = open(handle->logfile, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
	} while(fd == -1 && errno == EINTR);
	if(fd >= 0) {
		handle->logstream = fdopen(fd, "a");
	}
	/* if we couldn't open it, we have an issue */
	if(fd < 0 || handle->logstream == NULL) {
		if(errno == EACCES) {
			handle->pm_errno = ALPM_ERR_BADPERMS;
		} else if(errno == ENOENT) {
			handle->pm_errno = ALPM_ERR_NOT_A_DIR;
		} else {
			handle->pm_errno = ALPM_ERR_SYSTEM;
		}
		return -1;
	}
	return 0;
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
	va_start(args, fmt);

	if(handle->usesyslog) {
		/* we can't use a va_list more than once, so we need to copy it
		 * so we can use the original when calling vfprintf below. */
		va_list args_syslog;
		va_copy(args_syslog, args);
		vsyslog(LOG_WARNING, fmt, args_syslog);
		va_end(args_syslog);
	}

	if(handle->logfile) {
		/* check if the logstream is open already, opening it if needed */
		if(handle->logstream || _alpm_log_open(handle) == 0) {
			time_t t = time(NULL);
			struct tm tm;
			localtime_r(&t, &tm);

			/* Use ISO-8601 date format */
			fprintf(handle->logstream, "[%04d-%02d-%02d %02d:%02d] [%s] ",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
					tm.tm_hour, tm.tm_min, prefix);
			vfprintf(handle->logstream, fmt, args);
			fflush(handle->logstream);
		} else {
			ret = -1;
		}
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
	handle->logcb(flag, fmt, args);
	va_end(args);
}

/* vim: set noet: */
