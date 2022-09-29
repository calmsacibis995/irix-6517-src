/* 	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_FS_FS_SUBR_H	/* wrapper symbol for kernel use */
#define _FS_FS_SUBR_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.37 $"

#include <sys/vnode.h>

/*
 * Utilities shared among file system implementations.
 */
struct cred;
struct pollhead;
struct flock;
struct flid;
struct pfdat;
struct strbuf;

extern int	fs_noerr();
extern int	fs_nosys();
extern int	fs_nodev();
extern void	fs_noval();
extern int	fs_cmp(bhv_desc_t *, struct vnode *);
extern int	fs_frlock(bhv_desc_t *, int, struct flock *,
			  int, off_t, vrwlock_t, struct cred *);
extern int	fs_checklock(struct vnode *, int, off_t, off_t, int, 
			   struct cred *, struct flid *, vrwlock_t);
extern void	fs_cleanlock(struct vnode *, struct flid *, struct cred *);
extern int	fs_setfl(bhv_desc_t *, int, int, struct cred *);
extern void	fs_vnode_change(bhv_desc_t *, vchange_t, __psint_t);
extern int	fs_poll(bhv_desc_t *, short, int,
			short *, struct pollhead **, unsigned int *);
extern int	fs_pathconf(bhv_desc_t *, int, long *, struct cred *);
extern int 	fs_cover(bhv_desc_t *, struct mounta *, char *, struct cred *);
extern int 	fs_dounmount(bhv_desc_t *, int, vnode_t *, struct cred *);
extern int 	fs_realvfsops(vfs_t *, struct mounta *, vfsops_t **);

extern void	fs_tosspages(bhv_desc_t *, off_t, off_t, int);
extern void 	fs_flushinval_pages(bhv_desc_t *, off_t, off_t, int);
extern int 	fs_flush_pages(bhv_desc_t *, off_t, off_t, uint64_t, int);
extern void 	fs_invalfree_pages(bhv_desc_t *, off_t);
extern void 	fs_pages_sethole(bhv_desc_t *, struct pfdat*, int, int, off_t);

/*
 * NOTE!	The "stub" routines fs_strgetmsg() & fs_strputmsg()
 *		are located in io/streams/streamio.c in order to
 *		prevent os/fs_subr.c becoming unnecessarily dependent
 *		on streams rollup patches.
 */
extern int	fs_strgetmsg(bhv_desc_t *, struct strbuf *, struct strbuf *,
			     unsigned char *, int *, int, union rval *);
extern int	fs_strputmsg(bhv_desc_t *, struct strbuf *, struct strbuf *,
			     unsigned char, int, int);

#define fs_sync		(int(*)(bhv_desc_t *, int,  struct cred *))fs_noerr
#define fs_import	(int(*)(vfs_t *))fs_noerr
#define fs_rwlock	(vop_rwlock_t)fs_noval
#define fs_rwunlock	(vop_rwunlock_t)fs_noval
#define fs_quotactl	(int(*)(struct bhv_desc *, int, int, caddr_t))fs_noerr
/*
 * Common subroutine of conventional filesystems' VOP_MAP operation.
 */
extern int	fs_map_subr(struct vnode *, off_t, mode_t, off_t, 
			     void *, addr_t, size_t, u_int, u_int,
			     u_int, struct cred *);

extern int	fs_fmtdirent(void *, ino_t, off_t, char *);
extern int	irix5_fs_fmtdirent(void *, ino_t, off_t, char *);
#if _MIPS_SIM != _ABI64
extern int	irix5_n32_fs_fmtdirent(void *, ino_t, off_t, char *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _FS_FS_SUBR_H */
