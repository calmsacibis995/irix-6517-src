#ifndef _NETINET_IP6_H_
#define _NETINET_IP6_H_

/*
 * Definitions for internet protocol version 6.
 * Per RFC 1883
 */

/*
 * Structure of an IPv6 (first) header.
 */

struct ipv6 {
	u_int32_t ip6_head;		/* version and flow label */
	u_int16_t ip6_len;		/* payload length */
	u_int8_t  ip6_nh;		/* next header */
	u_int8_t  ip6_hlim;		/* hop limit */
	struct    in6_addr ip6_src;	/* source address */
	struct	  in6_addr ip6_dst;	/* destination address */
};

#define IP6_MMTU	576		/* minimal MTU and reassembly */

/*
 * Definitions of next header field values.
 */

#define IP6_NHDR_HOP	0	/* hop-by-hop IPv6 header */
#define IP6_NHDR_RT	43	/* routing IPv6 header */
#define IP6_NHDR_FRAG	44	/* fragment IPv6 header */
#define IP6_NHDR_AUTH	51	/* authentication IPv6 header */
#define IP6_NHDR_ESP	50	/* encryption IPv6 header */
#define IP6_NHDR_DOPT	60	/* destination options IPv6 header */
#define IP6_NHDR_NONH	59	/* no next header */

/*
 * Fragment Header.
 */

struct ipv6_fraghdr {
	u_int8_t  if6_nh;	/* next header */
	u_int8_t  if6_res;	/* reserved */
	u_int16_t if6_off;	/* offset */
#define IP6_MF 0x1		/* more flag */
#define IP6_OFFMASK 0xfff8	/* mask of real offset field */
	u_int32_t if6_id;	/* identifier */
};

/*
 * Hop-by-Hop Options Header.
 */

struct ipv6_h2hhdr {
	u_int8_t  ih6_nh;	/* next header */
	u_int8_t  ih6_hlen;	/* header extension length */
	u_int16_t ih6_pad1;	/* to 4 byte length */
	u_int32_t ih6_pad2;	/* to 8 byte length */
};

/*
 * Routing Header.
 */

struct ipv6_rthdr {
	u_int8_t  ir6_nh;	/* next header */
	u_int8_t  ir6_hlen;	/* header extension length */
	u_int8_t  ir6_type;	/* routing type */
#define IP6_LSRRT 0		/* type 0: loose source route */
	u_int8_t  ir6_sglt;	/* index of next address */
	u_int32_t ir6_slmsk;	/* strict/loose bit mask */
};
#define IP6_RT_MAX	23	/* maximum number of addresses */
#define IP6_RT_SLMSK	0x00ffffff
#define IP6_RT_SLBIT(n)	(1 << (IP6_RT_MAX - n))

/*
 * Destination Options Header.
 */

struct ipv6_dopthdr {
	u_int8_t  io6_nh;	/* next header */
	u_int8_t  io6_hlen;	/* header extension length */
	u_int16_t io6_pad1;	/* to 4 byte length */
	u_int32_t io6_pad2;	/* to 8 byte length */
};

/*
 * Authentication Header.
 */

struct ipv6_authhdr {
	u_int8_t  ah6_nh;	/* next header */
	u_int8_t  ah6_hlen;	/* header extension length */
	u_int16_t ah6_pad;	/* to 4 byte length */
	u_int32_t ah6_spi;	/* security parameter index */
};

/*
 * Encryption Security Payload Header.
 */

struct ipv6_esphdr {
	u_int32_t esp6_spi;	/* security parameter index */
};

#define IP6_OPT_PAD1	0	/* one-byte pad option type */
#define IP6_OPT_PADN	1	/* N-byte pad option type */

/*
 * IPv6 implementation parameters.
 */
#define	IP6FRAGTTL	120		/* time to live for frags, slowhz */

/*
 * Header structure of BSD advanced API
 */

struct ip6_hdr {
    union {
	struct ip6_hdrctl {
	    u_int32_t	ip6_un1_flow;	/* 24 bits of flow-ID */
	    u_int16_t	ip6_un1_plen;	/* payload length */
	    u_int8_t	ip6_un1_nxt;	/* next header */
	    u_int8_t	ip6_un1_hlim;	/* hop limit */
	} ip6_un1;
	u_int8_t ip6_un2_vfc;		/* 4 bits version, 4 bits priority */
    } ip6_ctlun;
    struct in6_addr ip6_src;		/* source address */
    struct in6_addr ip6_dst;		/* destination address */
};

#define	ip6_vfc		ip6_ctlun.ip6_un2_vfc
#define ip6_flow	ip6_ctlun.ip6_un1.ip6_un1_flow
#define ip6_plen	ip6_ctlun.ip6_un1.ip6_un1_plen
#define ip6_nxt		ip6_ctlun.ip6_un1.ip6_un1_nxt
#if 0
/* this conflicts with ip6_hlim in struct ipv6 above */
#define ip6_hlim	ip6_ctlun.ip6_un1.ip6_un1_hlim
#endif
#define ip6_hops	ip6_ctlun.ip6_un1.ip6_un1_hlim

/*
 * These structures are used by the user to pass options to the kernel that the
 * user would like to have appear in the packet.  They could possible be
 * merged with the structures further up in the file but they may not
 * necessarily always match the format that is in the packet in the future
 * so they are separate for now.
 */

/* Hop-by-Hop options header */
struct ip6_hbh {
	uint8_t	ip6h_nxt;	/* next header */
	uint8_t	ip6h_len;	/* length in units of 8 octets */
	/* followed by options */
};

/* Destination options header */
struct ip6_dest {
	uint8_t	ip6d_nxt;	/* next header */
	uint8_t	ip6d_len;	/* length in units of 8 octets */
	/* followed by options */
};

/* Routing header */
struct ip6_rthdr {
	uint8_t	ip6r_nxt;	/* next header */
	uint8_t	ip6r_len;	/* length in units of 8 octets */
	uint8_t	ip6r_type;	/* routing type */
	uint8_t	ip6r_segleft;	/* segments left */
	/* followed by routing type specific data */
};

/* Type 0 Routing header */
struct ip6_rthdr0 {
	uint8_t	ip6r0_nxt;	/* next header */
	uint8_t	ip6r0_len;	/* length in units of 8 octets */
	uint8_t	ip6r0_type;	/* always zero */
	uint8_t	ip6r0_segleft;	/* segmentis left */
	uint8_t	ip6r0_reserved;	/* reserved field */
	uint8_t	ip6r0_slmap[3];	/* strict/loose bit map */
	struct in6_addr	ip6r0_addr[1];	/* up to 23 adresses */
};

/* Fragment header */
struct ip6_frag {
	uint8_t	ip6f_nxt;	/* next header */
	uint8_t	ip6f_len;	/* length in units of 8 octets */
	uint8_t	ip6f_offlg;	/* offset, reserved, and flag */
	uint32_t ip6r_ident;	/* identification */
};

#if BYTE_ORDER == BIG_ENDIAN
#define IP6F_OFF_MASK		0xfff8	/* mask out offset from ip6f_offlg */
#define IP6F_RESERVED_AMSK	0x0006	/* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG		0x0001	/* more fragments flag */
#else /* BYTE_ORDER == LITTLE_ENDIAN */
#define IP6F_OFF_MASK		0xf8ff	/* mask out offset from ip6f_offlg */
#define IP6F_RESERVED_AMSK	0x0600	/* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG		0x0100	/* more fragments flag */
#endif

#endif
