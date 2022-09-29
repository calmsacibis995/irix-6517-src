/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: hs_host.h,v $
 * Revision 65.1  1997/10/20 19:19:22  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.39.1  1996/10/02  17:51:51  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:34  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/host/RCS/hs_host.h,v 65.1 1997/10/20 19:19:22 jdoak Exp $ */

#ifndef	_HS_GHOSTH_
#define	_HS_GHOSTH_ 

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/queue.h>
#include <dcedfs/common_data.h>


/*
 * This structure presents the operations that can be explicitly performed on a
 * particular host structure.  KEEP THIS STRUCTURE IN SYNC WITH the one in host
 */
struct hostops {
    long	(*hs_hold)();
    long	(*hs_rele)();
    long	(*hs_lock)();
    long	(*hs_unlock)();
    long	(*hs_revoketoken)();
    long	(*hs_asyncgrant)();
};


/* 
 * This is exported to the token manager 
 */
struct hs_tokenDescription {
    afs_token_t	        token;
    struct hs_host	*hostp;
    afsFid	        fileDesc;
};


/* 
 * hs_host's states bits 
 */
#define	HS_HOST_HOSTDOWN	0x1	/* Associated client is down */
#define	HS_HOST_NEEDINIT	0x2	/* Primary call requires InitTokenState */
#define	HS_HOST_HADTOKENS	0x4	/* TRUE if this host ever had a token */

/* 
 * This structure mainly describes the state of a client (usually a cache 
 * manager) that obtains tokens.
 */
struct hs_host {
    struct squeue lruq;			/* LRU queue of all hosts */             
    long refCount;			/* Reference counter */
    struct lock_data lock;		/* Assures synch of the structure */
    struct hostops *hstopsp;		/* Generic host-related operations */
    unsigned long states;		/* Host related errors; see below */
    char *privatep;			/* pointer to the private data */
    long type;				/* to distinguish different instantiations */
};


/*
 * Define macro to find out if a host is down.
 */
#define HS_ISHOSTDOWN(hostp)	((hostp)->states & HS_HOST_HOSTDOWN)

/* 
 * Host queue implementation routines
 */
#define HS_QTOH(q)		((struct hs_host *)((char *)(q)))


/* 
 * Hostops generic macros 
 */
#define	HS_HOLD(HOSTP) \
    (*(HOSTP)->hstopsp->hs_hold)(HOSTP)
#define	HS_RELE(HOSTP) \
    (*(HOSTP)->hstopsp->hs_rele)(HOSTP)
#define	HS_LOCK(HOSTP, TYPE, TIMEOUT) \
    (*(HOSTP)->hstopsp->hs_lock)(HOSTP, TYPE, TIMEOUT)
#define	HS_UNLOCK(HOSTP, TYPE) \
    (*(HOSTP)->hstopsp->hs_unlock)(HOSTP, TYPE)
#define	HS_REVOKETOKEN(HOSTP, REVLEN, REVLIST) \
    (*(HOSTP)->hstopsp->hs_revoketoken)(HOSTP, REVLEN, REVLIST)
#define HS_ASYNCGRANT(HOSTP,TOKENA,REQNUM) \
    (*(HOSTP)->hstopsp->hs_asyncgrant)(HOSTP,TOKENA,REQNUM)


#define	HS_LOCK_READ	1
#define	HS_LOCK_WRITE	2
#endif	/* _HS_GHOSTH_ */
