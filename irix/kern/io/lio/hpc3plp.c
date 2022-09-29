#ident	"io/lio/hpc3plp.c:  $Revision: 1.42 $"

#if IP22 || IP26 || IP28
/* 
 * hpc3plp.c - driver for the IP22/IP26/IP28 parallel port
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <ksys/vfile.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/systm.h>

#include <sys/ioctl.h>
#include <sys/debug.h>
#include <sys/mload.h>

#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/hpc3plpreg.h>
#include <sys/plp.h>

#include <sys/ddi.h>

#include <sys/pda.h>

int plpdevflag = 0;

char *plpmversion = M_VERSION;

extern int cachewrback;

#ifdef LATER
/* software copy of hardware write-only registers */
uint _hpc3_pbus_ctrl;
#endif

#ifdef DEBUG
int plp_debug = 1;
#define dprintf if (plp_debug) printf
void plp_show_dl(__uint32_t);
#endif

/* size of space for memory descriptor nodes, add one in case one of
 * the descriptors crosses a page boundary and needs to be thrown away.
 */
#define MDMEMSIZE	((NMD + 1) * sizeof(memd_t))

/* list of all free memory descriptors (K0 addresses) 
 */
static memd_t *free_mds = 0; 
static mutex_t free_mds_mutex;
static sv_t free_mds_sv;

static mutex_t free_rbufs_mutex;
static sv_t free_rbufs_sv;

/* memory for receive buffers and memory descriptors */
static caddr_t rbufmem;
static caddr_t mdmem;
static int plpinitfail;

#define LOCK(m)		mutex_lock(m, PZERO)
#define UNLOCK(m)	mutex_unlock(m)

/* insert/remove node _n_ into/from list _l_
 */
#define EMPTYNODE(n) (KDM_TO_IOPHYS(n)==0)
#define PUTNODE(l,n) (n)->memd_forw = KDM_TO_IOPHYS(l); (l) = (n);
#define GETNODE(l,n) (n) = (l); if (!EMPTYNODE(l)) \
		     (l) = (memd_t *)IOPHYS_TO_K0((l)->memd_forw);
#define LPUTNODE(l,n) { PUTNODE(l,n); }
#define LGETNODE(l,n) { GETNODE(l,n); }

/* Macro to get around hpc3's pio bug for nbdp and cbp,
 * where adr could be either addresses. This macro must
 * be called each time pio is done to nbdp or cbp.
 * This is somewhat questionable...
 */
#define GETDESC(adr)          	( _hpc_spl = spl7(),		\
                                  _bdp = (unsigned)(adr),	\
				  _bdp = (unsigned)(adr),	\
                                  splx(_hpc_spl), _bdp )

plp_t *plpport[NPLP];

extern int plp3probe[];		/* from master.d/hpc3plp */

static int plpio (struct buf *);
struct eframe_s;
static void plpintr (__psint_t, struct eframe_s *);
static void plp_dma (plp_t *, __uint32_t, int);
static void plp_stop_dma (plp_t *);
static void plp_reset(plp_t *);
static plp_regs_t *plp_probe (int);
static void plp_read_flush (plp_t *);
static int plp_read_copy (plp_t *, caddr_t, int);
static int plp_queue (memd_t **, void *, int);
static void plp_free (memd_t *);
static void rbuf_init(void);
static void rbuf_save (plp_t *);
static void del_rbuf (memd_t *);
static memd_t *new_rbuf (int);
static int plp_lock(sema_t *);
static int plp_unlock(sema_t *);
static int plp_mem_init(void);
static void plp_mem_free(void);

/*
 * plpinit - initialize parallel port driver
 */
void
plpinit (void)
{
    memd_t *node_space;
    __psunsigned_t end_space;
    plp_regs_t *plpaddr;
    plp_t *p;
    int plp;

    /* cpu-dependent allocation of space for memory descriptors, 
     * receive buffers, and transmit buffers (if needed).  Creates
     * mdmem, rbufmem, and lmembufs[NXMD].  Returns ENOMEM if memory 
     * cannot be allocated.
     */
    if (plp_mem_init()) {
	cmn_err(CE_WARN, "plp: initialization failed, opens will be refused");
	plpinitfail = 1;
	return;
    }

    /*
     * Allocate memory descriptor list resources.
     */
    mutex_init(&free_mds_mutex, MUTEX_DEFAULT, "mds_mutex");
    sv_init(&free_mds_sv, SV_DEFAULT, "mds_sv");

    /* initialize free_mds to contain all memory descriptors allocated
     * from mdmem.  
     */
    node_space = (memd_t *)mdmem;
    end_space = (__psunsigned_t)mdmem + MDMEMSIZE;

    while (((__psunsigned_t)node_space + sizeof (memd_t)) <= end_space) {
	/* throw out descriptors that cross page boundaries
	 */
	if (btoc(node_space) == btoc((__psint_t)node_space+sizeof(memd_t)-1)){
		LPUTNODE (free_mds, (memd_t *)PHYS_TO_K0(kvtophys(node_space)));
	}
	node_space++;
    }

    /* allocate buffers for read data and initialize
     * read data list
     */
    rbuf_init();

    /* initialize parallel port struct
     */
    for (plp = 0; plp < NPLP; ++plp) {
	NEW(plpport[plp], plp_t *);
	p = plpport[plp];
	p->state = 0;
	p->timeoutid = 0;
	/* unit structure lock */
	mutex_init(&p->plpmutex, MUTEX_DEFAULT, "plpmutex");

	/* read/write/dma channel locks */
	initnsema(&p->rlock, 1, "rlock");
	initnsema(&p->wlock, 1, "wlock");
	initnsema(&p->dlock, 1, "dlock");

	/* interrupt sync */
	initnsema(&p->dmasema, 0, "dmasema");

	if (plpaddr = plp_probe(plp)) {
	    /* set interrupt vector depending on hpc number
	     */
	    ASSERT(plp == 0);		/* allow only parallel port 0 */
	    setlclvector (plp ? PLP_VEC_XTRA : PLP_VEC_NORM, plpintr, 0);
	    sethpcdmaintr(7, plpintr, 0);

	    p->regs = plpaddr;
	    p->regs->extreg.tval1 = PI1_TVAL1;
	    p->regs->extreg.tval2 = PI1_TVAL2;
	    p->regs->extreg.tval3 = PI1_TVAL3;
	    p->regs->cfgdma = 0x80108a4;
	    p->state = PLP_ALIVE;
	    p->rbufs = (memd_t *)0;
	    mutex_init(&p->rbufsmutex, MUTEX_DEFAULT, "rbufsmutex");
	    sv_init(&p->rbufssv, SV_DEFAULT, "rbufssv");
	    p->dl = 0;
	    p->rto = PLP_RTO;
	    p->wto = PLP_WTO;
	    p->timeoutid = 0;
	    plp_reset(p);
	}
    }
    return;
}

/*
 * plpstart - One time system initialization.
 *
 * We will use this entry point to set up the Hardware Graph stuff.
 */
void
plpstart(void)
{
	static int plp_init = 0;
	vertex_hdl_t vhdl;
	vertex_hdl_t pvhdl;
	vertex_hdl_t ivhdl;
	plp_t *p;
	plpif_t *pif;
	int plp;
	int i;

	/*
	 * Be really paranoid and make sure this entry point
	 * is called only once... other drivers do this so
	 * there must be a reason for it... How's that for
	 * a sheep mentality?! ;-))
	 */
	if (atomicAddInt(&plp_init, 1) > 1) {
		return;
	}

	/* make sure our connection point is present */
	if (hwgraph_traverse(hwgraph_root, EDGE_LBL_NODE_IO, &vhdl) !=
	    GRAPH_SUCCESS) {
		return;
	}

	for (i = 0; i<NPLP; i++) {
		char *edge[NPLP] = { EDGE_LBL_HPCPLP };
		/* create parallel hwgraph vertex: /hw/node/io/gio/hpc/hpc3plp */
		if (hwgraph_path_add(vhdl, edge[i], &pvhdl) !=
		    GRAPH_SUCCESS)
			return;

		p = plpport[i];
		p->cvhdl = vhdl;
		p->pvhdl = pvhdl;
		device_info_set(pvhdl, (void *)p);

		/* there are two leaf nodes for now - plp and plpbi */
		for (plp = 0; plp < 2; plp++) {
 			char *leaf[2] = { EDGE_LBL_PLP, EDGE_LBL_PLPBI };
			char mode[2] = { PLP_CENT_MODE, PLP_BI_MODE };

			if (hwgraph_char_device_add(pvhdl, leaf[plp],
			    "plp", &ivhdl) != GRAPH_SUCCESS)
				return;

			hwgraph_chmod(ivhdl, (mode_t)0666);

			NEW(pif, plpif_t *);
		        pif->plp = p;
			pif->pvhdl = ivhdl;
			pif->opmode = mode[plp];	

			device_info_set(ivhdl, (void *)pif);
		}
	}

	/* XX - The following *_add steps make the assumption of only one plp */
	ASSERT(NPLP == 1);

	/* add alias edge, /hw/parallel -> /hw/node/io/gio/hpc/hpc3plp */
	(void)hwgraph_edge_add(hwgraph_root, p->pvhdl, EDGE_LBL_PARALLEL);

	/* add plp inventory */
	device_inventory_add(pvhdl, INV_PARALLEL, INV_ONBOARD_PLP, 0, 0, 1);

}

/*
 * plpunload - check every minor device number to be sure that there
 * 	are no open references to this driver.  Return EBUSY if the driver
 * 	is still in use.  If the driver is no longer in use, then free up
 *	any allocated resources and return 0.  The higher level mload
 *	unload function will guarantee that no opens are called on this
 *	driver while it is being unloaded.
 */
int
plpunload(void)
{
    int plp;

    for (plp = 0; plp < NPLP; ++plp) {
	    LOCK(&plpport[plp]->plpmutex);
	    if (plpport[plp]->state & PLP_INUSE) {
		    UNLOCK(&plpport[plp]->plpmutex);
		    return EBUSY;
	    }
	    UNLOCK(&plpport[plp]->plpmutex);
    }

    for (plp = 0; plp < NPLP; ++plp) {
	if (plpport[plp]->state & PLP_ALIVE)
	{   
		setlclvector (plp ? PLP_VEC_XTRA : PLP_VEC_NORM, 0, 0);
		sethpcdmaintr (7, 0, 0);
		mutex_destroy(&plpport[plp]->plpmutex);
		freesema(&plpport[plp]->dmasema);
		freesema(&plpport[plp]->dlock);
		freesema(&plpport[plp]->wlock);
		freesema(&plpport[plp]->rlock);
	}
    }

    mutex_destroy(&free_mds_mutex);
    sv_destroy(&free_mds_sv);
    mutex_destroy(&free_rbufs_mutex);
    sv_destroy(&free_rbufs_sv);
    plp_mem_free();
    return 0;
}

/*
 * plpopen - open parallel port device
 */
/*ARGSUSED2*/
int
plpopen (dev_t *devp, int mode)
{
    plp_t *plp;
    plpif_t *pif;

    if (plpinitfail)
	return EIO;

    if (dev_is_vertex(*devp)) {
	pif = (plpif_t *)device_info_get(dev_to_vhdl(*devp));
	plp = (plp_t *)(pif->plp);
    } else {
	return (ENXIO);
    }

    LOCK(&plp->plpmutex);

    if (pif->opmode == PLP_BI_MODE) {
	if (!(plp->state & PLP_OCCUPIED)) {
            plp_read_flush(plp);
	}
        plp->state |= PLP_RDOPEN|PLP_RICOH|PLP_OCCUPIED;
    } else {
        plp->state |= PLP_WROPEN|PLP_OCCUPIED;
    } 

    plp->state |= PLP_INUSE;
    UNLOCK(&plp->plpmutex);

    return 0;
}

/*
 * plpclose - close parallel port device
 */
int
plpclose (dev_t dev)
{
    plp_t *plp;

    if ((plp = PLP_DEV2PLP(dev)) == NULL)
	return EINVAL;

    LOCK(&plp->plpmutex);
    plp->state &= ~(PLP_WROPEN | PLP_RDOPEN | PLP_NBIO | PLP_OCCUPIED);
    plp_stop_dma (plp);
    if (plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMAREAD) 
	rbuf_save (plp);
    plp_read_flush (plp);		/* flush read buffer */
    plp_unlock (&plp->dlock);
    plp->dl = 0;
    plp->rto = PLP_RTO;
    plp->wto = PLP_WTO;
    plp->regs->extreg.tval1 = PI1_TVAL1;
    plp->regs->extreg.tval2 = PI1_TVAL2;
    plp->regs->extreg.tval3 = PI1_TVAL3;
    plp->regs->extreg.tval4 = PI1_TVAL4;

    plp->state &= ~PLP_INUSE;
    UNLOCK(&plp->plpmutex);

    return 0;
}

/*
 * plpread - read from parallel port device
 */
int
plpread (dev_t dev, uio_t *uiop)
{
    plp_t *plp;
    plpif_t *pif;

    if (uiop->uio_resid == 0)
	return 0;

    if (dev_is_vertex(dev)) {
        pif = (plpif_t *)device_info_get(dev_to_vhdl(dev));
        plp = (plp_t *)(pif->plp);
    } else {
        return (ENXIO);
    }

    /* 
     * If using output only device but PLPIOCREAD ioctl 
     * hasn't been done then return error.
     */
    LOCK(&plp->plpmutex);
    if ((pif->opmode != PLP_BI_MODE) && (!(plp->state & PLP_RDOPEN))) {
	    UNLOCK(&plp->plpmutex);
	    return EIO;
    }
    UNLOCK(&plp->plpmutex);

    return uiophysio (plpio, NULL, dev, B_READ, uiop);
}

/*
 * plpwrite - write to parallel port device
 */
int
plpwrite (dev_t dev, uio_t *uiop)
{
    if (uiop->uio_resid == 0)
	return 0;

    return uiophysio (plpio, NULL, dev, B_WRITE, uiop);
}

/*
 * plpioctl - control parallel port device
 */
/*ARGSUSED*/
int
plpioctl (dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    plp_t *plp;
    unsigned char ext;
    int rc = 0;

    if ((plp = PLP_DEV2PLP(dev)) == NULL)
	return EINVAL;

    /*
     * Just grab around this entire switch for now.
     */
    LOCK(&plp->plpmutex);
    switch (cmd) {
    case PLPIOCRESET:
	plp_reset (plp);
	break;
    case PLPIOCSTATUS:
        ext = plp->regs->extreg.stat;
	*rvp = 0;
	if(!(ext & PI1_STAT_ERROR_N))   /* inverted! */
		*rvp |= PLPFAULT;
	if(ext & PI1_STAT_ONLINE)
		*rvp |= PLPONLINE;
	if(ext & PI1_STAT_NOINK)
		*rvp |= PLPEOI;
	if(ext & PI1_STAT_PE)
		*rvp |= PLPEOP;
	break;
    case PLPIOCTYPE:
	break;
    case PLPIOCSTROBE:
	ext=((unsigned int)arg>>8)&0xff; /* trail edge */
	plp->regs->extreg.tval3 = ext;
	ext=((unsigned int)arg>>16)&0xff; /* lead edge */
	plp->regs->extreg.tval2 = ext;
	ext=((unsigned int)arg>>24)&0xff; /* min duty */
	plp->regs->extreg.tval1 = ext;
	break;
    case PLPIOCIGNACK:
	/* for compatibility with IP6/IP12/IP20 */
	break;
    case PLPIOCSETMASK:
	/* for compatibility with IP6 */
	break;
    case PLPIOCWTO:
	plp->wto = arg * HZ;
	break;
    case PLPIOCRTO:
	plp->rto = arg * HZ;
	break;
    case PLPIOCREAD:
	/* if this is the first open for read, then start
	 * up the read processing
	 */
	if (!(plp->state & PLP_RDOPEN)) {
	    plp_read_flush (plp);		/* flush read buffer */
	    plp->state |= PLP_RDOPEN;
	}
	break;
    case FIONBIO:
	if (arg)
	    plp->state |= PLP_NBIO;
	else
	    plp->state &= ~PLP_NBIO;
	break;
    case PLPIOMODE:
	plp->state |= PLP_RICOH;
	break;
    default:
	rc = EINVAL;
    }
    UNLOCK(&plp->plpmutex);
    return rc;
}

static int
plp_lock(sema_t *sema)
{
	if (psema(sema, PZERO+1) < 0)
		return 1;
	return 0;
		
}

static int
plp_unlock(sema_t *sema)
{
	return (vsema(sema));
}

/*
 * plpintr_read - initiate read from interrupt level
 */
static void
plpintr_read (plp_t *plp, int resethpc)
{
    memd_t *rb = new_rbuf(1);

    if (EMPTYNODE(rb)) {
	/* we ran out of rbufs.
	 * We'll be back in less than a second, and will allocate
	 * more rbufs then.
	 */
	(void) plp_unlock(&plp->dlock);
	plp->dl = 0;
    } else {
	/* we got an rbuf. start a read dma on the channel
	 */
	rb->memd_bc = NBPRD;
	rb->memd_eox = 1;
	rb->memd_xie = 1;
	rb->memd_forw = 0;
	if (cachewrback)
		dki_dcache_wbinval ((void *)rb, sizeof(memd_t));
	rb = (memd_t *) kvtophys (rb);
	if (resethpc)
		plp->regs->extreg.dmactrl = 
			(plp->regs->extreg.dmactrl&~PI1_DMA_CTRL_DMARUN);
	plp_dma (plp, KDM_TO_IOPHYS(rb), B_READ);
    }
}

#if 0
static void plptimeout(void *plp_pun)
{
    plp_t *plp = plp_pun;
    time_t plpto;

    if (plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMAREAD)
	plpto = plp->rto;
    else
	plpto = plp->wto;
    if (plpto)
    {
	time_t time_to_timeout = plp->last_dma_time + plpto - lbolt;

	if (time_to_timeout <= 0)
	{
	    plpintr((__psint_t)plp, NULL);
	    plp->timeoutid = 0;
	}
	else
	    plp->timeoutid = timeout(plptimeout, plp, time_to_timeout);
    }
}
#endif /* 0 */

/*
 * plpintr - parallel port device interrupt handler
 *
 * if plp is set, then the transfer timed out, otherwise,
 * we got a real interrupt and the devices get polled if 
 * they are alive
 */
/*ARGSUSED2*/
static void
plpintr (__psint_t plp_pun, struct eframe_s *ep)
{
    plp_t *plp = (plp_t *)plp_pun;
    __uint32_t _bdp;			/* For GETDESC */
    int _hpc_spl;			/* For GETDESC */
    int pcount;
    __uint32_t dl;
    memd_t *desc;

    /* read or write timed out
     */
    if (plp) {
	LOCK(&plp->plpmutex);
	plp_stop_dma (plp);

	/* On a partial write, set an error flag. On a partial
	 * read, put the data onto the rbuf list for the channel. 
	 */
	if (!(plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMAREAD)) {/* partial write */
	    /* add up transfer size from memory descriptors
	     */
	    dl = plp->dl;
	    pcount = 0;
	    while ((dl != 0) && (dl != GETDESC(plp->regs->nbdp))) {
		desc = (memd_t *)PHYS_TO_K0(dl);
		if (desc->memd_nbdp == (GETDESC(plp->regs->nbdp))){
		    pcount += (GETDESC(plp->regs->cbp) - desc->memd_cbp);
		} else
			pcount += desc->memd_bc;
		dl = desc->memd_forw;
	    }
	    plp->bp->b_resid -= pcount;
	    plp->bp->b_error = EAGAIN;	/* indicates write timed out */

	    /* if a write fails, maybe the receiver is waiting
	     * to transmit, so initiate a read if the port is open
	     * for reading
	     */
	    if (plp->state & PLP_RDOPEN)
		plpintr_read(plp, 1);
	    else
		plp_unlock (&plp->dlock);

	    /* wake up plpio sleeper */
	    vsema(&plp->dmasema);
	}
	else {						/* partial read */
	    /* if a read times out, then allow a write if there is
	     * one waiting.  if no write is waiting, then start
	     * a read if the port is open for reading
	     */
	    rbuf_save (plp);
	    if (!plp_unlock (&plp->dlock) && (plp->state & PLP_RDOPEN)) {
		plp_lock (&plp->dlock);
		plpintr_read(plp, 1);
	    }
	}
	UNLOCK(&plp->plpmutex);
    } 
    
    /* got a real read or write interrupt
     */
    else {

	/* poll the plp devices to find one that has interrupted
	 * and service it
	 */
	for (pcount = 0; pcount < NPLP; ++pcount) {
	    int clear_interrupt;
	    plp = plpport[pcount];

	    LOCK(&plp->plpmutex);
	    clear_interrupt = plp->regs->ctrl;
	    if (clear_interrupt & PBUS_CTRL_INTERRUPT)
	    {
		plp_stop_dma(plp);
		rbuf_save (plp);
		plpintr_read(plp, 0);
	    }
	    if ((plp->state & PLP_ALIVE) && 
			(plp->regs->extreg.intstat & PI1_INT_STAT_FEMPTYINT)) {
		plp_stop_dma (plp);
		if (plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMAREAD){
		    rbuf_save (plp);
		}else{
		    vsema(&plp->dmasema);
		}

		/* if the transfer was successful, always initiate a
		 * read if the port is open for reading
		 */
		if (plp->state & PLP_RDOPEN){
		    plpintr_read(plp, 0);
		}else
		    plp_unlock (&plp->dlock);
	    }
	    UNLOCK(&plp->plpmutex);
	}
    }
    return;
}

/*
 * plpio - do parallel port device I/O
 */
static int
plpio (struct buf *bp)
{
    int xbytes;		/* number of bytes in current transfer */
    void *vaddr;	/* address of current transfer */
    plp_t *plp;
    memd_t *dl;

    if ((plp = PLP_DEV2PLP(bp->b_edev)) == NULL)
	return EINVAL;

    bp->b_resid = bp->b_bcount;

    /* try to satisfy a read request on buffered data
     */
    if (bp->b_flags & B_READ) {
	/* grab the read lock so no processes get intermingled data 
	 */
	if (plp_lock (&plp->rlock)) {
	    bp->b_error = EINTR;
	} else {
	    while (bp->b_resid && !bp->b_error) {
		vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);
		/* copy as much data as we have buffered on the channel
		 */
		LOCK(&plp->rbufsmutex);
		if (!EMPTYNODE(plp->rbufs))
		    bp->b_resid -= plp_read_copy (plp, vaddr, bp->b_resid);
		UNLOCK(&plp->rbufsmutex);

		LOCK(&plp->plpmutex);
		if (plp->state & PLP_NBIO) {
		    UNLOCK(&plp->plpmutex);
		    break;
		}
		UNLOCK(&plp->plpmutex);

		/* if necessary, wait for additional data to arrive 
		 * asynchronously
		 */
		LOCK(&plp->rbufsmutex);
		if (bp->b_resid && EMPTYNODE(plp->rbufs)) {
		    if (sv_wait_sig(&plp->rbufssv, PZERO+1,
				&plp->rbufsmutex, 0))
			bp->b_error = EINTR;
		}
	    }
	    plp_unlock (&plp->rlock);
	}
    }
    
    else {
	/* grab the write lock so no processes send intermingled data
	 */
	if (plp_lock (&plp->wlock)) {
	    bp->b_error = EINTR;
	} else {
	    while (bp->b_resid && !bp->b_error) {
		/* determine the virtual address of the next portion of
		 * the buffer to transfer, then create a memory descriptor
		 * list for the transfer
		 */
		vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);
		xbytes = plp_queue (&dl, vaddr, bp->b_resid);
		if (xbytes < 0) {
		    bp->b_error = EINTR;
		    break;
		}
    		if (cachewrback)
		    dki_dcache_wbinval (vaddr, xbytes);

		/* get the dma channel lock.  the interrupt routine
		 * will unlock the channel when writing is allowed again.
		 */
		if (plp_lock(&plp->dlock)) {
		    bp->b_error = EINTR;
		    plp_free (dl);
		    break;
		}


		/* start dma transfer
		 */
		plp->bp = bp;

		plp_dma (plp, KDM_TO_IOPHYS(dl), B_WRITE);
		/* wait for transfer to complete
		 */
		if (psema(&plp->dmasema, PZERO+1)) {
		    bp->b_error = EINTR;
		    plp_stop_dma(plp);
		    plp_unlock (&plp->dlock);
		}
		/* put the descriptor list back on the free list
		 */
		plp_free (dl);

		/* if dma completes successfully update resid bytes, otherwise
		 * resid was already fixed up by the interrupt handler for
		 * a partial transfer, and we need to clear b_error
		 */
		if (!bp->b_error) {			/* complete xfer */
		    bp->b_resid -= xbytes;
		}else if (bp->b_error == EAGAIN) { 	/* partial xfer */
		    bp->b_error = 0;
		    break;
		}
	    }
	    plp_unlock (&plp->wlock);
	}
    }

    if (bp->b_error)
	bp->b_flags |= B_ERROR;
    iodone (bp);

    /* value returned but ignored, thanks USL */
    return 0;
}

/*
 * plp_free - free a memory descriptor list 
 * 
 * the descriptor list was presumably created by plp_queue()
 * dl and all nbdp's in list are physical addresses
 */
static void
plp_free (memd_t *dl)
{
    int dowakeup;
    memd_t *tmp;

    LOCK(&free_mds_mutex);
    dowakeup = (free_mds == 0);
    while (!EMPTYNODE(dl)) {
	dl = (memd_t *) PHYS_TO_K0 (dl);
	tmp = (memd_t *)(__psunsigned_t)(dl->memd_forw);
	LPUTNODE (free_mds, dl);
	dl = tmp;
    }
    if (dowakeup)
	sv_signal(&free_mds_sv);
    UNLOCK(&free_mds_mutex);

    return;
}

/*
 * plp_queue - create a descriptor list from a vaddr and byte count.  
 *
 * pdl is the address of where to put the memory desc list
 * addr is the virtual address to map to memory descriptors
 * bcount is the length of the buffer
 * This is guaranteed to create a list of at least 1 memd
 * returns number of bytes that are queued in list
 */
static int
plp_queue (memd_t **pdl, void *vaddr, int bcount)
{
    memd_t  *next, *current;
    unsigned size, bytes = 0;

    /* get at least a first memd for the transfer
     */
    do {
	LOCK(&free_mds_mutex);    
	LGETNODE(free_mds, current);
	if (EMPTYNODE(current)) {
	    if (sv_wait_sig(&free_mds_sv, PZERO+1,
			&free_mds_mutex, 0))
		    return -1;		/* interrupted */
	} else
		UNLOCK(&free_mds_mutex);
    } while (EMPTYNODE(current));
    
    /* first time through, we are guaranteed that
     * 'current' contains a usable node
     */
    ASSERT (current != (memd_t *)0);
    *pdl = (memd_t *)kvtophys(current);

    do {
	/* if the address crosses a page boundary
	 * set the size only up to that boundary
	 */
	if ((size = NBPP - poff(vaddr)) > bcount)
		size = bcount;

#if NBPP > IO_NBPP
	/* if by chance we try to DMA greater than IO_NBPP, reduce it */
	/* this could happen if we are using 16K pages */
	if (size > IO_NBPP)
		size = IO_NBPP;
#endif

	current->memd_bc = size;
	current->memd_eox = 0;
	current->memd_xie = 0;
	current->memd_cbp = kvtophys((void *)vaddr);

	bcount -= size;
	bytes += size;
	vaddr = (void *)((__psunsigned_t)vaddr + size);

	if (bcount > 0) {
	    LOCK(&free_mds_mutex);
	    LGETNODE(free_mds, next);
	    UNLOCK(&free_mds_mutex);
	    if (!EMPTYNODE(next)) {
		current->memd_forw = (__uint32_t)kvtophys(next);
		if (cachewrback)
		    dki_dcache_wbinval ((void *)current, sizeof(memd_t));
		current = next;
	    }
	}
	    
    } while (!EMPTYNODE(next) && bcount > 0);

    current->memd_forw = 0;
    current->memd_eox = 1;
    current->memd_xie = 0;
    if (cachewrback)
	dki_dcache_wbinval ((void *)current, sizeof(memd_t));
    return (bytes);
}

/*
 * plp_read_flush - flush the input buffer list on a channel
 */
/*
 * LOCKS HELD ON ENTRY:  plp->plpmutex
 * LOCKS ACQUIRED HERE:  plp->rbufsmutex
 */
static void
plp_read_flush (plp_t *plp)
{
    memd_t *rb;

    LOCK(&plp->rbufsmutex);
    LGETNODE (plp->rbufs, rb);
    while (!EMPTYNODE(rb)) {
	del_rbuf (rb);
	LGETNODE (plp->rbufs, rb);
    }
    UNLOCK(&plp->rbufsmutex);
}

/*
 * plp_read_copy - copy data from a channel's input buffer list to vaddr
 *
 * NOTE: For R10000_DMA_READ_WAR systems (IP28) it is important to access
 *	 the read DMA buffer uncached.  The code has always done this to
 *	 avoid flushing, but with the speculative execution problems,
 *	 it becomes more important.
 */
static int 
plp_read_copy (plp_t *plp, caddr_t vaddr, int nbytes)
{
    memd_t *rb;
    caddr_t cp = vaddr;
    int bcount = nbytes;

    /* start with the first rbuf on the list 
     */
    LOCK(&plp->rbufsmutex);
    LGETNODE (plp->rbufs, rb);
    while (!EMPTYNODE(rb) && bcount) {
	/* if readbuf fits in read request, then
	 * copy data, and free rbuf
	 */
	if (rb->memd_bc <= bcount) {
	    bcopy ((void *)PHYS_TO_K1(rb->memd_buf), cp, rb->memd_bc);
	    bcount -= rb->memd_bc;
	    cp += rb->memd_bc;			
	    del_rbuf(rb);
	    /* get next rbuf from list if more bytes are to
	     * be transfered
	     */
	    if (bcount)
		LGETNODE (plp->rbufs, rb)
	} 
	/* copy a portion of the read buffer to cp
	 */
	else {
	    bcopy ((void *)PHYS_TO_K1(rb->memd_buf), cp, bcount);
	    cp += bcount;			
	    rb->memd_buf += bcount;
	    rb->memd_bc -= bcount;
	    bcount = 0;
	    /* push rbuf with remaining data back onto head of list
	     */
	    LPUTNODE (plp->rbufs, rb);
	}
    }
    UNLOCK(&plp->rbufsmutex);
    return nbytes - bcount;
}

static memd_t *free_rbufs = 0; 	/* list of all free read descriptor nodes */

/*
 * rbuf_init - initialize free read buffer descriptor list
 */
static void
rbuf_init(void)
{
    caddr_t pbuf;
    int pcount;
    memd_t *md;

    mutex_init(&free_rbufs_mutex, MUTEX_DEFAULT, "freerbufs");
    sv_init(&free_rbufs_sv, SV_DEFAULT, "freerbufs");

    /* allocate the desired number of pages
     */
    pbuf = rbufmem;

    LOCK(&free_mds_mutex);
    LOCK(&free_rbufs_mutex);
    for (pcount = 0; pcount < NRP * NRDPP; pcount++) {
	LGETNODE (free_mds, md);
	md->memd_buf = (__uint32_t)kvtophys(pbuf);
	LPUTNODE (free_rbufs, md);
	pbuf += NBPRD;
    }
    UNLOCK(&free_rbufs_mutex);
    UNLOCK(&free_mds_mutex);
}

/*
 * rbuf_save - move newly received data from dl to rbuf list
 *
 * addresses are converted from physical to kernel again
 * unused nodes in descriptor list are returned to free rbuf list
 * this is called from interrupt level only
 * doesn't use LPUTNODE because it chains rbufs onto end of list
 */
/*
 * LOCKS HELD ON ENTRY: plpmutex
 * LOCKS ACQUIRED HERE: rbufsmutex
 */
static void
rbuf_save (plp_t *plp)
{
    __uint32_t dl = plp->dl;
    memd_t *newbuf;
    memd_t *inbufs;
    int rbcount = 0;
    __uint32_t _bdp;			/* For GETDESC */
    int _hpc_spl;			/* For GETDESC */

    /* If we get a read timeout but there was never any
     * DMA started, then dl is empty.  This happens if
     * we run out of rbufs at interrupt level.
     */
    if (dl == 0)
	return;

    /* point inbufs at last buffer in rbuf list
     */
    LOCK(&plp->rbufsmutex);
    if (!EMPTYNODE(inbufs = plp->rbufs))
	while (inbufs->memd_forw)
	    inbufs = (memd_t *)IOPHYS_TO_K0(inbufs->memd_forw);

    /* link newly received buffers onto rbuf list 
     */
    while ((dl != 0) && (dl != GETDESC(plp->regs->nbdp))) {
	newbuf = (memd_t *)PHYS_TO_K0(dl);

	/* adjust buffer length for last descriptor if transfer
	 * is incomplete
	 */
	if ((newbuf->memd_nbdp == GETDESC(plp->regs->nbdp)) && 
		((GETDESC(plp->regs->cbp) - newbuf->memd_cbp) != 0)) {
		newbuf->memd_bc = (GETDESC(plp->regs->cbp) - newbuf->memd_cbp);
	} else {
	   newbuf->memd_bc = 0;
	}

	if (newbuf->memd_bc == 0)
		break;

	dl = newbuf->memd_forw;
	newbuf->memd_forw = 0;
	if (!EMPTYNODE(inbufs))
	    inbufs->memd_forw = KDM_TO_IOPHYS(newbuf);
	else
	    plp->rbufs = newbuf;
	inbufs = newbuf;
	rbcount++;
    }

    ASSERT((dl == GETDESC(plp->regs->nbdp)) || (dl == GETDESC(plp->regs->cbp)));

    /* wakeup any process that may be awaiting input on this channel
     */
    /* free up unused buffers 
     */
    while (dl != 0) {
	newbuf = (memd_t *)PHYS_TO_K0(dl);
	dl = newbuf->memd_forw;
	del_rbuf (newbuf);
    }
    if (rbcount)
	    sv_broadcast(&plp->rbufssv);
    UNLOCK(&plp->rbufsmutex);
    plp->dl = 0;

}

/*
 * new_rbuf - allocate an rbuf from the free list
 *
 * will always return with an rbuf unless the list is empty
 * and it is called from interrupt level, or a signal is received
 */
static memd_t *
new_rbuf (int ilev)
{
    memd_t *rbuf;

    do {
	LOCK(&free_rbufs_mutex);
	LGETNODE(free_rbufs, rbuf);
	if (EMPTYNODE(rbuf)) {
		if (sv_wait_sig(&free_rbufs_sv, PZERO+1,
				&free_rbufs_mutex, 0))
			break;
	} else {
		UNLOCK(&free_rbufs_mutex);
	}
    } while (EMPTYNODE(rbuf));
    return rbuf;
}

/*
 * del_rbuf - put an rbuf back onto the free list
 */
/*
 * LOCKS HELD ON ENTRY: free_rbufs_mutex
 */
static void
del_rbuf (memd_t *rbuf)
{
    int dowakeup = (KDM_TO_IOPHYS(free_rbufs) == 0);

    rbuf->memd_cbp &= ~(NBPRD - 1);	/* realign buffer pointer */
    LPUTNODE(free_rbufs, rbuf);
    if (dowakeup)
	sv_broadcast(&free_rbufs_sv);
}

/* hardware functions for parallel interface
 */

/*
 * plp_dma - start dma on descriptor list dl and channel pointed to by plp
 *
 * dma channel must currently not be in use, dl is the
 * descriptor list for the transfer which is a physical address.
 */
static void
plp_dma (plp_t *plp, __uint32_t dl, int flags)
{

    plp->dl = dl;
    plp->regs->cbp = 0x0badbad0; 		/* for good measure */
    plp->regs->nbdp = dl;	

    if (flags & B_READ) {
	if (plp->rto){
	    plp->timeoutid = dtimeout(plpintr, plp, plp->rto, plbase, cpuid());
	}else
	    plp->timeoutid = 0;

        if (plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMARUN)
                plp->regs->extreg.dmactrl =
                        (plp->regs->extreg.dmactrl&~PI1_DMA_CTRL_DMARUN);

        plp->regs->ctrl = (PBUS_CTRL_DMASTART|PBUS_CTRL_LOAD_ENA|
			   PBUS_CTRL_DMADIR|
		PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);

	plp->regs->extreg.dmactrl = 
			(PI1_DMA_CTRL_RICOHMODE|PI1_DMA_CTRL_DMAREAD|
			PI1_DMA_CTRL_DMARUN|PI1_DMA_CTRL_BLKMODE);
	plp->regs->extreg.ctrl = (PI1_CTRL_STROBE_N|
			PI1_CTRL_INIT_N|PI1_CTRL_SLIN_N);
    } else {
	if (plp->wto) {
	    plp->timeoutid = dtimeout(plpintr, plp, plp->wto, plbase, cpuid());
	} else
	    plp->timeoutid = 0;

        plp->regs->extreg.dmactrl =
                        (plp->regs->extreg.dmactrl&~PI1_DMA_CTRL_DMARUN)|
			PI1_DMA_CTRL_ABORT;
	plp->regs->extreg.intmask = ~PI1_INT_MASK_FEMPTYINT&0xff;
        plp->regs->ctrl = (PBUS_CTRL_DMASTART|PBUS_CTRL_LOAD_ENA|
		PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);

	plp->regs->extreg.ctrl = (PI1_CTRL_STROBE_N|PI1_CTRL_AFD_N|
			PI1_CTRL_INIT_N|PI1_CTRL_SLIN_N);
        if (plp->state & PLP_RICOH) {
		plp->regs->extreg.dmactrl = 
			(PI1_DMA_CTRL_DMARUN|PI1_DMA_CTRL_BLKMODE|
			PI1_DMA_CTRL_RICOHMODE);
	}else
		plp->regs->extreg.dmactrl = 
			(PI1_DMA_CTRL_DMARUN|PI1_DMA_CTRL_BLKMODE);

    }
    return;
}

/*
 * plp_stop_dma - stop current dma transfer
 *
 * disable any timeouts, stop the dma transfer, and
 * if reading, then flush the HPC fifo to memory.
 */
static void
plp_stop_dma (plp_t *plp)
{
    int timout = 100;
    unsigned data, fcnt;
    unsigned char *cbp, *datap;
    memd_t *dl = (memd_t *)PHYS_TO_K0(plp->dl);
    __uint32_t _bdp;
    int _hpc_spl;

    if (plp->timeoutid) {
	untimeout(plp->timeoutid);
	plp->timeoutid = 0;
    }

    /* if reading, then wait for memory flush to complete
     */
    if (plp->regs->extreg.dmactrl & PI1_DMA_CTRL_DMAREAD) {
	    /* flush bit stops any transfer
	     */
        fcnt = (memd_t *)(__psunsigned_t)GETDESC(plp->regs->cbp) -
	       (memd_t *)(__psunsigned_t)(dl->memd_cbp);
        plp->regs->extreg.dmactrl =	/* stop pi1 */
               (plp->regs->extreg.dmactrl&~PI1_DMA_CTRL_DMARUN);
	if (fcnt > 0 && fcnt < 4) {
            plp->regs->ctrl = (PBUS_CTRL_LOAD_ENA|PBUS_CTRL_DMADIR|
                        PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);

            data = plp->regs->fifo;
            datap = (unsigned char *)&data;
            /* if hpc hasn't yet updated the cbp, then use the
             * cbp from the descriptor list, and force the hpc registers
             * to valid values for downstream processing
             */
            if (GETDESC(plp->regs->nbdp) == plp->dl) {
                memd_t *dl = (memd_t *)PHYS_TO_K0(plp->dl);
                cbp = (unsigned char *)PHYS_TO_K0(dl->memd_cbp);
                plp->regs->cbp = dl->memd_cbp;
                plp->regs->nbdp = dl->memd_nbdp;
            }
            else
                cbp = (unsigned char *)PHYS_TO_K0(plp->regs->cbp);
            if (fcnt >= 1)
                *cbp++ = *datap++;
            if (fcnt >= 2)
                *cbp++ = *datap++;
            if (fcnt >= 3)
                *cbp++ = *datap++;
	} else { /* flush */
        	plp->regs->ctrl = (PBUS_CTRL_FLUSH|PBUS_CTRL_DMADIR|
			PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);
		while ((plp->regs->ctrl & PBUS_CTRL_DMASTAT) && --timout)
			continue;
		if (!timout)
			cmn_err(CE_WARN,"Parallel port DMA never finished\n");
	}
        plp->regs->ctrl = (PBUS_CTRL_LOAD_ENA|PBUS_CTRL_DMADIR|
			PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);
    } else {
        plp->regs->ctrl = (PBUS_CTRL_LOAD_ENA|
			PBUS_CTRL_HIWATER|PBUS_CTRL_FIFOBEG|PBUS_CTRL_FIFOEND);
        plp->regs->extreg.dmactrl =
               ((plp->regs->extreg.dmactrl&~PI1_DMA_CTRL_DMARUN)|
               PI1_DMA_CTRL_ABORT); /* pi1 dma reset */
    }
    return;
}

/*
 * plp_probe - probe for parallel port hardware
 *
 * returns the address of the parallel port registers
 */
static plp_regs_t *
plp_probe (int plp)
{
    unsigned int *hpcbase = (unsigned int *)
	    PHYS_TO_K1((plp & 1) ? HPC_0_ID_ADDR : HPC_1_ID_ADDR);

    if (plp3probe[plp])
	return (plp_regs_t *)((__psint_t)hpcbase + PAR_OFFSET);
    else
	return (plp_regs_t *)0;
}

/*
 * plp_reset - reset the HPC/parallel port interface
 */
/*
 * LOCKS HELD ON ENTRY: plp->plpmutex
 */
static void
plp_reset(plp_t *plp)
{
    int i;

    if (plp_lock (&plp->dlock))
	return;

#ifdef LATER /* Need a R/W register, write1 is write only */
    plp->regs->write1 |= PAR_CTRL_RESET;		/* reset HPC logic */
    plp->regs->write1 &= ~PAR_CTRL_RESET;
#endif

    plp->regs->extreg.ctrl = (unsigned)(PI1_CTRL_STROBE_N|PI1_CTRL_AFD_N|
					PI1_CTRL_SLIN_N);

    for (i=0;i!=100;i++)
       DELAY(400);
    /* reset printer */ 
    plp->regs->extreg.ctrl = (unsigned)(PI1_CTRL_STROBE_N|PI1_CTRL_AFD_N|
				PI1_CTRL_SLIN_N|PI1_CTRL_INIT_N);

    /* reset DMA state machine */
    plp->regs->extreg.dmactrl |= PI1_DMA_CTRL_ABORT;

    plp_unlock (&plp->dlock);

    return;
}

static int
plp_mem_init(void)
{
    mdmem = (caddr_t)kmem_alloc(MDMEMSIZE, VM_NOSLEEP|VM_DIRECT|KM_CACHEALIGN);
    rbufmem = (caddr_t)kvpalloc(NRP, VM_NOSLEEP|VM_DIRECT|VM_UNCACHED, 0);
    if (!mdmem || !rbufmem) {
	plp_mem_free();
	cmn_err(CE_WARN, "plp: out of memory for hpc3plp driver");
	return ENOMEM;
    }
#ifdef R10000_SPECULATION_WAR
    /* make sure our page is relatively clean */
    dki_dcache_wbinval((void *)PHYS_TO_K0(KDM_TO_PHYS(rbufmem)),NBPP);
#endif
    return 0;
}

static void
plp_mem_free(void)
{
    if (mdmem)
	kern_free(mdmem);
    if (rbufmem)
	kvpfree(rbufmem, NRP);
    mdmem = rbufmem = 0;
}

#ifdef DEBUG
void
plp_show_dl(__uint32_t dl)
{
    static int showcount;
    memd_t *k0dl;

    showcount++;

    if (!dl) {
	dprintf ("plp(%d): dl empty\n", showcount);
    } else {
	while (dl) {
	    k0dl = (memd_t *)PHYS_TO_K0(dl);
	    dprintf ("plp(%d): dl 0x%x, bc %d, cbp 0x%x, nbdp 0x%x\n",
		showcount, dl, k0dl->memd_bc, k0dl->memd_cbp, k0dl->memd_nbdp);
	    dl = k0dl->memd_forw;
	}
    }
}
#endif /* DEBUG */
#endif /* IP22 || IP26 || IP28 */
