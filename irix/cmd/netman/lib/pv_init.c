/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * PacketView initializer and finalizer, used by subclass constructors
 * and destructors.
 */
#include <stdarg.h>
#include <stdio.h>
#include "exception.h"
#include "packetview.h"
#include "strings.h"

void
pv_init(PacketView *pv, void *private, struct pvops *ops, char *name,
	int level)
{
	pv->pv_private = private;
	pv->pv_ops = ops;
	pv->pv_name = name;
	pv->pv_error = 0;
	pv->pv_level = level;
	pv->pv_nullflag = 0;
	in_init(&pv->pv_nullprotos, 3, sizeof(int), 0);
	pv->pv_off = pv->pv_bitoff = pv->pv_stop = 0;
	pv->pv_hexbase = 0;
	pv->pv_hexoff = pv->pv_hexcount = 0;
}

void
pv_finish(PacketView *pv)
{
	in_finish(&pv->pv_nullprotos);
}

void
pv_exception(PacketView *pv, char *format, ...)
{
	va_list ap;
	int cc;
	char buf[BUFSIZ];

	if (format == 0) {
		exc_raise(pv->pv_error, pv->pv_name);
		return;
	}
	cc = nsprintf(buf, sizeof buf, "%s: ", pv->pv_name);
	va_start(ap, format);
	(void) vnsprintf(&buf[cc], sizeof buf - cc, format, ap);
	va_end(ap);
	exc_raise(pv->pv_error, buf);
}
