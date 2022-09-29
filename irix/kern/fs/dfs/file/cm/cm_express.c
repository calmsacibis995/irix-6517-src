/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_express.c,v 65.4 1998/03/23 16:24:20 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1991 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>				/* Should be always first */
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>			/* Standard vendor system headers */
#include <dcedfs/xvfs_vnode.h>			/* VFS+ basic defs */
#include <dcedfs/volume.h>
#include <dcedfs/volreg.h>
#include <cm.h>					/* Cm-based standard headers */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_express.c,v 65.4 1998/03/23 16:24:20 gwehrman Exp $")

int cm_EnableExpress = 0;      /* Turn if OFF for now ?????????  */

/* 
 * Release a volume structure returned by cm_ExpressPath 
 */
#ifdef SGIMIPS
void cm_ExpressRele(
    register struct volume *volp, 
    register struct vnode *vp1,
    register struct vnode *vp2)
#else  /* SGIMIPS */
void cm_ExpressRele(volp, vp1, vp2)
    register struct vnode *vp1, *vp2;
    register struct volume *volp; 
#endif /* SGIMIPS */
{
    if (vp1) 
	OSI_VN_RELE(vp1);
    if (vp2) 
	OSI_VN_RELE(vp2);
    VOL_RELE(volp);
}


#ifdef SGIMIPS
long cm_ExpressPath(
    register struct cm_scache *scp1,
    register struct cm_scache *scp2,
    struct volume **volpp,
    struct vnode **vpp1,
    struct vnode **vpp2) 
#else  /* SGIMIPS */
long cm_ExpressPath(scp1, scp2, volpp, vpp1, vpp2)
    register struct cm_scache *scp1, *scp2;
    struct volume **volpp;
    struct vnode **vpp1, **vpp2; 
#endif /* SGIMIPS */
{
    struct volume *volumep = 0;
    register long code;
    struct vnode *tvp1 = 0, *tvp2 = 0;
    struct afsFid tfid;

    if (!cm_EnableExpress) 
	return 0;		/* debug: don't do express path calls */
    if (scp1->states & SC_VDIR) 
	return 0;		/* not on virtual dirs, either */		
    if (scp2 && (scp2->states & SC_VDIR)) 
	return 0;		/* not on virtual dirs, either */

#ifdef notdef
    /* prevent volume pointer from disappearing */
    lock_ObtainRead(&scp1->llock);
    if (!(scp1->volp->states & VL_LOCAL)) {
	lock_ReleaseRead(&scp1->llock);
	return 0;	/* not local, we're not executing the request locally */
    }
    /* from now on, will only be referencing fid field, which doesn't
     * require any locking.
     */
    lock_ReleaseRead(&scp1->llock);
#endif

    /* 
     * Call the volume registry to see if the data is *really* local 
     */
    if (code = volreg_Lookup(&scp1->fid, &volumep)) {
	if (code == ENODEV) return 0;	/* not really local, so just return */
	return EBUSY;	/* assume this is true, but could disambiguate more. */
    }

    /* 
     * Ok, the volume *is* local; let's do something with it 
     */
    tfid = scp1->fid;
    tfid.Cell = tkm_localCellID;
    code = VOL_VGET(volumep, &tfid, &tvp1);
    if (!code) {
	/* 
	 * OK, if we get here, we're ready to do the call locally. We'll return
	 * a good code, a held volume and the requisite held vnodes. The held
	 * volume will prevent the fileset from being moved off of this
	 * partition during the execution of the local vnode call.
         */
	*volpp = volumep;
	*vpp1 = tvp1;
	if (scp2) {			/* see if there's a 2nd to xlate */
	    tfid = scp2->fid;
	    tfid.Cell = tkm_localCellID;
	    code = VOL_VGET(volumep, &tfid, &tvp2);
	    if (!code) {
		*vpp2 = tvp2;
		return 1;
	    }
	} else
	    return 1;
	OSI_VN_RELE(tvp1);
    }
    VOL_RELE(volumep);
    return 0;
}
