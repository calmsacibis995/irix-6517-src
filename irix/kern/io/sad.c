/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uts-comm:io/sad/sad.c	1.11"

/*
 * STREAMS Administrative Driver
 *
 * Currently only handles autopush and module name verification.
 */

#include <sys/sad.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/conf.h>
#include <string.h>
#include <sgidefs.h>

STATIC struct module_info sad_minfo = {
	0x7361, "sad", 0, INFPSZ, 0, 0
};

STATIC int sadopen(), sadclose(), sadwput();

STATIC struct qinit sad_rinit = {
	NULL, NULL, sadopen, sadclose, NULL, &sad_minfo, NULL
};

STATIC struct qinit sad_winit = {
	sadwput, NULL, NULL, NULL, NULL, &sad_minfo, NULL
};

int saddevflag = D_MP;
struct streamtab sadinfo = {
	&sad_rinit, &sad_winit, NULL, NULL
};

STATIC struct autopush *strpfreep;	/* autopush freelist */

/*
 * tunable parameters, defined in sad.cf
 */
extern struct saddev saddev[];		/* sad device array */
extern int sadcnt;			/* number of sad devices */
extern struct autopush autopush[];	/* autopush data array */
extern int nautopush;			/* maximum number of autopushable devices */

STATIC struct autopush *ap_alloc(void);
STATIC struct autopush *ap_hfind(long, long, long, uint);
STATIC void ap_free(struct autopush *);
STATIC void ap_hadd(struct autopush *);
STATIC void ap_hrmv(struct autopush *);
STATIC void apush_ioctl(queue_t *, mblk_t *);
STATIC void apush_iocdata(queue_t *, mblk_t *);
STATIC void nak_ioctl(queue_t *, mblk_t *, int);
STATIC void ack_ioctl(queue_t *, mblk_t *, int, int, int);
STATIC void vml_iocdata(queue_t *, mblk_t *);
STATIC void vml_ioctl(queue_t *, mblk_t *);
#if _MIPS_SIM == _ABI64
STATIC int ap_create(uint, long, long, long, uint, struct strapush *,
		struct strapush32 *, struct autopush* );
#else
STATIC int ap_create(uint, long, long, long, uint, struct strapush *,
		struct autopush* );
#endif

/*
 * sadinit() -
 * Initialize autopush freelist.
 */
void
sadinit()
{
	register struct autopush *ap;
	register int i;

	/*
	 * build the autpush freelist.
	 */
	strpfreep = autopush;
	ap = autopush;
	for (i = 1; i < nautopush; i++) {
		ap->ap_nextp = &autopush[i];
		ap->ap_flags = APFREE;
		ap = ap->ap_nextp;
	}
	ap->ap_nextp = NULL;
	ap->ap_flags = APFREE;
}

struct cred;

/*
 * sadopen() -
 * Allocate a sad device.  Only one
 * open at a time allowed per device.
 */
/* ARGSUSED */
STATIC int
sadopen(qp, devp, flag, sflag, credp)
	queue_t *qp;	/* pointer to read queue */
	dev_t *devp;	/* major/minor device of stream */
	int flag;	/* file open flags */
	int sflag;	/* stream open flags */
	struct cred *credp;	/* user credentials */
{
	register int i;

	if (sflag)		/* no longer called from clone driver */
		return (EINVAL);

	/*
	 * Both USRMIN and ADMMIN are clone interfaces.
	 */
	for (i = 0; i < sadcnt; i++)
		if (saddev[i].sa_qp == NULL)
			break;
	if (i >= sadcnt) {		/* no such device */
		return (ENXIO);
	}

	switch (getminor(*devp)) {
	case USRMIN:			/* mere mortal */
		saddev[i].sa_flags = 0;
		break;

	case ADMMIN:			/* priviledged user */
		saddev[i].sa_flags = SADPRIV;
		break;

	default:
		return (EINVAL);
	}

	saddev[i].sa_qp = qp;
	qp->q_ptr = (caddr_t)&saddev[i];
	WR(qp)->q_ptr = (caddr_t)&saddev[i];
	*devp = makedevice(getemajor(*devp), i);
	return (0);
}

/*
 * sadclose() -
 * Clean up the data structures.
 */
/* ARGSUSED */
STATIC int
sadclose(qp, flag, credp)
	queue_t *qp;	/* pointer to read queue */
	int flag;	/* file open flags */
	struct cred *credp;	/* user credentials */
{
	struct saddev *sadp;

	sadp = (struct saddev *)qp->q_ptr;
	sadp->sa_qp = NULL;
	sadp->sa_addr = NULL;
	qp->q_ptr = NULL;
	WR(qp)->q_ptr = NULL;
	return (0);
}

/*
 * sadwput() -
 * Write side put procedure.
 */
STATIC int
sadwput(qp, mp)
	queue_t *qp;	/* pointer to write queue */
	mblk_t *mp;	/* message pointer */
{
	struct iocblk *iocp;

	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			*mp->b_rptr &= ~FLUSHW;
			qreply(qp, mp);
		} else
			freemsg(mp);
		break;

	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case SAD_SAP:
		case SAD_GAP:
			apush_ioctl(qp, mp);
			break;

		case SAD_VML:
			vml_ioctl(qp, mp);
			break;

		default:
			nak_ioctl(qp, mp, EINVAL);
			break;
		}
		break;

	case M_IOCDATA:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case SAD_SAP:
		case SAD_GAP:
			apush_iocdata(qp, mp);
			break;

		case SAD_VML:
			vml_iocdata(qp, mp);
			break;

		default:
			ASSERT(0);
			freemsg(mp);
			break;
		}
		break;

	default:
		freemsg(mp);
		break;
	} /* switch (db_type) */
	return (0);
}

/*
 * ack_ioctl() -
 * Send an M_IOCACK message in the opposite
 * direction from qp.
 *	queue_t *qp;	queue pointer
 *	mblk_t *mp;	message block to use
 *	int count;	number of bytes to copyout
 *	int rval;	return value for icotl
 *	int errno;	error number to return
 */
STATIC void
ack_ioctl(queue_t *qp, mblk_t *mp, int count, int rval, int errno)
{
	struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_count = count;
	iocp->ioc_rval = rval;
	iocp->ioc_error = errno;
	mp->b_datap->db_type = M_IOCACK;
	qreply(qp, mp);
}

/*
 * nak_ioctl() -
 * Send an M_IOCNAK message in the opposite
 * direction from qp.
 *	queue_t *qp;	queue pointer
 *	mblk_t *mp;	message block to use
 *	int errno;	error number to return
 */
STATIC void
nak_ioctl(queue_t *qp, mblk_t *mp, int errno)
{
	struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_error = errno;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	mp->b_datap->db_type = M_IOCNAK;
	qreply(qp, mp);
}

/*
 * apush_ioctl() -
 * Handle the M_IOCTL messages associated with
 * the autopush feature.
 *	queue_t *qp;	pointer to write queue
 *	mblk_t *mp;	message pointer
 */
STATIC void
apush_ioctl(queue_t *qp, mblk_t *mp)
{
	struct iocblk *iocp;
	struct copyreq *cqp;
	struct saddev *sadp;
#if _MIPS_SIM == _ABI64
	long is64bit;
#endif

	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != TRANSPARENT) {
		nak_ioctl(qp, mp, EINVAL);
		return;
	}
#if _MIPS_SIM == _ABI64
	is64bit = iocp->ioc_64bit;
#endif
	sadp = (struct saddev *)qp->q_ptr;
	switch (iocp->ioc_cmd) {
	case SAD_SAP:
		if (!(sadp->sa_flags & SADPRIV)) {
			nak_ioctl(qp, mp, EPERM);
			break;
		}
		/* FALLTHRU */

	case SAD_GAP:
		cqp = (struct copyreq *)mp->b_rptr;
#if _MIPS_SIM == _ABI64
		if (is64bit)
			cqp->cq_addr = (caddr_t)*(app64_ptr_t *)mp->b_cont->b_rptr;
		else
			cqp->cq_addr = (caddr_t)(long)*(uint *)mp->b_cont->b_rptr;
#else
		cqp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
#endif
		sadp->sa_addr = cqp->cq_addr;
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		cqp->cq_size = sizeof(struct strapush);
		cqp->cq_flag = 0;
		cqp->cq_private = (mblk_t *)GETSTRUCT;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof(struct copyreq);
		qreply(qp, mp);
		break;

	default:
		ASSERT(0);
		nak_ioctl(qp, mp, EINVAL);
		break;
	} /* switch (ioc_cmd) */
}

/*
 * apush_iocdata() -
 * Handle the M_IOCDATA messages associated with
 * the autopush feature.
 *	queue_t *qp;	pointer to write queue
 *	mblk_t *mp;	message pointer
 */
STATIC void
apush_iocdata(queue_t *qp, mblk_t *mp)
{
	int i;
	long idx;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct strapush *sap;
	struct autopush *ap;
	struct saddev *sadp;
#if _MIPS_SIM == _ABI64
	struct strapush32 *sap32;
#endif
	uint cmd;
	long major;
	long minor;
	long lastminor;
	uint npush;

	csp = (struct copyresp *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	if (csp->cp_rval) {	/* if there was an error */
		freemsg(mp);
		return;
	}
	if (mp->b_cont) {
		/* sap needed only if mp->b_cont is set */
#if _MIPS_SIM == _ABI64
		if (csp->cp_64bit) {
			sap32 = NULL;
#endif
			sap = (struct strapush *)mp->b_cont->b_rptr;
			cmd = sap->sap_cmd;
			major = sap->sap_major;
			minor = sap->sap_minor;
			lastminor = sap->sap_lastminor;
			npush = sap->sap_npush;
#if _MIPS_SIM == _ABI64
		} else {
			sap32 = (struct strapush32 *)mp->b_cont->b_rptr;
			sap = (struct strapush *)mp->b_cont->b_rptr;
			cmd   = sap32->sap_cmd;
			major = sap32->sap_major;
			minor = sap32->sap_minor;
			lastminor = sap32->sap_lastminor;
			npush = sap32->sap_npush;
		}
#endif
	}
	switch (csp->cp_cmd) {
	case SAD_SAP:
		switch ((__psint_t)csp->cp_private) {
		case GETSTRUCT:
			switch (cmd) {
			case SAP_ONE:
			case SAP_RANGE:
			case SAP_ALL:
				if ((npush == 0) ||
				    (npush > MAXAPUSH) ||
				    (npush > nstrpush)) {

					/* invalid number of modules to push */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				if ((idx = (long)etoimajor(major)) == -1) {

					/* invalid major device number */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				if ((cmd == SAP_RANGE) &&
				    (lastminor <= minor)) {

					/* bad range */

					nak_ioctl(qp, mp, ERANGE);
					break;
				}
				if (cdevsw[idx].d_str == NULL) {

					/* not a STREAMS driver */

					nak_ioctl(qp, mp, ENOSTR);
					break;
				}
				if (ap_hfind(major, minor, lastminor, cmd)) {

					/* already configured */

					nak_ioctl(qp, mp, EEXIST);
					break;
				}
				if ((ap = ap_alloc()) == NULL) {

					/* no autopush structures - EAGAIN? */

					nak_ioctl(qp, mp, ENOSR);
					break;
				}
#if _MIPS_SIM == _ABI64
				if (ap_create(cmd, major, minor,
						lastminor, npush,
						sap, sap32, ap)) {

					/* bad module name */

					ap_free(ap);
					nak_ioctl(qp, mp, EINVAL);
					break;
				}
#else	/* 32 */
				if (ap_create(cmd, major, minor,
						lastminor, npush,
						sap, ap)) {

					/* bad module name */

					ap_free(ap);
					nak_ioctl(qp, mp, EINVAL);
					break;
				}
#endif /* _MIPS_SIM */

				ap_hadd(ap);
				ack_ioctl(qp, mp, 0, 0, 0);
				break;

			case SAP_CLEAR:
				if ((idx = (long)etoimajor(major)) == -1) {

					/* invalid major device number */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				if (cdevsw[idx].d_str == NULL) {

					/* not a STREAMS driver */

					nak_ioctl(qp, mp, ENOSTR);
					break;
				}
				if ((ap = ap_hfind(major, minor, lastminor, cmd)) == NULL) {

					/* not configured */

					nak_ioctl(qp, mp, ENODEV);
					break;
				}
				if ((ap->ap_type == SAP_RANGE) && (minor != ap->ap_minor)) {

					/* starting minors do not match */

					nak_ioctl(qp, mp, ERANGE);
					break;
				}
				if ((ap->ap_type == SAP_ALL) && (minor != 0)) {

					/* SAP_ALL must have minor == 0 */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				ap_hrmv(ap);
				ap_free(ap);
				ack_ioctl(qp, mp, 0, 0, 0);
				break;

			default:
				nak_ioctl(qp, mp, EINVAL);
				break;
			} /* switch (cmd) */
			break;

		default:
			ASSERT(0);
			freemsg(mp);
			break;
		} /* switch (cp_private) */
		break;

	case SAD_GAP:
		switch ((__psint_t)csp->cp_private) {
		case GETSTRUCT:
			if ((idx = (long)etoimajor(major)) == -1) {

				/* invalid major device number */

				nak_ioctl(qp, mp, EINVAL);
				break;
			}
			if (cdevsw[idx].d_str == NULL) {

				/* not a STREAMS driver */

				nak_ioctl(qp, mp, ENOSTR);
				break;
			}
			if ((ap = ap_hfind(major, minor, lastminor, SAP_ONE)) == NULL) {

				/* not configured */

				nak_ioctl(qp, mp, ENODEV);
				break;
			}
#if _MIPS_SIM == _ABI64
			if (csp->cp_64bit)
				sap->sap_common = ap->ap_common;
			else {
				sap32->sap_cmd = ap->ap_common.apc_cmd;
				sap32->sap_major = (app32_long_t)ap->ap_common.apc_major;
				sap32->sap_minor = (app32_long_t)ap->ap_common.apc_minor;
				sap32->sap_lastminor = (app32_long_t)ap->ap_common.apc_lastminor;
				sap32->sap_npush = ap->ap_common.apc_npush;
			}
#else
			sap->sap_common = ap->ap_common;
#endif /* _MIPS_SIM */
			for (i = 0; i < ap->ap_npush; i++) {
				if (fmhold(ap->ap_list[i])) {
#if _MIPS_SIM == _ABI64
				    if (csp->cp_64bit)
					strcpy(sap->sap_list[i], "");
				    else
					strcpy(sap32->sap_list[i], "");
#else
				    strcpy(sap->sap_list[i], "");
#endif
				}
				else {
#if _MIPS_SIM == _ABI64
				    if (csp->cp_64bit) {
					strcpy(sap->sap_list[i],
					    fmodsw[ap->ap_list[i]].f_name);
				    } else {
					strcpy(sap32->sap_list[i],
					    fmodsw[ap->ap_list[i]].f_name);
				    }
#else
				    strcpy(sap->sap_list[i],
					    fmodsw[ap->ap_list[i]].f_name);
#endif
				    fmrele(ap->ap_list[i]);
				}
			}
			for ( ; i < MAXAPUSH; i++) {
#if _MIPS_SIM == _ABI64
				if (csp->cp_64bit)
					bzero(sap->sap_list[i], FMNAMESZ + 1);
				else
					bzero(sap32->sap_list[i], FMNAMESZ + 1);
#else
				bzero(sap->sap_list[i], FMNAMESZ + 1);
#endif
			}
			mp->b_datap->db_type = M_COPYOUT;
			mp->b_wptr = mp->b_rptr + sizeof(struct copyreq);
			cqp->cq_private = (mblk_t *)GETRESULT;
			sadp = (struct saddev *)qp->q_ptr;
			cqp->cq_addr = sadp->sa_addr;
			cqp->cq_size = sizeof(struct strapush);
			cqp->cq_flag = 0;
			qreply(qp, mp);
			break;

		case GETRESULT:
			ack_ioctl(qp, mp, 0, 0, 0);
			break;

		default:
			ASSERT(0);
			freemsg(mp);
			break;
		} /* switch (cp_private) */
		break;

	default:	/* can't happen */
		ASSERT(0);
		freemsg(mp);
		break;
	} /* switch (cp_cmd) */
}

/*
 * ap_alloc() -
 * Allocate an autopush structure.
 */
STATIC struct autopush *
ap_alloc(void)
{
	register struct autopush *ap;

	if (strpfreep == NULL)
		return(NULL);
	ap = strpfreep;
	ASSERT(ap->ap_flags == APFREE);
	strpfreep = strpfreep->ap_nextp;
	ap->ap_nextp = NULL;
	ap->ap_flags = APUSED;
	return (ap);
}

/*
 * ap_free() -
 * Give an autopush structure back to the freelist.
 *	struct autopush *ap;	the autopush structure
 */
STATIC void
ap_free(struct autopush *ap)
{
	ASSERT(ap->ap_flags & APUSED);
	ASSERT(!(ap->ap_flags & APHASH));
	ap->ap_flags = APFREE;
	ap->ap_nextp = strpfreep;
	strpfreep = ap;
}

/*
 * ap_hadd() -
 * Add an autopush structure to the hash list.
 *	struct autopush *ap;	the autopush structure
 */
STATIC void
ap_hadd(struct autopush *ap)
{
	ASSERT(ap->ap_flags & APUSED);
	ASSERT(!(ap->ap_flags & APHASH));
	ap->ap_nextp = strphash(ap->ap_major);
	strphash(ap->ap_major) = ap;
	ap->ap_flags |= APHASH;
}

/*
 * ap_hrmv() -
 * Remove an autopush structure from the hash list.
 *	struct autopush *ap;	the autopush structure
 */
STATIC void
ap_hrmv(struct autopush *ap)
{
	struct autopush *hap;
	struct autopush *prevp = NULL;

	ASSERT(ap->ap_flags & APUSED);
	ASSERT(ap->ap_flags & APHASH);
	hap = strphash(ap->ap_major);
	while (hap) {
		if (ap == hap) {
			hap->ap_flags &= ~APHASH;
			if (prevp)
				prevp->ap_nextp = hap->ap_nextp;
			else
				strphash(ap->ap_major) = hap->ap_nextp;
			return;
		} /* if */
		prevp = hap;
		hap = hap->ap_nextp;
	} /* while */
}

/*
 * ap_hfind() -
 * Look for an autopush structure in the hash list
 * based on major, minor, lastminor, and command.
 *	long maj;	major device number
 *	long minor;	minor device number
 *	long last;	last minor device number (SAP_RANGE only)
 *	uint cmd;	who is asking
 */
STATIC struct autopush *
ap_hfind(long maj, long minor, long last, uint cmd)
{
	register struct autopush *ap;

	ap = strphash(maj);
	while (ap) {
		if (ap->ap_major == maj) {
			if (cmd == SAP_ALL)
				break;
			switch (ap->ap_type) {
			case SAP_ALL:
				break;

			case SAP_ONE:
				if (ap->ap_minor == minor)
					break;
				if ((cmd == SAP_RANGE) &&
				    (ap->ap_minor >= minor) &&
				    (ap->ap_minor <= last))
					break;
				ap = ap->ap_nextp;
				continue;

			case SAP_RANGE:
				if ((cmd == SAP_RANGE) &&
				    (((minor >= ap->ap_minor) &&
				      (minor <= ap->ap_lastminor)) ||
				     ((ap->ap_minor >= minor) &&
				      (ap->ap_minor <= last))))
					break;
				if ((minor >= ap->ap_minor) &&
				    (minor <= ap->ap_lastminor))
					break;
				ap = ap->ap_nextp;
				continue;

			default:
				ASSERT(0);
				break;
			}
			break;
		}
		ap = ap->ap_nextp;
	}
	return (ap);
}

/*
 * ap_create() -
 * Step through the list of modules to autopush and
 * validate their names.  Copy useful info from sap to ap.
 * Return 0 if the list is valid and 1 if it is not.
 */
STATIC int
#if _MIPS_SIM == _ABI64
ap_create(uint cmd,
	  long major,
	  long minor,
	  long lastminor,
	  uint npush,
	  struct strapush *sap,
	  struct strapush32 *sap32,
	  struct autopush *ap)
#else	/* 32 */
ap_create(uint cmd,
	  long major,
	  long minor,
	  long lastminor,
	  uint npush,
	  struct strapush *sap,
	  struct autopush *ap)
#endif
{
	register int i;

#if _MIPS_SIM == _ABI64
	for (i = 0; i < npush; i++) {
	    if (sap32) {
		if ((signed)(ap->ap_list[i]=findmod(sap32->sap_list[i])) == -1)
			return (1);
	    } else {
		if ((signed)(ap->ap_list[i]=findmod(sap->sap_list[i])) == -1)
			return (1);
	    }
	}
#else /* 32 */
	for (i = 0; i < npush; i++)
		if ((signed)(ap->ap_list[i]=findmod(sap->sap_list[i])) == -1)
			return (1);
#endif

	ap->ap_common.apc_cmd = cmd;
	ap->ap_common.apc_major = major;
	ap->ap_common.apc_minor = minor;
	ap->ap_common.apc_lastminor = lastminor;
	ap->ap_common.apc_npush = npush;
	ap->ap_npush = npush;
	return (0);
}

/*
 * vml_ioctl() -
 * Handle the M_IOCTL message associated with a request
 * to validate a module list.
 *	queue_t *qp;	pointer to write queue
 *	mblk_t *mp;	message pointer
 */
STATIC void
vml_ioctl(queue_t *qp, mblk_t *mp)
{
	struct iocblk *iocp;
	struct copyreq *cqp;
#if _MIPS_SIM == _ABI64
	long is64bit;
#endif

	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != TRANSPARENT) {
		nak_ioctl(qp, mp, EINVAL);
		return;
	}
#if _MIPS_SIM == _ABI64
	is64bit = iocp->ioc_64bit;
#endif
	ASSERT (iocp->ioc_cmd == SAD_VML);
	cqp = (struct copyreq *)mp->b_rptr;
#if _MIPS_SIM == _ABI64
	if (is64bit)
		cqp->cq_addr = (caddr_t)*(app64_ptr_t *)mp->b_cont->b_rptr;
	else
		cqp->cq_addr = (caddr_t)(long)*(uint *)mp->b_cont->b_rptr;
#else
	cqp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
#endif
	freemsg(mp->b_cont);
	mp->b_cont = NULL;
	cqp->cq_size = sizeof(struct str_list);
	cqp->cq_flag = 0;
	cqp->cq_private = (mblk_t *)GETSTRUCT;
	mp->b_datap->db_type = M_COPYIN;
	mp->b_wptr = mp->b_rptr + sizeof(struct copyreq);
	qreply(qp, mp);
}

/*
 * vml_iocdata() -
 * Handle the M_IOCDATA messages associated with
 * a request to validate a module list.
 *	queue_t *qp;	pointer to write queue
 *	mblk_t *mp;	message pointer
 */
STATIC void
vml_iocdata(queue_t *qp, mblk_t *mp)
{
	int i;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct str_mlist *lp;
	struct str_list *slp;
	struct saddev *sadp;

	csp = (struct copyresp *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	if (csp->cp_rval) {	/* if there was an error */
		freemsg(mp);
		return;
	}
	ASSERT (csp->cp_cmd == SAD_VML);
	sadp = (struct saddev *)qp->q_ptr;
	switch ((__psint_t)csp->cp_private) {
	case GETSTRUCT:
		slp = (struct str_list *)mp->b_cont->b_rptr;
		if (slp->sl_nmods <= 0) {
			nak_ioctl(qp, mp, EINVAL);
			break;
		}
		sadp->sa_addr = (caddr_t)(__psunsigned_t)slp->sl_nmods;
		cqp->cq_addr = (caddr_t)slp->sl_modlist;
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		cqp->cq_size = (__psint_t)sadp->sa_addr * sizeof(struct str_mlist);
		cqp->cq_flag = 0;
		cqp->cq_private = (mblk_t *)GETLIST;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof(struct copyreq);
		qreply(qp, mp);
		break;

	case GETLIST:
		lp = (struct str_mlist *)mp->b_cont->b_rptr;
		for (i = 0; i < (__psint_t)sadp->sa_addr; i++, lp++)
			if (findmod(lp->l_name) == -1) {
				ack_ioctl(qp, mp, 0, 1, 0);
				return;
			}
		ack_ioctl(qp, mp, 0, 0, 0);
		break;

	default:
		ASSERT(0);
		freemsg(mp);
		break;
	} /* switch (cp_private) */
}

