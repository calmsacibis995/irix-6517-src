/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_async.h,v $
 * Revision 65.1  1997/10/20 19:17:22  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.1  1996/10/02  17:07:12  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:38  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_async.h,v 65.1 1997/10/20 19:17:22 jdoak Exp $ */

#ifndef _CM_ASYNC_H_ENV_
#define _CM_ASYNC_H_ENV_ 1

/* structure representing an asynchronous token grant */
struct cm_asyncGrant {
    struct cm_asyncGrant *nextp;	/* next guy same server */
    long refCount;			/* ref count */
    afsFid fid;				/* relevant file ID */
    afs_token_t token;			/* token being allocated */
    struct cm_server *serverp;		/* back pointer */
    long states;			/* granted yet, etc (see below) */
};

/* values for "states" field of cm_asyncGrant structure */
#define CM_ASYNC_GRANTED	1	/* token has been granted */
#define CM_ASYNC_DELETED	4	/* delete this structure when done */
#define CM_ASYNC_CALLERGONE	8	/* caller is gone (probably EINTR) */

extern long cm_GetAsyncToken _TAKES((struct cm_scache *,
				     afs_token_t *,
				     struct cm_rrequest *));

#endif /* _CM_ASYNC_H_ENV_ */
