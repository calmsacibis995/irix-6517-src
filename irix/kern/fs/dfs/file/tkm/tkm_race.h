/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * Copyright (c) 1996, 1994, Transarc Corp.
 * All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_race.h,v 65.1 1997/10/20 19:18:06 jdoak Exp $ */

#ifndef _TRANSARC_TKM_RACE_H_
#define _TRANSARC_TKM_RACE_H_

/* Algorithm:
 * For each file, before we start a token granting call, the requestor
 * will call tkm_StartRacingCall.  We'll create a structure, keyed
 * by file ID, that will track the union of all of the rights revoked
 * from this host (implicit in the tkm_race structure) and this file ID.
 * When the token granting call is done, tkm_EndRacingCall is called, and
 * conservatively removes these rights from the caller's token.
 *
 * It doesn't bother to do this if no revokes actually came in while our
 * caller was getting its token, since in that case, none of the revokes
 * apply to us.  This optimization is important, since otherwise we could
 * have N threads working on a file, and a revoke could come in revoking
 * our rights.  We would then remove those rights from our grant, and
 * immediately get new tokens.  However, since the other dudes are still
 * waiting, the activeCount would still be > 0, and we wouldn't have
 * cleared the racingRights field, so we'd once again deduct the
 * racingRights from the caller's rights.  We could loop forever doing
 * this.
 *
 * In general, when no one has any more active grant calls for a file,
 * we remove the racingFile structure for that file; new calls will start
 * off with a freshly initialized racingRights field.  If a structure
 * gets *too* old (a high raceCount perhaps?), we could block new calls
 * to StartRacingCall until the count drops to 0, and then reinit the
 * structure; we'll leave that as an enhancement for now.
 */

/* state for tracking an individual file's revoke state */
typedef struct tkm_raceFile {
    struct tkm_raceFile *nextp;	/* next guy in list */
    afsFid fid;			/* relevant file */
    long activeCount;		/* count of guys making TKM calls now */
    long raceCount;		/* count of revokes that hit during grants */
    afs_hyper_t racingRights;	/* rights or'd together in this race period */
} tkm_raceFile_t;

/* state for tracking racing revokes: revokes that come in while we're
 * executing tkm_GetToken calls.  These structures are locked by the
 * tkc_rclock.
 */
typedef struct tkm_race {
    struct tkm_raceFile *raceListp;	/* list of file's currently racing */
    struct lock_data lock;		/* lock for synchronizing access */
} tkm_race_t;

typedef struct tkm_raceLocal {
    long count;			/* raceCount at start of call */
} tkm_raceLocal_t;

/* exported function prototypes */

extern int tkm_StartRacingCall(
    tkm_race_t *globalRace,
    afsFid *fidp,
    tkm_raceLocal_t *localRace
);

extern int tkm_InitRace(tkm_race_t *globalRace);

extern int tkm_EndRacingCall(
    tkm_race_t *globalRace,
    afsFid *fidp,
    tkm_raceLocal_t *localState,
    afs_token_t *tokenp
);
extern int tkm_RegisterTokenRace(
    tkm_race_t *globalRace,
    afsFid *fidp,
    afs_hyper_t *type
);
#endif /* _TRANSARC_TKM_RACE_H_ */
