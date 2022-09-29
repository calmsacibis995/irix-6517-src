#ifndef ARP_H
#define ARP_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Address Resolution Protocol (ARP) and Reverse ARP (RARP) declarations.
 */
#include <sys/types.h>
#include <sys/socket.h>		/* prerequisite of if_arp.h */
#include <net/if_arp.h>		/* prerequisite of if_ether.h */
#include <netinet/in.h>		/* for struct in_addr */
#include "ether.h"
#include "protostack.h"

/*
 * RARP definitions.  RARP uses the same encapsulation as ARP.
 */
#define	ETHERTYPE_RARP	0x8035

#define	RARPOP_REQUEST	3
#define	RARPOP_REPLY	4

#ifndef ARPHRD_802
#define ARPHRD_802      6       /* any 802 network */
#endif


/*
 * ARP/IP protocol option
 */
enum arpip_propt { ARPIP_PROPT_ETHERUPDATE };

/*
 * Structure of a decoded IP-ARP segment.
 * NB: may include structure padding; don't try to ds_inline.
 */
struct arpipseg {
	struct etheraddr	ai_sha;		/* sender hardware address */
	struct in_addr		ai_spa;		/* sender protocol address */
	struct etheraddr	ai_tha;		/* target hardware address */
	struct in_addr		ai_tpa;		/* target protocol address */
};

#define	ARPIP_SEGLEN \
	(2 * (sizeof(struct etheraddr) + sizeof(struct in_addr)))

/*
 * ARP/IP protocol stack frame data.
 */
struct arpframe {
	ProtoStackFrame	af_frame;	/* base class state */
	int		af_op;		/* request or reply */
	struct in_addr	af_spa;		/* sender protocol address */
	struct in_addr	af_tpa;		/* target protocol address */
};

/*
 * Get or put an arphdr or arpipseg given a datastream.
 */
int	ds_arphdr(struct datastream *, struct arphdr *);
int	ds_arpipseg(struct datastream *, struct arpipseg *);

/*
 * Protocol names.
 */
extern char	arpname[];
extern char	arpipname[];

#endif
