/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.		     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_SPECFS_ATNODE_H	/* wrapper symbol for kernel use */
#define _FS_SPECFS_ATNODE_H	/* subject to change without notice */

#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <fs/specfs/spec_ops.h>


/*
 * The atnode represents the attributes of a special file.
 *
 * The atnode is created to handle any spec vnode that doesn't
 * have an underlying file system that can handle the vattr_t
 * attributes.
 */
typedef struct atnode {
	uint64_t	at_flag;	/* flags, see below		*/
	bhv_desc_t	at_bhv_desc;	/* specfs attr behavior desc.	*/
	mutex_t		at_lock;	/* mutual exclusion lock	*/
	short		at_locktrips;	/* # of lock reacquisitions	*/
	vnode_t		*at_fsvp;	/* fs entry vnode (if any)	*/
	dev_t		at_dev;		/* device the atnode represents	*/
	dev_t		at_fsid;	/* file system identifier	*/
	daddr_t		at_size;	/* blk device size (basic blks)	*/
	time_t		at_atime;	/* time of last access		*/
	time_t		at_mtime;	/* time of last modification	*/
	time_t		at_ctime;	/* time of last attributes chg	*/
	u_int16_t	at_mode;	/* get/setattr mode bits	*/
	__uint32_t	at_uid;		/* owner's user id		*/
	__uint32_t	at_gid;		/* owner's group id		*/
	u_int16_t	at_projid;	/* owner's project id		*/
} atnode_t;

/*
 * Flags for changing time.
 */
#define SPECFS_ICHGTIME_MOD        0x1	/* modification timestamp	*/
#define SPECFS_ICHGTIME_ACC        0x2	/* access timestamp		*/
#define SPECFS_ICHGTIME_CHG        0x4	/* inode field change timestamp	*/

/*
 * File types (mode field)
 */
#define	IFMT		0170000		/* type of file		*/
#define	IFIFO		0010000		/* named pipe (fifo)	*/
#define	IFCHR		0020000		/* character special	*/
#define	IFDIR		0040000		/* directory		*/
#define	IFBLK		0060000		/* block special	*/
#define	IFREG		0100000		/* regular		*/
#define	IFLNK		0120000		/* symbolic link	*/
#define	IFSOCK		0140000		/* socket		*/
#define	IFMNT		0160000		/* mount point		*/

/*
 * File execution and access modes.
 */
#define	ISUID		04000		/* set user id on execution	    */
#define	ISGID		02000		/* set group id on execution	    */
#define	ISVTX		01000		/* sticky directory		    */
#define	IREAD		0400		/* read, write, execute permissions */
#define	IWRITE		0200
#define	IEXEC		0100

/*
 * Convert between behavior and atnode
 */
#define	BHV_TO_ATP(Bdp)	((atnode_t *)(BHV_PDATA(Bdp)))
#define	ATP_TO_BHV(Atp)	(&((Atp)->at_bhv_desc))
#define	ATP_TO_VP(Atp)	BHV_TO_VNODE(ATP_TO_BHV(Atp))

#ifdef _KERNEL
/*
 * If driver does not have a size routine (e.g. old drivers), the size of the
 * device is assumed to be infinite.
 */
#define UNKNOWN_SIZE 	0x7fffffff

/*
 * Lock and unlock snodes.
 */
#define SPEC_ATP_LOCK(Atp)	spec_at_lock(Atp)
#define SPEC_ATP_UNLOCK(Atp)	spec_at_unlock(Atp)

void spec_at_lock(struct atnode *);
void spec_at_unlock(struct atnode *);

extern struct vnodeops spec_at_vnodeops;


/*
 *  Externalized at spec functions
 */
extern void spec_at_insert_bhv(vnode_t *vp, vnode_t *fsvp);

#endif	/* _KERNEL */

#endif	/* _FS_SPECFS_ATNODE_H */
