#ifndef __SYS_EFS_CLNT_H__
#define __SYS_EFS_CLNT_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

/*
 * EFS arguments to the mount system call.
 */
struct efs_args {
	int	flags;		/* flags */
	u_int	lbsize;		/* logical block size in bytes */
};

/*
 * EFS mount option flags
 */
#define	EFSMNT_LBSIZE	0x0001	/* set logical block size */

#endif /* !__SYS_EFS_CLNT_H__ */
