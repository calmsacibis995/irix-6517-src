/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_MOUNT_H	/* wrapper symbol for kernel use */
#define _FS_MOUNT_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flags bits passed to mount(2).
 */
#define	MS_RDONLY	0x01	/* Read-only */
#define	MS_FSS		0x02	/* Old (4-argument) mount (compatibility) */
#define	MS_DATA		0x04	/* 6-argument mount */
#define	MS_NOSUID	0x10	/* Setuid programs disallowed */
#define MS_REMOUNT	0x20	/* Remount */
#define MS_NOTRUNC	0x40	/* Return ENAMETOOLONG for long filenames */
#define MS_OVERLAY	0x80	/* AutoFS direct mounts */
#define MS_DEFXATTR	0x0100	/* use default attributes */
#define MS_DOXATTR	0x0200	/* tell server to trust us with attributes */

#ifdef _SGI_SOURCE

#define	MS_GRPID	0x8	/* BSD group-ID from directory on create */
#define MS_DMI		0x1000	/* enable DMI interfaces (XFS only) */
#define	MS_NODEV	0x2000	/* disallow opening of device files */
#define	MS_BEFORE	0x4000	/* mount transparently before other mounts */
#define	MS_AFTER	0x8000	/* and after other mounts */

#endif	/* _SGI_SOURCE */

typedef struct mountid {
		unsigned int	val[4];		/* mount id is both words*/
} mountid_t;

#if !defined(_KERNEL)
extern int mount(const char *, const char *, int, ...);
extern int umount(const char *);

extern int getmountid(const char *, mountid_t *);
#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _FS_MOUNT_H */
