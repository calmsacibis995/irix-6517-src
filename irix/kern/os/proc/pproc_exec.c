/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "$Id: pproc_exec.c,v 1.27 1999/02/01 20:53:34 jph Exp $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <elf.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/imon.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sema.h>
#include <sys/ksignal.h>
#include <sys/signal.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <string.h>
#include <sys/par.h>
#include <sys/capability.h>
#include <procfs/prsystm.h>
#include <sys/prctl.h>
#include <sys/numa.h>
#include <ksys/exception.h>
#include <ksys/cell.h>
#include "pproc.h"
#include "vproc_private.h"
#include "pproc_private.h"

extern int reset_limits_on_exec;

/*
 * pproc_exec - complete exec processing (possibly after migrate during
 * a remote exec).  vproc_exec is invoked from elf2exec just after it
 * has removed the old proces AS, and before it starts building the new
 * AS.
 *
 * At that point in exec, the call tree looks like:
 *				iexec
 *				  |
 *				gexec (level == 0)
 *				  |
 *			---------------------
 *			|		    |
 *			elfexec		    intpexec
 *			|			|
 *			|			gexec (level == 1)
 *			|			|
 *			|			elfexec
 *			|_______________________|
 *				|
 *				elf2exec
 *				|
 *				vproc_exec
 *				|
 *			---------------------
 *			|		    |
 *			dsproc_exec	    pproc_exec
 *
 * Each procedure contributes to the state of the exec with it's stack
 * data and kmem data.  All of that state is stored in the struct uarg
 * passed among these routines.
 *
 * pproc_exec completes the exec by calling each of the above routines'
 * associated {i,g,intp}exec_complete() routine in reverse order to perform
 * the second phase of exec.
 *
 * Any error that is detected before calling pproc_exec causes the call
 * chain to unwind, clean up, and return the error to user mode.
 *
 * Any error that is detected after calling pproc_exec are seen by each
 * completion routine when it is called for the second phase, causing them
 * to cleanup their first phase exec state and return.
 *
 * For a local exec, both the first and second phases of exec happen on
 * the source cell.  When pproc_exec returns it is returning to a call
 * chain of first phase callers.  As the second phase of exec has already
 * been completed, each routine just returns to it's caller.
 *
 * For a remote exec, the second phase happens on a different cell than the
 * first phase.  In order to properly dispose of the exec state on the
 * original cell, dsproc_exec returns an error which causes the original
 * exec state to be cleaned up as if a phase one error had happened.  The
 * thread must also destroy itself rather than return to user.  The thread
 * and process created on the target cell must contrive it's environment
 * such that when the exec is complete it can start execution of the new
 * program in user mode.
 */

int
iexec_complete(struct uarg *args, int error)
{
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);

	/* second phase - check for second phase error */
	if (error)
		goto done;

	/*
	 * Remember file name for accounting.
	 */
	p->p_acflag &= ~(AFORK|ASU);
	bcopy(&args->ua_exec_file, p->p_comm, PSCOMSIZ);
	_SAT_SET_COMM(p->p_comm);

	/*
	 * The ASU flag is only for acctcom's -u# option.
	 */
	if (cap_able_any(ut->ut_cred))
		p->p_acflag |= ASU;
	/*
	 * reset syssgi(SGI_NOFPE,..) across exec()
	 */
	ut->ut_pproxy->prxy_fp.pfp_nofpefrom = 0;
	ut->ut_pproxy->prxy_fp.pfp_nofpeto = 0;

done:
	if (error == EAGAIN)
		nomemmsg("exec");

	VN_RELE(args->ua_exec_vp);
	_SAT_EXEC(error);

	return error;
}

int
gexec_complete(vnode_t *vp, struct uarg *args, int level, int error)
{
	proc_t *pp = curprocp;
	uthread_t *ut = curuthread;
	int closerr, temp_abi;

	/* second phase - check for second phase error */
	if (error)
		goto closevp;

	ASSERT(pp->p_proxy.prxy_nthreads == 1
	       && prxy_to_thread(&pp->p_proxy)->ut_next == NULL);

	ASSERT(!(args->ua_abiversion < _MIN_ABI || 
		 args->ua_abiversion > _MAX_ABI));

	/* A hack so we don't have a separate syscall switch for n32 */
	temp_abi = args->ua_abiversion == ABI_IRIX5_N32 ?
					ABI_IRIX5 : args->ua_abiversion;
	if (syscallsw[temp_abi].sc_sysent == 0) {
		error = ENOEXEC;
		goto closevp;
	}

	ut->ut_pproxy->prxy_syscall = &syscallsw[temp_abi];
	ut->ut_pproxy->prxy_abi = args->ua_abiversion;

	if (level == 0) {
		struct cred *cr = pcred_lock(pp);

		if (args->ua_setid) {
			#pragma mips_frequency_hint NEVER
			uid_t old_ruid;

			/*
			 * Release calling uthread's reference to process's
			 * credentials, and reacquire rights to the new copy.
			 * There's only one uthread, so we don't have
			 * to pcred_push() the changes to other uthreads.
			 */
			crfree(ut->ut_cred);
			ut->ut_cred = cr = crcopy(pp);
			crhold(cr);

			old_ruid = cr->cr_ruid;

			cr->cr_uid = args->ua_uid;
			cr->cr_gid = args->ua_gid;
			cr->cr_suid = args->ua_uid;
			cr->cr_sgid = args->ua_gid;
			uidact_switch(old_ruid, cr->cr_ruid);

			(void)cap_recalc(&args->ua_cap);

			pcred_unlock(pp);

			/*
			 * Prevent unprivileged processes from enforcing
			 * resource limitations on setuid/setgid processes
			 * by reinitializing them to system defaults.
			 */
			if ((args->ua_uid == 0 || args->ua_gid == 0) &&
			    reset_limits_on_exec) {
			    int i;
			    for (i = 0; i < RLIM_NLIMITS; i++) {
				if (pp->p_rlimit[i].rlim_cur < rlimits[i].rlim_cur)
					pp->p_rlimit[i].rlim_cur = rlimits[i].rlim_cur;
				if (pp->p_rlimit[i].rlim_max < rlimits[i].rlim_max)
					pp->p_rlimit[i].rlim_max = rlimits[i].rlim_max;
			    }
			}

			/*
			 * If process is traced via /proc, arrange to
			 * invalidate the associated /proc vnode.
			 */
			if (pp->p_trace || (pp->p_flag & (SPROPEN|SPROCTR))) {
				#pragma mips_frequency_hint NEVER
				args->ua_traceinval = 1;
			}

		} else {
			/*
			 * After an exec, the saved uid/gid should always
			 * equal the current effective.  This prevents the
			 * exec of a setuid process from setting euid back
			 * to a [different] saved euid.
			 *
			 * We also need to deal with the case where we have
			 * ``pure'' capability recalculation set and need
			 * to drop all of our capabilities.
			 *
			 * As an optimization, avoid the credential copy in
			 * the case where nothing needs to be done ...
			 */
			if (cr->cr_suid != cr->cr_uid ||
			    cr->cr_sgid != cr->cr_gid ||
			    (cr->cr_cap.cap_inheritable & CAP_FLAG_PURE_RECALC)) {
				#pragma mips_frequency_hint NEVER
				/*
				 * Release calling uthread's reference to
				 * process's credentials, and reacquire
				 * rights to the new copy.  There's only one
				 * uthread, so we don't have to pcred_push()
				 * the changes to other uthreads.
				 */
				crfree(ut->ut_cred);
				ut->ut_cred = cr = crcopy(pp);
				crhold(cr);	/* ut_cred's reference */

				cr->cr_suid = cr->cr_uid;
				cr->cr_sgid = cr->cr_gid;
				(void)cap_recalc(&args->ua_cap);
			}
			pcred_unlock(pp);
		}

		if (pp->p_flag & STRC) {
			#pragma mips_frequency_hint NEVER
			sigtopid(current_pid(), SIGTRAP, SIG_ISKERN, 0, 0, 0);
		}
		if (args->ua_traceinval) {
			#pragma mips_frequency_hint NEVER
			prinvalidate(pp);
		}
	}
	if (pp->p_ptimers) {
		#pragma mips_frequency_hint NEVER
	  	delete_ptimers();
	}

closevp:
	VOP_CLOSE(vp, FREAD, L_TRUE, ut->ut_cred, closerr);
	ASSERT(args->ua_prev_script[level] == NULL);
	if (error || closerr) {
		#pragma mips_frequency_hint NEVER
		/*
		 * Failed.  Decrement use of new script, 
		 * restore old one.
		 */
		if (pp->p_script) {
			int s = VN_LOCK(pp->p_script);
			int c = --pp->p_script->v_intpcount;
			ASSERT(c >= 0);
			VN_UNLOCK(pp->p_script, s);
			if (!c)
				IMON_EVENT(pp->p_script, ut->ut_cred, 
					   IMON_EXIT);
			VN_RELE(pp->p_script);
		}
		pp->p_script = args->ua_prev_script[level];
		return error ? error : closerr;
	}
	return 0;
}

intpexec_complete(struct vnode *vp, struct uarg *args, int error)
{
	int s, c;

	VN_RELE(args->ua_intp_vp);

	/* second phase - check for second phase error */
	if (error)
		goto out;

	/*
	 *  Here we know the interpreter was successfully launched, and
	 *  we still have the script's vp and pathname.
	 *
	 *  Notify imon that the file has been executed, and store
	 *  the script's vnode in p_script.
	 */

	ASSERT(vp);
	VN_HOLD(vp);
	s = VN_LOCK(vp);
	c = vp->v_intpcount++;
	/* printf("intp: %d\n", c); */
	VN_UNLOCK(vp, s);
	if (!c)
		IMON_EVENT(vp, get_current_cred(), IMON_EXEC);
	curprocp->p_script = vp;

out:
	/* before returning, free buffer allocated for interpreter pathname */
	exhead_free(args->ua_idata.line1p);
#ifdef CKPT
	args->ua_ckpt = args->ua_intp_ckpt;
#endif
	return error;
}

/*ARGSUSED*/
int
pproc_exec(bhv_desc_t *bhv, struct uarg *args)
{
	int error;
	int level = args->ua_level;
	vnode_t *execvp = level ? args->ua_intp_vp : args->ua_exec_vp;
#ifdef DEBUG
	proc_t *p = BHV_TO_PROC(bhv);

	ASSERT(p != NULL);
	ASSERT(p->p_script == NULL);
	ASSERT(p->p_exec == NULL);
#endif

	error = args->ua_elf2_proc(execvp,
				   args,
				   args->ua_phdrbase,
				   args->ua_ehdrp,
				   args->ua_vwinadr,
				   args->ua_size,
				   args->ua_newsp,
				   args->ua_sourceabi);
	error = gexec_complete(execvp, args, level, error);
	if (level) {
		#pragma mips_frequency_hint NEVER
		level--;
		args->ua_level--;
		error = intpexec_complete(args->ua_exec_vp, args, error);
		error = gexec_complete(args->ua_exec_vp, args, 0, error);
	}
	error = iexec_complete(args, error);

	return error;
}

