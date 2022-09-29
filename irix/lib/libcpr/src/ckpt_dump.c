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
#ident "$Revision: 1.134 $"

#include <fcntl.h>
#include <pwd.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <bstring.h>
#include <sys.s>
#include <limits.h>
#include <errno.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/arsess.h>
#include <sys/cred.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ckpt_procfs.h>
#include <sys/ckpt_sys.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <signal.h>
#include <libgen.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"


#define	DUMP_SBUF(sbuf)	cdebug("sbuf:mode=%x,fd=%d,fflags=%x,offset=%lld,rdev=0x%x\n",\
	(sbuf)->ckpt_mode,(sbuf)->ckpt_fd,(sbuf)->ckpt_fflags,(sbuf)->ckpt_offset,\
	(sbuf)->ckpt_rdev)

static int ckpt_dump_one_proc(ckpt_obj_t *);
static int ckpt_get_cwd_and_root(ckpt_obj_t *, ckpt_pi_t *);
static int ckpt_is_pipe(ckpt_statbuf_t *);

int
ckpt_create_statefile(ch_t *ch)
{
	ckpt_obj_t *co;
	int rc = 0, i;
	char *pathcopy, *dir;
	
	if((pathcopy = strdup (ch->ch_path)) == NULL) {
		cerror("Failed to strdup: %s (%s)\n", ch->ch_path, STRERR);
		return -1;
	}
	
	/* 
	 * make sure the user has the permission to create ckpt dir here.
	 * access uses real uid and gid do the checking.
	 */ 
	dir = ckpt_path_to_dirname(pathcopy);
	if(access(dir, X_OK|R_OK|W_OK)) {
		cerror("Access check failed: %s (%s)\n", dir, STRERR);
		return -1;
	}
	
	if (rc = mkdir((const char *)ch->ch_path, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
		if (oserror() == EEXIST) {
			printf("File %s exists. Use -f to overwrite\n", ch->ch_path);
		} else {
			cerror("Failed to mkdir for %s (%s)\n", ch->ch_path, STRERR);
		}
		return (rc);
	}
	/* make sure this dir is owned by root. If the path is a NSF mounted
	 * location with permission 0777, then, we still can mkdir above,
	 * and the dir will be owned by nobody in that case. We can't restart 
	 * it for security reasons. 
	 */
	if(ckpt_check_directory(ch->ch_path)) {
		cerror("directory: %s is not owned by root. This directory "
		"may be remotely mounted to this machine\n", ch->ch_path);
		setoserror(ECKPT);
		rmdir(ch->ch_path);
		return -1;
	}

	ch->ch_flags |= CKPT_PCI_CREATE;

	if (ckpt_write_pci(ch) < 0)
		return (-1);


	/*
	 * Save each process in the order of our parent-child-sibling tree so that
	 * we are guaranteed that if a child process is saved, its parent must have
	 * been saved. Important implication for error recovery.
	 */
	for (i = 0; i < ch->ch_ntrees; i++) {
		co = ch->ch_ptrees[i];
		IFDEB1(printf("\n"));
		IFDEB1(cdebug("dumping pid %d\n", co->co_pid));

		if ((rc = ckpt_dump_one_proc(co)) < 0) {
			ch->ch_flags |= CKPT_PCI_ABORT;
			if (oserror() == ENOSPC)
				return (rc);
			break;
		}
	}
	/*
	 * Save the per-cpr shared index and state even when error
	 */
	if (ckpt_write_share_properties(ch) < 0)
		return (-1);

	fsync(ch->ch_sfd);
	fsync(ch->ch_ifd);

	close(ch->ch_sfd);
	close(ch->ch_ifd);

	return (rc);
}

static int
ckpt_dump_one_proc(ckpt_obj_t *co)
{
	char path[CPATHLEN];
	ckpt_obj_t *cco;
	ckpt_ppi_t ppi;
	int rc;

	/*
	 * Create index, state and memory image file for this proc
	 */
	sprintf(path, "%s/%d.%s", co->co_ch->ch_path, co->co_pid, STATEF_INDEX);
	if ((co->co_ifd = open(path, O_CREAT|O_RDWR, S_IRUSR)) == -1) {
		cerror("failed to open %s (%s)\n", path, STRERR);
		return (-1);
	}
	sprintf(path, "%s/%d.%s", co->co_ch->ch_path, co->co_pid, STATEF_STATE);
	if ((co->co_sfd = open(path, O_CREAT|O_RDWR, S_IRUSR|S_ISUID)) == -1) {
		cerror("failed to open %s (%s)\n", path, STRERR);
		return (-1);
	}
	sprintf(path, "%s/%d.%s", co->co_ch->ch_path, co->co_pid, STATEF_IMAGE);
	if ((co->co_mfd = open(path, O_CREAT|O_RDWR, S_IRUSR)) == -1) {
		cerror("failed to open %s (%s)\n", path, STRERR);
		return (-1);
	}
	/*
	 * leave statefile owned by root to avoid uid subversion!
	 */
	if (fchown(co->co_ifd, getuid(), getgid()) ||
	    fchown(co->co_mfd, getuid(), getgid())) {
		cerror("Failed to change owner (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Mark the ppi indexfile before start saving the process
	 */
	ppi.ppi_magic = CKPT_MAGIC_INDEX;
	ppi.ppi_pid = co->co_pid;
	ppi.ppi_prop_count = 0;
	if (write(co->co_ifd, &ppi, sizeof(ckpt_ppi_t)) < 0) {
		cerror("Failed to write indexfile header for pid %d (%s)\n",
			co->co_pid, STRERR);
		return (-1);
	}
	rc = ckpt_write_proc_properties(co);

	fsync(co->co_ifd);
	fsync(co->co_sfd);
	fsync(co->co_mfd);

	close(co->co_ifd);
	close(co->co_sfd);
	close(co->co_mfd);
	if (rc < 0)
		return (rc);

	/*
	 * Write out its children
	 */
	for (cco = co->co_children; cco; cco = cco->co_sibling) {
		IFDEB1(cdebug("dumping pid %d\n", cco->co_pid));
		/*
		 * If error, set CKPT_PCI_ABORT but continue the rest of 
		 * children processe dumping to be consistent.
		 */
		if ((rc = ckpt_dump_one_proc(cco)) < 0) {
			cco->co_ch->ch_flags |= CKPT_PCI_ABORT;
			if (oserror() == ENOSPC)
				return (-1);
		}
	}
	return ((co->co_ch->ch_flags & CKPT_PCI_ABORT) ? -1: 0);
}
/*
 * Dump process context info to statefile
 *
 * dump one context record for each thread
 */
static int
ckpt_write_thread_context(ckpt_obj_t *co, ckpt_prop_t *pp, ckpt_thread_t *thread)
{
	ckpt_context_t context;
	ssize_t rc;
#ifdef DEBUG
	greg_t *gregp = thread->ct_ctxt.cc_ctxt.uc_mcontext.gregs;
#endif
	IFDEB1(cdebug("pid %d tid %d flags %x epc=%lx sp=%lx sp size=%d sr=%x v0=%ld, a0=%ld, a1=%lx,"
		      "a2=%ld a3=%ld ra=%lx\n",
			co->co_pid,
			thread->ct_id,
			thread->ct_ctxt.cc_ctxt.uc_flags,
			gregp[CXT_EPC],
			thread->ct_ctxt.cc_ctxt.uc_stack.ss_sp,
			thread->ct_ctxt.cc_ctxt.uc_stack.ss_size,
			gregp[CXT_SR],
			gregp[CXT_V0],
			gregp[CXT_A0],
			gregp[CXT_A1],
			gregp[CXT_A2],
			gregp[CXT_A3],
			gregp[CXT_RA]));

	context.cc_magic = CKPT_MAGIC_CTXINFO;
	context.cc_tid = thread->ct_id;
	context.cc_ctxt = thread->ct_ctxt;
	context.cc_flags = 0;

	if (thread->ct_flags & CKPT_THREAD_REISSUE)
		context.cc_flags |= CKPT_CTXT_REISSUE;

	CWRITE(co->co_sfd, &context, sizeof(context), 1, pp, rc);
	if (rc < 0) {
		cerror("ckpt_write_context failed (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_write_context(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_thread_t *thread;

	assert((co->co_flags & CKPT_CO_ZOMBIE)||(co->co_thread));

	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (ckpt_write_thread_context(co, pp, thread) < 0)
			return (-1);
	}
	return (0);
}
/*
 * Dump thread instance info to the statefile
 */
static int
ckpt_write_thread_instance(ckpt_obj_t *co, ckpt_prop_t *pp, ckpt_thread_t *thread)
{
	ckpt_thread_inst_t instance;
	ssize_t rc;
	ckpt_uti_t *uti = &instance.cti_uti;
	/*
	 * Re-fetch thread info now that we know all procs are stopped
	 */
	if (ckpt_thread_ioctl(co, thread->ct_id, 0, PIOCCKPTUTINFO, uti) < 0)
		return (-1);

#ifdef DEBUG_HARDTEST
	if (uti->uti_blkcnt || uti->uti_isblocked) {
		cdebug("DUMP:pid %d blockcnt %d isblocked %d\n", 
			co->co_pid, uti->uti_blkcnt, uti->uti_isblocked);
	}
#endif
	instance.cti_magic = CKPT_MAGIC_THREAD;
	instance.cti_tid = thread->ct_id;

	CWRITE(co->co_sfd, &instance, sizeof(instance), 1, pp, rc);
	if (rc < 0) {
		cerror("ckpt_write_instance failed (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}
int
ckpt_write_thread(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_thread_t *thread;

	assert((co->co_flags & CKPT_CO_ZOMBIE)||(co->co_thread));

	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (ckpt_write_thread_instance(co, pp, thread) < 0)
			return (-1);
	}
	return (0);
}

int
ckpt_write_childinfo(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_obj_t *cco;
	ckpt_ci_t cinfo;
	pid_t *cpids;
	int i;
	ssize_t rc;

	cinfo.ci_magic = pp->prop_magic;
	cinfo.ci_nchild = co->co_nchild;

	CWRITE(co->co_sfd, &cinfo, sizeof(cinfo), 1, pp, rc);
	if (rc < 0) {
		cerror("ckpt_write_childinfo failed (%s)\n", STRERR);
		return (-1);
	}

	if (cinfo.ci_nchild == 0)
		return (0);

	if ((cpids = (pid_t *)malloc(cinfo.ci_nchild*sizeof(pid_t))) == NULL) {
		cerror("ckpt_write_childinfo_header:malloc (%s)\n", STRERR);
		return (-1);
	}
	for (cco = co->co_children, i = 0; cco; cco = cco->co_sibling, i++)
		cpids[i] = cco->co_pid;

	assert(i == cinfo.ci_nchild);

	CWRITE(co->co_sfd, cpids, cinfo.ci_nchild*sizeof(pid_t), 0, pp, rc);
	if (rc < 0) {
		cerror("ckpt_write_childinfo failed (%s)\n", STRERR);
		free(cpids);
		return (-1);
	}
	free(cpids);
	return (0);
}

int
ckpt_write_procinfo(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_pi_t *pi;
	prusage_t info;
	ckpt_statbuf_t sbuf;
	int nfds;		/* number of fds */
	int npfds;		/* number of restart prototype fds */
	int maxfd;
	int slice = 10; /* tmp slice for getting the current cpu slice */
	int nocaffine;
	ssize_t rc;
	struct pracinfo acinfo;
	char *fdtype;
	struct sched_param schedparam;

	pi = co->co_pinfo;
	/*
	 * Re-fetch ps info.  May have changed since proc was opened.
	 * Note, processes may have exited and become zombies since opening.
	 */
	if (ioctl(co->co_prfd, PIOCPSINFO, co->co_psinfo) < 0) {
		cerror("Process %d PIOCPSINFO (%s)\n", co->co_pid, STRERR);
		return (-1);
	}
	if (ioctl(co->co_prfd, PIOCCRED, &co->co_pinfo->cp_cred) < 0) {
		cerror("PIOCCRED (%s)\n", STRERR);
		return (-1);
	}
	if (ioctl(co->co_prfd, PIOCCKPTPSINFO, &co->co_pinfo->cp_psi) < 0) {
		cerror("PIOCCKPTPSINFO (%s)\n", STRERR);
		return (-1);
	}
	if (ioctl(co->co_prfd, PIOCCKPTUSAGE, &info) == -1) {
		cerror("PIOCUSAGE (%s)\n", STRERR);
		return (-1);
	}
	pi->cp_magic = pp->prop_magic;

	strcpy(pi->cp_comm, co->co_psinfo->pr_fname);
	strcpy(pi->cp_psargs, co->co_psinfo->pr_psargs);

	pi->cp_stat = co->co_psinfo->pr_state;

	if (pi->cp_stat == SZOMB)
		/*
		 * Set zombie flag in checkpoint object to make it easy
		 * to tell.
		 * Might *not* be CKPT_CO_ZOPEN...process may have called
		 * exit after we opened prfd.
		 */
		co->co_flags |= CKPT_CO_ZOMBIE;

	if (co->co_flags & CKPT_CO_ZOPEN) {
		/*
		 * we should have caught this earlier....but
		 * just in case
		 */
		if ((co->co_flags & CKPT_CO_ZOMBIE) == 0) {
			cerror("zombie open of non-zombie process (pid %d state %d)\n",
				co->co_pid, pi->cp_stat);
			setoserror(ECKPT);
			return (-1);
		}
	}
	if (co->co_flags & CKPT_CO_ZOMBIE)
		pi->cp_spilen = 0;
	else {
		if (ioctl(co->co_prfd, PIOCACINFO, &acinfo) < 0) {
			cerror("PIOCACINFO (%s)\n", STRERR);
			return (-1);
		}
		if (arsop(ARSOP_GETSPILEN, acinfo.pr_ash, &pi->cp_spilen,
			  sizeof(pi->cp_spilen)) < 0)
		{
			cerror("unable to obtain length of service provider info\n");
			return (-1);
		}
		if (arsop(ARSOP_GETSPI, acinfo.pr_ash, pi->cp_spi, pi->cp_spilen) < 0)
		{
			cerror("unable to record service provider information\n");
			return (-1);
		}
	}
	pi->cp_prid = acinfo.pr_prid;

	pi->cp_cpu = co->co_psinfo->pr_cpu;
	strncpy(pi->cp_clname, co->co_psinfo->pr_clname, 3);
	pi->cp_nice = co->co_psinfo->pr_nice;
	pi->cp_pid = co->co_pid;
	pi->cp_ppid = co->co_psinfo->pr_ppid;
	pi->cp_pgrp = co->co_psinfo->pr_pgrp;
	pi->cp_sid = co->co_psinfo->pr_sid;
	pi->cp_ttydev = co->co_psinfo->pr_ttydev;
	if ((co->co_pinfo->cp_flags & CKPT_PI_ROOT) && pi->cp_ttydev != PRNODEV)
		co->co_ch->ch_flags |= CKPT_PCI_INTERACTIVE;

	pi->cp_birth_time.tv_sec = co->co_psinfo->pr_start.tv_sec;
	pi->cp_birth_time.tv_nsec = co->co_psinfo->pr_start.tv_nsec;

	pi->cp_ckpt_time.tv_sec = info.pu_tstamp.tv_sec;
	pi->cp_ckpt_time.tv_nsec = info.pu_tstamp.tv_nsec;

	/*
	 * accounting info (some are included in cp_psi)
	 * excluded items:
	 *	u_mem (p_rss): restarted process has this info 
	 */
	pi->cp_gbread = info.pu_gbread;
	pi->cp_bread = info.pu_bread;
	pi->cp_gbwrit = info.pu_gbwrit;
	pi->cp_bwrit = info.pu_bwrit;
	pi->cp_syscr = info.pu_syscr;
	pi->cp_syscw = info.pu_syscw;

	/* scheduler info ... these are allowed to fail */
	pi->cp_schedpolicy = sched_getscheduler(co->co_pid);
	sched_getparam(co->co_pid, &schedparam);
	pi->cp_schedpriority = schedparam.sched_priority;

	/* save cpu binding info */
	pi->cp_mustrun = (cpuid_t)sysmp(MP_GETMUSTRUN_PID, co->co_pid);
	IFDEB1(cdebug("process %d must run on processor %d\n",
		co->co_pid, pi->cp_mustrun));
	/*
	 * Work out scheduler tate
	 */
	/* save cpu time slice and restore it back to the process */
	if (co->co_flags & CKPT_CO_ZOMBIE)
		pi->cp_tslice = 0;

	else if ((slice = (int)schedctl(SLICE, co->co_pid, slice)) == -1) {
		cnotice("cannot read the time slice for process %d (%s)\n",
			co->co_pid, STRERR);
		pi->cp_tslice = 0;
	} else {
		pi->cp_tslice = (unsigned short)slice;
		(void) schedctl(SLICE, co->co_pid, slice);
	}
	IFDEB1(cdebug("process time slice %d\n", slice));

	if (co->co_flags & CKPT_CO_ZOMBIE)
		nocaffine = 0;

	else if ((nocaffine = (int)schedctl(AFFINITY_STATUS, co->co_pid)) < 0) {
		cnotice("cannot read the cache affinity for process %d (%s)\n",
			co->co_pid, STRERR);
	}
	if (nocaffine)
		pi->cp_flags |= CKPT_PI_NOCAFFINE;

	if (IS_SPROC(pi)) {
		pi->cp_schedmode = acinfo.pr_sched;
	}
	/*
	 * Get root, current dirs and count open fds
	 */
	if (ckpt_get_cwd_and_root(co, pi) < 0)
		return(-1);

	sbuf.ckpt_fd = 0;
	nfds = 0;
	npfds = 0;
	maxfd = 0;
	/*
	 * Only fetch fds for non-ZOMBIES 
	 */
	while (pi->cp_stat != SZOMB) {
	        if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
	                /* Hack to get the last pid */
			if (oserror() != ENOENT) {
				cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
				return(-1);
			}
	                break;
	        }
		if (fdtype = ckpt_unsupported_fdtype(&sbuf)) {
			cerror("Uncheckpointable fd, pid %d fd %d type %s\n",
					co->co_pid, sbuf.ckpt_fd, fdtype);
			setoserror(ECKPT);
			return (-1);
		}
		maxfd = sbuf.ckpt_fd;
	        sbuf.ckpt_fd++;
	        nfds++;
		if (ckpt_is_pipe(&sbuf))
			/*
			 * *if* this pipe has not been seen before,
			 * will allocate 2 prototype fds at restart
			 */
			npfds += 2;
	}
	if (maxfd > 10000) {
		cerror("Preposterous maxfd %d or pid %d\n", maxfd, co->co_pid);
		setoserror(ECKPT);
		return (-1);
	}
	pi->cp_nofiles = nfds;
	pi->cp_maxfd = maxfd;
	pi->cp_npfds = npfds;

	IFDEB1(cdebug("number of open fds: %d:maxfd %d\n", nfds, maxfd));
	/*
	 * hwperf stuff
	 */
	if (pi->cp_psi.ps_flags & CKPT_HWPERF) {
		hwperf_getctrlex_arg_t arg;

		pi->cp_hwpaux.hwp_aux_gen = ioctl(co->co_prfd,
						  PIOCGETEVCTRS,
						  &pi->cp_hwpaux.hwp_aux_cntr);
		if (pi->cp_hwpaux.hwp_aux_gen < 0) {
			cerror("Failed to read hwperf counters for proc %d (%s)\n",
				co->co_pid, STRERR);
			return (-1);
		}
		arg.hwp_evctrl = &pi->cp_hwpctl;
		arg.hwp_evctrlaux = &pi->cp_hwpctlaux;

		if (ioctl(co->co_prfd, PIOCGETEVCTRLEX, &arg) < 0) {
			cerror("Failed to read hwperf control for proc %d (%s)\n",
				co->co_pid, STRERR);
			return (-1);
		}
	}
	CWRITE(co->co_sfd, pi, sizeof(ckpt_pi_t), 1, pp, rc);
	if (rc < 0) {
		cerror("failed to write CPR main header (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

static int
ckpt_get_cwd_and_root(ckpt_obj_t *co, ckpt_pi_t *pi)
{
	int dirfd;
	char *path;

	if (co->co_flags & CKPT_CO_ZOMBIE) {
		pi->cp_cdir[0] = 0;
		pi->cp_rdir[0] = 0;
		return (0);
	}
	if ((dirfd = ioctl(co->co_prfd, PIOCCKPTOPENCWD, NULL)) < 0) {
		cerror("PIOCCKPTOPENCWD (%s)\n", STRERR);
		pi->cp_cdir[0] = 0;
		goto saveroot;
	}
	path = ckpt_get_dirname(co, dirfd);
	if (path == NULL)
		return (-1);
	strncpy(pi->cp_cdir, path, MAXPATHLEN);
	close(dirfd);
	free(path);
	IFDEB1(cdebug("cwd=%s\n", pi->cp_cdir));

saveroot:
	/* don't report errors since root dir may be set in the kernel */
	if ((dirfd = ioctl(co->co_prfd, PIOCCKPTOPENRT, NULL)) < 0) {
		IFDEB1(cdebug("PIOCCKPTOPENRT: %s\n", STRERR));
		pi->cp_rdir[0] = 0;
		return (0);
	}
	path = ckpt_get_dirname(co, dirfd);
	if (path == NULL)
		return (-1);
	strncpy(pi->cp_rdir, path, MAXPATHLEN);
	close(dirfd);
	free(path);
	IFDEB1(cdebug("root=%s\n", pi->cp_cdir));
	return (0);
}

static int
ckpt_is_pipe(ckpt_statbuf_t *sbuf)
{
	if (!(S_ISFIFO(sbuf->ckpt_mode)))
		return (0);

	if (strcmp(sbuf->ckpt_fstype, "pipefs") == 0)
		/*
		 * SVR3 pipe
		 */
		return (1);
	if (strcmp(sbuf->ckpt_fstype, "fifofs") == 0)
		/*
		 * SVR4 pipe(2) pipe
		 */
		return (1);
	/*
	 * svr4 named pipe...handled similarly to files
	 */
	return (0);
}

int
ckpt_write_svr3_pipe(	ckpt_obj_t *co,
			ckpt_statbuf_t *sbuf,
			ckpt_p_t *pp,
			int pipefd,
			int *protofd)
{
	ckpt_po_t *po;
	ushort_t fflags;

	fflags = sbuf->ckpt_fflags-FOPEN;

	pp->cp_magic = CKPT_MAGIC_SVR3PIPE;
	pp->cp_fd = sbuf->ckpt_fd;
	pp->cp_fdid = (caddr_t)(long)((fflags & FREAD)? PIPEREAD : PIPEWRITE);
	pp->cp_dev = sbuf->ckpt_dev;
	pp->cp_ino = sbuf->ckpt_ino;
	pp->cp_size = sbuf->ckpt_size;
	pp->cp_flags = 0;

	if (pp->cp_size > 0)
		ckpt_setpipepath(pp, co->co_ch->ch_path, 0);
	else
		pp->cp_path[0] = '\0';

	if ((po = ckpt_pipe_lookup(pp->cp_dev, pp->cp_ino)) == NULL) {
		if ((po = ckpt_pipe_add(co, pp, protofd)) == NULL)
			return (-1);

		pp->cp_flags |= CKPT_PIPE_HANDLED;

	}
	pp->cp_duppid = po->cpo_pid;
	pp->cp_dupfd = po->cpo_inst[(fflags&FREAD)?PIPEREAD:PIPEWRITE].fd;
	pp->cp_protofd[PIPEREAD] = po->cpo_inst[PIPEREAD].fd;
	pp->cp_protofd[PIPEWRITE] = po->cpo_inst[PIPEWRITE].fd;

	if ((fflags&FREAD)&& !(po->cpo_inst[PIPEREAD].flags&CKPT_PIPE_DUMPED)) {
		/*
		 * First look at a read fd to this pipe
		 */
		po->cpo_inst[PIPEREAD].flags |= CKPT_PIPE_DUMPED;

		if (pp->cp_size > 0) {

			if (ckpt_save_svr3fifo_data(pp->cp_path, pipefd) < 0)
				return (-1);
		}
	}
	return (0);
}

int
ckpt_write_svr4_pipe(	ckpt_obj_t *co,
			ckpt_statbuf_t *sbuf,
			ckpt_p_t *pp,
			int pipefd,
			int *protofd)
{
	ckpt_po_t *po;
	int instance = -1;

	pp->cp_magic = CKPT_MAGIC_SVR4PIPE;
	pp->cp_fd = sbuf->ckpt_fd;
	pp->cp_dev = sbuf->ckpt_dev;
	pp->cp_ino = sbuf->ckpt_ino;
	pp->cp_size = sbuf->ckpt_size;
	pp->cp_flags = 0;
	pp->cp_path[0] = '\0';
	/*
	 * Use kernel file struct addr to differentiate the 2 fd's 
	 * allocated in a pipe(2) syscall
	 */
	if ((pp->cp_fdid = (caddr_t)syssgi(SGI_CKPT_SYS, CKPT_FPADDR, pipefd))
		== (caddr_t)(-1L)) {
		cerror("ckpt_write_svr4_pipe:CKPT_FPADDR (%s)\n", STRERR);
		return (-1);
	}
	if ((po = ckpt_pipe_lookup(pp->cp_dev, pp->cp_ino)) == NULL) {
		if ((po = ckpt_pipe_add(co, pp, protofd)) == NULL)
			return (-1);

		pp->cp_flags |= CKPT_PIPE_HANDLED;
	}
	pp->cp_duppid = po->cpo_pid;
	pp->cp_protofd[0] = po->cpo_inst[0].fd;
	pp->cp_protofd[1] = po->cpo_inst[1].fd;

	if (po->cpo_inst[0].id == pp->cp_fdid)
		instance = 0;
	else if (po->cpo_inst[1].id == pp->cp_fdid)
		instance = 1;

	if (instance >= 0) {
		/*
		 * pipe instance already seen, dealt with
		 */
		pp->cp_dupfd = po->cpo_inst[instance].fd;

		if (pp->cp_size > 0)
			ckpt_setpipepath(pp, co->co_ch->ch_path, instance);
		return (0);
	}
	if (!(po->cpo_inst[0].flags & CKPT_PIPE_DUMPED))
		instance = 0;
	else {
		assert(!(po->cpo_inst[1].flags & CKPT_PIPE_DUMPED));
		instance = 1;
	}
	po->cpo_inst[instance].flags |= CKPT_PIPE_DUMPED;
	po->cpo_inst[instance].id = pp->cp_fdid;

	pp->cp_dupfd = po->cpo_inst[instance].fd;

	ckpt_setpipepath(pp, co->co_ch->ch_path, instance);

	ckpt_get_stropts(pipefd, &pp->cp_stropts, 0);
	/*
	 * If there is pipe data to save, save the data now
	 */
	if (pp->cp_size > 0) {

		if (ckpt_save_svr4fifo_data(pp->cp_path, pipefd) < 0)
			return (-1);
	}
	return (0);
}

int
ckpt_write_pipe(ckpt_obj_t *co, ckpt_statbuf_t *sbuf, ckpt_p_t *pp, int *protofd)
{
	int dupfd;
	int err;
	/*
	 * Get an fd to the fifo
	 */
	if ((dupfd = ioctl(co->co_prfd, PIOCCKPTDUP, &sbuf->ckpt_fd)) < 0) {
		cerror("PIOCCKPTDUP (%s)\n", STRERR);
		return (-1);
	}
	if ((pp->cp_fdflags = fcntl(dupfd, F_GETFD)) < 0) {
		cerror("F_GETFD (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Figure out what kind of fifo: svr3 pipe, svr4 pipe, named pipe?
	 */
	if (strcmp(sbuf->ckpt_fstype, "pipefs") == 0) {
		/*
		 * SVR3 pipe
		 */
		err = ckpt_write_svr3_pipe(co, sbuf, pp, dupfd, protofd);
		close(dupfd);
		return (err);
	}
	if (strcmp(sbuf->ckpt_fstype, "fifofs") == 0) {
		/*
		 * SVR4 pipe(2) pipe
		 */
		err = ckpt_write_svr4_pipe(co, sbuf, pp, dupfd, protofd);
		close(dupfd);
		return (err);
	}
	setoserror(ECKPT);
	cerror("Unsupported checkpoint object: fstype %s\n", sbuf->ckpt_fstype);
	close(dupfd);
	return(-1);
}

int
ckpt_write_openpipes(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_statbuf_t sbuf;
	ckpt_p_t pipe;
	int nofiles = co->co_pinfo->cp_nofiles;
	int protofd;
	int i;
	ssize_t rc;
	/*
	 * If fd sharing sproc and another member already did this,
	 * don't process any fds
	 *
	 * Assign prototype pipe fds.  Start them immediately following 
	 * procs max fd.
	 */
	if (co->co_flags & (CKPT_CO_SPROCFD_HANDLED|CKPT_CO_ZOMBIE))
		return (0);

	protofd = co->co_pinfo->cp_maxfd + 1;

	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {

		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
			break;
		}
		assert(ckpt_unsupported_fdtype(&sbuf) == NULL);

		if (!ckpt_is_pipe(&sbuf))
			continue;

		bzero(&pipe, sizeof(ckpt_p_t));

		if (ckpt_write_pipe(co, &sbuf, &pipe, &protofd) < 0) {
			return (-1);
		}

		/* write out pipe header */
		CWRITE(	co->co_sfd, &pipe, sizeof(ckpt_p_t), 1, pp, rc);
		if (rc < 0) {
			cerror("Failed to write openpipes (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_write_openspecs(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_statbuf_t sbuf;
	ckpt_f_t f;
	pid_t handled;
	int i, nofiles = co->co_pinfo->cp_nofiles;
	int dupfd;
	ssize_t rc;
	/*
	 * If fd sharing sproc and another member already did this,
	 * don't process any fds
	 */
	if (co->co_flags & (CKPT_CO_SPROCFD_HANDLED|CKPT_CO_ZOMBIE))
		return (0);

	if (ckpt_pfd_init(co, nofiles) < 0)
		return (-1);

	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {

		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
		        cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
		        break;
		}
                assert(ckpt_unsupported_fdtype(&sbuf) == NULL);
		/*
		 * only handle special devices here
		 */
		if (!S_ISCHR(sbuf.ckpt_mode) && !S_ISBLK(sbuf.ckpt_mode))
		        continue;

		handled = ckpt_file_is_handled(	co,
						sbuf.ckpt_rdev,
						0,
						sbuf.ckpt_fd,
						sbuf.ckpt_fid);
		if (handled < 0)
			return (-1);

		/*
		 * If another proc handling file, skip for now.  We'll get it
		 * next pass for open, dup or inherit
		 */
		if (handled)
			continue;

		IFDEB1(cdebug("specfiles:\n"));
		IFDEB1(DUMP_SBUF(&sbuf));

		if ((dupfd = ioctl(co->co_prfd, PIOCCKPTDUP, &sbuf.ckpt_fd)) < 0) {
			cerror("PIOCCKPTDUP (%s)\n", STRERR);
			return (-1);
		}
		f.cf_magic = CKPT_MAGIC_OPENSPECS; 
		f.cf_flags = 0;
		f.cf_rdev = sbuf.ckpt_rdev;
		f.cf_mode = CKPT_MODE_IGNORE;
		f.cf_fmode = sbuf.ckpt_mode;
		f.cf_fd = sbuf.ckpt_fd;
		f.cf_fflags = sbuf.ckpt_fflags;
		f.cf_offset = sbuf.ckpt_offset;
		f.cf_length = 0;
		f.cf_flp = NULL;
		f.cf_auxptr = NULL;
		f.cf_auxsize = 0;

		ckpt_setpaths(co, &f, NULL, sbuf.ckpt_rdev, sbuf.ckpt_ino,
					sbuf.ckpt_mode, co->co_pid, 0);
		if (ckpt_add_unlinked(co, f.cf_path) < 0)
			return (-1);

		if ((f.cf_fdflags = fcntl(dupfd, F_GETFD)) < 0) {
			cerror("F_GETFD (%s)\n", STRERR);
			return (-1);
		}
		if (ckpt_fdattr_special
			(co, &f, sbuf.ckpt_dev, sbuf.ckpt_ino, dupfd) < 0) {
			close(dupfd);
			return (-1);
		}
		IFDEB1(cdebug("specfs fmode=%x fd=%d dev=%x fflags=%x\n",
			f.cf_fmode, f.cf_fd, f.cf_rdev, f.cf_fflags));
		
		ckpt_warn_unsupported(co, &f, dupfd, NULL);

		/*
		 * Get streams options, if applicable
		 */
		ckpt_get_stropts(dupfd, &f.cf_stropts, 0);

		/*
		 * write out the specfile header
		 * (assume there is no file locks on the spec file)
		 */
		CWRITE(co->co_sfd, &f, sizeof(ckpt_f_t), 1, pp, rc);
		if (rc < 0) {
			cerror("failed to write per file header \n", STRERR);
			close(dupfd);
			return (-1);
		}
		if (f.cf_auxptr) {
			CWRITE(co->co_sfd, f.cf_auxptr, f.cf_auxsize, 0, pp, rc);
			if (rc < 0) {
				cerror("failed to write device aux info\n", STRERR);
				close(dupfd);
				return (-1);
			}
			free(f.cf_auxptr);
			f.cf_auxptr = NULL;
			f.cf_auxsize = 0;
		}
		ckpt_pfd_add(co, &f, sbuf.ckpt_dev, sbuf.ckpt_ino);

		close(dupfd);
	}
	return (0);
}

int
ckpt_write_openfiles(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	pid_t handled;
	ckpt_statbuf_t sbuf;
	ckpt_open_t obuf;
	int i, dupfd, unlinked;
	char *path;
	ckpt_f_t f;
	int nofiles = co->co_pinfo->cp_nofiles;
	ssize_t rc;
	/*
	 * If fd sharing sproc and another member already did this,
	 * don't process any fds
	 */
	if (co->co_flags & (CKPT_CO_SPROCFD_HANDLED|CKPT_CO_ZOMBIE))
		return (0);

	if (ckpt_pfd_init(co, nofiles) < 0)
		return (-1);

	/*
	 * loop on all open files 
	 */
	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {
		ckpt_fl_t *curp;

		bzero(&f, sizeof(ckpt_f_t));

		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
			break;
		}
                assert(ckpt_unsupported_fdtype(&sbuf) == NULL);

		/*
		 * skip pipes and specfiles since they handled seperately
		 */
		if (ckpt_is_pipe(&sbuf) ||
		    S_ISCHR(sbuf.ckpt_mode) ||
		    S_ISBLK(sbuf.ckpt_mode))
			continue;

		if ((handled = ckpt_file_is_handled(co, sbuf.ckpt_dev,
		    sbuf.ckpt_ino, sbuf.ckpt_fd, sbuf.ckpt_fid))) {
			if (handled < 0)
				return (-1);
			continue;
		}

		IFDEB1(cdebug("regular openfiles:\n"));
		IFDEB1(DUMP_SBUF(&sbuf));

		obuf.ckpt_fd = sbuf.ckpt_fd;
		obuf.ckpt_oflag = O_RDONLY | O_NDELAY;
		obuf.ckpt_mode_req = S_IRUSR;
		if ((dupfd = ioctl(co->co_prfd, PIOCCKPTOPEN, &obuf)) < 0) {
			cerror("Failed to PIOCCKPTOPEN (%s)\n", STRERR);
			return -1;
		}
		switch (unlinked = ckpt_pathname(co, dupfd, &path)) {
		case 0:
			IFDEB1(cdebug("dupfd=%d, path=%s\n", dupfd, path));
			break;
		case 1:
			f.cf_flags |= CKPT_FLAGS_UNLINKED;
			IFDEB1(cdebug("fd=%d (unlinked file) dir %s\n",
				sbuf.ckpt_fd, path));
			break;
		default:
			/*
			 * non-checkpointable object
			 */
			setoserror(ECKPT);
			cerror("Non-checkpointable object: pid %d: fd %d\n",
				co->co_pid, sbuf.ckpt_fd);
			return -1;
		}

		/* fill in per file info */
		f.cf_magic = CKPT_MAGIC_OPENFILES;

		ckpt_get_psemavalue(dupfd, &f);

		f.cf_mode = ckpt_attr_get_mode(co->co_ch, path, sbuf.ckpt_dev,
			sbuf.ckpt_ino, sbuf.ckpt_mode, unlinked,
			f.cf_flags & CKPT_FLAGS_PSEMA);

		ckpt_setpaths(co, &f, path, sbuf.ckpt_dev, sbuf.ckpt_ino,
					sbuf.ckpt_mode, co->co_pid, unlinked);

		IFDEB1(cdebug("saving openfile %s\n", path));
		free(path);

		if (unlinked) {
			if (ckpt_add_unlinked(co, f.cf_path) < 0)
				return (-1);
		}
		f.cf_fmode = sbuf.ckpt_mode;
		f.cf_fd = sbuf.ckpt_fd;
		f.cf_fflags = sbuf.ckpt_fflags;
		f.cf_offset = sbuf.ckpt_offset;
		f.cf_length = (long)sbuf.ckpt_size;
		f.cf_rdev = 0;
		f.cf_atime = sbuf.ckpt_atime;
		f.cf_mtime = sbuf.ckpt_mtime;
		f.cf_flp = NULL;

		if ((f.cf_fdflags = fcntl(dupfd, F_GETFD)) < 0) {
			cerror("F_GETFD (%s)\n", STRERR);
			return (-1);
		}
		IFDEB1(cdebug("with: fd=%d, dupfd=%d flag=0x%x\n",
			f.cf_fd, dupfd, f.cf_flags));

		ckpt_get_stropts(dupfd, &f.cf_stropts, 0);

		ckpt_get_flocks(&f, dupfd, co->co_pid);

		/* write out the per file header and locks */
		CWRITE(co->co_sfd, &f, sizeof(ckpt_f_t), 1, pp, rc);
		if (rc == -1) {
			cerror("failed to write per file header\n", STRERR);
			return (-1);
		}
		curp = f.cf_flp;
		while (curp) {
			CWRITE(co->co_sfd, curp, sizeof(ckpt_fl_t), 0, pp, rc);
			if (rc == -1) {
				cerror("Failed to save the file locks\n", STRERR);
				break;
			}
			curp = curp->fl_next;
		}
		/*
		 * Must save the propindex once we save the ckpt_f_t header.
		 * Otherwise we cannot unwind in case of errors.
		 *
		 * Due to the flocks may create variable property header in
		 * the statefile, create individual properties for each file
		 */
		if (ckpt_write_propindex(co->co_ifd, pp, &co->co_nprops, NULL) ||
		    rc == -1)
			return (-1);

		if (ckpt_dump_one_ofile(&f, dupfd)) {
			cerror("Failed to save openfile %s\n", f.cf_path);
			close(dupfd);
			return (-1);
		}
		ckpt_pfd_add(co, &f, sbuf.ckpt_dev, sbuf.ckpt_ino);

		close(dupfd);
	}
	return (0);
}

int
ckpt_count_openfiles(ckpt_obj_t *co)
{
	ckpt_statbuf_t sbuf;
	int nofiles;
	char *fdtype;

	nofiles = 0;
	sbuf.ckpt_fd = 0;
	for (;;) {
		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			if (oserror() != ENOENT) {
				cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
				return(-1);
			}
	                if (fdtype = ckpt_unsupported_fdtype(&sbuf)) {
	                        cerror("Uncheckpointable fd, pid %d fd %d type %s\n",
	                                        co->co_pid, sbuf.ckpt_fd, fdtype);
	                        setoserror(ECKPT);
	                        return (-1);
	                }
			break;
		}
		sbuf.ckpt_fd++;
		nofiles++;
	}
	co->co_nofiles = nofiles;
	return (0);
}

int
ckpt_display_openfiles(ckpt_obj_t *co)
{
	ckpt_statbuf_t sbuf;
	ckpt_open_t obuf;
	int i, dupfd, unlinked, mode;
	char *path, buf[CPATHLEN];
	int nofiles = co->co_nofiles;
	char *fdtype;

	assert(pipe_rc);

	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {

		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
			break;
		}
                if (fdtype = ckpt_unsupported_fdtype(&sbuf)) {
                        cerror("Uncheckpointable fd, pid %d fd %d type %s\n",
                                        co->co_pid, sbuf.ckpt_fd, fdtype);
                        setoserror(ECKPT);
                        return (-1);
                }
		/*
		 * skip pipes and specfiles since they handled seperately
		 */
		if (ckpt_is_pipe(&sbuf) ||
		    S_ISCHR(sbuf.ckpt_mode) ||
		    S_ISBLK(sbuf.ckpt_mode))
			continue;

		obuf.ckpt_fd = sbuf.ckpt_fd;
		obuf.ckpt_oflag = O_RDONLY | O_NDELAY;
		obuf.ckpt_mode_req = S_IRUSR;
		if ((dupfd = ioctl(co->co_prfd, PIOCCKPTOPEN, &obuf)) < 0) {
			cerror("Failed to PIOCCKPTOPEN (%s)\n", STRERR);
			return -1;
		}
		switch (unlinked = ckpt_pathname(co, dupfd, &path)) {
		case 0:
			IFDEB1(cdebug("dupfd=%d, path=%s\n", dupfd, path));
			break;
		case 1:
			IFDEB1(cdebug("fd=%d (unlinked file) dir %s\n",
				sbuf.ckpt_fd, path));
			break;
		default:
			/*
			 * non-checkpointable object
			 */
			setoserror(ECKPT);
			cerror("Non-checkpointable object: pid %d: fd %d\n",
				co->co_pid, sbuf.ckpt_fd);
			return -1;
		}
		mode = ckpt_attr_get_mode(co->co_ch, path, sbuf.ckpt_dev,
			sbuf.ckpt_ino, sbuf.ckpt_mode, unlinked, 0);

		sprintf(buf, " [PID %d: FD %2d] FILE: \"%s\": %s ",
			co->co_pid, sbuf.ckpt_fd, path, ckpt_action_str(mode));
		IFDEB1(cdebug("detected openfile %s [%s]\n", 
			path, ckpt_action_str(mode)));

		if (write(pipe_rc, buf, sizeof(buf)) < 0) {
			cerror("Failed to pass up openfile info (%s)\n", STRERR);
			return (-1);
		}
		free(path);
		close(dupfd);
	}
	return (0);
}

/*
 * Deal with fds not otherwise already dealt with
 *
 * These include fds referencing files other processes are saving/restoring
 * or fds that have been inherited.
 *
 * At this point, all files should be marked as 'handled'
 */
int
ckpt_write_fds(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_statbuf_t sbuf;
	ckpt_flist_t *flp;
	caddr_t fid;
	char *path;
	ckpt_f_t f;
	int i, dupfd, unlinked;
	int found;
	ssize_t rc;
	int specfile;
	int nofiles = co->co_pinfo->cp_nofiles;

	/*
	 * If fd sharing sproc and another member already did this,
	 * don't process any fds
	 */
	if (co->co_flags & (CKPT_CO_SPROCFD_HANDLED|CKPT_CO_ZOMBIE))
		nofiles = 0;

	if (ckpt_pfd_init(co, nofiles) < 0)
		return (-1);

	f.cf_auxptr = NULL;
	f.cf_auxsize = 0;
	/*
	 * loop on all open files 
	 */
	for (i = 0, sbuf.ckpt_fd = 0; i < nofiles; i++, sbuf.ckpt_fd++) {
		ckpt_fl_t *curp;

		f.cf_magic = CKPT_MAGIC_OTHER_OFDS;
		f.cf_flags = 0;

		if (ioctl(co->co_prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			cerror("PIOCCKPTFSTAT (%s)\n", STRERR);
			break;
		}
                assert(ckpt_unsupported_fdtype(&sbuf) == NULL);

		if (ckpt_is_pipe(&sbuf))
			continue;

		if (S_ISCHR(sbuf.ckpt_mode)||S_ISBLK(sbuf.ckpt_mode)) {
			specfile = 1;
			flp = ckpt_file_lookup(sbuf.ckpt_rdev, 0, &found);
		} else {
			specfile = 0;
			flp = ckpt_file_lookup(sbuf.ckpt_dev, sbuf.ckpt_ino, &found);
		}
		if (flp == NULL)
			return (-1);

		IFDEB1(cdebug("leftover fds:\n"));
		IFDEB1(DUMP_SBUF(&sbuf));

		assert(found);
		/*
		 * Is this file 'handled' by this process?
		 */
		if ((flp->cl_pid == co->co_pid)&&(flp->cl_fd == sbuf.ckpt_fd))
			continue;

		if ((dupfd = ioctl(co->co_prfd, PIOCCKPTDUP, &sbuf.ckpt_fd)) < 0) {
			cerror("PIOCCKPTDUP (%s)\n", STRERR);
			return -1;
		}
		fid = (caddr_t)syssgi(SGI_CKPT_SYS, CKPT_FPADDR, dupfd);
		if (fid  == (caddr_t)(-1L)) {
			cerror("ckpt_write_fds:CKPT_FPADDR (%s)\n", STRERR);
			return (-1);
		}
		/*
		 * Was this another open of the file, was if a dup,
		 * or was it inherited?
		 */
		if (flp->cl_id == fid) {
			if (flp->cl_pid == co->co_pid)
				f.cf_flags |= CKPT_FLAGS_DUPFD;
			else
				f.cf_flags |= CKPT_FLAGS_INHERIT;
			f.cf_duppid =  flp->cl_pid;
			f.cf_dupfd = flp->cl_fd;
		} else {
			f.cf_flags |= CKPT_FLAGS_OPENFD;

			switch (unlinked = ckpt_pathname(co, dupfd, &path)) {
			case 0:
				IFDEB1(cdebug("dupfd=%d, path=%s\n", dupfd, path));
				break;
			case 1:
				break;
			default:
				/*
				 * non-checkpointable object
				 * This should have been caught earlier
				 */
				setoserror(ECKPT);
				cerror("Non-checkpointable object:pid %d:fd %d\n",
					co->co_pid, sbuf.ckpt_fd);
				return -1;
			}
			f.cf_flags |= CKPT_FLAGS_OPENFD;
			f.cf_fflags = sbuf.ckpt_fflags;
			f.cf_offset = sbuf.ckpt_offset;
			f.cf_fmode = sbuf.ckpt_mode;
			if (specfile)
				f.cf_mode = CKPT_MODE_IGNORE;
			else 
				f.cf_mode = ckpt_attr_get_mode(	co->co_ch,
								path,
								sbuf.ckpt_dev,
								sbuf.ckpt_ino,
								sbuf.ckpt_mode,
								unlinked,
								0);
			ckpt_setpaths(	co,
					&f,
					path,
					(specfile)? sbuf.ckpt_rdev:sbuf.ckpt_dev,
					(specfile)? 0:sbuf.ckpt_ino,
					sbuf.ckpt_mode,
					flp->cl_pid,
					unlinked);
			free(path);
		}
		f.cf_fd = sbuf.ckpt_fd;
		IFDEB1(cdebug("openfile: fd=%d, dupfd=%d (orig dupfd=%d) flag=0x%x\n",
			f.cf_fd, dupfd, f.cf_dupfd, f.cf_flags));
		f.cf_flp = NULL;

		if ((f.cf_fdflags = fcntl(dupfd, F_GETFD)) < 0) {
			cerror("F_GETFD (%s)\n", STRERR);
			return (-1);
		}
		ckpt_get_stropts(dupfd, &f.cf_stropts, 1);

		ckpt_get_flocks(&f, dupfd, co->co_pid);
		close(dupfd);

		/* write out the per file header and locks */
		CWRITE(co->co_sfd, &f, sizeof(ckpt_f_t), 1, pp, rc);
		if (rc == -1) {
			cerror("failed to write per file header\n", STRERR);
			return (-1);
		}
		curp = f.cf_flp;
		while (curp) {
			CWRITE(co->co_sfd, curp, sizeof(ckpt_fl_t), 0, pp, rc);
			if (rc == -1) {
				cerror("failed to write per file header\n", STRERR);
				break;
			}
			curp = curp->fl_next;
		}
		/*
		 * Due to the flocks may create variable property header in
		 * the statefile, create individual properties for each file
		 */
		if (ckpt_write_propindex(co->co_ifd, pp, &co->co_nprops, NULL) ||
		    rc == -1)
			return (-1);

		ckpt_pfd_add(co, &f, sbuf.ckpt_dev, sbuf.ckpt_ino);

	}
	return (0);
}

int
ckpt_write_treeinfo(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_ci_t cinfo;
	pid_t *roots;
	ssize_t rc;
	int i;

	/*
	 * fill in treeinfo header in the share statefile
	 */
	cinfo.ci_magic = CKPT_MAGIC_CHILDINFO;
	cinfo.ci_nchild = ch->ch_ntrees;
	CWRITE(ch->ch_sfd, &cinfo, sizeof(ckpt_ci_t), 1, pp, rc);
	if (rc < 0) {
		cerror("write (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * calculate tree root list
	 */
	if ((roots = (pid_t *)malloc(ch->ch_ntrees * sizeof(pid_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	for (i = 0; i < ch->ch_ntrees; i++)
		roots[i] = ch->ch_ptrees[i]->co_pid;

	CWRITE(ch->ch_sfd, roots, ch->ch_ntrees*sizeof(pid_t), 0, pp, rc);
	if (rc < 0) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_write_depend(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_dpl_t *dpl;
	ssize_t rc;

	/*
	 * Dump inherited shared memory info
	 */
	dpl = NULL;
	while (dpl = ckpt_get_depend(dpl)) {

		if (dpl->dpl_count == 0)
			break;

		CWRITE(	ch->ch_sfd,
			dpl->dpl_depend,
			dpl->dpl_count * sizeof(ckpt_depend_t),
			dpl->dpl_count,
			pp,
			rc);
		if (rc < 0) {
			cerror("Failed write (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_write_prattach(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_prattach_t *prp;
	ssize_t rc;

	prp = NULL;
	while (prp = ckpt_get_prattach(prp)) {
		IFDEB1(cdebug("pr_vaddr=0x%x pr_owner=%d pr_attach=%d\n",
			prp->pr_vaddr, prp->pr_owner, prp->pr_attach));
		CWRITE(ch->ch_sfd, prp, sizeof(ckpt_prattach_t), 1, pp, rc);
		if (rc < 0) {
			cerror("Failed write (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_write_pusema(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_pul_t *pup;
	ssize_t rc;

	pup = NULL;
	while (pup = ckpt_get_pusema(pup)) {
		CWRITE(ch->ch_sfd, &pup->pul_pusema, sizeof(ckpt_pusema_t), 1, pp, rc);
		if (rc < 0) {
			cerror("Failed write (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}
