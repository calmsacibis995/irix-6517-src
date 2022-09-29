/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop protocol structure xdr routines.
 */
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "snooper.h"

int
xdr_snoopheader(XDR *xdr, SnoopHeader *sh)
{
	return xdr_u_long(xdr, &sh->snoop_seq)
	    && xdr_u_short(xdr, &sh->snoop_flags)
	    && xdr_u_short(xdr, &sh->snoop_packetlen)
	    && xdr_long(xdr, &sh->snoop_timestamp.tv_sec)
	    && xdr_long(xdr, &sh->snoop_timestamp.tv_usec);
}

int
xdr_snooppacket(XDR *xdr, SnoopPacket *sp, u_int *len, u_int maxlen)
{
	u_int cnt, pos;

	if (!xdr_snoopheader(xdr, &sp->sp_hdr) || !xdr_u_int(xdr, len))
		return FALSE;
	cnt = *len;
	if (cnt > maxlen) {
		pos = xdr_getpos(xdr) + RNDUP(cnt);
		*len = maxlen;
	} else
		pos = 0;
	if (!xdr_opaque(xdr, sp->sp_data, *len))
		return FALSE;
	if (pos != 0 && !xdr_setpos(xdr, pos))
		return FALSE;
	return TRUE;
}
