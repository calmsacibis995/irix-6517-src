/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_pag.c,v 65.12 1999/09/07 15:41:52 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1989, 1994 Transarc Corporation - All rights reserved */

/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/lock.h>

#ifndef KERNEL
#include <dce/pthread.h>
#endif /* !KERNEL */

#if defined(SGIMIPS) && defined(KERNEL)
#include <ksys/cred.h>
#include <sys/prctl.h>

/* Global definition for all packages */
osi_cred_t *osi_credp;
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_pag.c,v 65.12 1999/09/07 15:41:52 gwehrman Exp $")

/*
 * Pags are implemented as follows: the set of groups whose long representation
 * is '41XXXXXX' hex are used to represent the pags.
 *
 * We use this pag mechanism in various modules such as:
 *
 * In the cache manager (CM) being a member of such a group means you are
 * authenticated as pag XXXXXX (note that 0x41 == 'A', for Andrew file system).
 * You are never authenticated as multiple pags at once (See cm module for
 * more).
 *
 * In the NFS translator (nfstr) we use a pag to uniquely identify an nfs
 * [host,uid] pair when no setpag() authentication is used by the client.
 *
 * In the Extended credential package (xcred) we use the pag to uniquely
 * identify the proper xcred structure associated with a particular ucred one.
 */

/*
 * Tips on porting this module to new platforms
 *
 * In some kernels, there is space for a PAG value in the existing cred
 * structure.  For instance, the OSF/1 cred structure has such a field
 * (cr_pag), but only when compiled with KERNEL defined.
 * For porting to such a kernel, define OSI_HAS_CR_PAG to be 1 in
 * osi/???/osi_cred_mach.h.
 *
 * Otherwise, define OSI_HAS_CR_PAG to 0.  The code in this module will
 * then take care of shoehorning the pag into the grouplist.
 */

/*
 * We limit the rate at which we issue PAG's to avoid wrapping around,
 * since that could cause aliasing and hence a security hole.  A rigorous
 * solution would require checking whether a given PAG is already in use
 * (as, for instance, fork does with process ID's).  However, that is
 * "too hard", so we just stretch out the period between reuses far to
 * frustrate a systematic attack and make the chance of accidental
 * collisions extremely small.
 *
 * There are 16M possible PAG's, so if we give out at most one every
 * two seconds, it will take 388 days to wrap around.  We keep track
 * of the time since giving out the first PAG (or since the last wrap)
 * to ensure that we do not exceed this rate.  Since typically there
 * will be long intervals during which no PAG's are issued, most setpag
 * calls will complete without delay, while the cost imposed by a denial
 * of service attack will not be too severe.
 *
 * This method (copied from AFS) is an improvement over the old code,
 * which slept for ten seconds at a time until no setpag had completed
 * within the preceding ten seconds.
 */

static int osi_setpagInterval = 2;	/* Min. seconds per PAG */
static long osi_firstPagTime = 0;	/* Time of first setpag */

#define OSI_MAXPAG	0x41ffffff
#define OSI_MINPAG	0x41000000

static u_long osi_pagCounter = OSI_MAXPAG; /* start high to avoid AFS 3 #s */
static osi_mutex_t osi_pagLock;

#ifdef SGIMIPS
/*
 * AFS routine included for AFS/DFS co-existence.
 * See comments in cm_xsyscalls.c around afs_xsetgroups()
 */

/* This is AFS's current PagInCred. */
int AFS_PagInCred(osi_cred_t *credp)
{
    int pag;
    int g0, g1, l, h;

    if (credp->cr_ngroups < 2)
        return OSI_NOPAG;

    g0 = credp->cr_groups[0];
    g1 = credp->cr_groups[1];

    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
        l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
        h = (g0 >> 14);
        h = (g1 >> 14) + h + h + h;
        pag = ((h << 28) | l);
        /* Additional testing */
        if (((pag >> 24) & 0xff) == 'A')
            return pag;
        else
            return OSI_NOPAG;
    }
    return OSI_NOPAG;
}
#endif /* SGIMIPS */


/*
 * Generate (and return) a new pag value
 */
u_long
osi_genpag(void)
{
    u_long pag;

    osi_mutex_enter(&osi_pagLock);
    pag = osi_pagCounter;
    if (pag == OSI_MAXPAG)
	osi_firstPagTime = osi_Time();
    osi_pagCounter = (pag == OSI_MINPAG) ? OSI_MAXPAG : pag - 1;
    osi_mutex_exit(&osi_pagLock);
    return pag;
}

/*
 * Return the current pag value
 */
u_long
osi_getpag(void)
{
    return osi_pagCounter;
}

#if OSI_HAS_CR_PAG
/*
 * Get the pag value (if any) from the credential struct cred.
 */
u_long
osi_GetPagFromCred(osi_cred_t *credp)
{
    long pag = credp->cr_pag;
    return  (OSI_IS_PAG(pag) ? pag : OSI_NOPAG);
}

/*
 * Set the pagvalue pag in the credential structure
 */
long
osi_SetPagInCred(osi_cred_t *credp, u_long pagvalue)
{
    osi_assert(OSI_IS_PAG(pagvalue));
    credp->cr_pag = pagvalue;
    return (0);
}

static void
osi_DelPagFromCred(osi_cred_t *credp)
{
    credp->cr_pag = 0;
}
#endif /* OSI_HAS_CR_PAG */

/*
 * If your platform's cred structure cannot accomodate
 * a separate PAG value, you can do like AIX and store
 * the PAG as the first two entries in the group list.
 */
#if !OSI_HAS_CR_PAG
/*
 * Get the pag value (if any) from the credential struct cred.
 */
u_long
osi_GetPagFromCred(osi_cred_t *credp)
{
    int ngroups = osi_GetNGroups(credp);

    if (ngroups == 0 || !OSI_IS_PAG(credp->cr_groups[ngroups - 1]))
	return OSI_NOPAG;
    else
	return credp->cr_groups[ngroups - 1];
}

/*
 * Set the pagvalue pag in the credential structure
 */
long
osi_SetPagInCred(osi_cred_t *credp, u_long pagvalue)
{
    /*
     * On systems whose cred structure has no cr_pag, the PAG is stored
     * in the group list.
     */
    int ngroups;

    osi_assert(OSI_IS_PAG(pagvalue));

    ngroups = osi_GetNGroups(credp);

    /*
     * If there is already a PAG at the end of the list, overwrite it.
     * Otherwise, append pagvalue to the list.
     */
    if (ngroups == 0 || !OSI_IS_PAG(credp->cr_groups[ngroups - 1]))
	ngroups++;

#if defined(SGIMIPS) && defined(KERNEL)
    if (ngroups > ngroups_max)
#else  /* SGIMIPS  && KERNEL */
    if (ngroups > OSI_MAXGROUPS)
#endif /* SGIMIPS  && KERNEL */
	return (E2BIG);

    osi_SetNGroups(credp, ngroups);
    credp->cr_groups[ngroups - 1] = pagvalue;
    return (0);
}

static void
osi_DelPagFromCred(osi_cred_t *credp)
{
    int ngroups = osi_GetNGroups(credp);

    if (ngroups != 0 && OSI_IS_PAG(credp->cr_groups[ngroups - 1])) {
	ngroups--;
	credp->cr_groups[ngroups] = 0;
	osi_SetNGroups(credp, ngroups);
    }
}
#endif /* !OSI_HAS_CR_PAG */

/*
 * Get the AFS pag value (if any) in the credential struct cred.
 * We use this in osi_xsetgroups for AFS compatibility.
 */

#define AFS_MINGROUP	0x7f00
#define AFS_MAXGROUP	0xbeff
u_long
osi_GetAFSPagFromCred(osi_cred_t *credp)
{
    u_long afs_pag;
    u_long g0, g1;
    int ngroups = osi_GetNGroups(credp);

    if (ngroups < 2)
	return OSI_NOPAG;

    g0 = credp->cr_groups[0];
    g1 = credp->cr_groups[1];

    if (g0 < AFS_MINGROUP || g0 > AFS_MAXGROUP ||
	g1 < AFS_MINGROUP || g1 > AFS_MAXGROUP)
	return OSI_NOPAG;

    afs_pag = (0x40000000 | ((g0 - AFS_MINGROUP) << 14) | (g1 - AFS_MINGROUP));

    osi_assert(OSI_IS_PAG(afs_pag));
    return afs_pag;
}

/*
 * Set the AFS pag value pag in the credential structure
 */
long
osi_SetAFSPagInCred(osi_cred_t *cr, u_long afs_pag)
{
    int ngroups, i;
    u_long newpag;

    osi_assert(OSI_IS_PAG(afs_pag));

    ngroups = osi_GetNGroups(cr);

    if (ngroups > OSI_MAXGROUPS - 2)
	return (E2BIG);

    for (i = ngroups - 1; i >= 0; --i)
	cr->cr_groups[i + 2] = cr->cr_groups[i];

    cr->cr_groups[1] = (afs_pag & 0x3fff) + AFS_MINGROUP;
    cr->cr_groups[0] = ((afs_pag >> 14) & 0x3fff) + AFS_MINGROUP;

    osi_SetNGroups(cr, ngroups + 2);
    return (0);
}
#ifdef KERNEL
/*
 *
 * New system call specific to DFS - Setpag creates a new pag for the caller
 *
 * Group ID's whose long representation is '41XXXXXX' hex are used to
 * represent the pags.  Being a member of such a group means you are
 * authenticated as pag XXXXXX (note that 0x41 == 'A', for Andrew file
 * system).  You are never authenticated as multiple pags at once.
 *
 * The basic motivation behind pags is this: just because your unix uid
 * is N doesn't mean that you should have the same privileges as anyone
 * logged in on the machine as user N, since this would enable the superuser
 * on the machine to sneak in and make use of anyone's authentication info,
 * even that which is only accidentally left behind when someone leaves a
 * public workstation.
 *
 * The Andrew file system doesn't use the unix uid for anything except
 * a handle with which to find the actual authentication tokens anyway,
 * so the pag is an alternative handle which is somewhat more secure
 * (although of course not absolutely secure).
 */
/* ARGSUSED */
long
afscall_setpag(
  long unused,
  long unused2,
  long unused3,
  long unused4,
  long unused5,
#ifdef SGIMIPS
  long *rval)
#else  /* SGIMIPS */
  int *rval)
#endif /* SGIMIPS */
{
    u_long newpag;
    osi_cred_t *credp;
    long now, soonest;
    struct proc *p = osi_curproc();
    int code;

    osi_MakePreemptionRight();
    osi_mutex_enter(&osi_pagLock);
#ifdef SGIMIPS
    /*
     * BDR
     * Undocumented feature.  Allow priv caller of setpag to
     * set a PAG.  This allows sharing of a PAG by a non child
     * process.  This is usefull for the SSO work that ANL has
     * been doing with K5.
     */
    if (osi_suser(get_current_cred()) && unused) {
	osi_mutex_exit(&osi_pagLock);
	if (OSI_IS_PAG(unused)) {
	    newpag = unused;
	}
	else {
	    code = EINVAL;
	    goto out;
	}
    } else {
        now = osi_Time();
        soonest = osi_firstPagTime +
		        osi_setpagInterval * (OSI_MAXPAG - osi_pagCounter);

	while (now < soonest) {
            osi_mutex_exit(&osi_pagLock);
            osi_Wait_r(1000 * (soonest - now), (struct osi_WaitHandle *)0, 0);
            osi_mutex_enter(&osi_pagLock);
            now = osi_Time();
            soonest = osi_firstPagTime +
                            osi_setpagInterval * (OSI_MAXPAG - osi_pagCounter);

        }
	osi_mutex_exit(&osi_pagLock);
        newpag = osi_genpag();
    }
    credp = pcred_lock(p);
    credp = crcopy(p);
    code = osi_SetPagInCred(credp, newpag);
    /* update all share group processes (except p) */
    if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
    	shaddr_t *sa = p->p_shaddr;
    	int s;

    	s = mutex_spinlock(&sa->s_rupdlock);
    	crfree(sa->s_cred);
    	sa->s_cred = credp;
    	crhold(credp);
    	setshdsync(sa, p, PR_SID, UT_UPDUID);
    	mutex_spinunlock(&sa->s_rupdlock, s);
    }

    pcred_push(p);

out:
    if (code == 0)
	*rval = newpag;
    else
	*rval = OSI_NOPAG;

    osi_UnmakePreemptionRight();
#else  /* SGIMIPS */
    now = osi_Time();
    soonest = osi_firstPagTime +
		osi_setpagInterval * (OSI_MAXPAG - osi_pagCounter);

    while (now < soonest) {
	osi_mutex_exit(&osi_pagLock);
	osi_Wait_r(1000 * (soonest - now), (struct osi_WaitHandle *)0, 0);
	osi_mutex_enter(&osi_pagLock);
	now = osi_Time();
	soonest = osi_firstPagTime +
			osi_setpagInterval * (OSI_MAXPAG - osi_pagCounter);

    }
    osi_mutex_exit(&osi_pagLock);
    newpag = osi_genpag();
    osi_pcred_lock(p);
    credp = crcopy(p);
    code = osi_SetPagInCred(credp, newpag);
    osi_setucred(credp);
    osi_pcred_unlock(p);
    osi_set_thread_creds(p, credp);
    if (code == 0)
	*rval = newpag;
    osi_UnmakePreemptionRight();
#endif /* SGIMIPS */
    return code;
}

/* ARGSUSED */
long
afscall_getpag(
  long unused,
  long unused2,
  long unused3,
  long unused4,
  long unused5,
#ifdef SGIMIPS
  long *rval)
#else  /* SGIMIPS */
  int *rval)
#endif /* SGIMIPS */
{
    long pag =  osi_GetPagFromCred(osi_getucred());

    if (pag == OSI_NOPAG) {
	return (ENOENT);
    } else {
	*rval = pag;
	return (0);
    }
}

long
afscall_resetpag(
  long unused,
  long unused2,
  long unused3,
  long unused4,
  long unused5,
#ifdef SGIMIPS
  long *rval)
#else  /* SGIMIPS */
  int *rval)
#endif /* SGIMIPS */
{
    osi_cred_t *credp;
    struct proc *p = osi_curproc();

    osi_MakePreemptionRight();
#ifdef SGIMIPS
    credp = pcred_lock(p);
    credp = crcopy(p);
    osi_DelPagFromCred(credp);
    /* update all share group processes (except p) */
    if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
        shaddr_t *sa = p->p_shaddr;
        int s;

        s = mutex_spinlock(&sa->s_rupdlock);
        crfree(sa->s_cred);
        sa->s_cred = credp;
        crhold(credp);
        setshdsync(sa, p, PR_SID, UT_UPDUID);
        mutex_spinunlock(&sa->s_rupdlock, s);
    }

    pcred_push(p);
#else  /* SGIMIPS */
    osi_pcred_lock(p);
    credp = crcopy(p);
    osi_DelPagFromCred(credp);
    osi_setucred(credp);
    osi_pcred_unlock(p);
    osi_set_thread_creds(p, credp);
#endif /* SGIMIPS */
    osi_UnmakePreemptionRight();
    return (0);
}
#endif /* KERNEL */
