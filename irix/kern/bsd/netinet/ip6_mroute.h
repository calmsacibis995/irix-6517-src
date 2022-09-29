/*
 * Copyright (c) 1989 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	@(#)ip_mroute.h	8.1 (Berkeley) 6/10/93
 * $Id: ip6_mroute.h,v 1.1 1997/09/15 20:01:43 ms Exp $
 */
/*
 * From Tom Pusateri's proposal for minimal multicast forwarding
 *
 * Francis.Dupont@inria.fr, march 1997
 */

#ifndef _NETINET_IP6_MROUTE_H_
#define _NETINET_IP6_MROUTE_H_

/*
 * Definitions for IPv6 multicast forwarding.
 *
 */

/*
 * IPv6 multicast pseudo socket address structure
 */

#define AF_MCAST6	AF_INET6

struct sockaddr_mcast6 {
	u_int8_t	sm6_len;	/* length */
	u_int8_t	sm6_family;	/* family (AF_INET6) */
	u_int16_t	sm6_res16;	/* reserved */
	u_int32_t	sm6_res32;	/* reserved */
	struct in6_addr	sm6_group;	/* multicast group */
	struct in6_addr	sm6_origin;	/* multicast origin */
};

/*
 * Multicast Routing set/getsockopt commands.
 */
#define	MFC6_INIT	160	/* initialize forwarder */
#define	MFC6_DONE	161	/* shut down forwarder */
#define MFC6_ADD_MFC	162	/* insert a forwarding cache entry */
#define MFC6_DEL_MFC	163	/* delete a forwarding cache entry */
#define MFC6_ADD_DSIF	164	/* add a downstream interface */
#define MFC6_DEL_DSIF	165	/* delete a downstream interface */
#define MFC6_ENA_MFC	166	/* enable a forwarding cache entry */
#define MFC6_DIS_MFC	167	/* disable a forwarding cache entry */
#define MFC6_VERSION	168	/* get kernel version number */
#define MFC6_INFO_MFC	169	/* get forwarding cache entry infos */

#define MFC6_MSG	160	/* IPv6 multicast router ancillary data */

/*
 * Argument structures for multicast forwarding cache enties get/setsockopt
 */
struct mfc6ctl {
    struct in6_addr  mfc6c_origin;	/* IPv6 origin of mcasts */
    struct in6_addr  mfc6c_group; 	/* multicast group */
    u_short	     mfc6c_index;  	/* interface index */
    u_int16_t	     mfc6c_threshold;	/* threshold */
    u_int16_t	     mfc6c_scope;	/* scope */
};

struct mfc6info {
    struct in6_addr mfc6i_origin;	/* (given) origin */
    struct in6_addr mfc6i_group;	/* (given) group */
    u_short	    mfc6i_index;	/* upstream interface index */
    u_short	    mfc6i_ifnum;	/* number of downstream interfaces */
    u_long	    mfc6i_flags;	/* flags */
    u_long	    mfc6i_pktcnt;	/* packet counter */
    u_long	    mfc6i_bytecnt;	/* byte counter */
    u_long	    mfc6i_wrongif;	/* wrong interface counter */
    u_long	    mfc6i_lastuse;	/* last usage time */
};

struct mcast6_info {
    u_long	mi6_flags;		/* flags */
    int		mi6_errno;		/* error number */
    int		mi6_upindex;		/* upstream interface index */
    int		mi6_rcvindex;		/* incoming interface index */
};

#ifdef _KERNEL

/*
 * The kernel's multicast IPv6 forwarding cache entry structure 
 */
struct mfc6entry {
    struct radix_node	mfc6_nodes[2];		/* tree glue */
#define	mfc6_key(r)	((struct sockaddr_mcast6 *)((r)->mfc6_nodes->rn_key))
    struct ifnet	*mfc6_upstream;		/* incoming interface */
    struct {
            struct ds_ifaddr *stqh_first; /* first element */
	    struct ds_ifaddr **stqh_last; /* addr of last next element */
    } mfc6_ds_list;      /* downstream interfaces */
    u_long		mfc6_flags;		/* flags (RTF_*) */
    u_long		mfc6_pktcnt;		/* packet counter */
    u_long		mfc6_bytecnt;		/* byte counter */
    u_long		mfc6_wrongif;		/* wrong if counter */
    u_long		mfc6_lastuse;		/* last usage time */
    struct mbuf		*mfc6_hold;		/* last pending packet */
};

struct ds_ifaddr {
    struct {
            struct ds_ifaddr *stqe_next; /* next element */
    } ds_list;		/* chaining */
    struct ifnet	*ds_ifp;		/* the interface to use */
    u_int16_t		ds_threshold;		/* hop limit threshold */
    u_int16_t		ds_scope;		/* scope */
};

int	ip6_mrouter_set __P((int, struct socket *, struct mbuf *));
int	ip6_mrouter_get __P((int, struct socket *, struct mbuf **));
int	ip6_mrouter_done __P((void));
int	mrt6_ioctl __P((int, caddr_t, struct proc *));

#endif /* _KERNEL */

#endif /* _NETINET_IP6_MROUTE_H_ */
