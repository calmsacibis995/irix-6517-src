/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_race.c,v 65.6 1998/04/01 14:16:31 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1994 Transarc Corporation
 *      All rights reserved.
 */


/* functions for handling token race conditions.  Used by tkc and tkset
 * packages.  Could be taught to track races on a per-file ID basis.
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/tkm_tokens.h>

#if !defined(KERNEL)
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_race.c,v 65.6 1998/04/01 14:16:31 gwehrman Exp $")

/* lock to protect globals, which at this point is only the freeRaceList
 * pointer.
 */
struct lock_data tkm_raceLock;
struct tkm_raceFile *tkm_freeRaceListp = 0;

#ifdef SGIMIPS
struct tkm_raceFile *tkm_AllocRaceEntry(void)
#else
struct tkm_raceFile *tkm_AllocRaceEntry()
#endif /* SGIMIPS */
{
    register struct tkm_raceFile *tfp;

    lock_ObtainWrite(&tkm_raceLock);
    if (tfp = tkm_freeRaceListp) {
	tkm_freeRaceListp = tfp->nextp;
    }
    else tfp = (struct tkm_raceFile *) osi_Alloc(sizeof(struct tkm_raceFile));
    lock_ReleaseWrite(&tkm_raceLock);
    return tfp;
}

#ifdef SGIMIPS
void
#endif
tkm_FreeRaceEntry(struct tkm_raceFile *aep)
{
    lock_ObtainWrite(&tkm_raceLock);
    aep->nextp = tkm_freeRaceListp;
    tkm_freeRaceListp = aep;
    lock_ReleaseWrite(&tkm_raceLock);
}

/* Get a race entry, bumping reference count.  Caller must have write lock
 * on *arp.
 */
struct tkm_raceFile *tkm_GetRaceEntry(struct tkm_race *arp,
				      afsFid *afidp)
{
    register struct tkm_raceFile *fp;
    for(fp = arp->raceListp; fp; fp = fp->nextp) {
	if (FidCmp(&fp->fid, afidp) == 0) break;
    }
    if (fp) {
	/* bump ref count */
	fp->activeCount++;
    }
    else {
	fp = tkm_AllocRaceEntry();
	fp->nextp = arp->raceListp;
	arp->raceListp = fp;
	fp->fid = *afidp;
	fp->activeCount = 1;	/* init for our caller */
	fp->raceCount = 0;
	AFS_hzero(fp->racingRights);
    }
    return fp;
}

tkm_StartRacingCall(struct tkm_race *arp,
		    afsFid *fidp,
		    struct tkm_raceLocal *alstatep)
{
    register struct tkm_raceFile *fp;

    lock_ObtainWrite(&arp->lock);
    fp = tkm_GetRaceEntry(arp, fidp);
    alstatep->count = fp->raceCount;
    lock_ReleaseWrite(&arp->lock);
    return 0;
}


/* must init before using a race structure */
tkm_InitRace(register struct tkm_race *arp)
{
    static initd = 0;
    if (!initd) {
	initd = 1;
	lock_Init(&tkm_raceLock);
    }
    lock_Init(&arp->lock);
    arp->raceListp = (struct tkm_raceFile *) 0;
    return 0;
}

tkm_EndRacingCall(struct tkm_race *arp,
		  afsFid *afidp,
		  struct tkm_raceLocal *alstatep,
		  afs_token_t *tokenp)
{
    register struct tkm_raceFile *fp, **lfpp;

    lock_ObtainWrite(&arp->lock);
    /* find entry, prepare for deletion if necessary */
    lfpp = &arp->raceListp;
    for(fp = *lfpp; fp; lfpp = &fp->nextp, fp = *lfpp)
	if (FidCmp(&fp->fid, afidp) == 0) break;
    osi_assert(fp);

    /* only remove rights if a revoke came in while our call was executing.
     * otherwise, we could get in an infinite loop, clearing rights revoked
     * many calls ago, as long as someone else is active each time we
     * come into tkm_StartRacingCall.
     */
    if (alstatep->count != fp->raceCount) {
	AFS_HOP(tokenp->type, &~, fp->racingRights);
    }
    /* now decrement the usage count and free the entry if we're
     * the last user.  lfpp was set above to the pointer to adjust.
     */
    if(--fp->activeCount == 0) {
	*lfpp = fp->nextp;
	tkm_FreeRaceEntry(fp);
    }
    lock_ReleaseWrite(&arp->lock);
    return 0;
}


tkm_RegisterTokenRace(register struct tkm_race *arp,
		      afsFid *afidp,
		      afs_hyper_t *rightsp)
{
    register struct tkm_raceFile *fp;

    lock_ObtainWrite(&arp->lock);
    for(fp = arp->raceListp; fp; fp=fp->nextp) {
	if (FidCmp(&fp->fid, afidp) == 0)
	    break;
    }
    if (fp) {
	AFS_HOP(fp->racingRights, |, *rightsp);
	fp->raceCount++;
    }
    lock_ReleaseWrite(&arp->lock);
    return 0;
}





