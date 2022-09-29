#ifndef __RPCSVC_ETHER_H__
#define __RPCSVC_ETHER_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.7 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
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
/*	@(#)ether.h	1.2 88/05/08 4.0NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.5
 */

#define NPROTOS 6
#define NDPROTO 0
#define ICMPPROTO 1
#define UDPPROTO 2
#define TCPPROTO 3
#define ARPPROTO 4
#define OTHERPROTO 5

#define NBUCKETS 16
#define MINPACKETLEN 60
#define MAXPACKETLEN 1514
#define BUCKETLNTH ((MAXPACKETLEN - MINPACKETLEN + NBUCKETS - 1)/NBUCKETS)

#define HASHSIZE 256

#define ETHERSTATPROC_GETDATA 1
#define ETHERSTATPROC_ON 2
#define ETHERSTATPROC_OFF 3
#define ETHERSTATPROC_GETSRCDATA 4
#define ETHERSTATPROC_GETDSTDATA 5
#define ETHERSTATPROC_SELECTSRC 6
#define ETHERSTATPROC_SELECTDST 7
#define ETHERSTATPROC_SELECTPROTO 8
#define ETHERSTATPROC_SELECTLNTH 9
#define ETHERSTATPROG 100010
#define ETHERSTATVERS 1

/*
 * all ether stat's except src, dst addresses
 */
struct etherstat {
	struct timeval	e_time;
	unsigned long	e_bytes;
	unsigned long	e_packets;
	unsigned long	e_bcast;
	unsigned long	e_size[NBUCKETS];
	unsigned long	e_proto[NPROTOS];
};

/*
 * member of address hash table
 */
struct etherhmem {
	int ht_addr;
	unsigned ht_cnt;
	struct etherhmem *ht_nxt;
};

/*
 * src, dst address info
 */
struct etheraddrs {
	struct timeval	e_time;
	unsigned long	e_bytes;
	unsigned long	e_packets;
	unsigned long	e_bcast;
	struct etherhmem **e_addrs;
};

/*
 * for size, a_addr is lowvalue, a_mask is high value
 */
struct addrmask {
	int a_addr;
	int a_mask;		/* 0 means wild card */
};

int xdr_etherstat(XDR *, struct etherstat *);
int xdr_etherhbody(XDR *, struct etherhmem *);
int xdr_etherhmem(XDR *, struct etherhmem **);
int xdr_etherhtable(XDR *, struct etherhmem **);
int xdr_etheraddrs(XDR *, struct etheraddrs *);
int xdr_addrmask(XDR *, struct addrmask *);

extern char *protoname[];
extern int if_fd;
#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_ETHER_H__ */
