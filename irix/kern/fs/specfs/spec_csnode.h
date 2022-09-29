/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.		     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_SPECFS_CSNODE_H	/* wrapper symbol for kernel use */
#define _FS_SPECFS_CSNODE_H	/* subject to change without notice */

#ident	"$Revision: 1.17 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <fs/specfs/spec_ops.h>


/*
 * The csnode represents a special file in any filesystem.
 * There is exactly one csnode for each active special file.
 * Filesystems, and others use an "local" lsnode to lead to
 * the "common" snode via the ls2cs_handle field.
 */
typedef struct csnode {
	struct csnode	*cs_next;	/* hash link - must be first	*/
	uint64_t	cs_flag;	/* flags, must be 2nd, see below*/
	bhv_desc_t	cs_bhv_desc;	/* specfs behavior descriptor	*/
	mutex_t		cs_lock;	/* mutual exclusion lock	*/
	short		cs_locktrips;	/* # of lock reacquisitions	*/
	sv_t		cs_close_sync;	/* synchronizing variable	*/
	uint64_t	cs_close_flag;	/* close sync flags		*/
	int		cs_opencnt;	/* count of opened references	*/
	dev_t		cs_dev;		/* device the csnode represents	*/
	dev_t		cs_fsid;	/* file system identifier	*/
	daddr_t		cs_nextr;	/* next byte offset (read-ahead)*/
	daddr_t		cs_size;	/* blk device size (basic blks)	*/
	time_t		cs_atime;	/* time of last access		*/
	time_t		cs_mtime;	/* time of last modification	*/
	time_t		cs_ctime;	/* time of last attributes chg	*/
	u_int16_t	cs_mode;	/* get/setattr mode bits	*/
	__uint32_t	cs_uid;		/* owner's user id		*/
	__uint32_t	cs_gid;		/* owner's group id		*/
	u_int16_t	cs_projid;	/* owner's project id		*/
	long		cs_mapcnt;	/* count of mappings of pages	*/
	long		cs_gen;		/* commonvp gen number		*/
	union {
	  struct bdevsw	*cs_blk;	/* VBLK - devsw address		*/
	  struct cdevsw	*cs_chr;	/* VBLK - devsw address		*/
	} cs_devsw;
	int		(*cs_poll)(void *, short, int, short *,
						void **, unsigned int *);
	void		*cs_polla0;	/* first arg to cs_poll		*/
} csnode_t;

#define	cs_bdevsw	cs_devsw.cs_blk
#define	cs_cdevsw	cs_devsw.cs_chr

/*
 * flags
 */
#define	SUPD		0x0001		/* update access time		*/
#define	SACC		0x0002		/* update modification time	*/
#define	SCHG		0x0004		/* update change time		*/
#define	SMOUNTED	0x0008		/* block device is mounted	*/
#define	SWANTCLOSE	0x0010		/* device successfully opened	*/
#define	SCOMMON		0x0020		/* this snode is the "common"	*/
#define	SPASS		0x0040		/* "pass-thru" mode		*/
#define	SLINKREMOVED	0x0080		/* vop_link_removed() flag	*/
#define	SINACTIVE	0x0100		/* snode/vnode is "inactive"	*/
#define	SCLOSING	0x0200		/* snode/vnode is "mid-close"	*/
#define	SWAITING	0x0400		/* Waiting on SCLOSING		*/
#define	SATTR		0x0800		/* "attr" layer exists below	*/
#define	SSTREAM		0x1000		/* VCHR that's a "stream"	*/

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
 * Convert between behavior and csnode
 */
#define	VP_TO_CSP(Vp)	((csnode_t *)(BHV_PDATA(VNODE_TO_FIRST_BHV(Vp))))
#define	BHV_TO_CSP(Bdp)	((csnode_t *)(BHV_PDATA(Bdp)))
#define	CSP_TO_BHV(Csp)	(&((Csp)->cs_bhv_desc))
#define	CSP_TO_VP(Csp)	BHV_TO_VNODE(CSP_TO_BHV(Csp))

/*
 * Convert between a "handle" and a csnode address
 */
#ifdef	CELL_IRIX
# define HANDLE_TO_CSP(hndl)	((csnode_t *)(hndl)->sh_obj.h_objid)
#else	/* ! CELL_IRIX */
# define HANDLE_TO_CSP(hndl)	((csnode_t *)(hndl)->objid)
#endif	/* ! CELL_IRIX */


#ifdef _KERNEL
/*
 * If driver does not have a size routine (e.g. old drivers), the size of the
 * device is assumed to be infinite.
 */
#define UNKNOWN_SIZE 	0x7fffffff

/*
 * Lock and unlock snodes.
 */
#define SPEC_CSP_LOCK(Csp)	spec_cs_lock(Csp)
#define SPEC_CSP_UNLOCK(Csp)	spec_cs_unlock(Csp)

void spec_cs_lock(struct csnode *);
void spec_cs_unlock(struct csnode *);

/*
 * CSnode lookup stuff.
 * These routines maintain a table of csnodes hashed by dev so
 * that the csnode for a dev can be found if it already exists.
 * NOTE: DCSPECTBSIZE must be a power of 2 for DCSPECTBHASH to work!
 */

#define	DCSPECTBSIZE		128	/* XXX too small? */
#define	DCSPECTBHASH(dev) ((getemajor(dev) + getminor(dev)) & (DCSPECTBSIZE-1))

extern struct vnodeops spec_cs_vnodeops;



#ifdef	JTK_DEBUG
/*
 * Convert a device vnode pointer into a common device vnode pointer.
 */
extern int spec_vp_is_common(struct vnode *);
#endif	/* JTK_DEBUG */

/*
 *  Common cs spec functions
 */
extern csnode_t *spec_cs_get(dev_t dev, vtype_t type);
extern void spec_cs_free(struct csnode *);
extern void spec_cs_delete(struct csnode *);
extern void spec_cs_mark(struct csnode *, int);

extern int spec_cs_device_close(csnode_t *, int, struct cred *);

extern int spec_cs_device_hold(csnode_t *);
extern void spec_cs_device_rele(csnode_t *);

#endif	/* _KERNEL */

#endif	/* _FS_SPECFS_CSNODE_H */
