/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 * $Revision: 1.89 $
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986, 1987, 1988, 1989, 1990, 1991  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */
#include "types.h"
#include <sys/buf.h>            /* for B_ASYNC */
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/proc.h>           
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/vfs.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <sys/cred.h>
#include <sys/dnlc.h>
#include "xdr.h"
#define SVCFH
#include "nfs.h"
#include "export.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "bds.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"
#include "string.h"
#include <sys/siginfo.h>
#include <sys/ksignal.h>
#include <sys/syscall.h>
#include <sys/runq.h>
#include <sys/sched.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

#include <sys/ktrace.h>

extern dev_t nfs3_major;

void	fattr3_to_vattr(bhv_desc_t *, struct fattr3 *, struct vattr *);
static void	set_attrcache_time(bhv_desc_t *, struct vattr *,
			int, enum staleflush, int *);
extern int	rfs3call(struct mntinfo *, int, xdrproc_t, caddr_t,
		    xdrproc_t, caddr_t, cred_t *, int *, enum nfsstat3 *);
extern int	nfs3_attrcache(bhv_desc_t *, struct fattr3 *, 
		    enum staleflush, int *);
void		nfs_purge_caches(bhv_desc_t *, cred_t *);

void	nfs_purge_rddir_cache(bhv_desc_t *);
extern int	nfs_async_cleanup (void);
extern long make_local_fsid(u_long, struct mntinfo *, dev_t, 
			    struct minormap *);
extern int nfs3_do_commit(bhv_desc_t *, offset3, count3,
                        cred_t *, struct buf *);
extern struct minormap nfs3_minormap;

#ifdef NFSCACHEDEBUG
int nfs3_cache_inval_panic = 0;


int
nfs3_cache_valid(rnode_t *rp, timespec_t mtime, off_t fsize)
{
	if (MACRO_CACHE_VALID(rp, mtime, fsize)) {
	    return TRUE;
	}
	if (rp->r_attrtime && rp->r_mi->mi_pid == 0) {
		if ((mtime.tv_sec <  rp->r_attr.va_mtime.tv_sec) ||
		    (  mtime.tv_sec == rp->r_attr.va_mtime.tv_sec &&
		       mtime.tv_nsec < rp->r_attr.va_mtime.tv_nsec)) {
			printf("nfs3 attribute cache invalid: mtime before rtime\n");
			printf("	mtime.sec = %x, rtime.sec= %x\n",
					mtime.tv_sec, rp->r_attr.va_mtime.tv_sec);
			printf("	mtime.nsec = %x, rtime.nsec= %x\n",
					mtime.tv_nsec, rp->r_attr.va_mtime.tv_nsec);
			printf("	fsize = %d, rp_size = %d\n",
					fsize, rp->r_attr.va_size);
			if (nfs3_cache_inval_panic && (fsize != rp->r_attr.va_size))
			  debug("invalid nfs3 attribute cache");
		}
	}
	return FALSE;
}
#endif /* NFSCACHEDEBUG */

/*
 * Validate caches by checking cached attributes. If they have timed out
 * get the attributes from the server and compare mtimes. If mtimes are
 * different purge all caches for this vnode.
 */
int
nfs3_validate_caches(bhv_desc_t *bdp, cred_t *cr)
{
	struct vattr va;

	return (nfs3getattr(bdp, &va, 0, cr));
}

/*ARGSUSED*/
void
nfs_purge_caches(bhv_desc_t *bdp, cred_t *cr)
{
	rnode_t *rp;
	char *contents;
	int size;
	vnode_t *vp;

	rp = BHVTOR(bdp);
	vp = BHV_TO_VNODE(bdp);

	/*
	 * Purge the DNLC for any entries which refer to this file.
	 */
	dnlc_purge_vp(vp);

	/*
	 * Clear any readdir state bits and purge the readlink response cache.
	 */
	mutex_enter(&rp->r_statelock);
	/*
	 * Invalidate the attributes.
	 */
	INVAL_ATTRCACHE(bdp);

	/* 
	 * jws - This shouldn't be done, we should just fall
	 * back to readdirs instead of readdirplusses
	 * 
		rp->r_flags &= ~REOF;
	 */
	contents = rp->r_symlink.contents;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	mutex_exit(&rp->r_statelock);

	if (contents != NULL)
		kmem_free(contents, size);

	/*
	 * Flush the readdir response cache.
	 */
	nfs_purge_rddir_cache(bdp);
}

void
nfs_purge_access_cache(bhv_desc_t *bdp)
{
	rnode_t *rp;
	struct access_cache *acp;
	struct access_cache *nacp;

	rp = BHVTOR(bdp);
	if (rp->r_acc != NULL) {
		mutex_enter(&rp->r_statelock);
		acp = rp->r_acc;
		rp->r_acc = NULL;
		mutex_exit(&rp->r_statelock);
		while (acp != NULL) {
			crfree(acp->cred);
			nacp = acp->next;
			kmem_free((caddr_t)acp, sizeof (*acp));
			acp = nacp;
		}
	}
}

void
nfs_purge_rddir_cache(bhv_desc_t *bdp)
{
	rnode_t *rp;
	struct rddir_cache *rdc;
	struct rddir_cache *prdc;

	rp = BHVTOR(bdp);
top:
	mutex_enter(&rp->r_statelock);
	rdc = rp->r_dir;
	prdc = NULL;
	while (rdc != NULL) {
		if ((rdc->flags & RDDIR) != 0) {
			prdc = rdc;
			rdc = rdc->next;
			continue;
		}
		if (prdc == NULL)
			rp->r_dir = rdc->next;
		else
			prdc->next = rdc->next;
		if (rp->r_direof == rdc)
			rp->r_direof = NULL;
		mutex_exit(&rp->r_statelock);
		if (rdc->entries != NULL)
			kmem_free(rdc->entries, rdc->buflen);
		sv_destroy(&rdc->cv);
		kmem_free((caddr_t)rdc, sizeof (*rdc));
		goto top;
	}
	mutex_exit(&rp->r_statelock);
}


void
nfs_cache_check(bhv_desc_t *bdp, timespec_t ctime, timespec_t mtime,
	off_t fsize, cred_t *cr)
{
	rnode_t *rp;

	rp = BHVTOR(bdp);
	mutex_enter(&rp->r_statelock);
	if (!CACHE_VALID(rp, mtime, fsize)) {
		mutex_exit(&rp->r_statelock);
		nfs_purge_caches(bdp, cr);
	} else
		mutex_exit(&rp->r_statelock);

	if (rp->r_attr.va_ctime.tv_sec != ctime.tv_sec ||
	    rp->r_attr.va_ctime.tv_nsec != ctime.tv_nsec) {
		nfs_purge_access_cache(bdp);
	}
}

void
nfs3_cache_check(bhv_desc_t *bdp, struct wcc_attr *wcap, cred_t *cr)
{
	timespec_t ctime;
	timespec_t mtime;
	off_t fsize;

	ctime.tv_sec = wcap->ctime.seconds;
	ctime.tv_nsec = wcap->ctime.nseconds;
	mtime.tv_sec = wcap->mtime.seconds;
	mtime.tv_nsec = wcap->mtime.nseconds;
	fsize = wcap->size;

	nfs_cache_check(bdp, ctime, mtime, fsize, cr);
}

void
nfs3_cache_check_fattr3(bhv_desc_t *bdp, struct fattr3 *na, cred_t *cr)
{
	timespec_t ctime;
	timespec_t mtime;
	off_t fsize;

	ctime.tv_sec = na->ctime.seconds;
	ctime.tv_nsec = na->ctime.nseconds;
	mtime.tv_sec = na->mtime.seconds;
	mtime.tv_nsec = na->mtime.nseconds;
	fsize = na->size;

	nfs_cache_check(bdp, ctime, mtime, fsize, cr);
}

/*
 * Do a cache check based on the post-operation attributes.
 * Then make them the new cached attributes.
 *
 * If we didn't get any attributes, then do nothing.  This is
 * okay because the operations which return post_op_attr's do
 * not modify the object.  All of the modify operations return
 * wcc_data which is handled separately.
 */
#ifdef NFSCACHEDEBUG
int good_post_op_attrs=0;
#endif /* NFSCACHEDEBUG */
void
nfs3_cache_post_op_attr(bhv_desc_t *bdp, post_op_attr *poap, cred_t *cr,
	int *rlock_held)
{
	if (poap->attributes) {
		nfs3_cache_check_fattr3(bdp, &poap->attr, cr);
		if ( nfs3_attrcache(bdp, &poap->attr, SFLUSH, rlock_held)) {
			PURGE_ATTRCACHE(bdp);
#ifdef NFSCACHEDEBUG
			printf("bad cache nfs3_cache_post_op_attr %d\n",
				good_post_op_attrs);
			good_post_op_attrs=0;
		} else {
			good_post_op_attrs++;
#endif /* NFSCACHEDEBUG */
		}
	}
}


static enum nfsftype nf3_to_fa[] = {
    /*   XXX, NF3REG, NF3DIR, NF3BLK, NF3CHR, NF3LNK, NF3SOCK, NF3FIFO */
	NFNON, NFREG, NFDIR,   NFBLK, NFCHR,  NFLNK,  NFSOCK,  NFNON
};

/*
 * Set attributes cache for given vnode using nfsattr.
 */
void
fattr3_to_nfsfattr(struct fattr3 *na3, struct  nfsfattr	*na)
{

	na->na_type = nf3_to_fa[na3->type];
	na->na_mode = na3->mode & MODEMASK;
	na->na_nlink = na3->nlink;
	if (na3->uid == NFS_UID_NOBODY)
	    na->na_uid = UID_NOBODY;
	else
	    na->na_uid = na3->uid;
	if (na3->gid == NFS_GID_NOBODY)
	    na->na_gid = GID_NOBODY;
	else
	    na->na_gid = na3->gid;
	na->na_size = na3->size;
	na->na_rdev = makedevice(na3->rdev.specdata1,na3->rdev.specdata2);
	na->na_fsid = (u_long)na3->fsid;
	na->na_nodeid = (u_long)na3->fileid;
	na->na_atime.tv_sec = na3->atime.seconds;
	na->na_atime.tv_usec = na3->atime.nseconds;
	na->na_mtime.tv_sec = na3->mtime.seconds;
	na->na_mtime.tv_usec = na3->mtime.nseconds;
	na->na_ctime.tv_sec = na3->ctime.seconds;
	na->na_ctime.tv_usec = na3->ctime.nseconds;
}

vattr_mask(struct fattr3 *na)
{
	int	mask = 0;

	if (na->type != -1) mask |= AT_TYPE;
	if (na->mode != -1) mask |= AT_MODE;
	if (na->nlink != -1) mask |= AT_NLINK;
	if (na->uid != -1) mask |= AT_UID;
	if (na->gid != -1) mask |= AT_GID;
	if (na->size != -1) mask |= AT_SIZE;
	if (na->used != -1) mask |= AT_NBLOCKS;
	if (na->rdev.specdata1 != -1) mask |= AT_RDEV;
	if (na->fsid != -1) mask |= AT_RDEV;
	if (na->fileid != -1) mask |= AT_NODEID;
	if (na->atime.seconds != -1) mask |= AT_ATIME;
	if (na->mtime.seconds != -1) mask |= AT_MTIME;
	if (na->ctime.seconds != -1) mask |= AT_CTIME;

	return (mask);
}

int
nfs3_attrcache(bhv_desc_t *bdp, struct fattr3 *na, enum staleflush fflag,
	int *rlock_held)
{
	struct vattr	va;
	int modified;
	struct rnode   *rp = bhvtor(bdp);

	/* XXX temporary to force AT_TYPE */
	va.va_mask = vattr_mask(na);

	modified = na->mtime.seconds != rp->r_attr.va_mtime.tv_sec
		   || na->mtime.nseconds != rp->r_attr.va_mtime.tv_nsec;

	fattr3_to_vattr(bdp, na, &va);

	set_attrcache_time(bdp, &va, modified, fflag, rlock_held);
	return (0);
}

/*
 * Do a cache check based on the weak cache consistency attributes.
 * These consist of a small set of pre-operation attributes and the
 * full set of post-operation attributes.
 *
 * If we are given the pre-operation attributes, then use them to
 * check the validity of the various caches.  Then, if we got the
 * post-operation attributes, make them the new cached attributes.
 * If we didn't get the post-operation attributes, then mark the
 * attribute cache as timed out so that the next reference will
 * cause a GETATTR to the server to refresh with the current
 * attributes.
 *
 * Otherwise, if we didn't get the pre-operation attributes, but
 * we did get the post-operation attributes, then use these
 * attributes to check the validity of the various caches.  This
 * will probably cause a flush of the caches because if the
 * operation succeeded, the attributes of the object were changed
 * in some way from the old post-operation attributes.  This
 * should be okay because it is the safe thing to do.  After
 * checking the data caches, then we make these the new cached
 * attributes.
 *
 * Otherwise, we didn't get either the pre- or post-operation
 * attributes.  Simply mark the attribute cache as timed out so
 * the next reference will cause a GETATTR to the server to
 * refresh with the current attributes.
 */
#ifdef NFSCACHEDEBUG
int good_wcc=0;
int good_wcc1=0;
#endif /* NFSCACHEDEBUG */
void
nfs3_cache_wcc_data(bhv_desc_t *bdp, wcc_data *wccp, cred_t *cr,
	int *rlock_held)
{

	if (wccp->before.attributes) {
		nfs3_cache_check(bdp, &wccp->before.attr, cr);
		if (wccp->after.attributes) {
			if (nfs3_attrcache(bdp, 
				&wccp->after.attr, SFLUSH, rlock_held)) {
				PURGE_ATTRCACHE(bdp);
#ifdef NFSCACHEDEBUG
				printf ("bad cache nfs3_cache_wcc_data %d\n"
					, good_wcc);
				good_wcc=0;
			} else {
				good_wcc++;
#endif /* NFSCACHEDEBUG */
			}
		} else 
			PURGE_ATTRCACHE(bdp);
	} else if (wccp->after.attributes) {
		nfs3_cache_check_fattr3(bdp, &wccp->after.attr, cr);
		if (nfs3_attrcache(bdp, &wccp->after.attr, SFLUSH, rlock_held)) {
			PURGE_ATTRCACHE(bdp);
#ifdef NFSCACHEDEBUG
			printf ("bad cache nfs3_cache_wcc_data2 %d\n"
					, good_wcc1);
		} else {
			good_wcc1++;
#endif /* NFSCACHEDEBUG */
		}
	} else
		PURGE_ATTRCACHE(bdp);
}

/*
 * Set attributes cache for given vnode using vnode attributes.
 */
void
nfs_attrcache_va(bhv_desc_t *bdp, struct vattr *va, enum staleflush fflag,
	int *rlock_held)
{
	int modified;
	rnode_t *rp = bhvtor(bdp);
	vnode_t *vp = BHV_TO_VNODE(bdp);
	/* 
	 * The reference port had the code
	 * 	if (vp->v_flag & VNOCACHE) {
	 * 		return;
	 * 	}
	 * But they didn't try to handle locked mmapped files
	 * very well
	 */
	vp->v_type = va->va_type;
	
	modified = va->va_mtime.tv_sec != rp->r_attr.va_mtime.tv_sec
		   || va->va_mtime.tv_nsec != rp->r_attr.va_mtime.tv_nsec;

	set_attrcache_time(bdp, va, modified, fflag, rlock_held);
}

static void
set_attrcache_time(bhv_desc_t *bdp, struct vattr *va, 
		   int modified, enum staleflush fflag, int *rlock_held)
{
	rnode_t *rp;
	mntinfo_t *mi;
	int delta;
	vnode_t *vp;

	ASSERT(!rlock_held || *rlock_held);
	rp = BHVTOR(bdp);
	vp = BHV_TO_VNODE(bdp);
	mi = VN_BHVTOMI(bdp);
	if (mi->mi_flags & MIF_NOAC) {
		PURGE_ATTRCACHE(bdp);
		mutex_enter(&rp->r_statelock);
	} else {
		mutex_enter(&rp->r_statelock);
		/*
		 * Delta is the number of seconds that we will cache
		 * attributes of the file.  It is based on the number
		 * of seconds since the last time that we detected a
		 * a change (i.e. files that changed recently are
		 * likely to change soon), but there is a minimum and
		 * a maximum for regular files and for directories.
		 *
		 * Using the time since last change was detected
		 * eliminates the direct comparison or calculation
		 * using mixed client and server times.  NFS does
		 * not make any assumptions regarding the client
		 * and server clocks being synchronized.
		 */
		if (va->va_mtime.tv_sec != rp->r_attr.va_mtime.tv_sec) {
			rp->r_mtime = time;
			delta = 0;
		} else
			delta = time - rp->r_mtime;
		if (vp->v_type == VDIR) {
			if (delta < mi->mi_acdirmin) {
				delta = mi->mi_acdirmin;
			} else if (delta > mi->mi_acdirmax) {
				delta = mi->mi_acdirmax;
			}
		} else {
			if (delta < mi->mi_acregmin) {
				delta = mi->mi_acregmin;
			} else if (delta > mi->mi_acregmax) {
				delta = mi->mi_acregmax;
			}
		}
		rp->r_attrtime = time + delta;
	}
	rp->r_attr = *va;

	if (modified) {
		if (fflag == SFLUSH && vp->v_type == VREG) {
			mutex_exit(&rp->r_statelock);
			ASSERT(rlock_held);
			ASSERT(*rlock_held);
			runlock(rp);
			*rlock_held = 0;
			VOP_INVALFREE_PAGES(vp, rp->r_localsize);
			return;
		}
		if (vp->v_type == VDIR) {
			dnlc_purge_vp(vp);
			mutex_exit(&rp->r_statelock);
			nfs_purge_rddir_cache(bdp);
			return;
		}
	}
			
	mutex_exit(&rp->r_statelock);
}

/*
 * Fill in attribute from the cache. If valid return 1 otherwise 0;
 */
static int
nfs_getattr_cache(bhv_desc_t *bdp, struct vattr *vap, int flags)
{
	rnode_t *rp;

	rp = BHVTOR(bdp);
	mutex_enter(&rp->r_statelock);
	if (time < rp->r_attrtime || flags & ATTR_LAZY) {
		/*
		 * Cached attributes are valid
		 */

		long blksize;
		
		*vap = rp->r_attr;
		if ((vn_bhvtomi(bdp)->mi_flags & MIF_BDS) &&
		    (blksize = bds_blksize(rp)))
		{
			vap->va_blksize = blksize;
		}
		mutex_exit(&rp->r_statelock);
		return (1);
	}
	mutex_exit(&rp->r_statelock);
	return (0);
}

vtype_t nf3_to_vt[] = {
	VBAD, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO
};
/*
 * Get attributes over-the-wire and update attributes cache
 * if no error occurred in the over-the-wire operation.
 * Return 0 if successful, otherwise error.
 * If MIF_BDS, call bds_blksize if necessary to get BDS's va_blksize.
 */
int
nfs3_getattr_otw(bhv_desc_t *bdp, struct vattr *vap, cred_t *cr)
{
	int rlock_held;
	int error;
	struct GETATTR3args args;
	struct GETATTR3res res;
	int douprintf;
	rnode_t *rp = BHVTOR(bdp);

	struct fattr3 *na;
	vnode_t *vp;

	args.object = *bhvtofh3(bdp);

	douprintf = 1;
	error = rlock_nohang(rp);
	if (error) {
		return(error);
	}
	error = rfs3call(vn_bhvtomi(bdp), NFSPROC3_GETATTR,
			xdr_GETATTR3args, (caddr_t)&args,
			xdr_GETATTR3res, (caddr_t)&res,
			cr, &douprintf, &res.status);

	if (error) {
		runlock(rp);
		return error;
	}
	
	if (error = geterrno3(res.status)) {
		runlock(rp);
		PURGE_STALE_FH(error, bdp, cr);
		return error;
	}

	na = &res.resok.obj_attributes;
	vp = BHV_TO_VNODE(bdp);
	if ((vp->v_type != VNON) && (nf3_to_vt[na->type] != vp->v_type)) {
		runlock(rp);
                PURGE_STALE_FH(ESTALE, bdp, cr);
                return ESTALE;
        }
		
	fattr3_to_vattr(bdp, &res.resok.obj_attributes, vap);
	nfs_cache_check(bdp, vap->va_ctime, vap->va_mtime, vap->va_size, cr);
	rlock_held = 1;
	nfs_attrcache_va(bdp, vap, SFLUSH, &rlock_held);
	if (rlock_held) {
		runlock(rp);
	}

	if (vn_bhvtomi(bdp)->mi_flags & MIF_BDS) {
		long blksize;
		
		mutex_enter(&rp->r_statelock);
		if (blksize = bds_blksize(rp))
			vap->va_blksize = blksize;
		mutex_exit(&rp->r_statelock);
	}

	return (error);
}


/*
 * Return either cached or remote attributes. If get remote attr
 * use them to check and invalidate caches, then cache the new attributes.
 */
int
nfs3getattr(bhv_desc_t *bdp, struct vattr *vap, int flags, cred_t *cr)
{
	int error;

#ifdef KTRACE
	TRACE1(T_NFS3_GETATTR, T_START, BHV_TO_VNODE(vp));
#endif /* KTRACE  */
	/* SGI's xcstat() doesn't set AT_TYPE */
	vap->va_mask |= AT_TYPE;

	/*
	 * If we've got cached attributes, we're done, otherwise go
	 * to the server to get attributes, which will update the cache
	 * in the process.
	 */
	if (nfs_getattr_cache(bdp, vap, flags)) {
		error = 0;
	} else {
		error = nfs3_getattr_otw(bdp, vap, cr);
	}

	/* Return the client's view of file size */
	vap->va_size = bhvtor(bdp)->r_size;

#ifdef KTRACE
	TRACE2(T_NFS3_GETATTR, error ? T_EXIT_ERROR : T_EXIT_OK,
	       BHV_TO_VNODE(bdp), error);
#endif /* KTRACE  */
	return (error);
}

void
fattr3_to_vattr(bhv_desc_t *bdp, struct fattr3 *na, struct vattr *vap)
{
	rnode_t *rp;
	vnode_t *vp;
	struct mntinfo *mi;

	rp = BHVTOR(bdp);
	vp = BHV_TO_VNODE(bdp);
	mi = rtomi(rp);
	if (na->type < NF3REG || na->type > NF3FIFO)
		vap->va_type = VBAD;
	else
		vap->va_type = nf3_to_vt[na->type];
	vap->va_mode = na->mode;
	vap->va_uid = na->uid == NFS_UID_NOBODY ? UID_NOBODY : (uid_t)na->uid;
	vap->va_gid = na->gid == NFS_GID_NOBODY ? GID_NOBODY : (gid_t)na->gid;
	if (mi->mi_rootfsid == -1) {
		/* we are building a root rnode, we want real base fsid */
		mi->mi_rootfsid = (u_long)na->fsid;
	}
	if ((u_long)na->fsid == mi->mi_rootfsid) 
		vap->va_fsid = vp->v_vfsp->vfs_dev;
	else {
		vap->va_fsid = make_local_fsid((ulong)na->fsid, mi,
						nfs3_major, &nfs3_minormap);
	}
	vap->va_nodeid = na->fileid;
	vap->va_nlink = na->nlink;
	vap->va_size = (off_t)na->size;

	/*
	 * The real criteria for updating r_size should be if the
	 * file has grown on the server or if the client has not
	 * modified the file.
	 */
	if (rp->r_size != (off_t)na->size) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_size < (off_t)na->size ||
		    ((rp->r_flags & RDIRTY) == 0 &&
		     (rp->r_bds_flags & RBDS_DIRTY) == 0 &&
		     rp->r_iocount == 0))
			rp->r_size = (off_t)na->size;
		mutex_exit(&rp->r_statelock);
	}
	vap->va_atime.tv_sec  = na->atime.seconds;
	vap->va_atime.tv_nsec = na->atime.nseconds;
	vap->va_mtime.tv_sec  = na->mtime.seconds;
	vap->va_mtime.tv_nsec = na->mtime.nseconds;
	vap->va_ctime.tv_sec  = na->ctime.seconds;
	vap->va_ctime.tv_nsec = na->ctime.nseconds;

	vap->va_rdev = makedevice(na->rdev.specdata1, na->rdev.specdata2);

	switch (na->type) {
	case NF3BLK:
		vap->va_blksize = DEV_BSIZE;
		vap->va_nblocks = 0;
		break;
	case NF3CHR:
		vap->va_blksize = mi->mi_tsize;
		vap->va_nblocks = 0;
		break;
	case NF3REG:
	case NF3DIR:
		vap->va_blksize = mi->mi_tsize;
		vap->va_nblocks = OFFTOBB(na->size);
		break;
	default:
		vap->va_blksize = mi->mi_tsize;
		vap->va_nblocks = 0;
		break;
	}
	vap->va_mask = vattr_mask(na);
	/*
	 * SGI-added attributes, just zero them.
	 */
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;
}

/*
 * Asynchronous I/O parameters.  nfs_async_threads is the high-water mark
 * for the demand-based allocation of async threads per-mount.  The
 * nfs_async_timeout is the amount of time a thread will live after it
 * becomes idle, unless new I/O requests are received before the thread
 * dies.  See nfs_async_putpage and nfs_async_start.
 */

#define NFS_ASYNC_TIMEOUT       (60 * 10)   /* 10 minute */

int nfs_async_timeout = -1;     /* uninitialized */

int nfs_maxasynccount = 100;    /* max number of async reqs */

static void     nfs_async_start(mntinfo_t *, int);
extern int fork(void *uap, rval_t *rvp);
extern int setsid(void *, rval_t *);

/*ARGSUSED*/
int
thread_create(
	int ignored1,
	int ignored2,
	void (*routine)(mntinfo_t *, int),
	caddr_t arg,
	int arg1,
	pid_t *p0,
	int ignored4,
	int ignored5)
{
	rval_t	rval;
	rval_t	rval2;
	sigvec_t *sigvp;

	fdt_release();

	if (fork(NULL, &rval) == 0) {
		if (rval.r_val1 == 0) {		/* child */
			setsid(NULL, &rval2);
			(*routine)((mntinfo_t *)arg, arg1);
		} else {
			if (p0)
				*p0 = rval.r_val1;
			VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
			sigvp->sv_flags |= SNOWAIT;
			VPROC_PUT_SIGVEC(curvprocp);
		}
		return (rval.r_val1);
	} else {
		return (0);
	}
}

int
async_thread_create(mntinfo_t *mi)
{
	pid_t	p0;

	return (thread_create(NULL, NULL, nfs_async_start,
				(caddr_t) mi, 0, &p0, 0, 60));
}

int   async_bio = 1;

void
nfs_async_bio(bhv_desc_t *bdp, struct buf *bp, cred_t *cr, int (*bio)(bhv_desc_t *, struct buf *, stable_how *, cred_t *))
{
	mntinfo_t *mi;
	struct nfs_async_reqs *args;
#ifdef DEBUG
	rnode_t *rp = BHVTOR(bdp);
#endif
	stable_how stable = UNSTABLE;
	int sleep;

	ASSERT((rp == BHVTOR(bdp)) && (rp->r_iocount > 0));
	mi = VN_BHVTOMI(bdp);
	if (!async_bio) {
	    (void) ((*bio)(bdp, bp, &stable, cr));
	    return;
	}

	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (mi->mi_max_threads == 0) {
	        (void) ((*bio)(bdp, bp, &stable, cr)); 
	        return;
	}

	/*
	 * If we can't allocate a request structure, do the readdir
	 * operation synchronously in this thread's context.
	 *
	 * If this is B_BDFLUSH, the caller does not want to block.
	 * Blocking on kmem_alloc seems the lesser evil relative to
	 * blocking on I/O to a (maybe down) NFS server.
	 */
	sleep = (bp->b_flags & B_BDFLUSH) ? KM_SLEEP : KM_NOSLEEP;
	if ((args = (struct nfs_async_reqs *)
	    kmem_alloc(sizeof (*args), sleep)) == NULL) {
	        (void) ((*bio)(bdp, bp, &stable, cr));
	        return;
	}

	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the
	 * bio synchronously.
	 */
	mutex_enter(&mi->mi_async_lock);
	if (mi->mi_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		kmem_free((caddr_t)args, sizeof (*args));
	        (void) ((*bio)(bdp, bp, &stable, cr)); 
	        return;
	}

	args->a_next = NULL;
	args->a_bdp = bdp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_BIO;
	args->a_nfs_bio = (void (*)(bhv_desc_t *, struct buf *, stable_how *, cred_t *))bio;
	args->a_nfs_bp = bp;

        /*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs == NULL) {
		mi->mi_async_reqs = args;
		mi->mi_async_tail = args;
	} else {
		mi->mi_async_tail->a_next = args;
		mi->mi_async_tail = args;
	}
	sv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return;
}

void
nfs_async_readdir(bhv_desc_t *bdp, struct rddir_cache *rdc, cred_t *cr,
	int (*readdir)(bhv_desc_t*, struct rddir_cache *, cred_t *, u_char),
	u_char abi)
{
	vnode_t *vp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;
	extern int pages_before_pager;	/* XXX */

	vp = BHV_TO_VNODE(bdp);
	mi = VN_BHVTOMI(bdp);

	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (mi->mi_max_threads == 0) {
		(void) ((*readdir)(bdp, rdc, cr, abi));
		return;
	}

	/*
	 * If we can't allocate a request structure, do the readdir
	 * operation synchronously in this thread's context.
	 */
	if ((args = (struct nfs_async_reqs *)
	    kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL) {
		(void) ((*readdir)(bdp, rdc, cr, abi));
		return;
	}

	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the
	 * readdir synchronously.
	 */
	mutex_enter(&mi->mi_async_lock);
	if (mi->mi_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		kmem_free((caddr_t)args, sizeof (*args));
		(void) ((*readdir)(bdp, rdc, cr, abi));
		return;
	}

	args->a_next = NULL;
	VN_HOLD(vp);
	args->a_bdp = bdp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_READDIR;
	args->a_nfs_readdir = readdir;
	args->a_nfs_rdc = rdc;
	args->a_nfs_abi = abi;

        /*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs == NULL) {
		mi->mi_async_reqs = args;
		mi->mi_async_tail = args;
	} else {
		mi->mi_async_tail->a_next = args;
		mi->mi_async_tail = args;
	}
	sv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
}

extern struct syscallsw syscallsw[];
int
nfs_async_cleanup ()
{
	register int s;
	int error = 0;
	rval_t rvp;
	k_sigset_t catch_sigs = {
	    sigmask(SIGINT) /* | sigmask(SIGQUIT) */ |
	    sigmask(SIGKILL) | sigmask(SIGTERM)
	};
	sigvec_t *sigvp;
	proc_t	*curp = curprocp;

	/*
	 * Close parent's files.
	 */
	fdt_closeall();

	/*
	 * Set the ps-visible arg list to something
	 * sensible.
	 */
	bcopy("bio3d", curp->p_psargs, 6);
	bcopy("bio3d", curp->p_comm, 6);

	/*
	 * Set scheduler policy and priority
	 */
	VPROC_SCHED_SETSCHEDULER(curvprocp, 21, SCHED_TS, &rvp.r_val1, &error);
	if (error) {
	     printf("Unable to set bio3d scheduler priority/policy: error=%d\n",
		error);
	}

	/*
	 * set things up so zombies don't lie about
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	sigvp->sv_flags |= SNOWAIT|SNOCLDSTOP;
	
	/*
	 * set up the signals to be caught
	 */
	sigfillset(&sigvp->sv_sigign);
	sigdiffset(&sigvp->sv_sigign, &catch_sigs);
	sigvp->sv_sigcatch = catch_sigs;
	VPROC_PUT_SIGVEC(curvprocp);
	
	s = ut_lock(curuthread);
	sigemptyset(curuthread->ut_sighold);
	curuthread->ut_pproxy->prxy_syscall = &syscallsw[ABI_IRIX5];
	ASSERT(curuthread->ut_polldat == NULL);

	ut_unlock(curuthread, s);
	
	vrelvm();
	return (0);
}

static void
nfs_async_start(mntinfo_t *mi, int arg)
{
	struct nfs_async_reqs *args;
	timespec_t ts, rts;
	stable_how stable = UNSTABLE;
	pid_t	p0;
	int	status = 0;

	/*
	 * Dynamic initialization of nfs_async_timeout to allow nfs to be
	 * built in an implementation independent manner.
	 */
	if (nfs_async_timeout == -1)
		nfs_async_timeout = NFS_ASYNC_TIMEOUT;
	ts.tv_sec = nfs_async_timeout;
	ts.tv_nsec = 0;
	rts = ts;

	mutex_enter(&mi->mi_async_lock);
	status = nfs_async_cleanup();
	if (status) {
	    mutex_exit(&mi->mi_async_lock);
	    exit(CLD_EXITED, 0);
	}
	mutex_exit(&mi->mi_async_lock);
	ASSERT(!curuthread->ut_polldat);

	for (;;) {
		mutex_enter(&mi->mi_async_lock);
		while ((args = mi->mi_async_reqs) == NULL) {

			/*
			 * Wakeup thread waiting to unmount the file
			 * system only if all async threads are inactive.
			 *
			 * If we've timed-out and there's nothing to do,
			 * then get rid of this thread.
			 */
			if (mi->mi_max_threads == 0 ||
			    ((rts.tv_sec <= 0) && (mi->mi_threads > 1) && (arg))) {
				if (--mi->mi_threads == 0)
					sv_signal(&mi->mi_async_cv);
				mutex_exit(&mi->mi_async_lock);
				exit(CLD_EXITED, 0);
				/* NOTREACHED */
			}
			sv_timedwait(&mi->mi_async_reqs_cv, PLTWAIT,
				&mi->mi_async_lock, 0, 0, &ts, &rts);
			mutex_enter(&mi->mi_async_lock);
		}
		if ((!arg) && (mi->mi_threads < mi->mi_max_threads)) {
		    if (thread_create(NULL, NULL, nfs_async_start,
			(caddr_t) mi, arg+1, &p0, 0, 60) == NULL) {
			printf ("no new thread\n");
		    } else {
			mi->mi_threads++;
		    }
		}
		mi->mi_async_reqs = args->a_next;
		mutex_exit(&mi->mi_async_lock);

		/*
		 * Obtain arguments from the async request structure.
		 */
		if (args->a_io == NFS_BIO) {
			(void) ((*args->a_nfs_bio)(args->a_bdp,
					args->a_nfs_bp,
					&stable, args->a_cred));
		} else if (args->a_io == NFS_READDIR) {
			(void) ((*args->a_nfs_readdir)(args->a_bdp,
					args->a_nfs_rdc, args->a_cred,
					args->a_nfs_abi));
		} else if (args->a_io == NFS_COMMIT) {
			(void) nfs3_do_commit( args->a_bdp,
					args->a_nfs_offset,
					args->a_nfs_count,
					args->a_cred,
					args->a_nfs_bufp);
		}

		/*
		 * Now, release the vnode and free the credentials
		 * structure.
		 */
		if ((args->a_io != NFS_BIO) && (args->a_io != NFS_COMMIT))
		    VN_RELE(BHV_TO_VNODE(args->a_bdp));
		crfree(args->a_cred);
		kmem_free((caddr_t)args, sizeof (*args));
	}
}

void
nfs_async_stop(mntinfo_t *mi)
{
	/*
	 * Wait for all outstanding putpage operations to complete.
	 */
	mutex_enter(&mi->mi_async_lock);
	mi->mi_max_threads = 0;
	sv_broadcast(&mi->mi_async_reqs_cv);
	while (mi->mi_threads != 0) {
		sv_wait(&mi->mi_async_cv, PZERO-1, &mi->mi_async_lock, 0);
		mutex_lock(&mi->mi_async_lock, PZERO);
	}
	mutex_exit(&mi->mi_async_lock);
}
