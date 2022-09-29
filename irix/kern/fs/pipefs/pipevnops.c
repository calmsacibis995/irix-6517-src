#ident  "$Revision: 1.86 $"

#include <sys/types.h>
#include <sys/ktypes.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/strmp.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/idbgentry.h>
#include <sys/ioctl.h>
#include <sys/mac_label.h>
#include <fs/pipefs/pipenode.h>
#include <fs/pipefs/pipeioctl.h>
#include <fifofs/fifonode.h>


/*
 * Define the routines/data structures used in this file.
 */
int  pipe_read(bhv_desc_t *, struct uio *, int, struct cred *, struct flid *);
int  pipe_write(bhv_desc_t *, struct uio *, int, struct cred *, struct flid *);
int  pipe_ioctl(bhv_desc_t *, int, void *, int, struct cred *, int *, 
		struct vopbd *);
int  pipe_getattr(bhv_desc_t *, struct vattr *, int, struct cred *);
int  pipe_setattr(bhv_desc_t *, struct vattr *, int, struct cred *);
int  pipe_access(bhv_desc_t *, int, struct cred *);
int  pipe_seek(bhv_desc_t *, off_t, off_t *);
int  pipe_pathconf(bhv_desc_t *, int, long *, struct cred *);
int  pipe_close(bhv_desc_t *, int, lastclose_t, struct cred *);
int  pipe_poll(bhv_desc_t *, short, int, short *, struct pollhead **, unsigned int *);
int  pipe_attr_get(bhv_desc_t *,char *,char *,int *, int, struct cred *);
int  pipe_attr_set(bhv_desc_t *,char *,char *,int, int, struct cred *);

int pipe_inactive(bhv_desc_t *, struct cred *);
void pipe_rwlock(bhv_desc_t *bdp, vrwlock_t write_lock);
void pipe_rwunlock(bhv_desc_t *bdp, vrwlock_t write_lock);
static int pipe_peek(bhv_desc_t *, void *, int, int *);
static int pipe_nread(bhv_desc_t *, void *, int, int *);
static int pipe_fionread(bhv_desc_t *, void *, int, int *);
#ifdef DEBUG
static void dumppipe(register pipenode_t *);
#endif

/*
 * Define global data structures used in the pipe specific code.
 */

struct zone     	*pipezone;
static struct vfs       *pipevfsp;
static bhv_desc_t	pipe_bhv;

extern dev_t fifodev;

vfsops_t pipe_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
        fs_nosys,       /* mount */
	fs_nosys,	/* rootinit */
	fs_nosys,	/* mntupdate */
	fs_nosys,	/* dounmount */
        fs_nosys,       /* umount */
        fs_nosys,       /* root */
        fs_nosys,       /* statvfs */
        fs_sync,
        fs_nosys,       /* vget */
        fs_nosys,       /* mountroot */
	fs_noerr,	/* realvfsops */
        fs_import,      /* import */
	fs_quotactl     /* quotactl */
};

vnodeops_t pipe_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
        (vop_open_t)fs_nosys,
        pipe_close,
        pipe_read,
        pipe_write,
	pipe_ioctl,
        fs_setfl,
        pipe_getattr,
        pipe_setattr,
        pipe_access,
        (vop_lookup_t)fs_nosys,
        (vop_create_t)fs_nosys,
        (vop_remove_t)fs_nosys,
        (vop_link_t)fs_nosys,
        (vop_rename_t)fs_nosys,
        (vop_mkdir_t)fs_nosys,
        (vop_rmdir_t)fs_nosys,
        (vop_readdir_t)fs_nosys,
        (vop_symlink_t)fs_nosys,
        (vop_readlink_t)fs_nosys,
        (vop_fsync_t)fs_nosys,
        pipe_inactive,
        (vop_fid_t)fs_nosys,
        (vop_fid2_t)fs_nosys,
        pipe_rwlock,
        pipe_rwunlock,
        pipe_seek,
        fs_cmp,
        (vop_frlock_t)fs_nosys,
        (vop_realvp_t)fs_nosys,
        (vop_bmap_t)fs_nosys,
        (vop_strategy_t)fs_noval,
        (vop_map_t)fs_nodev,
        (vop_addmap_t)fs_nosys,
        (vop_delmap_t)fs_nosys,
        pipe_poll,
        (vop_dump_t)fs_nosys,
        pipe_pathconf,
        (vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	pipe_attr_get,
	pipe_attr_set,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};

/*
 * Irix 4.0.5 Generic pipe system call.
 * Switch to pipefs
 */
/* ARGSUSED */
int
oldpipe(void *uap, rval_t *rvp)
{
	struct vnode *vp;
	struct vfile *rf, *wf;
	struct pipenode *pnp;
	timespec_t tv;
	int fd1, fd2;
	register int error = 0;
	
	/*
	 * Allocate and initialize a vnode and associated pipe node 
	 */

	pnp = kmem_zone_zalloc(pipezone, KM_SLEEP);
	nanotime_syscall(&tv);
	pnp->pi_atime = pnp->pi_mtime = pnp->pi_ctime = tv.tv_sec;
	vp = vn_alloc(pipevfsp, VFIFO, fifodev);
	bhv_desc_init(PTOBHV(pnp), pnp, vp, &pipe_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), PTOBHV(pnp));
	VN_FLAGSET(vp, VINACTIVE_TEARDOWN);

	pnp->pi_size = 0;
	pnp->pi_ino = fifogetid();
	VN_HOLD(vp);

	init_sv(&pnp->pi_empty, SV_DEFAULT, "pipeem", (long)vp->v_number);
	init_sv(&pnp->pi_full, SV_DEFAULT, "pipefu", (long)vp->v_number);
	init_sema(&pnp->pi_frcnt, 1, "piperc", (long)vp->v_number);
	init_sema(&pnp->pi_fwcnt, 1, "pipewc", (long)vp->v_number);
	init_mutex(&pnp->pi_lock, MUTEX_DEFAULT, "pipelk", (long)vp->v_number);
	initpollhead(&pnp->pi_rpq);
	initpollhead(&pnp->pi_wpq);

	/*
	 * Allocate and initialize two file table entries and two
	 * file pointers. Each file pointer is open for read and write.
	 */

	if (error = vfile_alloc(FREAD, &rf, &fd1)) 
		goto out;
	if (error = vfile_alloc(FWRITE, &wf, &fd2)) 
		goto out2;

	vfile_ready(rf, vp);
	vfile_ready(wf, vp);

	/*
	 * Return the file descriptors to the user. 
	 */
	rvp->r_val1 = fd1;
	rvp->r_val2 = fd2;
	return (0);
out2:
	vfile_alloc_undo(fd1, rf);
out:
	psema(&pnp->pi_frcnt, PZERO);
	psema(&pnp->pi_fwcnt, PZERO);
	VN_RELE(vp);
	VN_RELE(vp);
	return (error);
}


/*
 * Save file system type/index, initialize vfs operations vector, get
 * unique device number for PIPEFS.
 * Create and initialize a "generic" vfs pointer that will be placed
 * in the v_vfsp field of each pipes vnode.
 */
/* ARGSUSED */
int
pipe_init(
	register struct vfssw *vswp,
	int fstype)
{

	if ((pipevfsp = vfs_allocate_dummy(fstype, fifodev)) == NULL)
		return -1;
	pipezone = kmem_zone_init(sizeof(struct pipenode), "PIPE nodes");

	vfs_insertbhv(pipevfsp, &pipe_bhv, &pipe_vfsops, &pipe_bhv);

#ifdef DEBUG
        idbg_addfunc("pipefs", dumppipe);
#endif
	return 0;
}


/*
 * Close a pipe.  Update counts and cleanup.
 */
/* ARGSUSED */
int
pipe_close(
	bhv_desc_t *bdp,
	int flag,
	lastclose_t lastclose,
	struct cred *crp)
{
	register struct pipenode *pnp = BHVTOP(bdp);

        if (!pnp) {
		ASSERT(0);
                return 0;
	}

        /*
         * If a file still has the pipe/FIFO open, return.
         */
        if (lastclose == L_FALSE)
		return(0);

	mutex_lock(&pnp->pi_lock, PZERO);
        /*
         * Wake up any sleeping readers/writers.
         */
        if (flag & FREAD) {
		psema(&pnp->pi_frcnt, PZERO);
		/* if no more readers, wake up all writers stuck on full */
		
		sv_broadcast(&pnp->pi_full);
		pollwakeup(&pnp->pi_wpq, POLLOUT);
	}

	if (flag & FWRITE) {
		psema(&pnp->pi_fwcnt, PZERO);
		 /* if no more writers, wake up all readers stuck on empty */
		sv_broadcast(&pnp->pi_empty);
		pnp->pi_flags |= PIPE_WCLOSED;
		pollwakeup(&pnp->pi_rpq, (POLLIN | POLLRDNORM));
	}

	/* if pipe has no readers/writers then close it down */
	
	if ((valusema(&pnp->pi_frcnt) == 0)
	 && (valusema(&pnp->pi_fwcnt) == 0)) {
		if (pnp->pi_bp) {
			kern_free(pnp->pi_bp);
			pnp->pi_bp = NULL;
		}
		pnp->pi_frptr = 0;
		pnp->pi_fwptr = 0;
		pnp->pi_size = 0;
	}
	mutex_unlock(&pnp->pi_lock);
	return(0);
}	

/* 
 * Read from a pipe.
 */
/* ARGSUSED */
int 
pipe_read(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *fcrp,
	struct flid *fl)
{
	register struct pipenode *pnp = BHVTOP(bdp);
	register int error = 0;
	timespec_t tv;
	register unsigned n;

	if (uiop->uio_resid == 0)
		return (0);

	if (!(ioflag & IO_ISLOCKED))
		pipe_rwlock(bdp, VRWLOCK_READ);
	
	/* 
	 * Wait for data to arrive in the pipe from a writer
	 */
	while (pnp->pi_size == 0) {
		/*
		 * Make sure there is at least one writer.  Otherwise, return
		 *  end of file.  If the user won't allow a delay, return
		 * immediately.
		 */
		if (valusema(&pnp->pi_fwcnt) == 0) {
			error = 0;
			goto out;
		}
		if (uiop->uio_fmode & FNDELAY) {
			error = 0;
			goto out;
		}
		if (uiop->uio_fmode & FNONBLOCK) {
			error = EAGAIN;
			goto out;
		}

		ASSERT(mutex_mine(&pnp->pi_lock));
		error = sv_wait_sig(&pnp->pi_empty, PPIPE, &pnp->pi_lock, 0);
		pipe_rwlock(bdp, VRWLOCK_WRITE);
		if (error == -1) {
			error = EINTR;
			goto out;
		}
	}

	if (pnp->pi_bp == NULL)
		pnp->pi_bp = kern_malloc(PIPE_BUF);

	do {
		/* 
		 * Bound users i/o request against (1) the amount of data
                 * actually in the pipe, and (2) the amount we can read
                 * before wrapping around.
                 */
		n = uiop->uio_resid;
		if (n > pnp->pi_size)
                        n = pnp->pi_size;
                if (n > PIPE_BUF - pnp->pi_frptr)
                        n = PIPE_BUF - pnp->pi_frptr;
                if (n) {
                        error = uiomove(pnp->pi_bp + pnp->pi_frptr, 
					(int)n, UIO_READ, uiop);
                        pnp->pi_size -= n;
                }

		/*
		 * Wrap the read pointer around, if its past the
                 * end of the buffer.
                 */

		pnp->pi_frptr += n;
		if (pnp->pi_frptr >= PIPE_BUF)
                        pnp->pi_frptr = 0;
	} while (error == 0 && uiop->uio_resid > 0 && n );

	nanotime_syscall(&tv);
	pnp->pi_atime = tv.tv_sec;

	if (pnp->pi_size) {
                /* things still in pipe - wake up anyone waiting cause empty */
                sv_signal(&pnp->pi_empty);
                pollwakeup(&pnp->pi_rpq, (POLLIN | POLLRDNORM));
        } else
                ASSERT(pnp->pi_frptr == pnp->pi_fwptr);

	/* pipe not full anymore - wake up anyone waiting cause full */
        sv_signal(&pnp->pi_full);
	pollwakeup(&pnp->pi_wpq, POLLOUT);

 out:
	if (!(ioflag & IO_ISLOCKED))
		pipe_rwunlock(bdp, VRWLOCK_READ);
	
	return(error);
}


/*
 * pipe_write:
 *      - called to write a pipe from the user
 */
/* ARGSUSED */
int
pipe_write(
        bhv_desc_t *bdp,
        struct uio *uiop,
        int ioflag,
        struct cred *fcrp,
	struct flid *fl)
{
	register struct pipenode *pnp = BHVTOP(bdp);
	register int error = 0;
	timespec_t tv;
	unsigned int usave;
	unsigned int n;
	int didio = 0;

	uiop->uio_offset = 0;
	
	if (!(ioflag & IO_ISLOCKED))
		pipe_rwlock(bdp, VRWLOCK_WRITE);
	
	/*
         * Apparently, we should zap even zero-length writes with SIGPIPE.
         */
        if (valusema(&pnp->pi_frcnt) == 0) {
		uiop->uio_sigpipe = 1;
		error = EPIPE;
		goto out;
	}

floop:
	/*
         * Check and see if the users write request will fit in the pipe
         * in one piece.  If not, wait until there is room.  If the request
         * is too large to ever fit in the pipe (> PIPE_BUF) then break
         * the request in to smaller pieces.
         */
        usave = 0;
	while ((uiop->uio_resid + pnp->pi_size)  > PIPE_BUF) {

		/* if the pipe is broken, do not continue */
                if (valusema(&pnp->pi_frcnt) == 0) {
			uiop->uio_sigpipe = 1;
			error = EPIPE;
			goto out;
                }

                /*
                 * If users i/o request is larger than amount we can buffer
                 * in the pipe and there is more room in the pipe, truncate
                 * the write request.  If we loop around before a reader reads,
                 * then we will hang up below and wait for the reader to read.
                 */

		if ((uiop->uio_resid > PIPE_BUF) && (pnp->pi_size < PIPE_BUF)) {
                        usave = uiop->uio_resid;
			uiop->uio_resid = PIPE_BUF - pnp->pi_size;
			usave -= uiop->uio_resid;
			break;
		}
		if (uiop->uio_fmode & FNDELAY) {
			error = 0;
			goto out;
		}
		if (uiop->uio_fmode & FNONBLOCK) {
			/*
			 * if we have written to the pipe, the
			 * return val is that number of bytes.
			 */
			if (didio == 0)
				error = EAGAIN;
			else
				error = 0;
			goto out;
		}
		error = sv_wait_sig(&pnp->pi_full, PPIPE, &pnp->pi_lock, 0);
		pipe_rwlock(bdp, VRWLOCK_WRITE);
		if (error == -1) {
                        error = EINTR;
			goto out;
		}
	}
	if (pnp->pi_bp == NULL)
		pnp->pi_bp = kern_malloc(PIPE_BUF);

	while (error == 0 && uiop->uio_resid > 0) {
		/*
                 * Bound users i/o request against (1) the amount of room
                 * left in the pipe, and (2) the amount we can write
                 * before wrapping around.
                 */
		n = uiop->uio_resid;
		if (n > PIPE_BUF - pnp->pi_fwptr)
                        n = PIPE_BUF - pnp->pi_fwptr;
                if (n) {
#ifdef  DEBUG
                        if (pnp->pi_fwptr + n > PIPE_BUF) {
                                printf("vp=%x i_size=%d n=%d wptr=%d\n",
				       PTOV(pnp), pnp->pi_size, n, pnp->pi_fwptr);
                        }
#endif
			error = uiomove(pnp->pi_bp + pnp->pi_fwptr, (int)n, 
							UIO_WRITE, uiop);
			/*
			 * Update pointers iff the data transfer succeeded.
			 * Solves bug 230823.
			 */
			if (!error) {
				pnp->pi_size += n;
				didio = 1;
                		pnp->pi_fwptr += n;
			}
		}

		/*
                 * Wrap the write pointer back to the beginning
                 */
                if (pnp->pi_fwptr >= PIPE_BUF)
                        pnp->pi_fwptr = 0;
		
        }
	
	if (error == 0) {
		nanotime_syscall(&tv);
                        pnp->pi_mtime = pnp->pi_ctime = tv.tv_sec;
	}

	
        /* things now in pipe - wake up anyone waiting cause empty */
        sv_signal(&pnp->pi_empty);
        pollwakeup(&pnp->pi_rpq, (POLLIN | POLLRDNORM));

        /* if pipe not full - wake up anyone waiting cause full */
        if (pnp->pi_size < PIPE_BUF) {
                sv_signal(&pnp->pi_full);
                pollwakeup(&pnp->pi_wpq, POLLOUT);
        }

	/*
         * Continue writing in case user is reading a large amount
         */
	if (error == 0 && (usave != 0)) {
		uiop->uio_resid = usave;
		goto floop;
	}

 out:
	if (!(ioflag & IO_ISLOCKED))
		pipe_rwunlock(bdp, VRWLOCK_WRITE);
	
	return(error);
}

/* ARGSUSED */
int
pipe_ioctl(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	int mode,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	int error;

	switch (cmd) {
	case I_PPEEK:
		error = pipe_peek(bdp, arg, mode, rvalp);
		break;
	case FIONREAD:
		error = pipe_fionread(bdp, arg, mode, rvalp);
		break;
	case I_NREAD:
		error = pipe_nread(bdp, arg, mode, rvalp);
		break;
	default:
		error = ENOSYS;
		break;
	}
	return (error);
}

/*
 * pipe poll operation.
 */
int
pipe_poll(
	bhv_desc_t *bdp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	register short revents = events;
	struct pipenode *pnp = BHVTOP(bdp);
	register int pwclosed = pnp->pi_flags & PIPE_WCLOSED;

	/*
	 * Note that in both the read and write cases below we need to snapshot
	 * the pollhead generation number *before* checking for the requested
	 * events since we do all of this without holding any kind of lock.
	 */
	revents &= POLLIN | POLLRDNORM | POLLOUT;
	if (revents & (POLLIN | POLLRDNORM)) {
		*genp = POLLGEN(&pnp->pi_rpq);
		if (pnp->pi_size == 0) {
			revents &= ~(POLLIN | POLLRDNORM);
			if (!anyyet)
				*phpp = &pnp->pi_rpq;
		}
	}
	if (revents & POLLOUT) {
		*genp = POLLGEN(&pnp->pi_wpq);
		if (pnp->pi_size == PIPE_BUF) {
			revents &= ~POLLOUT;
			if (!anyyet)
				*phpp = &pnp->pi_wpq;
		}
	}
	if (valusema(&pnp->pi_frcnt) == 0 || pwclosed) {
		/*
	 	 * If ip is an anonymous pipe and its last writer has
	 	 * closed, satisfy readability selects.
		 */
		if (pwclosed)
			revents |= (POLLIN | POLLRDNORM);
		revents |= POLLHUP;
	}
	*reventsp = revents;

	return 0;
}

/*
 * Called when the upper level no longer holds references to the vnode.
 */
/* ARGSUSED */
int
pipe_inactive(
	bhv_desc_t *bdp,
	struct cred *crp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register struct pipenode *pnp = BHVTOP(bdp);

	if (!pnp) {
		ASSERT(0);
		return VN_INACTIVE_NOCACHE;
	}

	ASSERT(vp->v_count == 0);
	ASSERT(BHV_OPS(bdp) == &pipe_vnodeops);

	ASSERT(valusema(&pnp->pi_frcnt) == 0);
	ASSERT(valusema(&pnp->pi_fwcnt) == 0);
	ASSERT(sv_waitq(&pnp->pi_empty) == 0);
	ASSERT(sv_waitq(&pnp->pi_full) == 0);
	fifoclearid(pnp->pi_ino);
	sv_destroy(&pnp->pi_empty);
	sv_destroy(&pnp->pi_full);
	freesema(&pnp->pi_frcnt);
	freesema(&pnp->pi_fwcnt);
	mutex_destroy(&pnp->pi_lock);
	destroypollhead(&pnp->pi_rpq);
	destroypollhead(&pnp->pi_wpq);

	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	kmem_zone_free(pipezone, pnp);

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

/*
 * Obtain the node information from the credentials structure.
 */
/* ARGSUSED */
int
pipe_getattr(
	bhv_desc_t *bdp,
	struct vattr *vap,
	int flags,
	struct cred *crp)
{
	register struct pipenode *pnp = BHVTOP(bdp);

	vap->va_mode = 0;
	vap->va_atime.tv_sec = pnp->pi_atime;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_sec = pnp->pi_mtime;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_sec = pnp->pi_ctime;
	vap->va_ctime.tv_nsec = 0;
	vap->va_uid = crp->cr_uid;
	vap->va_gid = crp->cr_gid;
	vap->va_nlink = 0;
	vap->va_fsid = fifodev;
	vap->va_nodeid = pnp->pi_ino;
	vap->va_rdev = 0;

	vap->va_type = VFIFO;
	vap->va_blksize = PIPE_BUF;
	vap->va_size = pnp->pi_size;
	vap->va_nblocks = 0;
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;
	return 0;
}

/*
 * set the time and return 0.
 */
/* ARGSUSED */
int
pipe_setattr(
	bhv_desc_t *bdp,
	register struct vattr *vap,
	int flags,
	struct cred *crp)
{
	register struct pipenode *pnp = BHVTOP(bdp);
	timespec_t tv;

	mutex_lock(&pnp->pi_lock, PINOD);
	if (vap->va_mask & AT_ATIME)
		pnp->pi_atime = vap->va_atime.tv_sec;
	if (vap->va_mask & AT_MTIME) {
		pnp->pi_mtime = vap->va_mtime.tv_sec;
		nanotime_syscall(&tv);
		pnp->pi_ctime = tv.tv_sec;
	}
	mutex_unlock(&pnp->pi_lock);
	return 0;
}

/*
 * allow all access
 */
/* ARGSUSED */
int 
pipe_access(
	bhv_desc_t *bdp,
	int mode,
	struct cred *crp)
{
	return(0);
}


/*
 * Return error since seeks are not allowed on pipes.
 */
/* ARGSUSED */
int
pipe_seek(
        bhv_desc_t *bdp,
        off_t ooff,
        off_t *noffp)
{
        return (ESPIPE);
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
pipe_pathconf(
	bhv_desc_t *bdp,
	int cmd,
	long *valp,
	struct cred *cr)
{
	long val;
	register int error;

	error = 0;
	
	switch (cmd) {
	case _PC_PIPE_BUF:
		val = PIPE_BUF;
		break;
	case _PC_ASYNC_IO:
		val = 1;
                break;
	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		*valp = val;
	return error;
}

int
pipe_attr_get(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	int error;

	error = _MAC_PIPE_ATTR_GET(bdp, name, value, valuelenp, flags, cred);
#ifdef	DEBUG
	if (error)
		cmn_err(CE_NOTE, "pipe_attr_get error=%d", error);
#endif	/* DEBUG */

	return error;
}

int
pipe_attr_set(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int valuelen,
	int flags,
	struct cred *cred)
{
	int error;

	error = _MAC_PIPE_ATTR_SET(bdp, name, value, valuelen, flags, cred);
#ifdef	DEBUG
	if (error)
		cmn_err(CE_NOTE, "pipe_attr_set error=%d", error);
#endif	/* DEBUG */

	return error;
}
/*
 * Lock a read/write pipenode given the vnode.
 */
/* ARGSUSED */
void
pipe_rwlock(
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	struct pipenode *pnp = BHVTOP(bdp);

	mutex_lock(&pnp->pi_lock, PZERO);
	return;
}

/*
 *  Unlock a read/write pipenode given the vnode.
 */
/* ARGSUSED */
void
pipe_rwunlock(
	bhv_desc_t *bdp,
	vrwlock_t write_lock)
{
	struct pipenode *pnp = BHVTOP(bdp);

	ASSERT(mutex_mine(&pnp->pi_lock));
	mutex_unlock(&pnp->pi_lock);
	return;
}

static int
pipe_peek(
	bhv_desc_t *bdp,
	void *arg,
	int mode,
	int *rvalp)
{
	struct pipenode *pnp = BHVTOP(bdp);
	pipepeek_t peek;
	long size;
	short frptr;
	short offset;
	unsigned n;
	int error;

	if ((mode & FREAD) == 0)
		return (EBADF);

	if (copyin(arg, &peek, sizeof(peek)))
		return (EFAULT);

	pipe_rwlock(bdp, VRWLOCK_READ);

	size = pnp->pi_size;
	frptr = pnp->pi_frptr;
	/*
	 * Make user peek it all at once
	 */
	if (peek.pp_size < size) {
		pipe_rwlock(bdp, VRWLOCK_READ);
		return (EINVAL);
	}

	offset = 0;

	do {
		/*
		 * Bound users i/o request against (1) the amount of data
		 * actually in the pipe, and (2) the amount we can read
		 * before wrapping around.
		 */
		n = peek.pp_size;
		if (n > size)
			n = size;
		if (n > PIPE_BUF - frptr)
			n = PIPE_BUF - frptr;
		if (n) {
			error = copyout(pnp->pi_bp + frptr,
					peek.pp_buf + offset,
					(int)n);
			size -= n;
			offset += n;
		}

		/*
		 * Wrap the read pointer around, if its past the
		 * end of the buffer.
		 */

		frptr += n;
		if (frptr >= PIPE_BUF)
			frptr = 0;
	} while (error == 0 && n);

	pipe_rwunlock(bdp, VRWLOCK_READ);

	if (error == 0)
		*rvalp = offset;

	return (error);
}

static int
pipe_nread_copyout(size_t size, caddr_t arg, int force_32)
{
	irix5_size_t i5_size;

	if (force_32 || !(ABI_IS_IRIX5_64(get_current_abi()))) {
		/* truncate 64 bit size to 32 bit */
		i5_size = (int)size;
		if (copyout((caddr_t)&i5_size, arg, sizeof i5_size)) {
			return EFAULT;
		}
	} else {
		if (copyout((caddr_t)&size, arg, sizeof size)) {
			return EFAULT;
		}
	}
	return 0;
}

static int
pipe_nread(bhv_desc_t *bdp, void *arg, int mode, int *rvalp)
{
	struct pipenode *pnp = BHVTOP(bdp);
	size_t size;
	int error = 0;

	if ((mode & FREAD) == 0) {
		return (EBADF);
	}

	pipe_rwlock(bdp, VRWLOCK_READ);

	size = pnp->pi_size;

	error = pipe_nread_copyout(size, arg, 1);
	if (error == 0) {
		/*
		 * Pipes are not message-based, so just claim there's one if
		 * there is any data on the pipe.
		 */
		*rvalp = size ? 1 : 0;
	}

	pipe_rwunlock(bdp, VRWLOCK_READ);

	return (error);
}

/* ARGSUSED */
static int
pipe_fionread(bhv_desc_t *bdp, void *arg, int mode, int *rvalp)
{
	struct pipenode *pnp = BHVTOP(bdp);
	int error = 0;

	if ((mode & FREAD) == 0) {
		return (EBADF);
	}

	pipe_rwlock(bdp, VRWLOCK_READ);

	error = pipe_nread_copyout((size_t)pnp->pi_size, arg, 0);

	pipe_rwunlock(bdp, VRWLOCK_READ);

	return (error);
}

#ifdef DEBUG
static void
dumppipe(register pipenode_t *pnp)
{
	qprintf("pipe node: addr 0x%x\n", pnp);

	qprintf(
	"pi_vnode 0x%x pi_frptr 0x%x pi_fwptr 0x%x pi_size 0x%x pi_bp 0x%x\n",
			PTOV(pnp), pnp->pi_frptr, pnp->pi_fwptr,
			pnp->pi_size, pnp->pi_bp);

	qprintf(
	"*lock 0x%x (0x%x) &empty 0x%x (0x%x) &full 0x%x (0x%x) \n",
			&pnp->pi_lock, mutex_owner(&pnp->pi_lock),
			&pnp->pi_empty, sv_waitq(&pnp->pi_empty),
			&pnp->pi_full, sv_waitq(&pnp->pi_full));
	qprintf(
	"&wcnt 0x%x (%d) &rcnt 0x%x (%d)\n",
			&pnp->pi_fwcnt, valusema(&pnp->pi_fwcnt),
			&pnp->pi_frcnt, valusema(&pnp->pi_frcnt));
	qprintf(
	"flags %s\n",
			pnp->pi_flags & PIPE_WCLOSED ? "PIPE_WCLOSED " : "");
	qprintf(
	"atime %d mtime %d ctime %d\n",
			pnp->pi_atime, pnp->pi_mtime, pnp->pi_ctime);
	qprintf(
	"&pi_rpq 0x%x &pi_wpq 0x%x\n",
			&pnp->pi_rpq, &pnp->pi_wpq);
}
#endif  /* DEBUG */
