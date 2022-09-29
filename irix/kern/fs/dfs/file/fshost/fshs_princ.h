/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */


#ifndef	_FSHS_PRINCIPALH_
#define	_FSHS_PRINCIPALH_

#include <dcedfs/xcred.h>
#include <dcedfs/fshs_deleg.h>		/* pac_list_t */

/*
 * fshs_principal's general macros
 */
#define	FSHS_PRINCIPALGCTIME	120*60   /* GC entry after 2 inactive hours  */
#define	FSHS_PRINCSPERBLOCK	256	/* Group of princs to alloc at an instance */
#define	FSHS_MAXPRINCBLOCKS	16	/* thus maximum princs is: 16 * 256 */
#define	FSHS_MINLRUPRINCS	10	/* MAX of LRU principals to free */
  

/* 
 * fshs_principal's states 
 */
#define	FSHS_PRINC_STALE    	0x1	/* Structure is stale and must be GCed */

/*
 * This structure represents the entry for each authenticated (and 
 * nonauthenticated) user. This, along with the host data structure, represents
 * the context of the user. 
 */
struct fshs_principal {
    struct fshs_principal *next;/* Next pointer in host's princ list */
    long refCount;		/* Reference counter */
    struct lock_data lock;	/* Assures synch of the structure */
    struct fshs_host *hostp;	/* Pointer to the parent host structure */
    struct xcred *xcredp;	/* Xcred info regarding this principal */
    unsigned long lastCall;	/* TimeStamp of last principal request */
    unsigned short states;	/* Principal related errors; see below */
    pac_list_t *delegationChain; /* principal's identity, including delegates */
    uuid_t session_uuid;	/* allows faster cache searches */
};


/* 
 * Export important fshs_principal entry's fields 
 */
#define	fshs_GetPrincAuth(PRINCP)	(PRINCP)->xcredp
#define	fshs_GetPrincHost(PRINCP)	(struct hs_host *)((PRINCP)->hostp)
#define	fshs_HostHasTKNSvc(PRINCP)     \
			((struct fshs_host *)((PRINCP)->hostp))->registerTokens

/*
 * Principal Queue-related routines
 */
#define	FSHS_QTOP(q)		((struct fshs_principal *)((char *)(q)))


/*
 * Exported by fshs_princ.c
 */
extern struct lock_data fshs_princlock;
extern struct squeue fshs_plru;
extern struct fshs_principal *fshs_PrincFreeListp;

extern void fshs_InitPrincMod _TAKES((void));
extern int fshs_FreePrincipal _TAKES((struct fshs_principal *));
extern int fshs_GetPrincipal _TAKES((struct context *,
				     struct fshs_principal **, int));
extern int fshs_InqContext _TAKES((handle_t, struct context **));
extern void fshs_PutPrincipal _TAKES((struct fshs_principal *));
extern int fshs_CheckAuthn _TAKES((struct context *, struct fshs_host *));

#endif	/* _FSHS_PRINCIPALH_ */
