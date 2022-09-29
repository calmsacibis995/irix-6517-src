/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Histogram service implementation
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
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include "expr.h"
#include "heap.h"
#include "histogram.h"
#include "macros.h"
#include "packetbuf.h"
#include "protocol.h"
#include "snoopd.h"
#include "snooper.h"
#include "timer.h"

struct histclient {
	struct histogram	hc_hist;
	unsigned long		hc_tpkts[HIST_MAXBINS];
	unsigned long		hc_tbytes[HIST_MAXBINS];
	struct filter		hc_filter[HIST_MAXBINS+1];
	struct timeval		hc_interval;
	unsigned int		hc_filters;
	unsigned int		hc_rows;
	unsigned int		hc_columns;
	unsigned int		hc_matrix;
};
#define	hc_bins		hc_hist.h_bins
#define	hc_timestamp	hc_hist.h_timestamp
#define	hc_count	hc_hist.h_count

#define	sctohc(sc)	((struct histclient *)(sc)->sc_private)

static void addpacket(struct histclient *, u_int, u_int);

void
hist_init(void)
{
}

void
hist_subscribe(SnoopClient *sc, u_int rows, u_int columns, u_int interval)
{
	struct histclient *hc;

	hc = new(struct histclient);
	bzero(hc, sizeof *hc);
	hc->hc_interval.tv_sec = interval / TICKS_PER_SEC;
	hc->hc_interval.tv_usec =
		(interval % TICKS_PER_SEC) * (1000000 / TICKS_PER_SEC);
	if (sc->sc_if->if_file == 0) {
		timeadd(&hc->hc_timestamp, &curtime, &hc->hc_interval);
	} else
		timerclear(&hc->hc_timestamp);
	hc->hc_rows = rows;
	hc->hc_columns = columns;
	sc->sc_private = hc;
}

void
hist_unsubscribe(SnoopClient *sc)
{
	struct histclient *hc;
	int i;

	hc = sctohc(sc);
	for (i = 0; i < hc->hc_filters; i++)
		snoop_freefilter(sc->sc_if, &hc->hc_filter[i]);
	delete(hc);
}

enum snoopstat
hist_add(SnoopClient *sc, Expr *ex, ExprError *err, int *bin)
{
	struct histclient *hc;

	hc = sctohc(sc);
	if (hc->hc_filters == HIST_MAXBINS)
		return SNOOPERR_INVAL;
	if (!snoop_update(sc->sc_if, ex, err, &hc->hc_filter[hc->hc_filters]))
		return SNOOPERR_BADF;
	*bin = hc->hc_filters++;
	if (hc->hc_filters == hc->hc_rows + hc->hc_columns) {
		hc->hc_matrix = 1;
		hc->hc_bins = hc->hc_rows * hc->hc_columns;
	} else {
		hc->hc_matrix = 0;
		hc->hc_bins = hc->hc_filters;
	}
	return SNOOP_OK;
}

enum snoopstat
hist_delete(SnoopClient *sc, int bin)
{
	struct histclient *hc;
	int i;

	hc = sctohc(sc);
	if (bin >= hc->hc_filters)
		return SNOOPERR_INVAL;
	snoop_freefilter(sc->sc_if, &hc->hc_filter[bin]);
	for (i = bin; i < hc->hc_filters; i++) {
		hc->hc_filter[i] = hc->hc_filter[i+1];
		hc->hc_count[i] = hc->hc_count[i+1];
	}
	--hc->hc_filters;
	hc->hc_matrix = 0;
	return SNOOP_OK;
}

/* ARGSUSED */
int
hist_process(SnoopClient *sc, PacketBuf *pb, Packet *p)
{
	struct histclient *hc;
	struct snoopheader *sh;
	u_int i;

	hc = sctohc(sc);
	if (hc->hc_filters == 0)
		return 0;

	/* Check for EOF */
	if (p->p_len == 0) {
		if (!svc_sendreply(sc->sc_xprt, xdr_histogram, &hc->hc_hist))
			sc->sc_sddrops++;
		timerclear(&hc->hc_timestamp);
		if (!svc_sendreply(sc->sc_xprt, xdr_histogram, &hc->hc_hist))
			sc->sc_state = BLOCKED;
		return 0;
	}

	sh = &p->p_sp.sp_hdr;
	if (timercmp(&hc->hc_timestamp, &sh->snoop_timestamp, <)) {
		if (sc->sc_if->if_file != 0
		    && !timerisset(&hc->hc_timestamp)) {
			timeadd(&hc->hc_timestamp, &sh->snoop_timestamp,
				&hc->hc_interval);
		} else {
			/*
			 * Call svc_sendreply() directly since we want
			 * to drop the data if it can't be sent
			 */
			if (!svc_sendreply(sc->sc_xprt, xdr_histogram,
					   &hc->hc_hist))
				sc->sc_sddrops++;
			timeadd(&hc->hc_timestamp, &hc->hc_timestamp,
				&hc->hc_interval);
		}
	}
	if (hc->hc_matrix != 0) {
		u_int j;

		for (i = 0; i < hc->hc_rows; i++) {
			if (!snoop_test(sc->sc_if, sc->sc_errflags,
					&hc->hc_filter[i],
					&p->p_sp, p->p_len))
				continue;
			for (j = 0; j < hc->hc_columns; j++) {
				if (snoop_test(sc->sc_if, sc->sc_errflags,
					       &hc->hc_filter[j + hc->hc_rows],
					       &p->p_sp, p->p_len))
					addpacket(hc, i * hc->hc_columns + j,
						  sh->snoop_packetlen);
			}
		}
	} else {
		for (i = 0; i < hc->hc_filters; i++) {
			if (snoop_test(sc->sc_if, sc->sc_errflags,
				       &hc->hc_filter[i], &p->p_sp, p->p_len))
				addpacket(hc, i, sh->snoop_packetlen);
		}
	}
	return 0;
}

void
hist_timer(SnoopClient *sc)
{
	if (sc->sc_state == BLOCKED) {
		struct histclient *hc = sctohc(sc);

		if (svc_sendreply(sc->sc_xprt, xdr_histogram, &hc->hc_hist))
			sc->sc_state = STARTED;
	}
}

void
hist_setinterval(SnoopClient *sc, u_int interval)
{
	struct histclient *hc = sctohc(sc);

	hc->hc_interval.tv_sec = interval / TICKS_PER_SEC;
	hc->hc_interval.tv_usec =
		(interval % TICKS_PER_SEC) * (1000000 / TICKS_PER_SEC);
	if (sc->sc_if->if_file != 0)
		timerclear(&hc->hc_timestamp);
	else {
		timeadd(&hc->hc_timestamp, &curtime, &hc->hc_interval);
	}
}

static void
addpacket(struct histclient *hc, u_int ind, u_int packetlen)
{
	unsigned long c, d;
	float x;

	if (hc->hc_count[ind].c_packets < 16777216.0)
		hc->hc_count[ind].c_packets++;
	else {
		float x;

		hc->hc_tpkts[ind]++;
		x = hc->hc_count[ind].c_packets + hc->hc_tpkts[ind];
		if (x - hc->hc_count[ind].c_packets == hc->hc_tpkts[ind]) {
			hc->hc_count[ind].c_packets = x;
			hc->hc_tpkts[ind] = 0;
		}
	}
	if (hc->hc_count[ind].c_bytes < 16777216.0)
		hc->hc_count[ind].c_bytes += packetlen;
	else {
		unsigned long c, d;
		float x;

		for (x = 16777216.0, c = 0;
		     hc->hc_count[ind].c_bytes >= x; x *= 2, c++)
			;

		hc->hc_tbytes[ind] += packetlen;
		c = 1 << c;
		d = hc->hc_tbytes[ind] / c;
		hc->hc_tbytes[ind] %= c;
		hc->hc_count[ind].c_bytes += d * c;
	}
}
