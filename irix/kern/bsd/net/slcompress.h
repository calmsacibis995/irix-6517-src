/*	slcompress.h	7.4	90/06/28	*/
/*
 * Definitions for tcp compression routines.
 *
 * $Revision: 1.10 $
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */
#ifdef __cplusplus
extern "C" {
#endif

#define DEF_MAX_STATES 16		/* must be > 2 and < 256 */
#define MAX_HDR	   64

#define MIN_MAX_STATES 3
#define MAX_MAX_STATES 254

#define MAX_LINKNUM 4

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
#define NEW_C	0x40	/* flag bits for what changed in a packet */
#define NEW_I	0x20
#define NEW_S	0x08
#define NEW_A	0x04
#define NEW_W	0x02
#define NEW_U	0x01

/* reserved, special-case values of above */
#define SPECIAL_I (NEW_S|NEW_W|NEW_U)		/* echoed interactive traffic */
#define SPECIAL_D (NEW_S|NEW_A|NEW_W|NEW_U)	/* unidirectional data */
#define SPECIALS_MASK (NEW_S|NEW_A|NEW_W|NEW_U)

#define TCP_PUSH_BIT 0x10


/*
 * "state" data for each active tcp conversation on the wire.  This is
 * basically a copy of the entire IP/TCP header from the last packet
 * we saw from the conversation together with a small identifier
 * the transmit & receive ends of the line use to locate saved header.
 */
struct vj_comp {
	struct tx_cstate *last_cs;	/* most recently used tstate */
	int	tx_states;		/* # of TX slots */
	int	actconn;		/* apparent active TCP connections */
	u_char	last;			/* last sent slot id */

	int	cnt_packets;		/* outbound packets */
	int	cnt_compressed;		/* outbound compressed packets */
	int	cnt_searches;		/* searches for connection state */
	int	cnt_misses;		/* could not find state */

	/* xmit connection states */
	struct tx_cstate {
	    struct tx_cstate *cs_next;  /* next most recently used cstate */
	    u_char	cs_id;		/* slot # of this state */
	    u_char	cs_linknum;	/* BF&I multilink number */
	    signed char	cs_active;	/* >0 if alive--no FIN seen */
	    union slcs_u {
		char csu_hdr[MAX_HDR];
		struct ip csu_ip;	/* most recent ip/tcp hdr */
	    } slcs_u;
	} tstate[1];
};
#define cs_ip slcs_u.csu_ip
#define cs_hdr slcs_u.csu_hdr


struct vj_uncomp {
	int	rx_states;		/* # of RX slots */
	u_char	last;			/* last rcvd slot id */
	u_short	flags;
#	 define SLF_TOSS 1		/* tossing frames because of err */

	int	cnt_uncompressed;	/* inbound uncompressed packets */
	int	cnt_compressed;		/* inbound compressed packets */
	int	cnt_tossed;		/* inbound packets tossed for error */
	int	cnt_error;		/* inbound unknown type packets */

	/* receive connection states */
	struct rx_cstate {
	    u_short	cs_hlen;	/* size of hdr */
	    union slcs_u slcs_u;
	} rstate[1];
};


extern struct vj_comp* vj_comp_init(struct vj_comp*, int);
extern u_char vj_compress(struct mbuf**, struct vj_comp*, int*, u_int,int,int);

extern struct vj_uncomp* vj_uncomp_init(struct vj_uncomp*, int);
extern int vj_uncompress(struct mbuf*, int, u_char**, u_int,struct vj_uncomp*);
#ifdef __cplusplus
}
#endif
