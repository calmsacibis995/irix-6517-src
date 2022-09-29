/**************************************************************************
 *                                                                        *
 *                 Copyright (C) 1996, Silicon Graphics, Inc              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "io/giobr.c: $Revision: 1.53 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/nic.h>
#include <sys/ddi.h>

#include <sys/xtalk/xwidget.h>

#include <sys/GIO/gioio.h>
#include <sys/GIO/giobr.h>
#include <sys/GIO/giobr_private.h>

#include <sys/PCI/bridge.h>

/*
 * dev prefix and devflag are required by convention
 */
#define	GIOBR_PREFIX	"giobr_"
int                     giobr_devflag = D_MP;

#ifdef GIOBR_BRINGUP
#define DEBUG			1
#define ATTACH_DEBUG		1
#define ERROR_DEBUG		1
#define DPRINTF(x)		printf x
#else
#define DPRINTF(x)
#endif

/* XXX for path1831 ONLY, in kudzu these definitions will be in gioio.h */
#define GIOIO_NUM_SPACE         4

#define GIOIO_GFX_PIO_OFFS              0x0L
#define GIOIO_GIO1_PIO_OFFS             0x00400000L
#define GIOIO_GIO1X_PIO_OFFS            0x00500000L
#define GIOIO_GIO2_PIO_OFFS             0x00600000L


#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))


/*
 * bridge llp stats
 */
typedef struct giobr_stats_s {
    u_long                  br_llp_crc;
    u_long                  br_llp_seq;
    u_long                  br_llp_tx_retry;
    u_long                  br_llp_rx_retry;
    u_long                  br_llp_tx_retry_ovfl;
    u_long                  br_llp_rx_retry_ovfl;
} giobr_stats_t;

/* =====================================================================
 *            Bridge Device State structure
 *
 *      one instance of this structure is kept for each
 *      Bridge ASIC in the system.
 */

typedef struct giobr_soft_s *giobr_soft_t;

struct giobr_soft_s {
    vertex_hdl_t            gs_vhdl;	/* backpointer is useful, */
    bridge_t               *gs_base;	/* as is a PIO pointer */
    xwidgetnum_t            gs_xid;	/* and our crosstalk target ID number */

    vertex_hdl_t            gs_xconn;	/* where to get xtalk services */
    vertex_hdl_t            gs_master;	/* CACHED master vertex number */
    xwidgetnum_t            gs_mxid;	/* CACHED master's xtalk ID number */

    bridgereg_t		    gs_dirmap;	/* dirmap register shadow */		
    size_t                  gs_dirmap_size;/* size, GIO direct map */
    iopaddr_t               gs_dirmap_xbase;/* XIO address, GIO direct map */
    struct map             *bs_ate_map;	/* dma resource allocation map */

    /* gs_product will the unique id which will 
     * identify the board. It is made from the 
     * gio ids probed from the 4 possible addresses.
     * In addition there will be a probed flag.
     * gs_device is currently unused
     */
    char		    gs_probed[GIOIO_NUM_SPACE];
    gioio_product_id_t      gs_product;
    gioio_device_id_t       gs_device;

    unsigned char           gs_dma_free;
    unsigned char           gs_dma_inuse;

    /* INTERUPT CONNECTION INFORMATION
     * device will get 8 wires, INT0 thru
     * INT7, they can pull for interrupt
     * service. These wires are hooked up to
     * the eight GIO Interrupt pins on
     * Bridge in a way that seems to be
     * unspecified, but the device make use
     * none or all of them.
     */

    struct gs_intr_s {
	xtalk_intr_t            bsi_xtalk_intr;
	giobr_intr_t            bsi_giobr_intr;
    } gs_intr[8];

    unsigned int	    gs_llp_errcnt;
    giobr_stats_t           stats;
};

static error_handler_f  giobr_error_handler;

/* =====================================================================
 *            Bridge (giobr) state management functions
 *
 *      giobr_soft_get is here because we do it in a lot
 *      of places and I want to make sure they all stay
 *      in step with each other.
 *
 *      giobr_soft_set is here because I want it to be
 *      closely associated with giobr_soft_get, even
 *      though it is only called in one place.
 */

#define	giobr_soft_get(v)	((giobr_soft_t)hwgraph_fastinfo_get(v))
#define	giobr_soft_set(v,i)	(hwgraph_fastinfo_set((v),(arbitrary_info_t)(i)))

/* =====================================================================
 * proto-types
 */
static void do_giobr_bridge_config(bridge_t *, giobr_bridge_config_t);
static void do_giobr_link_reset(bridge_t *, giobr_soft_t);


/* =====================================================================
 *            RRB management
 */

/*
 *    giobr_rrb_count_valid: count how many RRBs are
 *      marked valid for the specified GIO slot on this
 *      bridge.
 *
 *      NOTE: The "slot" parameter for all giobr_rrb
 *      management routines must include the "virtual"
 *      bit; when manageing both the normal and the
 *      virtual channel, separate calls to these
 *      routines must be made. To denote the virtual
 *      channel, add GIOBR_RRB_SLOT_VIRTUAL to the slot
 *      number.
 *
 *      IMPL NOTE: The obvious algorithm is to iterate
 *      through the RRB fields, incrementing a count if
 *      the RRB is valid and matches the slot. However,
 *      it is much simpler to use an algorithm derived
 *      from the "partitioned add" idea. First, XOR in a
 *      pattern such that the fields that match this
 *      slot come up "all ones" and all other fields
 *      have zeros in the mismatching bits. Then AND
 *      together the bits in the field, so we end up
 *      withg one bit turned on for each field that
 *      matched. Now we need to count these bits. This
 *      can be done either with a series of shift/add
 *      instructions or by using "tmp % 15"; I expect
 *      that the cascaded shift/add will be faster.
 */

#define	LSBIT(word)		((word) &~ ((word)-1))

#define GIOBR_RRB_MAX		8
#define	GIOBR_RRB_SLOT_VIRTUAL	8

static int
giobr_rrb_count_valid(bridge_t *bridge,
		      gioio_slot_t slot)
{
    bridgereg_t             tmp;

    tmp = (slot & 1) ? bridge->b_odd_resp : bridge->b_even_resp;
    tmp ^= 0x11111111 * (7 - slot / 2);
    tmp &= (0xCCCCCCCC & tmp) >> 2;
    tmp &= (0x22222222 & tmp) >> 1;
    tmp += tmp >> 4;
    tmp += tmp >> 8;
    tmp += tmp >> 16;
    return tmp & 15;
}

/*
 *    giobr_rrb_count_avail: count how many RRBs are
 *      available to be allocated for the specified
 *      slot.
 *
 *      IMPL NOTE: similar to the above, except we are
 *      just counting how many fields have the valid bit
 *      turned off.
 */
static int
giobr_rrb_count_avail(bridge_t *bridge,
		      gioio_slot_t slot)
{
    bridgereg_t             tmp;

    tmp = (slot & 1) ? bridge->b_odd_resp : bridge->b_even_resp;
    tmp = (0x88888888 & ~tmp) >> 3;
    tmp += tmp >> 4;
    tmp += tmp >> 8;
    tmp += tmp >> 16;
    return tmp & 15;
}

/*
 *    giobr_rrb_alloc: allocate some additional RRBs
 *      for the specified slot. Returns -1 if there were
 *      insufficient free RRBs to satisfy the request,
 *      or 0 if the request was fulfilled.
 *
 *      Note that if a request can be partially filled,
 *      it will be, even if we return failure.
 *
 *      IMPL NOTE: again we avoid iterating across all
 *      the RRBs; instead, we form up a word containing
 *      one bit for each free RRB, then peel the bits
 *      off from the low end.
 */
static int
giobr_rrb_alloc(bridge_t *bridge,
		gioio_slot_t slot,
		int more)
{
    int                     oops = 0;
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp, bit;

    regp = (slot & 1) ? &bridge->b_odd_resp : &bridge->b_even_resp;
    reg = *regp;
    tmp = (0x88888888 & ~reg) >> 3;
    while (more-- > 0) {
	bit = LSBIT(tmp);
	if (!bit) {
	    oops = -1;
	    break;
	}
	tmp &= ~bit;
	reg = ((reg & ~(bit * 15)) | (bit * (8 + slot / 2)));
    }
    *regp = reg;
    return oops;
}

/*
 *    giobr_rrb_alloc_all: quickly allocate all
 *      available RRBs to the specified slot.
 *
 *      IMPL NOTE: this is *much* faster than calling
 *      giobr_rrb_alloc with a large allocation, since
 *      we can allocate all the RRBs in parallel.
 */
static void
giobr_rrb_alloc_all(bridge_t *bridge,
		    gioio_slot_t slot)
{
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp;

    regp = (slot & 1) ? &bridge->b_odd_resp : &bridge->b_even_resp;
    reg = *regp;
    tmp = (0x88888888 & ~reg) >> 3;
    reg &= ~(15 * tmp);
    reg |= ((8 + slot / 2) * tmp);
    *regp = reg;
}

/*
 *    giobr_rrb_free: release some of the RRBs that
 *      have been allocated for the specified
 *      slot. Returns zero for success, or negative if
 *      it was unable to free that many RRBs.
 *
 *      IMPL NOTE: We form up a bit for each RRB
 *      allocated to the slot, aligned with the VALID
 *      bitfield this time; then we peel bits off one at
 *      a time, releasing the corresponding RRB.
 */
static int
giobr_rrb_free(bridge_t *bridge,
	       gioio_slot_t slot,
	       int less)
{
    int                     oops = 0;
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp, bit;

    regp = (slot & 1) ? &bridge->b_odd_resp : &bridge->b_even_resp;
    reg = *regp;
    tmp = reg ^ (0x11111111 * (7 - slot / 2));
    tmp &= (0x33333333 & tmp) << 2;
    tmp &= (0x44444444 & tmp) << 1;
    while (less-- > 0) {
	bit = LSBIT(tmp);
	if (!bit) {
	    oops = -1;
	    break;
	}
	tmp &= ~bit;
	reg &= ~bit;
    }
    *regp = reg;
    return oops;
}

/*
 *    giobr_rrb_free_all: quickly free all RRBs
 *      allocated to the specified slot. Remember to
 *      call this with both the normal and the virtual
 *      slot numbers if you are tearing down a device
 *      that has been using both channels!
 *
 *      IMPL NOTE: We form up one bit per matching RRB,
 *      aligned with the valid bits, then slap the valid
 *      bits off all at once.
 */
static void
giobr_rrb_free_all(bridge_t *bridge,
		   gioio_slot_t slot)
{
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp;

    regp = (slot & 1) ? &bridge->b_odd_resp : &bridge->b_even_resp;
    reg = *regp;
    tmp = reg ^ (0x11111111 * (7 - slot / 2));
    tmp &= (0x33333333 & tmp) << 2;
    tmp &= (0x44444444 & tmp) << 1;
    *regp = reg & ~tmp;
}

/* XXX- temporary hack to avoid compiler warnings,
 * until above functions are hooked into giobr_dma*
 */
int
giobr_rrb_call_all(bridge_t *bridge,
		   gioio_slot_t slot,
		   int more,
		   int less)
{
    int                     rv = 0;

    rv += giobr_rrb_count_valid(bridge, slot);
    rv += giobr_rrb_count_avail(bridge, slot);
    rv += giobr_rrb_alloc(bridge, slot, more);
    giobr_rrb_alloc_all(bridge, slot);
    rv += giobr_rrb_free(bridge, slot, less);
    giobr_rrb_free_all(bridge, slot);
    return rv;
}

/* =====================================================================
 *              Bridge (giobr) "Device Driver" entry points
 */
static void             giobr_setwidint(xtalk_intr_t xtalk_intr);
static void             giobr_error_intr_handler(intr_arg_t arg);

/* giobr_slot_probe: check a bridge to see if a gio slot responds.
 * On the GIO Bridge Cards there is only GIO slot 0.
 * This slot has the GIO address range 0x1f00_0000 - 0x1f3_FFFF.
 * The card must respond with a valid Product ID otherwise
 * we would consider the giobr card dead.
 */
/*ARGSUSED2 */
static gioio_product_id_t
giobr_slot_probe(bridge_t *bridge, int slot,
		 volatile gioio_product_id_t *pr_id)
{
    gioio_product_id_t      rv;
    bridgereg_t             old_enable, new_enable;

    old_enable = bridge->b_int_enable;
    new_enable = old_enable & ~BRIDGE_IMR_GIO_MST_TIMEOUT;
    bridge->b_int_enable = new_enable;

    if (bridge->b_int_status & BRIDGE_IRR_GIO_GRP) {
	bridge->b_int_rst_stat = BRIDGE_IRR_GIO_GRP_CLR;
	(void) bridge->b_wid_tflush;	/* flushbus */
    }
    if (badaddr_val((void *) pr_id, 4, &rv))
	rv = 0;

    bridge->b_int_enable = old_enable;
    (void) bridge->b_int_enable;	/* stall until store arrives */

    return (rv & GIOIO_PROD_ID_MASK);
}

/*

 *      under the new i/o infrastructure, we only need
 *      to register ourselves as a driver; our attach
 *      routine will be called if a giobr is located.
 */
int
giobr_init(void)
{
#if DEBUG && ATTACH_DEBUG
    printf("giobr_reg\n");
#endif
    return xwidget_driver_register(BRIDGE_WIDGET_PART_NUM,
				   BRIDGE_WIDGET_MFGR_NUM,
				   "giobr_",
				   0);
}

#if NOT_NEEDED_YET
/* ARGSUSED */
int
giobr_open(dev_t *devp, int oflag, int otyp, cred_t *credp)
{
    return 0;
}

/*ARGSUSED */
int
giobr_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    return 0;
}

/*ARGSUSED */
int
giobr_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{

    return error;
}

/*ARGSUSED */
int
giobr_unmap(dev_t dev, vhandl_t *vt)
{
    return 0;
}
#endif				/* NOT_NEEDED_YET */

/* Convert from ssram_bits in control register to number of SSRAM entries */
#define ATE_NUM_ENTRIES(n) _ate_info[n]

/* Possible choices for number of ATE entries in Bridge's SSRAM */
static int              _ate_info[] =
{
    0,					/* 0 entries */
    8 * 1024,				/* 8K entries */
    16 * 1024,				/* 16K entries */
    64 * 1024				/* 64K entries */
};

#define ATE_NUM_SIZES (sizeof(_ate_info) / sizeof(int))
#define ATE_PROBE_VALUE 0x0123456789abcdefULL

/*
 * Determine the size of this bridge's external mapping SSRAM, and set
 * the control register appropriately to reflect this size, and initialize
 * the external SSRAM.
 */
static int
giobr_setup_SSRAM(bridge_t *bridge)
{
    int                     largest_working_size = 0;
    int                     num_entries, entry;
    int                     i, j;
    bridgereg_t             old_enable, new_enable;

    /* Probe SSRAM to determine its size. */
    old_enable = bridge->b_int_enable;
    new_enable = old_enable & ~BRIDGE_IMR_PCI_MST_TIMEOUT;
    bridge->b_int_enable = new_enable;

    for (i = 1; i < ATE_NUM_SIZES; i++) {
	/* Try writing a value */
	bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(i) - 1] = ATE_PROBE_VALUE;

	/* Guard against wrap */
	for (j = 1; j < i; j++)
	    bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(j) - 1] = 0;

	/* See if value was written */
	if (bridge->b_ext_ate_ram[ATE_NUM_ENTRIES(i) - 1] == ATE_PROBE_VALUE)
	    largest_working_size = i;
    }
    bridge->b_int_enable = old_enable;
    (void) bridge->b_int_enable;	/* stall until store arrives */

    bridge->b_wid_control &= ~BRIDGE_CTRL_SSRAM_SIZE_MASK;
    bridge->b_wid_control |= BRIDGE_CTRL_SSRAM_SIZE(largest_working_size);
    bridge->b_wid_control;		/* inval addr bug war */

    num_entries = ATE_NUM_ENTRIES(largest_working_size);

    /* Initialize external mapping entries */
    for (entry = 0; entry < num_entries; entry++)
	bridge->b_ext_ate_ram[entry] = 0;

    return (num_entries);
}

/*
 * Allocate "count" contiguous Bridge Address Translation Entries
 * on the specified bridge to be used for PCI to XTALK mappings.
 * Indices in rm map range from 1..num_entries.  Indicies returned
 * to caller range from 0..num_entries-1.
 *
 * Return the start index on success, -1 on failure.
 */
/* REFERENCED */
static int
giobr_ate_alloc(giobr_soft_t giobr_soft, int count)
{
    struct map             *ate_map = giobr_soft->bs_ate_map;
    int                     found = (int) rmalloc(ate_map, (size_t) count);

    if (found)
	return (found - 1);
    else
	return (-1);
}

static void
giobr_ate_free(giobr_soft_t giobr_soft, int index, int count)
{
    struct map             *ate_map = giobr_soft->bs_ate_map;

    rmfree(ate_map, count, index + 1);
}

/*
 * although the giobr cards will NOT have slots there may
 * be multiple devices on the bus which will respond to
 * probes at GFX, GIO1, GIO1X or GIO2 addresses
 * so the unique combination of gio dev id's probed out at
 * various slots will determine the product id of the xtalk
 * card and the device drivers should register accordinly. 
 */

#define			XPIO(x)		((iopaddr_t)(x) + BRIDGE_DEVIO0)

static struct gio_probe_list_s {
    iopaddr_t		pio_xbase;
    int			map_size;
    char		prid_shft;
} gio_probe_list[GIOIO_NUM_SPACE] = {

    /* NOTE: do not re-arrange the ordering of this list.
     * the index to gio_probe_list[x] must == GIOIO_SPACE_XXX value
     */
    /* real probe */
    {	/* index==0, GIOIO_SPACE_GFX */
	XPIO(GIOIO_GFX_PIO_OFFS),
	GIOIO_GFX_PIO_MAP_SIZE,
	GIOIO_SL0_PR_SHFT,
    },
    {	/* index==1, GIOIO_SPACE_GIO1 */
	XPIO(GIOIO_GIO1_PIO_OFFS),
	GIOIO_GIO1_PIO_MAP_SIZE,
	GIOIO_SL1_PR_SHFT,
    },
    {	/* index==2, GIOIO_SPACE_GIO1X */
	XPIO(GIOIO_GIO1X_PIO_OFFS),
	GIOIO_GIO1X_PIO_MAP_SIZE,
	GIOIO_SL1X_PR_SHFT,
    },
    {	/* index==3, GIOIO_SPACE_GIO2 */
	XPIO(GIOIO_GIO2_PIO_OFFS),
	GIOIO_GIO2_PIO_MAP_SIZE,
	GIOIO_SL2_PR_SHFT,
    },
};

#define GIOBR_DEV_CONFIG(flgs)		((flgs) & ~BRIDGE_DEV_OFF_MASK)

/*
 * DMA Dirmap attributes for Command (desc) channel
 */
#define DEVICE_DMA_ATTRS_CMD			( \
	/* BRIDGE_DEV_VIRTUAL_EN */ 0		| \
	/* BRIDGE_DEV_RT */ 0			| \
	/* BRIDGE_DEV_PREF */ 0			| \
	/* BRIDGE_DEV_PRECISE */ 0		| \
	BRIDGE_DEV_COH				| \
	/* BRIDGE_DEV_BARRIER */ 0		| \
	BRIDGE_DEV_GBR				)

/*
 * DMA Dirmap attributes for Data channel
 */
#define DEVICE_DMA_ATTRS_DATA			( \
	/* BRIDGE_DEV_VIRTUAL_EN */ 0		| \
	/* BRIDGE_DEV_RT */ 0			| \
	/* BRIDGE_DEV_PREF */ 0			| \
	/* BRIDGE_DEV_PRECISE */ 0		| \
	BRIDGE_DEV_COH				| \
	/* BRIDGE_DEV_BARRIER */ 0		| \
	BRIDGE_DEV_GBR				)

#define DEVICE_PIO_DEVOFF	0x1f0

static struct giobr_bridge_config_s giobr_config_default = {

    (GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV),

    0,	/* use default */

    {	/* Device(x) params
	 */
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 0),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 2),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 4),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 5),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 6),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 7),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 8),
	DEVICE_DMA_ATTRS_CMD | (DEVICE_PIO_DEVOFF + 9),
    },
    {	/* RRB's
	 * even devs		odd devs
	 */ 
	4,  /*0*/		4,  /*1*/
	2,  /*2*/		2,  /*3*/
	1,  /*4*/		1,  /*5*/
	1,  /*6*/		1,  /*7*/
    },
    {	/* Intr-line[n] to req/grant(m) mapping, where n=m
         */
	0, 1, 2, 3, 4, 5, 6, 7,
    },
};

static void
do_giobr_hw_init(bridge_t *bridge,
                 giobr_soft_t giobr_soft,
		 int alloc_intrs)
{
    int			num_ate;
    int			entry;
    device_desc_t	dev_desc;
    xtalk_intr_t	xtalk_intr;

    bridge->b_dir_map = giobr_soft->gs_dirmap;

#if IOPGSIZE == 4096
    bridge->b_wid_control &= ~(BRIDGE_CTRL_PAGE_SIZE);
#elif IOPGSIZE == 16384
    bridge->b_wid_control |= BRIDGE_CTRL_PAGE_SIZE;
#else
    <<<bomb ! !!Unable to deal with IOPGSIZE >>>
#endif
    bridge->b_wid_control;  /* inval addr bug war */


    /* Initialize internal mapping entries */
    for (entry = 0; entry < BRIDGE_INTERNAL_ATES; entry++)
	bridge->b_int_ate_ram[entry].wr = 0;
    /*
     * Determine if there's external mapping SSRAM on this
     * bridge.  Set up Bridge control register appropriately,
     * inititlize SSRAM, and set software up to manage RAM
     * entries as an allocatable resource.
     *
     * Currently, we just use the rm* routines to manage ATE
     * allocation.  We should probably replace this with a
     * Best Fit allocator.
     */
    num_ate = giobr_setup_SSRAM(bridge);
    if (num_ate == 0)
	num_ate = BRIDGE_INTERNAL_ATES;
    giobr_soft->bs_ate_map = rmallocmap(num_ate);
    giobr_ate_free(giobr_soft, 0, num_ate);

    /*
     * set the GIO_TIMEOUT bit in the gio/pci bus timeout register. This
     * prevents baddaddr to a non existant address on the gio address
     * space from hanging the system
     */
    bridge->b_bus_timeout |= BRIDGE_TMO_GIO_TIMEOUT_MASK;

    /*
     * set up the error intr handler
     * prom had setup some initial values but lets explicitly
     * setup the ones we need/want
     */
    bridge->b_int_enable = 0;
    bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

    if (alloc_intrs) {

	dev_desc = device_desc_dup(giobr_soft->gs_vhdl);
	device_desc_flags_set(dev_desc,
			  device_desc_flags_get(dev_desc) | D_INTR_ISERR);
	xtalk_intr = xtalk_intr_alloc(giobr_soft->gs_xconn,
				      dev_desc, giobr_soft->gs_vhdl);
	device_desc_free(dev_desc);

	xtalk_intr_connect(xtalk_intr,
			    (intr_func_t) giobr_error_intr_handler,
			    (intr_arg_t) giobr_soft,
			    (xtalk_intr_setfunc_t) giobr_setwidint,
			    (void *) bridge,
			    (void *) 0);

	/*
	 * arrange to handle errors.
	 */
	xwidget_error_register(giobr_soft->gs_xconn,
			       giobr_error_handler, giobr_soft);
    }
    /*
     * now we can start handling error interrupts;
     * enable most err ints, but keep the bridge int lines
     * 0..7 disabled
     */
    bridge->b_int_enable |= (BRIDGE_IRR_CRP_GRP |
    			     BRIDGE_IRR_RESP_BUF_GRP |
			     BRIDGE_IRR_REQ_DSP_GRP |
			     BRIDGE_IRR_LLP_GRP |
			     BRIDGE_IRR_SSRAM_GRP |
			     BRIDGE_IRR_PCI_GRP |
			     BRIDGE_IRR_GIO_GRP);

    /* don't want an interrupt on every LLP micropacket retry */
    bridge->b_int_enable &= ~(BRIDGE_ISR_LLP_REC_SNERR |
    			      BRIDGE_ISR_LLP_REC_CBERR |
			      BRIDGE_ISR_LLP_TX_RETRY);

#ifdef BRIDGE1_TIMEOUT_WAR
    if (XWIDGET_REV_NUM(bridge->b_wid_id) == BRIDGE_REV_A) {
	/*
	 * Turn off these interrupts.  They can't be trusted in bridge 1
	 */
	bridge->b_int_enable &= ~(BRIDGE_IMR_XREAD_REQ_TIMEOUT |
				  BRIDGE_IMR_UNEXP_RESP);
    }
#endif
    do_giobr_bridge_config(bridge, &giobr_config_default);

}

/*
 * giobr_attach: called every time the crosstalk
 * infrastructure is asked to initialize a widget
 * that matches the part number we handed to the
 * registration routine above.
 */
int
giobr_attach(vertex_hdl_t xconn)
{
    /* REFERENCED */
    graph_error_t           rc;
    vertex_hdl_t            giobr;
    bridge_t               *bridge;
    giobr_soft_t            giobr_soft;
    xwidget_info_t          info;
    int                     n;
    vertex_hdl_t            gconn;

#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v: giobr_attach\n", xconn);
#endif

    /* XXX patch1831 ONLY
     * setting the TO value to be GIO-Xtalk friendly
     * take this out in kudzu when there is better fix in heart.c
     */
    {
	heart_cfg_t         *heart_cfg = HEART_CFG_K1PTR;

	if (heart_cfg->h_wid_req_timeout < 128)
	    heart_cfg->h_wid_req_timeout = 128;         /* ~163 us */
    }

    bridge = (bridge_t *) xtalk_piotrans_addr(xconn, NULL, 0,
					      sizeof(bridge_t), 0);

    if ((bridge->b_wid_stat & BRIDGE_STAT_PCI_GIO_N))
	return -1;			/* someone else handles PCI bridges. */

    rc = hwgraph_path_add(xconn, EDGE_LBL_GIO, &giobr);
    ASSERT(rc == GRAPH_SUCCESS);

    /*
     * decode the nic, and hang its stuff off our
     * connection point where other drivers can get
     * at it.
     */
    nic_bridge_vertex_info(xconn, (nic_data_t)&bridge->b_nic);

    /* if we want to "open" the /gio/ vertex, add this.  */
    rc = hwgraph_char_device_add(giobr, EDGE_LBL_CONTROLLER, GIOBR_PREFIX, NULL);
    ASSERT(rc == GRAPH_SUCCESS);

    /*
     * allocate soft state structure, fill in some
     * fields, and hook it up to our vertex.
     */
    NEW(giobr_soft);
    bzero(giobr_soft, sizeof *giobr_soft);

    giobr_soft_set(giobr, giobr_soft);
    giobr_soft->gs_xconn = xconn;
    giobr_soft->gs_vhdl = giobr;
    giobr_soft->gs_base = bridge;

    info = xwidget_info_get(xconn);
    giobr_soft->gs_xid = xwidget_info_id_get(info);
    /* the master is heart for IP30 and master id should be 8 */
    giobr_soft->gs_master = xwidget_info_master_get(info);
    giobr_soft->gs_mxid = xwidget_info_masterid_get(info);

    /*
     * set up initial values for state fields
     */
    giobr_soft->gs_dma_free = 0xFF;
    giobr_soft->gs_dma_inuse = 0;
    for (n = 0; n < 8; ++n) {
	giobr_soft->gs_intr[n].bsi_xtalk_intr = 0;
	giobr_soft->gs_intr[n].bsi_giobr_intr = 0;

    }
    /*
     * setup the GIO -> xtalk mapping for DMAs
     */
    {
	paddr_t                 paddr;
	iopaddr_t               mem_xbase;
	iopaddr_t               offset;

	/*
	 * The dirmap space will be set to Heart and
	 * to addr 512MB to match the host sys_ram_base.
	 * Set the Bridge's GIO to XTalk
	 * Direct Map register to above specifications
	 */
	giobr_soft->gs_dirmap = giobr_soft->gs_mxid << BRIDGE_DIRMAP_W_ID_SHFT;

	/*
	 * XXX- ask master for phys mem base? the xtalk provider
	 * (heart) knows the mem base currently specifying paddr
	 * of 0 returns the AGS_SYSMEM_BASE
	 */
	paddr = 0;

	giobr_soft->gs_dirmap_size = MIN(BRIDGE_DMA_DIRECT_SIZE,ctob(physmem));
	mem_xbase = xtalk_dmatrans_addr(xconn, 0, paddr,
				        giobr_soft->gs_dirmap_size, 0);

	/* mem_xbase is sys memory base in the heart xtalk addr space */
	offset = mem_xbase & ((1ull << BRIDGE_DIRMAP_OFF_ADDRSHFT) - 1ull);
	mem_xbase >>= BRIDGE_DIRMAP_OFF_ADDRSHFT;

	/* mem_xbase is  bits 48 - 31 of xtalk addr */
	/* offset    is bits 30 -  0 of xtalk addr */

	if (mem_xbase) {
	    giobr_soft->gs_dirmap |= BRIDGE_DIRMAP_OFF & mem_xbase;
	    mem_xbase <<= BRIDGE_DIRMAP_OFF_ADDRSHFT;
	    giobr_soft->gs_dirmap_xbase = mem_xbase;
	} else {
	    if (offset >= (512 << 20)) {	/* 512MB */
		giobr_soft->gs_dirmap |= BRIDGE_DIRMAP_ADD512;
		giobr_soft->gs_dirmap_xbase = (512 << 20);
	    } else
		giobr_soft->gs_dirmap_xbase = 0;
	}
	ASSERT(mem_xbase == 0 && offset >= (512 << 20));

    }

    do_giobr_hw_init(bridge, giobr_soft, 1);

    /*
     * setup pio map for the GIO space
     * whatever devices we find should fit into the default
     * 10MByte GIO space for PIO
     */
    /* first find out who's out there. */
    for (n = 0; n < GIOIO_NUM_SPACE; n++) {
	__uint32_t             *wptr;
	iopaddr_t               pio_xbase;
	gioio_product_id_t	id;

	/*
	 * use the 10 MByte device PIO space
	 */
	pio_xbase = gio_probe_list[n].pio_xbase;
	wptr = (__uint32_t *) xtalk_piotrans_addr(xconn, 0, pio_xbase + 4,
						  sizeof(*wptr), 0);

	id = giobr_slot_probe(bridge, 0, wptr);
	if (id) {
	    giobr_soft->gs_product |= id << gio_probe_list[n].prid_shft;
	    giobr_soft->gs_probed[n] = 1;
	}
    }					/* for (gio_probe_list[n]) */
    DPRINTF(("  product id 0x%x\n", giobr_soft->gs_product));
    if (giobr_soft->gs_product == 0)
	cmn_err(CE_DEBUG, "unknown device on gio bridge\n");

    /*
     * reset the bridge to work around the baddr()/probe bug
     */
    do_giobr_link_reset(bridge, giobr_soft);

    /*
     * Register the "gio" vertex as the bus
     * provider vertex, with our provider ops.
     */
    gioio_provider_register(giobr, &giobr_provider);
    gioio_provider_startup(giobr);

    giobr_soft->gs_device = 0;		/* device id always 0 */
    gconn = gioio_device_register(giobr, giobr,
				  0,	/* we'll only have slot 0 */
				  giobr_soft->gs_product,
				  giobr_soft->gs_device);

    /*
     * register the device on the GIO Bus
     * will call attach routine that the device drvr registered
     */
    gioio_device_attach(gconn);

    return 0;
}

/* =====================================================================
 *            PIU MANAGEMENT
 */

/* while "0" is a real xtalk address,
 * it does not get you to a gio device,
 * so giobr_xtalk_addr can use it to
 * note an impossible request.
 * take gio addr and return xtalk address
 * in the widget space
 */
#define	GIOBR_XTALK_ADDR_FAILED	(0)

static iopaddr_t
giobr_xtalk_addr(vertex_hdl_t gconn,
		 gioio_space_t slotspace,
		 iopaddr_t gio_addr,
		 size_t byte_count)
{
    gioio_info_t	    gioio_info = gioio_info_get(gconn);
    giobr_soft_t	    giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    iopaddr_t               xtalk_addr;

    DPRINTF(("giobr_xtalk_addr: slotspace %x gio_addr %x byte_count %x\n",
	   slotspace, gio_addr, byte_count));
    if (slotspace > GIOIO_NUM_SPACE || slotspace < GIOIO_SPACE_GFX)
	return GIOBR_XTALK_ADDR_FAILED;

    if (!giobr_soft->gs_probed[slotspace])
	return GIOBR_XTALK_ADDR_FAILED;

    xtalk_addr = gio_probe_list[slotspace].pio_xbase + gio_addr;

    DPRINTF(("   %x xtalk_addr %x xlimit %x\n",
	   xtalk_addr, xtalk_addr + byte_count));

    if (gio_addr + byte_count <= gio_probe_list[slotspace].map_size)
	return xtalk_addr;

    DPRINTF(("Bad GIO address/len 0x%x/%d\n", gio_addr, byte_count));

    return GIOBR_XTALK_ADDR_FAILED;
}

/* ARGSUSED */
giobr_piomap_t
giobr_piomap_alloc(vertex_hdl_t gconn,
		   device_desc_t dev_desc,
		   gioio_space_t slotspace,
		   iopaddr_t gio_addr,
		   size_t byte_count,
		   size_t byte_count_max,
		   unsigned flags)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;
    giobr_piomap_t          giobr_piomap = 0;
    iopaddr_t               xtalk_addr;
    xtalk_piomap_t          xtalk_piomap;

    /*
     * Code to translate slot-space/addr
     * into xtalk_addr is common between
     * this routine and giobr_piotrans_addr.
     */
    xtalk_addr = giobr_xtalk_addr(gconn, slotspace, gio_addr, byte_count);

    if (xtalk_addr == GIOBR_XTALK_ADDR_FAILED)
	return NULL;

    NEW(giobr_piomap);
    if (giobr_piomap) {
	xtalk_piomap =
	    xtalk_piomap_alloc(xconn, 0,
			       xtalk_addr,
			       byte_count, byte_count_max,
			       flags & 0);	/* process flags */
	if (xtalk_piomap) {
	    giobr_piomap->bp_flags = flags;
	    giobr_piomap->bp_dev = gconn;
	    giobr_piomap->bp_gioaddr = gio_addr;
	    giobr_piomap->bp_mapsz = byte_count;
	    giobr_piomap->bp_xtalk_addr = xtalk_addr;
	    giobr_piomap->bp_xtalk_pio = xtalk_piomap;
	} else {
	    DEL(giobr_piomap);
	    giobr_piomap = 0;
	}
    }
    return giobr_piomap;
}

void
giobr_piomap_free(giobr_piomap_t giobr_piomap)
{
    xtalk_piomap_free(giobr_piomap->bp_xtalk_pio);
    DEL(giobr_piomap);
}

caddr_t
giobr_piomap_addr(giobr_piomap_t giobr_piomap,
		  iopaddr_t gio_addr,
		  size_t byte_count)
{
    return xtalk_piomap_addr(giobr_piomap->bp_xtalk_pio,
			     giobr_piomap->bp_xtalk_addr + gio_addr - giobr_piomap->bp_gioaddr,
			     byte_count);
}

/*ARGSUSED */
void
giobr_piomap_done(giobr_piomap_t giobr_piomap)
{
}

/* ARGSUSED */
caddr_t
giobr_piotrans_addr(vertex_hdl_t gconn,
		    device_desc_t dev_desc,
		    gioio_space_t slotspace,
		    iopaddr_t gio_addr,
		    size_t byte_count,
		    unsigned flags)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;
    iopaddr_t               xtalk_addr;

    xtalk_addr = giobr_xtalk_addr(gconn, slotspace, gio_addr, byte_count);

    if (xtalk_addr == GIOBR_XTALK_ADDR_FAILED)
	return NULL;

    return xtalk_piotrans_addr(xconn, 0, xtalk_addr, byte_count, 0);
}

/* =====================================================================
 *            DMA MANAGEMENT
 *
 *      The Bridge ASIC provides three methods of doing
 *      DMA: via a "direct map" register available in
 *      32-bit GIO space (which selects a contiguous 2G
 *      address space on some other widget), via
 *      "direct" addressing via 64-bit GIO space (all
 *      destination information comes from the GIO
 *      address, including transfer attributes), and via
 *      a "mapped" region that allows a bunch of
 *      different small mappings to be established with
 *      the PMU.
 *
 *      For efficiency, we most prefer to use the 32-bit
 *      direct mapping facility, since it requires no
 *      resource allocations. The advantage of using the
 *      PMU over the 64-bit direct is that single-cycle
 *      GIO addressing can be used; the advantage of
 *      using 64-bit direct over PMU addressing is that
 *      we do not have to allocate entries in the PMU.
 *
 *      XXX- BYTE SWAPPING ISSUES
 */
/*ARGSUSED */
giobr_dmamap_t
giobr_dmamap_alloc(vertex_hdl_t gconn,
		   device_desc_t dev_desc,
		   size_t byte_count_max,
		   unsigned flags)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            giobr = giobr_soft->gs_vhdl;
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;
    giobr_dmamap_t          giobr_dmamap;

    NEW(giobr_dmamap);
    if (giobr_dmamap) {
	giobr_dmamap->bd_flags = flags;
/*
 * XXX TBD: flags->attrs
 giobr_dmamap->bd_dirmap_attrbits = giobr_flags_to_dirmap_attrs(flags);
 giobr_dmamap->bd_ate_attrbits = giobr_flags_to_ate_attrs(flags);
 */
	giobr_dmamap->bd_dev = gconn;
	giobr_dmamap->bd_slot = gioio_info_slot_get(gioio_info);
	giobr_dmamap->bd_soft = giobr_soft_get(giobr);
	giobr_dmamap->bd_xtalk =
	    xtalk_dmamap_alloc(xconn, 0, byte_count_max, flags);

	if (!giobr_dmamap->bd_xtalk) {
	    DEL(giobr_dmamap);
	    return 0;
	}
	/*
	 * Allocate Address Translation Entries from the mapping RAM.
	 * Grab one extra to handle misaligned buffers.
	 * TBD: Really want to grab the extra one?
	 {
	 int ate_count, ate_index;

	 ate_count = IOPG(byte_count_max) + 1;
	 ate_index = giobr_ate_alloc(pcibr_dmamap->bd_soft, ate_count);
	 if (ate_index < 0) {
	 DEL(pcibr_dmamap);
	 return 0;
	 }

	 giobr_dmamap->bd_byte_count = byte_count_max;
	 giobr_dmamap->bd_busy = 0;
	 giobr_dmamap->bd_ate_index = ate_index;
	 giobr_dmamap->bd_ate_count = ate_count;
	 giobr_dmamap->bd_pci_addr = GIO32_MAPPED_BASE + IOPGSIZE*ate_index;
	 }
	 */

    }
    return giobr_dmamap;
}

/*ARGSUSED */
void
giobr_dmamap_free(giobr_dmamap_t giobr_dmamap)
{
    xtalk_dmamap_free(giobr_dmamap->bd_xtalk);

    /*
     * XXX  TBD: ate
     giobr_ate_free(pcibr_dmamap->bd_soft,
     pcibr_dmamap->bd_ate_index,
     pcibr_dmamap->bd_ate_count);

     giobr_dmamap->bd_byte_count = 0;
     */

    DEL(giobr_dmamap);
}

/* take a xtalk_addr and return gio address
 * which can be mapped to the system memory
 * by bridges' Direct Map register
 */
/* ARGSUSED */
static iopaddr_t
giobr_dma_for_xtalk(giobr_soft_t giobr_soft,
		    giobr_dmamap_t giobr_dmamap,	/* might be NULL */
		    iopaddr_t xtalk_addr,
		    size_t byte_count)
{
    iopaddr_t               offset;

    if (!xtalk_addr)
	return 0;			/* pass thru error indicators */

    offset = xtalk_addr - giobr_soft->gs_dirmap_xbase;

    /* offset must fit in 2G dirmap window, range check offset */
    if (offset < BRIDGE_DMA_DIRECT_SIZE)
	return GIO_DIRECT_BASE + offset;

    cmn_err(CE_ALERT, "giobr_dma_for_xtalk: xaddr 0x%x count %d\n",
	    xtalk_addr, byte_count);

    return 0;				/* unable to construct useful DMA address */
}

/* paddr - physical memory addr
 * return - gio bus addr in local space (0 - 1G)
 */
/*ARGSUSED */
iopaddr_t
giobr_dmamap_addr(giobr_dmamap_t dmamap,
		  paddr_t paddr,
		  size_t byte_count)
{
    iopaddr_t               xtalk_addr;

    xtalk_addr = xtalk_dmamap_addr(dmamap->bd_xtalk, paddr, byte_count);
    if (!xtalk_addr)
	return NULL;
    return giobr_dma_for_xtalk(dmamap->bd_soft, dmamap,
			       xtalk_addr, byte_count);
}

/*ARGSUSED */
alenlist_t
giobr_dmamap_list(giobr_dmamap_t giobr_dmamap,
		  alenlist_t palenlist)
{
    palenlist = xtalk_dmamap_list(giobr_dmamap->bd_xtalk, palenlist, 0);
    /* XXX TODO- translate addresses in palenlist from
     * xtalk addresses to gio addresses that map to
     * those xtalk addresses.
     */
    return palenlist;
}

/*ARGSUSED */
void
giobr_dmamap_done(giobr_dmamap_t giobr_dmamap)
{
}

/* take a xtalk_addr and return gio address
 * which can be mapped to the system memory
 * by bridges' PMU
 */
/*ARGSUSED */
iopaddr_t
giobr_dmatrans_addr(vertex_hdl_t gconn,
		    device_desc_t dev_desc,
		    paddr_t paddr,
		    size_t byte_count,
		    unsigned flags)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;
    iopaddr_t               xtalk_addr;

    xtalk_addr = xtalk_dmatrans_addr(xconn, 0, paddr, byte_count, 0);
    if (!xtalk_addr)
	return NULL;

    return giobr_dma_for_xtalk(giobr_soft, 0,
			       xtalk_addr, byte_count);
}

/* ARGSUSED */
alenlist_t
giobr_dmatrans_list(vertex_hdl_t gconn,
		    device_desc_t dev_desc,
		    alenlist_t palenlist,
		    unsigned flags)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;

    palenlist = xtalk_dmatrans_list(xconn, 0, palenlist, flags);
    /* XXX TODO- translate xtalk addresses in palenlist
     *    into GIO addresses using giobr_dma_for_xtalk.
     */
    return palenlist;
}

/* =====================================================================
 *            INTERRUPT MANAGEMENT
 */
/* ARGSUSED */
giobr_intr_t
giobr_intr_alloc(vertex_hdl_t gconn,
		 device_desc_t dev_desc,
		 gioio_intr_line_t int_line,
		 vertex_hdl_t owner_dev)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    vertex_hdl_t            xconn = giobr_soft->gs_xconn;
    giobr_intr_t            giobr_intr;
    xtalk_intr_t            xtalk_intr;
    struct gs_intr_s       *bi = &giobr_soft->gs_intr[int_line];

    if (bi->bsi_xtalk_intr || bi->bsi_giobr_intr) {
	/* log errors ?XXX */
	printf("giobr_intr_alloc: line not free\n");
	return 0;
    }
    NEW(giobr_intr);
    if (!giobr_intr)
	return 0;

    giobr_intr->bi_flags = 0;
    giobr_intr->bi_dev = gconn;
    giobr_intr->bi_line = int_line;
    giobr_intr->bi_soft = giobr_soft;
    giobr_intr->bi_func = 0;		/* unset until connect */
    giobr_intr->bi_arg = 0;		/* unset until connect */

    /*
     * For each GIO interrupt line requested, figure
     * out which Bridge GIO Interrupt Line it maps
     * to, and make sure there are xtalk resources
     * allocated for it.
     */

    xtalk_intr = giobr_soft->gs_intr[int_line].bsi_xtalk_intr;
    if (xtalk_intr == NULL) {
	/*
	 * THIS xtalk_intr_alloc NEEDS TO BE
	 * CONSTRAINED: it needs to use the same
	 * xtalk target and low 38 bits of xtalk
	 * offset as the bridge error
	 * interrupt. It can differ in the upper
	 * 10 bits (selecting a different CPU in
	 * hub based systems), and should use a
	 * different vector number.
	 */
	xtalk_intr = xtalk_intr_alloc(xconn, 0, owner_dev);
	bi->bsi_xtalk_intr = xtalk_intr;
	bi->bsi_giobr_intr = giobr_intr;

	/*
	 * The calling device driver must ensure that correct
	 * intr-device mapping has been set to the bridge->b_int_device
	 * register throught the bridge config.
	 */

    } else {
	printf("giobr_intr_alloc: BRIDGE GIO INT BIT SHARING NOT ALLOWED\n");

	giobr_intr_free(giobr_intr);
	return 0;
    }

    return giobr_intr;
}

void
giobr_intr_free(giobr_intr_t giobr_intr)
{
    gioio_intr_line_t       int_line = giobr_intr->bi_line;
    giobr_soft_t            giobr_soft = giobr_intr->bi_soft;
    struct gs_intr_s       *bi = &giobr_soft->gs_intr[int_line];

    xtalk_intr_free(bi->bsi_xtalk_intr);
    bi->bsi_xtalk_intr = 0;
    bi->bsi_giobr_intr = 0;

    DEL(giobr_intr);
}

static void
giobr_setgioint(xtalk_intr_t xtalk_intr)
{
    iopaddr_t               addr = xtalk_intr_addr_get(xtalk_intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(xtalk_intr);

    bridgereg_t            *int_addr = (bridgereg_t *)
    xtalk_intr_sfarg_get(xtalk_intr);

    *int_addr = ((BRIDGE_INT_ADDR_HOST & (addr >> 30)) |
		 (BRIDGE_INT_ADDR_FLD & vect));
}

int
giobr_intr_connect(giobr_intr_t giobr_intr,
		   intr_func_t intr_func,
		   intr_arg_t intr_arg,
		   void *thread)
{
    giobr_soft_t            giobr_soft = giobr_intr->bi_soft;
    bridge_t               *bridge = giobr_soft->gs_base;
    gioio_intr_line_t       int_line = giobr_intr->bi_line;

    giobr_intr->bi_func = intr_func;
    giobr_intr->bi_arg = intr_arg;

    /*
     * For each GIO interrupt line requested, figure
     * out which Bridge GIO Interrupt Line it maps
     * to, and make sure there are xtalk resources
     * allocated for it.
     */

    xtalk_intr_connect(giobr_soft->gs_intr[int_line].bsi_xtalk_intr,
		       (intr_func_t) intr_func,
		       (intr_arg_t) intr_arg,
		       (xtalk_intr_setfunc_t) giobr_setgioint,
		       (void *) &(bridge->b_int_addr[int_line].addr),
		       thread);

    bridge->b_int_enable |= 1 << int_line;

    return 0;
}

void
giobr_intr_disconnect(giobr_intr_t giobr_intr)
{
    giobr_soft_t            giobr_soft = giobr_intr->bi_soft;
    bridge_t               *bridge = giobr_soft->gs_base;
    gioio_intr_line_t       int_line = giobr_intr->bi_line;
    bridgereg_t             giobr_int_bits = 1 << giobr_intr->bi_line;

    bridge->b_int_enable &= ~giobr_int_bits;

    xtalk_intr_disconnect(giobr_soft->gs_intr[int_line].bsi_xtalk_intr);

    giobr_intr->bi_func = 0;
    giobr_intr->bi_arg = 0;
}

vertex_hdl_t
giobr_intr_cpu_get(giobr_intr_t giobr_intr)
{
    giobr_soft_t            giobr_soft = giobr_intr->bi_soft;
    gioio_intr_line_t       int_line = giobr_intr->bi_line;

    return xtalk_intr_cpu_get(giobr_soft->gs_intr[int_line].bsi_xtalk_intr);
}
/* =====================================================================
 *            INTERRUPT HANDLING
 */
static void
giobr_setwidint(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);

    bridge_t               *bridge = (bridge_t *) xtalk_intr_sfarg_get(intr);

    bridge->b_wid_int_upper = ((0xFF000000 & (vect << 24)) |
			       (0x000F0000 & (targ << 16)) |
			       XTALK_ADDR_TO_UPPER(addr));
    bridge->b_wid_int_lower = XTALK_ADDR_TO_LOWER(addr);
    bridge->b_int_host_err = vect;
}


/* =====================================================================
 *            ERROR HANDLING
 *
 * Most of the error handling fcts are almost identical
 * to those in pcibr.c
 * Once pcibr.c src is segmented to bridge general and
 * pci specific stuff then it may become
 * more practical to use the common bridge fcts.
 */

/*
 * BRIDGE_IRR_CRP_GRP 		- Unexpected response or FIFO overflow
 * BRIDGE_IRR_RESP_BUF_GRP	- XIO response pkt related error
 * BRIDGE_IRR_REQ_DSP_GRP	- XIO request pkt related error
 * BRIDGE_IRR_SSRAM_GRP		- SSRAM parity or bad GIO addr
 * BRIDGE_IRR_GIO_GRP		- GIO bus problem
 */
#define GIOBR_ISR_ERROR_FATAL	( \
     BRIDGE_IRR_CRP_GRP 	| \
     BRIDGE_IRR_RESP_BUF_GRP	| \
     BRIDGE_IRR_REQ_DSP_GRP	| \
     BRIDGE_IRR_SSRAM_GRP	| \
     BRIDGE_IRR_GIO_GRP		)


#define GIOBR_ISR_ERR_START	8
#define GIOBR_ISR_MAX_ERRS	32

struct reg_desc bridge_ssram_err_desc[] = {
    {0x1L << 28,	0,	"SIZE_FAULT",  	NULL,	NULL},
    {0x7L << 25,	-25,	"GIO_DEV",  	"0x%x",	NULL},
    {0x1L << 24,	0,	"ACC_SRC",	NULL,	NULL},
    {0x1L << 23,	0,	"DS0",		NULL,	NULL},
    {0x1L << 22,	0,	"DS1",		NULL,	NULL},
    {0x1L << 21,	0,	"DS2",		NULL,	NULL},
    {0x1L << 20,	0,	"DS3",		NULL,	NULL},
    {0x1L << 19,	0,	"DS4",		NULL,	NULL},
    {0x1L << 18,	0,	"DS5",		NULL,	NULL},
    {0x1L << 17,	0,	"DS6",		NULL,	NULL},
    {0x1L << 16,	0,	"DS7",		NULL,	NULL},
    {0xffffL,		3,	"ADDR",		"0x%x", NULL},
    {0,0,NULL,NULL,NULL}
};

struct reg_desc bridge_pci_err_desc[] = {
    {0x1L << 52,	0,	"DEV_MASTER",	NULL,	NULL},
    {0x1L << 51,	0,	"VDEV",		NULL,	NULL},
    {0x7L << 48,	-48,	"DEV_NUM",	"0x%x",	NULL},
    {0xffffffffffffL,	-0,	"ADDR",		"0x%x",	NULL},
    {0,0,NULL,NULL,NULL}
};

struct reg_desc bridge_resp_buf_err_desc[] = {
    {0x7L << 52,	-52,	"DEV_NUM",      "0x%x",	NULL},
    {0xfL << 48,	-48,	"BUFF_NUM",     "0x%x",	NULL},
    {0xffffffffffffL,	-0,	"ADDR",         "0x%x",	NULL},
    {0,0,NULL,NULL,NULL}
};

static struct reg_values xio_cmd_pactyp[] =
{
    {0x0, "RdReq"},
    {0x1, "RdResp"},
    {0x2, "WrReqWithResp"},
    {0x3, "WrResp"},
    {0x4, "WrReqNoResp"},
    {0x5, "Reserved(5)"},
    {0x6, "FetchAndOp"},
    {0x7, "Reserved(7)"},
    {0x8, "StoreAndOp"},
    {0x9, "Reserved(9)"},
    {0xa, "Reserved(a)"},
    {0xb, "Reserved(b)"},
    {0xc, "Reserved(c)"},
    {0xd, "Reserved(d)"},
    {0xe, "SpecialReq"},
    {0xf, "SpecialResp"},
    {0}
};

static struct reg_desc  xio_cmd_bits[] =
{
    {WIDGET_DIDN, -28, "DIDN", "%x"},
    {WIDGET_SIDN, -24, "SIDN", "%x"},
    {WIDGET_PACTYP, -20, "PACTYP", 0, xio_cmd_pactyp},
    {WIDGET_TNUM, -15, "TNUM", "%x"},
    {WIDGET_COHERENT, 0, "COHERENT"},
    {WIDGET_DS, 0, "DS"},
    {WIDGET_GBR, 0, "GBR"},
    {WIDGET_VBPM, 0, "VBPM"},
    {WIDGET_ERROR, 0, "ERROR"},
    {WIDGET_BARRIER, 0, "BARRIER"},
    {0}
};

static void
print_bridge_errcmd(__uint32_t cmdword, char *errtype)
{
    cmn_err(CE_WARN,
	    "\t%serror command: %R",
	    errtype, cmdword, xio_cmd_bits);
}

static char            *giobr_isr_errs[] =
{
    "", "", "", "", "", "", "", "",
    "08: GIO non-contiguous byte enable in crosstalk packet",
    "09: BUS to Crosstalk read request timeout",
    "10: BUS retry operation count exhausted.",
    "11: GIO bus timeout",
    "12: BUS device reported parity error",
    "13: BUS Address/Cmd parity error ",
    "14: BUS Bridge detected parity error",
    "15: BUS abort condition",
    "16: SSRAM parity error",
    "17: LLP Transmitter Retry count wrapped",
    "18: LLP Transmitter side required Retry",
    "19: LLP Receiver retry count wrapped",
    "20: LLP Receiver check bit error",
    "21: LLP Receiver sequence number error",
    "22: Request packet overflow",
    "23: Request operation not supported by bridge",
    "24: Request packet has invalid address for bridge widget",
    "25: Incoming request xtalk command word error bit set or invalid sideband",
    "26: Incoming response xtalk command word error bit set or invalid sideband",
    "27: Framing error, request cmd data size does not match actual",
    "28: Framing error, response cmd data size does not match actual",
    "29: Unexpected response arrived",
    "30: Access to SSRAM beyond device limits",
    "31: Multiple errors occurred",
};

/*
 * GIO Bridge Error interrupt handling.
 * copied and slightly modified from pcibr_error_dump()
 */
/*ARGSUSED */
void
giobr_error_dump(giobr_soft_t giobr_soft)
{
    bridge_t               *bridge = giobr_soft->gs_base;
    bridgereg_t             int_status;
    int			    i;

    int_status = (bridge->b_int_status & ~BRIDGE_ISR_INT_MSK);

    cmn_err(CE_ALERT, "\t%v: product id: 0x%x",
	    giobr_soft->gs_vhdl, giobr_soft->gs_product);

    cmn_err(CE_ALERT, "GIO BRIDGE ERROR INTERRUPT: int_status 0x%x",
	    int_status);

    for (i = GIOBR_ISR_ERR_START; i < GIOBR_ISR_MAX_ERRS; i++) {
	if (int_status & (1 << i)) {
	    cmn_err(CE_WARN, "%s", giobr_isr_errs[i]);
	}
    }

    print_bridge_errcmd(bridge->b_wid_err_cmdword, "");

    cmn_err(CE_WARN,
	    "\twidget error address: 0x%x",
	    (((__uint64_t)bridge->b_wid_err_upper << 32) |
	     bridge->b_wid_err_lower));

    print_bridge_errcmd(bridge->b_wid_aux_err, "Aux ");

    cmn_err(CE_WARN,
	    "\tresponse buffer: %R",
	    (((__uint64_t)bridge->b_wid_resp_upper << 32) |
	     bridge->b_wid_resp_lower), bridge_resp_buf_err_desc);

    if (int_status & BRIDGE_IRR_SSRAM_GRP)
	cmn_err(CE_WARN,
		"\tSSRAM error: %R",
		bridge->b_ram_perr, bridge_ssram_err_desc);

    cmn_err(CE_WARN,
	    "\tGIO error address: %R",
	    (((__uint64_t)bridge->b_pci_err_upper << 32) |
	     bridge->b_pci_err_lower),
	    bridge_pci_err_desc);

    if (int_status & GIOBR_ISR_ERROR_FATAL) {
	    machine_error_dump("");
	    cmn_err_tag(15, CE_PANIC, "GIO Bridge Error interrupt killed the system");
	    /*NOTREACHED */
    } else {
	cmn_err(CE_ALERT, "Non-fatal Error in Bridge..");
    }
}

static bridgereg_t
giobr_errintr_group(bridgereg_t error)
{
    bridgereg_t              group = BRIDGE_IRR_MULTI_CLR;

    if (error & BRIDGE_IRR_PCI_GRP)
        group |= BRIDGE_IRR_PCI_GRP_CLR;
    if (error & BRIDGE_IRR_SSRAM_GRP)
        group |= BRIDGE_IRR_SSRAM_GRP_CLR;
    if (error & BRIDGE_IRR_LLP_GRP)
        group |= BRIDGE_IRR_LLP_GRP_CLR;
    if (error & BRIDGE_IRR_REQ_DSP_GRP)
        group |= BRIDGE_IRR_REQ_DSP_GRP_CLR;
    if (error & BRIDGE_IRR_RESP_BUF_GRP)
        group |= BRIDGE_IRR_RESP_BUF_GRP_CLR;
    if (error & BRIDGE_IRR_CRP_GRP)
        group |= BRIDGE_IRR_CRP_GRP_CLR;

    return group;

}

/*
 * GIO Bridge Error interrupt handling.
 * This routine gets invoked from system interrupt dispatcher
 * and is responsible for invoking appropriate error handler,
 * depending on the type of error.
 * This IS a duplicate of bridge_errintr defined specfic to IP30.
 * There are some minor differences in terms of the return value and
 * parameters passed. One of these two should be removed at some point
 * of time.
 */
/*ARGSUSED */
void
giobr_error_intr_handler(intr_arg_t arg)
{
    /* REFERENCED */
    graph_error_t           rc;
    giobr_soft_t            giobr_soft = (giobr_soft_t) arg;
    bridge_t               *bridge = giobr_soft->gs_base;
    bridgereg_t             int_status;

    int_status = bridge->b_int_status;

    DPRINTF(("giobr_error_intr_handler: 0x%x\n", int_status));
    /* Handle individual groups of interrupts separately. */
    if (int_status & GIOBR_ISR_ERROR_FATAL) {
	giobr_error_dump(giobr_soft);
	/* we will panic in dump fct */
    }
    if (int_status & BRIDGE_IRR_LLP_GRP) {
	/* LLP Related error.
	 * Good chance that this is fatal!!.
	 */
	giobr_soft->gs_llp_errcnt++;
    }

    bridge->b_int_rst_stat = giobr_errintr_group(int_status);
    (void) bridge->b_wid_tflush;	/* flushbus */
    return;
}

static void
giobr_error_cleanup(giobr_soft_t giobr_soft, int error_code)
{
    bridge_t               *bridge = giobr_soft->gs_base;

    DPRINTF(("giobr_error_cleanup: %v\n", giobr_soft->gs_xconn));
    ASSERT(error_code & IOECODE_PIO);
    error_code = error_code;

    DPRINTF(("  b_int_status: %x\n", bridge->b_int_status));
    bridge->b_int_rst_stat =
	(BRIDGE_IRR_GIO_GRP_CLR | BRIDGE_IRR_MULTI_CLR);
    (void) bridge->b_wid_tflush;	/* flushbus */
}

/*ARGSUSED */
void
giobr_device_disable(giobr_soft_t giobr_soft, int devnum)
{
    /*
     * XXX
     * Device failed to handle error. Take steps to
     * disable this device ? HOW TO DO IT ?
     *
     * If there are any Read response buffers associated
     * with this device, it's time to get them back!!
     *
     * We can disassociate any interrupt level associated
     * with this device, and disable that interrupt level
     *
     * For now it's just a place holder
     */
}

/* ARGSUSED */
static int
giobr_error_refine(
		      vertex_hdl_t gioio_vhdl,
		      int error_code,
		      ioerror_mode_t mode,
		      ioerror_t *ioerror)
{
    gioio_info_t            gioio_info;
    vertex_hdl_t            gconn;
    gioio_slot_t            slot;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: giobr_error_refine\n", gioio_vhdl);
#endif
    DPRINTF(("giobr_error_refine ecode(0x%x) mode(0x%x)\n", error_code, mode));

    if (!IOERROR_FIELDVALID(ioerror, widgetdev)) {
	/* XXX- calculate slot based on xtalkaddr
	 * and the state of the giobr. For now,
	 * we just assume slot zero ... but this
	 * could be wrong!
	 */
	slot = 0;
	IOERROR_SETVALUE(ioerror, widgetdev, slot);
    } else
	slot = IOERROR_GETVALUE(ioerror, widgetdev);

    gioio_info = gioio_cardinfo_get(gioio_vhdl, slot, &gconn);

    IOERR_PRINTF(cmn_err(CE_NOTE,
			 "GIO Bus Error: %v Error code: %d Error mode: %d\n",
			 gioio_vhdl, error_code, mode));

    IOERR_PRINTF(cmn_err(CE_NOTE,
			 "!Error GIO Device Product ID: 0x%x : Bus Slot: %d\n",
			 gioio_info_product_id_get(gioio_info),
			 gioio_info_slot_get(gioio_info)));

    /* If the gio device has registered a handler
     * at this connect point, turn control over to it.
     * IOERROR_HANDLED means we have nothing left to do.
     */
    if (IOERROR_HANDLED ==
	gioio_error_handler(gconn, error_code, mode, ioerror))
	return IOERROR_HANDLED;

    /* no handler for that GIO device, or the handler
     * was not able to recover.
     */

    cmn_err(CE_WARN,
	    "%v: GIO Bus Access error\n"
	    "GIO Device Product ID: 0x%x : Bus Slot: %d\n",
	    gconn,
	    gioio_info_product_id_get(gioio_info),
	    gioio_info_slot_get(gioio_info));

    ioerror_dump("gioio", error_code, mode, ioerror);

    return IOERROR_HANDLED;
}

/*
 * giobr_pioerror
 *      Handle PIO error that happened at the bridge pointed by giobr_soft.
 *
 *      Queries the Bus interface attached to see if the device driver
 *      mapping the device-number that caused error can handle the
 *      situation. If so, it will clean up any error, and return
 *      indicating the error was handled. If the device driver is unable
 *      to handle the error, it expects the bus-interface to disable that
 *      device, and takes any steps needed here to take away any resources
 *      associated with this device.
 */
/*ARGSUSED */
static int
giobr_pioerror(
		  giobr_soft_t giobr_soft,
		  int error_code,
		  ioerror_mode_t mode,
		  ioerror_t *ioerror)
{
    bridge_t               *brbase = giobr_soft->gs_base;
    bridgereg_t             bus_lowaddr, bus_uppraddr;
    caddr_t                 bad_vaddr;
    bridgereg_t             int_status;
    int                     retval = IOERROR_HANDLED;

    DPRINTF(("giobr_pioerror: %v\n", giobr_soft->gs_xconn));
    DPRINTF(("   mode 0x%x\n", mode));
    /*
     * In case of PIO read errors, bridge should have logged the
     * address that caused the error.
     * Look up the address, in the bridge error registers, and
     * take appropriate action
     */
    ASSERT(IOERROR_GETVALUE(ioerror, widgetnum) == giobr_soft->gs_xid);

    if (mode == MODE_DEVPROBE) {
	/*
	 * During probing, we don't really care what the
	 * error is. Clean up the error, and return success
	 */
	giobr_error_cleanup(giobr_soft, error_code);
	/*
	 * unqueue the timeout function queued by the error interrupt
	 * handler
	 * XXX implement timeout/untimeout
	 */
	return retval;
    }
    /*
     *  check if the address that we got an error on,
     * is bridge address. If so, let's not cause further problem.
     */
    if (IOERROR_FIELDVALID(ioerror, vaddr)) {
	bad_vaddr = IOERROR_GETVALUE(ioerror, vaddr);
	if ((bad_vaddr >= (caddr_t) brbase) &&
	    (bad_vaddr < ((caddr_t) brbase + sizeof(bridge_t))))
				    return IOERROR_XTALKLEVEL;
    }
    int_status = brbase->b_int_status;
    /*
     * check for GIO BUS PIO ERR XXX
     */

    /*
     * read error log registers
     */
    bus_lowaddr = brbase->b_gio_err_lower;
    bus_uppraddr = brbase->b_gio_err_upper;

    IOERROR_SETVALUE(ioerror, widgetdev, BRIDGE_ERRUPPR_DEVICE(bus_uppraddr));
    IOERROR_SETVALUE(ioerror, busaddr,
		     (bus_lowaddr |
		      ((iopaddr_t)
		       (bus_uppraddr &
			BRIDGE_ERRUPPR_ADDRMASK) << 32)));

    /* Pass the error onward to the GIO driver,
     * if appropriate, or possibly take action
     * generic to GIO errors.
     */
    retval = giobr_error_refine
	(giobr_soft->gs_xconn, error_code, mode, ioerror);

    if (retval != IOERROR_HANDLED)
	giobr_device_disable(giobr_soft, IOERROR_GETVALUE(ioerror, widgetdev));

    /*
     * CAUTION: Resetting bit BRIDGE_IRR_GIO_GRP_CLR, acknowledges
     *      a group of interrupts. If while handling this error, some
     *      other bus error has occured, that would be implicitly cleared
     *      by this write. This should not happen.
     *      Next ASSERT makes sure that no other error has occured.
     */
    int_status = brbase->b_int_status;
    ASSERT((int_status & BRIDGE_IRR_PCI_GRP) == 0);
    int_status = int_status;
    brbase->b_int_rst_stat = BRIDGE_IRR_GIO_GRP_CLR;

    return retval;
}

/*
 * bridge_dmaerror
 *      Some error was identified in a DMA transaction.
 *      This routine will identify the <device, address> that caused the error,
 *      and try to invoke the appropriate bus service to handle this.
 */

#define	BRIDGE_DMA_READ_ERROR (BRIDGE_ISR_RESP_XTLK_ERR|BRIDGE_ISR_XREAD_REQ_TIMEOUT)

static int
giobr_dmard_error(
		     giobr_soft_t giobr_soft,
		     int error_code,
		     ioerror_mode_t mode,
		     ioerror_t *ioerror)
{
    bridge_t               *brbase = giobr_soft->gs_base;
    bridgereg_t             bus_lowaddr, bus_uppraddr;
    bridgereg_t             int_status;
    int                     retval = 0;
    int                     bufnum;

    DPRINTF(("giobr_dmard_error: %v\n", giobr_soft->gs_xconn));
    /*
     * In case of DMA errors, bridge should have logged the
     * address that caused the error.
     * Look up the address, in the bridge error registers, and
     * take appropriate action
     */
    ASSERT(IOERROR_GETVALUE(ioerror, widgetnum) == giobr_soft->gs_xid);
    ASSERT(brbase);

    /*
     * Make sure this's a GIO ABORT situation
     */
    int_status = brbase->b_int_status;

    if (!(int_status & BRIDGE_DMA_READ_ERROR)) {
	/*
	 * Huh, GIO ABORT is not set for PIO Read errors.
	 * Print an error message and return can't handle.
	 * Could be the case of multiple errors. We will
	 * handle that later.
	 */
	cmn_err(CE_WARN,
		"giobr_dmard_error error-code %d Expected Int status %x got 0x%x",
		error_code, BRIDGE_DMA_READ_ERROR, int_status);
	return IOERROR_WIDGETLEVEL;
    }
    /*
     * read error log registers
     */
    bus_lowaddr = brbase->b_wid_resp_lower;
    bus_uppraddr = brbase->b_wid_resp_upper;

    bufnum = BRIDGE_RESP_ERRUPPR_BUFNUM(bus_uppraddr);
    IOERROR_SETVALUE(ioerror, widgetdev, BRIDGE_RESP_ERRUPPR_DEVICE(bus_uppraddr));
    IOERROR_SETVALUE(ioerror, busaddr,
		     (bus_lowaddr |
		      ((iopaddr_t)
		       (bus_uppraddr &
			BRIDGE_ERRUPPR_ADDRMASK) << 32)));

    /*
     * need to ensure that the xtalk adress in ioerror
     * maps to GIO error address read from bridge.
     * How to convert GIO address back to Xtalk address ?
     */

    /*
     * Invoke GIO bus error handling mechanism.
     */
    retval = giobr_error_refine(giobr_soft->gs_xconn, error_code, mode, ioerror);
    if (retval != IOERROR_HANDLED)
	giobr_device_disable(giobr_soft, IOERROR_GETVALUE(ioerror, widgetdev));

    /*
     * Re-enable bridge to interrupt on BRIDGE_IRR_RESP_BUF_GRP_CLR
     * NOTE: Wheather we get the interrupt on BRIDGE_IRR_RESP_BUF_GRP_CLR or
     * not is dependent on INT_ENABLE register. This write just makes sure
     * that if the interrupt was enabled, we do get the interrupt.
     */
    brbase->b_int_rst_stat = BRIDGE_IRR_RESP_BUF_GRP_CLR;

    /*
     * Also, release the "bufnum" back to buffer pool that could be re-used.
     * This is done by "disabling" the buffer.
     *
     * XXX Integrate following freeing of Read response buffers
     * with the alloc/free_rrb routines above.
     * XXX We need a centralized scheme for managing RRBs.
     */

    if (bufnum & 1)
	brbase->b_odd_resp &= ~(0xF << ((bufnum >> 1) * 4));
    else
	brbase->b_even_resp &= ~(0xF << ((bufnum >> 1) * 4));

    return retval;
}

/*
 * giobr_dmawr_error:
 *      Handle a dma write error caused by a device attached to this bridge.
 *
 *      ioerror has the widgetnum, widgetdev, and memaddr fields updated
 *      But we don't know the GIO address that corresponds to "memaddr"
 *      nor do we know which device driver is generating this address.
 *
 *      There is no easy way to find out the GIO address(es) that map
 *      to a specific system memory address. Bus handling code is also
 *      of not much help, since they don't keep track of the DMA mapping
 *      that have been handed out.
 *      So it's a dead-end at this time.
 *
 *      If translation is available, we could invoke the error handling
 *      interface of the device driver.
 */
/*ARGSUSED */
static int
giobr_dmawr_error(
		     giobr_soft_t giobr_soft,
		     int error_code,
		     ioerror_mode_t mode,
		     ioerror_t *ioerror)
{
    int                     retval;

    DPRINTF(("giobr_dmawr_error: %v\n", giobr_soft->gs_xconn));
    retval = giobr_error_refine
	(giobr_soft->gs_xconn, error_code, mode, ioerror);

    if (retval != IOERROR_HANDLED) {
	giobr_device_disable(giobr_soft, IOERROR_GETVALUE(ioerror, widgetdev));
    }
    return retval;
}

/*
 * Bridge error handler.
 *      Interface to handle all errors that involve bridge in some way.
 *
 *      This normally gets called from xtalk error handler,
 *      via the indirection mechanism.
 *      ioerror has different set of fields set depending on the error that
 *      was encountered. So, we have a bit field indicating which of the
 *      fields are valid.
 *
 * NOTE: This routine could be operating in interrupt context. So,
 *      don't try to sleep here (till interrupt threads work!!)
 */
static int
giobr_error_handler(
		       void *einfo,
		       int error_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerror)
{
    giobr_soft_t            giobr_soft;
    int                     retval = IOERROR_BADERRORCODE;

    giobr_soft = einfo;
    ASSERT(giobr_soft != NULL);

    DPRINTF(("giobr_error_handler: %v\n", giobr_soft->gs_xconn));
    DPRINTF(("  error_code 0x%x\n", error_code));

    if (error_code & IOECODE_PIO)
	retval = giobr_pioerror(giobr_soft, error_code, mode, ioerror);

    if (error_code & IOECODE_DMA) {
	if (error_code & IOECODE_READ) {
	    /*
	     * DMA read error occurs when a device attached to the bridge
	     * tries to read some data from system memory, and this
	     * either results in a timeout or access error.
	     * First case is indicated by the bit "XREAD_REQ_TOUT"
	     * and second case by "RESP_XTALK_ERROR" bit in bridge error
	     * interrupt status register.
	     *
	     * giobr_error_intr_handler would get invoked first, and it has
	     * the responsibility of calling giobr_error_handler with
	     * suitable parameters.
	     */

	    retval = giobr_dmard_error(giobr_soft, error_code, MODE_DEVERROR, ioerror);
	}
	if (error_code & IOECODE_WRITE) {
	    /*
	     * A device attached to this bridge has been generating
	     * bad DMA writes. Find out the device attached, and
	     * slap on it's wrist.
	     */

	    retval = giobr_dmawr_error(giobr_soft, error_code, MODE_DEVERROR, ioerror);
	}
    }
    return retval;
}
/*
 * Reenable a device after handling the error.
 * This is called by the lower layers when they wish to be reenabled
 * after an error.
 * Note that each layer would be calling the previous layer to reenable
 * first, before going ahead with their own re-enabling.
 */

int
giobr_error_devenable(vertex_hdl_t gconn_vhdl, int error_code)
{
    gioio_info_t            gioio_info = gioio_info_get(gconn_vhdl);
    gioio_slot_t            gioio_slot = gioio_info_slot_get(gioio_info);
    giobr_soft_t            giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);
    int                     rc;

    ASSERT(error_code & IOECODE_PIO);
    DPRINTF(("giobr_error_devenable: %v\n", gconn_vhdl));

    /* If the error is not known to be a write,
     * we have to call devenable.
     * write errors are isolated to the bridge.
     */
    if (!(error_code & IOECODE_WRITE)) {
	rc = xtalk_error_devenable(giobr_soft->gs_xconn,
				   gioio_slot, error_code);
	if (rc != IOERROR_HANDLED)
	    return rc;
    }
    giobr_error_cleanup(giobr_soft, error_code);
    return IOERROR_HANDLED;
}

/* =====================================================================
 *            CONFIGURATION MANAGEMENT
 */
/*ARGSUSED */
void
giobr_provider_startup(vertex_hdl_t giobr)
{
}

/*ARGSUSED */
void
giobr_provider_shutdown(vertex_hdl_t giobr)
{
}

/*ARGSUSED */
gioio_priority_t
giobr_priority_set(vertex_hdl_t dev,
		   gioio_priority_t device_prio)
{
    return device_prio;
}

gioio_provider_t        giobr_provider =
{
    (gioio_piomap_alloc_f *) giobr_piomap_alloc,
    (gioio_piomap_free_f *) giobr_piomap_free,
    (gioio_piomap_addr_f *) giobr_piomap_addr,
    (gioio_piomap_done_f *) giobr_piomap_done,
    (gioio_piotrans_addr_f *) giobr_piotrans_addr,

    (gioio_dmamap_alloc_f *) giobr_dmamap_alloc,
    (gioio_dmamap_free_f *) giobr_dmamap_free,
    (gioio_dmamap_addr_f *) giobr_dmamap_addr,
    (gioio_dmamap_list_f *) giobr_dmamap_list,
    (gioio_dmamap_done_f *) giobr_dmamap_done,
    (gioio_dmatrans_addr_f *) giobr_dmatrans_addr,
    (gioio_dmatrans_list_f *) giobr_dmatrans_list,

    (gioio_intr_alloc_f *) giobr_intr_alloc,
    (gioio_intr_free_f *) giobr_intr_free,
    (gioio_intr_connect_f *) giobr_intr_connect,
    (gioio_intr_disconnect_f *) giobr_intr_disconnect,
    (gioio_intr_cpu_get_f *) giobr_intr_cpu_get,

    (gioio_provider_startup_f *) giobr_provider_startup,
    (gioio_provider_shutdown_f *) giobr_provider_shutdown,
    (gioio_priority_set_f *) giobr_priority_set,

};

/*
 * Set up bridge registers for a device driver, given the driver's
 * connection vertex ( the one passed to xxx_attach() and the
 * address of the driver's gioio_bridge_config struct.
 */
void
giobr_bridge_config(vertex_hdl_t gconn, giobr_bridge_config_t bridge_config)
{
    gioio_info_t	gioio_info = gioio_info_get(gconn);
    giobr_soft_t	giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);

    ASSERT(bridge_config != NULL);
    if ( showconfig )
	cmn_err(CE_DEBUG, "%v: Setting GIO bridge configuration\n", gconn);

    do_giobr_bridge_config(giobr_soft->gs_base, bridge_config);
}

static void
do_giobr_bridge_config(bridge_t *bridge, giobr_bridge_config_t bridge_config)
{
    /*REFERENCED*/
    int                     rv;
    int                     n;
    bridgereg_t             intr_device = 0;

    /*
     * RRB's are initialized to all zeros
     */
    bridge->b_even_resp = 0x0;
    bridge->b_odd_resp = 0x0;

    for (n = 0; n < 8; n++) {
	/*
	 * The 8 device register are used to completely map all of
	 * the 3 gio "device" PIO space. Both have 10 MB of Map,
	 * where 8 device register maps sizes are 2,2,1,1,1,1,1,1 MBs
	 * for bridge devices 0 ~ 7
	 * and the 3 gio GIO dev PIO maps are 4,2,4 MBs
	 * for GFX, GIO1, and GIO2 (1f5 'hack' is a special within GIO1).   
	 * Device drivers should use the default mapping and should NOT
	 * change the DEV_OFF fields in the device conf registers.
	 */
	if (bridge_config->mask & GIOBR_DEV_ATTRS) {
	    bridgereg_t devx;
	    /*
	     * user should not be specifying DEV_OFF fields, but
	     * the call from giobr_attach will setup the defaults,
	     * preserve the configured default values in the register
	     */
	    devx = bridge_config->device_attrs[n] & BRIDGE_DEV_OFF_MASK;
	    if (devx == 0)
		devx = bridge->b_device[n].reg & BRIDGE_DEV_OFF_MASK;
	    devx |= GIOBR_DEV_CONFIG(bridge_config->device_attrs[n]);
	    bridge->b_device[n].reg = devx;
	}
	/*
	 * setup the RRB's, assumed to have been all cleared
	 */
	if (bridge_config->mask & GIOBR_RRB_MAP) {
	    rv = giobr_rrb_alloc(bridge, n, bridge_config->rrb[n]);
	    ASSERT(rv == 0);
	}
	if (bridge_config->mask & GIOBR_INTR_DEV) {
	    intr_device &= ~BRIDGE_INT_DEV_MASK(n);
	    intr_device |= BRIDGE_INT_DEV_SET(bridge_config->intr_devices[n], n);
	}
    }

    if (bridge_config->mask & GIOBR_ARB_FLAGS) {
	bridge->b_arb |= bridge_config->arb_flags;
    }
    if (bridge_config->mask & GIOBR_INTR_DEV) {
	bridge->b_int_device = intr_device;
    }
}

/*
 * assert reset on the GIO-Bus for approx 8 usecs
 */
static void
do_giobr_bus_reset(bridge_t *bridge)
{
    /* bring bridge reset pins low */
    bridge->b_wid_control &= ~BRIDGE_CTRL_RST_MASK;
    bridge->b_wid_control;		/* inval addr bug war */

    if ( showconfig )
	cmn_err(CE_DEBUG, "GIO-Bridge bus reset asserted for widget %d\n",
		bridge->b_wid_control & BRIDGE_CTRL_WIDGET_ID_MASK);

    /* assert the bus reset for ~8usecs, actually ~10*/
    DELAY(8);

    /* take the gio out of reset */
    bridge->b_wid_control |= BRIDGE_CTRL_RST_MASK;
    bridge->b_wid_control;		/* inval addr bug war */

    /* wait 100 usecs for the devices to come out of reset */
    DELAY(100);

}

void
giobr_bus_reset(vertex_hdl_t gconn)
{
    gioio_info_t	gioio_info = gioio_info_get(gconn);
    giobr_soft_t	giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);

    do_giobr_bus_reset(giobr_soft->gs_base);
}

/*
 * assert (warm) link-reset on the bridge
 */
static void
do_giobr_link_reset(bridge_t *bridge, giobr_soft_t gs)
{
    bridgereg_t		control = bridge->b_wid_control;
    bridgereg_t		llp = bridge->b_wid_llp;

    xwidget_reset(gs->gs_xconn);
    if ( showconfig )
	cmn_err(CE_DEBUG, "XIO link reset for %v\n", gs->gs_xconn);

    /* wait 100 usecs for the devices to come out of reset */
    DELAY(100);

    /* restore saved registers */
    bridge->b_wid_control = control;
    bridge->b_wid_llp = llp;
    do_giobr_hw_init(bridge, gs, 0);
}

void
giobr_link_reset(vertex_hdl_t gconn)
{
    gioio_info_t	gioio_info = gioio_info_get(gconn);
    giobr_soft_t	giobr_soft = (giobr_soft_t) gioio_info_mfast_get(gioio_info);

    do_giobr_link_reset(giobr_soft->gs_base, giobr_soft);
}
