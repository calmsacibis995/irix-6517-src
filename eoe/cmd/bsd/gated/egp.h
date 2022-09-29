/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/egp.h,v 1.1 1989/09/18 19:01:55 jleong Exp $
 *
 */

/* egp.h
 *
 * Uses: defs.h
 *
 * Structure egpngh stores state information for an EGP neighbor. There is 
 * one such structure allocated at initialization for each of the trusted EGP
 * neighbors read from the initialization file. The egpngh structures are in a
 * singly linked list pointed to by external variable "egpngh". 
 * The major states of a neighbor are ACQUIRED, DOWN, UP and 
 * CEASE. In the UP state there are three reachability substates 
 * according to whether the neighbor and this gateway both consider each other
 * up (BOTH_UP), this gateway considers its neighbor down (NG_DOWN), or the 
 * neighbor has reported that it considers this gateway down (ME_DOWN).
 */


struct egpngh {
	struct	egpngh	 *ng_next;	/* next state table of linked list */
	int	ng_state;
	int	ng_reach;		/* reachability substate */
	u_int	ng_flags;
	u_char	ng_status;		/* Info saved for cease retransmission */
	struct in_addr	ng_myaddr;	/* My inet address for common net */
	struct in_addr	ng_addr;	/* Neighbor's inet address */
	u_short	ng_sid;			/* seq.# next acq., hello or poll to send */
	u_short ng_rid;			/* seq.# last acq., hello or poll recvd */

	/* Acquire and hello info */
	int	ng_retry;		/* For states DOWN and CEASE,
					  No. retries to acquire or cease neighbor.
					  For UP state, no. consecutive times
						neighbor is unreachable */
	int	ng_hint;		/* Send acquire, hello or cease interval 
						in seconds */
	long	ng_htime;		/* Time for next acquire, hello, or cease */
	int	ng_responses;		/* Shift register of responses for determining
					reachability, each set bit corresponds to a
					response, each zero to no response */
	int	ng_rcmd;		/* No. acq., hello and poll commands recvd
					   since lats check */

	/* NR Poll info */
	int	ng_snpoll;		/* No. sends of this NR poll id */
	int	ng_spint;		/* Send NR poll interval (in seconds) */
	long	ng_stime;		/* Time to send next NR poll */
	int	ng_runsol;		/* No. unsolicited NR msgs received for 
					   previous id */
	int	ng_noupdate;		/* # successive polls (new id) which did not
					   receive a valid update */
	int	ng_rnpoll;		/* No. polls received before next poll time */
	int	ng_rpint;		/* Rcv NR poll interval (in seconds) */
	long	ng_rtime;		/* Time to rcv next NR poll */
	struct	in_addr	ng_paddr;	/* Last address he polled */
	struct	in_addr	ng_saddr;	/* Address I should poll */
	struct	in_addr	ng_gateway;	/* Address of local gateway */
	int	ng_metricin;		/* Metric to use for all incoming nets */
	u_char	ng_metricout;		/* Metric to use for all outgoing nets */
	u_short	ng_asin;		/* AS number our neighbor should have */
	u_short ng_asout;		/* AS number we should tell our neighbor */
	u_short ng_as;			/* AS number we assign to routes */
	u_short ng_defaultmetric;	/* Metric to use for default */
	struct as_list *ng_aslist;	/* Pointer to ASes whose networks we can send to our AS */
	};

#define egpng	struct egpngh

/* States */
#define NGS_IDLE		0
#define NGS_ACQUISITION		1
#define NGS_DOWN		2
#define NGS_UP			3
#define NGS_CEASE		4

/* reachability substates for state UP */
#define BOTH_UP	0
#define NG_DOWN	1	/* according to me my peer is unreachable */
#define ME_DOWN	2	/* according to my peer I am unreachable */

/* flags */
#define NG_BAD		0x01	/* change to state BAD when cease complete */
#define NG_METRICIN	0x02	/* Use an inbound metric */
#define NG_METRICOUT	0x04	/* Use and outbound metric */
#define NG_ASIN		0x08	/* Verify inbound AS number */
#define NG_ASOUT	0x10	/* Use this outbound AS number */
#define NG_NOGENDEFAULT	0x20	/* Don't consider this neighbor for default generation */
#define NG_DEFAULTIN	0x40	/* Allow default in an NG packet */
#define NG_DEFAULTOUT	0x80	/* Send DEFAULT net to this neighbor */
#define NG_VALIDATE	0x100	/* Incoming nets must have validate clause */
#define	NG_INTF		0x200	/* Interface was specified */
#define	NG_SADDR	0x400	/* IP Source Network was specified */
#define	NG_GATEWAY	0x800	/* Address of local gateway to Source Network */
#define	NG_AS		0x1000	/* Use different AS number for routes */

/* Basic EGP packet */
#define egpkt	struct egppkt

egpkt	{	u_char	egp_ver;	/* Version # */
		u_char	egp_type;	/* Opcode */
		u_char	egp_code;
		u_char	egp_status;
		u_short	egp_chksum;
		u_short	egp_system;	/* Autonomous system */
		u_short	egp_id;
		};

/* EGP neighbor acquisition packet */
struct egpacq	{
	egpkt	ea_pkt;
	u_short	ea_hint;	/* Hello interval in seconds */
	u_short	ea_pint;	/* NR poll interval in seconds */
	};

/* EGP NR poll packet */
struct egppoll	{
	egpkt	ep_pkt;
	u_short	ep_unused;
	struct in_addr	ep_net;		/* Source net */
	};

/* EGP NR Message packet */
struct egpnr	{
	egpkt	en_pkt;
	u_char	en_igw;		/* No. internal gateways */
	u_char	en_egw;		/* No. external gateways */
	struct in_addr	en_net;		/* shared net */
	};

#define NRMAXNETUNIT 9		/* maximum size per net in octets of net part
				of NR message */
/* EGP Error packet */
struct egperr	{
	egpkt	ee_pkt;
	u_short ee_rsn;
	u_char 	ee_egphd[12];	/* First 12 bytes of bad egp pkt */
};

#define EGPLEN	(sizeof(egpkt))
#define EGPVER	2
#define	EGPVMASK	0x01	/* We only speak version 2 */

/* EGP Types */
#define EGPNR		1
#define	EGPPOLL		2
#define EGPACQ		3
#define EGPHELLO	5
#define	EGPERR		8

/* Neighbor Acquisition Codes */
#define NAREQ	0	/* Neighbor acq. request */
#define NACONF	1	/* Neighbor acq. confirmation */
#define NAREFUS	2	/* Neighbor acq. refuse */
#define NACEASE	3	/* Neighbor cease */
#define NACACK	4	/* Neighbor cease ack */

/* Neighbor Acquisition Message Status Info */
#define UNSPEC		0
#define	ACTIVE		1
#define	PASSIVE		2
#define	NORESOURCE	3
#define	ADMINPROHIB	4
#define	GODOWN		5
#define	PARAMPROB	6
#define	PROTOVIOL	7

/* Neighbor Hello Codes */
#define HELLO	0
#define HEARDU	1

/* Reachability, poll and update status */
#define INDETERMINATE	0
#define UP		1
#define DOWN		2
#define UNSOLICITED   128

/* Error reason status */
#define	EUNSPEC		0
#define EBADHEAD	1
#define	EBADDATA	2
#define ENOREACH	3
#define	EXSPOLL		4
#define ENORESPONSE	5
#define	EUVERSION	6

/* Calculate the AS number to be used with this neighbor */
#define MY_AS(ngp) htons( ngp && (ngp->ng_flags & NG_ASOUT) ? ngp->ng_asout : mysystem )
