/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc           	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "io/sample_gioio.c: $Revision: 1.18 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <ksys/ddmap.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/ddi.h>

#include <sys/GIO/gioio.h>
#include <sys/GIO/giobr.h>

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

/*
 *    gsamp: a generic device driver for a generic GIO device.
 */

int                     gsamp_devflag = D_MP;

/* =====================================================================
 *            Device-Related Constants and Structures
 */
/*
 * choose 1 of 3 Examples to run the gsamp driver
 */
#define EXAMPLE_FOR_VIDEO		0
#define EXAMPLE_FOR_COMPRESSION		1
#define EXAMPLE_FOR_EVO			0

#if EXAMPLE_FOR_COMPRESSION
#define GSAMP_PRODUCT_ID_NUM	GIOIO_BUS_PROBE(0,0,GIOIO_CGI1_ID,0)

#elif EXAMPLE_FOR_VIDEO
#define GSAMP_PRODUCT_ID_NUM    GIOIO_BUS_PROBE(GIOIO_VGI1_ID,0,0,0)

#elif EXAMPLE_FOR_EVO
#define GSAMP_PRODUCT_ID_NUM	GIOIO_BUS_PROBE(GIOIO_VGI1_ID,GIOIO_VGI1_ID,GIOIO_EVO_ID,0)

#endif

#define	GSAMP_DEVICE_ID_NUM	0x0

/*
 *    All registers on the Sample GIOIO Client
 *      device are 32 bits wide.
 */
typedef __uint32_t      gsamp_reg_t;

#define GSAMP_REG_SPACE_SIZE		(4 << 20)	/* lets go for full 4 MB */

typedef volatile struct gsamp_regs_s *gsamp_regs_t;	/* device registers */
typedef struct gsamp_soft_s *gsamp_soft_t;	/* software state */

/*
 *    gsamp_regs: layout of device registers
 *
 *      Our device config registers are, of course, at
 *      the base of our assigned CFG space.
 *
 *      Our sample device registers are in the GIO area
 *      decoded by the device's first BASE_ADDR window.
 */

struct gsamp_regs_s {
    gsamp_reg_t             gr_control;
    gsamp_reg_t             gr_status;
};

struct gsamp_soft_s {
    vertex_hdl_t            gs_conn;	/* gio connection point */
    vertex_hdl_t            gs_vhdl;	/* backpointer to device vertex */
    vertex_hdl_t            gs_blockv;	/* backpointer to block vertex */
    vertex_hdl_t            gs_charv;	/* backpointer to char vertex */
    gsamp_regs_t            gs_regs;	/* ptr to my regs */

    unsigned                gs_sst;	/* driver "software state" */

#define	GSAMP_SST_RX_READY	(0x0001)
#define	GSAMP_SST_TX_READY	(0x0002)
#define	GSAMP_SST_ERROR		(0x0004)

    gioio_intr_t            gs_intr[8];		/* gioio intr */

    gioio_dmamap_t          gs_ctl_dmamap;	/* control channel dma mapping resoures */
    gioio_dmamap_t          gs_str_dmamap;	/* stream channel dma mapping resoures */

    struct pollhead        *gs_pollhead;	/* for poll() */

    int                     gs_blocks;	/* block device size in NBPSCTR blocks */
};

#define	gsamp_soft_set(v,i)	device_info_set((v),(void *)(i))
#define	gsamp_soft_get(v)	((gsamp_soft_t)device_info_get((v)))


/* =====================================================================
 *            FUNCTION TABLE OF CONTENTS
 */

extern void             gsamp_init(void);
extern int		gsamp_reg(void);
extern int		gsamp_unreg(void);
extern int              gsamp_attach(vertex_hdl_t vhdl);

extern int              gsamp_open(dev_t * devp, int oflag, int otyp, cred_t * crp);
extern int              gsamp_close(dev_t dev, int oflag, int otyp, cred_t * crp);

extern int              gsamp_ioctl(dev_t dev, int cmd, void *arg,
				    int mode, cred_t * crp, int *rvalp);

extern int              gsamp_read(dev_t dev, uio_t * uiop, cred_t * crp);
extern int              gsamp_write(dev_t dev, uio_t * uiop, cred_t * crp);

extern int              gsamp_strategy(struct buf *bp);

extern int              gsamp_poll(dev_t dev, short events, int anyyet,
				short *reventsp, struct pollhead **phpp,
				unsigned int *genp);

extern int              gsamp_map(dev_t dev, vhandl_t * vt,
				  off_t off, size_t len, uint_t prot);
extern int              gsamp_unmap(dev_t dev, vhandl_t * vt);

static void             gsamp_dma_intr0(intr_arg_t arg);
static void             gsamp_dma_intr1(intr_arg_t arg);
static void             gsamp_dma_intr2(intr_arg_t arg);
static void             gsamp_dma_intr3(intr_arg_t arg);
static void             gsamp_dma_intr4(intr_arg_t arg);
static void             gsamp_dma_intr5(intr_arg_t arg);
static void             gsamp_dma_intr6(intr_arg_t arg);
static void             gsamp_dma_intr7(intr_arg_t arg);

static error_handler_f  gsamp_error_handler;

extern int              gsamp_unload(void);
extern void             gsamp_halt(void);
extern int              gsamp_size(dev_t dev);
extern int              gsamp_print(dev_t dev, char *str);

static struct {
    intr_func_t             func;
} gsamp_dma_intrx[] = {

    gsamp_dma_intr0,
	gsamp_dma_intr1,
	gsamp_dma_intr2,
	gsamp_dma_intr3,
	gsamp_dma_intr4,
	gsamp_dma_intr5,
	gsamp_dma_intr6,
	gsamp_dma_intr7,
};

/* =====================================================================
 *                    Driver Initialization
 */

/*
 *    gsamp_init: called once during system startup or
 *      when a loadable driver is loaded.
 */
void
gsamp_init(void)
{
    printf("gsamp_init() product 0x%x devid 0x%x\n",
	   GSAMP_PRODUCT_ID_NUM, GSAMP_DEVICE_ID_NUM);
}

/*
 *    gsamp_reg: called once during system startup or
 *      when a loadable driver is loaded. Call bus
 *	provider register routine from here.
 *
 *    NOTE: a bus provider register routine should always be 
 *	called from _reg, rather than from _init. In the case
 *	of a loadable module, the devsw is not hooked up
 *	when the _init routines are called.
 *
 */
int
gsamp_reg(void)
{
    printf("gsamp_reg() product 0x%x devid 0x%x\n",
	GSAMP_PRODUCT_ID_NUM, GSAMP_DEVICE_ID_NUM);

    gioio_driver_register(GSAMP_PRODUCT_ID_NUM,
			  GSAMP_DEVICE_ID_NUM,
			  "gsamp_",
			  0);
    return 0;
}

/*
 *    gsamp_unreg: called when a loadable driver is unloaded.
 */
int
gsamp_unreg(void)
{
    gioio_driver_unregister("gsamp_");
    return 0;
}

/*
 *    gsamp_attach: called by the gioio infrastructure
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
gsamp_attach(vertex_hdl_t conn)
{
    vertex_hdl_t            blockv, charv, vhdl;
    gsamp_regs_t            regs;
    gsamp_soft_t            soft;
    int                     n;
    uint                   *ptr;
    gioio_info_t	    ginfo;

    hwgraph_device_add(conn, "gsamp", "gsamp_", &vhdl, &blockv, &charv);

    cmn_err(CE_DEBUG, "%v gsamp_attach", vhdl);

    /*
     * Allocate a place to put per-device
     * information for this vertex. Then
     * associate it with the block and char
     * vertexes.
     */
    NEW(soft);
    ASSERT(soft != NULL);
    gsamp_soft_set(blockv, soft);
    gsamp_soft_set(charv, soft);
    gsamp_soft_set(vhdl, soft);

    soft->gs_conn = conn;
    soft->gs_vhdl = vhdl;
    soft->gs_blockv = blockv;
    soft->gs_charv = charv;

#if EXAMPLE_FOR_COMPRESSION || EXAMPLE_FOR_VIDEO
    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (gsamp_regs_t) gioio_piotrans_addr
	(conn, 0,		/* device and (override) dev_info */

#if EXAMPLE_FOR_COMPRESSION
	 GIOIO_SPACE_GIO1X,	/* device in slot 1X space */
#else
	 GIOIO_SPACE_GFX,	/* device in slot 0 space */
#endif
	 0,			/* base relative offset */
#if EXAMPLE_FOR_COMPRESSION
	 GIOIO_GIO1X_PIO_MAP_SIZE,/*pio space size */
#else
	 GIOIO_GIO1_PIO_MAP_SIZE,/* pio space size */
#endif
	 0);			/* flag word */

    soft->gs_regs = regs;	/* save for later */
    ptr = (uint *) regs;


    printf("gsamp_attach: I can see my device regs at 0x%x\n", ptr);
    printf("              base[0] 0x%x [4] 0x%x [8] 0x%x\n",
	   (uint) ptr[0], (uint) ptr[1], (uint) ptr[2]);
#elif EXAMPLE_FOR_EVO
    /*
     * Get a pointer to our DEVICE registers
     */
    regs = (gsamp_regs_t) gioio_piotrans_addr
	(conn, 0,		/* device and (override) dev_info */
	 GIOIO_SPACE_GFX,	/* device in slot space */
	 0,			/* base relative offset */
	 GIOIO_GFX_PIO_MAP_SIZE,	/* pio space size */
	 0);			/* flag word */

    soft->gs_regs = regs;	/* save for later */
    ptr = (uint *) regs;

    printf("gsamp_attach: I can see my device regs at 0x%x\n", ptr);
    printf("              base[0] 0x%x [4] 0x%x [8] 0x%x\n",
	   (uint) ptr[0], (uint) ptr[1], (uint) ptr[2]);
    ptr += 1024 * 1024;
    printf("              base[0x400000] 0x%x [4] 0x%x [8] 0x%x\n",
	   ptr[0], ptr[1], ptr[2]);
#endif

  {
    extern void giobr_bus_reset(vertex_hdl_t gconn);

    printf(" calling bus reset\n");
    giobr_bus_reset(conn);
  }


#if 1
  {
    int val;
    int *p;
    extern unsigned giobr_badaddr(vertex_hdl_t gconn, int *ptr);

    p = (int *)&ptr[1];
    val = giobr_badaddr(conn, p);
    printf("badaddr_val @ 0x%x val=%x\n",p, val);

    p = (int *)&ptr[0x100000/4];
    val = giobr_badaddr(conn, p);
    printf("badaddr_val @ 0x%x val=%x\n",p, val);

    p = (int *)&ptr[1];
    if (!badaddr_val((void *) p, 4, &val))
	printf("badaddr_val @ 0x%x val=%x\n", p, val);
    else
	printf("badaddr_val failed\n");

    p -= 0x100000/4;
    if (!badaddr_val((void *) p, 4, &val))
	printf("badaddr_val @ 0x%x val=%x\n", p, val);
    else
	printf("badaddr_val failed\n");

    printf("  I can see my device regs at AGAIN 0x%x\n", ptr);
    printf("              base[0] 0x%x [4] 0x%x [8] 0x%x\n",
	   (uint) ptr[0], (uint) ptr[1], (uint) ptr[2]);
  }
#endif

    ginfo = gioio_info_get(conn);
    printf("  product id 0x%x\n", gioio_info_product_id_get(ginfo));

    /*
     * Set up our interrupt.
     * for fun lets allocate 4
     */
    for (n = 0; n < 4; n++) {
	soft->gs_intr[n] = gioio_intr_alloc(conn, 0,
					    GIOIO_INTR_LINE(n),
					    soft->gs_vhdl);
	if (!soft->gs_intr[n]) {
	    printf("gsamp_attach: failed intr_alloc[%d]\n", n);
	    continue;
	}
	gioio_intr_connect(soft->gs_intr[n],
			   gsamp_dma_intrx[n].func, soft, (void *) 0);
    }

    /*
     * set up our error handler
     */
    gioio_error_register(conn, gsamp_error_handler, soft);

    /*
     * For gioio clients, *now* is the time to
     * allocate pollhead structures.
     */
    soft->gs_pollhead = phalloc(0);

    return 0;			/* attach successsful */
}

/* =====================================================================
 *            DRIVER OPEN/CLOSE
 */

/*
 *    gsamp_open: called when a device special file is
 *      opened or when a block device is mounted.
 */

/* ARGSUSED */
int
gsamp_open(dev_t * devp, int oflag, int otyp, cred_t * crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(*devp);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);
    gsamp_regs_t            regs = soft->gs_regs;

    printf("gsamp_open() regs=%x\n", regs);

    /*
     * BLOCK DEVICES: now would be a good time to
     * calculate the size of the device and stash it
     * away for use by gsamp_size.
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
 *    gsamp_close: called when a device special file
 *      is closed by a process and no other processes
 *      still have it open ("last close").
 */

/* ARGSUSED */
int
gsamp_close(dev_t dev, int oflag, int otyp, cred_t * crp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);
    gsamp_regs_t            regs = soft->gs_regs;

    printf("gsamp_close() regs=%x\n", regs);
    return 0;
}

/* =====================================================================
 *            CONTROL ENTRY POINT
 */

/*
 *    gsamp_ioctl: a user has made an ioctl request
 *      for an open character device.
 *
 *      cmd and arg are as specified by the user; arg is
 *      probably a pointer to something in the user's
 *      address space, so you need to use copyin() to
 *      read through it and copyout() to write through it.
 */

/* ARGSUSED */
int
gsamp_ioctl(dev_t dev, int cmd, void *arg,
	    int mode, cred_t * crp, int *rvalp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);
    gsamp_regs_t            regs = soft->gs_regs;

    printf("gsamp_ioctl() regs=%x\n", regs);

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
gsamp_read(dev_t dev, uio_t * uiop, cred_t * crp)
{
    return physiock(gsamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_READ,	/* direction flag for buf_t */
		    gsamp_size(dev),
		    uiop);
}

/* ARGSUSED */
int
gsamp_write(dev_t dev, uio_t * uiop, cred_t * crp)
{
    return physiock(gsamp_strategy,
		    0,		/* alocate temp buffer & buf_t */
		    dev,	/* dev_t arg for strategy */
		    B_WRITE,	/* direction flag for buf_t */
		    gsamp_size(dev),
		    uiop);
}

/* ARGSUSED */
int
gsamp_strategy(struct buf *bp)
{
    /* TBD */

    return 0;
}

/* =====================================================================
 *            POLL ENTRY POINT
 */

int
gsamp_poll(dev_t dev, short events, int anyyet,
	   short *reventsp, struct pollhead **phpp, unsigned int *genp)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);
    gsamp_regs_t            regs = soft->gs_regs;

    short                   happened = 0;
    unsigned int            gen;

    printf("gsamp_poll() regs=%x\n", regs);

    /*
     * Need to snapshot the pollhead generation number before we check the
     * device state.  In many drivers a lock is used to interlock the
     * ``high'' and ``low'' portions of the driver.  In those cases we
     * can wait to do this snapshot till we're in the critical region.
     * Sanpshotting it early isn't a problem since that merely makes the
     * snapshotted generation number a more conservative estimate of
     * what ``generation'' of the pollhead our event state report indicates.
     */
    gen = POLLGEN(soft->gs_pollhead);

    if (events & (POLLIN | POLLRDNORM))
	if (soft->gs_sst & GSAMP_SST_RX_READY)
	    happened |= POLLIN | POLLRDNORM;

    if (events & POLLOUT)
	if (soft->gs_sst & GSAMP_SST_TX_READY)
	    happened |= POLLOUT;

    if (soft->gs_sst & GSAMP_SST_ERROR)
	happened |= POLLERR;

    *reventsp = happened;

    if (!happened && anyyet) {
	*phpp = soft->gs_pollhead;
	*genp = gen;
    }

    return 0;
}

/* =====================================================================
 *            MEMORY MAP ENTRY POINTS
 */

/* ARGSUSED */
int
gsamp_map(dev_t dev, vhandl_t * vt,
	  off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);
    gsamp_regs_t            regs = soft->gs_regs;
    caddr_t                 kaddr;

    printf("gsamp_map() regs=%x\n", regs);

    kaddr = gioio_piotrans_addr(vhdl, 0,
				GIOIO_SPACE_GFX,
				off, len, 0);
    if (kaddr == NULL)
	return EINVAL;

    v_mapphys(vt, kaddr, len);

    return 0;
}

/* ARGSUSED2 */
int
gsamp_unmap(dev_t dev, vhandl_t * vt)
{
    /*
     * If you had to do an gioio_piomap_alloc inside
     * gsamp_map, this is the place to chase down
     * the gioio_piomap_t for gioio_piomap_free.
     */
    return (0);
}

/* =====================================================================
 *            INTERRUPT ENTRY POINTS
 *
 *      We avoid using the standard name, since our
 *      prototype has changed.
 */

static void
gsamp_dma_intr0(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr0; regs are at 0x%x\n", vhdl, regs);

    /*
     * for each buf our hardware has processed,
     *      set buf->b_resid,
     *      call gioio_dmamap_done,
     *      call bioerror() or biodone().
     *
     * XXX- would it be better for buf->b_iodone
     * to be used to get to gioio_dmamap_done?
     */

    /*
     * may want to call pollwakeup.
     */
}

static void
gsamp_dma_intr1(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr1; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr2(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr2; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr3(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr3; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr4(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr4; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr5(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr5; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr6(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr6; regs are at 0x%x\n", vhdl, regs);
}

static void
gsamp_dma_intr7(intr_arg_t arg)
{
    gsamp_soft_t            soft = (gsamp_soft_t) arg;
    vertex_hdl_t            vhdl = soft->gs_vhdl;
    gsamp_regs_t            regs = soft->gs_regs;

    cmn_err(CE_CONT, "%v intr7; regs are at 0x%x\n", vhdl, regs);
}


/* =====================================================================
 *            ERROR ENTRY POINTS
 */
static int
gsamp_error_handler(void *einfo,
		    int error_code,
		    ioerror_mode_t mode,
		    ioerror_t * ioerror)
{
    gsamp_soft_t            soft = (gsamp_soft_t) einfo;
    vertex_hdl_t            vhdl = soft->gs_vhdl;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: gsamp_error_handler\n", vhdl);
#else
    vhdl = vhdl;
#endif

    /*
     * XXX- there is probably a lot more to do
     * to recover from an error on a real device;
     * experts on this are encouraged to add common
     * things that need to be done into this function.
     */

    ioerror_dump("sample_gioio", error_code, mode, ioerror);

    return IOERROR_HANDLED;
}

/* =====================================================================
 *            SUPPORT ENTRY POINTS
 */

/*
 *    gsamp_unload: tear down EVERYTHING so the driver
 *      can be unloaded. Return nonzero if you can't.
 */

int
gsamp_unload(void)
{
    /*
     * I'm not sure I *can* ... yet.
     */
    return -1;			/* zero would be "ok, did it." */
}

/*
 *    gsamp_halt: called during orderly system
 *      shutdown; no other device driver call will be
 *      made after this one.
 */

void
gsamp_halt(void)
{
    printf("gsamp_halt()\n");
}

/*
 *    gsamp_size: return the size of the device in
 *      "sector" units (multiples of NBPSCTR).
 */
int
gsamp_size(dev_t dev)
{
    vertex_hdl_t            vhdl = dev_to_vhdl(dev);
    gsamp_soft_t            soft = gsamp_soft_get(vhdl);

    return soft->gs_blocks;
}

/*
 *    gsamp_print: used by the kernel to report an
 *      error detected on a block device.
 */
int
gsamp_print(dev_t dev, char *str)
{
    cmn_err(CE_NOTE, "%V: %s\n", dev, str);
    return 0;
}
