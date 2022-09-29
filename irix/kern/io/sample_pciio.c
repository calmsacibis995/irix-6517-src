/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "io/sample_pciio.c: $Revision: 1.16 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/poll.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/atomic_ops.h>

#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>

#define NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

/*
 *    psamp: a generic device driver for a generic PCI device.
 */

int			psamp_devflag = D_MP;

/* number of "psamp" devices open */
int			psamp_inuse = 0;

/* =====================================================================
 *	    Device-Related Constants and Structures
 */

#define PSAMP_VENDOR_ID_NUM	0x5555
#define PSAMP_DEVICE_ID_NUM	0x555

/*
 *    All registers on the Sample PCIIO Client
 *	device are 32 bits wide.
 */
typedef __uint32_t	psamp_reg_t;

typedef volatile struct psamp_regs_s *psamp_regs_t;	/* device registers */
typedef struct psamp_soft_s *psamp_soft_t;	/* software state */

/*
 *    psamp_regs: layout of device registers
 *
 *	Our device config registers are, of course, at
 *	the base of our assigned CFG space.
 *
 *	Our sample device registers are in the PCI area
 *	decoded by the device's first BASE_ADDR window.
 */

struct psamp_regs_s {
    psamp_reg_t		    pr_control;
    psamp_reg_t		    pr_status;
};

struct psamp_soft_s {
    vertex_hdl_t	    ps_conn;	/* connection for pci services */
    vertex_hdl_t	    ps_vhdl;	/* backpointer to device vertex */
    vertex_hdl_t	    ps_blockv;	/* backpointer to block vertex */
    vertex_hdl_t	    ps_charv;	/* backpointer to char vertex */
    volatile uchar_t	   *ps_cfg;	/* cached ptr to my config regs */
    psamp_regs_t	    ps_regs;	/* cached ptr to my regs */

    pciio_piomap_t	    ps_cmap;	/* piomap (if any) for ps_cfg */
    pciio_piomap_t	    ps_rmap;	/* piomap (if any) for ps_regs */

    unsigned		    ps_sst;	/* driver "software state" */

#define PSAMP_SST_RX_READY	(0x0001)
#define PSAMP_SST_TX_READY	(0x0002)
#define PSAMP_SST_ERROR		(0x0004)
#define PSAMP_SST_INUSE		(0x8000)

    pciio_intr_t	    ps_intr;	/* pciio intr for INTA and INTB */

    pciio_dmamap_t	    ps_ctl_dmamap;	/* control channel dma mapping resoures */
    pciio_dmamap_t	    ps_str_dmamap;	/* stream channel dma mapping resoures */

    struct pollhead	   *ps_pollhead;	/* for poll() */

    int			    ps_blocks;	/* block device size in NBPSCTR blocks */
};

#define psamp_soft_set(v,i)	device_info_set((v),(void *)(i))
#define psamp_soft_get(v)	((psamp_soft_t)device_info_get((v)))

/* =====================================================================
 *	    FUNCTION TABLE OF CONTENTS
 */

void			psamp_init(void);
int			psamp_unload(void);

int			psamp_reg(void);
int                     psamp_unreg(void);

int			psamp_attach(vertex_hdl_t conn);
int			psamp_detach(vertex_hdl_t conn);

static pciio_iter_f	psamp_reloadme;
static pciio_iter_f	psamp_unloadme;

int			psamp_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int			psamp_close(dev_t dev, int oflag, int otyp, cred_t *crp);

int			psamp_ioctl(dev_t dev, int cmd, void *arg,
				    int mode, cred_t *crp, int *rvalp);

int			psamp_read(dev_t dev, uio_t * uiop, cred_t *crp);
int			psamp_write(dev_t dev, uio_t * uiop, cred_t *crp);

int			psamp_strategy(struct buf *bp);

int			psamp_poll(dev_t dev, short events, int anyyet,
				short *reventsp, struct pollhead **phpp,
				unsigned int *genp);

int			psamp_map(dev_t dev, vhandl_t *vt,
				  off_t off, size_t len, uint_t prot);
int			psamp_unmap(dev_t dev, vhandl_t *vt);

void			psamp_dma_intr(intr_arg_t arg);

static error_handler_f	psamp_error_handler;

void			psamp_halt(void);
int			psamp_size(dev_t dev);
int			psamp_print(dev_t dev, char *str);

/* =====================================================================
 *		    Driver Initialization
 */

/*
 *    psamp_init: called once during system startup or
 *	when a loadable driver is loaded.
 */
void
psamp_init(void)
{
    printf("psamp_init()\n");

    /*
     * if we are already registered, note
     * that this is a "reload" and reconnect
     * all the places we attached.
     */
    pciio_iterate("psamp_", psamp_reloadme);
}

/*
 *    psamp_unload: if no "psamp" is open, put us to bed
 *	and let the driver text get unloaded.
 */
int
psamp_unload(void)
{
    if (psamp_inuse)
	return EBUSY;

    pciio_iterate("psamp_", psamp_unloadme);

    return 0;
}

/*
 *    psamp_reg: called once during system startup or
 *	when a loadable driver is loaded.
 *
 *    NOTE: a bus provider register routine should always be
 *	called from _reg, rather than from _init. In the case
 *	of a loadable module, the devsw is not hooked up
 *	when the _init routines are called.
 *
 */
int
psamp_reg(void)
{
    printf("psamp_reg()\n");

    pciio_driver_register(PSAMP_VENDOR_ID_NUM,
			  PSAMP_DEVICE_ID_NUM,
			  "psamp_",
			  0);

    return 0;
}

/*
 *    psamp_unreg: called when a loadable driver is unloaded.
 */
int
psamp_unreg(void)
{
    pciio_driver_unregister("psamp_");
    return 0;
}

/*
 *    psamp_attach: called by the pciio infrastructure
 *	once for each vertex representing a crosstalk
 *	widget.
 *
 *	In large configurations, it is possible for a
 *	huge number of CPUs to enter this routine all at
 *	nearly the same time, for different specific
 *	instances of the device. Attempting to give your
 *	devices sequence numbers based on the order they
 *	are found in the system is not only futile but
 *	may be dangerous as the order may differ from
 *	run to run.
 */
int
psamp_attach(vertex_hdl_t conn)
{
    vertex_hdl_t	    vhdl, blockv, charv;
    volatile uchar_t	   *cfg;
    psamp_regs_t	    regs;
    psamp_soft_t	    soft;
    pciio_piomap_t	    cmap = 0;
    pciio_piomap_t	    rmap = 0;

    printf("psamp_attach()\n");

    hwgraph_device_add(conn, "psamp", "psamp_", &vhdl, &blockv, &charv);

    /*
     * Allocate a place to put per-device
     * information for this vertex. Then
     * associate it with the vertex in the
     * most efficient manner.
     */
    NEW(soft);
    ASSERT(soft != NULL);
    psamp_soft_set(vhdl, soft);
    psamp_soft_set(blockv, soft);
    psamp_soft_set(charv, soft);

    soft->ps_conn = conn;
    soft->ps_vhdl = vhdl;
    soft->ps_blockv = blockv;
    soft->ps_charv = charv;

    /*
     * Find our PCI CONFIG registers.
     */

    cfg = (volatile uchar_t *) pciio_pio_addr
	(conn, 0,		/* device and (override) dev_info */
	 PCIIO_SPACE_CFG,	/* select configuration address space */
	 0,			/* from the start of space, */
	 PCI_CFG_VEND_SPECIFIC, /* ... up to the vendor specific stuff. */
	 &cmap,			/* in case we needed a piomap */
	 0);			/* flag word */

    soft->ps_cfg = cfg;		/* save for later */
    soft->ps_cmap = cmap;

    printf("psamp_attach: I can see my CFG regs at 0x%x\n", cfg);

    /*
     * Get a pointer to our DEVICE registers
     */

    regs = (psamp_regs_t) pciio_pio_addr
	(conn, 0,		/* device and (override) dev_info */
	 PCIIO_SPACE_WIN(0),	/* in my primary decode window, */
	 0, sizeof(*regs),	/* base and size */
	 &rmap,			/* in case we needed a piomap */
	 0);			/* flag word */

    soft->ps_regs = regs;	/* save for later */
    soft->ps_rmap = cmap;

    printf("psamp_attach: I can see my device regs at 0x%x\n", regs);

    /*
     * Set up our interrupt.
     * We might interrupt on INTA or INTB,
     * but route 'em both to the same function.
     */
    soft->ps_intr = pciio_intr_alloc
	(conn, 0,
	 PCIIO_INTR_LINE_A |
	 PCIIO_INTR_LINE_B,
	 vhdl);

    pciio_intr_connect(soft->ps_intr,
		       psamp_dma_intr, soft,
		       (void *) 0);

    /*
     * set up our error handler.
     */
    pciio_error_register(conn, psamp_error_handler, soft);

    /*
     * For pciio clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->ps_pollhead = phalloc(0);

    return 0;			/* attach successsful */
}
/*
 *    psamp_detach: called by the pciio infrastructure
 *	once for each vertex representing a crosstalk
 *	widget when unregistering the driver.
 *
 *	In large configurations, it is possible for a
 *	huge number of CPUs to enter this routine all at
 *	nearly the same time, for different specific
 *	instances of the device. Attempting to give your
 *	devices sequence numbers based on the order they
 *	are found in the system is not only futile but
 *	may be dangerous as the order may differ from
 *	run to run.
 */
int
psamp_detach(vertex_hdl_t conn)
{
    vertex_hdl_t	    vhdl, blockv, charv;
    psamp_soft_t	    soft;

    printf("psamp_detach()\n");

    if (GRAPH_SUCCESS !=
	hwgraph_traverse(conn, "psamp", &vhdl))
	return -1;

    soft = psamp_soft_get(vhdl);

    pciio_error_register(conn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
    pciio_intr_free(soft->ps_intr);

    phfree(soft->ps_pollhead);

    if (soft->ps_ctl_dmamap)
	pciio_dmamap_free(soft->ps_ctl_dmamap);

    if (soft->ps_str_dmamap)
	pciio_dmamap_free(soft->ps_str_dmamap);

    if (soft->ps_cmap)
	pciio_piomap_free(soft->ps_cmap);

    if (soft->ps_rmap)
	pciio_piomap_free(soft->ps_rmap);

    hwgraph_edge_remove(conn, "psamp", &vhdl);

    /* we really need "hwgraph_dev_remove" ... */

    if (GRAPH_SUCCESS ==
	hwgraph_edge_remove(vhdl, EDGE_LBL_BLOCK, &blockv)) {
	psamp_soft_set(blockv, 0);
	hwgraph_vertex_destroy(blockv);
    }
    if (GRAPH_SUCCESS ==
	hwgraph_edge_remove(vhdl, EDGE_LBL_CHAR, &charv)) {
	psamp_soft_set(charv, 0);
	hwgraph_vertex_destroy(charv);
    }
    psamp_soft_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);

    DEL(soft);

    return 0;
}

/*
 *    psamp_reloadme: utility function used indirectly
 *	by psamp_init, via pciio_iterate, to "reconnect"
 *	each connection point when the driver has been
 * 	reloaded.
 */
static void
psamp_reloadme(vertex_hdl_t conn)
{
    vertex_hdl_t	    vhdl;
    psamp_soft_t	    soft;

    if (GRAPH_SUCCESS !=
	hwgraph_traverse(conn, "psamp", &vhdl))
	return;

    soft = psamp_soft_get(vhdl);

    /*
     * Reconnect our error and interrupt handlers
     */
    pciio_error_register(conn, psamp_error_handler, soft);
    pciio_intr_connect(soft->ps_intr, psamp_dma_intr, soft, 0);
}


/*
 *    psamp_reloadme: utility function used indirectly
 *	by psamp_unload, via pciio_iterate, to "disconnect"
 *	each connection point before the driver becomes
 * 	unloaded.
 */
static void
psamp_unloadme(vertex_hdl_t pconn)
{
    vertex_hdl_t	    vhdl;
    psamp_soft_t	    soft;

    if (GRAPH_SUCCESS !=
	hwgraph_traverse(pconn, "psamp", &vhdl))
	return;

    soft = psamp_soft_get(vhdl);

    /*
     * Disconnect our error and interrupt handlers
     */
    pciio_error_register(pconn, 0, 0);
    pciio_intr_disconnect(soft->ps_intr);
}

/* =====================================================================
 *	    DRIVER OPEN/CLOSE
 */

/*
 *    psamp_open: called when a device special file is
 *	opened or when a block device is mounted.
 */

/* ARGSUSED */
int
psamp_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(*devp);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);
    psamp_regs_t	    regs = soft->ps_regs;

    printf("psamp_open() regs=%x\n", regs);

    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by psamp_size.
     */

    /*
     * USER ABI (64-bit): chances are, you are being
     * compiled for use in a 64-bit IRIX kernel; if
     * you use the _ioctl or _poll entry points, now
     * would be a good time to test and save the
     * user process' model so you know how to
     * interpret the user ioctl and poll requests.
     */

    if (!(PSAMP_SST_INUSE & atomicSetUint(&soft->ps_sst, PSAMP_SST_INUSE)))
	atomicAddInt(&psamp_inuse, 1);

    return 0;
}

/*
 *    psamp_close: called when a device special file
 *	is closed by a process and no other processes
 *	still have it open ("last close").
 */

/* ARGSUSED */
int
psamp_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);
    psamp_regs_t	    regs = soft->ps_regs;

    printf("psamp_close() regs=%x\n", regs);

    atomicClearUint(&soft->ps_sst, PSAMP_SST_INUSE);
    atomicAddInt(&psamp_inuse, -1);

    return 0;
}

/* =====================================================================
 *	    CONTROL ENTRY POINT
 */

/*
 *    psamp_ioctl: a user has made an ioctl request
 *	for an open character device.
 *
 *	cmd and arg are as specified by the user; arg is
 *	probably a pointer to something in the user's
 *	address space, so you need to use copyin() to
 *	read through it and copyout() to write through it.
 */

/* ARGSUSED */
int
psamp_ioctl(dev_t dev, int cmd, void *arg,
	    int mode, cred_t *crp, int *rvalp)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);
    psamp_regs_t	    regs = soft->ps_regs;

    printf("psamp_ioctl() regs=%x\n", regs);

    *rvalp = -1;
    return ENOTTY;		/* TeleType® is a registered trademark. */
}

/* =====================================================================
 *	    DATA TRANSFER ENTRY POINTS
 *
 *	Since I'm trying to provide an example for both
 *	character and block devices, I'm routing read
 *	and write back through strategy as described in
 *	the IRIX Device Driver Programming Guide.
 *
 *	This limits our character driver to reading and
 *	writing in multiples of the standard sector
 *	length.
 */

/* ARGSUSED */
int
psamp_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(psamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_READ,	/* direction flag for buf_t */
		    psamp_size(dev),
		    uiop);
}

/* ARGSUSED */
int
psamp_write(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return physiock(psamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_WRITE,	/* direction flag for buf_t */
		    psamp_size(dev),
		    uiop);
}

/* ARGSUSED */
int
psamp_strategy(struct buf *bp)
{
    /*
     * I think Len has some code that I can
     * modify and drop into here.
     */
    return 0;
}

/* =====================================================================
 *	    POLL ENTRY POINT
 */

int
psamp_poll(dev_t dev, short events, int anyyet,
	   short *reventsp, struct pollhead **phpp, unsigned int *genp)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);
    psamp_regs_t	    regs = soft->ps_regs;

    short		    happened = 0;
    unsigned int            gen;

    printf("psamp_poll() regs=%x\n", regs);

    /*
     * Need to snapshot the pollhead generation number before we check the
     * device state.  In many drivers a lock is used to interlock the
     * ``high'' and ``low'' portions of the driver.  In those cases we
     * can wait to do this snapshot till we're in the critical region.
     * Sanpshotting it early isn't a problem since that merely makes the
     * snapshotted generation number a more conservative estimate of
     * what ``generation'' of the pollhead our event state report indicates.
     */
    gen = POLLGEN(soft->ps_pollhead);

    if (events & (POLLIN | POLLRDNORM))
	if (soft->ps_sst & PSAMP_SST_RX_READY)
	    happened |= POLLIN | POLLRDNORM;

    if (events & POLLOUT)
	if (soft->ps_sst & PSAMP_SST_TX_READY)
	    happened |= POLLOUT;

    if (soft->ps_sst & PSAMP_SST_ERROR)
	happened |= POLLERR;

    *reventsp = happened;

    if (!happened && anyyet) {
	*phpp = soft->ps_pollhead;
	*genp = gen;
    }

    return 0;
}

/* =====================================================================
 *	    MEMORY MAP ENTRY POINTS
 */

/* ARGSUSED */
int
psamp_map(dev_t dev, vhandl_t *vt,
	  off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);
    vertex_hdl_t	    conn = soft->ps_conn;
    psamp_regs_t	    regs = soft->ps_regs;
    pciio_piomap_t	    amap = 0;
    caddr_t		    kaddr;

    printf("psamp_map() regs=%x\n", regs);

    /*
     * The stuff we want users to mmap
     * is in our second BASE_ADDR window,
     * just for fun.
     */

    kaddr = (caddr_t) pciio_pio_addr
	(conn, 0,
	 PCIIO_SPACE_WIN(1),
	 off, len, &amap, 0);

    if (kaddr == NULL)
	return EINVAL;

    /* XXX- must stash amap somewhere
     * so we can pciio_piomap_free it
     * when the mapping goes away.
     */

    v_mapphys(vt, kaddr, len);

    return 0;
}

/* ARGSUSED2 */
int
psamp_unmap(dev_t dev, vhandl_t *vt)
{
    /* XXX- need to find "amap" that we
     * used in psamp_map() above, and
     *	if (amap) pciio_piomap_free(amap);
     */
    return (0);
}

/* =====================================================================
 *	    INTERRUPT ENTRY POINTS
 *
 *	We avoid using the standard name, since our
 *	prototype has changed.
 */

void
psamp_dma_intr(intr_arg_t arg)
{
    psamp_soft_t	    soft = (psamp_soft_t) arg;
    vertex_hdl_t	    vhdl = soft->ps_vhdl;
    psamp_regs_t	    regs = soft->ps_regs;

    cmn_err(CE_CONT, "psamp %v: dma done, regs at 0x%X\n", vhdl, regs);

    /*
     * for each buf our hardware has processed,
     *	    set buf->b_resid,
     *	    call pciio_dmamap_done,
     *	    call bioerror() or biodone().
     *
     * XXX- would it be better for buf->b_iodone
     * to be used to get to pciio_dmamap_done?
     */

    /*
     * may want to call pollwakeup.
     */
}

/* =====================================================================
 *	    ERROR HANDLING ENTRY POINTS
 */
static int
psamp_error_handler(void *einfo,
		    int error_code,
		    ioerror_mode_t mode,
		    ioerror_t *ioerror)
{
    psamp_soft_t	    soft = (psamp_soft_t) einfo;
    vertex_hdl_t	    vhdl = soft->ps_vhdl;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: psamp_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif

    /*
     * XXX- there is probably a lot more to do
     * to recover from an error on a real device;
     * experts on this are encouraged to add common
     * things that need to be done into this function.
     */

    ioerror_dump("sample_pciio", error_code, mode, ioerror);

    return IOERROR_HANDLED;
}

/* =====================================================================
 *	    SUPPORT ENTRY POINTS
 */

/*
 *    psamp_halt: called during orderly system
 *	shutdown; no other device driver call will be
 *	made after this one.
 */

void
psamp_halt(void)
{
    printf("psamp_halt()\n");
}

/*
 *    psamp_size: return the size of the device in
 *	"sector" units (multiples of NBPSCTR).
 */
int
psamp_size(dev_t dev)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    psamp_soft_t	    soft = psamp_soft_get(vhdl);

    return soft->ps_blocks;
}

/*
 *    psamp_print: used by the kernel to report an
 *	error detected on a block device.
 */
int
psamp_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
