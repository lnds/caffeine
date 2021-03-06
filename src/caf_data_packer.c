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
#include <arpa/inet.h>
#include <netinet/in.h>

#include "caf/caf.h"
#include "caf/caf_data_mem.h"
#include "caf/caf_data_buffer.h"
#include "caf/caf_data_packer.h"


static struct caf_unit_size_map_s {
	caf_unit_type_t type;
	size_t sz;
}

caf_unit_size_map_v[] = {
	{ CAF_UNIT_OCTET, sizeof(u_int8_t) },
	{ CAF_UNIT_WORD, sizeof(u_int16_t) },
	{ CAF_UNIT_DWORD, sizeof(u_int32_t) },
	{ CAF_UNIT_QWORD, sizeof(u_int64_t) }
};

static inline uint64_t
htonll(uint64_t x) {
	return ((((x) & 0xff00000000000000LL) >> 56) | \
			(((x) & 0x00ff000000000000LL) >> 40) | \
			(((x) & 0x0000ff0000000000LL) >> 24) | \
			(((x) & 0x000000ff00000000LL) >> 8)  | \
			(((x) & 0x00000000ff000000LL) << 8)  | \
			(((x) & 0x0000000000ff0000LL) << 24) | \
			(((x) & 0x000000000000ff00LL) << 40) | \
			(((x) & 0x00000000000000ffLL) << 56));
}

static inline uint64_t
ntohll(uint64_t x) {
	return ((((x) & 0x00000000000000ffLL) << 56) | \
			(((x) & 0x000000000000ff00LL) << 40) | \
			(((x) & 0x0000000000ff0000LL) << 24) | \
			(((x) & 0x00000000ff000000LL) << 8)  | \
			(((x) & 0x000000ff00000000LL) >> 8)  | \
			(((x) & 0x0000ff0000000000LL) >> 24) | \
			(((x) & 0x00ff000000000000LL) >> 40) | \
			(((x) & 0xff00000000000000LL) >> 56));
}

static size_t
caf_unit_get_size (caf_unit_type_t type) {
	int i;
	for (i = 0; i < (int)sizeof (caf_unit_size_map_v); i++) {
		if (type == caf_unit_size_map_v[i].type) {
			return caf_unit_size_map_v[i].sz;
		}
	}
	return 0;
}


caf_unit_t *
caf_unit_new (int id, caf_unit_type_t type, size_t length, void *u_start,
              void *u_end, size_t su_sz, size_t eu_sz) {
	caf_unit_t *r = (caf_unit_t *)NULL;
	if (id < 1 || length == 0) {
		return r;
	}
	switch (type) {
	case CAF_UNIT_OCTET:
	case CAF_UNIT_WORD:
	case CAF_UNIT_DWORD:
	case CAF_UNIT_QWORD:
	case CAF_UNIT_STRING:
	case CAF_UNIT_PSTRING:
		r = (caf_unit_t *)xmalloc (CAF_UNIT_SZ);
		if (r != (caf_unit_t *)NULL) {
			r->id = id;
			r->type = type;
			r->length = length;
			r->u_start = u_start;
			r->u_end = u_end;
			r->su_sz = su_sz;
			r->eu_sz = eu_sz;
		}
		break;
	default:
		break;
	}
	return r;
}


int
caf_unit_delete (caf_unit_t *r) {
	if (r != (caf_unit_t *)NULL) {
		xfree (r);
		return CAF_OK;
	}
	return CAF_ERROR;
}


caf_unit_value_t *
caf_unit_value_new (caf_unit_type_t type, size_t sz, void *data) {
	caf_unit_value_t *r = (caf_unit_value_t *)NULL;
	if (sz > 0 && data != (void *)NULL) {
		switch (type) {
		case CAF_UNIT_OCTET:
		case CAF_UNIT_WORD:
		case CAF_UNIT_DWORD:
		case CAF_UNIT_QWORD:
		case CAF_UNIT_STRING:
		case CAF_UNIT_PSTRING:
			r = (caf_unit_value_t *)xmalloc (CAF_UNIT_VALUE_SZ);
			if (r != (caf_unit_value_t *)NULL) {
				r->type = type;
				r->sz = sz;
				memcpy (r->data, data, sz);
			}
			break;
		}
	}
	return r;
}


int
caf_unit_value_delete (caf_unit_value_t *r) {
	if (r != (caf_unit_value_t *)NULL) {
		xfree (r->data);
		xfree (r);
		return CAF_OK;
	}
	return CAF_ERROR;
}


int
caf_unit_delete_callback (void *r) {
	return caf_unit_delete (r);
}


caf_pack_t *
caf_pack_new (int id, const char *name) {
	caf_pack_t *r = (caf_pack_t *)NULL;
	if (id > 0 && name != (const char *)NULL) {
		r = (caf_pack_t *)xmalloc (CAF_PACK_SZ);
		r->id = id;
		r->name = strdup(name);
		r->units = deque_create ();
		if (r->units == (deque_t *)NULL) {
			xfree (r);
			r = (caf_pack_t *)NULL;
		}
	}
	return r;
}


int
caf_pack_delete (caf_pack_t *r) {
	if (r != (caf_pack_t *)NULL) {
		if (r->name != (char *)NULL) {
			xfree (r->name);
		}
		if (r->units != (deque_t *)NULL) {
			deque_delete (r->units, caf_unit_delete_callback);
		}
		xfree (r);
		return CAF_OK;
	}
	return CAF_ERROR;
}


caf_packet_t *
caf_packet_new (int id, int pack_id, const char *pack_name) {
	caf_packet_t *r = (caf_packet_t *)NULL;
	if (id > 0 && pack_id > 0 && pack_name != (char *)NULL) {
		r = (caf_packet_t *)xmalloc (CAF_PACKET_SZ);
		if (r != (caf_packet_t *)NULL) {
			r->id = id;
			r->pack = caf_pack_new (pack_id, pack_name);
			if (r->pack == (caf_pack_t *)NULL) {
				xfree (r);
				r = (caf_packet_t *)NULL;
			} else {
				r->packets = deque_create ();
			}
		}
	}
	return r;
}


int
caf_packet_delete (caf_packet_t *r) {
	if (r != (caf_packet_t *)NULL) {
		caf_pack_delete (r->pack);
		deque_delete_nocb (r->packets);
		return CAF_OK;
	}
	return CAF_ERROR;
}


int
caf_packet_addunit (caf_packet_t *r, int id, caf_unit_type_t type,
                    size_t length) {
	caf_unit_t *u = (caf_unit_t *)NULL;
	if (r != (caf_packet_t *)NULL && type != CAF_UNIT_STRING) {
		if (r->pack != (caf_pack_t *)NULL) {
			if (r->pack->units != (deque_t *)NULL) {
				u = caf_unit_new (id, type, length, 0, 0, 0, 0);
				if (u != (caf_unit_t *)NULL) {
					deque_push (r->pack->units, u);
					return CAF_OK;
				}
			}
		}
	}
	return CAF_ERROR;
}


int
caf_packet_addunitstr (caf_packet_t *r, int id, size_t length, void *u_start,
                       void *u_end, size_t su_sz, size_t eu_sz) {
	caf_unit_t *u = (caf_unit_t *)NULL;
	if (r != (caf_packet_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL) {
			if (r->pack->units != (deque_t *)NULL) {
				u = caf_unit_new (id, CAF_UNIT_STRING, length, u_start, u_end,
				                  su_sz, eu_sz);
				if (u != (caf_unit_t *)NULL) {
					deque_push (r->pack->units, u);
					return CAF_OK;
				}
			}
		}
	}
	return CAF_ERROR;
}


int
caf_packet_addunitpstr (caf_packet_t *r, int id, size_t length,
						void *u_start) {
	caf_unit_t *u = (caf_unit_t *)NULL;
	if (r != (caf_packet_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL) {
			if (r->pack->units != (deque_t *)NULL) {
				u = caf_unit_new (id, CAF_UNIT_STRING, length, u_start,
				                  (void *)NULL, 0, 0);
				if (u != (caf_unit_t *)NULL) {
					deque_push (r->pack->units, u);
					return CAF_OK;
				}
			}
		}
	}
	return CAF_ERROR;
}


caf_unit_value_t *
caf_packet_getpstr (caf_unit_t *u, void *b, size_t p) {
	caf_unit_value_t *r = (caf_unit_value_t *)NULL;
	void *ptr = (void *)NULL;
	size_t sz = 0;
	if (u != (caf_unit_t *)NULL && b != (void *)NULL && p > 0 &&
		p < u->length) {
		if (u->type == CAF_UNIT_STRING) {
			switch (u->length) {
			case CAF_UNIT_OCTET_SZ:
				sz = ((size_t)*((caf_unit_octet_t *)(b))) & 0xff;
			case CAF_UNIT_WORD_SZ:
				sz = ((size_t)*((caf_unit_word_t *)(b))) & 0xffff;
			case CAF_UNIT_DWORD_SZ:
				sz = ((size_t)*((caf_unit_dword_t *)(b))) & 0xffffffff;
			case CAF_UNIT_QWORD_SZ:
				/* the is no packet with that size... ;) */
				sz = ((size_t)*((caf_unit_qword_t *)(b))) & 0xffffffff;
			}
			ptr = (void *)((size_t)p + u->length);
			r = caf_unit_value_new (CAF_UNIT_PSTRING, sz, ptr);
		}
	}
	return r;
}


caf_unit_value_t *
caf_packet_getstr (caf_unit_t *u, void *b, size_t p) {
	caf_unit_value_t *r = (caf_unit_value_t *)NULL;
	void *ptr = (void *)NULL;
	void *rp = (void *)NULL;
	size_t sz = 0;
	if (u != (caf_unit_t *)NULL && b != (void *)NULL && p > 0 &&
		p < u->length) {
		if (u->type == CAF_UNIT_STRING) {
			if ((memcmp (b, u->u_start, u->su_sz)) == 0) {
				ptr = (void *)((size_t)p + u->su_sz);
				rp = ptr;
				while ((memcmp (ptr, u->u_end, u->eu_sz)) != 0) {
					ptr = (void *)((size_t)ptr + 1);
					sz++;
				}
				r = caf_unit_value_new (CAF_UNIT_STRING, sz, rp);
			}
		}
	}
	return r;
}


size_t
caf_packet_getsize (caf_packet_t *r) {
	deque_t *l;
	caf_dequen_t *n;
	caf_unit_t *u;
	size_t sz = 0;
	if (r == (caf_packet_t *)NULL) {
		return 0;
	}
	if (r->pack == (caf_pack_t *)NULL) {
		return 0;
	}
	l = r->pack->units;
	if (l != (deque_t *)NULL) {
		n = l->head;
		while (n != (caf_dequen_t *)NULL) {
			u = (caf_unit_t *)n->data;
			sz += (u->su_sz + u->eu_sz + u->length);
			n = n->next;
		}
	}
	return sz;
}


int
caf_packet_parse (caf_packet_t *r, cbuffer_t *buf) {
	caf_dequen_t *n = (caf_dequen_t *)NULL;
	caf_unit_t *u = (caf_unit_t *)NULL;
	caf_unit_value_t *v = (caf_unit_value_t *)NULL;
	void *ptr = (void *)NULL;
	size_t pos = 0;
	uint16_t *c16;
	uint32_t *c32;
	uint64_t *c64;
	if (r != (caf_packet_t *)NULL && buf != (cbuffer_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL && r->packets != (deque_t *)NULL) {
			deque_delete_nocb (r->packets);
			r->packets = deque_create ();
			if (r->packets == (deque_t *)NULL) {
				return CAF_ERROR;
			}
			if (r->pack->units != (deque_t *)NULL) {
				n = r->pack->units->head;
				while (n != (caf_dequen_t *)NULL) {
					u = (caf_unit_t *)n->data;
					switch (u->type) {
					case CAF_UNIT_STRING:
						v = caf_packet_getstr (u, ptr, buf->sz - pos);
						break;
					case CAF_UNIT_PSTRING:
						v = caf_packet_getpstr (u, ptr, u->length);
						break;
					case CAF_UNIT_WORD_SZ:
						c16 = (uint16_t *)ptr;
						*c16 = ntohs (*c16);
						v = caf_unit_value_new (u->type, u->length, c16);
						break;
					case CAF_UNIT_DWORD_SZ:
						c32 = (uint32_t *)ptr;
						*c32 = ntohl (*c32);
						v = caf_unit_value_new (u->type, u->length, c32);
						break;
					case CAF_UNIT_QWORD_SZ:
						c64 = (uint64_t *)ptr;
						*c64 = ntohll (*c64);
						v = caf_unit_value_new (u->type, u->length, c64);
						break;
					default:
						v = caf_unit_value_new (u->type, u->length,
						                        (void *)ptr);
						break;
					}
					if (v != (caf_unit_value_t *)NULL) {
						deque_push (r->packets, v);
						ptr = (void *)((size_t)ptr +
						               caf_unit_get_size (u->type));
					}
					pos += v->sz;
					n = n->next;
				}
			}
		}
	}
	return CAF_ERROR;
}


int
caf_packet_parse_machine (caf_packet_t *r, cbuffer_t *buf) {
	caf_dequen_t *n = (caf_dequen_t *)NULL;
	caf_unit_t *u = (caf_unit_t *)NULL;
	caf_unit_value_t *v = (caf_unit_value_t *)NULL;
	void *ptr = (void *)NULL;
	size_t pos = 0;
	if (r != (caf_packet_t *)NULL && buf != (cbuffer_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL && r->packets != (deque_t *)NULL) {
			deque_delete_nocb (r->packets);
			r->packets = deque_create ();
			if (r->packets == (deque_t *)NULL) {
				return CAF_ERROR;
			}
			if (r->pack->units != (deque_t *)NULL) {
				n = r->pack->units->head;
				while (n != (caf_dequen_t *)NULL) {
					u = (caf_unit_t *)n->data;
					switch (u->type) {
					case CAF_UNIT_STRING:
						v = caf_packet_getstr (u, ptr, buf->sz - pos);
						break;
					case CAF_UNIT_PSTRING:
						v = caf_packet_getpstr (u, ptr, u->length);
						break;
					case CAF_UNIT_WORD_SZ:
						v = caf_unit_value_new (u->type, u->length, ptr);
						break;
					case CAF_UNIT_DWORD_SZ:
						v = caf_unit_value_new (u->type, u->length, ptr);
						break;
					case CAF_UNIT_QWORD_SZ:
						v = caf_unit_value_new (u->type, u->length, ptr);
						break;
					default:
						v = caf_unit_value_new (u->type, u->length,
						                        (void *)ptr);
						break;
					}
					if (v != (caf_unit_value_t *)NULL) {
						deque_push (r->packets, v);
						ptr = (void *)((size_t)ptr +
						               caf_unit_get_size (u->type));
					}
					pos += v->sz;
					n = n->next;
				}
			}
		}
	}
	return CAF_ERROR;
}


cbuffer_t *
caf_packet_translate (caf_packet_t *r) {
	caf_dequen_t *un = (caf_dequen_t *)NULL;
	caf_unit_value_t *u = (caf_unit_value_t *)NULL;
	cbuffer_t *buf = (cbuffer_t *)NULL;
	size_t bsz = 0, pos = 0;
	uint16_t c16;
	uint32_t c32;
	uint64_t c64;
	if (r != (caf_packet_t *)NULL && buf != (cbuffer_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL && r->packets != (deque_t *)NULL) {
			if (r->pack->units != (deque_t *)NULL) {
				un = r->pack->units->head;
				while (un != (caf_dequen_t *)NULL) {
					bsz += ((caf_unit_t *)un->data)->length;
					un = un->next;
				}
				if (bsz > 0) {
					buf = cbuf_create (bsz);
					un = r->packets->head;
					pos = (size_t)buf->data;
					while (un != (caf_dequen_t *)NULL) {
						u = (caf_unit_value_t *)un->data;
						switch (u->type) {
						case CAF_UNIT_STRING:
							memcpy ((void *)pos, u->data, u->sz);
							break;
						case CAF_UNIT_PSTRING:
							memcpy ((void *)pos, u->data, u->sz);
							break;
						case CAF_UNIT_WORD_SZ:
							c16 = *((uint16_t *)u->data);
							c16 = ntohs(c16);
							memcpy ((void *)(pos), &c16, u->sz);
							break;
						case CAF_UNIT_DWORD_SZ:
							c32 = *((uint32_t *)u->data);
							c32 = ntohl(c32);
							memcpy ((void *)(pos), &c32, u->sz);
							break;
						case CAF_UNIT_QWORD_SZ:
							c64 = *((uint64_t *)u->data);
							c64 = ntohll(c64);
							memcpy ((void *)(pos), &c64, u->sz);
							break;
						default:
							memcpy ((void *)(pos), u->data, u->sz);
							break;
						}
						pos += u->sz;
						un = un->next;
					}
				}
			}
		}
	}
	return buf;
}


cbuffer_t *
caf_packet_translate_machine (caf_packet_t *r) {
	caf_dequen_t *un = (caf_dequen_t *)NULL;
	caf_unit_value_t *u = (caf_unit_value_t *)NULL;
	cbuffer_t *buf = (cbuffer_t *)NULL;
	size_t bsz = 0, pos = 0;
	if (r != (caf_packet_t *)NULL && buf != (cbuffer_t *)NULL) {
		if (r->pack != (caf_pack_t *)NULL && r->packets != (deque_t *)NULL) {
			if (r->pack->units != (deque_t *)NULL) {
				un = r->pack->units->head;
				while (un != (caf_dequen_t *)NULL) {
					bsz += ((caf_unit_t *)un->data)->length;
					un = un->next;
				}
				if (bsz > 0) {
					buf = cbuf_create (bsz);
					un = r->packets->head;
					pos = (size_t)buf->data;
					while (un != (caf_dequen_t *)NULL) {
						u = (caf_unit_value_t *)un->data;
						switch (u->type) {
						case CAF_UNIT_STRING:
							memcpy ((void *)pos, u->data, u->sz);
							break;
						case CAF_UNIT_PSTRING:
							memcpy ((void *)pos, u->data, u->sz);
							break;
						case CAF_UNIT_WORD_SZ:
							memcpy ((void *)(pos), u->data, u->sz);
							break;
						case CAF_UNIT_DWORD_SZ:
							memcpy ((void *)(pos), u->data, u->sz);
							break;
						case CAF_UNIT_QWORD_SZ:
							memcpy ((void *)(pos), u->data, u->sz);
							break;
						default:
							memcpy ((void *)(pos), u->data, u->sz);
							break;
						}
						pos += u->sz;
						un = un->next;
					}
				}
			}
		}
	}
	return buf;
}


/* caf_data_packer.c ends here */

