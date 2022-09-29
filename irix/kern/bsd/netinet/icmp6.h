#ifndef _NETINET_ICMP6_H_
#define _NETINET_ICMP6_H_

/*
 * Header for the BSD advanced API
 */

struct icmp6_hdr {
    u_int8_t		icmp6_type;	/* type field */
    u_int8_t		icmp6_code;	/* code field */
    u_int16_t		icmp6_cksum;	/* checksum field */
    union {
	u_int32_t	icmp6_un_data32[1];	/* type-specific field */
	u_int16_t	icmp6_un_data16[2];	/* type-specific field */
	u_int8_t	icmp6_un_data8[4];	/* type-specific field */
    } icmp6_dataun;
};

#define icmp6_data32	icmp6_dataun.icmp6_un_data32
#define icmp6_data16	icmp6_dataun.icmp6_un_data16
#define icmp6_data8	icmp6_dataun.icmp6_un_data8
#define icmp6_pptr	icmp6_data32[0]	/* parameter prob */
#define icmp6_mtu	icmp6_data32[0]	/* packet too big */
#define icmp6_id	icmp6_data16[0]	/* echo request/reply */
#define icmp6_seq	icmp6_data16[1]	/* echo request/reply */
#define icmp6_maxdelay	icmp6_data16[0]	/* mcast group membership */

#define ICMP6_DST_UNREACH	1
#define ICMP6_PACKET_TOO_BIG	2
#define ICMP6_TIME_EXCEEDED	3
#define ICMP6_PARAM_PROB	4

#define ICMP6_INFOMSG_MASK	0x80	/* all information messages */

#define ICMP6_ECHO_REQUEST		128
#define ICMP6_ECHO_REPLY		129
#define ICMP6_MEMBERSHIP_QUERY		130
#define ICMP6_MEMBERSHIP_REPORT		131
#define ICMP6_MEMBERSHIP_REDUCTION	132

#define ICMP6_DST_UNREACH_NOROUTE	0  /* no route to destination */
#define ICMP6_DST_UNREACH_ADMIN		1  /* communication with destination
					    * administratively prohibited */
#define ICMP6_DST_UNREACH_NOTNEIGHBOR	2  /* not a neighbor */
#define ICMP6_DST_UNREACH_ADDR		3  /* address unreachable */
#define ICMP6_DST_UNREACH_NOPORT	4  /* bad port */

#define ICMP6_TIME_EXCEEDED_TRANSIT	0  /* Hop Limit == 0 in transit */
#define ICMP6_TIME_EXCEEDED_REASSEMBLY	1  /* Reassembly time out */

#define ICMP6_PARAMPROB_HEADER		0  /* erroneous header field */
#define ICMP6_PARAMPROB_NEXTHEADER	1  /* unreconized Next Header */
#define ICMP6_PARAMPROB_OPTION		2  /* unreconized IPv6 option */

#define ND_ROUTER_SOLICIT		133
#define ND_ROUTER_ADVERT		134
#define ND_NEIGHBOR_SOLICIT		135
#define ND_NEIGHBOR_ADVERT		136
#define ND_REDIRECT			137

struct nd_router_solicit {	/* router solicitation */
    struct icmp6_hdr nd_rs_hdr;
    /* could be followed by options */
};

#define nd_rs_type	nd_rs_hdr.icmp6_type
#define nd_rs_code	nd_rs_hdr.icmp6_code
#define nd_rs_cksum	nd_rs_hdr.icmp6_cksum
#define nd_rs_reserved	nd_rs_hdr.icmp6_data32[0]

struct nd_router_advert {	/* router advertisement */
    struct icmp6_hdr nd_ra_hdr;
    u_int32_t	nd_ra_reachable;	/* reachable time */
    u_int32_t	nd_ra_retransmit;	/* reachable retransmit time */
    /* could be followed by options */
};

#define nd_ra_type		nd_ra_hdr.icmp6_type
#define nd_ra_code		nd_ra_hdr.icmp6_code
#define nd_ra_cksum		nd_ra_hdr.icmp6_cksum
#define nd_ra_curhoplimit	nd_ra_hdr.icmp6_data8[0]
#define nd_ra_flags_reserved	nd_ra_hdr.icmp6_data8[1]
#define ND_RA_FLAG_MANAGED	0x80
#define ND_RA_FLAG_OTHER	0x40
#define nd_ra_router_lifetime	nd_ra_hdr.icmp6_data16[1]

struct nd_neighbor_solicit {	/* neighbor solicitation */
    struct icmp6_hdr	nd_ns_hdr;
    struct in6_addr	nd_ns_target; /* target address */
    /* could be followed by options */
};

#define nd_ns_type		nd_ns_hdr.icmp6_type
#define nd_ns_code		nd_ns_hdr.icmp6_code
#define nd_ns_cksum		nd_ns_hdr.icmp6_cksum
#define nd_ns_reserved		nd_ns_hdr.icmp6_data32[0]

struct nd_neighbor_advert {	/* neighbor advertisement */
    struct nd_na_hdr	nd_na_hdr;
    struct in6_addr	nd_na_target; /* target address */
};

#define nd_na_type		nd_na_hdr.icmp6_type
#define nd_na_code		nd_na_hdr.icmp6_code
#define nd_na_cksum		nd_na_hdr.icmp6_cksum
#define nd_na_flags_reserved	nd_na_hdr.icmp6_data32[0]
#if BYTE_ORDER == BIG_ENDIAN
#define ND_NA_FLAG_ROUTER		0x80000000
#define ND_NA_FLAG_SOLICITED		0x40000000
#define ND_NA_FLAG_OVERRIDE		0x20000000
#else /* #if BYTE_ORDER == LITTLE_ENDIAN */
#define ND_NA_FLAG_ROUTER		0x00000080
#define ND_NA_FLAG_SOLICITED		0x00000040
#define ND_NA_FLAG_OVERRIDE		0x00000020
#endif

struct nd_redirect {		/* redirect */
    struct icmp6_hdr	nd_rd_hdr;
    struct in6_addr	nd_rd_target; /* target address */
    struct in6_addr	nd_rd_dst;    /* destination address */
    /* could be followed by options */
};

#define nd_rd_type		nd_rd_hdr.icmp6_type
#define nd_rd_code		nd_rd_hdr.icmp6_code
#define nd_rd_cksum		nd_rd_hdr.icmp6_cksum
#define nd_rd_reserved		nd_rd_hdr.icmp6_data32[0]

struct nd_opt_hdr {		/* Neighbor discovery option header */
    uint8_t	nd_opt_type;
    uint8_t	nd_opt_len;    /* in units of 8 octets */
    /* followed by option specific data */
};

#define ND_OPT_SOURCE_LINKADDR		1
#define ND_OPT_TARGET_LINKADDR		2
#define ND_OPT_PREFIX_INFORMATION	3
#define ND_OPT_REDIRECTED_HEADER	4
#define ND_OPT_MTU			5

struct nd_opt_prefix_info {	/* prefix information */
    uint8_t	nd_opt_pi_type;
    uint8_t	nd_opt_pi_len;
    uint8_t	nd_opt_pi_prefix_len;
    uint8_t	nd_opt_pi_flags_reserved;
    uint32_t	nd_opt_pi_valid_time;
    uint32_t	nd_opt_pi_preferred_time;
    uint32_t	nd_opt_pi_reserved2;
    struct in6_addr nd_opt_pi_prefix;
};

#define ND_OPT_PI_FLAG_ONLINK	0x80
#define ND_OPT_PI_FLAG_AUTO	0x40

struct nd_opt_rd_hdr {	/* redirected header */
    uint8_t	nd_opt_rh_type;
    uint8_t	nd_opt_rh_len;
    uint32_t	nd_opt_rh_reserved1;
    uint32_t	nd_opt_rh_reserved2;
    /* followed by IP header and data */
};

struct nd_opt_mtu {		/* MTU option */
    uint8_t	nd_opt_mtu_type;
    uint8_t	nd_opt_mtu_length;
    uint16_t	nd_opt_mtu_reserved;
    uint32_t	nd_opt_mtu_mtu;
};



/* icmp6 filtering */

struct ocmp6_filter {
	uint32_t icmp6_filt[8];		/* 8*32 = 256 bits */
};

#define ICMP6_FILTER_SETPASSALL(filtp) {\
	(filtp)->icmp6_filt[1] = (filtp)->icmp6_filt[0] = 0xff; \
	(filtp)->icmp6_filt[3] = (filtp)->icmp6_filt[2] = 0xff; \
	(filtp)->icmp6_filt[5] = (filtp)->icmp6_filt[4] = 0xff; \
	(filtp)->icmp6_filt[7] = (filtp)->icmp6_filt[6] = 0xff; \
	}

#define ICMP6_FILTER_SETBLOCKALL(filtp) {\
	(filtp)->icmp6_filt[1] = (filtp)->icmp6_filt[0] = 0; \
	(filtp)->icmp6_filt[3] = (filtp)->icmp6_filt[2] = 0; \
	(filtp)->icmp6_filt[5] = (filtp)->icmp6_filt[4] = 0; \
	(filtp)->icmp6_filt[7] = (filtp)->icmp6_filt[6] = 0; \
	}

#define ICMP6_FILTER_SETPASS(type, filtp) \
	((filtp)->icmp6_filt[(type) >> 5] |= (1 << ((type) & 31)))

#define ICMP6_FILTER_SETBLOCK(type, filtp) \
	((filtp)->icmp6_filt[(type) >> 5] &= ~(1 << ((type) & 31)))


#define ICMP6_FILTER_WILLPASS(type, filtp) \
	(((filtp)->icmp6_filt[(type) >> 5] & (1 << ((type) & 31))) != 0)

#define ICMP6_FILTER_WILLBLOCK(type, filtp) \
	(((filtp)->icmp6_filt[(type) >> 5] & (1 << ((type) & 31))) == 0)

#endif
