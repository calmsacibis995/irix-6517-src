#ifndef _NETINET_IP_MROUTE_H
#define _NETINET_IP_MROUTE_H
#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 * Definitions for IP multicast forwarding.
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Ajit Thyagarajan, PARC, August 1994.
 * Modified by Ahmed Helmy, SGI, June 1996.
 *
 * MROUTING  3.3.1.3
 *
 * SGI needs to have RSVP stubed at link time instead of compile:
 */
#define RSVP_ISI

/*
 * Multicast Routing set/getsockopt commands.
 */
#define MRT_INIT		100	/* initialize forwarder */
#define MRT_DONE		101	/* shut down forwarder */
#define MRT_ADD_VIF		102	/* create virtual interface */
#define MRT_DEL_VIF		103	/* delete virtual interface */
#define MRT_ADD_MFC		104	/* insert forwarding cache entry */
#define MRT_DEL_MFC		105	/* delete forwarding cache entry */
#define MRT_VERSION		106	/* get kernel version number */
#define MRT_PIM                 107     /* enable pim code */

/* Macro to get current time of day */
#define GET_TIME(t)	microtime(&t)

/*
 * Types and macros for handling bitmaps with one bit per virtual interface.
 * (Used in routing daemons only right now.)
 */
#define MAXVIFS 32
typedef u_int vifbitmap_t;
typedef u_short vifi_t;		/* type of a vif index */

#define	VIFM_SET(n, m)		((m) |=  (1 << (n)))
#define	VIFM_CLR(n, m)		((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)	((m) &   (1 << (n)))
#define VIFM_CLRALL(m)		((m) = 0x00000000)
#define VIFM_COPY(mfrom, mto)	((mto) = (mfrom))
#define VIFM_SAME(m1, m2)	((m1) == (m2))

#define ALL_VIFS (vifi_t)-1	/* "wild-card" interface */

/*
 * Argument structure for MRT_ADD_VIF.
 * (MRT_DEL_VIF takes a single vifi_t argument.)
 */
struct vifctl {
    vifi_t	    vifc_vifi;	    	/* the index of the vif to be added  */
    u_char	    vifc_flags;     	/* VIFF_ flags defined below         */
    u_char	    vifc_threshold; 	/* min ttl required to forward on vif*/
    u_int	    vifc_rate_limit;    /* max rate           		     */
    struct in_addr  vifc_lcl_addr;  	/* local interface address           */
    struct in_addr  vifc_rmt_addr;  	/* remote address (tunnels only)     */
};

#define VIFF_TUNNEL	  0x1	    	/* vif represents a tunnel end-point */
#define VIFF_SRCRT	  0x2	    	/* tunnel uses IP src routing	     */
#define VIFF_REGISTER     0x4           /* vif used for register encap/decap */

/*
 * Argument structure for MRT_ADD_MFC and MRT_DEL_MFC
 * (mfcc_tos to be added at a future point)
 */
struct mfcctl {
    struct in_addr  mfcc_origin;		/* ip origin of mcasts       */
    struct in_addr  mfcc_mcastgrp; 		/* multicast group associated*/
    vifi_t	    mfcc_parent;   		/* incoming vif              */
    u_char	    mfcc_ttls[MAXVIFS]; 	/* forwarding ttls on vifs   */
};

/*
 * Argument structure used by mrouted to get src-grp pkt counts
 */
struct sioc_sg_req {
    struct in_addr src;
    struct in_addr grp;
    u_int pktcnt;
    u_int bytecnt;
    u_int wrong_if;
};

/*
 * The kernel's multicast routing statistics.
 * See bsd/sys/tcpipstats.h for the struct mrtstat definition.
 */

/*
 * Argument structure used by mrouted to get vif pkt counts
 */
struct sioc_vif_req {
    vifi_t vifi;		/* vif number				*/
    u_int icount;		/* Input packet count on vif		*/
    u_int ocount;		/* Output packet count on vif		*/
    u_int ibytes;		/* Input byte count on vif		*/
    u_int obytes;		/* Output byte count on vif		*/
};

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * The kernel's virtual-interface structure.
 */
struct vif {
    u_char   		v_flags;     	/* VIFF_ flags defined above         */
    u_char   		v_threshold;	/* min ttl required to forward on vif*/
    u_int      		v_rate_limit; 	/* max rate			     */
    struct tbf 	       *v_tbf;       	/* token bucket structure at intf.   */
    struct in_addr 	v_lcl_addr;   	/* local interface address           */
    struct in_addr 	v_rmt_addr;   	/* remote address (tunnels only)     */
    struct ifnet       *v_ifp;	     	/* pointer to interface              */
    u_int		v_pkt_in;	/* # pkts in on interface            */
    u_int		v_pkt_out;	/* # pkts out on interface           */
    u_int		v_bytes_in;	/* # bytes in on interface	     */
    u_int		v_bytes_out;	/* # bytes out on interface	     */
    struct route	v_route;	/* Cached route if this is a tunnel  */
#ifdef RSVP_ISI
    u_int               v_rsvp_on;      /* # RSVP listening on this vif      */
    struct socket      *v_rsvpd;	/* # RSVPD daemon                    */
#endif /* RSVP_ISI */
};

/*
 * The kernel's multicast forwarding cache entry structure 
 * (A field for the type of service (mfc_tos) is to be added 
 * at a future point)
 */
struct mfc {
    struct in_addr  mfc_origin;	 		/* ip origin of mcasts       */
    struct in_addr  mfc_mcastgrp;  		/* multicast group associated*/
    vifi_t	    mfc_parent; 		/* incoming vif              */
    u_char	    mfc_ttls[MAXVIFS]; 		/* forwarding ttls on vifs   */
    u_int	    mfc_pkt_cnt;		/* pkt count for src-grp     */
    u_int	    mfc_byte_cnt;		/* byte count for src-grp    */
    u_int	    mfc_wrong_if;		/* wrong if for src-grp	     */
    int		    mfc_expire;			/* time to clean entry up    */
    struct timeval  mfc_last_assert;		/* last time I sent an assert*/
};

/*
 * Struct used to communicate from kernel to multicast router
 * note the convenient similarity to an IP packet
 */
struct igmpmsg {
    u_int	    unused1;
    u_int	    unused2;
    u_char	    im_msgtype;			/* what type of message	    */
#define IGMPMSG_NOCACHE		1
#define IGMPMSG_WRONGVIF	2
#define IGMPMSG_WHOLEPKT        3               /* used for user level encap*/
    u_char	    im_mbz;			/* must be zero		    */
    u_char	    im_vif;			/* vif rec'd on		    */
    u_char	    unused3;
    struct in_addr  im_src, im_dst;
};

/*
 * Argument structure used for pkt info. while upcall is made
 */
struct rtdetq {

    struct mbuf 	*m;		/* A copy of the packet	    	    */
    struct ifnet	*ifp;		/* Interface pkt came in on 	    */
#ifdef UPCALL_TIMING
    struct timeval	t;		/* Timestamp */
#endif /* UPCALL_TIMING */
};

#define MFCTBLSIZ	256
#if (MFCTBLSIZ & (MFCTBLSIZ - 1)) == 0	  /* from sys:route.h */
#define MFCHASHMOD(h)	((h) & (MFCTBLSIZ - 1))
#else
#define MFCHASHMOD(h)	((h) % MFCTBLSIZ)
#endif

#define MAX_UPQ	4		/* max. no of pkts in upcall Q */

/*
 * Token Bucket filter code 
 */
#define MAX_BKT_SIZE    20000           /* max packet size 		*/
#define MAXQSIZE        8		/* max # of pkts in queue 	*/

/*
 * queue structure at each vif
 */
struct pkt_queue 
{
    u_int pkt_len;		  /* length of packet in queue 	*/
    struct mbuf *pkt_m;           /* pointer to packet mbuf	*/
    struct ip  *pkt_ip;           /* pointer to ip header	*/
};

/*
 * the token bucket filter at each vif
 */
struct tbf
{
    u_int last_pkt_t;	/* arr. time of last pkt 	*/
    u_int n_tok;      	/* no of tokens in bucket 	*/
    u_int q_len;    	/* length of queue at this vif	*/
    mutex_t tbf_lock;	/* lock to protect this */
    struct pkt_queue qtable[MAXQSIZE];	/* the actual queue */
};

#endif /* _KERNEL || _KMEMUSER */

#ifdef _KERNEL
struct ifnet;

extern int ip_mforward(struct ip *, struct mbuf*, struct ifnet*,
		struct ip_moptions *imo);
extern int legal_vif_num(int vif);
extern int ip_rsvp_init(struct socket *);
extern int ip_rsvp_vif_init(struct socket *, struct mbuf *);
extern int ip_rsvp_vif_done(struct socket *, struct mbuf *);
extern int ip_mrouter_set(int, struct socket *, struct mbuf *);
extern int ip_mrouter_get(int, struct socket *, struct mbuf **);

#endif /* _KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* _NETINET_IP_MROUTE_H */
