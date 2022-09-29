/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:proc/acct.c	1.6"*/
#ident	"$Revision: 3.59 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <ksys/vpag.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/extacct.h>
#include <ksys/vfile.h>
#include <sys/kabi.h>
#include <sys/vnode.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/sat.h>
#include <ksys/vsession.h>
#include <ksys/vhost.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

extern int do_procacct, do_extpacct, do_sessacct;

mutex_t			aclock;
static struct vnode	*acctvp;

#define ACLOCK()	mutex_lock(&aclock, PZERO)
#define	ACUNLOCK()	mutex_unlock(&aclock)

/*
 * Perform process accounting functions.
 */

struct accta {
	char	*fname;
};

/* ARGSUSED */
int
sysacct(struct accta *uap, rval_t *rvp)
{
	struct vnode *vp;
	int error = 0;
	int cflags = 0;
	cred_t *cr = get_current_cred();

	if (!_CAP_CRABLE(cr, CAP_ACCT_MGT))
		return EPERM;
	ACLOCK();
	if (uap->fname == NULL) {
		if (acctvp) {
			VOP_CLOSE(acctvp, FWRITE, L_TRUE, cr, error); 
			if (error)
				goto out;
			VN_RELE(acctvp);
			VHOST_SYSACCT(0);	/* Notify stop */
			acctvp = NULL;
		}
	} else {
		_SAT_PN_BOOK(SAT_SYSACCT, curuthread);
		if (getfsizelimit() == 0)
			cflags |= VZFS;
		if (error = vn_open(uap->fname, UIO_USERSPACE, FWRITE, 0,
				    &vp, CRCREAT, cflags, NULL)) {
			/* SVID  compliance */
			if (error == EISDIR)
				error = EACCES;
			goto out;
		}
		if (acctvp && VN_CMP(acctvp, vp)) {
			error = EBUSY;
			goto closevp;
		}
		if (vp->v_type != VREG) {
			error = EACCES;
			goto closevp;
		}
		if (acctvp) {
			VOP_CLOSE(acctvp, FWRITE, L_TRUE, cr, error); 
			if (error)
				goto closevp;
			VN_RELE(acctvp);
		}
		acctvp = vp;
		VHOST_SYSACCT(1); 	/* Notify start */
	}
	goto out;
closevp:
	VOP_CLOSE(vp, FWRITE, L_TRUE, cr, cflags);
	VN_RELE(vp);
out:
	ACUNLOCK();
	_SAT_ACCT(uap->fname,error);
	return error;
}

/*
 * As a result of making acctvp static,
 * other modules (e.g.,AUDIT) need a 
 * functional interface to determine
 * when the process accounting is enabled.
 */
int
accton()
{
 	return(acctvp!=NULL);
}

/*
 * Produce a pseudo-floating point representation
 * with 3 bits base-8 exponent, 13 bits fraction.
 */

static comp_t
compress(register time_t t)
{
	register int exp = 0, round = 0;

	while (t >= 8192) {
		exp++;
		round = t&04;
		t >>= 3;
	}
	if (round) {
		t++;
		if (t >= 8192) {
			t >>= 3;
			exp++;
		}
	}
	return (exp<<13) + t;
}


/*
 * MAX value of ioch that can be represented by compress -> uncompress.
 * One more than the following value would, on compress() -> uncompress(),
 * be (MAX_IOCH + 1).
 */
#define MAX_COMP_IOCH		(MAX_IOCH - (MAX_IOCH/8192))

/*
 * Same as compress, but takes an unsigned long as input and checks for
 * overflow. This is unnecessary for 32-bit kernels since a 32-bit value
 * cannot overflow a comp_t.
 */
#if _MIPS_SIM == _ABI64
static comp_t
ul_compress(register u_long t)
{
	register int exp = 0, round = 0;

	if (t > MAX_COMP_T)
	  t = MAX_COMP_T;

	while (t >= 8192) {
		exp++;
		round = t&04;
		t >>= 3;
	}
	if (round) {
		t++;
		if (t >= 8192) {
			t >>= 3;
			exp++;
		}
	}
	return (exp<<13) + t;
}
#else
#define ul_compress compress
#endif   /* _ABI64 */

/*
 * On exit, write a record on the accounting file.
 */

void
acct(char st)
{
	struct vnode *vp;
	cred_t *cr;
	struct acct acctbuf;
	uthread_t *ut;
	proc_t *p;

	ASSERT(do_procacct != 0);

	if (acctvp == NULL)
		return;

	ACLOCK();
	if ((vp = acctvp) == NULL) {
		ACUNLOCK();
		return;
	}

	ut = curuthread;
	p = UT_TO_PROC(ut);

	bcopy(p->p_comm, acctbuf.ac_comm, sizeof(acctbuf.ac_comm));
	acctbuf.ac_btime = p->p_start;
	acctbuf.ac_etime = compress(lbolt - p->p_ticks);

	if (p->p_shacct == NULL) {
		acctbuf.ac_utime = compress(
			p->p_proxy.prxy_exit_accum.ac_utime / NSEC_PER_TICK);

		acctbuf.ac_stime = compress(
			p->p_proxy.prxy_exit_accum.ac_stime / NSEC_PER_TICK);

#if _PAGESZ == 16384
		/* Convert page size to 4k because that is what KCORE
		   expects. */
		acctbuf.ac_mem = ul_compress(p->p_proxy.prxy_exit_mem * 4);
#else
		acctbuf.ac_mem = ul_compress(p->p_proxy.prxy_exit_mem);
#endif
		acctbuf.ac_io =
		    ul_compress((p->p_proxy.prxy_exit_ioch > MAX_COMP_IOCH)
				? MAX_COMP_IOCH
				: p->p_proxy.prxy_exit_ioch);
		acctbuf.ac_rw =
		    ul_compress(p->p_proxy.prxy_ru.ru_inblock +
					p->p_proxy.prxy_ru.ru_oublock);
	} else {
		shacct_t *shacct = p->p_shacct;
		u_long ioch;

		ASSERT(shacct->sha_type == SHATYPE_PROC);

		acctbuf.ac_utime = compress(
			(p->p_proxy.prxy_exit_accum.ac_utime / NSEC_PER_TICK)
			- (shacct->sha_timers.ac_utime/NSEC_PER_TICK));

		acctbuf.ac_stime = compress(
			(p->p_proxy.prxy_exit_accum.ac_stime / NSEC_PER_TICK)
			- (shacct->sha_timers.ac_stime/NSEC_PER_TICK));

#if _PAGESZ == 16384
		/* Convert page size to 4k because that is what KCORE
		   expects. */
		acctbuf.ac_mem =
		    ul_compress((p->p_proxy.prxy_exit_mem -
					shacct->sha_counts.ac_mem) * 4);
#else
		acctbuf.ac_mem =
		    ul_compress(p->p_proxy.prxy_exit_mem -
					shacct->sha_counts.ac_mem);
#endif
		acctbuf.ac_rw =
		    ul_compress((p->p_proxy.prxy_ru.ru_inblock
				 - shacct->sha_counts.ac_br)
				+ (p->p_proxy.prxy_ru.ru_oublock
				   - shacct->sha_counts.ac_bw));

		/* cast of sha_ioch is safe on 32-bit systems: it's */
		/* only assigned u_long values			    */
		ioch = p->p_proxy.prxy_exit_ioch - (u_long) shacct->sha_ioch;
		acctbuf.ac_io =
		    ul_compress((ioch > MAX_COMP_IOCH) ? MAX_COMP_IOCH : ioch);
	}

	cr = ut->ut_cred;
	acctbuf.ac_uid = cr->cr_ruid;
	acctbuf.ac_gid = cr->cr_rgid;
	acctbuf.ac_tty = cttydev(p);
	acctbuf.ac_stat = st;
	acctbuf.ac_flag = (p->p_acflag & ~ACKPT) | AEXPND;

	/* Pass the system's credentials rather than the user's.  The
	 * file must be written on behalf of the system, so using the
	 * user's credentials gives errors when the accounting file
	 * is over NFS.  This works in the local case because we don't
	 * check write permissions after opening the file, and the open
	 * comes in as root.
	 */
	(void) vn_rdwr(UIO_WRITE, vp, (caddr_t) &acctbuf,
		       sizeof(acctbuf), 0, UIO_SYSSPACE, IO_APPEND,
		       RLIM_INFINITY, sys_cred, (ssize_t *)NULL, sys_flid);
	ACUNLOCK();
}


/*
 * Generate an extended accounting record
 */
void
extacct(proc_t *p)
{
	struct acct_timers timers;
	struct acct_counts counts;

	ASSERT(do_extpacct || do_sessacct);

	timers.ac_utime = p->p_proxy.prxy_exit_accum.ac_utime;
	timers.ac_stime = p->p_proxy.prxy_exit_accum.ac_stime;
	timers.ac_bwtime = p->p_proxy.prxy_exit_accum.ac_bwtime;
	timers.ac_rwtime = p->p_proxy.prxy_exit_accum.ac_rwtime;
	timers.ac_qwtime = p->p_proxy.prxy_exit_accum.ac_qwtime;

	counts.ac_mem    = (accum_t) p->p_proxy.prxy_exit_mem;
	counts.ac_syscr  = (accum_t) p->p_proxy.prxy_exit_syscr;
	counts.ac_syscw  = (accum_t) p->p_proxy.prxy_exit_syscw;
	counts.ac_chr    = (accum_t) p->p_proxy.prxy_exit_bread;
	counts.ac_chw    = (accum_t) p->p_proxy.prxy_exit_bwrit;
	counts.ac_swaps  = (accum_t) p->p_proxy.prxy_ru.ru_nswap;
	counts.ac_br     = (accum_t) p->p_proxy.prxy_ru.ru_inblock;
	counts.ac_bw     = (accum_t) p->p_proxy.prxy_ru.ru_oublock;

	if (p->p_shacct != NULL) {
		ASSERT(p->p_shacct->sha_type == SHATYPE_PROC);

		timers.ac_utime  -= p->p_shacct->sha_timers.ac_utime;
		timers.ac_stime  -= p->p_shacct->sha_timers.ac_stime;
		timers.ac_bwtime -= p->p_shacct->sha_timers.ac_bwtime;
		timers.ac_rwtime -= p->p_shacct->sha_timers.ac_rwtime;
		timers.ac_qwtime -= p->p_shacct->sha_timers.ac_qwtime;

		counts.ac_mem    -= p->p_shacct->sha_counts.ac_mem;
		counts.ac_swaps  -= p->p_shacct->sha_counts.ac_swaps;
		counts.ac_chr    -= p->p_shacct->sha_counts.ac_chr;
		counts.ac_chw    -= p->p_shacct->sha_counts.ac_chw;
		counts.ac_br     -= p->p_shacct->sha_counts.ac_br;
		counts.ac_bw     -= p->p_shacct->sha_counts.ac_bw;
		counts.ac_syscr  -= p->p_shacct->sha_counts.ac_syscr;
		counts.ac_syscw  -= p->p_shacct->sha_counts.ac_syscw;
	}		

#if _PAGESZ == 16384
	/* Convert page size to 4k because that is what KCORE expects. */
	counts.ac_mem *= 4;
#endif
	if (do_extpacct)
	    _SAT_PROC_ACCT(p, &timers, &counts, 0);
	if (do_sessacct) {
		/*
	 	 * Do array session accounting.
		 */
		vpagg_t *vpag;
		VPROC_GETVPAGG(PROC_TO_VPROC(p), &vpag);
		VPAG_ACCUM_STATS(vpag, &timers, &counts);
	}
}


/*
 * Copy the current accounting data into a set of shadow timers/counters.
 */
void
shadowacct(proc_t *p, int type)
{
	shacct_t *shacct = p->p_shacct;

	if (shacct == NULL) {
		shacct = kern_malloc(sizeof(shacct_t));
		shacct->sha_type = type;
		p->p_shacct = shacct;
	}

	ASSERT(shacct->sha_type == type);

	shacct->sha_ioch = (accum_t) p->p_proxy.prxy_exit_ioch;

	VPROC_GET_ACCT_TIMERS(PROC_TO_VPROC(p), &shacct->sha_timers);
 
	shacct->sha_counts.ac_mem    = (accum_t) p->p_proxy.prxy_exit_mem;
	shacct->sha_counts.ac_syscr  = (accum_t) p->p_proxy.prxy_exit_syscr;
	shacct->sha_counts.ac_syscw  = (accum_t) p->p_proxy.prxy_exit_syscw;
	shacct->sha_counts.ac_chr    = (accum_t) p->p_proxy.prxy_exit_bread;
	shacct->sha_counts.ac_chw    = (accum_t) p->p_proxy.prxy_exit_bwrit;
	shacct->sha_counts.ac_swaps  = (accum_t) p->p_proxy.prxy_ru.ru_nswap;
	shacct->sha_counts.ac_br     = (accum_t) p->p_proxy.prxy_ru.ru_inblock;
	shacct->sha_counts.ac_bw     = (accum_t) p->p_proxy.prxy_ru.ru_oublock;
}

#if CELL_IRIX
/* ARGSUSED */
void
phost_sysacct(
	bhv_desc_t	*bdp,
	int		enable,
	cfs_handle_t 	*handlep)
{
	ACLOCK();
	if (enable) {
		if (acctvp) {
			VN_RELE(acctvp);
		}
		cfs_vnimport(handlep, &acctvp);
	}
	else {
		VN_RELE(acctvp);
		acctvp = NULL;
	}
	ACUNLOCK();
}
#else
/* ARGSUSED */
void
phost_sysacct(
	bhv_desc_t	*bdp,
	int		enable,
	cfs_handle_t 	*handlep)
{
}
#endif
	

