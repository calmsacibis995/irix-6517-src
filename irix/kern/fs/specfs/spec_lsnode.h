/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.		     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_SPECFS_LSNODE_H	/* wrapper symbol for kernel use */
#define _FS_SPECFS_LSNODE_H	/* subject to change without notice */

#ident	"$Revision: 1.24 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/fs/spec_ops.h>

/*
 * The lsnode represents the local path to a special file in any filesystem.
 *
 * There is one lsnode for each active special file (per vnode).
 *
 * Filesystems that support special files use spec_vp(vp, dev, type, cr)
 * to convert a normal vnode to a special vnode in the ops lookup() and
 * create().
 *
 * To deal with having multiple lsnodes that represent the same underlying
 * device vnode without cache aliasing problems, "ls2cs_handle" is used to
 * lead to the "common" csnode/vnode used for caching data, and accessing
 * the underlying physical device.
 *
 * If an lsnode is created internally by the kernel, then the ls_fsvp
 * field is NULL and the specfs vnode is found via the BHV_TO_VNODE() macro.
 *
 * The other lsnodes that are created as a result of a lookup of a device
 * in a file system have ls_fsvp pointing to the vp that represents the
 * device in the file system, and ls2cs_handle leads to the "common" csnode/
 * vnode for the device.
 */
typedef struct lsnode {
	struct lsnode	*ls_next;	/* hash link - must be first	*/
	uint64_t	ls_flag;	/* flags, must be 2nd, see below*/
	spec_handle_t	ls2cs_handle;	/* common csnode handle		*/
	spec_handle_t	ls_cvp_handle;	/* common device vnode handle	*/
	vnode_t		*ls_fsvp;	/* fs entry vnode (if any)	*/
	bhv_desc_t	ls_bhv_desc;	/* specfs behavior descriptor	*/
	mutex_t		ls_lock;	/* mutual exclusion lock	*/
	short		ls_locktrips;	/* # of lock reacquisitions	*/
	int		ls_opencnt;	/* count of opened references	*/
	dev_t		ls_dev;		/* device the lsnode represents	*/
	dev_t		ls_fsid;	/* file system identifier	*/
	daddr_t		ls_size;	/* blk device size (basic blks)	*/
	time_t		ls_atime;	/* time of last access		*/
	time_t		ls_mtime;	/* time of last modification	*/
	time_t		ls_ctime;	/* time of last attributes chg	*/
	long		ls_gen;		/* commonvp gen number		*/
#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
	union {
	  struct bdevsw	*ls_blk;	/* VBLK - devsw address		*/
	  struct cdevsw	*ls_chr;	/* VBLK - devsw address		*/
	} ls_devsw;
	int		(*ls_poll)(void *, short, int, short *,
						void **, unsigned int *);
	void		*ls_polla0;	/* first arg to ls_poll		*/
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
	spec_ls_ops_t	*ls2cs_ops;	/* "cs" ops vector		*/
#ifdef	SPECFS_DEBUG
	int		ls_line;	/* "line #" of snode creator	*/
	int		ls_cycle;	/* spec_makesnode iteration #	*/
#endif	/* SPECFS_DEBUG */
} lsnode_t;

#define	ls_bdevsw	ls_devsw.ls_blk
#define	ls_cdevsw	ls_devsw.ls_chr

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
 * Convert between behavior and lsnode
 */
#define	VP_TO_LSP(Vp)	((lsnode_t *)(BHV_PDATA(VNODE_TO_FIRST_BHV(Vp))))
#define	BHV_TO_LSP(Bdp)	((lsnode_t *)(BHV_PDATA(Bdp)))
#define	LSP_TO_BHV(Lsp)	(&((Lsp)->ls_bhv_desc))
#define	LSP_TO_VP(Lsp)	BHV_TO_VNODE(LSP_TO_BHV(Lsp))


#ifdef _KERNEL

/*
 * Macros used to transition from the "ls/local" to "cs/common"
 * modules via the spec_ls_ops_t ops vector.
 */
#define	VSPECFS_SUBR_ATTACH(Lsp, Dev, Type, Handle,			\
				Commonvp, Gen, Size, Stream)		\
		((Lsp)->ls2cs_ops->spec_ops_cs_attach((Dev), (Type),	\
						     (Handle),		\
						     (Commonvp), (Gen),	\
						     (Size), (Stream)))
#define	VSPECFS_SUBR_CMP_GEN(Lsp, Handle, Vsp_gen)			\
		((Lsp)->ls2cs_ops->spec_ops_cs_cmp_gen((Handle), Vsp_gen))
#define	VSPECFS_SUBR_GET_GEN(Lsp, Handle)				\
		((Lsp)->ls2cs_ops->spec_ops_cs_get_gen((Handle)))
#define	VSPECFS_SUBR_GET_OPENCNT(Lsp, Handle)				\
		((Lsp)->ls2cs_ops->spec_ops_cs_get_opencnt((Handle)))
#define	VSPECFS_SUBR_GET_SIZE(Lsp, Handle)				\
		((Lsp)->ls2cs_ops->spec_ops_cs_get_size((Handle)))
#define	VSPECFS_SUBR_GET_ISMOUNTED(Lsp, Handle)				\
		((Lsp)->ls2cs_ops->spec_ops_cs_ismounted((Handle)))
#define	VSPECFS_SUBR_MOUNTEDFLAG(Lsp, Handle, Flag)			\
		((Lsp)->ls2cs_ops->spec_ops_cs_mountedflag((Handle), (Flag)))
#define	VSPECFS_SUBR_CLONE(Lsp, Handle, Stdata, Oldvp, Flag, Stream)	\
		((Lsp)->ls2cs_ops->spec_ops_cs_clone((Handle),		\
							(Stdata), (Oldvp), \
							(Flag), (Stream)))
#define	VSPECFS_VOP_BMAP(Lsp, Handle, Off, Cnt, Flag, Cred, Bmap, Nbmap) \
		((Lsp)->ls2cs_ops->spec_ops_bmap((Handle), (Off),	\
						 (Cnt), (Flag), (Cred),	\
						 (Bmap), (Nbmap)))
#define	VSPECFS_VOP_OPEN(Lsp, Handle, Gen, Newdev, Size, Stdata, Flag, Cred) \
		((Lsp)->ls2cs_ops->spec_ops_open((Handle), (Gen),	\
						  (Newdev), (Size),	\
						  (Stdata), (Flag), (Cred)))
#define	VSPECFS_VOP_READ(Lsp, Handle, Gen, Vp, Uiop, Ioflag, Cred, Flid)  \
		((Lsp)->ls2cs_ops->spec_ops_read((Handle), (Gen), (Vp),	\
						  (Uiop), (Ioflag),	\
						  (Cred), (Flid)))
#define	VSPECFS_VOP_WRITE(Lsp, Handle, Gen, Vp, Uiop, Ioflag, Cred, Flid) \
		((Lsp)->ls2cs_ops->spec_ops_write((Handle), (Gen), (Vp),  \
						  (Uiop), (Ioflag),	\
						  (Cred), (Flid)))
#define	VSPECFS_VOP_IOCTL(Lsp, Handle, Gen, Cmd, Arg, Mode, Cred, Rvalp, Vbds) \
		((Lsp)->ls2cs_ops->spec_ops_ioctl((Handle), (Gen),	\
						  (Cmd), (Arg),		\
						  (Mode), (Cred),	\
						  (Rvalp), (Vbds)))
#define	VSPECFS_VOP_STRGETMSG(Lsp, Handle, Mctl, Mdata, Prip, Flagsp,	\
							Fmode, Rvalp)	\
		((Lsp)->ls2cs_ops->spec_ops_strgetmsg((Handle),		\
						  (Mctl), (Mdata),	\
						  (Prip), (Flagsp),	\
						  (Fmode), (Rvalp)))
#define	VSPECFS_VOP_STRPUTMSG(Lsp, Handle, Mctl, Mdata, Pri, Flag, Fmode) \
		((Lsp)->ls2cs_ops->spec_ops_strputmsg((Handle),		\
						  (Mctl), (Mdata),	\
						  (Pri), (Flag), (Fmode)))
#define	VSPECFS_VOP_POLL(Lsp, Handle, Gen, Events, Anyyet, Reventsp,	\
							Phpp, Genp)	\
		((Lsp)->ls2cs_ops->spec_ops_poll((Handle), (Gen),	\
						   (Events), (Anyyet),	\
						   (Reventsp), (Phpp), (Genp)))
#define	VSPECFS_VOP_STRATEGY(Lsp, Handle, Bp)				\
		((Lsp)->ls2cs_ops->spec_ops_strategy((Handle), (Bp)))

#define	VSPECFS_VOP_ADDMAP(Lsp, Handle, Op, Vt, Off, Len, Prot, Cred, Pgno) \
		((Lsp)->ls2cs_ops->spec_ops_addmap((Handle), (Op),	\
						   (Vt), (Off),		\
						   (Len), (Prot),	\
						   (Cred), (Pgno)))
#define	VSPECFS_VOP_DELMAP(Lsp, Handle, Vt, Len, Cred) \
		((Lsp)->ls2cs_ops->spec_ops_delmap((Handle),		\
						   (Vt), (Len),	(Cred)))
#define	VSPECFS_VOP_CLOSE(Lsp, Handle, Flag, LastClose, Cred)		\
		((Lsp)->ls2cs_ops->spec_ops_close((Handle), (Flag),	\
						 (LastClose), (Cred)))
#define	VSPECFS_SUBR_TEARDOWN(Lsp, Handle)				\
		((Lsp)->ls2cs_ops->spec_ops_teardown((Handle)))


/*
 * Lock and unlock lsnodes.
 */
#define SPEC_LSP_LOCK(Lsp)	spec_lock(Lsp)
#define SPEC_LSP_UNLOCK(Lsp)	spec_unlock(Lsp)

void spec_lock(lsnode_t *);
void spec_unlock(lsnode_t *);



/*
 * Function Prototypes
 */
enum vtype;
struct cred;
struct vnode;

/*
 * Construct a spec vnode for a given device that shadows a particular
 * "real" vnode.
 */
extern struct vnode *spec_vp(struct vnode *vp, dev_t, enum vtype, struct cred *);

/*
 * Construct a spec vnode for a given device that shadows nothing.
 */
extern struct vnode *make_specvp(dev_t, enum vtype);

/*
 * Bogus SVR4 reference-count query function exported to generic os.
 */
extern int stillreferenced(dev_t, enum vtype);

/*
 * Flag a device (no longer) containing a mounted filesystem.
 */
extern void spec_mountedflag(struct vnode *, int);

#define spec_mounted(vp)	spec_mountedflag(vp, 1)
#define spec_unmounted(vp)	spec_mountedflag(vp, 0)

/*
 * Return the SMOUNTED flag state
 */
extern int spec_ismounted(struct vnode *);

/*
 * Return the cs_size field
 */
extern daddr_t spec_devsize(struct vnode *);

/*
 * LSnode lookup stuff.
 * These routines maintain a table of lsnodes hashed by dev so
 * that the lsnode for a dev can be found if it already exists.
 * NOTE: DLSPECTBSIZE must be a power of 2 for DLSPECTBHASH to work!
 */

#define	DLSPECTBSIZE		128	/* XXX too small? */
#define	DLSPECTBHASH(dev) ((getemajor(dev) + getminor(dev)) & (DLSPECTBSIZE-1))

extern struct vnodeops spec_vnodeops;


/*
 *  Common ls spec functions
 */
struct cred;
struct stdata;

extern lsnode_t *spec_make_clone(lsnode_t *, dev_t dev, vtype_t type,
				 vnode_t *vp, vnode_t **vpp,
				 struct stdata *stp, int);
extern void spec_free(lsnode_t *);
extern void spec_delete(lsnode_t *);
extern void spec_mark(lsnode_t *, int);

#endif	/* _KERNEL */

#endif	/* _FS_SPECFS_LSNODE_H */
