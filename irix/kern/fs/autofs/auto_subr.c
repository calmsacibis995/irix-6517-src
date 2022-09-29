/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#include <stdarg.h>
#include <sys/param.h>
#include <string.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/ksignal.h>
#include <sys/systm.h>
#include <sys/ktrace.h>
#include <ksys/vproc.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/xdr.h>
#include <limits.h>

#include <sys/fs/lofs_info.h>
#include <sys/fs/autofs.h>
#include <sys/fs/autofs_prot.h>

extern vnodeops_t autofs_vnodeops;
extern struct sockaddr_in autofs_addr;
extern int autofs_logging;

#ifdef	AUTODEBUG
extern int autodebug;
#endif

#define MUTEX_HELD(M)      mutex_mine(M)

/*
 * Unmount_list is a list of device ids that need to be
 * unmounted. Each corresponds to a mounted filesystem.
 * A mount hierarchy may have mounts underneath it, but
 * this list could be empty if any one of them is busy.
 * But we do keep a count of underlying mounts. This is
 * because if the daemon is able to unmount everything
 * but the kernel times out, then we will have a directory
 * hierarchy that will need to be removed. So when this
 * count is zero we know that we can safely remove the
 * hierarchy
 */
static umntrequest *unmount_list;
static int mnts_in_hierarchy;

kmutex_t autonode_list_lock;
struct autonode *autonode_list;
kmutex_t autonode_count_lock;
int anode_cnt, freeautonode_count;


bool_t	xdr_mntrequest(XDR *, mntrequest *);
bool_t	xdr_mntres(XDR *, mntres *);
bool_t	xdr_umntrequest(XDR *, umntrequest *);
bool_t	xdr_umntres(XDR *, umntres *);

static int	autofs_busy(autonode_t *);
static int	send_unmount_request(autonode_t *);
static int	rm_autonode(autonode_t *);
static int	rm_hierarchy(autonode_t *);
static void	unmount_hierarchy(autonode_t *);
static void	make_unmount_list(autonode_t *);
static int	get_hierarchical_mounts(vfs_t *);

/*
 * Make a new autofs node.
 */
autonode_t *
makeautonode(
	vtype_t type,
	vfs_t *vfsp,
	struct autoinfo *aip,
	struct cred *cred)
{
	autonode_t *ap;
	vnode_t *vp;
	static ino_t nodeid = 3;

	ap = (autonode_t *) kmem_zalloc(sizeof (*ap), KM_SLEEP);
	vp = antovn(ap);

	ap->an_uid	= cred->cr_uid;
	ap->an_gid	= cred->cr_gid;
	ap->an_size	= 2;	/* for . and .. dir entries */
	ap->an_count	= 1;
	ap->an_ainfo	= aip;
	vp->v_count	= 1;
	vp->v_type	= type;
	vp->v_rdev	= 0;
	vp->v_vfsp	= vfsp;

	/* Initialize the first behavior and the behavior chain head. */
	vn_bhv_head_init(VN_BHV_HEAD(vp), "vnode");
        bhv_desc_init(antobhv(ap), ap, vp, &autofs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), antobhv(ap));
	VN_FLAGSET(vp, VINACTIVE_TEARDOWN);

#ifdef VNODE_TRACING
	vp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, 0);
#endif
	mutex_enter(&autonode_count_lock);
	anode_cnt++;
	ap->an_nodeid = nodeid++;
	mutex_exit(&autonode_count_lock);
	rw_init(&ap->an_rwlock, "autonode rwlock", RW_DEFAULT, NULL);
	cv_init(&ap->an_cv_mount, "autofs mount cv", CV_DEFAULT, NULL);
	cv_init(&ap->an_cv_umount, "autofs umount cv", CV_DEFAULT, NULL);
	mutex_init(&ap->an_lock, MUTEX_DEFAULT, "autonode lock");

	return (ap);
}

void
freeautonode(autonode_t *anp)
{
	vnode_t *vp = antovn(anp);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "freeautonode: 0x%lx, vp 0x%lx, ref %d\n", 
		    anp, vp, anp->an_count);
#endif
	vn_bhv_remove(VN_BHV_HEAD(vp), antobhv(anp));
	vn_bhv_head_destroy(VN_BHV_HEAD(vp));
	vp->v_type = VNON;	/* 
				 * clear the type so others who are
				 * using this will know.
				 */

	mutex_enter(&autonode_count_lock);
	freeautonode_count++;
	anode_cnt--;
	mutex_exit(&autonode_count_lock);
	rw_destroy(&anp->an_rwlock);
	cv_destroy(&anp->an_cv_mount);
	cv_destroy(&anp->an_cv_umount);
	mutex_destroy(&anp->an_lock);
	kmem_free((caddr_t) anp, sizeof (*anp));
}

int
do_mount(
	bhv_desc_t *bdp,
	char *name,
	struct cred *cr)
{
	autonode_t *anp = bhvtoan(bdp);
	struct autoinfo *aip = antoai(anp);
	CLIENT *clnt;
	enum clnt_stat status;
	struct timeval timeout;
	int retrans = 5;	/*XXX forever? */
	mntrequest request;
	mntres result;
	int error = 0;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "do_mount: path=%s name=%s\n", 
		aip->ai_path, name);
#endif

	ASSERT(MUTEX_HELD(&anp->an_lock));
	if (anp->an_mntflags & MF_INPROG) {	/* mount in progress */
		int ret;
		k_sigset_t set;
		k_sigset_t set2 = {
			~(sigmask(SIGHUP)|sigmask(SIGINT)|
			sigmask(SIGQUIT)|sigmask(SIGKILL)|
			sigmask(SIGTERM)|sigmask(SIGTSTP))};
		anp->an_mntflags |= MF_WAITING_MOUNT;
		/*
		 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
		 * and SIGTERM. (Preserving the existing masks).
		 */
		assign_cursighold(&set, &set2);

		if (sv_wait_sig(&anp->an_cv_mount, PZERO, 
				&anp->an_lock, 0) < 0)
			ret = EINTR;
		else if (anp->an_mntflags & MF_MNTPNT) {
			/*
			 * Direct mount for which we're waiting and just
			 * finished. Return the same error.
			 */
			ret = anp->an_error;
		} else {
			/*
			 * Indirect mount, we are waiting for the same
			 * mount.  Do the lookup again to determine if
			 * our mount has already taken place; if not it will
			 * end up calling this routine again.
			 */
			ret = EAGAIN;
		}

		/*
		 * restore original signal mask
		 */
		assign_cursighold(0, &set);
		mutex_enter(&anp->an_lock);
		return (ret);
	}
	anp->an_mntflags |= MF_INPROG;
	mutex_exit(&anp->an_lock);

	clnt = clntkudp_create(&autofs_addr, AUTOFS_PROG, AUTOFS_VERS, 
		retrans, KUDP_INTR, KUDP_XID_PERCALL, cr);
	if (clnt == NULL) {
		cmn_err(CE_WARN, 
			"autofs: clntkudp_create failed for mount\n");
		error = EIO;
		goto errout;
	}

	timeout.tv_sec = aip->ai_rpc_to;
	timeout.tv_usec = 0;

	if (aip->ai_direct)
		request.name = aip->ai_path;
	else
		request.name	= name;

	request.map	= aip->ai_map;
	request.opts	= aip->ai_opts;
	request.path	= aip->ai_path;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "do_mount: calling daemon\n");
#endif
	/* XXX make sure daemon is running */
	status = clnt_call(clnt, AUTOFS_MOUNT,
		(xdrproc_t) xdr_mntrequest, (caddr_t) &request,
		(xdrproc_t) xdr_mntres, (caddr_t) &result, timeout);

	switch (status) {
	case RPC_SUCCESS:
		error = result.status;
		if (autofs_logging) {
			if (result.status != 0) {
                		cmn_err(CE_NOTE,
                		"![autofs]: a request to mount directory '%s' failed, requestors uid was %d, and the pid was %d\n",
                		request.name, cr->cr_ruid, current_pid());
        		}
        	}
		break;
	case RPC_INTR:
		printf("autofs: mount request interrupted\n");
		error = EINTR;
		break;
	case RPC_TIMEDOUT:
		/*
		 * Shouldn't get here since we're
		 * supposed to retry forever (almost).
		 */
		printf("autofs: mount request for %s timed out\n", anp->an_name);
		error = ETIMEDOUT;
		break;
	default:
		printf("autofs: %s\n", clnt_sperrno(status));
		error = ENOENT;
		break;
	}

	clnt_destroy(clnt);		/* drop the client handle */

errout:
	if (error != EINTR) {
		anp->an_error = error;
	}
	
	anp->an_ref_time = time;
	mutex_enter(&anp->an_lock);
	if (anp->an_mntflags & MF_WAITING_MOUNT)
		cv_broadcast(&anp->an_cv_mount);
	anp->an_mntflags &= ~(MF_INPROG | MF_WAITING_MOUNT);
	if (!error) {
		anp->an_mntflags |= MF_MOUNTED;
	}

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "do_mount: return error=%d\n", error);
#endif
	return (error);
}

/* ARGSUSED */
int
autodir_lookup(
	bhv_desc_t *dbdp,
	char *nm,
	bhv_desc_t **bdpp,
	struct cred *cred)
{
	autonode_t *ap, *dap;
	vnode_t *dvp;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "autodir_lookup: %s\n", nm);
#endif
	dvp = BHV_TO_VNODE(dbdp);
	*bdpp = NULL;
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	dap = bhvtoan(dbdp);

	ASSERT(ismrlocked(&dap->an_rwlock,MR_UPDATE|MR_ACCESS));
	for (ap = dap->an_dirents; ap; ap = ap->an_next) {
		if (strcmp(ap->an_name, nm) == 0) {
			ap->an_ref_time = time;
			*bdpp = antobhv(ap);
			VN_HOLD(BHV_TO_VNODE(*bdpp));
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "autodir_lookup HOLD 0x%lx, count %d\n", 
		    BHV_TO_VNODE(*bdpp), (BHV_TO_VNODE(*bdpp))->v_count);
#endif
			return (0);
		}
	}
	return (ENOENT);
}


int
auto_direnter(
	autonode_t *dap,
	autonode_t *ap)
{
	autonode_t *cap, **spp;
	u_short offset = 0;
	u_short diff;
	vnode_t *dvp = antovn(dap);

#ifdef AUTODEBUG
	auto_dprint(autodebug, 1, "auto_direnter: %s\n", ap->an_name);
#endif
	rw_enter(&dap->an_rwlock, RW_WRITER);

	/* 
	 * Check if it is a directory.
	 */
	if (dvp->v_type != VDIR) {
		rw_exit(&dap->an_rwlock);
		return (ENOTDIR);
	}

	/* 
	 * Don't make directory entries in an autonode which is marked as
	 * having an error.
	 */
	if (dap->an_error != 0) {
		rw_exit(&dap->an_rwlock);
                return dap->an_error;
        }

	cap = dap->an_dirents;
	if (cap == NULL) {
		/*
		 * offset = 0 for ., 1 for .. & 2 for the new dir
		 */
		spp = &dap->an_dirents;
		offset = 2;
	}

	/*
	 * even after the slot is determined, this
	 * loop must go on to ensure that the name
	 * does not exist
	 */
	for (; cap; cap = cap->an_next) {
		if (strcmp(cap->an_name, ap->an_name) == 0) {
			rw_exit(&dap->an_rwlock);
			return (EEXIST);
		}
		if (cap->an_next != NULL) {
			diff = cap->an_next->an_offset - cap->an_offset;
			ASSERT(diff != 0);
			if ((diff > 1) && (offset == 0)) {
				offset = cap->an_offset + 1;
				spp = &cap->an_next;
			}
		} else if (offset == 0) {
			offset = cap->an_offset + 1;
			spp = &cap->an_next;
			}
	}
	ap->an_offset	= offset;
	ap->an_next	= *spp;
	*spp		= ap;
	mutex_enter(&dap->an_lock);
	dap->an_size++;
#ifdef AUTODEBUG
	auto_dprint(autodebug, 1, "bump size 0x%lx, %d\n", 
		    (antovn(dap)), dap->an_size);
#endif
	mutex_exit(&dap->an_lock);

	rw_exit(&dap->an_rwlock);
	return (0);
}

static int
send_unmount_request(
	autonode_t *ap)
{
	umntrequest *ul, *t;
	struct autoinfo *aip;
	int retrans = 5;	/*XXX INT_MAX? */
	int error = 0;
	int reqcount = 0;
	CLIENT *clnt;
	struct timeval timeout;
	enum clnt_stat status;
	umntres result;

	if (unmount_list == NULL)
		return (0);

	ul = unmount_list;
#ifdef AUTODEBUG
        auto_dprint(autodebug, 3, 
		    "send_unmount_req 0x%lx, %s, flags %x, size %d, ref %d\n",
                    (antovn(ap)), ap->an_name, ap->an_mntflags, ap->an_size,
		    ap->an_count);
#endif

	aip = antoai(ap);

	clnt = clntkudp_create(&autofs_addr, AUTOFS_PROG, AUTOFS_VERS, 
				retrans, KUDP_NOINTR, KUDP_XID_PERCALL, 
				get_current_cred());
	if (clnt == NULL) {
		cmn_err(CE_WARN,
			"autofs: clntkudp_create failed for unmount\n");
		error = EIO;
		goto done;
	}

	/* allow time for umount (and remount) of hierarchy */
	for (t = ul; t; t = t->next)
		reqcount++;
	timeout.tv_sec = aip->ai_rpc_to * reqcount;
	timeout.tv_usec = 0;

	status = clnt_call(clnt, AUTOFS_UNMOUNT,
			(xdrproc_t) xdr_umntrequest,
			(caddr_t) ul, (xdrproc_t) xdr_umntres,
			(caddr_t) &result, timeout);
	clnt_destroy(clnt);

	switch (status) {
	case RPC_SUCCESS:
		error = result.status;
		break;
	case RPC_INTR:
		error = EINTR;
		break;
	case RPC_TIMEDOUT:
		/*
		 * Shouldn't get here since we're
		 * supposed to retry forever (almost).
		 */
		cmn_err(CE_WARN, "autofs: unmount request for %s timed out\n",
					ap->an_name);
		error = ETIMEDOUT;
		break;
	default:
		cmn_err(CE_WARN, "autofs: %s\n",
			clnt_sperrno(status));
		error = ENOENT;
		break;
	}
done:
	while (ul) {
		unmount_list = unmount_list->next;
		kmem_free((caddr_t) ul, sizeof (*ul));
		ul = unmount_list;
	}
	unmount_list = NULL;
	return (error);
}

int rm_autonode_forgiving(
	autonode_t *ap)
{
	vnode_t *vp = antovn(ap);
	autonode_t *dap;
	autonode_t **app, *cap;
	int s;

	s = VN_LOCK(vp);
	if (vp->v_vfsmountedhere != NULL) {
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, 
		    "autofs: rm_autonode of persistent mount: %s\n",
		    ap->an_name);
#endif
		VN_UNLOCK(vp, s);
		return (1);
	}

	if (vp->v_flag & VMOUNTING) {
		VN_UNLOCK(vp, s);
                return (1);
        }

	/* 
	 * Will prevent other mounts from taking place. 
	 * Look at function fs_cover in fs_subr.c.
	 */
	vp->v_flag |= VMOUNTING;
	VN_UNLOCK(vp, s);

	rw_enter(&ap->an_rwlock, RW_WRITER);

	if (ap->an_dirents != NULL) {
		rw_exit(&ap->an_rwlock);
		return 1;
	}
	/*
	 * remove the node only if is an indirect mount
	 * or offset node for a direct mount. ie. dont
	 * remove the node if it was the direct mount
	 * mountpoint.
	 */
	if (ap->an_mntflags & MF_MNTPNT) {
		rw_exit(&ap->an_rwlock);
		return (0);
	}

	dap = ap->an_parent;

	if (dap == NULL)
	{
		cmn_err(CE_WARN,"rm_autonode: No parent for autonode : %llx.",ap);
		rw_exit(&ap->an_rwlock);
		return 0;
	}
	app = &dap->an_dirents;
	/*
	 * do not need to hold a RW lock because we are
	 * holding the RW LOCK for the top of the hierarchy
	 */
	for (;;) {
		cap = *app;
		if (cap == NULL) {
			cmn_err(CE_WARN,
				"rm_autonode: No entry for 0x%lx\n", ap);
			rw_exit(&ap->an_rwlock);
			return 0;
		}
		if (cap == ap)
			break;
		app = &cap->an_next;
	}
	*app = cap->an_next;
	cap->an_next = NULL;
	/* 
	 * This is to mark that this autonode is no longer
	 * available.
	 */
	if (cap->an_error == 0)
		cap->an_error = ENOENT;
	rw_exit(&ap->an_rwlock);
	mutex_enter(&dap->an_lock);
	dap->an_size--;
	mutex_exit(&dap->an_lock);

	/* the autonode had a pointer to parent so vn_rele it */
	
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode rele 0x%lx %s, ref %d\n", 
		    (antovn(dap)), dap->an_name, (antovn(dap))->v_count-1);
#endif
	VN_RELE(antovn(dap));


	/* 
	 * Get a reference to the vnode which will be VN_RELE'd below.
	 * The purpose of this reference, under the old reference counting
	 * scheme, is to ensure that we go through inactive for the last
	 * time, once an_count and an_size allow it to be effective.
	 *
         * This VN_HOLD is not necessary under the new scheme in which
	 * we only go thorugh inactive once.  The VN_HOLD should be 
	 * reoved after the new tree splits off from kudzu.
	 */
#ifndef CELL_IRIX
	VN_HOLD(antovn(ap));
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode HOLD 0x%lx %s, ref %d\n", 
		    (antovn(ap)), ap->an_name, (antovn(ap))->v_count);
#endif
#endif /* !CELL_IRIX */

	mutex_enter(&ap->an_lock);
	ap->an_size -= 2;
	ap->an_count--;
	mutex_exit(&ap->an_lock);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode exit rele 0x%lx %s, ref %d\n", 
		    (antovn(ap)), ap->an_name, (antovn(ap))->v_count-1);
#endif
	VN_RELE(antovn(ap));
	return (0);
}

static int
rm_autonode(
	autonode_t *ap)
{
	vnode_t *vp = antovn(ap);
	autonode_t *dap;
	autonode_t **app, *cap;

	if (vp->v_vfsmountedhere != NULL) {
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, 
		    "autofs: rm_autonode of persistent mount: %s\n",
		    ap->an_name);
#endif
		return (1);
	}
	/*
	 * remove the node only if is an indirect mount
	 * or offset node for a direct mount. ie. dont
	 * remove the node if it was the direct mount
	 * mountpoint.
	 */
	if (ap->an_mntflags & MF_MNTPNT)
		return (0);

	dap = ap->an_parent;

	ASSERT((dap != ap) && (dap != NULL));

	if ((app = &dap->an_dirents) == NULL)
		cmn_err(CE_PANIC,
			"rm_autonode: null directory list in parent 0x%lx",
			dap);

	/*
	 * do not need to hold a RW lock because we are
	 * holding the RW LOCK for the top of the hierarchy
	 */
	for (;;) {
		cap = *app;
		if (cap == NULL) {
			cmn_err(CE_PANIC,
				"rm_autonode: No entry for 0x%lx\n", ap);
		}
		if (cap == ap)
			break;
		app = &cap->an_next;
	}
	*app = cap->an_next;
	cap->an_next = NULL;
	mutex_enter(&dap->an_lock);
	dap->an_size--;
	mutex_exit(&dap->an_lock);

	/* the autonode had a pointer to parent so vn_rele it */
	
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode rele 0x%lx %s, ref %d\n", 
		    (antovn(dap)), dap->an_name, (antovn(dap))->v_count-1);
#endif
	VN_RELE(antovn(dap));


	/* 
	 * Get a reference to the vnode which will be VN_RELE'd below.
	 * The purpose of this reference, under the old reference counting
	 * scheme, is to ensure that we go through inactive for the last
	 * time, once an_count and an_size allow it to be effective.
	 *
         * This VN_HOLD is not necessary under the new scheme in which
	 * we only go thorugh inactive once.  The VN_HOLD should be 
	 * reoved after the new tree splits off from kudzu.
	 */
#ifndef CELL_IRIX
	VN_HOLD(antovn(ap));
#endif /* !CELL_IRIX */

#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode HOLD 0x%lx %s, ref %d\n", 
		    (antovn(ap)), ap->an_name, (antovn(ap))->v_count);
#endif
	mutex_enter(&ap->an_lock);
	ap->an_size -= 2;
	ap->an_count--;
	mutex_exit(&ap->an_lock);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_autonode exit rele 0x%lx %s, ref %d\n", 
		    (antovn(ap)), ap->an_name, (antovn(ap))->v_count-1);
#endif
	VN_RELE(antovn(ap));
	return (0);
}

/*
 * Check the reference counts on an autonode and
 * sub-autonodes to see if there's a process that's
 * holding a reference.  If so, then return 1.
 */
static int
autofs_busy(
	struct autonode *ap)
{
	int dirs = ap->an_size - 2;	/* don't count "." and ".." */
	int refs = antovn(ap)->v_count - 1; /* don't count the VN_HOLD done before calling autofs_busy */
	struct autonode *child;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, 
		    "autofs_busy ap 0x%lx flags %x, refs %d, dirs %d\n", 
		    ap, ap->an_mntflags, refs, dirs);
#endif
	if (ap->an_mntflags & MF_MOUNTED)
		refs--;

	if (dirs && refs > dirs)
		return (1);

	rw_enter(&ap->an_rwlock, RW_READER);
	for (child = ap->an_dirents; child; child = child->an_next) {
		VN_HOLD(antovn(child));
		if (autofs_busy(child)) {
			VN_RELE(antovn(child));
			rw_exit(&ap->an_rwlock);
			return (1);
		}
		VN_RELE(antovn(child));
	}
	rw_exit(&ap->an_rwlock);

	return (0);
}

static int
rm_hierarchy(
	autonode_t *ap)
{
	autonode_t *child, *next, *parent;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "rm_hierarchy %s HOLD 0x%lx v_count %d\n", 
		    ap->an_name, antovn(ap), (antovn(ap))->v_count);
#endif
	parent = ap->an_parent;
	ASSERT(parent);

	VN_HOLD(antovn(ap));
#ifdef AUTODEBUG
	auto_dprint(autodebug, 8, "rm_hierarchy HOLD 0x%lx %s, ref %d\n", 
		    (antovn(ap)), ap->an_name, (antovn(ap))->v_count);
#endif
	rw_exit(&parent->an_rwlock);

	rw_enter(&ap->an_rwlock, RW_READER);
	/* LINTED lint thinks next may be used before set */
	for (child = ap->an_dirents; child; child = next) {
		next = child->an_next;
		if (rm_hierarchy(child)) {
			rw_exit(&ap->an_rwlock);
			
			rw_enter(&parent->an_rwlock, RW_WRITER);
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, 
				    "auto_lookup rm_hierarchy exit RELE 0x%lx v_ct %d\n", 
				    antovn(ap), (antovn(ap))->v_count);
#endif
			VN_RELE(antovn(ap));
			return (1);
		}
	}
	rw_exit(&ap->an_rwlock);

	rw_enter(&parent->an_rwlock, RW_WRITER);
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, 
		    "auto_lookup rm_hierarchy RELE 0x%lx v_ct %d\n", 
		    antovn(ap), (antovn(ap))->v_count);
#endif
	VN_RELE(antovn(ap));
	return (rm_autonode(ap));
}

static void
unmount_hierarchy(
	autonode_t *ap)
{
	autonode_t *dap;
	int res = 0;
	struct autoinfo *aip;
	time_t time_now;

	unmount_list = NULL;
	mnts_in_hierarchy = 0;
	time_now = time;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, 
		    "unmount_hierarchy: %s, size %d, v_count %d, ref %d\n", 
		    ap->an_name, ap->an_size, (antovn(ap))->v_count, 
		    ap->an_count);
#endif
	aip = antoai(ap);

	if (ap->an_ref_time + aip->ai_mount_to > time_now)
		return;
	if (ap->an_mntflags & MF_INPROG)
		return;

	if (autofs_busy(ap)) {
		ap->an_ref_time = time_now;
		return;
	}

	make_unmount_list(ap);
	if (mnts_in_hierarchy == 0) {
		ASSERT(unmount_list == NULL);

		dap = ap->an_parent;
		rw_enter(&dap->an_rwlock, RW_WRITER);
		/*
		 * it is important to grab this lock before
		 * checking if somebody is waiting to
		 * prevent anybody from doing a lookup
		 * before we get this lock
		 */
		rm_hierarchy(ap);
		rw_exit(&dap->an_rwlock);
	}

	if (unmount_list) {
		/*
		 * if it is a direct mount without any offset
		 * set the is direct flag in the unmount request
		 * It help the daemon to determine that it is a
		 * direct mount and so it does not attach any spaces
		 * to the end of the path for unmounting
		 */
		if ((ap->an_mntflags & MF_MNTPNT) &&
		    (ap->an_dirents == NULL)) {
			umntrequest *ur;
			for (ur = unmount_list; ur; ur = ur->next)
				ur->isdirect = 1;
		}

		mutex_enter(&ap->an_lock);
		ap->an_mntflags |= MF_UNMOUNTING;
		mutex_exit(&ap->an_lock);

#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, "unmounting %s vp 0x%lx ref %d\n",
			    ap->an_name, antovn(ap), ap->an_count);
#endif
		res = send_unmount_request(ap);

		/*
		 * the result can never be a timeout. send_unmount_req
		 * ensures that. The trouble with timeout is that we
		 * do not know how far the unmount has succeeded if
		 * at all. Therefore we need to keep the MF_UNMOUNTING
		 * flag. This will not let any process enter the
		 * hierarchy. So in send_unmount_request we keep
		 * trying until we get a response other than timeout
		 */
		if (res) {
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, "failed unmount\n");
#endif
			/*
			 * Not successful, so wakeup every
			 * thread sleeping on this hierarchy
			 */
			mutex_enter(&ap->an_lock);
			ap->an_mntflags &= ~MF_UNMOUNTING;
			if (ap->an_mntflags & MF_WAITING_UMOUNT) {
				/*
				 * threads are waiting, but since
				 * the unmount did not succeed, we
				 * dont want them to attempt a re-mount
				 */
				ap->an_mntflags |= MF_DONTMOUNT;
				cv_broadcast(&ap->an_cv_umount);
			}
			ap->an_ref_time = time_now;
			mutex_exit(&ap->an_lock);

		} else {
#ifdef AUTODEBUG
			auto_dprint(autodebug, 10, "successful unmount\n");
#endif
			/* successful unmount */
			dap = ap->an_parent;
			rw_enter(&dap->an_rwlock, RW_WRITER);
			/*
			 * it is important to grab this lock before
			 * checking if somebody is waiting to
			 * prevent anybody from doing a lookup
			 * before we get this lock
			 */
			mutex_enter(&ap->an_lock);
			ap->an_mntflags &= ~MF_UNMOUNTING;
			ap->an_mntflags &= ~MF_MOUNTED;
			if (ap->an_mntflags & MF_WAITING_UMOUNT) {
				cv_broadcast(&ap->an_cv_umount);
				mutex_exit(&ap->an_lock);
			} else {
				mutex_exit(&ap->an_lock);
				rm_hierarchy(ap);
			}
			rw_exit(&dap->an_rwlock);
		}
	}
}

void
do_unmount(void)
{
	autonode_t *ap, *next;
	autonode_t *root_ap;

	for (;;) {			/* loop forever */
		delay(120 * HZ);

		/*
		 * The unmount cannot hold the
		 * autonode_list_lock during unmounts
		 * because one of those unmounts might
		 * be an autofs if we have hierarchical
		 * autofs mounts.
		 * Hence we use a simple marking scheme
		 * to make sure we check all the autonodes
		 * for unmounting.
		 * Start by marking them all "not checked"
		 */
		mutex_enter(&autonode_list_lock);
		for (ap = autonode_list; ap; ap = ap->an_next)
		{
			mutex_enter(&ap->an_lock);
			ap->an_mntflags &= ~(MF_CHECKED);
			mutex_exit(&ap->an_lock);
		}
		mutex_exit(&autonode_list_lock);

		/*
		 * Now check the list, starting from the
		 * beginning each pass until we make
		 * a complete pass without finding
		 * any not checked.
		 */
		for (;;) {
			mutex_enter(&autonode_list_lock);
			for (ap = autonode_list; ap; ap = ap->an_next) {
				if (!(ap->an_mntflags & MF_CHECKED))
					break;
			}
			mutex_exit(&autonode_list_lock);
			if (ap == NULL)
				break;

			/*
			 * XXX this should change. The check for
			 * MF_INPROG flag should be set at one level
			 * lower.
			 * But till we fix do_mount this remains
			 */
			mutex_enter(&ap->an_lock);
			if (ap->an_mntflags & MF_INPROG) {
				ap->an_mntflags |= MF_CHECKED;
				mutex_exit(&ap->an_lock);
				continue;
			}
			mutex_exit(&ap->an_lock);

			/*
			 * if it is a direct mount then treat it
			 * as a hierarchy, otherwise go one more
			 * level and treat that as a hierarchy
			 */
			if (ap->an_mntflags & MF_MNTPNT) {
				/* a direct mount */
				if (ap->an_dirents == NULL) {
					/*
					 * must be direct mount
					 * without offsets, so,
					 * only go further if
					 * something is mounted
					 * here
					 */
					if ((antovn(ap))->v_vfsmountedhere)
						unmount_hierarchy(ap);
				} else
					/*
					 * direct mount with offsets
					 */
					unmount_hierarchy(ap);
			} else if (ap->an_dirents) {
				rw_enter(&ap->an_rwlock, RW_READER);
				if (root_ap = ap->an_dirents) {
					VN_HOLD(antovn(root_ap));
#ifdef AUTODEBUG
					auto_dprint(autodebug, 8, "rm_autonode HOLD 0x%lx %s, ref %d\n", 
						    (antovn(root_ap)), root_ap->an_name, (antovn(root_ap))->v_count);
#endif
				}
				for (; root_ap;	root_ap = next) {
					next = root_ap->an_next;
					if (next) {
						VN_HOLD(antovn(next));
#ifdef AUTODEBUG
						auto_dprint(autodebug, 10, 
							    "auto_lookup DO_UNMOUNT HOLD 0x%lx v_ct %d\n", 
							    antovn(next), (antovn(next))->v_count);
#endif
					}
					rw_exit(&ap->an_rwlock);

					unmount_hierarchy(root_ap);

					rw_enter(&ap->an_rwlock, RW_READER);
#ifdef AUTODEBUG
					auto_dprint(autodebug, 10, 
						    "auto_lookup DO_UNMOUNT RELE 0x%lx v_ct %d\n", 
						    antovn(root_ap), (antovn(root_ap))->v_count);
#endif
					VN_RELE(antovn(root_ap));
				}
				rw_exit(&ap->an_rwlock);
			}
			mutex_enter(&ap->an_lock);
			ap->an_mntflags |= MF_CHECKED;
			mutex_exit(&ap->an_lock);
		}

	}
}

static void
make_unmount_list(
	autonode_t *auto_dir)
{
	vnode_t *vp;
	int t;
	int error = 0;
	autonode_t *ap;

#ifdef AUTODEBUG
	umntrequest *ul;

	auto_dprint(autodebug, 3, "make_unmount_list: %s\n", 
		    auto_dir->an_name);
	for (ap = auto_dir->an_dirents; ap; ap = ap->an_next)
		auto_dprint(autodebug, 3, "	dir %s\n", ap->an_name);
#endif
	vp = antovn(auto_dir);

	if (auto_dir->an_dirents && !(vp->v_vfsmountedhere)) {
		for (ap = auto_dir->an_dirents; ap; ap = ap->an_next)
			make_unmount_list(ap);
	}
	/*
	 * check if any other vfs is rooted here
	 * if yes then add this path to the
	 * unmount list and recurse again
	 */
	if (vp->v_vfsmountedhere) {
#ifdef AUTODEBUG
	auto_dprint(autodebug, 10, "vfsmountedhere 0x%lx dev %x\n", 
		    vp->v_vfsmountedhere, vp->v_vfsmountedhere->vfs_dev);
#endif
		t = vfs_spinlock();
		error = get_hierarchical_mounts(vp->v_vfsmountedhere);
		vfs_spinunlock(t);
		if (error) {
			umntrequest *temp;
			while (unmount_list) {
				temp = unmount_list;
				unmount_list = temp->next;
				kmem_free((caddr_t) temp, sizeof (*temp));
			}
		}
	}
#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "exit make_unmount_list:\n"); 
	for (ul = unmount_list; ul; ul = ul->next)
		auto_dprint(autodebug, 3, "    dev %lx rdev %lx\n", 
			    ul->devid, ul->rdevid);
#endif
}


static int
get_hierarchical_mounts(
	vfs_t *vfsp)
{
	vfs_t *cvfs;
	vnode_t *covered_vp = NULL;
	umntrequest *ul;
	int error;

#ifdef AUTODEBUG
	umntrequest *ur;

		auto_dprint(autodebug, 3, 
			    "get_hierarchical_mounts: checking dev=%x\n",
			    vfsp->vfs_dev);
#endif
	if (vfsp->vfs_nsubmounts) {
		for (cvfs = rootvfs->vfs_next; cvfs; cvfs = cvfs->vfs_next) {
			if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT))
				return (EBUSY);
			if (vfsp != cvfs) {
				if (!cvfs->vfs_vnodecovered)
					continue;
				covered_vp = cvfs->vfs_vnodecovered;
				if (covered_vp->v_vfsp == vfsp) {
					/* something else is rooted here */
					error = get_hierarchical_mounts(cvfs);
					if (error)
						return (error);
				}
			}
		}
	}
	mnts_in_hierarchy++;

	/*
	 * XXX check for VFS_BUSY can come here
	 * put it in the FRONT of the unmount list
	 */

	ul = (umntrequest *)kmem_alloc(sizeof (*ul), KM_SLEEP);
	ul->next = NULL;

	ul->devid = vfsp->vfs_dev;
#ifdef AUTODEBUG
		auto_dprint(autodebug, 10, "unmounting dev %x\n", ul->devid);
#endif
	/*
	 * XXXdmt -- They made me do it.
	 * Find a way for lofs to stash this info
	 * somewhere else (underlying vnode's rdev maybe,
	 * ul->rdevid = vfsp->vfs_vnodecovered->v_rdev?)
	 */
	if (vfsp->vfs_fstype == lofsfstype) {
		bhv_desc_t *bdp;

		bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &lofs_vfsops);
#ifdef AUTODEBUG
		auto_dprint(autodebug, 4, "unmounting LOFS\n");
#endif
		ul->rdevid = vfs_bhvtoli(bdp)->li_rdev;
	} else {
		ul->rdevid = 0;
	}
	ul->isdirect = 0;
	if (unmount_list)
		ul->next = unmount_list;
	unmount_list = ul;

#ifdef AUTODEBUG
	auto_dprint(autodebug, 3, "current unmount list:\n");
	for (ur = ul; ur; ur = ur->next)
		auto_dprint(autodebug, 3, 
			    "   dev %x, rdev %x, next 0x%lx\n", 
			    ur->devid, ur->rdevid, ur->next);
#endif
	return (0);
}

bool_t
xdr_umntrequest(
	register XDR *xdrs,
	umntrequest *objp)
{
	umntrequest *ur;
	bool_t more_data;

	for (ur = objp; ur; ur = ur->next) {
#ifdef AUTODEBUG
		auto_dprint(autodebug, 3, 
			    "xdr_umntrequest: dev %lx, rdev %lx\n", 
			    ur->devid, ur->rdevid);
#endif
		if (!xdr_u_int(xdrs, &ur->isdirect))
			return (FALSE);
		if (!xdr_u_int(xdrs, (u_int *)&ur->devid))
			return (FALSE);
		if (!xdr_u_int(xdrs, (u_int *)&ur->rdevid))
			return (FALSE);
		more_data = (ur->next != NULL);
		if (!xdr_bool(xdrs, &more_data))
			return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_mntrequest(
	register XDR *xdrs,
	mntrequest *objp)
{
	return (xdr_string(xdrs, &objp->name, A_MAXNAME) &&
		xdr_string(xdrs, &objp->map,  A_MAXNAME) &&
		xdr_string(xdrs, &objp->opts, 256)	&&
		xdr_string(xdrs, &objp->path, A_MAXPATH));
}

bool_t
xdr_mntres(
	register XDR *xdrs,
	mntres *objp)
{
	return (xdr_int(xdrs, &objp->status));
}

bool_t
xdr_umntres(
	register XDR *xdrs,
	umntres *objp)
{
	return (xdr_int(xdrs, &objp->status));
}

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */

#ifdef AUTODEBUG
/* VARARGS3 */
void
auto_dprint(int var, int level, char *str, ...)
{
	va_list ap;
	va_start(ap, str);
	if (var == level || (var > 10 && (var - 10) >= level))
		icmn_err(CE_CONT, str, ap);
	va_end(ap);
	return;
}
#endif
