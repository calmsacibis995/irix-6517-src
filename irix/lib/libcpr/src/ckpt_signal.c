/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.1 $"

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <mutex.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <strings.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/ckpt_sys.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ckpt.h"
#include "ckpt_internal.h"
#include "ckpt_revision.h"
/*
 * ckpt_write_siginfo
 *
 * Get and write the signal header, sigaction and siginfo structures to the
 * state file
 */
int
ckpt_write_siginfo(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	prstatus_t status;
	int nsigs;
	ckpt_sig_t sig;
	sigaction_t *sap;
	siginfo_t *sip = NULL;
	siginfo_t curinfo;
#define MAXTRIES	5
	int tries = 0;
	ckpt_siginfo_arg_t sarg;
	ssize_t rc = 0;
	int maxninfo;
	/*
	 * Need a thread id for some ioctls...any thread in the process 
	 * will do
	 */
	tid_t tid;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	tid = co->co_thread->ct_id;

	if (ioctl(co->co_prfd, PIOCSTATUS, &status) < 0) {
		cerror("PIOCSTATUS (%s)\n", STRERR);
		return (-1);
	}

	sig.cs_magic = pp->prop_magic;

	sig.cs_altstack = status.pr_altstack;
	sig.cs_sigtramp = co->co_pinfo->cp_psi.ps_sigtramp;
	/*
	 * Refetch signal disposition now that proc is stopped and can
	 * no longer change them.
	 */
	assert(co->co_sap);
	free(co->co_sap);
	co->co_sap = 0;

	if (ioctl(co->co_prfd, PIOCMAXSIG, &nsigs) < 0) {
		cerror("PIOCMAXSIG:%s", STRERR);
		return (-1);
	}
	sap = (sigaction_t *)malloc(nsigs * sizeof(sigaction_t));
	if (sap == NULL) {
		cerror("malloc (%s)", STRERR);
		return (-1);
	}
	if (ioctl(co->co_prfd, PIOCACTION, sap) < 0) {
		cerror("PIOCACTION:%s", STRERR);
		free(sap);
		return (-1);
	}
	sig.cs_nsigs = co->co_nsigs = nsigs;
	/*
	 * Get the number of siginfo structs, allocate space and fetch structs
	 * Note that it is possible, though unlikely, that new signals,
	 * and therefore, info structs could get posted.  If that happens, we
	 * get an EAGAIN, and start over.  If happens too many times, we
	 * give up.
	 */
again:
	sarg.csa_count = -1;
	sarg.csa_buffer = NULL;
	sarg.csa_curinfo = NULL;
	sarg.csa_async = 1;

	if ((maxninfo = ckpt_thread_ioctl(co, tid, 0, PIOCCKPTGETSI, &sarg)) < 0) {
		cerror("Failed to issue ioctl PIOCCKPTGETSI (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	if (maxninfo > 0) {
		sip = (siginfo_t *)malloc(maxninfo * sizeof(siginfo_t));
		if (sip == NULL) {
			cerror("Failed to allocate memory (%s)\n", STRERR);
			rc = -1;
			goto cleanup;
		}
	}
	sarg.csa_count = maxninfo;
	sarg.csa_buffer = (caddr_t)sip;
	sarg.csa_curinfo = (caddr_t)&curinfo;
	sarg.csa_async = 1;

	if ((sig.cs_ninfo = ckpt_thread_ioctl(co, tid, 0, PIOCCKPTGETSI,
							(caddr_t)&sarg)) < 0) {
		if (oserror() == EAGAIN && tries++ < MAXTRIES) {
			if (sip) {
				free(sip);
				sip = NULL;
			}
			goto again;
		}
		cerror("Failed to get signal info (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	assert(maxninfo >= sig.cs_ninfo);
	assert(sarg.csa_cursig == 0);

	sig.cs_pending = sarg.csa_pending;

	CWRITE(co->co_sfd, (caddr_t)&sig, sizeof(ckpt_sig_t), 1, pp, rc);
	if (rc < 0) {
		cerror("failed to write signal info (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	CWRITE(co->co_sfd, (caddr_t)sap, nsigs*sizeof(sigaction_t), 0, pp, rc);
	if (rc < 0) {
		cerror("failed to write signal info (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	if (sip) {
		assert(maxninfo > 0);
		CWRITE(co->co_sfd, (caddr_t)sip, sig.cs_ninfo*sizeof(siginfo_t),
			0, pp, rc);
		if (rc < 0) {
			cerror("failed to write signal info (%s)\n", STRERR);
			rc = -1;
			goto cleanup;
		}
	}

cleanup:
	free(sap);
	if (sip)
		free(sip);
	return((int)rc);
}

static int
ckpt_write_one_thread_siginfo(ckpt_obj_t *co, ckpt_prop_t *pp, ckpt_thread_t *thread)
{
	tid_t tid = thread->ct_id;
	ckpt_thread_sig_t threadsig;
	siginfo_t *sip = NULL;
	siginfo_t curinfo;
#define MAXTRIES	5
	int tries = 0;
	ckpt_siginfo_arg_t sarg;
	ssize_t rc = 0;
	int maxninfo;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	bzero(&curinfo, sizeof(siginfo_t));

	threadsig.cts_magic = pp->prop_magic;
	threadsig.cts_tid = tid;
	/*
	 * Get the number of siginfo structs, allocate space and fetch structs
	 * Note that it is possible, though unlikely, that new signals,
	 * and therefore, info structs could get posted.  If that happens, we
	 * get an EAGAIN, and start over.  If happens too many times, we
	 * give up.
	 */
again:
	sarg.csa_count = -1;
	sarg.csa_buffer = NULL;
	sarg.csa_curinfo = NULL;
	sarg.csa_async = 0;

	if ((maxninfo = ckpt_thread_ioctl(co, tid, 0, PIOCCKPTGETSI, &sarg)) < 0) {
		cerror("Failed to issue ioctl PIOCCKPTGETSI (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	if (maxninfo > 0) {
		sip = (siginfo_t *)malloc(maxninfo * sizeof(siginfo_t));
		if (sip == NULL) {
			cerror("Failed to allocate memory (%s)\n", STRERR);
			rc = -1;
			goto cleanup;
		}
	}
	sarg.csa_count = maxninfo;
	sarg.csa_buffer = (caddr_t)sip;
	sarg.csa_curinfo = (caddr_t)&curinfo;
	sarg.csa_async = 0;

	if ((threadsig.cts_ninfo = ckpt_thread_ioctl(	co,
							tid,
							0,
							PIOCCKPTGETSI,
							(caddr_t)&sarg)) < 0) {
		if (oserror() == EAGAIN && tries++ < MAXTRIES) {
			if (sip) {
				free(sip);
				sip = NULL;
			}
			goto again;
		}
		cerror("Failed to get signal info (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	assert(maxninfo >= threadsig.cts_ninfo);

	threadsig.cts_cursig = sarg.csa_cursig;;
	threadsig.cts_pending = sarg.csa_pending;

	CWRITE(co->co_sfd, (caddr_t)&threadsig, sizeof(threadsig), 1, pp, rc);
	if (rc < 0) {
		cerror("failed to write signal info (%s)\n", STRERR);
		rc = -1;
		goto cleanup;
	}
	assert((curinfo.si_signo == 0)||(threadsig.cts_cursig == curinfo.si_signo));

	if (threadsig.cts_cursig > 0) {
		CWRITE(co->co_sfd, &curinfo, sizeof(siginfo_t), 0, pp, rc);
		if (rc < 0) {
			cerror("failed to write signal info (%s)\n", STRERR);
			rc = -1;
			goto cleanup;
		}
	}
	if (sip) {
		assert(maxninfo > 0);
		CWRITE(co->co_sfd, (caddr_t)sip, threadsig.cts_ninfo*sizeof(siginfo_t),
			0, pp, rc);
		if (rc < 0) {
			cerror("failed to write signal info (%s)\n", STRERR);
			rc = -1;
			goto cleanup;
		}
	}

cleanup:
	if (sip)
		free(sip);
	return((int)rc);
}

int
ckpt_write_thread_siginfo(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_thread_t *thread;

	assert((co->co_flags & CKPT_CO_ZOMBIE)||(co->co_thread));

	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (ckpt_write_one_thread_siginfo(co, pp, thread) < 0) 
			return (-1);
	}
	return (0);
}

/*
 * Signal restoration
 */
static sigset_t jobcontrol;

static int
ckpt_requeue_siginfo(int prfd, siginfo_t *infop)
{
	IFDEB1(cdebug("signo %d:si_addr %lx\n", infop->si_signo, infop->si_addr));

	if (restart_ioctl(prfd, PIOCCKPTQSIG, infop) <0) {
		/*
		 * EAGAIN means all info structs occupied.  This
		 * *should not* happen, since we are requeuing only
		 * the signals present at checkpoint!
		 */
		cerror("ckpt_requeue_siginfo:PIOCCKPTQSIG (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

static int
ckpt_zombie_count(ckpt_ta_t *tp)
{
	int i;
	int zcount;
	ckpt_pl_t *pl;

	for (i = 0, zcount = 0; i < tp->nchild; i++) {
		pl = ckpt_get_proclist_entry(tp->cpid[i]);
		if (pl->pl_states & CKPT_PL_ZOMBIE)
			zcount++;
	}
	return (zcount);
}

int
ckpt_read_siginfo(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	static int once = 0;

	if (once == 0) {
		/*
		 * Init jobcontrol signal dat
		 */
		sigaddset(&jobcontrol, SIGSTOP);
		sigaddset(&jobcontrol, SIGTSTP);
		sigaddset(&jobcontrol, SIGTTIN);
		sigaddset(&jobcontrol, SIGTTOU);
		sigaddset(&jobcontrol, SIGCONT);

		once = 1;
	}
	return (ckpt_rxlate_siginfo(cr.cr_pci.pci_revision, tp, magic));
}

int
ckpt_restore_sigstate(ckpt_sig_t *sigp)
{
	struct sigstack sstk;
	/*
	 * See if we need to set up a signal stack
	 */
	if(sigp->cs_altstack.ss_sp) {
		/*
		 * Now see if sigstack (ss_size is 0) or
		 * sigaltstack (ss_size != 0)
		 */
		if (sigp->cs_altstack.ss_size) {

			sigp->cs_altstack.ss_flags &= ~SS_ONSTACK;

			if (sigaltstack(&sigp->cs_altstack, NULL) < 0) {
				cerror("restore_sigaction: sigaltstack (%s)\n", STRERR);
				return (-1);
			}
		} else {
			sstk.ss_sp = sigp->cs_altstack.ss_sp;
			sstk.ss_onstack = sigp->cs_altstack.ss_flags & SS_ONSTACK;
			if (sigstack(&sstk, NULL) < 0) {
				cerror("restore_sigaction: sigstack (%s)\n", STRERR);
				return (-1);
			}
		}
	}
	return (0);
}

static int
ckpt_requeue_thread_signal(int prfd, tid_t tid, int signo, siginfo_t *infop)
{
	ckpt_cursig_arg_t arg;

	assert(!infop || (infop->si_signo == signo));

	arg.signo = signo;
	arg.infop = (caddr_t)infop;

	if (restart_thread_ioctl(prfd, tid, PIOCCKPTSIGTHRD, &arg) < 0) {
		cerror("thread_ioctl (%s)\n", STRERR);
		return(-1);
	}
	return (0);
}

int
ckpt_read_thread_siginfo(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	int prfd = proclist[tp->pindex].pl_prfd;
	ckpt_thread_sig_t cts;
	uint_t warned;
	ckpt_cursig_arg_t arg;
	siginfo_t curinfo;
	siginfo_t *infop, *sip = NULL;
	int i;
	int signo;

	warned = ckpt_fetch_proclist_states(tp->pindex);
	warned &= CKPT_PL_RESTARTWARN;
	/*
	 * state is layed out with ckpt_thread_sig headers followed by
	 * a sininfo struct, if there's a cursig, and then followed by
	 * array of queued siginfos
	 */
	if (read(tp->sfd, &cts, sizeof(cts)) < 0) {
		cerror("statefile read (%s)\n", STRERR);
		return(-1);
	}
	if (cts.cts_magic != magic) {
		setoserror(EINVAL);
		cerror("incorrect per thread signal magic %8.8s (v.s. %8.8s)\n",
			&cts.cts_magic, &magic);
		return (-1);
        }
	if (cts.cts_cursig) {
		if (read(tp->sfd, &curinfo, sizeof(siginfo_t)) < 0) {
			cerror("read thread siginfo (%s)\n", STRERR);
			free(sip);
			return (-1);
		}
	}
	if (cts.cts_ninfo) {
		sip = (siginfo_t *)malloc(cts.cts_ninfo * sizeof(siginfo_t));
		if (sip == NULL) {
			cerror("malloc (%s)\n", STRERR);
			return (-1);
		}
		if (read(tp->sfd, sip, cts.cts_ninfo * sizeof(siginfo_t)) < 0) {
			cerror("read thread siginfo (%s)\n", STRERR);
			free(sip);
			return (-1);
		}
	}
	if (warned && cts.cts_cursig) {
		/*
		 * demote cursig...
		 */
		sigaddset(&cts.cts_pending, cts.cts_cursig);

		if (curinfo.si_signo) {

			assert(cts.cts_cursig == curinfo.si_signo);

			if (ckpt_requeue_thread_signal(	prfd,
							cts.cts_tid,
							curinfo.si_signo,
							&curinfo) < 0) {
				if (sip) free(sip);
				return (-1);
			}
		}
		cts.cts_cursig = 0;
	}
	if (cts.cts_cursig) {

                IFDEB1(cdebug("cursig %d, %d\n", cts.cts_cursig, curinfo.si_signo));

		assert((curinfo.si_signo == 0)||(curinfo.si_signo == cts.cts_cursig));
                arg.signo = cts.cts_cursig;
                arg.infop = (curinfo.si_signo)? (caddr_t)&curinfo : NULL;

		if (restart_thread_ioctl(prfd, cts.cts_tid, PIOCCKPTSSIG, &arg) < 0) {
                        cerror("ckpt_requeue_signals:PIOCCKPTSSIG (%s)\n", STRERR);
			if (sip) free(sip);
                        return (-1);
                }
        }
	/*
	 * Requeue signals with info structs.  Not going to worry about
	 * queue ordering other than that enforced by kernel. Can't believe
	 * any app sensitive to that!
	 */
	for (i = 0, infop = sip; i < cts.cts_ninfo; i++, infop++) {
		int rv;

		if (sigismember(&jobcontrol, infop->si_signo))
			rv = ckpt_requeue_siginfo(prfd, infop);
		else 
			rv = ckpt_requeue_thread_signal(prfd,
							cts.cts_tid,
							infop->si_signo,
							infop);
		if (rv < 0) {
			free(sip);
			return (-1);
		}
		sigdelset(&cts.cts_pending, infop->si_signo);
	}
	/*
	 * Post remaining signals.  
	 */
	while ((signo = sgi_sigffset(&cts.cts_pending, 1)) > 0) {
		int rv;

		IFDEB1(cdebug("repost signal %d\n", signo));

		if (sigismember(&jobcontrol, signo))
			rv = restart_ioctl(prfd, PIOCKILL, &signo);
		else
			rv = ckpt_requeue_thread_signal(prfd,
							cts.cts_tid,
							signo,
							NULL);
		if (rv > 0) {
			cerror("ckpt_requeue_signals:PIOCKILL (%s)\n", STRERR);
			if (sip) free(sip);
			return (-1);
		}
	}
	assert(sgi_siganyset(&cts.cts_pending) == 0);
	if (sip) free(sip);

	return (0);
}

int
ckpt_requeue_signals(ckpt_ta_t *tp)
{
	int prfd = proclist[tp->pindex].pl_prfd;
	ckpt_sig_t *sigp = &tp->sighdr;
	siginfo_t *sip = tp->sip;
	struct sigaction *sap = tp->sap;
	sigset_t queuedsigs;
	siginfo_t *infop;
	int i;
	int signo;
	int zcount;

	if (tp->pi.cp_stat == SZOMB)
		return (0);

	if (zcount = ckpt_zombie_count(tp)) {
		/*
		 * Cancel any posted SIGCLD caused by zombies exiting
		 * Going to bute force do it once for each possible zombie
		 * in case target is/was interested in siginfo
		 * Possible to have changed handler/flags??
		 */
		signo = SIGCLD;
		for (i = 0; i < zcount; i++) {
			if (restart_ioctl(prfd, PIOCUNKILL, &signo) < 0) {
				cerror("ckpt_requeue_signals:unkill (%s)\n", STRERR);
				return (-1);
			}
		}
	}
	queuedsigs = sigp->cs_pending;

	if ((sap[SIGRESTART-1].sa_handler != SIG_DFL) &&
	    (sap[SIGRESTART-1].sa_handler != SIG_IGN)) {

		if (kill(proclist[tp->pindex].pl_pid, SIGRESTART) < 0) {
			cerror("ckpt_requeue_signals:kill (%s)\n", STRERR);
			return (-1);
		}
		sigdelset(&queuedsigs, SIGRESTART);
		ckpt_update_proclist_states(tp->pindex, CKPT_PL_RESTARTWARN);
	}
	/*
	 * Now requeue threads signals
	 */
	if (ckpt_read_proc_property(tp, CKPT_MAGIC_THRDSIGINFO))
		return (-1);
	/*
	 * Requeue signals with info structs.  Not going to worry about
	 * queue ordering other than that enforced by kernel. Can't believe
	 * any app sensitive to that!
	 */
	for (i = 0, infop = sip; i < sigp->cs_ninfo; i++, infop++) {

		if (ckpt_requeue_siginfo(prfd, infop) < 0)
			return (-1);
	
		sigdelset(&queuedsigs, infop->si_signo);
	}
	/*
	 * Post remaining signals.  
	 */
	while ((signo = sgi_sigffset(&queuedsigs, 1)) > 0) {

		IFDEB1(cdebug("repost signal %d\n", signo));

		if (restart_ioctl(prfd, PIOCKILL, &signo) < 0) {
			cerror("ckpt_requeue_signals:PIOCKILL (%s)\n", STRERR);
			return (-1);
		}
	}
	assert(sgi_siganyset(&queuedsigs) == 0);

	return (0);
}
