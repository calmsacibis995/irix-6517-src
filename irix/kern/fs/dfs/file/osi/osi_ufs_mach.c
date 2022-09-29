/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_ufs_mach.c,v 1.3 1998/03/23 16:26:33 gwehrman Exp $";
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
 * $Log: osi_ufs_mach.c,v $
 * Revision 1.3  1998/03/23 16:26:33  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 1.2  1998/03/05 17:33:15  gwehrman
 * Added include for osi_uio_mach.h.  Fixed variable name typo.
 *
 * Revision 1.1  1998/03/04 21:16:01  lmc
 * Add osi_getvdev() function, which moved in the 1.1 to 1.2.2 merge.
 *
 * Revision 65.2  1997/11/06  19:58:12  jdoak
 * Source Identification added.
 * 
 * $EndLog$
 */
/*
 * Copyright (c) 1993, 1994 Transarc Corporation.  All rights reserved.
 */

#include <dcedfs/osi.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/osi_uio_mach.h>

/*
 * Given a device name, return the device number and a vnode ptr.
 * If passed a non-null vpp, the vnode will be passed back in vpp
 *    and it will be held (VN_HOLD).  The caller assumes responsibility
 *    for VN_RELEasing it.
 * If, on the other hand, vpp is NULL, then the vnode will be
 *    VN_RELEased here, since the caller would have no handle on the
 *    vnode with which to release it.
 */
int osi_getvdev(fspec, devp, vpp)
    caddr_t fspec;			/* device pathname */
    dev_t *devp;			/* place to put device number */
    struct vnode **vpp;			/* device vnode */
{
    int 	code = 0;	/* error return code */
    struct vnode *vp;

    if (code = osi_lookupname (fspec, OSI_UIOSYS, FOLLOW_LINK, 0, &vp))
	return (code);

    if (osi_vType(vp) != VBLK) {
	code = ENOTBLK;
    } else {
	/* Get device number */
	*devp = vp->v_rdev;
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
    return (code);
}
