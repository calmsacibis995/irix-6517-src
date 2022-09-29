/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1993, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "io/sample_xtalk.c: $Revision: 1.14 $"

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

#include <sys/xtalk/xwidget.h>

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

/*
 *    xsamp: a generic device driver for a generic crosstalk device.
 */

int                     xsamp_devflag = D_MP;


/* =====================================================================
 *            Device-Related Constants and Structures
 */

#define	XSAMP_WIDGET_PART_NUM	0x5555
#define	XSAMP_WIDGET_MFGR_NUM	0x555

/*
 *    All registers on the Sample XTalk Client
 *      device are 64 bits wide.
 */
typedef __uint64_t      xsamp_reg_t;

typedef volatile struct xsamp_regs_s *xsamp_regs_t;	/* device registers */
typedef struct xsamp_soft_s *xsamp_soft_t;	/* software state */

/*
 *    xsamp_regs: layout of device registers
 *
 *      As usual for crosstalk devices, offset zero
 *      starts with standard widget registers.
 *
 *      The xsamp device has chosen to place its
 *      device registers at offset 0x1000 in its
 *      crosstalk address space.
 */

#define	XSAMP_REGS_OFFSET		0x1000

struct xsamp_regs_s {
    xsamp_reg_t             xr_control;
    xsamp_reg_t             xr_status;

    xsamp_reg_t             xr_dma_ahi;
    xsamp_reg_t             xr_dma_vec;
};

struct xsamp_soft_s {
    vertex_hdl_t            xs_conn;	/* xtalk connection */
    vertex_hdl_t            xs_vhdl;	/* backpointer to device vertex */
    vertex_hdl_t            xs_blockv;	/* backpointer to block vertex */
    vertex_hdl_t            xs_charv;	/* backpointer to char vertex */
    widget_cfg_t           *xs_cfg;	/* cached ptr to my config regs */
    xsamp_regs_t            xs_regs;	/* cached ptr to my regs */

    unsigned                xs_sst;	/* soft status */

#define	XSAMP_SST_RX_READY	(0x0001)
#define	XSAMP_SST_TX_READY	(0x0002)
#define	XSAMP_SST_ERROR		(0x0004)

    xtalk_intr_t            xs_err_intr;	/* xtalk intr for widget errors */
    xtalk_intr_t            xs_dma_intr;	/* xtalk intr for dma interrupt */

    xtalk_dmamap_t          xs_ctl_dmamap;	/* control channel dma mapping resoures */
    xtalk_dmamap_t          xs_str_dmamap;	/* stream channel dma mapping resoures */

    struct pollhead        *xs_pollhead;	/* for poll() */

    int                     xs_blocks;	/* block device size in NBPSCTR blocks */
};

#define	xsamp_soft_set(v,i)	device_info_set((v),(void *)(i))
#define	xsamp_soft_get(v)	((xsamp_soft_t)device_info_get((v)))


/* =====================================================================
 *                    Function Table of Contents
 */

extern void             xsamp_init(void);
extern int		xsamp_reg(void);
extern int		xsamp_unreg(void);
extern int              xsamp_attach(vertex_hdl_t conn);

extern int              xsamp_open(dev_t * devp, int oflag, int otyp, cred_t * crp);
extern int              xsamp_close(dev_t dev, int oflag, int otyp, cred_t * crp);

extern int              xsamp_ioctl(dev_t dev, int cmd, void *arg,
				    int mode, cred_t * crp, int *rvalp);

extern int              xsamp_read(dev_t dev, uio_t * uiop, cred_t * crp);
extern int              xsamp_write(dev_t dev, uio_t * uiop, cred_t * crp);

extern int              xsamp_strategy(struct buf *bp);

extern int              xsamp_poll(dev_t dev, short events, int anyyet,
				short *reventsp, struct pollhead **phpp,
				unsigned int *genp);

extern int              xsamp_map(dev_t dev, vhandl_t * vt,
				  off_t off, size_t len, uint_t prot);
extern int              xsamp_unmap(dev_t dev, vhandl_t * vt);

static void             xsamp_err_intr(intr_arg_t arg);
static void             xsamp_dma_intr(intr_arg_t arg);
static int              xsamp_err_setfunc(xtalk_intr_t intr);
static int              xsamp_dma_setfunc(xtalk_intr_t intr);

extern int              xsamp_unload(void);
extern void             xsamp_halt(void);
extern int              xsamp_size(dev_t dev);
extern int              xsamp_print(dev_t, char *str);

/* =====================================================================
 *                    Driver Initialization
 */

/*
 *    xsamp_init: called once during system startup or
 *      when a loadable driver is loaded.
 */
void
xsamp_init(void)
{
    printf("xsamp_init()\n");
}

/*
 *    xsamp_reg: called once during system startup or
 *      when a loadable driver is loaded.
 *
 *    NOTE: a bus provider register routine should always be 
 *	called from _reg, rather than from _init. In the case
 *	of a loadable module, the devsw is not hooked up
 *	when the _init routines are called.
 *
 */
int
xsamp_reg(void)
{
    printf("xsamp_reg()\n");

    xwidget_driver_register(XSAMP_WIDGET_PART_NUM,
			    XSAMP_WIDGET_MFGR_NUM,
			    "xsamp_",
			    0);

    return 0;
}

/*
 *    xsamp_unreg: called when a loadable driver is unloaded.
 */
int
xsamp_unreg(void)
{
    xwidget_driver_unregister("xsamp_");
    return 0;
}

/*
 *    xsamp_attach: called by the xtalk infrastructure
 *      once for each vertex representing a crosstalk
 *      widget.
 *
 *      In large configurations, it is possible for a
 *      huge number of CPUs to enter this routine all at
 *      nearly the same time, for different specific
 *      instances of the device. Attempting to give your
 *      devices sequence numbers based on the order they
 *      are found in the system is not only futile but
 *      may be dangerous as the order may differ from
 *      run to run.
 */
int
xsamp_attach(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl, blockv, charv;
    widget_cfg_t           *cfg;
    xsamp_regs_t            regs;
    xsamp_soft_t            soft;

    printf("xsamp_attach()\n");

    hwgraph_device_add(conn, "xsamp", "xsamp_", &vhdl, &blockv, &charv);

    /*
     * Allocate a place to put per-device
     * information for this vertex. Then
     * associate it with the vertex in the
     * most efficient manner.
     */
    NEW(soft);
    ASSERT(soft != NULL);
    xsamp_soft_set(vhdl, soft);
    xsamp_soft_set(blockv, soft);
    xsamp_soft_set(charv, soft);

    soft->xs_conn = conn;
    soft->xs_vhdl = vhdl;
    soft->xs_blockv = blockv;
    soft->xs_charv = charv;


    /*
     * Find our WIDGET CONFIG registers.
     * The "widget_cfg_t" structure defined in
     * <sys/xtalk/xwidget.h> maps accurately to the
     * standard registers expected to be found
     * starting at the base of the widget's address
     * space, for all crosstalk widgets.
     */

    cfg = (widget_cfg_t *) xtalk_piotrans_addr
	(conn, 0,		/* device and (override) dev_info */
	 0, sizeof(*cfg),	/* xtalk base and size */
	 0);			/* flag word */

    soft->xs_cfg = cfg;		/* save for later */

    printf("xsamp_attach: I can see my cfg regs at 0x%x\n", cfg);

    /*
     * Get a pointer to our DEVICE registers
     */

    regs = (xsamp_regs_t) xtalk_piotrans_addr
	(conn, 0,		/* device and (override) dev_info */
	 XSAMP_REGS_OFFSET, sizeof(*regs),	/* xtalk base and size */
	 0);			/* flag word */

    soft->xs_regs = regs;	/* save for later */

    printf("xsamp_attach: I can see my device regs at 0x%x\n", regs);

    /*
     * Set up our "widget error" interrupt.
     * Note the manipulation of dev_desc to get
     * proper priority for the error; we may shift
     * to passing this information as a flag to
     * xtlk_inter_alloc. Send feedback on this issue to
     * limes@sgi.com and len@sgi.com if this matters
     * a great deal to you (but we promise that this
     * code will keep working ...)
     */

    {
	device_desc_t           error_intr_desc;

	error_intr_desc = device_desc_dup(vhdl);
	device_desc_flags_set(error_intr_desc,
			      device_desc_flags_get(error_intr_desc) |
			      D_INTR_ISERR);

	soft->xs_err_intr = xtalk_intr_alloc(conn, error_intr_desc, vhdl);
	xtalk_intr_connect(soft->xs_err_intr,
			   xsamp_err_intr, soft,
			   xsamp_err_setfunc, soft,
			   (void *) 0);

	device_desc_free(error_intr_desc);
    }

    /*
     * Set up our "dma complete" interrupt.
     * No overrides, so we pass "0" for dev_desc.
     * This is the normal way to set up interrupts.
     */

    soft->xs_dma_intr = xtalk_intr_alloc(conn, 0, vhdl);
    xtalk_intr_connect(soft->xs_dma_intr,
		       xsamp_dma_intr, soft,
		       xsamp_dma_setfunc, soft,
		       (void *) 0);

    /*
     * For xtalk clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->xs_pollhead = phalloc(0);

    return 0;			/* attach successful */
}

/* =====================================================================
 *            DRIVER OPEN/CLOSE
 */

/*
 *    xsamp_open: called when a device special file is
 *      opened or when a block device is mounted.
 */

/* ARGSUSED */
int
xsamp_open(dev_t * devp, int oflag, int otyp, cred_t * crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(*devp);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);
    xsamp_regs_t            regs = soft->xs_regs;

    printf("xsamp_open() regs=%x\n", regs);

    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by xsamp_size.
     */

    /*
     * USER ABI (64-bit): chances are, you are being
     * compiled for use in a 64-bit IRIX kernel; if
     * you use the _ioctl or _poll entry points, now
     * would be a good time to test and save the
     * user process' model so you know how to
     * interpret the user ioctl and poll requests.
     */

    return 0;
}

/*
 *    xsamp_close: called when a device special file
 *      is closed by a process and no other processes
 *      still have it open ("last close").
 */

/* ARGSUSED */
int
xsamp_close(dev_t dev, int oflag, int otyp, cred_t * crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);
    xsamp_regs_t            regs = soft->xs_regs;

    printf("xsamp_close() regs=%x\n", regs);
    return 0;
}

/* =====================================================================
 *            CONTROL ENTRY POINT
 */

/*
 *    xsamp_ioctl: a user has made an ioctl request
 *      for an open character device.
 *
 *      cmd and arg are as specified by the user; arg is
 *      probably a pointer to something in the user's
 *      address space, so you need to use copyin() to
 *      read through it and copyout() to write through it.
 */

/* ARGSUSED */
int
xsamp_ioctl(dev_t dev, int cmd, void *arg,
	    int mode, cred_t * crp, int *rvalp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);
    xsamp_regs_t            regs = soft->xs_regs;

    printf("xsamp_ioctl() regs=%x\n", regs);

    *rvalp = -1;
    return ENOTTY;		/* TeleType® is a registered trademark. */
}

/* =====================================================================
 *            DATA TRANSFER ENTRY POINTS
 *
 *      Since I'm trying to provide an example for both
 *      character and block devices, I'm routing read
 *      and write back through strategy as described in
 *      the IRIX Device Driver Programming Guide.
 *
 *      This limits our character driver to reading and
 *      writing in multiples of the standard sector
 *      length.
 */

/* ARGSUSED */
int
xsamp_read(dev_t dev, uio_t * uiop, cred_t * crp)
{
    return physiock(xsamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_READ,	/* direction flag for buf_t */
		    xsamp_size(dev),
		    uiop);
}

/* ARGSUSED */
int
xsamp_write(dev_t dev, uio_t * uiop, cred_t * crp)
{
    return physiock(xsamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_WRITE,	/* direction flag for buf_t */
		    xsamp_size(dev),
		    uiop);
}

/* ARGSUSED1 */
int
xsamp_strategy(struct buf *bp)
{
    /*
     * I think Len has some code that I can
     * drop into here.
     */
    return 0;
}

/* =====================================================================
 *            POLL ENTRY POINT
 */

int
xsamp_poll(dev_t dev, short events, int anyyet,
	   short *reventsp, struct pollhead **phpp, unsigned int *genp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);
    xsamp_regs_t            regs = soft->xs_regs;

    short                   happened = 0;
    unsigned int            gen;

    printf("xsamp_poll() regs=%x\n", regs);

    /*
     * Need to snapshot the pollhead generation number before we check the
     * device state.  In many drivers a lock is used to interlock the
     * ``high'' and ``low'' portions of the driver.  In those cases we
     * can wait to do this snapshot till we're in the critical region.
     * Sanpshotting it early isn't a problem since that merely makes the
     * snapshotted generation number a more conservative estimate of
     * what ``generation'' of the pollhead our event state report indicates.
     */
    gen = POLLGEN(soft->xs_pollhead);

    if (events & (POLLIN | POLLRDNORM))
	if (soft->xs_sst & XSAMP_SST_RX_READY)
	    happened |= POLLIN | POLLRDNORM;

    if (events & POLLOUT)
	if (soft->xs_sst & XSAMP_SST_TX_READY)
	    happened |= POLLOUT;

    if (soft->xs_sst & XSAMP_SST_ERROR)
	happened |= POLLERR;

    *reventsp = happened;

    if (!happened && anyyet) {
	*phpp = soft->xs_pollhead;
	*genp = gen;
    }

    return 0;
}

/* =====================================================================
 *            MEMORY MAP ENTRY POINTS
 */

/* ARGSUSED */
int
xsamp_map(dev_t dev, vhandl_t * vt,
	  off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);
    vertex_hdl_t            conn = soft->xs_conn;
    xsamp_regs_t            regs = soft->xs_regs;
    caddr_t                 kaddr;

    printf("xsamp_map() regs=%x\n", regs);

    /*
     * We're gonna allow anyone to mmap our
     * entire widget address space, if they want.
     */

    kaddr = (caddr_t) xtalk_piotrans_addr(conn, 0, off, len, 0);
    if (kaddr == NULL)
	return EINVAL;

    len = ctob(btoc(len));	/* make len page aligned */

    v_mapphys(vt, kaddr, len);

    return 0;
}

/* ARGSUSED2 */
int
xsamp_unmap(dev_t dev, vhandl_t * vt)
{
    /*
     * If you had to do an xtalk_piomap_alloc inside
     * xsamp_map, this is the place to chase down
     * the xtalk_piomap_t for xtalk_piomap_free.
     */
    return (0);
}

/* =====================================================================
 *            INTERRUPT ENTRY POINTS
 *
 *      IRIX will look for (and not find) xsamp_intr;
 *      generally, the interrupt requirements for
 *      crosstalk widgets are more complex (besides,
 *      someone needs to set up the hardware).
 *
 *      xtalk infrastructure clients explicitly notify
 *      the infrastructure of their interrupt needs as
 *      each device instance is attached.
 */

static void
xsamp_err_intr(intr_arg_t arg)
{
    xsamp_soft_t            soft = (xsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->xs_vhdl;
    xsamp_regs_t            regs = soft->xs_regs;

    cmn_err(CE_WARN, "%v error intr; regs at 0x%x\n", vhdl, regs);
}

static void
xsamp_dma_intr(intr_arg_t arg)
{
    xsamp_soft_t            soft = (xsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->xs_vhdl;
    xsamp_regs_t            regs = soft->xs_regs;

    cmn_err(CE_WARN, "%v dma done; regs at 0x%x\n", vhdl, regs);

    /*
     * for each buf our hardware has processed,
     *      set buf->b_resid,
     *      call xtalk_dmamap_done,
     *      call bioerror() or biodone().
     *
     * XXX- would it be better for buf->b_iodone
     * to be used to get to xtalk_dmamap_done?
     */

    /*
     * may want to call pollwakeup.
     */
}

/* =====================================================================
 *            INTERRUPT DESTINATION SELECTION
 *
 *      It may be necessary to migrate xtalk widget
 *      interrupts around the system; without
 *      provocation from the driver, the infrastructure
 *      could call our setfunc entry points, presented
 *      to it at attach time.
 */

/* =====================================================================
 *            Interrupt Management
 */

static int
xsamp_err_setfunc(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);
    xsamp_soft_t            soft = (xsamp_soft_t) xtalk_intr_sfarg_get(intr);
    widget_cfg_t           *cfg = soft->xs_cfg;

    cfg->w_intdest_upper_addr =
	((0xFF000000 & (vect << 24)) |
	 (0x000F0000 & (targ << 16)) |
	 (0x0000FFFF & (addr >> 32)));
    cfg->w_intdest_lower_addr = 0xFFFFFFFF & addr;

    printf("xsamp: error ints now going to widget 0x%X, offset 0x%X, vector 0x%X\n",
	   targ, addr, vect);

    return 0;
}

static int
xsamp_dma_setfunc(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);
    xsamp_soft_t            soft = (xsamp_soft_t) xtalk_intr_sfarg_get(intr);
    xsamp_regs_t            regs = soft->xs_regs;

    /*
     * The xsamp device sends DMADONE interrupts to the same
     * target as ERROR interrupts, and to mostly the same
     * address, but uses a different vector and allows
     * some address bits to be different.
     *
     * We should probably ASSERT that the xtalk widget
     * target number and the low 32 bits of the address
     * match.
     */
    regs->xr_dma_vec = vect;
    regs->xr_dma_ahi = 0x0000FFFF & (addr >> 32);

    printf("xsamp: dma done ints now going to widget 0x%X, offset 0x%X, vector 0x%X\n",
	   targ, addr, vect);

    return 0;
}

/* =====================================================================
 *            SUPPORT ENTRY POINTS
 */

/*
 *    xsamp_unload: tear down EVERYTHING so the driver
 *      can be unloaded. Return nonzero if you can't.
 */

int
xsamp_unload(void)
{
    /*
     * I'm not sure I *can* ... yet.
     */
    return -1;			/* zero would be "ok, did it." */
}

/*
 *    xsamp_halt: called during orderly system
 *      shutdown; no other device driver call will be
 *      made after this one.
 */

void
xsamp_halt(void)
{
    printf("xsamp_halt()\n");
}

/*
 *    xsamp_size: return the size of the device in
 *      "sector" units (multiples of NBPSCTR).
 */
int
xsamp_size(dev_t dev)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    xsamp_soft_t            soft = xsamp_soft_get(vhdl);

    return soft->xs_blocks;
}

/*
 *    xsamp_print: used by the kernel to report an
 *      error detected on a block device.
 */
int
xsamp_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
