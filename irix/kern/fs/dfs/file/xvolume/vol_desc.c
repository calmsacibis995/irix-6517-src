/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: vol_desc.c,v 65.3 1998/03/23 16:46:50 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1989, 1996 Transarc Corporation - All rights reserved */

/*
 * HISTORY
 * $Log: vol_desc.c,v $
 * Revision 65.3  1998/03/23 16:46:50  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:01:46  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:21:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.834.1  1996/10/02  19:04:35  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:55  damon]
 *
 * $EndLog$
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/volume.h>
#include <vol_init.h>
#include "vol_trace.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/vol_desc.c,v 65.3 1998/03/23 16:46:50 gwehrman Exp $")

/*
 *  vol descriptor related globals
 */
struct vol_calldesc *vol_descs[VOL_NODESC];
struct vol_calldesc *vol_freeCDList = 0;
struct lock_data vol_desclock;
static long vol_runaway = 0;

/*
 * vol_InitDescMod -- Initialize the vol_desc file structures.
 */
void vol_InitDescMod()
{
    lock_Init(&vol_desclock);
    vol_freeCDList = 0;
    vol_runaway = 0;
    bzero((caddr_t)&vol_descs[0], sizeof (vol_descs));
}

/*
 * Destroy the associated volume descriptor.
 *
 * vol_desclock must be write locked.
 */
void vol_PurgeDesc(cdp)
    register struct vol_calldesc *cdp;
{
    if (cdp->refCount <= 0) {
	/*
	 * Put the struct back in the free list and free the array slot
	 */
	if (cdp->handle.volp) {
	    register struct volume *volp = cdp->handle.volp;

	    /*
            * XXX must call VOL_CLOSE before VOL_BUSY is off. We need
            * to fix the AIX code so that WakeUpBusyBuffers does the
            * right thing
            */
	    (void) VOL_CLOSE(volp, &cdp->handle, 1 /* forced close */);
	    cdp->handle.volp = 0;       /* The VOL_RELE for this is below. */

	    /* Have to make this volume available again, at least for repair */
	    /* Do an analog of VOLOP_CLOSE here, but with some attention to
	     the VOL_BUSY bit. */
	    lock_ObtainWrite(&volp->v_lock);
	    (void) vol_close(volp, NULL);
	    lock_ReleaseWrite(&volp->v_lock);

	    /* The fileset's VOL_DELONSALVAGE bit will prevent any
	     * inappropriate access.
	     */

	    /* Now, if the volume was destroyed, eliminate it. */
	    if (volp->v_states & VOL_DEADMEAT)
		(void) vol_Detach(volp, 1);
	    VOL_RELE(volp);
	}

	cdp->lruq.next = (struct squeue *) vol_freeCDList;
	vol_freeCDList = cdp;
	vol_descs[cdp->descId] = (struct vol_calldesc *)0;
    } else {
	cdp->states |= VOL_DESCDELETED;
    }
  }


/*
 * Garbage descriptor structures that we haven't heard from for a while (3 mins)
 */
int vol_GCDesc(lockit)
    int lockit;
{
    register struct vol_calldesc *cdp;
    register long i, now = osi_Time(), gcnt= 0;

    if (lockit)
	lock_ObtainShared(&vol_desclock);
    for (i = 0; i < VOL_NODESC; i++) {
	if (!(cdp = vol_descs[i]))
	   continue;
	osi_assert(cdp->descId == i);		/* sanity test */
	/*
	 * Don't garbage collect descriptors now in use (refCount != 0)
	 */
	if (cdp->refCount == 0) {
	    if ((cdp->states & VOL_DESCDELETED) ||
		(cdp->lastCall < now - VOL_DESCTIMEOUT)) {
		/*
		 * This descriptor is now GCed: put the structure back in the
		 * free list and clear its slot in the vol_descs array so that
		 * it will be reused.
		 */
		lock_UpgradeSToW(&vol_desclock);
		vol_PurgeDesc(cdp);
		lock_ConvertWToS(&vol_desclock);
		gcnt++;
	    }
	}
    }
    if (lockit)
	lock_ReleaseShared(&vol_desclock);
    icl_Trace1(xops_iclSetp, XVOL_TRACE_GCDONE,
		ICL_TYPE_LONG, (unsigned)gcnt);
    return 300;		/* Run this daemon every 5 mins */
  }


/*
 * Return a new volume descriptor entry and fill it up properly
 */

int vol_GetDesc(cdescpp)
struct vol_calldesc **cdescpp;
{
    register struct vol_calldesc *cdp;
    register long i;
    DEFINE_OSI_UERROR;

    lock_ObtainShared(&vol_desclock);
    for (i = 0; i < VOL_NODESC; i++) {
	if (!vol_descs[i]) {
	   break;
	}
    }
    if (i  >= VOL_NODESC) {
	vol_GCDesc(0);
	for (i = 0; i < VOL_NODESC; i++) {
	    if (!vol_descs[i]) {
		break;
	    }
	}
	if ( i >= VOL_NODESC ) {
	    osi_setuerror(EMFILE);		/* XXX The closest we can get!! XXX */
	    icl_Trace0(xops_iclSetp, XVOL_TRACE_GETDESCFULL);
	    lock_ReleaseShared(&vol_desclock);
	    *cdescpp = (struct vol_calldesc *)0;
	    return(EMFILE);
	}
    }

    if (!vol_freeCDList)
	vol_GCDesc(0);
    lock_UpgradeSToW(&vol_desclock);
    if (!vol_freeCDList) {
	vol_runaway++;
	cdp = (struct vol_calldesc *) osi_Alloc(sizeof (struct vol_calldesc));
    } else {
	cdp = vol_freeCDList;
	vol_freeCDList = (struct vol_calldesc *) (cdp->lruq.next);
    }
    vol_descs[i] = cdp;
    cdp->refCount = 1;
    cdp->descId = i;
    cdp->proc = osi_GetPid();
    cdp->lastCall = osi_Time();
    cdp->states = 0;
    bzero((char *)&cdp->handle, sizeof(cdp->handle));
    lock_ReleaseWrite(&vol_desclock);
    icl_Trace1(xops_iclSetp, XVOL_TRACE_GETDESCRETURNS,
		ICL_TYPE_LONG, (u_long) i);
    *cdescpp = cdp;
    return (0);
}


/*
 * Return the descriptor structure associated with the index, descId
 */
int vol_FindDesc(descId, cdescpp)
    int descId;
    struct vol_calldesc **cdescpp;
{
    register struct vol_calldesc *cdp;
    DEFINE_OSI_UERROR; /* default to no error */

    if ((descId < 0) || (descId > VOL_NODESC)) {
	osi_setuerror(EDOM);			/* sanity check */
	icl_Trace1(xops_iclSetp, XVOL_TRACE_FINDDESCBOUNDS,
		   ICL_TYPE_LONG, (u_long) descId);
	*cdescpp = (struct vol_calldesc *)0;
	return(EDOM);
    }
    lock_ObtainShared(&vol_desclock);
    if (!(cdp = vol_descs[descId])) {
	osi_setuerror(EBADF);			/* XXX Better code? XXX */
	icl_Trace1(xops_iclSetp, XVOL_TRACE_FINDDESCEMPTY,
		   ICL_TYPE_LONG, (u_long) descId);
	lock_ReleaseShared(&vol_desclock);
	*cdescpp = (struct vol_calldesc *)0;
	return(EBADF);
    }
    if (cdp->proc != osi_GetPid()) {
	osi_setuerror(EBADF);			/* XXX Better code? XXX */
	icl_Trace2(xops_iclSetp, XVOL_TRACE_FINDDESCDIFFPID,
		   ICL_TYPE_LONG, (u_long) cdp->proc,
		   ICL_TYPE_LONG, (u_long) osi_GetPid());
	lock_ReleaseShared(&vol_desclock);
	*cdescpp = (struct vol_calldesc *)0;
	return(EBADF);
    } else if (descId != cdp->descId) {
	osi_setuerror(EINVAL);
	icl_Trace2(xops_iclSetp, XVOL_TRACE_FINDDESCDIFFDID,
		   ICL_TYPE_LONG, (u_long) cdp->descId,
		   ICL_TYPE_LONG, (u_long) descId);
	lock_ReleaseShared(&vol_desclock);
	*cdescpp = (struct vol_calldesc *)0;
	return(EINVAL);
    } else if (cdp->states & VOL_DESCDELETED) {
	osi_setuerror(EINVAL);
	icl_Trace2(xops_iclSetp, XVOL_TRACE_FINDDESCDELETED,
		   ICL_TYPE_POINTER, (u_long) cdp,
		   ICL_TYPE_LONG, (u_long) cdp->descId);
	lock_ReleaseShared(&vol_desclock);
	*cdescpp = (struct vol_calldesc *)0;
	return(EINVAL);
    }
    lock_UpgradeSToW(&vol_desclock);
    cdp->refCount++;
    cdp->lastCall = osi_Time();
    lock_ReleaseWrite(&vol_desclock);
    *cdescpp = cdp;
    return(0);
}


/*
 * Decrement the reference counter that was increased via the vol_GetDesc() and
 * vol_FindDesc() calls.
 */
void vol_PutDesc(cdp)
    register struct vol_calldesc *cdp;
{
    lock_ObtainWrite(&vol_desclock);
    cdp->refCount--;
    if ((!cdp->refCount) && (cdp->states & VOL_DESCDELETED)) {
	vol_PurgeDesc(cdp);
    }
    lock_ReleaseWrite(&vol_desclock);
}

void vol_SetDeleted(cdp)
    register struct vol_calldesc *cdp;
{
    lock_ObtainWrite(&vol_desclock);
    /* Set this descriptor to be GC'ed when released. */
    cdp->states |= VOL_DESCDELETED;
    lock_ReleaseWrite(&vol_desclock);
}
