/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.44 $"

#ifdef IP32

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/schedctl.h>
#include <sys/edt.h>
#include <sys/mace.h>
#include <sys/invent.h>
#include <sys/ddi.h>
#include <sys/mkdev.h>			/* NBITSMINOR */
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/pcimh.h>
#include <sys/PCI/pciio_private.h>
#include <sys/driver.h>
#include <sys/IP32.h>

/* =====================================================================
 *            DEBUG macros of various sorts
 */

#if DEBUG
#define DEBUG_PCIMH		1
#endif

#ifndef DEBUG_PCIMH
#define DEBUG_PCIMH		0
#endif

#ifndef DEBUG_PCIMH_REG
#define DEBUG_PCIMH_REG		0
#endif

#ifndef DEBUG_PCIMH_PPB
#define DEBUG_PCIMH_PPB		0
#endif

#ifndef	LOCAL
#define	LOCAL			static
#endif

/* =====================================================================
 *            Exported globals
 */
int                     pcimh_devflag = D_MP;
int                     pcimh_verbose = 0;

/* =====================================================================
 *            Imported globals
 */
extern int              enable_pci_BIST;
extern int		ip32_pio_predelay;   /* from mtune/kernel */
extern int		ip32_pio_postdelay;  /* from mtune/kernel */
extern int 		peer_peer_dma;	     /* from mtune */

/* =====================================================================
 *            Imported functions
 */
extern void             pciio_init(void);

/* =====================================================================
 *            Utility Macros
 */

#define CONFIG_PTR(bus, slot, func)					\
	(char *)(0x80000000 |						\
		 (bus << CFG1_BUS_SHIFT) |				\
		 (slot << CFG1_DEVICE_SHIFT) |				\
		 (func << CFG1_FUNCTION_SHIFT))

#define	NEWAf(ptr,n,f)	(ptr = kmem_zalloc((n)*sizeof (*(ptr)), (f&PCIIO_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP))
#define	NEWA(ptr,n)	(ptr = kmem_zalloc((n)*sizeof (*(ptr)), KM_SLEEP))
#define	DELA(ptr,n)	(kmem_free(ptr, (n)*sizeof (*(ptr))))

#define	NEWf(ptr,f)	NEWAf(ptr,1,f)
#define	NEW(ptr)	NEWA(ptr,1)
#define	DEL(ptr)	DELA(ptr,1)

#define	FOR_B		for (bus = 0; bus <= maxbus; bus++)
#define	FOR_S		for (slot = FIRST_DEV_NUM(bus); slot <= LAST_DEV_NUM(bus); slot++)
#define	FOR_F		for (func = 0; func < MAX_FUNCS; func++)
#define	FOR_W		for (win = 0; win < NUM_BASE_REGS; ++win)

#define	BAR_MASK	(info->f_winmap & (1 << win))
#define	FOR_BARS	FOR_W if (BAR_MASK)

#define	LINKED_DEV	for (info = dev_list; info; info = info->f_next)

#define	SPACE_NAMES	(sizeof _space_name / sizeof _space_name[0])
#define	SPACE_NAME(n)	((((unsigned)(n)) < SPACE_NAMES) ? _space_name[(unsigned)(n)] : "UNK")

#define P_SPACE 	(info->f_winaddr[win].space)
#define P_ADDR  	(info->f_winaddr[win].p_addr)
#define P_SIZE  	(info->f_winaddr[win].size)

#define	pcimh_info_get(vhdl)	((pcimh_info_t)device_info_get(vhdl))

#define	pcimh_config_lock()	spl7()
#define	pcimh_config_free(s)	splx(s)

/* ================================================================
 *            Manifest Constants
 */

/* Attributes of the PCI bus */
#define	INTRS_PER_SLOT	4
#define MAX_FUNCS	8
#define NUM_BASE_REGS	6

/* Number of "user spaces" we track.
 * This can be adjusted at will.
 */
#define NUM_USER_SPACES 6

/* Limit PCI scanning to ranges that
 * we know may exist.
 */
#define	FIRST_MH_SLOT	1
#define	LAST_MH_SLOT	5		/* Yes, support RoadRunner */
#define	MH_SLOTS	(LAST_MH_SLOT-FIRST_MH_SLOT+1)

#define	FIRST_PPB_SLOT	0		/* XXX Bit3 chassis specific */
#define	LAST_PPB_SLOT	32		/* XXX Bit3 chassis specific */

#define FIRST_DEV_NUM(bus)	(bus ? FIRST_PPB_SLOT : FIRST_MH_SLOT)
#define LAST_DEV_NUM(bus)	(bus ? LAST_PPB_SLOT : LAST_MH_SLOT)

/* Allocation starting points for PCI I/O and MEM spaces */
#define	INITIAL_ISPACE	0x00000004
#define	INITIAL_MSPACE	0x80000000

/* MACE-related constants */
#define PCI_CONFIG_ADDR_PTR     ((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CONFIG_DATA_PTR     ((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_DATA)))

/* ================================================================
 *            File-private types
 */

struct pci_addr_s {
    pciio_space_t           space;		/* PCIIO_SPACE_{IO,MEM} */
    size_t                  size;		/* requested size, bytes */
    iopaddr_t               p_addr;		/* PCI view of space */
    caddr_t                 v_addr;		/* CPU view of space */
};
typedef struct pci_addr_s pci_addr_t;

/* Per-CONN data including PCIMH specific information.
 */
typedef struct pcimh_info_s *pcimh_info_t, **pcimh_info_h;

struct pcimh_info_s {
    struct pciio_info_s     f_c;		/* MUST BE FIRST. */
#define	f_vertex	f_c.c_vertex		/* back pointer to vertex */
#define	f_bus		f_c.c_bus		/* which bus the card is in */
#define	f_slot		f_c.c_slot		/* which slot the card is in */
#define	f_func		f_c.c_func		/* which func (on multi-func cards) */
#define	f_vendor	f_c.c_vendor		/* PCI card "vendor" code */
#define	f_device	f_c.c_device		/* PCI card "device" code */
#define	f_master	f_c.c_master		/* PCI bus provider */
#define	f_mfast		f_c.c_mfast		/* cached fastinfo from c_master */
#define	f_pops		f_c.c_pops		/* cached provider from c_master */
#define	f_efunc		f_c.c_efunc		/* error handling func */
#define	f_einfo		f_c.c_einfo		/* first parameter for efunc */
#define	f_window	f_c.c_window		/* state of BASE regs */
#define	f_rbase		f_c.c_rbase		/* expansion rom base */
#define	f_rsize		f_c.c_rsize		/* expansion rom size */
#define	f_piospace	f_c.c_piospace		/* additional I/O spaces allocated */

    /* pcimh-specific connection state */
    caddr_t                 f_cfgaddr;		/* addr of config regs */
    pci_addr_t              f_winaddr[NUM_BASE_REGS + NUM_USER_SPACES];
    pci_addr_t              f_romaddr;
    unsigned                f_winmap;		/* bitmap of windows used */
    pciio_endian_t          f_endian;		/* desired endianness */

    pcimh_info_t            f_next;		/* next device */
    pcimh_info_t            f_host;		/* host PPB if any */
    int                     f_sec;		/* secondary bus number */
};

/* Per-INTR state, both PCIIO generic and PCIMH private
 */
struct pcimh_intr_s {
    struct pciio_intr_s     mi_pi;
#define	mi_flags	mi_pi.pi_flags		/* PCIIO_INTR flags */
#define	mi_dev		mi_pi.pi_dev		/* associated pci card */
#define	mi_dev_desc	mi_pi.pi_dev_desc	/* override descriptor */
#define	mi_lines	mi_pi.pi_lines		/* which PCI interrupt line(s) */
#define	mi_func		mi_pi.pi_func		/* handler function (when connected) */
#define	mi_arg		mi_pi.pi_arg		/* handler parameter (when connected) */
    unsigned                mi_mvecs;		/* bitmap of mace interrupts */
    int                     mi_level;		/* interrupt level */
};

/* =====================================================================
 *            File-local Global Variables
 */

/* PCI space allocation high-water marks */
LOCAL uint32_t          next_ispace = INITIAL_ISPACE;
LOCAL uint32_t          next_mspace = INITIAL_MSPACE;

/* Linked list (FIFO) of all DEV info structs */
LOCAL pcimh_info_t      dev_list = 0;
LOCAL pcimh_info_h      dev_last = &dev_list;

/* Mapping from PCI slot/intr to Mace intr */
LOCAL char              mace_ivec[MH_SLOTS][INTRS_PER_SLOT] =
{
    {MACE_PCI_SCSI0},
    {MACE_PCI_SCSI1},
    {MACE_PCI_SLOT0, MACE_PCI_SHARED0, MACE_PCI_SHARED1, MACE_PCI_SHARED2},
    {MACE_PCI_SLOT1, MACE_PCI_SHARED2, MACE_PCI_SHARED0, MACE_PCI_SHARED1},
    {MACE_PCI_SLOT2, MACE_PCI_SHARED1, MACE_PCI_SHARED2, MACE_PCI_SHARED0}
};

/* Translate PCIIO SPACE number into name string.
 * XXX should come from pciio, where it can be
 * shared with other pciio users.
 */
LOCAL char             *_space_name[] =
{
    "NIL", "ROM", "I/O", "(3)", "MEM", "M32", "M64", "CFG",
    "BASE0", "BASE1", "BASE2", "BASE3", "BASE4", "BASE5",
    "(E)", "BAD",
    "USER0", "USER1", "USER2", "USER3", "USER4", "USER5"
};

/* Decode structure for the Mace ERROR FLAGS word
 * to be used by printf "%r" and "%R" formats.
 */
LOCAL struct reg_desc   pci_err_bits[] =
{
    {PERR_MASTER_ABORT, 0, "MASTER_ABORT"},
    {PERR_TARGET_ABORT, 0, "TARGET_ABORT"},
    {PERR_DATA_PARITY_ERR, 0, "DATA_PARITY_ERR"},
    {PERR_RETRY_ERR, 0, "RETRY_ERR"},
    {PERR_ILLEGAL_CMD, 0, "ILLEGAL_CMD"},
    {PERR_SYSTEM_ERR, 0, "SYSTEM_ERR"},
    {PERR_INTERRUPT_TEST, 0, "INTERRUPT_TEST"},
    {PERR_PARITY_ERR, 0, "PARITY_ERR"},
    {PERR_OVERRUN, 0, "OVERRUN"},
#if 0
    /* don't decode these bits in %r */
    {PERR_RSVD, 0, "RSVD"},
    {PERR_MEMORY_ADDR, 0, "MEMORY_ADDR"},
    {PERR_CONFIG_ADDR, 0, "CONFIG_ADDR"},
    {PERR_MASTER_ABORT_ADDR_VALID, 0, "MASTER_ABORT_ADDR_VALID"},
    {PERR_TARGET_ABORT_ADDR_VALID, 0, "TARGET_ABORT_ADDR_VALID"},
    {PERR_DATA_PARITY_ADDR_VALID, 0, "DATA_PARITY_ADDR_VALID"},
    {PERR_RETRY_ADDR_VALID, 0, "RETRY_ADDR_VALID"},
#endif
    {0}
};

LOCAL int               maxbus = 0;		/* highest bus number in system */
LOCAL int		extended_slot = 8;	/* beginning slot number for
						   cards in expansion chassis */


LOCAL vertex_hdl_t      pcimh_vhdl;		/* providers vhdl */

LOCAL pciio_provider_t  pcimh_provider =
{
    (pciio_piomap_alloc_f *) pcimh_piomap_alloc,
    (pciio_piomap_free_f *) pcimh_piomap_free,
    (pciio_piomap_addr_f *) pcimh_piomap_addr,
    (pciio_piomap_done_f *) pcimh_piomap_done,
    (pciio_piotrans_addr_f *) pcimh_piotrans_addr,
    (pciio_piospace_alloc_f *) pcimh_piospace_alloc,
    (pciio_piospace_free_f *) pcimh_piospace_free,

    (pciio_dmamap_alloc_f *) pcimh_dmamap_alloc,
    (pciio_dmamap_free_f *) pcimh_dmamap_free,
    (pciio_dmamap_addr_f *) pcimh_dmamap_addr,
    (pciio_dmamap_list_f *) pcimh_dmamap_list,
    (pciio_dmamap_done_f *) pcimh_dmamap_done,
    (pciio_dmatrans_addr_f *) pcimh_dmatrans_addr,
    (pciio_dmatrans_list_f *) pcimh_dmatrans_list,
    (pciio_dmamap_drain_f *) pcimh_dmamap_drain,
    (pciio_dmaaddr_drain_f *) pcimh_dmaaddr_drain,
    (pciio_dmalist_drain_f *) pcimh_dmalist_drain,

    (pciio_intr_alloc_f *) pcimh_intr_alloc,
    (pciio_intr_free_f *) pcimh_intr_free,
    (pciio_intr_connect_f *) pcimh_intr_connect,
    (pciio_intr_disconnect_f *) pcimh_intr_disconnect,
    (pciio_intr_cpu_get_f *) pcimh_intr_cpu_get,

    (pciio_provider_startup_f *) pcimh_provider_startup,
    (pciio_provider_shutdown_f *) pcimh_provider_shutdown,
    (pciio_reset_f *) pcimh_reset,
    (pciio_write_gather_flush_f *) pcimh_write_gather_flush,
    (pciio_endian_set_f *) pcimh_endian_set,
    (pciio_priority_set_f *) pcimh_priority_set,
    (pciio_config_get_f *) pcimh_config_get,
    (pciio_config_set_f *) pcimh_config_set,
    (pciio_error_devenable_f *) pcimh_error_devenable,
    (pciio_error_extract_f *) pcimh_error_extract,
};


/* =====================================================================
 *            Function Table of Contents
 */

/* PCI startup code */
void                    pcimh_attach(vertex_hdl_t);
LOCAL void              pcimh_scan_bus(pcimh_info_t);
LOCAL void              pcimh_scan_subbus(pcimh_info_t);
LOCAL void              do_bist_for(pcimh_info_t);

/* PIO entry points */
pciio_piomap_t          pcimh_piomap_alloc(vertex_hdl_t, device_desc_t, pciio_space_t, iopaddr_t, size_t, size_t, unsigned);
void                    pcimh_piomap_free(pciio_piomap_t);
caddr_t                 pcimh_piomap_addr(pciio_piomap_t, iopaddr_t, size_t);
void                    pcimh_piomap_done(pciio_piomap_t);
caddr_t                 pcimh_piotrans_addr(vertex_hdl_t, device_desc_t, pciio_space_t, iopaddr_t, size_t, unsigned);
iopaddr_t               pcimh_piospace_alloc(vertex_hdl_t, device_desc_t, pciio_space_t, size_t, size_t);
void                    pcimh_piospace_free(vertex_hdl_t, pciio_space_t, iopaddr_t, size_t);

/* DMA entry points */
pciio_dmamap_t          pcimh_dmamap_alloc(vertex_hdl_t, device_desc_t, size_t, unsigned);
void                    pcimh_dmamap_free(pciio_dmamap_t);
iopaddr_t               pcimh_dmamap_addr(pciio_dmamap_t, paddr_t, size_t);
alenlist_t              pcimh_dmamap_list(pciio_dmamap_t, alenlist_t, unsigned);
void                    pcimh_dmamap_done(pciio_dmamap_t);
iopaddr_t               pcimh_dmatrans_addr(vertex_hdl_t, device_desc_t, paddr_t, size_t, unsigned);
alenlist_t              pcimh_dmatrans_list(vertex_hdl_t, device_desc_t, alenlist_t, unsigned);
void                    pcimh_dmamap_drain(pciio_dmamap_t);
void                    pcimh_dmaaddr_drain(vertex_hdl_t, paddr_t, size_t);
void                    pcimh_dmalist_drain(vertex_hdl_t, alenlist_t);
int                     pcimh_flush_buffers(vertex_hdl_t);
LOCAL iopaddr_t         pcimh_endian_apply(iopaddr_t, vertex_hdl_t, unsigned);

/* INTR entry points */
pcimh_intr_t            pcimh_intr_alloc(vertex_hdl_t, device_desc_t, pciio_intr_line_t, vertex_hdl_t);
void                    pcimh_intr_free(pcimh_intr_t);
int                     pcimh_intr_connect(pcimh_intr_t, intr_func_t, intr_arg_t, void *);
void                    pcimh_intr_disconnect(pcimh_intr_t);
vertex_hdl_t            pcimh_intr_cpu_get(pcimh_intr_t);
LOCAL unsigned          mace_ivecs(pcimh_info_t, unsigned);
LOCAL int               mace_level(vertex_hdl_t, device_desc_t);

/* MISC entry points */
void                    pcimh_provider_startup(vertex_hdl_t);
void                    pcimh_provider_shutdown(vertex_hdl_t);
int                     pcimh_reset(vertex_hdl_t);
int			pcimh_write_gather_flush(vertex_hdl_t);
pciio_endian_t          pcimh_endian_set(vertex_hdl_t, pciio_endian_t, pciio_endian_t);
pciio_priority_t        pcimh_priority_set(vertex_hdl_t, pciio_priority_t);
uint64_t                pcimh_config_get(vertex_hdl_t, unsigned, unsigned);
LOCAL uint64_t          pcimh_config_ptr_get(void *, unsigned, unsigned);
void                    pcimh_config_set(vertex_hdl_t, unsigned, unsigned, uint64_t);
LOCAL void              pcimh_config_ptr_set(void *, unsigned, unsigned, uint64_t);
LOCAL uint32_t          pcimh_config_ptr_setget(void *, unsigned, int, uint32_t);
int                     pcimh_error_devenable(vertex_hdl_t, int);
pciio_slot_t            pcimh_error_extract(vertex_hdl_t, pciio_space_t *, iopaddr_t *);

/* Error Handling */
LOCAL void              mace_pci_error(eframe_t *, __psint_t);

/* General Support */
int                     IP32_hinv_location(vertex_hdl_t);

/* =====================================================================
 *            Driver Startup
 */

void
pcimh_attach(vertex_hdl_t node_vhdl)
{
    pcimh_info_t            info;

    /*REFERENCED */
    graph_error_t           rc;

    /*
     * Initialize data structs and create the PCI vertex
     */
    rc = hwgraph_path_add(node_vhdl, EDGE_LBL_PCI, &pcimh_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    rc = hwgraph_char_device_add(pcimh_vhdl, EDGE_LBL_CONTROLLER,
				 "pcimh_", NULL);
    ASSERT(rc == GRAPH_SUCCESS);

    pciio_provider_register(pcimh_vhdl, &pcimh_provider);
    pciio_provider_startup(pcimh_vhdl);

    /* Send us error interrupts */
    setcrimevector(MACE_INTR(MACE_PCI_BRIDGE), SPLHINTR,
		   mace_pci_error, 0, 0);

    /* construct the "no-slot" connection point,
     * which we will also use as the "host bus"
     * info for the devices on Bus 0.
     */
    NEW(info);
    info->f_bus = 0;
    info->f_slot = PCIIO_SLOT_NONE;
    info->f_func = PCIIO_FUNC_NONE;
    info->f_vendor = PCIIO_VENDOR_ID_NONE;
    info->f_device = PCIIO_DEVICE_ID_NONE;
    info->f_cfgaddr = 0;
    info->f_master = pcimh_vhdl;
    info->f_mfast = hwgraph_fastinfo_get(pcimh_vhdl);
    info->f_pops = pciio_provider_fns_get(pcimh_vhdl);
    info->f_host = NULL;
    info->f_sec = 0;
    info->f_vertex =
	pciio_device_info_register(pcimh_vhdl, &info->f_c);

    *dev_last = info;
    dev_last = &info->f_next;

    /*
     * Scan the bus, and (recursively) all busses
     * connected to it via PPBs. Allocate PCI
     * address space for the devices, get them
     * set up, and trigger their device drivers.
     */
    pcimh_scan_bus(info);
}

LOCAL void
pcimh_scan_bus(pcimh_info_t binfo)
{
    int                     bus = binfo->f_sec;
    int                     slot, func;
    void                   *cfg_p;
    unsigned                vid;
    unsigned                did;
    unsigned                ht;
    pcimh_info_t            info;
    int                     win;
    unsigned                winoff;
    uint32_t                bits = 0;
    uint32_t                addr;
    uint32_t                size;
    uint32_t                mask;
    uint32_t                minmask = ctob(1) - 1;
    pciio_space_t           space;
    unsigned                pci_cmd;
    unsigned                ppb_map = 0;

#if !DEBUG_PCIMH
    if (showconfig)
#endif
	if (!bus)
	    cmn_err(CE_CONT, "\nPCI Configuration Report\n");

    FOR_S {
	FOR_F {
	    cfg_p = CONFIG_PTR(bus, slot, func);
	    vid = pcimh_config_ptr_get(cfg_p, PCI_CFG_VENDOR_ID, 2);

	    if ((vid == 0) || (vid == 0xFFFF)) {
		if (func)
		    continue;
		else
		    break;
	    }
	    did = pcimh_config_ptr_get(cfg_p, PCI_CFG_DEVICE_ID, 2);
	    ht = pcimh_config_ptr_get(cfg_p, PCI_CFG_HEADER_TYPE, 1);

	    /* If looking at function zero, and the
	     * multifunction bit is not set,
	     * use the FUNC_NONE code.
	     */
	    if (!func && !(ht & 0x80))
		func = PCIIO_FUNC_NONE;

	    if ((ht & 0x7F) == 0x01)
		ppb_map |= 1 << slot;
	    if ((ht & 0x7F) != 0x00)
		continue;

	    NEW(info);
	    info->f_bus = bus;

	    if (bus > 0)
		info->f_slot = extended_slot++;
	    else
		info->f_slot = slot;

	    info->f_func = func;
	    info->f_vendor = vid;
	    info->f_device = did;
	    info->f_cfgaddr = cfg_p;
	    info->f_master = binfo->f_master;
	    info->f_mfast = binfo->f_mfast;
	    info->f_pops = binfo->f_pops;
	    info->f_host = binfo;

	    info->f_vertex =
		pciio_device_info_register(pcimh_vhdl, &info->f_c);
	    info->f_slot = slot;

#if !DEBUG_PCIMH
	    if (showconfig)
#endif
		cmn_err(CE_CONT, "    %v is bus %d slot %d VENDOR 0x%x DEVICE 0x%x\n",
			info->f_vertex, info->f_bus, slot, vid, did);

	    if (enable_pci_BIST)
		do_bist_for(info);

	    *dev_last = info;
	    dev_last = &info->f_next;

	    FOR_W {
		winoff = PCI_CFG_BASE_ADDR(win);
		if (bits & PCI_BA_MEM_64BIT) { /* 64-bit */
			pcimh_config_ptr_set(cfg_p, winoff, 4, 0x0);
			bits = 0;
			continue;
		}
		bits = pcimh_config_ptr_setget(cfg_p, winoff, 4, 0xFFFFFFFF);

#if DEBUG_PCIMH_REG
		cmn_err(CE_CONT, "\tBASE%d raw value is 0x%x\n",
			win, bits);
#endif

		if ((bits == 0x00000000) ||
		    (bits == 0xFFFFFFFF))
		    break;

		info->f_winmap |= 1 << win;

		if (bits & 1) {		/* I/O */
		    addr = bits & 0xFFFFFFFC;
		    size = addr & -addr;
		    mask = size - 1;
		    if (mask < minmask)
			mask = minmask;	/* so mmap will work */
		    space = PCIIO_SPACE_IO;

		    next_ispace += mask;
		    next_ispace -= mask & next_ispace;
		    addr = next_ispace;
		    next_ispace += size;
		} else {		/* MEM */
		    addr = bits & 0xFFFFFFF0;
		    size = addr & -addr;
		    mask = size - 1;
		    if (mask < minmask)
			mask = minmask;	/* so mmap will work */
		    space = PCIIO_SPACE_MEM;

		    next_mspace += mask;
		    next_mspace -= mask & next_mspace;
		    addr = next_mspace;
		    next_mspace += size;
		}

		info->f_winaddr[win].space = space;
		info->f_winaddr[win].size = size;
		info->f_winaddr[win].p_addr = addr;

		pcimh_config_ptr_set(cfg_p, winoff, 4, addr);

#if !DEBUG_PCIMH
		if (showconfig)
#endif
		    cmn_err(CE_CONT, "\tBASE%d is %s[0x%x..0x%x]\n", win,
			    SPACE_NAME(info->f_winaddr[win].space),
			    info->f_winaddr[win].p_addr,
			    info->f_winaddr[win].p_addr +
			    info->f_winaddr[win].size - 1);
	    }				/* win */

	    bits = pcimh_config_ptr_setget(cfg_p, PCI_EXPANSION_ROM, 4, 0x00000000);
	    if (bits != 0xFFFFFFFF) {	/* watch out for "nobody home" */
		addr = pcimh_config_ptr_setget(cfg_p, PCI_EXPANSION_ROM, 4, 0xFFFFF000);
		if ((addr != 0xFFFFFFFF) && (addr &= 0xFFFFF000)) {
#if DEBUG_PCIMH_REG
		    cmn_err(CE_CONT, "\tExpROM raw value is 0x%x\n",
			    addr);
#endif
		    size = addr & -addr;
		    mask = size - 1;
		    if (mask < minmask)
			mask = minmask;	/* so mmap will work */
		    space = PCIIO_SPACE_MEM;

		    next_mspace += mask;
		    next_mspace -= mask & next_mspace;
		    addr = next_mspace;
		    next_mspace += size;

		    pcimh_config_ptr_set(cfg_p, PCI_EXPANSION_ROM, 4, addr);

		    info->f_romaddr.space = space;
		    info->f_romaddr.size = size;
		    info->f_romaddr.p_addr = addr;

#if !DEBUG_PCIMH
		    if (showconfig)
#endif
			cmn_err(CE_CONT, "\tExpROM is %s[0x%x..0x%x]\n",
				SPACE_NAME(info->f_romaddr.space),
				info->f_romaddr.p_addr,
				info->f_romaddr.p_addr +
				info->f_romaddr.size - 1);
		}
	    }
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_CACHE_LINE, 1, 0x20);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_LATENCY_TIMER, 1, 0x30);
	    pci_cmd = pcimh_config_ptr_get(cfg_p, PCI_CFG_COMMAND, 2);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_COMMAND, 2, pci_cmd | 0x07);

	    pciio_device_attach(info->f_vertex);
	}				/* func */
    }					/* slot */

    if (ppb_map)
	pcimh_scan_subbus(binfo);

#if !DEBUG_PCIMH
    if (showconfig)
#endif
	if (!bus)
	    cmn_err(CE_CONT, "\n");
}

LOCAL void
pcimh_scan_subbus(pcimh_info_t binfo)
{
    int                     bus = binfo->f_sec;
    int                     slot, func;
    void                   *cfg_p;
    unsigned                vid;
    unsigned                did;
    unsigned                ht;
    pcimh_info_t            info;
    unsigned                pci_cmd;

    FOR_S {
	FOR_F {
	    cfg_p = CONFIG_PTR(bus, slot, func);
	    vid = pcimh_config_ptr_get(cfg_p, PCI_CFG_VENDOR_ID, 2);

	    if ((vid == 0) || (vid == 0xFFFF)) {
		if (func)
		    continue;
		else
		    break;
	    }
	    did = pcimh_config_ptr_get(cfg_p, PCI_CFG_DEVICE_ID, 2);
	    ht = pcimh_config_ptr_get(cfg_p, PCI_CFG_HEADER_TYPE, 1);

	    /* If looking at function zero, and the
	     * multifunction bit is not set,
	     * use the FUNC_NONE code.
	     */
	    if (!func && !(ht & 0x80))
		func = PCIIO_FUNC_NONE;

	    if ((ht & 0x7F) != 0x01)
		continue;

	    maxbus++;
	    NEW(info);
	    info->f_bus = bus;

	    if (bus > 0)
		info->f_slot = extended_slot++;
	    else
		info->f_slot = slot;

	    info->f_func = func;
	    info->f_vendor = vid;
	    info->f_device = did;
	    info->f_cfgaddr = cfg_p;
	    info->f_master = binfo->f_master;
	    info->f_mfast = binfo->f_mfast;
	    info->f_pops = binfo->f_pops;
	    info->f_sec = maxbus;
	    info->f_host = binfo;
	    info->f_vertex =
		pciio_device_info_register(pcimh_vhdl, &info->f_c);
	    info->f_slot = slot;

	    /* NOTE: PPBs are *NOT* included in the linked
	     * list of PCI devices searched by the
	     * error handler.
	     */

#if DEBUG_PCIMH
	    cmn_err(CE_CONT, "    %v is bus %d slot %d VENDOR 0x%x DEVICE 0x%x (PCI-PCI Bridge)\n",
		    info->f_vertex, info->f_bus, slot, vid, did);
#endif

	    if (enable_pci_BIST)
		do_bist_for(info);

	    /* Found a PPB. Allocate secondary bus number,
	     * and set up the chip.
	     */
	    if (next_mspace != INITIAL_MSPACE) {
		next_mspace += 0xFFFFF;
		next_mspace -= 0xFFFFF & next_mspace;
	    }
	    if (next_ispace != INITIAL_ISPACE) {
		next_ispace += 0xFFF;
		next_ispace -= 0xFFF & next_ispace;
	    }
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_COMMAND, 1, 0x87 /* 0x1C7 */ );
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_CACHE_LINE, 1, 0x8);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_LATENCY_TIMER, 1, 0x30);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_BUS_PRI, 1, bus);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_BUS_SEC, 1, maxbus);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_BUS_SUB, 1, 0xFF);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_SEC_LAT, 1, 0x30);

	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMBASE, 2, 0xFFF0 & (next_mspace >> 16));
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMLIM, 2, 0xFFF0);

	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOBASE, 1, 0xF0 & (next_ispace >> 8));
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOLIM, 1, 0xF0);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOBASEHI, 2, 0xFFFF & (next_ispace >> 16));
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOLIMHI, 2, 0xFFFF);

	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMPFBASE, 2, 0xFFF0);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMPFLIM, 2, 0x0000);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMPFBASEHI, 4, 0x00000000);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMPFLIMHI, 0, 0x00000000);

	    pci_cmd = pcimh_config_ptr_get(cfg_p, PCI_CFG_COMMAND, 2);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_COMMAND, 2, pci_cmd | 0x87 /* 0x1C7 */ );

	    pciio_device_attach(info->f_vertex);

	    pcimh_scan_bus(info);

	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_BUS_SUB, 1, maxbus);
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_MEMLIM, 2, 0xFFF0 & ((next_mspace - 1) >> 16));
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOLIM, 1, 0xF0 & ((next_ispace - 1) >> 8));
	    pcimh_config_ptr_set(cfg_p, PCI_CFG_PPB_IOLIMHI, 2, 0xFFFF & ((next_ispace - 1) >> 16));
	}				/* func */
    }					/* slot */
}

/*
 * Run Built-In Self Test on this device,
 * if the device supports it.
 */
LOCAL void
do_bist_for(pcimh_info_t info)
{
    void                   *cfg_p;
    unsigned char           bist;
    int                     count;

    cfg_p = info->f_cfgaddr;
    /*
     * If the slot doesn't support BIST, then continue on
     * to the next slot
     */

    bist = pcimh_config_ptr_get(cfg_p, PCI_CFG_BIST, 1);
    if (!(bist & 0x80)) {
#if !DEBUG_PCIMH
	if (showconfig)
#endif
	    cmn_err(CE_CONT, "%v does not support BIST\n",
		    info->f_vertex);
	return;
    }
    /*
     * Start the BIST
     */
#if !DEBUG_PCIMH
    if (showconfig)
#endif
	cmn_err(CE_CONT, "%v starting BIST\n", info->f_vertex);

    pcimh_config_ptr_set(cfg_p, PCI_CFG_BIST, 1, 0x40);

    /*
     * Wait for it to complete
     */
    for (count = 0; count < 3000; count++) {
	drv_usecwait(1000);
	bist = pcimh_config_ptr_get(cfg_p, PCI_CFG_BIST, 1);
	if (!(bist & 0x40))
	    break;
#if !DEBUG_PCIMH
	if (showconfig)
#endif
	    if ((count % 100) == 0)
		cmn_err(CE_CONT, "\tWaiting for BIST to complete\n");
    }

    /*
     * Print out the results
     */
    if (count == 3000) {
	cmn_err(CE_ALERT, "%v BIST Timed Out (3 seconds)", info->f_vertex);
    } else if ((bist & 0xf) == 0) {
#if !DEBUG_PCIMH
	if (showconfig)
#endif
	    cmn_err(CE_CONT, "\tBIST Passes\n");
    } else {
	cmn_err(CE_ALERT, "%v BIST Fails - Code 0x%x\n", info->f_vertex, bist & 0xf);
    }
}

/* =====================================================================
 *                    PIO MANAGEMENT
 *
 *      For mapping system virtual address space to
 *      pciio space on a specified card
 */

/*ARGSUSED */
pciio_piomap_t
pcimh_piomap_alloc(vertex_hdl_t vertex,		/* set up mapping for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   pciio_space_t space,		/* CFG, MEM, IO, or dev-decoded win */
		   iopaddr_t offset,		/* lowest addr (offset in window) */
		   size_t byte_count,		/* size of region mappings */
		   size_t byte_count_max,	/* maximum size of a mapping */
		   unsigned flags)		/* defined in sys/pio.h */
{
    pciio_piomap_t          new_map;

    if (NEWf(new_map, flags)) {
	new_map->pp_dev = vertex;
	new_map->pp_space = space;
	new_map->pp_pciaddr = offset;
	new_map->pp_flags = flags;
	new_map->pp_mapsz = byte_count_max;
    }
    return (new_map);
}

void
pcimh_piomap_free(pciio_piomap_t pciio_piomap)
{
    DEL(pciio_piomap);
}

caddr_t
pcimh_piomap_addr(pciio_piomap_t map,		/* mapping resources */
		  iopaddr_t offset,		/* another offset */
		  size_t byte_count)		/* map this many bytes */
{
    return (pcimh_piotrans_addr(map->pp_dev, 0, map->pp_space,
		map->pp_pciaddr + offset, byte_count, map->pp_flags));
}

/*ARGSUSED */
void
pcimh_piomap_done(pciio_piomap_t pciio_piomap)
{
}

/*
 *    Return virtual address for the given space
 */
/*ARGSUSED */
caddr_t
pcimh_piotrans_addr(
		       vertex_hdl_t vertex,	/* translate for this device */
		       device_desc_t dev_desc,	/* device descriptor */
		       pciio_space_t space,	/* CFG, MEM, IO, or device-decoded window */
		       iopaddr_t addr,		/* starting address (or offset in window) */
		       size_t size,		/* map this many bytes */
		       unsigned flags)		/* (currently unused) */
{
    pcimh_info_t            info = pcimh_info_get(vertex);
    int                     win;
    caddr_t                 kva;
    iopaddr_t               offset;
    unsigned                pfn = 0;
    pde_t                  *pd;

    if (space == PCIIO_SPACE_CFG) {
	return info->f_cfgaddr + addr;
    }
    win = space - PCIIO_SPACE_WIN(0);
    if ((win < 0) || (win >= NUM_BASE_REGS)) {
	win = space - PCIIO_SPACE_USER(0);
	if ((win < 0) || (win >= NUM_USER_SPACES))
	    win = -1;
	else
	    win += NUM_BASE_REGS;
    }
    if (win != -1) {
	/* If you ask for a piotrans to an area that
	 * goes beyond the window, results are
	 * undefined.
	 */
	kva = info->f_winaddr[win].v_addr;
	if (kva)
	    return kva + addr;
	offset = addr;
	space = info->f_winaddr[win].space;
	addr = info->f_winaddr[win].p_addr;
	size = info->f_winaddr[win].size;
    }
    if (space == PCIIO_SPACE_ROM) {
	kva = info->f_romaddr.v_addr;
	if (kva)
	    return kva + addr;
	offset = addr;
	space = info->f_romaddr.space;
	addr = info->f_romaddr.p_addr;
	size = info->f_romaddr.size;
	if (size < 1)
	    return NULL;
    }

#define USE_K1_SPACE(addr, size) ( ((addr <= (32 << 20)) && \
				    (size <= (32 << 20)) && \
				   ((addr + size) <= (32 << 20))))
    /*
     * alway use K2 space for peer-to-peer dma
     */
    if (space == PCIIO_SPACE_IO) {
	if (!peer_peer_dma && USE_K1_SPACE(addr, size)) {
	    /* target mappable via PCI Low I/O */
	    kva = (caddr_t) PHYS_TO_K1(addr + PCI_LOW_IO);
	} else {
	    /* must use K2 space */
	    pfn = btoc64(addr + PCI_HI_IO);
	}
    }
    if (space == PCIIO_SPACE_MEM) {
	if (addr < 0x80000000)
	    return NULL;
	addr -= 0x80000000;
	if (!peer_peer_dma && USE_K1_SPACE(addr, size)) {
	    /* target mappable via PCI Low I/O */
	    kva = (caddr_t) PHYS_TO_K1(addr + PCI_LOW_MEMORY);
	} else {
	    /* must use K2 space */
	    pfn = btoc64(addr + PCI_HI_MEMORY);
	}
    }
    if (pfn) {
	/* Allocate K2 space to point at the window */
	unsigned                pageoff = addr & (btoc(1) - 1);
	unsigned                pages = btoc(size + pageoff);

	kva = kvalloc(pages, VM_VM | VM_UNCACHED | VM_NOSLEEP, 0);
	if (!kva) {
#if DEBUG_PCIMH
	    cmn_err(CE_CONT,
		    "%v: pcimh_piotrans_addr failed\n"
		    "\tunable to allocate %d pages of K2 space",
		    vertex, pages);
#endif
	    return NULL;
	}
	pd = kvtokptbl(kva);
	while (pages-- > 0) {
	    pg_setvalid(pd);		/* Make valid */
	    pg_setmod(pd);		/* Make (modified) writeable */
	    pd->pte.pte_pfn = pfn;
	    pd++;
	    pfn++;
	}
	kva += pageoff;
    }
    if (win != -1)
	info->f_winaddr[win].v_addr = kva;

    return kva + offset;
}

/*
 * Allow the user (device driver) to create a Memory or IO space
 * just as if the config space of the board had asked for it.
 * XXX- does not reuse free'd space.
 */
/*ARGSUSED */
iopaddr_t
pcimh_piospace_alloc(vertex_hdl_t vertex,
		     device_desc_t dev_desc,
		     pciio_space_t space,
		     size_t size,
		     size_t align)
{
    pcimh_info_t            info = pcimh_info_get(vertex);
    int                     win;
    unsigned                amask;
    iopaddr_t               pciaddr;
    unsigned                s;

    if (size < 1) {
	cmn_err(CE_CONT, "pcimh_piospace_alloc: Bad byte count 0x%x\n",
		size);
	return (-1);
    }
    /*
     * Forget it if there are any busses, since changing the space
     * allocations means I would have to reprogram the busses while
     * they were in use.
     */
    if (maxbus) {
	cmn_err(CE_CONT, "pcimh_piospace_alloc() can't use with exp bus\n");
	return (-1);
    }
    if (space == PCIIO_SPACE_MEM32)
	space = PCIIO_SPACE_MEM;
    else if ((space != PCIIO_SPACE_IO) && (space != PCIIO_SPACE_MEM)) {
	cmn_err(CE_CONT, "pcimh_piospace_alloc: Bad space 0x%x\n", space);
	return (-1);
    }
    /*
     * Search the USER_SPACE table for an open slot
     */
    for (win = NUM_BASE_REGS; win < NUM_BASE_REGS + NUM_USER_SPACES; win++) {
	if (info->f_winaddr[win].size == 0)
	    break;
    }
    if (win == NUM_BASE_REGS + NUM_USER_SPACES) {
	cmn_err(CE_CONT, "pcimh_piospace_alloc: No more user space slots\n");
	return (-1);
    }
    align &= -align;
    if (align < 4096)
	align = 4096;
    amask = align - 1;

    s = pcimh_config_lock();
    if (space == PCIIO_SPACE_IO) {
	next_ispace += amask;
	next_ispace -= amask & next_ispace;
	pciaddr = next_ispace;
	next_ispace += size;
    } else {
	next_mspace += amask;
	next_mspace -= amask & next_mspace;
	pciaddr = next_mspace;
	next_mspace += size;
    }
    pcimh_config_free(s);

    info->f_winaddr[win].space = space;
    info->f_winaddr[win].p_addr = pciaddr;
    info->f_winaddr[win].size = size;

    return pciaddr;
}

/*
 * XXX free'd space never gets reused.
 */
/*ARGSUSED */
void
pcimh_piospace_free(vertex_hdl_t vertex,
		    pciio_space_t space,
		    iopaddr_t pciaddr,
		    size_t byte_count)
{
    pcimh_info_t            info = pcimh_info_get(vertex);
    unsigned                win;

    win = space - PCIIO_SPACE_USER(0);
    if (win < NUM_USER_SPACES) {
	/* PCIIO_SPACE_USER(n) specified: free it. */
	win += NUM_BASE_REGS;
	info->f_winaddr[win].space = 0;
	info->f_winaddr[win].size = 0;
	info->f_winaddr[win].v_addr = 0;
	info->f_winaddr[win].p_addr = 0;
	return;
    }
    if (space == PCIIO_SPACE_MEM32)
	space = PCIIO_SPACE_MEM;
    else if (space != PCIIO_SPACE_IO)
	return;

    /* PCIIO_SPACE_IO or PCIIO_SPACE_MEM specified:
     * find the USER window and free it.
     */
    for (win = NUM_BASE_REGS; win < NUM_BASE_REGS + NUM_USER_SPACES; win++) {
	if ((space == info->f_winaddr[win].space) &&
	    (pciaddr == info->f_winaddr[win].p_addr)) {
	    info->f_winaddr[win].space = 0;
	    info->f_winaddr[win].size = 0;
	    info->f_winaddr[win].v_addr = 0;
	    info->f_winaddr[win].p_addr = 0;
	    return;
	}
    }
}

/* =====================================================================
 *                    DMA MANAGEMENT
 *
 *      For mapping from pci space to system
 *      physical space.
 */

/*ARGSUSED */
pciio_dmamap_t
pcimh_dmamap_alloc(vertex_hdl_t dev,		/* set up mappings for this device */
		   device_desc_t dev_desc,	/* device descriptor */
		   size_t byte_count_max,	/* max size of a mapping */
		   unsigned flags)		/* defined in dma.h */
{
    pciio_dmamap_t          new_map;
    pciio_info_t            info = pciio_info_get(dev);

    if (NEWf(new_map, flags)) {
	new_map->pd_flags = flags;
	new_map->pd_slot = pciio_info_slot_get(info);
	new_map->pd_dev = dev;
    }
    return (new_map);
}

void
pcimh_dmamap_free(pciio_dmamap_t pciio_dmamap)
{
    DEL(pciio_dmamap);
}

/*ARGSUSED */
iopaddr_t
pcimh_dmamap_addr(pciio_dmamap_t pciio_dmamap,	/* use this map */
		  paddr_t paddr,		/* map for this address */
		  size_t byte_count)		/* map this many bytes */
{
    return pcimh_endian_apply
	(paddr, pciio_dmamap->pd_dev, pciio_dmamap->pd_flags);
}

/*
 * Note this assumes that the alenlist has already been converted
 * to Physical addresses (probably by kvaddr_to_alenlist())
 */

alenlist_t
pcimh_dmamap_list(pciio_dmamap_t pciio_dmamap,	/* use these map resources */
		  alenlist_t alenlist,		/* map this Addr/Len List */
		  unsigned flags)
{
    vertex_hdl_t            pconn = pciio_dmamap->pd_dev;
    alenlist_t              new_alenlist;
    alenaddr_t              addr;
    size_t                  len;
    unsigned                inplace = flags & PCIIO_INPLACE;
    unsigned                al_flags = flags & PCIIO_NOSLEEP ? AL_NOSLEEP : 0;

    if (inplace) {
	new_alenlist = alenlist;
    } else {
	new_alenlist = alenlist_create(al_flags);
	if (!new_alenlist)
	    goto fail;
    }

    /* using internal alenlist cursor */
    (void) alenlist_cursor_init(alenlist, 0, NULL);

    while (ALENLIST_SUCCESS ==
	   alenlist_get(alenlist, NULL, 0, &addr, &len, al_flags)) {

	addr = PTR_EXT(pcimh_endian_apply(addr, pconn, flags));

	if (inplace) {
	    if (ALENLIST_SUCCESS !=
		alenlist_replace(new_alenlist, NULL,
				 &addr, &len, al_flags))
		goto fail;
	} else {
	    if (ALENLIST_SUCCESS !=
		alenlist_append(new_alenlist,
				addr, len, al_flags))
		goto fail;
	}
    }

    if (!inplace)
	alenlist_done(alenlist);
    return (new_alenlist);
  fail:
    if (new_alenlist && !inplace)
	alenlist_destroy(new_alenlist);
    return 0;
}

/*ARGSUSED */
void
pcimh_dmamap_done(pciio_dmamap_t pciio_dmamap)
{
}

/*
 * Return physical address of memory space for PCI board to use
 */
/*ARGSUSED */
iopaddr_t
pcimh_dmatrans_addr(vertex_hdl_t vertex,	/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    paddr_t paddr,		/* system physical address */
		    size_t byte_count,		/* length */
		    unsigned flags)		/* defined in dma.h */
{
    return pcimh_endian_apply(paddr, vertex, flags);
}

/*ARGSUSED */
alenlist_t
pcimh_dmatrans_list(vertex_hdl_t dev,		/* translate for this device */
		    device_desc_t dev_desc,	/* device descriptor */
		    alenlist_t palenlist,	/* system address/length list */
		    unsigned flags)		/* defined in dma.h */
{
    alenlist_t              new_alenlist;
    alenaddr_t              addr;
    size_t                  len;
    unsigned                inplace = flags & PCIIO_INPLACE;
    unsigned                al_flags = flags & PCIIO_NOSLEEP ? AL_NOSLEEP : 0;

    if (inplace) {
	new_alenlist = palenlist;
    } else {
	new_alenlist = alenlist_create(al_flags);
	if (!new_alenlist)
	    goto fail;
    }

    /* using internal alenlist cursor */
    (void) alenlist_cursor_init(palenlist, 0, NULL);

    while (ALENLIST_SUCCESS ==
	   alenlist_get(palenlist, NULL, 0, &addr, &len, al_flags)) {

	addr = PTR_EXT(pcimh_endian_apply(addr, dev, flags));

	if (inplace) {
	    if (ALENLIST_SUCCESS !=
		alenlist_replace(new_alenlist, NULL,
				 &addr, &len, al_flags))
		goto fail;
	} else {
	    if (ALENLIST_SUCCESS !=
		alenlist_append(new_alenlist,
				addr, len, al_flags))
		goto fail;
	}
    }

    if (!inplace)
	alenlist_done(palenlist);
    return (new_alenlist);
  fail:
    if (new_alenlist && !inplace)
	alenlist_destroy(new_alenlist);
    return 0;
}

/*ARGSUSED */
void
pcimh_dmamap_drain(pciio_dmamap_t map)
{
}

/*ARGSUSED */
void
pcimh_dmaaddr_drain(vertex_hdl_t pconn_vhdl,
		    paddr_t paddr,
		    size_t bytes)
{
}

/*ARGSUSED */
void
pcimh_dmalist_drain(vertex_hdl_t pconn_vhdl,
		    alenlist_t list)
{
}

/*
 * This register is overloaded.  Reading gets the revision level,
 * but writing cause the read ahead buffers to be flushed.
 */
/*ARGSUSED */
int
pcimh_flush_buffers(vertex_hdl_t dev)
{
    extern int              ip32_pci_enable_prefetch, ip32_pci_flush_prefetch;

    /*
     * NOTE: All the pio operations on IP32 need to go through the 
     * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
     * the CRIME->MACE  phantom pio read problem. 
     */
    if (ip32_pci_enable_prefetch ||
	ip32_pci_flush_prefetch)
	pciio_pio_write32(1,(volatile uint32_t *) PHYS_TO_K1(PCI_FLUSH_W));

    return (0);
}

LOCAL iopaddr_t
pcimh_endian_apply(iopaddr_t addr,
		   vertex_hdl_t pconn,
		   unsigned flags)
{
    pcimh_info_t            info = pcimh_info_get(pconn);
    pciio_endian_t          endian = info->f_endian;

    if (endian == PCIDMA_ENDIAN_BIG)
	addr |= PCI_NATIVE_VIEW;	/* no swap */

    if (endian == PCIDMA_ENDIAN_LITTLE)
	addr &= ~PCI_NATIVE_VIEW;	/* swap */

    if (flags & PCIIO_WORD_VALUES)
	addr |= PCI_NATIVE_VIEW;	/* no swap */

    if (flags & PCIIO_BYTE_STREAM)
	addr &= ~PCI_NATIVE_VIEW;	/* swap */

    return addr;
}

/* =====================================================================
 *                    INTERRUPT MANAGEMENT
 *
 *      Allow devices to establish interrupts
 */

/*
 * pcimh_intr_alloc does as much parameter checking
 * and precalculation as possible, so we can rapidly
 * and repeatedly connect and disconnect the interrupt.
 */
/*ARGSUSED */
pcimh_intr_t
pcimh_intr_alloc(vertex_hdl_t dev,		/* which device */
		 device_desc_t dev_desc,	/* device descriptor */
		 pciio_intr_line_t lines,	/* INTR line(s) to attach */
		 vertex_hdl_t owner_dev)	/* owner of this interrupt */
{
    pcimh_info_t            info = pcimh_info_get(dev);
    unsigned                mvecs = mace_ivecs(info, lines);
    int                     level = mace_level(dev, dev_desc);
    pcimh_intr_t            intr;

    if (!mvecs)
	return NULL;

    if (!NEW(intr))
	return NULL;

    intr->mi_dev = dev;
    intr->mi_dev_desc = dev_desc;
    intr->mi_lines = lines;
    intr->mi_flags = 0;
    intr->mi_mvecs = mvecs;
    intr->mi_level = level;
    return intr;
}

/*
 * Free resources consumed by intr_alloc.
 */
void
pcimh_intr_free(pcimh_intr_t intr)
{
    DEL(intr);
}

/*
 * Direct a previously set up interrupt channel
 * to the desired interrupt handler function
 * with the desired parameter.
 *
 * Returns 0 on success, returns <0 on failure.
 */
/*ARGSUSED */
int
pcimh_intr_connect(pcimh_intr_t intr,		/* pciio intr handle */
		   intr_func_t intr_func,	/* pciio intr handler */
		   intr_arg_t intr_arg,		/* arg to intr handler */
		   void *thread)
{
    vertex_hdl_t            vhdl = intr->mi_dev;
    unsigned                mvecs = intr->mi_mvecs;
    int                     level = intr->mi_level;
    int                     vec;
    int                     set = 0;
    int                     bad = 0;

    intr->mi_func = intr_func;
    intr->mi_arg = intr_arg;

    for (vec = 0; vec < 16; ++vec)
	if (mvecs & (1 << vec))
	    if (setcrimevector(MACE_INTR(vec), level,
			       (intvec_func_t) intr_func,
			       (__psint_t) intr_arg, 0)) {
		cmn_err(CE_ALERT,
		"%v pcimh_intr_connect failure on vec 0x%x level 0x%x",
			vhdl, vec, level);
		bad = 1;
	    } else
		set = 1;

    return (set && !bad) ? 0 : -1;
}

/*
 * Disassociate handler from the specified interrupt.
 */
void
pcimh_intr_disconnect(pcimh_intr_t intr)
{
    vertex_hdl_t            vhdl = intr->mi_dev;
    unsigned                mvecs = intr->mi_mvecs;
    int                     vec;

    intr->mi_func = 0;
    intr->mi_arg = 0;

    for (vec = 0; vec < 16; ++vec)
	if (mvecs & (1 << vec))
	    if (!unsetcrimevector(MACE_INTR(vec), NULL))
		cmn_err(CE_ALERT,
		     "%v pcimh_intr_disconnect failure on vec 0x%x\n",
			vhdl, vec);
}

/*
 * Return a vertex that represents the CPU currently
 * targeted by an interrupt.
 */
/*ARGSUSED */
vertex_hdl_t
pcimh_intr_cpu_get(pcimh_intr_t intr)
{
    return GRAPH_VERTEX_NONE;
}

LOCAL unsigned
mace_ivecs(pcimh_info_t info,
	   unsigned lines)
{
    pciio_slot_t            slot = info->f_slot;
    pcimh_info_t            binfo = info->f_host;
    unsigned                vecs = 0;
    int                     line;

    /* this function returns correct data for
     * requests including the following interrupt lines:
     *
     *  Bus     Slot    Interrupts
     *  0       1       INTA
     *  0       2       INTA
     *  0       3       INTA/B/C/D
     *  0       4       INTA/B/C/D      (roadrunner only)
     *  0       5       INTA/B/C/D      (roadrunner only)
     *
     *  With Bit3 Bridge plugged into slot 3:
     *
     *  2       5       INTA
     *  2       6       INTA
     *  2       7       INTA
     *  3       4       INTA
     *  3       5       INTA
     *  3       6       INTA
     *  3       7       INTA
     *
     * operation when other lines are specified is
     * undefined; I have no reference information
     * on how the Bit3 chassis routes other interrupts.
     *
     * the logic is organized so that this *should*
     * work properly if the Bit3 PCI expansion is
     * plugged into other slots in the RoadRunner.
     * in theory, this should even be OK if you
     * hooked three Bit3 chassis into a roadrunner,
     * but we can't cascade them yet since we do
     * not know what the chassis does with INTB/C/D.
     */
    if (binfo && info->f_bus) {
	/* XXX- let PPB driver specify the mappings from
	 * client slot/line to host bus/slot/line?
	 */
	if (slot &= 3)
	    lines = (lines >> (4 - slot)) | ((lines << slot) & 15);
	return mace_ivecs(binfo, lines);
    }
    /* XXX- once PPB drivers get to specify interrupt
     * bit mappings, we can handle mace the same way,
     * and promote this to be generic code.
     */
    slot -= FIRST_MH_SLOT;
    for (line = 0; line < INTRS_PER_SLOT; ++line)
	if (lines & (1 << line))
	    vecs |= 1 << mace_ivec[slot][line];

    return vecs;
}

/* XXX- is anything in mace_level specific to mace,
 * or should a facility like this be PCI-generic?
 */
LOCAL int
mace_level(vertex_hdl_t dev,			/* which device */
	   device_desc_t dev_desc)		/* device descriptor */
{
    int                     level;
    ilvl_t                  intr_swlevel;

    if (!dev_desc)
	dev_desc = device_desc_default_get(dev);

    if (!dev_desc)
	level = SPL5;			/* chose a sane default */
    else if (!(intr_swlevel = device_desc_intr_swlevel_get(dev_desc)))
	level = SPL5;			/* chose a sane default */
    /* XXX needs ithread work */
    else if (intr_swlevel == (ilvl_t) spl0)
	level = SPL0;
    else if (intr_swlevel == (ilvl_t) spl1)
	level = SPL1;
    else if (intr_swlevel == (ilvl_t) spl3)
	level = SPL3;
    else if (intr_swlevel == (ilvl_t) spl5)
	level = SPL5;
    else if (intr_swlevel == (ilvl_t) spl6)
	level = SPL6;
    else if (intr_swlevel == (ilvl_t) spl7)
	level = SPL7;
    else if (intr_swlevel == (ilvl_t) splhintr)
	level = SPLHINTR;
    else
	level = SPL5;			/* chose a sane default */

    return level;
}

/* =====================================================================
 *            OTHER ENTRY POINTS FROM PCIIO
 */

/*
 * Startup a  provider (bus)
 */
/*ARGSUSED */
void
pcimh_provider_startup(vertex_hdl_t pciio_provider)
{
    pciio_init();
}

/*
 * Shutdown a provider (bus)
 */
/*ARGSUSED */
void
pcimh_provider_shutdown(vertex_hdl_t pciio_provider)
{
}

/*
 * write gather stub
 */
/*ARGSUSED */
int
pcimh_write_gather_flush(vertex_hdl_t vhdl)
{
	return 0;
}

/* reset a particular PCI slot */
/*ARGSUSED */
int
pcimh_reset(vertex_hdl_t conn)
{
    return -1;
}

/*ARGSUSED */
pciio_endian_t
pcimh_endian_set(vertex_hdl_t vhdl,
		 pciio_endian_t device_end,
		 pciio_endian_t desired_end)
{
    pcimh_info_t            info = pcimh_info_get(vhdl);

    /* remember the desired endianness */
    info->f_endian = desired_end;
    return desired_end;
}

/* Stub for pcimh_priority_set.
 * Does MACE have a way to give different arbitration priorities
 * to PCI devices??
 */
/*ARGSUSED */
pciio_priority_t
pcimh_priority_set(vertex_hdl_t vhdl,
		   pciio_priority_t device_prio)
{
    return device_prio;
}

/*
 * Read a value from a configuration register.
 * called via the infrastructure.
 */
uint64_t
pcimh_config_get(vertex_hdl_t conn, unsigned cfg_reg, unsigned size)
{
    pcimh_info_t            info = pcimh_info_get(conn);
    void                   *bus_addr = info->f_cfgaddr;

    return pcimh_config_ptr_get(bus_addr, cfg_reg, size);
}

/*
 * Read a value from a configuration register.
 * Needed by code that runs before connection
 * points are constructed.
 */
LOCAL uint64_t
pcimh_config_ptr_get(void *bus_addr, unsigned cfg_reg, unsigned size)
{
    unsigned                tmp;
    unsigned                cap;
    unsigned                s;

    cap = (unsigned) bus_addr + (cfg_reg & 0xfc);

    s = pcimh_config_lock();

    /*
     * NOTE: All the pio operations on IP32 need to go through the 
     * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
     * the CRIME->MACE  phantom pio read problem. 
     */

    pciio_pio_write32(cap,PCI_CONFIG_ADDR_PTR);
    tmp = pciio_pio_read32(PCI_CONFIG_DATA_PTR);

    pcimh_config_free(s);

    if (cfg_reg & 3)
	tmp >>= (cfg_reg & 3) * 8;
    if (size < 4)
	tmp &= (1 << (8 * size)) - 1;
    return tmp;
}

/*
 * Write a value into a configuration register.
 * called via the infrastructure.
 */
void
pcimh_config_set(vertex_hdl_t conn, unsigned cfg_reg, unsigned size, uint64_t val)
{
    pcimh_info_t            info = pcimh_info_get(conn);
    void                   *bus_addr = info->f_cfgaddr;

    pcimh_config_ptr_set(bus_addr, cfg_reg, size, val);
}

/*
 * Write a value into a configuration register
 * Needed by code that runs before connection
 * points are constructed.
 */
void
pcimh_config_ptr_set(void *bus_addr, unsigned cfg_reg, unsigned size, uint64_t val)
{
    unsigned                cap;
    unsigned                s;

    cap = (unsigned) bus_addr + (cfg_reg & 0xfc);

    if (size < 4) {
	unsigned                tmp;
	unsigned                shft;
	unsigned                mask;

	shft = 8 * (cfg_reg & 3);
	mask = (1 << (8 * size)) - 1;
	val &= mask;
	mask <<= shft;
	val <<= shft;

	s = pcimh_config_lock();

	/*
	 * NOTE: All the pio operations on IP32 need to go through the 
	 * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to 
	 * workaround the CRIME->MACE  phantom pio read problem. 
	 */

	pciio_pio_write32(cap,PCI_CONFIG_ADDR_PTR);
	tmp = pciio_pio_read32(PCI_CONFIG_DATA_PTR);

	/* SPECIAL CASE: when writing to the
	 * COMMAND/STATUS word, write zeros to
	 * the status half of the word to avoid
	 * changing the RW1C status bits.
	 */
	if ((cfg_reg & 0xfc) == (PCI_CFG_STATUS & 0xfc))
	    tmp &= 0xffff;

	val |= tmp & ~mask;
    } else
	s = pcimh_config_lock();

    /*
     * NOTE: All the pio operations on IP32 need to go through the 
     * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
     * the CRIME->MACE  phantom pio read problem. 
     */

    pciio_pio_write32(cap,PCI_CONFIG_ADDR_PTR);
    pciio_pio_write32(val,PCI_CONFIG_DATA_PTR);

    pcimh_config_free(s);
}

LOCAL uint32_t
pcimh_config_ptr_setget(void *cfg_p, unsigned offset, int size, uint32_t value)
{
    pcimh_config_ptr_set(cfg_p, offset, size, value);
    return pcimh_config_ptr_get(cfg_p, offset, size);
}

/* Stub for pcimh_error_devenable. */
/*ARGSUSED */
int
pcimh_error_devenable(vertex_hdl_t vhdl,
		      int error_code)
{
    /* XXX do any cleanup after an error */
    return IOERROR_HANDLED;
}

/*ARGSUSED */
pciio_slot_t
pcimh_error_extract(vertex_hdl_t vhdl, pciio_space_t *space, iopaddr_t *addr)
{
    /*
     * XXX This entry point is only used by
     * the IP27 FRU analyser, and should never
     * be called on the IP32.
     */
    ASSERT(0);
    return -1;
}

/* CRIME 1.1 PIO read war routines.
 * 
 * These routines add delays before and after PIO reads to addresses in MACE
 * and its subsystems.  They must be used with CRIME 1.1 parts to ensure that
 * any bogus reads generated by the hardware bug on that part have completed
 * prior to issuing the PIO read.  The delay after the read is required to
 * prevent subsequent back-to-back writes from hanging MACE.
 * 
 * Note that this workaround does nothing to prevent the generation of bogus
 * reads in the first place.  Those may still cause problems for registers
 * with read side effects.
 */
 
#ifdef USE_PCI_PIO
/* function: pciio_pio_read8(uint8_t *)
 * purpose:  return result of pio byte read 
 */
uint8_t
pciio_pio_read8(volatile uint8_t *addr)
{
	int s;
	uint8_t readval;

	s = spl7();
	us_delay(ip32_pio_predelay);
	readval = *addr;
	us_delay(ip32_pio_postdelay);
	splx(s);

	return readval;
}


/* function: pciio_pio_read16(uint16_t *)
 * purpose:  return result of pio halfword read 
 */
uint16_t
pciio_pio_read16(volatile uint16_t *addr)
{
	int s;
	uint16_t readval;

	s = spl7();
	us_delay(ip32_pio_predelay);
	readval = *addr;
	us_delay(ip32_pio_postdelay);
	splx(s);

	return readval;
}


/* function: pciio_pio_read32(uint32_t *)
 * purpose:  return result of pio word read 
 */
uint32_t
pciio_pio_read32(volatile uint32_t *addr)
{
	int s;
	uint32_t readval;

	s = spl7();
	us_delay(ip32_pio_predelay);
	readval = *addr;
	us_delay(ip32_pio_postdelay);
	splx(s);

	return readval;
}

/* function: pciio_pio_read64(uint64_t *)
 * purpose:  return result of pio doubleword read 
 */
uint64_t
pciio_pio_read64(volatile uint64_t *addr)
{
	int s;
	uint64_t readval;

	s = spl7();
	us_delay(ip32_pio_predelay);
	readval = *addr;
	us_delay(ip32_pio_postdelay);
	splx(s);

	return readval;
}

/* function: pciio_pio_write8(uint8_t, uint8_t *)
 * purpose:  pio write byte to addr 
 */
void
pciio_pio_write8(uint8_t val, volatile uint8_t *addr)
{
	*addr = val;
}

/* function: pciio_pio_write16(uint16_t, uint16_t *)
 * purpose:  pio write halfword to addr 
 */
void
pciio_pio_write16(uint16_t val, volatile uint16_t *addr)
{
	*addr = val;
}

/* function: pciio_pio_write32(uint32_t, uint32_t *)
 * purpose:  pio write word to addr
 */
void
pciio_pio_write32(uint32_t val, volatile uint32_t *addr)
{
	*addr = val;
}

/* function: pciio_pio_write64(uint64_t, uint64_t *)
 * purpose:  pio write doubleword to addr 
 */
void
pciio_pio_write64(uint64_t val, volatile uint64_t *addr)
{
	*addr = val;
}

#endif /* USE_PCI_PIO */
/* =====================================================================
 *            ERROR HANDLING
 */

/*ARGSUSED */
LOCAL void
mace_pci_error(eframe_t *ef, __psint_t arg)
{
    uint                    pci_err_flags, pci_err_addr;
    pciio_space_t           pci_err_space;
    uint                    config_read;
    int                     win;
    pcimh_info_t            info;
    ioerror_mode_t          mode;
    ioerror_t               ioerror;

    /*
     * NOTE: All the pio operations on IP32 need to go through the 
     * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
     * the CRIME->MACE  phantom pio read problem. 
     */

    pci_err_flags = pciio_pio_read32((volatile uint32_t *) 
			       PHYS_TO_K1(PCI_ERROR_FLAGS));
    pci_err_addr  = pciio_pio_read32((volatile uint32_t *) 
			       PHYS_TO_K1(PCI_ERROR_ADDR) );
    pciio_pio_write32(0,(volatile uint32_t *)PHYS_TO_K1(PCI_ERROR_FLAGS)); 

    config_read = PERR_MASTER_ABORT | PERR_CONFIG_ADDR |
	PERR_MASTER_ABORT_ADDR_VALID;

    /*
     * Normally, do not report master aborts on configuration space
     * reads.  However, if pcimh_verbose is set, then report
     * everything.
     */
    if (((pci_err_flags & config_read) == config_read) &&
	(pcimh_verbose == 0))
	return;

    /*
     * If one of these bits are not set, we don't have
     * a valid address, so report the error and return.
     */
    if (!(pci_err_flags & (PERR_RETRY_ADDR_VALID |
			   PERR_DATA_PARITY_ADDR_VALID |
			   PERR_TARGET_ABORT_ADDR_VALID |
			   PERR_MASTER_ABORT_ADDR_VALID))) {
	if (pcimh_verbose)
		cmn_err(CE_ALERT, "PCI %r Error (no PCI address logged)",
			pci_err_flags);
	return;
    }
    if (pci_err_flags & PERR_CONFIG_ADDR) {
	int                     bus = 0;
	int                     slot;
	int                     slotok = 0;
	int                     func = PCI_TYPE0_FUNC(pci_err_addr);
	unsigned                reg = PCI_TYPE0_REG(pci_err_addr);

	pci_err_space = PCIIO_SPACE_CFG;

	FOR_S {
	    if (pci_err_addr & (PCI_TYPE0(slot, 0, 0))) {
		slotok = 1;
		break;
	    }
	}
	if (!slotok) {
	    if (func)
		cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n"
			"\tConfig reg 0x%x for unknown slot, func %d",
			pci_err_flags, pci_err_bits,
			SPACE_NAME(pci_err_space), pci_err_addr,
			reg, func);
	    else
		cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n"
			"\tConfig reg 0x%x for unknown slot",
			pci_err_flags, pci_err_bits,
			SPACE_NAME(pci_err_space), pci_err_addr,
			reg, func);
	    return;
	}
	LINKED_DEV {
	    if ((info->f_bus == 0) &&
		(info->f_slot == slot) &&
		((info->f_func == func) ||
		 ((info->f_func == PCIIO_FUNC_NONE) &&
		  (func == 0)))) {
		cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n\tConfig reg 0x%x for %v",
			pci_err_flags, pci_err_bits,
			SPACE_NAME(pci_err_space), pci_err_addr,
			reg, info->f_vertex);
		return;
	    }
	}
	if (func)
	    cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n"
		    "\tConfig reg 0x%x for slot %d func %d (no device present)",
		    pci_err_flags, pci_err_bits,
		    SPACE_NAME(pci_err_space), pci_err_addr,
		    reg, slot, func);
	else
	    cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n"
		  "\tConfig reg 0x%x for slot %d (no device present)",
		    pci_err_flags, pci_err_bits,
		    SPACE_NAME(pci_err_space), pci_err_addr,
		    reg, slot);
	return;
    }
    pci_err_space = ((pci_err_flags & PERR_MEMORY_ADDR)
		     ? PCIIO_SPACE_MEM
		     : PCIIO_SPACE_IO);

    IOERROR_INIT(&ioerror);
    IOERROR_SETVALUE(&ioerror, ef, (caddr_t) ef);
    IOERROR_SETVALUE(&ioerror, epc, (caddr_t) ef->ef_epc);

    /*
     * OK, we now have the offending address so try to figure out
     * who it belongs to ... for pciio_error_handler() to work we
     * need to determine on which slot the error occurred.
     * (XXX eventually down to the func level??)
     */
    LINKED_DEV {
	FOR_BARS {
	    if ((pci_err_space == P_SPACE) &&
		(pci_err_addr >= (uint) P_ADDR) &&
		(pci_err_addr < (uint) (P_ADDR + P_SIZE))) {
		goto found_addr;
	    }
	}				/* bar */
    }					/* dev */

    cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n\t(no associated PCI device)",
	    pci_err_flags, pci_err_bits,
	    SPACE_NAME(pci_err_space), pci_err_addr);
    return;

  found_addr:
    IOERROR_SETVALUE(&ioerror, busaddr, (iopaddr_t) pci_err_addr);
    IOERROR_SETVALUE(&ioerror, widgetdev, (short) info->f_slot);

    /*
     * XXX should we know the error_code (PIO/DMA, READ/WRITE)
     * and the ioerror_mode at this point?? Weak attempt at a
     * ioerror_mode.
     */
    mode = (ef->ef_sr & SR_PREVMODE) ?
	MODE_DEVUSERERROR : MODE_DEVERROR;
    if (pciio_error_handler(pcimh_vhdl, IOECODE_UNSPEC, mode,
			    &ioerror) != IOERROR_UNHANDLED)
	return;

    cmn_err(CE_ALERT, "PCI %r Error accessing %s 0x%x\n"
	    "\tAddress owned by %v %s%d",
	    pci_err_flags, pci_err_bits,
	    SPACE_NAME(pci_err_space), pci_err_addr,
	    info->f_vertex,
	    (win < NUM_BASE_REGS) ? "BASE" : "USER",
	    (win < NUM_BASE_REGS) ? win : (win - NUM_BASE_REGS));
}

/* HINV support for O2 PCI drivers
 */
int
IP32_hinv_location(vertex_hdl_t vertex)
{
    pcimh_info_t            info = pcimh_info_get(vertex);
    int                     bus = info->f_bus;
    int                     slot = info->f_slot;
    int                     func = info->f_func;

    return (bus << 16) | (func << 8) | (slot << 0);
}
#endif				/* IP32 */
