/* slcompress.h - CSLIP TCP/IP compression header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
 * Definitions for tcp compression routines.
 *
 * Copyright (c) 1989, 1990, 1992, 1993 Regents of the University of
 * California. All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Van Jacobson (van@ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

/*
modification history
--------------------
01b,14nov94,dzb  fixed decompress macros for little endian.
01a,07mar94,dzb  first WRS version.  Added macros from slcompress.c.
                 Changed to compile out stats counters by default.
		 removed TOS definitions.
*/

/*
DESCRIPTION

This file contains header information for TCP/IP compression routines
used for CSLIP.
*/

#ifndef __INCslcompressh
#define __INCslcompressh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

/* defines */

/*
 * Compressed packet format:
 *
 * The first octet contains the packet type (top 3 bits), TCP
 * 'push' bit, and flags that indicate which of the 4 TCP sequence
 * numbers have changed (bottom 5 bits).  The next octet is a
 * conversation number that associates a saved IP/TCP header with
 * the compressed packet.  The next two octets are the TCP checksum
 * from the original datagram.  The next 0 to 15 octets are
 * sequence number changes, one change per bit set in the header
 * (there may be no changes and there are two special cases where
 * the receiver implicitly knows what changed -- see below).
 *
 * There are 5 numbers which can change (they are always inserted
 * in the following order): TCP urgent pointer, window,
 * acknowlegement, sequence number and IP ID.  (The urgent pointer
 * is different from the others in that its value is sent, not the
 * change in value.)  Since typical use of SLIP links is biased
 * toward small packets (see comments on MTU/MSS below), changes
 * use a variable length coding with one octet for numbers in the
 * range 1 - 255 and 3 octets (0, MSB, LSB) for numbers in the
 * range 256 - 65535 or 0.  (If the change in sequence number or
 * ack is more than 65535, an uncompressed packet is sent.)
 */

/*
 * Packet types (must not conflict with IP protocol version)
 *
 * The top nibble of the first octet is the packet type.  There are
 * three possible types: IP (not proto TCP or tcp with one of the
 * control flags set); uncompressed TCP (a normal IP/TCP packet but
 * with the 8-bit protocol field replaced by an 8-bit connection id --
 * this type of packet syncs the sender & receiver); and compressed
 * TCP (described above).
 *
 * LSB of 4-bit field is TCP "PUSH" bit (a worthless anachronism) and
 * is logically part of the 4-bit "changes" field that follows.  Top
 * three bits are actual packet type.  For backward compatibility
 * and in the interest of conserving bits, numbers are chosen so the
 * IP protocol version number (4) which normally appears in this nibble
 * means "IP packet".
 */

/* packet types */

#define TYPE_IP 0x40
#define TYPE_UNCOMPRESSED_TCP 0x70
#define TYPE_COMPRESSED_TCP 0x80
#define TYPE_ERROR 0x00

/* Bits in first octet of compressed packet */

#define NEW_C		0x40	/* flag bits for what changed in a packet */
#define NEW_I		0x20
#define TCP_PUSH_BIT	0x10
#define NEW_S		0x08
#define NEW_A		0x04
#define NEW_W		0x02
#define NEW_U		0x01

/* reserved, special-case values of above */

#define SPECIAL_I (NEW_S|NEW_W|NEW_U)		/* echoed interactive traffic */
#define SPECIAL_D (NEW_S|NEW_A|NEW_W|NEW_U)	/* unidirectional data */

#define SPECIALS_MASK (NEW_S|NEW_A|NEW_W|NEW_U)

#define MAX_STATES 16		/* must be > 2 and < 256 */
#define MAX_HDR MLEN            /* XXX 4bsd-ism: should really be 128 */

/* flag values */

#define SLF_TOSS 1		/* tossing rcvd frames because of input err */

#ifdef	SL_STATS
#define INCR(counter) ++comp->counter;
#else	/* SL_STATS */
#define INCR(counter)
#endif	/* SL_STATS */

#define BCMP(p1, p2, n) bcmp((char *)(p1), (char *)(p2), (int)(n))
#define BCOPY(p1, p2, n) bcopy((char *)(p1), (char *)(p2), (int)(n))

/*
 * ENCODE encodes a number that is known to be non-zero.  ENCODEZ
 * checks for zero (since zero has to be encoded in the long, 3 byte
 * form).
 */

#define ENCODE(n)							\
    {									\
    if ((u_short)(n) >= 256)						\
	{								\
	*cp++ = 0;							\
	cp[1] = (n);							\
	cp[0] = (n) >> 8;						\
	cp += 2;							\
	}								\
    else								\
	*cp++ = (n);							\
    }

#define ENCODEZ(n)							\
    {									\
    if ((u_short)(n) >= 256 || (u_short)(n) == 0)			\
	{								\
	*cp++ = 0;							\
	cp[1] = (n);							\
	cp[0] = (n) >> 8;						\
	cp += 2;							\
	}								\
    else								\
	*cp++ = (n);							\
    }

#define DECODEL(f)							\
    {									\
    if (*cp == 0)							\
	{								\
	(f) = htonl(ntohl(f) + ((cp[1] << 8) | cp[2]));			\
	cp += 3;							\
	}								\
    else								\
	{								\
        (f) = htonl(ntohl(f) + (u_long)*cp);				\
	cp++;								\
	}								\
    }

#define DECODES(f)							\
    {									\
    if (*cp == 0)							\
	{								\
	(f) = htons(ntohs(f) + ((cp[1] << 8) | cp[2]));			\
	cp += 3;							\
	}								\
    else								\
	{								\
	(f) = htons(ntohs(f) + (u_long)*cp);				\
	cp++;								\
	}								\
    }

#define DECODEU(f)							\
    {									\
    if (*cp == 0)							\
	{								\
	(f) = htons((cp[1] << 8) | cp[2]);				\
	cp += 3;							\
        }								\
    else								\
	{								\
	(f) = htons((u_long)*cp);					\
	cp++;								\
	}								\
    }

/* typedefs */

/*
 * "state" data for each active tcp conversation on the wire.  This is
 * basically a copy of the entire IP/TCP header from the last packet
 * we saw from the conversation together with a small identifier
 * the transmit & receive ends of the line use to locate saved header.
 */

struct cstate
    {
    struct cstate *cs_next;	/* next most recently used cstate (xmit only) */
    u_short cs_hlen;		/* size of hdr (receive only) */
    u_char cs_id;		/* connection # associated with this state */
    u_char cs_filler;
    struct ip cs_ip;		/* ip/tcp hdr from most recent packet */
    union
        {
        char csu_hdr[MAX_HDR];
        struct ip csu_ip;       /* ip/tcp hdr from most recent packet */
        } slcs_u;
    };

#define cs_ip  slcs_u.csu_ip
#define cs_hdr slcs_u.csu_hdr

/* all the state data for one serial line (we need one of these per line).  */

struct slcompress
    {
    struct cstate *last_cs;	/* most recently used tstate */
    u_char last_recv;		/* last rcvd conn. id */
    u_char last_xmit;		/* last sent conn. id */
    u_short flags;		/* compression related flags */
#ifdef	SL_STATS
    u_int sls_packets;		/* outbound packets */
    u_int sls_compressed;	/* outbound compressed packets */
    u_int sls_searches;		/* searches for connection state */
    u_int sls_misses;		/* times couldn't find conn. state */
    u_int sls_uncompressedin;	/* inbound uncompressed packets */
    u_int sls_compressedin;	/* inbound compressed packets */
    u_int sls_errorin;		/* inbound unknown type packets */
    u_int sls_tossed;		/* inbound packets tossed because of error */
#endif	/* SL_STATS */
    struct cstate tstate[MAX_STATES];	/* xmit connection states */
    struct cstate rstate[MAX_STATES];	/* receive connection states */
    };

/* forward declarations */

#ifdef __STDC__

extern	void   sl_compress_init (struct slcompress *comp);
extern	u_char sl_compress_tcp (struct mbuf *m, struct ip *ip,
				struct slcompress *comp);
extern	int    sl_uncompress_tcp (char **buf, short *len, u_int type,
				struct slcompress *comp);

#else	/* __STDC__ */

extern	void   sl_compress_init ();
extern	u_char sl_compress_tcp ();
extern	int    sl_uncompress_tcp ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */
 
#ifdef __cplusplus
}
#endif
  
#endif	/* __INCslcompressh */
