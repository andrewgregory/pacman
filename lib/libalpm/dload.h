/*
 *  dload.h
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
#ifndef _ALPM_DLOAD_H
#define _ALPM_DLOAD_H

#include "alpm_list.h"
#include "alpm.h"

struct _alpm_dload_payload_t {
	alpm_handle_t *handle;
	const char *tempfile_openmode;
	char *remote_name;
	char *tempfile_name;
	char *destfile_name;
	char *content_disp_name;
	char *fileurl;
	off_t initial_size;
	off_t max_size;
	off_t prevprogress;
	int force;
	int allow_resume;
	int errors_ok;
	int unlink_on_fail;
	int trust_remote_name;
	alpm_list_t *servers;
#ifdef HAVE_LIBCURL
	CURLcode curlerr;       /* last error produced by curl */
#endif
	long respcode;
};

void _alpm_dload_payload_reset(struct _alpm_dload_payload_t *payload);

int _alpm_download(struct _alpm_dload_payload_t *payload, const char *localpath,
		char **final_file, char **final_url);

#endif /* _ALPM_DLOAD_H */

/* vim: set ts=2 sw=2 noet: */
