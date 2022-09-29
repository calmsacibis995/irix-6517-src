/*
 * Copyright (C) 1996 Transarc Corporation - All rights reserved.
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_site.h,v 65.1 1997/10/20 19:17:26 jdoak Exp $
 */

/* cm_site.h -- site module.
 *
 *   This module implements the site object, which represents the state
 *   of a server's network connections on a multi-homed host (site).
 *   Site objects are used in conjunction with server (cm_server) and
 *   connection (cm_conn) objects to implement flexible, fault-tolerant
 *   RPC within the DFS cache manager.
 *
 *   See OSF-RFC 77 for further details.
 */

#ifndef TRANSARC_CM_SITE_H
#define TRANSARC_CM_SITE_H

/*
 * struct cm_siteAddr -- site-address declaration.  Represents an individual
 *     site address, including its (server-centric) rank and state.
 */

struct cm_siteAddr{
    struct cm_siteAddr    *next_sahash;   /* next address in hash table */
    struct cm_siteAddr    *next_sa;       /* next site address (for server) */
    struct cm_site        *sitep;         /* site back-pointer */
    struct sockaddr_in    addr;           /* address */
    u_short               rank;           /* address rank */
    u_short               state;          /* address state (SITEADDR_ flags) */
};

#define SITEADDR_KEEPER      0x01         /* keep site addr after update */
#define SITEADDR_DOWN        0x02         /* site addr is down */
#define SITEADDR_PINGED      0x04         /* site addr was pinged */


/*
 * struct cm_site -- site-object declaration.  Represents the state of
 *      a server's network connections on a multi-homed host (site).
 */

struct cm_site{
    struct cm_siteAddr    *addrListp;     /* address-list */
    struct cm_siteAddr    *addrCurp;      /* current address */
    u_short               svc;            /* address type (SRT_FL,REP,FX) */
    u_short               state;          /* site state (SITE_ flags) */
};

#define SITE_DOWNADDR     0x01            /* site has an addr marked down */
#define SITE_PINGING      0x02            /* down addrs are being pinged */

/* Note: addrCurp always set to the best-ranking up address, if any;
 *       otherwise, addrCurp set to the absolute best-ranking down address.
 *
 *       The only exception is in the case of a rank-override resulting
 *       from a conflicting transport-layer routing decision.
 *       See cm_SiteAddrRankOverride() for details.
 */


/*
 * cm_siteLock -- site-data lock declaration.  Lock that controls
 *     read/write access to all site-data for all servers.
 *
 *     Site-data is infrequently read and very rarely written, thus just
 *     a single lock can be used without compromising performance.
 *     Note that site-data is only read by operations that perform
 *     address selection (cm_ConnByHost()/cm_ConnByMHosts()), and is
 *     only written when:
 *         + a new server entry is allocated,
 *         + a server address change in the CDS/FLDB is noticed,
 *         + a new server address is learned via an RPC binding-address change,
 *         + a 'cm setpreferences' command is issued, or
 *         + server/network failure-revival activity occurs.
 *
 *     The cm_siteLock fits into the CM locking hierarchy as follows:
 *         + cm_celllock, cm_volumelock, cm_serverlock - global
 *         + cell locks                                - individual
 *         + volume locks                              - individual
 *         + server locks                              - individual
 *         + cm_addrRankReqsLock                       - global
 *         + cm_siteLock                               - global
 *
 *     Locking must be performed in the order listed; only the most
 *     immediately-enclosing locks are shown.
 */

extern osi_dlock_t cm_siteLock;


/*
 * cm_siteAddrGen -- address generation-counter declaration.
 *     Signals operations that select a particular server-address for
 *     communication (cm_ConnByHost()/cm_ConnByMHosts()) that this
 *     decision requires re-evaluation.
 *
 *     Site-module functions that change any site's addrCur addr/rank/state
 *     will increment cm_siteAddrGen.
 *
 *     Note that cm_siteAddrGen is protected by cm_siteLock.
 */

extern u_long cm_siteAddrGen;



/* Functions exported by the site module. See cm_site.c for descriptions. */

extern struct cm_site*
cm_SiteAlloc(u_short svc,
	     struct sockaddr_in *addrvp,
	     int addrvcnt);

extern void
cm_SiteAddrReplace(struct cm_site *sitep,
		   struct sockaddr_in *addrvp,
		   int addrvcnt);

extern void
cm_SiteAddrSetRankAll(struct sockaddr_in *addrp,
		      u_short svc,
		      u_short rank);

extern int
cm_SiteAddrDown(struct cm_site *sitep,
		struct sockaddr_in *addrp,
		int *wasUpp);

extern void
cm_SiteAddrUp(struct cm_site *sitep,
	      struct sockaddr_in *addrp,
	      int *wasDownp);

extern void
cm_SiteAddrMarkAllUp(struct cm_site *sitep,
		     int ifAllDown);

extern void
cm_SiteAddrRankOverride(struct cm_site *sitep,
			struct sockaddr_in *addrp,
			int *wasCurp);

#endif /* TRANSARC_CM_SITE_H */
