/*
 *	/dev/scsi generic driver(for raw device access only)
 *
 *	Provides ioctl access by user-mode programs to arbitrary SCSI
 *	devices.  
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek(Vulcan Laboratory)and
 *	Rich Morin(Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *	Ported to SGI by Dave Olson, 3/89
 */

#ident "io/dsreq.c: $Revision: 1.68 $"


/* stuff to support other include files */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/buf.h>		/* for B_READ  */
#include <ksys/vfile.h>		/* for FREAD   */
#include <sys/fcntl.h>
#include <sys/scsi.h>
#include <sys/dsreq.h>		/* devscsi include files */
#include <sys/dsglue.h>
#include <sys/conf.h>
#include <sys/immu.h>
#include <sys/debug.h>
#include <sys/scsi.h>
#include <sys/capability.h>
#include <sys/var.h>
#include <sys/invent.h>
#include <sys/iograph.h>
#ifdef MH_R10000_SPECULATION_WAR
#include <sys/pfdat.h>
#endif /* MH_R10000_SPECULATION_WAR */

int dsdevflag = D_MP;
int dsdebug = 0; 

/*
 * Definitions and externs used for cacheline fixup stuff used when
 * doing DMA into user buffers.
 */
/* Bits we should turn on to make the virtual address kernel addressable */
#define GOOD    (PG_G | PG_M | PG_SV | PG_VR)
#ifdef MH_R10000_SPECULATION_WAR
#define       GOOD_R10000     (PG_G | PG_M | PG_SV)
#endif /* MH_R10000_SPECULATION_WAR */
extern int cachewrback;
#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif

#ifdef MH_R10000_SPECULATION_WAR
extern caddr_t _maputokv(caddr_t, size_t, int);
#endif /* MH_R10000_SPECULATION_WAR */

#ifdef DEBUG
#define CFREE(x)  ds_cfree(x)
#else
#define CFREE(x) _CFREE(x)
#define ds_copyin   copyin
#define ds_copyout  copyout
#endif

extern int ds_report_cmdlen_err;

/* function prototypes */
static struct task_desc *task_find(dev_t);
static int  task_init_desc(struct task_desc *, dev_t);	/* allocate storage */
static int  task_glue(char *, struct dsreq *, struct task_desc *);
static void task_free_desc( dev_t);
static int dma_lock(struct dsiovec *, long, int);
static caddr_t ds_calloc(size_t);
int task_unglue(struct task_desc *tp, struct dsreq *dp, char *arg);

#if _MIPS_SIM == _ABI64
static void irix5_dsreq_to_dsreq(struct irix5_dsreq *, struct dsreq *);
static void dsreq_to_irix5_dsreq(struct dsreq *, struct irix5_dsreq *);
static void irix5_iovecs_to_dsiovecs(struct irix5_dsiovec *, dsiovec_t *, int);
#endif /* _ABI64 */

#ifdef DEBUG
int ds_copyin(caddr_t, caddr_t, int);
int ds_copyout(caddr_t, caddr_t, int);
void ds_cfree(caddr_t);
#endif


/* ARGSUSED */
static int
cpin_ds(char *arg, struct dsreq  *dp, struct task_desc *tp)
{
	int error = 0;
	
#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(get_current_abi())) {
		struct irix5_dsreq i5_dsreq;

		error = ds_copyin(arg, (caddr_t)&i5_dsreq, sizeof(i5_dsreq));
		if (!error)
			irix5_dsreq_to_dsreq(&i5_dsreq, dp);
	}
	else
#endif /* _ABI64 */
		error = ds_copyin((caddr_t)arg, (caddr_t)dp, sizeof *dp);

	if (error)
		return EFAULT;
	return 0;
}

/* ARGSUSED */
static int
cpout_ds(char *arg, struct dsreq  *dp, struct task_desc *tp)
{
	int error = 0;
#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(get_current_abi())) {
		struct irix5_dsreq i5_dsreq;

		dsreq_to_irix5_dsreq(dp,&i5_dsreq);
		error = ds_copyout((caddr_t)&i5_dsreq, arg, sizeof(i5_dsreq));
	} else
#endif /* _ABI64 */
		error = ds_copyout((caddr_t)dp, arg, sizeof (*dp));

	if (error)
		return EFAULT;
	return 0;
}


int
dsinit()
{
	return 0;
}


/*
 *	open - open exclusive devscsi task descriptor
 */
/* ARGSUSED */
int
dsopen(dev_t *devp, int flag, int otyp, struct cred *crp)
{
	scsi_dev_info_t		*sdevi;
	mutex_t			*sdevi_lock;
	struct task_desc	*tp;
	int			error;

	DBG(printf("dsopen(%x,%x) id %d. ", *devp, flag, get_id(*devp)));
	sdevi = scsi_dev_info_get(*devp);
	sdevi_lock = &SDEVI_TDSLOCK(sdevi);
	mutex_lock(sdevi_lock, PRIBIO);
	tp = task_find(*devp);
	mutex_unlock(sdevi_lock);

	if ( !tp )
		return ENXIO;

	/* always call init_desc; there was a bug where it was done only
	 * on state TDS_START, which meant multiple opens could occur
	 * and really mess things up.  The alternative is to provide
	 * semaphoring at dsioctl, so only one request is active at
	 * a time.  The original intent was that this driver be an
	 * exclusive use driver, so we continue to  check only at open.
	 * note that this still doesn't correctly handle the case of
	 * programs which fork after opening.  The tpsc driver has
	 * a similar problem, but changing it would break programs
	 * like bstream.  This whole routine really should be
	 * semaphored, but I simply don't want to spend the time
	 * on it for 5.1.1; now that we are worrying about things
	 * like O_EXCL and putting them in iflag, this is even
	 * more so...
	 */
	if(flag & O_EXCL)
		tp->td_iflags |= TASK_EXCLUSIVE;
	error = task_init_desc(tp, *devp);
	if(!error && tp->td_state != TDS_INIT)  {
		DBG(printf("open: state not INIT. "));
		mutex_lock(sdevi_lock, PRIBIO);
		task_free_desc(*devp);
		mutex_unlock(sdevi_lock);
		if(!error)
			error = EBUSY; /* already busy */
	}
	tp->td_iflags &= ~TASK_EXCLUSIVE;
	DBG(if(error)printf("dsopen errno %d\n", error));
	return (error);
}


/*
 *	close - release device structures
 */
/* ARGSUSED */
int
dsclose(dev_t dev, int flag, int otyp, struct cred *crp)
{
	scsi_dev_info_t		*sdevi;
	mutex_t			*sdevi_lock;
	struct task_desc	*tp;

	DBG(printf("dsclose(%x,%x)\n", dev, flag));
	sdevi = scsi_dev_info_get(dev);
	sdevi_lock = &SDEVI_TDSLOCK(sdevi);
	mutex_lock(sdevi_lock, PRIBIO);
	tp = task_find(dev);

	if(!tp) {
		DBG(printf("tp null in dsclose!\n"));
		mutex_unlock(sdevi_lock);
		return ENXIO;	/* no such or not opened */
	}

	task_free_desc(dev);
	mutex_unlock(sdevi_lock);
	return 0;
}

/*
 *  ioctl - devscsi request handling
 */
/*ARGSUSED3*/
int
dsioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *crp, int *rvalp)
{
	scsi_dev_info_t		*sdevi;
	mutex_t			*sdevi_lock;
	struct dsreq		dsreq;
	struct dsreq		*dp = &dsreq;
	struct task_desc	*tp;
	int			err = 0, tmp, locked = 0;

	DBG(printf("dsioctl(%x,%x,%x). ", dev, cmd, arg));
	sdevi = scsi_dev_info_get(dev);
	sdevi_lock = &SDEVI_TDSLOCK(sdevi);
	mutex_lock(sdevi_lock, PRIBIO);
	tp = task_find( dev);
	mutex_unlock(sdevi_lock);
	if(!tp) {
		DBG(printf("tp is NULL!\n"));
		return (ENXIO);
	}
	if((tp->td_flags & DSRQ_CTRL2) && _CAP_CRABLE(crp, CAP_DEVICE_MGT))
		dsdebug = 1; /* CTRL2 triggers dsdebug */

	err = 0;
	switch(cmd) {
    	 case DS_SET:			     /* Set device flags */
		DBG(printf("DS_SET. "));
		/* We don't support setting of device flags. */
		err = EINVAL;
		break;
    	 case DS_CLEAR:	     /* Clear device flags */
		DBG(printf("DS_CLEAR. "));
		/* Clearing device flags == setting to 0 (see DS_SET) */
		err = EINVAL;
		break;
    	 case DS_GET:		 /* Get device flags */
		DBG(printf("DS_GET. "));
		err = 0;
		/* note that since GET returns a value, the function must
		 * exist; the not_impl function won't do the job */
		tmp = glue_getopt(tp, 0);
		if(suword((caddr_t)arg, tmp))
			err = EFAULT;
		break;
    	 case DS_CONF:     /* Get supported device flags */
		{
		dsconf_t dsconf;
		DBG(printf("DS_CONF. "));
		err = 0;
		dsconf.dsc_flags = glue_support;
		dsconf.dsc_preset = glue_default;
		dsconf.dsc_bus = get_bus(dev);
		dsconf.dsc_imax = I_MAX;
		dsconf.dsc_lmax = L_MAX;
		dsconf.dsc_iomax = ctob(v.v_maxdmasz);
		dsconf.dsc_biomax = 0;
		if(copyout((caddr_t)&dsconf, (caddr_t)arg, sizeof(dsconf)))
			err = EFAULT;
		break;
		}
#if _MIPS_SIM == _ABI64
	case IRIX5_DS_ENTER:			     /* Enter a request	*/
#endif /* _ABI64 */
	case DS_ENTER:			     /* Enter a request	*/
	{
		/* These are used only on machines with writeback caches */
		int		count;
		caddr_t		base;
		int		first_aligned, last_aligned;
		caddr_t		safebuf_base, first_kva, last_kva;
		uint		lead_size, trail_size, trail_offset;

		DBG(printf("DS_ENTER tp%x, state%d\n",tp,tp->td_state));
		if(tp->td_state != TDS_INIT) {
			err = EBUSY;	/* already has req */
			break;
		}

		/* copyin user request and ready for gluing */
		dp = &dsreq;
		if (err = task_glue((caddr_t)arg, dp, tp))
			break;
		locked++;

		/* From os/physio.c:
		 *
		 * Some machines (such as IP17) require that DMA reads into 
		 * memory be done to buffers that are cache-aligned.  DMA can
		 * still be done to non-aligned buffers, provided that 
		 * nothing accesses the cache lines during the operation.
		 *
		 * If a user attempts a raw I/O read into cache-misaligned 
		 * buffers, we actually do the read to a kernel buffer (that 
		 * will not be touched during the operation) and then copy 
		 * the data to user space.  Since we want to avoid allocating
		 * and copying potentially large kernel buffers for raw I/O, 
		 * most of the physical pages for the operation are actually 
		 * the user's page frames.  Only the first and/or last pages 
		 * are kernel pages which require copying.
		 *
		 * This code works only for USER-misaligned buffers.  For the
		 * most part, drivers don't do DMA via physio.  When they do,
		 * they must align properly or guarantee that the cache line
		 * they are DMAing into is not accessed during the DMA.
		 */
		base = tp->td_iovbuf[0].iov_base;
		count = tp->td_iovbuf[0].iov_len;
		if ((tp->td_flags & DSRQ_READ) && cachewrback) {
#ifdef MH_R10000_SPECULATION_WAR
 		/* MH R10000 WAR requires I/O buffers be page-aligned */
 		first_aligned = IS_R10000() ?
 					poff(base) == 0 :
 					SCACHE_ALIGNED(base);
 		last_aligned = IS_R10000() ?
 					poff(base + count) == 0 :
 					SCACHE_ALIGNED(base + count);
#else /* MH_R10000_SPECULATION_WAR */
			first_aligned = SCACHE_ALIGNED(base);
			last_aligned = SCACHE_ALIGNED(base + count);
#endif /* MH_R10000_SPECULATION_WAR */
		} else {
			first_aligned = 1;
			last_aligned = 1;
		}
		if (!first_aligned || !last_aligned) {
			ASSERT(IS_KUSEG(base));
			ASSERT(cachewrback);

			/* Either the beginning or the end of the user's buffer is 
			 * misaligned.  Map the user's buffer into kernel space, so 
			 * we can refer to these pages with a kernel virtual address.
			 *
			 * For R4000 based platforms, it's much faster to let the
			 * buffer be allocated cacheable.  In fact, for EVEREST
			 * platforms it's mandatory since 128 MB of memory can't
			 * be accessed non-cached.
			 */
#ifdef MH_R10000_SPECULATION_WAR
			safebuf_base = _maputokv(base, count, pte_cachebits() | (
						IS_R10000() ? GOOD_R10000 :
						GOOD));
#else
			safebuf_base = maputokv(base, count, pte_cachebits() | GOOD);
#endif /* MH_R10000_SPECULATION_WAR */

			if (safebuf_base == NULL) {
				err = EAGAIN;
				break;	/* cleanup done below after end of switch */
			}

			/* Don't try to handle alignment on both ends of the buffer
			 * if the whole thing fits on 1 page.
			 */
			if (numpages(base, count) == 1) {
				first_aligned = 0;
				last_aligned = 1;
			}

			if (!first_aligned) {
				register pde_t	*pde;

				/* Replace the first page of the kernel buffer
				 * with a newly-allocated page.
				 */
				pde = kvtokptbl(safebuf_base);
				/* Any physical page is OK. Won't use VA. */
				first_kva =
#ifdef _VCE_AVOIDANCE
					    vce_avoidance ?
					    kvpalloc(1, VM_VACOLOR,
							colorof(safebuf_base)) :
#endif /* _VCE_AVOIDANCE */
					    kvpalloc(1, 0, 0);
#ifdef MH_R10000_SPECULATION_WAR
                                if (IS_R10000()) {
                                        __psint_t vfn = vatovfn(safebuf_base);

                                        krpf_remove_reference(pdetopfdat(pde),vfn);
                                        krpf_add_reference(kvatopfdat(first_kva),
                                                           vfn);
                                }
#endif /* MH_R10000_SPECULATION_WAR */
				pg_setpfn(pde, kvatopfn(first_kva));
				/*
				 * Dropping in the new page table entry into
				 * the tlb ensures that there are no entries
				 * with the old pfn value in the tlb.  While
				 * we haven't referenced the address returned
				 * from maputokv(), it could be that the
				 * address would be the second half in a tlb
				 * entry and that we referenced the first
				 * half since allocating the address.  That
				 * would fault into the tlb an entry pointing
				 * to the user's page rather than our newly
				 * allocated page.
				 */
				tlbdropin(0, safebuf_base, pde->pte);
				lead_size = min(ctob(btoc(base+1)) - (__psunsigned_t)base, count);
			}
			if (!last_aligned) {
				register pde_t	*pde;

				/* Replace the last page of the kernel buffer
				 * with a newly-allocated page.
				 */
				pde = kvtokptbl(safebuf_base + count - 1);
				/* Any physical page is OK. Won't use VA. */
				last_kva =
#ifdef _VCE_AVOIDANCE
					   vce_avoidance ?
					   (caddr_t)kvpalloc(1, VM_VACOLOR,
						colorof(safebuf_base + count - 1)) :
#endif /* _VCE_AVOIDANCE */
					   (caddr_t)kvpalloc(1, 0, 0);
#ifdef MH_R10000_SPECULATION_WAR
                                if (IS_R10000()) {
                                        __psint_t vfn = vatovfn(safebuf_base + \
								count - 1);

                                        krpf_remove_reference(pdetopfdat(pde),vfn);
                                        krpf_add_reference(kvatopfdat(last_kva),                                                           vfn);
                                }
#endif /* MH_R10000_SPECULATION_WAR */
				pg_setpfn(pde, kvatopfn(last_kva));
				/*
				 * Dropping in the new page table entry into
				 * the tlb ensures that there are no entries
				 * with the old pfn value in the tlb.  While
				 * we haven't referenced the address returned
				 * from maputokv(), it could be that the end
				 * address would be the first half in a tlb
				 * entry and that we referenced the second
				 * half since allocating the address.  That
				 * would fault into the tlb an entry pointing
				 * to the user's page rather than our newly
				 * allocated page.
				 */
				tlbdropin(0, safebuf_base + count - 1,
					  pde->pte);
				trail_size = ((__psunsigned_t)base + count) & (NBPC-1);
				trail_offset = count - trail_size;
				ASSERT(((__psunsigned_t)(safebuf_base + trail_offset) \
					& (NBPC-1)) == 0);
			}
#ifdef MH_R10000_SPECULATION_WAR
			if (IS_R10000() && (tp->td_flags & DSRQ_READ))
				invalidate_range_references(safebuf_base,count,
							    CACH_DCACHE|CACH_SCACHE|
							    CACH_INVAL|CACH_IO_COHERENCY,
							    0);
#endif /* MH_R10000_SPECULATION_WAR */	
		} else
			safebuf_base = base;
		tp->td_iovbuf[0].iov_base = safebuf_base;

		/*
		 *  call glue layer to start host request
		 *  	returns 0 if request entered ok
		 *	unix errno otherwise
		 */
		if(err = glue_start(tp, tp->td_flags))
		{
			tp->td_iovbuf[0].iov_base = base;
			if (!first_aligned || !last_aligned) {
				if (!first_aligned)
					kvpfree(first_kva, 1);
				if (!last_aligned)
					kvpfree(last_kva, 1);
				unmaputokv(safebuf_base, count);
			}
			break;
		}

#ifdef notdef
		/*
		 * return to user immediately if async, otherwise sleep
		 * XXX need to deal with cache alignment if ASYNC implemented.
		 */
		/* DRONEK (Need additional ioctl for buffer completion.) */
		if ( tp->td_flags & DSRQ_ASYNC )  {
			/* async, need poll ioctl for status */
			tp->td_state = TDS_WAITASYNC;
			locked = 0;	/* don't want to release the locked 
				pages below! */
			break;
		}
		else
#endif
			/* sleep till glue says done */
			psema(&tp->td_done, PRIBIO);

		/*
		 *  call glue layer to get results,
		 *  actual data length, return code, etc.
		 */
		glue_return(tp, tp->td_flags);

		tp->td_iovbuf[0].iov_base = base;
	
		locked--;
		/*  copyback devscsi returns to user */
		err = task_unglue(tp, dp, (caddr_t)arg);

		if (!first_aligned || !last_aligned) {
			if (!first_aligned) {
				lead_size = min(lead_size, tp->td_datalen);
				copyout(safebuf_base, base, lead_size);
				kvpfree(first_kva, 1);
			}

			if (!last_aligned) {
				trail_size = min(trail_size,
					tp->td_datalen - trail_offset);
				copyout(safebuf_base + trail_offset, 
					base + trail_offset, trail_size);
				kvpfree(last_kva, 1);
			}

			unmaputokv(safebuf_base, count);
		}

		/* mark task slot empty (even if unglue error) */
		tp->td_state = TDS_INIT;
		break;
	}

	case DS_ABORT:
		if(err=cpin_ds((caddr_t)arg, dp, tp))
			break;
		tp->td_iflags |= TASK_ABORT;
		/* this operation is synchronous; there is no glue_return */
		(void) glue_start(tp, 0);
		tp->td_iflags &= ~TASK_ABORT;
		RET(dp)	= tp->td_ret;
		STATUS(dp) = tp->td_status;
		err = cpout_ds((caddr_t)arg, dp, tp);
		break;
	case DS_MKALIAS:
	{
		dev_t ctl, hwscsi, disk, lun;
		int rc;
		char loc_str[MAXDEVNAME];
		scsi_dev_info_t *sdinfo;
		int contr;
		void dk_alias_make(vertex_hdl_t);

		if ((hwscsi = hwgraph_path_to_dev("/hw/" EDGE_LBL_SCSI)) == NODEV) {
			rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI, &hwscsi);
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		}


		sdinfo = scsi_dev_info_get(dev);
		ctl = SLI_CTLR_VHDL(SDEVI_LUN_INFO(sdinfo));
		contr = device_controller_num_get(ctl);
		device_controller_num_set(dev,contr);

		/*
		 * Non-fabric devices will have a target node value of 0.
		 * Fabric devices will be non-zero, and we need to make
		 * different shortcut devices.
		 */
		if (SDEVI_NODE(sdinfo) == 0) {
			sprintf(loc_str,"sc%dd%dl%d",
				contr,
				SDEVI_TARG(sdinfo),
				SDEVI_LUN(sdinfo));
			if (hwgraph_traverse(hwscsi,
					     loc_str,
					     &ctl) == GRAPH_NOT_FOUND) {
				rc = hwgraph_edge_add(hwscsi, dev, loc_str);
				ASSERT(rc == GRAPH_SUCCESS); rc = rc;
			} else
				hwgraph_vertex_unref(ctl);
		}
		else {
			uint64_t node, port;
			node = SDEVI_NODE(sdinfo);
			port = SDEVI_PORT(sdinfo);
			sprintf(loc_str, "%llx/lun%d", node, SDEVI_LUN(sdinfo));
			rc = hwgraph_path_add(hwscsi, loc_str, &ctl);
			ASSERT(rc == GRAPH_SUCCESS);
			sprintf(loc_str, "c%dp%x", contr, port);
			hwgraph_edge_add(ctl, dev, loc_str);
			hwgraph_vertex_unref(ctl);
		}

		/*
		 * If this is for a disk then we need to also update
		 * the disk to have the same controller number.  We have to
		 * update here in case the disk cannot be opened (removeable
		 * media disk without media).
		 */
		if (hwgraph_traverse(dev, "../"EDGE_LBL_DISK, &disk) == GRAPH_SUCCESS) {
			dev_t	vol;
			device_controller_num_set(disk, contr);
			/* For RAID disks */
			if (hwgraph_traverse(disk,
					     EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR,	
					     &vol) == GRAPH_SUCCESS) {
				dk_alias_make(vol);
				hwgraph_vertex_unref(vol);
			}
			hwgraph_vertex_unref(disk);
		}

		/*
		 * Unref base /hw/scsi alias directory.
		 */
		hwgraph_vertex_unref(hwscsi);

		/*
		 * This is for any Raid or other strange devices that
		 * might have an invent hanging off the lun
		 */
		lun = hwgraph_connectpt_get(dev);
		device_controller_num_set(lun, contr);
		hwgraph_vertex_unref(lun);

		break;
	}
			
	default:
		err = EINVAL;	/* an unsupported ioctl */
	}

	/*
	 * If an error occured after task_glue, but before task_unglue,
	 * we need to unlock the pages here.
	 */
	if(locked)
	{
		dma_unlock(tp->td_iovbuf, tp->td_iovlen/sizeof(dsiovec_t),
			(tp->td_flags & DSRQ_READ) ? B_READ : B_WRITE);
		/* reset the state to INIT so next cmd will work */
		tp->td_state = TDS_INIT;
	}
	if((tp->td_flags & DSRQ_CTRL2) && _CAP_CRABLE(crp, CAP_DEVICE_MGT))
		dsdebug = 0;

	DBG(if(err)printf("dsioctl returns %d\n", err));
	return (err);
}


static caddr_t
ds_calloc(size_t  len)
{
	caddr_t p;

	_CALLOC(p, len);
	DBG(printf("calloc(%d)-> 0x%x. ",len,p));
	return p;
}

#ifdef DEBUG
void
ds_cfree(caddr_t x)
{
	DBG(printf("ds_cfree(%x). ",x));
	_CFREE(x);
}

ds_copyin(
	caddr_t ua,
	caddr_t k,
	int n)
{
	register err = 0;
	DBG(printf("copyin 0x%x 0x%x %d. ",ua,k,n));
	err = copyin(ua, k, n);
	DBG(if(err) printf("fails!\n"));
	return(err);
}

int
ds_copyout(
	caddr_t k,
	caddr_t ua,
	int n)
{
	register err = 0;
	DBG(printf("copyout 0x%x 0x%x %d. ",k,ua,n));
	err = copyout(k, ua, n);
	if(err) DBG(printf("fails!\n"));
	return(err);
}
#endif /* DEBUG */


int
ds_copyiov( iovs, n, kbuf, bufl, io)
	struct dsiovec *iovs;	/* base-length dma pairs */
	int              n;	/* number pairs */
	char 		*kbuf;	/* kernel buffer */
	int   		 bufl;	/* length xfer */
	int		 io;	/* 1 copyin, 2 copyout */
{
	int err = 0;
	/*
	 *  iterate over user iov-s, do copyin/copyout
	 */
	DBG(printf("ds_copyiov(0x%x,%d,0x%x,%d,%s)\n",
		iovs, n, kbuf, bufl, (io==1) ? "in" : "out"));
	while ( !err && n-- && bufl > 0)  {
		register int l;
		l = min( bufl, iovs->iov_len);

		if ( io == 1 )
			err = ds_copyin(  iovs->iov_base, kbuf, l);
		else
			err = ds_copyout( kbuf, iovs->iov_base, l);

		kbuf  += l;
		bufl -= l;
		++iovs;
	}
	return err;
}

static int
dma_lock(
	register struct dsiovec *iovs,
	long n,
	int rw)
{
	register int i;
	DBG(printf("dma_lock(0x%x,%d,%d). ", iovs, n,rw));
	for(i = 0; i < n; i++) {
		DBG(printf("locking 0x%x %d. ",
			   iovs[i].iov_base,
			   iovs[i].iov_len));
		if(iovs[i].iov_len == 0)
			break;
		/* at SGI, we use the userdma() instead useracc() since it
		 * handles some more stuff we don't want to have to worry
		 * about here.
		 *
		 * It also handles error cleanup(), and cache flushing.
		 */
		if(iovs[i].iov_len < 0 ||
		   userdma(iovs[i].iov_base, iovs[i].iov_len, rw, NULL)) {
			DBG(printf("locking fails!\n"));
			dma_unlock(iovs, i, rw);	/* unlock the ones we
							   already locked */
			return 0;		/* failure */
		}
	}
	DBG(printf("dma_lock done\n"));
	return  1;				/* success */
}


void
dma_unlock(
	register struct dsiovec *iovs,
	long n,
	int rw)
{
	DBG(printf("dma_unlock(0x%x,%d). ", iovs, n));
	while(n-- > 0){
		DBG(printf("unlocking 0x%x %d. ",iovs->iov_base,iovs->iov_len));
		if(iovs->iov_len == 0)
			break;
		undma(iovs->iov_base, iovs->iov_len, rw);
		++iovs;
	}
	DBG(printf("dma_unlock done\n"));
}



/*
 *	task support functions
 */


/*
 *  locate task pointer
 *  allocates intermediate storage as needed
 *  this should really make a call to the glue layer
 *  layer to determine validity of bus/id/lun...
 */

static struct task_desc *
task_find(dev_t dev)
{
	scsi_dev_info_t		*sdevi;
	struct task_desc	*tp;
	uchar_t			bus;
	uchar_t			id;
	uchar_t			lun;

	/* to guard against user programs that still try 
	 * to use old style devs
	 */
	if (!dev_is_vertex(dev))
		return NULL;

	sdevi = scsi_dev_info_get(dev);

	bus  = SDEVI_ADAP(sdevi);
	id   = SDEVI_TARG(sdevi);
	lun  = SDEVI_LUN(sdevi);

	DBG(printf("task_find(%d,%d,%d)", bus,id,lun));

	tp = sdevi->sdevi_tds;
	if (!tp)  {
		tp = (struct task_desc *)ds_calloc(sizeof (struct task_desc));
		if(tp == NULL)  {	/* shouldn't happen... */
			DBG(printf("calloc of tp failed!\n"));
			return NULL;
		}
		tp->td_sensebuf = (uchar_t*)kmem_alloc(TDS_SENSELEN, KM_CACHEALIGN);
		sdevi->sdevi_tds = tp;
		tp->td_bus   = bus;
		tp->td_id    = id;
		tp->td_lun   = lun;
		tp->td_lun_vhdl = SDEVI_LUN_VHDL(sdevi);
		tp->td_dev_vhdl = dev;
		tp->td_state = TDS_START;
		DBG(printf("findtask ret new tp 0x%x. ",tp));

		/* this is done primarily because it does the scsi_info call,
		 * and our spec says you are supposed to do that at open.  It
		 * also has the side effect of causing devices probed via devscsi
		 * to show up in the hardware inventory, if they hadn't yet been
		 * seen, or they have been hot swapped */
		(void)glue_getopt(tp, 0);
	}
	else DBG(printf("tp %x, state %d. ", tp, tp->td_state));

	return tp;
}


static int
task_init_desc(struct task_desc *tp, dev_t dev)
{
	int err;
	/*  
	 *  We call the glue layer to (1) allocate and initialize
	 *  "td_hsreq", and (2) do any other pre-ask host preparatins
	 *  necessary before starting the host request.  This could
	 *  include, for example, reserving a host-bus-adapter channel.
	 */
	DBG(printf(" task_init_desc(%x,%x) ",tp,dev));
	err = glue_reserve(tp, dev);
	if(err) {
		/* target in use by some other driver, no memory, etc */
		DBG(printf("g_init fails, err %d. ", err));
	}
	else {
		tp->td_state = TDS_INIT;
		DBG(printf("hsreq %x. ", tp->td_hsreq));
	}
	return (err);
}

/*
 *  release per-task descriptor.
 *  First call glue layer to return host resources
 *  and then de-allocate the task descriptor.
 */

static void
task_free_desc(dev_t  dev)
{
	scsi_dev_info_t		*sdevi;
	struct task_desc	*tp;
	uchar_t			bus;
	uchar_t			id;
	/*REFERENCED*/
	uchar_t			lun;

	sdevi = scsi_dev_info_get(dev);

	bus  = SDEVI_ADAP(sdevi);
	id   = SDEVI_TARG(sdevi);
	lun  = SDEVI_LUN(sdevi);

	DBG(printf("task_free_desc(%d,%d,%d)\n",bus,id,lun));
	tp = task_find(dev);
	if ( !tp )  {
		DBG(printf("task_free called, but tp null or invalid\n"));
		return;	/* not open or no such device */
	}
	if ( tp->td_bus != bus || tp->td_id != id)  {
		DBG(printf("bus or ID doesn't match bus %d/%d, ID %d/%d\n",
				tp->td_bus, bus, tp->td_id, id));
		return;	/* not open or no such device */
	}

	/*
	 *  call glue layer to release "tp->td_hsreq" space
	 */
	glue_release(tp);

	/*
	 *  and release direct task space
	 */
	kmem_free(tp->td_sensebuf, TDS_SENSELEN);
	CFREE( (caddr_t) tp);
	sdevi->sdevi_tds = NULL;
}

static int
task_glue( char *arg, struct dsreq  *dp, struct task_desc *tp)
{
	int    rw;
	int    i;
	int    error;
	int    group, length;
	int    cmdlen_err = 0;
	DBG(printf("task_glue(0x%x,0x%x)\n", arg,tp));

	if(error=cpin_ds(arg, dp, tp))
		return error;

	/*
	 *  generic flag defaulting and testing
	 *  reject any unimplemented, except for TRACE and PRINT
	 */
	tp->td_flags = FLAGS(dp);
	i = glue_default;		/* get gluer defaults */
	tp->td_flags |= i;		/* force defaults on  */
	if(tp->td_flags &
	   ~(i | glue_support | DSRQ_TRACE | DSRQ_PRINT))
	{
		tp->td_ret = DSRT_UNIMPL;
		return  (ENXIO);
	}
	DBG(printf("task flags= %x ", tp->td_flags));

	/*
	 * Check to ensure that max DMA size is not exceeded.
	 */
	if (numpages(DATABUF(dp), DATALEN(dp)) > v.v_maxdmasz)
		return ENOMEM;

	/*
	 *  fetch command to task area
	 */
	if (CMDLEN(dp) > sizeof(tp->td_cmdbuf))
		return EINVAL;
	tp->td_cmdlen = CMDLEN(dp);
	if (ds_copyin(CMDBUF(dp), (caddr_t)tp->td_cmdbuf, tp->td_cmdlen) )
		return (EFAULT);	/* problem copying in args */

	group = (tp->td_cmdbuf[0] & 0xe0) >> 5;
	length = tp->td_cmdlen;

	switch (group) {
	  case 0: 
             if (length != 6) {
                 length = 6;
                 cmdlen_err = 1;
             }
	     break;

	  case 1:
          case 2: 
             if (length != 10)
	     {
                 length = 10;
                 cmdlen_err = 1;
             }
	     break;

	  case 5: 
            if (length != 12)
	    {
                length = 12;
                cmdlen_err = 1;
            }
          }

        if (cmdlen_err) {
	     tp->td_cmdlen = length;
	     if (ds_report_cmdlen_err)
	        printf("illegal command length, cdb=0x%x ,length %d",tp->td_cmdbuf[0], length);	
	}

	/*
	 *  If IOV set, fetch iovs to task area,
	 *  otherwise crate a normal iov  (from databuf and datalen)
	 *  so glue layer only deals with iov-s.
	 */
	if ( tp->td_flags & DSRQ_IOV )  {
		/* fetch iovs */
		tp->td_iovlen = min( IOVLEN(dp), sizeof tp->td_iovbuf);
#if _MIPS_SIM == _ABI64
		if(!ABI_IS_IRIX5_64(get_current_abi())) {
			struct irix5_dsiovec i5vecs[V_MAX];
			int len;

			len = min(IOVLEN(dp), sizeof(i5vecs));
			error = ds_copyin( IOVBUF(dp), (caddr_t)i5vecs, len);
			if( !error )
				irix5_iovecs_to_dsiovecs(i5vecs,tp->td_iovbuf,
					len/sizeof(struct irix5_dsiovec));
		}
		else
#endif /* _ABI64 */
			error = ds_copyin( IOVBUF(dp), (caddr_t)tp->td_iovbuf,
					tp->td_iovlen);
		if (error)
			return (EFAULT);	/* problem with iovs */

	}
	else  {
		/* build ersatz iov */
		tp->td_iovlen = sizeof (dsiovec_t);
		FILLIOV( tp->td_iovbuf, 0, DATABUF(dp), DATALEN(dp));
		DBG(printf("ersatz at %x, databuf/datalen %x/%d\n",
				tp->td_iovbuf,
				tp->td_iovbuf[0].iov_base,
				tp->td_iovbuf[0].iov_len));
	}

	/*
	 *  final preparations user buffer before passing to gluer
	 */
	tp->td_datalen = DATALEN(dp);
	rw = (tp->td_flags & DSRQ_READ) ? B_READ : B_WRITE;

	/* direct i/o:  lock things down */
	if ( !dma_lock( tp->td_iovbuf,
			tp->td_iovlen/sizeof(dsiovec_t),
			rw) )
		return (EFAULT);

	/*
	 *  finish up generic preparation
	 */
	tp->td_state = TDS_TASK;
	tp->td_time  = TIME(dp) ? TIME(dp) : 5000;	/* 5 sec if not given */

	return  (0);
}

int
task_unglue(struct task_desc *tp, struct dsreq *dp, char *arg)
{
	int    err = 0;		/* 0=>ok, otherwise unix errno */
	int    n;

	/*
	 *  unglue task, pass back returns even if errors
	 */
	DBG(printf(" task_unglue(%x,%x,%x) ",tp,dp,arg));

	CMDSENT(dp)   = tp->td_cmdlen;
	DATASENT(dp)  = tp->td_datalen;
	RET(dp)	      = tp->td_ret;
	STATUS(dp)    = tp->td_status;
	MSG(dp)       = tp->td_msgbuf[0];	/* only 1 byte return */

	/* direct i/o, release locking */
	dma_unlock(tp->td_iovbuf, tp->td_iovlen/sizeof(dsiovec_t),
		(tp->td_flags & DSRQ_READ) ? B_READ : B_WRITE);

	/*
	 * Copy the lesser of the amount of data we got and the amount
	 * of space they provided.
	 */
	n = min(SENSELEN(dp), tp->td_senselen);
	SENSESENT(dp) = n;
	if (n > 0 && ds_copyout((caddr_t) tp->td_sensebuf, SENSEBUF(dp), n)) {
		DBG(printf(" %d sense bytes",n));
		err = EFAULT;
		goto out;
	}

	err = cpout_ds(arg, dp, tp);

out:
	return err;
}

#if _MIPS_SIM == _ABI64
static void 
irix5_dsreq_to_dsreq(struct irix5_dsreq *d5p, struct dsreq *dp)
{
	dp->ds_flags = d5p->ds_flags;
	dp->ds_time = d5p->ds_time;
	dp->ds_private = d5p->ds_private;
	dp->ds_cmdbuf = (caddr_t)(__psint_t)d5p->ds_cmdbuf;
	dp->ds_cmdlen = d5p->ds_cmdlen;
	dp->ds_databuf = (caddr_t)(__psint_t)d5p->ds_databuf;
	dp->ds_datalen = d5p->ds_datalen;
	dp->ds_sensebuf = (caddr_t)(__psint_t)d5p->ds_sensebuf;
	dp->ds_senselen = d5p->ds_senselen;
	dp->ds_iovbuf = (dsiovec_t *)(__psint_t)d5p->ds_iovbuf;
	dp->ds_iovlen = d5p->ds_iovlen;
	dp->ds_link = (struct dsreq *)(__psint_t)d5p->ds_link;
	dp->ds_synch = d5p->ds_synch;
	dp->ds_revcode = d5p->ds_revcode;
	dp->ds_ret = d5p->ds_ret;
	dp->ds_status = d5p->ds_status;
	dp->ds_msg = d5p->ds_msg;
	dp->ds_cmdsent = d5p->ds_cmdsent;
	dp->ds_datasent = d5p->ds_datasent;
	dp->ds_sensesent = d5p->ds_sensesent;
}

static void 
dsreq_to_irix5_dsreq(struct dsreq *dp, struct irix5_dsreq *d5p)
{
	d5p->ds_flags = dp->ds_flags;
	d5p->ds_time = dp->ds_time;
	d5p->ds_private = dp->ds_private;
	d5p->ds_cmdbuf = (__psint_t)dp->ds_cmdbuf;
	d5p->ds_cmdlen = dp->ds_cmdlen;
	d5p->ds_databuf = (__psint_t)dp->ds_databuf;
	d5p->ds_datalen = dp->ds_datalen;
	d5p->ds_sensebuf = (__psint_t)dp->ds_sensebuf;
	d5p->ds_senselen = dp->ds_senselen;
	d5p->ds_iovbuf = (__psint_t)dp->ds_iovbuf;
	d5p->ds_iovlen = dp->ds_iovlen;
	d5p->ds_link = (__psint_t)dp->ds_link;
	d5p->ds_synch = dp->ds_synch;
	d5p->ds_revcode = dp->ds_revcode;
	d5p->ds_ret = dp->ds_ret;
	d5p->ds_status = dp->ds_status;
	d5p->ds_msg = dp->ds_msg;
	d5p->ds_cmdsent = dp->ds_cmdsent;
	d5p->ds_datasent = dp->ds_datasent;
	d5p->ds_sensesent = dp->ds_sensesent;
}

static void
irix5_iovecs_to_dsiovecs(struct irix5_dsiovec *v5p, dsiovec_t *vp, int cnt)
{
	int i;

	for( i = 0 ; i < cnt ; i++, vp++, v5p++ ) {
		vp->iov_base = (caddr_t)(__psint_t)v5p->iov_base;
		vp->iov_len = v5p->iov_len;
	}
}
#endif /* _ABI64 */
