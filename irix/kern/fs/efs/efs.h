#ifndef __SYS_FS_EFS_H__
#define __SYS_FS_EFS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.9 $"

/*
 * Include file to include efs definitions
 */

#include <sys/param.h>
#include "efs_ino.h"
#include "efs_fs.h"
#include "efs_sb.h"
#include "efs_dir.h"
#ifdef _KERNEL
#include "efs_inode.h"
#include "efs_bitmap.h"
#endif

#endif /* __SYS_FS_EFS_H__ */
