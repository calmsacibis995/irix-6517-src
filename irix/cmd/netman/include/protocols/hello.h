#ifndef __HELLO_H__
#define __HELLO_H__
/*
 * hello.h
 *
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * This defines the packet layout of the Distributed Computer Network (DCN)
 * HELLO routing Protocol.
 *
 */
#include <sys/types.h>
#include <netinet/in.h>

#ifndef IPPROTO_HELLO
#define		IPPROTO_HELLO	63
#endif
#define HELLOMAXPACKETSIZE	1440

#define	HELLO_HDRSIZE	12

/*
 * Define the DCN HELLO protocol packet
 */

struct hellohdr {
	u_short	h_cksum;	/* Ip checksum of this header and data areas */
	u_short h_date;		/* Julian days since 1 January 1972 */
	u_long	h_time;		/* Local time (msec since midnight UT) */
	u_short	h_tstp;		/* (used to calculate delay/offset) */
	u_char	h_count;	/* Number of elements that follow */
	u_char	h_type;		/* Type of elements */
};

struct type0pair {
	u_short d0_delay;	/* Delay to peer (milliseconds) */
	u_short	d0_offset;	/* Clock offset of peer (milliseconds) */
};
			
struct type1pair {
	struct in_addr d1_dst;	/* IP host/network address */
	u_short	d1_delay;	/* Delay to peer (milliseconds) */
	short	d1_offset;	/* Clock offset of peer (milliseconds) */
};

extern char	helloname[];

#endif /* __HELLO_H__ */
