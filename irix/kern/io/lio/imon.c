/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * imon - inode monitor pseudo device
 *
 * $Revision: 1.115 $
 *
 * Overview
 *
 * This driver provides a mechanism whereby a user process can monitor
 * various system activity on a list files.  The user process expresses
 * interest in files by passing the name of each file via an ioctl, and
 * then does a read on the device to obtain event records.  Each event
 * record contains the device and inode number of the modified file and
 * a field describing the action that took place.
 *
 * User Interface
 *
 * The express-interest ioctl takes three arguments: the pathname to the
 * target file, a bitmask indicating which events are of interest, and
 * optionally a pointer to a stat buffer in which the current state of
 * the file is returned.  Multiple expressions on the same or different
 * files are permitted.  Multiple expressions on the same file accumulate
 * interests.  A revoke ioctl that takes the same arguments is also available.
 *
 * Interest can be expressed in all files contained in the target file's
 * filesystem device.  Inodes are monitored both for their individual event
 * interests and for any events of interest to all files contained in their
 * filesystem device.
 *
 * The device is currently implemented as an exclusive open driver.
 *
 * It is possible to monitor efs, xfs, nfs, and nfs3 files
 * although events are generated only for local activity on nfs files.
 *
 * Theory of Operation
 *
 * The device works by hooking into a filesystem's iget routine so that a
 * driver function is called every time a new inode is brought into the inode
 * cache.  The hook, checkinode(), then looks up the device and inode number
 * in a local hash table to see if the user has expressed interest on it.
 * An extension of the vnode structure is then used to splice in a set of
 * functions as the normal entry points to that vnode's filesystem specific
 * code.  The trick lies in the use of a redundant labeling of filesystem
 * type in the vnode and its vfs.  There are two fields containing equivalent
 * information: v_vfsp->vfs_fstype is an index into the vfssw[] array, which
 * contains a vnodeops pointer for each filesystem type; and v_op is pointer
 * directly from a vnode to its filesystem's vnodeops struct containing the
 * entry points.
 *
 * When checkinode determines that a vnode should be monitored, the vnode's
 * v_op field is set to the address of a local vnodeops jump table that re-
 * directs all of the per-file file system calls to this driver.  Most of
 * the calls are passed through to the real filesystem switch by using the
 * v_vfsp->vfs_fstype member as an index into the vfssw table, and then via
 * the vnodeops pointer stashed there by each filesystem's init routine.
 * Operations that are interesting cause an event to be queued for the user
 * program that has opened /dev/imon.
 *
 * As an optimization, an added field in the vnode structure is used to
 * hold a shadow copy of the interest mask for that inode.  This saves a
 * hash table lookup that would otherwise have to be performed each time
 * an event happens to an inode to see if that event is one that the user
 * wants to see.  Another extension to the vnode struct contains linkage
 * pointers for the list of all monitored, active vnodes.
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fsid.h>
#include <sys/fs_subr.h>
#include <sys/imon.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/poll.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <os/as/region.h> /* XXX */
#include <sys/mload.h>
#include <sys/cred.h>
#include <sys/flock.h>
#include <sys/pvnode.h>
#include <string.h>
#include <bstring.h>
#include <stddef.h>

#ifdef	DEBUG
#undef STATIC
#define	STATIC
#endif

#if defined(DEBUG) || defined(IMON_TEST)
#define TESTING_CODE 1
#ifdef	DEBUG
#define TESTING_DFLT 1
#else
#define TESTING_DFLT 1
#endif
#endif 

/*
 * Imon private per-vnode data.
 */

typedef struct idlist {
	struct imon_data	*il_next;
	struct imon_data	*il_prev;
} idlist_t;
STATIC idlist_t	imon_idlist;	/* list of all monitored, active objects */

typedef struct imon_data {
	idlist_t	id_list;
	bhv_desc_t	id_bhv;
	intmask_t	id_what;
} imon_data_t;
#define	id_next	id_list.il_next
#define	id_prev	id_list.il_prev

STATIC void id_initlist(void);
STATIC void id_insert(imon_data_t *);
STATIC void id_unlink(imon_data_t *);
STATIC imon_data_t *id_alloc(void);
STATIC void id_free(imon_data_t *);

#define	VN_TO_BHVU(vp)	vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &imon_vnodeops)
#define	VN_TO_BHVL(vp)	vn_bhv_lookup(VN_BHV_HEAD(vp), &imon_vnodeops)
#define	ID_TO_VN(id)	BHV_VOBJ(&id->id_bhv)
#define	BHV_TO_ID(bhvp)	\
	((imon_data_t *)((char *)(bhvp) - offsetof(imon_data_t, id_bhv)))

#define FSINDEX(vp)		(vp)->v_vfsp->vfs_fstype 

int imondevflag = D_MP;

char *imonmversion = M_VERSION;

/* queue functions */
STATIC int	dequeue(int, qelem_t *);
STATIC void	enqueue(imon_data_t *, intmask_t, cred_t *);
STATIC void	initqueue(void);
STATIC void	freequeue(void);
STATIC int	imonqtest(void);

#define ENQUEUE(id,e,cr)	{ \
        ENQUEUE_DELAY; \
        if (id->id_what & (e)) { enqueue(id,e,cr); } \
	}
#if TESTING_CODE
#define ENQUEUE_DELAY 		{ \
        if (imon_test_delay) delay(1); \
	}
#else
#define ENQUEUE_DELAY
#endif

/* Only vn_unlink vnodes that have been put on a list. */
#define VN_UNLINK(vp)	\
	if (vp->v_next != vp && vp->v_prev != vp) { vn_unlink(vp); }

/* queue structure is a circular array with pointers to head and tail */
STATIC struct	{
	qelem_t *q_head;	/* next qe to send to user */
	qelem_t *q_tail;	/* next avail qe to add an event */
	qelem_t	*q_base;	/* first element in qe array */
	qelem_t *q_last;	/* last element in qe array */
	int	q_thresh;	/* quick wakeup threshold */
	toid_t	q_timeid;	/* timeout id */
	int	q_count;	/* number of events in q */
	char	q_tpending;	/* timeout pending */
	char	q_over;		/* overflow flag */
	char	q_wanted;	/* user is sleeping */
} ino_ev_q;

/* hash functions */
STATIC void	clearhash(void);
STATIC void	hashremove(ino_t, dev_t, intmask_t);
STATIC int	hashinsert(ino_t, dev_t, intmask_t);
STATIC intmask_t probehash(ino_t, dev_t);

STATIC int	getdevino(vnode_t *, dev_t *, ino_t *, cred_t *);
STATIC void	checkinode(vnode_t *, dev_t, ino_t);
STATIC void	oobevent(vnode_t *, cred_t *, int);
STATIC void	broadcastevent(dev_t, int);

#define BADFS(vfsp)	((vfsp)->vfs_fstype != efs_fstype && \
			 (vfsp)->vfs_fstype != xfs_fstype && \
			 (vfsp)->vfs_fstype != spec_fstype && \
			 (vfsp)->vfs_fstype != nfs_fstype && \
			 (vfsp)->vfs_fstype != nfs3_fstype)

/* our special vnode operations jump table, filled in dynamically */
vnodeops_t imon_vnodeops;
STATIC void imon_init_vnodeops(void);

#define	BADVNODE(vp)	(VNODE_TO_FIRST_BHV(vp) == NULL)

STATIC lock_t	imonqlock;	/* lock guarding event queue */
STATIC mutex_t	imonhsema;	/* mutex guarding interest hash table */
STATIC mutex_t	imonsema;	/* guards against simultaneous open/close */
STATIC mutex_t	imonlsema;	/* imon data list lock */
STATIC sv_t	imonwsema;	/* sleeper */
STATIC struct pollhead	imonsel;	

#if TESTING_CODE
int imon_test_poison = TESTING_DFLT;
int imon_test_delay = TESTING_DFLT;
#endif

extern int	efs_fstype;	/* fs type index for EFS */
extern int	xfs_fstype;	/* fs type index for XFS */
extern int	spec_fstype;	/* fs type index for special devices */
static int	nfs_fstype;	/* fs type index for NFS */
static int	nfs3_fstype;	/* fs type index for NFS3 */

void
imoninit(void)
{
	int		i;
	static int	inited = 0;

	if (inited)
		return;
	inited = 1;
	imon_init_vnodeops();
	id_initlist();
	spinlock_init(&imonqlock, "imnq");
	initpollhead(&imonsel);
	mutex_init(&imonhsema, MUTEX_DEFAULT, "imonh");
	mutex_init(&imonsema, MUTEX_DEFAULT, "imon");
	mutex_init(&imonlsema, MUTEX_DEFAULT, "imonl");
	sv_init(&imonwsema, SV_DEFAULT, "imnw");
	for (i = 1; i < nfstype; i++) {
#define	EQ(name)	(vfssw[i].vsw_name && !strcmp(vfssw[i].vsw_name, name))
		if (EQ(FSID_NFS))
			nfs_fstype = i;
		else if (EQ(FSID_NFS3))
			nfs3_fstype = i;
#undef	EQ
	}
}

static int nodelaymode;

/*
 * Plug ourselves into the file open hook.  The hook is in iget so
 * that it is called the first time an inode is read in from disk.
 */
/* ARGSUSED */
int
imonopen(dev_t *devp, int flag)
{
	mutex_lock(&imonsema, PINOD);

	/* multiple clients are not yet supported */
	if (imon_enabled) {
		mutex_unlock(&imonsema);
		return EBUSY;
	}

	initqueue();
	nodelaymode = flag & (FNONBLOCK|FNDELAY);
	imon_hook = checkinode;
	imon_event = oobevent;
	imon_broadcast = broadcastevent;
	imon_enabled = 1;

	mutex_unlock(&imonsema);
	return 0;
}

/*
 * Un-plug file open hook and release data structures.
 */
/* ARGSUSED */
int
imonclose(dev_t dev, int flag)
{
	imon_data_t	*id;

	mutex_lock(&imonsema, PINOD);
	imon_enabled = 0;

	/*
	 * Shutdown hash table before un-hooking inodes.
	 * Calls may still be made to probehash() after the
	 * inode monitor enabled flag is turned off since the flag
	 * is not protected.  Calling clearhash() will effectively
	 * disable probehash() so that no further inodes will get
	 * plugged.
	 */
	clearhash();

	/*
	 * Undo any interpositions that we've put in.
	 */
	mutex_lock(&imonlsema, PINOD);
	id = imon_idlist.il_next;
	while (id != (imon_data_t *)&imon_idlist) {
	        id->id_what &= ~IMON_EVENTMASK;
		id = id->id_list.il_next;
	}
	mutex_unlock(&imonlsema);

	freequeue();

	mutex_unlock(&imonsema);
	return 0;
}

STATIC imon_data_t *
imon_plug(vnode_t *vp, intmask_t what)
{
	bhv_desc_t	*bdp;
	int		error;
	imon_data_t	*id;

	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));
	mutex_lock(&imonlsema, PINOD);
	bdp = VN_TO_BHVL(vp);
	if (bdp) {
		/* already there, all we need to do is or in the new events */
		id = BHV_TO_ID(bdp);
		id->id_what |= what;
	} else {
		id = id_alloc();
		bdp = &id->id_bhv;
		bhv_desc_init(bdp, id, vp, &imon_vnodeops);
		id->id_what = what;
		error = vn_bhv_insert(VN_BHV_HEAD(vp), bdp);
		if (error) {
			id_free(id);
			bdp = VN_TO_BHVL(vp);
			id = BHV_TO_ID(bdp);
			id->id_what |= what;
		} else {
			id_insert(id);
		}
	}
	mutex_unlock(&imonlsema);
	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));
	return id;
}

/*
 * Collect and convert imon interest ioctl arguments.
 */
STATIC int
imon_getargs(__psint_t args, interest_t *inp, vnode_t **vpp, dev_t *devp,
	     ino_t *inop, cred_t *cr)
{
	int error;
	vnode_t *vp;

	/* get and check user request */
	switch (get_current_abi()) {
	case ABI_IRIX5:
	case ABI_IRIX5_N32:
	    {
		irix5_32_interest_t interest;

		if (copyin((caddr_t)args, &interest, sizeof interest))
			return EFAULT;
		inp->in_fname = (char *)(__psint_t)interest.in_fname;
		inp->in_sb = (struct stat *)(__psint_t)interest.in_sb;
		inp->in_what = interest.in_what;
		break;
	    }
#if _MIPS_SIM == _ABI64
	case ABI_IRIX5_64:
	    {
		irix5_64_interest_t interest;

		if (copyin((caddr_t)args, &interest, sizeof interest))
			return EFAULT;
		inp->in_fname = (char *)(__psint_t)interest.in_fname;
		inp->in_sb = (struct stat *)(__psint_t)interest.in_sb;
		inp->in_what = interest.in_what;
		break;
	    }
#endif
	}

	if (!inp->in_what || (inp->in_what & ~IMON_USERMASK))
		return EINVAL;

	/* find inode of file */
	error = lookupname(inp->in_fname, UIO_USERSPACE, NO_FOLLOW,
			   NULLVPP, &vp, NULL);
	if (error)
		return error;

	/* make sure this is an interesting fs */
	if (BADFS(vp->v_vfsp) || BADVNODE(vp)) {
		VN_RELE(vp);
		return EINVAL;
	}

	/* if user wants a stat too, get it now. */
	if (inp->in_sb) {
		error = xcstat(vp, inp->in_sb, _STAT_VER, 0, cr);
	}

	/* get the inode and dev returned by stat */
	if (!error)
		error = getdevino(vp, devp, inop, cr);
	if (error) {
		VN_RELE(vp);
		return error;
	}

	/* check whether to match any inode in dev */
	if (inp->in_what & IMON_CONTAINED)
		*inop = IMON_ANYINODE;
	inp->in_what &= IMON_EVENTMASK;

	*vpp = vp;
	return 0;
}

STATIC int imon_anyinodes;

/* ARGSUSED */
int
imonioctl(dev_t xxx, int cmd, __psint_t args, int mode, cred_t *cr, int *rval)
{
	interest_t in;
	vnode_t *vp = NULL;
	dev_t dev;
	ino_t ino;
	int error;
	revokdi_t rv_in;
	imon_data_t *id;
	bhv_desc_t *bdp;

	switch (cmd) {

	/*
	 * Express interest in a file and optionally return that
	 * file's stat structure.  Multiple expressions have their
	 * interests or'd together.
	 */
	case IMONIOC_EXPRESS:
		error = imon_getargs(args, &in, &vp, &dev, &ino, cr);
		if (error)
			return error;

		/* insert interest in inode/dev pair in hash table */
		error = hashinsert(ino, dev, in.in_what);
		if (!error) {
			if (ino == IMON_ANYINODE)
				imon_anyinodes++;

			/* Plug in our fs switch hook. */
			id = imon_plug(vp, in.in_what);

			/* Check if it is currently running. */
			if (vp->v_mreg && (vp->v_mreg->p_maxprots&PROT_EXEC)) {
				ENQUEUE(id, IMON_EXEC, cr);
				id->id_what |= IMON_EXECUTING;
			}
			if (vp->v_intpcount) {
				ENQUEUE(id, IMON_EXEC, cr);
				id->id_what |= IMON_EXECUTING;
			}
		}

		/* done with inode for now */
		VN_RELE(vp);
		return error;

	/*
	 * Revoke interest in a dev/inode pair.  Only the interests
	 * specified by the in_what field are revoked.  When the last
	 * interest in a dev/inode pair is revoked it is removed from
	 * the hash table.
	 */
	case IMONIOC_REVOKE:
		error = imon_getargs(args, &in, &vp, &dev, &ino, cr);
		if (error)
			return error;

		/* remove interest in inode/dev pair from hash table */
		hashremove(ino, dev, in.in_what);
		if (ino == IMON_ANYINODE)
			imon_anyinodes--;

		bdp = VN_TO_BHVU(vp);
		if (bdp != NULL) {
			id = BHV_TO_ID(bdp);
			id->id_what &= ~in.in_what;
		}

		VN_RELE(vp);
		break;

        /*
         * Revoke interest in a dev/inode pair.  Specified
         * by dev and ino, not by path.
         */
        case IMONIOC_REVOKDI:
	    {
		switch (get_current_abi()) {
		case ABI_IRIX5:
		    {
			irix5_o32_revokdi_t revokdi;

			if (copyin((caddr_t)args, &revokdi, sizeof(revokdi)))
				return EFAULT;
			rv_in.rv_dev = revokdi.rv_dev;
			rv_in.rv_ino = revokdi.rv_ino;
			rv_in.rv_what = revokdi.rv_what;
			break;
		    }
		case ABI_IRIX5_N32:
		    {
			irix5_n32_revokdi_t revokdi;

			if (copyin((caddr_t)args, &revokdi, sizeof(revokdi)))
				return EFAULT;
			rv_in.rv_dev = revokdi.rv_dev;
			rv_in.rv_ino = revokdi.rv_ino;
			rv_in.rv_what = revokdi.rv_what;
			break;
		    }
#if _MIPS_SIM == _ABI64
		case ABI_IRIX5_64:
		    {
			irix5_64_revokdi_t revokdi;

			if (copyin((caddr_t)args, &revokdi, sizeof(revokdi)))
				return EFAULT;
			rv_in.rv_dev = revokdi.rv_dev;
			rv_in.rv_ino = revokdi.rv_ino;
			rv_in.rv_what = revokdi.rv_what;
			break;
		    }
#endif /* _ABI64 */
		}
                if (!rv_in.rv_what || (rv_in.rv_what & ~IMON_USERMASK))
                        return EINVAL;
                hashremove(rv_in.rv_ino, rv_in.rv_dev, rv_in.rv_what);

                /*
                 *  Leave vnode (if any) plugged.  Will clean up on next
                 *  vnodeop.
                 */

                break;
	    }

	/*
	 * Qtest returns non-zero when there are events available
	 * in the queue.
	 */
	case IMONIOC_QTEST:
		*rval = imonqtest();
		break;

	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Read events off of the queue.
 *
 * Reads as many events as will fit in user's buffer.  Normally waits
 * for at least one event and then reads as many as are available without
 * blocking.  If the device is opened in non-delay mode, then it will 
 * return immediately with EAGAIN.
 * This outputs in binary form a series of structs of which one member
 * is a dev_t. So this has to be massaged depending on the abi of the
 * invoker.
 */

/* ARGSUSED2 */
int
imonread(dev_t dev, uio_t *uio, cred_t *cr)
{
	int resid, error, sleepok;
	qelem_t qe;
	int abi;

	resid = uio->uio_resid;
	switch (abi = get_current_abi()) {
	case ABI_IRIX5:
		if (resid < sizeof(irix5_o32_qelem_t))
			return EINVAL;
		break;
	case ABI_IRIX5_N32:
		if (resid < sizeof(irix5_n32_qelem_t))
			return EINVAL;
		break;
#if _MIPS_SIM == _ABI64
	case ABI_IRIX5_64:
		if (resid < sizeof(irix5_64_qelem_t))
			return EINVAL;
		break;
#endif /* _ABI64 */
	}

	error = 0;
	sleepok = !nodelaymode;
	while (uio->uio_resid >= sizeof qe) {
		if (error = dequeue(sleepok, &qe))
			break;
		switch (abi) {
		case ABI_IRIX5:
		    {
			irix5_o32_qelem_t i5_o32_qe;

			i5_o32_qe.qe_inode = qe.qe_inode;
			i5_o32_qe.qe_dev = qe.qe_dev;
			i5_o32_qe.qe_what = qe.qe_what;
			error = uiomove((caddr_t)&i5_o32_qe, sizeof i5_o32_qe,
					UIO_READ, uio);
			break;
		    }
		case ABI_IRIX5_N32:
		    {
			irix5_n32_qelem_t i5_n32_qe;

			i5_n32_qe.qe_inode = qe.qe_inode;
			i5_n32_qe.qe_dev = qe.qe_dev;
			i5_n32_qe.qe_what = qe.qe_what;
			error = uiomove((caddr_t)&i5_n32_qe, sizeof i5_n32_qe,
					UIO_READ, uio);
			break;
		    }
#if _MIPS_SIM == _ABI64
		case ABI_IRIX5_64:
		    {
			irix5_64_qelem_t i5_64_qe;

			i5_64_qe.qe_inode = qe.qe_inode;
			i5_64_qe.qe_dev = qe.qe_dev;
			i5_64_qe.qe_what = qe.qe_what;
			error = uiomove((caddr_t)&i5_64_qe, sizeof i5_64_qe,
					UIO_READ, uio);
			break;
		    }
#endif /* _ABI64 */
		}
		if (error)
			return error;
		sleepok = 0;
	}

	return (uio->uio_resid == resid) ? error : 0;
}

/* ARGSUSED */
int
imonwrite(dev_t dev, uio_t *uio, cred_t *cr)
{
	return EROFS;
}

/*
 * Called by select and poll system call code to see what's available.
 * Imon provides only select on read by only one selector.
 */
/* ARGSUSED */
int
imonpoll(dev_t dev, short events, int anyyet, short *reventsp,
	 struct pollhead **phpp, unsigned int *genp)
{
	events &= POLLIN | POLLRDNORM;

	if (events & (POLLIN | POLLRDNORM)) {
		/* snapshot pollhead generation number before we test queue */
		unsigned int gen = POLLGEN(&imonsel);
		if (!imonqtest()) {
			events &= ~(POLLIN | POLLRDNORM);
			if (!anyyet) {
				*phpp = &imonsel;
				*genp = gen;
			}
			ino_ev_q.q_wanted = 1;
		}
	}
	
	*reventsp = events;
	return 0;
}

/*
 * Get the real dev/inode numbers for a vnode.
 */
STATIC int
getdevino(vnode_t *vp, dev_t *devp, ino_t *inop, cred_t *cr)
{
	vattr_t vattr;
	bhv_desc_t *bdp, *nbdp;
	int error;

	vattr.va_mask = AT_FSID|AT_NODEID;
	bdp = VNODE_TO_FIRST_BHV(vp);
	if (BHV_OPS(bdp) == &imon_vnodeops) {
		PV_NEXT(bdp, nbdp, vop_getattr);
	} else {
		nbdp = bdp;
	}
	PVOP_GETATTR(nbdp, &vattr, 0, cr, error);
	if (error)
		return error;
	*devp = vattr.va_fsid;
	*inop = vattr.va_nodeid;
	return 0;
}

/*----------------------------imon data management-----------------------*/

STATIC zone_t *idlist_zone;

STATIC void
id_initlist(void)
{
	imon_idlist.il_next = imon_idlist.il_prev = (imon_data_t *)&imon_idlist;
	idlist_zone = kmem_zone_init(sizeof(imon_data_t), "imondata");
}

STATIC void
id_insert(imon_data_t *id)
{
	id->id_next = imon_idlist.il_next;
	id->id_prev = (imon_data_t *)&imon_idlist;
	imon_idlist.il_next = id;
	id->id_next->id_prev = id;
}

STATIC void
id_unlink(imon_data_t *id)
{
	imon_data_t	*next;
	imon_data_t	*prev;

	next = id->id_next;
	prev = id->id_prev;
	next->id_prev = prev;
	prev->id_next = next;
	id->id_next = id->id_prev = id;
}

STATIC imon_data_t *
id_alloc(void)
{
	return kmem_zone_zalloc(idlist_zone, KM_SLEEP);
}

STATIC void
id_free(imon_data_t *id)
{
	kmem_zone_free(idlist_zone, id);
}


/*----------------------------queue operators----------------------------*/
/*
 * The queue is implemented as a circular list of events.  New events are
 * matched against the last QBACKSTEPS events and if the dev/inode are the
 * same, then the two events are or'd together.  After an event is entered
 * into the queue, a timeout of imon_qlag ticks is set to wakeup the reader.
 * If the queue fills beyond q_thresh elements, the timeout is canceled and
 * the reader is woken immediately.
 */

extern int imon_qsize;	/* defined in master.d/imon */
extern int imon_qlag;	/* defined in master.d/imon */

#define QBACKSTEPS	10	  /* how far back to look for event compress */

STATIC void
dequeuewakeup(void)
{
	sv_signal(&imonwsema);
	pollwakeup(&imonsel, POLLIN | POLLRDNORM);
	ino_ev_q.q_tpending = 0;
	ino_ev_q.q_wanted = 0;
}

/* ARGSUSED */
STATIC void
enqueue_di(dev_t dev, ino_t ino, intmask_t cause)
{
	int s;

	/* XXX The following assertion is unreasonable because of a
	 * race between this function and imonclose().
	 * ASSERT(imon_enabled && ino_ev_q.q_base);
	 */
	if (imon_enabled == 0 && ino_ev_q.q_base == 0)
		return;				/* imonclose'ed */

	s = mp_mutex_spinlock(&imonqlock);

	/* Make sure queue is still active once inside of imonqlock */
	if (ino_ev_q.q_base == 0)
		goto out;

	/* Check if this event is happening to an inode that already has
	 * an outstanding event and if so reduce them to one event.
	 *
	 * There's a race condition here: If a program execs-exits-execs,
	 * then the user program won't be able to tell the current state.
	 * The same situation occurs for a file that opens-closes-opens.
	 * Therefore, these events turn off their counterparts so the
	 * event the user sees reflects the current state.
	 */
	if (ino_ev_q.q_tail != ino_ev_q.q_head) {
		qelem_t *qe = ino_ev_q.q_tail;
		int steps = 0;
		do {
			if (qe == ino_ev_q.q_base)
				qe = ino_ev_q.q_last;
			else
				--qe;
			if (qe->qe_inode == ino && qe->qe_dev == dev) {
				if (cause == IMON_EXEC)
					qe->qe_what &= ~IMON_EXIT;
				else if (cause == IMON_EXIT)
					qe->qe_what &= ~IMON_EXEC;
				qe->qe_what |= cause;
				goto out;
			}
		} while (qe != ino_ev_q.q_head && steps++ < QBACKSTEPS);
	}

	/* last enqueue filled last slot in queue */
	if (ino_ev_q.q_over)
		goto out;

	/* Put data in q slot */
	ino_ev_q.q_tail->qe_inode = ino;
	ino_ev_q.q_tail->qe_dev   = dev;
	ino_ev_q.q_tail->qe_what  = cause;

	/* Bump pointer to next available slot, wrap if we're at the limit. */
	if (ino_ev_q.q_tail == ino_ev_q.q_last)
		ino_ev_q.q_tail = ino_ev_q.q_base;
	else
		ino_ev_q.q_tail++;

	++ino_ev_q.q_count;

	/* See if we've caught up with our tail. */
	if (ino_ev_q.q_tail == ino_ev_q.q_head) {
		ino_ev_q.q_over = 1;
		ASSERT(ino_ev_q.q_tpending == 0); /*should have passed qthresh*/
		cmn_err(CE_WARN,"/dev/imon: event queue overflow");
	} else if (ino_ev_q.q_count == 1 && !ino_ev_q.q_tpending
		   && ino_ev_q.q_wanted) {
		/* on the first event, start a timeout to wake up user */
		ino_ev_q.q_tpending = 1;
		ino_ev_q.q_timeid = timeout(dequeuewakeup, 0, imon_qlag);
	} else {
		int t = splhi();
		/*
		 * on subsequent events, check for a rapidly filling queue
		 * and wakeup user early if so
		 */
		if (ino_ev_q.q_count>ino_ev_q.q_thresh && ino_ev_q.q_tpending) {
			untimeout(ino_ev_q.q_timeid);
			ino_ev_q.q_tpending = 0;
			dequeuewakeup();
		}
		splx(t);
	}

out:
	mp_mutex_spinunlock(&imonqlock, s);
}

STATIC void
enqueue(imon_data_t *id, intmask_t cause, cred_t *cr)
{
	ino_t ino;
	dev_t dev;

	if (imon_enabled == 0 && ino_ev_q.q_base == 0)
		return;				/* imonclose'ed */

	if (getdevino(ID_TO_VN(id), &dev, &ino, cr))
		return;

	enqueue_di(dev, ino, cause);
}

STATIC int
imonqtest(void)
{
	return ino_ev_q.q_count ? 1 : 0;
}

/*
 * Get an element off the queue.  Return 0 or an errno.
 */
/* ARGSUSED */
STATIC int
dequeue(int sleepok, qelem_t *qe)
{
	int s;
	static qelem_t oe = { 0, 0, IMON_OVER };	/* Overflow token */

	s = mp_mutex_spinlock(&imonqlock);

	/* If we've overflowed, then throw out everything and start over. */
	if (ino_ev_q.q_over) {
		ino_ev_q.q_over = 0;
		ino_ev_q.q_count = 0;
		mp_mutex_spinunlock(&imonqlock, s);
		*qe = oe;
		return 0;
	}

	while (ino_ev_q.q_tail == ino_ev_q.q_head) {	/* queue is empty */
		if (!sleepok) {
			mp_mutex_spinunlock(&imonqlock, s);
			return EAGAIN;
		}
		ino_ev_q.q_wanted = 1;
		mp_sv_wait_sig(&imonwsema, PZERO+1, &imonqlock, s);
		s = mp_mutex_spinlock(&imonqlock);
	}

	*qe = *ino_ev_q.q_head;
	--ino_ev_q.q_count;

	if (ino_ev_q.q_head == ino_ev_q.q_last)
		ino_ev_q.q_head = ino_ev_q.q_base;
	else
		ino_ev_q.q_head++;

	mp_mutex_spinunlock(&imonqlock, s);
	return 0;
}

/*
 * Initialize event queue.
 */
STATIC void
initqueue(void)
{
	qelem_t *base;
	int qsize = imon_qsize;

	base = kmem_alloc(qsize * sizeof(qelem_t), KM_SLEEP);
	ino_ev_q.q_head = ino_ev_q.q_tail = ino_ev_q.q_base = base;
	ino_ev_q.q_last = base + qsize - 1;
	ino_ev_q.q_thresh = qsize / 4;
	ino_ev_q.q_count = 0;
}

/*
 * Drain and delete event queue.
 */
STATIC void
freequeue(void)
{
	int s;
	s = mp_mutex_spinlock(&imonqlock);

	ino_ev_q.q_tail = ino_ev_q.q_head = 0;

	if (ino_ev_q.q_base) {
		void *base = ino_ev_q.q_base;
		ino_ev_q.q_base = 0;
		mp_mutex_spinunlock(&imonqlock, s);
		kmem_free(base, imon_qsize * sizeof(qelem_t));
		return;
	}

	mp_mutex_spinunlock(&imonqlock, s);
}

/*----------------------------hash operators-----------------------------*/

/*
 * Interest hash table
 */
typedef struct inthash {
	u_int		ih_shift;	/* log2(hash table size) */
	u_int		ih_numfree;	/* count of free entries */
	qelem_t		*ih_base;	/* dynamically allocated hash table */
	qelem_t		*ih_limit;	/* end of hash table */
#ifdef OS_METER
	struct {
		u_long	im_lookups;	/* hash table lookups */
		u_long	im_probes;	/* linear probes in hash table */
		u_long	im_hits;	/* lookups which found key */
		u_long	im_misses;	/* keys not found */
		u_long	im_adds;	/* entries added */
		u_long	im_grows;	/* table expansions */
		u_long	im_shrinks;	/* table compressions */
		u_long	im_deletes;	/* entries deleted */
		u_long	im_dprobes;	/* entries hashed while deleting */
		u_long	im_dmoves;	/* entries moved while deleting */
	} ih_meters;
#endif
} inthash_t;

#define ACTIVE(x)	((x)->qe_what)
#define ALPHA(n)	((1 << (n)) - (1 << (n)-3))

/*
 * Create a new hash table of size 2^shift entries.
 */
STATIC void
ihnew(inthash_t *ih, u_short shift)
{
	int count = 1 << shift;

	/*
	 * Reserve one empty slot for the hash probe sentinel, and waste
	 * the remaining 1/8 entries to get a good alpha.
	 */
	ih->ih_numfree = ALPHA(shift);
	ih->ih_shift = shift;
	ih->ih_base = kmem_zalloc(count * sizeof(qelem_t), KM_SLEEP);
	ih->ih_limit = ih->ih_base + count;

#ifdef OS_METER
	/* clear the meters */
	bzero(&ih->ih_meters, sizeof ih->ih_meters);
#endif
}

/*
 * Multiplicative hash with linear probe.  Assumes long ints are 32 bits.
 */
extern int imon_hashhisize;
extern int imon_hashlosize;

#define	PHI	(int)2654435769	/* (golden mean) * (2 to the 32) */

#define	IHHASH(ih, ik) \
	(((__uint32_t)(PHI * ((ik)->qe_dev ^ (ik)->qe_inode))) >> (32 - (ih)->ih_shift))

/*
 * Return a pointer to the entry matching ik if it exists, otherwise
 * to the empty entry in which ik should be installed.
 */
STATIC qelem_t *
ihlookup(inthash_t *ih, qelem_t *ik, int adding)
{
	qelem_t *ihe;

	METER(ih->ih_meters.im_lookups++);
	ihe = &ih->ih_base[IHHASH(ih, ik)];
	while (ACTIVE(ihe) &&
	    (ihe->qe_inode != ik->qe_inode || ihe->qe_dev != ik->qe_dev)) {
		/*
		 * Because ih_numfree was initialized to 7/8 of the hash
		 * table size, we need not check for for table fullness.
		 * We're guaranteed to hit an empty (!ACTIVE(ihe)) entry
		 * before revisiting the primary hash.
		 */
		METER(ih->ih_meters.im_probes++);
		if (adding)
			ihe->qe_what |= IMON_COLLISION;
		if (ihe == ih->ih_base)
			ihe = ih->ih_limit;
		--ihe;
	}
	return ihe;
}

/*
 * Add a new key/value pair to the inode hash.
 * If the hash overflows, and its size is below its maximum, grow the
 * hash by doubling its size and re-inserting all of its elements.
 */
STATIC int
ihadd(inthash_t *ih, qelem_t *ik)
{
	qelem_t *ihe;

	ASSERT(ACTIVE(ik));
	if (ih->ih_numfree == 0) {
		inthash_t tih;

		if (ih->ih_shift == imon_hashhisize)
			return 0;
		/*
		 * Hash table is full, so double its size and re-insert all
		 * active entries plus the new one, ik.
		 */
		METER(ih->ih_meters.im_grows++);
		ihnew(&tih, ih->ih_shift + 1);
		for (ihe = ih->ih_base; ihe < ih->ih_limit; ihe++)
			if (ACTIVE(ihe))
				(void) ihadd(&tih,ihe);
#ifdef OS_METER
		tih.ih_meters = ih->ih_meters;	/* copy the meters */
#endif
		kmem_free(ih->ih_base, (1<<ih->ih_shift) * sizeof(qelem_t));
		*ih = tih;
	}

	ihe = ihlookup(ih, ik, 1);
	if (ACTIVE(ihe))
		ihe->qe_what |= ik->qe_what;
	else {
		METER(ih->ih_meters.im_adds++);
		ASSERT(ih->ih_numfree > 0);
		--ih->ih_numfree;
		*ihe = *ik;
	}
	return 1;
}

/*
 * Remove an interest from the hash and shrink the hash table if necessary.
 */
STATIC void
ihdelete(inthash_t *ih, qelem_t *ik)
{
	qelem_t *ihe;

	ihe = ihlookup(ih, ik, 0);
	if (!ACTIVE(ihe))
		return;
	ihe->qe_what &= ~ik->qe_what;
	if ((ihe->qe_what & ~IMON_COLLISION) != 0)
		return;
	METER(ih->ih_meters.im_deletes++);
	ASSERT(ih->ih_numfree < ALPHA(ih->ih_shift));
	ih->ih_numfree++;

	if (ih->ih_shift > imon_hashlosize &&
	    ih->ih_numfree == (1 << ih->ih_shift-1) + (1 << ih->ih_shift-2)) {
		inthash_t tih;

		/*
		 * If 3/4 of the hash table is empty, shrink it.
		 */
		METER(ih->ih_meters.im_shrinks++);
		ihe->qe_what = 0;
		ihnew(&tih, ih->ih_shift - 1);
		for (ihe = ih->ih_base; ihe < ih->ih_limit; ihe++)
			if (ACTIVE(ihe))
				(void) ihadd(&tih, ihe);
#ifdef OS_METER
		tih.ih_meters = ih->ih_meters;	/* copy the meters */
#endif
		kmem_free(ih->ih_base, (1<<ih->ih_shift) * sizeof(qelem_t));
		*ih = tih;
	} else if (ihe->qe_what & IMON_COLLISION) {
		int slot, slot2, hash;
		qelem_t *ihe2;

		/*
		 * If not shrinking, reorganize any colliding entries.
		 */
		slot = slot2 = ihe - ih->ih_base;
		for (;;) {
			ihe->qe_what = 0;
			do {
				if (--slot2 < 0)
					slot2 += 1 << ih->ih_shift;
				ihe2 = &ih->ih_base[slot2];
				if (!ACTIVE(ihe2))
					return;
				METER(ih->ih_meters.im_dprobes++);
				hash = IHHASH(ih, ihe2);
			} while (slot2 <= hash && hash < slot
			      || slot < slot2 && (hash < slot
			      || slot2 <= hash));
			METER(ih->ih_meters.im_dmoves++);
			*ihe = *ihe2;
			ihe = ihe2;
			slot = slot2;
		}
	}
}

inthash_t imon_htable;	/* not static, so we can nlist it */

STATIC int
hashinsert(ino_t inum, dev_t dev, intmask_t what)
{
	qelem_t ik;
	int error;
	static time_t nextprint, ntickstowait;

	mutex_lock(&imonhsema, PINOD);

	if (!imon_htable.ih_base)
		ihnew(&imon_htable, imon_hashlosize);

	ik.qe_dev   = dev;
	ik.qe_inode = inum;
	ik.qe_what  = what;

	if (ihadd(&imon_htable,&ik))
		error = 0;
	else {
		error = ENOMEM;
		if (lbolt >= nextprint) {
			/*
			 * Prevent printfs from clobbering performance.
			 * fam(1M) continues to express interest after
			 * a hash table overflow.
			 */
			cmn_err(CE_WARN,"/dev/imon: hash table overflow\n");
			if (lbolt > nextprint + ntickstowait)
				ntickstowait = 100;	/* (re)start */
			else if (ntickstowait < 6400)	/* ~ 1 minute */
				ntickstowait *= 2;
			nextprint = lbolt + ntickstowait;
		}
	}

	mutex_unlock(&imonhsema);
	return error;
}

STATIC void
hashremove(ino_t inum, dev_t dev,intmask_t what)
{
	qelem_t ik;

	mutex_lock(&imonhsema, PINOD);

	if (!imon_htable.ih_base) {
		mutex_unlock(&imonhsema);
		return;
	}

	ik.qe_dev   = dev;
	ik.qe_inode = inum;
	ik.qe_what  = what;

	ihdelete(&imon_htable,&ik);

	mutex_unlock(&imonhsema);
}

/*
 * Check if a given dev/inode pair is in the interest hash table.
 * Returns interest mask if in table, 0 otherwise.
 */
STATIC intmask_t
probehash(ino_t inum, dev_t dev)
{
	qelem_t ik;
	intmask_t what;

	mutex_lock(&imonhsema, PINOD);

	/* check if hash table is not allocated yet */
	if (imon_htable.ih_base == NULL) {
		mutex_unlock(&imonhsema);
		return 0;
	}

	ik.qe_dev   = dev;
	ik.qe_inode = inum;

	what = ihlookup(&imon_htable,&ik,0)->qe_what & IMON_EVENTMASK;

	METER(what ? imon_htable.ih_meters.im_hits++
		   : imon_htable.ih_meters.im_misses++);

	mutex_unlock(&imonhsema);
	return what;
}

STATIC void
clearhash(void)
{
	void *base;
	size_t size;

	if (imon_htable.ih_base == 0)
		return;
	mutex_lock(&imonhsema, PINOD);
	base = imon_htable.ih_base;
	size = (1 << imon_htable.ih_shift) * sizeof(qelem_t);
	imon_htable.ih_base = 0;
	mutex_unlock(&imonhsema);
	kmem_free(base, size);
}

STATIC void
checkinode(vnode_t *vp, dev_t dev, ino_t ino)
{
	intmask_t what;

	/*
	 * Make some general eliminations
	 */
	if (BADFS(vp->v_vfsp) || BADVNODE(vp))
		return;

	/*
	 * If we're interested in this inode, plug its vnodeops with our
	 * special operations hook.
	 */
	what = probehash(ino, dev);
	if (imon_anyinodes)
		what |= probehash(IMON_ANYINODE, dev);
	if (what)
		imon_plug(vp, what);
}

/*
 * Broadcast event to all active nodes on that device.
 */
STATIC void
broadcastevent(dev_t dev, int event)
{
	qelem_t *qe;

	/*
	 * Should broadcast IMON_UNMOUNT. Use DELETE until Famd is changed.
	 */
	if (event == IMON_UNMOUNT) {
		event = IMON_DELETE;
	}

	/* check if hash table is not allocated yet */
	mutex_lock(&imonhsema, PINOD);
	if (imon_htable.ih_base == NULL) {
		mutex_unlock(&imonhsema);
		return;
	}

	/* run through table and check each element */
	for (qe = imon_htable.ih_base; qe < imon_htable.ih_limit; qe++) {
		if (ACTIVE(qe) && (dev == qe->qe_dev)) {
			enqueue_di(dev, qe->qe_inode, event);
		}
	}

	mutex_unlock(&imonhsema);
}

/*
 * Out of band imon event
 *
 * This fills in some holes in the current implementation
 */
STATIC void
oobevent(vnode_t *vp, cred_t *cr, int event)
{
	bhv_desc_t *bdp;


	/*
	 * We get oob events for all sorts of things, so 
	 * we may not have an imon behavior for this vnode.
	 * Just return silently if that's the case.
	 */
	bdp = VN_TO_BHVU(vp);

	if (bdp)
		ENQUEUE(BHV_TO_ID(bdp), event, cr);
}

/*----------------------------vnode operations---------------------------*/

STATIC int
imon_write(bhv_desc_t *bdp, uio_t *uiop, int ioflag, cred_t *cr, flid_t *fl)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_write);
	PVOP_WRITE(nbdp, uiop, ioflag, cr, fl, error);
	return error;
}

STATIC int
imon_setattr(bhv_desc_t *bdp, vattr_t *vap, int flags, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	/* Fam doesn't monitor access time and
	 * it changes frequently.
	 */
	if (vap->va_mask != AT_UPDATIME)
		ENQUEUE(BHV_TO_ID(bdp), IMON_ATTRIBUTE, cr);
	PV_NEXT(bdp, nbdp, vop_setattr);
	PVOP_SETATTR(nbdp, vap, flags, cr, error);
	return error;
}

STATIC int
imon_create(bhv_desc_t *bdp, char *name, vattr_t *vap, int flags, int mode,
	    vnode_t **vpp, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	if (!*vpp)
		ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_create);
	PVOP_CREATE(nbdp, name, vap, flags, mode, vpp, cr, error);
	return error;
}

STATIC int
imon_remove(bhv_desc_t *bdp, char *nm, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_remove);
	PVOP_REMOVE(nbdp, nm, cr, error);
	return error;
}

STATIC int
imon_link(bhv_desc_t *bdp, vnode_t *svp, char *tnm, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	nbdp = VN_TO_BHVU(svp);
	if (nbdp)
		ENQUEUE(BHV_TO_ID(nbdp), IMON_ATTRIBUTE, cr);
	PV_NEXT(bdp, nbdp, vop_link);
	PVOP_LINK(nbdp, svp, tnm, cr, error);
	return error;
}

STATIC int
imon_rename(bhv_desc_t *bdp, char *snm, vnode_t *tdvp, char *tnm,
	    pathname_t *tpnp, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	PV_NEXT(bdp, nbdp, vop_rename);
	PVOP_RENAME(nbdp, snm, tdvp, tnm, tpnp, cr, error);
	if (error)
		return error;
	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	return 0;
}

STATIC int
imon_mkdir(bhv_desc_t *bdp, char *dirname, vattr_t *vap, vnode_t **vpp,
	   cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_mkdir);
	PVOP_MKDIR(nbdp, dirname, vap, vpp, cr, error);
	return error;
}

STATIC int
imon_rmdir(bhv_desc_t *bdp, char *nm, vnode_t *cdir, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_rmdir);
	PVOP_RMDIR(nbdp, nm, cdir, cr, error);
	return error;
}

STATIC int
imon_symlink(bhv_desc_t *bdp, char *linkname, vattr_t *vap, char *target,
	     cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_CONTENT, cr);
	PV_NEXT(bdp, nbdp, vop_symlink);
	PVOP_SYMLINK(nbdp, linkname, vap, target, cr, error);
	return error;
}

STATIC int
imon_inactive(bhv_desc_t *bdp, cred_t *cr)
{
	int		error;
	imon_data_t	*id;
	bhv_desc_t	*nbdp;
	vattr_t		vattr;

	id = BHV_TO_ID(bdp);
	if (id->id_what & IMON_EXECUTING) {
		ENQUEUE(id, IMON_EXIT, cr);
		id->id_what &= ~IMON_EXECUTING;
	}
	vattr.va_mask = AT_NLINK;
	PV_NEXT(bdp, nbdp, vop_getattr);
	PVOP_GETATTR(nbdp, &vattr, ATTR_LAZY, cr, error);
	if (!error && vattr.va_nlink <= 0)
		ENQUEUE(id, IMON_DELETE, cr);
	PV_NEXT(bdp, nbdp, vop_inactive);
	PVOP_INACTIVE(nbdp, cr, error);
	/*
	 * If the underlying behaviors all go away now,
	 * then we have to go as well.
	 */
	if (error == VN_INACTIVE_NOCACHE) {
		vn_bhv_remove(VN_BHV_HEAD(BHV_TO_VNODE(bdp)), &id->id_bhv);
		mutex_lock(&imonlsema, PINOD);
		id_unlink(id);
		mutex_unlock(&imonlsema);
		id_free(id);
	}
	return error;
}

STATIC int
imon_map(bhv_desc_t *bdp, off_t off, size_t len,
	 mprot_t prot, u_int flags, cred_t *cr, vnode_t **nvp)
{
	int		error;
	imon_data_t	*id;
	bhv_desc_t	*nbdp;

	if (prot & PROT_EXEC) {
		id = BHV_TO_ID(bdp);
		ENQUEUE(id, IMON_EXEC, cr);
		id->id_what |= IMON_EXECUTING;
	}
	PV_NEXT(bdp, nbdp, vop_map);
	PVOP_MAP(nbdp, off, len, prot, flags, cr, nvp, error);
	return error;
}

STATIC int
imon_reclaim(bhv_desc_t *bdp, int flags)
{
	int		error = 0;
	imon_data_t	*id;
	bhv_desc_t	*nbdp;
	vnode_t		*vp;

	id = BHV_TO_ID(bdp);
	vp = ID_TO_VN(id);
	if (bdp->bd_next) {
		PV_NEXT(bdp, nbdp, vop_reclaim);
		PVOP_RECLAIM(nbdp, flags, error);
		/*
		 * If the behavior below us refuses to be reclaimed,
		 * then stick around until it decides to go away.
		 */
		if (error) {
			return error;
		}
	} else {
		nbdp = NULL;
	}
	vn_bhv_remove(VN_BHV_HEAD(vp), &id->id_bhv);
	mutex_lock(&imonlsema, PINOD);
	id_unlink(id);
	mutex_unlock(&imonlsema);
	id_free(id);

	return error;
}

STATIC int
imon_attr_set(bhv_desc_t *bdp, char *attrname, char *attrvalue, int valuelength,
	      int flags, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_ATTRIBUTE, cr);
	PV_NEXT(bdp, nbdp, vop_attr_set);
	PVOP_ATTR_SET(nbdp, attrname, attrvalue, valuelength, flags, cr, error);
	return error;
}

STATIC int
imon_attr_remove(bhv_desc_t *bdp, char *attrname, int flags, cred_t *cr)
{
	int		error;
	bhv_desc_t	*nbdp;

	ENQUEUE(BHV_TO_ID(bdp), IMON_ATTRIBUTE, cr);
	PV_NEXT(bdp, nbdp, vop_attr_remove);
	PVOP_ATTR_REMOVE(nbdp, attrname, flags, cr, error);
	return error;
}

STATIC void
imon_init_vnodeops(void)
{
	imon_vnodeops = *vn_passthrup;
	imon_vnodeops.vn_position.bi_position = VNODE_POSITION_IMON;
	imon_vnodeops.vop_attr_remove = imon_attr_remove;
	imon_vnodeops.vop_attr_set = imon_attr_set;
	imon_vnodeops.vop_create = imon_create;
	imon_vnodeops.vop_inactive = imon_inactive;
	imon_vnodeops.vop_link = imon_link;
	imon_vnodeops.vop_map = imon_map;
	imon_vnodeops.vop_mkdir = imon_mkdir;
	imon_vnodeops.vop_reclaim = imon_reclaim;
	imon_vnodeops.vop_remove = imon_remove;
	imon_vnodeops.vop_rename = imon_rename;
	imon_vnodeops.vop_rmdir = imon_rmdir;
	imon_vnodeops.vop_setattr = imon_setattr;
	imon_vnodeops.vop_symlink = imon_symlink;
	imon_vnodeops.vop_write = imon_write;
}
