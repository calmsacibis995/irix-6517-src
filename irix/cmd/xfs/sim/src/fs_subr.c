/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/fs_subr.c	1.8"*/
#ident	"$Revision: 1.6 $"

/*
 * Generic vnode operations.
 */
#include <sys/errno.h>
#include <sys/vnode.h>

/*
 * Stub for no-op vnode operations that return error status.
 */
int
fs_noerr()
{
	return 0;
}

/*
 * The associated operation is not supported by the file system.
 */
int
fs_nosys()
{
	return ENOSYS;
}

/*
 * For VOP's, like VOP_MAP, that wish to return ENODEV.
 */
int
fs_nodev()
{
	return ENODEV;
}

/*
 * Stub for inactive, strategy, and read/write lock/unlock.  Does nothing.
 */
/* ARGSUSED */
void
fs_noval(vp)
	vnode_t *vp;
{
}
