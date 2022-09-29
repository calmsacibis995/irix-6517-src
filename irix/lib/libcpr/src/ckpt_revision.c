#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <signal.h>
#include <sys/mkdev.h>
#undef _KMEMUSER
#include <sys/usioctl.h>
#define _KMEMUSER
#include "ckpt_internal.h"
#include "ckpt_revision.h"

/*
 * Revision check.  Support current and 1 old revision
 */
int
ckpt_revision_check(ckpt_rev_t revision)
{
	if (revision < CKPT_REVISION_11 || revision > CKPT_REVISION_CURRENT)
		return (-1);
	return (0);
}
/*
 * ckpt_stat support
 */
off_t
ckpt_segobj_size(ckpt_rev_t revision)
{
	off_t segobj_size;

        if (revision < CKPT_REVISION_CURRENT)
                segobj_size = sizeof(ckpt_seg_old_t);
        else
                segobj_size = sizeof(ckpt_seg_t);

	return (segobj_size);
}
/*
 * Routines to read & translate objects
 */
int
ckpt_rxlate_context(ckpt_rev_t revision, ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_context_t *context)
{
       if (revision < CKPT_REVISION_CURRENT) {
                /*
                 * down rev..read & convert old context struct
                 */
		ckpt_ctxt_old_t old;
                context->cc_magic = CKPT_MAGIC_CTXINFO;
                context->cc_tid = 0;
		context->cc_flags = 0;

                if (read(tp->sfd, &old, sizeof(old)) < 0) {
                        cerror("Failed to read context (%s)\n", STRERR);
                        return (-1);
                }
		context->cc_ctxt.cc_ctxt = old.cc_ctxt;
		context->cc_ctxt.cc_abi = old.cc_abi;
		context->cc_ctxt.cc_fpflags = old.cc_fpflags;
        } else {
                if (read(tp->sfd, context, sizeof(ckpt_context_t)) < 0) {
                        cerror("Failed to read context (%s)\n", STRERR);
                        return (-1);
                }
        }
        IFDEB1(cdebug("read context (magic %8.8s)\n", &magic));

        if (context->cc_magic != magic) {
                cerror("mismatched magic %8.8s (vs. %8.8s)\n",
                        &context->cc_magic, &magic);
                setoserror(EINVAL);
                return (-1);
        }
	return (0);
}

static int
ckpt_read_siginfo_old(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_sig_old_t sighdr;
	ckpt_sig_old_t *sigp = &sighdr;
	siginfo_t siginfo;
	int signo;

	if (read(tp->sfd, (caddr_t)sigp, sizeof(ckpt_sig_old_t)) < 0) {
		cerror("Failed to read signal info (%s)\n", STRERR);
		return (-1);
	}
	if (sigp->cs_magic != magic) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&sigp->cs_magic, &magic);
		return (-1);
	}
	tp->sap = (sigaction_t *)malloc(sigp->cs_nsigs*sizeof(sigaction_t));
	if (tp->sap == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, (caddr_t)tp->sap,
		sigp->cs_nsigs * sizeof(sigaction_t)) < 0) {
		cerror("Failed to :read sigaction (%s)\n", STRERR);
		goto error;
	}
	/*
	 * If there is a current signal, demote it.
	 * If there's also a related iginfo struct, move that over
	 * to siginfo list.
	 */
	if (sigp->cs_cursig > 0) {
		if (read(tp->sfd, (caddr_t)&siginfo,
			sizeof(siginfo_t)) < 0) {
			cerror("siginfo:read curinfo (%s)\n", STRERR);
			goto error;
		}
	} else
		siginfo.si_signo = 0;

	if (sigp->cs_ninfo > 0) {
		tp->sip = (siginfo_t *)malloc(sigp->cs_ninfo*sizeof(siginfo_t));		if (tp->sip == NULL) {
			cerror("siginfo: malloc (%s)\n", STRERR);
			goto error;
		}
		if (read(tp->sfd, (caddr_t)tp->sip,
			sigp->cs_ninfo * sizeof(siginfo_t)) < 0) {
			cerror("siginfo:read siginfo (%s)\n", STRERR);
			goto error;
		}
	} else 
		tp->sip = NULL;
	/*
	 * We've read in all the info, now xlate.
	 * any cursig and curinfo will go into pending and queued info.
	 * any async signals merge into pending
	 */
	if (siginfo.si_signo) {
		siginfo_t *sip;

		assert(siginfo.si_signo == sigp->cs_cursig);

		sip = (siginfo_t *)malloc((sigp->cs_ninfo+1)*sizeof(siginfo_t));
		if (sip == NULL) {
			cerror("siginfo: malloc (%s)\n", STRERR);
			goto error;
		}
		bcopy(&siginfo, sip, sizeof(siginfo_t));
		if (tp->sip) {
			bcopy(tp->sip, &sip[1], sigp->cs_ninfo*sizeof(siginfo_t));
			free(tp->sip);
		}
		tp->sip = sip;
		sigp->cs_ninfo++;
	}
	if (sigp->cs_cursig)
		sigaddset(&sigp->cs_pending, sigp->cs_cursig);

	while (sgi_siganyset(&sigp->cs_async)) {

		signo = sgi_sigffset(&sigp->cs_async, 1);

		sigaddset(&sigp->cs_pending, signo);

		sigdelset(&sigp->cs_async, signo);
	}
	/*
	 * Mow copy over to sighdr
	 */
	tp->sighdr.cs_magic = sigp->cs_magic;
	tp->sighdr.cs_nsigs = sigp->cs_nsigs;
	tp->sighdr.cs_ninfo = sigp->cs_ninfo;
	tp->sighdr.cs_altstack = sigp->cs_altstack;
	tp->sighdr.cs_pending = sigp->cs_pending;
	tp->sighdr.cs_sigtramp = sigp->cs_sigtramp;

	return (0);

error:
	free(tp->sap);
	free(tp->sip);
	return (-1);
}


int
ckpt_rxlate_siginfo(ckpt_rev_t revision, ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_sig_t *sigp = &tp->sighdr;

       	if (revision < CKPT_REVISION_CURRENT)
		return (ckpt_read_siginfo_old(tp, magic));

	if (read(tp->sfd, (caddr_t)sigp, sizeof(ckpt_sig_t)) < 0) {
		cerror("Failed to read signal info (%s)\n", STRERR);
		return (-1);
	}
	if (sigp->cs_magic != magic) {
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&sigp->cs_magic, &magic);
		setoserror(EINVAL);
		return (-1);
	}
	tp->sap = (sigaction_t *)malloc(sigp->cs_nsigs*sizeof(sigaction_t));
	if (tp->sap == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, (caddr_t)tp->sap,
		sigp->cs_nsigs * sizeof(sigaction_t)) < 0) {
		cerror("Failed to :read sigaction (%s)\n", STRERR);
		goto error;
	}
	if (sigp->cs_ninfo > 0) {
		tp->sip = (siginfo_t *)malloc(sigp->cs_ninfo*sizeof(siginfo_t));
		if (tp->sip == NULL) {
			cerror("siginfo: malloc (%s)\n", STRERR);
			goto error;
		}
		if (read(tp->sfd, (caddr_t)tp->sip,
			sigp->cs_ninfo * sizeof(siginfo_t)) < 0) {
			cerror("siginfo:read siginfo (%s)\n", STRERR);
			goto error;
		}
	}
	return (0);

error:
	free(tp->sap);
	free(tp->sip);
	return (-1);
}
/*
 * Some pending issues regarding UIOCBLOCK, per-process prepost count
 */
int
ckpt_xlate_fdattr_special(ckpt_f_t *fp, int fd)
{
	extern cr_t cr;
	extern int validusema_dev;
	extern dev_t usemaclone_dev;

	if (cr.cr_pci.pci_revision == CKPT_REVISION_CURRENT)
		return (0);

	if (isatty(fd))
		return (0);

	if (ckpt_is_special(fp->cf_rdev) <= 0)
		return (-1);

	if (validusema_dev && (major(fp->cf_rdev) == major(usemaclone_dev))) {
		ussemastate_old_t *old = (ussemastate_old_t *)fp->cf_auxptr;
		ussemastate_t *ussema;
		int size;
		int i;

		size = (int)(sizeof(ussemastate_t) +
			    old->nfilled * sizeof(struct ussematidstate_s));

		ussema = (ussemastate_t *)malloc(size);
		if (ussema == NULL) {
			cerror("malloc (%s)\n", STRERR);
			return (-1);
		}
		old->pidinfo = (struct ussemapidstate_old_s *)
				((char *)old + sizeof(ussemastate_old_t));
		ussema->tidinfo = (struct ussematidstate_s *)
				((char *)ussema + sizeof(ussemastate_t));

		ussema->ntid = old->nfilled;
		ussema->nprepost = 0; 
		ussema->nfilled = old->nfilled;
		ussema->nthread = old->nfilled;

		for (i = 0; i < old->nfilled; i++) {
			ussema->tidinfo[i].pid = old->pidinfo[i].pid;
			ussema->tidinfo[i].tid = 0;
			ussema->tidinfo[i].count = 0;
		}
		if (ussema->nthread) {
			if (ioctl(fd, UIOCSETSEMASTATE, ussema) < 0) {
				cerror("set ussema state (%s)\n", STRERR);
				free(ussema);
				return (-1);
			}
		}
		free(ussema);

		for (i = 0; i < old->nfilled; i++) {
			int count;
			int cmd;

			if ((count = old->pidinfo[i].count) == 0)
				continue;

			if (count > 0)
				cmd = UIOCAUNBLOCK;
			else  {
				cmd = UIOCABLOCKPID;
				count = -count;
			}
			while (--count >= 0) {
				if (ioctl(fd, cmd, old->pidinfo[i].pid) < 0) {
					cerror("semaphore count (%s)\n", STRERR);
					return (-1);
				}
			}
		}
		return (1);
	}
	return (0);
}

int
ckpt_rxlate_procinfo(ckpt_rev_t revision, ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_pi_t *pi)
{
	if (revision < CKPT_REVISION_CURRENT) {
		ckpt_pi_old_t opi;
		ckpt_psi_old_t *opsi;
		ckpt_psi_t *psi;

		if (read(tp->sfd, &opi, sizeof (ckpt_pi_old_t)) == -1) {
			cerror("Failed to read procinfo header (%s)\n", STRERR);
			return (-1);
		}
		pi->cp_magic = opi.cp_magic;
		bcopy(opi.cp_comm, pi->cp_comm, OLD_PSCOMSIZ);
		bcopy(opi.cp_psargs, pi->cp_psargs, OLD_PSARGSZ);
		bcopy(opi.cp_cdir, pi->cp_cdir, MAXPATHLEN);
		bcopy(opi.cp_rdir, pi->cp_rdir, MAXPATHLEN);
		pi->cp_gbread = opi.cp_gbread;
		pi->cp_bread = opi.cp_bread;
		pi->cp_gbwrit = opi.cp_gbwrit;
		pi->cp_bwrit = opi.cp_bwrit;
		pi->cp_syscr = opi.cp_syscr;
		pi->cp_syscw = opi.cp_syscw;
		pi->cp_stat = opi.cp_stat;
		pi->cp_cpu = opi.cp_cpu;
		pi->cp_nice = opi.cp_nice;
		pi->cp_time = opi.cp_time;
		pi->cp_pid = opi.cp_pid;
		pi->cp_ppid = opi.cp_ppid;
		pi->cp_pgrp = opi.cp_pgrp;
		pi->cp_sid = opi.cp_sid;
		pi->cp_mustrun = opi.cp_mustrun;
		pi->cp_tslice = opi.cp_tslice;
		pi->cp_ttydev = opi.cp_ttydev;
		pi->cp_hwpctl = opi.cp_hwpctl;
		pi->cp_hwpctlaux = opi.cp_hwpctlaux;
		pi->cp_hwpaux = opi.cp_hwpaux;
		pi->cp_spilen = sizeof(acct_spi_t);
		bcopy(&opi.cp_spi, pi->cp_spi, pi->cp_spilen);
		pi->cp_prid = opi.cp_prid;
		pi->cp_birth_time = opi.cp_birth_time;
		pi->cp_ckpt_time = opi.cp_ckpt_time;
		pi->cp_nofiles = opi.cp_nofiles;
		pi->cp_maxfd = opi.cp_maxfd;
		pi->cp_npfds = opi.cp_npfds;
		pi->cp_flags = opi.cp_flags;
		pi->cp_schedmode = opi.cp_schedmode;
		bcopy(opi.cp_clname, pi->cp_clname, sizeof(pi->cp_clname));

		opsi = &opi.cp_psi;
		psi = &pi->cp_psi;

		psi->ps_execid = opsi->ps_execid;
		psi->ps_acflag = opsi->ps_acflag;

		psi->ps_state = opsi->ps_state;
		psi->ps_ckptflag = opsi->ps_ckptflag;
		psi->ps_lock = opsi->ps_lock;
		psi->ps_brkbase = opsi->ps_brkbase;
		psi->ps_brksize = opsi->ps_brksize;
		psi->ps_stackbase = opsi->ps_stackbase;
		psi->ps_stacksize = opsi->ps_stacksize;
		psi->ps_sigtramp = opsi->ps_sigtramp;
		psi->ps_xstat = opsi->ps_xstat;
		psi->ps_execid = opsi->ps_execid;

		pi->cp_cred.pr_euid = opsi->ps_cred.cr_uid;
		pi->cp_cred.pr_egid = opsi->ps_cred.cr_gid;
		pi->cp_cred.pr_ruid = opsi->ps_cred.cr_ruid;
		pi->cp_cred.pr_rgid = opsi->ps_cred.cr_rgid;
		pi->cp_cred.pr_suid = opsi->ps_cred.cr_suid;
		pi->cp_cred.pr_sgid = opsi->ps_cred.cr_sgid;
		pi->cp_cred.pr_ngroups = opsi->ps_cred.cr_ngroups;

		psi->ps_mem = opsi->ps_mem;
		psi->ps_ioch = opsi->ps_ioch;
		psi->ps_ru = opsi->ps_ru;
		psi->ps_cru = opsi->ps_cru;

		/*
		 * XXX how do we convert old timers to new ???
	 	 *
		bcopy(opsi->ps_timers.kp_timertab, psi->ps_timers.kp_timertab, 
			sizeof(psi->ps_timers.kp_timertab));
		psi->ps_timers.kp_whichtimer = opsi->ps_timers.kp_whichtimer;

		bcopy(opsi->ps_ctimertab, psi->ps_ctimertab, sizeof(psi->ps_ctimertab));
		 *
		 */

		bcopy(opsi->ps_itimer, pi->cp_itimer, sizeof(pi->cp_itimer));
		bcopy(opsi->ps_rlimit, psi->ps_rlimit, sizeof(opsi->ps_rlimit));
		/*
		 * 6.4 app better not be spawning pthreads
		 */
		psi->ps_rlimit[RLIMIT_PTHREAD].rlim_cur = 0;
		psi->ps_rlimit[RLIMIT_PTHREAD].rlim_max = 0;

		psi->ps_cmask = opsi->ps_cmask;
		psi->ps_shaddr = opsi->ps_shaddr;
		psi->ps_shmask = opsi->ps_shmask;
		psi->ps_shrefcnt = opsi->ps_shrefcnt;
		psi->ps_shflags = opsi->ps_shflags;
		psi->ps_unblkonexecpid = opsi->ps_unblkonexecpid;
		tp->blkcnt = opsi->ps_blkcnt;
		psi->ps_exitsig = opsi->ps_exitsig;
		psi->ps_flags = (ulong_t)opsi->ps_flags;

		if (opsi->ps_flag2 & OLD_SCEXIT)
			psi->ps_flags |= CKPT_TERMCHILD;
		if (opsi->ps_flag2 & OLD_SCOREPID)
			psi->ps_flags |= CKPT_COREPID;

		psi->ps_mldlink = 0;

	} else {
		if (read(tp->sfd, pi, sizeof (ckpt_pi_t)) == -1) {
			cerror("Failed to read procinfo header (%s)\n", STRERR);
			return (-1);
		}
	}
        if (pi->cp_magic != magic) {
                setoserror(EINVAL);
                cerror("mismatched magic %8.8s (vs. %8.8s)\n",
                        &pi->cp_magic, &magic);
                return (-1);
        }
	return (0);
}
