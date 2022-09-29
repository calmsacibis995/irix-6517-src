/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-3b2:fs/fstyp.h	1.2"*/
#ident	"$Revision: 3.21 $"

#ifndef _FS_FSTYP_H	/* wrapper symbol for kernel use */
#define _FS_FSTYP_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

#define FSTYPSZ		16	/* max size of fs identifier */

/*
 * Opcodes for the sysfs() system call.
 */
#define GETFSIND	1	/* translate fs identifier to fstype index */
#define GETFSTYP	2	/* translate fstype index to fs identifier */
#define GETNFSTYP	3	/* return the number of fstypes */

#ifndef _KERNEL
int sysfs(int, ...);
#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _FS_FSTYP_H */
