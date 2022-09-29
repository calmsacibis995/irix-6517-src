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

/*
 * Interface procedures for UniCenter.
 */

#include	<sys/uncintf.h>
#include        <ksys/vproc.h>
#include        <ksys/fdt.h>
#include	<ksys/fdt_private.h>
#include	<ksys/vop_backdoor.h>
#include	<ksys/vsession.h>
#include 	"os/proc/pproc_private.h"      /* XXX bogus */

#define		DEFINED_32	0x01
#define		DEFINED_64	0x02

/*
 * This routine provides an interface to intercept any system call based
 * on their numbers in sys.s. The original system call handler is returned
 * as a cookie that can be passed to syscall_continue.
 */
void *
syscall_intercept(int sysnum, sy_call_t intercept_function)
{
	int     num = sysnum - SYSVoffset;
	void    *cookie;

#if _MIPS_SIM == _ABI64
	sy_call_t	cookie1, cookie2;
	int		both = 0;

	cookie1 = cookie2 = NULL;
	if (sysent[num].sy_call != nosys) {
		cookie1 = sysent[num].sy_call;
		sysent[num].sy_call = intercept_function;
		both |= DEFINED_32;
	}
	if (irix5_64_sysent[num].sy_call != nosys) {
		cookie2 = irix5_64_sysent[num].sy_call;
		irix5_64_sysent[num].sy_call = intercept_function;
		both |= DEFINED_64;
	}
	if (cookie1 == cookie2) {
		/* defined or undefined for both */
		cookie = (void *)cookie1;
	} else {
		if ((both & DEFINED_32) && (both & DEFINED_64)) {
			/* if defined differently for two */
			sy_call_t *ptr = kmem_alloc(2 * sizeof(sy_call_t),
								KM_SLEEP);

			ptr[0] = (sy_call_t)cookie1;
			ptr[1] = (sy_call_t)cookie2;
			cookie = (void *)((__psunsigned_t)ptr | 
						DEFINED_32 | DEFINED_64);
		} else {
			/* if defined for only one */
			if (both & DEFINED_32)
				cookie = (void *)((__psunsigned_t)cookie1 | 
								DEFINED_32);
			else
				cookie = (void *)((__psunsigned_t)cookie2 | 
								DEFINED_64);
		}
	}
#else
	cookie = (void *)sysent[num].sy_call;
	if (cookie != (void *)nosys)
		sysent[num].sy_call = intercept_function;
	else
		cookie = NULL;
#endif
	return(cookie);
}


/*
 * This routine provides an interface to restore the original syscall
 * handler, when intercepting is no more needed.
 */
void
syscall_restore(int sysnum, void * cookie)
{
	int     num = sysnum - SYSVoffset;

#if _MIPS_SIM == _ABI64
	int	both;
	void	*cooky;
	sy_call_t *ptr;

	if (cookie == NULL)
		return;
	both = ((__psunsigned_t)cookie) & (DEFINED_32 | DEFINED_64);
	switch (both) {
		case 0 :
			irix5_64_sysent[num].sy_call = (sy_call_t)cookie;
			sysent[num].sy_call = (sy_call_t)cookie;
			break;
		case DEFINED_32 :
			cooky = (void *)((__psunsigned_t)cookie - DEFINED_32);
			sysent[num].sy_call = (sy_call_t)cooky;
			break;
		case DEFINED_64 :
			cooky = (void *)((__psunsigned_t)cookie - DEFINED_64);
			irix5_64_sysent[num].sy_call = (sy_call_t)cooky;
			break;
		default :
			cooky = (void *)((__psunsigned_t)cookie - DEFINED_64 
								- DEFINED_32);
			ptr = (sy_call_t *)cooky;
			sysent[num].sy_call = ptr[0];
			irix5_64_sysent[num].sy_call = ptr[1];
			kmem_free(cooky, 2 * sizeof(sy_call_t));
			break;
	}
#else
	if (cookie == NULL)
		return;
	sysent[num].sy_call = (sy_call_t)cookie;
#endif
}


/*
 * This provides an interface for an intercepted system call to be allowed
 * to proceed transparently, without being aware of the intercept.
 */
int
syscall_continue(void * cookie, sysarg_t * sysarg , void *rv, uint sysnum)
{
	return((*(sy_call_t)(cookie))(sysarg, rv, sysnum));
}


/*
 * The following kernel messaging primitives utilize the existing msg
 * primitives to allow msging to be used between kernel code and user
 * level code. Needed by 3rd party UniCenter software.
 */
int
kmsgget(key_t key, int msgflg, int *errno)
{
	struct msggeta	tmp;
	rval_t		rv;
	int		error = 0;

	rv.r_val1 = 1;
	tmp.opcode = MSGGET;
	tmp.key = key;
	tmp.msgflg = msgflg;
	error = msgsys((struct msgsysa *)&tmp, &rv);
	if (!error)
		return(rv.r_val1);
	else {	
		if (errno) *errno = error;
		return(-1);
	}
}

int 
kmsgctl(int msqid, int cmd, struct msqid_ds *buf, int *errno)
{
	struct msgctla 	tmp;
	rval_t		rv;
	int             error = 0;

	rv.r_val1 = 1;
	tmp.opcode = MSGCTL;
	tmp.msgid = msqid;
	tmp.cmd = cmd;
	tmp.buf = buf;
	error = msgsys((struct msgsysa *)&tmp, &rv);
	if (!error)
		return(0);
	else {
		if (errno) *errno = error;
		return(-1);
	}
}

int
kmsgsnd(int msqid, void *msgp, size_t msgsz, int msgflg, int *errno)
{
	struct msgsnda	tmp;
	rval_t		rv;
	int		error = 0;

	rv.r_val1 = 1;
	tmp.opcode = MSGSND;
	tmp.msqid = msqid;
	tmp.msgp = msgp;
	tmp.msgsz = msgsz;
	tmp.msgflg = msgflg;
	error = msgsys((struct msgsysa *)&tmp, &rv);
	if (!error)
		return(0);
	else {
		if (errno) *errno = error;
		return(-1);
	}
}

int
kmsgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg, 
	int *errno)
{
	struct msgrcva	tmp;
	rval_t		rv;
	int		error = 0;

	rv.r_val1 = 1;
	tmp.opcode = MSGRCV;
	tmp.msqid = msqid;
	tmp.msgp = msgp;
	tmp.msgsz = msgsz;
	tmp.msgtyp = msgtyp;
	tmp.msgflg = msgflg;
	error = msgsys((struct msgsysa *)&tmp, &rv);
	if (!error)
		return(rv.r_val1);
	else {
		if (errno) *errno = error;
		return(-1);
	}
}


/*
 * Tape driver interface call for 3rd party software (UniCenter).
 * Returns 0 if the input vnode does not belong to a tape device, 1 if
 * it does. Also indicates whether the driver is 3rd party or not.
 */
int
isa_scsi_tape(vnode_t *tvn, int *thirdp)
{
	dev_t		dev;
	device_driver_t	ddt;
	char		dpref[MAXDEVNAME];

	if ((tvn == NULL) || (tvn->v_type != VCHR))
		return(0);

	dev = tvn->v_rdev;
	/*
	 * Get device driver handle for device.
	 */
	if ((ddt = device_driver_getbydev(dev)) == NULL)
		return(0);

	/*
	 * Get driver prefix.
	 */
	bzero(dpref, MAXDEVNAME);
	device_driver_name_get(ddt, dpref, MAXDEVNAME);

	/*
	 * Is the driver "tpsc"?
	 */
	if (!strncmp(dpref, TPSC_PREFIX, MAXDEVNAME)) {
		if (thirdp) *thirdp = 0;
		return(1);
	}

	/*
	 * See if the device is a 3rd party tape driver.
	 */
	if (thirdp) *thirdp = 1;
	if (device_driver_admin_info_get(dpref, TAPE_IDENT))
		return(1);
	return(0);
}

/*
 * Tape driver interface call for 3rd party (UniCenter) software.
 * The input vnode should be the vnode for the tape device. Note that
 * this can be invoked only after a tpscopen (that will dynamically
 * load the driver)
 * Returns 0 on success, with some fields in mt updated, or a positive
 * error number.
 * For the MTIOCKGET call:
 * mt_state - CT_ONL, CT_WRP, CT_EOM, CT_BOT, CT_FM, CT_COMPRESS,
 *	       CT_QIC24, CT_QIC120, CT_SMK in tpsc.h
 * mt_dens -  scsi tape density for mode select if drive is MTCAN_SETDEN,
 *	      -1 otherwise.
 * mt_block - block size if in fixed block mode, -1 otherwise.
 */
int
tpscstat(vnode_t *tvn, void *mt, int cmd)
{
	vopbd_t	vbd;
	int	error, rv;

	ASSERT(mt && tvn && ((cmd == MTIOCKGET) || (cmd == MTIOCGET)));
	vbd.vopbd_req = VOPBDR_NIL;
	VOP_IOCTL(tvn, cmd, (void *)mt, 0, sys_cred, &rv, &vbd, error);
	return(error);
}

/*
 * This routine returns the # open fds that a the current process has.
 */
int
fdt_getnum(void)
{
	proc_t 		*curp = curprocp;
	struct ufchunk 	*ufp;
	vfile_t 	*fp;
	fdt_t		*fdt;
	int 		fd, num = 0;

	/*
	 * Lock the fdt while scanning the list of chunks.
	 */
	fdt = curp->p_fdt;

	FDT_READ_LOCK(fdt);

	ufp = fdt->fd_flist;
	for (fd = 0; fd < fdt->fd_nofiles; fd++) {
		fp = ufp[fd].uf_ofile;
		if (fp != NULL && !(fp->vf_flag & FINPROGRESS))
			num++;
	}

	FDT_UNLOCK(fdt);
	return(num);
}

/*
 * dozombie :   either set to ZNO or ZYES by caller. If set to ZYES, the pid
 *              might get deallocated and reallocated to a different process.
 * caveats  :   most of the code is as racy as prgetpsinfo.
 *              fields other than requested might be updated in the buffer.
 * returns  :   EINVAL on senseless parameters.
 *              ENOENT if pid couldn't be found.
 *              0 on success, the buffer is filled with requested info.
 */
int
pidstats(pid_t tpid, pidparm_t *buf, uint_t mask, int dozombie)
{
	vp_get_xattr_t	attr;
	vproc_t 	*vpr;
	vsession_t	*vsp;
	dev_t		devnum;
	int		xflags = 0;

	if (buf == NULL)
		return(EINVAL);
	if (dozombie == NOZOMB)
		dozombie = ZNO;
	else if (dozombie == DOZOMB)
		dozombie = ZYES;
	else
		return(EINVAL);

	if (mask & PARENTPID)
		xflags |= VGATTR_PPID;
	if (mask & CONTROLDEV) 
		xflags |= (VGATTR_SID|VGATTR_HAS_CTTY);
	if (mask & NICEVAL)
		xflags |= VGXATTR_NICE;
	if (mask & PGRPID)
		xflags |= VGATTR_PGID;
	if (mask & USERID)
		xflags |= VGXATTR_UID;
	if (mask & SLPCHAN)
		xflags |= VGXATTR_WCHAN;
	if (mask & STARTTIME) 
		xflags |= VGATTR_SIGNATURE;
	if (mask & PRIORITY)
		xflags |= VGXATTR_PRI;
	if (mask & LASTCPU)
		xflags |= VGXATTR_LCPU;
	if (mask & PRSS)
		xflags |= VGXATTR_RSS;
	if (mask & PNAME)
		xflags |= VGXATTR_NAME;
	if (mask & PSIZE)
		xflags |= VGXATTR_SIZE;
	if (mask & USTIME)
		xflags |= VGXATTR_UST;
	if (mask & CID)
		xflags |= VGATTR_CELLID;

	/*
	 * Race here with tpid exit.
	 */
	if ((vpr = VPROC_LOOKUP_STATE(tpid, dozombie)) == NULL) {
		return(ENOENT);
	} else {
		VPROC_GET_XATTR(vpr, xflags, &attr);
		VPROC_RELE(vpr);
		if (mask & PARENTPID)
			buf->parentpid = attr.xa_basic.va_ppid;
		if (mask & PGRPID)
			buf->pgrpid = attr.xa_basic.va_pgid;
		if (mask & PNAME) {
			bcopy(attr.xa_name, buf->procname, 
					min(sizeof(attr.xa_name), PSCOMSIZ));
			bcopy(attr.xa_args, buf->procargs, 
					min(sizeof(attr.xa_args), PSARGSZ));
		}
		if (mask & PSIZE) {
			buf->stacksz = attr.xa_stksz;
			buf->totsz = attr.xa_size;
		}
		if (mask & PRSS)
			buf->prss = attr.xa_rss;
		if (mask & NICEVAL)
			buf->niceval = attr.xa_nice;
		if (mask & USERID) {
			buf->effuid = attr.xa_basic.va_uid;
			buf->realuid = attr.xa_realuid;
			buf->saveuid = attr.xa_saveuid;
		}
		if (mask & SLPCHAN)
			buf->slpchan = attr.xa_slpchan;
		if (mask & STARTTIME) {
			/*
			 * Time process was created.
			 */
			buf->starttime.tv_sec = attr.xa_basic.va_signature;
			buf->starttime.tv_usec = 0L;
		}
		if (mask & PRIORITY)
			buf->priority = attr.xa_pri;
		if (mask & LASTCPU) 
			buf->lastcpu = attr.xa_lcpu;
		if (mask & USTIME) {
			buf->utime = attr.xa_utime;
			buf->stime = attr.xa_stime;
		}
		if (mask & CID)
			buf->cellid = attr.xa_basic.va_cellid;
		if (mask & CONTROLDEV) {
			/*
	 		 * Raced with tpid changing sid or ctty.
	 		 */
			if (!attr.xa_basic.va_has_ctty) {
				buf->controldev = NODEV;
			} else {
				/*
	 			 * Race here with session going away.
	 			 */
				vsp = VSESSION_LOOKUP(attr.xa_basic.va_sid);
				if (vsp == NULL) {
					buf->controldev = NODEV;
				} else {
					/*
	 				 * Race here with session changing ctty.
	 				 */
					VSESSION_CTTY_DEVNUM(vsp, &devnum);
					VSESSION_RELE(vsp);
					buf->controldev = devnum;
				}
			}
		}
	}
	return(0);
}

/*
 * These are retained for backwards compatibility and should be
 * removed once we know how CA is supporting their software on
 * the 6.5 stream.
 */
pid_t
getparent(pid_t cpid)
{
	pidparm_t	buf;

	if (pidstats(cpid, &buf, PARENTPID, NOZOMB))
		return(NOPID);
	else 
		return(buf.parentpid);
}

dev_t
getcttydev(pid_t tpid)
{
	pidparm_t	buf;

	if (pidstats(tpid, &buf, PARENTPID, NOZOMB))
		return(NODEV);
	else
		return(buf.controldev);
}
