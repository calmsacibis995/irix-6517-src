/* RFC-1042 style 802.2 headers
 *
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef __SYS_LLC_H
#define __SYS_LLC_H 1

#ident "$Revision: 1.1 $"

/* RFC-1042 says to insert the following header before every IP datagram
 *	on an 802.3 network
 */
struct llc {
	union {
		struct {
			unsigned char	dsap;
			unsigned char	ssap;
			unsigned char	cont;
			unsigned char	org[3];
		} s;
		struct {
			unsigned long	c1;
			unsigned short	c2;
			unsigned short	etype;
		} c;
	} ullc;
};

/* RFC1042 and 1188 stuff */
#define llc_c1		ullc.c.c1
#define llc_c2		ullc.c.c2
#define llc_etype	ullc.c.etype

/* the first 6 bytes look like these */
#define RFC1042_K1 170
#define RFC1042_CONT 3
#define RFC1042_K2 0

/* or these */
#define RFC1042_C1 htonl(0xaaaa0300)
#define RFC1042_C2 htons(0)


#define llc_dsap	ullc.s.dsap
#define llc_ssap	ullc.s.ssap
#define llc_cont	ullc.s.cont
#define llc_org		ullc.s.org


#endif /* __SYS_LLC_H */
