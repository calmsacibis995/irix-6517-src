/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: tkset.h,v $
 * Revision 65.1  1997/10/20 19:19:48  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.542.1  1996/10/02  18:47:19  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:48  damon]
 *
 * $EndLog$
 */
/*
 * Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkset/RCS/tkset.h,v 65.1 1997/10/20 19:19:48 jdoak Exp $ */

#ifndef TRANSARC_TKSET_H
#define TRANSARC_TKSET_H

#include <dcedfs/stds.h>
#include <dcedfs/tkm_tokens.h>

#define TKSET_RELOCK	1	/* other success code */

struct tkset_set {
    long refCount;			/* reference count */
    long flags;				/* set's status */
    long revokeCount;			/* count of revokes while obtaining */
    struct volume *volp;		/* volume to release, if any */
    long volSyncType;			/* synchronization required for vol */
    struct tkset_set *next;		/* next token set currently active */
    struct tkset_token *tokens;		/* list of all tokens in this set */
};

/*
 *  flags values
 */
#define TKSET_SET_DELETED	1	/* the set has been deleted */
#define TKSET_SET_WAITING	2	/* waiting for the set to be deleted */
#define TKSET_SET_LOSTSOME	4	/* lost tokens during processing */
#define TKSET_SET_NOCLAMP	8	/* can release tokens if revoked */

struct tkset_token {
    struct tkset_token *next;		/* next token element in this set */
    struct afsFid fid;			/* file ID */
    afs_token_t token;			/* the token represented here */
    afs_hyper_t desiredRights;		/* min rights we need */
    struct hs_host *hostp;		/* host object */
    long flags;				/* flags */
};

/* tkset_token flag values:
 * Internal Use Only ! (ie., use within this package)
 *
 * The difference between TKSET_HAVE_TOKEN and TKSET_ID_GRANTED
 * is that HAVE_TOKEN means we have at least the tokens we're looking
 * for, but ID_GRANTED just means we have *some* tokens.  We could
 * have some tokens, but not the ones we want, if some of them were
 * revoked before we clamped things down.
 */
#define TKSET_HAVE_TOKEN	1	/* obtained desired tokens */
#define TKSET_ID_GRANTED	2	/* a token has been obtained */
#define TKSET_DONT_RELEASE	4	/* don't release it when it is freed */
#define TKSET_DONT_KEEP		8	/* don't return it to client */

/* these flags represent tkm flags that we have to remember.  They're stored
 * in the same tkset_token.flags  word, but are broken out for the
 * reader's convenience (amazing, considering the source, n'est-ce pas?)
 */
#define TKSET_OPTIMISTIC	0x100	/* true if want optimistic grant */
#define TKSET_JUMPQUEUE		0x200	/* want to jump over waiting tokens */
#define TKSET_NO_NEWEPOCH	0x400	/* prevent advancing epoch */
#define TKSET_SAME_TOKENID	0x800	/* return specific token */
#define TKSET_NO_REVOKE		0x1000	/* no revocations should be done */
#define TKSET_VOLQUIESCE		0x2000	/* make the volume be quiescent */

/*
 * tkset_AddTokenSet flags -- For Outsiders to use !
 */
#define	TKSET_ATS_NOOPTIMISM	0x0   /* Reserve this for no optimistic */
#define	TKSET_ATS_WANTOPTIMISM	0x1   /* want an optimistic grant */
#define	TKSET_ATS_WANTJUMPQUEUE	0x2   /* want to jump over waiting tokens */
#define TKSET_ATS_NOREVOKE	0x4   /* no revocations should be done */
#define TKSET_ATS_NONEWEPOCH	0x8	/* prevent advancing epoch */
#define TKSET_ATS_NO_NEW_EPOCH	TKSET_ATS_NONEWEPOCH
#define TKSET_ATS_TOKENID	0x10	/* return specific token */
#define TKSET_ATS_SAMETOKENID	TKSET_ATS_TOKENID
#define TKSET_ATS_DONT_KEEP	0x20	/* don't return this token to client */
#define TKSET_ATS_FORCEREVOCATION 0x100	/* Force revocation on conflicts. */
#define TKSET_ATS_FORCEVOLQUIESCE 0x200	/* Hold the volume quiescent while getting this */

#define tkset_SetVolSync(asetp, avolp, atype) \
    do { \
	if (asetp) { \
            (asetp)->volp = (avolp); \
            (asetp)->volSyncType = (atype); \
	} \
    } while(0);

#define tkset_GetVolSync(asetp)	((asetp)->volp)

/* max number of retries before starvation code kicks in */
#define TKSET_MAXRETRIES	4

/*
 * The routines exported by tkset.c
 */
extern struct tkset_set *tkset_Create _TAKES((long));
extern int tkset_Init _TAKES((void));
extern int tkset_AddTokenSet _TAKES((struct tkset_set *asetp,
				     struct afsFid *afidp,
				     afs_token_t *atokenp,
				     afs_recordLock_t *blockerp,
				     struct hs_host *ahostp,
				     int aflags,
				     opaque *dmptrp));
extern int tkset_Delete _TAKES((struct tkset_set *asetp));
extern int tkset_KeepToken _TAKES((struct tkset_set *asetp,
				   struct afsFid *afidp,
				   afs_token_t *atokenp));
extern int tkset_HereIsToken _TAKES((struct afsFid *afidp,
				     afs_token_t *atokenp,
				     long areqID));
extern int tkset_TokenIsRevoked _TAKES((struct afsRevokeDesc *arp));
extern int tkset_rele _TAKES((struct tkset_set *asetp));
extern int tkset_hold _TAKES((struct tkset_set *asetp));

#endif /* TRANSARC_TKSET_H */
