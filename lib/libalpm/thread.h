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

#ifdef HAVE_PTHREAD

#define _ALPM_TLOCK_TIMED(m) do { \
	int lockret; \
	struct timespec timeout; \
	clock_gettime(CLOCK_REALTIME, &timeout); \
	timeout.tv_sec += 60; \
	if((lockret = pthread_mutex_timedlock(m, &timeout)) != 0) { \
		printf("warning: unable to obtain %s lock within %d seconds (%d: %s) (%s: %d)\n", \
				#m, 60, lockret, strerror(lockret), __FILE__, __LINE__); \
		pthread_mutex_lock(m); \
	} \
} while (0)

#define _ALPM_TLOCK_CB(h) _ALPM_TLOCK_TIMED(&((h)->tlock_cb))
#define _ALPM_TLOCK_LOG(h) _ALPM_TLOCK_TIMED(&((h)->tlock_log))
#define _ALPM_TLOCK_TASK(h) _ALPM_TLOCK_TIMED(&((h)->tlock_task))

#define _ALPM_TUNLOCK_CB(h) pthread_mutex_unlock(&((h)->tlock_cb))
#define _ALPM_TUNLOCK_LOG(h) pthread_mutex_unlock(&((h)->tlock_log))
#define _ALPM_TUNLOCK_TASK(h) pthread_mutex_unlock(&((h)->tlock_task))

#else

#define _ALPM_TLOCK_CB(h)
#define _ALPM_TLOCK_LOG(h)
#define _ALPM_TLOCK_TASK(h)

#define _ALPM_TUNLOCK_CB(h)
#define _ALPM_TUNLOCK_LOG(h)
#define _ALPM_TUNLOCK_TASK(h)

#endif /* HAVE_PTHREAD */

#endif /* _ALPM_THREAD_H */

/* vim: set noet: */
