/*************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.355 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <sys/exec.h>
#include <ksys/fdt.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/runq.h>
#include <sys/sema.h>
#include <sys/sat.h>
#include <sys/attributes.h>
#include <sys/signal.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <ksys/vfile.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <ksys/vproc.h>
#include <string.h>
#include <sys/rtmon.h>
#include <sys/par.h>
#include <sys/imon.h>
#include <procfs/prsystm.h>
#include <sys/numa.h>
#include <sys/ckpt.h>
#ifdef _SYSTEM_SIMULATION
#include <sys/kopt.h>
#endif 
#include "os/proc/pproc_private.h"	/* XXX bogus */
#if CELL_IRIX
#include <ksys/cell.h>
#include <ksys/cell/membership.h>
#endif
#include <sys/dmi.h>
#include <sys/dmi_kern.h>
#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif

/* these are dynamically changable */
extern int ncargs;
extern int reset_limits_on_exec;

extern sv_t nexit;

extern int gfx_exit(void);

/*
 * Length of /dev/fd prefix and suffix
 */
#define DEV_FD_PREFIX_LEN		8
#define DEV_FD_SUFFIX_LEN		8

/*
 * The exec switch table.  Called in order to try and exec a
 * particular a.out type.
 */
int (*execsw[])(vnode_t *, vattr_t *, struct uarg *, int) = {
	elfexec,
	intpexec,
};

int nexectype = sizeof(execsw) / sizeof(int (*)());

struct rexeca {
	sysarg_t	cell;
	char	*fname;
	char	**argp;
	char	**envp;
};

struct execa {
	char	*fname;
	char	**argp;
	char	**envp;
};
static int iexec(char *fname, char **argp, char **envp, cell_t cell);

#if CELL_IRIX
/*
 * TESTING - Set exec_rotor_low/exec_rotor_high to the min & max cells
 * you want to run on. The default is all cells.  Set do_rexec to
 * turn it on using a combination of the REXEC bit flags.
 */
int	exec_rotor = 0;
int	exec_rotor_low=0, exec_rotor_high=MAX_CELLS - 1;

#define REXEC_ON		0x1
#define REXEC_SKIP_GOLDEN  	0x2
#define REXEC_RANDOM		0x4

int	do_rexec = 0;

static cell_t
pick_cell(void)
{
	/* pick a cell to run on */

	if (!do_rexec) {
		return(cellid());
	}
	while (do_rexec & REXEC_ON) {
		if (do_rexec & REXEC_RANDOM) {
			exec_rotor = get_timestamp() % 
				((cell_membership >> 1) + 1);
		}
		else {

			/* round-robin */

			exec_rotor++;
			if (exec_rotor > exec_rotor_high) {
				exec_rotor = exec_rotor_low;
			} 
			if (exec_rotor == cellid())
				continue;
		}
		if (cell_in_membership(exec_rotor)) {
			if ((do_rexec & REXEC_SKIP_GOLDEN) && 
					exec_rotor == golden_cell) {
				continue;
			}
			break;
		}
	}
	return(exec_rotor);
}
#else
#define	pick_cell()	cellid()
#endif

/* ARGSUSED */
int
exec(struct execa *uap, rval_t *rvp)
{
	cell_t cell;

	cell = pick_cell();
	return (iexec(uap->fname, uap->argp, NULL, cell));
}

/* ARGSUSED */
int
exece(struct execa *uap, rval_t *rvp)
{
	cell_t cell;

	cell = pick_cell();
	return (iexec(uap->fname, uap->argp, uap->envp, cell)); 
}

/*
 * rexec - new process executes on specified cell
 */
/* ARGSUSED */
int
rexec(struct rexeca *uap, rval_t *rvp)
{
	cell_t cell;

	cell = uap->cell;
	if (cell < 0)
		return EINVAL;
#if CELL_IRIX
	if (!cell_in_membership(cell))
	        return ECELLDOWN;
#else
	if (cell != 0)
		return EINVAL;
#endif
	return (iexec(uap->fname, uap->argp, uap->envp, cell));
}

#ifdef DEBUG
int exectrace = 0;
#endif

static int
iexec(char *fname, char **argp, char **envp, cell_t cell)
{
	int error = 0;
	vnode_t *vp;
	struct pathname pn;
	int snapncargs = ncargs;
	struct uarg args;
	ckpt_handle_t *ckptp = NULL;
#ifdef CKPT
	ckpt_handle_t ckpt = NULL;

	if (ckpt_enabled)
		ckptp = &ckpt;
#endif
	SYSINFO.sysexec++;

	/*
	 * Lookup path name and remember last component for later.
	 */
	if (error = pn_get(fname, UIO_USERSPACE, &pn))
		return error;
	_SAT_PN_SAVE(&pn, curuthread);
#ifdef DEBUG
	{
	proc_t	*p = curprocp;
	if (exectrace == -1 || exectrace == p->p_pid)
		cmn_err(CE_CONT, "exec of (pid=%d) %s by %s\n",
			p->p_pid, pn.pn_buf, get_current_name());
	}
#endif
	bzero (&args, sizeof(args));

	_SAT_PN_BOOK(SAT_EXEC, curuthread);
	if (error = lookuppn(&pn, FOLLOW, NULLVPP, &vp, ckptp)) {
		pn_free(&pn);
		return error;
	}
	/*
	 * Save audit information about the old state.
	 * When the record's generated for real it'll be
	 * after the exec has changed attributes.
	 */
	_SAT_SAVE_ATTR(SAT_CAP_SET_TOKEN, curuthread);
	_SAT_SAVE_ATTR(SAT_UGID_TOKEN, curuthread);

	strncpy(args.ua_exec_file, pn.pn_path, PSCOMSIZ);
	args.ua_exec_file[PSCOMSIZ - 1] = '\0';

	args.ua_argp = argp;
	args.ua_envp = envp;
	args.ua_fname = fname;
	args.ua_fnameseg = UIO_USERSPACE;
	args.ua_ncargs = snapncargs;
	args.ua_cell = cell;
	args.ua_exec_vp = vp;
#ifdef CKPT
	args.ua_ckpt = (ckptp && *ckptp)? ckpt_lookup_add(vp, *ckptp) : -1;
#endif

#ifdef _SYSTEM_SIMULATION
	{
	proc_t	*p = curprocp;
	if (is_enabled(arg_sableexectrace))
		printf("Exec of %s (PID %d) by %s\n", pn.pn_path, p->p_pid, 
		       	p->p_comm);
	}
#endif
	pn_free(&pn);

	/* We have state that needs to be cleaned up on error */
	args.ua_exec_cleanup = 1;

	error = gexec(&vp, &args, 0);

	if (error == 0) {
		/*
		 * Tell rtmond our new name.  This actually generates more
		 * events than we typically want since this will cause
		 * events to be generated for every process in the system
		 * when we often are just tracing a small handful of
		 * processes.  We may need to think about ways of trying to
		 * avoid unneeded events ...
		 */
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_PIDAWARE)) {
			#pragma mips_frequency_hint NEVER
			fawlty_exec(args.ua_exec_file);
		}
	} else if (args.ua_exec_cleanup) {	/* handle error */
		#pragma mips_frequency_hint NEVER
		if (error == EAGAIN)
			nomemmsg("exec");

		VN_RELE(args.ua_exec_vp);
	}
	_SAT_EXEC(error);

	return(error);
}

/*
 * Get executable permissions (and capabilities if requested) of a vnode.
 * Check for various access/execute/etc. issues and return an appropriate
 * error if things are amiss.
 */
int
execpermissions(struct vnode *vp,
		struct vattr *vattrp,
		struct uarg *args)
{
	int error;
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);

	vattrp->va_mask = AT_MODE|AT_UID|AT_GID|AT_SIZE;

	VOP_GETATTR(vp, vattrp, ATTR_EXEC, ut->ut_cred, error);
	if (error)
		return error;

	/*
	 * Check the access mode.
	 */
	VOP_ACCESS(vp, VEXEC, ut->ut_cred, error);
	if (error != 0
	  || vp->v_type != VREG
	  || (vattrp->va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
		if (error == 0)
			error = EACCES;
		return error;
	}

	if (p->p_trace || PTRACED(p)) {
		#pragma mips_frequency_hint NEVER
		/*
		 * If we have read access then it's okay to let the exec()
		 * happen.
		 */
		VOP_ACCESS(vp, VREAD, ut->ut_cred, error);
		if (!error)
			return 0;
		/*
		 * If process is traced via ptrace(2), fail the exec(2).
		 */
		if (p->p_flag & STRC)
			return ENOEXEC;
		/*
		 * Process is traced via /proc.
		 * Arrange to invalidate the /proc vnode.
		 */
		args->ua_traceinval = 1;
	}

	return 0;
}

/*
 * Returns with args->ua_setid[level] set to:
 *   0 - not a setuid/setgid/setcap program or setuid/setgid/setcap disallowed
 *  !0 - a permitted setuid/setgid/setcap program
 *
 * Also reads any capabilities attached to the executable into args->ua_cap.
 * We need them here to determine if the image has attached capabilities and
 * later on we'll need them to recalculate our capabilities.  If an image does
 * not have attached capabilities, we mark args->ua_cap as invalid for that
 * capability recalculation.
 *
 * If the image is a permitted setuid/setgid/setcap program, then we also set
 * args->ua_uid and args->ua_gid.
 *
 * If there are no errors, 0 is returned; otherwise an error code is returned.
 */
static int
execsetid(vnode_t *vp, vattr_t *vattrp, struct uarg *args)
{
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);
	cap_set_t *acap = &args->ua_cap;
	int setid, capsize, error;
	uid_t uid;
	gid_t gid;

	/*
	 * Grab any capabilities attached to the executable.
	 */
	capsize = sizeof(cap_set_t);
	VOP_ATTR_GET(vp, SGI_CAP_FILE, (char *)acap, &capsize,
		     ATTR_ROOT, sys_cred, error);
	if (error ||
	    (acap->cap_effective   & CAP_INVALID) ||
	    (acap->cap_permitted   & CAP_INVALID) ||
	    (acap->cap_inheritable & CAP_INVALID)) {
		#pragma mips_frequency_hint FREQUENT
		/*
		 * A non-zero error indicates that there is either no
		 * capability set on the file or that if there was one, we
		 * couldn't get it.  Mark as an invalid capability set to
		 * keep track of the fact the file had no capabilities
		 * attached to it.  Treat an invalid capability set as a
		 * missing one.
		 */
		acap->cap_effective   = CAP_INVALID;
		acap->cap_permitted   = CAP_INVALID;
		acap->cap_inheritable = CAP_INVALID;
	}

	/*
	 * If neither SUID, SGID or SCAP simply return successfully.  Also
	 * silently ignore SUID/SGID/SCAP for file systems mounted with
	 * "nosuid" option.
	 */
	if (((vattrp->va_mode & (VSUID|VSGID)) == 0 &&
	     (acap->cap_effective & CAP_INVALID)) ||
	    (vp->v_vfsp->vfs_flag & VFS_NOSUID)) {
		#pragma mips_frequency_hint FREQUENT
		args->ua_setid = 0;
		return 0;
	}

	/*
	 * Compute proposed execution credentials.
	 */
	setid = 0;
	uid = ut->ut_cred->cr_uid;
	gid = ut->ut_cred->cr_gid;
	if (vattrp->va_mode & VSUID && vattrp->va_uid != uid) {
		uid = vattrp->va_uid;
		setid = 1;
	}
	if (vattrp->va_mode & VSGID && vattrp->va_gid != gid) {
		gid = vattrp->va_gid;
		setid = 1;
	}
	if (!(acap->cap_effective & CAP_INVALID)) {
		cap_set_t *ucapp = &ut->ut_cred->cr_cap;
		if (acap->cap_effective   != ucapp->cap_effective ||
		    acap->cap_permitted   != ucapp->cap_permitted ||
		    acap->cap_inheritable != ucapp->cap_inheritable) {
			setid = 1;
		}
	}

	/*
 	 * Set setuid/setgid/setcap protections, if not tracing.  If the
	 * process is being debuged (STRC) we never allow SUID/SGID/SCAP.
	 * If the process has system call and/or context switch tracing
	 * enabled, we allow the tracing to remain active if the process is
	 * privileged (CAP_PROC_MGT) or if it's being traced by a privileged
	 * tracer (SPARPRIV).
	 */
  	 if (setid) {
		if ((p->p_flag & (STRC|SPARSYS|SPARSWTCH)) &&
		    !(p->p_flag & SPARPRIV) && !_CAP_ABLE(CAP_PROC_MGT)) {
			#pragma mips_frequency_hint NEVER
			int s;
			if (p->p_flag & STRC)
				return EPERM;
			s = p_lock(p);
			p->p_flag &= ~(SPARSYS|SPARSWTCH|SPARINH);
			p->p_parcookie = 0;
			p_unlock(p, s);
		}
		args->ua_uid = uid;
		args->ua_gid = gid;
		args->ua_setid = 1;
	}
	return 0;
}

int
gexec(struct vnode **vpp, struct uarg *args, int level)
{
	proc_t *pp = curprocp;
	uthread_t *ut = curuthread;
	int i, error;
	struct vnode *vp;
	struct vattr vattr;

	vp = *vpp;
	if ((error = execpermissions(vp, &vattr, args)) != 0)
		return error;

	VOP_OPEN(vp, vpp, FREAD, ut->ut_cred, error);
	if (error)
		return error;

	vp = *vpp;
	args->ua_prev_script[level] = pp->p_script;
	args->ua_level = level;
	pp->p_script = NULL;

	repl_interpose(vp, "ReplDefault");

	if (vattr.va_size < MAGIC_SIZE) {
		#pragma mips_frequency_hint NEVER
		error = ENOEXEC;
		goto closevp;
	}

	if (level == 0) {
		#pragma mips_frequency_hint FREQUENT
		/*
		 * We only check for suid/sgid/scap on the thing being
		 * exec()'d.  In particular this means that for script
		 * execution, we only check the script and not its
		 * interpreter.
		 */
		error = execsetid(vp, &vattr, args);
		if (error)
			goto closevp;
	}
	if (!(ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_PRESERVE)) {
#if TFP
		ut->ut_pproxy->prxy_fp.pfp_fpflags = P_FP_IMPRECISE_EXCP;
#else
		ut->ut_pproxy->prxy_fp.pfp_fpflags = 0;
#endif
	}

	error = check_dmapi_file(vp);
	if (error)
		goto closevp;

	/*
	 * Loop through the switch table looking for the module that
	 * can handle this executable.
	 */
	for (i = 0; i < nexectype; i++) {
		error = (*execsw[i]) (vp, &vattr, args, level);
		if (error != ENOEXEC)
			break;
	}

	if (!error) {
		#pragma mips_frequency_hint FREQUENT
		pp->p_exec_cnt++;
		return 0;
	}
	
	if (!(args->ua_exec_cleanup)) {
		/* don't handle error */
		return error;
	}

closevp:
	ASSERT(error);
	VOP_CLOSE(vp, FREAD, L_TRUE, ut->ut_cred, i);

	/*
	 * Decrement use of new script, restore old one.
	 */
	if (pp->p_script) {
		int s = VN_LOCK(pp->p_script);
		int c = --pp->p_script->v_intpcount;
		ASSERT(c >= 0);
		VN_UNLOCK(pp->p_script, s);
		if (!c)
			IMON_EVENT(pp->p_script, ut->ut_cred, IMON_EXIT);
		VN_RELE(pp->p_script);
	}
	pp->p_script = args->ua_prev_script[level];

	return error;
}

int
exrdhead(struct vnode *vp, off_t off, size_t len, caddr_t *addrp)
{
	ssize_t resid;

	*addrp = kern_malloc(len);

	if (vn_rdwr(UIO_READ, vp, *addrp, len, off, UIO_SYSSPACE, 0, 0L,
		    get_current_cred(), &resid, &curuthread->ut_flid) != 0
	    || resid) {
		return ENOEXEC;
	}
	return 0;
}

void
exhead_free(caddr_t addr)
{
	kern_free(addr);
}

int
execmap(vnode_t 	*vp,
	caddr_t 	addr,
	size_t 		len,
	size_t 		zfodlen,
	off_t 		offset,
	int 		prot,
	int 		flags,
	vasid_t 	vasid,
	int 		ckpt)
{
	caddr_t 	oldaddr;
	size_t 		oldlen;
	off_t 		oldoffset;
	vnode_t 	*oldvp;
	as_addspace_t 	asadd;
	/* REFERENCED */
	as_addspaceres_t asres;
	int 		error = 0;
	int 		ismappable = 0;

	oldvp = vp;	/* for VN_RELE check at the end */

	oldaddr = addr;
	addr = (caddr_t)((long)addr & ~POFFMASK);
	oldlen = len;
	len += ((size_t)oldaddr - (size_t)addr);
	oldoffset = offset;
	offset = (off_t)((long)offset & ~POFFMASK);

	flags |= MAP_FIXED;
	if (poff(oldoffset) == poff(oldaddr) && len) {
		/* potentially map-able */
		ismappable = 1;
		/*
		 * Non-writable mappings are assumed to be text.  For these,
		 * we set them up as MAP_SHARED so that we don't have to
		 * allocate smem.  If we need to write to them later on,
		 * like when setting a breakpoint, we convert them to
		 * MAP_PRIVATE.  The MAP_TEXT flag tells us when to do this.
		 */
		if ((vp->v_type == VCHR) || (vp->v_type == VBLK))
			/* always PRIVATE */
			flags |= MAP_PRIVATE;
		else if (vp->v_type == VREG) {
			if (prot & PROT_WRITE)
				flags |= MAP_PRIVATE;
			else
				flags |= MAP_SHARED|MAP_TEXT;
		} else
			return ENODEV;

		/*
		 * check if ok with file system to be mapped
		 * We could get back a different vp ... (lofs)
		 *
		 * If a new vp is returned, then it has a reference
		 * that we must VN_RELE.
		 */
		VOP_MAP(vp, offset, len, prot, flags, get_current_cred(),
			&vp, error);
		if (error)
			return error;
	}

	if (ismappable) {
		/*
		 * this can occur when rld maps /dev/zero ...
		 */
		if ((vp->v_type == VCHR) || (vp->v_type == VBLK)) {
			asadd.as_op = AS_ADD_MMAPDEV;
			asadd.as_addr = addr;
			asadd.as_length = len;
			asadd.as_prot = prot;
			asadd.as_maxprot = PROT_ALL;
			asadd.as_mmap_off = offset;
			asadd.as_mmap_vp = vp;
			asadd.as_mmap_flags = flags;
			asadd.as_mmap_ckpt = ckpt;
			/* XXX really shouldn't have to do this -
			 * we shouldn't be called with aspacelock held
			 */
			VAS_UNLOCK(vasid);
			error = VAS_ADDSPACE(vasid, &asadd, &asres);
			ASSERT(error || asres.as_addr == addr);
			VAS_LOCK(vasid, AS_EXCL);
			goto out;
		}

		/*
		 * If vnode is being mapped for execution, mark it for
		 * replication also.
		 * This is done here instead of at gexec() time to allow
		 * mapping libraries for replication.
		 * If it's a one-node system, don't interpose the 
		 * replication layer on vnode.
		 */
		if (((prot & (PROT_WRITE|PROT_EXEC)) == PROT_EXEC) && 
				curprocp->p_shaddr == 0 && numnodes > 1) {
			repl_interpose(vp, "ReplDefault");
		}

		asadd.as_op = AS_ADD_EXEC;
		asadd.as_addr = addr;
		asadd.as_length = len;
		asadd.as_prot = prot;
		asadd.as_maxprot = PROT_ALL;
		asadd.as_exec_off = offset;
		asadd.as_exec_vp = vp;
		asadd.as_exec_flags = flags;
		asadd.as_exec_ckpt = ckpt;
		asadd.as_exec_zfodlen = zfodlen;
		error = VAS_ADDSPACE(vasid, &asadd, &asres);

		if (error) {
			/*
			 * don't repl_dispose - there could be others using
			 * it since nothing here is locked. Shouldn't
			 * really do any harm...
			 */
			goto out;
		}
		ASSERT(asres.as_addr == addr);
	} else {	
		if (vp->v_type != VREG) {
			error = ENODEV;
			goto out;
		}
		asadd.as_op = AS_ADD_LOAD;
		asadd.as_addr = addr;
		asadd.as_length = len;
		asadd.as_prot = prot;
		asadd.as_maxprot = PROT_ALL;
		asadd.as_load_off = oldoffset;
		asadd.as_load_vp = vp;
		asadd.as_load_flags = flags;
		asadd.as_load_ckpt = ckpt;
		asadd.as_load_laddr = oldaddr;
		asadd.as_load_llength = oldlen;
		asadd.as_load_zfodlen = zfodlen;
		error = VAS_ADDSPACE(vasid, &asadd, &asres);
		if (error)
			goto out;
		ASSERT(asres.as_addr == addr);
	}

	if (zfodlen) {
		/*
		 * space already set up - all we have to do is zero the
		 * portion from the end of the load data to the end of the
		 * page
		 */
		size_t end;

		ASSERT(error == 0);
		end = (size_t)addr + len;
		if (poff(end)) {
			/* need to unlock so can fault */
			VAS_UNLOCK(vasid);
			if (uzero((caddr_t)end, NBPP - poff(end)) != 0)
				error = EFAULT;
			VAS_LOCK(vasid, AS_EXCL);
			if (error)
				goto out;
		}
	}

 out:
	if (oldvp != vp)
		VN_RELE(vp);		/* release ref. from VOP_MAP */

	return error;
}

/*
 * Machine independent final setup goes here
 */
void
setexecenv(struct vnode *vp)
{
	int		i;
	uthread_t	*ut = curuthread;
	struct proc	*p = curprocp;
	proc_proxy_t	*prxy = ut->ut_pproxy;
	sigvec_t	*sigvp = &p->p_sigvec;

	mrlock(&p->p_who, MR_UPDATE, PZERO);
	ASSERT(p->p_exec == NULL);
	p->p_exec = vp;
	if (p->p_exec)
		VN_HOLD(p->p_exec);     /* in with the new */
	mrunlock(&p->p_who);

	prxy->prxy_oldcontext = 0;
	sigemptyset(&prxy->prxy_sigonstack);
	prxy->prxy_ssflags = 0;
	prxy->prxy_sigsp = 0;
	prxy->prxy_siglb = (caddr_t)0;

	/*
	 * In 1003.1b, section 3.1.2.2:
	 *
	 * "Signals set to the default action (SIG_DFL) in the calling
	 * process shall be set to the default action in the new process
	 * image.  Signals set to be ignored (SIG_IGN) by the calling
	 * process image shall be set to be ignored by the new process
	 * image.  Signals set to be caught by the calling process image
	 * shall be set to the default action in the new process image."
	 *
	 * XXX	Push all this into sig.c!
	 */
	sigvec_lock(&p->p_sigvec);
	for (i = 0; i < NUMSIGS; i++) {
		if (sigvp->sv_hndlr[i] != SIG_DFL &&
		    sigvp->sv_hndlr[i] != SIG_IGN) {
			sigvp->sv_hndlr[i] = SIG_DFL;
			sigemptyset(&sigvp->sv_sigmasks[i]);

			if (!sigvp->sv_sigpend.s_sigqueue &&
			    !ut->ut_sigpend.s_sigqueue)
				continue;
			if (sigismember(&ignoredefault, i + 1)) {
				sigdelq(&sigvp->sv_sigqueue, i + 1, sigvp);
				sigdelq(&ut->ut_sigqueue, i + 1, sigvp);
			} else
			if (sigvp->sv_sigpend.s_sigqueue) {
				sigqueue_t *sqp;
				while (sqp = sigdeq(&sigvp->sv_sigpend,
						    i+1, sigvp))
				{
					ASSERT(sqp->sq_info.si_signo == i + 1);
					sigaddq(&ut->ut_sigqueue, sqp, sigvp);
				}
			}
		}
	}

	/*
	 * Don't clear sv_sigrestart.  It should be inherited from the
	 * parent.
	 */
	sigorset(&sigvp->sv_sigign, &ignoredefault);
	sigemptyset(&sigvp->sv_sigcatch);
	sigemptyset(&sigvp->sv_signodefer);
	sigemptyset(&sigvp->sv_sigresethand);
	sigdiffset(&sigvp->sv_sainfo, &ignoredefault);

	/*
	 * If the user asked for NOCLDSTOP, the should still get it.  We
	 * turn off SNOWAIT, though, since in general we want children to
	 * zombify (POSIX assumes this behavior).  If the process wants
	 * SNOWAIT on, it will need to explicitly set it via
	 * sigaction().
	 */
	sigvp->sv_flags &= ~SNOWAIT;
	 
	/*
	 * Pending signals remain pending and held signals remain held, so
	 * don't clear p_phold or p_sig.  We should clear out any 'default
	 * == ignore' signals from p_sig, though.
	 */
	sigorset(&ut->ut_sig, &sigvp->sv_sigpend.s_sig);
	sigemptyset(&sigvp->sv_sigpend.s_sig);
	sigdiffset(&ut->ut_sig, &ignoredefault);
	sigvec_unlock(&p->p_sigvec);
}

/*
 * remove old process address space.
 * At this point, the exec no longer 'fails' the new process
 * is simply killed
 */
/*ARGSUSED4*/
int
remove_proc(
	struct proc *p,
	struct uarg *args,
	struct vnode *vp,
	int rmp)
{
	int	s;
	int	c, level;
	int	error;
	vasid_t vasid;
	as_deletespace_t asd;
	uthread_t *ut = curuthread;
	vproc_t *vpr;
        int oldf;

	ASSERT(p == curprocp);

	/*
	 * If the process is a graphics process, call the
	 * graphics exit routine since we are giving up graphics.
	 */
	if (UT_TO_KT(ut)->k_runcond & RQF_GFX)
		gfx_exit();

	/*
	 * Kill off other uthreads if this is pthreaded app.
	 * This discards pshare structure, too.
	 */
	if (ut->ut_flags & UT_PTHREAD) {
		vpr = UT_TO_VPROC(ut);
		VPROC_HOLD(vpr);
		VPROC_THREAD_STATE(vpr, THRD_EXEC, s);
		VPROC_RELE(vpr);

		if (s)
			return EBUSY;
	}

	/* if unblock on exec/exit flag is set, do that now */
	if (p->p_unblkonexecpid) {
		vpr = VPROC_LOOKUP(p->p_unblkonexecpid);

		if (vpr != NULL) {
			VPROC_UNBLKPID(vpr);
			VPROC_RELE(vpr);
		}
		p->p_unblkonexecpid = 0;
	}

	/*
	 * certain other things like profiling and single stepping are not
	 * held across an exec
	 */
#if R10000
	/*
	 * If we have a hardware event counter CPU monitoring structure
	 * attached to the uthread and it is set up for PC profiling, disable
	 * it because we're going to be tearing down the p_profp array below
	 * which PC profiling depends on.  We don't bother freeing it up here
	 * since the eventual exit path for the process/uthread will handle
	 * that and there are some complications with the tear down because
	 * the last uthread in a process is torn down after the proc/proxy
	 * are.
	 */
	if (ut->ut_cpumon && (ut->ut_cpumon->cm_flags & HWPERF_CM_PROFILING))
		hwperf_disable_counters(ut->ut_cpumon);
#endif
	s = p_lock(p);
	/*
	 * POSIX says to inherit all non-specified attributes.
	 * XXX FIXADE is of marginal use to inherit..
	 */
	if (p->p_flag & SPROFFAST)
		stopprfclk();
	if (p->p_flag & SABORTSIG)
		p->p_exitsig = 0;
	p->p_flag &= ~(SPROF|SPROF32|SPROFFAST|SABORTSIG);
	p->p_flag |= SEXECED;		/* for setpgid() */
	p_unlock(p, s);

	prxy_flagclr(&p->p_proxy, PRXY_USERVME);

	s = ut_lock(ut);
        oldf = ut->ut_flags;
	ut->ut_flags &= ~(UT_STEP|UT_SRIGHT|UT_PTHREAD|UT_PTPSCOPE|UT_OWEUPC);
	ut_unlock(ut, s);
	if (oldf & UT_SRIGHT)
		prsright_release(&p->p_proxy);

	if (p->p_profp) {
		(void) kern_free(p->p_profp);
		p->p_profp = NULL;
		p->p_profn = 0;
	} else {
		ASSERT(p->p_profn == 0);
	}

	/*
	 * If ut_sighold points to prda, change back to kernel space.
	 */
	if (ut->ut_sighold != &p->p_sigvec.sv_sighold) {
		s = ut_lock(ut);
		p->p_sigvec.sv_sighold = *ut->ut_sighold;
		ut->ut_sighold = &p->p_sigvec.sv_sighold;
		ut_unlock(ut, s);
#if	(_MIPS_SIM != _ABIO32)
		ut->ut_prda->t_sys.t_flags &= ~T_HOLD_VALID;
#else
		ut->ut_prda->t_sys.t_flags &= ~(T_HOLD_VALID|T_HOLD_KSIG_O32);
#endif
	}
	
	/*
	 * if part of a share group - get rid of that
	 */
	if (IS_SPROC(&p->p_proxy)) {
		/* Notify scheduler that we are leaving share group */
		leaveshaddrRunq(ut);
		asd.as_exec_detachstk = detachshaddr(p, SHDEXEC);
	} else
		asd.as_exec_detachstk = 0;

	/*
	 * Now that we've detached from the share group, we can check
	 * to see if this is an intp suid exec.  If so, we can do the
	 * open and adjust the arg list appropriately.
	 */
	if (args->ua_intpvp) {
		/*
		 * Note that in fuexarg(), the args->ua_intpstkloc has been
		 * set up to point at a location that contains args->ua_fname.
		 * In intp, we set up args->ua_name to be "/dev/fd/XXXXXXX".
		 *
		 * Things would be a lot easier if we set up the
		 * stack after sloughing the share group.
		 */
		int fd;
		char fdnum[32];
		int len;

		if (error = execopen(&args->ua_intpvp, &fd)) {
			/*
			 * Caller will kill the process. 
			 */
			return error;
		}
#ifdef CKPT
		if (args->ua_ckpt >= 0)
			ckpt_setfd(fd, args->ua_ckpt);
#endif
		/*
		 * Substitute the name of the /dev/fd node into
		 * the args structure.
		 */
		ASSERT(args->ua_intpstkloc);

		/*
		 * Get the fd value.
		 */
		numtos(fd, fdnum);
		
		/*
		 * Wipe out the "XXXXXXX" in the stack location.
		 */
		bzero(&args->ua_intpstkloc[DEV_FD_PREFIX_LEN],
							DEV_FD_SUFFIX_LEN);
		
		/*
		 * Now insert the new descriptor value into the stack
		 * location.
		 */
		strcpy(&args->ua_intpstkloc[DEV_FD_PREFIX_LEN], fdnum);
		
		/*
		 * Fix up the p_psargs location.  We know that this must
		 * be the last argument in the list according to how
		 * intp sets up arguments.
		 */
		if (args->ua_intppsloc) {
			/*
			 * We need to make sure we don't stomp off the end
			 * of the psargs array when copying the fd number.
			 */
			len = ((PSARGSZ - 1) -
				(&args->ua_intppsloc[DEV_FD_PREFIX_LEN] -
				&p->p_psargs[0]));
			if (len > 0) {
				len = MIN(strlen(fdnum), len);
				strncpy(&args->ua_intppsloc[DEV_FD_PREFIX_LEN],
								fdnum, len);
				args->ua_intppsloc[DEV_FD_PREFIX_LEN+len] = '\0';
			}
		}
	}

	/*
	 * Perform exec processing, including close-on-exec.
	 * 
	 * Note that for sprocs the fdt_exec logic assumes that
	 * detachshaddr (and hence fdt_detach_shaddr) has already
	 * been called.  This ensures that close-on-exec
	 * processing occurs for a sproc after it has gotten
	 * its own copy of the fdt.
	 */
	fdt_exec();

	/* remove old exec image */
	if (p->p_exec) {
		mrlock(&p->p_who, MR_UPDATE, PZERO);
		VN_RELE(p->p_exec);     /* out with the old */
		p->p_exec = NULL;
		mrunlock(&p->p_who);
	}

	/*
	 * release old scripts before rexec - ok to do here because
	 * any failures from this point on cause the process
	 * to be killed
	 */

	for (level = 0; level < 2; level++) {
		if (args->ua_prev_script[level]) {
			s = VN_LOCK(args->ua_prev_script[level]);
			c = --args->ua_prev_script[level]->v_intpcount;
			ASSERT(c >= 0);
			VN_UNLOCK(args->ua_prev_script[level], s);
			if (!c)
				IMON_EVENT(args->ua_prev_script[level],
					   ut->ut_cred, IMON_EXIT);
			VN_RELE(args->ua_prev_script[level]);
			args->ua_prev_script[level] = NULL;
		}
	}

	/* Remove old address space */
	asd.as_op = AS_DEL_EXEC;
	asd.as_exec_rmp = rmp;
	asd.as_exec_prda = ut->ut_prda;
	ut->ut_prda = 0;
	as_lookup_current(&vasid);
	VAS_DELETESPACE(vasid, &asd, NULL);

	ASSERT(ut->ut_sharena == NULL);
        
	/* The new tlbpid is needed after we're done with the
	 * old process and before we need anything for the new one.
	 * This effectively flushes the tlb of any pages that
	 * were released by execbld.
	 * Getxfile may read in the text of the new process and 
	 * will need to use the new tlbpid.
	 */
	new_tlbpid(&ut->ut_as, VM_TLBINVAL);

	/* 
	 * Initialized the wired tlb entries for the new process.
	 */
	setup_wired_tlb(1);

	return 0;
}

int
execopen(struct vnode **vpp, int *fdp)
{
	struct vnode *vp = *vpp;
	struct vnode *openvp = vp;
	vfile_t *fp;
	int error = 0;
	int filemode = FREAD;

	VN_HOLD(vp);		/* open reference */
	if (error = vfile_alloc(filemode, &fp, fdp)) {
		VN_RELE(vp);
		*fdp = -1;	/* just in case vfile_alloc changed value */
		return error;
	}

	VOP_OPEN(openvp, &vp, filemode, sys_cred, error);
	if (error) {
		VN_RELE(vp);
		vfile_alloc_undo(*fdp, fp);
		*fdp = -1;		
		return error;
	}
	vfile_ready(fp, vp);

	*vpp = vp;		/* vnode should not have changed */
	return 0;
}

int
execclose(int fd)
{
	int error;
	auto vfile_t *fp;

	if (error = closefd(fd, &fp))
		return error;
	return vfile_close(fp);
}

/*
 *	Check if the file has DMAPI managed regions/events.  If so,
 *	generate a DMAPI read event for the entire file.  
 *
 *	Only a "read" event will be generated since check_dmapi_file
 *	is intended to be called only for gexec() and elfmap() files.
 *	In these cases, if the process later uses the mprotect() syscall
 *	to upgrade the page protection to include PROT_WRITE, the mapping
 *	type is changed to private.
 *
 *	Note that the VOP_FCNTL used here returns an error if the
 *	underlying file system is unaware of the F_DMAPI subfunction
 *	being used.  This causes no problems, since a non-zero return
 *	status is simply ignored.  Only in the case of a zero return status
 *	can we be sure that the VOP_FCNTL F_DMAPI subfunction
 *	DM_FCNTL_MAPEVENT is implemented for this file system, and then
 *	interpret the maprq.error field.
 */
/* ARGSUSED */
int
check_dmapi_file(vnode_t	*vp)
{
#ifdef CELL_IRIX
/*
 * This code doesn't work with cells for the following reasons:
 * 1) idl cannot tolerate null rval pointer
 * 2) stack variable dmfcntl is pass as an in/out param;  server cell
 *    accesses it directly (read and write);  this will fail on
 *    sn0 due to the firewall protections
 */
	return(0);
#else
	int		error;
	dm_fcntl_t	dmfcntl;

	dmfcntl.dmfc_subfunc = DM_FCNTL_MAPEVENT;
	dmfcntl.u_fcntl.maprq.length = 0;	/* length = 0 for whole file */
	dmfcntl.u_fcntl.maprq.max_event = DM_EVENT_READ;

	VOP_FCNTL(vp, F_DMAPI, &dmfcntl, 0, (off_t)0, sys_cred, NULL, error);
	if (error == 0) {
		if ((error = dmfcntl.u_fcntl.maprq.error) != 0)
			return error;
	}
	return 0;
#endif
}
