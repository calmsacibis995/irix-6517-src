/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_xsyscalls.c,v 65.7 1998/06/29 16:02:48 bdr Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1989, 1995 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/stds.h>
#include <cm.h>				/* Cm-based standard headers */
#ifdef AFS_OSF_ENV
#include <kern/parallel.h>
#endif
#include <cm_cell.h>
#include <cm_conn.h>
#include <dcedfs/osi_cred.h>
#ifdef SGIMIPS
#ifdef KERNEL
#include <ksys/cred.h>
#endif  /* KERNEL */
int setgroups(int, gid_t *);
#endif /* SGIMIPS */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_xsyscalls.c,v 65.7 1998/06/29 16:02:48 bdr Exp $")

#ifdef SGIMIPS
/* 
 * NOTE:  This is common code for AFS and DFS to support the setgroups
 * call.  Any changes made to this code *must* be reflected in both Transarc's
 * AFS code base and SGI's DFS code base.
 */

extern void estgroups(cred_t *cr, cred_t *newcr);

/*
 * Encrypt the pag into the two groups, g0p, g1p
 */
static void
get_groups_from_pag(u_long pag, u_long *g0p, u_long *g1p)
{
    unsigned short g0, g1;

    pag &= 0x7fffffff;
    g0 = 0x3fff & (unsigned short)(pag >> 14);
    g1 = 0x3fff & (unsigned short)pag;
    g0 |= ((pag >> 28) / 3) << 14;
    g1 |= ((pag >> 28) % 3) << 14;
    *g0p = g0 + 0x3f00;
    *g1p = g1 + 0x3f00;
}


/*
 * Generic routine to set the PAG in the cred for AFS and DFS.
 * If flag = 0 this is a DFS pag held in one group.
 * If flag = 1 this is a AFS pag held in two group entries
 */
static int afsDFS_SetPagInCred( int pag, int flag)
{
    int i, ngrps;
    u_long g0, g1;
    osi_cred_t *credp;
    int groups_taken = (flag ? 2 : 1);
    struct proc *p = osi_curproc();
    gid_t newgids[NGROUPS_UMAX];

    credp = pcred_access(p);
    ngrps = credp->cr_ngroups + groups_taken;
    if (ngrps > ngroups_max) {
	pcred_unaccess(p);
        return E2BIG;
    }
    
    if (flag) {
	/* AFS case */
        /* Break out the AFS pag into two groups */
        get_groups_from_pag(pag, &g0, &g1);
	newgids[0] = (gid_t)g0;
	newgids[1] = (gid_t)g1;
    } else {
	/* DFS case */
	newgids[ngrps - 1] = pag;
	groups_taken = 0;
    }
    for (i=0; i<credp->cr_ngroups; i++)
        newgids[i+groups_taken] = credp->cr_groups[i];
    pcred_unaccess(p);
    /* 
     * Use CURVPROC_SETGROUPS so that we get proc behaviors instead of 
     * estgroups, which expects us to do the behavior stuff on our own.
     * This will help out in the port to cell irix.
     */
    CURVPROC_SETGROUPS(ngrps, newgids);

    return 0;
}

int afs_xsetgroups(int ngroups, gid_t *gidset)
{
    int old_afs_pag = OSI_NOPAG;
    int old_dfs_pag = OSI_NOPAG;
    int code;
    osi_cred_t *credp = osi_getucred();
    struct proc *p = osi_curproc();

    if (!_CAP_CRABLE(credp, CAP_SETGID))
    	return EPERM;

    /* First get any old PAG's */
    old_afs_pag = AFS_PagInCred(credp);
    old_dfs_pag = (int)osi_GetPagFromCred(credp);

    /* Set the passed in group list. */
    if (code = setgroups(ngroups, gidset))  
        return code;

    /*  
     * The setgroups gave our curent thread a new cred pointer.
     * Get the value again but this time don't use the uthread cred
     * as this is now waiting for a credsync to be updated.  Instead
     * we must use the cred out of the proc structure.
     */
    credp = pcred_access(p);
    if ((AFS_PagInCred(credp) == OSI_NOPAG) && (old_afs_pag != OSI_NOPAG)) {
	/* reset the AFS PAG */
	pcred_unaccess(p);
	code = afsDFS_SetPagInCred(old_afs_pag, 1);
    } else {
	pcred_unaccess(p);
    }
    /*
     * Once again get the credp because the afsDFS_SetPagInCred might have
     * assigned a new one.  We again must use the cred out of the proc.
     */
    credp = pcred_access(p);
    if ((osi_GetPagFromCred(credp)==OSI_NOPAG) && (old_dfs_pag != OSI_NOPAG)) {
	pcred_unaccess(p);
	code = afsDFS_SetPagInCred(old_dfs_pag, 0);
    } else {
	pcred_unaccess(p);
    }

    return code;
}

/*
 * End of common AFS/DFS code.
 */

#endif /* SGIMIPS */

/*
 * A system call to inform the CM that a new ticket-granted ticket (tgt) is
 * granted. This is called by security service routines when a user runs
 * either dce_login or kinit.
 */
long afscall_cm_newtgt(
  long uPag,		/* input: specify a particular login session */
  long uLifeTime,	/* input: new tgt's expiration time  */
  long unused3,
  long unused4,
  long unused5,
  int *rval)
{
    unsigned32 pag = (unsigned32)uPag;
    time_t lifeTime = (time_t)uLifeTime;
    struct cm_tgt *tgtp;
#ifndef SGIMIPS
    struct icl_log *logp;
    long code;
#endif /* SGIMIPS */
    unsigned32 mypag;

    /* first initialize the ICL system */
    if (cm_iclSetp == NULL) {
	cm_BasicInit();
    }

    icl_Trace2(cm_iclSetp, CM_TRACE_GETNEWTGT_CALL,
	       ICL_TYPE_LONG, pag,
	       ICL_TYPE_LONG, lifeTime);
    /*
     * If lifeTime is 0, we will stop any attempt to authenticate from this pag
     */
    if (lifeTime != 0 && lifeTime < osi_Time()) {
	icl_Trace0(cm_iclSetp, CM_TRACE_GETNEWTGT_CALL_OLD);
	return EINVAL;
    }

#ifdef SGIMIPS
    mypag = (unsigned32)osi_GetPagFromCred(osi_getucred());
#else  /* SGIMIPS */
    mypag = osi_GetPagFromCred(osi_getucred());
#endif /* SGIMIPS */
    if (pag == 0)
	pag = mypag;
    else
	/* Check if this process has right to do this */
	if (mypag != pag && !osi_suser(osi_getucred())) {
	    icl_Trace2(cm_iclSetp, CM_TRACE_GETNEWTGT_ACCESS,
		       ICL_TYPE_LONG, pag,
		       ICL_TYPE_LONG, mypag);
	    return EACCES;
	}

    lock_ObtainWrite(&cm_tgtlock);
    for (tgtp = cm_tgtList; tgtp != NULL; tgtp = tgtp->next) {
         if (tgtp->pag == pag)
	     break;
    }
    if (tgtp == NULL) {
        if (!FreeTGTList)
	    tgtp = (struct cm_tgt *) osi_Alloc(sizeof (struct cm_tgt));
	else {
	    tgtp = FreeTGTList;
	    FreeTGTList = tgtp->next;
	}
	bzero((char *) tgtp, sizeof (struct cm_tgt));
	tgtp->next = cm_tgtList;
	cm_tgtList = tgtp;
	tgtp->pag = pag;
	icl_Trace2(cm_iclSetp, CM_TRACE_GETNEWTGT, ICL_TYPE_LONG, pag,
		   ICL_TYPE_LONG, lifeTime);
    }
    tgtp->tgtTime = lifeTime;
    if (lifeTime != 0)
	tgtp->tgtFlags &= ~CM_TGT_EXPIRED;
    lock_ReleaseWrite(&cm_tgtlock);
    /*
     * NOTE: This tgt should not be GCed if 'lifeTime' is still in the future.
     *
     * Reset all rpc bindings associated with this pag, if any
     */
    cm_ResetUserConns(pag);
    return 0;
}
