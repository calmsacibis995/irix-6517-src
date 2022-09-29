/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _SYS_PVNODE_H
#define _SYS_PVNODE_H

#ident	"$Revision: 1.30 $"

/*
 * Include file associated with vnode behaviors.
 */
#include <ksys/behavior.h>

/*
 * One global passthru function pointer for all vnode pass thru operations.
 * Modules often will interpose on a few functions, and pass thru many others.
 * This can be easily done by using the passthru ops associated with the 
 * vn_passthrup pointer.
 */
extern vnodeops_t *vn_passthrup;

/*
 * Macro to fetch the next behavior descriptor.
 * Optimized to skip passthru ops.
 */
#define PV_NEXT(cur, next, vop)						\
{       								\
        next = BHV_NEXT(cur);  						\
	ASSERT(next);							\
        while ((((vnodeops_t *)(next)->bd_ops)->vop)==vn_passthru_ ## vop){ \
                next = BHV_NEXT(next);  				\
		ASSERT(next);						\
	} 								\
}

/* 
 * PVOP's.  Operates on a behavior descriptor pointer.  
 * Unlike VOP's, no ops-in-progress synchronization is done.
 */
#define	PVOP_OPEN(bdp, vpp, mode, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_open)(bdp, vpp, mode, cr); \
}
#define	PVOP_CLOSE(bdp,f,c,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_close)(bdp,f,c,cr); \
}
#define PVOP_READ(bdp,uiop,iof,cr,fl,rv) \
{       \
        rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_read)(bdp,uiop,iof,cr,fl); \
}
#define	PVOP_WRITE(bdp,uiop,iof,cr,fl,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_write)(bdp,uiop,iof,cr,fl);\
}
#define	PVOP_IOCTL(bdp,cmd,a,f,cr,rvp,vopbd,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_ioctl)(bdp,cmd,a,f,cr,rvp,vopbd);\
}
#define	PVOP_SETFL(bdp, f, a, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_setfl)(bdp, f, a, cr); \
}
#define	PVOP_GETATTR(bdp, vap, f, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_getattr)(bdp, vap, f, cr);\
}
#define	PVOP_SETATTR(bdp, vap, f, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_setattr)(bdp, vap, f, cr);\
}
#define	PVOP_ACCESS(bdp, mode, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_access)(bdp, mode, cr);\
}
#define	PVOP_LOOKUP(bdp,cp,vpp,pnp,f,rdir,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_lookup)(bdp,cp,vpp,pnp,f,rdir,cr);	\
}
#define	PVOP_CREATE(bdp,p,vap,ex,mode,vpp,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_create)(bdp,p,vap,ex,mode,vpp,cr);	\
}
#define	PVOP_REMOVE(bdp,p,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_remove)(bdp,p,cr); \
}
#define	PVOP_LINK(bdp,fvp,p,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_link)(bdp,fvp,p,cr);	\
}
#define	PVOP_RENAME(bdp,fnm,tdvp,tnm,tpnp,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_rename)(bdp,fnm,tdvp,tnm,tpnp,cr);	\
}
#define	PVOP_MKDIR(bdp,p,vap,vpp,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_mkdir)(bdp,p,vap,vpp,cr); \
}
#define	PVOP_RMDIR(bdp,p,cdir,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_rmdir)(bdp,p,cdir,cr); \
}
#define	PVOP_READDIR(bdp,uiop,cr,eofp,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_readdir)(bdp,uiop,cr,eofp);\
}
#define	PVOP_SYMLINK(pdvp,lnm,vap,tnm,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(pdvp)->bd_ops)->vop_symlink) (pdvp,lnm,vap,tnm,cr);	\
}
#define	PVOP_READLINK(bdp,uiop,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_readlink)(bdp,uiop,cr); \
}
#define	PVOP_FSYNC(bdp,f,cr,b,e,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_fsync)(bdp,f,cr,b,e);	\
}
#define	PVOP_INACTIVE(bdp, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_inactive)(bdp, cr);	\
}
#define	PVOP_FID(bdp, fidpp, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_fid)(bdp, fidpp);	\
}
#define	PVOP_FID2(bdp, fidp, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_fid2)(bdp, fidp);	\
}
#define	PVOP_RWLOCK(bdp,i) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_rwlock)(bdp, i);	\
}
#define	PVOP_RWUNLOCK(bdp,i) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_rwunlock)(bdp, i);	\
}
#define	PVOP_SEEK(bdp, ooff, noffp, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_seek)(bdp, ooff, noffp); \
}
#define	PVOP_CMP(bdp, vp2, equal) \
{	\
	equal = (*((vnodeops_t *)(bdp)->bd_ops)->vop_cmp)(bdp,vp2);	\
}
#define	PVOP_FRLOCK(bdp,cmd,a,f,o,cr,rwl,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_frlock)(bdp,cmd,a,f,o,cr,rwl); \
}
#define	PVOP_REALVP(bdp, vp2, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_realvp)(bdp, vp2);	\
}
#define	PVOP_RECLAIM(bdp, flag, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_reclaim)(bdp, flag);	\
}
#define	PVOP_BMAP(bdp, of, sz, rw, cr, b, n, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_bmap)(bdp,of,sz,rw,cr,b,n);\
}
#define	PVOP_STRATEGY(bdp,bp) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_strategy)(bdp,bp); \
}
#define	PVOP_MAP(bdp,of,sz,p,fl,cr,nvp,rv)	\
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_map)(bdp,of,sz,p,fl,cr,nvp); \
}
#define PVOP_ADDMAP(bdp,op,vt,pg,off,sz,p,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_addmap)(bdp,op,vt,pg,off,sz,p,cr); \
}
#define PVOP_DELMAP(bdp,vt,sz,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_delmap)(bdp,vt,sz,cr); \
}
#define PVOP_POLL(bdp, events, anyyet, reventsp, phpp, genp, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_poll)(bdp, events, anyyet, reventsp, phpp, genp); \
}
#define PVOP_DUMP(bdp,addr,bn,count,rv)	\
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_dump)(bdp,addr,bn,count); \
}
#define PVOP_PATHCONF(bdp, cmd, valp, cr, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_pathconf)(bdp, cmd, valp, cr); \
}
#define PVOP_ALLOCSTORE(bdp,off,len,cr,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_allocstore)(bdp,off,len,cr); \
}
#define PVOP_FCNTL(bdp,cmd,a,f,of,cr,rvp,rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_fcntl)(bdp,cmd,a,f,of,cr,rvp); \
}
#define	PVOP_ATTR_GET(bdp, name, value, valuelenp, flags, cred, rv) \
{	\
	 rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_attr_get)(bdp, name, value, valuelenp, flags, cred); \
}
#define PVOP_ATTR_SET(bdp, name, value, valuelen, flags, cred, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_attr_set)(bdp, name, value, valuelen, flags, cred); \
}
#define PVOP_ATTR_REMOVE(bdp, name, flags, cred, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_attr_remove)(bdp, name, flags, cred); \
}
#define PVOP_ATTR_LIST(bdp, buffer, buflen, flags, cursor, cred, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_attr_list)(bdp, buffer, buflen, flags, cursor, cred); \
}
#define PVOP_COVER(bdp, uap, attrs, vfsops, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_cover)(bdp, uap, attrs, vfsops); \
}
#define PVOP_LINK_REMOVED(bdp, dvp, linkzero) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_link_removed)(bdp, dvp, linkzero); \
}
#define PVOP_VNODE_CHANGE(bdp, cmd, val) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_vnode_change)(bdp, cmd, val); \
}
#define PVOP_TOSS_PAGES(bdp, first, last, fiopt) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_tosspages)(bdp, first, last, fiopt); \
}
#define PVOP_FLUSHINVAL_PAGES(bdp, first, last, fiopt) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_flushinval_pages)(bdp, first, last, fiopt); \
}
#define PVOP_FLUSH_PAGES(bdp, first, last, flags, fiopt, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_flush_pages)(bdp, first, last, flags, fiopt); \
}
#define PVOP_INVALFREE_PAGES(bdp, filesize) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_invalfree_pages)(bdp, filesize); \
}
#define PVOP_PAGES_SETHOLE(bdp, pfd, cnt, doremap, remapoffset) \
{	\
	(void)(*((vnodeops_t *)(bdp)->bd_ops)->vop_pages_sethole)(bdp, pfd, cnt, doremap, remapoffset); \
}
#define PVOP_COMMIT(bdp, bp, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_commit)(bdp, bp); \
}
#define PVOP_READBUF(bdp, offset, count, flags, credp, flid, \
			bp, pboff, pbsize, rv) \
{	\
	rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_readbuf)(bdp,offset,count,\
			flags,credp,flid,bp,pboff,pbsize); \
}
	 
/*
 * Two special PVOP's needed for distributed streams msgs.
 */
#define PVOP_STRGETMSG(bdp,mctl,mdata,prip,flagsp,fmode,rvp,rv)		\
{       								\
        rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_strgetmsg)(bdp,mctl,mdata,prip,flagsp,fmode,rvp); \
}
#define PVOP_STRPUTMSG(bdp,mctl,mdata,pri,flags,fmode,rv)		\
{									\
        rv = (*((vnodeops_t *)(bdp)->bd_ops)->vop_strputmsg)(bdp,mctl,mdata,pri,flags,fmode); \
}

struct strbuf;

extern int vn_passthru_vop_open(bhv_desc_t *, vnode_t **, mode_t, 
				struct cred *);
extern int vn_passthru_vop_close(bhv_desc_t *, int, lastclose_t, 
				 struct cred *);
extern int vn_passthru_vop_read(bhv_desc_t *, struct uio *, int, 
				struct cred *, struct flid *);
extern int vn_passthru_vop_write(bhv_desc_t *, struct uio *, int, 
				 struct cred *, struct flid *);
extern int vn_passthru_vop_ioctl(bhv_desc_t *, int, void *, int, 
				 struct cred *, int *, struct vopbd *);
extern int vn_passthru_vop_setfl(bhv_desc_t *, int, int, struct cred *);
extern int vn_passthru_vop_getattr(bhv_desc_t *, struct vattr *, int, 
				   struct cred *);
extern int vn_passthru_vop_setattr(bhv_desc_t *, struct vattr *, int, 
				   struct cred *);
extern int vn_passthru_vop_access(bhv_desc_t *, int, struct cred *);
extern int vn_passthru_vop_lookup(bhv_desc_t *, char *, vnode_t **, 	
				  struct pathname *, int, vnode_t *, 
				  struct cred *);
extern int vn_passthru_vop_create(bhv_desc_t *, char *, struct vattr *, int, 
				  int, vnode_t **, struct cred *);
extern int vn_passthru_vop_remove(bhv_desc_t *, char *, struct cred *);
extern int vn_passthru_vop_link(bhv_desc_t *, vnode_t *, char *, 
				struct cred *);
extern int vn_passthru_vop_rename(bhv_desc_t *, char *, vnode_t *, char *, 
				  struct pathname *npnp, struct cred *);
extern int vn_passthru_vop_mkdir(bhv_desc_t *, char *, struct vattr *, 
				 vnode_t **, struct cred *);
extern int vn_passthru_vop_rmdir(bhv_desc_t *, char *, vnode_t *, 
				 struct cred *);
extern int vn_passthru_vop_readdir(bhv_desc_t *, struct uio *, struct cred *, 
				   int *);
extern int vn_passthru_vop_symlink(bhv_desc_t *, char *, struct vattr *, 
				   char *, struct cred *);
extern int vn_passthru_vop_readlink(bhv_desc_t *, struct uio *, struct cred *);
extern int vn_passthru_vop_fsync(bhv_desc_t *, int, struct cred *,
				 off_t, off_t);
extern int vn_passthru_vop_inactive(bhv_desc_t *, struct cred *);
extern int vn_passthru_vop_fid(bhv_desc_t *, struct fid **);
extern int vn_passthru_vop_fid2(bhv_desc_t *, struct fid *);
extern void vn_passthru_vop_rwlock(bhv_desc_t *, vrwlock_t);
extern void vn_passthru_vop_rwunlock(bhv_desc_t *, vrwlock_t);
extern int vn_passthru_vop_seek(bhv_desc_t *, off_t, off_t*);
extern int vn_passthru_vop_cmp(bhv_desc_t *, vnode_t *);
extern int vn_passthru_vop_frlock(bhv_desc_t *, int, struct flock *, int, 
				  off_t, vrwlock_t, struct cred *);
extern int vn_passthru_vop_realvp(bhv_desc_t *, vnode_t **);
extern int vn_passthru_vop_bmap(bhv_desc_t *, off_t, ssize_t, int, 
				struct cred *, struct bmapval *, int *);
extern void vn_passthru_vop_strategy(bhv_desc_t *, struct buf *);
extern int vn_passthru_vop_map(bhv_desc_t *, off_t, size_t, 
				mprot_t, u_int, struct cred *, vnode_t **);
extern int vn_passthru_vop_addmap(bhv_desc_t *, vaddmap_t, struct __vhandl_s *,
				pgno_t *, off_t, size_t,
				mprot_t, struct cred *);
extern int vn_passthru_vop_delmap(bhv_desc_t *, struct __vhandl_s *,
				size_t, struct cred *);
extern int vn_passthru_vop_poll(bhv_desc_t *, short, int, short *, 
				struct pollhead **, unsigned int *);
extern int vn_passthru_vop_dump(bhv_desc_t *, caddr_t, daddr_t, u_int);
extern int vn_passthru_vop_pathconf(bhv_desc_t *, int, long *, struct cred *);
extern int vn_passthru_vop_allocstore(bhv_desc_t *, off_t, size_t, 
				      struct cred *);
extern int vn_passthru_vop_fcntl(bhv_desc_t *, int, void *, int, off_t, 
				 struct cred *, union rval *);
extern int vn_passthru_vop_reclaim(bhv_desc_t *, int);
extern int vn_passthru_vop_attr_get(bhv_desc_t *, char *, char *, int *, int, 
				    struct cred *);
extern int vn_passthru_vop_attr_set(bhv_desc_t *, char *, char *, int, int, 
				    struct cred *);
extern int vn_passthru_vop_attr_remove(bhv_desc_t *, char *, int, 
				       struct cred *);
extern int vn_passthru_vop_attr_list(bhv_desc_t *, char *, int, int, 
				     struct attrlist_cursor_kern *, 
				     struct cred *);
extern int vn_passthru_vop_cover(bhv_desc_t *, struct mounta *, char *,
				 struct cred *);
extern void vn_passthru_vop_link_removed(bhv_desc_t *, vnode_t *, int);
extern void vn_passthru_vop_vnode_change(bhv_desc_t *, vchange_t, __psint_t);
extern void vn_passthru_vop_tosspages(bhv_desc_t *, off_t, off_t, int);
extern void vn_passthru_vop_flushinval_pages(bhv_desc_t *, off_t, off_t, int);
extern int  vn_passthru_vop_flush_pages(bhv_desc_t *, off_t, off_t,
					uint64_t, int);
extern void vn_passthru_vop_invalfree_pages(bhv_desc_t *, off_t);
extern void vn_passthru_vop_pages_sethole(bhv_desc_t *, struct pfdat *, int, int, off_t);
extern void vn_passthru_vop_remapf(bhv_desc_t *, off_t, off_t, int);
extern int  vn_passthru_vop_commit(bhv_desc_t *, struct buf *);
extern int  vn_passthru_vop_readbuf(bhv_desc_t *, off_t, ssize_t, int,
					struct cred *, struct flid *,
					struct buf **, int *, int *);
extern int vn_passthru_vop_strgetmsg(bhv_desc_t *,
				struct strbuf *, struct strbuf *,
				unsigned char *, int *, int, union rval *);
extern int vn_passthru_vop_strputmsg(bhv_desc_t *,
				struct strbuf *, struct strbuf *,
				unsigned char, int, int);

#endif /* _SYS_PVNODE_H */
