#ifndef ETHER_H
#define ETHER_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Ethernet constants, structures, and functions.  We don't use the
 * kernel's ether_header definition in <netinet/if_ether.h> because 
 * it uses vectors rather than structs for the addresses.
 */
#include "protostack.h"

/*
 * Ethernet field identifiers are array indices, so don't change them.
 */
#define	ETHERFID_DST	0
#define	ETHERFID_SRC	1
#define	ETHERFID_TYPE	2

/*
 * Ethernet protocol option
 */
enum ether_propt { ETHER_PROPT_HOSTBYNAME };

#define	ETHERADDRLEN	6
#define	ETHERHDRLEN	sizeof(struct etherhdr)

struct etheraddr {
	unsigned char	ea_vec[ETHERADDRLEN];
};

struct etherhdr {
	struct etheraddr	eh_dst;		/* destination address */ 
	struct etheraddr	eh_src;		/* source address */
	union {
		unsigned short	ehu_type;	/* nested protocol type */
		unsigned short	ehu_len;	/* stupid 802.3 length */
	} eh_u;
};
#define	eh_type	eh_u.ehu_type
#define	eh_len	eh_u.ehu_len

struct etherframe {
	ProtoStackFrame		ef_frame;	/* base class state */
	struct etheraddr	ef_src;		/* source address*/
	struct etheraddr	ef_dst;		/* destination address */
};

/*
 * Get or put an etheraddr given a datastream.
 */
#define	ds_etheraddr(ds, ea)	ds_bytes(ds, (ea)->ea_vec, sizeof (ea)->ea_vec)

/*
 * ether_vendor(ea)
 *	Return a pointer to the static string containing an abbreviated
 *	organization name for ea's vendor code.
 * ether_typedesc(type)
 *	Return a pointer to the static string describing the protocol
 *	associated with the given typecode.
 * ether_hostname(ea)
 *	Return a pointer to the cached hostname or hex byte address string
 *	representation of an Ethernet host.
 * ether_hostaddr(name)
 *	Return a pointer to the cached address named by the given hostname
 *	or hex byte Ethernet address representation.
 * ether_addhost(name, ea)
 *	Associate name with ea and ea with name.
 */
char		 *ether_vendor(struct etheraddr *);
char		 *ether_typedesc(unsigned short);
char		 *ether_hostname(struct etheraddr *);
struct etheraddr *ether_hostaddr(char *);
struct symbol	 *ether_addhost(char *, struct etheraddr *);

/*
 * New PIDL-conforming names for the ether_host*() functions.
 */
#define	etheraddr_to_name(p)	ether_hostname((struct etheraddr *)(p))
#define	name_to_etheraddr(n)	ether_hostaddr(n)

extern char	 	ethername[];
extern struct etheraddr	etherbroadcastaddr;

#endif
