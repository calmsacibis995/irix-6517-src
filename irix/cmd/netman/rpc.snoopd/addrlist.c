/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	AddrList service implementation
 *
 *	$Revision: 1.3 $
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
#include "addrlist.h"
#include "packetbuf.h"
#include "packetview.h"
#include "protocol.h"
#include "snoopd.h"
#include "snooper.h"

extern PacketView *addrlist_packetview(struct alentry *, struct timeval *);

static PacketView *alpacketview;
static struct alentry entry;
static struct timeval timestamp;
static unsigned int validentry;

struct alclient {
	struct filter	al_filter;
	u_int		al_buffer;
	u_int		al_interval;
	u_int		al_remaining;
	u_int		al_sendflag;
	struct alpacket	al_packet;
};

#define	sctoal(sc)	((struct alclient *)(sc)->sc_private)
#define UNBLOCK_TIME	(TICKS_PER_SEC / 2)

void
al_init(void)
{
	alpacketview = addrlist_packetview(&entry, &timestamp);
}

void
al_subscribe(SnoopClient *sc, u_int buffer, u_int count, u_int interval)
{
	struct alclient *al;

	al = new(struct alclient);
	bzero(&al->al_filter, sizeof al->al_filter);
	al->al_interval = al->al_remaining = interval;
	al->al_buffer = buffer == 0 ? AL_MAXENTRIES : buffer;
	al->al_sendflag = 0;
	al->al_packet.al_source = sc->sc_if->if_addr.s_addr;
	al->al_packet.al_version = AL_VERSION;
	al->al_packet.al_type = AL_DATATYPE;
	al->al_packet.al_nentries = 0;
	sc->sc_private = al;
}

void
al_unsubscribe(SnoopClient *sc)
{
	struct alclient *al;

	al = sctoal(sc);
	snoop_freefilter(sc->sc_if, &al->al_filter);
	delete(al);
}

enum snoopstat
al_add(SnoopClient *sc, Expr *ex, ExprError *err, int *bin)
{
	struct alclient *al;

	al = sctoal(sc);
	if (!snoop_update(sc->sc_if, ex, err, &al->al_filter))
		return SNOOPERR_BADF;
	*bin = 0;
	al->al_packet.al_nentries = 0;
	return SNOOP_OK;
}

/* ARGSUSED */
enum snoopstat
al_delete(SnoopClient *sc, int bin)
{
	struct alclient *al;

	al = sctoal(sc);
	snoop_freefilter(sc->sc_if, &al->al_filter);
	al->al_packet.al_nentries = 0;
	return SNOOP_OK;
}

static int
al_send(SnoopClient *sc)
{
	struct alclient *al;

	al = sctoal(sc);
	if (!svc_sendreply(sc->sc_xprt, xdr_alpacket, &al->al_packet))
		return errno;
	return 0;
}

void
al_preprocess(void)
{
	validentry = 0;
}

/* ARGSUSED */
int
al_process(SnoopClient *sc, PacketBuf *pb, Packet *p)
{
	struct alclient *al;
	struct alspacket *newsp;
	int i;

	al = sctoal(sc);
	if (!snoop_test(sc->sc_if, sc->sc_errflags, &al->al_filter,
			&p->p_sp, p->p_len)) {
		return 0;
	}

        /* Check for EOF */
        if (p->p_len == 0) {
		al->al_packet.al_type = AL_ENDOFDATA;
		if (al_send(sc) != 0) {
			al->al_interval = UNBLOCK_TIME;
                        sc->sc_state = BLOCKED;
                        sc->sc_sddrops++;
                }
                al->al_packet.al_nentries = 0;
                return 0;
        }

	/* Decode the packet once */
	if (validentry == 0) {
		p_decode(p, sc->sc_if->if_snooper, HEXDUMP_NONE, alpacketview);
		validentry = 1;
	}

	/* Copy the timestamp */
	al->al_packet.al_timestamp = timestamp;

	/* See if this entry exists */
	for (i = al->al_packet.al_nentries - 1; i >= 0; --i) {
		if (bcmp(&entry, &al->al_packet.al_entry[i],
			 sizeof(entry) - sizeof(struct counts)) == 0) {
			/* XXX - same as in histogram */
			al->al_packet.al_entry[i].ale_count.c_packets++;
			al->al_packet.al_entry[i].ale_count.c_bytes +=
				entry.ale_count.c_bytes;
			break;
		}
	}
	if (i < 0) {
		/* New entry */
		bcopy(&entry,
		      &al->al_packet.al_entry[al->al_packet.al_nentries++],
		      sizeof entry);
	}

	if (al->al_packet.al_nentries == al->al_buffer) {
		if (al_send(sc) != 0) {
			al->al_remaining = UNBLOCK_TIME;
			sc->sc_state = BLOCKED;
			sc->sc_sddrops++;
		}
		al->al_sendflag = 0;
		al->al_packet.al_nentries = 0;
	}
	return 0;
}

void
al_timer(SnoopClient *sc)
{
	struct alclient *al;

	al = sctoal(sc);
	if (--al->al_remaining != 0)
		return;
	if (sc->sc_state == BLOCKED)
		sc->sc_state = STARTED;
	if (al->al_sendflag == 0)
		al->al_sendflag = 1;
	else if (al->al_packet.al_nentries != 0 ||
		 al->al_packet.al_type == AL_ENDOFDATA) {
		if (al_send(sc) != 0) {
			al->al_remaining = UNBLOCK_TIME;
			sc->sc_state = BLOCKED;
			sc->sc_sddrops++;
			al->al_packet.al_nentries = 0;
			return;
		}
		al->al_packet.al_nentries = 0;
	}
	al->al_remaining = al->al_interval;
}
