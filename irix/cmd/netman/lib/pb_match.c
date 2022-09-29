/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop expression filter matching for packetbufs.
 */
#include "packetbuf.h"
#include "snooper.h"

void
pb_match(PacketBuf *pb, Snooper *sn)
{
	MatchArgs ma;

	MA_INIT(&ma, SEQ_MATCH, sn);
	pb_scan(pb, pb_matchpacket, &ma);
}

int
pb_matchpacket(PacketBuf *pb, Packet *p, void *arg)
{
	MatchArgs *ma;
	unsigned long rawseq;

	ma = arg;
	if (p->p_sp.sp_seq != ma->ma_rawseq) {
		if (ma->ma_seq)
			ma->ma_seq += p->p_sp.sp_seq - ma->ma_rawseq;
		ma->ma_rawseq = p->p_sp.sp_seq;
	}
	rawseq = ma->ma_rawseq++;
	p->p_sp.sp_seq = (ma->ma_seqflag == SEQ_RAW) ? rawseq : ma->ma_seq;
	if (!sn_match(ma->ma_snoop, &p->p_sp, p->p_len)) {
		(void) pb_put(pb, p);
		return 0;
	}
	ma->ma_seq++;
	return 1;
}
