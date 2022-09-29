/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 3.38 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/strmp.h>
#include <sys/kabi.h>
#include <sys/kstropts.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/fs_subr.h>

/*
 * STREAMS system calls.
 */

struct msgnp {		/* non-priority version retained for compatibility */
	sysarg_t fdes;
	struct strbuf *ctl;
	struct strbuf *data;
	sysarg_t flags;
};

struct msgp {		/* priority version */
	sysarg_t fdes;
	struct strbuf *ctl;
	struct strbuf *data;
	sysarg_t pri;
	sysarg_t flags;
};

static int msgio(struct msgp *, rval_t *, int, unsigned char *, int *);

int
getmsg(struct msgnp *uap, rval_t *rvp)
{
	struct msgp ua;
	int error;
	int localflags;
	int realflags = 0;
	unsigned char pri = 0;

	/*
	 * Convert between old flags (localflags) and new flags (realflags).
	 */
	if (copyin((caddr_t)uap->flags, (caddr_t)&localflags, sizeof(int)))
		return (EFAULT);
	switch (localflags) {
	case 0:
		realflags = MSG_ANY;
		break;

	case RS_HIPRI:
		realflags = MSG_HIPRI;
		break;

	default:
		return (EINVAL);
	}

	ua.fdes = uap->fdes;
	ua.ctl = uap->ctl;
	ua.data = uap->data;
	if ((error = msgio(&ua, rvp, FREAD, &pri, &realflags)) == 0) {
		/*
		 * massage realflags based on localflags.
		 */
		if (realflags == MSG_HIPRI)
			localflags = RS_HIPRI;
		else
			localflags = 0;
		if (copyout((caddr_t)&localflags, (caddr_t)uap->flags,
		  sizeof(int)))
			error = EFAULT;
	}
	_SAT_FD_RDWR(uap->fdes, FREAD, error);
	return (error);
}

int
putmsg(struct msgnp *uap, rval_t *rvp)
{
	unsigned char pri = 0;
	int flags;
	int error;

	switch (uap->flags) {
	case RS_HIPRI:
		flags = MSG_HIPRI;
		break;

	case 0:
		flags = MSG_BAND;
		break;

	default:
		return (EINVAL);
	}
	error = msgio((struct msgp *)uap, rvp, FWRITE, &pri, &flags);
	_SAT_FD_RDWR(uap->fdes, FWRITE, error);
	return (error);
}

int
getpmsg(struct msgp *uap, rval_t *rvp)
{
	int error;
	int flags;
	int intpri;
	unsigned char pri;

	if (copyin((caddr_t)uap->flags, (caddr_t)&flags, sizeof(int)))
		return (EFAULT);
	if (copyin((caddr_t)uap->pri, (caddr_t)&intpri, sizeof(int)))
		return (EFAULT);
	if ((intpri > 255) || (intpri < 0))
		return (EINVAL);
	pri = (unsigned char)intpri;
	if ((error = msgio(uap, rvp, FREAD, &pri, &flags)) == 0) {
		if (copyout((caddr_t)&flags, (caddr_t)uap->flags, sizeof(int)))
			return (EFAULT);
		intpri = (int)pri;
		if (copyout((caddr_t)&intpri, (caddr_t)uap->pri, sizeof(int)))
			error = EFAULT;
	}
	_SAT_FD_RDWR(uap->fdes, FREAD, error);
	return (error);
}

int
putpmsg(struct msgp *uap, rval_t *rvp)
{
	unsigned char pri;
	int flags = uap->flags;
	int error;

	if ((uap->pri > 255) || (uap->pri < 0))
		return (EINVAL);
	pri = (unsigned char)uap->pri;
	error = msgio(uap, rvp, FWRITE, &pri, &flags);
	_SAT_FD_RDWR(uap->fdes, FWRITE, error);
	return (error);
}

/*
 * Common code for getmsg and putmsg calls: check permissions,
 * copy in args, do preliminary setup, and switch to
 * appropriate stream routine.
 */
static int
msgio(register struct msgp *uap,
      rval_t *rvp,
      register int mode,
      unsigned char *prip,
      int *flagsp)
{
		 vfile_t	*fp;
	register vnode_t	*vp;
		 struct strbuf	msgctl, msgdata;
	register int		error;
#if _MIPS_SIM == _ABI64
		 struct irix5_strbuf i5_msg;
#endif

	if (error = getf(uap->fdes, &fp))
		return (error);
	if ((fp->vf_flag & mode) == 0)
		return (EBADF);
	if (!VF_IS_VNODE(fp))
		return (EINVAL);

	vp = VF_TO_VNODE(fp);

	if ((vp->v_type != VFIFO && vp->v_type != VCHR) || vp->v_stream == NULL)
		return (ENOSTR);
#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(get_current_abi())) {
		if (uap->ctl) {
			if (copyin((caddr_t)uap->ctl, (caddr_t)&i5_msg,
					sizeof(struct irix5_strbuf)))
				return (EFAULT);
			msgctl.maxlen = i5_msg.maxlen;
			msgctl.len = i5_msg.len;
			msgctl.buf = (char *)(__psunsigned_t)(i5_msg.buf);
		}

		if (uap->data) {
			if (copyin((caddr_t)uap->data, (caddr_t)&i5_msg,
		    			sizeof(struct irix5_strbuf)))
				return (EFAULT);
			msgdata.maxlen = i5_msg.maxlen;
			msgdata.len = i5_msg.len;
			msgdata.buf = (char *)(__psunsigned_t)i5_msg.buf;
		}
	} else
#endif
	{
		if (uap->ctl && copyin((caddr_t)uap->ctl, (caddr_t)&msgctl,
		    sizeof(struct strbuf)))
			return (EFAULT);
		if (uap->data && copyin((caddr_t)uap->data, (caddr_t)&msgdata,
		    sizeof(struct strbuf)))
			return (EFAULT);
	}
	if (mode == FREAD) {
		if (!uap->ctl)
			msgctl.maxlen = -1;
		if (!uap->data)
			msgdata.maxlen = -1;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))
		VOP_STRGETMSG(vp, &msgctl, &msgdata,
					prip, flagsp, fp->vf_flag, rvp, error);
#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */
		error = strgetmsg(vp, &msgctl, &msgdata,
					  prip, flagsp, fp->vf_flag, rvp);
#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

		if (error)
			return error;

#if _MIPS_SIM == _ABI64
		if (!ABI_IS_IRIX5_64(get_current_abi())) {
			if (uap->ctl) {
				i5_msg.maxlen = msgctl.maxlen;
				i5_msg.len = msgctl.len;
				i5_msg.buf =
				    (app32_ptr_t)(__psunsigned_t)msgctl.buf;
				if (copyout((caddr_t)&i5_msg, (caddr_t)uap->ctl,
						sizeof(struct irix5_strbuf)))
					return (EFAULT);
			}
			if (uap->data) {
				i5_msg.maxlen = msgdata.maxlen;
				i5_msg.len = msgdata.len;
				i5_msg.buf =
				    (app32_ptr_t)(__psunsigned_t)msgdata.buf;
				if (copyout((caddr_t)&i5_msg, 
					    (caddr_t)uap->data,
					    sizeof(struct irix5_strbuf)))
					return (EFAULT);
			}
		} else
#endif
		if ((uap->ctl && copyout((caddr_t)&msgctl, (caddr_t)uap->ctl,
		    sizeof(struct strbuf))) || (uap->data &&
		    copyout((caddr_t)&msgdata, (caddr_t)uap->data,
		    sizeof(struct strbuf))))
			return (EFAULT);
		return (0);
	}

	/*
	 * FWRITE case 
	 */
	if (!uap->ctl)
		msgctl.len = -1;
	if (!uap->data)
		msgdata.len = -1;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))
	VOP_STRPUTMSG(vp, &msgctl, &msgdata,
				*prip, *flagsp, fp->vf_flag, error);
#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */
	error = strputmsg(vp, &msgctl, &msgdata,
					*prip, *flagsp, fp->vf_flag);
#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	return error;
}

