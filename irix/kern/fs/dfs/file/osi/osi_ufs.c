/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_ufs.c,v 65.3 1998/03/23 16:26:13 gwehrman Exp $";
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
 * HISTORY
 * $Log: osi_ufs.c,v $
 * Revision 65.3  1998/03/23 16:26:13  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:16  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.668.1  1996/10/02  18:11:52  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:14  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_kvnode.h>

#ifdef	AFS_OSF_ENV
#include <sys/specdev.h>
#endif	/* AFS_OSF_ENV */

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_ufs.c,v 65.3 1998/03/23 16:26:13 gwehrman Exp $")


#if !defined(KERNEL)
/*
 * Given a device name, return the device number and a vnode ptr.
 * If passed a non-null vpp, the vnode will be passed back in vpp
 *    and it will be held (VN_HOLD).  The caller assumes responsibility
 *    for VN_RELEasing it.
 * If, on the other hand, vpp is NULL, then the vnode will be
 *    VN_RELEased here, since the caller would have no handle on the
 *    vnode with which to release it.
 * This is going to remain the USER mode version.  Kernel mode versions
 * will be found in MACHINE/osi_ufs_mach.c.
 */
int osi_getvdev(fspec, devp, vpp)
    caddr_t fspec;			/* device pathname */
    dev_t *devp;			/* place to put device number */
    struct vnode **vpp;			/* device vnode */
{
    int code;				/* error return code */
    struct vnode *vp;

    if (code = osi_lookupname (fspec, OSI_UIOSYS, FOLLOW_LINK, 0, &vp))
	return (code);

    if (osi_vType (vp) != VBLK) {
	OSI_VN_RELE(vp);
	code = ENOTBLK;
    } else {
	if (devp) {
	    *devp = vp->v_rdev;
#ifdef	AFS_AIX31_ENV
	    *devp = brdev(*devp);	/* is this really necessary? */
#endif
	}
	if (vpp) {
	    /* caller intends to use the vnode,
	       therefore caller assumes responsibility for
	       VN_RELEasing it */
	    *vpp = vp;
	} else {
	    /* caller wasn't interested in the vnode */
	    OSI_VN_RELE(vp);
	}
    }
    return (code);
}
#endif	/* !KERNEL */
