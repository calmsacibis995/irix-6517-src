/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.52 $"

/*
 * Implementation of a subset of listio. Perform synchronous IOs to 
 * a list of devices. The interface is selected to minimize the overhead
 * of copying in/out entries in the IO list one at a time .
 */


#if defined(IP19) || defined(IP25) || defined(IP27)    /* Supported on big servers only */

#include <limits.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kfcntl.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/filio.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/locking.h>
#include <sys/mode.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/ksignal.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/kuio.h>
#include <sys/unistd.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/runq.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <fifofs/fifonode.h>		/* for fifo_rdchk prototype */
#include <sys/flock.h>
#include <sys/ktime.h>
#include <sys/ktypes.h>
#include <sys/capability.h>
#include <sys/uthread.h>
#include <sys/lio.h>
#include <sys/atomic_ops.h>
#include <string.h>
#include <sys/xlate.h>
#include <sys/var.h>
#include <sys/dbacc.h>
#include <sys/nodepda.h>

#if _MIPS_SIM == _ABI64
struct irix5_parg {		/* 32bit application calls klistio32 */
        app32_int_t       fdes;
        app32_ptr_t       cbuf;
        app32_uint_t      count;
        app32_long_t      offset;
        app32_int_t       sbase;	/* like lseek whence */
        app32_int_t       rw;		/* FREAD or FWRITE */
        app32_int_t       rval;
        app32_int_t       err;
        irix5_timespec_t startq;
        irix5_timespec_t startwait;
        irix5_timespec_t endwait;
        irix5_timespec_t endphysio;
        irix5_timespec_t endstat;
        dev_t   	  rdev;
};

struct irix5_n32_parg {		/* n32bit binary calls klistio32 */
	app32_int_t        fdes;
	app32_ptr_t        cbuf;
	app32_uint_t       count;
	app32_long_long_t  offset;
	app32_int_t        sbase; /* like lseek whence */
	app32_int_t        rw;	/* FREAD or FWRITE */
	app32_int_t        rval;
	app32_int_t	   err;
	irix5_timespec_t   startq;
	irix5_timespec_t   startwait;
	irix5_timespec_t   endwait;
	irix5_timespec_t   endphysio;
	irix5_timespec_t   endstat;
	dev_t   	   rdev;
};

struct irix5_64_parg {		/* 64bit binary calls klistio32 */
	app64_int_t	    fdes;
	app64_ptr_t	    cbuf;
	app64_uint_t	    count;
	app64_long_t	    offset;
	app64_int_t	    sbase; /* like lseek whence */
	app64_int_t	    rw;	/* FREAD or FWRITE */
	app64_int_t	    rval;
	app64_int_t	    err;
	irix5_64_timespec_t startq;
	irix5_64_timespec_t startwait;
	irix5_64_timespec_t endwait;
	irix5_64_timespec_t endphysio;
	irix5_64_timespec_t endstat;
	dev_t		    rdev;
};

static int irix5_to_parg(enum xlate_mode, void *, int, xlate_info_t *);
static int parg_to_irix5(void *, int, xlate_info_t *);
static int irix5_64_to_parg(void *, parg_t *, int);
static int parg_to_irix5_64(parg_t *, void *, int);
#endif

static int _kpread(parg_t *uap, puio_t *puio);
static int _kpwrite(parg_t *uap, puio_t *puio);
static int klistio(usysarg_t, parg_t *, puio_t *, kl_timings_t *, timespec_t *);

#define KL_HIGHWATER ((v.v_buf >> 1) + (v.v_buf >> 2))	/* 75% */
static int klistio_inuse = 0;
lock_t kl_inuse_lock;

/*#define KL_DEBUG*/
#ifdef KL_DEBUG
int kl_dbg_print = 0;
int kl_in_klistio = 0;
#endif
#ifdef KL_TIMING_TEST
static int timediff(timespec_t *t1, timespec_t *t2);
int tdiff_pio = 0x7fffffff;
int time_pio = 0;
#endif

int
klistio_init(void) {
	spinlock_init(&kl_inuse_lock, "klistio");
	return 0;
}

int
klistio_installed(void)
{
	return SGI_DBACF_PPHYSIO32 | SGI_DBACF_PPHYSIO64;
}

void
klistio_getparams(struct dba_conf *dbc)
{
	dbc->dbcf_pphysio_max = KL_HIGHWATER;
}

/* ARGSUSED */
int 
klistio32(usysarg_t nents, void *physioarg, sysarg_t arg, rval_t *rvp)
{
	parg_t		*parg;
	puio_t		*puio;
	kl_timings_t	*timing_block = 0;
	timespec_t	startio;
	int		err;
	size_t		bufsz;

	if (nents == 0)
		return 0;

#ifdef KL_TIMING_TEST
	if (time_pio) {
		nanotime((timespec_t *)&startio);
		timing_block = kmem_alloc(nents * sizeof(kl_timings_t), KM_SLEEP);
	}
#endif
	bufsz = (size_t) (nents * sizeof(parg_t));

	parg = kmem_alloc(bufsz, KM_SLEEP);
	puio = kmem_alloc(nents * sizeof(puio_t), KM_SLEEP);

	if (ABI_IS_IRIX5_64(get_current_abi())) {

		if (irix5_64_to_parg(physioarg, parg, nents)) {
			err = EFAULT;
			goto out_32;
		}

		err = klistio(nents, parg, puio, timing_block, &startio);


		if (parg_to_irix5_64(parg, physioarg, nents)) { 
			err = EFAULT;
			goto out_32;
		}

	} else {
		if (COPYIN_XLATE((void *)physioarg, (void *)parg, bufsz, 
				irix5_to_parg, get_current_abi(), nents)) {
			err = EFAULT;
			goto out_32;
		}

		err = klistio(nents, parg, puio, timing_block, &startio);

		if (XLATE_COPYOUT((void *)parg, (void *)physioarg, bufsz,
				parg_to_irix5, get_current_abi(), nents)) {
			err = EFAULT;
			goto out_32;
		}
	}
 out_32:
	kmem_free(parg, 0);
	kmem_free(puio, 0);
#ifdef KL_TIMING_TEST
	if (time_pio) {
		kmem_free(timing_block, 0);
	}
#endif
	return err;
}

/* ARGSUSED */
int 
klistio64(usysarg_t nents, void *physioarg, sysarg_t arg, rval_t *rvp)
{
	parg_t		*parg;
	puio_t  	*puio;
	kl_timings_t	*timing_block = 0;
	timespec_t	startio;
	int		err;

	if (nents == 0)
		return 0;

#ifdef KL_TIMING_TEST
	if (time_pio) {
		nanotime((timespec_t *)&startio);
		timing_block = kmem_alloc(nents * sizeof(kl_timings_t), KM_SLEEP);
	}
#endif
	/* user-space parg64_t is same size/layout as K64 parg_t - no xlate needed */
        parg = kmem_alloc(nents * sizeof(parg_t), KM_SLEEP);
        puio = kmem_alloc(nents * sizeof(puio_t), KM_SLEEP);
        if (copyin((caddr_t)physioarg, parg, nents * sizeof(parg_t))) {
		err = EFAULT;
		goto out_64;
        }

	err = klistio(nents, parg, puio, timing_block, &startio);

        if (copyout(parg, (caddr_t)physioarg, nents * sizeof(parg_t))) {
		err = EFAULT;
		goto out_64;
        }

 out_64:
	kmem_free(parg, 0);
	kmem_free(puio, 0);
#ifdef KL_TIMING_TEST
	if (time_pio) {
		kmem_free(timing_block, 0);
	}
#endif
	return err;
}

/*ARGSUSED*/
int
klistio(usysarg_t nents, parg_t *parg, puio_t *puio,
	kl_timings_t *timing_block, timespec_t *startio)
{
	int i, s;
	int err = 0;
	struct uio *uio;
	struct iovec *iovp;
	parg_t *argp;
	struct vfile *fp;
	struct buf *bp;
	struct vnode *vp;
	uthread_t *curut = curuthread;
#ifdef KL_TIMING_TEST
	timespec_t endio, endq;
	int d;
#endif

	/*
	 * An over-zealous DB could request more parallel i/o ops than the system
	 * can handle (nbuf/v.v_buf). Politely refuse such requests; ENOBUFS is
	 * usually used for "out of mbufs" but it works here. Plus, the disk driver
	 * will never return it, making it easy for DBs to tell what the true
	 * problem is.
	 */
	s = mutex_spinlock(&kl_inuse_lock);
	if (nents >= KL_HIGHWATER) {
		mutex_spinunlock(&kl_inuse_lock, s);
		return ENOBUFS;
	}
	if (nents + klistio_inuse >= KL_HIGHWATER) {
		mutex_spinunlock(&kl_inuse_lock, s);
		return EAGAIN;
	}
	klistio_inuse += nents;
	mutex_spinunlock(&kl_inuse_lock, s);

#ifdef KL_DEBUG
	kl_dbg_print = ((parg[0].rdev == 0xdef04321) ? 1 : 0);
	if (kl_dbg_print)
		printf("pphysio: nents: %d\n", nents);
	kl_in_klistio = 1;
#endif
	for (i = 0; i < nents; i++) {
		/*
		 * setjmp?
		 */
		argp = &parg[i];
		uio = &puio[i].uio;
		argp->err = 0;
		uio->uio_pio = KLISTIO;
		uio->uio_pbuf = 0;
#ifdef KL_TIMING_TEST
		if (time_pio) {
			argp->timings = &timing_block[i];
			nanotime(&argp->timings->startq);
		}
#endif
		switch (argp->rw) {
		case FREAD:
			argp->err = _kpread(argp, &puio[i]);
			break;
		case FWRITE:
			argp->err = _kpwrite(argp, &puio[i]);
			break;
		default:
			argp->err = EINVAL;
			break;
		}
#ifdef KL_TIMING_TEST
		if (time_pio) {
			nanotime(&endq);
			if ((d = timediff(&endq, &argp->timings->startq)) > tdiff_pio)
				printf("pid %d time1 %d\n", current_pid(), d);
		}
#endif
		if (argp->err && err == 0) {
			err = argp->err;
		}
	}

	for (i = 0; i < nents; i++) {
		argp = &parg[i];
		uio = &puio[i].uio;
#ifdef KL_DEBUG
		kl_dbg_print = ((parg[0].rdev == 0xdef04321) ? 1 : 0);
#endif
		if (argp->err == 0 && uio->uio_pio) {
			iovp = &puio[i].iov;
			fp = puio[i].fp;
			bp = uio->uio_pbuf;
			ASSERT(bp);

			/* wait for completion */
#ifdef KL_DEBUG
			if (kl_dbg_print)
				printf("pphysio: wait for complete entry: %d\n", i);
#endif
#ifdef KL_TIMING_TEST
			if (time_pio)
				nanotime(&argp->timings->startwait);
#endif
			psema(&bp->b_iodonesema, PRIBIO|TIMER_SHIFT(AS_PHYSIO_WAIT));
#ifdef KL_TIMING_TEST
			if (time_pio) {
				nanotime(&argp->timings->endwait);
				if ((d = timediff(&argp->timings->endwait, &argp->timings->startwait)) > tdiff_pio)
					printf("pid %d time2 %d\n", current_pid(), d);
			}
#endif
#ifdef KL_DEBUG
			if (kl_dbg_print)
				printf("pphysio: wait entry%d done\n", i);
#endif

			iounmap(bp);
			undma(iovp->iov_base, iovp->iov_len, argp->rw == FREAD ? B_READ : B_WRITE);
			argp->err = geterror(bp);
			if (argp->err && err == 0)
				err = argp->err;
			argp->rval = argp->count - bp->b_resid;
#ifdef KL_DEBUG
			if (kl_dbg_print)
				if (argp->rval == 0)
					printf("physio return 0. uio_resid %d, bp->b_resid %d\n", argp->count, bp->b_resid);
#endif
			freerbuf(bp);
			atomicAddInt(&klistio_inuse, -1);

			SUB_SYSWAIT(physio);
#ifdef KL_TIMING_TEST
			if (time_pio)
				nanotime(&argp->timings->endphysio);
#endif
			vp = VF_TO_VNODE(fp);
			if (argp->count)
				VFILE_SETOFFSET(fp, uio->uio_offset);

			/* update statistics */
			UPDATE_IOCH(curut,((u_long)argp->rval));

			if (vp->v_vfsp != NULL)
				vp->v_vfsp->vfs_bcount += argp->rval >> SCTRSHFT;
			switch (argp->rw) {
			case FWRITE:
				SYSINFO.writech += (u_long)argp->rval;
				curut->ut_acct.ua_bwrit += argp->rval;
				_SAT_FD_RDWR(argp->fdes, FWRITE, argp->err);
				break;
			case FREAD:
				SYSINFO.readch += (u_long)argp->rval;
				curut->ut_acct.ua_bread += argp->rval;
				_SAT_FD_RDWR(argp->fdes, FREAD, argp->err);
				break;
			}
#ifdef KL_TIMING_TEST
			if (time_pio)
				nanotime(&argp->timings->endstat);
#endif
		}
	}
#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("pphysio returns\n");
	kl_in_klistio = 0;
#endif
#ifdef KL_TIMING_TEST
	if (time_pio) {
		nanotime(&endio);
		if ((d = timediff(&endio, &startio)) > tdiff_pio)
			printf("pid %d timeA %d\n", current_pid(), d);
	}
#endif
	return err;
}

int
_kpwrite(parg_t *uap, puio_t *puio)
{
	register struct uio *uio = &puio->uio;
	register struct iovec *aiov = &puio->iov;
	int ioflag = 0;
	/*int iolock;*/
	int error = 0;
	register vnode_t	*vp;
	struct vattr vattr;
	struct vfile *fp;
	off_t offset;
#if (_MIPS_SZPTR == 64)
	int abi = get_current_abi();
#endif

	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return EINVAL;
	
	puio->fp = fp;
	if (((uio->uio_fmode = fp->vf_flag) & FWRITE) == 0) {
		_SAT_FD_RDWR(uap->fdes, FWRITE, EBADF);
		return EBADF;
	}
	SYSINFO.syswrite++;
	curuthread->ut_acct.ua_syscw++;
#if (_MIPS_SZPTR == 64)
	if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi)) {
		/* Make sure top bits of cbuf pointer are cleared */
		aiov->iov_base = (caddr_t)((u_long)uap->cbuf &~ 0xffffffff00000000LL);
	} else {
		aiov->iov_base = (caddr_t)uap->cbuf;
	}
#endif
	if ((uio->uio_resid = aiov->iov_len = uap->count) < 0)
		return EINVAL;
	uio->uio_iov = aiov;
	uio->uio_iovcnt = 1;
	vp = VF_TO_VNODE(fp);
	if (vp->v_type != VCHR)
		return ENODEV;

	/* uap->rdev = vp->v_rdev; */

	/* 
	 * Do lseek
	 */
	VFILE_GETOFFSET(fp, &offset);
	if (uap->sbase == 1)
		uap->offset += offset;
	else if (uap->sbase == 2) {
		vattr.va_mask = AT_SIZE;
		VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error) 
			return EINVAL;
		uap->offset += vattr.va_size;
	} else if (uap->sbase != 0) {
		sigtopid(current_pid(), SIGSYS, SIG_ISKERN, 0, 0, 0);
		return EINVAL;
	}
	/* offset is 64bit in kernel internal struct now */
	VOP_SEEK(vp, offset, &uap->offset, error);
	if (!error) {
		VFILE_SETOFFSET(fp, uap->offset);
	} else
		return error;

	/*
	 * Do IO
	 */
	uio->uio_offset = uap->offset;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_limit = getfsizelimit();
	uio->uio_pmp = NULL;
	uio->uio_readiolog = 0;
	uio->uio_writeiolog = 0;
	uio->uio_sigpipe = 0;

	VOP_WRITE(vp, uio, ioflag, fp->vf_cred, &curuthread->ut_flid, error);
#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpwrite: VOP_WRITE returns %d\n", error);
#endif
	ASSERT(uio->uio_sigpipe == 0);
	if (error == EINTR && uio->uio_resid != uap->count)
		error = 0;
	return error;
}

int
_kpread(parg_t *uap, puio_t *puio)
{
	register struct uio *uio = &puio->uio;
	register struct iovec *aiov = &puio->iov;
	int ioflag = 0;
	/*int iolock;*/
	int error = 0;
	register vnode_t	*vp;
	struct vattr vattr;
	struct vfile *fp;
	off_t offset;
#if (_MIPS_SZPTR == 64)
	int abi = get_current_abi();
#endif

	if (error = getf((int)uap->fdes, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return EINVAL;

	puio->fp = fp;
	if (((uio->uio_fmode = fp->vf_flag) & FREAD) == 0) {
		_SAT_FD_RDWR(uap->fdes, FREAD, EBADF);
		return EBADF;
	}
	SYSINFO.sysread++;
	curuthread->ut_acct.ua_syscr++;
#if (_MIPS_SZPTR == 64)
	if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi)) {
		/* Make sure top bits of cbuf pointer are cleared */
		aiov->iov_base = (caddr_t)((u_long)uap->cbuf &~ 0xffffffff00000000LL);
	} else {
		aiov->iov_base = (caddr_t)uap->cbuf;
	}
#endif
	if ((uio->uio_resid = aiov->iov_len = uap->count) < 0)
		return EINVAL;
	uio->uio_iov = aiov;
	uio->uio_iovcnt = 1;
	vp = VF_TO_VNODE(fp);
	if (vp->v_type != VCHR)
		return ENODEV;

#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpread: fd: %d, base: %x, count: %d, offset: %x, sbase: %d\n",
		       uap->fdes, uap->cbuf, uap->count, uap->offset, uap->sbase);
#endif
	/* uap->rdev = vp->v_rdev; */

	/* 
	 * Do lseek
	 */
	VFILE_GETOFFSET(fp, &offset);
	if (uap->sbase == 1)
		uap->offset += offset;
	else if (uap->sbase == 2) {
		vattr.va_mask = AT_SIZE;
		VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error)
			return EINVAL;
		uap->offset += vattr.va_size;
	} else if (uap->sbase != 0) {
		sigtopid(current_pid(), SIGSYS, SIG_ISKERN, 0, 0, 0);
		return EINVAL;
	}
#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpread: offset before VOP_SEEK is 0x%x / 0x%x\n",
		       uap->offset, offset);
#endif
	VOP_SEEK(vp, offset, &uap->offset, error);
	if (!error) {
		VFILE_SETOFFSET(fp, uap->offset);
	} else
		return error;

#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpread: offset after VOP_SEEK is 0x%x / 0x%x\n",
		       uap->offset, offset);
#endif
	/*
	 * Do IO
	 */
	uio->uio_offset = uap->offset;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_limit = getfsizelimit();
	uio->uio_pmp = NULL;
	uio->uio_readiolog = 0;
	uio->uio_writeiolog = 0;

#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpread: initiate read to vnode %x, ioflag %x, off 0x%x\n",
		       vp, ioflag, uio->uio_offset);
#endif
	VOP_READ(vp, uio, ioflag, fp->vf_cred, &curuthread->ut_flid, error);
#ifdef KL_DEBUG
	if (kl_dbg_print)
		printf("_kpread: VOP READ returns %d\n", error);
#endif
	if (error == EINTR && uio->uio_resid != uap->count)
		error = 0;
	return error;
}

#ifdef KL_TIMING_TEST
/*
 * Return difference in millsec */
timediff(
	timespec_t *t2,
	timespec_t *t1)
{
        return ((t2->tv_sec - t1->tv_sec) * 1000);
}
#endif

#if _MIPS_SIM == _ABI64

/*
 * Map the kernel parg struct to user, before copyout().
 * Support o32 and n32 structs.
 */
static int 
parg_to_irix5(void *from, int count, register xlate_info_t *info)
{

	if (ABI_IS_IRIX5_N32(get_current_abi())) {
		int dstsz = count * sizeof(struct irix5_n32_parg);

		XLATE_COPYOUT_VARYING_PROLOGUE(parg, irix5_n32_parg, dstsz);

		for ( ; count--; target++, source++) {
			target->fdes = source->fdes;
			target->cbuf =(app32_ptr_t)(__psunsigned_t)source->cbuf;
			target->count = source->count;
			target->offset = source->offset;
			target->sbase = source->sbase;
			target->rw = source->rw;
			target->rval = source->rval;
			target->err = source->err;
			/* rdev is not used */
		}
	} else {
		int dstsz = count * sizeof(struct irix5_parg);

		XLATE_COPYOUT_VARYING_PROLOGUE(parg, irix5_parg, dstsz);

		for ( ; count--; target++, source++) {
			target->fdes = source->fdes;
			target->cbuf =(app32_ptr_t)(__psunsigned_t)source->cbuf;
			target->count = source->count;
			target->offset = source->offset;
			target->sbase = source->sbase;
			target->rw = source->rw;
			target->rval = source->rval;
			target->err = source->err;
			/* rdev is not used */
		}
	}
	return 0;
}

/*
 * copyin() the user parg struct, then map to the kernel parg.
 * Support o32 and n32 structs.
 */
static int 
irix5_to_parg(enum xlate_mode mode, void *to, int count, xlate_info_t *info)
{
	if (ABI_IS_IRIX5_N32(get_current_abi())) {
		int srcsz = count * sizeof(struct irix5_n32_parg);

		COPYIN_XLATE_VARYING_PROLOGUE(irix5_n32_parg, parg, srcsz);

		for ( ; count--; target++, source++) {
			target->fdes = source->fdes;
			target->cbuf = (void *)(__psunsigned_t)source->cbuf;
			target->count = source->count;
			target->offset = source->offset;
			target->sbase = source->sbase;
			target->rw = source->rw;
			/* timespec_t values have never been used for input */
			/* rdev is not normally used */
#ifdef KL_DEBUG
			target->rdev = source->rdev;
#endif
		}

	} else {
		int srcsz = count * sizeof(struct irix5_parg);

		COPYIN_XLATE_VARYING_PROLOGUE(irix5_parg, parg, srcsz);

		for ( ; count--; target++, source++) {
			target->fdes = source->fdes;
			target->cbuf = (void *)(__psunsigned_t)source->cbuf;
			target->count = source->count;
			target->offset = source->offset;
			target->sbase = source->sbase;
			target->rw = source->rw;
			/* timespec_t values have never been used for input */
			/* rdev is not normally used */
#ifdef KL_DEBUG
			target->rdev = source->rdev;
#endif
		}
	}
	return 0;
}

/*
 * Map the kernel parg struct to user, before copyout().
 * Support 64bit binary issuing klistio32().
 */
static int
parg_to_irix5_64(parg_t *source, void *user_parg, int count)
{
	void *buf;
	int err = 0;
	struct irix5_64_parg *target;
	size_t sz = count * sizeof(struct irix5_64_parg);

        buf = kmem_alloc(sz, KM_SLEEP);
	target = (struct irix5_64_parg *) buf;

	for ( ; count--; target++, source++) {
		target->fdes = source->fdes;
		target->cbuf =(app32_ptr_t)(__psunsigned_t)source->cbuf;
		target->count = source->count;
		target->offset = source->offset;
		target->sbase = source->sbase;
		target->rw = source->rw;
		target->rval = source->rval;
		target->err = source->err;
		/* rdev is not used */
	}
	if (copyout(buf, user_parg, (int) sz))
		err = EFAULT;

	kmem_free(buf, 0);
	return (err);
}

/*
 * copyin() the user parg struct, then map to the kernel parg.
 * Support 64bit binary issuing klisio32().
 */
static int
irix5_64_to_parg(void *user_parg, parg_t *target, int count)
{
	void *buf;
	int err = 0;
	struct irix5_64_parg *source;
	size_t sz = count * sizeof(struct irix5_64_parg);

        buf = kmem_alloc(sz, KM_SLEEP);
	source = (struct irix5_64_parg *) buf;

        if (copyin(user_parg, buf, (int) sz)) {
		err = EFAULT;
		goto out;
        }
	for ( ; count--; target++, source++) {
		target->fdes = source->fdes;
		target->cbuf = (void *)(__psunsigned_t)source->cbuf;
		target->count = source->count;
		target->offset = source->offset;
		target->sbase = source->sbase;
		target->rw = source->rw;
		/* timespec_t values have never been used for input */
		/* rdev is not normally used */
#ifdef KL_DEBUG
		target->rdev = source->rdev;
#endif
	}
 out:
	kmem_free(buf, 0);
	return(err);
}

#endif /* MIPS_SIM == _ABI64 */

#endif /*IP19 || IP25 || IP27*/
