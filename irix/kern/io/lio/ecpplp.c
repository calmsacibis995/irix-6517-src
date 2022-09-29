#if IP27 || IP30
/*
 * ecpplp.c - driver for the IP30 parallel port
 */

#include <sys/types.h>
#include <ksys/vfile.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/mload.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/iobus.h>
#include <sys/atomic_ops.h>
#include <sys/invent.h>

#if IP27 || IP30
#include <sys/iograph.h>
#include <sys/PCI/pciio.h>
#endif /* IP27 || IP30 */

#include <sys/PCI/ioc3.h>
#include <sys/plp.h>
#include <sys/ecpplpreg.h>

#include <sys/PCI/PCI_defs.h>

#ifdef SN
#include <sys/SN/addrs.h>
#endif /* IP27 */

#if SN0 || IP30
#define KVTOIOADDR(plp, addr) (pciio_dmatrans_addr((plp)->plp_conn_vhdl, 0, \
				kvtophys((caddr_t)(addr)), sizeof(int), \
				PCIIO_DMA_A64 | PCIIO_DMA_DATA))
#endif /* SN0 || IP30 */

#define KDM_TO_IOPHYS(addr)	KDM_TO_PHYS(addr)
#define IOPHYS_TO_K0(addr)	PHYS_TO_K0(addr)
#include <sys/pda.h>

#define	NEW(ptr)		(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)		(kmem_free(ptr, sizeof (*(ptr))))

#define LOW32(addr)		((0xffffffff & addr))
#define HIGH32(addr)		(((0xffffffff00000000 & addr) >> 32))

#define SET_PHASE(plp, sphase)	(plp->phase = sphase)
#define SET_MODE(plp, smode)	(plp->mode = smode)
#define PICK_BYTE(x,y)		(0x0 | (((0xff<<(y*8))&(x))>>(y*8)))
#define CLEAR_CONTEXT(ctxt)	{ctxt.bc = ctxt.cbp_h = ctxt.cbp_l = 0; }

#define LOCK(m)			mutex_lock(m, PZERO)
#define UNLOCK(m)		mutex_unlock(m)

/* insert/remove node _n_ into/from list _l_
 */
#define EMPTYNODE(n) 		(KDM_TO_IOPHYS(n)==0)
#define PUTNODE(l,n) 		do {(n)->memd_forw = (memd_t *)(l); (l) = (n);} while (0)
#define GETNODE(l,n) 		do {(n) = (l); if (!EMPTYNODE(l)) \
                     		(l) = (memd_t *)IOPHYS_TO_K0((l)->memd_forw);} while (0)
#define LPUTNODE(l,n)		PUTNODE(l,n)
#define LGETNODE(l,n)		GETNODE(l,n)

#define	DRIVER_PFX		"plp"

#define	ecp_set(v,p)		(hwgraph_fastinfo_set((v), (arbitrary_info_t)(p)))
#define	ecp_get(v)		((ecp_t *)hwgraph_fastinfo_get((v)))
#define	dev_to_ecp(d)		ecp_get(dev_to_vhdl(d))

int plpdevflag = D_MP;

/* Needed for dynamically loadable modules */
char *plpmversion = M_VERSION;

#if DEBUG && ECP_DEBUG
int ecpplp_debug = 1;
#else
int ecpplp_debug = 0;
#endif /* DEBUG && ECP_DEBUG */
#define dprintf if (ecpplp_debug) printf

int plp_initfail;
int plp_inuse;

extern int plpwardelay;                 /* from master.d/ecpplp */

/* Note: Input is not supported right now. The input code has been ifdef'ed out
 * using #ifdef INPUT. If/when input is supported, this will need to be removed.
 * Caution: the ifdef'ed code is not guaranteed to work.
 */
/*
 * The locking and synchronization model of this driver is something like this:
 *
 * Mutexes and SVs:
 * ---------------
 * free_rbufs_mutex & free_rbufs_sv : Used for protecting and synchronizing access
 *               to free_rbufs
 * free_mds_mutex & free_mds_sv : Used for protecting and synchronizing access to
 *               free_mds
 * ecp.rbufsmutex and ecp.rbufssv : Used for protecting and synchronizing access to
 *               ecp.rbufs
 * ecp.ecpmutex/spinlock : Used for protecting state information in ecp struct in
 * INTR_KTHREAD/NON-INTR_KTHREAD case
 *
 * Semaphores
 * ----------
 * ecp.wlock : provides write access to parallel port
 * ecp.rlock : provides read access to parallel port
 * ecp.dmasema : for synchronizing dma activity
 * ecp.dlock : lock for dma access
 */


/* memory for buffers and memory descriptors */
caddr_t mdmem;
#ifdef INPUT
static caddr_t bufmem;
#endif /* INPUT */

memd_t *free_mds;
memd_t *free_rbufs;

static mutex_t free_mds_mutex;
static sv_t free_mds_sv;

static mutex_t free_rbufs_mutex;
static sv_t free_rbufs_sv;

/*
 *	Function Table of Contents
 */

void			plpinit(void);
int			plpreg(void);
int			plpunreg(void);
int			plpattach(vertex_hdl_t conn_vhdl);
static pciio_iter_f	plpreloadme;
void			plpdetach(vertex_hdl_t conn_vhdl);
int			plpopen(dev_t *devp, int mode);	int plpclose(dev_t dev);

int			plpunload(void);
static pciio_iter_f	plpunloadme;

int			plpread(dev_t dev, uio_t *uiop);
int			plpwrite(dev_t dev, uio_t *uiop);
int			plpioctl(dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp);

int			plpio (struct buf *bp);

int			plp_queue(ecp_t *plp, memd_t **pdl, void *vaddr, int bcount);
void			plp_free(memd_t *dl);
int			plp_mem_init(void);
void			plp_mem_free(void);

static void		plpintr(intr_arg_t arg, ioc3reg_t sio_ir);

static int		plp_lock(sema_t *sema);
static int		plp_unlock(sema_t *sema);
void			plp_reset(ecp_t *plp );
void			plp_dma(ecp_t *plp, __uint64_t dl, int flags);

static void		plp_terminate(ecp_t *plp, char extreq);
static int		plp_negotiate(ecp_t *plp, char extreq);
static int		plp_fwdtorev(ecp_t *plp);
static int		plp_revtofwd(ecp_t *plp);
static int		plp_sigdelay(ecp_t *, int);
static void		plp_sigdelay_tmout(ecp_t *);

#if DEBUG && ECP_DEBUG
void			plp_show_dl(ecp_t *plp);
#endif /* DEBUG && ECP_DEBUG */

#ifdef INPUT
void			rbuf_init(void);
int			plp_read_copy (ecp_t *plp, caddr_t vaddr, int nbytes);
void			del_rbuf (memd_t *rbuf);
void			plpintr_read(ecp_t *plp, int timedout);
memd_t *		new_rbuf (int ilev);
void			rbuf_save(ecp_t *plp);
void			plp_read_flush(ecp_t *plp);
void			plprtimeout(ecp_t *plp);
void			plptimeout1(ecp_t *plp);
#endif /* INPUT */

void			plpwtimeout(ecp_t *plp);
void			plp_stop_dma(ecp_t *plp, int flag);
void			plp_dma_reset(ecp_t *plp);

/*
 * The following are from the master.d/ecpplp file.
 * Please see that file for descriptions of each
 * variable below.
 */
extern int	plpopmode;
extern int	plpwardelay;
extern int	plpStrobeDelay;
extern int	plpNumRetries1;
extern int	plpRetryDelay1;
extern int	plpRetryDelay2;

int
plpreg(void)
{
    /* register plp driver as a pciio
     * registered driver that handles the ioc3.
     */

    pciio_driver_register(IOC3_VENDOR_ID_NUM,
                          IOC3_DEVICE_ID_NUM,
                          "plp",
                          0);
    return(0);
}

int
plpunreg(void)
{
    pciio_driver_unregister("plp");
    return 0;
}

/*
 * plpinit is called each time the driver
 * is loaded into memory.
 */
void
plpinit(void)
{
    static int once = 0;
    memd_t *node_space;
    __psunsigned_t end_space;
    static volatile int ready = 0;

    /* Since the buffer lists are common,
     * we want to do this only once.
     */
    if (atomicAddInt(&once, 1) == 1) {

        /* Initialize some globals */
        mdmem = 0;
#ifdef INPUT
        bufmem = 0;
#endif /* INPUT */
        free_mds = free_rbufs = 0;

        /* cpu-dependent allocation of space for memory descriptors,
         * receive buffers, and transmit buffers(if needed). Creates
         * mdmem, bufmem. Returns ENOMEM if memory
         * cannot be allocated.
         */

        if (plp_mem_init()) {
#if DEBUG && ECP_DEBUG
	    cmn_err(CE_WARN, "ecpplp: initialization failed,"
			" unable to allocate memory - opens will be refused");
#endif /* DEBUG && ECP_DEBUG */
	    plp_initfail =1;
	    return;
        }

        /* Initialize mutexes and synchronization variables */
        mutex_init(&free_mds_mutex, MUTEX_DEFAULT, "mds_mutex");
        sv_init(&free_mds_sv, SV_DEFAULT, "mds_sv");

        mutex_init(&free_rbufs_mutex, MUTEX_DEFAULT, "freerbufs");
        sv_init(&free_rbufs_sv, SV_DEFAULT, "freerbufs");

        /* initialize free_mds to contain all memory descriptors allocated
         * from mdmem.
         */
        node_space = (memd_t *)mdmem;
        end_space = (__psunsigned_t)mdmem + MDMEMSIZE;

        while (((__psunsigned_t)node_space + sizeof (memd_t)) <= end_space) {
            /* throw out descriptors that cross page boundaries
             */
            if (btoc(node_space) == btoc((__psint_t)node_space+sizeof(memd_t)-1))
		LPUTNODE (free_mds, (memd_t *)PHYS_TO_K0(kvtophys(node_space)));
            node_space++;
        }

#ifdef INPUT
        /* Initialize read buffer list */
        rbuf_init();
#endif /* INPUT */

	/*
	 * Tell the other CPUs waiting just below can
	 * continue, now that we have set up our buffer
	 * lists.
	 */
	ready = 1;
    } else {
	/*
	 * Wait here until the first guy in has finished
	 * setting up the buffer lists.
	 */
	while (ready == 0)
	    ;
    }

    /*
     * This turns interrupts back on when
     * we get reloaded (ie. we're loaded,
     * someone unloads us or we timeout,
     * then we get loaded again without being
     * unregistered).
     */
    pciio_iterate("plp", plpreloadme);
}

/*
 * plpattach: called by the ioc3 driver (or
 * by the pci infrastructure) when we have
 * an IOC3 in our sights.
 */
int
plpattach(vertex_hdl_t conn_vhdl)
{
    unsigned char ecr;
    ecp_t *p;
    ioc3_mem_t *plpaddr;
    vertex_hdl_t ioc3_vhdl;
    vertex_hdl_t ecpp_vhdl;
    void *ioc3_soft;
    /*REFERENCED*/
    graph_error_t rc;

    if (plp_initfail) {
	cmn_err(CE_WARN, "plpinit failed: driver won't work!\n");
	return -1;
    }

    if (!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ecpp))
        return -1;

    /*
     * get a pio address for the ioc3
     */
    plpaddr = (ioc3_mem_t *)pciio_piotrans_addr
	(conn_vhdl, 0, PCIIO_SPACE_WIN(0), 0, sizeof (*plpaddr), 0);

    /* do some preliminary testing to verify existence of device */
    ecr = plpaddr->sregs.pp_ecr;

    /* Check that empty bit is 1 and full bit is 0 */
    if (! ((ecr & ECR_EMPTY) && !(ecr & ECR_FULL))){
#if DEBUG && ECP_DEBUG
	cmn_err(CE_WARN, "ecpplp initialization failed on %v\n",conn_vhdl);
#endif /* DEBUG && ECP_DEBUG */
	return -1;
    }

    /* Write 0x34 to ECR and read back; It should contain 0x35 */
    plpaddr->sregs.pp_ecr = ECR_TEST_PATTERN;
    ecr = plpaddr->sregs.pp_ecr;
    if (! ((ecr & ~ECR_TEST_PATTERN) & ECR_EMPTY)){
#if DEBUG && ECP_DEBUG
	cmn_err(CE_WARN, "ecpplp initialization test failed on %v\n",conn_vhdl);
#endif /* DEBUG && ECP_DEBUG */
	return -1;
    }

    /*
     * If the ioc3 is using two connect points,
     * we want to connect to the second one.
     * If there is no GUEST edge, this leaves
     * conn_vhdl unmodified.
     */
    (void)hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

    /*
     * find our ioc3 vertex
     */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    if (rc != GRAPH_SUCCESS) {
	cmn_err(CE_ALERT,"%v: ecpplp unable to locate ioc3 vertex\n",
		conn_vhdl);
	return(-1);
    }

    /* get back pointer to the ioc3 soft area */
    ioc3_soft = (void*)hwgraph_fastinfo_get(ioc3_vhdl);

    /*
     * Find or add our private graph vertex.
     */
    rc = hwgraph_char_device_add(conn_vhdl, EDGE_LBL_ECPP, DRIVER_PFX, &ecpp_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    /*
     * Allocate our soft-state structure
     */
    NEW(p);
    ecp_set(ecpp_vhdl, p);
    p->vhdl = ecpp_vhdl;

    /* Initialize parallel port struct */
    p->ioc3_soft = ioc3_soft;
    p->plp_conn_vhdl = conn_vhdl;
    p->state = 0;
    p->mode = 0;
    p->phase = 0;

    /* Init dma locks */
    initnsema(&p->dlock, 1, "dlock");
    initnsema(&p->wlock, 1, "wlock");
    initnsema(&p->rlock, 1, "rlock");
    initnsema(&p->dmasema, 0, "dmasema");

    p->rbufs = (memd_t *) NULL;
    p->rdl = 0;
    p->wdl = 0;
    p->cwdp = 0;
    p->rto = PLP_RTO;
    p->wto = PLP_WTO;
    p->rtimeoutid = 0;
    p->wtimeoutid = 0;
    p->bp = 0;
    CLEAR_CONTEXT(p->contexta);
    CLEAR_CONTEXT(p->contextb);
    p->plpinitfail = 0;
    p->ecp_mode_supp = 0;
    p->timed_out = 0;
    p->ecpregs = plpaddr;

    mutex_init(&p->rbufsmutex, MUTEX_DEFAULT, "rbufsmutex");
    sv_init(&p->rbufssv, SV_DEFAULT, "rbufssv");

    mutex_init(&p->ecpmutex, MUTEX_DEFAULT, "ecpmutex");

    mutex_init(&p->tmoutlock, MUTEX_DEFAULT, "tmoutlock");
    sv_init(&p->sv_tmout, SV_DEFAULT, "sv_tmout");

    /* create inventory entry */
    device_inventory_add(ecpp_vhdl,
			 INV_PARALLEL,
			 INV_EPP_ECP_PLP,
			 -1, 0, 0);

    /* Reset DMA state machines in IOC3 */
    plp_dma_reset(p);

    /* Clear interrupt bits */
    p->ecpregs->sio_ir = SIO_IR_PP;

    /* Clear interrupt enable bits */
    IOC3_WRITE_IEC(p->ioc3_soft, SIO_IR_PP);

    /* Reset printer */
    plp_reset(p);

    /* Before returning place port in Compatibility mode(default) */
    p->ecpregs->sregs.pp_ecr = (ECR_COMP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
    SET_MODE(p, PLP_COMP_MODE);
    SET_PHASE(p, COMP_FWD_IDLE_PHASE);

    /*
     * Tell the IOC3 where and how
     * to deliver our interrupts
     */
    ioc3_intr_connect(conn_vhdl, SIO_IR_PP,
		      (ioc3_intr_func_t)plpintr,
		      (intr_arg_t)p,
		      ecpp_vhdl,
		      ecpp_vhdl,
		      0);

    return (0);
}

static void
plpreloadme(vertex_hdl_t conn_vhdl)
{
    ecp_t	*plp;
    vertex_hdl_t plp_vhdl;
    graph_error_t rc;

    /*
     * If the ioc3 is using two connect points,
     * we want to connect to the second one.
     * If there is no GUEST edge, this leaves
     * conn_vhdl unmodified.
     */
    (void)hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

    /*
     * find our ecpp vertex.
     * Failure is not fatal,
     * we could have decided to
     * not attach to this vertex
     * for any of several reasons.
     */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_ECPP, &plp_vhdl);
    if (rc != GRAPH_SUCCESS)
	return;

    plp = ecp_get(plp_vhdl);

    /* reconnect our interrupt */
    ioc3_intr_connect(conn_vhdl, SIO_IR_PP,
		      (ioc3_intr_func_t)plpintr,
		      (intr_arg_t)plp,
		      plp_vhdl,
		      plp_vhdl,
		      0);

}

/* free all the device specific data maintained
 * by the driver.
 */
void
plpdetach(vertex_hdl_t conn_vhdl)
{
    ecp_t		   *plp;
    vertex_hdl_t	    plp_vhdl;
    vertex_hdl_t	    plpdir_vhdl;

    /*
     * If the ioc3 is using two connect points,
     * we want to connect to the second one.
     * If there is no GUEST edge, this leaves
     * conn_vhdl unmodified.
     */
    (void) hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

    /*
     * find our ecpp vertex.
     * Failure is not fatal,
     * we could have decided to
     * not attach to this vertex
     * for any of several reasons.
     */
    if (GRAPH_SUCCESS !=
	hwgraph_traverse(conn_vhdl, EDGE_LBL_ECPP, &plp_vhdl))
	return;

    plp = ecp_get(plp_vhdl);

    /* Clear interrupt enable bits */
    IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP);

    ecp_set(plp_vhdl, 0);	/* don't leave dangling ptr */

    /* disconnect our interrupt */
    ioc3_intr_disconnect(conn_vhdl, SIO_IR_PP,
			 (ioc3_intr_func_t) plpintr,
			 (intr_arg_t) plp,
			 plp_vhdl);

    /* Aftter we destroy the graph here,
     * we do not get /hw/parallel/plp%d
     * back when we reregister until
     * someone runs ioconfig again.
     */

    /*
     * if /hw/parallel/plp%d exists,
     * zap the plp%d edge.
     */
    if (GRAPH_SUCCESS ==
	hwgraph_traverse(hwgraph_root, PLP_HWG_DIR, &plpdir_vhdl)) {
	char			name[20];

	sprintf(name, "plp%d", plp->ctlr_num);
	(void) hwgraph_edge_remove(plpdir_vhdl, name, 0);
    }
    /*
     * delete the connection edge, we don't need
     * it any more either.
     */
    (void) hwgraph_edge_remove(conn_vhdl, EDGE_LBL_ECPP, 0);

    /* XXX- what we really want here is
     * a function to reverse a "device add"
     * including ripping our cdevsw info
     * out by the roots.
     *
     * XXX- due to mishandling of vertex
     * reference counts throughout the
     * system, this vertex never actually
     * goes away; but we have removed all
     * its incoming edges, so the only
     * downside here is the memory leak,
     * in vertex-sized chunks, each time we
     * unregister this driver.
     */
    hwgraph_vertex_destroy(plp_vhdl);

    freesema(&plp->dmasema);
    freesema(&plp->wlock);
    freesema(&plp->rlock);
    freesema(&plp->dlock);
    mutex_destroy(&plp->rbufsmutex);
    mutex_destroy(&plp->ecpmutex);
    sv_destroy(&plp->rbufssv);

    mutex_destroy(&plp->tmoutlock);
    sv_destroy(&plp->sv_tmout);

    DEL(plp);
}

int
plpopen(dev_t *devp, int mode)
{
    ecp_t *plp;

    if (!dev_is_vertex(*devp))
	    return ENXIO;
    
    plp = dev_to_ecp(*devp);

    if (plp_initfail || plp->plpinitfail)
	return EIO;

    /* Set state accordingly */
    if (mode & FWRITE)
	plp->state |= (ECPPLP_WROPEN | ECPPLP_OCCUPIED);

    if (mode & FREAD) {
	/* If device does not support ECP mode, fail open.
	 * Else, open for reading.
	 */
#ifdef INPUT
	plp->state |= (ECPPLP_RDOPEN | ECPPLP_OCCUPIED);
#endif /* INPUT */

    }

    /*
     * Setup the operation mode. For now, it is
     * determined by the ecpplp file variable
     * plpopmode. Since the plp->mode can be
     * changed dynamically via the PLPIOMODE ioctl
     * during the previous open, we may need to
     * reset the mode here. That's why the code
     * is here instead of in the attach routine.
     */

    if (plp->mode != plpopmode) {

	switch (plpopmode) {
	    case PLP_ECP_MODE:
		/* try ECP mode */
		if (plp_negotiate(plp, EXT_REQ_ECP)) {
			plp->ecpregs->sregs.pp_ecr =
                            (ECR_ECP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
                SET_MODE(plp, PLP_ECP_MODE);	
		} else {
			cmn_err(CE_WARN, "plpopen: failed to open ECP mode."
			    " failing back to Compatibility FIFO mode.");

			plp->ecpregs->sregs.pp_ecr =
			    (ECR_COMP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
			SET_MODE(plp, PLP_COMP_MODE);
		}

		break;
	    case PLP_PIO_MODE:

		/* use compatibility non-FIFO mode */
		plp->ecpregs->sregs.pp_ecr =
			    (ECR_PIO_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
		SET_MODE(plp, PLP_PIO_MODE);

		break;
	    case PLP_COMP_MODE:
	    default:
		/* use compatibility FIFO mode */
		plp->ecpregs->sregs.pp_ecr =
			    (ECR_COMP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
		SET_MODE(plp, PLP_COMP_MODE);

		break;
	}
    }

    dprintf("plpopen: using operation MODE %d\n", plp->mode);

    if (!(plp->state & ECPPLP_INUSE)) {
	plp->state |= ECPPLP_INUSE;
	atomicAddInt(&plp_inuse, 1);
    }

    return 0;
}

int plpclose(dev_t dev)
{
    ecp_t *plp = dev_to_ecp(dev);

    plp->state &= ~(ECPPLP_WROPEN | ECPPLP_RDOPEN | ECPPLP_NBIO | ECPPLP_OCCUPIED);

    plp_stop_dma(plp, B_READ|B_WRITE);

#ifdef INPUT
    rbuf_save(plp);
    plp_read_flush(plp);   /* flush read buffer */
#endif /* INPUT */

    plp_unlock(&plp->dlock);

    plp->rdl = plp->wdl = 0;  /* Reset pointers */
    plp->rbufs = 0;
    plp->cwdp = 0;

    atomicAddInt(&plp_inuse, -1);
    plp->state &= ~ECPPLP_INUSE;

    return 0;
}

/*
 * plpunload - check every device to be sure that there
 *      are no open references to this driver.  Return EBUSY if the driver
 *      is still in use.  If the driver is no longer in use, then free up
 *      any allocated resources and return 0.  The higher level mload
 *      unload function will guarantee that no opens are called on this
 *      driver while it is being unloaded.
 */
int
plpunload(void)
{
    /* OK to unload? */
    if (plp_inuse)
	return EBUSY;

    pciio_iterate("plp", plpunloadme);

    /* destroy mutexes & svs */
    mutex_destroy(&free_mds_mutex);
    sv_destroy(&free_mds_sv);

    mutex_destroy(&free_rbufs_mutex);
    sv_destroy(&free_rbufs_sv);

    plp_mem_free();

    return 0;
}

static void
plpunloadme(vertex_hdl_t conn_vhdl)
{
    ecp_t		   *plp;
    vertex_hdl_t	    plp_vhdl;
    graph_error_t	    rc;

    /*
     * If the ioc3 is using two connect points,
     * we want to connect to the second one.
     * If there is no GUEST edge, this leaves
     * conn_vhdl unmodified.
     */
    (void) hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);

    /*
     * find our ecpp vertex.
     * Failure is not fatal,
     * we could have decided to
     * not attach to this vertex
     * for any of several reasons.
     */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_ECPP, &plp_vhdl);
    if (rc != GRAPH_SUCCESS)
	return;

    plp = ecp_get(plp_vhdl);

    /* Clear interrupt enable bits */
    IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP);

    /* disconnect our interrupt */
    ioc3_intr_disconnect(conn_vhdl, SIO_IR_PP,
			 (ioc3_intr_func_t) plpintr,
			 (intr_arg_t) plp,
			 plp_vhdl);

}

/*ARGSUSED*/
int
plpread(dev_t dev, uio_t *uiop)
{

    if (uiop->uio_resid == 0)
	return 0;

    /* If using output only device then return EIO */

#ifdef INPUT
    if (!(plp->state & ECPPLP_RDOPEN)) /* Right now, return EIO in any case */
	/* Input not currently supported 	   */
#endif /* INPUT */
	return EIO;

#ifdef INPUT
    return uiophysio (plpio, NULL, dev, B_READ, uiop);
#endif /* INPUT */
}

int
plpwrite(dev_t dev, uio_t *uiop)
{
   if (uiop->uio_resid == 0)
      return 0;

   return uiophysio (plpio, NULL, dev, B_WRITE, uiop);
}

static void
plp_make_hwgraph_nodes(ecp_t *ecp)
{
    /*REFERENCED*/
    graph_error_t rc;
    int num;
    vertex_hdl_t plpdir_vhdl;
    char name[8];

    /* only do this once per device */
    if (atomicSetInt(&ecp->flags, ECPPLP_HWG_OK) & ECPPLP_HWG_OK)
	return;

    /* get the assigned number for this ecp device */
    num = device_controller_num_get(ecp->vhdl);
    if (num < 0 || num > 999)
        return;

    ecp->ctlr_num = num;

    /* make sure plp dir (parallel) exists */
    rc = hwgraph_path_add(hwgraph_root, PLP_HWG_DIR, &plpdir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    /* create a duplicate edge to the real device */
    sprintf(name, "plp%d", num);
    hwgraph_edge_add(plpdir_vhdl, ecp->vhdl, name);
}

/*ARGSUSED*/
int
plpioctl(dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
    volatile char dsr;
    ecp_t *plp = dev_to_ecp(dev);

    switch (cmd) {

    case PLPMKHWG:
	plp_make_hwgraph_nodes(plp);
	break;

    case PLPIOCRESET:
        plp_reset(plp);
	break;

    case PLPIOCSTATUS:
	dsr = plp->ecpregs->sregs.pp_dsr;
	*rvp = 0;
	if (!(dsr & DSR_NFAULT))	/* nFault; 0=fault */
	    *rvp |= PLPFAULT;
	if (dsr & DSR_PERROR)		/* PError; 1=paper end */
	    *rvp |= PLPEOP;
	if (dsr & DSR_SELECT)		/* Select; 1=online */
	    *rvp |= PLPONLINE;
	if (dsr & DSR_INTR)		/* device interrupt; 1=no ink */
	    *rvp |= PLPEOI;
	break;

    case PLPIOCTYPE:
	/* for compatibility */
        break;

    case PLPIOCSTROBE:
	/* for compatibility */
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
#ifdef INPUT
        /* if this is the first open for read, then start up the
         * read processing
         */
        if (!(plp->state & ECPPLP_RDOPEN)) {
	    plp_read_flush(plp);		/* flush read buffer */
	    plp->state |= ECPPLP_RDOPEN;
        }
#endif /* INPUT */
	break;

    case PLPIOMODE:
	/* grab dma lock since mode change during data transfer can result
	 * in data loss.
	 */
	if (plp_lock(&plp->dlock)) {
	  return(EINTR);
	} else {
	   plp_terminate(plp, 0);

	   /*
	    * PLP_* defs below are taken from plp.h which is what the user
 	    * is supposed to pass to the ioctl.
	    */
	   switch (arg) {
#ifdef INPUT
	       case PLP_BI:
#endif /* INPUT */
	       case PLP_ECP:
		   if (!plp_negotiate(plp, EXT_REQ_ECP)) {
		     plp_unlock(&plp->dlock);
		     return (ENXIO);
		   }
		   plp->ecpregs->sregs.pp_ecr |=
				    (ECR_ECP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
		   SET_MODE(plp, PLP_ECP_MODE);

	       break;

	       case PLP_PIO:

			/* set compatibility non-FIFO mode */
			plp->ecpregs->sregs.pp_ecr =
			    (ECR_PIO_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
			SET_MODE(plp, PLP_PIO_MODE);

		break;

	       case PLP_CMPTBL:
	       default: /* Set to compatibility FIFO mode by default */
                   plp->ecpregs->sregs.pp_ecr |=
                                    (ECR_COMP_MODE | ECR_ENFLTINTB | ECR_SVCINTB);
                   SET_MODE(plp, PLP_COMP_MODE);

	       break;
	   }
	   plp_unlock(&plp->dlock);
	}
	break;

    default:
	return EINVAL;
    }
    return 0;
}

int
plpio (struct buf *bp)
{
    void *vaddr;
    /*REFERENCED*/
    int xferbytes;
    memd_t *dl;
    volatile int dcr, ecr;

    ecp_t *plp = dev_to_ecp(bp->b_edev);

    bp->b_resid = bp->b_bcount;

    if (bp->b_flags & B_READ) {

#ifdef INPUT
	/* Verify mode here ! */
	if (plp->mode != PLP_ECP_MODE)
	  return (EIO);

	if ((plp->mode == PLP_ECP_MODE && (plp->phase != ECP_REV_IDLE_PHASE &&
						!plp_fwdtorev(plp))))
	  return (EIO);

      /* grab the read lock so no processes get intermingled data */
      if (plp_lock(&plp->rlock)) {
	  bp->b_error = EINTR;
      } else {

	/* enable service interrupt and nFault interrupt */
	plp->ecpregs->sregs.pp_ecr &= ~(ECR_SVCINTB | ECR_ENFLTINTB);

	while (bp->b_resid && !bp->b_error) {
	    vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);
	    /* copy as much data as we have buffered on the channel */
	    LOCK(&plp->rbufsmutex);
	    if (!EMPTYNODE(plp->rbufs))
		bp->b_resid -= plp_read_copy (plp, vaddr, bp->b_resid);

	    UNLOCK(&plp->rbufsmutex);
	    if (plp->timed_out){ /* Break out of loop here and return
				  * buffered data to application
				  * program.
				  */
		plp->timed_out = 0;
		break;
	    }

	    /* if necessary, wait for additional data to arrive
	     * asynchronously.
	     */
	    LOCK(&plp->rbufsmutex);
	    if (bp->b_resid && EMPTYNODE(plp->rbufs)){
		if (sv_wait_sig(&plp->rbufssv, PZERO+1,
				&plp->rbufsmutex, 0))
                    bp->b_error = EINTR;
	    } else {
		UNLOCK(&plp->rbufsmutex);
	    }
	}
	plp_unlock(&plp->rlock);
      }
#endif /* INPUT */

    } else {

	/*
	 * Check what type of operation mode we are in.
	 * ECP and Compatibility FIFO are lumped together
	 * and use DMA. Compatibility non-FIFO uses straight
	 * PIO accesses.
	 */
	if (plp->mode != PLP_PIO_MODE) {

	    /* Verify mode here ! */
	    if ((plp->mode == PLP_ECP_MODE && (plp->phase != ECP_FWD_IDLE_PHASE &&
				!plp_revtofwd(plp))))
		return (EIO);

            /* grab the write lock so no processes send intermingled data */
	    if (plp_lock(&plp->wlock)) {
		bp->b_error = EINTR;
            } else {

        	while (bp->b_resid && !bp->b_error) {
		    /* determine the virtual address of the next portion of the
		     * buffer to transfer, then create a memory descriptor list
	             * for the transfer
	             */
	            vaddr = bp->b_dmaaddr + (bp->b_bcount - bp->b_resid);

	            xferbytes = plp_queue (plp, &dl, vaddr, bp->b_resid);

#ifdef HEART_COHERENCY_WAR
	    	    heart_dcache_wb_inval(vaddr, xferbytes);
#endif /* HEART_COHERENCY_WAR */
	        
	            /* For DMA xfer, grab dma lock here */
	            if (plp_lock(&plp->dlock)) {
		        bp->b_error = EINTR;
			plp_free(dl);
			break;
	    	    }

		    /* start DMA transfer */
		    plp->bp = bp;
		    plp_dma(plp, KDM_TO_IOPHYS(dl), B_WRITE);

		    /* Wait for transfer to complete */
		    if (psema(&plp->dmasema, (PZERO+1))) {
			bp->b_error = EINTR;
			plp_stop_dma(plp, B_WRITE);
			plp_unlock(&plp->dlock);
		    }

		    /* Put the descriptor list back on the free list */
		    plp_free(dl);

		    /* Resid was already fixed up by the interrupt handler (do_xfer()
		     * for PIO). For a partial transfer, we need to clear b_error.
		     */
		    if (bp->b_error == EAGAIN) {
			bp->b_error = 0;
			break;
		    }
	        } /* of while */
		plp_unlock(&plp->wlock);
	    }
	} else {	/* compatibility non-FIFO mode */
	    uint_t tm;
	    uint_t retry_delay;
	    register uint_t bcount;
	    char *cp;
	    uint64_t us_tot = 0;


	    cp = (char *)bp->b_dmaaddr;


#ifdef HEART_COHERENCY_WAR
	    	    heart_dcache_wb_inval(bp->b_dmaaddr, bp->b_bcount);
#endif /* HEART_COHERENCY_WAR */

	    for (cp = (char *)bp->b_dmaaddr, bcount = bp->b_bcount;
		bcount; cp++, bcount--) {
		    retry_delay = plpRetryDelay1;
		    tm = plpNumRetries1;

		    /* wait for device ready */

		    while (!(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_BUSY)) { 
			if (!tm--) {
			   /*
		    	    * The printer has been busy for a while.
			    * It could be offline, so bump up
			    * the delay.
			    */
			    retry_delay = plpRetryDelay2;
			}
			/* Sleep for retry_delay or until signal */
			if (plp_sigdelay(plp, retry_delay)) {
			    bp->b_error = EINTR;
			    goto pio_done;
			}

			us_tot += (retry_delay * 1000000) / HZ;

			/* Check if we've timed out */
			if ((plp->wto > 0) &&
			    (plp->wto * 1000000 / HZ < us_tot))
				goto pio_done;
		    }
		    /* write out the data */
		    plp->ecpregs->sregs.pp_data = *cp;
		    DELAYBUS(1);
		    plp->ecpregs->sregs.pp_dcr |= DCR_NSTB;
		    DELAYBUS(1);
		    plp->ecpregs->sregs.pp_dcr &= ~(DCR_NSTB);
		    bp->b_resid--;
		    DELAYBUS(plpStrobeDelay);

		    us_tot += 2 + plpStrobeDelay;

	    	    /* Check if we've timed out */
		    if ((plp->wto > 0) &&
		        (plp->wto * 1000000 / HZ < us_tot))
			    goto pio_done;
	    }
	    bp->b_error = 0;
pio_done:
	    plp_unlock(&plp->wlock);		
	}
    } /* of if */

    if (bp->b_error)
	bp->b_flags |= B_ERROR;

    iodone (bp);

    return 0;
}

int
plp_queue(ecp_t *plp, memd_t **pdl, void *vaddr, int bcount)
{
    memd_t  *next, *current;
    unsigned size, bytes = 0;

    /* get at least a first memd for the transfer */
    do {
        LOCK(&free_mds_mutex);
        LGETNODE(free_mds, current);
        if (EMPTYNODE(current)) {
            if (sv_wait_sig(&free_mds_sv, PZERO+1,
			    &free_mds_mutex, 0)) {
		UNLOCK(&free_mds_mutex);
		return -1;    /* interrupted */
	    }
        } else {
            UNLOCK(&free_mds_mutex);
	}
    } while (EMPTYNODE(current));

    *pdl = (memd_t *)kvtophys(current);

    do {
        /* if the address crosses a page boundary
         * set the size only up to that boundary
         */
        if ((size = NBPP - poff(vaddr)) > bcount)
	    size = bcount;

        current->memd_bc = size;
        current->memd_cbp = KVTOIOADDR(plp, (void *)vaddr);

        bcount -= size;
        bytes += size;
        vaddr = (void *)((caddr_t)vaddr + size);

        if (bcount > 0) {
            LOCK(&free_mds_mutex);
            LGETNODE(free_mds, next);
	    UNLOCK(&free_mds_mutex);
            if (!EMPTYNODE(next)) {
                current->memd_forw = (memd_t *)kvtophys(next);
                current = next;
            }
        }

    } while (!EMPTYNODE(next) && bcount > 0);

    current->memd_forw = 0;
    return (bytes);
}

/*
 * plp_free - free a memory descriptor list
 *
 * the descriptor list was presumably created by plp_queue()
 * dl and all nbdp's in list are physical addresses
 */
void
plp_free(memd_t *dl)
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

int
plp_mem_init(void)
{
    mdmem = (caddr_t)kmem_alloc(MDMEMSIZE, KM_CACHEALIGN);
    if (!mdmem)
       cmn_err(CE_WARN, "mdmem allocation failed\n");
#ifdef INPUT
    bufmem = (caddr_t)kvpalloc(NRP, VM_NOSLEEP|VM_DIRECT|VM_UNCACHED, 0);
    if (!bufmem)
       cmn_err(CE_WARN, "bufmem allocation failed\n");
#endif /* INPUT */
    if (!mdmem /* || !bufmem */) {
       plp_mem_free();
       cmn_err(CE_WARN, "ecpplp: out of memory for ecpplp driver");
       return ENOMEM;
    }
    return 0;
}

void
plp_mem_free(void)
{
    if (mdmem)
       kmem_free(mdmem, MDMEMSIZE);
#ifdef INPUT
    if (bufmem)
       kvpfree(bufmem, NRP);
#endif /* INPUT */
    mdmem = 0;
#ifdef INPUT
    bufmem = 0;
#endif /* INPUT */
}

/*
 * plp_sigdelay()
 *
 * Description:
 *    Sleep for specified number of ticks, or until signal
 */
static int
plp_sigdelay(ecp_t *plp, int ticks)
{
    int tmoutid = 0;

    LOCK(&plp->tmoutlock);
    tmoutid = timeout(plp_sigdelay_tmout, plp, ticks);
    if (sv_wait_sig(&plp->sv_tmout, PZERO, &plp->tmoutlock, 0) < 0) {
	LOCK(&plp->ecpmutex);
        if (tmoutid)
            untimeout(tmoutid);
    	UNLOCK(&plp->ecpmutex);
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
plp_sigdelay_tmout(ecp_t *plp)
{
    LOCK(&plp->tmoutlock);
    sv_signal(&plp->sv_tmout);
    UNLOCK(&plp->tmoutlock);
}

#ifdef INPUT
/*
 * rbuf_init - initialize free read buffer descriptor list
 */

void
rbuf_init(void)
{
    caddr_t pbuf;
    int pcount;
    memd_t *md;

    pbuf = bufmem;

    LOCK(&free_mds_mutex);
    LOCK(&free_rbufs_mutex);
    for (pcount = 0; pcount < NRMD; pcount++) {
        LGETNODE (free_mds, md);
        md->memd_buf = (caddr_t)kvtophys(pbuf);
        LPUTNODE (free_rbufs, md);
        pbuf += NBPRD;
    }
    UNLOCK(&free_rbufs_mutex);
    UNLOCK(&free_mds_mutex);
}
#endif /* INPUT */

/*
 * Kinds of interrupts possible in ECP/Compatibility mode:
 * 1. Service Interrupt - this indicates that the FIFO is ready for further
 *    read/write operation.
 *    # DMAen =1, svcintb =0 and the DMA reaches a terminal count (PPDACKZ =0,
 *      TC =1).
 *    # DMAen =0, svcintb =0, DIR =0 (write) and the FIFO has less than 8 bytes
 *    # DMAen =0, svcintb =0, DIR =1 (read) and the FIFO has more than 8 bytes
 * 2. nFault Interrupt - this is set by the device when it has data to send
 *    # enFltIntb =0, nFAULT goes from high to low (done by device)
 * 3. DMA Context completion interrupts
 *    # DMAen =1, svcintb =1, DIR =0 (write) and context A or B completes
 *    # DMAen =1, svcintb =1, DIR =1 (read) and context A or B completes
 *
 * Acknowledge interrupts are possible in modes 000 and 001 when ackInten is set.
 * ackInten should be disabled in all other modes. Not used currently.
 */

static void
plpintr(intr_arg_t arg, ioc3reg_t sio_ir)
{
    volatile int ecr, ppcr;
    memd_t *vdl;
    ecp_t *plp = (ecp_t *)arg;
    int bc;

#ifdef INPUT
    if (!(dsr & DSR_NFAULT)){ /* nFault Interrupt */
	/* Device has data to send. Initiate read only if
	 * the port is open for reading
	 */
	/* If timeout is set, cancel it first */
	if (plp->rtimeoutid) {
	    untimeout(plp->rtimeoutid);
	    plp->rtimeoutid = 0;
	}

	rbuf_save(plp);
	if (plp->phase == ECP_REV_IDLE_PHASE) {
	    if (plp->state & ECPPLP_RDOPEN){
		plpintr_read(plp, 0);
	    }
	}

    }
#endif /* INPUT */
	/* PP_INT_A or PP_INT_B context completion interrupts */
    if ((sio_ir & SIO_IR_PP_INTA) || (sio_ir & SIO_IR_PP_INTB)) {

	/* We grab the lock around the whole thing for now */
	LOCK(&plp->ecpmutex);

	if ((plp->phase == ECP_FWD_IDLE_PHASE) ||
	    (plp->phase == COMP_FWD_IDLE_PHASE)) { /* Output */

	    if (plp->wtimeoutid) {
		untimeout(plp->wtimeoutid);
		plp->wtimeoutid = 0;
	    }

	    vdl = (memd_t *)PHYS_TO_K0(plp->cwdp);
context_a: 
	    /* Context A completed */
	    if ((sio_ir & SIO_IR_PP_INTA)) {

		plp->ecpregs->sio_ir = SIO_IR_PP_INTA;

		plp->bp->b_resid -= plp->contexta.bc;  /* decrement resid byte count */

		if (vdl->memd_forw) { /* More buffers in chain */
		    plp->cwdp = (__uint64_t) vdl->memd_forw;  /* Store descriptor in ecp struct */
		    vdl = (memd_t *) PHYS_TO_K0(vdl->memd_forw);

		    /* Load buffer address into PPAR_H_A & PPAR_L_A */
		    plp->ecpregs->ppbr_l_a = LOW32(vdl->memd_cbp);
		    plp->ecpregs->ppbr_h_a = HIGH32(vdl->memd_cbp);

		    /* Store context info in ecp struct */
		    plp->contexta.cbp_l = vdl->memd_cbp;
		    plp->contexta.bc = vdl->memd_bc;

		    IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INTA | SIO_IR_PP_INTB);

		    if (!vdl->memd_forw) {
			plp->final = 1;
			/* clear PPINT_A to prevent taking an extra interrupt */
			IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP_INTA);
			IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INT);
		    }

		    plp->ecpregs->ppcr_a = (vdl->memd_forw ?(vdl->memd_bc | PPCR_EN):
						     (vdl->memd_bc | PPCR_EN | PPCR_LAST));
		} else {
		    if (plp->final) /* re-enable service interrupt */
			IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INT|SIO_IR_PP_INTB);
		}
		sio_ir &= ~SIO_IR_PP_INTA;
	    }

		/* Context B completed */
            if ((sio_ir & SIO_IR_PP_INTB)) {
		plp->bp->b_resid -= plp->contextb.bc;  /* decrement resid byte count */

		plp->ecpregs->sio_ir = SIO_IR_PP_INTB;

		if (vdl->memd_forw) { /* More buffers in chain */
		    plp->cwdp = (__uint64_t) vdl->memd_forw;  /* Store descriptor in ecp struct */
		    vdl = (memd_t *) PHYS_TO_K0(vdl->memd_forw);

		    /* Load buffer address into PPAR_H_B & PPAR_L_B */
		    plp->ecpregs->ppbr_l_b = LOW32(vdl->memd_cbp);
		    plp->ecpregs->ppbr_h_b = HIGH32(vdl->memd_cbp);

		    /* Store context info in ecp struct */
		    plp->contextb.cbp_l = vdl->memd_cbp;
		    plp->contextb.bc = vdl->memd_bc;

		    IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INTA | SIO_IR_PP_INTB);

		    if (!vdl->memd_forw) {/* clear PPINT_B to prevent taking an extra interrupt */
			plp->final = 1;
			IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP_INTB);
			IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INT);
		    }

		    plp->ecpregs->ppcr_b = (vdl->memd_forw ? (vdl->memd_bc | PPCR_EN):
						     (vdl->memd_bc | PPCR_EN | PPCR_LAST));
		} else {
		    if (plp->final) /* re-enable service interrupt */
			IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INT|SIO_IR_PP_INTA);
		}
		sio_ir &= ~SIO_IR_PP_INTB;
		if (sio_ir & SIO_IR_PP_INTA) {
			goto context_a;
		}
            }

            /* Restart timeout */
            if (plp->wto) {
		plp->wtimeoutid = dtimeout(plpwtimeout, plp, plp->wto, plbase, cpuid());
            } else
                plp->wtimeoutid = 0;
	}

	UNLOCK(&plp->ecpmutex);
    }

    if (sio_ir & SIO_IR_PP_INT) {  /* Service Interrupt */

	/* Clear service interrupt */
	plp->ecpregs->sregs.pp_ecr &= ~ECR_SVCINTB;

	plp->ecpregs->sio_ir = (SIO_IR_PP_INT );
	IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP_INT );

	/* Next 8 lines duplicated here for 2-superIO h/w */
#ifdef INPUT
	if (plp->phase == ECP_REV_BUSY_PHASE) {
	    /* If timeout is set, cancel it first */
	    if (plp->rtimeoutid) {
		untimeout(plp->rtimeoutid);
		plp->rtimeoutid = 0;
	    }

	    /* Update byte count in the read buffer */
	    bc = plp->ecpregs->ppcr_a;
	    bc = (bc & PPCR_COUNT_MASK);
	    LOCK(&plp->ecpmutex);
	    ((memd_t *) PHYS_TO_K0(plp->rdl))->memd_bc = NBPRD - bc;
	    UNLOCK(&plp->ecpmutex);

	    rbuf_save(plp);
	    if (plp->state & ECPPLP_RDOPEN){
		plpintr_read(plp, 0);
	    }
	} else
#endif /* INPUT */
	    if ((plp->phase == ECP_FWD_IDLE_PHASE) ||
		(plp->phase == COMP_FWD_IDLE_PHASE)) {
		/* service interrupt - implies end of current
		 * write buffer list. Wakeup sleeping plpio
		 */

		/* Reset DMA state m/c */
		plp_dma_reset(plp);

		LOCK(&plp->ecpmutex);
		plp->bp->b_resid = 0;

		/* clear out the DMA contexts */
		CLEAR_CONTEXT(plp->contexta);
		CLEAR_CONTEXT(plp->contextb);
		plp->cwdp = 0;

		/* clear any timeouts */
		if (plp->wtimeoutid) {
		    untimeout(plp->wtimeoutid);
		    plp->wtimeoutid = 0;
		}
		UNLOCK(&plp->ecpmutex);

		plp_unlock(&plp->dlock);
		vsema(&plp->dmasema);
	    }
    }

     if ((sio_ir & SIO_IR_PP_MEMERR)) {
	/* on a partial write, set an error flag */

	LOCK(&plp->ecpmutex);

	plp->bp->b_error = EAGAIN;   /* indicates try again */

	/* Disable DMA */
	plp->ecpregs->ppcr &= ~PPCR_DMA_EN;

	/* Determine which context was currently busy */
	/* Update resid byte count, clear dma contexts, release DMA lock */
	ppcr = plp->ecpregs->ppcr;

	if (ppcr & PPCR_BUSY_A) {
	    bc = plp->ecpregs->ppcr_a;
	    bc = (bc & PPCR_COUNT_MASK);
	    plp->bp->b_resid -= (plp->contexta.bc - bc);

	} else if (ppcr & PPCR_BUSY_B) {
	    bc = plp->ecpregs->ppcr_b;
	    bc = (bc & PPCR_COUNT_MASK);
	    plp->bp->b_resid -= (plp->contextb.bc - bc);
	}

	/* Clear out contexts */
	CLEAR_CONTEXT(plp->contexta);
	CLEAR_CONTEXT(plp->contextb);

	UNLOCK(&plp->ecpmutex);

	plp_dma_reset(plp);

	plp_unlock(&plp->dlock); /* release DMA lock */
	vsema(&plp->dmasema);
     }

}

/*
 * plp_lock - protect port and dma channel accesses
 *
 * returns 1 if it catches a signal before getting the lock
 * returns 0 if it gets the lock successfully
 */
static int
plp_lock(sema_t *sema)
{
    if (psema(sema, PZERO+1) < 0)
	return 1;
    return 0;
}
/*
 * plp_unlock - give up port and dma channel access locks
 * returns 1 if it wakes up another process waiting on the lock
 * returns 0 if no process gets awakened
 */
static int
plp_unlock(sema_t *sema)
{
    return (vsema(sema));
}

void
plp_reset(ecp_t *plp )
{
    if (plp_lock(&plp->dlock)) {
      return;
    } else {

      plp->ecpregs->sregs.pp_dcr = DCR_NSLCTIN;
      DELAY(4000);
      plp->ecpregs->sregs.pp_dcr = (DCR_NSLCTIN | DCR_NAFD | DCR_NINIT);

      plp_unlock(&plp->dlock);

    }
    return;
}
/*
 * plp_war_delay()
 *
 * Description:
 *   Work-around delay for some printers, e.g. HP DJ1600C.
 *   delay plpwardely * 1ms to prevent possible data corruption in
 *   compatibilty mode, if printer is still busy right before write
 *   ms < 0: always force delay
 *      > 0: conditional delay
 */
static void
plp_war_delay (ecp_t *plp, int ms)
{
    int force = 0;

    if (plp->mode == PLP_COMP_MODE) {
        /* check conditional delay */
        if (ms < 0) {
                force = 1;
                ms *= -1;
        }
	while (ms-- && (force || !(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_BUSY)))
		DELAY(1000);
    }
}


void
plp_dma(ecp_t *plp, __uint64_t dl, int flags)
{
    memd_t *vdl= (memd_t *)PHYS_TO_K0(dl);

    volatile int ecr, sio_ir, sio_iec, sio_ies;
    volatile int dcr, dsr;
    volatile int ppcra, ppcrb, ppcr, pci_scr;
    volatile int ppbr_l_a, ppbr_h_a, ppbr_l_b, ppbr_h_b;

    if (flags == B_WRITE) {
	LOCK(&plp->ecpmutex);
	plp->wdl = dl;  /* Store head of desc. list in ecp.wdl */
	plp->cwdp = dl;  /* Store current position of write desc pointer in ecp.cwdp */

	/* For certain printers, such as HP 1600C, allow delay
 	** if the printer is busy */

	if (plpwardelay)
		plp_war_delay(plp, plpwardelay);

	/* Set up output direction */
	plp->ecpregs->sregs.pp_dcr &= ~DCR_DIR;

	/* Set DMA_EN (bit 31) = 1 &
	 * DIR_FROM_PP (bit 29) = 0 (write)
	 */
	plp->ecpregs->ppcr |= (PPCR_DMA_EN );
	plp->ecpregs->ppcr &= ~PPCR_DIR_FROM_PP;

	/* clear all parallel port interrupts
	 */
	plp->ecpregs->sio_ir = SIO_IR_PP;

	/* enable all parallel port interrupts
	 */
	IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP);

	/* Load buffer address into PPAR_H_A & PPAR_L_A */
	plp->ecpregs->ppbr_l_a = LOW32(vdl->memd_cbp);
	plp->ecpregs->ppbr_h_a = HIGH32(vdl->memd_cbp);
	if (!vdl->memd_forw) { /* Last desc in list - disable PPINT_A 
				* to avoid taking an extra interrupt
				*/
	    IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP_INTA);
	}

	plp->ecpregs->ppcr_a = (vdl->memd_forw ? (vdl->memd_bc | PPCR_EN) :
					 (vdl->memd_bc | PPCR_EN | PPCR_LAST));

	/* Store context information in ecp struct */
	plp->contexta.cbp_l = vdl->memd_cbp;
	plp->contexta.bc = vdl->memd_bc;

	if (vdl->memd_forw) {

	    plp->cwdp = (__uint64_t) vdl->memd_forw;  /* Store descriptor in ecp struct */
	    vdl = (memd_t *) PHYS_TO_K0(vdl->memd_forw);

	    /* Load buffer address into PPAR_H_B & PPAR_L_B */
	    plp->ecpregs->ppbr_l_b = LOW32(vdl->memd_cbp);
	    plp->ecpregs->ppbr_h_b = HIGH32(vdl->memd_cbp);
	    if (!vdl->memd_forw) { /* Last desc in list - disable PPINT_B 
				    * to avoid taking an extra interrupt
				    */

		IOC3_WRITE_IEC(plp->ioc3_soft, SIO_IR_PP_INTB);
		plp->final = 1;
	    } else {
		plp->final = 0;
	    }

	    plp->ecpregs->ppcr_b = (vdl->memd_forw ? (vdl->memd_bc | PPCR_EN) :
					     (vdl->memd_bc | PPCR_EN | PPCR_LAST));

	    /* Store context information in ecp struct */
	    plp->contextb.cbp_l = vdl->memd_cbp;
	    plp->contextb.bc = vdl->memd_bc;

	} else {
	    plp->final = 1;
	}

	/* Enable service interrupt & enable DMA */
	ecr = plp->ecpregs->sregs.pp_ecr;
	plp->ecpregs->sregs.pp_ecr = ((ecr | ECR_DMA_EN) & ~ECR_SVCINTB);

	if (plp->wto) {
	    plp->wtimeoutid = dtimeout(plpwtimeout, plp, plp->wto, plbase, cpuid());
	} else
	    plp->wtimeoutid = 0;

	UNLOCK(&plp->ecpmutex);
    }
#ifdef INPUT
    else if (flags == B_READ) {
	LOCK(&plp->ecpmutex);
	plp->rdl = dl;

	/* Set up input direction */
	plp->ecpregs->sregs.pp_dcr |= DCR_DIR;

	/* Set DMA_EN (bit 31) = 1 &
	 * DIR_FROM_PP (bit 29) = 1 (read)
	 */
	plp->ecpregs->ppcr |= (PPCR_DMA_EN | PPCR_DIR_FROM_PP);

	/* clear out current interrupt state */
	plp->ecpregs->sio_ir = SIO_IR_PP;

	/* enable parallel port INT and MEMERR interrupts. */
	IOC3_WRITE_IES(plp->ioc3_soft, SIO_IR_PP_INT | SIO_IR_PP_MEMERR);

	/* Load buffer address into PPAR_H_A & PPAR_L_A */
	plp->ecpregs->ppbr_l_a = LOW32(vdl->memd_cbp);
	plp->ecpregs->ppbr_h_a = HIGH32(vdl->memd_cbp);
	plp->ecpregs->ppcr_a = (NBPRD | PPCR_EN | PPCR_LAST);

	/* Store context information in ecp struct */
	plp->contexta.cbp_l = vdl->memd_cbp;
	plp->contexta.bc = NBPRD;

	ecr = plp->ecpregs->sregs.pp_ecr;
	plp->ecpregs->sregs.pp_ecr = 
		((ecr | ECR_ENFLTINTB | ECR_DMA_EN) & ~ECR_SVCINTB));
	/* Enable service interrupt, disable
	 * nFault interrupts and enable DMA
	 */
	/* Now start timeout */
	if (plp->rto) {
	    plp->rtimeoutid = dtimeout(plprtimeout, plp, plp->rto, plbase, cpuid());
	} else
	    plp->rtimeoutid = 0;

	UNLOCK(&plp->ecpmutex);
    }
#endif /* INPUT */
}

#if DEBUG && ECP_DEBUG

void
plp_show_dl(ecp_t *plp)
{
    memd_t *k0dl;
    __uint64_t dl = (__uint64_t) plp->rdl;
    memd_t *k0rbuf;
    __uint64_t rbuf = (__uint64_t) plp->rbufs;


    if (!dl) {
	printf("plp: dl empty\n");
    } else {
	while (dl) {
	    k0dl = (memd_t *)PHYS_TO_K0(dl);
	    printf("ecpplp: dl 0x%x, bc %d, cbp 0x%x, nbdp 0x%x\n",
		   dl, k0dl->memd_bc, k0dl->memd_cbp, k0dl->memd_nbdp);
	    dl = (__uint64_t) k0dl->memd_forw;
	}
    }
    if (!rbuf) {
	printf("plp: rbufs empty\n");
    } else {
	while (rbuf) {
	    k0rbuf = (memd_t *)PHYS_TO_K0(rbuf);
	    printf("ecpplp: rbuf 0x%x, bc %d, cbp 0x%x, nbdp 0x%x\n",
		   rbuf, k0rbuf->memd_bc, k0rbuf->memd_cbp, k0rbuf->memd_nbdp);
	    rbuf = (__uint64_t) k0rbuf->memd_forw;
	}
    }
}

#endif /* DEBUG && ECP_DEBUG */

#ifdef INPUT

/*
 * plp_read_copy - copy data from a channel's input buffer list to vaddr
 */
int
plp_read_copy (ecp_t *plp, caddr_t vaddr, int nbytes)
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
            bcopy ((void *)PHYS_TO_K0(rb->memd_buf), cp, rb->memd_bc);

            bcount -= rb->memd_bc;
            cp += rb->memd_bc;
            del_rbuf(rb);
            /* get next rbuf from list if more bytes are to
             * be transfered
             */
            if (bcount)
                LGETNODE (plp->rbufs, rb);
	}
        /* copy a portion of the read buffer to cp
         */
        else {
            bcopy ((void *)PHYS_TO_K0(rb->memd_buf), cp, bcount);

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

/*
 * del_rbuf - put an rbuf back onto the free list
 * Locks held on entry : free_rbufs_mutex
 */
void
del_rbuf (memd_t *rbuf)
{
    int dowakeup = (KDM_TO_IOPHYS(free_rbufs) == 0);

    rbuf->memd_cbp &= ~(NBPRD - 1);     /* realign buffer pointer */
    bzero((void *)PHYS_TO_K1(rbuf->memd_buf), NBPRD);
    rbuf->memd_bc = 0;			/* reset byte count */
    LPUTNODE(free_rbufs, rbuf);
    if (dowakeup)
        sv_broadcast (&free_rbufs_mutex);
}

void
plpintr_read(ecp_t *plp, int timedout)
{
    memd_t *rb =new_rbuf(1);

    if (EMPTYNODE(rb)) {
	/* we ran out of rbufs.
	 * We'll be back in less than a second, and will allocate
	 * more rbufs then.
	 */
	timeoutid=dtimeout(plptimeout1, plp, plp->rto, plbase, cpuid());
	plp->rdl = 0;
    } else {
        if (timeoutid){
	    untimeout(timeoutid);
	    timeoutid=0;
        }
        /* we got an rbuf. start a read
         */
        rb->memd_bc = 0;
        rb->memd_forw = 0;
        if (timedout) { /* Scenario - Read timed out which implies
                         * that data in current read is over. There is no write waiting, hence we
                         * have initiated another read. Need to clear DMA contexts here
                         */
	    plp->ecpregs->sregs.ppcr &= ~PPCR_DMA_EN;
	    plp_dma_reset(plp);
        }
        plp_dma(KDM_TO_IOPHYS(rb), B_READ);
    }
}

/*
 * new_rbuf - allocate an rbuf from the free list
 *
 * will always return with an rbuf unless the list is empty
 * and it is called from interrupt level, or a signal is received
 */
memd_t *
new_rbuf (int ilev)
{
    memd_t *rbuf;

    do {
	LOCK(&free_rbufs_mutex);
        LGETNODE(free_rbufs, rbuf);
        if (EMPTYNODE(rbuf)) {
	      if (sv_wait_sig(&free_rbufs_sv, (PZERO+1), &free_rbufs_mutex, 0))
                break;
        } else {
	        UNLOCK(&free_rbufs_mutex);
        }
    } while (EMPTYNODE(rbuf));
    return rbuf;
}

/*
 * rbuf_save - move newly received data from dl to rbuf list
 *
 * addresses are converted from physical to kernel again
 * unused nodes in descriptor list are returned to free rbuf list
 * this is called from interrupt level only
 */
void
rbuf_save(ecp_t *plp)
{
    __uint64_t dl = plp->rdl;
    memd_t *newbuf;
    memd_t *inbufs;
    int rbcount = 0;

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
    newbuf = (memd_t *) dl;
    while ((dl != 0) ) {
        newbuf = (memd_t *)PHYS_TO_K0(dl);

        /* adjust buffer length for last descriptor if transfer
         * is incomplete
         */

        if (newbuf->memd_bc == 0)
	    break;

        dl = (__uint64_t) newbuf->memd_forw;
        newbuf->memd_forw = 0;
        if (!EMPTYNODE(inbufs))
            inbufs->memd_forw = (memd_t *)(newbuf);
        else
            plp->rbufs = newbuf;
        inbufs = newbuf;
        rbcount++;
    }

    /* wakeup any process that may be awaiting input on this channel
     */

    /* free up unused buffers
     */
    while (dl != 0) {
        newbuf = (memd_t *)PHYS_TO_K0(dl);
        dl = (__uint64_t) newbuf->memd_forw;
        del_rbuf (newbuf);
    }

    if (rbcount)
	sv_broadcast(&plp->rbufssv);
    UNLOCK(&plp->rbufsmutex);

    plp->rdl = 0;
}

/*
 * plp_read_flush - flush the input buffer list on a channel
 */
void
plp_read_flush(ecp_t *plp)
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

void
plprtimeout(ecp_t *plp)
{
    int bc;

    plp->timed_out = 1;

    /* Update byte count in the read buffer */
    bc = plp->ecpregs->ppcr_a;
    bc = (bc & PPCR_COUNT_MASK);
    ((memd_t *) PHYS_TO_K0(plp->rdl))->memd_bc = NBPRD - bc;

    rbuf_save(plp);  /* Save the currently buffered data onto rbuf list */

    /* Allow a write if one is waiting. If no write is waiting, then
     * start a read if the port is open for reading.
     */
    if (plp->phase == ECP_REV_IDLE_PHASE)
	if (!plp_unlock(&plp->dlock) &&
	    (plp->state & ECPPLP_RDOPEN)){
	    plp_lock(&plp->dlock);
	    plpintr_read(plp, 1);
	}
}
#endif /* INPUT */


void
plpwtimeout(ecp_t *plp)
{
    volatile int ppcr;
    int bc;

    /* on a partial write, set an error flag */

    LOCK(&plp->ecpmutex);

    plp->bp->b_error = EAGAIN;   /* indicates write timed out */

    /* Disable DMA */
    plp->ecpregs->ppcr &= ~PPCR_DMA_EN;

    /* Determine which context was currently busy */
    /* Update resid byte count, clear dma contexts, release DMA lock */
    ppcr = plp->ecpregs->ppcr;

    if (ppcr & PPCR_BUSY_A) {
        bc = plp->ecpregs->ppcr_a;
	bc = (bc & PPCR_COUNT_MASK);
	plp->bp->b_resid -= (plp->contexta.bc - bc);
    } else if (ppcr & PPCR_BUSY_B) {
	bc = plp->ecpregs->ppcr_b;
	bc = (bc & PPCR_COUNT_MASK);
	plp->bp->b_resid -= (plp->contextb.bc - bc);
    }

    /* Clear out contexts */
    CLEAR_CONTEXT(plp->contexta);
    CLEAR_CONTEXT(plp->contextb);

    UNLOCK(&plp->ecpmutex);

    plp_dma_reset(plp);

    plp_unlock(&plp->dlock); /* release DMA lock */
    vsema(&plp->dmasema);
}

#if 0

void
plpwtimeout(ecp_t *plp)
{
    volatile int ecr;

    /* Enable service interrupt & enable DMA */
    ecr = plp->ecpregs->sregs.pp_ecr;
    plp->ecpregs->sregs.pp_ecr = ((ecr | ECR_DMA_EN) & ~ECR_SVCINTB);
}

#endif

#ifdef INPUT
/* Scenario - tried to get a new rbuf. None available. So started a timeout.
 * This is the timeout for that
 */
void
plptimeout1(ecp_t *plp)
{
    plpintr_read(plp, 0);
}
#endif /* INPUT */

void
plp_dma_reset(ecp_t *plp)
{
    plp->ecpregs->ppcr |= PPCR_DMA_RST;
    while ((plp->ecpregs->sio_cr & SIO_CR_ARB_DIAG_PP) &&
			(!(plp->ecpregs->sio_cr & SIO_CR_ARB_DIAG_IDLE))) {
	DELAY(1);
    }
    plp->ecpregs->ppcr &= ~PPCR_DMA_RST;
}

/* Stop the current dma transfer, disable any timeouts */
/*ARGSUSED*/
void
plp_stop_dma(ecp_t *plp, int flag)
{
#ifdef INPUT
    int bc;
#endif /* INPUT */
    volatile int ppcr;

#ifdef INPUT
    if (flag & B_READ) {

	/* Update byte count in the read buffer */
	bc = plp->ecpregs->ppcr_a;
	bc = (bc & PPCR_COUNT_MASK);
	if (plp->rdl != NULL)
	    ((memd_t *) PHYS_TO_K0(plp->rdl))->memd_bc = NBPRD - bc;

    }
#endif /* INPUT */

    /* Clear DMA_EN to halt DMA */
    plp->ecpregs->ppcr &= ~(PPCR_DMA_EN | PPCR_DIR_FROM_PP);
    /* Clean up DMA state machines */
    plp_dma_reset(plp);

    /* Clear DMA contexts */
    LOCK(&plp->ecpmutex);
    CLEAR_CONTEXT(plp->contexta);
    CLEAR_CONTEXT(plp->contextb);
    UNLOCK(&plp->ecpmutex);

    if (plp->wtimeoutid) {
	untimeout(plp->wtimeoutid);
	plp->wtimeoutid = 0;
    }

    if (plp->rtimeoutid) {
	untimeout(plp->rtimeoutid);
	plp->rtimeoutid = 0;
    }
}


/* IEEE P1284 Peripheral Auxilliary Routines */

/* On successful negotiation returns the phase.
 * Else returns 0.
 */
static int
plp_negotiate(ecp_t *plp, char extreq)
{
    int i;
    volatile unsigned char dcr;
    unsigned char dsr;

    /* Check phase */

    /* host places the Extensibility Request Value on data bus */
    plp->ecpregs->sregs.pp_data = extreq;

    /* host requests IEEE P1284 support:
     * nSelectIn(H), nAutoFd(L)
     */
    dcr = plp->ecpregs->sregs.pp_dcr;
    plp->ecpregs->sregs.pp_dcr = ((dcr | DCR_NAFD) & ~DCR_NSLCTIN);

    /* peripheral responds IEEE P1284 support:
     * Select(H), PError(H), nFault(H), nAck(L)
     * May need to wait 35ms max
     */
    for (i = 0; (PCI_INB(&plp->ecpregs->sregs.pp_dsr) & (DSR_NACK|DSR_PERROR|DSR_SELECT|
                   DSR_NFAULT)) != (DSR_PERROR|DSR_SELECT|DSR_NFAULT);) {
       if (++i > 3500)
	 goto termination_phase;
       DELAY(10);
    }

    /* host strobes Extensibility Request:
     * nStrobe(L)
     */
    plp->ecpregs->sregs.pp_dcr |= DCR_NSTB;

    /* wait min nStrobe pulse width 0.5us */
    DELAY(1);	/* 1  us delay */

    /* host acks the IEEE P1284 support request to peripheral:
     * nStrobe(H), nAutoFd(H)
     */
    plp->ecpregs->sregs.pp_dcr &= ~(DCR_NSTB | DCR_NAFD);   

    /* peripheral responds extreq support:
     * ECP: Select(H), PError(L), Busy(L)
     */
    for (i = 0; !(dsr = PCI_INB(&plp->ecpregs->sregs.pp_dsr)) ||
          (!(extreq == EXT_REQ_ECP &&
            (dsr & (DSR_BUSY | DSR_PERROR | DSR_SELECT)) == (DSR_BUSY | DSR_SELECT)))
            ; ) {
        if (++i > 500)
          goto termination_phase;
        DELAY(10);
    }

    /* peripheral indicates 4 status signals available:
     * nAck(H)
     */
    for (i = 0; !(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_NACK); ) {
       if (++i > 500)
	 goto termination_phase;
       DELAY(10);
    }

    /* enter setup phase */

    /* host acks successful negotiation
     * nAutoFd(L)
     */
    plp->ecpregs->sregs.pp_dcr |= DCR_NAFD;

    /* peripheral acks ECP mode operation:
     * PError(H), Busy(L), nFault(L)
     */
    if (extreq == EXT_REQ_ECP) {
      for (i = 0; !(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_PERROR); ) {
         if (++i > 500)
	   goto termination_phase;
         DELAY(10);
      }
      /* host sets data xfer
       * nAutoFd(H)
       */
      plp->ecpregs->sregs.pp_dcr &= ~DCR_NAFD;
      SET_PHASE(plp, ECP_FWD_IDLE_PHASE);
    }

    /* success */
    return (plp->phase);

    /* fail */
termination_phase :
    plp_terminate(plp, extreq);
    return (0);
}

static void
plp_terminate(ecp_t *plp, char extreq)
{
    volatile unsigned char dcr;

    if (extreq == EXT_REQ_ECP || plp->phase == ECP_FWD_IDLE_PHASE) {

      /* host: terminate IEEE P1284 support request by
       *       assert: Actibe(nSelectIn):L
       *       deassert: HostBusy(nAutoFd):H
       */
      dcr = plp->ecpregs->sregs.pp_dcr;
      plp->ecpregs->sregs.pp_dcr = ((dcr | DCR_NSLCTIN) & ~DCR_NAFD);

      /* device: places data bus in a high impedance state and
       *      deasserts: PeriphAck(Busy):H, nDataAvail(nFault):H
      if (((PCI_INB(&plp->ecpregs->pp_dsr) & DSR_BUSY) ||
             !(PCI_INB(&plp->ecpregs->pp_dsr) & FSR_NFAULT))
       */

      /* device: acknowledges the request by
       *      assert: PeriphClk(nAck):L
       *      set Xflag(Select) to its opposite sense
      if (PCI_INB(&plp->ecpregs->pp_dsr) & DSR_NACK)
       */

      /* host: handshakes by
       *      deassert: HostBusy(nAutoFd):L
       */
      plp->ecpregs->sregs.pp_dcr |= DCR_NAFD;

      /* device: set Xflag(Select), nReverseReq(nInit), AckDataReq(PError),
       *       and nDataAvail(nFault) their to Compatiblity mode values.
       */
      /* device: completes handshake by
       *      deassert: PeriphClk(nAck):H
      if (!(PCI_INB(&plp->ecpregs->pp_dsr) & DSR_NACK))
       */

      /* host: completes handshake by
       *    deassert: HostBusy(nAutoFd):H
       */
      plp->ecpregs->sregs.pp_dcr &= ~DCR_NAFD;

      /* device: updates PeriphAck(Busy) to its Compatiblity mode values */
    }

    SET_PHASE(plp, COMP_FWD_IDLE_PHASE);
}

/*REFERENCED*/
static int
plp_fwdtorev(ecp_t *plp)
{
    if (plp->phase != ECP_FWD_IDLE_PHASE)
      return (0);

    if (plp_lock(&plp->dlock)) {
      return 0;
    } else {
       /* forward to reverse phase:
        * host: places the reverse reguest
        *     assert:   HostAck(nAutoFd):L
        *               wait for setup time
        *     assert:   nReverseReq(nInit):L
        */
       plp->ecpregs->sregs.pp_dcr |= DCR_NAFD;
       DELAY(1);
       plp->ecpregs->sregs.pp_dcr &= ~DCR_NINIT;
 
       /* device: acknowledges the reversal
        *     deassert: nAckReverse(PError):L
        */
       if (PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_PERROR) {
	 plp_unlock(&plp->dlock);
         return (0);
       }
 
       SET_PHASE(plp, ECP_REV_IDLE_PHASE);
       plp_unlock(&plp->dlock);
    }
    
    return (plp->phase);
}

static int
plp_revtofwd(ecp_t *plp)
{
    if (plp->phase != ECP_REV_IDLE_PHASE && plp->phase != ECP_REV_BUSY_PHASE)
      return (0);

    if (plp_lock(&plp->dlock)) {
      return(0);
    } else {
       /* reverse to forward phase */
       /* host: initiates a bus reversal
        *     deassert: nReverseReq(nInit):H
        */
       plp->ecpregs->sregs.pp_dcr |= DCR_NINIT;

       /* device: terminates any ongoing transfer, tri-states the data bus
        *     deassert:   PeriphClk(nAck):H
        */
       if (!(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_NACK)) {
	 plp_unlock(&plp->dlock);
         return (0);
       }

       DELAY (1);

       /* device: acknowledges the bus has been relinquished
        *     assert:   nAckReverse(PError):H
        */
       if (!(PCI_INB(&plp->ecpregs->sregs.pp_dsr) & DSR_PERROR)) {
         plp_unlock(&plp->dlock);
         return (0);
       }

       SET_PHASE(plp, ECP_FWD_IDLE_PHASE);
       plp_unlock(&plp->dlock);
    }

    return (plp->phase);
}

/*REFERENCED*/
static void
plp_phase_abort(ecp_t *plp)
{
    /* host: requests IEEE P1284 immediate termination by
     *     assert: Active(nSelectIn):L
     */
    plp->ecpregs->sregs.pp_dcr |= DCR_NSLCTIN;
}

#endif	/* IP27 || IP30 */
