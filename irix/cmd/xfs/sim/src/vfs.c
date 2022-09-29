/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

/*#ident	"@(#)uts-comm:fs/vfs.c	1.18"*/
#ident	"$Revision: 1.10 $"

#include <sys/param.h>
#include <sys/cred.h>
#include <ksys/behavior.h>
#include <sys/vfs.h>

vfs_t *rootvfs;

/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found.
 */
/* ARGSUSED */
struct vfs *
vfs_devsearch(dev_t dev)
{
	return NULL;
}

/*
 * Map VFS flags to statvfs flags.  These shouldn't really be separate
 * flags at all.
 */
/* ARGSUSED */
u_long
vf_to_stf(u_long vf)
{
	return 0;
}

/*
 * Declare the switch here for the simulation.
 */
vfssw_t	vfssw[2];

/*
 * called by fs dependent VFS_MOUNT code to link the VFS base file system
 * dependent behavior with the VFS virtual object.
 */
void
vfs_insertbhv(
        vfs_t *vfsp,
        bhv_desc_t *bdp,
        vfsops_t *vfsops,
        void *mount)
{
        /* initialize vfs behavior desc with ops and data */
        bhv_desc_init(bdp, mount, vfsp, vfsops);
        /* insert this base vfs behavior in the chain */
        bhv_insert_initial(&vfsp->vfs_bh, bdp);
}

