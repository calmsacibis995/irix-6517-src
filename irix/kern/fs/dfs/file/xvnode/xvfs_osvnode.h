/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/vol_init.h>
#include <sys/vnode.h>
#include <sys/vfs.h>

#ifdef AFS_IRIX_ENV
/*
 * Macros to the GLUED SGI-IRIX vnodeops
 */
/*
 *  These macros aren't used in the code.  Anything coming into the
 *  glue comes from a VOP_XXX call.
 */
#define	VOPO_OPEN(vpp, mode, cr) \
	(*((struct xvfs_vnodeops *)((*(vpp))->v_fops))->oops.vop_open)(vpp, mode, cr)
#define	VOPO_CLOSE(vp, f, c, o, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_close)(vp, f, c, o, cr)
#define	VOPO_READ(vp,uiop,iof,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_read)(vp,uiop,iof,cr)
#define	VOPO_WRITE(vp,uiop,iof,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_write)(vp,uiop,iof,cr)
#define	VOPO_IOCTL(vp,cmd,a,f,cr,rvp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_ioctl)(vp,cmd,a,f,cr,rvp)
#define	VOPO_SETFL(vp, f, a, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_setfl)(vp, f, a, cr)
#define	VOPO_GETATTR(vp, vap, f, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_getattr)(vp, vap, f, cr)
#define	VOPO_SETATTR(vp, vap, f, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_setattr)(vp, vap, f, cr)
#define	VOPO_ACCESS(vp, mode, f, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_access)(vp, mode, f, cr)
#define	VOPO_LOOKUP(vp,cp,vpp,pnp,f,rdir,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_lookup)(vp,cp,vpp,pnp,f,rdir,cr)
#define	VOPO_CREATE(dvp,p,vap,ex,mode,vpp,cr) \
	(*((struct xvfs_vnodeops *)((dvp)->v_fops))->oops.vop_create)(dvp,p,vap,ex,mode,vpp,cr)
#define	VOPO_REMOVE(dvp,p,cr) \
	(*((struct xvfs_vnodeops *)((dvp)->v_fops))->oops.vop_remove)(dvp,p,cr)
#define	VOPO_LINK(tdvp,fvp,p,cr) \
	(*((struct xvfs_vnodeops *)((tdvp)->v_fops))->oops.vop_link)(tdvp,fvp,p,cr)
#define	VOPO_RENAME(fvp,fnm,tdvp,tnm,tpnp,cr) \
	(*((struct xvfs_vnodeops *)((fvp)->v_fops))->oops.vop_rename)(fvp,fnm,tdvp,tnm,tpnp,cr)
#define	VOPO_MKDIR(dp,p,vap,vpp,cr) \
	(*((struct xvfs_vnodeops *)((dp)->v_fops))->oops.vop_mkdir)(dp,p,vap,vpp,cr)
#define	VOPO_RMDIR(dp,p,cdir,cr) \
	(*((struct xvfs_vnodeops *)((dp)->v_fops))->oops.vop_rmdir)(dp,p,cdir,cr)
#define	VOPO_READDIR(vp,uiop,cr,eofp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_readdir)(vp,uiop,cr,eofp)
#define	VOPO_SYMLINK(dvp,lnm,vap,tnm,cr) \
	(*((struct xvfs_vnodeops *)((dvp)->v_fops))->oops.vop_symlink) (dvp,lnm,vap,tnm,cr)
#define	VOPO_READLINK(vp, uiop, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_readlink)(vp, uiop, cr)
#define	VOPO_FSYNC(vp,f,cr,stop,start) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_fsync)(vp,f,cr,start,stop)
#define	VOPO_INACTIVE(vp, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_inactive)(vp, cr)
#define	VOPO_FID(vp, fidpp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_fid)(vp, fidpp)
#define	VOPO_RWLOCK(vp,i) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_rwlock)(vp, i)
#define	VOPO_RWUNLOCK(vp,i) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_rwunlock)(vp, i)
#define	VOPO_SEEK(vp, ooff, noffp) \
	(*((struct xvfs_vndoeops *)((vp)->v_fops))->oops.vop_seek)(vp, ooff, noffp)
#define	VOPO_CMP(vp1, vp2) \
	(*((struct xvfs_vnodeops *)((vp1)->v_fops))->oops.vop_cmp)(vp1, vp2)
#define	VOPO_FRLOCK(vp,cmd,a,f,o,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_frlock)(vp,cmd,a,f,o,cr)
#define	VOPO_REALVP(vp1, vp2) \
	(*((struct xvfs_vnodeops *)((vp1)->v_fops))->oops.vop_realvp)(vp1, vp2)
#define	VOPO_BMAP(vp,of,sz,rw,cr,b,n) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_bmap)(vp,of,sz,rw,cr,b,n)
#define	VOPO_STRATEGY(vp, bp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_strategy)(vp, bp)
#define	VOPO_MAP(vp,of,pr,a,sz,p,mp,fl,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_map) (vp,of,pr,a,sz,p,mp,fl,cr)
#define	VOPO_ADDMAP(vp,of,pr,a,sz,p,mp,fl,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_addmap) (vp,of,pr,a,sz,p,mp,fl,cr)
#define	VOPO_DELMAP(vp,of,pr,a,sz,p,mp,fl,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_delmap) (vp,of,pr,a,sz,p,mp,fl,cr)
#define	VOPO_POLL(vp, events, anyyet, reventsp, phpp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_poll)(vp, events, anyyet, reventsp, phpp)
#define	VOPO_DUMP(vp,addr,bn,count) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_dump)(vp,addr,bn,count)
#define	VOPO_PATHCONF(vp, cmd, valp, cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_pathconf)(vp, cmd, valp, cr)
#define VOPO_ALLOCSTORE(vp,off,len,cr) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_allocstore) (vp,off,len,cr)
#define	VOPO_FCNTL(vp,cmd,a,f,of,cr,rvp) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_fcntl)(vp,cmd,a,f,of,cr,rvp)
#define	VOPO_RECLAIM(vp, flag) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_reclaim)(vp, flag)
#define	VOPO_ATTR_GET(vp, name, value, valuelenp, flags, cred) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_attr_get)\
							(vp,name,value,valuelenp,flags,cred)
#define	VOPO_ATTR_SET(vp, name, value, valuelen, flags, cred) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_attr_set)\
							(vp,name,value,valuelen,flags,cred)
#define	VOPO_ATTR_REMOVE(vp, name, flags, cred) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_attr_remove)(vp,name,flags,cred)
#define	VOPO_ATTR_LIST(vp, buffer, buflen, flags, cursor, cred) \
	(*((struct xvfs_vnodeops *)((vp)->v_fops))->oops.vop_attr_list)\
							(vp,buffer,buflen,flags,cursor,cred)

/*
 * Macros to the original SGI IRIX vnodeops
 */
#define VOPN_OPEN(vp, vpp, mode, cr, rv)                       \
{                                                               \
        bhv_desc_t *nbdp;                                       \
        bhv_head_t *bhvh = &(vp)->v_bh;                  \
        nbdp = bhv_lookup_unlocked(VN_BHV_HEAD(*(vpp)), xvfs_ops); \
        BHV_READ_LOCK(bhvh);                       \
        rv = (*((struct xvfs_vnodeops *)BHV_OPS(nbdp))->nops.vop_open) \
                ((vp)->v_fbhv, vpp, mode, cr);                      \
        BHV_READ_UNLOCK(bhvh);                         \
}
#define	VOPN_CLOSE(bdp, f, c, cr, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_close)  \
		(bdp, f, c, cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_READ(bdp,uiop,iof,cr,flid,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv=(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_read)  \
		(bdp,uiop,iof,cr,flid);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_WRITE(bdp,uiop,iof,cr,flid,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_write)  \
		(bdp,uiop,iof,cr,flid);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_IOCTL(bdp,cmd,a,f,cr,rvp,vopbdp,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_ioctl)  \
		(bdp,cmd,a,f,cr,rvp,vopbdp);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_SETFL(bdp, f, a, cr, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_setfl)  \
		(bdp, f, a, cr);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_GETATTR(bdp, vap, f, cr, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_getattr)  \
		(bdp, vap, f, cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_SETATTR(bdp, vap, f, cr, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_setattr)  \
		(bdp, vap, f, cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_ACCESS(bdp, mode, cr, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        			\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_access)  \
		(bdp, mode, cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_LOOKUP(bdp,cp,vpp,pnp,f,rdir,cr,rv) 		\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_lookup)  \
		(bdp,cp,vpp,pnp,f,rdir,cr);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_CREATE(dbdp,p,vap,ex,mode,vpp,cr,rv) 		\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(dbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(dbdp))->nops.vop_create)  \
		(dbdp,p,vap,ex,mode,vpp,cr);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_REMOVE(dbdp,p,cr,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(dbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(dbdp))->nops.vop_remove)  \
		(dbdp,p,cr);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_LINK(tbdp,fvp,p,cr,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(tbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(tbdp))->nops.vop_link)  \
		(tbdp,fvp,p,cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_RENAME(fbdp,fnm,tdvp,tnm,tpnp,cr,rv) 		\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(fbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(fbdp))->nops.vop_rename)  \
		(fbdp,fnm,tdvp,tnm,tpnp,cr);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_MKDIR(dbdp,p,vap,vpp,cr,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(dbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(dbdp))->nops.vop_mkdir)  \
		(dbdp,p,vap,vpp,cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_RMDIR(dbdp,p,cdir,cr,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(dbdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(dbdp))->nops.vop_rmdir)  \
		(dbdp,p,cdir,cr);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_READDIR(bdp,uiop,cr,eofp,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_readdir)  \
		(bdp,uiop,cr,eofp);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_SYMLINK(bdp,lnm,vap,tnm,cr,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_symlink)  \
		 (bdp,lnm,vap,tnm,cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_READLINK(bdp, uiop, cr, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_readlink)  \
		(bdp, uiop, cr);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_FSYNC(bdp,f,cr,stop,start,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_fsync)  \
		(bdp,f,cr,start,stop);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_INACTIVE(bdp, cr, rv) 				\
{	/* vnode not reference-able, so no need to lock chain */ \
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_inactive)  \
		(bdp, cr);				\
}
#define	VOPN_FID(bdp, fidpp, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_fid) \
		(bdp, fidpp);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_FID2(bdp, fidpp, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_fid2)  \
		(bdp, fidp);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_RWLOCK(bdp,i) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_rwlock)(bdp, i); \
	BHV_READ_UNLOCK(bhvh);  			\
	/* "alloc" is done by rwunlock */	\
}
#define	VOPN_RWUNLOCK(bdp,i) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_rwunlock)(bdp, i); \
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_SEEK(bdp, ooff, noffp, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_seek)  \
		(bdp, ooff, noffp);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_CMP(bdp1, vp2, rv) 					\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp1))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp1))->nops.vop_cmp)  \
		(bdp1, vp2);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_FRLOCK(bdp,cmd,a,f,o,rwl,cr,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_frlock)  \
		(bdp,cmd,a,f,o,rwl,cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_REALVP(bdp1, vp2, rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp1))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp1))->nops.vop_realvp)  \
		(bdp1, vp2);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_BMAP(bdp,of,sz,rw,cr,b,n, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_bmap)  \
		(bdp,of,sz,rw,cr,b,n);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
/*
 * The vnode pointer passed to this macro sometimes changes during the
 * execution of the underlying vop_strategy op.  => save the vp in a local var.
 */
#define	VOPN_STRATEGY(bdp, bp) \
{									\
	bhv_desc_t *_mybdp_ = bdp;					\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(_mybdp_))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	(*((struct xvfs_vnodeops *)BHV_OPS(_mybdp_))->nops.vop_strategy)  \
		(_mybdp_, bp);				\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_MAP(bdp,of,sz,p,fl,cr,nvp,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_map)  \
		(bdp,of,sz,p,fl,cr,nvp);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_ADDMAP(bdp,mp,vt,pt,of,sz,p,cr,rv) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_addmap)  \
		(bdp,mp,vt,pt,of,sz,p,cr);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_DELMAP(bdp,vt,sz,cr,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_delmap)  \
		(bdp,vt,sz,cr);		\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_POLL(bdp, events, anyyet, reventsp, phpp, genp, rv) 	\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_poll)  \
		(bdp, events, anyyet, reventsp, phpp,genp);	\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_DUMP(bdp,addr,bn,count,rv) 				\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_UNLOCK(bhvh);  			\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_dump)  \
		(bdp,addr,bn,count);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_PATHCONF(bdp, cmd, valp, cr, rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_pathconf)  \
		(bdp, cmd, valp, cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define VOPN_ALLOCSTORE(bdp,off,len,cr,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_allocstore) \
		(bdp,off,len,cr);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_FCNTL(bdp,cmd,a,f,of,cr,rvp,rv) 			\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_fcntl)  \
		(bdp,cmd,a,f,of,cr,rvp);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_RECLAIM(bdp, flag, rv) 				\
{	/* vnode not reference-able, so no need to lock chain */ \
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_reclaim)  \
		(bdp, flag);				\
}
#define	VOPN_ATTR_GET(bdp, name, value, valuelenp, flags, cred, rv) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_attr_get)  \
		(vp->v_fbhv,name,value,valuelenp,flags,cred);	\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_ATTR_SET(bdp, name, value, valuelen, flags, cred, rv) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_attr_set)  \
		(bdp,name,value,valuelen,flags,cred);	\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_ATTR_REMOVE(bdp, name, flags, cred, rv) 		\
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_attr_remove) \
		(bdp,name,flags,cred);			\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define	VOPN_ATTR_LIST(bdp, buffer, buflen, flags, cursor, cred, rv) \
{								\
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;		\
	BHV_READ_LOCK(bhvh);        		\
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_attr_list) \
		(bdp,buffer,buflen,flags,cursor,cred);	\
	BHV_READ_UNLOCK(bhvh);  			\
}
#define VOPN_LINK_REMOVED(bdp, dvp, linkzero) 				 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);						 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_link_removed) \
		(bdp,dvp,linkzero);					 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_VNODE_CHANGE(bdp, cmd, val) 				 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_vnode_change) \
		(bdp,cmd,val);						 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_TOSS_PAGES(bdp, first, last, fiopt)			 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_tosspages)	 \
		(bdp,first,last,fiopt);				 	 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_FLUSHINVAL_PAGES(bdp, first, last, fiopt)			 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_flushinval_pages) \
		(bdp,first,last,fiopt);				 	 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_FLUSH_PAGES(bdp, first, last, flags, fiopt, rv)		 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_flush_pages) \
		(bdp,first,last,flags,fiopt);				 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_INVALFREE_PAGES(bdp, filesize)				 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_invalfree_pages) \
		(bdp,filesize);						 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_PAGES_SETHOLE(bdp, pfd, cnt, doremap, remap_offset)	 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	(*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_pages_sethole) \
		(bdp,pfd,cnt,doremap,remap_offset);			 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_STRGETMSG(bdp, mctl, mdata, prip, flagsp, fmode, rvp, rv)	 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_strgetmsg) \
		(bdp,mctl,mdata,prip,flagsp,fmode,rvp);			 \
	BHV_READ_UNLOCK(bhvh);  					 \
}
#define VOPN_STRPUTMSG(bdp, mctl, mdata, pri, flag, fmode, rv) 		 \
{									 \
	bhv_head_t *bhvh = &(BHV_TO_VNODE(bdp))->v_bh;			 \
	BHV_READ_LOCK(bhvh);        					 \
	rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->nops.vop_strputmsg) \
		(bdp,mctl,mdata,pri,flag,fmode);			 \
	BHV_READ_UNLOCK(bhvh);  					 \
}

/* This is included to stop all the "implictly declared function" warnings
 * Since this is all within SGIMIPS-specific code, no maintenance issues arise
 * either, until unless OSF defines these functions in a later version
 * elsewhere.
 */
extern int xvfs_convert (struct vnode *);
   
#endif
