#ifndef __SYS_EFS_BITMAP_H__
#define __SYS_EFS_BITMAP_H__

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

#ident "$Revision: 4.7 $"

struct efs;
extern int	efs_bitfunc(struct efs *, int, int, int);

#define EFS_TESTFREE		1
#define EFS_TESTALLOC		2
#define EFS_BITFIELDFREE	3
#define EFS_BITFIELDALLOC	4
#define EFS_BITFIELDSYNCFREE	5

/*
 * Return count of number of contigous set (free) bits (max len bits)
 * in bitmap for fs starting at b.
 */

#define efs_tstfree(fs, b, len)	efs_bitfunc(fs, b, len, EFS_TESTFREE)

/*
 * Return count of number of contigous clear (allocated) bits
 * (max len bits) in bitmap for fs starting at b.
 */

#define	efs_tstalloc(fs, b, len) efs_bitfunc(fs, b, len, EFS_TESTALLOC)

/*
 * Set (mark free) a bit field of length len in bitmap for fs starting at b.
 */

#define	efs_bitfree(fs, b, len)	efs_bitfunc(fs, b, len, EFS_BITFIELDFREE)

/*
 * as above but do the write synchronously 
 */

#define	efs_bitfreesync(fs,b,len) efs_bitfunc(fs,b,len,EFS_BITFIELDSYNCFREE)

/*
 * Clear (mark allocated) a bit field of length len in bitmap for fs
 * starting at b.
 */

#define	efs_bitalloc(fs, b, len) efs_bitfunc(fs, b, len, EFS_BITFIELDALLOC)

#endif /* __SYS_EFS_BITMAP_H__ */
