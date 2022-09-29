/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * PacketView printf.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "packetview.h"
#include "strings.h"

int
pv_printf(PacketView *pv, char *format, ...)
{
	va_list ap;
	int cc, len;
	char buf[BUFSIZ];
	char *bp, *ep;

	if (pv->pv_nullflag)
		return 0;
	va_start(ap, format);
	cc = vnsprintf(buf, sizeof buf, format, ap);
	va_end(ap);
	bp = buf;
	for (;;) {
		ep = strchr(bp, '\n');
		if (ep == 0)
			len = strlen(bp);
		else {
			len = ep - bp;
			*ep++ = '\0';
		}
		if (*bp != '\0' && !pv_text(pv, bp, len))
			goto bad;
		if (ep == 0)
			break;
		if (!pv_newline(pv))
			goto bad;
		if (*ep == '\0')
			break;
		bp = ep;
	}
	return cc;
bad:
	pv_exception(pv, "printf");
	return -1;
}
