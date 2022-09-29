/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Packet buffer operations.
 */
#include <sys/types.h>
#include <net/raw.h>
#include "heap.h"
#include "macros.h"
#include "packetbuf.h"

#define	P_NULL(pbl) \
	((pbl)->pbl_forw = (pbl)->pbl_back = (Packet *)(pbl))
#define	P_APPEND(pbl, p) \
	((p)->p_forw = (Packet *)(pbl), \
	 (p)->p_back = (pbl)->pbl_back, \
	 (p)->p_forw->p_back = (p)->p_back->p_forw = (p))
#define	P_REMOVE(p) \
	((p)->p_forw->p_back = (p)->p_back, \
	 (p)->p_back->p_forw = (p)->p_forw, \
	 P_NULL(&(p)->p_pbl))

void
pb_init(PacketBuf *pb, int packets, int packetsize)
{
	char *p;

	if (packets <= 0)
		packets = 1;
	if (packetsize < 0)
		packetsize = 0;

	pb->pb_packets = packets;
	packetsize += structoff(packet, p_sp.sp_data[0]);
	packetsize = ROUNDUP(packetsize, RAW_ALIGNGRAIN);
	pb->pb_packetsize = packetsize;

	P_NULL(&pb->pb_busy);
	P_NULL(&pb->pb_free);
	pb->pb_base = vnew(packets * packetsize, char);
	for (p = pb->pb_base; --packets >= 0; p += packetsize)
		P_APPEND(&pb->pb_free, (Packet *)p);
}

void
pb_finish(PacketBuf *pb)
{
	delete(pb->pb_base);
}

Packet *
pb_get(PacketBuf *pb)
{
	Packet *p;

	p = pb->pb_free.pbl_forw;
	if (p == (Packet *) &pb->pb_free)
		p = pb->pb_busy.pbl_forw;
	P_REMOVE(p);
	P_APPEND(&pb->pb_busy, p);
	return p;
}

Packet *
pb_put(PacketBuf *pb, Packet *p)
{
	Packet *forw = p->p_forw;

	P_REMOVE(p);
	P_APPEND(&pb->pb_free, p);
	return forw;
}

void
pb_flush(PacketBuf *pb)
{
	struct pblinks *pbl;
	Packet *p;

	pbl = &pb->pb_busy;
	while ((p = (Packet *)pbl->pbl_forw) != (Packet *)pbl) {
		P_REMOVE(p);
		P_APPEND(&pb->pb_free, p);
	}
}

void
pb_scan(PacketBuf *pb, int (*fun)(), void *arg)
{
	Packet *p;

	p = pb->pb_busy.pbl_forw;
	while (p != (Packet *) &pb->pb_busy) {
		/*
		 * Advance p before calling fun, so that fun can
		 * modify p->p_forw.
		 */
		p = p->p_forw;
		(*fun)(pb, p->p_back, arg);
	}
}
