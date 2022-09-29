/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 3.260 $"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/filio.h>
#include <sys/ioccom.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/sema.h>
#include <sys/sad.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/kabi.h>
#include <sys/kstropts.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/termio.h>
#include <sys/termios.h>
#include <bsd/sys/socket.h>	/* for SIOC ioctls */
#include <bsd/net/if.h>		/* for SIOC ioctls */
#include <bsd/net/soioctl.h>	/* for SIOC ioctls */
#include <sys/strlog.h>		/* for I_CHKSYSLOGD XXX */
#if _MIPS_SIM == _ABI64
#include <bsd/net/ksoioctl.h>	/* for SIOC ioctls */
#endif /* _ABI64 */

#ifdef _IRIX_LATER
#include <io/xt/jioctl.h>
#endif /* _IRIX_LATER */
#include <sys/cred.h>
#include <ksys/vpgrp.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <ksys/vsession.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/serialio.h>

#ifdef _IRIX_LATER
#include <util/inline.h>
#endif /* _IRIX_LATER */
#include <sys/sysmacros.h>
#include <fifofs/fifonode.h>
#include <limits.h>		/* for USHRT_MAX  */
#include <sys/ttold.h>
#include <sys/ddi.h>
#include <sys/sat.h>
#include <sys/idbgactlog.h>
#include <sys/capability.h>
#include <sys/ksignal.h>
#include <net/soioctl.h>
#include <sys/fs_subr.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

/*
 * id value used to distinguish between different ioctl messages
 */
STATIC uint ioc_id;

/*
 *  Qinit structure and Module_info structures
 *        for stream head read and write queues
 */
static int strrput(queue_t *, mblk_t *);
int strwsrv(queue_t *);
STATIC int strsink(queue_t *, mblk_t *);

/* Prototypes: */
extern vnode_t *spec_find(dev_t, vtype_t);

STATIC int inbackground(struct stdata *);
STATIC int accessctty(enum jobcontrol);

extern int iscn(vnode_t *vp);

#if _MIPS_SIM == _ABI64
static void irix5_strioctl_to_strioctl(
			struct irix5_strioctl *, struct strioctl *);
static void strioctl_to_irix5_strioctl(
			struct strioctl *, struct irix5_strioctl *);
static void irix5_strbuf_to_strbuf(
			struct irix5_strbuf *, struct strbuf *);
static void irix5_strpeek_to_strpeek(
			struct irix5_strpeek *, struct strpeek *);
static void strpeek_to_irix5_strpeek(
			struct strpeek *, struct irix5_strpeek *);
static void irix5_str_list_to_str_list(
			struct irix5_str_list *, struct str_list *);
static void irix5_strfdinsert_to_strfdinsert(
			struct irix5_strfdinsert *, struct strfdinsert *);
#endif


STATIC struct module_info strm_info = { 0, "strrhead", 0, INFPSZ, STRHIGH, STRLOW };
STATIC struct module_info stwm_info = { 0, "strwhead", 0, 0, 0, 0 };
struct	qinit strdata = { strrput, NULL, NULL, NULL, NULL, &strm_info, NULL };
struct	qinit stwdata = { NULL, strwsrv, NULL, NULL, NULL, &stwm_info, NULL };
STATIC struct qinit deadrend = {
	strsink, NULL, NULL, NULL, NULL, &strm_info, NULL
};
STATIC struct qinit deadwend = {
	NULL, NULL, NULL, NULL, NULL, &stwm_info, NULL
};

extern struct streamtab fifoinfo;

const speed_t cbaud_to_baud_table[] = {
    0,
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    4800,
    9600,
    19200,
    38400
};

extern int baud_to_cbaud(speed_t);

/*
 * Tunable parameters set in kernel.cf and used only in this file.
 */
extern int	strmsgsz;	/* maximum stream message size */
extern int	strctlsz;	/* maximum size of ctl part of message */

long stropenfail;	/* DEBUG */

extern int clnopen();
/*
 * Open a stream device.
 */
int
stropen(struct vnode *vp, dev_t *devp, struct vnode **vpp,int flag,cred_t *crp)
{
	register struct stdata *stp;
	register queue_t *qp;
	register int s, s2;
	dev_t dummydev;
	struct autopush *ap;
	int error = 0;
	int freed = 0;
        dev_t olddev = *devp;
	struct cdevsw *my_cdevsw;
	int useprivmon;
	struct streamtab *stab;
	int sl;
	mon_t *monp;

retry:
	/*
	 * Obtain monitor
	 * single thread open so we don't have situation of monitor with
	 * an NULL stp. Code section below is expanded STHEAD_LOCK macro.
	 */
	LOCK_STRHEAD_MONP(sl);
	if (((stp) = (vp->v_stream)) && (monp = (stp)->sd_monp)) {
		/*
		 * XXXrs Hope this works. We pass a -1 as the pri so that
		 * spinunlock_pmonitor will return (1) if it found the
		 * monitor in use and had to sleep. We return in this case
		 * because we can not be sure the monp's are still valid.
		 */
		if (spinunlock_pmonitor(&strhead_monp_lock, sl,
			&(stp)->sd_monp, -1, &(stp)->sd_monp)) {
			stropenfail++;	/* DEBUG */
			goto retry;
		}
		LOCK_STRHEAD_MONP(sl);
		if (!((stp) = (vp->v_stream)))
			vmonitor(monp);
	} else {
		stp = 0;
	}
	/*
	 * If the stream already exists, wait for any open in progress
	 * to complete, then call the open function of each module and
	 * driver in the stream.  Otherwise create the stream.
	 */
	if (stp) {
                UNLOCK_STRHEAD_MONP(sl);
		/*
		 * Waiting for stream to be created to device
		 * due to another open.
		 */
		if (stp->sd_flag & (STWOPEN|STRCLOSE)) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				STRHEAD_UNLOCK(stp);
				return (EAGAIN);
			}
			/*
			 * XXXrs - The structure of the original code assumes
			 *	   that v_stream might be changed/deleted
			 *	   during sleep and thus stp (and its monitor
			 *	   pointer) could be bogus.
			 */
			STRHEAD_UNLOCK(stp);
			if (sleep((caddr_t)&vp->v_stream, STOPRI)) {
				return (EINTR);
			}
			goto retry;  /* could be clone! */
		}

		if (stp->sd_flag & (STRDERR|STWRERR)) {
			STRHEAD_UNLOCK(stp);
			return (EIO);
		}
		stp->sd_flag |= STWOPEN;
		/*
		 * Open all modules and devices down stream to notify
		 * that another user is streaming.  For modules, set the
		 * last argument to MODOPEN and do not pass any open flags.
		 * Ignore dummydev since this is not the first open.
		 */
		qp = stp->sd_wrq;
		while (SAMESTR(qp)) { 
			qp = qp->q_next;
			if (qp->q_flag & QREADR)
				break;
			dummydev = *devp;
			if (error =
			    ((*RD(qp)->q_qinfo->qi_qopen) (RD(qp),
					    &dummydev, flag,
					    (qp->q_next ? MODOPEN : 0),
					     crp)))
				break;
		}

		/*
		 * Assign process group if controlling tty requested by
		 * driver/module.
		 *
		 * If FNOCTTY flag is on proc doesn't want it as controlling
		 * tty no matter who requested it downstream.
		 */
		if (error == 0 && !(flag & FNOCTTY))
			stralloctty(vp, stp);
		stp->sd_flag &= ~(STWOPEN|STRHUP);
		stp->sd_rerror = 0;
		stp->sd_werror = 0;
		if (vpp != NULL) {
			/* XXXrs - v_stream race?? */
			(*vpp)->v_stream = stp;
		}
		/*
		 * If there was an error and a stream is still
		 * associated with the vnode and this is the last
		 * reference to the stream, close it down.
		 */
		if (error) {
			LOCK_STRHEAD_MONP(s);
			stp = vp->v_stream;
			if (stp && vp->v_count == 1) {
				struct vnode *rvp;

				UNLOCK_STRHEAD_MONP(s);

				rvp = spec_find(vp->v_rdev, vp->v_type);

				if (rvp) {
					VN_RELE(rvp);
					if (rvp->v_count == 1) {
						STRHEAD_UNLOCK(stp);
						strclose(vp, flag, crp);
						wakeup((caddr_t)&vp->v_stream);
						return(error);
					}
				}
			} else {
				UNLOCK_STRHEAD_MONP(s);
			}
		}
		if (stp) {
			STRHEAD_UNLOCK(stp);
		} else {
			ASSERT(curthreadp->k_activemonpp);
			ASSERT(curthreadp->k_monitor == *curthreadp->k_activemonpp);
			vmonitor(curthreadp->k_monitor);
		}
		wakeup((caddr_t)&vp->v_stream);
		return (error);
	} else {
                UNLOCK_STRHEAD_MONP(sl);
	}

	/* 
	 * This vnode isn't streaming.  SPECFS already
	 * checked for multiple vnodes pointing to the
	 * same stream, so create a stream to the driver.
	 */

	if (!(qp = allocq())) {
		cmn_err(CE_CONT, "stropen: out of queues\n");
		return (ENOSR);
	}

	if ((stp = shalloc(qp)) == NULL) {
		freeq(qp);
		return (ENOSR);
	}

	/*
	 * We could have slept in the above allocations.
	 * Check vp->v_stream again to make sure nobody
	 * came in and opened the stream before us.
	 *
	 * This really isn't good enough.  Two racing opens could
	 * both see v_stream null since we don't block on the monp_lock.
	 * Need to hold the monp_lock until v_stream filled in?
	 */
	LOCK_STRHEAD_MONP(s2);
	if (vp->v_stream) {
		UNLOCK_STRHEAD_MONP(s2);
		shfree(stp);
		freeq(qp);
		goto retry;
	}
	
	/* 
	 * Initialize stream head.
	 */
	if (vp->v_type == VFIFO) {
		my_cdevsw = NULL;
		useprivmon = 1;
	} else {
		my_cdevsw = get_cdevsw(*devp);
		ASSERT(my_cdevsw != NULL);
		useprivmon = 0;

		if (my_cdevsw->d_str->st_rdinit->qi_qopen == clnopen) {
			struct cdevsw *cln_cdevsw;
	
			cln_cdevsw = clone_get_cdevsw(*devp);
			if (!cdvalid(cln_cdevsw)) {
				UNLOCK_STRHEAD_MONP(s2);
				shfree(stp);
				freeq(qp);
	               		return (ENXIO);
		  	}
		  	if ((stab = cln_cdevsw->d_str) &&
				stab->st_rdinit->qi_minfo->mi_locking == MULTI_THREADED) {
					useprivmon = 1;
		  		}
		} else {
			if (my_cdevsw->d_str->st_rdinit->qi_minfo->mi_locking ==
				MULTI_THREADED) {
					useprivmon = 1;
		  	}
		}
	}
	if (useprivmon) {
		monp = str_privmon_alloc();
		if (monp == NULL) {
			/* Use the global monitor. */
			str_globmon_attach(stp);
		} else {
			error = str_privmon_attach(stp, monp);
			ASSERT(!error);
		}
	} else {
		str_globmon_attach(stp);
	}

	stp->sd_flag = STWOPEN;
	stp->sd_siglist = NULL;
	stp->sd_sigflags = 0;
	stp->sd_mark = NULL;
	stp->sd_closetime = STRTIMOUT * HZ;
	stp->sd_vsession = NULL;
	stp->sd_pgid = 0;
	stp->sd_vnode = vp;
	stp->sd_rerror = 0;
	stp->sd_werror = 0;
	stp->sd_wroff = 0;
	stp->sd_iocwait = 0;
	stp->sd_iocblk = NULL;
	stp->sd_pushcnt = 0;
	stp->sd_hnext = NULL;
	qp->q_monpp = WR(qp)->q_monpp = &stp->sd_monp;
	qp->q_strh  = WR(qp)->q_strh = stp;
	qp->q_ptr = WR(qp)->q_ptr = (caddr_t)stp;
	vp->v_stream = stp;

	spinunlock_pmonitor(&strhead_monp_lock, s2, &stp->sd_monp,
			    PZERO, &stp->sd_monp);
	ASSERT(curthreadp->k_activemonpp);
	ASSERT(curthreadp->k_monitor == *curthreadp->k_activemonpp);

	setq(qp, &strdata, &stwdata);
	if (vp->v_type == VFIFO) {
		stp->sd_strtab = &fifoinfo;
		if (vpp != NULL) {
			/* XXXrs - v_stream race? */
			(*vpp)->v_stream = stp;
		}
		goto opendone;
	}

	/*
	 * Open driver and create stream to it (via qattach).
	 */
	stp->sd_strtab = my_cdevsw->d_str;
	dummydev = *devp;
	if (error = qattach(qp, devp, flag, CDEVSW, my_cdevsw, crp))
		goto opendone;

	/* XXXrs - v_stream race? */
	if (vpp != NULL)
		(*vpp)->v_stream = stp;
	if (vp->v_type == VCHR) {
		/*
		 * Update the stream table entry pointer in the case when
		 * you have a character device since a clone open or an open
		 * of an auto-registered loadable module under an I_LINK ioctl
		 * operation doesn't update the strtab address!
		 */
		my_cdevsw = get_cdevsw(*devp);
		ASSERT(my_cdevsw != NULL);
		stp->sd_strtab = my_cdevsw->d_str;
	}

	/*
	 * check for autopush
	 */
	ap = strphash(getemajor(*devp));
	while (ap) {
		if (ap->ap_major == getemajor(*devp)) {
			if (ap->ap_type == SAP_ALL)
				break;
			else if ((ap->ap_type == SAP_ONE) &&
				 (ap->ap_minor == geteminor(*devp)))
					break;
			else if ((ap->ap_type == SAP_RANGE) &&
				 (geteminor(*devp) >= ap->ap_minor) &&
				 (geteminor(*devp) <= ap->ap_lastminor))
					break;
		}
		ap = ap->ap_nextp;
	}
	if (ap == NULL)
		goto opendone;

	for (s = 0; s < ap->ap_npush; s++) {
		if (stp->sd_flag & (STRHUP|STRDERR|STWRERR)) {
			error = (stp->sd_flag & STRHUP) ? ENXIO : EIO;
			strclose(vp, flag, crp);
			freed++;
			break;
		}
		if (stp->sd_pushcnt >= nstrpush) {
			strclose(vp, flag, crp);
			freed++;
			error = EINVAL;
			break;
		}

		if (error = fmhold(ap->ap_list[s])) {
			strclose(vp, flag, crp);
			freed++;
			break;
		}

		/*
		 * Revert to the global monitor if the module does not
		 * support full MP-ization.
		 */
		if (fmodsw[ap->ap_list[s]].f_str->st_rdinit->
		     qi_minfo->mi_locking != MULTI_THREADED)
			useglobalmon(qp);
		/*
 		* push new module and call its open routine via qattach
 		*/
		if (error = qattach(qp, &dummydev, 0, FMODSW, 
				&fmodsw[ap->ap_list[s]], crp)) {
			strclose(vp, flag, crp);
			freed++;
			fmrele(ap->ap_list[s]);
			break;
		} else
			stp->sd_pushcnt++;
		fmrele(ap->ap_list[s]);
	} /* for */

opendone:

	if (error) {
		if (!freed) {
			LOCK_STRHEAD_MONP(s2);
			vp->v_stream = NULL;
			if (vpp != NULL)
				(*vpp)->v_stream = NULL;
			stp->sd_vnode = NULL;
			UNLOCK_STRHEAD_MONP(s2);
                        stp->sd_flag &= ~STWOPEN;

			LOCK_STRHEAD_MONP(s2);
			monp = str_mon_detach(stp);
			UNLOCK_STRHEAD_MONP(s2);

			shfree(stp);

			vmonitor(monp);
			str_mon_free(monp);

			freeq(qp);
			wakeup((caddr_t)&vp->v_stream);
			return(error);
		}

		return (error);
	}

	if (olddev != *devp) {

		/*
		 * 1st update the streams driver address
		 */
		my_cdevsw = get_cdevsw(*devp);
		ASSERT(my_cdevsw != NULL);

		stp->sd_strtab = my_cdevsw->d_str;

		/*
		 * The open cloned on us; when we return to specfs
		 * (which is the only caller truly prepared for
		 * "cloning"), the modified "*devp" will be the trigger
		 * for specfs to clean-up and release the current
		 * vnode/snode pair, and to create a "new" set.
		 *
		 * We'll use the v_stream field of the current vnode
		 * to let specfs know what the new stream pointer is.
		 * It's up to specfs to manage the locking on the
		 * current and new vnode/snode pairs to prevent any
		 * "inadvertent" shuffling due to open/vn_kill races.
		 */
		LOCK_STRHEAD_MONP(s2);
		vp->v_stream     = NULL;
		(*vpp)->v_stream = stp;
		UNLOCK_STRHEAD_MONP(s2);

#ifdef	CELL_IRIX
		/*
		 * In the CELL_IRIX case, we need to return to specfs with the
		 * stream head unlocked (with STWOPEN still set), as the call
		 * to stropen_clone_finish() will happen on a different msg
		 * thread, after specfs has had a chance to react to the
		 * cloning on its side.  The stream is not quite completely
		 * open, the STWOPEN will be cleared in stropen_clone_finish().
		 */
		STRHEAD_UNLOCK(stp);

#else	/* ! CELL_IRIX */
		/*
		 * Return to specfs with the stream head still locked
		 * & not quite completely open.  Specfs will call us
		 * back at stropen_clone_finish() after it's had a
		 * chance to react to the cloning on its side.
		 */
#endif	/* ! CELL_IRIX */
		return 0;
	}

	/*
	 * Assign process group if controlling tty, unless FNOCTTY
	 * was specified.
	 */
	if (!(flag & FNOCTTY))
		stralloctty(stp->sd_vnode, stp);
	/*
	 * Wake up others that are waiting for stream to be created.
	 */
	stp->sd_flag &= ~STWOPEN;

	wakeup((caddr_t)&vp->v_stream);
	STRHEAD_UNLOCK(stp);

	return (0);
}

void
stropen_clone_finish(struct vnode *ovp, struct stdata *stp, int flag)
{

#ifdef	CELL_IRIX
	{
		vnode_t *vp;
		/* REFERENCED */
		struct stdata *nstp;

		/*
		 * Reobtain the stream head lock.
		 * Theory says nobody else was able to get past the STWOPEN
		 * flag.
		 */
		vp = stp->sd_vnode;
		ASSERT(stp == vp->v_stream);
		STRHEAD_LOCK(&vp->v_stream, nstp);
		ASSERT(nstp == stp);
	}
#endif	/* CELL_IRIX */

	/*
	 * Assign process group if controlling tty, unless FNOCTTY
	 * was specified.
	 */
	if (!(flag & FNOCTTY)) {
		stralloctty(stp->sd_vnode, stp);
	}
	/*
	 * Wake up others that are waiting for stream to be created.
	 */
	stp->sd_flag &= ~STWOPEN;
	wakeup((caddr_t)&ovp->v_stream);

	STRHEAD_UNLOCK(stp);
}

/*
 * Close a stream.  
 * This is called from vfile_close() on the last close of an open stream.
 * Strclean() will already have removed the siglist and pollist
 * information, so all that remains is to remove all multiplexor links
 * for the stream, pop all the modules (and the driver), and free the
 * stream structure.
 */
int
strclose(struct vnode *vp, int flag, cred_t *crp)
{
	register int s2;
	register struct stdata *stp;
	register queue_t *qp;
	mblk_t *mp;
	int id;
	int rval;
	vsession_t *vsp;
	mon_t *monp;
	int freerd = 0, freenext = 0;
	queue_t *rq;
	qband_t *qbp;

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);
	ASSERT(stp);

	qp = stp->sd_wrq;
	/*
	 * if the stream is for a controlling terminal,
	 * detach that controlling terminal from the session.
	 */
	if ((vsp = stp->sd_vsession) != NULL) {
		VSESSION_HOLD(vsp);
		STRHEAD_UNLOCK(stp);
		VSESSION_CTTY_DEALLOC(vsp, SESSTTY_CLOSE);
		VSESSION_RELE(vsp);
		STRHEAD_LOCK(&vp->v_stream, stp);
	}

	ASSERT(stp);
	ASSERT(STREAM_LOCKED(qp));

	stp->sd_flag |= STRCLOSE;

	if (mp = qp->q_first)		/* message "held" by strwrite */
		qp->q_first = NULL;

	LOCK_STR_RESOURCE(s2);
	if (qp->q_flag & QHLIST) {
		register struct stdata *sp = strholdhead;
		register struct stdata **prevp = &strholdhead;

		/*
		 * Remove this stream from the list of streams with
		 * held messages.
		 */
		while (sp) {
			if (sp == stp) {
				*prevp = sp->sd_hnext;
				qp->q_flag &= ~QHLIST;
			}
			else
				prevp = &sp->sd_hnext;
			sp = sp->sd_hnext;
		}
		strholdtailp = prevp;

		ASSERT(!(qp->q_flag & QHLIST));
	}
	UNLOCK_STR_RESOURCE(s2);

	if (mp)  {
		if (stp->sd_flag & (STRHUP|STWRERR))
			freemsg(mp);
		else
			putnext(qp, mp);
	}

	stp->sd_flag &= ~(STRDERR|STWRERR);	/* help unlink succeed */
	stp->sd_rerror = 0;
	stp->sd_werror = 0;

	(void) munlinkall(&vp->v_stream, LINKCLOSE|LINKNORMAL, crp, &rval);

	while (SAMESTR(qp)) {
		if (!(flag & (FNDELAY|FNONBLOCK)) && (stp->sd_closetime > 0)) {
			stp->sd_flag |= STRTIME;

			LOCK_STR_RESOURCE(s2);
			stp->sd_pflag |= WSLEEP;
			UNLOCK_STR_RESOURCE(s2);

			id = MP_STREAMS_TIMEOUT(qp->q_monpp, strtime, stp,
						stp->sd_closetime);
			/*
			 * sleep until awakened by strwsrv() or strtime() 
			 */
			while ((stp->sd_flag & STRTIME) &&
			       qp->q_next->q_first) {

				LOCK_STR_RESOURCE(s2);
				stp->sd_pflag |= WSLEEP;
				UNLOCK_STR_RESOURCE(s2);

				/* ensure strwsrv gets enabled */
				qp->q_next->q_flag |= QWANTW;
				if (sleep((caddr_t)qp, STIPRI))
					break;
			}
			untimeout(id);
			stp->sd_flag &= ~STRTIME;

			LOCK_STR_RESOURCE(s2);
			stp->sd_pflag &= ~WSLEEP;
			UNLOCK_STR_RESOURCE(s2);
		}
		qdetach(RD(qp->q_next), 1, flag, crp);
	}

	rq = RD(qp);
	if (qp->q_next && (qp->q_next == rq)) {
		/* FIFO's evil */
		rq->q_flag &= ~QWANTW;
		for(qbp = rq->q_bandp; qbp; qbp = qbp->qb_next)
			qbp->qb_flag &= ~QB_WANTW;
	}

	flushq(qp, FLUSHALL);
	for (mp = RD(qp)->q_first; mp; mp = mp->b_next) {
		if (mp->b_datap->db_type == M_PASSFP) {
			/*
			 * Release streams monitor lock to avoid a
			 * deadlock with the close code, which also
			 * needs to monitor lock. What a kludge!
			 */
			vfile_close(((struct strrecvfd *)mp->b_rptr)->f.fp);
		}
	}
	flushq(RD(qp), FLUSHALL);

	LOCK_STR_RESOURCE(s2);
	if (qp->q_pflag & QENAB) {
		register queue_t *q;
		register queue_t **prevp;

		q = qhead;
		prevp = &qhead;
		while (q) {
			if (q == qp)
				*prevp = q->q_link;
			else
				prevp = &q->q_link;
			q = q->q_link;
		}
		qtailp = prevp;
	}
	UNLOCK_STR_RESOURCE(s2);

	/*
	 * If the write queue of the stream head is pointing to a
	 * read queue, we have a twisted stream.  If the read queue
	 * is alive, convert the stream head queues into a dead end.
	 * If the read queue is dead, free the dead pair.
	 */
	if (qp->q_next && !SAMESTR(qp)) {
		if (qp->q_next->q_qinfo == &deadrend) {	/* half-closed pipe */
			freerd = 1;
			freenext = 1;
		} else if (qp->q_next == RD(qp)) {	/* fifo */
			freerd = 1;
		} else {				/* pipe */
			qp->q_qinfo = &deadwend;
			RD(qp)->q_qinfo = &deadrend;
		}
	} else {
		freerd = 1;
	}

	if (stp->sd_iocblk) {
		freemsg(stp->sd_iocblk);
		stp->sd_iocblk = NULL;
	}

	/*
	 * remove any pending qenables on stream head queues about to be
	 * freed.
	 */
	streams_mpdetach(RD(qp));

	/* vmonitor, before shfree(stp) is called */
	STRHEAD_UNLOCK(stp);

	/*
	 * now that any pending queue ops may have run against this queue,
	 * free it
	 */
	if (freenext) {
		freeq(qp->q_next);
	}
	if (freerd) {
		freeq(RD(qp));
	}

	LOCK_STRHEAD_MONP(s2);
	stp->sd_vnode = NULL;
	vp->v_stream = NULL;
	stp->sd_flag &= ~STRCLOSE;

	monp = str_mon_detach(stp);
	UNLOCK_STRHEAD_MONP(s2);

	wakeup((caddr_t)stp);
	shfree(stp);

	str_mon_free(monp);
	return (0);
}

/*
 * Clean up after a process when it closes a stream.  This is called
 * from vfile_close for all closes, whereas strclose is called only for the 
 * last close on a stream.  The pollist, siglist, and eventlist are
 * scanned for entries for the current process, and these are removed.
 */
void
strclean(struct stdata *stp)
{
	register struct strevent *sep, *psep, *tsep;
	register int s;
	int update = 0;

	/* XXXrs - no monitor required?!? */
	ASSERT(stp);

	psep = NULL;
	LOCK_STR_RESOURCE(s);
	sep = stp->sd_siglist;
	while (sep) {
		if (sep->se_pid == current_pid()) {
			tsep = sep->se_next;
			if (psep)
				psep->se_next = tsep;
			else
				stp->sd_siglist = tsep;
			sefree(sep);
			update++;
			sep = tsep;
		} else {
			psep = sep;
			sep = sep->se_next;
		}
	}
	if (update) {
		stp->sd_sigflags = 0;
		for (sep = stp->sd_siglist; sep; sep = sep->se_next)
			stp->sd_sigflags |= sep->se_events;
		update = 0;
	}
	UNLOCK_STR_RESOURCE(s);
}

/*
 * Read a stream according to the mode flags in sd_flag:
 *
 * (default mode)              - Byte stream, msg boundries are ignored
 * RMSGDIS (msg discard)       - Read on msg boundries and throw away 
 *                               any data remaining in msg
 * RMSGNODIS (msg non-discard) - Read on msg boundries and put back
 *		                 any remaining data on head of read queue
 *
 * Consume readable messages on the front of the queue until uio
 * is satisfied, the readable messages are exhausted, or a message
 * boundary is reached in a message mode.  If no data was read and
 * the stream was not opened with the NDELAY flag, block until data arrives.
 * Otherwise return the data read and update the count.
 *
 * In default mode a 0 length message signifies end-of-file and terminates
 * a read in progress.  The 0 length message is removed from the queue
 * only if it is the only message read (no data is read).
 *
 * An attempt to read an M_PROTO or M_PCPROTO message results in an 
 * EBADMSG error return, unless either RDPROTDAT or RDPROTDIS are set.
 * If RDPROTDAT is set, M_PROTO and M_PCPROTO messages are read as data.
 * If RDPROTDIS is set, the M_PROTO and M_PCPROTO parts of the message
 * are unlinked from and M_DATA blocks in the message, the protos are
 * thrown away, and the data is read.
 */
/* ARGSUSED */
int
strread(struct vnode *vp, struct uio *uiop, cred_t *crp)
{
	register struct stdata *stp;
	register mblk_t *bp, *nbp;
	int n;
	int done = 0;
	int error = 0;
	char rflg;
	short mark;
	short delim;
	unsigned char pri;
#ifndef _IRIX_LATER
	char wait_flg;
#endif /* !_IRIX_LATER */

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);
	ASSERT(stp);

	if (stp->sd_flag & (STRDERR|STPLEX)) {
		STRHEAD_UNLOCK(stp);
		return ((stp->sd_flag & STPLEX) ? EINVAL : stp->sd_rerror);
	}

	/*
	 * Loop terminates when uiop->uio_resid == 0.
	 */
	rflg = 0;
	for (;;) {
		mark = 0;
		delim = 0;
#ifndef _IRIX_LATER
		wait_flg = 0;
#endif /* !_IRIX_LATER */
		for(;;) {
			/*
			 * job control
			 *
			 * Block the process if it's in background and it's
			 * trying to read its controlling terminal.
			 */
			if (inbackground(stp))
			{
				STRHEAD_UNLOCK(stp); /* XXXrs why? */

				if (error = accessctty(JCTLREAD))
					return (error);

				STRHEAD_LOCK(&vp->v_stream, stp);
				ASSERT(stp);

				continue;
			}
			if (bp = getq(RD(stp->sd_wrq)))
				break;
			if (stp->sd_flag & STRHUP) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			if (rflg && !(stp->sd_flag & STRDELIM)) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			if (stp->sd_vtime < 0 || wait_flg != 0) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}

			/*
			 * if FIFO/pipe, don't sleep here. Sleep in the
			 * fifo read routine.
			 */
			if (vp->v_type == VFIFO) {
				STRHEAD_UNLOCK(stp);
				return (ESTRPIPE);
			}

			if ((error = strwaitq(stp, READWAIT, uiop->uio_resid,
					      uiop->uio_fmode,
					      stp->sd_vtime,
					      &done))
			    || done) {
				if ((uiop->uio_fmode & FNDELAY) &&
				    (stp->sd_flag & OLDNDELAY) &&
				    (error == EAGAIN))
					error = 0;
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			if (stp->sd_vtime != 0)
				wait_flg++;
		}
		/* splstr still held here */
		if (stp->sd_mark == bp) {
			if (rflg) {
				putbq(RD(stp->sd_wrq), bp);
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mark = 1;
			stp->sd_mark = NULL;
		}
		if ((stp->sd_flag & STRDELIM) && (bp->b_flag & MSGDELIM))
			delim = 1;
		pri = bp->b_band;
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */

		switch (bp->b_datap->db_type) {

		case M_DATA:
ismdata:
			if (msgdsize(bp) == 0) {
				if (mark || delim) {
					freemsg(bp);
				} else if (rflg) {

					/*
					 * If already read data put zero
					 * length message back on queue else
					 * free msg and return 0.
					 */
					bp->b_band = pri;
					putbq(RD(stp->sd_wrq), bp);
				} else {
					freemsg(bp);
				}
				STRHEAD_UNLOCK(stp);
				return (0);
			}

			rflg = 1;
			while (bp && uiop->uio_resid) {
				if (n = MIN(uiop->uio_resid, bp->b_wptr - bp->b_rptr))
					error = STREAMS_UIOMOVE(
							(caddr_t)bp->b_rptr,
							n, UIO_READ, uiop);
			
				if (error) {
					freemsg(bp);
					STRHEAD_UNLOCK(stp);
					return (error);
				}

				bp->b_rptr += n;
				while (bp && (bp->b_rptr >= bp->b_wptr)) {
					nbp = bp;
					bp = bp->b_cont;
					freeb(nbp);
				}
			}
	
			/*
			 * The data may have been the leftover of a PCPROTO, so
			 * if none is left reset the STRPRI flag just in case.
			 */
			if (bp) {
				/* 
				 * Have remaining data in message.
				 * Free msg if in discard mode.
				 */
				if (stp->sd_flag & RMSGDIS) {
					freemsg(bp);
					stp->sd_flag &= ~STRPRI;
				} else {
					bp->b_band = pri;
					if (mark && !stp->sd_mark) {
						stp->sd_mark = bp;
						bp->b_flag |= MSGMARK;
					}
					if (delim)
						bp->b_flag |= MSGDELIM;
					putbq(RD(stp->sd_wrq),bp);
				}
			} else {
				stp->sd_flag &= ~STRPRI;
			}
	
			/*
			 * Check for signal messages at the front of the read
			 * queue and generate the signal(s) if appropriate.
			 * The only signal that can be on queue is M_SIG at
			 * this point.
			 */
			while (((bp = RD(stp->sd_wrq)->q_first)) &&
				(bp->b_datap->db_type == M_SIG)) {
				bp = getq(RD(stp->sd_wrq));
				strsignal(stp, *bp->b_rptr, (long)bp->b_band);
				freemsg(bp);
#ifdef _EAGER_RUNQUEUES
				if (qready())
					runqueues();
#endif /* _EAGER_RUNQUEUES */
			}
	
			if ((uiop->uio_resid == 0) || mark || delim ||
			    (stp->sd_flag & (RMSGDIS|RMSGNODIS))) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			continue;
		
		case M_PROTO:
		case M_PCPROTO:
			/*
			 * Only data messages are readable.  
			 * Any others generate an error, unless
			 * RDPROTDIS or RDPROTDAT is set.
			 */
			if (stp->sd_flag & RDPROTDAT) {
				for (nbp = bp; nbp; nbp = bp->b_next)
					nbp->b_datap->db_type = M_DATA;

				stp->sd_flag &= ~STRPRI;
				goto ismdata;
			} else if (stp->sd_flag & RDPROTDIS) {
				while (bp &&
				    ((bp->b_datap->db_type == M_PROTO) ||
				    (bp->b_datap->db_type == M_PCPROTO))) {
					nbp = unlinkb(bp);
					freeb(bp);
					bp = nbp;
				}
				stp->sd_flag &= ~STRPRI;
				if (bp) {
					bp->b_band = pri;
					goto ismdata;
				} else {
					break;
				}
			}
			/* fall through */

		case M_PASSFP:
			if ((bp->b_datap->db_type == M_PASSFP) &&
			    (stp->sd_flag & RDPROTDIS)) {
				/*
				 * Release the streams monitor to avoid a
				 * deadlock with the close code which also
				 * must acquire the monitor lock.
				 * What a kludge!
				 */
				STRHEAD_UNLOCK(stp);
				vfile_close(((struct strrecvfd *)bp->b_rptr)->f.fp);
				STRHEAD_LOCK(&vp->v_stream, stp);
				ASSERT(stp);
				freemsg(bp);
				break;
			}
			putbq(RD(stp->sd_wrq), bp);
			STRHEAD_UNLOCK(stp);
			return (EBADMSG);

		default:
			/*
			 * Garbage on stream head read queue.
			 */
			ASSERT(0);
			freemsg(bp);
			break;
		}
	}

	/* NOTREACHED */
}

/* XXXbe do this once, in-line, at the top of strrput? */
/* XXXrs - return 0 unless was a VFIFO and we didn't wake anyone! */
#define MAXWAKEQLOOP	1000
int strwakeq_giveup = 0;

int
strwakeq(struct stdata *stp, queue_t *q, int flag)
{
	vnode_t *vp;
	sv_t *svp;
	fifonode_t *fnp;
	bhv_desc_t *bdp;

	vp = stp->sd_vnode;
	if (vp->v_type == VFIFO) {
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &fifo_vnodeops);
		ASSERT(bdp != NULL);
		fnp = BHVTOF(bdp);
		svp = (flag & READWAIT || flag & GETWAIT) ?
		     &(fnp->fn_empty) : &(fnp->fn_full);
/* XXXbe need to interlock with fifo_read! */
		sv_broadcast(svp);
	} else {
		wakeup(q);
	}
	return(0);
}

/*
 * Stream read put procedure.  Called from downstream driver/module
 * with messages for the stream head.  Data, protocol, and in-stream
 * signal messages are placed on the queue, others are handled directly.
 */

static int
strrput(register queue_t *q, register mblk_t *bp)
{
	register struct stdata *stp;
	register struct iocblk *iocbp;
	int s2, cnt;
	struct stroptions *sop;
	struct copyreq *reqp;
	struct copyresp *resp;
	unsigned char bpri;
	qband_t *qbp;
	unsigned char qbf[NBAND];

	stp = (struct stdata *)q->q_ptr;

	ASSERT(stp);
	ASSERT(!(stp->sd_flag & STPLEX));
	ASSERT(STREAM_LOCKED(q));

	switch (bp->b_datap->db_type) {

	case M_DATA:
	case M_PROTO:
	case M_PCPROTO:
	case M_PASSFP:
		if (bp->b_datap->db_type == M_PCPROTO) {
			/*
			 * Only one priority protocol message is allowed at the
			 * stream head at a time.
			 */
			if (stp->sd_flag & STRPRI) {
				freemsg(bp);
				return 0;
			}
		}

		/*
		 * Marking doesn't work well when messages
		 * are marked in more than one band.  We only
		 * remember the last message received, even if
		 * it is placed on the queue ahead of other
		 * marked messages.
		 */
		if (bp->b_flag & MSGMARK)
			stp->sd_mark = bp;

		/* 
		 * Wake sleeping read/getmsg
		 */
		LOCK_STR_RESOURCE(s2);

		if (stp->sd_pflag & RSLEEP) {
		/*
		 * XXXrs - let fifo code clear the flag when it's
		 * finally awake.  This is a REALLY BAD HACK to address the
		 * race where we see the flag set here, but do the vsema on
		 * the empty semaphore before the fifo code is actually on
		 * the empty semaphore.  There's no way to interlock yet, so
		 * the hope is that if we race and miss on the first message,
		 * the next one will get things flowing again.  This presumes
		 * the stream is not filled by a single message.
			stp->sd_pflag &= ~RSLEEP;
		 */
			cnt = 0;
			UNLOCK_STR_RESOURCE(s2);

			/*
			 * XXXrs - more ugly hacking.  Retry if nobody awakened
			 */
			while (strwakeq(stp, q, READWAIT)) {
				cnt++;
				if (cnt == MAXWAKEQLOOP) {
					strwakeq_giveup++;
					break;
				}
			}
		} else {
			UNLOCK_STR_RESOURCE(s2);
		}
		putq(q, bp);

		if (bp->b_datap->db_type == M_PCPROTO) {
			stp->sd_flag |= STRPRI;
			if (stp->sd_sigflags & S_HIPRI)
				strsendsig(stp->sd_siglist, S_HIPRI, 0L);
			pollwakeup(&stp->sd_pollist, POLLPRI);
		} else if (q->q_first == bp) {
			if (stp->sd_sigflags & S_INPUT)
				strsendsig(stp->sd_siglist, S_INPUT,
				    (long)bp->b_band);
			pollwakeup(&stp->sd_pollist, POLLIN);
			if (bp->b_band == 0) {
			    if (stp->sd_sigflags & S_RDNORM)
				    strsendsig(stp->sd_siglist, S_RDNORM, 0L);
			    pollwakeup(&stp->sd_pollist, POLLRDNORM);
			} else {
			    if (stp->sd_sigflags & S_RDBAND)
				    strsendsig(stp->sd_siglist, S_RDBAND,
					(long)bp->b_band);
			    pollwakeup(&stp->sd_pollist, POLLRDBAND);
			}
                } else {
                        pollwakeup(&stp->sd_pollist,
                                        POLLIN | POLLRDNORM | POLLRDBAND);
                }
		return 0;

	case M_ERROR:
		/* 
		 * An error has occured downstream, the errno is in the first
		 * byte of the message.
		 */
		if ((bp->b_wptr - bp->b_rptr) == 2) {	/* New flavor */
			unsigned char rw = 0;

			if (*bp->b_rptr != NOERROR) {	/* read error */
				if (*bp->b_rptr != 0) {
					stp->sd_flag |= STRDERR;
					rw |= FLUSHR;
				} else {
					stp->sd_flag &= ~STRDERR;
				}
				stp->sd_rerror = *bp->b_rptr;
			}
			bp->b_rptr++;
			if (*bp->b_rptr != NOERROR) {	/* write error */
				if (*bp->b_rptr != 0) {
					stp->sd_flag |= STWRERR;
					rw |= FLUSHW;
				} else {
					stp->sd_flag &= ~STWRERR;
				}
				stp->sd_werror = *bp->b_rptr;
			}
			if (rw) {
				strwakeq(stp, q, READWAIT);	/* readers */
				strwakeq(stp, WR(q), WRITEWAIT);/* writers */
				wakeup(stp);			/* ioctlers */

				if (stp->sd_sigflags & S_ERROR) 
					strsendsig(stp->sd_siglist, S_ERROR,
					    ((rw & FLUSHR) ?
					    (long)stp->sd_rerror :
					    (long)stp->sd_werror));
				pollwakeup(&stp->sd_pollist, POLLERR);

				freemsg(bp);
				(void) putctl1(WR(q)->q_next, M_FLUSH, rw);
				return 0;
			}
		} else if (*bp->b_rptr != 0) {		/* Old flavor */

			stp->sd_flag |= (STRDERR|STWRERR);
			stp->sd_rerror = *bp->b_rptr;
			stp->sd_werror = *bp->b_rptr;

			strwakeq(stp, q, READWAIT);	/* the readers */
			strwakeq(stp, WR(q), WRITEWAIT);/* the writers */
			wakeup(stp);			/* the ioctlers */

			if (stp->sd_sigflags & S_ERROR) 
				strsendsig(stp->sd_siglist, S_ERROR,
				    (stp->sd_werror ? (long)stp->sd_werror :
				    (long)stp->sd_rerror));
			pollwakeup(&stp->sd_pollist, POLLERR);

			freemsg(bp);
			(void) putctl1(WR(q)->q_next, M_FLUSH, FLUSHRW);
			return 0;
		}
		freemsg(bp);
		return 0;

	case M_HANGUP:
	
		freemsg(bp);

		stp->sd_werror = ENXIO;
		stp->sd_flag |= STRHUP;
		/*
		 * send signal if controlling tty
		 */

                if (stp->sd_vsession)
                        signal(stp->sd_vsession->vs_sid, SIGHUP);

		strwakeq(stp, q, READWAIT);	/* the readers */
		strwakeq(stp, WR(q), WRITEWAIT);/* the writers */
		wakeup(stp);			/* the ioctlers */

		/*
		 * wake up read, write, and exception pollers and
		 * reset wakeup mechanism.
		 */

		strhup(stp);
		return 0;


	case M_SIG:
		/*
		 * Someone downstream wants to post a signal.  The
		 * signal to post is contained in the first byte of the
		 * message.  If the message would go on the front of
		 * the queue, send a signal to the process group
		 * (if not SIGPOLL) or to the siglist processes
		 * (SIGPOLL).  If something is already on the queue,
		 * just enqueue the message.
		 */
		if (q->q_first) {
			putq(q, bp);
			return 0;
		}
		/* flow through */

	case M_PCSIG:
		/*
		 * Don't enqueue, just post the signal.
		 */
		strsignal(stp, *bp->b_rptr, 0L);
		freemsg(bp);
		return 0;

	case M_FLUSH:
		/*
		 * Flush queues.  The indication of which queues to flush
		 * is in the first byte of the message.  If the read queue 
		 * is specified, then flush it.  If FLUSHBAND is set, just
		 * flush the band specified by the second byte of the message.
		 */
	    {
		register mblk_t *mp;
		register mblk_t *nmp;
		mblk_t *last;
		int backenable = 0;
		int band;
		queue_t *nq;
		unsigned char pri, i;

		if (*bp->b_rptr & FLUSHR) {
		    if (*bp->b_rptr & FLUSHBAND) {
			ASSERT((bp->b_wptr - bp->b_rptr) >= 2);
			pri = *(bp->b_rptr + 1);
			if (pri > q->q_nband) {
			    goto wrapflush;
			}
			if (pri == 0) {
			    mp = q->q_first;
			    q->q_first = NULL;
			    q->q_last = NULL;
#ifdef _STRQ_TRACING
			if (q->q_flag & QTRC)
				q_trace_zero(q, (inst_t *)__return_address);
#endif
			    q->q_count = 0;
			    for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
				qbp->qb_first = NULL;
				qbp->qb_last = NULL;
				qbp->qb_count = 0;
				qbp->qb_flag &= ~QB_FULL;
			    }
			    q->q_blocked = 0;
			    q->q_flag &= ~QFULL;
			    while (mp) {
				nmp = mp->b_next;
				if (mp->b_band == 0) {
				    if (mp->b_datap->db_type == M_PASSFP) 
					vfile_close(((struct strrecvfd *)
					  mp->b_rptr)->f.fp);
				    freemsg(mp);
				} else {
					putq(q, mp);
				}
				mp = nmp;
			    }
			    if ((q->q_flag & QWANTW) &&
			      (q->q_count <= q->q_lowat)) {
				/* find nearest back queue with service proc */
				q->q_flag &= ~QWANTW;
				for (nq = backq(q); nq && !nq->q_qinfo->qi_srvp;
				  nq = backq(nq))
				    ;
				if (nq) {
				    qenable(nq);
				    setqback(nq, 0);
				}
			    }
			} else {	/* pri != 0 */
			    band = pri;
			    qbp = q->q_bandp;
			    while (--band > 0)
				qbp = qbp->qb_next;
			    mp = qbp->qb_first;
			    if (mp == NULL)
				goto wrapflush;
			    last = qbp->qb_last;
			    if (mp == last)	/* only message in band */
				last = mp->b_next;
			    while (mp != last) {
				nmp = mp->b_next;
				if (mp->b_band == pri) {
				    if (mp->b_datap->db_type == M_PASSFP) 
					vfile_close(((struct strrecvfd *)
					  mp->b_rptr)->f.fp);
				    rmvq(q, mp);
				    freemsg(mp);
				}
				mp = nmp;
			    }
			    if (mp && mp->b_band == pri) {
				if (mp->b_datap->db_type == M_PASSFP) 
				    vfile_close(((struct strrecvfd *)
				      mp->b_rptr)->f.fp);
				rmvq(q, mp);
				freemsg(mp);
			    }
			}
		    } else {	/* flush entire queue */
			mp = q->q_first;
			q->q_first = NULL;
			q->q_last = NULL;
			q->q_count = 0;
#ifdef _STRQ_TRACING
			if (q->q_flag & QTRC)
				q_trace_zero(q, (inst_t *)__return_address);
#endif
			stp->sd_flag &= ~STRPRI;
			for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
			    qbp->qb_first = NULL;
			    qbp->qb_last = NULL;
			    qbp->qb_count = 0;
			    qbp->qb_flag &= ~QB_FULL;
			}
			q->q_blocked = 0;
			q->q_flag &= ~QFULL;
			while (mp) {
			    nmp = mp->b_next;
			    if (mp->b_datap->db_type == M_PASSFP) 
				vfile_close(((struct strrecvfd *)mp->b_rptr)->f.fp);
			    freemsg(mp);
			    mp = nmp;
			}
			bzero((caddr_t)qbf, NBAND);
			bpri = 1;
			for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
			    if ((qbp->qb_flag & QB_WANTW) &&
			      (qbp->qb_count <= qbp->qb_lowat)) {
				qbp->qb_flag &= ~QB_WANTW;
				backenable = 1;
				qbf[bpri] = 1;
			    }
			    bpri++;
			}
			if ((q->q_flag & QWANTW) && (q->q_count <= q->q_lowat)) {
			    q->q_flag &= ~QWANTW;
			    backenable = 1;
			    qbf[0] = 1;
			}

			/*
			 * If any band can now be written to, and there is a
			 * writer for that band, then backenable the closest
			 * service procedure.
			 */
			if (backenable) {
			    /* find nearest back queue with service proc */
			    for (nq = backq(q); nq && !nq->q_qinfo->qi_srvp;
			      nq = backq(nq))
				;
			    if (nq) {
				qenable(nq);
				for (i = 0; i < bpri; i++)
				    if (qbf[i])
					setqback(nq, i);
			    }
			}
		    }
		}
wrapflush:
		if ((*bp->b_rptr & FLUSHW) && !(bp->b_flag & MSGNOLOOP)) {
			*bp->b_rptr &= ~FLUSHR;
			bp->b_flag |= MSGNOLOOP;
			qreply(q, bp);
			return 0;
		}
		freemsg(bp);
		return 0;
	    }

	case M_IOCACK:
	case M_IOCNAK:
		iocbp = (struct iocblk *)bp->b_rptr;
		/*
		 * If not waiting for ACK or NAK then just free msg.
		 * If already have ACK or NAK for user then just free msg.
		 * If incorrect id sequence number then just free msg.
		 */
		if ((stp->sd_flag & IOCWAIT) == 0 || stp->sd_iocblk ||
		    (stp->sd_iocid != iocbp->ioc_id)) {
			freemsg(bp);
			return 0;
		}

		/*
		 * Assign ACK or NAK to user and wake up.
		 */
		stp->sd_iocblk = bp;

		wakeup(stp);
		return 0;

	case M_COPYIN:
	case M_COPYOUT:
		reqp = (struct copyreq *)bp->b_rptr;

		/*
		 * If not waiting for ACK or NAK then just fail request.
		 * If already have ACK, NAK, or copy request, then just
		 * fail request.
		 * If incorrect id sequence number then just fail request.
		 */
		if ((stp->sd_flag & IOCWAIT) == 0 || stp->sd_iocblk ||
		    (stp->sd_iocid != reqp->cq_id)) {
			if (bp->b_cont) {
				freemsg(bp->b_cont);
				bp->b_cont = NULL;
			}
			bp->b_datap->db_type = M_IOCDATA;
			resp = (struct copyresp *)bp->b_rptr;
			resp->cp_rval = (caddr_t)1L;	/* failure */

			STREAMS_BLOCKQ();
			(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, bp);
			STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
			if (qready())
				runqueues();
#endif /* _EAGER_RUNQUEUES */
			return 0;
		}

		/*
		 * Assign copy request to user and wake up.
		 */
		stp->sd_iocblk = bp;
		wakeup(stp);
		return 0;

	case M_SETOPTS:
		/*
		 * Set stream head options (read option, write offset,
		 * min/max packet size, and/or high/low water marks for 
		 * the read side only).
		 */

		bpri = 0;
		ASSERT((bp->b_wptr - bp->b_rptr) == sizeof(struct stroptions));
		sop = (struct stroptions *)bp->b_rptr;

		if (sop->so_flags & SO_READOPT) {
			switch (sop->so_readopt & RMODEMASK) {
			case RNORM: 
				stp->sd_flag &= ~(RMSGDIS | RMSGNODIS);
				break;

			case RMSGD:
				stp->sd_flag = ((stp->sd_flag & ~RMSGNODIS) |
				    RMSGDIS);
				break;

			case RMSGN:
				stp->sd_flag = ((stp->sd_flag & ~RMSGDIS) |
				    RMSGNODIS);
				break;
			}
			switch(sop->so_readopt & RPROTMASK) {
			case RPROTNORM:
				stp->sd_flag &= ~(RDPROTDAT | RDPROTDIS);
				break;

			case RPROTDAT:
				stp->sd_flag = ((stp->sd_flag & ~RDPROTDIS) |
				    RDPROTDAT);
				break;

			case RPROTDIS:
				stp->sd_flag = ((stp->sd_flag & ~RDPROTDAT) |
				    RDPROTDIS);
				break;
			}
		}
				
		if (sop->so_flags & SO_WROFF)
			stp->sd_wroff = sop->so_wroff;
		if (sop->so_flags & SO_MINPSZ)
			q->q_minpsz = sop->so_minpsz;
		if (sop->so_flags & SO_MAXPSZ)
			q->q_maxpsz = sop->so_maxpsz;
		if (sop->so_flags & SO_HIWAT) {
		    if (sop->so_flags & SO_BAND) {
			if (strqset(q, QHIWAT, sop->so_band, sop->so_hiwat))
				cmn_err_tag(156,CE_WARN,
				    "strrput: could not allocate qband\n");
			else
				bpri = sop->so_band;
		    } else {
			q->q_hiwat = sop->so_hiwat;
		    }
		}
		if (sop->so_flags & SO_LOWAT) {
		    if (sop->so_flags & SO_BAND) {
			if (strqset(q, QLOWAT, sop->so_band, sop->so_lowat))
				cmn_err_tag(157,CE_WARN,
				    "strrput: could not allocate qband\n");
			else
				bpri = sop->so_band;
		    } else {
			q->q_lowat = sop->so_lowat;
		    }
		}
		if (sop->so_flags & SO_MREADON)
			stp->sd_flag |= SNDMREAD;
		if (sop->so_flags & SO_MREADOFF)
			stp->sd_flag &= ~SNDMREAD;
		if (sop->so_flags & SO_NDELON)
			stp->sd_flag |= OLDNDELAY;
		if (sop->so_flags & SO_NDELOFF)
			stp->sd_flag &= ~OLDNDELAY;
		if (sop->so_flags & SO_ISTTY)
			stp->sd_flag |= STRISTTY;
		if (sop->so_flags & SO_ISNTTY)
			stp->sd_flag &= ~STRISTTY;
#ifdef _IRIX_LATER
		if (sop->so_flags & SO_TOSTOP) 
			stp->sd_flag |= STRTOSTOP;
		if (sop->so_flags & SO_TONSTOP) 
			stp->sd_flag &= ~STRTOSTOP;
#endif /* _IRIX_LATER */
		if (sop->so_flags & SO_DELIM)
			stp->sd_flag |= STRDELIM;
		if (sop->so_flags & SO_NODELIM)
			stp->sd_flag &= ~STRDELIM;
		if (sop->so_flags & SO_STRHOLD)
			stp->sd_flag |= STRHOLD;
		if (sop->so_flags & SO_NOSTRHOLD)
			stp->sd_flag &= ~STRHOLD;
#ifndef _IRIX_LATER
		if (sop->so_flags & SO_VTIME)
			stp->sd_vtime = sop->so_vtime;

		if (sop->so_flags & SO_TOSTOP) {
			if (sop->so_tostop)
				stp->sd_flag |= STRTOSTOP;
			else
				stp->sd_flag &= ~STRTOSTOP;
		}
#endif /* !_IRIX_LATER */

		freemsg(bp);

		if (bpri == 0) {
			if ((q->q_count <= q->q_lowat) &&
			    (q->q_flag & QWANTW)) {
				q->q_flag &= ~QWANTW;
				for (q = backq(q); q && !q->q_qinfo->qi_srvp;
				    q = backq(q))
					;
				if (q) {
					qenable(q);
					setqback(q, bpri);
				}
			}
		} else {
			unsigned char i;

			qbp = q->q_bandp;
			for (i = 1; i < bpri; i++)
				qbp = qbp->qb_next;
			if ((qbp->qb_count <= qbp->qb_lowat) &&
			    (qbp->qb_flag & QB_WANTW)) {
				qbp->qb_flag &= ~QB_WANTW;
				for (q = backq(q); q && !q->q_qinfo->qi_srvp;
				    q = backq(q))
					;
				if (q) {
					qenable(q);
					setqback(q, bpri);
				}
			}
		}
		return 0;

	/*
	 * The following set of cases deal with situations where two stream
	 * heads are connected to each other (twisted streams).  These messages
	 * have no meaning at the stream head.
	 */
	case M_BREAK:
	case M_CTL:
	case M_DELAY:
	case M_START:
	case M_STOP:
	case M_IOCDATA:
	case M_STARTI:
	case M_STOPI:
		freemsg(bp);
		return 0;

	case M_IOCTL:
		/*
		 * Always NAK this condition
		 * (makes no sense)
		 */
		bp->b_datap->db_type = M_IOCNAK;
		qreply(q, bp);
		return 0;

	default:
		ASSERT(0);
		freemsg(bp);
		return 0;
	}
}

/*
 * Write attempts to break the read request into messages conforming
 * with the minimum and maximum packet sizes set downstream.  
 *
 * Write will always attempt to get the largest buffer it can to satisfy the
 * message size. If it can not, then it will try up to 2 classes down to try
 * to satisfy the write. Write will not block if downstream queue is full and
 * O_NDELAY is set, otherwise it will block waiting for the queue to get room.
 * 
 * A write of zero bytes gets packaged into a zero length message and sent
 * downstream like any other message.
 *
 * If buffers of the requested sizes are not available, the write will
 * sleep until the buffers become available.
 *
 * Write (if specified) will supply a write offset in a message if it
 * makes sense. This can be specified by downstream modules as part of
 * a M_SETOPTS message.  Write will not supply the write offset if it
 * cannot supply any data in a buffer.  In other words, write will never
 * send down an empty packet due to a write offset.
 */

/* ARGSUSED */
int
strwrite(struct vnode *vp, struct uio *uiop, cred_t *crp)
{
	register struct stdata *stp;
	register struct queue *wqp;
	register mblk_t *mp;
	int s2;
	long rmin, rmax;
	long iosize;
	char waitflag;
	int tempmode;
	int done = 0;
	int error = 0;

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);
	ASSERT(stp || iscn(vp));

	if (stp == NULL) {
		return EIO;
	}

	if (stp->sd_flag & STPLEX) {
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	}
	if (stp->sd_flag & (STWRERR|STRHUP)) {
		if (stp->sd_flag & STRSIGPIPE)
			uiop->uio_sigpipe = 1;
		/*
		 * this is for POSIX compatibility
		 */
		STRHEAD_UNLOCK(stp);
		return ((stp->sd_flag & STRHUP) ? EIO : stp->sd_werror);
	}

	/*
	 * Check the min/max packet size constraints.  If min packet size
	 * is non-zero, the write cannot be split into multiple messages
	 * and still guarantee the size constraints. 
	 */
	wqp = stp->sd_wrq;
	rmin = wqp->q_next->q_minpsz;
	rmax = wqp->q_next->q_maxpsz;
	ASSERT((rmax >= 0) || (rmax == INFPSZ));
	if (rmax == 0) {
		STRHEAD_UNLOCK(stp);
		return (0);
	}
	if (strmsgsz != 0) {
		if (rmax == INFPSZ)
			rmax = strmsgsz;
		else  {
			if (vp->v_type == VFIFO)
				rmax = MIN(PIPE_BUF, rmax);
			else	rmax = MIN(strmsgsz, rmax);
		}
	}
	if (rmin > 0) {
		if (uiop->uio_resid < rmin) {
			STRHEAD_UNLOCK(stp);
			return (ERANGE);
		}
	    	if ((rmax != INFPSZ) && (uiop->uio_resid > rmax)) {
			STRHEAD_UNLOCK(stp);
			return (ERANGE);
		}
	}

	/*
	 * Do until count satisfied or error.
	 */
	waitflag = WRITEWAIT;
	if (stp->sd_flag & OLDNDELAY)
		tempmode = uiop->uio_fmode & ~FNDELAY;
	else
		tempmode = uiop->uio_fmode;

	do {
		register int size = uiop->uio_resid;
		mblk_t *amp;	/* auto */

		while (!bcanput(wqp->q_next, 0)) {

			/*
			 * if FIFO/pipe, don't sleep here. Sleep in the
			 * fifo write routine.
			 */
			if (vp->v_type == VFIFO) {
				STRHEAD_UNLOCK(stp);
				return (ESTRPIPE);
			}
#ifdef _IRIX_LATER
			if ((error = strwaitq(stp, waitflag, (off_t)0,
					      tempmode, &done))
#else
			if ((error = strwaitq(stp, waitflag, (off_t)0,
					      tempmode, 0, &done))
#endif /* _IRIX_LATER */
			    || done) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
		}


		/*
		 * job control
		 *
		 * Block the process if it's in background and it's
		 * trying to write to its controlling terminal.
		 */
		if (inbackground(stp) && (stp->sd_flag & STRTOSTOP) != 0) {

			STRHEAD_UNLOCK(stp);

			error = accessctty(JCTLWRITE);
			if (error > 0) {
				return (error);
			}

			STRHEAD_LOCK(&vp->v_stream, stp);
			ASSERT(stp);

			if (error == 0) {
				/* we may have stopped so recheck
				 * buffer status
				 */
				continue;
			}
			/* can't send TTOU - allow writing */
			error = 0;
		}

		/* still at splstr */
		if (mp = wqp->q_first)  {
			int spaceleft;
			/*
			 * Grab the previously held message "mp" and see
			 * if there's room in it to hold this write's data.
			 */
			spaceleft = mp->b_datap->db_lim - mp->b_wptr;
			if (size <= spaceleft)  {
				if (error = uiomove((caddr_t)mp->b_wptr, size,
						    UIO_WRITE, uiop))  {
					STRHEAD_UNLOCK(stp);
					return (error);
				}
				mp->b_wptr += size;
				/*
				 * If the buffer would hold yet another write
				 * of the same size, return.  (wait a while)
				 */
				if ((size<<1) <= spaceleft)  {
					STRHEAD_UNLOCK(stp);
					return (0);
				}
			}

			wqp->q_first = NULL;
		} else {
			/*
			 * Determine the size of the next message to be
			 * packaged.  May have to break write into several
			 * messages based on max packet size.
			 */
			if (rmax == INFPSZ)
				iosize = uiop->uio_resid;
			else	iosize = MIN(uiop->uio_resid, rmax);

			if ((error = strmakemsg((struct strbuf *)NULL,
			    iosize, uiop, stp, (long)0, &amp)) || !amp) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mp = amp;

			/*
			 * When explicitly enabled (typically for TTYs), check
			 * to see if data might be coalesced for better
			 * performance.
			 *
			 * Policy:  Use the size of this write as predictor
			 * of the size of the next write.  If this msg buffer
			 * has space for another write of the same size, hold
			 * onto it for a while.
			 */
			if (strholdtime > 0 && (stp->sd_flag & STRHOLD) &&
			    size <= (mp->b_datap->db_lim - mp->b_wptr))  {

				wqp->q_first = mp;
				stp->sd_rtime = lbolt + STRHOLDTIME;

				/*
				 * Add stream to held-message list
				 */
				LOCK_STR_RESOURCE(s2);
				if (!(wqp->q_flag & QHLIST)) {
					wqp->q_flag |= QHLIST;
					ASSERT(*strholdtailp == NULL);
					*strholdtailp = stp;
					stp->sd_hnext = NULL;
					strholdtailp = &(stp->sd_hnext);
				}
				if (!strholdflag)  {
					strholdflag++;
					MP_STREAMS_TIMEOUT(&streams_monp,
							strrelease,
							(void *)0,
							STRHOLDTIME);
				}
				UNLOCK_STR_RESOURCE(s2);

				STRHEAD_UNLOCK(stp);
				return (0);
			}
		}

		/*
		 * Put block downstream.
		 */
		wqp->q_first = NULL;
		if ((uiop->uio_resid == 0) && (stp->sd_flag & STRDELIM))
			mp->b_flag |= MSGDELIM;
		STREAMS_BLOCKQ();
		ASSERT(wqp->q_next->q_qinfo->qi_putp);
		(*wqp->q_next->q_qinfo->qi_putp)(wqp->q_next, mp);
		STREAMS_UNBLOCKQ();
		waitflag |= NOINTR;
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */

	} while (uiop->uio_resid);

	STRHEAD_UNLOCK(stp);
	return (0);
}

/*
 * Stream head write service routine.
 * Its job is to wake up any sleeping writers when a queue
 * downstream needs data (part of the flow control in putq and getq).
 * It also must wake anyone sleeping on a poll().
 * For stream head right below mux module, it must also invoke put procedure
 * of next downstream module.
 */
int
strwsrv(register queue_t *q)
{
	register struct stdata *stp;
	register int s2, cnt;
	register queue_t *tq;
	register qband_t *qbp;
	register int i;
	qband_t *myqbp;
	int isevent;
	unsigned char qbf[NBAND];

	stp = (struct stdata *)q->q_ptr;
	ASSERT(!(stp->sd_flag & STPLEX));
	ASSERT(STREAM_LOCKED(q));

	LOCK_STR_RESOURCE(s2);
	if (stp->sd_pflag & WSLEEP) {
		/* XXXrs - see comment in strrput describing this hack
		stp->sd_pflag &= ~WSLEEP;
		*/
		UNLOCK_STR_RESOURCE(s2);
		cnt = 0;
		/* XXXrs - more ugly hacking.  Retry if nobody awakened. */
		while (strwakeq(stp, q, WRITEWAIT) && cnt < 1000) {
			cnt++;
			if (cnt == MAXWAKEQLOOP) {
				strwakeq_giveup++;
				break;
			}
		}
	} else {
		UNLOCK_STR_RESOURCE(s2);
	}

	if ((tq = q->q_next) == NULL) {
		/*
		 * The other end of a stream pipe went away.
		 */
		return 0;
	}
	while (tq->q_next && !tq->q_qinfo->qi_srvp)
		tq = tq->q_next;

	if (q->q_flag & QBACK) {
		if (tq->q_flag & QFULL) {
			tq->q_flag |= QWANTW;
		} else {
			if (stp->sd_sigflags & S_WRNORM)
				strsendsig(stp->sd_siglist, S_WRNORM, 0L);
			pollwakeup(&stp->sd_pollist, POLLWRNORM);
		}
	}

	isevent = 0;
	i = 1;
	bzero((caddr_t)qbf, NBAND);
	myqbp = q->q_bandp;
	for (qbp = tq->q_bandp; qbp; qbp = qbp->qb_next) {
		if (!myqbp)
			break;
		if (myqbp->qb_flag & QB_BACK) {
			if (qbp->qb_flag & QB_FULL) {
				qbp->qb_flag |= QB_WANTW;
			} else {
				isevent = 1;
				qbf[i] = 1;
			}
		}
		myqbp = myqbp->qb_next;
		i++;
	}
	while (myqbp) {
		if (myqbp->qb_flag & QB_BACK) {
			isevent = 1;
			qbf[i] = 1;
		}
		myqbp = myqbp->qb_next;
		i++;
	}

	if (isevent) {
	    for (i--; i; i--) {
		if (qbf[i]) {
			if (stp->sd_sigflags & S_WRBAND)
				strsendsig(stp->sd_siglist, S_WRBAND, (long)i);
			pollwakeup(&stp->sd_pollist, POLLWRBAND);
		}
	    }
	}
	return(0);
}


/*
 * ioctl for streams
 */
int
strioctl(struct vnode *vp,
	 int cmd,
	 void *ptr_arg,
	 int flag,
	 int copyflag,
	 cred_t *crp,
	 int *rvalp)
{
	register struct stdata *stp;
	int s2;
	struct strioctl strioc;
	struct uio uio;
	struct iovec iov;
	enum jobcontrol access;
	vproc_t *vpr;
	vp_get_attr_t attr;
	vsession_t *vsp;
	pid_t sid;
	mblk_t *mp;
	int error = 0;
	int done = 0;
	int old_cmd = 0;
	__int32_t int_arg;
	struct __new_termio ntio,tio;
	struct __old_termio otio;
	struct __old_termios otios;
	struct __new_termios tios;
	char *odp;
	int size, count, i;

#if _MIPS_SIM == _ABI64
	int convert_abi = copyflag == U_TO_K && 
				  !ABI_IS_IRIX5_64(get_current_abi());
	int irix5_fix_fdinsert(mblk_t **, int *);
#endif

	int_arg = (__int32_t)(__psint_t)ptr_arg;

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);

	ASSERT(stp);
	ASSERT(copyflag == U_TO_K || copyflag == K_TO_K);

	/*
	 *  if a message is being "held" awaiting possible consolidation,
	 *  send it downstream before processing ioctl.
	 */
	{
		register queue_t *q;
		
		q= stp->sd_wrq;

		if (mp = q->q_first)  {
			q->q_first = NULL;

			putnext(q, mp);
		}
	}

	switch (cmd) {
	case I_E_RECVFD:
	case I_RECVFD:
	case I_S_RECVFD:
		access = JCTLREAD;
		break;

	case FIOASYNC:
	case FIONBIO:
	case I_ATMARK:
	case I_FDINSERT:
	case I_FLUSH:
	case I_FLUSHBAND:
	case I_LINK:
	case I_POP:
	case I_PUNLINK:
	case I_PUSH:
	case I_SENDFD:
	case I_SETCLTIME:
	case I_SETSIG:
	case I_SRDOPT:
	case I_STR:
	case I_SWROPT: 
	case I_UNLINK:
	case TCBLKMD:
	case TCFLSH:
	case TCSBRK:
	case __NEW_TCSETS:
	case __NEW_TCSETSW:
	case __NEW_TCSETSF:
	case __OLD_TCSETS:
	case __OLD_TCSETSW:
	case __OLD_TCSETSF:
	case __NEW_TCSETA:
	case __NEW_TCSETAW:
	case __NEW_TCSETAF:
	case __OLD_TCSETA:
	case __OLD_TCSETAW:
	case __OLD_TCSETAF:
	case TCXONC:
	case TIOCCONS:
	case TIOCFLUSH:
	case TIOCPKT:
	case TIOCSETP:
	case TIOCSPGRP:
	case TIOCSSID:
	case TIOCSTI:
	case TIOCSWINSZ:
	case TCSETLABEL:
		access = JCTLWRITE;
		break;

	case FIONREAD:
	case FIORDCHK:
#ifdef _IRIX_LATER
	case JWINSIZE:
#endif /* _IRIX_LATER */
	case LDGETT:
	case LDGMAP:
	case I_CANPUT:
	case I_CKBAND:
	case I_FIND:
	case I_GETBAND:
	case I_GETCLTIME:
	case I_GETSIG:
	case I_GRDOPT:
	case I_GWROPT:
	case I_LIST:
	case I_LOOK:
	case I_NREAD:
	case I_PEEK:
#ifdef CKPT
	case I_PEEKN:
	case I_NREADN:
#endif /* CKPT */
	case __NEW_TCGETA:
	case __NEW_TCGETS:
	case __OLD_TCGETA:
	case __OLD_TCGETS:
	case TIOCGETC:
	case TIOCGETD:
	case TIOCGETP:
	case TIOCGLTC:
	case TIOCGPGRP:
	case TIOCGSID:
	case TIOCGWINSZ:
	case TIOCLGET:
	case TIOCMGET:
	case TIOCNOTTY:
		access = JCTLGST;
		break;

	default:
		access = JCTLWRITE;
		break;
	}

	/*
	 * job control
	 *
	 * Block the process if it's in background and the ioctl it is
	 * doing will modify its controlling terminal.
	 */
	while (inbackground(stp)) {

		STRHEAD_UNLOCK(stp);

		error = accessctty(access);
		if (error > 0)
			return (error);

		STRHEAD_LOCK(&vp->v_stream, stp);
		ASSERT(stp);

		if (access != JCTLWRITE)
			break;
		if (error < 0) {
			/* can't send TTOU - permit writing */
			error = 0;
			break;
		}
	}


	if (stp->sd_flag & (STRDERR|STWRERR|STPLEX)) {
		STRHEAD_UNLOCK(stp);
		return ((stp->sd_flag & STPLEX) ? EINVAL :
		    (stp->sd_werror ? stp->sd_werror : stp->sd_rerror));
	}

	switch (cmd) {

	default:
		if ((cmd & IOCTYPE) == LDIOC ||
		    (cmd & IOCTYPE) == tIOC ||
		    (cmd & IOCTYPE) == TIOC ||
		    (cmd & IOCTYPE) == SIOC) {

			/*
			 * The ioctl is a tty ioctl - set up strioc buffer 
			 * and call strdoioctl() to do the work.
			 */
			if (stp->sd_flag & STRHUP) {
				STRHEAD_UNLOCK(stp);
				return (ENXIO);
			}
			strioc.ic_cmd = cmd;
			strioc.ic_timout = INFTIM;

			switch (cmd) {
			case TIOCCONS:
				/* Same semantics as 4.3BSD */

				if (!(error = STREAMS_COPYIN((caddr_t)ptr_arg,
							     (caddr_t)&int_arg,
							     sizeof(int_arg),
							     NULL,
							     copyflag)))
					error = cn_link(vp, int_arg);
				STRHEAD_UNLOCK(stp);
				return(error);

			case TCSETLABEL: {
				mac_label *new_label;

				if (!_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
					STRHEAD_UNLOCK(stp);
#ifdef	DEBUG
					cmn_err(CE_NOTE,
					    "TCSETLABEL by %s gets EPERM",
					    get_current_name());
#endif	/* DEBUG */
					return EPERM;
				}
				STRHEAD_UNLOCK(stp);
				error = mac_copyin_label((mac_label *)ptr_arg,
				    &new_label);
				STRHEAD_LOCK(&vp->v_stream, stp);
#ifdef	DEBUG
				if (error)
					cmn_err(CE_NOTE,
					    "TCSETLABEL mac_copyin_label %d",
					    error);
#endif	/* DEBUG */
				if (error) {
					STRHEAD_UNLOCK(stp);
					_SAT_TTY_SETLABEL(new_label,error);
					return (error);
				}
				strioc.ic_dp = (char *)&new_label;
				strioc.ic_len = sizeof(mac_label *);
				error = strdoioctl(stp, &strioc, NULL, K_TO_K,
						   (char *)NULL, crp, rvalp);
				STRHEAD_UNLOCK(stp);

#ifdef	DEBUG
				if (error)
					cmn_err(CE_NOTE,
					    "TCSETLABEL strdoioctl %d", error);
#endif	/* DEBUG */
				_SAT_TTY_SETLABEL( new_label, error );
				return (error);
				}
			case TIOCFLUSH:
				/*
				 * Map into TCFLSH.
				 * Note that ptr_arg == 0 is taken to be
				 * the same as *ptr_arg == 0.
				 */
				if (ptr_arg == 0)
					int_arg = 2;
				else if (STREAMS_COPYIN((caddr_t)ptr_arg,
							(caddr_t)&int_arg,
							sizeof(int_arg),
							NULL,
							copyflag)) {
					STRHEAD_UNLOCK(stp);
					return (EFAULT);
				}
				int_arg &= FREAD|FWRITE;
				if (int_arg == FREAD)
					int_arg = 0;
				else if (int_arg == FWRITE)
					int_arg = 1;
				else
					int_arg = 2;
				/* fall through... */

			case TCFLSH:

				switch (int_arg) {
				case 0:
					STRHEAD_UNLOCK(stp);
					return (strioctl(vp, I_FLUSH,
							 (void *)FLUSHR,
							 flag, copyflag, crp,
							 rvalp));
				case 1:
					STRHEAD_UNLOCK(stp);
					return (strioctl(vp, I_FLUSH,
							 (void *)FLUSHW,
							 flag, copyflag, crp,
							 rvalp));
				case 2:
					STRHEAD_UNLOCK(stp);
					return (strioctl(vp, I_FLUSH,
							 (void *)FLUSHRW,
							 flag, copyflag, crp,
							 rvalp));
				default:
					STRHEAD_UNLOCK(stp);
					return (EINVAL);
				}

			case TCXONC:
			case TCSBRK:
			case TCDSET:
			case TCBLKMD:
			case SIOC_ITIMER:
			case SIOC_RS422:
			case SIOC_EXTCLK:
				strioc.ic_len = sizeof(int);
				strioc.ic_dp = (char *)&int_arg;
				error = strdoioctl(stp, &strioc, NULL, K_TO_K,
						   STRINT, crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);


				/*
				 * __OLD_TC[GS]ETA are converted
				 * to __NEW_TC[GS]ETA...
				 * (__old_termio -> __new_termio)
				 */
			case __OLD_TCGETA:

				bzero (&ntio, sizeof (ntio));

				strioc.ic_cmd = __NEW_TCGETA;
				strioc.ic_len = sizeof(ntio);
				strioc.ic_dp = (char *)&ntio;
				error = strdoioctl(stp, &strioc, NULL,
						   K_TO_K, STRTERMIO,
						   crp, rvalp);

				STRHEAD_UNLOCK(stp);
				if (error) {
					return (error);
				}
				otio.c_iflag = ntio.c_iflag;
				otio.c_oflag = ntio.c_oflag;
				otio.c_cflag = ntio.c_cflag;
				otio.c_lflag = ntio.c_lflag;
				otio.c_line = ntio.c_line;
				otio.c_cflag &= ~(CBAUD|CIBAUD);
				otio.c_cflag |=
				    baud_to_cbaud(ntio.c_ospeed);
				otio.c_cflag |=
				    baud_to_cbaud(ntio.c_ispeed) << 16;
				bcopy(&(ntio.c_cc[0]), &(otio.c_cc[0]),
				      NCCS*sizeof(cc_t));
				error = strcopyout((caddr_t)&otio,
						   (caddr_t)ptr_arg,
						   sizeof(otio),
						   STRTERMIO, copyflag);

				return (error);

			case __OLD_TCSETA:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETA;
				}
				/* fall through... */
			case __OLD_TCSETAW:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAW;
				}
				/* fall through... */
			case __OLD_TCSETAF:
			   {
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAF;
				}
				/*
				 * Kernel always uses struct __new_termio.
				 * Mutate what the caller passed us.
				 */
				error = STREAMS_COPYIN((caddr_t)ptr_arg,
						       (caddr_t)&otio,
		    		       		       sizeof(otio),
						       STRTERMIO, copyflag);
				if (error) {
					STRHEAD_UNLOCK(stp);
					return (error);
				}
				ntio.c_iflag = otio.c_iflag;
				ntio.c_oflag = otio.c_oflag;
				ntio.c_cflag = otio.c_cflag;
				ntio.c_lflag = otio.c_lflag;
				ntio.c_ospeed = cbaud_to_baud_table[otio.c_cflag & CBAUD];
				ntio.c_ispeed = cbaud_to_baud_table[(otio.c_cflag & CIBAUD) >> 16];
				ntio.c_line = otio.c_line;
				bcopy(&(otio.c_cc[0]), &(ntio.c_cc[0]),
				      NCCS*sizeof(cc_t));

				strioc.ic_len = sizeof(ntio);
				strioc.ic_dp = (char *)&ntio;
				error = strdoioctl(stp, &strioc, NULL,
				    		   K_TO_K, STRTERMIO,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);

				return (error);
			   }


			case __NEW_TCGETA:
			case __NEW_TCSETA:
			case __NEW_TCSETAW:
			case __NEW_TCSETAF:
				strioc.ic_len = sizeof(struct __new_termio);
				strioc.ic_dp = (char *)ptr_arg;
				error = strdoioctl(stp, &strioc, NULL,
						   copyflag, STRTERMIO,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);


				/*
				 * __OLD_TC[GS]ETS are converted
				 * to __NEW_TC[GS]ETA.  
				 * (__old_termios -> __new_termio)
				 */
			case __OLD_TCGETS:

				bzero (&ntio, sizeof (ntio));

				strioc.ic_cmd = __NEW_TCGETA;
				strioc.ic_len = sizeof(ntio);
				strioc.ic_dp = (char *)&ntio;
				error = strdoioctl(stp, &strioc, NULL,
						   K_TO_K, STRTERMIO,
						   crp, rvalp);

				STRHEAD_UNLOCK(stp);

				if (error) {
					return (error);
				}

				otios.c_iflag = ntio.c_iflag;
				otios.c_oflag = ntio.c_oflag;
				otios.c_cflag = ntio.c_cflag;
				otios.c_lflag = ntio.c_lflag;
				otios.c_cflag &= ~(CBAUD|CIBAUD);
				otios.c_cflag |=
					baud_to_cbaud(ntio.c_ospeed);
				otios.c_cflag |=
					(baud_to_cbaud(ntio.c_ispeed) << 16);
				bcopy(&(ntio.c_cc[0]), &(otios.c_cc[0]),
					NCCS*sizeof(cc_t));

				error = strcopyout((caddr_t)&otios,
					(caddr_t)ptr_arg,
					sizeof(otios),
					STRTERMIOS, copyflag);

				return (error);

			case __OLD_TCSETS:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETA;
				}
				/* fall through... */
			case __OLD_TCSETSW:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAW;
				}
				/* fall through... */
			case __OLD_TCSETSF:
			   {
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAF;
				}
				/*
				 * Kernel always uses struct __new_termio.
				 * Mutate what the caller passed us.
				 */
				error = STREAMS_COPYIN((caddr_t)ptr_arg,
						       (caddr_t)&otios,
		    		       		       sizeof(otios),
						       STRTERMIOS, copyflag);
				if (error) {
					STRHEAD_UNLOCK(stp);
					return (error);
				}

				ntio.c_iflag = otios.c_iflag;
				ntio.c_oflag = otios.c_oflag;
				ntio.c_cflag = otios.c_cflag;
				ntio.c_lflag = otios.c_lflag;
				ntio.c_ospeed =
			cbaud_to_baud_table[otios.c_cflag & CBAUD];
				ntio.c_ispeed =
			cbaud_to_baud_table[(otios.c_cflag & CIBAUD) >> 16];
				ntio.c_line = LDISC1;
				bcopy(&(otios.c_cc[0]), &(ntio.c_cc[0]),
				      NCCS*sizeof(cc_t));

				strioc.ic_len = sizeof(ntio);
				strioc.ic_dp = (char *)&ntio;
				error = strdoioctl(stp, &strioc, NULL,
				    		   K_TO_K, STRTERMIO,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);

				return (error);
			   }


				/*
				 * __NEW_TC[GS]ETS are converted
				 * to __NEW_TC[GS]ETA...
				 */
			case __NEW_TCGETS:

				bzero(&tio, sizeof (tio));

				strioc.ic_cmd = __NEW_TCGETA;
				strioc.ic_len = sizeof(tio);
				strioc.ic_dp = (char *)&tio;
				error = strdoioctl(stp, &strioc, NULL,
						   K_TO_K, STRTERMIO,
						   crp, rvalp);

				STRHEAD_UNLOCK(stp);

				if (error) {
					return (error);
				}

				tios.c_iflag = tio.c_iflag;
				tios.c_oflag = tio.c_oflag;
				tios.c_cflag = tio.c_cflag;
				tios.c_lflag = tio.c_lflag;
				tios.c_ospeed = tio.c_ospeed;
				tios.c_ispeed = tio.c_ispeed;
				bcopy(&(tio.c_cc[0]), &(tios.c_cc[0]),
				      NCCS*sizeof(cc_t));
				error = strcopyout((caddr_t)&tios,
						   (caddr_t)ptr_arg,
						   sizeof(tios),
						   STRTERMIOS, copyflag);

				return (error);

			case __NEW_TCSETS:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETA;
				}
				/* fall through... */
			case __NEW_TCSETSW:
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAW;
				}
				/* fall through... */
			case __NEW_TCSETSF:
			   {
				if (!old_cmd) {
					old_cmd = strioc.ic_cmd;
					strioc.ic_cmd = __NEW_TCSETAF;
				}
				/*
				 * Kernel always uses struct termio.
				 * Mutate what the caller passed us.
				 */
				error = STREAMS_COPYIN((caddr_t)ptr_arg,
						       (caddr_t)&tios,
		    		       		       sizeof(tios),
						       STRTERMIOS, copyflag);
				if (error) {
					STRHEAD_UNLOCK(stp);
					return (error);
				}
				tio.c_iflag = tios.c_iflag;
				tio.c_oflag = tios.c_oflag;
				tio.c_cflag = tios.c_cflag;
				tio.c_lflag = tios.c_lflag;
				tio.c_ospeed = tios.c_ospeed;
				tio.c_ispeed = tios.c_ispeed;
				tio.c_line = LDISC1;
				bcopy(&(tios.c_cc[0]), &(tio.c_cc[0]),
				      NCCS*sizeof(cc_t));

				strioc.ic_len = sizeof(tio);
				strioc.ic_dp = (char *)&tio;
				error = strdoioctl(stp, &strioc, NULL,
				    		   K_TO_K, STRTERMIO,
						   crp, rvalp);

				STRHEAD_UNLOCK(stp);

				return (error);
			   }

			case LDGETT:
			case LDSETT:
				strioc.ic_len = sizeof(struct termcb);
				strioc.ic_dp = (char *)ptr_arg;
				error = strdoioctl(stp, &strioc, NULL,
				    		   copyflag, STRTERMCB,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);

			case TIOCGETP:
			case TIOCSETP:
				strioc.ic_len = sizeof(struct sgttyb);
				strioc.ic_dp = (char *)ptr_arg;
				error = strdoioctl(stp, &strioc, NULL,
				    		   copyflag, STRSGTTYB,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);

			case TIOCSTI:
				/*
				 * Security is important here, since this
				 * allows one to submit input as if it were
				 * actually typed on some tty.  These checks
				 * are equivalent to the ones that are made
				 * in BSD4.3.
				 */
				if ((flag & FREAD) == 0 &&
				    !_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
					STRHEAD_UNLOCK(stp);
					return (EPERM);
				}
				/*
				 * If the tty is not the controlling tty of the
				 * process, disallow for all but the root.
				 */
				vpr = VPROC_LOOKUP(current_pid());
				ASSERT(vpr);
				VPROC_GET_ATTR(vpr, VGATTR_SID, &attr);
				VPROC_RELE(vpr);
				vsp = stp->sd_vsession;
				if ((vsp == NULL || attr.va_sid != vsp->vs_sid)
				    && !_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
					STRHEAD_UNLOCK(stp);
					return (EACCES);
				}
                                strioc.ic_len = sizeof(char);
                                strioc.ic_dp = (char *)ptr_arg;
                                error = strdoioctl(stp, &strioc, NULL,
						   copyflag, "c", crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);

	               	case TIOCMGET:
			case TIOCPKT:
				strioc.ic_len = sizeof(int);
				strioc.ic_dp = (char *)ptr_arg;
				error = strdoioctl(stp, &strioc, NULL,
						   U_TO_K, STRINT,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);

			case TIOCGWINSZ:
			case TIOCSWINSZ:
				strioc.ic_len = sizeof(struct winsize);
				strioc.ic_dp = (char *)ptr_arg;
				error = strdoioctl(stp, &strioc, NULL,
						   copyflag, (char *)NULL,
						   crp, rvalp);
				STRHEAD_UNLOCK(stp);
				return (error);

			}
		}

		/*
		 * Unknown cmd - send down request to support 
		 * transparent ioctls.
		 */

		strioc.ic_cmd = cmd;
		strioc.ic_timout = INFTIM;
		/* hack to support tpisocket code */
		if (cmd & IOC_IN) {
			strioc.ic_len = (cmd >> 16) & IOCPARM_MASK;
			strioc.ic_dp = (char *)ptr_arg;
		} else {
			strioc.ic_len = (int)TRANSPARENT;
#if _MIPS_SIM == _ABI64
			if (ABI_IS_64BIT(get_current_abi()))
				strioc.ic_dp = (char *)&ptr_arg;
			else
				strioc.ic_dp = (char *)&int_arg;
#else
			strioc.ic_dp = (char *)&int_arg;
#endif
			/* strdoioctl will ignore copyflag & use K_TO_K */
		}
		error = strdoioctl(stp, &strioc, NULL, copyflag,
				   (char *)NULL, crp, rvalp);
		STRHEAD_UNLOCK(stp);
		return (error);

	case I_STR: {
#if _MIPS_SIM == _ABI64
		struct irix5_strioctl i5_strioc;
#endif
		/*
		 * Stream ioctl.  Read in an strioctl buffer from the user
		 * along with any data specified and send it downstream.
		 * Strdoioctl will wait allow only one ioctl message at
		 * a time, and waits for the acknowledgement.
		 */

		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
						(caddr_t)&i5_strioc,
						sizeof(struct irix5_strioctl),
						STRIOCTL,
						copyflag);
			if (!error)
				irix5_strioctl_to_strioctl(&i5_strioc, &strioc);
		} else
#endif
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
						(caddr_t)&strioc,
						sizeof(struct strioctl),
						STRIOCTL,
						copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if ((strioc.ic_len < 0) || (strioc.ic_timout < -1)
		    || (strioc.ic_len > strmsgsz)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/*
		 * Because the requested IOCTL might be a TIOCSTI or other
		 * powerful command, require the user to have permission.
		 * XXX I_CHKSYSLOGD will need to be cleaned up later
		 */
		if ((flag & FREAD) == 0) {
			extern struct streamtab loginfo;
			switch (strioc.ic_cmd) {
			case I_CHKSYSLOGD:
				if (stp->sd_strtab == &loginfo) {
					/*
					 * this is our ioctl on our device;
					 * allow it
					 */
					break;
				}
				/* let the chips fall where they may */
				/* FALL THROUGH */
			default:
		    		if (!_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
					STRHEAD_UNLOCK(stp);
					return (EPERM);
				}
			}
		}
		_SAT_FD_READ(curuthread->ut_sat_proc->sat_openfd, 0);

		switch(strioc.ic_cmd) {

		case __OLD_TCGETS:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCGETA;
			}

			bzero(&ntio, sizeof (ntio));

			strioc.ic_len = sizeof(ntio);
			odp = strioc.ic_dp;
			strioc.ic_dp = (char *)&ntio;
			error = strdoioctl(stp, &strioc, NULL,
					   K_TO_K, STRTERMIO,
					   crp, rvalp);
			if (error) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			otios.c_iflag = ntio.c_iflag;
			otios.c_oflag = ntio.c_oflag;
			otios.c_cflag = ntio.c_cflag;
			otios.c_lflag = ntio.c_lflag;
			otios.c_cflag &= ~(CBAUD|CIBAUD);
			otios.c_cflag |= baud_to_cbaud(ntio.c_ospeed);
			otios.c_cflag |= (baud_to_cbaud(ntio.c_ispeed) << 16);
			bcopy(&(ntio.c_cc[0]), &(otios.c_cc[0]),
			      NCCS*sizeof(cc_t));
			strioc.ic_cmd = old_cmd;
			strioc.ic_len = sizeof(otios);
			strioc.ic_dp = odp;
			error = STREAMS_COPYOUT((caddr_t)&otios,
						(caddr_t)strioc.ic_dp,
						sizeof(otios),
						STRTERMIOS, copyflag);

			break;

		case __OLD_TCSETS:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETA;
			}
			/* fall through... */
		case __OLD_TCSETSW:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETAW;
			}
			/* fall through... */
		case __OLD_TCSETSF:
		    {
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETAF;
			}
			/*
			 * Kernel always uses struct __new_termio.  Mutate
			 * what the caller passed us.
			 */
			error = STREAMS_COPYIN((caddr_t)strioc.ic_dp,
					       (caddr_t)&otios,
		    	       		       sizeof(otios),
					       STRTERMIOS, copyflag);
			if (error) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			ntio.c_iflag = otios.c_iflag;
			ntio.c_oflag = otios.c_oflag;
			ntio.c_cflag = otios.c_cflag;
			ntio.c_lflag = otios.c_lflag;
			ntio.c_ospeed =
			   cbaud_to_baud_table[otios.c_cflag & CBAUD];
			ntio.c_ispeed =
			   cbaud_to_baud_table[(otios.c_cflag & CIBAUD) >> 16];
			ntio.c_line = LDISC1;
			bcopy(&(otios.c_cc[0]), &(ntio.c_cc[0]),
			      NCCS*sizeof(cc_t));

			strioc.ic_len = sizeof(ntio);
			odp = strioc.ic_dp;
			strioc.ic_dp = (char *)&ntio;
			error = strdoioctl(stp, &strioc, NULL,
			    		   K_TO_K, STRTERMIO,
					   crp, rvalp);

			strioc.ic_cmd = old_cmd;
			strioc.ic_len = sizeof(otios);
			strioc.ic_dp = odp;

			break;

		    }

		case __NEW_TCGETS:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCGETA;
			}

			bzero(&tio, sizeof(tio));

			strioc.ic_len = sizeof(tio);
			odp = strioc.ic_dp;
			strioc.ic_dp = (char *)&tio;
			error = strdoioctl(stp, &strioc, NULL,
					   K_TO_K, STRTERMIO,
					   crp, rvalp);
			if (error) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			tios.c_iflag = tio.c_iflag;
			tios.c_oflag = tio.c_oflag;
			tios.c_cflag = tio.c_cflag;
			tios.c_ospeed = tio.c_ospeed;
			tios.c_ispeed = tio.c_ispeed;
			tios.c_lflag = tio.c_lflag;
			bcopy(&(tio.c_cc[0]), &(tios.c_cc[0]),
			      NCCS*sizeof(cc_t));
			strioc.ic_cmd = old_cmd;
			strioc.ic_len = sizeof(tios);
			strioc.ic_dp = odp;
			error = STREAMS_COPYOUT((caddr_t)&tios,
						(caddr_t)strioc.ic_dp,
						sizeof(tios),
						STRTERMIOS, copyflag);

			break;

		case __NEW_TCSETS:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETA;
			}
			/* fall through... */
		case __NEW_TCSETSW:
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETAW;
			}
			/* fall through... */
		case __NEW_TCSETSF:
		    {
			if (!old_cmd) {
				old_cmd = strioc.ic_cmd;
				strioc.ic_cmd = __NEW_TCSETAF;
			}
			/*
			 * Kernel always uses struct termio.  Mutate
			 * what the caller passed us.
			 */
			error = STREAMS_COPYIN((caddr_t)strioc.ic_dp,
					       (caddr_t)&tios,
		    	       		       sizeof(tios),
					       STRTERMIOS, copyflag);
			if (error) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			tio.c_iflag = tios.c_iflag;
			tio.c_oflag = tios.c_oflag;
			tio.c_cflag = tios.c_cflag;
			tio.c_ospeed = tios.c_ospeed;
			tio.c_ispeed = tios.c_ispeed;
			tio.c_lflag = tios.c_lflag;
			tio.c_line = LDISC1;
			bcopy(&(tios.c_cc[0]), &(tio.c_cc[0]),
			      NCCS*sizeof(cc_t));

			strioc.ic_len = sizeof(tio);
			odp = strioc.ic_dp;
			strioc.ic_dp = (char *)&tio;
			error = strdoioctl(stp, &strioc, NULL,
			    		   K_TO_K, STRTERMIO,
					   crp, rvalp);

			strioc.ic_cmd = old_cmd;
			strioc.ic_len = sizeof(tios);
			strioc.ic_dp = odp;

			break;
		    }

		default:
			/* hack to support tpisocket code */
#if _MIPS_SIM == _ABI64
			if (IRIX5_SIOCGIFCONF == (old_cmd = strioc.ic_cmd)) {
				strioc.ic_cmd = SIOCGIFCONF_INTERNAL;
			}
#else
			if (SIOCGIFCONF == (old_cmd = strioc.ic_cmd)) {
				strioc.ic_cmd = SIOCGIFCONF_INTERNAL;
			}
#endif
			error = strdoioctl(stp, &strioc, NULL, copyflag,
					   (char *)NULL, crp, rvalp);
			break;
		}

		STRHEAD_UNLOCK(stp);
		if (error == 0) {
			strioc.ic_cmd = old_cmd;
#if _MIPS_SIM == _ABI64
			if (convert_abi) {
				strioctl_to_irix5_strioctl(&strioc, &i5_strioc);
				error = strcopyout((caddr_t)&i5_strioc,
						   (caddr_t)ptr_arg,
						   sizeof(i5_strioc),
						   STRIOCTL, copyflag);
			} else
#endif
				error = strcopyout((caddr_t)&strioc,
						   (caddr_t)ptr_arg,
						   sizeof(struct strioctl),
						   STRIOCTL, copyflag);
		}
		return (error);
	}

	case I_NREAD:
		/*
		 * Return number of bytes of data in first message
		 * in queue in "arg" and return the number of messages
		 * in queue in return value.
		 */
	    {
		size = 0;
		count = 0;

		mp = RD(stp->sd_wrq)->q_first;
		while (mp && (mp->b_flag & MSGNOGET))
			mp = mp->b_next;
		if (mp)
			size = msgdsize(mp);
		for (; mp; mp = mp->b_next)
			if (!(mp->b_flag & MSGNOGET))
				count++;

		error = STREAMS_COPYOUT((caddr_t)&size, 
					(caddr_t)ptr_arg,
					sizeof(int), STRINT, copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		*rvalp = count;
		STRHEAD_UNLOCK(stp);
		return (error);
	    }

	case FIONREAD:
		/*
		 * XXXrs - I modified this to be more IRIX-like instead of
		 *         being similar to I_NREAD above as it is in SVR4.
		 *         It now returns the total number of bytes in
		 *	   "gettable" messages in arg instead of just the
		 *	   number of bytes in the first message.
		 */
	    {
		size = 0;

		for (mp = RD(stp->sd_wrq)->q_first; mp; mp = mp->b_next) {
			if (mp->b_flag & MSGNOGET)
				continue;
			size += msgdsize(mp);
		}
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&size, (caddr_t)ptr_arg,
				   sizeof(int), STRINT, copyflag);
		return (error);
	    }

	case FIORDCHK:
		/*
		 * FIORDCHK does not use arg value (like FIONREAD),
	         * instead a count is returned. I_NREAD value may
		 * not be accurate but safe. The real thing to do is
		 * to add the msgdsizes of all data  messages until
		 * a non-data message.
		 */

	    {
		size = 0;
		mp = RD(stp->sd_wrq)->q_first;
		while (mp && (mp->b_flag & MSGNOGET))
			mp = mp->b_next;
		if (mp)
			size = msgdsize(mp);
		*rvalp = size;
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

#ifdef CKPT
	case I_NREADN:
	{
		int i;
		struct strreadn rdn;
		mblk_t	       *bp;

		if (error = STREAMS_COPYIN((caddr_t)ptr_arg, (caddr_t)&rdn,
				      sizeof(rdn), STRNREADN, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return(error);
		}

		/*
		 * Find the n'th message
		 */
		for (mp = RD(stp->sd_wrq)->q_first, i = 0;
		     i < rdn.msgnum && mp;
		     i++, mp = mp->b_next)
			; /* nothing */

		/*
		 * If mp == NULL, no such message
		 */
		if (!mp) {
			STRHEAD_UNLOCK(stp);
			return(ENODATA);
		}

		rdn.datasz = msgdsize(mp);

		/*
		 * Add the sizes for all control data blocks.
		 */
		for (rdn.ctlsz = 0, bp = mp;
		     bp;
		     bp = bp->b_cont)
			if (bp->b_datap->db_type != M_DATA)
				rdn.ctlsz += 
					bp->b_wptr 
						- bp->b_rptr;
		if (error = STREAMS_COPYOUT((caddr_t)&rdn,
			(caddr_t)ptr_arg, sizeof(rdn),
			STRNREADN, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return error;
		}
		STRHEAD_UNLOCK(stp);
		return 0;
	}

	case I_PEEKN:
	{
		int 	i;
		struct strpeekn pn;
		int	n;

		if (error = STREAMS_COPYIN((caddr_t)ptr_arg,
			(caddr_t)&pn, sizeof(pn),
			STRPEEKN, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return(error);
		}
		
		/*
		 * Follow list of messages to the n'th, or the end
		 * of the list, whichever is first.
		 * Up the  interrupt level to ensure nothing interferes while
		 * we're tracing the chain.
		 */
		for (mp = RD(stp->sd_wrq)->q_first, i = 0;
		     i < pn.msgnum && mp;
		     i++, mp = mp->b_next)
			/* nothing */ ;

		ASSERT((mp == 0) ||
			(i == pn.msgnum));

		if (!mp) {
			*rvalp = 0;
			STRHEAD_UNLOCK(stp);
			return(0);
		}

		/*
		 * Can't do anything with file descriptor transfers.
		 */
		if (mp->b_datap->db_type == M_PASSFP) {
			STRHEAD_UNLOCK(stp);
			return(EBADMSG);
		}

		/*
		 * High priority message?
		 */
		if (mp->b_datap->db_type == M_PCPROTO)
			pn.flags = RS_HIPRI;
		else
			pn.flags = 0;

		/*
		 * First process PROTO blocks, if any.
		 */
		iov.iov_base = pn.ctlbuf.buf;
		iov.iov_len = pn.ctlbuf.maxlen;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_segflg = (copyflag == U_TO_K) ? UIO_USERSPACE :
		    UIO_SYSSPACE;
		uio.uio_fmode = 0;
		uio.uio_resid = iov.iov_len;
		uio.uio_pmp = NULL;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;

		/*
		 * NOTE: these next two lines mirror those that appear in
		 * I_PEEK, and are assumed necessary.
		 */
		uio.uio_pio = 0;
		uio.uio_pbuf = 0;

		while (mp && mp->b_datap->db_type != M_DATA &&
		       uio.uio_resid >= 0) {
			if ((n = MIN(uio.uio_resid, mp->b_wptr - mp->b_rptr)) &&
			    (error = STREAMS_UIOMOVE((caddr_t)mp->b_rptr,
						     n, UIO_READ, &uio))) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mp = mp->b_cont;
		}
		pn.ctlbuf.len = pn.ctlbuf.maxlen - uio.uio_resid;
	
		/*
		 * Now process DATA blocks, if any.
		 */
		iov.iov_base = pn.databuf.buf;
		iov.iov_len = pn.databuf.maxlen;
		uio.uio_iovcnt = 1;
		uio.uio_resid = iov.iov_len;
		uio.uio_pmp = NULL;

		/*
		 * NOTE: these next two lines mirror those that appear in
		 * I_PEEK, and are assumed necessary.
		 */
		uio.uio_pio = 0;
		uio.uio_pbuf = 0;

		while (mp && uio.uio_resid) {
			if ((n = MIN(uio.uio_resid, mp->b_wptr - mp->b_rptr)) &&
			    (error = STREAMS_UIOMOVE((caddr_t)mp->b_rptr,
						     n, UIO_READ, &uio))) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mp = mp->b_cont;
		}

		pn.databuf.len = pn.databuf.maxlen - uio.uio_resid;

		/*
		 * NOTE: previously we have had MP locks in place for the
		 * following copyout, however this now follows the equivalent
		 * IRIX code for I_PEEK.
		 */
		error = strcopyout((caddr_t)&pn, (caddr_t)ptr_arg,
		    sizeof(pn), STRPEEK, copyflag);

		STRHEAD_UNLOCK(stp);

		if (error)
			return (error);
		*rvalp = 1;
		return 0;
	}
#endif	/* CKPT */

	case I_FIND:
		/*
		 * Get module name.
		 */

	    {
		char mname[FMNAMESZ+1];
		queue_t *q;
		int i;
		
		error = STREAMS_COPYIN((caddr_t)ptr_arg, (caddr_t)mname,
				       FMNAMESZ+1, STRNAME, copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}

		/*
		 * Find module in fmodsw.
		 */
		if ((i = fmfind(mname)) < 0) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/* Look downstream to see if module is there. */
		for (q = stp->sd_wrq->q_next; q &&
		    (fmodsw[i].f_str->st_wrinit != q->q_qinfo); 
		    q = q->q_next)
			;

		fmrele(i);
		*rvalp =  (q ? 1 : 0);
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_PUSH:
		/*
		 * Push a module.
		 */

	    {
		register int i;
		register queue_t *q;
		register queue_t *tq;
		register qband_t *qbp;
		int idx;
		dev_t dummydev;
		uchar_t *rqbf, *wqbf;
		int renab, wenab;
		int nrband, nwband;
		char mname[FMNAMESZ+1];

		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (stp->sd_pushcnt >= nstrpush) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/*
		 * Get module name and look up in fmodsw.
		 */
		if (error = STREAMS_COPYIN((caddr_t)ptr_arg, 
		    			   (caddr_t)mname,
					   FMNAMESZ+1, STRNAME, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if ((idx = fmfind(mname)) < 0) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		rqbf = kmem_zalloc(NBAND, KM_SLEEP);
		wqbf = kmem_zalloc(NBAND, KM_SLEEP);

		while (stp->sd_flag & STWOPEN) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				kmem_free(rqbf, NBAND);
				kmem_free(wqbf, NBAND);
				STRHEAD_UNLOCK(stp);
				fmrele(idx);
				return (EAGAIN);
			}
			if (sleep((caddr_t)stp, STOPRI)) {
				kmem_free(rqbf, NBAND);
				kmem_free(wqbf, NBAND);
				STRHEAD_UNLOCK(stp);
				fmrele(idx);
				return (EINTR);
			}
			if (stp->sd_flag & (STRDERR|STWRERR|STRHUP|STPLEX)) {
				kmem_free(rqbf, NBAND);
				kmem_free(wqbf, NBAND);
				STRHEAD_UNLOCK(stp);
				fmrele(idx);
				return ((stp->sd_flag & STPLEX) ? EINVAL :
					 (stp->sd_werror ? stp->sd_werror :
					  stp->sd_rerror));
			}
		}

		stp->sd_flag |= STWOPEN;
		q = RD(stp->sd_wrq);
		renab = 0;
		if (q->q_flag & QWANTW) {
			renab = 1;
			rqbf[0] = 1;
		}
		nrband = (int)q->q_nband;
		for (i = 1, qbp = q->q_bandp; 
		     i <= nrband; i++) {
			if (qbp->qb_flag & QB_WANTW) {
				renab = 1;
				rqbf[i] = 1;
			}
			qbp = qbp->qb_next;
		}
		for (q = stp->sd_wrq->q_next; 
		     q && !q->q_qinfo->qi_srvp;
		     q = q->q_next)
			;
		wenab = 0;
		nwband = 0;
		if (q) {
			if (q->q_flag & QWANTW) {
				wenab = 1;
				wqbf[0] = 1;
			}
			nwband = (int)q->q_nband;
			for (i = 1, qbp = q->q_bandp; 
			     i <= nwband; i++) {
				if (qbp->qb_flag & QB_WANTW) {
					wenab = 1;
					wqbf[i] = 1;
				}
				qbp = qbp->qb_next;
			}
		}

		/*
		 * Revert to the global monitor if the module does not
		 * support full MP-ization.
		 */
		if (fmodsw[idx].f_str->st_rdinit->
		     qi_minfo->mi_locking != MULTI_THREADED)
			useglobalmon(stp->sd_wrq);

		/*
		 * Push new module and call its open routine
		 * via qattach().  Modules don't change device
		 * numbers, so just ignore local.i_push.dummydev here.
		 */
		 
		dummydev = vp->v_rdev;
		error = qattach(RD(stp->sd_wrq), &dummydev, 
				0, FMODSW, &fmodsw[idx], crp);
		fmrele(idx);

		if (!error) {
			stralloctty(vp, stp);
			stp->sd_pushcnt++;
		} else {
			tq = RD(stp->sd_wrq);
			for (q = backq(tq); 
			     q && !q->q_qinfo->qi_srvp;
			     q = backq(q))
				;
			if (q) {
				done = 0;
				if (rqbf[0] && 
				    !(tq->q_flag & QWANTW))
					done = 1;
				else
					rqbf[0] = 0;
				qbp = tq->q_bandp;
				for (i = 1; i <= nrband; i++) {
					if (rqbf[i] && !(qbp->qb_flag&QB_WANTW))
						done = 1;
					else
						rqbf[i] = 0;
					qbp = qbp->qb_next;
				}
				if (done) {
					qenable(q);
					for (i = 0; i <= nrband; i++) {
						if (rqbf[i])
							setqback(q, i);
					}
				}
			}
			for (q = stp->sd_wrq->q_next; 
			     q && !q->q_qinfo->qi_srvp;
			     q = q->q_next)
				;
			if (q) {
				done = 0;
				if (wqbf[0] && 
				    !(q->q_flag & QWANTW))
					done = 1;
				else
					wqbf[0] = 0;
				qbp = q->q_bandp;
				for (i = 1; i <= nwband; i++) {
					if (wqbf[i] && !(qbp->qb_flag&QB_WANTW))
						done = 1;
					else
						wqbf[i] = 0;
					qbp = qbp->qb_next;
				}
				if (done) {
					qenable(stp->sd_wrq);
					for (i = 0; i <= nwband; i++) {
						if (wqbf[i])
						    setqback(stp->sd_wrq, i);
					}
				}
			}
			goto pushdone;
		}

		/*
		 * If flow control is on, don't break it - enable
		 * first back queue with svc procedure.
		 */
		q = RD(stp->sd_wrq->q_next);
		if (q->q_qinfo->qi_srvp) {
			for (q = backq(q); 
			     q && !q->q_qinfo->qi_srvp;
			     q = backq(q))
				;
			if (q && renab) {
				qenable(q);
				for (i = 0; i <= nrband; i++) {
					if (rqbf[i])
						setqback(q, i);
				}
			}
		}
		q = stp->sd_wrq->q_next;
		if (q->q_qinfo->qi_srvp) {
			if (wenab) {
				qenable(stp->sd_wrq);
				for (i = 0; i <= nwband; i++) {
					if (wqbf[i])
					    setqback(stp->sd_wrq, i);
				}
			}
		}
pushdone:
		stp->sd_flag &= ~STWOPEN;
		kmem_free((caddr_t)rqbf, NBAND);
		kmem_free((caddr_t)wqbf, NBAND);
		wakeup(stp);
		STRHEAD_UNLOCK(stp);
		return (error);
	    }

	case I_POP:
		/*
		 * Pop module (if module exists).
		 */
	    {
		queue_t *q;

		if (stp->sd_flag&STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (!(q = stp->sd_wrq->q_next)) { /* for broken pipes */
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (q->q_next &&		/* module was pushed */
		    !(q->q_flag & QREADR)) {	/* ... from this side */
			qdetach(RD(q), 1, flag, crp);
			stp->sd_pushcnt--;
			STRHEAD_UNLOCK(stp);
			return (0);
		}
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	    }

	case I_LOOK:
		/*
		 * Get name of first module downstream.
		 * If no module, return an error.
		 */
	    {
		for (i = 0; i < fmodmax; i++) {
			if (fmhold(i))
				continue;
			if (fmodsw[i].f_str->st_wrinit ==
						stp->sd_wrq->q_next->q_qinfo) {
				STRHEAD_UNLOCK(stp);
				error = strcopyout((caddr_t)fmodsw[i].f_name,
						   (caddr_t)ptr_arg, FMNAMESZ+1,
						   STRNAME, copyflag);
				fmrele(i);
				return (error);
			}
			fmrele(i);
		}
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	    }

	case I_LINK:
	case I_PLINK:
		/* 
		 * Link a multiplexor.
		 */
	    {
		struct vfile *fpdown;
		struct linkinfo *linkp;
		struct stdata *stpdown;
		queue_t *rq;

		/*
		 * Test for invalid upper stream
		 */
		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (vp->v_type == VFIFO) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (!stp->sd_strtab->st_muxwinit) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (error = getf(int_arg, &fpdown)) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if (!VF_IS_VNODE(fpdown)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/*
		 * Test for invalid lower stream.
		 */

		stpdown = 
			VF_TO_VNODE(fpdown)->v_stream;
		if ((stpdown == NULL) ||
		    (stpdown == stp) || 
		    (stpdown->sd_flag &
		     (STPLEX|STRHUP|STRDERR|STWRERR|IOCWAIT)) ||
		    linkcycle(stp, stpdown)) {
		        STRHEAD_UNLOCK(stp);
		        return (EINVAL);
		}
		if (cmd == I_PLINK)
			rq = NULL;
		else
			rq = getendq(stp->sd_wrq);
		if (!(linkp = alloclink(rq, 
		      stpdown->sd_wrq, fpdown))) {
			STRHEAD_UNLOCK(stp);
			return (EAGAIN);
		}
		strioc.ic_cmd = cmd;
		strioc.ic_timout = 0;
		strioc.ic_len = sizeof(struct linkblk);
		strioc.ic_dp = (char *)&linkp->li_lblk;
	
		/* Set up queues for link */
		rq = RD(stpdown->sd_wrq);
		/*
		 * XXXrs - join streams so they share one monitor.  We hold
		 *         an open file descriptor for the lower stream so
		 *	   it shouldn't close while we join.
		 *
		 *	   In future, might be good to allow lower streams
		 *	   to hold private monitors and invent a fancy
		 *	   UPPER_STREAMS_INTERRUPT that could be used to
		 *	   pass data from a lower stream on one monitor to
		 *	   an upper stream on another monitor.
		 */
		JOINSTREAMS(stp->sd_wrq, &stpdown->sd_wrq);
		setq(rq, stp->sd_strtab->st_muxrinit,
		     stp->sd_strtab->st_muxwinit);
		rq->q_ptr = WR(rq)->q_ptr = NULL;
		rq->q_flag |= QWANTR;	
		WR(rq)->q_flag |= QWANTR;

		if (error = strdoioctl(stp, &strioc, NULL,
				       K_TO_K, STRLINK, crp, rvalp)) {
			lbfree(linkp);
			setq(rq, &strdata, &stwdata);
			rq->q_ptr = WR(rq)->q_ptr = (caddr_t)stpdown;

			STRHEAD_UNLOCK(stp);
			return (error);
		}

		stpdown->sd_flag |= STPLEX;
		VFILE_REF_HOLD(fpdown);
		if (error = mux_addedge(stp, stpdown, linkp->li_lblk.l_index)) {
			int type;
			if (cmd == I_LINK) {
				type = LINKIOCTL|LINKNORMAL;
				strioc.ic_cmd = I_UNLINK;
			} else {	/* I_PLINK */
				type = LINKIOCTL|LINKPERSIST;
				strioc.ic_cmd = I_PUNLINK;
			}
			(void) munlink(&vp->v_stream, linkp,
				       type, crp, rvalp);
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		/*
		 * Wake up any other processes that may have been
		 * waiting on the lower stream.  These will all
		 * error out.
		 */
		wakeup(rq);
		wakeup(WR(rq));
		wakeup(stpdown);
		*rvalp = linkp->li_lblk.l_index;
		STRHEAD_UNLOCK(stp);
		return (0);
	    }
	
	case I_UNLINK:
	case I_PUNLINK:
		/*
		 * Unlink a multiplexor.
		 * If arg is -1, unlink all links for which this is the
		 * controlling stream.  Otherwise, arg is a index number
		 * for a link to be removed.
		 */
	    {
		struct linkinfo *linkp;
		int type;
		
		if (vp->v_type == VFIFO) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (cmd == I_UNLINK)
			type = LINKIOCTL|LINKNORMAL;
		else	/* I_PUNLINK */
			type = LINKIOCTL|LINKPERSIST;
		if (int_arg == 0) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (int_arg == MUXID_ALL)
			error = munlinkall(&vp->v_stream, type, 
					   crp, rvalp);
		else {
			if (!(linkp = findlinks(stp, int_arg, 
			    				type))) {
				/* invalid user supplied index number */
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}
			error = munlink(&vp->v_stream, linkp, 
					type, crp, rvalp);
		}
		STRHEAD_UNLOCK(stp);
		return (error);
	    }

	case I_FLUSH:
		/*
		 * send a flush message downstream
		 * flush message can indicate 
		 * FLUSHR - flush read queue
		 * FLUSHW - flush write queue
		 * FLUSHRW - flush read/write queue
		 */
		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (int_arg & ~FLUSHRW) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		while (!putctl1(stp->sd_wrq->q_next, M_FLUSH, int_arg))
			if (error = strwaitbuf(1, BPRI_HI)) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}

#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */

		STRHEAD_UNLOCK(stp);
		return (0);

	case I_FLUSHBAND:
	    {
		struct bandinfo binfo;

		error = STREAMS_COPYIN((caddr_t)ptr_arg, (caddr_t)&binfo,
				       sizeof(struct bandinfo), STRBANDINFO,
				       copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (binfo.bi_flag & ~FLUSHRW) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		while (!(mp = allocb(2, BPRI_HI))) {
			if (error = strwaitbuf(2, BPRI_HI)) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
		}
		if (binfo.bi_flag & FLUSHR)
			flushband(RD(stp->sd_wrq),binfo.bi_pri,FLUSHR);
		if (binfo.bi_flag & FLUSHW)
			flushband(stp->sd_wrq,binfo.bi_pri,FLUSHW);
		mp->b_datap->db_type = M_FLUSH;
		*mp->b_wptr++ = binfo.bi_flag | FLUSHBAND;
		*mp->b_wptr++ = binfo.bi_pri;
		putnext(stp->sd_wrq, mp);
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_SRDOPT:
		/*
		 * Set read options
		 *
		 * RNORM - default stream mode
		 * RMSGN - message no discard
		 * RMSGD - message discard
		 * RPROTNORM - fail read with EBADMSG for M_[PC]PROTOs
		 * RPROTDAT - convert M_[PC]PROTOs to M_DATAs
		 * RPROTDIS - discard M_[PC]PROTOs and retain M_DATAs
		 */
		if (int_arg & ~(RMODEMASK | RPROTMASK)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if ((int_arg & RMSGD) && (int_arg & RMSGN)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		switch (int_arg & RMODEMASK) {
		case RNORM: 
			stp->sd_flag &= ~(RMSGDIS | RMSGNODIS);
			break;
		case RMSGD:
			stp->sd_flag = (stp->sd_flag & ~RMSGNODIS) | RMSGDIS;
			break;
		case RMSGN:
			stp->sd_flag = (stp->sd_flag & ~RMSGDIS) | RMSGNODIS;
			break;
		}

		switch(int_arg & RPROTMASK) {
		case RPROTNORM:
			stp->sd_flag &= ~(RDPROTDAT | RDPROTDIS);
			break;

		case RPROTDAT:
			stp->sd_flag = ((stp->sd_flag & ~RDPROTDIS) |
			    RDPROTDAT);
			break;

		case RPROTDIS:
			stp->sd_flag = ((stp->sd_flag & ~RDPROTDAT) |
			    RDPROTDIS);
			break;
		}
		STRHEAD_UNLOCK(stp);
		return (0);

	case I_GRDOPT:
		/*
		 * Get read option and return the value
		 * to spot pointed to by arg
		 */
	    {
		int rdopt;

		rdopt = ((stp->sd_flag & RMSGDIS) ? RMSGD :
			 ((stp->sd_flag & RMSGNODIS) ? RMSGN : RNORM));
		rdopt |= ((stp->sd_flag & RDPROTDAT) ? RPROTDAT :
			  ((stp->sd_flag & RDPROTDIS) ? RPROTDIS :
			   RPROTNORM));
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&rdopt, (caddr_t)ptr_arg,
				   sizeof(rdopt), STRINT, copyflag);
		return (error);
	    }

	case I_SETSIG:
		/*
		 * Register the calling proc to receive the SIGPOLL
		 * signal based on the events given in arg.  If
		 * arg is zero, remove the proc from register list.
		 */
	    {
		struct strevent *sep, *psep;

		psep = NULL;

		for (sep = stp->sd_siglist; 
		     sep && (sep->se_pid !=
		     current_pid()); 
		     psep = sep, 
		     sep = sep->se_next)
			;

		if (int_arg) {
			if (int_arg & ~(S_INPUT|S_HIPRI|S_MSG|S_HANGUP|S_ERROR|
			    S_RDNORM|S_WRNORM|S_RDBAND|S_WRBAND|S_BANDURG)) {
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}
			if ((int_arg & S_BANDURG) && !(int_arg & S_RDBAND)) {
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}

			/*
			 * If proc not already registered, add it
			 * to list.
			 */
			if (!sep) {
				if (!(sep = sealloc(SE_SLEEP))){
					STRHEAD_UNLOCK(stp);
					return (EAGAIN);
				}
				if (psep)
					psep->se_next = sep;
				else
					stp->sd_siglist = sep;
				sep->se_pid = current_pid();
			}

			/*
			 * Set events.
			 */
			sep->se_events = int_arg;
			stp->sd_sigflags |= int_arg;
			STRHEAD_UNLOCK(stp);
			return (0);

		}
		/*
		 * Remove proc from register list.
		 */
		if (sep) {
			if (psep)
				psep->se_next = 
				    sep->se_next;
			else
				stp->sd_siglist = sep->se_next;

			LOCK_STR_RESOURCE(s2);
			sefree(sep);
			UNLOCK_STR_RESOURCE(s2);
			/*
			 * Recalculate OR of sig events.
			 */
			stp->sd_sigflags = 0;
			for (sep = stp->sd_siglist; 
			     sep; 
			     sep = sep->se_next)
				stp->sd_sigflags |=
					sep->se_events;
			STRHEAD_UNLOCK(stp);
			return (0);
		}
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	    }

	case I_GETSIG:
		/*
		 * Return (in arg) the current registration of events
		 * for which the calling proc is to be signalled.
		 */
	    {
		struct strevent *sep;

		/* s = splstr(); XXXrs IRIX removed this from SVR3.2 */
		for (sep = stp->sd_siglist; sep; 
		     sep = sep->se_next)
			if (sep->se_pid == current_pid()) {
				STRHEAD_UNLOCK(stp);
				ASSERT(sizeof(sep->se_events) == sizeof(int));
				error = strcopyout((caddr_t)&sep->se_events,
				    		   (caddr_t)ptr_arg,
						   sizeof(int),
						   STRINT, copyflag);
				/* splx(s); XXXrs */
				return (error);
			}
		STRHEAD_UNLOCK(stp);
		/* splx(s); XXXrs */
		return (EINVAL);
	    }

	case I_PEEK:
	    {
		struct strpeek foopeek;
#if _MIPS_SIM == _ABI64
		struct irix5_strpeek i5_strpeek;
#endif
		int msgtype = 0;
#define STRCTL_TYPE	1
#define STRDATA_TYPE	2
		/*
		 * Insist the perpetrator legally be able to
		 *      read from the stream
		 */
		if ((flag & FREAD) == 0 &&
		    !_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
			error = EPERM;
			break;
		}

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
					(caddr_t)&i5_strpeek,
					sizeof(i5_strpeek),
						STRPEEK,
						copyflag);
			if (!error)
				irix5_strpeek_to_strpeek(
					&i5_strpeek,
					&foopeek);
		} else
#endif
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
						(caddr_t)&foopeek,
						sizeof(foopeek),
						STRPEEK,
						copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}

		mp = RD(stp->sd_wrq)->q_first;
		while (mp) {
			if (!(mp->b_flag & MSGNOGET))
				break;
			mp = mp->b_next;
		}
		if (!mp || ((foopeek.flags & RS_HIPRI) &&
		    queclass(mp) == QNORM)) {
			*rvalp = 0;
			STRHEAD_UNLOCK(stp);
			return (0);
		}

		if (mp->b_datap->db_type == M_PASSFP) {
			STRHEAD_UNLOCK(stp);
			return (EBADMSG);
		}

		if (mp->b_datap->db_type == M_PCPROTO) 
			foopeek.flags = RS_HIPRI;
		else
			foopeek.flags = 0;

		/*
		 * First process PROTO blocks, if any.
		 */
		iov.iov_base = foopeek.ctlbuf.buf;
		iov.iov_len = foopeek.ctlbuf.maxlen;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_segflg = (copyflag == U_TO_K) ? UIO_USERSPACE :
		    UIO_SYSSPACE;
		uio.uio_fmode = 0;
		uio.uio_resid = iov.iov_len;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		while (mp && mp->b_datap->db_type != M_DATA &&
		       uio.uio_resid >= 0) {
			msgtype |= STRCTL_TYPE;
			if ((i = MIN(uio.uio_resid, mp->b_wptr - mp->b_rptr)) &&
			    (error = STREAMS_UIOMOVE((caddr_t)mp->b_rptr, i,
						     UIO_READ, &uio))) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mp = mp->b_cont;
		}
		if (msgtype & STRCTL_TYPE)
			foopeek.ctlbuf.len = foopeek.ctlbuf.maxlen - uio.uio_resid;
		else
			foopeek.ctlbuf.len = -1;
	
		/*
		 * Now process DATA blocks, if any.
		 */
		while (mp && mp->b_datap->db_type != M_DATA)
			mp = mp->b_cont;
		iov.iov_base = foopeek.databuf.buf;
		iov.iov_len = foopeek.databuf.maxlen;
		uio.uio_iovcnt = 1;
		uio.uio_resid = iov.iov_len;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_pbuf = 0;
		while (mp && uio.uio_resid >= 0) {
			msgtype |= STRDATA_TYPE;
			if ((i = MIN(uio.uio_resid, mp->b_wptr - mp->b_rptr)) &&
			    (error = STREAMS_UIOMOVE((caddr_t)mp->b_rptr, i,
						     UIO_READ, &uio))) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			mp = mp->b_cont;
		}

		if (msgtype & STRDATA_TYPE)
			foopeek.databuf.len = foopeek.databuf.maxlen - uio.uio_resid;
		else
			foopeek.databuf.len = -1;
		STRHEAD_UNLOCK(stp);
#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			strpeek_to_irix5_strpeek(&foopeek, 
						 &i5_strpeek);
			error = strcopyout((caddr_t)&i5_strpeek,
					   (caddr_t)ptr_arg,
				   	   sizeof(i5_strpeek),
					   STRPEEK, copyflag);
		} else
#endif
			error = strcopyout((caddr_t)&foopeek,
					   (caddr_t)ptr_arg,
				   	   sizeof(foopeek),
					   STRPEEK, copyflag);
		if (error)
			return (error);
		*rvalp = 1;
		return (0);
#undef STRCTL_TYPE
#undef STRDATA_TYPE
	    }

	case I_FDINSERT:
	    {
		struct strfdinsert strfdinsert;
		struct vfile *resftp;
		struct stdata *resstp;
		queue_t *q;
		register long msgsize;
		long rmin, rmax;
#if _MIPS_SIM == _ABI64
		struct irix5_strfdinsert i5_strfdinsert;
#endif
		int sizeof_qptr;

		sizeof_qptr = sizeof(queue_t *);

		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}
		if (stp->sd_flag & (STRDERR|STWRERR|STPLEX)) {
			STRHEAD_UNLOCK(stp);
			return ((stp->sd_flag & STPLEX) ? EINVAL :
			    (stp->sd_werror ? stp->sd_werror : stp->sd_rerror));
		}
#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
				    (caddr_t)&i5_strfdinsert, 
				    sizeof(i5_strfdinsert), 
				    STRFDINSERT, copyflag);
			if (!error)
				irix5_strfdinsert_to_strfdinsert(
					&i5_strfdinsert, 
					&strfdinsert);
		} else
#endif
			error = STREAMS_COPYIN((caddr_t)ptr_arg,
					(caddr_t)&strfdinsert, 
					sizeof(strfdinsert),
					STRFDINSERT,
					copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
#if _MIPS_SIM == _ABI64
		if (!convert_abi) {
			/*
			 * This calculation is wrong if the pointer is 4-byte
			 * aligned in user-space, so we need to do
			 * this check later if we're converting ABI formats.
			 */
			if (strfdinsert.offset < 0 ||
			   (strfdinsert.offset % sizeof_qptr) != 0) {
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}
		}
#endif	/* _ABI64 */
		if (getf(strfdinsert.fildes, 
			 &resftp) ||
		    !VF_IS_VNODE(resftp) ||
		    ((resstp = 
		      VF_TO_VNODE(resftp)->v_stream) == NULL))
		{
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		if (resstp->sd_flag & (STRDERR|STWRERR|STRHUP|STPLEX)) {
			STRHEAD_UNLOCK(stp);
			return ((resstp->sd_flag & STPLEX) ? EINVAL :
			    (resstp->sd_werror ? resstp->sd_werror :
			    resstp->sd_rerror));
		}

		/* get read queue of stream telocal.i_fdinsert.rminus */
		for (q = resstp->sd_wrq->q_next; q->q_next; q = q->q_next)
			;
		q = RD(q);

#if _MIPS_SIM == _ABI64
		if (!convert_abi) {
			/*
			 * This calculation is wrong if the pointer is 4-byte
			 * aligned in user-space, so we need to do
			 * this check later if we're converting ABI formats.
			 */
			if (strfdinsert.ctlbuf.len < strfdinsert.offset
							+ sizeof_qptr) {
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}
		}
#endif	/* _ABI64 */
		/*
		 * Check for legal flag value.
		 */
		if (strfdinsert.flags & ~RS_HIPRI) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/*
		 * Make sure ctl and data sizes together fall within 
		 * the limits of the max and min receive packet sizes 
		 * and do not exceed system limit.  A negative data
		 * length means that no data part is to be sent.
		 */
		rmin = stp->sd_wrq->q_next->q_minpsz;
		rmax = stp->sd_wrq->q_next->q_maxpsz;
		ASSERT((rmax >= 0) || (rmax == INFPSZ));
		if (rmax == 0) {
			STRHEAD_UNLOCK(stp);
			return (ERANGE);
		}
		if (strmsgsz != 0) {
			if (rmax == INFPSZ)
				rmax = strmsgsz;
			else
				rmax = MIN(strmsgsz, rmax);
		}
		if ((msgsize = strfdinsert.databuf.len) < 0)
			msgsize = 0;
		if ((msgsize < rmin) ||
		    ((msgsize > rmax) && (rmax != INFPSZ)) ||
		    (strfdinsert.ctlbuf.len > strctlsz)) {
			STRHEAD_UNLOCK(stp);
			return (ERANGE);
		}

		while (!(strfdinsert.flags & RS_HIPRI) &&
		    !bcanput(stp->sd_wrq->q_next, 0)) {
#ifdef _IRIX_LATER
			if ((error = strwaitq(stp, WRITEWAIT, (off_t)0,
					      flag, &done))
#else
			if ((error = strwaitq(stp, WRITEWAIT, (off_t)0,
					      flag, 0, &done))
#endif /* _IRIX_LATER */
			    || done) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
		}

		iov.iov_base = strfdinsert.databuf.buf;
		iov.iov_len = strfdinsert.databuf.len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_segflg = (copyflag == U_TO_K) ? UIO_USERSPACE :
		    UIO_SYSSPACE;
		uio.uio_fmode = 0;
		uio.uio_resid = iov.iov_len;
		uio.uio_pmp = NULL;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		if ((error = strmakemsg(&strfdinsert.ctlbuf,
		    strfdinsert.databuf.len, &uio, stp,
		    strfdinsert.flags, &mp)) || !mp) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			/*
			 * Alignment and size checks that we skipped above
			 * will be done in irix5_fix_fdinsert()
			 */
			error = irix5_fix_fdinsert(&mp,
				&strfdinsert.offset);
			if (error) {
				freemsg(mp);
				STRHEAD_UNLOCK(stp);
				return error;
			}
		}
#endif
		/*
		 * Place pointer to queue 'offset' bytes from the
		 * start of the control portion of the message.
		 */

		*((queue_t **)(mp->b_rptr + strfdinsert.offset)) = q;

		/*
		 * Put message downstream.
		 */
		STREAMS_BLOCKQ();
		(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, mp);
		STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */
		STRHEAD_UNLOCK(stp);
		return (error);
	    }

	case I_SENDFD:
	    {
		register queue_t *qp;
		register struct strrecvfd *srf;
		struct vfile *fp;
		
		if (stp->sd_flag & STRHUP) {
			STRHEAD_UNLOCK(stp);
			return (ENXIO);
		}

		for (qp = stp->sd_wrq; 
		     qp->q_next; 
		     qp = qp->q_next)
			;
		if (qp->q_qinfo != &strdata) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		/*
		 * Get a reference to the file structure.
		 * We don't use getf/VFILE_REF_HOLD because getf screws
		 * up the fd inuse accounting logic when we're
		 * called from connld().
		 */
		fp = fdt_getref(curprocp, int_arg);
		if (fp == NULL) {
			STRHEAD_UNLOCK(stp);
			return (EBADF);
		}
		if ((qp->q_flag & QFULL) ||
		    !(mp = allocb(sizeof(struct strrecvfd), BPRI_MED))) {
			STRHEAD_UNLOCK(stp);
			VFILE_REF_RELEASE(fp);
			return (EAGAIN);
		}

		srf = (struct strrecvfd *)mp->b_rptr;
		mp->b_wptr += sizeof(struct strrecvfd);
		mp->b_datap->db_type = M_PASSFP;
		srf->f.fp = fp;
		srf->uid = crp->cr_uid;
		srf->gid = crp->cr_gid;
		strrput(qp, mp);
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_RECVFD:
	case I_E_RECVFD:
	case I_S_RECVFD:
	    {
		register struct strrecvfd *srf;
		int fd;
		struct cred *rcv_crp;
		struct vfile *fp;
		union {
			/* user view of EFT data structure */
			struct user_e_strrecvfd estrfd;
			struct o_strrecvfd ostrfd; /* non-EFT data structure -
						    * SVR3 compatibility mode.
						    */
		} str;
		
		while (!(mp = getq(RD(stp->sd_wrq)))) {
			if (stp->sd_flag & STRHUP) {
				STRHEAD_UNLOCK(stp);
				return (ENXIO);
			}
#ifdef _IRIX_LATER
			if ((error = strwaitq(stp, GETWAIT, (off_t) 0,
					      flag, &done))
#else
			if ((error = strwaitq(stp, GETWAIT, (off_t) 0,
					      flag, 0, &done))
#endif /* _IRIX_LATER */
			    || done) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
		}
		if (mp->b_datap->db_type != M_PASSFP) {
			putbq(RD(stp->sd_wrq), mp);
			STRHEAD_UNLOCK(stp);
			return (EBADMSG);
		}
		srf = (struct strrecvfd *)mp->b_rptr;

 		/*
 		 * make sure the process receiving this file
 		 * descriptor has the appropriate domination and/or
 		 * privileges to use this file descriptor.
 		 */
		fp = srf->f.fp;

		if (error = fdt_alloc(fp, &fd)) {
			putbq(RD(stp->sd_wrq), mp);
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		*rvalp = 0;

		if (cmd == I_S_RECVFD)
			rcv_crp = fp->vf_cred;
		srf->f.fd = fd;

		if (cmd == I_RECVFD) {
			/* check to see if uid/gid values are too large. */

			if (srf->uid > USHRT_MAX || srf->gid > USHRT_MAX) {
				error = EOVERFLOW;
				goto fderror;
			}
			str.ostrfd.f.fd = srf->f.fd;
			str.ostrfd.uid = (o_uid_t) srf->uid;
			str.ostrfd.gid = (o_gid_t) srf->gid;
			for(i = 0; i < 8; i++)
				str.ostrfd.fill[i] = 0;

			if (error = STREAMS_COPYOUT(
			    		(caddr_t)&str.ostrfd,
					(caddr_t)ptr_arg,
					sizeof(struct o_strrecvfd),
					O_STRRECVFD, copyflag)) {
				goto fderror;
			}
		} else if (cmd == I_S_RECVFD) {
			struct s_strrecvfd *sstrfd = (struct s_strrecvfd *)NULL;
			size_t sstrfdsize = 0;

			if (sstrfdsize == 0)
				sstrfdsize = sizeof(struct s_strrecvfd) +
				    crgetsize() - sizeof(struct sub_attr);
			if ((sstrfd = (struct s_strrecvfd *)
			    kmem_alloc(sstrfdsize, KM_SLEEP)) == NULL) {
				error = ENOMEM;
				goto fderror;
			}

			sstrfd->fd = srf->f.fd;
			sstrfd->uid = (uid_t)srf->uid;
			sstrfd->gid = (gid_t)srf->gid;
			bcopy((caddr_t)rcv_crp, (caddr_t)&sstrfd->s_attrs,
			    crgetsize());

			if (error = STREAMS_COPYOUT((caddr_t)sstrfd,
						    (caddr_t)ptr_arg,
			    			    sstrfdsize, S_STRRECVFD,
						    copyflag)) {
                                kmem_free(sstrfd, sstrfdsize);
				goto fderror;
			}
                        kmem_free(sstrfd, sstrfdsize);
		} else {		/* I_E_RECVFD */
			str.estrfd.fd = srf->f.fd;
			str.estrfd.uid = (uid_t)srf->uid;
			str.estrfd.gid = (gid_t)srf->gid;
			for(i = 0; i < 8; i++)
				str.estrfd.fill[i] = 0;

			if (error = STREAMS_COPYOUT(
			    		(caddr_t)&str.estrfd,
					(caddr_t)ptr_arg,
					sizeof(str.estrfd),
					STRRECVFD, copyflag)) {
				goto fderror;
			}
		}
		freemsg(mp);
		STRHEAD_UNLOCK(stp);
		return (0);
fderror:
		srf->f.fp = fp;	/* XXX why?? */
		putbq(RD(stp->sd_wrq), mp);
		(void) fdt_alloc_undo(fd);	
		STRHEAD_UNLOCK(stp);
		return (error);
	    }

	case I_SWROPT: 
		/*
		 * Set/clear the write options. arg is a bit
		 * mask with any of the following bits set...
		 * 	SNDZERO - send zero length message
		 *	SNDPIPE - send sigpipe to process if
		 *		sd_werror is set and process is
		 *		doing a write or putmsg.
		 *	SNDHOLD - set strhold feature
		 * The new stream head write options should reflect
		 * what is in arg.
		 */
		if (int_arg & ~(SNDZERO|SNDPIPE|SNDHOLD)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		stp->sd_flag &= ~(STRSNDZERO|STRSIGPIPE|STRHOLD);
		if (int_arg & SNDZERO)
			stp->sd_flag |= STRSNDZERO;
		if (int_arg & SNDPIPE)
			stp->sd_flag |= STRSIGPIPE;
		if (int_arg & SNDHOLD)
			stp->sd_flag |= STRHOLD;

		STRHEAD_UNLOCK(stp);
		return (0);

	case I_GWROPT:
	    {
		int wropt = 0;

		if (stp->sd_flag & STRSNDZERO)
			wropt |= SNDZERO;
		if (stp->sd_flag & STRSIGPIPE)
			wropt |= SNDPIPE;
		if (stp->sd_flag & STRHOLD)
			wropt |= SNDHOLD;
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&wropt, (caddr_t)ptr_arg,
				   sizeof(wropt), STRINT, copyflag);
		return (error);
	    }

	case I_LIST:
		/* 
		 * Returns all the modules found on this stream,
		 * up to the driver. If argument is NULL, return the
		 * number of modules (including driver). If argument
		 * is not NULL, copy the names into the structure
		 * provided.
		 */

	    {
		queue_t *q;
		int num_modules, space_allocated;
		struct str_list strlist;
#if _MIPS_SIM == _ABI64
		struct irix5_str_list i5_strlist;
#endif
		if (ptr_arg == NULL) {
			/* Return number of modules plus driver */
			q = stp->sd_wrq;
			*rvalp = stp->sd_pushcnt + 1;
			STRHEAD_UNLOCK(stp);
		} else {
#if _MIPS_SIM == _ABI64
			if (convert_abi) {
				error = STREAMS_COPYIN((caddr_t)ptr_arg,
					    (caddr_t)&i5_strlist,
					    sizeof(i5_strlist),
					    STRLIST, copyflag);
				if (!error)
					irix5_str_list_to_str_list(
						&i5_strlist, 
						&strlist);
			} else
#endif
				error = STREAMS_COPYIN((caddr_t)ptr_arg,
							(caddr_t)&strlist,
							sizeof(struct str_list),
							STRLIST, copyflag);
			if (error) {
				STRHEAD_UNLOCK(stp);
				return (error);
			}
			if ((space_allocated = strlist.sl_nmods) <= 0)  {
				STRHEAD_UNLOCK(stp);
				return (EINVAL);
			}
			q = stp->sd_wrq;
			num_modules = 0;
			while (SAMESTR(q) && 
			       (space_allocated != 0)) {
				error = STREAMS_COPYOUT(
			       (caddr_t)q->q_next->q_qinfo->qi_minfo->mi_idname,
			       (caddr_t)strlist.sl_modlist, 
			       (FMNAMESZ + 1),
			       STRNAME, copyflag);

				if (error) {
					STRHEAD_UNLOCK(stp);
					return (error);
				}
				q = q->q_next;
				space_allocated--;
				num_modules++;
				strlist.sl_modlist++;
			}
			STRHEAD_UNLOCK(stp);
			/* arg points to strlist.sl_nmods in user space */
			error = strcopyout((caddr_t)&num_modules,
					   (caddr_t)ptr_arg, sizeof(int),
					   STRINT, copyflag);
		}
		return (error);
	    }

	case I_CKBAND:
	    {
		queue_t *q;
		qband_t *qbp;
		
		q = RD(stp->sd_wrq);		
		
		/*
		 * Ignores MSGNOGET.
		 */
		if ((int_arg < 0) || (int_arg >= NBAND)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		if (int_arg > (int)q->q_nband) {
			*rvalp = 0;
		} else {
			if (int_arg == 0) {
				if (q->q_first)
					*rvalp = 1;
				else
					*rvalp = 0;
			} else {
				qbp = q->q_bandp;
				while (--int_arg > 0)
					qbp = qbp->qb_next;
				if (qbp->qb_first)
					*rvalp = 1;
				else
					*rvalp = 0;
			}
		}
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_GETBAND:
	    {
		int intpri;

		/*
		 * Ignores MSGNOGET.
		 */
		mp = RD(stp->sd_wrq)->q_first;
		if (!mp) {
			STRHEAD_UNLOCK(stp);
			return (ENODATA);
		}
		intpri = (int)mp->b_band;
		STRHEAD_UNLOCK(stp);
		error = strcopyout(
				(caddr_t)&intpri, 
				(caddr_t)ptr_arg,
				 sizeof(int), STRINT, copyflag);
		return (error);
	    }

	case I_ATMARK:
	    {
		queue_t *q;
		
		q = RD(stp->sd_wrq);

		if (int_arg & ~(ANYMARK|LASTMARK)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		mp = q->q_first;

		/*
		 * Hack for sockets compatibility.  We need to
		 * ignore any messages at the stream head that
		 * are marked MSGNOGET and are not marked MSGMARK.
		 */
		while (mp && ((mp->b_flag & (MSGNOGET|MSGMARK)) == MSGNOGET))
			mp = mp->b_next;

		if (!mp)
			*rvalp = 0;
		else if ((int_arg == ANYMARK) && (mp->b_flag & MSGMARK))
			*rvalp = 1;
		else if ((int_arg == LASTMARK) && (mp == stp->sd_mark))
			*rvalp = 1;
		else
			*rvalp = 0;
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_CANPUT:
	    {
		char band;

		if ((int_arg < 0) || (int_arg >= NBAND)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		band = (char)int_arg;
		*rvalp = bcanput(stp->sd_wrq->q_next, band);
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_SETCLTIME:
	    {
		/* AT+T had this as a long, but sd_closetime is
		 * specified as an int, and SVID does not specify.
		 */
		__int32_t closetime;

		error = STREAMS_COPYIN((caddr_t)ptr_arg, 
				(caddr_t)&closetime,
			       	sizeof(__int32_t), STRLONG, copyflag);
		if (error) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if (closetime < 0) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		/*
		 *  Convert between milliseconds and clock ticks.
		 */
		stp->sd_closetime = ((closetime / 1000) * HZ) +
		    ((((closetime % 1000) * HZ) + 999) / 1000);
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case I_GETCLTIME:
	    {
		/* AT+T had this as a long, but sd_closetime is
		 * specified as an int, and SVID specifies "the
		 * integer pointed to by arg".
		 * 
		 */
		__int32_t closetime;

		/*
		 * Convert between clock ticks and milliseconds.
		 */
		closetime = ((stp->sd_closetime / HZ) * 1000) +
		    (((stp->sd_closetime % HZ) * 1000) / HZ);
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&closetime, (caddr_t)ptr_arg,
		    		   sizeof(closetime), STRLONG, copyflag);
		return (error);
	    }

	case TIOCGSID:
		if (stp->sd_vsession == NULL) {
			STRHEAD_UNLOCK(stp);
			return (ENOTTY);
		}
		sid = stp->sd_vsession->vs_sid;
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&sid, (caddr_t)ptr_arg,
				   sizeof(pid_t), STRPIDT, copyflag);
		return (error);

	case TIOCNOTTY:		/* disconnect from tty */
		vpr = VPROC_LOOKUP(current_pid());
		ASSERT(vpr);
		VPROC_CTTY_CLEAR(vpr, error);
		VPROC_RELE(vpr);
		STRHEAD_UNLOCK(stp);
		return(error);

	case TIOCSPGRP:
	    {
		pid_t pgrp;
		
		vpr = VPROC_LOOKUP(current_pid());
		ASSERT(vpr);
		VPROC_GET_ATTR(vpr, VGATTR_SID, &attr);
		VPROC_RELE(vpr);
		/*
		 * ADDED FOR POSIX: ensure that caller is effecting
		 * its own controlling terminal
		 */
		vsp = stp->sd_vsession;
		if (vsp == NULL || vsp->vs_sid != attr.va_sid) {
			STRHEAD_UNLOCK(stp);
			return (ENOTTY);
		}
		if (STREAMS_COPYIN((caddr_t)ptr_arg, (caddr_t)&pgrp,
				   sizeof(pgrp), NULL, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return (EFAULT);
		}
		if ( (pgrp < 0) || (pgrp > MAXPID) ) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}

		if ((flag & FREAD) == 0 &&
		    !_CAP_CRABLE(crp, CAP_STREAMS_MGT)) {
			STRHEAD_UNLOCK(stp);
			return (EPERM);
		}

		/*
		 * Also POSIX: ensure that the new pgrp is an active
		 * pgrp in this proc's session. Special case: to allow
		 * calling child to grab controlling terminal while
		 * in foreground grp then call setpgrp(), let this
		 * ioctl succeed if pgrp == child's pid.
		 *
		 * Need to lookup the pgrp and get the session to which
		 * it belongs in order to check that the caller is a
		 * member of this session.
		 * XXX calling process' session id!
		 */
		if (current_pid() != pgrp) {
			pid_t	sid;
			int	is_orphaned;
			vpgrp_t	*vpg = VPGRP_LOOKUP(pgrp);
			if (vpg == NULL) {
				STRHEAD_UNLOCK(stp);
				return (EPERM);
			}
			VPGRP_GETATTR(vpg, &sid, &is_orphaned, NULL);
			VPGRP_RELE(vpg);
			if (sid != attr.va_sid) {
				STRHEAD_UNLOCK(stp);
				return (EPERM);
			}
		}
		stp->sd_pgid = pgrp;
		STRHEAD_UNLOCK(stp);
		return (0);
	    }

	case TIOCGPGRP:
		/*
		 * If device is not the process's controlling terminal
		 * or if the process does not have a controlling terminal,
		 * return ENOTTY
		 */
		vpr = VPROC_LOOKUP(current_pid());
		ASSERT(vpr);
		VPROC_GET_ATTR(vpr, VGATTR_SID, &attr);
		VPROC_RELE(vpr);
		vsp = stp->sd_vsession;
		if (vsp == NULL || vsp->vs_sid != attr.va_sid) {
			STRHEAD_UNLOCK(stp);
			return (ENOTTY);
		}
		STRHEAD_UNLOCK(stp);
		error = strcopyout((caddr_t)&stp->sd_pgid, (caddr_t)ptr_arg,
				   sizeof(pid_t), STRPIDT, copyflag);
		return (error);

	case FIONBIO:
		if (error = STREAMS_COPYIN((caddr_t)ptr_arg, (caddr_t)&int_arg,
					   sizeof(int_arg), NULL, copyflag)) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		if (int_arg)
			stp->sd_flag |= STFIONBIO;
		else
			stp->sd_flag &= ~STFIONBIO;

		STRHEAD_UNLOCK(stp);
		return (0);

	case FIOASYNC:
		STRHEAD_UNLOCK(stp);
		return (0);	/* handled by the upper layer */

	}
	/* NOTREACHED */
}

static timespec_t backoff = { 0, 1L };	/* a minimal backoff */

/*
 * Send an ioctl message downstream and wait for acknowledgement.
 * Monitor must be held here.
 */
int
strdoioctl(struct stdata *stp,
	   struct strioctl *strioc,
	   mblk_t *ebp,
	   int copyflag,
	   char *fmtp,
	   cred_t *crp,
	   int *rvalp)
{
	register mblk_t *bp;
	struct iocblk *iocbp;
	struct copyreq *reqp;
	struct copyresp *resp;
	mblk_t *fmtbp;
	int id;
	int transparent = 0;
	int error = 0;
	int len = 0;
	int locked;
	caddr_t taddr;
	extern void str2time(), str3time();

	ASSERT(STREAM_LOCKED(RD(stp->sd_wrq)));

	if (strioc->ic_len == TRANSPARENT) {	/* send arg in M_DATA block */
		transparent = 1;
#if _MIPS_SIM == _ABI64
		if (ABI_IS_64BIT(get_current_abi()))
			/* 64 bit on _ABI64 machines */
			strioc->ic_len = sizeof(long);
		else
			strioc->ic_len = sizeof(int);
#else
		strioc->ic_len = sizeof(int);
#endif
	}
	
	if ((strioc->ic_len < 0) ||
	    ((strmsgsz > 0) && (strioc->ic_len > strmsgsz))) {
		if (ebp)
			freeb(ebp);
		return (EINVAL);
	}

	while (!(bp = allocb(max(sizeof(struct iocblk),
			     sizeof(struct copyreq)),
			     BPRI_HI))) {
		if (error = strwaitbuf(sizeof(struct iocblk), BPRI_HI)) {
			if (ebp)
				freeb(ebp);
			return (error);
		}
	}

	iocbp = (struct iocblk *)bp->b_wptr;
	iocbp->ioc_count = strioc->ic_len;
	iocbp->ioc_cmd = strioc->ic_cmd;
	crhold(crp);
	iocbp->ioc_cr = crp;
	iocbp->ioc_error = 0;
	iocbp->ioc_rval = 0;
#if _MIPS_SIM == _ABI64
	iocbp->ioc_64bit = ABI_IS_64BIT(get_current_abi());
#endif
	bp->b_datap->db_type = M_IOCTL;
	bp->b_wptr += sizeof(struct iocblk);

	/*
	 * If there is data to copy into ioctl block, do so.
	 */
	if (iocbp->ioc_count) {
		if (transparent)
			id = K_TO_K;
		else
			id = copyflag;
		if (error = putiocd(bp, ebp, strioc->ic_dp,
				    id, SE_NOSLP, fmtp)) {
			freemsg(bp);
			if (ebp)
				freeb(ebp);
			crfree(crp);
			return (error);
		}

		/*
		 * We could have slept copying in user pages.
		 * Recheck the stream head state (the other end
		 * of a pipe could have gone away).
		 */
		if (stp->sd_flag & (STRHUP|STRDERR|STWRERR|STPLEX)) {
			error = ((stp->sd_flag & STPLEX) ? EINVAL :
			    (stp->sd_werror ? stp->sd_werror : stp->sd_rerror));
			freemsg(bp);
			crfree(crp);
			return (error);
		}
	} else {
		bp->b_cont = ebp;
	}

	if (transparent)
		iocbp->ioc_count = TRANSPARENT;

	if (!(stp->sd_flag & IOCWAIT) && stp->sd_iocwait) {
		STRQUEUE_UNLOCK(&stp->sd_monp);
		/*
		 * force a wakeup on everyone in the loop if they are
		 * asleep
		 */
		wakeup(&(stp->sd_iocwait));
		/* now delay a moment to allow any to run */
		nano_delay(&backoff);
		STRQUEUE_LOCK(&stp->sd_monp, locked);
	}

	/*
	 * Block for up to STRTIMOUT sec if there is a outstanding
	 * ioctl for this stream already pending.  All processes
	 * sleeping here will be awakened as a result of an ACK
	 * or NAK being received for the outstanding ioctl, or
	 * as a result of the timer expiring on the outstanding
	 * ioctl (a failure), or as a result of any waiting
	 * process's timer expiring (also a failure).
	 */
	stp->sd_flag |= STR2TIME;
	id = MP_STREAMS_TIMEOUT(stp->sd_wrq->q_monpp, str2time,
				stp, STRTIMOUT*HZ);

	while (stp->sd_flag & IOCWAIT) {
		stp->sd_iocwait++;
		if (sleep((caddr_t)&stp->sd_iocwait,STIPRI) ||
		   !(stp->sd_flag & STR2TIME)) {
			stp->sd_iocwait--;
			error = ((stp->sd_flag & STR2TIME) ? EINTR : ETIME);
			if (!stp->sd_iocwait)
				stp->sd_flag &= ~STR2TIME;

			untimeout(id);
			freemsg(bp);
			crfree(crp);
			return (error);
		}
		stp->sd_iocwait--;
		if (stp->sd_flag & (STRHUP|STRDERR|STWRERR|STPLEX)) {
			error = ((stp->sd_flag & STPLEX) ? EINVAL :
			    (stp->sd_werror ? stp->sd_werror : stp->sd_rerror));
			if (!stp->sd_iocwait)
				stp->sd_flag &= ~STR2TIME;

			untimeout(id);
			freemsg(bp);
			crfree(crp);
			return (error);
		}
	}
	untimeout(id);
	if (!stp->sd_iocwait)
		stp->sd_flag &= ~STR2TIME;

	/*
	 * Have control of ioctl mechanism.
	 * Send down ioctl packet and wait for response.
	 */
	if (stp->sd_iocblk) {
		freemsg(stp->sd_iocblk);
		stp->sd_iocblk = NULL;
	}
	stp->sd_flag |= IOCWAIT;

	/* 
	 * Assign sequence number.
	 */
	iocbp->ioc_id = (stp->sd_iocid = (ulong) atomicAddUint(&ioc_id, 1));

	STREAMS_BLOCKQ();
	(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, bp);
	STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
	if (qready())
		runqueues();
#endif /* _EAGER_RUNQUEUES */

	/*
	 * Timed wait for acknowledgment.  The wait time is limited by the
	 * timeout value, which must be a positive integer (number of seconds
	 * to wait, or 0 (use default value of STRTIMOUT seconds), or -1
	 * (wait forever).  This will be awakened either by an ACK/NAK
	 * message arriving, the timer expiring, or the timer expiring 
	 * on another ioctl waiting for control of the mechanism.
	 */
waitioc:
	if (!(stp->sd_flag & STR3TIME) && strioc->ic_timout >= 0)
		id = MP_STREAMS_TIMEOUT(stp->sd_wrq->q_monpp, str3time, stp,
		      (strioc->ic_timout ? strioc->ic_timout: STRTIMOUT) * HZ);

	stp->sd_flag |= STR3TIME;
	/*
	 * If the reply has already arrived, don't sleep.  If awakened from
	 * the sleep, fail only if the reply has not arrived by then.
	 * Otherwise, process the reply.
	 */
	while (!stp->sd_iocblk) {
		if (stp->sd_flag & (STRDERR|STWRERR|STPLEX|STRHUP)) {
			error = ((stp->sd_flag & STPLEX) ? EINVAL :
			    (stp->sd_werror ? stp->sd_werror : stp->sd_rerror));
			stp->sd_flag &= ~(STR3TIME|IOCWAIT);
			if (strioc->ic_timout >= 0)
				untimeout(id);

			wakeup(&(stp->sd_iocwait));
			crfree(crp);
			return (error);
		}

		if (sleep((caddr_t)stp,STIPRI) ||
		    !(stp->sd_flag & STR3TIME))  {
			error = ((stp->sd_flag & STR3TIME) ? EINTR : ETIME);
			stp->sd_flag &= ~(STR3TIME|IOCWAIT);
			bp = NULL;
			/*
			 * A message could have come in after we were scheduled
			 * but before we were actually run.
			 */
			if (stp->sd_iocblk) {
				bp = stp->sd_iocblk;
				stp->sd_iocblk = NULL;
			}
			if (strioc->ic_timout >= 0)
				untimeout(id);
			if (bp) {
				if ((bp->b_datap->db_type == M_COPYIN) ||
				    (bp->b_datap->db_type == M_COPYOUT)) {
					if (bp->b_cont) {
						freemsg(bp->b_cont);
						bp->b_cont = NULL;
					}
					bp->b_datap->db_type = M_IOCDATA;
					resp = (struct copyresp *)bp->b_rptr;
					resp->cp_rval = (caddr_t)1L;	/* failure */
					STREAMS_BLOCKQ();
					(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, bp);
					STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
					if (qready())
						runqueues();
#endif /* _EAGER_RUNQUEUES */
				} else
					freemsg(bp);
			}
			wakeup(&(stp->sd_iocwait));
			crfree(crp);
			return (error);
		}
	}
	ASSERT(stp->sd_iocblk);
	bp = stp->sd_iocblk;
	stp->sd_iocblk = NULL;
	if ((bp->b_datap->db_type == M_IOCACK) || (bp->b_datap->db_type == M_IOCNAK)) {
		stp->sd_flag &= ~(STR3TIME|IOCWAIT);
		if (strioc->ic_timout >= 0)
			untimeout(id);

		wakeup(&(stp->sd_iocwait));
	}
	/*
	 * Have received acknowlegment.
	 */

	switch (bp->b_datap->db_type) {
	case M_IOCACK:
		/*
		 * Positive ack.
		 */
		iocbp = (struct iocblk *)bp->b_rptr;

		/*
		 * Set error if indicated.
		 */
		if (iocbp->ioc_error) {
			error = iocbp->ioc_error;
			break;
		}

		/*
		 * Set return value.
		 */
		*rvalp = iocbp->ioc_rval;

		/*
		 * Data may have been returned in ACK message (ioc_count > 0).
		 * If so, copy it out to the user's buffer.
		 */
		if (iocbp->ioc_count && !transparent) {
			if (strioc->ic_cmd == __NEW_TCGETA ||
			    strioc->ic_cmd == __NEW_TCGETS ||
			    /* don't need __OLD_TCGET[AS] because they */
			    /* are converted to __NEW_* in strioctl() */
			    strioc->ic_cmd == TIOCGETP ||
			    strioc->ic_cmd == LDGETT) {
				if (error = getiocd(bp, strioc->ic_dp,
						    copyflag, fmtp))
					break;
			} else if (error = getiocd(bp, strioc->ic_dp,
						   copyflag, (char *)NULL)) {
				break;
			}
		}
		if (!transparent) {
			if (len)	/* an M_COPYOUT was used with I_STR */
				strioc->ic_len = len;
			else
				strioc->ic_len = iocbp->ioc_count;
		}
		break;

	case M_IOCNAK:
		/*
		 * Negative ack.
		 *
		 * The only thing to do is set error as specified
		 * in neg ack packet.
		 */
		iocbp = (struct iocblk *)bp->b_rptr;

		error = (iocbp->ioc_error ? iocbp->ioc_error : EINVAL);
		break;

	case M_COPYIN:
		/*
		 * Driver or module has requested user ioctl data.
		 */
		reqp = (struct copyreq *)bp->b_rptr;
		fmtbp = bp->b_cont;
		bp->b_cont = NULL;
		if (reqp->cq_flag & RECOPY) {
			/* redo I_STR copyin with canonical processing */
			ASSERT(fmtbp);
			reqp->cq_size = strioc->ic_len;
			error = putiocd(bp, NULL, strioc->ic_dp,
					copyflag, SE_SLEEP,
					(fmtbp ? (char *)fmtbp->b_rptr :
								(char *)NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else if (reqp->cq_flag & STRCANON) {
			/* copyin with canonical processing */
			ASSERT(fmtbp);
			error = putiocd(bp, NULL, reqp->cq_addr,
					copyflag, SE_SLEEP,
			    		(fmtbp ? (char *)fmtbp->b_rptr :
								(char *) NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else {
			/* copyin raw data (i.e. no canonical processing) */
			error = putiocd(bp, NULL, reqp->cq_addr, copyflag,
					SE_SLEEP, (char *)NULL);
		}
		if (error && bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}

		bp->b_wptr = bp->b_rptr + sizeof(struct copyresp);
		bp->b_datap->db_type = M_IOCDATA;
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)(__psint_t)error;
#if _MIPS_SIM == _ABI64
		resp->cp_64bit = ABI_IS_64BIT(get_current_abi());
#endif

		STREAMS_BLOCKQ();
		(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, bp);
		STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */

		if (error) {
			if (strioc->ic_timout >= 0)
				untimeout(id);
			crfree(crp);
			return (error);
		}

		goto waitioc;

	case M_COPYOUT:
		/*
		 * Driver or module has ioctl data for a user.
		 */
		reqp = (struct copyreq *)bp->b_rptr;
		ASSERT(bp->b_cont);
		if (transparent || reqp->cq_flag & STRCOPYTRANS) {
			taddr = reqp->cq_addr;
		} else {
			taddr = strioc->ic_dp;
			len = reqp->cq_size;
		}
		if (reqp->cq_flag & STRCANON) {
			/* copyout with canonical processing */
			if (fmtbp = bp->b_cont) {
				bp->b_cont = fmtbp->b_cont;
				fmtbp->b_cont = NULL;
			}
			error = getiocd(bp, taddr, copyflag,
			    (fmtbp ? (char *)fmtbp->b_rptr : (char *)NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else {
			/* copyout raw data (i.e. no canonical processing) */
			error = getiocd(bp, taddr, copyflag, (char *)NULL);
		}
		freemsg(bp->b_cont);
		bp->b_cont = NULL;

		bp->b_wptr = bp->b_rptr + sizeof(struct copyresp);
		bp->b_datap->db_type = M_IOCDATA;
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)(__psint_t)error;

		STREAMS_BLOCKQ();
		(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, bp);
		STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */

		if (error) {
			if (strioc->ic_timout >= 0)
				untimeout(id);
			crfree(crp);
			return (error);
		}
		goto waitioc;

	default:
		ASSERT(0);
		break;
	}

	freemsg(bp);
	crfree(crp);
	return (error);
}

/*
 * Get the next message from the read queue.  If the message is 
 * priority, STRPRI will have been set by strrput().  This flag
 * should be reset only when the entire message at the front of the
 * queue as been consumed.
 */
int
strgetmsg(register struct vnode *vp,
	  register struct strbuf *mctl,
	  register struct strbuf *mdata,
	  unsigned char *prip,
	  int *flagsp,
	  int fmode,
	  rval_t *rvp)
{
	register struct stdata *stp;
	register mblk_t *bp, *nbp;
	mblk_t *savemp = NULL;
	mblk_t *savemptail = NULL;
	int n, bcnt;
	int done = 0;
	int flg = 0;
	int more = 0;
	int error = 0;
	char *ubuf;
	int mark;
	unsigned char pri;
	queue_t *q;

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);
	ASSERT(stp);

	q = RD(stp->sd_wrq);
	rvp->r_val1 = 0;

	if (stp->sd_flag & (STRDERR|STPLEX)) {
		STRHEAD_UNLOCK(stp);
		return ((stp->sd_flag & STPLEX) ? EINVAL : stp->sd_rerror);
	}

	switch (*flagsp) {
	case MSG_HIPRI:
		if (*prip != 0) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		break;

	case MSG_ANY:
	case MSG_BAND:
		break;

	default:
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	}

	mark = 0;
	for (;;) {
		/*
		 * job control
		 *
		 * Block the process if it's in background and it's
		 * trying to read its controlling terminal.
		 */
		if (inbackground(stp))
		{
			STRHEAD_UNLOCK(stp);

			if (error = accessctty(JCTLREAD)) {
				return (error);
			}

			STRHEAD_LOCK(&vp->v_stream, stp);
			ASSERT(stp);

			continue;
		}
		if (!(((*flagsp & MSG_HIPRI) && !(stp->sd_flag & STRPRI)) ||
	    	     ((*flagsp & MSG_BAND) &&
		      (!q->q_first ||
	               ((q->q_first->b_band < *prip) &&
			!(stp->sd_flag & STRPRI)))) ||
		     !(bp = getq(q)))) {
			break;
		}
		/*
		 * If STRHUP, return 0 length control and data.
		 */
		if (stp->sd_flag & STRHUP) {
			mctl->len = mdata->len = 0;
			*flagsp = flg;
			STRHEAD_UNLOCK(stp);
			return (error);
		}
#ifdef _IRIX_LATER
		if ((error = strwaitq(stp, GETWAIT, (off_t)0, fmode, &done))
#else
		if ((error = strwaitq(stp, GETWAIT, (off_t)0, fmode, 0, &done))
#endif /* _IRIX_LATER */
		    || done) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
	}
	if (bp == stp->sd_mark) {
		mark = 1;
		stp->sd_mark = NULL;
	}
	
	if (bp->b_datap->db_type == M_PASSFP) {
		if (mark && !stp->sd_mark)
			stp->sd_mark = bp;
		putbq(q, bp);
		STRHEAD_UNLOCK(stp);
		return (EBADMSG);
	}

	pri = bp->b_band;
#ifdef _EAGER_RUNQUEUES
	if (qready())
		runqueues();
#endif /* _EAGER_RUNQUEUES */

	/*
	 * Set HIPRI flag if message is priority.
	 */
	if (stp->sd_flag & STRPRI)
		flg = MSG_HIPRI;
	else
		flg = MSG_BAND;

	/*
	 * First process PROTO or PCPROTO blocks, if any.
	 */
	if (mctl->maxlen >= 0 && bp && bp->b_datap->db_type != M_DATA) {
		bcnt = mctl->maxlen;
		ubuf = mctl->buf;
		while (bp && bp->b_datap->db_type != M_DATA && bcnt >= 0) {
			if ((n = MIN(bcnt, bp->b_wptr - bp->b_rptr)) &&
			    STREAMS_COPYOUT((caddr_t)bp->b_rptr,
					    (caddr_t)ubuf, n, NULL, U_TO_K)) {
				error = EFAULT;
				stp->sd_flag &= ~STRPRI;
				more = 0;
				freemsg(bp);
				goto getmout;
			}
			ubuf += n;
			bp->b_rptr += n;
			if (bp->b_rptr >= bp->b_wptr) {
				nbp = bp;
				bp = bp->b_cont;
				freeb(nbp);
			}
			if ((bcnt -= n) <= 0)
				break;
		}
		mctl->len = mctl->maxlen - bcnt;
	} else
		mctl->len = -1;
	
		
	if (bp && bp->b_datap->db_type != M_DATA) {	
		/*
		 * More PROTO blocks in msg.
		 */
		more |= MORECTL;
		savemp = bp;
		while (bp && bp->b_datap->db_type != M_DATA) {
			savemptail = bp;
			bp = bp->b_cont;
		}
		savemptail->b_cont = NULL;
	}

	/*
	 * Now process DATA blocks, if any.
	 */
	if (mdata->maxlen >= 0 && bp) {
		bcnt = mdata->maxlen;
		ubuf = mdata->buf;
		while (bp && bcnt >= 0) {
			if ((n = MIN(bcnt, bp->b_wptr - bp->b_rptr)) &&
			    STREAMS_COPYOUT((caddr_t)bp->b_rptr,
					    (caddr_t)ubuf, n, NULL, U_TO_K)) {
				error = EFAULT;
				stp->sd_flag &= ~STRPRI;
				more = 0;
				freemsg(bp);
				goto getmout;
			}
			ubuf += n;
			bp->b_rptr += n;
			if (bp->b_rptr >= bp->b_wptr) {
				nbp = bp;
				bp = bp->b_cont;
				freeb(nbp);
			}
			if ((bcnt -= n) <= 0)
				break;
		}
		mdata->len = mdata->maxlen - bcnt;
	} else
		mdata->len = -1;

	if (bp) {			/* more data blocks in msg */
		more |= MOREDATA;
		if (savemp)
			savemptail->b_cont = bp;
		else
			savemp = bp;
	} 

	if (savemp) {
		savemp->b_band = pri;
		if (mark && !stp->sd_mark) {
			savemp->b_flag |= MSGMARK;
			stp->sd_mark = savemp;
		}
		putbq(q, savemp);
	} else {
		stp->sd_flag &= ~STRPRI;
	}

	*flagsp = flg;
	*prip = pri;

	/*
	 * Getmsg cleanup processing - if the state of the queue has changed
	 * some signals may need to be sent and/or poll awakened.
	 */
getmout:
	while ((bp = q->q_first) && (bp->b_datap->db_type == M_SIG)) {
		bp = getq(q);
		strsignal(stp, *bp->b_rptr, (long)bp->b_band);
		freemsg(bp);
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */
	}

	/*
	 * If we have just received a high priority message and a
	 * regular message is now at the front of the queue, send
	 * signals in S_INPUT processes and wake up processes polling
	 * on POLLIN.
	 */
	if ((bp = q->q_first) && !(stp->sd_flag & STRPRI)) {
 	    if (flg & MSG_HIPRI) {

		if (stp->sd_sigflags & S_INPUT) 
			strsendsig(stp->sd_siglist, S_INPUT, (long)bp->b_band);
		pollwakeup(&stp->sd_pollist, POLLIN);
		if (bp->b_band == 0) {
		    if (stp->sd_sigflags & S_RDNORM)
			    strsendsig(stp->sd_siglist, S_RDNORM, 0L);
		    pollwakeup(&stp->sd_pollist, POLLRDNORM);
		} else {
		    if (stp->sd_sigflags & S_RDBAND)
			    strsendsig(stp->sd_siglist, S_RDBAND,
				(long)bp->b_band);
		    pollwakeup(&stp->sd_pollist, POLLRDBAND);
		}
	    } else {
		if (pri != bp->b_band) {
		    if (bp->b_band == 0) {
			if (stp->sd_sigflags & S_RDNORM)
				strsendsig(stp->sd_siglist, S_RDNORM, 0L);
			pollwakeup(&stp->sd_pollist, POLLRDNORM);
		    } else {
			if (stp->sd_sigflags & S_RDBAND)
				strsendsig(stp->sd_siglist, S_RDBAND,
				    (long)bp->b_band);
			pollwakeup(&stp->sd_pollist, POLLRDBAND);
		    }
		}
	    }
	}
	rvp->r_val1 = more;
	STRHEAD_UNLOCK(stp);
	return (error);
}

/*
 * Put a message downstream.
 */
int
strputmsg(register struct vnode *vp,
	  register struct strbuf *mctl,
	  register struct strbuf *mdata,
	  unsigned char pri,
	  register int flag,
	  int fmode)
{
	register struct stdata *stp;
	mblk_t *mp;
	long msgsize, rmin, rmax;
	int error, done;
	struct uio uio;
	struct iovec iov;

	/* Obtain monitor */
	STRHEAD_LOCK(&vp->v_stream, stp);
	ASSERT(stp);

	ASSERT(vp->v_stream);
	stp = vp->v_stream;

	if (stp->sd_flag & STPLEX) {
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	}
	if (stp->sd_flag & (STRHUP|STWRERR)) {
		if (vp->v_type == VFIFO || stp->sd_flag & STRSIGPIPE) {
			sigtouthread(curuthread, SIGPIPE, (k_siginfo_t *)NULL);
			/*
			 * this is for POSIX compatibility
			 */
			STRHEAD_UNLOCK(stp);
			return ((stp->sd_flag&STRHUP) ? EIO : stp->sd_werror);
		} else {
			STRHEAD_UNLOCK(stp);
			return (stp->sd_werror);
		}
	}

	/*
	 * Check for legal flag value.
	 */
	switch (flag) {
	case MSG_HIPRI:
		if ((mctl->len < 0) || (pri != 0)) {
			STRHEAD_UNLOCK(stp);
			return (EINVAL);
		}
		break;
	case MSG_BAND:
		break;

	default:
		STRHEAD_UNLOCK(stp);
		return (EINVAL);
	}

	/*
	 * Make sure ctl and data sizes together fall within the
	 * limits of the max and min receive packet sizes and do
	 * not exceed system limit.
	 */
	rmin = stp->sd_wrq->q_next->q_minpsz;
	rmax = stp->sd_wrq->q_next->q_maxpsz;
	ASSERT((rmax >= 0) || (rmax == INFPSZ));
	if (rmax == 0) {
		STRHEAD_UNLOCK(stp);
		return (ERANGE);
	}
	if (strmsgsz != 0) {
		if (rmax == INFPSZ)
			rmax = strmsgsz;
		else
			rmax = MIN(strmsgsz, rmax);
	}
	if ((msgsize = mdata->len) < 0) {
		msgsize = 0;
		rmin = 0;	/* no range check for NULL data part */
	}
	if ((msgsize < rmin) ||
	    ((msgsize > rmax) && (rmax != INFPSZ)) ||
	    (mctl->len > strctlsz)) {
		STRHEAD_UNLOCK(stp);
		return (ERANGE);
	}

	while (!(flag & MSG_HIPRI) && !bcanput(stp->sd_wrq->q_next, pri)) {
#ifdef _IRIX_LATER
		if ((error = strwaitq(stp, WRITEWAIT, (off_t) 0,
				      fmode, &done))
#else
		if ((error = strwaitq(stp, WRITEWAIT, (off_t) 0,
				      fmode, 0, &done))
#endif /* _IRIX_LATER */
		    || done) {
			STRHEAD_UNLOCK(stp);
			return (error);
		}
		/*
		 * job control
		 *
		 * Block the process if it's in background and it's
		 * trying to write to its controlling terminal.
		 */
		if (inbackground(stp) && (stp->sd_flag & STRTOSTOP) != 0) {

			STRHEAD_UNLOCK(stp);

			error = accessctty(JCTLWRITE);
			if (error > 0) {
				return (error);
			}

			STRHEAD_LOCK(&vp->v_stream, stp);
			ASSERT(stp);

			if (error == 0) {
				/* we may have stopped so recheck
				 * buffer status
				 */
				continue;
			}
			/* can't send TTOU - allow writing */
		}
	}

	iov.iov_base = mdata->buf;
	iov.iov_len = mdata->len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_fmode = 0;
	uio.uio_resid = iov.iov_len;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	if ((error = strmakemsg(mctl, mdata->len, &uio, stp, (long)flag, &mp)) || !mp) {
		STRHEAD_UNLOCK(stp);
		return (error);
	}
	mp->b_band = pri;

	/*
	 * Put message downstream.
	 */
	STREAMS_BLOCKQ();

	SAVE_PUTNEXT_Q_ADDRESS(mp, (stp->sd_wrq->q_next));
	SAVE_PUTNEXT_QIPUTP_ADDRESS(mp,
		(stp->sd_wrq->q_next->q_qinfo->qi_putp));
	(*stp->sd_wrq->q_next->q_qinfo->qi_putp)(stp->sd_wrq->q_next, mp);

	STREAMS_UNBLOCKQ();
#ifdef _EAGER_RUNQUEUES
	if (qready())
		runqueues();
#endif /* _EAGER_RUNQUEUES */

	STRHEAD_UNLOCK(stp);
	return (0);
}

/*
 * Determines whether the necessary conditions are set on a stream
 * for it to be readable, writeable, or have exceptions.
 */
int
strpoll(register struct stdata *stparg,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	register short retevents = 0;
	register mblk_t *mp;
	qband_t *qbp;
	struct stdata *stp;

	ASSERT(stparg);
	stp = stparg;
	STRHEAD_LOCK(&stp, stp);
	ASSERT(stp);

	if (stp->sd_flag & STPLEX) {
		*reventsp = POLLNVAL;
		STRHEAD_UNLOCK(stp);
		return (0);
	}

	if (stp->sd_flag & STRDERR) {
		if ((events == 0) ||
		    (events & (POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND))) {
			*reventsp = POLLERR;
			STRHEAD_UNLOCK(stp);
			return (0);
		}
	}
	if (stp->sd_flag & STWRERR) {
		if ((events == 0) || (events & (POLLOUT|POLLWRBAND))) {
			*reventsp = POLLERR;
			STRHEAD_UNLOCK(stp);
			return (0);
		}
	}

	if (stp->sd_flag & STRHUP) {
		retevents |= POLLHUP;
	} else {
		register queue_t *tq;

		tq = stp->sd_wrq->q_next;
		while (tq->q_next && !tq->q_qinfo->qi_srvp)
	    		tq = tq->q_next;
		if (events & POLLWRNORM) {
			if (tq->q_flag & QFULL)
				/* ensure backq svc procedure runs */
				tq->q_flag |= QWANTW;
			else
				retevents |= POLLOUT;
		}
		if (events & POLLWRBAND) {
			qbp = tq->q_bandp;
			if (qbp) {
				while (qbp) {
					if (qbp->qb_flag & QB_FULL)
						qbp->qb_flag |= QB_WANTW;
					else
						retevents |= POLLWRBAND;
					qbp = qbp->qb_next;
				}
			} else {
				retevents |= POLLWRBAND;
			}
		}
	}

	mp = RD(stp->sd_wrq)->q_first;
	if (!(stp->sd_flag & STRPRI)) {
		while (mp) {
			if ((events & POLLRDNORM) && (mp->b_band == 0))
				retevents |= POLLRDNORM;
			if ((events & POLLRDBAND) && (mp->b_band != 0))
				retevents |= POLLRDBAND;
			if (events & POLLIN)
				retevents |= POLLIN;

			/*
			 * MSGNOGET is really only to make poll return
			 * the intended events when the module is really
			 * holding onto the data.  Yeah, it's a hack and
			 * we need a better solution.
			 */
			if (mp->b_flag & MSGNOGET)
				mp = mp->b_next;
			else
				break;
		}
	} else {
		ASSERT(mp != NULL);
		if (events & POLLPRI)
			retevents |= POLLPRI;
	}
	*reventsp = retevents;
	if (retevents) {
		STRHEAD_UNLOCK(stp);
		return (0);
	}

	/*
	 * If poll() has not found any events yet, set up event cell
	 * to wake up the poll if a requested event occurs on this
	 * stream.  Check for collisions with outstanding poll requests.
	 */
	if (!anyyet) {
		*phpp = &stp->sd_pollist;
		*genp = POLLGEN(&stp->sd_pollist);
	}
	STRHEAD_UNLOCK(stp);
	return (0);
}

STATIC int
strsink(register queue_t *q, register mblk_t *bp)
{
	struct copyresp *resp;

	switch (bp->b_datap->db_type) {
	case M_FLUSH:
		if ((*bp->b_rptr & FLUSHW) && !(bp->b_flag & MSGNOLOOP)) {
			*bp->b_rptr &= ~FLUSHR;
			bp->b_flag |= MSGNOLOOP;
			qreply(q, bp);
		} else {
			freemsg(bp);
		}
		break;

	case M_COPYIN:
	case M_COPYOUT:
		if (bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}
		bp->b_datap->db_type = M_IOCDATA;
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)1L;	/* failure */
		qreply(q, bp);
		break;

	case M_IOCTL:
		if (bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}
		bp->b_datap->db_type = M_IOCNAK;
		qreply(q, bp);
		break;

	default:
		freemsg(bp);
		break;
	}
	return(0);
}

#if _MIPS_SIM == _ABI64
static void
irix5_strioctl_to_strioctl(
	register struct irix5_strioctl *i5_strioc,
	register struct strioctl *strioc)
{
	strioc->ic_cmd = i5_strioc->ic_cmd;
	strioc->ic_timout = i5_strioc->ic_timout;
	strioc->ic_len = i5_strioc->ic_len;
	strioc->ic_dp = (void *)(__psunsigned_t)i5_strioc->ic_dp;
}

static void
strioctl_to_irix5_strioctl(
	register struct strioctl *strioc,
	register struct irix5_strioctl *i5_strioc)
{
	i5_strioc->ic_cmd = strioc->ic_cmd;
	i5_strioc->ic_timout = strioc->ic_timout;
	i5_strioc->ic_len = strioc->ic_len;
	i5_strioc->ic_dp = (app32_ptr_t)(__psint_t)strioc->ic_dp;
}

static void
irix5_strbuf_to_strbuf(
	register struct irix5_strbuf *i5_strbuf,
	register struct strbuf *strbuf)
{
	strbuf->maxlen = i5_strbuf->maxlen;
	strbuf->len = i5_strbuf->len;
	strbuf->buf = (void *)(__psunsigned_t)i5_strbuf->buf;
}

static void
strbuf_to_irix5_strbuf(
	register struct strbuf *strbuf,
	register struct irix5_strbuf *i5_strbuf)
{
	i5_strbuf->maxlen = strbuf->maxlen;
	i5_strbuf->len = strbuf->len;
	i5_strbuf->buf = (app32_ptr_t)(__psint_t)strbuf->buf;
}

static void
irix5_strpeek_to_strpeek(
	register struct irix5_strpeek *i5_strpeek,
	register struct strpeek *strpeek)
{
	irix5_strbuf_to_strbuf(&i5_strpeek->ctlbuf, &strpeek->ctlbuf);
	irix5_strbuf_to_strbuf(&i5_strpeek->databuf, &strpeek->databuf);
	strpeek->flags = i5_strpeek->flags;
}

static void
strpeek_to_irix5_strpeek(
	register struct strpeek *strpeek,
	register struct irix5_strpeek *i5_strpeek)
{
	strbuf_to_irix5_strbuf(&strpeek->ctlbuf, &i5_strpeek->ctlbuf);
	strbuf_to_irix5_strbuf(&strpeek->databuf, &i5_strpeek->databuf);
	i5_strpeek->flags = strpeek->flags;
}

static void
irix5_str_list_to_str_list(
	register struct irix5_str_list *i5_strlist,
	register struct str_list *strlist)
{
	strlist->sl_nmods = i5_strlist->sl_nmods;
	strlist->sl_modlist = (void *)(__psint_t)i5_strlist->sl_modlist;
}

static void
irix5_strfdinsert_to_strfdinsert(
	struct irix5_strfdinsert *i5_strfdinsert,
	struct strfdinsert *strfdinsert)
{
	irix5_strbuf_to_strbuf(&i5_strfdinsert->ctlbuf, &strfdinsert->ctlbuf);
	irix5_strbuf_to_strbuf(&i5_strfdinsert->databuf, &strfdinsert->databuf);
	strfdinsert->flags = i5_strfdinsert->flags;
	strfdinsert->fildes = i5_strfdinsert->fildes;
	strfdinsert->offset = i5_strfdinsert->offset;
}
#endif	/* _ABI64 */

#ifndef _IRIX_LATER
/* push a stream module onto a stream for a driver that is being
 *	opened for the first time.
 *
 *	This is a little strange (i.e. kludgy), but ...
 */
int					/* 0 = success, !0 failure */
strdrv_push(
	queue_t *drvq,			/* read queue of the driver */
	char *mname,			/* module to push */
	dev_t *devp,
	struct cred *crp)
{
	register struct stdata *stp;
	register queue_t *rq;
	register short i;
	register int error;

	ASSERT(drvq->q_flag & QREADR);
	/* get queue that is the head */
	for (rq = drvq; rq->q_next; rq = rq->q_next)
		;
	ASSERT(!rq->q_next);

	stp = (struct stdata*)rq->q_ptr;

	/*
	 * only allow limited number of modules to be pushed.
	 */
	if (stp->sd_pushcnt >= nstrpush) {
		return (EINVAL);
	}

	if ((i = fmfind(mname)) < 0) {
		return (ENXIO);
	}

	/*
	 * Revert to the global monitor if the module does not
	 * support full MP-ization.
	 */
	if (fmodsw[i].f_str->st_rdinit->qi_minfo->mi_locking != MULTI_THREADED)
		useglobalmon(rq);

	if ((error = qattach(rq, devp, 0, FMODSW, &fmodsw[i], crp))) {
		fmrele(i);
		return (error);
	}
	fmrele(i);

	stp->sd_pushcnt++;
	return (0);			/* success */
}

/* send a bad message back up toward the stream head
 */
void
sdrv_error(queue_t *wq, mblk_t *bp)
{
	bp->b_datap->db_type = M_ERROR;
	bp->b_rptr = bp->b_datap->db_base;
	bp->b_wptr = bp->b_rptr + 1;
	*bp->b_rptr = EIO;

	qreply(wq, bp);
}



/* flush for a simple stream driver
 */
void
sdrv_flush(queue_t *wq, mblk_t *bp)
{
	if (*bp->b_rptr & FLUSHW) {
		flushq(wq, FLUSHDATA);
	}

	if (*bp->b_rptr & FLUSHR) {
		queue_t *rq = RD(wq);
		flushq(rq, FLUSHDATA);
		*bp->b_rptr &= ~FLUSHW;
		qreply(wq, bp);
		if (!canenable(rq))	/* fake a "back enable" since the */
			qenable(rq);	/* upstream may not remember to */
	} else {
		freemsg(bp);
	}
}

/* get a block,
 *	or request a wake-up call for the queue
 *
 *	If you use this, you must remember to use str_unbcall().
 */
mblk_t*
str_allocb(int sz,			/* this big */
	   queue_t *q,			/* put this queue to sleep */
	   uint pri)
{
	register mblk_t *bp;

	ASSERT(sz <= STR_MAXBSIZE);
	bp = allocb(sz, pri);

	if (!bp) { /* if we fail, set to try again */
		(void)bufcall(sz, pri, (void (*)())qenable, (long)q);
	}
	return bp;
}

/* remove stream events waiting for buffers for a queue
 */
void
str_unbcall(queue_t *rq)		/* this queue & its write-brother */
{
	register queue_t *wq = WR(rq);
	register struct strevent *sep, *prevsep, *nextsep;
	register int s2;

	LOCK_STR_RESOURCE(s2);
	sep = strbcalls.bc_head;
	prevsep = NULL;
	while (sep) {
		if (sep->se_arg == (long)rq || sep->se_arg == (long)wq) {
			if (prevsep == NULL)
				strbcalls.bc_head = sep->se_next;
			else
				prevsep->se_next = sep->se_next;
			if (sep == strbcalls.bc_tail)
				strbcalls.bc_tail = prevsep;
			nextsep = sep->se_next;
			sefree(sep);
			sep = nextsep;
			continue;
		}
		prevsep = sep;
		sep = sep->se_next;
	}
	UNLOCK_STR_RESOURCE(s2);
}

/* concatenate two messages
 */
void
str_conmsg(mblk_t **mpp,		/* append to this (maybe 0) */
	   mblk_t **mep,		/* which ends here */
	   mblk_t *new)			/* this message */
{
	register mblk_t *bp;

	if (!*mpp)
		*mpp = new;
	else {
		ASSERT((*mep)->b_next == 0);
		ASSERT((*mep)->b_prev == 0);
		(*mep)->b_cont = new;
	}

	do {				/* find end of combination */
		bp = new;
		new = new->b_cont;
	} while (NULL != new);

	*mep = bp;
}
#endif /* !_IRIX_LATER */

/*
 * XXX switch to binary search later
 */
int
baud_to_cbaud(speed_t baud)
{
    static const struct {
	speed_t s;
	int cbaud;
    } lookup[] = {
	/*
	 * first duplicated for convenience (permit [n-1] and [n+1] indexing)
	 */
	0,	__OLD_B0,
	0,	__OLD_B0,
	50,	__OLD_B50,
	75,	__OLD_B75,
	110,	__OLD_B110,
	135,	__OLD_B134,	   /* actually 134.5 */
	150,	__OLD_B150,
	200,	__OLD_B200,
	300,	__OLD_B300,
	600,	__OLD_B600,
	1200,	__OLD_B1200,
	1800,	__OLD_B1800,
	2400,	__OLD_B2400,
	4800,	__OLD_B4800,
	9600,	__OLD_B9600,
	19200,	__OLD_B19200,
	38400,	__OLD_B38400,
	__NEW_MAX_BAUD,	__OLD_INVALID_BAUD,
	__NEW_MAX_BAUD,	__OLD_INVALID_BAUD
    };
    /*REFERENCED*/
    static const int lookup_size = sizeof(lookup)/sizeof(lookup[0]);
    static const int starting_point = 15;	       /* B38400 */

    int i;

    if (baud > __NEW_MAX_BAUD) {
	return __OLD_INVALID_BAUD;
    }
    i = starting_point;
    while(1) {
	ASSERT(i >= 0 && i < lookup_size);
	if (baud == lookup[i].s)
	    return lookup[i].cbaud;
	else if (   ((baud > lookup[i].s) && (baud < lookup[i+1].s)) 
		 || ((baud < lookup[i].s) && (baud > lookup[i-1].s)) )
	    return __OLD_INVALID_BAUD;
	else if (baud < lookup[i].s)
	    i--;
	else if (baud > lookup[i].s)
	    i++;
    }
    /* NOTREACHED */
}

#if _MIPS_SIM == _ABI64

irix5_fix_fdinsert(mblk_t **mpp, int *offsetp)
{
	mblk_t *mp = *mpp;
	int offset = *offsetp;
	mblk_t *bp;
	int olen;
	int fudge;
	int xpad = 0;

#define _FUDGE	(sizeof(long) - sizeof(int))

	ASSERT(mp);

	fudge = _FUDGE;

	/*
	 * If the pointer offset is 8-byte aligned, all we need to do is
	 * account for the fact that it changed size.  Otherwise we need some
	 * padding in front of where we want to put it.
	 */
	if (offset % sizeof(queue_t *)) {
		/* OK, adjust the offset to account for the padding */
		*offsetp += fudge;

		if (*offsetp % sizeof(queue_t *)) {
			/* still not good */
			return EINVAL;
		}

		fudge *= 2;	/* add space for pad */
	}

	/* the length of the original block */
	olen = mp->b_wptr - mp->b_rptr;

	if (((olen + fudge) - *offsetp) < sizeof(queue_t *)) {
		/*
		 * Where the user wants to put the pointer is too
		 * close to the end; fail.
		 */
		return EINVAL;
	}
	while ((olen + fudge + xpad) & 0x7) {
		/* hack to get T_conn_res right size; ensure multiple of 8 */
		xpad++;
	}

	bp = allocb(olen + fudge + xpad, BPRI_HI);
	if (bp == (mblk_t *)0) {
		return ENOSR;
	}

	bzero(bp->b_rptr, olen + fudge + xpad);

	if (offset) {
		/* 
		 * If the pointer is not the first thing, copy preceding bytes
		 */
		bcopy(mp->b_rptr, bp->b_wptr, offset);
	}

	/*
	 * skip what the user-level pointer actually takes up
	 */
	offset += sizeof(app32_ptr_t);

	/*
	 * Note that this includes the original space, plus the size change
	 * of the pointer, and possibly the pad if we have one
	 */
	bp->b_wptr += (offset + fudge);

	/*
	 * If anything trails the pointer slot, copy it into the new block
	 */
	if (olen - offset) {
		bcopy(mp->b_rptr + offset, bp->b_wptr, (olen - offset));
		bp->b_wptr += (olen - offset);
	}

	ASSERT((bp->b_wptr - bp->b_rptr) == (olen + fudge));
	bp->b_wptr += xpad;
	bp->b_datap->db_type = mp->b_datap->db_type;
	bp->b_cont = mp->b_cont;
	mp->b_cont = 0;

	freeb(mp);

	*mpp = bp;
	return 0;
}
#undef _FUDGE
#endif	/* _ABI64 */

/*
 * Answers the question: is the calling process in the
 * foregound process group.
 */
STATIC int
inbackground(struct stdata *stp)
{
	vproc_t *vpr;
	vp_get_attr_t attr;
	vsession_t *vsp;

	if ((vsp = stp->sd_vsession) == NULL)
		return 0;	/* no controlling session */
	vpr = VPROC_LOOKUP(current_pid());

	ASSERT(vpr);
	VPROC_GET_ATTR(vpr, VGATTR_SID|VGATTR_PGID|VGATTR_HAS_CTTY, &attr);
	VPROC_RELE(vpr);
	return vsp->vs_sid == attr.va_sid
	    && attr.va_pgid != 0
	    && attr.va_pgid != stp->sd_pgid;
}

/*
 * Perform job control discipline access checks.
 * Return 0 for success and the errno for failure.
 */
STATIC int
accessctty(enum jobcontrol mode)
{
	int error;
	vproc_t *vpr;

	vpr = VPROC_LOOKUP(current_pid());
	ASSERT(vpr);
	VPROC_CTTY_ACCESS(vpr, mode, error);
	VPROC_RELE(vpr);
	return error;
}

int
fs_strgetmsg(bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	return strgetmsg(vp, mctl, mdata, prip, flagsp, fmode, rvp);
}

int
fs_strputmsg(bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	return strputmsg(vp, mctl, mdata, pri, flag, fmode);
}
