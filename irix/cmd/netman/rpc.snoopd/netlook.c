/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetLook service implementation
 *
 *	$Revision: 1.10 $
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

#include <bstring.h>
#include <errno.h>
#include <syslog.h>
#include "expr.h"
#include "heap.h"
#include "macros.h"
#include "netlook.h"
#include "packetbuf.h"
#include "packetview.h"
#include "protocol.h"
#include "snoopd.h"
#include "snooper.h"

extern PacketView *netlook_packetview(void);
extern void nlpv_setnlspacket(PacketView *, struct nlspacket *);

static PacketView *nlpacketview;
static struct nlspacket *nlsp;

struct nlclient {
	struct filter	nl_filter;
	u_int		nl_buffer;
	u_int		nl_interval;
	u_int		nl_remaining;
	struct nlpacket	nl_packet;
};

#define	sctonl(sc)	((struct nlclient *)(sc)->sc_private)
#define UNBLOCK_TIME	(TICKS_PER_SEC / 4)

void
nl_init(void)
{
	nlpacketview = netlook_packetview();
}

void
nl_subscribe(SnoopClient *sc, u_int buffer, u_int count, u_int interval)
{
	struct nlclient *nl;

	nl = new(struct nlclient);
	bzero(&nl->nl_filter, sizeof nl->nl_filter);
	nl->nl_interval = nl->nl_remaining = interval;
	nl->nl_buffer = buffer == 0 ? NLP_MAXSPACKETS : buffer;
	nl->nl_packet.nlp_snoophost = sc->sc_if->if_addr.s_addr;
	nl->nl_packet.nlp_version = NLP_VERSION;
	nl->nl_packet.nlp_type = NLP_SNOOPDATA;
	nl->nl_packet.nlp_count = 0;
	sc->sc_private = nl;
}

void
nl_unsubscribe(SnoopClient *sc)
{
	struct nlclient *nl;

	nl = sctonl(sc);
	snoop_freefilter(sc->sc_if, &nl->nl_filter);
	delete(nl);
}

enum snoopstat
nl_add(SnoopClient *sc, Expr *ex, ExprError *err, int *bin)
{
	struct nlclient *nl;

	nl = sctonl(sc);
	if (!snoop_update(sc->sc_if, ex, err, &nl->nl_filter))
		return SNOOPERR_BADF;
	*bin = 0;
	nl->nl_packet.nlp_count = 0;
	return SNOOP_OK;
}

/* ARGSUSED */
enum snoopstat
nl_delete(SnoopClient *sc, int bin)
{
	struct nlclient *nl;

	nl = sctonl(sc);
	snoop_freefilter(sc->sc_if, &nl->nl_filter);
	nl->nl_packet.nlp_count = 0;
	return SNOOP_OK;
}

static int
nl_send(SnoopClient *sc)
{
	struct nlclient *nl;

	nl = sctonl(sc);
	if (!svc_sendreply(sc->sc_xprt, xdr_nlpacket, &nl->nl_packet))
		return errno;
	return 0;
}

void
nl_preprocess(void)
{
	nlsp = 0;
}

/* ARGSUSED */
int
nl_process(SnoopClient *sc, PacketBuf *pb, Packet *p)
{
	struct nlclient *nl;
	struct nlspacket *newsp;

	nl = sctonl(sc);

	/* Check for EOF */
	if (p->p_len == 0) {
		nl->nl_packet.nlp_type = NLP_ENDOFDATA;
		if (nl_send(sc) != 0) {
			nl->nl_interval = UNBLOCK_TIME;
			sc->sc_state = BLOCKED;
			sc->sc_sddrops++;
		}
		nl->nl_packet.nlp_count = 0;
		return 0;
	}

	if (!snoop_test(sc->sc_if, sc->sc_errflags, &nl->nl_filter,
			&p->p_sp, p->p_len)) {
		return 0;
	}

	newsp = &nl->nl_packet.nlp_nlsp[nl->nl_packet.nlp_count];
	if (nlsp == 0) {
		nlsp = newsp;
		nlpv_setnlspacket(nlpacketview, nlsp);
		p_decode(p, sc->sc_if->if_snooper, HEXDUMP_NONE, nlpacketview);
	} else
		bcopy(nlsp, newsp, sizeof *newsp);

	if (++nl->nl_packet.nlp_count == nl->nl_buffer) {
		if (nl_send(sc) != 0) {
			nl->nl_remaining = UNBLOCK_TIME;
			sc->sc_state = BLOCKED;
			sc->sc_sddrops++;
		}
		nl->nl_packet.nlp_count = 0;
	}
	return 0;
}

void
nl_timer(SnoopClient *sc)
{
	struct nlclient *nl;

	nl = sctonl(sc);
	if (--nl->nl_remaining != 0)
		return;
	if (sc->sc_state == BLOCKED)
		sc->sc_state = STARTED;
	if (nl->nl_packet.nlp_count != 0
	    || nl->nl_packet.nlp_type == NLP_ENDOFDATA) {
		if (nl_send(sc) != 0) {
			/* Still blocked */
			nl->nl_remaining = UNBLOCK_TIME;
			sc->sc_state = BLOCKED;
			sc->sc_sddrops++;
			nl->nl_packet.nlp_count = 0;
			return;
		}
		nl->nl_packet.nlp_count = 0;
	}
	nl->nl_remaining = nl->nl_interval;
}
