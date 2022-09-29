/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1995-1997 Silicon Graphics, Inc.                   *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/statfs.h	10.2"*/
#ident	"$Revision: 3.10 $"

#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sgidefs.h"

/*
 * Structure returned by the statfs() system call.
 */

#if (_MIPS_SIM == _ABIN32) || defined(_KERNEL)
typedef	__int64_t	_statfs_bc_t, _statfs_fc_t;
#else
typedef long		_statfs_bc_t, _statfs_fc_t;
#endif

struct	statfs {
	short		f_fstyp;	/* File system type */
	long		f_bsize;	/* Block size */
	long		f_frsize;	/* Fragment size (if supported) */
	_statfs_bc_t	f_blocks;	/* Total number of blocks on file system */
	_statfs_bc_t	f_bfree;	/* Total number of free blocks */
	_statfs_fc_t	f_files;	/* Total number of file nodes (inodes) */
	_statfs_fc_t	f_ffree;	/* Total number of free file nodes */
	char		f_fname[6];	/* Volume name */
	char		f_fpack[6];	/* Pack name */
};

#ifndef _KERNEL
extern int	statfs(const char *, struct statfs *, int, int);
extern int	fstatfs(int, struct statfs *, int, int);
#endif

#ifdef _KERNEL
struct irix5_statfs {
	short	f_fstyp;	/* File system type */
	app32_long_t f_bsize;	/* Block size */
	app32_long_t f_frsize;	/* Fragment size (if supported) */
	app32_long_t f_blocks;	/* Total number of blocks on file system */
	app32_long_t f_bfree;	/* Total number of free blocks */
	app32_long_t f_files;	/* Total number of file nodes (inodes) */
	app32_long_t f_ffree;	/* Total number of free file nodes */
	char	f_fname[6];	/* Volume name */
	char	f_fpack[6];	/* Pack name */
};
struct irix5_n32_statfs {
	short	f_fstyp;	/* File system type */
	app32_long_t f_bsize;	/* Block size */
	app32_long_t f_frsize;	/* Fragment size (if supported) */
	app32_long_long_t f_blocks; /* Total number of blocks on file system */
	app32_long_long_t f_bfree; /* Total number of free blocks */
	app32_long_long_t f_files; /* Total number of file nodes (inodes) */
	app32_long_long_t f_ffree; /* Total number of free file nodes */
	char	f_fname[6];	/* Volume name */
	char	f_fpack[6];	/* Pack name */
};
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STATFS_H */
