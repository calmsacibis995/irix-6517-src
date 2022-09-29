/*
 *      Copyright (C) 1996, 1995, 1994 Transarc Corporation
 *      All rights reserved.
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_serverpref.h,v 65.1 1997/10/20 19:17:25 jdoak Exp $
 */

#ifndef	TRANSARC_CM_SERVERPREFH_
#define	TRANSARC_CM_SERVERPREFH_

#include <dcedfs/osi_net.h>

/*
 * User interface (pioctl) carried structures.
 */
struct spref {
    struct sockaddr_in	host;	/* Socket address in network order */
    u_short		rank; 	/* Address rank */
    u_short		flags; 	/* Flags for various types of prefs */
};

/* Flag values for struct spref */
#define CM_PREF_MASK	0xff
#define CM_PREF_FLDB	0x01
#define CM_PREF_FX	0x02
#define CM_PREF_REP	0x03
#define CM_PREF_QUEUED	0x04

struct sprefrequest {
    unsigned	offset;
    unsigned	num_servers;
};

struct sprefinfo {
    unsigned		next_offset;
    unsigned		num_servers;
    struct spref	servers[1];     /* this will be overrun */
};

/*
 * Default address ranks for network adjacency
 */
#define CM_RANK_DEF_SAMEHOST 	 5000
#define CM_RANK_DEF_SAMESUBNET 	20000
#define CM_RANK_DEF_SAMENET	30000
#define CM_RANK_DEF_OTHER	40000


#if defined(KERNEL) || defined(CM_USERTEST)
/*
 * Queued address-rank request.
 */
typedef struct cm_addrRank {
	struct in_addr		addr;    /* IP address in network order */
	u_short			svc;     /* Server type (SRT_FL,REP,FX) */
	u_short			rank;  	 /* Address rank */
	struct cm_addrRank	*next;   /* Next in list */
	struct cm_addrRank	*prev;   /* Previous in list */
} cm_addrRank_t;

/*
 * Address-rank queue and associated lock.
 */
extern osi_dlock_t    cm_addrRankReqsLock;
extern cm_addrRank_t  *cm_addrRankReqs;

/* 
 * Functions exported by preference module; see cm_serverpref.c
 */
extern void
cm_AddrRankPut(struct sockaddr_in *addrp,
	       u_short svc,
	       u_short rank);

extern int
cm_AddrRankGet(struct sockaddr_in *addrp,
	       u_short svc,
	       u_short *rank);

extern u_short
cm_AddrRankCompute(struct sockaddr_in *addrp);

#endif /* KERNEL || CM_USERTEST */
#endif /* TRANSARC_CM_SERVERPREFH_ */
