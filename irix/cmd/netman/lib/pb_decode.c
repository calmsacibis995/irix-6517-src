/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * PacketBuf and Packet decoders.
 */
#include "datastream.h"
#include "packetbuf.h"
#include "packetview.h"
#include "protocol.h"
#include "snooper.h"

void
pb_decode(PacketBuf *pb, Snooper *sn, int hex, PacketView *pv)
{
	DecodeArgs da;

	DA_INIT(&da, sn, hex, pv);
	pb_scan(pb, pb_decodepacket, &da);
}

/* ARGSUSED */
int
pb_decodepacket(PacketBuf *pb, Packet *p, void *arg)
{
	DecodeArgs *da = arg;

	return p_decode(p, da->da_snoop, da->da_hex, da->da_view);
}

int
p_decode(Packet *p, Snooper *sn, int hex, PacketView *pv)
{
	Protocol *rawproto;
	DataStream ds;

	rawproto = sn->sn_rawproto;
	ds_init(&ds, p->p_sp.sp_data + sn->sn_rawhdrpad,
		p->p_len - sn->sn_rawhdrpad, DS_DECODE,
		rawproto ? rawproto->pr_byteorder : DS_BIG_ENDIAN);
	return pv_decodepacket(pv, sn, &p->p_sp.sp_hdr, hex, &ds);
}
