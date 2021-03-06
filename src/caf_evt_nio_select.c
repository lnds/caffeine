/* -*- mode: c; indent-tabs-mode: t; tab-width: 4; c-file-style: "caf" -*- */
/* vim:set ft=c ff=unix ts=4 sw=4 enc=latin1 noexpandtab: */
/* kate: space-indent off; indent-width 4; mixedindent off; indent-mode cstyle; */
/*
  Caffeine - C Application Framework
  Copyright (C) 2006 Daniel Molina Wegener <dmw@coder.cl>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301 USA
*/
#ifndef lint
static char Id[] = "$Id$";
#endif /* !lint */

#ifdef HAVE_CONFIG_H
#include "caf/config.h"
#endif /* !HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "caf/caf.h"
#include "caf/caf_data_mem.h"

#define IO_EVENT_USE_SELECT
#include "caf/caf_evt_nio.h"


io_evt_t *
caf_io_evt_new  (int fd, int type, int to) {
	io_evt_t *r = (io_evt_t *)NULL;
	if (fd > -1 && type > 0) {
		r = (io_evt_t *)xmalloc (IO_EVT_SZ);
		if (r != (io_evt_t *)NULL) {
			r->ev_mfd = fd;
			r->ev_type = type;
			r->ev_use = IO_EVENTS_SELECT;
			r->ev_sz = IO_EVENT_DATA_SELECT_SZ;
			r->ev_info = (io_evt_select_t *)xmalloc (r->ev_sz);
			r->ev_timeout = to;
			if (r->ev_info == (io_evt_select_t *)NULL ||
				(caf_io_evt_init (r)) != CAF_OK) {
				xfree (r);
				r = (io_evt_t *)NULL;
			}
		}
	}
	return r;
}


int
caf_io_evt_delete (io_evt_t *e) {
	if (e != (io_evt_t *)NULL) {
		if ((caf_io_evt_destroy (e)) == CAF_OK) {
			if (e->ev_info != (void *)NULL) {
				xfree (e->ev_info);
			}
			xfree (e);
			return CAF_OK;
		}
	}
	return CAF_ERROR;
}


int
caf_io_evt_init (io_evt_t *e) {
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			s->timeout.tv_sec = e->ev_timeout;
			s->timeout.tv_usec = 0;
			FD_ZERO (&(s->rd));
			FD_ZERO (&(s->wr));
			if (e->ev_type & EVT_IO_READ) {
				FD_SET (e->ev_mfd, &(s->rd));
			}
			if (e->ev_type & EVT_IO_WRITE) {
				FD_SET (e->ev_mfd, &(s->wr));
			}
			return CAF_OK;
		}
	}
	return CAF_ERROR;
}


int
caf_io_evt_reinit (io_evt_t *e) {
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			s->timeout.tv_sec = e->ev_timeout;
			s->timeout.tv_usec = 0;
			FD_ZERO (&(s->rd));
			FD_ZERO (&(s->wr));
			if (e->ev_type & EVT_IO_READ) {
				FD_SET (e->ev_mfd, &(s->rd));
			}
			if (e->ev_type & EVT_IO_WRITE) {
				FD_SET (e->ev_mfd, &(s->wr));
			}
			return CAF_OK;
		}
	}
	return CAF_ERROR;
}


int
caf_io_evt_add (io_evt_t *e, int ev) {
	return e != (io_evt_t *)NULL && ev > 0 ? CAF_ERROR : CAF_ERROR;
}


int
caf_io_evt_destroy (io_evt_t *e) {
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			if (e->ev_type & EVT_IO_READ) {
				FD_CLR (e->ev_mfd, &(s->rd));
			}
			if (e->ev_type & EVT_IO_WRITE) {
				FD_CLR (e->ev_mfd, &(s->wr));
			}
			FD_ZERO (&(s->rd));
			FD_ZERO (&(s->wr));
			return CAF_OK;
		}
	}
	return CAF_ERROR;
}


int
caf_io_evt_handle (io_evt_t *e) {
	int r = CAF_ERROR;
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			if ((select (e->ev_mfd, &(s->rd), &(s->wr), NULL, &(s->timeout))) >
				0) {
				return CAF_OK;
			}
		}
	}
	return r;
}


int
caf_io_evt_isread (io_evt_t *e) {
	int r = CAF_ERROR;
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			r = FD_ISSET(e->ev_mfd, &(s->wr)) ? CAF_OK : CAF_ERROR;
		}
	}
	return r;
}


int
caf_io_evt_iswrite (io_evt_t *e) {
	int r = CAF_ERROR;
	io_evt_select_t *s;
	if (e != (io_evt_t *)NULL) {
		s = (io_evt_select_t *)e->ev_info;
		if (s != (io_evt_select_t *)NULL) {
			r = FD_ISSET(e->ev_mfd, &(s->rd)) ? CAF_OK : CAF_ERROR;
		}
	}
	return r;
}

/* caf_evt_io_select.c ends here */

