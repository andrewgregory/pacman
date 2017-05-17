/*
 *  thread.h
 *
 *  Copyright (c) 2006-2014 Pacman Development Team <pacman-dev@archlinux.org>
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
#ifndef _ALPM_THREAD_H
#define _ALPM_THREAD_H

#include <pthread.h>
#include <string.h>

#include <log.h>

#ifdef HAVE_PTHREAD

#define _ALPM_TLOCK_CB(h) _alpm_lock_mutex(h, &h->tlock_cb, "CB", __FILE__, __LINE__)
#define _ALPM_TLOCK_LOG(h) _alpm_lock_mutex(h, &h->tlock_log, "LOG", __FILE__, __LINE__)
#define _ALPM_TLOCK_TASK(h) _alpm_lock_mutex(h, &h->tlock_task, "TASK", __FILE__, __LINE__)

#define _ALPM_TUNLOCK_CB(h) pthread_mutex_unlock(&((h)->tlock_cb))
#define _ALPM_TUNLOCK_LOG(h) pthread_mutex_unlock(&((h)->tlock_log))
#define _ALPM_TUNLOCK_TASK(h) pthread_mutex_unlock(&((h)->tlock_task))

static inline int _alpm_lock_mutex(alpm_handle_t *handle, pthread_mutex_t *m,
		const char *label, const char *file, int lineno)
{
	int lockret;
	struct timespec timeout;
	const int period = 20;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += period;
	if((lockret = pthread_mutex_timedlock(m, &timeout)) != 0) {
		_alpm_log(handle, ALPM_LOG_WARNING,
				"unable to obtain %s lock within %d seconds (%d: %s) (%s: %d)\n",
				label, period, lockret, strerror(lockret), file, lineno);
		return lockret;
	}
	return 0;
}

#else

#define _ALPM_TLOCK_CB(h) ((int)0)
#define _ALPM_TLOCK_LOG(h) ((int)0)
#define _ALPM_TLOCK_TASK(h) ((int)0)

#define _ALPM_TUNLOCK_CB(h) ((int)0)
#define _ALPM_TUNLOCK_LOG(h) ((int)0)
#define _ALPM_TUNLOCK_TASK(h) ((int)0)

#endif /* HAVE_PTHREAD */

#endif /* _ALPM_THREAD_H */

/* vim: set noet: */
