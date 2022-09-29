/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	A packetviewer for NetLook
 *
 *	$Revision: 1.13 $
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
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <net/raw.h>
#include "heap.h"
#include "macros.h"
#include "netlook.h"
#include "packetview.h"
#include "protoid.h"
#include "protocol.h"
#include "protocols/ether.h"
#include "protocols/fddi.h"
#include "llc/llc_nostruct.h"
#include "protocols/ip.h"
#include "tokenring/tokenring.h"
#include "snoopd.h"

DefinePacketViewOperations(netlook_pvops,nlpv)

#define PKTBUFSIZE	32	/* ~ 4KB */

typedef struct {
	struct nlspacket	*nlsp;		/* Pointer for next packet */
	int			lastproto;	/* Last protocol pushed */
	int			didicmp;	/* We pushed icmp */
	PacketView		view;		/* base class state */
} NetLookPacketView;

#define	NLPV(pv)	((NetLookPacketView *) (pv)->pv_private)


PacketView *
netlook_packetview(void)
{
	NetLookPacketView *nlpv;
	Protocol *pr;
	char val = '1';

	nlpv = new(NetLookPacketView);
	nlpv->nlsp = 0;
	pv_init(&nlpv->view, nlpv, &netlook_pvops, "netlook", PV_TERSE);

	/* Set ethernet and FDDI to translate addresses to names */
	pr = findprotobyid(PRID_ETHER);
	pr_setopt(pr, ETHER_PROPT_HOSTBYNAME, &val);
	pr = findprotobyid(PRID_FDDI);
	pr_setopt(pr, FDDI_PROPT_HOSTBYNAME, &val);

	/* Make llc appear at PV_TERSE */
	pr = findprotobyid(PRID_LLC);
	pr->pr_level = PV_TERSE;
	
	return &nlpv->view;
}

void
nlpv_setnlspacket(PacketView *pv, struct nlspacket *nlsp)
{
	NetLookPacketView *nlpv = NLPV(pv);

	nlpv->nlsp = nlsp;
}

static void
nlpv_destroy(PacketView *pv)
{
	NetLookPacketView *nlpv = NLPV(pv);

	pv_finish(pv);
	delete(nlpv);
}

/* ARGSUSED */
static int
nlpv_head(PacketView *pv, struct snoopheader *sh, struct tm *tm)
{
	NetLookPacketView *nlpv = NLPV(pv);
	struct nlspacket *nlsp = nlpv->nlsp;

	bzero(nlsp, sizeof *nlsp);
	nlsp->nlsp_length = sh->snoop_packetlen;
	nlsp->nlsp_timestamp = sh->snoop_timestamp;
	nlsp->nlsp_protocols = 0;
	nlpv->lastproto = 0;
	nlpv->didicmp = 0;
	return 1;
}

/* ARGSUSED */
static int
nlpv_tail(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
nlpv_push(PacketView *pv, Protocol *pr, char *name, int namlen, char *title)
{
	int protoid = pr->pr_id;
	NetLookPacketView *nlpv = NLPV(pv);
	struct nlspacket *nlsp = nlpv->nlsp;

	if (protoid != nlpv->lastproto
	    && nlsp->nlsp_protocols < NLP_MAXPROTOS
	    && nlpv->didicmp == 0)
		nlsp->nlsp_protolist[nlsp->nlsp_protocols++] =
					nlpv->lastproto = protoid;
	if (protoid == PRID_ICMP)
		nlpv->didicmp = 1;
	return 1;
}

/* ARGSUSED */
static int
nlpv_pop(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
nlpv_field(PacketView *pv, void *base, int size, struct protofield *pf,
	  char *contents, int contlen)
{
	NetLookPacketView *nlpv = NLPV(pv);
	struct nlspacket *nlsp = nlpv->nlsp;
	extern void fddi_bitswap(unsigned char *, unsigned char *);

	switch (nlpv->lastproto) {
	    case PRID_ETHER:
		switch (pf->pf_id) {
		    case ETHERFID_SRC:
			bcopy(base, &nlsp->nlsp_src.nlep_eaddr,
					sizeof nlsp->nlsp_src.nlep_eaddr);
			strncpy(nlsp->nlsp_src.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_src.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		    case ETHERFID_DST:
			bcopy(base, &nlsp->nlsp_dst.nlep_eaddr,
					sizeof nlsp->nlsp_dst.nlep_eaddr);
			strncpy(nlsp->nlsp_dst.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_dst.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		    case ETHERFID_TYPE:
			nlsp->nlsp_type = *(u_short*)base;
			break;
		}
		
		break;

	    case PRID_FDDI:
		switch (pf->pf_id) {
		    case FDDIFID_DST:
			fddi_bitswap(base, (unsigned char *)
					   &nlsp->nlsp_dst.nlep_eaddr);
			strncpy(nlsp->nlsp_dst.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_dst.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		    case FDDIFID_SRC:
			fddi_bitswap(base, (unsigned char *)
					   &nlsp->nlsp_src.nlep_eaddr);
			strncpy(nlsp->nlsp_src.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_src.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_TOKENRING:
		switch (pf->pf_id) {
		    case tokenringfid_dst:
			bcopy(base, &nlsp->nlsp_dst.nlep_eaddr,
					sizeof nlsp->nlsp_dst.nlep_eaddr);
			strncpy(nlsp->nlsp_dst.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_dst.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		    case tokenringfid_src:
			bcopy(base, &nlsp->nlsp_src.nlep_eaddr,
					sizeof nlsp->nlsp_src.nlep_eaddr);
			strncpy(nlsp->nlsp_src.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_src.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_LLC:
		switch (pf->pf_id) {
		    case llcfid_type:
			nlsp->nlsp_type = *(u_short*)base;
			break;
		}
		break;

	    case PRID_IP:
		if (nlpv->didicmp)
			break;

		switch (pf->pf_id) {
		    case IPFID_SRC:
			nlsp->nlsp_src.nlep_naddr = *(u_long*)base;
			strncpy(nlsp->nlsp_src.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_src.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		    case IPFID_DST:
			nlsp->nlsp_dst.nlep_naddr = *(u_long*)base;
			strncpy(nlsp->nlsp_dst.nlep_name,
						contents, NLP_NAMELEN-1);
			nlsp->nlsp_dst.nlep_name[NLP_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_UDP:
	    case PRID_TCP:
		if (nlpv->didicmp)
			break;

		switch (pf->pf_id) {
		    case IPFID_SPORT:
			nlsp->nlsp_src.nlep_port = *(u_short*)base;
			break;
		    case IPFID_DPORT:
			nlsp->nlsp_dst.nlep_port = *(u_short*)base;
			break;
		}
		break;
	}

	return 1;
}

/* ARGSUSED */
static int
nlpv_break(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
nlpv_text(PacketView *pv, char *buf, int len)
{
	return 1;
}

/* ARGSUSED */
static int
nlpv_newline(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
nlpv_hexdump(PacketView *pv, unsigned char *bp, int off, int len)
{
	return 1;
}
