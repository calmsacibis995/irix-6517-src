/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetSnoop service implementation
 *
 *	$Revision: 1.8 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/time.h>
#include <bstring.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include "expr.h"
#include "heap.h"
#include "macros.h"
#include "packetbuf.h"
#include "protocol.h"
#include "snoopd.h"
#include "snooper.h"

struct nsclient {
	struct filter	ns_filter;
	PacketBuf	ns_pb;
	u_int		ns_rawhdrsize;
	u_int		ns_unblock;
	u_int		ns_lastseq;
	u_int		ns_counted;
	u_int		ns_count;
	u_int		ns_interval;
	struct timeval	ns_endtime;
};

#define	sctons(sc)	((struct nsclient *)(sc)->sc_private)

#define UNBLOCK_TIME	(TICKS_PER_SEC / 10)
#define MAX_SEND_COUNT	10

void
ns_init(void)
{
}

void
ns_subscribe(SnoopClient *sc, u_int buffer, u_int count, u_int interval)
{
	struct nsclient *ns;
	Snooper *sn;

	ns = new(struct nsclient);
	bzero(&ns->ns_filter, sizeof ns->ns_filter);
	sn = sc->sc_if->if_snooper;
	ns->ns_rawhdrsize = sn->sn_rawhdrpad + sn->sn_rawproto->pr_maxhdrlen;
	pb_init(&ns->ns_pb, buffer, ns->ns_rawhdrsize + sc->sc_if->if_mtu);
	ns->ns_unblock = 0;
	ns->ns_count = count;
	ns->ns_counted = (count != 0);
	ns->ns_interval = interval;	/* XXX - Interval for use later */
	timerclear(&ns->ns_endtime);
	sc->sc_private = ns;
}

void
ns_unsubscribe(SnoopClient *sc)
{
	struct nsclient *ns;

	ns = sctons(sc);
	snoop_freefilter(sc->sc_if, &ns->ns_filter);
	pb_finish(&ns->ns_pb);
	delete(ns);
}

enum snoopstat
ns_add(SnoopClient *sc, Expr *ex, ExprError *err, int *bin)
{
	struct nsclient *ns;

	ns = sctons(sc);
	if (!snoop_update(sc->sc_if, ex, err, &ns->ns_filter))
		return SNOOPERR_BADF;
	*bin = 0;
	return SNOOP_OK;
}

/* ARGSUSED */
int
ns_delete(SnoopClient *sc, int bin)
{
	struct nsclient *ns;

	ns = sctons(sc);
	snoop_freefilter(sc->sc_if, &ns->ns_filter);
	return SNOOP_OK;
}

static int
ns_send(SnoopClient *sc, struct packet *p)
{
	SnoopPacketWrap spw;

	spw.spw_sp = &p->p_sp;
	spw.spw_len = &p->p_len;
	spw.spw_maxlen = p->p_len;

	return svc_sendreply(sc->sc_xprt, xdr_snooppacketwrap, &spw);
}

/* ARGSUSED */
int
ns_process(SnoopClient *sc, PacketBuf *pb, Packet *p)
{
	struct nsclient *ns;
	Packet *np;
	int maxlen;

	ns = sctons(sc);
	if (!snoop_test(sc->sc_if, sc->sc_errflags, &ns->ns_filter,
			&p->p_sp, p->p_len)) {
		return 0;
	}
	if (PB_FULL(&ns->ns_pb)) {
		np = ns->ns_pb.pb_busy.pbl_forw;
		if (!ns_send(sc, np)) {
			sc->sc_state = BLOCKED;
			ns->ns_unblock = UNBLOCK_TIME;
			sc->sc_sddrops++;
			(void) pb_put(&ns->ns_pb, np);
			return errno;
		}
		(void) pb_put(&ns->ns_pb, np);
	}

	/* XXX copy the current packet into np */
	np = pb_get(&ns->ns_pb);
	np->p_len = p->p_len;
	np->p_sp.sp_hdr = p->p_sp.sp_hdr;
	np->p_sp.sp_seq = sc->sc_seq++;
	maxlen = ns->ns_rawhdrsize + sc->sc_snooplen;
	bcopy(p->p_sp.sp_data, np->p_sp.sp_data, MIN(p->p_len, maxlen));
	if (ns->ns_counted && --ns->ns_count == 0) {
		struct snoopstats ss;

		sn_getstats(sc->sc_if->if_snooper, &ss);
		sc->sc_ifdrops = ss.ss_ifdrops - sc->sc_ifdrops;
		sc->sc_sbdrops = ss.ss_sbdrops - sc->sc_sbdrops;
	}
	return 0;
}

void
ns_timer(SnoopClient *sc)
{
	struct nsclient *ns;
	Packet *p;
	u_int sent;

	ns = sctons(sc);
	switch (sc->sc_state) {
	    case STARTED:
		if (sc->sc_seq != ns->ns_lastseq) {
			ns->ns_lastseq = sc->sc_seq;
			ns->ns_unblock = UNBLOCK_TIME;
			return;
		}
		break;

	    case BLOCKED:
		break;

	    default:
		return;
	}
	if (ns->ns_unblock != 0) {
		ns->ns_unblock--;
		return;
	}
	sc->sc_state = STARTED;
	sent = MAX_SEND_COUNT;
	p = ns->ns_pb.pb_busy.pbl_forw;
	while (p != (Packet *) &ns->ns_pb.pb_busy && sent != 0) {
		if (!ns_send(sc, p)) {
			sc->sc_state = BLOCKED;
			ns->ns_unblock = UNBLOCK_TIME;
			(void) pb_put(&ns->ns_pb, p);
			return;
		}
		p = pb_put(&ns->ns_pb, p);
		--sent;
	}
	if (sent == 0)
		ns->ns_unblock = 1;
}
