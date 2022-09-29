/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	A packetviewer for AddrList
 *
 *	$Revision: 1.4 $
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
#include "addrlist.h"
#include "packetview.h"
#include "protoid.h"
#include "protocol.h"
#include "protocols/ether.h"
#include "protocols/fddi.h"
#include "protocols/ip.h"
#include "tokenring/tokenring.h"
#include "snoopd.h"

DefinePacketViewOperations(addrlist_pvops,alpv)

typedef struct {
	struct alentry	*entry;		/* Pointer for entry */
	struct timeval	*timestamp;	/* Pointer for timestamp */
	int		lastproto;	/* Last protocol pushed */
	int		didicmp;	/* We pushed icmp */
	PacketView	view;		/* base class state */
} AddrListPacketView;

#define	ALPV(pv)	((AddrListPacketView *) (pv)->pv_private)


PacketView *
addrlist_packetview(struct alentry *e, struct timeval *t)
{
	AddrListPacketView *alpv;
	Protocol *pr;
	char val = '1';

	alpv = new(AddrListPacketView);
	alpv->entry = e;
	alpv->timestamp = t;
	pv_init(&alpv->view, alpv, &addrlist_pvops, "addrlist", PV_TERSE);

	/* Set ethernet and FDDI to translate addresses to names */
	pr = findprotobyid(PRID_ETHER);
	pr_setopt(pr, ETHER_PROPT_HOSTBYNAME, &val);
	pr = findprotobyid(PRID_FDDI);
	pr_setopt(pr, FDDI_PROPT_HOSTBYNAME, &val);

	/* Make llc appear at PV_TERSE */
	pr = findprotobyid(PRID_LLC);
	pr->pr_level = PV_TERSE;
	
	return &alpv->view;
}

static void
alpv_destroy(PacketView *pv)
{
	AddrListPacketView *alpv = ALPV(pv);

	pv_finish(pv);
	delete(alpv);
}

/* ARGSUSED */
static int
alpv_head(PacketView *pv, struct snoopheader *sh, struct tm *tm)
{
	AddrListPacketView *alpv = ALPV(pv);
	*(alpv->timestamp) = sh->snoop_timestamp;
	bzero(alpv->entry, sizeof *alpv->entry);
	alpv->entry->ale_count.c_packets = 1;
	alpv->entry->ale_count.c_bytes = sh->snoop_packetlen;
	alpv->lastproto = alpv->didicmp = 0;
	return 1;
}

/* ARGSUSED */
static int
alpv_tail(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
alpv_push(PacketView *pv, Protocol *pr, char *name, int namlen, char *title)
{
	AddrListPacketView *alpv = ALPV(pv);

	if (alpv->lastproto != pr->pr_id && alpv->didicmp == 0)
		alpv->lastproto = pr->pr_id;

	if (pr->pr_id == PRID_ICMP)
		alpv->didicmp = 1;
	return 1;
}

/* ARGSUSED */
static int
alpv_pop(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
alpv_field(PacketView *pv, void *base, int size, struct protofield *pf,
	  char *contents, int contlen)
{
	AddrListPacketView *alpv = ALPV(pv);
	struct alentry *entry = alpv->entry;
	extern void fddi_bitswap(unsigned char *, unsigned char *);

	switch (alpv->lastproto) {
	    case PRID_ETHER:
		switch (pf->pf_id) {
		    case ETHERFID_SRC:
			bcopy(base, &entry->ale_src.alk_paddr,
			      sizeof entry->ale_src.alk_paddr);
			strncpy(entry->ale_src.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_src.alk_name[AL_NAMELEN-1] = '\0';
			break;
		    case ETHERFID_DST:
			bcopy(base, &entry->ale_dst.alk_paddr,
			      sizeof entry->ale_dst.alk_paddr);
			strncpy(entry->ale_dst.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_dst.alk_name[AL_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_FDDI:
		switch (pf->pf_id) {
		    case FDDIFID_DST:
			fddi_bitswap(base, (unsigned char *)
				     &entry->ale_dst.alk_paddr);
			strncpy(entry->ale_dst.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_dst.alk_name[AL_NAMELEN-1] = '\0';
			break;
		    case FDDIFID_SRC:
			fddi_bitswap(base, (unsigned char *)
				     &entry->ale_src.alk_paddr);
			strncpy(entry->ale_src.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_src.alk_name[AL_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_TOKENRING:
		switch (pf->pf_id) {
		    case tokenringfid_dst:
			bcopy(base, &entry->ale_dst.alk_paddr,
			      sizeof entry->ale_dst.alk_paddr);
			strncpy(entry->ale_dst.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_dst.alk_name[AL_NAMELEN-1] = '\0';
			break;
		    case tokenringfid_src:
			bcopy(base, &entry->ale_src.alk_paddr,
			      sizeof entry->ale_src.alk_paddr);
			strncpy(entry->ale_src.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_src.alk_name[AL_NAMELEN-1] = '\0';
			break;
		}
		break;

	    case PRID_IP:
		if (alpv->didicmp)
			break;

		switch (pf->pf_id) {
		    case IPFID_SRC:
			entry->ale_src.alk_naddr = *(u_long*)base;
			strncpy(entry->ale_src.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_src.alk_name[AL_NAMELEN-1] = '\0';
			break;
		    case IPFID_DST:
			entry->ale_dst.alk_naddr = *(u_long*)base;
			strncpy(entry->ale_dst.alk_name,
				contents, AL_NAMELEN-1);
			entry->ale_dst.alk_name[AL_NAMELEN-1] = '\0';
			break;
		}
		break;
	}

	return 1;
}

/* ARGSUSED */
static int
alpv_break(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
alpv_text(PacketView *pv, char *buf, int len)
{
	return 1;
}

/* ARGSUSED */
static int
alpv_newline(PacketView *pv)
{
	return 1;
}

/* ARGSUSED */
static int
alpv_hexdump(PacketView *pv, unsigned char *bp, int off, int len)
{
	return 1;
}
