/*
 * Multicast traceroute related definitions
 *
 * mtrace.h,v 5.1 1996/12/19 21:31:26 fenner Exp
 */

/*
 * The packet format for a traceroute request.
 */
struct tr_query {
    u_int32  tr_src;		/* traceroute source */
    u_int32  tr_dst;		/* traceroute destination */
    u_int32  tr_raddr;		/* traceroute response address */
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
    struct {
	u_int	qid : 24;	/* traceroute query id */
	u_int	ttl : 8;	/* traceroute response ttl */
    } q;
#else
    struct {
	u_int   ttl : 8;	/* traceroute response ttl */
	u_int   qid : 24;	/* traceroute query id */
    } q;
#endif /* BYTE_ORDER */
};

#define tr_rttl q.ttl
#define tr_qid  q.qid

/*
 * Traceroute response format.  A traceroute response has a tr_query at the
 * beginning, followed by one tr_resp for each hop taken.
 */
struct tr_resp {
    u_int32 tr_qarr;		/* query arrival time */
    u_int32 tr_inaddr;		/* incoming interface address */
    u_int32 tr_outaddr;		/* outgoing interface address */
    u_int32 tr_rmtaddr;		/* parent address in source tree */
    u_int32 tr_vifin;		/* input packet count on interface */
    u_int32 tr_vifout;		/* output packet count on interface */
    u_int32 tr_pktcnt;		/* total incoming packets for src-grp */
    u_char  tr_rproto;		/* routing protocol deployed on router */
    u_char  tr_fttl;		/* ttl required to forward on outvif */
    u_char  tr_smask;		/* subnet mask for src addr */
    u_char  tr_rflags;		/* forwarding error codes */
};

/* defs within mtrace */
#define QUERY	1
#define RESP	2
#define QLEN	sizeof(struct tr_query)
#define RLEN	sizeof(struct tr_resp)

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR	0
#define TR_WRONG_IF	1
#define TR_PRUNED	2
#define TR_OPRUNED	3
#define TR_SCOPED	4
#define TR_NO_RTE	5
#define TR_NO_FWD	7
#define TR_HIT_RP	8
#define	TR_RPF_IF	9
#define	TR_NO_MULTI	10
#define TR_NO_SPACE	0x81
#define TR_OLD_ROUTER	0x82
#define	TR_ADMIN_PROHIB	0x83

/* fields for tr_rproto (routing protocol) */
#define PROTO_DVMRP		1
#define PROTO_MOSPF		2
#define PROTO_PIM		3
#define PROTO_CBT		4
#define PROTO_PIM_SPECIAL	5
#define PROTO_PIM_STATIC	6
#define PROTO_DVMRP_STATIC	7

#define VAL_TO_MASK(x, i) { \
			x = htonl(~((1 << (32 - (i))) - 1)); \
			};

#if defined(__STDC__) || defined(__GNUC__)
#define JAN_1970	2208988800UL	/* 1970 - 1900 in seconds */
#else
#define JAN_1970	2208988800L	/* 1970 - 1900 in seconds */
#define const		/**/
#endif
