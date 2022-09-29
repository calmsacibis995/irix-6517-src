/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_XFS_CELL_V1_DCXVFS_H_
#define	_XFS_CELL_V1_DCXVFS_H_

#ident	"$Id: dcxvfs.h,v 1.3 1997/09/12 17:41:46 lord Exp $"

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_mount.h>
#include <ksys/cell/tkm.h>
#include "dcxvn.h"

struct dcxvfs {
	dcvfs_t		*dcxv_dcvfs;	/* Associated CFS structure */
	vfs_t		*dcxv_vfs;	/* vfs structure - just in case */
	xfs_mount_t	*dcxv_mp;	/* Copy of superblock */
};

#endif /* _XFS_CELL_V1_DCXVFS_H_ */
