/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __FS_XFS_XFS_CXFS_H__
#define __FS_XFS_XFS_CXFS_H__

#ident "$Revision: 1.2 $"

/*
 * xfs_cxfs.h -- Interface cxfs presents to non-cell xfs code
 *
 * This header specifies the interface that cxfs V1 code presents to the 
 * non-cellular parts of xfs.  When the specfied routines are not present,
 * stubs will be provided.
 */

struct xfs_mount;
struct xfs_args;

/*
 * Array mount routines.  Stubs provided in non-CELL and in CELL_IRIX
 * cases.
 */
extern void cxfs_arrinit(void); /* Initialization for array mount logic. */
extern int cxfs_mount(	        /* For any specia mount handling. */
		struct xfs_mount    *mp,
                struct xfs_args     *ap,
		dev_t		    dev,
		int	            *client);
extern void cxfs_unmount(       /* For any special unmount handling. */
		struct xfs_mount    *mp);

#endif /* __FS_XFS_XFS_CXFS_H__ */
