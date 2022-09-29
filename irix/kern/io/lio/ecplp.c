/*
 * ecplp.c -- Device driver for IP32 (Moosehead/O2) ECP/EPP parallel port
 *
 * The driver supports IEEE-1284: ECP(Extended Capabilities Port) protocol,
 * EPP(Enhanced Parallel Port) protocol, and Compatibility (Centronics)
 * protocol with DMA mode.
 */

#ident  "io/lio/ecplp.c:  $Revision: 1.10 $"

#if defined(IP32) || defined(IPMHSIM)

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/systm.h>

#include <sys/ioctl.h>
#include <sys/debug.h>
#include <sys/mload.h>

#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/atomic_ops.h>
#include <sys/ksynch.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include "sys/invent.h"
#include <sys/ecplp.h>
#include <sys/plp.h>

#include <sys/ddi.h>

extern int cachewrback;

/* master.d/ecplp */
extern int plpopmode;			/* operation mode */
extern int plpwardelay;			/* workaround flag */
extern int plprtmout;			/* read timeout */

plp_t *plpinfo;

char *plpmversion = M_VERSION;
int   plpdevflag = D_MP;

/*
 * The thread model for this driver goes something like this:
 *	rlock/wlock/dlock - semaphores for read/write/dma access
 *	dmaxfr - semaphore for dma xfer synchronization
 *	mlock - mutex lock for plp opmode and state change
 *	plock - mutex lock for plp/pfd protocol change
 */


/******************************************************************************
 * function prototypes
 ******************************************************************************
 */

static void plpintr(eframe_t *, __psint_t);
static plp_t *plp_setup(vertex_hdl_t);
static int  plp_io(struct buf *);
static void plp_reset(plp_t *);
static void plp_dmaxfr(plp_t *, struct buf *);
static void plp_reset_dmacntxts(plp_t *);
static void plp_clrcntxt (plp_t *);
static int  plp_sigdelay(plp_t *, int);
static void plp_sigdelay_tmout(plp_t *);
static int  plp_prlock(plp_t *, char);
static void plp_prunlock(plp_t *, char);
static int  plp_lock(sema_t *);
static int  plp_unlock(sema_t *);
static int  plp_negotiate(plp_t *, char);
static void plp_phase_reset(plp_t *, char);
static int  plp_fwd2rvs(plp_t *);
static int  plp_rvs2fwd(plp_t *);
static void plp_rbflush(plp_t *);
static void plp_war_delay(plp_t *, int);

/* functions for pfd parallel floppy driver */
int	    plpfdc_open(void);
int	    plpfdc_close(void);
int	    plpfdc_read(char, char *, int, int *, char, char);
int	    plpfdc_write(char, char *, int, int *, char, char);
int	    plpfdc_chkirq(void);
int	    plpfdc_fifo_ctrl(int, int, int);
int         plpfdc_hdout(void);
void        plpfdc_dmatc(int);
static int  plpfdc_cpp(char, char);


/*
 * From the master.d/ecplp description:
 *
 * The following parameters apply to non-FIFO compatibility mode
 * (mode 1) only.  In general, this mode should only be used for
 * slower devices which are not operating correctly with the default
 * FIFO mode.
 *
 * plpStrobeDelay -     Minimum delay in us between byte strobes.
 *                      default = 1 us
 * plpRetryDelay1 -     Delay in 10 ms ticks if device is still busy after
 *                      plpStrobeDelay.
 *                      default = 1 tick (10 ms)
 * plpNumRetries1 -     Number of retries to attempt initially.
 *                      default = 100
 * plpRetryDelay2 -     Delay in 10 ms ticks if device remains busy after
 *                      plpNumRetries1 * plpRetryDelay1 ticks; in case,
 *                      for example, the device is offline.
 *                      default = 100 (1 s)
 */

extern int plpStrobeDelay;
extern int plpRetryDelay1;
extern int plpNumRetries1;
extern int plpRetryDelay2;


/******************************************************************************
 *           Parallel Peripheral Driver Entry Point Routines                  *
 ******************************************************************************
 */

/*
 * plpinit()
 *
 * Description:
 *    Initialize parallel port driver,
 */
void
plpinit (void)
{
    return;
}


/*
 * plpstart()
 *
 * Description:
 *    Setup plp info, initialize read buffer, mutex locks, and semaphors.
 */
void
plpstart (void)
{
    plp_t *plp;
    vertex_hdl_t vhdl;
    static int plp_init = 0;
    caddr_t plprbuf;
    int i;
#ifdef ONSPEC_BUG_RD_WAR
    int pfdrbpg = (((int)PFD_RBUF_SZ - 1) / NBPP) + 1;
    caddr_t pfdrbuf;
#endif

    /* make sure single entry and connection vertex exists */
    if (atomicAddInt(&plp_init, 1) > 1 ||
	hwgraph_traverse(hwgraph_root, EDGE_LBL_NODE_IO, &vhdl) !=
	GRAPH_SUCCESS) {
	return;
    }
    
    /* setup plp info and probe pfd */
    if (!(plp = plp_setup(vhdl)))
	return;

    /* allocate receive buffers. */
    if (!(plprbuf = (caddr_t)kvpalloc(NRP, VM_NOSLEEP | VM_DIRECT |
	VM_UNCACHED, 0))) {
	cmn_err(CE_WARN, "plp: out of memory, plp init failed");
	return;
    }
#ifdef R10000_SPECULATION_WAR
    /* make sure our page is relatively clean */
    dki_dcache_wbinval((void *)PHYS_TO_K0(KDM_TO_PHYS(plprbuf)), NBPP);
#endif

    /* init plp receive buffers */
    for (i = 0; i < NRP; i++)
	plp->rbuf[i].vaddr = plprbuf + (i * NBPP);

#ifdef ONSPEC_BUG_RD_WAR
    if (plp->fvhdl) {
	/* init pfd read sanity buffers */
	if (!(pfdrbuf = (caddr_t)kvpalloc(NPFDRB * pfdrbpg, VM_NOSLEEP |
		VM_DIRECT | VM_UNCACHED, 0))) {
		cmn_err(CE_WARN, "plp: out of memory, pfd init failed");
		return;
	}
	for (i = 0; i < NPFDRB; i++)
		plp->pfdrbuf[i] = pfdrbuf + (i * pfdrbpg * NBPP);
    }
#endif

    /* init mutex locks */
    mutex_init(&plp->mlock, MUTEX_DEFAULT, "mlock");
    mutex_init(&plp->plock, MUTEX_DEFAULT, "plock");
    mutex_init(&plp->xlock, MUTEX_DEFAULT, "xlock");
    mutex_init(&plp->tmoutlock, MUTEX_DEFAULT, "tmoutlock");

    /* init read, write, dma, xfr semaphores */
    initnsema(&plp->rlock, 1, "rlock");
    initnsema(&plp->wlock, 1, "wlock");
    initnsema(&plp->dlock, 1, "dlock");
    
    /* init dma xfer synch variables */
    sv_init(&plp->dmaxfr, SV_DEFAULT, "dmaxfr");
    sv_init(&plp->sv_tmout, SV_DEFAULT, "sv_tmout");

    plp->prlcks = 0;
    plp->rtmout = (plprtmout) ? plprtmout : PLP_RTO;
    plp->wtmout = PLP_WTO;
    plp->tmoutid = 0;
    plp->opmode = PLP_NULL_MODE; /* force negotiation */
    plp->phase = CMPTBL_IDLE_PHASE;
    plp->state = PLP_ALIVE;

    /* reset MACE plp DMA, SuperIO, and printer */
    plp_reset(plp);

#ifdef _MACEINTR_BUG_FIXED
    /* temporarily disable PLP_DEV_IRQ because mace_intr() cannot handle
       it correctly at time being.
     */
    /* turn on device/tc and memory error interrupts */
    plp->irqmask = (PLP_DEV_IRQ | PLP_MEM_IRQ);
#else
    plp->irqmask = 0;
#endif

    /* setup parallel port interrupt vector. */
    setmaceisavector(MACE_INTR(MACE_PERIPH_PARALLEL), PLP_IRQS, plpintr);

    return;
}


#ifdef _UNSET_MACE_VEC_OK
/*
 * plpunload()
 *
 * Description:
 *    Check every minor device number to be sure that there are no open
 *    references to this driver. Return EBUSY if the driver is still in use.
 *    If the driver is no longer in use, then free up any allocated resources
 *    and return 0.  The higher level mload unload function will guarantee
 *    that no opens are called on this driver while it is being unloaded.
 */
int
plpunload (void)
{
#ifdef ONSPEC_BUG_RD_WAR
    int pfdrbpg = (((int)PFD_RBUF_SZ - 1) / NBPP) + 1;
#endif

    PLP_LOCK(&plpinfo->mlock);
    if (plpinfo->state & PLP_INUSE) {
	PLP_UNLOCK(&plpinfo->mlock);
	return (EBUSY);
    }
    PLP_UNLOCK(&plpinfo->mlock);

    /* detach from IP32 interrupt dispatch core */
    if (plpinfo->state & PLP_ALIVE) {
	/* clear plp interrupt mask bits */
	mace_mask_update(PLP_IRQS,
		(plpinfo->irqmask &= ~(PLP_DEV_IRQ | PLP_MEM_IRQ)));
    }

    /* destroy mutexes */
    mutex_destroy(&plpinfo->mlock);
    mutex_destroy(&plpinfo->plock);
    mutex_destroy(&plpinfo->xlock);
    mutex_destroy(&plpinfo->tmoutlock);

    /* free semaphores */
    freesema(&plpinfo->dlock);
    freesema(&plpinfo->wlock);
    freesema(&plpinfo->rlock);
    
    /* destroy synch vars */
    sv_destroy(&plpinfo->dmaxfr);
    sv_destroy(&plpinfo->sv_tmout);

    /* free read buffers */
    if (plpinfo->rbuf[0].vaddr)
	kvpfree(plpinfo->rbuf[0].vaddr, NRP);

#ifdef ONSPEC_BUG_RD_WAR
    if (plpinfo->pfdrbuf[0])
	kvpfree(plpinfo->pfdrbuf[0], pfdrbpg * NPFDRB);
#endif

    /* unset parallel port interrupt vector. */
    unsetmaceisavector(MACE_INTR(MACE_PERIPH_PARALLEL), PLP_IRQS);

    return (0);
}
#endif


/*
 * plpopen()
 *
 * Description:
 *    Open parallel port device
 */
/* ARGSUSED */
int
plpopen (dev_t *devp, int mode)
{
    plp_t *plp;
    int opmode;

    /* check if the driver exist */
    if (!plpinfo)
	return (EIO);

    if (!dev_is_vertex(*devp) ||
	!((plp = PLP_DEV2PLP(*devp))->state & PLP_ALIVE))
        return (ENXIO);

    plp->devt = *devp;

    /* set up I/O operation mode:
     * master.d/ecplp may overwrite opmode of /dev/plp
     */
    if (!(opmode = PLP_GET_OPMODE(*devp)))
	opmode = (plpopmode << 4);

    /* check plp operation mode */
    if (plp->opmode != opmode && !((plp->opmode == PLP_CDF_MODE ||
	plp->opmode == PLP_ECP_MODE) && opmode == PLP_CMPTBL_MODE)) {

	/* check for conflict operation mode */
	if (plp->state & PLP_INUSE)
		return (EBUSY);

	PLP_LOCK(&plp->mlock);
	/* set up the operation mode */
	switch (opmode) {
	   case PLP_AUTO_MODE:
		/* try ECP mode */
		if (plp_negotiate(plp, EXTREQ_ECP_MODE)) {
			PLP_SET_OPMODE(plp, PLP_ECP_MODE);
			plp->state |= PLP_RDOPEN;
		}
		else /* use Centronics FIFO mode */
			PLP_SET_OPMODE(plp, PLP_CDF_MODE);
		break;

	   case PLP_ECP_MODE:
	   case PLP_BIDIR_MODE:
		if (!plp_negotiate(plp, EXTREQ_ECP_MODE)) {
			PLP_UNLOCK(&plp->mlock);
			return (ENXIO);
		}

		PLP_SET_OPMODE(plp, PLP_ECP_MODE);
		plp->state |= PLP_RDOPEN;
		break;

	   case PLP_EPP_MODE:
		if (!plp_negotiate(plp, EXTREQ_EPP_MODE)) {
			PLP_UNLOCK(&plp->mlock);
			return (ENXIO);
		}
		PLP_SET_OPMODE(plp, PLP_EPP_MODE);
		plp->state |= PLP_RDOPEN;
		break;

	   case PLP_CDF_MODE:
		PLP_SET_OPMODE(plp, PLP_CDF_MODE);
		break;

	   case PLP_TEST_MODE:
		PLP_SET_OPMODE(plp, PLP_TEST_MODE);
		plp->state |= PLP_RDOPEN;
		break;

           case PLP_PIO_MODE:
                PLP_SET_OPMODE(plp, PLP_PIO_MODE);
                break;

	   default:
		PLP_SET_OPMODE(plp, PLP_CMPTBL_MODE);
	}
	PLP_UNLOCK(&plp->mlock);
    }
    plp->state |= PLP_WROPEN | PLP_OCCUPIED | PLP_INUSE;

    return (0);
}


/*
 * plpclose()
 *
 * Description:
 *    Close parallel port device
 */
int
plpclose (dev_t dev)
{
    plp_t *plp = PLP_DEV2PLP(dev);

    /* make sure parallel floppy is not in use */
    if (plp_prlock(plp, PLP_PRN))
	return (EBUSY);

    PLP_LOCK(&plp->mlock);
    /* reset plp state */
    plp->state &= ~(PLP_WROPEN | PLP_RDOPEN | PLP_NBIO | PLP_INUSE);

    /* stop DMA */
    PLP_DMA_STOP(plp);

    /* flush receive buffer */
    if (MACE_ISA_RD(PLP_DMA_CTRL_REG) & MACE_DMA_INPUT)
	plp_rbflush(plp);

    plp_unlock(&plp->dlock);

    /* reset timeout */
    plp->rtmout = (plprtmout) ? plprtmout : PLP_RTO;
    plp->wtmout = PLP_WTO;

    /* release protocol lock */
    plp_prunlock(plp, PLP_PRN);
    plp->state &= ~PLP_OCCUPIED;

    PLP_UNLOCK(&plp->mlock);

    return (0);
}


/*
 * plpread()
 *
 * Description:
 *    Read from parallel port device
 */
int
plpread (
    dev_t dev, uio_t *uiop)
{
    plp_t *plp = PLP_DEV2PLP(dev);
    int erc;

    /* make sure something to xfer */
    if (!uiop->uio_resid)
	return (0);

    /* check if PLP_RDOPEN is set properly */
    if (!(plp->state & PLP_RDOPEN))
	return (EIO);

    /* get plp/pfd protocol lock */
    if (plp_prlock(plp, PLP_PRN))
	return (EBUSY);

    erc = uiophysio(plp_io, NULL, dev, B_READ, uiop);
    plp_prunlock(plp, PLP_PRN);

    return (erc);
}


/*
 * plpwrite()
 *
 * Description:
 *    Write to parallel port device
 */
int
plpwrite (
    dev_t dev, uio_t *uiop)
{
    plp_t *plp = PLP_DEV2PLP(dev);
    int erc;

    /* make sure something to xfer */
    if (!uiop->uio_resid)
	return (0);

    /* get plp/pfd protocol lock */
    if (plp_prlock(plp, PLP_PRN))
	return (EBUSY);

    erc = uiophysio(plp_io, NULL, dev, B_WRITE, uiop);
    plp_prunlock(plp, PLP_PRN);

    return (erc);
}


/*
 * plpioctl()
 *
 * Description:
 *    Control parallel port device
 */
/* ARGSUSED */
int
plpioctl (dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    plp_t *plp = PLP_DEV2PLP(dev);
    int erc = 0;
    BYTE status;

    /* get plp/pfd protocol lock */
    if (plp_prlock(plp, PLP_PRN))
	return (EBUSY);

    switch (cmd) {
	case PLPIOCRESET:
	     plp_reset(plp);
	     break;

	case PLPIOCSTATUS:
	     status = LPT_CTRL_RD(PLP_LPT_DSR_REG);
	     *rvp = 0;
	     if (status & DSR_ONLINE)	/* Select; 1=online */
     	     	*rvp |= PLPONLINE;

	     if (status & DSR_NOINKZ)	/* device interrupt; 1=no ink */
		*rvp |= PLPEOI;

	     /* these are valid only in compatibility mode */
	     if (plp->opmode == PLP_CDF_MODE ||
		 plp->opmode == PLP_CMPTBL_MODE) {

		if (status & DSR_PE)	/* PError; 1=paper end */
			*rvp |= PLPEOP;

		if (!(status & DSR_FAULTZ))/* nFault; 0=fault */
			*rvp |= PLPFAULT;
	     }
	     break;

	case PLPIOMODE:
	     /* make sure no dma activity */
	     plp_lock(&plp->dlock);
	     plp_phase_reset(plp, 0);

	     switch (arg) {
		case PLP_ECP_MODE:
		case PLP_BIDIR_MODE:
		     if (!plp_negotiate(plp, EXTREQ_ECP_MODE)) {
			erc = ENXIO;
			goto ioctl_done;
		     }
		     PLP_SET_OPMODE(plp, PLP_ECP_MODE);
		     break;

		case PLP_EPP_MODE:
		    if (!plp_negotiate(plp, EXTREQ_EPP_MODE)) {
			erc = ENXIO;
			goto ioctl_done;
		     }
		    PLP_SET_OPMODE(plp, PLP_EPP_MODE);
		    break;
		    
		case PLP_TEST_MODE:
		    PLP_SET_OPMODE(plp, PLP_TEST_MODE);
		    break;

		case PLP_CFG_MODE:
		    PLP_SET_OPMODE(plp, PLP_CFG_MODE);
		    break;

		case PLP_CMPTBL_MODE:
		    PLP_SET_OPMODE(plp, PLP_CMPTBL_MODE);
		    break;

		case PLP_CDF_MODE:
		default:
		    PLP_SET_OPMODE(plp, PLP_CDF_MODE);
		    break;
	     }
	     plp_unlock(&plp->dlock);
	     break;

	case PLPIOCWTO:
	     plp->wtmout = arg * HZ;
	     break;
	case PLPIOCRTO:
	     plp->rtmout = arg * HZ;
	     break;

	case PLPIOCREAD:
	     if (plp->opmode != PLP_ECP_MODE &&
		plp->opmode != PLP_EPP_MODE &&
		plp->opmode != PLP_BIDIR_MODE) {
		erc = EIO;
		goto ioctl_done;
	     }

	if (plp->opmode == PLP_ECP_MODE &&
           (plp->phase != ECP_RVS_IDLE_PHASE || !plp_rvs2fwd(plp))) {
		erc = EIO;
		goto ioctl_done;
	     }

	     plp->state |= PLP_RDOPEN;
	     break;

	case PLPIOCDCR:
	     LPT_CTRL_WR(PLP_LPT_DCR_REG, (arg & 0xff));
	     break;

	case PLPIOCDSR:
	     *rvp = LPT_CTRL_RD(PLP_LPT_DSR_REG);
	     break;

        case PLPIOCADDRWRITE:
             if (plp->opmode != PLP_EPP_MODE) {
                     erc = EIO;
                     goto ioctl_done;
             }
             LPT_CTRL_WR(PLP_EPP_ADDR_REG, arg);
             break;

        case PLPIOCADDRREAD:
             if (plp->opmode != PLP_EPP_MODE) {
                     erc = EIO;
                     goto ioctl_done;
             }
             *rvp = LPT_CTRL_RD(PLP_EPP_ADDR_REG);
             break;

	case PLPIOCDATA:
	     LPT_CTRL_WR(PLP_LPT_DATA_REG, (arg & 0xff));
	     break;

	case PLPIOCTYPE:
	case PLPIOCIGNACK:
	case PLPIOCSETMASK:
	case PLPIOCSTROBE:
	case PLPIOCESTROBE:
	     /* unsupported, keep for compatibility */
	     break;

	default:
	     erc = EINVAL;
    }
    ioctl_done:

    /* release plp/pfd protocol lock */
    plp_prunlock(plp, PLP_PRN);

    return (erc);
}


/*
 * plpintr()
 *
 * Description:
 *    Parallel port device interrupt handler
 */
/* ARGSUSED */
static void
plpintr (eframe_t *ef, __psint_t irq_status)
{
    plp_t *plp = plpinfo;
    _macereg_t plp_irq = irq_status;
    uchar_t xirqs;

    /* context exhausted interrupt request */
    if (xirqs = ((plp_irq & PLP_CTXS_IRQ) >> PLP_CTXS_IRQ_SHIFT)) {

	/* acquire context mutex lock */
	PLP_LOCK(&plp->xlock);

	for (; xirqs; plp->curcntxt = PLP_NEXT_CTX(plp->curcntxt)) {

		/* make sure got the right context exhausted intr */
		if (!(xirqs & (1 << plp->curcntxt))) {
		    cmn_err(CE_WARN, "plp: context IRQ out of order");
		    continue;
		}
		/* stop further interrupt */
		/* clear context mask bit and update it at the end */
		plp->irqmask &= ~(PLP_CTX_IRQ << plp->curcntxt);

		/* mark it done */
		xirqs &= ~(1 << plp->curcntxt);

		if (plp->iodir) { /* PLP_INPUT */
		    plp_rbuf_t *rbp = &plp->rbuf[plp->curcntxt];
		    bcopy(rbp->vaddr, rbp->usrbuf, rbp->bcount);
		}
		/* current contexts xfer complete, mark it free */
		plp->cntxts |= (1 << plp->curcntxt);
	}

	/* wake up process waiting for contexts */
	sv_signal(&plp->dmaxfr);

	/* release context mutex lock */
	PLP_UNLOCK(&plp->xlock);
    }

    /* memory error interrupt request */
    if (plp_irq & PLP_MEM_IRQ) {
	plp_reset_dmacntxts(plp);
	bioerror(plp->bp, EIO);
	biodone(plp->bp);
	cmn_err(CE_WARN, "plp: DMA memory error");
    }

    /* peripheral device interrupt request */
    if (plp_irq & PLP_DEV_IRQ) {
	/* clear interrupt */
	MACE_ISA_CLR(ISA_IRQ_STS_REG, PLP_DEV_IRQ);
    }

    /* update interrupt mask bits */
    mace_mask_update(PLP_IRQS, plp->irqmask);

    return;
}


/*
 * plp_tmout()
 *
 * Description:
 *    Data transfer timeout
 */
static void
plp_tmout (plp_t *plp)
{
    int lockAcquired;
    
    /* check if plpintr has cleared it */
    if (!plp->tmoutid)
	return;

    plp->tmoutid = 0;
    
    /* try to acquire context lock, but we continue even if we fail */
    lockAcquired = mutex_trylock(&plp->xlock);
    
    PLP_DMA_STOP(plp);
    plp->tmout_ctxts = plp->cntxts;
    plp->diag = MACE_ISA_RD(PLP_DMA_DIAG_REG);

    plp_reset_dmacntxts(plp);
    plp_unlock(&plp->dlock);
    plp->bp->b_error = EAGAIN;

    if (plp->iodir) { /* PLP_INPUT */
	plp_rbuf_t *rbp = &plp->rbuf[plp->curcntxt];
	bcopy(rbp->vaddr, rbp->usrbuf, rbp->bcount -
		PLP_DMA_RESID(MACE_ISA_RD(PLP_DMA_DIAG_REG)));
	plp->rbufctrl = ALL_CTXS;
    }
    sv_signal(&plp->dmaxfr);
    
    if (lockAcquired)
    	mutex_unlock(&plp->xlock);

    return;
}


/******************************************************************************
 *           Parallel Peripheral Driver Auxiliary Routines                    *
 ******************************************************************************
 */

/*
 * plp_setup()
 *
 * Description:
 *    Create plp info, hwgraph vertices, and inventory.
 *    probe pfd.
 */
static plp_t *
plp_setup (
    vertex_hdl_t vhdl)
{
    plp_t *plp;
    plpif_t *pif;
    vertex_hdl_t pvhdl, ivhdl;
    int i;

    /* setup plp info and probe pfd */
    if (hwgraph_traverse(vhdl, EDGE_LBL_ECPLP, &pvhdl) == GRAPH_SUCCESS)
	return (plpinfo = device_info_get(pvhdl));

    /* create parallel hwgraph vertex: /hw/note/io/ecplp */
    if (hwgraph_path_add(vhdl, EDGE_LBL_ECPLP, &pvhdl) != GRAPH_SUCCESS)
	return ((plp_t *)0);

    /* create plp info struct */
    plpinfo = NEW(plp, plp_t *);
    plp->cvhdl = vhdl;
    plp->pvhdl = pvhdl;
    device_info_set(pvhdl, (void *)plp);

    /* add plp hwgraph vertices: /hw/note/io/ecplp/plp,epp */
    for (i = 0; i < 2; i++) {
	char *edge[2] = { EDGE_LBL_PLP, EDGE_LBL_EPP };
	char mode[2] = { PLP_AUTO_MODE, PLP_EPP_MODE };

	if (hwgraph_char_device_add(pvhdl, edge[i], "plp", &ivhdl) !=
	    GRAPH_SUCCESS)
		continue;

	NEW(pif, plpif_t *);
	pif->plp = plp;
	pif->pvhdl = ivhdl;
	pif->opmode = mode[i];
	device_info_set(ivhdl, (void *)pif);
    }

    /* add alias edge, /hw/parallel -> /hw/node/io/ecplp */
    (void)hwgraph_edge_add(hwgraph_root, plpinfo->pvhdl, EDGE_LBL_PARALLEL);

    /* add plp inventory */
    device_inventory_add(pvhdl, INV_PARALLEL, INV_EPP_ECP_PLP, 0, 0, 1);

    /* probe parallel floppy drive */
    PLP_SET_OPMODE(plp, PLP_CMPTBL_MODE);

    if (PLP_ONSPEC_ENBL(0) &&
	hwgraph_path_add(pvhdl, EDGE_LBL_PFD, &ivhdl) == GRAPH_SUCCESS) {

	/* add pfd inventory */
	device_inventory_add(ivhdl, INV_PARALLEL, INV_EPP_PFD, 0, 0, 0);
	plp->fvhdl = ivhdl;
    }

    return (plp);
}


/*
 * plp_io()
 *
 * Description:
 *    Parallel I/O strategy routine.
 */
static int
plp_io (struct buf *bp)
{
    plp_t *plp = PLP_DEV2PLP(bp->b_edev);
    int phase, bcount;
    char *cp;

    bp->b_resid = bp->b_bcount;
    phase = plp->phase;	
    plp->bp = bp;

    /* process read request on buffered data
     */
    if (bp->b_flags & B_READ) {

	/* grab the read lock to prevent receiving intermingled data */ 
	if (plp_lock(&plp->rlock))
		bp->b_error = EINTR;
	else {
		/* verify correct operation phase */
		if (plp->opmode != PLP_ECP_MODE && plp->opmode != PLP_EPP_MODE)
			return (EIO);

		if ((plp->opmode == PLP_ECP_MODE &&
		    (plp->phase != ECP_RVS_IDLE_PHASE && !plp_fwd2rvs(plp))) ||
		    (plp->opmode == PLP_EPP_MODE &&
		    (plp->phase != EPP_IDLE_PHASE)))
			return (EIO);

		/* set time out value */
		plp->tmout = plp->rtmout;

		/* set up input direction */
		PLP_SET_INPUT_DIR(plp);

		/* enable DMA on both MACE and SuperIO */
		PLP_DMA_ENBL(plp);

		/* start DMA transfer */
		plp_dmaxfr(plp, bp);

		/* unset input direction */
		PLP_CLR_INPUT_DIR(plp);

		plp_unlock(&plp->rlock);
	}
    }
    
    /* process write request through DMA channel
     */
    else {
	/* grab the write lock to prevent sending intermingled data */
	if (plp_lock(&plp->wlock)) {
		bp->b_error = EINTR;
	}
	/* Centronics FIFO, EPP, ECP Mode: DMA */
	else if (plp->opmode != PLP_CMPTBL_MODE &&
		 plp->opmode != PLP_TEST_MODE &&
		 plp->opmode != PLP_PIO_MODE) {
		/* verify correct operation phase */
		if ((plp->opmode == PLP_ECP_MODE &&
		    (plp->phase != ECP_IDLE_PHASE && !plp_rvs2fwd(plp))) ||
		    (plp->opmode == PLP_EPP_MODE &&
		    (plp->phase != EPP_IDLE_PHASE)))
			return (EIO);

		/* set time out value */
		plp->tmout = plp->wtmout;

		/* enable DMA on both MACE and SuperIO */
		PLP_DMA_ENBL(plp);

		/* work-around delay for some printers, e.g. HP DJ1600C.
		 * to prevent possible data corruption in compatibilty mode
		 */
		if (plpwardelay)
			plp_war_delay(plp, plpwardelay);

		/* start DMA transfer */
		plp_dmaxfr(plp, bp);

		plp_unlock(&plp->wlock);
	}
	/* Centronics PIO Mode */
	else {
		uint_t    tm;
		uint_t    retry_delay;
		uint64_t  us_tot = 0;
		
		cp = (char *)bp->b_dmaaddr;
		if (cachewrback)
			DCACHE_WB(bp->b_dmaaddr, bp->b_bcount);

		for (cp = (char *)bp->b_dmaaddr, bcount = bp->b_bcount;
		     bcount; cp++, bcount--) {
			retry_delay = plpRetryDelay1;
			tm = plpNumRetries1;
			
			/* wait for device ready */
			while (!(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_BUSYZ)) {
				if (!tm--) {
                                        /* The printer has been busy for a
                                         * while.  It could be offline, so
                                         * bump up the delay. */
                                         retry_delay = plpRetryDelay2;
                                }
                                /* Sleep for retry_delay or until signal */
                                if (plp_sigdelay(plp, retry_delay)) {
                                        bp->b_error = EINTR;
                                        goto pio_done;
                                }
				
				us_tot += (retry_delay * 1000000) / HZ;
				
				/* Check if we've timed out */
				if (plp->wtmout > 0 && 
				   (plp->wtmout * 1000000) / HZ < us_tot)
					goto pio_done;
			}
			LPT_CTRL_WR(PLP_LPT_DATA_REG, *cp);
			DELAYBUS(1);
			LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_STB);
			DELAYBUS(1);
			LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_STB);
			bp->b_resid--;
			DELAYBUS(plpStrobeDelay);
			us_tot += 2 + plpStrobeDelay;
			
			/* Check if we've timed out */
			if (plp->wtmout > 0 && 
			   (plp->wtmout * 1000000) / HZ < us_tot)
				goto pio_done;
		}
		bp->b_error = 0;
		pio_done:
		plp_unlock(&plp->wlock);
	}
    }
    plp->phase = phase;

    if (bp->b_error)
	bp->b_flags |= B_ERROR;

    /* only for normal io call */
    if (bp->b_edev)
	iodone (bp);

    return (0);
}


/*
 * plp_dmaxfr()
 *
 * Description:
 *    Start DMA transfer with all available contexts
 *    acquire/release dlock
 */
static void
plp_dmaxfr (plp_t *plp, struct buf *bp)
{
    uint_t quota, load, b_resid;
    long long cntxt_data, lload;
    caddr_t vaddr, b_endaddr;

    /* acquire dma access lock */
    if (plp_lock(&plp->dlock)) {
	bp->b_error = EINTR;
	cmn_err(CE_WARN, "plp: I/O abort\n");
	return;
    }

    /* acquire context mutex lock */
    PLP_LOCK(&plp->xlock);
    
    /* set up time out */
    if (plp->tmoutid)
                untimeout(plp->tmoutid);

    plp->tmoutid = (plp->tmout) ?
                   timeout(plp_tmout, plp, plp->tmout) : 0;

    /* start DMA transfer */
    for (b_endaddr = bp->b_dmaaddr + bp->b_bcount, b_resid = bp->b_resid;
         b_resid && !bp->b_error;) {

	/* use all available contexts for DMA */
	for (; plp->cntxts && b_resid > 0;
	     plp->nxtcntxt = PLP_NEXT_CTX(plp->nxtcntxt)) {

		/* make sure the desired context is available */
		if (!(plp->cntxts & (1 << plp->nxtcntxt))) {
			cmn_err(CE_WARN, "plp: free context out of order");
			plp->nxtcntxt = PLP_NEXT_CTX(plp->nxtcntxt);
		}

		/* mark the context as used */
		plp->cntxts &= ~(1 << plp->nxtcntxt);

		/* set up MACE DMA context register */
		vaddr = b_endaddr - b_resid;
		quota = (plp->iodir) ? NBPP : (NBPP - poff(vaddr));
		lload = load = MIN(b_resid, quota);
		b_resid -= load;

		if (plp->iodir) {
			plp->rbuf[plp->nxtcntxt].bcount = load;
			plp->rbuf[plp->nxtcntxt].usrbuf = vaddr;
			vaddr = plp->rbuf[plp->nxtcntxt].vaddr;
			if (cachewrback)
				DCACHE_INVAL(vaddr, load);
		}
		else {
			if (cachewrback)
				DCACHE_WB(vaddr, load);
		}

		cntxt_data = ((b_resid) ? 0 : LAST_FLAG) | ((lload - 1) << 32) |
			kvtophys((void *)vaddr);
			
                if (plp->nxtcntxt)
                        plp->loadB = load;
                else
                        plp->loadA = load;

		/* start DMA xfr */
		MACE_ISA_WR(((plp->nxtcntxt) ? PLP_DMA_CTX1_REG :
			PLP_DMA_CTX0_REG), cntxt_data);

		/* enable context exhausted IRQ */
		mace_mask_update(PLP_IRQS,
			(plp->irqmask |= (PLP_CTX_IRQ << plp->nxtcntxt)));
	}

	/* wait for any context available */
	while (plp->cntxts == 0) {
		if (sv_wait_sig(&plp->dmaxfr, PZERO, &plp->xlock, 0) < 0) {		
			bp->b_error = EINTR;
			PLP_DMA_STOP(plp);
			plp_reset_dmacntxts(plp);
			goto dma_done;
		}
		PLP_LOCK(&plp->xlock);
	}

	/* check if the previous xfer complete */
	if (!bp->b_error) {		/* complete */
		bp->b_resid = b_resid;
	}
	else if (bp->b_error == EAGAIN) { /* partial xfer */
	        /* Compute b_resid by adding back leftover bytes */
                if (!(plp->tmout_ctxts & CTX_A)) {
                        if (PLP_DMA_ACTIVE(plp->diag) &&
                            PLP_DMA_INUSE_CTX(plp->diag) == CTX_A)
                                b_resid += PLP_DMA_RESID(plp->diag) + 1;
                        else
                                b_resid += plp->loadA;
                }
                if (!(plp->tmout_ctxts & CTX_B)) {
                        if (PLP_DMA_ACTIVE(plp->diag) &&
                            PLP_DMA_INUSE_CTX(plp->diag) == CTX_B)
                                b_resid += PLP_DMA_RESID(plp->diag) + 1;
                        else
                                b_resid += plp->loadB;
                }

                bp->b_resid = b_resid;
                bp->b_error = 0;
                PLP_UNLOCK(&plp->xlock);
                goto dma_done;
	}
    }

    /* wait for all DMA transfer complete */
    while (plp->cntxts != ALL_CTXS) {
	if (sv_wait_sig(&plp->dmaxfr, PZERO, &plp->xlock, 0) < 0) {	
		bp->b_error = EINTR;
		PLP_DMA_STOP(plp);
		plp_reset_dmacntxts(plp);
		break;
	}
	PLP_LOCK(&plp->xlock);
    }
    
    /* check if the previous xfer complete */
    if (!bp->b_error) {		/* complete */
	bp->b_resid = b_resid;
    }
    else if (bp->b_error == EAGAIN) { /* partial xfer */
        /* Compute b_resid by adding back leftover bytes */
        if (!(plp->tmout_ctxts & CTX_A)) {
                if (PLP_DMA_ACTIVE(plp->diag) &&
                    PLP_DMA_INUSE_CTX(plp->diag) == CTX_A)
                        b_resid += PLP_DMA_RESID(plp->diag) + 1;
                else
                        b_resid += plp->loadA;
        }
        if (!(plp->tmout_ctxts & CTX_B)) {
                if (PLP_DMA_ACTIVE(plp->diag) &&
                    PLP_DMA_INUSE_CTX(plp->diag) == CTX_B)
                        b_resid += PLP_DMA_RESID(plp->diag) + 1;
                else
                        b_resid += plp->loadB;
        }

        /* reset b_error to so we return # of bytes xfer'd before timeout */
        bp->b_resid = b_resid;
        bp->b_error = 0;

    }
	
    PLP_UNLOCK(&plp->xlock);

    dma_done:
    
    /* clear timeout */
    if (plp->tmoutid) {
        untimeout(plp->tmoutid);
        plp->tmoutid = 0;
    }

    plp_unlock(&plp->dlock);

    if (bp->b_resid)
	cmn_err(CE_WARN, "plp: I/O abort\n");

    return;
}


/*
 * plp_reset_dmacntxts()
 *
 * Description:
 *    Reset plp DMA and contexts
 */
static void
plp_reset_dmacntxts (plp_t *plp)
{
    PLP_DMA_RESET(plp);

    plp->curcntxt = plp->nxtcntxt = PLP_DMA_CTX0;
    plp->cntxts = ALL_CTXS;
    plp->rbufctrl = 0;
}


/*
 * plp_clrcntxt()
 *
 * Description:
 *    Clear all configed DMA contexts
 */
static void
plp_clrcntxt (plp_t *plp)
{
    uchar_t cntxt = plp->curcntxt;

    do {
	plp->cntxts |= (1 << cntxt);
	cntxt = !cntxt;
    } while (cntxt != plp->nxtcntxt);

    plp->nxtcntxt = plp->curcntxt;
    sv_signal(&plp->dmaxfr);

    return;
}

/*
 * plp_sigdelay()
 *
 * Description:
 *    Sleep for specified number of ticks, or until signal
 */
static int
plp_sigdelay(plp_t *plp, int ticks)
{
    int tmoutid = 0;

    PLP_LOCK(&plp->tmoutlock);
    tmoutid = timeout(plp_sigdelay_tmout, plp, ticks);
    if (sv_wait_sig(&plp->sv_tmout, PZERO, &plp->tmoutlock, 0) < 0) {
        PLP_LOCK(&plp->xlock);
        if (tmoutid)
            untimeout(tmoutid);
	PLP_UNLOCK(&plp->xlock);
        return 1;
    }
    return 0;
}

/*
 * plp_sigdelay_tmout()
 *
 * Description:
 *    Timeout routine for plp_sigdelay
 */
static void
plp_sigdelay_tmout(plp_t *plp)
{
    PLP_LOCK(&plp->tmoutlock);
    sv_signal(&plp->sv_tmout);
    PLP_UNLOCK(&plp->tmoutlock);
}

/*
 * plp_prlock()
 *
 * Description:
 *    Acquire lock for plp/pfd accessing with different protocols
 *    plp(printer): CMPTBL/ECP; pfd(parallel floppy): EPP
 */
static int 
plp_prlock (
    plp_t *plp, char devtype)
{
    int erc = EIO;

    PLP_LOCK(&plp->plock);
    if (!(plp->state & PLP_PLOCK) || ((plp->state & PLP_PFD) == devtype)) {
	plp->state |= (PLP_PLOCK | devtype);
	plp->prlcks++;
	erc = 0;
    }
    PLP_UNLOCK(&plp->plock);

    return (erc);
}


/* 
 * plp_prunlock()
 *
 * Description:
 *    Release lock for plp/pfd accessing with different protocols
 *    plp(printer): CMPTBL/ECP; pfd(parallel floppy): EPP
 */
static void 
plp_prunlock (
    plp_t *plp, char devtype)
{
    PLP_LOCK(&plp->plock);
    if ((plp->state & PLP_PFD) == devtype && !--plp->prlcks)
	plp->state &= ~(PLP_PLOCK | devtype);
    PLP_UNLOCK(&plp->plock);
}


/*
 * plp_lock()
 *
 * Description:
 *    acquire read/write/dma access lock
 *    returns: 1: catches a signal before getting the lock
 *             0: gets the lock successfully
 */
static int 
plp_lock (sema_t *sema)
{
    return ((psema(sema, PZERO + 1) < 0) ? 1 : 0);
}


/* 
 * plp_unlock()
 *
 * Description:
 *    Release read/write/dma access lock
 *    returns: 1: wakes up another process waiting on the lock
 *             0: no process gets awakened
 */
static int 
plp_unlock (sema_t *sema)
{
    return (vsema(sema));
}


/* 
 * plp_timeout()
 *
 * Description:
 *    Timeout handling routine called when DMA transaction time out.
 */
static void
plp_timeout (
    void *plpn)
{
    plp_t *plp = plpn;
    time_t tmout;

    if (!(tmout = (MACE_ISA_RD(PLP_DMA_CTRL_REG) & MACE_DMA_INPUT) ?
	plp->wtmout : plp->rtmout))
	return;

    if ((tmout = plp->dma_tmstamp + tmout - lbolt) > 0) {
	    /* not timeout yet, wait for a while. (unlikely happen?) */
	    plp->tmoutid = timeout(plp_timeout, plp, tmout);
	    return;
    }
    /* DMA transfer timed out, now prevent further interrupts */
    PLP_LOCK(&plp->mlock);

    PLP_DMA_STOP(plp);

    if (MACE_ISA_RD(PLP_DMA_CTRL_REG) & MACE_DMA_INPUT) {
	/* partial read:
	 * abort the current context, collect all received data,
	 * reset DMA, and wake up the process waiting for rbufs.
	 */
	plp_rbflush(plp);
	plp_reset_dmacntxts(plp);
	plp_unlock(&plp->dlock);

#ifdef _RDRESUME
	/* release dmacntxt if there is any waiting write, otherwise,
	 * resume read process if the port is open for reading.
	 */
	if (!plp_unlock(&plp->dmacntxt) && (plp->state & PLP_RDOPEN)) {
	    plp_lock(&plp->dlock);
	    plp_read_resume(plp);
	}
#endif
    }
    else {
	/* partial write:
	 * adjust b_resid and set an error flag.
	 */
#ifdef _REWORK
	uchar_t cntxt;
	cntxt = DMA_INUSE_CONTEXT(plp->diag);
	plp->bp->b_resid -= (CONTEXT_BCOUNT(cntxt) - DMA_RESID(plp->diag));
#endif
	plp->bp->b_error = EAGAIN;
	plp_unlock(&plp->dlock);

#ifdef _RDRESUME
	/* resume a read process if the port is opened for read.
	 * since the receiver may be waiting for transmitting.
	 */ 
	if (plp->state & PLP_RDOPEN)
	    plp_read_resume(plp);
	else
	    plp_unlock(&plp->dlock);
#endif
    }
    PLP_UNLOCK(&plp->mlock);
    plp->tmoutid = 0;
    plp_clrcntxt(plp);
}


/* 
 * plp_rbflush()
 *
 * Description:
 *    Flush current context, wake up slepping process to get partial
 *    received data in rbuf. This is called due to read time out.
 */
static void
plp_rbflush(plp_t *plp)
{
    plp_rbuf_t *rbp;
    uchar_t cntxt;

    if (PLP_DMA_ACTIVE(MACE_ISA_RD(PLP_DMA_DIAG_REG))) {
	cntxt = PLP_DMA_INUSE_CTX(MACE_ISA_RD(PLP_DMA_DIAG_REG));
	plp->rbufctrl |= cntxt;
	plp->nxtcntxt = cntxt;
	rbp = &plp->rbuf[cntxt];
	rbp->bcount -= PLP_DMA_RESID(MACE_ISA_RD(PLP_DMA_DIAG_REG));
	bcopy(rbp->vaddr, rbp->usrbuf, rbp->bcount);
	sv_signal(&plp->dmaxfr);
    }
}


/*
 * plp_reset()
 *
 * Description:
 *    Reset the parallel peripheral and activate MACE DMA 
 */
static void
plp_reset(plp_t *plp)
{
    if (!plp_lock(&plp->dlock)) {

	/* select & reset the device */
	LPT_CTRL_WR(PLP_LPT_DCR_REG, DCR_SLCTIN);
	LPT_DELAY(1);
	LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_SLCTIN | DCR_AFD | DCR_NINIT));

	/* reset parallel DMA and make all contexts available */
	plp_reset_dmacntxts(plp);

	/* activate MACE DMA and ISA BUS */
	MACE_ISA_CLR(PLP_DMA_CTRL_REG, MACE_DMA_RESET);
	MACE_ISA_CLR(ISA_RING_RESET_REG, ISA_DEV_RESET);

	plp_unlock(&plp->dlock);
    }
    return;
}


/******************************************************************************
 *            IEEE P1284 Peripheral Auxiliary Driver Routines                 *
 ******************************************************************************
 */

/*
 * plp_negotiate()
 *
 * Description:
 *    Negotiation phase to decide operation mode
 */
static int
plp_negotiate (plp_t *plp, char extreq)
{
    uchar_t dsr;
    int i;

    if (plp->phase != CMPTBL_IDLE_PHASE)
	return (0);

    /* host places the Extensibility Request Value on data bus */
    LPT_CTRL_WR(PLP_LPT_DATA_REG, extreq);

    /* host requests IEEE P1284 support:
     * -> nSelectIn(H), nAutoFd(L)
     */
    LPT_CTRL_WR(PLP_LPT_DCR_REG,
	((LPT_CTRL_RD(PLP_LPT_DCR_REG) & ~DCR_SLCTIN) | DCR_AFD));

    /* peripheral responses IEEE P1284 support:
     * -> Select(H), PError(H), nFault(H), nAck(L)
     */
    for (i = 0; (LPT_CTRL_RD(PLP_LPT_DSR_REG) & (DSR_ACKZ | DSR_PE | DSR_SLCT |
	DSR_FAULTZ)) != (DSR_PE | DSR_SLCT | DSR_FAULTZ); i++) {
	if (i > LPT_TM_MAX_WAIT)
		goto termination_phase;
	else if (i > LPT_TM_LONG_WAIT)
		LPT_DELAY(1);
    }

    /* host strobes Extensibility Request:
     * nStrobe(L)
     */
    LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_STB);

    /* wait min nStrobe pulse width 0.5us */
    US_DELAY(1);	/* 1 us delay */

    /* host acks the IEEE P1284 support request to peripheral:
     * -> nStrobe(H), nAutoFd(H)
     */
    LPT_CTRL_CLR(PLP_LPT_DCR_REG, (DCR_STB | DCR_AFD));

    /* peripheral responses extreq support:
     * -> ECP: Select(H), PError(L), Busy(L)
     * -> EPP: Select(H)
     */
    for (i = 0; !(dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG)) ||
	 (!(extreq == EXTREQ_ECP_MODE &&
	   (dsr & (DSR_BUSYZ | DSR_PE | DSR_SLCT)) == (DSR_BUSYZ | DSR_SLCT))
	   &&
	  !(extreq == EXTREQ_EPP_MODE &&
	   (dsr & DSR_SLCT))); i++) {

	if (i > LPT_TM_MAX_WAIT)
		goto termination_phase;
	else if (i > LPT_TM_LONG_WAIT)
		LPT_DELAY(1);
    }

    /* peripheral indicates 4 status signals available:
     * -> nAck(H)
     */
    for (i = 0; !(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_ACKZ); i++) {
	if (i > LPT_TM_MAX_WAIT)
		goto termination_phase;
	else if (i > LPT_TM_LONG_WAIT)
		LPT_DELAY(1);
    }

    /* enter set up phase */

    /* peripheral ack ECP mode operation:
     * -> PError(H), Busy(L), nFault(L)
     */
    if (extreq == EXTREQ_ECP_MODE) {
	for (i = 0; !(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_PE); i++) {
		if (i > LPT_TM_MAX_WAIT)
			goto termination_phase;
		else if (i > LPT_TM_LONG_WAIT)
			LPT_DELAY(1);
	}
	/* host set data xfer
	 * -> nAutoFd(H)
	 */
	LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_AFD);

	plp->phase = ECP_IDLE_PHASE;
    }
    else
	plp->phase = EPP_IDLE_PHASE;

    /* success */
    return (plp->phase);

    /* fail */
    termination_phase:
    plp_phase_reset(plp, extreq);

    return (0);
}


/*
 * plp_phase_reset()
 *
 * Description:
 *    IEEE P1284 terminate phase
 */
static void
plp_phase_reset(plp_t *plp, char extreq)
{
    int i;

    if (extreq == EXTREQ_ECP_MODE || plp->phase == ECP_IDLE_PHASE) {
	/* host: terminate IEEE P1284 support request by
	 *     assert:   Active(nSelectIn):L
	 *     deassert: HostBusy(nAutoFd):H
	 */
	LPT_CTRL_WR(PLP_LPT_DCR_REG, ((LPT_CTRL_RD(PLP_LPT_DCR_REG) |
		DCR_SLCTIN) & ~DCR_AFD));

	/* device: places data bus in a high impedance state and
	 *      deassert: PeriphAck(Busy):H, nDataAvail(nFault):H
	 *
	 * device: acknowledges the request by
	 *     assert:   PeriphClk(nAck):L
	 *     set Xflag(Select) to its opposite sense
	 */
	for (i = 0; ((LPT_CTRL_RD(PLP_LPT_DSR_REG) & (DSR_BUSYZ | DSR_FAULTZ |
	     DSR_ACKZ)) != DSR_FAULTZ); i++) {
		if (i > LPT_TM_MAX_WAIT)
			goto phase_reset_done;
		else if (i > LPT_TM_LONG_WAIT)
			LPT_DELAY(1);
	}

	/* host: handshakes by
	 *     deassert: HostBusy(nAutoFd):L
	 */
	LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_AFD);

	/* device: set Xflag(Select), nReverseReq(nInit), AckDataReq(PError),
	 *         and nDataAvail(nFault) their to Compatiblity mode values.
	 *
	 * device: completes handshake by
	 *     deassert: PeriphClk(nAck):H
	 */
	for (i = 0; !(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_ACKZ); i++) {
		if (i > LPT_TM_MAX_WAIT)
			goto phase_reset_done;
		else if (i > LPT_TM_LONG_WAIT)
			LPT_DELAY(1);
	}

	/* host: completes handshake by
	 *     deassert: HostBusy(nAutoFd):H
	 */
	LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_AFD);

	/* device: updates PeriphAck(Busy) to its Compatiblity mode values.
	 */
    }
    else if (extreq == EXTREQ_EPP_MODE || plp->phase == EPP_IDLE_PHASE) {
	/* (event #68, #69)
	 * host: terminate IEEE P1284 EPP support request by
	 *     assert:   nInit:L
	 *     deassert: nInit:H
	 */
	LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_NINIT);
	LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_NINIT);
    }

    phase_reset_done:
    plp->phase = CMPTBL_IDLE_PHASE;
}


/*
 * plp_fwd2rvs()
 *
 * Description:
 *    IEEE P1284 ECP forward to reverse phase
 */
static int
plp_fwd2rvs (plp_t *plp)
{
    if (plp->phase != ECP_IDLE_PHASE)
	return (0);

    /* forward to reverse phase */
    /* host: places the reverse reguest
     *     assert:   HostAck(nAutoFd):L
     *               wait for setup time
     *     assert:   nReverseReq(nInit):L
     */
    LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_AFD);
    US_DELAY(1); /* may not need if two RDs are used */
    LPT_CTRL_CLR(PLP_LPT_DCR_REG, DCR_NINIT);

    /* device: acknowledges the reversal
     *     deassert: nAckReverse(PError):L
     */
    if (LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_PE)
	return (0);

    return (plp->phase = ECP_RVS_IDLE_PHASE);
}


/*
 * plp_rvs2fwd()
 *
 * Description:
 *    IEEE P1284 ECP reverse to forward phase
 */
static int
plp_rvs2fwd (plp_t *plp)
{
    if (plp->phase != ECP_RVS_IDLE_PHASE && plp->phase != RVS_XFR_PHASE)
	return (0);

    /* reverse to forward phase */

    /* host: initiates a bus reversal
     *     deassert: nReverseReq(nInit):H
     */
    LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_NINIT);

    /* device: terminates any ongoing transfer, tri-states the data bus
     *     deassert:   PeriphClk(nAck):H
     */
    if (!(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_ACKZ))
	return (0);

    US_DELAY(1); /* may not need if two RDs are used */

    /* device: acknowledges the bus has been relinquished
     *     assert:   nAckReverse(PError):H
     */
    if (!(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_PE))
	return (0);

    return (plp->phase = ECP_IDLE_PHASE);
}


/*
 * plp_war_delay()
 *
 * Description:
 *   Work-around delay for some printers, e.g. HP DJ1600C.
 *   delay plpwardely * 1ms to prevent possible data corruption in
 *   compatibilty mode, if printer is still busy right before write
 */
static void
plp_war_delay (plp_t *plp, int ms)
{
    timespec_t ts = {0, 1000000};

    if (plp->opmode == PLP_CDF_MODE) {
	while (ms-- && !(LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_BUSYZ))
		NS_DELAY(&ts);
    }
}


#ifdef _PHASE_ABORT
/*
 * plp_phase_abort()
 *
 * Description:
 *    IEEE P1284 immediate abort phase
 */
static void
plp_phase_abort (plp_t *plp)
{
    /* host: requests IEEE P1284 immediate termination by
     *     assert: Active(nSelectIn):L
     */
    LPT_CTRL_SET(PLP_LPT_DCR_REG, DCR_SLCTIN);
}
#endif


#ifdef _RDRESUME
/*
 * plp_read_resume()
 *
 * Description:
 *    Continue read process from interrupt level
 */
static void
plp_read_resume (plp_t *plp)
{
    /* find next available contexts. if all contexts are either being
     * consumed by hardware or being drained by software driver
     * then check back in less than a second.
     */
    if (!plp->cntxts)
	(void) plp_unlock(&plp->dlock);
    else {
	/* make contexts and start a DMA input
	 */
	/* (void) plp_dmaxfr(plp, plp->rbuf); */
	/* plp_dma(plp, B_READ); */
    }
}
#endif


/******************************************************************************
 *           Parallel Floppy Drive Support Routines                           *
 ******************************************************************************
 */

/*
 * plpfdc_cpp()
 *
 * Description:
 *    Send out a packet with IEEE-1284.P3 Command Packet Protocol (CPP)
 */
static int
plpfdc_cpp (
    char cmd,
    char devno)
{
    char dsr;

    /* sanity check, reset OnSpec state machine */
    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0xfe);
    US_DELAY(LPT_TM_SGNL_HOLD);

    /* CPP packet for daisy chain device: "AA 55 00 FF 87 78 cmd FF" */
    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0xaa);	/* preamble (1st byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);
    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0x55);	/* preamble (2nd byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);
    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0x00);	/* func. id (1st byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);
    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0xff);	/* func. id (2nd byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);

    if (cmd != PLP_CPP_CMD_ASSGN &&
        ((dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG)) & 0xf0) !=
	(DSR_BUSYZ | DSR_PE | DSR_SLCT))
	return (0);

    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0x87);	/* func. id (3rd byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);
 
    if (cmd != PLP_CPP_CMD_ASSGN &&
	((dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG)) & 0xf8) !=
	(DSR_ACKZ | DSR_SLCT | DSR_FAULTZ))
	return (0);

    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0x78);	/* func. id (4th byte) */
    US_DELAY(LPT_TM_SGNL_HOLD);

    /* assign id to all devices and detect the last one */
    if (cmd == PLP_CPP_CMD_ASSGN) {
	int i;
	for (i = 0; i < PLP_CPP_MAX_DEV; i++, devno++) {

		dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG);

		LPT_CTRL_WR(PLP_LPT_DATA_REG, (cmd | devno));
		US_DELAY(LPT_TM_SGNL_HOLD);
		LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB));
		US_DELAY(LPT_TM_SGNL_HOLD);
		LPT_CTRL_WR(PLP_LPT_DCR_REG, DCR_NINIT);
		US_DELAY(LPT_TM_SGNL_HOLD);

		if (dsr & PLP_CPP_LAST_DEV)
			break;
	}
    }
    /* select device #devno */ 
    else if (cmd == PLP_CPP_CMD_SLCT || cmd == PLP_CPP_CMD_DESLCT) {

	/* check if the device has been selected */
	if ((dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG)) & DSR_FAULTZ)
		return (PLP_CPP_ALRDY_SLCT);

	if (((dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG)) & 0xb0) !=
	    (DSR_BUSYZ | DSR_PE | DSR_SLCT))
		return (0);

	/* send out cmd */
	LPT_CTRL_WR(PLP_LPT_DATA_REG, (cmd | devno));
	US_DELAY(LPT_TM_SGNL_HOLD);

	/* strobe cmd out and get status */
	LPT_CTRL_WR(PLP_LPT_DCR_REG, (DCR_NINIT | DCR_STB));
	US_DELAY(LPT_TM_SGNL_HOLD);

	/* get status */
	dsr = LPT_CTRL_RD(PLP_LPT_DSR_REG) &
	       (PLP_CPP_CHIP_SLCT | PLP_CPP_LAST_DEV);

	LPT_CTRL_WR(PLP_LPT_DCR_REG, DCR_NINIT);
	US_DELAY(LPT_TM_SGNL_HOLD);
    }

    LPT_CTRL_WR(PLP_LPT_DATA_REG, 0xff);	/* terminator */
    US_DELAY(LPT_TM_SGNL_HOLD);

    return (dsr);
}


/*
 * plpfdc_open()
 *
 * Description:
 *    Open parallel floppy device
 */
int
plpfdc_open ()
{
    plp_t *plp = plpinfo;

    /* check if the driver exist and at valid state */
    if (!plpinfo || !(plp->state & PLP_ALIVE))
	return (ENXIO);

    /* get mutual exclusive port lock */
    plp->sav_state = plp->state;
    if (plp_prlock(plp, PLP_PFD))
	return (EBUSY);

    /* open parallel port with EPP protocol */
    plp->sav_opmode = plp->opmode;
    if (plp->opmode == PLP_ECP_MODE || plp->opmode == PLP_EPP_MODE)
	/* reset the current opmode */
	plp_phase_reset(plp, 0);

    /* must be in standard centronics mode before turnning on OpSpec */
    PLP_SET_OPMODE(plp, PLP_CMPTBL_MODE);

    /* enable OpSpec 90C26 control chip */
    if (!PLP_ONSPEC_ENBL(0)) {
	plp_prunlock(plp, PLP_PFD);
	plp->opmode = plp->sav_opmode;
	plp->state = plp->sav_state;
	return (ENXIO);
    }

    /* OnSpec 90C26 don't support P1284 negotiation
       if (!plp_negotiate(plp, EXTREQ_EPP_MODE))
	   return(0);
    */

    /* set EPP mode on OnSpec 90C26 */
    PLP_ONSPEC_STD_WR(PLP_ONSPEC_CFG1_REG, (PLP_ONSPEC_EPP | PLP_ONSPEC_IDE8));

    /* set EPP mode on SuperIO */
    PLP_SET_OPMODE(plp, PLP_EPP_MODE);

    /* reset OnSpec fifo control reg */
    PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, PLP_ONSPEC_FIFO_OFF);

#ifdef _PFD_VER_CHK
    /* read OnSpec 90C26 version */
    ver = PLP_EPP_RD(PLP_ONSPEC_VER_REG);
#endif

    /* no data in rbufs */
    plp->rbufctrl = 0;

    /* support plp[0] only */
    plp->fdcbuf.b_edev = 0;

    plp->phase = EPP_IDLE_PHASE;
    plp->state |= PLP_ALIVE | PLP_WROPEN | PLP_RDOPEN | PLP_OCCUPIED | PLP_INUSE;
    return (0);
}


/*
 * plpfdc_close()
 *
 * Description:
 *    Open parallel floppy device
 */
int
plpfdc_close ()
{
    plp_t *plp = plpinfo;

    /* disable OpSpec 90C26 control chip */
    PLP_ONSPEC_REL(0);

    /* reset phase */
    plp_phase_reset(plp, 0);
    
    /* reset opmode */
    PLP_CLR_OPMODE(plp);

    /* restore opmode and redo auto-negotiation */
    if ((plp->opmode = plp->sav_opmode) != PLP_NULL_MODE) {
	if ((plp->opmode == PLP_ECP_MODE || plp->opmode == PLP_EPP_MODE) &&
	    !plp_negotiate(plp, (plp->opmode == PLP_ECP_MODE ?
	     EXTREQ_ECP_MODE : EXTREQ_EPP_MODE))) {
		cmn_err(CE_WARN,"plp: cannot restore %s mode, set to compatible mode\n",
			(plp->opmode == PLP_ECP_MODE) ? "ECP" : "EPP");
		plp->opmode = PLP_CDF_MODE;
	}
	PLP_SET_OPMODE(plp, plp->opmode);
    }

    /* release the port lock */
    plp_prunlock(plp, PLP_PFD);

    /* restore state and close plp if not in use */
    if (!((plp->state = plp->sav_state) & (PLP_INUSE | PLP_OCCUPIED)))
	plpclose(plp->devt);

    return (0);
}


/*
 * plpfdc_fifo_ctrl()
 *
 * Description:
 *    OnSpec fifo control. read fifo level or turn on/off fifo.
 */
int
plpfdc_fifo_ctrl (int op, int rd, int bcount)
{
    switch (op) {
	case PLP_FDC_FIFO_CTRL_LEVEL:
	     {
		timespec_t ts = {0, 1000000};
		int level, waits;

		/* wait for DMA start */
		waits = PLP_FDC_TM_FIFOWAIT;
		level = PLP_ONSPEC_LEVEL();
		while (level == PLP_ONSPEC_LEVEL()) {
			if (!waits--)
				return (level);
			LPT_DELAY(1);
		}
		/* watch fifo level */
		waits = PLP_FDC_TM_FIFOWAIT;
		level = PLP_ONSPEC_LEVEL();
		while (waits-- && ((rd && level < bcount) || (!rd && level))) {
			NS_DELAY(&ts);
			level = PLP_ONSPEC_LEVEL();
		}
		return (level);
	     }

	/* enable external fifo and set I/O direction */
	case PLP_FDC_FIFO_CTRL_ON:
	     {
		char cfg2;

		cfg2 = PLP_ONSPEC_FIFO_ENBL | (rd ? PLP_ONSPEC_FIFO_RD : 0);
		PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, cfg2);
		return (cfg2);
	     }

	/* disable external fifo */
	case PLP_FDC_FIFO_CTRL_OFF:
	     {
		PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, PLP_ONSPEC_FIFO_OFF);
		return (PLP_ONSPEC_FIFO_OFF);
	     }
	default:
		return (0);
    }
}


/*
 * plpfdc_read()
 *
 * Description:
 *    Read data from OnSpec controller/floppy device
 */
int
plpfdc_read (char fdcr, char *buf, int bcount, int *erc, char fifo, char cfg2)
{
    if (fifo) { /* read from OnSpec fifo */
	plp_t *plp = plpinfo;
	int level;

	/* wait for fifo level being filled */
	if ((level = plpfdc_fifo_ctrl(PLP_FDC_FIFO_CTRL_LEVEL, B_READ,
	    bcount)) < bcount) {
		PLP_EPP_WR(PLP_ONSPEC_CMD_REG, PLP_FDC_TC);
		*erc = plp->fdcbuf.b_error = EIO;
	}

	if (level) { /* transfer the data */	
		LPT_CTRL_WR(PLP_EPP_ADDR_REG, PLP_ONSPEC_FIFO_REG);

		plp->fdcbuf.b_flags = B_READ;
		plp->fdcbuf.b_bcount = min(bcount, level);
		plp->fdcbuf.b_dmaaddr = buf;
		plp->fdcbuf.b_error = 0;
		plp_io(&plp->fdcbuf);

#ifdef ONSPEC_BUG_RD_WAR
		if (!plp->fdcbuf.b_error) {
		    char *p, *q;
		    int i, bcnt;

		    for (p = buf, i = 0; i < NPFDRB && !plp->fdcbuf.b_error;
			 p = plp->pfdrbuf[0], i++) {

			/* reset the fifo pointer */
			PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, PLP_ONSPEC_FIFO_OFF);
			PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, (PLP_ONSPEC_FIFO_ENBL |
				 PLP_ONSPEC_FIFO_HALT | PLP_ONSPEC_FIFO_RD));

			/* read again to check buffer */
			LPT_CTRL_WR(PLP_EPP_ADDR_REG, PLP_ONSPEC_FIFO_REG);
			plp->fdcbuf.b_dmaaddr = plp->pfdrbuf[i];
			plp->fdcbuf.b_error = 0;
			plp_io(&plp->fdcbuf);

			/* compare two buffers */
			for (bcnt = plp->fdcbuf.b_bcount, q = plp->pfdrbuf[i];
			     bcnt && *p++ == *q++; bcnt--);

			/* chekc if any error */
			if (bcnt <= 0 && i++ > 0)
			     /* buf is bad, use pfdrbuf */
			     bcopy(plp->pfdrbuf[0], buf, plp->fdcbuf.b_bcount);
		    }
		    /* last hope, check if buf is really ok */
		    if (bcnt > 0)
			for (bcnt = plp->fdcbuf.b_bcount, p = buf,
			     q = plp->pfdrbuf[1]; bcnt && *p++ == *q++; bcnt--);

		    if (!plp->fdcbuf.b_error && (level < bcount || bcnt > 0))
			plp->fdcbuf.b_error = EIO;
		}
#else
		if (!plp->fdcbuf.b_error && level < bcount)
			plp->fdcbuf.b_error = EIO;
#endif
		*erc = plp->fdcbuf.b_error;
	}
	return (level);
    }
    else { /* read a byte from fdc register */

	/* halt fifo dma first */
	PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, (cfg2 | PLP_ONSPEC_FIFO_HALT));

	/* specify the fd control reg */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, fdcr);

	/* read in the byte */
	*buf = PLP_EPP_RD(PLP_ONSPEC_DATA_REG);

	/* re-enable DMA */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, PLP_FDC_AEN);

	/* restore OnSpec cfg2 reg */
	PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, cfg2);

	return (1);
    }
}


/*
 * plpfdc_write()
 *
 * Description:
 *    Write data to OnSpec controller/floppy device
 */
int
plpfdc_write (char fdcr, char *buf, int bcount, int *erc, char fifo, char cfg2)
{
    if (fifo) { /* write to OnSpec fifo */
	plp_t *plp = plpinfo;

	LPT_CTRL_WR(PLP_EPP_ADDR_REG, PLP_ONSPEC_FIFO_REG);

	plp->fdcbuf.b_flags = B_WRITE;
	plp->fdcbuf.b_bcount = bcount;
	plp->fdcbuf.b_dmaaddr = buf;
	plp->fdcbuf.b_error = 0;
	plp_io(&plp->fdcbuf);
	*erc = plp->fdcbuf.b_error;

	return (bcount);
    }
    else { /* write a byte to fdc register */

	/* halt fifo dma first */
	PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, (cfg2 | PLP_ONSPEC_FIFO_HALT));

	/* specify the fd control reg */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, fdcr);

	/* write out the byte */
	PLP_EPP_WR(PLP_ONSPEC_DATA_REG, *buf);
	
	/* re-enable DMA */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, PLP_FDC_AEN);

	/* restore OnSpec cfg2 reg */
	PLP_EPP_WR(PLP_ONSPEC_CFG2_REG, cfg2);

	return (1);
    }
}


/*
 * plpfdc_chkirq()
 *
 * Description:
 *    Check interrupt from floppy controller
 */
int
plpfdc_chkirq ()
{
    int irq;

    /* temporarily set TI I/O chip and OnSpec 90C26 to standard mode */
    LPT_CTRL_WR(PLP_LPT_ECR_REG, (LPT_CTRL_RD(PLP_LPT_ECR_REG) & ~PLP_OP_MODE));
    PLP_ONSPEC_STD_WR(PLP_ONSPEC_CFG1_REG, PLP_ONSPEC_IDE8);

    /* read busy bit */
    irq = LPT_CTRL_RD(PLP_LPT_DSR_REG) & DSR_BUSYZ;

    /* set TI I/O chip and OnSpec 90C26 back to EPP mode */
    PLP_ONSPEC_STD_DATA_WR(PLP_ONSPEC_EPP | PLP_ONSPEC_IDE8);
    LPT_CTRL_WR(PLP_LPT_ECR_REG, (LPT_CTRL_RD(PLP_LPT_ECR_REG) | PLP_EPP_MODE));

    return (irq);
}


/*
 * plpfdc_hdout()
 *
 * Description:
 *    Read 16-bit data port to check HD-OUT (high density media out) signal
 */
int
plpfdc_hdout ()
{
    u_char mout;

    /* temporarily set to EPP & IDE16 mode */
    PLP_EPP_WR(PLP_ONSPEC_CFG1_REG, (PLP_ONSPEC_EPP | PLP_ONSPEC_IDE16));

    /* read HD-OUT bit from 16-bit data port */
    mout = PLP_EPP_RD(PLP_ONSPEC_DATA_REG); /* lsw */
    mout = PLP_EPP_RD(PLP_ONSPEC_DATA_REG); /* msw */

    /* restore to EPP & IDE8 mode */
    PLP_EPP_WR(PLP_ONSPEC_CFG1_REG, (PLP_ONSPEC_EPP | PLP_ONSPEC_IDE8));

    return (mout & PLP_FDC_HD_OUT);
}


/*
 * plpfdc_dmatc()
 *
 * Description:
 *    Enable/disable hardware DMA TC
 */
void
plpfdc_dmatc (int bcount)
{
    if (bcount > 0) { /* set up DMA bcount */
	/* temporarily set to EPP & IDE16 mode */
	PLP_EPP_WR(PLP_ONSPEC_CFG1_REG, (PLP_ONSPEC_EPP | PLP_ONSPEC_IDE16));

	/* write DMA-write bytes count to 16-bit data port */
	PLP_EPP_WR(PLP_ONSPEC_DATA_REG, (bcount & 0xff));
	PLP_EPP_WR(PLP_ONSPEC_DATA_REG, ((bcount >> 8) & 0xff));

	/* restore to EPP & IDE8 mode */
	PLP_EPP_WR(PLP_ONSPEC_CFG1_REG, (PLP_ONSPEC_EPP | PLP_ONSPEC_IDE8));
    }
    else if (bcount < 0) /* enable DMA bcount & TC */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, PLP_FDC_DMATC);

    else /* disable DMA bcount & TC */
	PLP_EPP_WR(PLP_ONSPEC_CMD_REG, PLP_FDC_AEN);
}

#endif /* IP32 */
