/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * vif.h,v 3.8.4.18 1997/05/16 19:43:38 fenner Exp
 */


/*
 * Bitmap handling functions.
 * These should be fast but generic.  bytes can be slow to zero and compare,
 * words are hard to make generic.  Thus two sets of macros (yuk).
 */

/*
 * The VIFM_ functions should migrate out of <netinet/ip_mroute.h>, since
 * the kernel no longer uses vifbitmaps.
 */
#ifndef VIFM_SET
typedef	u_long vifbitmap_t;

#define	VIFM_SET(n, m)			((m) |=  (1 << (n)))
#define	VIFM_CLR(n, m)			((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)		((m) &   (1 << (n)))
#define VIFM_CLRALL(m)			((m) = 0x00000000)
#define VIFM_COPY(mfrom, mto)		((mto) = (mfrom))
#define VIFM_SAME(m1, m2)		((m1) == (m2))
#endif
/*
 * And <netinet/ip_mroute.h> was missing some required functions anyway
 */
#define	VIFM_SETALL(m)			((m) = ~0)
#define	VIFM_ISSET_ONLY(n, m)		((m) == (1 << (n)))
#define	VIFM_ISEMPTY(m)			((m) == 0)
#define	VIFM_CLR_MASK(m, mask)		((m) &= ~(mask))
#define	VIFM_SET_MASK(m, mask)		((m) |= (mask))

/*
 * Neighbor bitmaps are, for efficiency, implemented as a struct
 * containing two variables of a native machine type.  If you
 * have a native type that's bigger than a long, define it below.
 */
#define	NBRTYPE		u_long
#define NBRBITS		sizeof(NBRTYPE) * 8

typedef struct {
    NBRTYPE hi;
    NBRTYPE lo;
} nbrbitmap_t;
#define	MAXNBRS		2 * NBRBITS

#define	NBRM_SET(n, m)		(((n) < NBRBITS) ? ((m).lo |= (1 << (n))) : \
				      ((m).hi |= (1 << (n - NBRBITS))))
#define	NBRM_CLR(n, m)		(((n) < NBRBITS) ? ((m).lo &= ~(1 << (n))) : \
				      ((m).hi &= ~(1 << (n - NBRBITS))))
#define	NBRM_ISSET(n, m)	(((n) < NBRBITS) ? ((m).lo & (1 << (n))) : \
				      ((m).hi & (1 << ((n) - NBRBITS))))
#define	NBRM_CLRALL(m)		((m).lo = (m).hi = 0)
#define	NBRM_COPY(mfrom, mto)	((mto).lo = (mfrom).lo, (mto).hi = (mfrom).hi)
#define	NBRM_SAME(m1, m2)	(((m1).lo == (m2).lo) && ((m1).hi == (m2).hi))
#define	NBRM_ISEMPTY(m)		(((m).lo == 0) && ((m).hi == 0))
#define	NBRM_SETMASK(m, mask)	(((m).lo |= (mask).lo),((m).hi |= (mask).hi))
#define	NBRM_CLRMASK(m, mask)	(((m).lo &= ~(mask).lo),((m).hi &= ~(mask).hi))
#define	NBRM_MASK(m, mask)	(((m).lo &= (mask).lo),((m).hi &= (mask).hi))
#define	NBRM_ISSETMASK(m, mask)	(((m).lo & (mask).lo) || ((m).hi & (mask).hi))
#define	NBRM_ISSETALLMASK(m, mask)\
				((((m).lo & (mask).lo) == (mask).lo) && \
				 (((m).hi & (mask).hi) == (mask).hi))
/*
 * This macro is TRUE if all the subordinates have been pruned, or if
 * there are no subordinates on this vif.
 * The arguments is the map of subordinates, the map of neighbors on the
 * vif, and the map of received prunes.
 */
#define	SUBS_ARE_PRUNED(sub, vifmask, prunes)	\
    (((sub).lo & (vifmask).lo) == ((prunes).lo & (vifmask).lo & (sub).lo) && \
     ((sub).hi & (vifmask).hi) == ((prunes).hi & (vifmask).hi & (sub).hi))

/*
 * User level Virtual Interface structure
 *
 * A "virtual interface" is either a physical, multicast-capable interface
 * (called a "phyint") or a virtual point-to-point link (called a "tunnel").
 * (Note: all addresses, subnet numbers and masks are kept in NETWORK order.)
 */
struct uvif {
    u_int	     uv_flags;	    /* VIFF_ flags defined below            */
    u_char	     uv_metric;     /* cost of this vif                     */
    u_char	     uv_admetric;   /* advertised cost of this vif          */
    u_char	     uv_threshold;  /* min ttl required to forward on vif   */
    u_int	     uv_rate_limit; /* rate limit on this vif               */
    u_int32	     uv_lcl_addr;   /* local address of this vif            */
    u_int32	     uv_rmt_addr;   /* remote end-point addr (tunnels only) */
    u_int32	     uv_dst_addr;   /* destination for DVMRP messages       */
    u_int32	     uv_subnet;     /* subnet number         (phyints only) */
    u_int32	     uv_subnetmask; /* subnet mask           (phyints only) */
    u_int32	     uv_subnetbcast;/* subnet broadcast addr (phyints only) */
    char	     uv_name[IFNAMSIZ]; /* interface name                   */
    struct listaddr *uv_groups;     /* list of local groups  (phyints only) */
    struct listaddr *uv_neighbors;  /* list of neighboring routers          */
    nbrbitmap_t	     uv_nbrmap;	    /* bitmap of active neighboring routers */
    struct listaddr *uv_querier;    /* IGMP querier on vif                  */
    int		     uv_igmpv1_warn;/* To rate-limit IGMPv1 warnings	    */
    int		     uv_prune_lifetime; /* Prune lifetime or 0 for default  */
    struct vif_acl  *uv_acl;	    /* access control list of groups        */
    int		     uv_leaf_timer; /* time until this vif is considrd leaf */
    struct phaddr   *uv_addrs;	    /* Additional subnets on this vif       */
    struct vif_filter *uv_filter;   /* Route filters on this vif	    */
};

#define VIFF_KERNEL_FLAGS	(VIFF_TUNNEL|VIFF_SRCRT)
#define VIFF_DOWN		0x000100	/* kernel state of interface */
#define VIFF_DISABLED		0x000200	/* administratively disabled */
#define VIFF_QUERIER		0x000400	/* I am the subnet's querier */
#define VIFF_ONEWAY		0x000800	/* Maybe one way interface   */
#define VIFF_LEAF		0x001000	/* all neighbors are leaves  */
#define VIFF_IGMPV1		0x002000	/* Act as an IGMPv1 Router   */
#define	VIFF_REXMIT_PRUNES	0x004000	/* retransmit prunes         */
#define VIFF_PASSIVE		0x008000	/* passive tunnel	     */
#define	VIFF_ALLOW_NONPRUNERS	0x010000	/* ok to peer with nonprunrs */
#define VIFF_NOFLOOD		0x020000	/* don't flood on this vif   */

struct phaddr {
    struct phaddr   *pa_next;
    u_int32	     pa_subnet;		/* extra subnet			*/
    u_int32	     pa_subnetmask;	/* netmask of extra subnet	*/
    u_int32	     pa_subnetbcast;	/* broadcast of extra subnet	*/
};

struct vif_acl {
    struct vif_acl  *acl_next;	    /* next acl member         */
    u_int32	     acl_addr;	    /* Group address           */
    u_int32	     acl_mask;	    /* Group addr. mask        */
};

struct vif_filter {
    int			vf_type;
#define	VFT_ACCEPT	1
#define	VFT_DENY	2
    int			vf_flags;
#define	VFF_BIDIR	1
    struct vf_element  *vf_filter;
};

struct vf_element {
    struct vf_element  *vfe_next;
    u_int32		vfe_addr;
    u_int32		vfe_mask;
    int			vfe_flags;
#define	VFEF_EXACT	0x0001
};

struct listaddr {
    struct listaddr *al_next;		/* link to next addr, MUST BE FIRST */
    u_int32	     al_addr;		/* local group or neighbor address  */
    u_long	     al_timer;		/* for timing out group or neighbor */
    time_t	     al_ctime;		/* entry creation time		    */
    union {
    	u_int32	     alu_genid;		/* generation id for neighbor       */
    	u_int32	     alu_reporter;	/* a host which reported membership */
    } al_alu;
    u_char	     al_pv;		/* router protocol version	    */
    u_char	     al_mv;		/* router mrouted version	    */
    u_char	     al_old;            /* time since heard old report      */
    u_char	     al_index;		/* neighbor index		    */
    u_long	     al_timerid;        /* timer for group membership	    */
    u_long	     al_query;		/* timer for repeated leave query   */
    u_short	     al_flags;		/* flags related to this neighbor   */
};
#define	al_genid	al_alu.alu_genid
#define	al_reporter	al_alu.alu_reporter

#define	NBRF_LEAF		0x0001	/* This neighbor is a leaf 	    */
#define	NBRF_GENID		0x0100	/* I know this neighbor's genid	    */
#define	NBRF_WAITING		0x0200	/* Waiting for peering to come up   */
#define	NBRF_ONEWAY		0x0400	/* One-way peering 		    */
#define	NBRF_TOOOLD		0x0800	/* Too old (policy decision) 	    */
#define	NBRF_TOOMANYROUTES	0x1000	/* Neighbor is spouting routes 	    */
#define	NBRF_NOTPRUNING		0x2000	/* Neighbor doesn't appear to prune */

/*
 * Don't peer with neighbors with any of these flags set
 */
#define	NBRF_DONTPEER		(NBRF_WAITING|NBRF_ONEWAY|NBRF_TOOOLD| \
				 NBRF_TOOMANYROUTES|NBRF_NOTPRUNING)

#define NO_VIF		((vifi_t)MAXVIFS)  /* An invalid vif index */
