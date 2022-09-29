/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.62 $"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/errno.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

static int ckpt_warn_procs(ckpt_obj_t *);
static int ckpt_stop_one_thread(ckpt_obj_t *co, ckpt_thread_t *thread);

#define	CKPT_STOP_TIMEOUT	30
#define	CKPT_STOP_RETRY		100

static int
ckpt_parent_stopped(ckpt_obj_t *zombie)
{
	ckpt_obj_t *co;

	assert(zombie->co_psinfo);

	FOR_EACH_PROC(zombie->co_ch->ch_first, co) {
		if (co->co_pid == zombie->co_psinfo->pr_ppid)
			return (co->co_flags & CKPT_CO_STOPPED);
	}
	/*
	 * didn't find parent
	 */
	return (0);
}

static int
ckpt_ioctl(ckpt_obj_t *co, int request, void *arg, int maxretry)
{
	int retries = 0;

retry:
	if (ioctl(co->co_prfd, request, arg) < 0) {
		if (oserror() == ENOENT) {
			/*
			 * Force rescan
			 */
			co->co_flags |= CKPT_CO_EXITED;
			return (-1);
		}
		if ((oserror() == EAGAIN)&&(retries++ < maxretry))
			goto retry;

		cerror("/proc ioctl failed (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}
/*
 * operate on a specific thread
 */
int
ckpt_thread_ioctl(ckpt_obj_t *co, tid_t tid, int exit_ok, int cmd, void *arg)
{
	int rv;
	prthreadctl_t prt;

	prt.pt_tid = tid;
	prt.pt_cmd = cmd;
	prt.pt_flags = PTFS_ALL | PTFD_EQL;
	prt.pt_data = arg;

	if ((rv = ioctl(co->co_prfd, PIOCTHREAD, &prt)) < 0) {
		if ((exit_ok)&&((oserror() == ENOENT)||(oserror() == ESRCH))) {
			/*
			 * Force rescan
			 */
			co->co_flags |= CKPT_CO_EXITED;
			return (-1);
		}
		cerror("PIOCTHREAD (%s)\n", STRERR);
		return (-1);
	}
	return (rv);
}
		
static int
ckpt_wait_stop(ckpt_obj_t *co, ckpt_thread_t *thread, prstatus_t *status)
{
	int loops = 0;
	tid_t tid = thread->ct_id;
	ckpt_uti_t *uti = &thread->ct_uti;
	/*
	 * co->co_pinfo->cp_psi initialized upon stopping the
	 * proc
	 */
	do {
		if (loops++)
			sginap(0);

		if (ckpt_thread_ioctl(co, tid, 1, PIOCCKPTUTINFO, uti) < 0)
			return (-1);
	}
	while (uti->uti_whystop == 0 && !uti->uti_isblocked);
	
	if (ckpt_thread_ioctl(co, tid, 1, PIOCSTATUS, status) < 0)
		return (-1);

	return (0);
}

static int
ckpt_fullrestore_syscall(long sysnum)
{
	IFDEB1(cdebug("fullrestore sysnum %d?\n", sysnum));

	switch (sysnum) {
	case SYS_sigreturn:
	case SYS_setcontext:
		return (1);
	default:
		break;
	}
	return (0);
}

static int
ckpt_reissue_syscall(ckpt_obj_t *co, tid_t tid, long sysnum, greg_t *gregp)
{
	IFDEB1(cdebug("reissue sysnum %d?\n", sysnum));

	switch (sysnum) {
	case SYS_waitsys:
#ifdef DEBUG_WAITSYS
		printf("pid %d restart waitsys rv %ld %ld epxc %lx\n",
			co->co_pid,
			gregp[CXT_A3],
			gregp[CXT_V0],
			gregp[CXT_EPC]);
#endif
		return (1);
	case SYS_pause:
	case SYS_sigsuspend:
		return (1);
	case SYS_sginap:
		/*
		 * Only sleep the remaining time
		 */
		if (gregp[CXT_V0] < gregp[CXT_A0])
			gregp[CXT_A0] = gregp[CXT_V0];

		if (ckpt_thread_ioctl(co, tid, 1, PIOCSREG, gregp) < 0)
			return (-1);

		return (1);
	case SYS_sigpoll:
		/*
		 * workaround sigpoll EAGAIN bug
		 */
		if ((gregp[CXT_A3] > 0)&&(gregp[CXT_V0] == EAGAIN))
			return (1);
		break;
	default:
		break;
	}
	return (0);
}

static int
ckpt_syscall_abort(ckpt_obj_t *co, ckpt_thread_t *thread, long sysnum, int how,
		   prstatus_t *status)
{
	tid_t tid = thread->ct_id;
	ckpt_ctxt_t *ctxp = &thread->ct_ctxt;
	greg_t saved_a3;
	greg_t *gregp = ctxp->cc_ctxt.uc_mcontext.gregs;
	int backup_pc = status->pr_what;

#ifdef DEBUG_HARDTEST
	cdebug("pid %d syscall abort sysnum %d how %d backup %d epc %lx v0 %lx a0 %ld a3 %lx\n",
		co->co_pid, sysnum, how, backup_pc, gregp[CXT_EPC], gregp[CXT_V0],
		gregp[CXT_A0], gregp[CXT_A3]);
#endif
	IFDEB1(cdebug("syscall abort sysnum %d how %d\n", sysnum, how));
	/*
	 * refetch context now that we know it's stopped
	 */
	saved_a3 = gregp[CXT_A3];

	if (ckpt_thread_ioctl(co, tid, 1, PIOCCKPTGETCTX, ctxp)  < 0)
		return (-1);

	/*
	 * if we caught it on it's way to a signal, don't dink with
	 * registers
	 */
	if (how == SYSTRAMP)
		return (0);

	sysnum--;		/* adjust for /proc increment */
	sysnum += SYSVoffset;
	
	/*
	 * If it's a full-restore syscall, don't dink regiaters
	 */
	if (ckpt_fullrestore_syscall(sysnum))
		return (0);

	/*
	 * If it's a syscall that always reissues or if returning an
	 * EINTR, back up and reissue syscall.
	 * Must reload v0 and a3.
	 *
	 * Otherwise, just let it continue with partial results
	 * or real error
	 *
	 * Do this for process we're checkpointing and for
	 * state file
	 */
	if (ckpt_reissue_syscall(co, tid, sysnum, gregp)) {
		/*
		 * If the targt is blocked, do not set reissue flag
		 * (which tells restart to advance into syscall entry)
		 * since process will be blocked at restart
		 */
		if (thread->ct_uti.uti_blkcnt <= 0)
			thread->ct_flags |= CKPT_THREAD_REISSUE;

		gregp[CXT_EPC] -= 8;	/* cause v0 reload */
		gregp[CXT_A3] = saved_a3;
		
		if (!backup_pc) {
			if (ckpt_thread_ioctl(co, tid, 1, PIOCSREG, gregp) < 0)
				return (-1);
		}
	} else if (backup_pc) {
		/*
		 * The kernel is going to backup the pc of the target
		 * process due our cancelling of the signal.  Reflect that
		 * in the saved context.
		 */
		gregp[CXT_EPC] -= 8;	/* cause v0 reload */
		gregp[CXT_A3] = saved_a3;

	} else if ((gregp[CXT_A3] > 0)&&(gregp[CXT_V0] == EINTR)) {
		/*
		 * This should never happen...should be caught by
		 * backup_pc
		 */
		assert(0);

		gregp[CXT_EPC] -= 8;	/* cause v0 reload */
		gregp[CXT_A3] = saved_a3;
/*
 *		Don't do this for process anymore...the kernel is going
 *		to back up the context based upon the cancelled signal
 *		and the EINTR
 *
		if (ckpt_thread_ioctl(co, tid, 1, PIOCSREG, gregp) < 0)
			return (-1);
 */
	}
	return (0);
}

/*
 * return 1 if the process is checkpointable, 0 otherwise.
 */
static int
ckpt_proc_checkpointable(ckpt_obj_t *co)
{
	ckpt_statbuf_t sbuf;
	int i, nofiles;
	char *fdtype;

	/* If fd sharing sproc and another member already did this */
	if (co->co_flags & CKPT_CO_ZOMBIE)
		return 1;
	if(ckpt_count_openfiles(co))
		return 0;
	nofiles = co->co_nofiles;
	/* loop on all open files */
	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {
		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
			return 0;
		}
		if (fdtype = ckpt_unsupported_fdtype(&sbuf)) {
			cerror("Uncheckpointable fd, pid %d fd %d type %s\n",
			co->co_pid, sbuf.ckpt_fd, fdtype);
			setoserror(ECKPT);
			return 0;
		}
	}
	return 1;
}

static int
ckpt_stop_one_proc(ckpt_obj_t *co)
{
	int stop;
	ckpt_psi_t zinfo;
	ckpt_thread_t *thread;
#define MAXRETRIES	10

	IFDEB1(cdebug("stopping %d (fd=%d)\n", co->co_pid, co->co_prfd));

	if ((co->co_prfd == -1)||(co->co_flags & CKPT_CO_EXITED))
		return (0);

	if (co->co_flags & CKPT_CO_ZOPEN) {
		/*
		 * If parent is stopped and zombie still exists, then
		 * it's not going anywhere
		 */
		int stopped; 

		if ((stopped = ckpt_parent_stopped(co)) < 0)
			return (-1);
		if (!stopped) {
			co->co_flags |= CKPT_CO_EXITED;
			return (0);
		}
		do {
			if (ckpt_ioctl(co, PIOCCKPTPSINFO, &zinfo, 0) < 0)
				return (-1);
		} while ((zinfo.ps_state != SZOMB)&&
			 !(zinfo.ps_ckptflag&CKPT_PROC_EXIT));

		if (ckpt_ioctl(co, PIOCPSINFO, co->co_psinfo, 0) < 0)
			return (-1);

		co->co_flags |= CKPT_CO_CTXT;

		return (0);
	}
	/*
	 * Might get here stopped in sigreturn
	 */
	if ((co->co_flags & CKPT_CO_STOPPED) == 0) {
		/*
		 * Procs can exit out from under us, so check ENOENT
		 */
#ifdef NOTDEF
/*
 * why do we bother with this???
 */
		sysset_t exitset;
		/*
		 * Register to catch syscall exit
		 */
		prfillset(&exitset);
		if (ckpt_ioctl(co, PIOCSEXIT, &exitset, 0) < 0)
			return (-1);
#endif
		/*
		 * Register to catch checkpoint stop
		 */
		stop = 1;
		for (thread = co->co_thread; thread; thread = thread->ct_next) {
			if (ckpt_thread_ioctl(co, thread->ct_id, 1,
					      PIOCCKPTSTOP, &stop) < 0)
				return (-1);
		}
		/*
		 * Ask and wait for process to stop
		 */
		if (ckpt_ioctl(co, PIOCSTOP, NULL, 0) < 0)
			return (-1);

		co->co_flags |= CKPT_CO_STOPPED;
	}

	/* Now, the process has stopped and won't be able to run in user mode.
	 * We want to check if the process is checkpointable before aborting
	 * the system call by PIOCCKPTABORT.
	 */
	if(!ckpt_proc_checkpointable(co))  {
		return -1;
	}
	
	/*
	 * Now grab context...will either get it or an error,
	 * so mark it now
	 *
	 * Get process info first, then get thread specific info
	 */
	if ((co->co_flags & CKPT_CO_CTXT) == 0) {

		co->co_flags |= CKPT_CO_CTXT;
		/*
		 * Get additional status info
		 */
		if (ckpt_ioctl(co, PIOCCKPTPSINFO, &co->co_pinfo->cp_psi, 0) < 0)
			return (-1);
	

		/*
		 * fetch itimers, stop real itimer
		 */
		if (ckpt_ioctl(co, PIOCCKPTGETITMRS,
			       co->co_pinfo->cp_itimer, MAXRETRIES) < 0)
			return (-1);
	}
	/*
	 * Process is stopped in snese that won't run again in user mode
	 * until PIOCRUN'd, but can still run in the kernel.  Get it 
	 * really stopped and grab context in the process
	 */
	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (ckpt_stop_one_thread(co, thread) < 0)
			return (-1);
	}
	return (0);
}

static int
ckpt_stop_one_thread(ckpt_obj_t *co, ckpt_thread_t *thread)
{
	tid_t tid = thread->ct_id;
	int rc;
	greg_t *gregp = thread->ct_ctxt.cc_ctxt.uc_mcontext.gregs;
	ckpt_uti_t *uti = &thread->ct_uti;
	prstatus_t status;
	long sysnum;
	int how;
	int restop = 0;

	if (thread->ct_flags & CKPT_THREAD_CTXT)
		return (0);

	thread->ct_flags |= CKPT_THREAD_CTXT;
again:
	rc = ckpt_thread_ioctl(co, tid, 1, PIOCSTATUS, &status);
	if (rc != 0)
		return (rc);

	rc = ckpt_thread_ioctl(co, tid, 1, PIOCCKPTGETCTX, &thread->ct_ctxt);
	if (rc != 0)
		return (rc);

	rc = ckpt_thread_ioctl(co, tid, 1, PIOCCKPTUTINFO, uti);
	if (rc != 0)
		return (rc);

#ifdef DEBUG_HARDTEST
	cdebug("STOP:pid %d why %d what %d sysnum %d blkcnt %d isblkd %d epc %lx v0 %lx a0 %ld a3 %lx\n",
			co->co_pid,
			status.pr_why,
			status.pr_what,
			status.pr_syscall,
			uti->uti_blkcnt,
			uti->uti_isblocked,
			gregp[CXT_EPC],
			gregp[CXT_V0],
			gregp[CXT_A0],
			gregp[CXT_A3]);
#endif
	if (uti->uti_isblocked) {
		if (uti->uti_flags & CKPT_UTI_BLKONENTRY) {
			/*
			 * We need to reissue the syscall in the saved image.
			 * Leave the current image alone.
			 */
	 		gregp[CXT_EPC] -= 8;    /* cause v0 reload */
		}
		return (0);
	}
	if ((status.pr_why == 0)||(status.pr_why == REQUESTED)) {

		IFDEB1(cdebug("why == REQUESTED!\n"));

		sysnum = status.pr_syscall;
		/*
		 * If it's in a slow syscall, abort it...
		 */
		if ((how = ckpt_thread_ioctl(co, tid, 1, PIOCCKPTABORT, &status)) < 0)
			return (how);

		IFDEB1(cdebug("thread CKPTABORT how %d why %d what %d\n",
					how, status.pr_why, status.pr_what));

		if (how > 0) {
			/*
			 * syscall abort, stop abort and syscall end treated
			 * uniformly...get updated regs, check for EINTR,
			 * etc
			 */
			if (ckpt_syscall_abort(co, thread, sysnum, how, &status) < 0)
				return (-1);
			return (0);
		}
		/*
		 * wait for 'real stop', then fall through
		 */
		if (ckpt_wait_stop(co, thread, &status) < 0)
			return (-1);
	}
	if (status.pr_why == JOBCONTROL) {

		IFDEB1(cdebug("why == JOBCONTROL!\n"));

		if ((how = ckpt_thread_ioctl(co, tid, 1, PIOCCKPTABORT, &status)) < 0)
			return (-1);

		else if (how == 0) {
			/*
			 * Process moved out from under us. before we could 
			 * abort jobcontrol stop.  Wait for proc to stop again,
			 * maybe on same or different reason, and go back to the
			 * top
			 */
			if (ckpt_thread_ioctl(co, tid, 1, PIOCWSTOP, &status) < 0)
				return (-1);

			goto again;
		}
		if (status.pr_why == REQUESTED) {
			/*
			 * A process will stop REQUESTED in a block or
			 * on stop request *before* SYSEXIT
			 * in syscall exit, or before CHECKPOINT in trap
			 * exit.
			 * 
			 * Check for a block.  If it is, dump it and return.
			 *
			 * Otherwise, run the process so it stops on either
			 * SYSEXIT or CHECKPOINT, then PIOCSTOP it again
			 */
			restop = 1;
			if (ckpt_thread_ioctl(co, tid, 1, PIOCRUN, NULL) < 0)
				return (-1);

			if (ckpt_thread_ioctl(co, tid, 1, PIOCWSTOP, &status) < 0)
				return (-1);

		}
		sysnum = status.pr_syscall;
		if ((uti->uti_isblocked && (sysnum > 0)) ||
		    (status.pr_why == SYSEXIT)) {
			/*
			 * We aborted a syscall
			 */
			if (ckpt_syscall_abort(co, thread, sysnum, how, &status) < 0)
				return (-1);
		} else {
			/*
			 * We were done with syscall or were in a trap
			 */
			assert(status.pr_why == CHECKPOINT);
		}
		if (restop) {
			if (ckpt_thread_ioctl(co, tid, 1, PIOCSTOP, NULL) < 0)
				return (-1);

		}
		return (0);
	}
	return (0);
}

int
ckpt_stop_procs(ch_t *cp)
{
	ckpt_obj_t *co;
	int count = 0, rc;

	while (count++ <= CKPT_STOP_RETRY) {

		if (ckpt_perm_check(cp->ch_first))
			return (EPERM);

		if (rc = ckpt_warn_procs(cp->ch_first))
			return (rc);

		FOR_EACH_PROC(cp->ch_first, co) {

			if (((rc = ckpt_stop_one_proc(co)) < 0)&&
			    ((co->co_flags & CKPT_CO_EXITED) == 0))
				return (rc);

		}
		if ((rc = ckpt_update_proc_list(cp)) <= 0)
			return (rc);
	}
	cerror("Exhausted process stop retry count\n");
	return (-1);
}
/*
 * Terminate the process
 *
 * Not as easy said as done.  Procs that are "blocked" (blockproc)
 * won't unblock on delivery of fatal sig *if* we've got em /proc open.
 * Can't just unblock since could be pthreaded, in which case unblockproc
 * will fail.
 *
 * So, clear run-on-last-close, close, kill, open, run (so will wakeup an
 * process signal)
 *
 * Keep trying, even in the event an error
 */
static void
ckpt_terminate_proc(ckpt_obj_t *co)
{
	char procname[32];
	ulong_t prflags;
	
	prflags = PR_RLC;
	if (ioctl(co->co_prfd, PIOCRESET, &prflags) < 0) {
		cerror("PIOCRESET (%s)", STRERR);
	}
	(void)close(co->co_prfd);
	(void)kill(co->co_pid, SIGKILL);

	sprintf(procname, "/proc/%d", co->co_pid);
	if ((co->co_prfd = open(procname, O_RDWR)) < 0) {
		if (oserror() == ENOENT)
			return;
		cerror("/proc open of %s (%s)\n", procname, STRERR);
		/*
		 * pointless to attempt ioctl
		 */
		return;
	}
	if (ioctl(co->co_prfd, PIOCRUN, 0) < 0) {
		if ((oserror() == EBUSY)||(oserror() == ENOENT))
			return;
		cerror("PIOCRUN (%s)\n", STRERR);
	}
}

/*
 * Clear stop conditions for processes.  Either terminate or continue the
 * processes
 */
int 
ckpt_continue_proc(ckpt_obj_t *co, int terminate) 
{
	int stop = 0;
	sysset_t exitset;
	ulong_t dbgflags;
	ckpt_thread_t *thread;

	premptyset(&exitset);

	if (co->co_prfd == -1)
		return(0);

	if ((co->co_flags & CKPT_CO_STOPPED) == 0)
		/*
		 * never got stopped in the first place
		 */
		return (0);

	if (ioctl(co->co_prfd, PIOCSEXIT, &exitset) < 0) {
		if (oserror() == ENOENT)
			return(0);
		cerror("failed to rerun process %d (%s)\n",
			co->co_pid, STRERR);
		return (-1);
	}
	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (ckpt_thread_ioctl(co, thread->ct_id, 1,
				      PIOCCKPTSTOP, &stop) < 0) {
			if (oserror() == ENOENT)
				continue;
			return (-1);
		}
	}
	/*
	 * Check for fatal sigs
	 */
	dbgflags = PR_CKF;
	if (ioctl(co->co_prfd, PIOCSET,  &dbgflags) < 0) {
		if (oserror() == ENOENT)
			return(0);
		cerror("failed to set flag process %d (%s)\n",
			co->co_pid, STRERR);
		return (-1);
	}
	if (terminate) {
		ckpt_terminate_proc(co);

	} else {
		int signo = SIGRESTART;
		/*
		 * Let proc know that recess is over!
		 */
		if (ioctl(co->co_prfd, PIOCKILL, &signo) < 0) {
			if (oserror() == ENOENT)
				return (0);
			cerror("Failed to restart signal to  process %d (%s)\n",
				co->co_pid, STRERR);
		}
		if (timerisset(&co->co_pinfo->cp_itimer[ITIMER_REAL].it_value)) {
			/*
			 * restart real itimer
			 */
			if (ioctl(co->co_prfd, PIOCCKPTSETRITMR,
			    &co->co_pinfo->cp_itimer[ITIMER_REAL]) < 0) {
				if (oserror() == ENOENT)
					return (0);
				cerror("Failed to restart real itimer "
				       "for process %d (%s)\n",
					co->co_pid, STRERR);
				return (-1);
			}
		}
		if (ioctl(co->co_prfd, PIOCRUN, 0) < 0) {
			if (oserror() == ENOENT || oserror() == EBUSY)
				return (0);
			cerror("failed to rerun process %d (%s)\n",
				co->co_pid, STRERR);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_continue_procs(ckpt_obj_t *first, int terminate)
{
	ckpt_obj_t *co;

	FOR_EACH_PROC(first, co) {

		if (ckpt_continue_proc(co, terminate) < 0)
			return (-1);
	}
	return (0);
}
/*
 * Send checkpoint warining to each thread in a process
 *
 * return:
 * 0 on sucess
 * -1 on error
 * 1 if process exited
 */
static int
ckpt_warn_threads(ckpt_obj_t *co)
{
	ckpt_thread_t *thread;
	ckpt_cursig_arg_t arg;
	int mark = 1;

	arg.signo = SIGCKPT;
	arg.infop = NULL;

	if (ioctl(co->co_prfd, PIOCCKPTMARK, &mark) < 0) {
		if (oserror() == ENOENT)
			return (1);
		cerror("warn threads:PIOCCKPTMARK:%s", STRERR);
		return (-1);
	}
	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (thread->ct_flags & CKPT_THREAD_WARN)
			continue;

		if (ckpt_thread_ioctl(co, thread->ct_id, 1, PIOCCKPTSIGTHRD, &arg) < 0) {
			if (oserror() == ENOENT)
				continue;
			cerror("warn threads:PIOCCKPTSIGTHRD:%s", STRERR);
			return (-1);
		}
		thread->ct_flags |= CKPT_THREAD_WARN;
		co->co_flags |= CKPT_CO_WARN;
	}
	return (0);
}
/*
 * Wait for warned threads to return from warning
 */
static int
ckpt_warnret_threads(ckpt_obj_t *co)
{
	ckpt_thread_t *thread;
	int clear = 0;

	if ((co->co_flags & CKPT_CO_WARN) == 0)
		/*
		 * No threads warned
		 */
		goto clear_mark;

	co->co_flags &= ~CKPT_CO_WARN;

	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if ((thread->ct_flags & CKPT_THREAD_WARN) == 0) {
			/*
			 * this "shouldn't" happn
			 */
			assert(0);
			continue;
		}
		if (thread->ct_flags & CKPT_THREAD_WARNRET)
			continue;

		if (ckpt_thread_ioctl(co, thread->ct_id, 1, PIOCWSTOP, NULL) < 0) {
			if (oserror() == ENOENT)
				continue;
			cerror("warnret threads:PIOCWSTOP");
			return (-1);
		}
		thread->ct_flags |= CKPT_THREAD_WARNRET;
	}
clear_mark:
	if (ioctl(co->co_prfd, PIOCCKPTMARK, &clear) < 0) {
		if (oserror() == ENOENT)
			return (1);

		cerror("warnret threads:PIOCCKPTMARK:%s", STRERR);
		return (-1);
	}
	return (0);
}

/*
 * warn processes that handle SIGCKPT
 */
static int
ckpt_warn_procs(ckpt_obj_t *first)
{
	ckpt_obj_t *co;
        sigaction_t *sap;
	int nsigs;
	int rc;
	/*
	 * fetch sigction arrays as necessary, and send ckpt warnings
	 */
	FOR_EACH_PROC(first, co) {
		if ((co->co_prfd == -1) ||
		    (co->co_flags & CKPT_CO_ZOPEN) ||
		     co->co_sap)
			continue;
		/*
		 * Get the number of signals, allocate space for the sigaction 
		 * array and fetch out the sigaction array
		 *
		 * Processes can exit out from under us, so check for
		 * ENOENT.
		 */
		if (ioctl(co->co_prfd, PIOCMAXSIG, &nsigs) < 0) {
			if (oserror() == ENOENT)
				continue;
			cerror("warn procs:PIOCMAXSIG:%s", STRERR);
			return (-1);
		}
		sap = (sigaction_t *)malloc(nsigs * sizeof(sigaction_t));
		if (sap == NULL) {
			cerror("warn procs:malloc");
			return (-1);
		}
		if (ioctl(co->co_prfd, PIOCACTION, sap) < 0) {
			if (oserror() == ENOENT) {
				free(sap);
				continue;
			}
			cerror("warn procs:PIOCACTION:%s", STRERR);
			return (-1);
		}
		if ((sap[SIGCKPT-1].sa_handler != SIG_DFL)&&
		    (sap[SIGCKPT-1].sa_handler != SIG_IGN)) {
			if ((rc = ckpt_warn_threads(co)) < 0)
				return (-1);

			if (rc == 1) {
				free(sap);
				continue;
			}
		}
		co->co_sap = sap;
		co->co_nsigs = nsigs;
	}
	/*
	 * wait for warning completion
	 */
	FOR_EACH_PROC(first, co) {
		if ((co->co_prfd == -1) ||
		    (co->co_flags & CKPT_CO_ZOPEN))
			continue;

		if ((rc = ckpt_warnret_threads(co)) < 0)
			return (-1);

	}
	return (0);
}
