/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Packet field viewing operations and utilities.
 * NB: _char_image depends on consecutive alphabetic codes such as those
 *     found in ASCII and ISO-Latin-1.
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <values.h>
#include "packetview.h"
#include "protocol.h"
#include "strings.h"

char *
_char_image(char c)
{
	static char buf[4];

	if (isprint(c))
		buf[0] = c, buf[1] = '\0';
	else if (iscntrl(c))
		buf[0] = '^', buf[1] = 'A' + c - 1, buf[2] = '\0';
	else
		(void) sprintf(buf, "%o", c);
	return buf;
}

/*
 * This function expects pv_off to indicate the field's packet offset.
 */
int	showfield(PacketView *, ProtoField *, void *, int, char *, va_list);

int
pv_showfield(PacketView *pv, ProtoField *pf, void *base, char *format, ...)
{
	va_list ap;
	int ok;

	if (pf->pf_size == PF_VARIABLE) {
		pv->pv_error = EINVAL;
		pv_exception(pv, "cannot show variable field %s", pf->pf_name);
		return 0;
	}
	if (pf->pf_size > 0 && pv->pv_bitoff != 0) {
		pv->pv_error = EINVAL;
		pv_exception(pv, "byte field %s on bit boundary", pf->pf_name);
		return 0;
	}
	va_start(ap, format);
	ok = showfield(pv, pf, base, pf->pf_size, format, ap);
	va_end(ap);
	if (pf->pf_size > 0) {
		pv->pv_off += pf->pf_size;
	} else {
		pv->pv_bitoff -= pf->pf_size;
		pv->pv_off += pv->pv_bitoff / BITSPERBYTE;
		pv->pv_bitoff %= BITSPERBYTE;
	}
	return ok;
}

int
pv_showvarfield(PacketView *pv, ProtoField *pf, void *base, int size,
		char *format, ...)
{
	va_list ap;
	int ok;

	if (pf->pf_size != PF_VARIABLE || size < 0) {
		pv->pv_error = EINVAL;
		pv_exception(pv, "cannot show fixed field %s", pf->pf_name);
		return 0;
	}
	va_start(ap, format);
	ok = showfield(pv, pf, base, size, format, ap);
	va_end(ap);
	pv->pv_off += size;
	return ok;
}

int
pv_replayfield(PacketView *pv, ProtoField *pf, void *base, int poff, int size,
	       char *format, ...)
{
	int save, ok;
	va_list ap;

	save = pv->pv_off;
	pv->pv_off = poff;
	va_start(ap, format);
	ok = showfield(pv, pf, base, size, format, ap);
	va_end(ap);
	pv->pv_off = save;
	return ok;
}

static int
showfield(PacketView *pv, ProtoField *pf, void *base, int size,
	  char *format, va_list ap)
{
	int level, cc;
	char buf[BUFSIZ];

	if (pv->pv_nullflag)
		return 1;
	level = pf->pf_level;

	if (format[0] == '@') {
		switch (format[1]) {
		  case 'd':
			level = PV_DEFAULT;
			break;
		  case 't':
			level = PV_TERSE;
			break;
		  case 'v':
			level = PV_VERBOSE;
			break;
		  default:
			goto checklevel;
		}
		format += 2;
	}
checklevel:
	if (level > pv->pv_level)
		return 1;

	cc = vnsprintf(buf, sizeof buf, format, ap);
	if (pv->pv_off >= pv->pv_stop)
		memset(buf, '?', cc);
	if (!pv_field(pv, base, size, pf, buf, cc)) {
		pv_exception(pv, pf->pf_name);
		return 0;
	}
	return 1;
}
