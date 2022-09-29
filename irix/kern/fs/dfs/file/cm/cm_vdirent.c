/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_vdirent.c,v 65.8 1998/06/22 15:06:08 bdr Exp $";
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
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/direntry.h>
#ifdef AFS_OSF_ENV
#include <sys/uio.h>
#endif /* AFS_OSF_ENV */
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV) || defined(SGIMIPS)
#include <sys/types.h>
#endif /* AFS_AIX_ENV || AFS_HPUX_ENV SGIMIPS */
#include <dce/ker/krpc_helper.h>
#include <cm.h>
#include <cm_cell.h>
#include <cm_volume.h>
#include <cm_conn.h>
#ifdef SGIMIPS
#include <sys/dirent.h>
#include <sys/kabi.h>
#endif /* SGIMIPS */


/*
 * Try to maintain no more than CM_VDIR_COUNT_TARGET
 * VDir entries
 */
static int   CM_VDIR_COUNT_TARGET = 100;

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_vdirent.c,v 65.8 1998/06/22 15:06:08 bdr Exp $")

static struct squeue  VDIRQ;

static long cm_TryBind _TAKES((struct cm_scache *, char *, struct cm_scache **,
				struct cm_rrequest *, struct cm_cell **));
#ifdef CM_DISKLESS_SUPPORT
static long cm_DisklessTryBind _TAKES((char *, struct cm_rrequest *));
#endif /* CM_DISKLESS_SUPPORT */

#if defined(KERNEL) && defined(SGIMIPS)
static int cm_EvalVDirName _TAKES((struct cm_scache *, char *, char *, long, 
				   char **));
static int cm_ReadHelper _TAKES((struct krpc_helper *, unsigned char *, int, 
			         long *));
static void cm_ConfigSubnode _TAKES((struct cm_scache *, struct cm_vdirent *, 
				     int, char *));
static int cm_ConfigVDirCell _TAKES((char *, struct afsNetAddr *,
                                     struct cm_rrequest *, u_long, 
				     struct cm_scache **,
                                     afsUUID *, char *, char **, long));
static void cm_InstallVDirCell _TAKES((struct cm_scache *, struct cm_vdirent *,
                                       struct cm_scache *));
static void cm_ConfigSubnode _TAKES((struct cm_scache *, struct cm_vdirent *, 
				     int, char *));
#endif /* KERNEL && SGIMIPS */

#define CM_ROOTINO	2		/* value to use for root's inode */
#ifdef SGIMIPS
static afs_hyper_t cm_VDirVnodeCtr = 100; /* make it bigger than root inode */
#else  /* SGIMIPS */
static long cm_VDirVnodeCtr = 100;	/* make it bigger than root inode */
#endif /* SGIMIPS */

static struct lock_data cm_vdirlock;		/* for the following counters */
static long cm_VDirCount = 0;	/* count of vdirs */

#ifdef SGIMIPS
void cm_VDirInit(void)
#else  /* SGIMIPS */
void cm_VDirInit()
#endif /* SGIMIPS */
{
    lock_Init(&cm_vdirlock);
    cm_VDirCount = 0;
    QInit(&VDIRQ);
}

#ifdef SGIMIPS
static cm_ReadHelper(
  struct krpc_helper *hcp,
  unsigned char *bp,
  int bsize,
  long *out)
#else  /* SGIMIPS */
static cm_ReadHelper(hcp, bp, bsize, out)
  struct krpc_helper *hcp;
  unsigned char *bp;
  int bsize;
  long *out;
#endif /* SGIMIPS */
{
    int code;

    osi_RestorePreemption(0);
    code = krpc_ReadHelper(hcp, bp, bsize, out);
    osi_PreemptionOff();
    return code;
}

/*
 * Evaluate the whole path name to a subdir of a virtual dir. Don't change
 * scache reference counts because they're already held by the basic vdirents
 * that name them.
 */
#ifdef SGIMIPS
static int cm_EvalVDirName(
    struct cm_scache *scp,
    char *namep,
    char *bufferp,
    long bufferLen,
    char **startPospp)
#else  /* SGIMIPS */
static cm_EvalVDirName(scp, namep, bufferp, bufferLen, startPospp)
    struct cm_scache *scp;
    char *namep;
    char **startPospp;
    char *bufferp;
    long bufferLen;
#endif /* SGIMIPS */
{
    register struct cm_scache *tscp = scp;
    register struct cm_vdirent *vdirp;
    register char *sp;
    register int tlen;
    int count = 0;

#ifdef SGIMIPS
    tlen = (int) strlen(namep)+1;
#else  /* SGIMIPS */
    tlen = strlen(namep)+1;
#endif /* SGIMIPS */

    /* start just past the first usable character */
    sp = bufferp + bufferLen;
    bcopy(namep, sp - tlen, tlen);	/* copy name, including null */
    sp -= tlen;				/* re-adjust this pointer */
    for (;;) {
	if (count++ > 60)
	    panic("EvalVDirName looping");
	vdirp = tscp->parentVDIR;
	if (!vdirp)
	    break;			/* finally hit the afs root */
	if (!(tscp->states & SC_VDIR))
	    panic("real virtual dir");
	/*
	 * Otherwise we have a virtual dir that has a non-trivial name
	 */
	*(--sp) = '/';			/* push a "/" on the stack */
#ifdef SGIMIPS
	tlen = (int)strlen(vdirp->name);
#else  /* SGIMIPS */
	tlen = strlen(vdirp->name);
#endif /* SGIMIPS */
	sp -= tlen;
	bcopy(vdirp->name, sp, tlen);
	tscp = vdirp->pvp;		/* move back up */
    }
    /*
     * Here we've got the whole name generated, so just return
     */
    *startPospp = sp;			/* tell them where we put the name */
    return 0;
}


/*
 * Return a new VDIR-style vcache entry
 */
#ifdef SGIMIPS
struct cm_scache *cm_GetVDirSCache(int destType)
#else  /* SGIMIPS */
struct cm_scache *cm_GetVDirSCache(destType)
    int destType;
#endif /* SGIMIPS */
{
    afsFid tfid;
    register struct cm_scache *scp;

    AFS_hset64(tfid.Cell, 0xffffffff, 0xffffffff);
    AFS_hset64(tfid.Volume, 0xffffffff, 0xffffffff);
#ifdef SGIMIPS
    tfid.Vnode = (unsigned32) cm_VDirVnodeCtr++;
#else  /* SGIMIPS */
    tfid.Vnode = cm_VDirVnodeCtr++;
#endif /* SGIMIPS */
    tfid.Unique = 1;

    lock_ObtainWrite(&cm_scachelock);
    scp = cm_FindSCache(&tfid);
    if (!scp)
	/*
	 * No cache entry, better grab one.
	 */
	scp = cm_NewSCache(&tfid, (struct cm_volume *) 0);
    lock_ReleaseWrite(&cm_scachelock);
    lock_ObtainWrite(&scp->llock);
    scp->states |= SC_VDIR;
#ifdef SGIMIPS
    cm_vSetType(scp, ((enum vtype) destType));
#else  /* SGIMIPS */
    cm_vSetType(scp, destType);
#endif /* SGIMIPS */
    scp->m.Mode = 0555;
    lock_ReleaseWrite(&scp->llock);
    return scp;
}


/*
 * Find a vdir entry if it exists.  If an entry is found which is
 * under construction, then wait for it to stabilize in which case
 * the dscp->llock will be released.  If an entry is not found, and
 * a lastElement is provided, then lastElement is set the the last
 * entry in the vdir list. The caller must have the dscp->llock write locked.
 */
#ifdef SGIMIPS
static struct cm_vdirent *cm_FindVDirElement(
    struct cm_scache *dscp,
    char *namep,
    struct cm_vdirent **lastElement)
#else  /* SGIMIPS */
static struct cm_vdirent *cm_FindVDirElement(dscp, namep, lastElement)
    struct cm_scache *dscp;
    char *namep;
    struct cm_vdirent **lastElement;
#endif /* SGIMIPS */
{
    register struct cm_vdirent *vdirp, *pvdirp;

    for (;;) {
	pvdirp = dscp->vdirList;
	for (vdirp = pvdirp; vdirp; vdirp = vdirp->next) {
	    if (strcmp(namep, vdirp->name) == 0)
		break;
	    pvdirp = vdirp;
	}
	if (vdirp && (vdirp->states & CM_VDIR_BUSY)) {
	    vdirp->states |= CM_VDIR_WAITING;
	    osi_SleepW(&vdirp->states, &dscp->llock);
	    lock_ObtainWrite(&dscp->llock);
	    continue; /* recheck */
	}
	break;
    }
    if (lastElement)
	*lastElement = ((vdirp)? (struct cm_vdirent *)0: pvdirp);

    if (vdirp &&
	strcmp(namep, "..") && strcmp(namep, ".")) {
	/*
	 * move this entry to the end of the age queue
	 */
	lock_ObtainWrite(&cm_vdirlock);
	QRemove(&vdirp->ageq);
	QAdd(&VDIRQ, &vdirp->ageq);
	lock_ReleaseWrite(&cm_vdirlock);
    }
    return(vdirp);
}


/*
 * Set the busy state on the vdir while it is unstable.
 * The caller should have the dscp->llock write locked.
 */
#ifdef SGIMIPS
static void cm_SetVDirBusy( struct cm_vdirent *vdirp)
#else  /* SGIMIPS */
static void cm_SetVDirBusy(vdirp)
    struct cm_vdirent *vdirp;
#endif /* SGIMIPS */
{
    osi_assert((vdirp->states & CM_VDIR_BUSY) == 0);
    vdirp->states |= CM_VDIR_BUSY;
}


/*
 * Clear the busy state on the vdir and notify waiters.
 * The caller should have the dscp->llock write locked.
 */
#ifdef SGIMIPS
static void cm_ClearVDirBusy( struct cm_vdirent *vdirp)
#else  /* SGIMIPS */
static void cm_ClearVDirBusy(vdirp)
    struct cm_vdirent *vdirp;
#endif /* SGIMIPS */
{
    osi_assert(vdirp->states & CM_VDIR_BUSY);
    vdirp->states &= ~CM_VDIR_BUSY;
    if (vdirp->states & CM_VDIR_WAITING) {
	vdirp->states &= ~CM_VDIR_WAITING;
	osi_Wakeup(&vdirp->states);
    }
}


/*
 * Ensure that an element is present in a vdir's list of dir elements. Create
 * a new one if not present.  Create it in an "unknown" state, which will be
 * further resolved as we proceed.
 */
#ifdef SGIMIPS
static struct cm_vdirent *cm_AddVDirElement(
    struct cm_scache *dscp,
    char *namep)
#else  /* SGIMIPS */
static struct cm_vdirent *cm_AddVDirElement(dscp, namep)
    struct cm_scache *dscp;
    char *namep;
#endif /* SGIMIPS */
{
    struct cm_vdirent *vdirp, *lastElement;
    vdirp = cm_FindVDirElement(dscp, namep, &lastElement);
    if (!vdirp) {
	/*
	 * No such entry, create it (at the end of the list, so as to avoid
	 * changing all the vdirent directory offsets). Note that terminating
	 * null is part of struct cm_vdirent size.
	 */
	vdirp = (struct cm_vdirent *)osi_Alloc(sizeof(*vdirp) + strlen(namep));
	bzero((caddr_t)vdirp, sizeof(*vdirp));
	if (lastElement)
	    lastElement->next = vdirp;
	else
	    dscp->vdirList = vdirp;
	strcpy(vdirp->name, namep);
	vdirp->states = CM_VDIR_GOODNAME;
	vdirp->timeout = 0xffffffff;	/* not until 2106 */
	vdirp->pvp = dscp;
	/*
	 * place the new entry in the age queue
	 * "." and ".." are excluded from this queue
	 */
	if (strcmp(namep, "..") && strcmp(namep, ".")) {
	    lock_ObtainWrite(&cm_vdirlock);
	    QAdd(&VDIRQ, &vdirp->ageq);
	    cm_VDirCount++;
	    lock_ReleaseWrite(&cm_vdirlock);
	}
    }
    return vdirp;
}


/*
 * Reclaim the least recently used VDir entries until their population
 * is at or below CM_VDIR_COUNT_TARGET
 */
#ifdef SGIMIPS
static void cm_ReclaimVDirs(void)
#else  /* SGIMIPS */
static void cm_ReclaimVDirs()
#endif /* SGIMIPS */
{
    struct cm_scache   *scp, *pscp;
    struct cm_vdirent  *vdirp, *vdirp2, **prevpp;
    struct squeue *vp, *nextp;
    long           freed = 0;
    long           inspected = 0;

    lock_ObtainWrite(&cm_vdirlock);
    if (cm_VDirCount <= CM_VDIR_COUNT_TARGET) {
	lock_ReleaseWrite(&cm_vdirlock);
	return;
    }

    icl_Trace1(cm_iclSetp, CM_TRACE_VDIR_RECLAIM,
		   ICL_TYPE_LONG, cm_VDirCount);

    /*
     * Reclaim the least-recently used eligible VDir until
     * we reach CM_VDIR_COUNT_TARGET
     */

    lock_ObtainWrite(&cm_scachelock);
    for (vp = VDIRQ.prev; vp != &VDIRQ &&
	 cm_VDirCount > CM_VDIR_COUNT_TARGET; vp = nextp) {
	inspected++;
	vdirp = (struct cm_vdirent *) vp;
	nextp = QPrev(vp);

	osi_assert(vdirp->pvp->states & SC_VDIR);

	scp = vdirp->vp;      /* the child scache  (may be NULL) */
	pscp = vdirp->pvp;    /* the parent scache               */

	/*
	 * We need to get the low-level locks on both the parent scache
	 * and child scache (if it exists).  Because these locks are higher up
	 * in the locking order than ones that we already have, we risk deadlock
	 * if we block here.  So, we attempt to obtain the locks without blocking,
	 * and we simply pass this vdir by for the time being if we fail.
	 */

	if (!lock_ObtainWriteNoBlock(&pscp->llock)) {
	    continue;
	}

	if (scp && !lock_ObtainWriteNoBlock(&scp->llock)) {
	    lock_ReleaseWrite(&pscp->llock);
	    continue;
	}

	/*
	 * A  vdir entry may now be removed if it is non-busy and either:
	 *  1) it does not reference an scache (i.e., the vdir
	 *     was generated by a failed name lookup) or
	 *
	 *  2) it uniquely references an scache, and the scache
	 *     has no non-trivial entries (i.e., the directory has
	 *     only "." and ".." cached).
	 *
	 *  Note:  The second part of Condition 2) drives the reclamation in a bottom-up
	 *  fashion.  We could have decided to reclaim non-leaf-level vdirs, but this
	 *  would have required us to search the entire subtree to verify that there
	 *  are no scaches that are referenced from outside the tree.
	 */

	if ((vdirp->states & (CM_VDIR_BUSY| CM_VDIR_WAITING)) ||
#ifdef SGIMIPS
	    (scp && (CM_RC(scp) > 2 ||
#else  /* SGIMIPS */
	    (scp && (CM_RC(scp) > 1 ||
#endif /* SGIMIPS */
		     scp->vdirList == NULL ||
		     scp->vdirList->next == NULL ||
		     scp->vdirList->next->next != NULL))) {

	    /*
	     * place this ineligible vdir entry at the end of the
	     * list (so we don't search through it again next time)
	     */
	    QRemove(&vdirp->ageq);
	    QAdd(&VDIRQ, &vdirp->ageq);
	} else {
	    /*
	     * remove the vdir entry
	     */
	    freed++;

	    if (scp) {
		osi_assert((!(strcmp(scp->vdirList->name, "..") &&
			      strcmp(scp->vdirList->name, ".")) &&
			    !(strcmp(scp->vdirList->next->name, "..") &&
			      strcmp(scp->vdirList->next->name, "."))));
		/* Unlink the scache, so it may be reclaimed as well */
		/* Also, clear .. ptr in scache we're disconnecting */
		scp->parentVDIR = NULL;
		CM_RELE(scp);
	    }

	    /* remove the vdir entry from its parent's vdir list  */
	    vdirp2 = pscp->vdirList;
	    prevpp = &pscp->vdirList;

	    /* Find entry in scache list  */
	    while (vdirp2 != vdirp) {
		prevpp = &vdirp2->next;
		vdirp2 = vdirp2->next;
		osi_assert(vdirp2 != NULL);
	    }
	    /* remove it from the list */
	    *prevpp = vdirp->next;
	    icl_Trace4(cm_iclSetp, CM_TRACE_VDIR_RECLAIMING_NOW,
		       ICL_TYPE_POINTER, pscp,
		       ICL_TYPE_STRING, vdirp->name,
		       ICL_TYPE_POINTER, vdirp->vp,
		       ICL_TYPE_LONG, vdirp->states);
	    QRemove(&vdirp->ageq);
	    osi_Free(vdirp, sizeof(*vdirp) + strlen(vdirp->name));
	    cm_VDirCount--;
	}
	lock_ReleaseWrite(&pscp->llock);
	if (scp) lock_ReleaseWrite(&scp->llock);
    }
    lock_ReleaseWrite(&cm_scachelock);
    icl_Trace3(cm_iclSetp, CM_TRACE_VDIR_RECLAIM_DONE,
	       ICL_TYPE_LONG, inspected,
	       ICL_TYPE_LONG, freed,
	       ICL_TYPE_LONG, cm_VDirCount);
    lock_ReleaseWrite(&cm_vdirlock);
}


/*
 * Make a vdirent structure have a fake vnode representing either a virtual
 * directory or a virtual symlink.
 * Takes the vdirent structure into which to insert the fake object, and the
 * parent dir, so that we can do any required locking.
 */
#ifdef SGIMIPS
static void cm_ConfigSubnode(
    struct cm_scache *dscp,
    struct cm_vdirent *vdirp,
    int destType,
    char *linkContents)
#else  /* SGIMIPS */
static void cm_ConfigSubnode(dscp, vdirp, destType, linkContents)
    struct cm_scache *dscp;
    struct cm_vdirent *vdirp;
    int destType;
    char *linkContents;
#endif /* SGIMIPS */
{
    struct cm_scache *scp;
    struct cm_vdirent *tvdirp;
    extern char *cm_rootName;
    extern int cm_rootNameLen;
    char *newLinkData = NULL;
    int nldLen;

    if (destType == ((int) VLNK)) {
#ifdef SGIMIPS
	nldLen = (int)strlen(linkContents) + cm_rootNameLen + 1;
#else  /* SGIMIPS */
	nldLen = strlen(linkContents) + cm_rootNameLen + 1;
#endif /* SGIMIPS */
	newLinkData = (char *) osi_Alloc(nldLen);
	strcpy(newLinkData, cm_rootName);
	strcpy(newLinkData+cm_rootNameLen, linkContents);
    }

    lock_ObtainWrite(&dscp->llock);
    if (vdirp->vp) {
	/* If it's the right type and the right data, leave it alone. */
	/* Otherwise, throw it away and use the new one. */
	if (((int) cm_vType(vdirp->vp) == destType)
	    && ((destType != ((int) VLNK))
		|| (strcmp(newLinkData, vdirp->vp->linkDatap) == 0))) {
	    lock_ReleaseWrite(&dscp->llock);
	    if (newLinkData)
		osi_Free(newLinkData, nldLen);
	    return;
	} else {
	    /* Clear .. ptr in scache we're disconnecting */
	    vdirp->vp->parentVDIR = NULL;
	    cm_PutSCache(vdirp->vp);
	    vdirp->vp = NULL;
	}
    }
    lock_ReleaseWrite(&dscp->llock);
    scp = cm_GetVDirSCache(destType);
    lock_ObtainWrite(&dscp->llock);
    /*
     * See if someone else filled it in for us
     */
    if (!vdirp->vp) {
	vdirp->vp = scp;
    } else {
	if (((int) cm_vType(vdirp->vp)) == destType) {
	    cm_PutSCache(scp);	/* throw away the new one */
	    scp = vdirp->vp;		/* then update the old one */
	} else {
	    /* Clear .. ptr in scache we're disconnecting */
	    vdirp->vp->parentVDIR = NULL;
	    cm_PutSCache(vdirp->vp);	/* throw away the old one */
	    vdirp->vp = scp;
	}
    }
    vdirp->inode = scp->fid.Vnode + (AFS_hgetlo(scp->fid.Volume)<<16);
    scp->parentVDIR = vdirp;

    if (destType == ((int) VDIR)) {
	/*
	 * Make up fake . and .. entries for things to work
	 */
	tvdirp = cm_AddVDirElement(scp, ".");
	tvdirp->inode = vdirp->inode;
	tvdirp = cm_AddVDirElement(scp, "..");
	if (dscp->parentVDIR)
	    tvdirp->inode = dscp->parentVDIR->inode;
	else
	    tvdirp->inode = CM_ROOTINO;	/* look like UFS root inode */
    } else if (destType == ((int) VLNK)) {
	/* Store the correct contents: this will be a symbolic link. */
	/* If the arg is, say, "com/xarc", this points to "/afs/com/xarc". */
	if (scp->linkDatap) {
	    osi_Free(scp->linkDatap, (strlen(scp->linkDatap) + 1));
	}
	scp->linkDatap = newLinkData;
	newLinkData = NULL;
    }
    lock_ReleaseWrite(&dscp->llock);
    if (newLinkData)
	osi_Free(newLinkData, nldLen);
}


/*
 * Same thing as ConfigSubnode(VDIR), only for the global root, which doesn't
 * have an avdp to connect to.
 */
#ifdef SGIMIPS
void cm_ConfigRootVDir(struct cm_scache *dscp)
#else  /* SGIMIPS */
void cm_ConfigRootVDir(dscp)
    struct cm_scache *dscp;
#endif /* SGIMIPS */
{
    struct cm_vdirent *vdirp;

    lock_ObtainWrite(&dscp->llock);
    vdirp = cm_AddVDirElement(dscp, ".");
    vdirp->inode = CM_ROOTINO;
    vdirp = cm_AddVDirElement(dscp, "..");
    vdirp->inode = CM_ROOTINO;
    lock_ReleaseWrite(&dscp->llock);
}

/*
 * Common code to get a root scache.
 */
#ifdef SGIMIPS
static int cm_DoCellRootScp(
    afsFid *afidp,
    struct cm_volume *avolp,
    struct cm_rrequest *rreqp,
    struct cm_scache **newscpp)
#else  /* SGIMIPS */
static int cm_DoCellRootScp(afidp, avolp, rreqp, newscpp)
    afsFid *afidp;
    struct cm_volume *avolp;
    struct cm_rrequest *rreqp;
    struct cm_scache **newscpp;
#endif /* SGIMIPS */
{
    struct cm_scache *rootscp;
    long code;

    /*
     * Get cell root's vnode number and fid from FX
     */
    icl_Trace1(cm_iclSetp, CM_TRACE_GETCELLROOT,
	       ICL_TYPE_FID, afidp);
    rootscp = cm_GetRootSCache(afidp, rreqp);
    if (!rootscp) {
	return ENOENT;
    }
    rootscp->states |= SC_CELLROOT;
    /* Set the whole fid (cell, vol too) in the volp, for the HERE token */
    avolp->rootFid = rootscp->fid;
    /* Check for staleness */
    lock_ObtainRead(&rootscp->llock);
    code = cm_GetTokens(rootscp, TKN_OPEN_PRESERVE, rreqp, READ_LOCK);
    lock_ReleaseRead(&rootscp->llock);
    if (code) {              /* shouldn't happen! */
	/* Maybe, if the fid of the root of the cell's root.dfs changes. */
	cm_PutSCache(rootscp);
	return ENOENT;
    }
    *newscpp = rootscp;	/* return the high-ref-counted structure here */
    return 0;
}

/*
 * Common code for disk-full and diskless case.
 */
#ifdef SGIMIPS
static int cm_DoCellVDirConfig(
    struct cm_cell *acellp,
    struct cm_rrequest *rreqp,
    struct cm_scache **newscpp)
#else  /* SGIMIPS */
static int cm_DoCellVDirConfig(acellp, rreqp, newscpp)
    struct cm_cell *acellp;
    struct cm_rrequest *rreqp;
    struct cm_scache **newscpp;
#endif /* SGIMIPS */
{
    struct cm_volume *volp;
    afsFid tfid;
    struct cm_scache *rootscp;
    long code;

    if ((acellp->lclStates & CLL_IDSET) == 0) {
	if (code = cm_FindCellID(acellp))
#ifdef SGIMIPS
	    return (int)code;
#else  /* SGIMIPS */
	    return code;
#endif /* SGIMIPS */
    }
    /*
     * Now we have the configured cell.  Find the root vnode, too.
     */
    volp = cm_GetVolumeByName("root.dfs", &acellp->cellId, 1, rreqp);
    if (!volp)
	return ENODEV;
    /*
     * Now we have enough information to generate a file ID and get a vnode
     * from it, which we'll need to configure the rest of the cell.
     */
    tfid.Cell = acellp->cellId;
    if (!AFS_hiszero(volp->roVol)) {
	tfid.Volume = volp->roVol;
	cm_PutVolume(volp);
	volp = cm_GetVolume(&tfid, rreqp);
	if (!volp) {
	    return ENODEV;
	}
    } else {
	tfid.Volume = volp->volume;
    }
    code = cm_DoCellRootScp(&tfid, volp, rreqp, &rootscp);
    if (code == 0) {
	*newscpp = rootscp;	/* return the high-ref-counted structure here */
    }
    cm_PutVolume(volp);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

/*
 * Merge in the new information that we just got from the bind system,
 * looking up the appropriate vnode and squirreling it away.
 */
#ifdef SGIMIPS
static int cm_ConfigVDirCell(
    char *pathp,
    struct afsNetAddr addrsp[],
    struct cm_rrequest *rreqp,
    u_long flags,
    struct cm_scache **newscpp,
    afsUUID *objIdp,
    char *cellnamep,
    char **princnamesp,
    long princCount)
#else  /* SGIMIPS */
static int cm_ConfigVDirCell(pathp, addrsp, rreqp, flags, newscpp, objIdp,
			     cellnamep, princnamesp, princCount)
    char *pathp;
    char *cellnamep;
    struct afsNetAddr addrsp[];
    struct cm_rrequest *rreqp;
    u_long flags;
    struct cm_scache **newscpp;
    afsUUID *objIdp;
    char **princnamesp;
    long princCount;
#endif /* SGIMIPS */
{
    struct cm_cell *tcellp;
#ifndef SGIMIPS
    struct cm_volume *volp;
    struct cm_scache *rootscp;
#endif /* !SGIMIPS */
    long code;

    /* don't check for existence first, since we want to update
     * configuration info even if cell already exists.
     */
#ifdef SGIMIPS
    tcellp = cm_NewCell(cellnamep, (long)flags, objIdp, (int)princCount, 
			princnamesp,
			(struct sockaddr_in *)addrsp);
#else  /* SGIMIPS */
    tcellp = cm_NewCell(cellnamep, flags, objIdp, princCount, princnamesp,
			(struct sockaddr_in *)addrsp);
#endif /* SGIMIPS */
    if (!tcellp)
	return ENOTDIR;
    code = cm_DoCellVDirConfig(tcellp, rreqp, newscpp);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}

#ifdef CM_DISKLESS_SUPPORT
/*
 * Given a cell name and a volume name, configure the root of that cell.  This
 * is typically used by the diskless folks, but is also used when a user
 * specifies a non-global root for the top of the naming system.
 */
#ifdef SGIMIPS
int cm_ConfigDisklessRoot(
     char *acellNamep,
     char *avolNamep,
     struct cm_scache **newscpp,
     struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
int cm_ConfigDisklessRoot(acellNamep, avolNamep, newscpp, rreqp)
     char *acellNamep;
     char *avolNamep;
     struct cm_scache **newscpp;
     struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    register struct cm_volume *volp;
    struct cm_scache *rootscp;
    register long code;
    register struct cm_cell *tcellp;

    /* first make sure the cell is configured */
    code = cm_DisklessTryBind(acellNamep, rreqp);
    if (code)
        return code;
    tcellp = cm_GetCellByName(acellNamep);
    if (!tcellp)
	return ENODEV;
    code = cm_DoCellVDirConfig(tcellp, rreqp, newscpp);
    return code;
}
#endif /* CM_DISKLESS_SUPPORT */

/*
 * Fix up a vdirent once we have a new cell entry.
 * Called with dscp->llock write-locked.
 */
#ifdef SGIMIPS
static void cm_InstallVDirCell(
    struct cm_scache *dscp,
    struct cm_vdirent *vdirp,
    struct cm_scache *scp)
#else  /* SGIMIPS */
static void cm_InstallVDirCell(dscp, vdirp, scp)
    struct cm_scache *dscp;
    struct cm_vdirent *vdirp;
    struct cm_scache *scp;
#endif /* SGIMIPS */
{
    /*
     * This is part of the old trickiness.  Allocate a file id for the
     * mount point, which really doesn't exist, since this is a virtual dir.
     * Don't generate a new inode number if this is a reused vdirent,
     * since that'll confuse pwd the first time that it comes across it.
     */
    if (vdirp->vp) {
	/* OK, we've got enough information to finish filling in the
	 * vnode entry. Since this may be a re-evaluation call,
	 * we must be prepared to drop the reference count on
	 * the old vnode stored in the vdirent, if any.
	 */
	/* Also, clear .. ptr in scache we're disconnecting */
	vdirp->vp->parentVDIR = NULL;
	cm_PutSCache(vdirp->vp);
    } else {
	vdirp->inode = (cm_VDirVnodeCtr++) + (0xffffffff<<16);
    }

    /*
     * Leave the ref count high since we're storing new ref here
     */
    vdirp->vp = scp;

    /*
     * Finally, set the parentVDIR back pointer to this vdirent, so that stat
     * will return an inode # that'll connect up properly (for getwd).
     */
    scp->parentVDIR = vdirp;
}

/*
 * A cell's root fileset may have changed.
 */
#ifdef SGIMIPS
static int cm_RevalidateCellRoot(
    struct cm_scache *dscp,
    struct cm_vdirent *tbp,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
static int cm_RevalidateCellRoot(dscp, tbp, rreqp)
    struct cm_scache *dscp;
    struct cm_vdirent *tbp;
    struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    struct cm_scache *scp;
    struct cm_volume *volp;
    afsFid tfid;
#ifndef SGIMIPS
    struct cm_scache *rootscp;
#endif /* !SGIMIPS */
    long code;
    int needGet = 0;

    lock_ObtainRead(&tbp->vp->volp->lock);
    if (!(tbp->vp->volp->states & (VL_RECHECK|VL_HISTORICAL))) {
	/* it's been fixed by now */
	lock_ReleaseRead(&tbp->vp->volp->lock);
	return 0;
    }
    lock_ReleaseRead(&tbp->vp->volp->lock);
    lock_ReleaseWrite(&dscp->llock);
    scp = tbp->vp;
    bzero((char *)&tfid, sizeof(tfid));
    tfid.Cell = scp->volp->cellp->cellId;
    tfid.Volume = scp->volp->volume;
    volp = cm_GetVolume(&tfid, rreqp);
    if (volp == NULL) {
	volp = scp->volp;
	cm_HoldVolume(volp);
	needGet = 1;
    }
    if (needGet == 0
	&& (volp == scp->volp)
	&& ((volp->states & VL_HISTORICAL) == 0)
	&& ((volp->states & VL_RO) ?
	    (AFS_hsame(volp->volume, volp->roVol)) :
	    (AFS_hiszero(volp->roVol)))) {
	/* All's OK.  Just re-lock. */
	lock_ObtainWrite(&dscp->llock);
    } else {
	/* The old volume is invalid, one way or another */
	if (AFS_hiszero(volp->roVol))
	    tfid.Volume = volp->rwVol;
	else
	    tfid.Volume = volp->roVol;
	cm_PutVolume(volp);
	volp = cm_GetVolume(&tfid, rreqp);
	if (volp == NULL) {
	    lock_ObtainWrite(&dscp->llock);
	    return ENODEV;
	}
	icl_Trace3(cm_iclSetp, CM_TRACE_FIXCELLROOT,
		   ICL_TYPE_HYPER, &scp->volp->volume,
		   ICL_TYPE_HYPER, &tfid.Volume,
		   ICL_TYPE_STRING, (volp->volnamep? volp->volnamep : ""));
	/*
	 * Get cell root's vnode number and fid from FX
	 */
	code = cm_DoCellRootScp(&tfid, volp, rreqp, &scp);
	cm_PutVolume(volp);
	/* Now have to install the new scp. */
	lock_ObtainWrite(&dscp->llock);
	if (code != 0)
#ifdef SGIMIPS
	    return (int)code;
#else  /* SGIMIPS */
	    return code;
#endif /* SGIMIPS */
	cm_InstallVDirCell(dscp, tbp, scp);
    }
    return 0;
}


/*
 * Lookup a name in the directory, returning the appropriate vnode or an error
 * code. The vnode is returned held.  No vnode locks should be held when this
 * call is made. Takes into account that info previously obtained from the
 * name system may be obsolete.
 */

/*
 * If called with avpp == NULL, just find and configure the cell without
 * messing with mount points.The cell name will already be in baz/bar/foo form.
 */
#ifdef SGIMIPS
static long cm_TryBind(
    register struct cm_scache *dscp,
    char *namep,
    struct cm_scache **avpp,
    struct cm_rrequest *rreqp,
    struct cm_cell **cellpp)
#else  /* SGIMIPS */
static long cm_TryBind(dscp, namep, avpp, rreqp, cellpp)
    register struct cm_scache *dscp;
    char *namep;
    struct cm_scache **avpp;
    struct cm_rrequest *rreqp;
    struct cm_cell **cellpp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS64
    int buffer, replyCode, stringsLen, princCount, addrCount;
#else  /* SGIMIPS64 */
    long buffer, replyCode, stringsLen, princCount, addrCount;
#endif /* SGIMIPS64 */
    struct afsNetAddr *netAddrsp = 0;
#ifdef SGIMIPS64
    unsigned int Flags, TTL;
    u_long now = (unsigned long) osi_Time();
#else  /* SGIMIPS64 */
    u_long Flags, now = (unsigned long) osi_Time(), TTL;
#endif /* SGIMIPS64 */
    struct osi_buffer *bufferp = 0;
    char *namebufp, *tsp;
    char *pnames = 0;
    register struct cm_vdirent *tbp;
    register char *s;
    char *pathNP, *cellNP, *cellNameToUse;
    char **principalsp = 0;
    struct krpc_helper *hcp = 0;
    afs_hyper_t newCellID;
    register long code, temp;
    struct afsNetAddr taddr;
    struct cm_scache *newcellscp;
    register struct cm_cell *cellp;
    afsUUID newCellUUID;

    icl_Trace1(cm_iclSetp, CM_TRACE_TRYBIND, ICL_TYPE_STRING, namep);
    if (avpp) {
	lock_ObtainWrite(&dscp->llock);
	tbp = cm_FindVDirElement(dscp, namep, (struct cm_vdirent **)0);
    } else
	tbp = NULL;
    if (!tbp || tbp->timeout < now ||
	((tbp->vp) && (tbp->vp->fid.Unique == -1))) {
	if (avpp)
	    lock_ReleaseWrite(&dscp->llock);
	/*
	 * Didn't find the fake cell; try creating one. Start by calling out
	 * to user space to translate the cell name to the cell information.
	 */

	/* may block */

	osi_RestorePreemption(0);
	hcp = krpc_GetHelper(0);		/* get a helper request block */
	osi_PreemptionOff();

	if (!hcp) {
	    code = EIO;
	    icl_Trace0(cm_iclSetp, CM_TRACE_TRYBIND1);
	    goto done;
	}
	/*
	 * Send the query first. It consists of:
	 *	cell opcode (0)
	 * 	cell's len
	 * 	cell's name
	 */
	buffer = 0;			/* send cell opcode (0) */
	if (code = krpc_WriteHelper(hcp, (unsigned char *)&buffer,
		sizeof(buffer)))
	    goto done;
	bufferp = osi_AllocBufferSpace();
	/* Parcel out space from bufferp */
	netAddrsp = (struct afsNetAddr *)bufferp;
	principalsp = (char **) (((char *)netAddrsp) +
				 ((MAXVLHOSTSPERCELL * (ADDRSINSITE + 1)) *
				  sizeof(afsNetAddr)));
	namebufp = (char *) (((char *)principalsp)
			     + (MAXVLHOSTSPERCELL * sizeof(char *)));
	/* Verify that there was enough space */
	osi_assert((namebufp + 1024) < ((char *)bufferp + osi_BUFFERSIZE));
	if (avpp) {
	    if (code = cm_EvalVDirName(dscp, namep, namebufp, 1024, &tsp))
		goto done;
	} else {
	    tsp = namep;
	}
#ifdef SGIMIPS
	temp = buffer = (int)strlen(tsp)+1;	/* send cell's len */
#else  /* SGIMIPS */
	temp = buffer = strlen(tsp)+1;	/* send cell's len */
#endif /* SGIMIPS */
	if (code = krpc_WriteHelper(hcp, (unsigned char *)&buffer,
		sizeof(buffer)))
	    goto done;
	if (code = krpc_WriteHelper(hcp, (unsigned char *)tsp, temp))
	    goto done;

	/*
	 * Read the # of entries back; reversing the direction of the helper
	 * channel implicitly sends the requests and waits for a reply.  The
	 * reply can fail, however, if the process, for instance, is vaporized
	 * while processing the request, so checking for failure is important.
	 */
	now = (unsigned long) osi_Time();
	code = cm_ReadHelper(hcp, (unsigned char *)&replyCode,
			     sizeof(replyCode), (long *)NULL);
	if (code != 0)
#ifdef SGIMIPS
	    replyCode = (int)code;
#else  /* SGIMIPS */
	    replyCode = code;
#endif /* SGIMIPS */
	icl_Trace2(cm_iclSetp, CM_TRACE_TRYBIND2, ICL_TYPE_POINTER, avpp,
		   ICL_TYPE_LONG, replyCode);
	/*
	 * Either 0, ENOENT or EISDIR (name exists but is not a cell) are
	 * "expected" results. Others are bona fide errors which we should
	 * return to our caller.
	 */
	if (code != 0 /* error from cm_ReadHelper */ ||
	    (replyCode != 0 && replyCode != ENOENT && replyCode != EISDIR)) {
	    /* record error so we don't retry this for a while */
	    krpc_PutHelper(hcp);	/* we're done with helper now */
	    hcp = (struct krpc_helper *) 0;
	    if (avpp) {
		lock_ObtainWrite(&dscp->llock);
		tbp = cm_AddVDirElement(dscp, namep);
		/* This is a transient-error condition: neither bit is set */
		tbp->states &= ~(CM_VDIR_GOODNAME | CM_VDIR_BADNAME);
#if 0 /* keep the reference in case it returns */
		if (tbp->vp) {
		    /* Also, clear .. ptr in scache we're disconnecting */
		    tbp->vp->parentVDIR = NULL;
		    cm_PutSCache(tbp->vp);
		    tbp->vp = NULL;
		}
#endif /* 0 */
		if (replyCode > 255) {
		    cm_printf("dfs: dce error (%d) from dfsbind helper.\n",
			      replyCode);
		    /* map to a useable error value */
		    replyCode = EIO;
		}
		tbp->errCode = replyCode;
		temp = osi_Time();
		tbp->timeout = temp + 600;  /* down for the next ten minutes */
		/* Also increase proportionally to the delay time */
		temp -= now;	/* gives the duration of the helper call */
		if (temp > 0)
		    tbp->timeout += (temp << 2);
		lock_ReleaseWrite(&dscp->llock);
	    }
	    code = replyCode;		/* could pass ETIMEDOUT this way */
	    goto done;
	}
	if (replyCode == ENOENT) {	/* we cache authoritative negatives */
	    /*
	     * Need to read the TTL field, to find out how long to cache the
	     * failure.
	     */
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&TTL,
		sizeof(TTL), (long *)NULL ))
		goto done;
	    krpc_PutHelper(hcp);	/* we're done with helper now */
	    hcp = (struct krpc_helper *) 0;
	    if (avpp) {
		lock_ObtainWrite(&dscp->llock);
		tbp = cm_AddVDirElement(dscp, namep);
		tbp->states |= CM_VDIR_BADNAME;
		tbp->states &= ~CM_VDIR_GOODNAME;
		tbp->timeout = now + TTL;
#if 0 /* keep the reference in case it returns */
		if (tbp->vp) {
		    /* Also, clear .. ptr in scache we're disconnecting */
		    tbp->vp->parentVDIR = NULL;
		    cm_PutSCache(tbp->vp);
		    tbp->vp = NULL;
		}
#endif /* 0 */
		lock_ReleaseWrite(&dscp->llock);
	    }
	    code = ENOENT;
	} else if (replyCode == EISDIR) {
	    /*
	     * We create an actual new node here, but it is just a directory,
	     * not a cell root.We'll collect names within it over time. We get:
	     *		Timeout Time
	     *		Flags, if any
	     *		Length of dir name
	     *		Canonicalized dir name
	     */
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&TTL,
		sizeof(TTL), (long *)NULL ))
		goto done;
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&Flags,
		sizeof(Flags), (long *)NULL ))
		goto done;
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&stringsLen,
		sizeof(stringsLen), (long *) NULL))
		goto done;
	    if (stringsLen > (osi_BUFFERSIZE-1)) {
		code = E2BIG;
		goto done;
	    }
	    /*
	     * The remaining value is a string. It's null-terminated but we
	     * don't know its length a priori.  We use cm_ReadHelper to read
	     * into an oversize buffer, then parse up the results.
	     */
	    pnames = osi_AllocBufferSpace();
	    if (code = cm_ReadHelper(hcp, (unsigned char *)pnames, stringsLen,
				     (long *)NULL))
		goto done;
	    krpc_PutHelper(hcp);	/* we're done with helper now */
	    hcp = (struct krpc_helper *) 0;
	    pnames[stringsLen] = '\0';	/* ensure null termination */

	    s = pnames;
	    if (*s == '\0')  {
	        code = EINVAL; /* expect a name */
		goto done;
	    }
	    pathNP = s;
	    if (avpp) {
		/*
		 * We create a node here for either the primary name or just
		 * its alias.
		 */
		lock_ObtainWrite(&dscp->llock);
		tbp = cm_AddVDirElement(dscp, namep);
		cm_SetVDirBusy(tbp);
		lock_ReleaseWrite(&dscp->llock);
		if (strcmp(tsp, pathNP) == 0) {
		    /* The same name: add it as a vdir element. */
		    cm_ConfigSubnode(dscp, tbp, VDIR, (char *) NULL);
		} else {
		    /* Different names. One is a nickname.  Make a sym link. */
		    cm_ConfigSubnode(dscp, tbp, VLNK, pathNP);
		}
		lock_ObtainWrite(&dscp->llock);
		tbp->states |= CM_VDIR_GOODNAME;
		tbp->states &= ~(CM_VDIR_BADNAME);
		tbp->timeout = now + TTL;
		cm_HoldSCache(tbp->vp);	/* return the reference held */
		cm_ClearVDirBusy(tbp);
		lock_ReleaseWrite(&dscp->llock);
		*avpp = tbp->vp;
	    }
	} else {		/* We just got a good new cell! */
	    /*
	     * A response for a new cell contains the following:
	     *		# of db servers in Cell
	     *		Cell's ID
	     *		Timeout Time
	     *		Flags for cell configuration
	     *	        [sockaddrs of all db servers in Cell]
	     *		UUID for the cell (for the flservers there)
	     *		length of the remaining text
	     *		distinguished (canonical) path name
	     *		name of the cell
	     *		all numAddrs principal name strings.
	     */
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&buffer,
		sizeof(buffer), (long *)NULL ))
		goto done;
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&newCellID,
		sizeof(newCellID), (long *)NULL ))
		goto done;
	    if (buffer > 200) {	/* sanity check */
		code = E2BIG;
		goto done;
	    }
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&TTL,
		sizeof(TTL), (long *)NULL ))
		goto done;
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&Flags,
		sizeof(Flags), (long *)NULL ))
		goto done;

	    /* Read all "buffer" address-sets (null-terminated), remembering
	     * the first MAXVLHOSTSPERCELL of them for the newcell call.
	     * Guaranteed to be at least one address per set.
	     */
	    temp = 0;

	    for (princCount = 0; princCount < buffer; princCount++) {
		/* read some number of addrs for flserver */
		for (addrCount = 0; ; addrCount++) {
		    if (code = cm_ReadHelper(hcp,
					     (unsigned char *)&taddr,
					     sizeof(struct afsNetAddr),
					     (long *)NULL )) {
			goto done;
		    }

		    if (((struct sockaddr_in *)&taddr)->sin_addr.s_addr == 0) {
			/* address-set terminator */
			osi_assert(addrCount > 0);
			if (princCount < MAXVLHOSTSPERCELL) {
			    netAddrsp[temp++] = taddr;
			}
			break;
		    }

		    if (princCount < MAXVLHOSTSPERCELL &&
			addrCount < ADDRSINSITE) {
			/* save addr from set; convert to kernel-usable form */
			osi_KernelSockaddr(&taddr);
			((struct sockaddr_in *)&taddr)->sin_port = 0;
			netAddrsp[temp++] = taddr;
		    }
		}
	    }

	    if (code = cm_ReadHelper(hcp, (unsigned char *)&newCellUUID,
		sizeof(newCellUUID), (long *)NULL ))
		goto done;
	    if (code = cm_ReadHelper(hcp, (unsigned char *)&stringsLen,
		sizeof(stringsLen), (long *)NULL))
		goto done;
	    /* make sure there's space for this many terminal nulls */
	    if (stringsLen > ((osi_BUFFERSIZE-3)-buffer)) {
		code = E2BIG;
		goto done;
	    }
	    /*
	     * The remaining values are strings. They're null-terminated but
	     * we don't know their lengths a priori.  We use cm_ReadHelper to
	     * read into an oversize buffer,  then parse up the results.
	     */
	    pnames = osi_AllocBufferSpace();
	    if (code = cm_ReadHelper(hcp, (unsigned char *)pnames, stringsLen,
				     (long *)NULL))
		goto done;
	    krpc_PutHelper(hcp);	/* we're done with helper now */
	    hcp = (struct krpc_helper *) 0;
	    /* ensure termination for this many separate strings */
	    bzero(&pnames[stringsLen], buffer+3);

	    s = pnames;
	    if (*s == '\0') {
	       code = EINVAL; /* expect a path name */
	       goto done;
	    }
	    pathNP = s;
	    s += strlen(s) + 1;

#ifdef notdef /* To be resolved */
	    /*
	     * We expect a cell name from dfsbind, but don't crap out if we're
	     * doing unauth testing.
	     */
	    if (*s == '\0') {
	        code = EINVAL;  /* expect a cell name */
		goto done;
	    }
#endif /* To be resolved */

	    cellNP = s;
	    s += strlen(s) + 1;
	    /*
	     * Now we store the first MAXVLHOSTSPERCELL principal names,
	     * up to the number of hosts named.
	     */
	    for (temp = 0; temp < buffer; ++temp) {

#ifdef notdef /* To be resolved */
	        /*
		 * We expect a princ name from dfsbind, but don't crap out if
		 * we're doing unauth testing.
		 */
		 if (*s == '\0') {
		     code = EINVAL; /* expect a principal name */
		     goto done;
		 }
#endif /* To be resolved */

		if (temp < MAXVLHOSTSPERCELL) {
		    principalsp[temp] = s;
		}
		s += strlen(s) + 1;
	    }
	    if (temp < MAXVLHOSTSPERCELL) /* null terminate */
		principalsp[temp] = NULL;

	    /*
	     * We expect a princ name from dfsbind, but don't crap out if
	     * we're doing unauth testing.
	     */

	    cellNameToUse = cellNP;
	    if (cellNameToUse == NULL || *cellNameToUse == '\0')
		cellNameToUse = pathNP;
	    if (cellNameToUse != NULL && strncmp(cellNameToUse, "/.../", 5)==0)
		cellNameToUse += 5;

	    /*
	     * Now we have the hosts and the cell ID, so we're ready to call
	     * newcell to create the new cell structure. If this is really just
	     * a nickname, create only the nickname record for now.
	     */
	    if (avpp) {
		if (strcmp(tsp, pathNP) == 0) {
		    /*
		     * Same name: this is really the new cell. Go to some pain
		     * to make sure the new cell is creatable before actually
		     * adding it to the parent's virtual directory.
		     */
		    if (code = cm_ConfigVDirCell(pathNP, netAddrsp,
						 rreqp, Flags, &newcellscp,
						 &newCellUUID, cellNameToUse,
						 principalsp, buffer))

			goto done;
		    lock_ObtainWrite(&dscp->llock);
		    tbp = cm_AddVDirElement(dscp, namep);
		    cm_InstallVDirCell(dscp, tbp, newcellscp);
		    cm_SetVDirBusy(tbp);
		    lock_ReleaseWrite(&dscp->llock);
		} else {
		    /* Different name: namep is just a nickname for pathNP. */
		    lock_ObtainWrite(&dscp->llock);
		    tbp = cm_AddVDirElement(dscp, namep);
		    cm_SetVDirBusy(tbp);
		    lock_ReleaseWrite(&dscp->llock);
		    cm_ConfigSubnode(dscp, tbp, VLNK, pathNP);
#if 0
		    /* this can cause lots of extra cm_server entries */
		    /* configure in the new cell while we're here. */
		    cellp = cm_NewCell(cellNameToUse,
				       Flags,
				       &newCellUUID,
				       buffer,
				       principalsp,
				       (struct sockaddr_in *)netAddrsp);

		    if (cellp && (cellp->lclStates & CLL_IDSET) == 0)
			(void) cm_FindCellID(cellp);
#endif /* 0 */
		}
		lock_ObtainWrite(&dscp->llock);
		cm_HoldSCache(tbp->vp);	/* Return the reference held */
		*avpp = tbp->vp;
		tbp->states &= ~CM_VDIR_BADNAME;
		tbp->states |= CM_VDIR_GOODNAME;
		tbp->timeout = now + TTL;
		cm_ClearVDirBusy(tbp);
		lock_ReleaseWrite(&dscp->llock);
	    } else {
		/* Just in configure-a-new-cell mode, no mount points. */
		if (strcmp(tsp, pathNP) == 0) {
		    cellp = cm_NewCell(cellNameToUse,
				       Flags,
				       &newCellUUID,
				       buffer,
				       principalsp,
				       (struct sockaddr_in *)netAddrsp);
		    if (!cellp) {
			code = ENOTDIR;
		    } else {
			if ((cellp->lclStates & CLL_IDSET) == 0) {
			    if (code = cm_FindCellID(cellp))
				goto done;
			}
		    }
		} else {
#if 0
		    /* this can cause lots of extra cm_server entries */
		    /* configure in the new cell while we're here. */
		    cellp = cm_NewCell(cellNameToUse,
				       Flags,
				       &newCellUUID,
				       buffer,
				       principalsp,
				       (struct sockaddr_in *)netAddrsp);

		    if (cellp && (cellp->lclStates & CLL_IDSET) == 0)
			(void) cm_FindCellID(cellp);
#endif /* 0 */
		    code = EISDIR;
		}
	    }
	}
    } else {		/* have up-to-date tbp, saying yes or no */
	if (tbp->states & CM_VDIR_GOODNAME) {
	    code = 0;
	    if (tbp->vp) {
		cm_HoldSCache(tbp->vp);
		if (((tbp->vp->states & (SC_VDIR|SC_CELLROOT)) == SC_CELLROOT)
		    && (tbp->vp->volp != NULL)
		    && (tbp->vp->volp->states & (VL_RECHECK|VL_HISTORICAL))) {
		    /* it's a cell root: validate its volp */
		    code = cm_RevalidateCellRoot(dscp, tbp, rreqp);
		}
		*avpp = tbp->vp;
	    } else
		panic("trybind no vp3");
	} else {
	    if (tbp->states & CM_VDIR_BADNAME) {
		code = ENOENT;	/* not good name, this is a missing entry */
	    } else {
		code = tbp->errCode;	/* some transient error code */
	    }
	}
	lock_ReleaseWrite(&dscp->llock);
    }
done:
    if (bufferp)
	osi_FreeBufferSpace((struct osi_buffer *) bufferp);
    if (pnames)
	osi_FreeBufferSpace((struct osi_buffer *) pnames);
    if (hcp)
	krpc_PutHelper(hcp);
    if (code > 255) {
	cm_printf("dfs: dce errors (%d) from dfsbind helper.\n", code);
	code = EIO;   /* mapping to a "meaningful" error ! */
    }
    cm_ReclaimVDirs();
    return code;
}

#ifdef CM_DISKLESS_SUPPORT
/*
 * This is the diskless-only version of cm_TryBind().  It takes the
 * information that was stashed away in *cm_dlinfo as a result of
 * a previous call to cm_DoInitDLInit().  This is the way that the
 * necessary information is passed to cm_NewCell(), just as would
 * be done as the result of calling cm_TryBind().  The difference is
 * that this information is acquired from the diskless (DL) code/server
 * rather than by using RPC to communicate with "dfsbind" which is not
 * available.
 *
 * This code is otherwise the same as that of cm_TryBind() with
 * "struct cm_scache *dscp, **avpp" and "struct cm_cell **cellpp" set
 * to NULL from cm_ConfigDisklessRoot() as had originally been done.
 * It seemed like a better idea to come-up with a separate routine
 * rather than change the flow of an already overly-complicated
 * cm_TryBind().
 *
 * We are only calling the uninit function if everything goes ok.
 * The reason for this is primarily for debugging purposes because it
 * the information that will make debugging easier.  This could easily be
 * changed if needed.
 */

#ifdef SGIMIPS
static long cm_DisklessTryBind(
     char *namep,
     struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
static long cm_DisklessTryBind(namep, rreqp)
     char *namep;
     struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{
    register struct cm_cell *cellp;
    int code;
    extern struct cm_dlinfo *cm_dlinfo;		/* from cm_init.c */

    if (!cm_dlinfo)
       panic("cm_DisklessTryBind");
    cellp = cm_NewCell(namep, (struct afsNetAddr *)cm_dlinfo->serverAddr,
		       cm_dlinfo->flags, &cm_dlinfo->uuid,
		       cm_dlinfo->principal);

    if (!cellp) {
       return ENOTDIR;
    } else {
       if ((cellp->lclStates & CLL_IDSET) == 0) {
	   if (code = cm_FindCellID(cellp))
	       return code;
       }
    }
    cm_DoInitDLUnInit();
    return 0;
}
#endif /* CM_DISKLESS_SUPPORT */

/* function to reset vdir timeouts, for use by pioctl to refresh fileset
 * info.
 */
#ifdef SGIMIPS
void cm_ResetAllVDirs(void)
#else  /* SGIMIPS */
void cm_ResetAllVDirs()
#endif /* SGIMIPS */
{
    register struct cm_scache *scp;
    register long i;
    register struct cm_vdirent *tvp;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (scp = cm_shashTable[i]; scp; scp = scp->next) {
	    /*
	     * Here we have scachelock held, and a reference to scp, which
	     * we'll have to hold.
	     */
#ifdef SGIMIPS
            s = VN_LOCK(SCTOV(scp));
            if ( SCTOV(scp)->v_flag & VINACT ) {
                osi_assert(CM_RC(scp) == 0);
                VN_UNLOCK(SCTOV(scp), s);
                continue;
            }
            CM_RAISERC(scp);
            VN_UNLOCK(SCTOV(scp), s);
#else  /* SGIMIPS */
	    CM_HOLD(scp);
#endif /* SGIMIPS */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&scp->llock);
	    if (scp->states & SC_VDIR) {
		for (tvp = scp->vdirList; tvp; tvp=tvp->next) {
		    /* don't cut the timeout for the predefined entries */
		    if (strcmp(tvp->name, "..") && strcmp(tvp->name, ".")) {
			tvp->timeout = 0;
		    }
		}
	    }
	    /*
	     * Now drop this one and get to the next
	     */
	    lock_ReleaseWrite(&scp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(scp);
	}
    }
    lock_ReleaseWrite(&cm_scachelock);
}


/*
 * Handle stats for VDIRs
 */
#ifdef SGIMIPS
long cm_GetAttrVDir(
    register struct cm_scache *scp,
    register struct vattr *attrsp,
    struct cm_rrequest *reqp)
#else  /* SGIMIPS */
long cm_GetAttrVDir(scp, attrsp, reqp)
    register struct cm_scache *scp;
    register struct vattr *attrsp;
    struct cm_rrequest *reqp;
#endif /* SGIMIPS */
{
    osi_vattr_init(attrsp,
	OSI_VA_UID | OSI_VA_GID | OSI_VA_FSID | OSI_VA_TYPE |
	OSI_VA_MODE | OSI_VA_TIMES | OSI_VA_RDEV |
	OSI_VA_NBLOCKS | OSI_VA_SIZE | OSI_VA_BLKSIZE | OSI_VA_NLINK);
    attrsp->va_uid = 0;
    attrsp->va_gid = 0;
#ifdef AFS_SUNOS5_ENV
    attrsp->va_fsid = scp->v.v_vfsp->vfs_dev;
#else
    /* This just does what cmattr_to_vattr() does */
    attrsp->va_fsid = 1;
#endif /* AFS_SUNOS5_ENV */
    attrsp->va_type = cm_vType(scp);
    if (cm_vType(scp) == VDIR) {
	attrsp->va_mode = S_IFDIR | 0555;
    } else {		/* presume that it's our symlink */
	attrsp->va_mode = S_IFLNK | 0555;
    }
    if (scp->parentVDIR == 0) {
	/*
	 * This is "/afs" (or equivalent) itself, so make the nodeID 2 so that
	 * rpc.mountd thinks this is a root directory.
	 */
	attrsp->va_nodeid = CM_ROOTINO;
    } else
	attrsp->va_nodeid = (scp->fid.Vnode +
			     (AFS_hgetlo(scp->fid.Volume) << 16));
    attrsp->va_atime.tv_sec = attrsp->va_ctime.tv_sec =
      attrsp->va_mtime.tv_sec = 610992000;
    attrsp->va_atime.osi_subsec = attrsp->va_ctime.osi_subsec =
      attrsp->va_mtime.osi_subsec = 0;
    attrsp->va_rdev = 1;
#ifdef AFS_OSF_ENV
    attrsp->va_bytes = 1024;
#else
#ifdef SGIMIPS
    attrsp->va_nblocks = 1;
#else  /* SGIMIPS */
    attrsp->va_blocks = 1;
#endif /* SGIMIPS */
#endif /* AFS_OSF_ENV */
    /*
     * For old time's sake, should make positive to avoid confusing uiomove.
     */
    attrsp->va_size = 2048;
#ifdef SGIMIPS 
    attrsp->va_blksize = 8192;
#else  /* SGIMIPS */
    attrsp->va_blocksize = 8192; /* a reasonable transfer size */
#endif  /* SGIMIPS */
    attrsp->va_nlink = 3;	 /* non-empty dirs have link counts >= 3 */
    return 0;
}


/*
 * Show a dir entry if the name is good
 */
#define CM_SHOWVDIRENT(vx)	((((vx)->states) & CM_VDIR_GOODNAME) != 0)

#define ROUNDTOQUAD(len)        (((len + 4) &~ 3))

#ifdef SGIMIPS
long cm_ReadVDir(
  register struct cm_scache *scp,
  struct uio *uiop,
  int *eofp,
  struct cm_rrequest *reqp)
#else  /* SGIMIPS */
long cm_ReadVDir(scp, uiop, eofp, reqp)
  register struct cm_scache *scp;
  struct uio *uiop;
  int *eofp;
  struct cm_rrequest *reqp;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    dirent_t newirixdir;
    irix5_dirent_t oldirixdir;
    int is32;
#else  /* SGIMIPS */
    struct dir_minentry dirEntry;
#endif  /* SGIMIPS */
    register long code = 0;
    register struct cm_vdirent *vdirp;
    register struct cm_vdirent *next_vdirp;
    long oldOffset, newOffset = 0;

    *eofp = 0;
#ifdef SGIMIPS
    is32 = ABI_IS_IRIX5(GETDENTS_ABI(get_current_abi(), uiop));
#endif  /* SGIMIPS */
    lock_ObtainRead(&scp->llock);
    /*
     * Now scan for the specified offset.
     *
     * We recompute offsets on each call just in case a new name has
     * become valid recently.
     */
    for (vdirp = scp->vdirList; vdirp; vdirp = vdirp->next) {
	if (CM_SHOWVDIRENT(vdirp)) {
	    oldOffset = newOffset;
#ifdef SGIMIPS
	    if (is32) {
		newOffset += IRIX5_DIRENTSIZE(strlen(vdirp->name) +1);
	    } else {
		newOffset += DIRENTSIZE(strlen(vdirp->name) +1);
	    }
#else  /* SGIMIPS */
	    newOffset += ROUNDTOQUAD(strlen(vdirp->name) + SIZEOF_DIR_MINENTRY);
#endif  /* SGIMIPS */
	    if (oldOffset >= uiop->osi_uio_offset) {
		/*
		 * Found the first record we want,
		 * copy out from this point onward
		 */
		if ((newOffset - oldOffset) > uiop->osi_uio_resid) {
		    *eofp = 1;
		    break;
		}
#ifdef SGIMIPS
		if (is32) {
		    oldirixdir.d_reclen= (unsigned short)
		       		      IRIX5_DIRENTSIZE(strlen(vdirp->name)+1);
		    oldirixdir.d_ino = (unsigned int) vdirp->inode;
		} else {
		    newirixdir.d_reclen = (unsigned short)
					     DIRENTSIZE(strlen(vdirp->name)+1);
		    newirixdir.d_ino= (unsigned long)vdirp->inode;
		}
#else  /* SGIMIPS */
#ifndef AFS_SUNOS5_ENV
		dirEntry.namelen = strlen(vdirp->name);
#endif /* !AFS_SUNOS5_ENV */
		dirEntry.recordlen = newOffset - oldOffset;
		dirEntry.inode = vdirp->inode;
#endif /* SGIMIPS */
#if defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
		/*
		 * Find next valid entry in order to calculate offset value.
		 * This is important if NFS is exporting DFS as the NFS server
		 * is very picky about the offset field.
		 */
		next_vdirp = vdirp->next;
		while (next_vdirp && !CM_SHOWVDIRENT(next_vdirp))
		    next_vdirp = next_vdirp->next;

#ifdef SGIMIPS
                if (is32) {
                    oldirixdir.d_off = ((next_vdirp) ? (int)newOffset : 
					0x7fffffff);
                } else {
                    newirixdir.d_off = ((next_vdirp) ? newOffset : 0x7fffffff);
		}
		
#else  /* SGIMIPS */

		dirEntry.offset = ((next_vdirp)? newOffset: 0x7fffffff);
#endif /* SGIMIPS */
#endif /* AFS_AIX31_ENV || AFS_SUNOS5_ENV || SGIMIPS */

#ifdef SGIMIPS
                if(is32)  {
                    if (code = osi_uiomove((caddr_t)&oldirixdir,
                                        IRIX5_DIRENTBASESIZE, UIO_READ, uiop))
                        break;
                    if (code = osi_uiomove(vdirp->name, oldirixdir.d_reclen -
                                        IRIX5_DIRENTBASESIZE, UIO_READ, uiop))
                        break;
                } else {
                    if (code = osi_uiomove((caddr_t)&newirixdir,
                                        DIRENTBASESIZE, UIO_READ, uiop))
                        break;
                    if (code = osi_uiomove(vdirp->name, newirixdir.d_reclen -
                                        DIRENTBASESIZE, UIO_READ, uiop))
                        break;
                }
	    }
#else  /* SGIMIPS */
		if (code = osi_uiomove((caddr_t)&dirEntry,
				       SIZEOF_DIR_MINENTRY, UIO_READ, uiop))
		    break;
		if (code = osi_uiomove(vdirp->name, dirEntry.recordlen -
				       SIZEOF_DIR_MINENTRY, UIO_READ, uiop))
		    break;
	    }
#endif /* SGIMIPS */
	}
    }
    lock_ReleaseRead(&scp->llock);
    icl_Trace1(cm_iclSetp, CM_TRACE_CMREADVDIR, ICL_TYPE_LONG,
	       uiop->osi_uio_resid);
    return code;
}


#ifdef SGIMIPS
long cm_LookupVDir(
    register struct cm_scache *dscp, 
    char *namep,
    register struct cm_scache **scpp,
    struct cm_rrequest *reqp)
#else  /* SGIMIPS */
long cm_LookupVDir(dscp, namep, scpp, reqp)
    register struct cm_scache *dscp, **scpp;
    char *namep;
    struct cm_rrequest *reqp;
#endif /* SGIMIPS */
{
    register long code;
    struct cm_scache *tscp;
    struct cm_vdirent *vdirp;

    /*
     * Before doing any processing, check for "." and ".."
     */
    if (namep[0] == '.') {
	if (namep[1] == 0) {		/* "." */
	    *scpp = dscp;
	    cm_HoldSCache(dscp);
	    return 0;
	} else if (namep[1] == '.' && namep[2] == 0) {
	    vdirp = dscp->parentVDIR;	/* ".." */
	    if (!vdirp) {
		/*
		 * The real root (/afs); treat it as "." if we're asked
		 */
		tscp = dscp;
	    } else 			/* there's a real parent */
		tscp = vdirp->pvp;
	    if (tscp) {
		cm_HoldSCache(tscp);
		*scpp = tscp;
		return 0;
	    } else
		return ENOENT;
	}
    }

    /*
     * Special global-style bind vnode
     */
    code = cm_TryBind(dscp, namep, &tscp, reqp, NULL);
    if (code == 0)
	*scpp = tscp;
    return code;
}

#ifdef SGIMIPS
long cm_ConfigureCell(
    char *cellnp,
    struct cm_cell **cellpp,
    struct cm_rrequest *rreqp)
#else  /* SGIMIPS */
long cm_ConfigureCell(cellnp, cellpp, rreqp)
    char *cellnp;
    struct cm_cell **cellpp;
    struct cm_rrequest *rreqp;
#endif /* SGIMIPS */
{/* Attempt to configure in, dynamically, the cell with the given name. */
    long code;
    extern struct cm_scache *cm_rootVnode;

    icl_Trace1(cm_iclSetp, CM_TRACE_CONFIGCELL, ICL_TYPE_STRING, cellnp);
    *cellpp = NULL;
    /* Kazar changed this next line to use NULL for last parm since last
     * parm currently ignored, and I'm going to change that.
     */
    code = cm_TryBind(cm_rootVnode, cellnp, NULL, rreqp, NULL);

    /* try once again to do the lookup */
    if (code == 0 && *cellpp == NULL) {
	*cellpp = cm_GetCellByName(cellnp);  /* rest is cell name, look it up */
    }
    if (code != 0) {
	*cellpp = NULL;
	code = ESRCH;
    }
    icl_Trace1(cm_iclSetp, CM_TRACE_CONFIGCELLEND, ICL_TYPE_LONG, code);
    return code;
}

/*
 * Called when a vnode block is no longer referred-to.
 */
#ifdef SGIMIPS
void cm_FreeAllVDirs(
    struct cm_scache *scp)
#else  /* SGIMIPS */
void cm_FreeAllVDirs(scp)
    struct cm_scache *scp;
#endif /* SGIMIPS */
{
    register struct cm_vdirent *vdirp, *next_vdirp;
/* Called with cm_scachelock write-locked. */

    for (vdirp = scp->vdirList; vdirp; vdirp = next_vdirp) {
	next_vdirp = vdirp->next;
	osi_assert(vdirp->pvp == scp);
	if (vdirp->vp) {
	    /* Also, clear .. ptr in scache we're disconnecting */
	    vdirp->vp->parentVDIR = NULL;
	    CM_RELE(vdirp->vp);
	    vdirp->vp = NULL;
	}
	if (strcmp(vdirp->name, "..") &&
	    strcmp(vdirp->name, ".")) {
	    /*
	     * remove the entry from the vdir age queue
	     * ("." and ".." are excluded from this queue)
	     */
	    lock_ObtainWrite(&cm_vdirlock);
	    QRemove(&vdirp->ageq);
	    cm_VDirCount--;
	    lock_ReleaseWrite(&cm_vdirlock);
	}
	osi_Free(vdirp, sizeof(*vdirp) + strlen(vdirp->name));
    }

    scp->vdirList = 0;
}
