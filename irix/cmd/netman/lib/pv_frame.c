/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Packet and protocol frame viewing ops.
 */
#include <time.h>
#include <sys/types.h>
#include <net/raw.h>
#include "datastream.h"
#include "packetview.h"
#include "protocol.h"
#include "protostack.h"
#include "snooper.h"

int	decodeframe(PacketView *, Protocol *, DataStream *, ProtoStack *);
void	hexify(PacketView *, DataStream *);

int
pv_decodepacket(PacketView *pv, Snooper *sn, SnoopHeader *sh, int how,
		DataStream *ds)
{
	ProtoStack ps;
	extern long _now;

	pv->pv_off = pv->pv_bitoff = 0;
	pv->pv_stop = ds->ds_size;
	pv->pv_hexbase = 0;
	_now = sh->snoop_timestamp.tv_sec;
	if (!pv_head(pv, sh, localtime(&sh->snoop_timestamp.tv_sec))) {
		pv_exception(pv, "packet head viewing error");
		return 0;
	}
	if (how == HEXDUMP_PACKET || sn->sn_rawproto == 0) {
		if (!pv_decodehex(pv, ds, 0, ds->ds_count))
			return 0;
	} else {
		PS_INIT(&ps, sn, sh);
		if (!decodeframe(pv, sn->sn_rawproto, ds, &ps))
			return 0;
		if (how == HEXDUMP_EXTRA) {
			ds->ds_bitoff = 0;
			(void) pv_decodehex(pv, ds, DS_TELL(ds), ds->ds_count);
		}
	}
	if (!pv_tail(pv)) {
		pv_exception(pv, "packet tail viewing error");
		return 0;
	}
	return 1;
}

void
pv_decodeframe(PacketView *pv, Protocol *pr, DataStream *ds, ProtoStack *ps)
{
	enum dsbyteorder saveorder;

	if (pr == 0 || ds->ds_count == 0)
		return;
	saveorder = ds_setbyteorder(ds, pr->pr_byteorder);
	(void) decodeframe(pv, pr, ds, ps);
	(void) ds_setbyteorder(ds, saveorder);
}

static int
decodeframe(PacketView *pv, Protocol *pr, DataStream *ds, ProtoStack *ps)
{
	int saveflag;

	if (pv->pv_hexbase)
		hexify(pv, ds);

	saveflag = pv->pv_nullflag;
	pv->pv_nullflag = (in_match(&pv->pv_nullprotos, &pr->pr_id)
			   ? PV_NULLIFY
			   : 0);
	if (!pv->pv_nullflag && pv->pv_level < pr->pr_level)
		pv->pv_nullflag = PV_NULLIFY;

	if (pv->pv_nullflag != PV_NULLIFY)
		(void) pv_push(pv, pr, pr->pr_name, pr->pr_namlen,
			       pr->pr_title);
	if (pv->pv_nullflag == PV_HEXIFY) {
		pv->pv_hexbase = ds->ds_next;
		pv->pv_hexoff = pv->pv_off;
		pv->pv_hexcount = ds->ds_count;
	}

	pr_decode(pr, ds, ps, pv);
	if (pv->pv_hexbase)
		hexify(pv, ds);

	if (pv->pv_nullflag != PV_NULLIFY)
		(void) pv_pop(pv);
	if (saveflag == PV_HEXIFY) {
		pv->pv_hexbase = ds->ds_next;
		pv->pv_hexoff = pv->pv_off;
		pv->pv_hexcount = ds->ds_count;
	}

	pr->pr_flags &= ~PR_DECODESTALE;
	pv->pv_nullflag = saveflag;
	if (pv->pv_error) {
		pv_exception(pv, "packet frame viewing error");
		return 0;
	}
	return 1;
}

static void
hexify(PacketView *pv, DataStream *ds)
{
	int len;

	len = pv->pv_hexcount - ds->ds_count;
	if (len <= 0)
		return;
	(void) pv_hexdump(pv, pv->pv_hexbase, pv->pv_hexoff, len);
	pv->pv_hexbase = 0;
}
