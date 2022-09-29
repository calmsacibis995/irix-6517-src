/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Protocol viewing suppression.
 */
#include <errno.h>
#include <string.h>
#include "packetview.h"
#include "protocol.h"

int	scanprotolist(PacketView *, char *, void (*)(), int);

int
pv_nullify(PacketView *pv, char *proto)
{
	return scanprotolist(pv, proto, (void (*)())in_enter, PV_NULLIFY);
}

int
pv_hexify(PacketView *pv, char *proto)
{
	return scanprotolist(pv, proto, (void (*)())in_enter, PV_HEXIFY);
}

int
pv_reify(PacketView *pv, char *proto)
{
	return scanprotolist(pv, proto, in_remove, 0);
}

static int
scanprotolist(PacketView *pv, char *proto, void (*indexfun)(), int nullflag)
{
	char *nextproto;
	int protolen;
	Protocol *pr;

	for (;;) {
		nextproto = strchr(proto, ',');
		if (nextproto == 0)
			protolen = strlen(proto);
		else {
			protolen = nextproto - proto;
			*nextproto++ = '\0';
		}
		pr = findprotobyname(proto, protolen);
		if (pr == 0) {
			pv->pv_error = EINVAL;
			pv_exception(pv, "unknown protocol %s", proto);
			return 0;
		}
		(*indexfun)(&pv->pv_nullprotos, &pr->pr_id, (void *) nullflag);
		if (nextproto == 0)
			break;
		proto = nextproto;
	}
	return 1;
}
