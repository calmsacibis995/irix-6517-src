/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_FS_CELL_MOUNT_IMPORT_H_
#define	_FS_CELL_MOUNT_IMPORT_H_

#ident	"$Id: mount_import.h,v 1.1 1997/08/26 18:21:20 dnoveck Exp $"

/*
 * fs/cell/mount_import.h -- Mount import structure
 *
 * This header file defines the mount import structure which is used
 * to describe mounted file systems for the purpose of importing them
 * to other cells.  This may happen:
 *
 *      As part of a collective mount operation in an HPC SSI system
 *      Lazily as part of lookup of the covered vnode in an HPC SSI system
 *      As part of cxfs client mountoperations in an Enterprise system
 *
 * The internals of this data structure should only be known to cfs.
 * This header file is in fs/cell (rather than fs/cfs) and thus visible
 * to cxfs-v1 code because cxfs code such as idl stubs needs to declare
 * mount_import structures (as opposed to pointers thereto) and thus 
 * needs to know the size of the data structures.
 *
 * This header should only be included by cfs and cxfs-v1 code.
 */
 
#ifndef CELL
#error included by non-CELL configuration
#endif

#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <ksys/cell.h>
#include <ksys/cell/tkm.h>
#include "fs/cell/fsc_types.h"
#include "fs/cfs/cfs.h"

struct mount_import {
	cfs_handle_t	vfs_hdl;
	tk_set_t	vfs_tkset;
	int		vfs_flags;
	dev_t		vfs_dev;
	u_int		vfs_bsize;
	fsid_t		vfs_fsid;
	fsid_t		*vfs_altfsid;		/* XXX huy */
	cell_t		vfs_cell;
        int             vfs_fstype;
        size_t          vfs_eisize;
        char            vfs_expinfo[VFS_EILIMIT];
	
	cfs_handle_t	rvp_hdl;
	tk_set_t	rvp_tkset;
	vtype_t		rvp_vtype;
	dev_t		rvp_rdev;
	int		rvp_flags;
};

#endif /* _FS_CELL_MOUNT_IMPORT_H_ */
