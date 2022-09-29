/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Hexdumping packetview routines.
 */
#include <errno.h>
#include "datastream.h"
#include "packetview.h"

int
pv_showhex(PacketView *pv, unsigned char *bp, int off, int len)
{
	int rem;

	if (len <= 0)
		return 1;
	rem = pv->pv_stop - pv->pv_off;
	pv->pv_off += len;
	if (pv->pv_nullflag || rem <= 0)
		return 1;
	if (len > rem)
		len = rem;
	if (!pv_hexdump(pv, bp, off, len)) {
		pv_exception(pv, "cannot show hex data");
		return 0;
	}
	return 1;
}

int
pv_decodehex(PacketView *pv, DataStream *ds, int off, int len)
{
	unsigned char *bp;

	if (len <= 0)
		return 1;
	if (len > ds->ds_count)
		len = ds->ds_count;
	bp = (unsigned char *) ds_inline(ds, len, sizeof *bp);
	if (bp == 0) {
		pv->pv_error = EINVAL;
		pv_exception(pv, "cannot decode hex data");
		return 0;
	}
	return pv_showhex(pv, bp, off, len);
}
