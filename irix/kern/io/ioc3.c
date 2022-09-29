/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/* This is the top level IOC3 device driver. It does very little, farming
 * out actual tasks to the various slave IOC3 drivers (serial, ethernet, etc)
 */
#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/RCS/ioc3.c,v 1.115 1997/10/18 00:53:43 nigel Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mips_addrspace.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
#include <sys/sysinfo.h>
#include <sys/rtmon.h>

#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/driver.h>
#include <sys/cdl.h>
#include <sys/PCI/pciio.h>

#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/ioc3.h>
#include <sys/ns16550.h>

#include <sys/runq.h>
#include <sys/nic.h>

#include <ksys/xthread.h>

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

#define NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

#ifdef DEBUG
#define IOC3_ITHREAD_QUALIFY(_func) ithread_qualify((void*)_func)
#else
#define  IOC3_ITHREAD_QUALIFY(_func)
#endif /* DEBUG */

int			ioc3_devflag = D_MP;

/*
 * some external functions we still need.
 */

#ifndef IOC3_PIO_MODE
extern void		ioc3_serial_kill(void *port);
extern int		ioc3_serial_attach(vertex_hdl_t conn);
#else				/* IOC3_PIO_MODE */
extern int		ns16550_attach(vertex_hdl_t conn);
#endif

extern int		pckm_attach(vertex_hdl_t conn);
extern int		ef_attach(vertex_hdl_t conn);

#if IP30 && HWG_PERF_CHECK && !DEBUG
#include <sys/RACER/heart.h>
static heart_piu_t     *heart_piu = HEART_PIU_K1PTR;
#define	RAW_COUNT()	(heart_piu->h_count)
#endif

#ifdef SN
#include <sys/SN/agent.h>	/* for hub_check_pci_equiv */
/*
 * On SN0, we can have two PCI addresses for the same device.  We might
 * try to talk to it in the prom from one node and then in the kernel from
 * another one.	 That would mean that we couldn't figure out where the
 * console is, for example.  Calling hub_check_pci_equiv() lets us know
 * if two different PCI small window addresses really refer to the same
 * device.
 */
#define CHECK_IO_EQUIV(_a, _b)	hub_check_pci_equiv((void*)(_a), (void*)(_b))
#else
#define CHECK_IO_EQUIV(_a, _b)	((void *)(_a) == (void *)(_b))
#endif

/*
 * per-IOC3 data structure
 */
typedef struct ioc3_soft_s {
    vertex_hdl_t	    is_ioc3_vhdl;
    vertex_hdl_t	    is_econn_vhdl;
    vertex_hdl_t	    is_oconn_vhdl;

    ioc3_cfg_t		   *is_ioc3_cfg;
    ioc3_mem_t		   *is_ioc3_mem;

    pciio_intr_t	    is_intr;
    cpuid_t		    is_intrdest;	/* CPU this intr runs on */

    /*
     * each in-use entry in this array contains at least
     * one nonzero bit in sd_bits; no two entries in this
     * array have overlapping sd_bits values.
     */
#define MAX_IOC3_INTR_ENTS	(8*sizeof (ioc3reg_t))
    struct ioc3_intr_info {
	ioc3reg_t		sd_bits;
	ioc3_intr_func_f       *sd_intr;
	intr_arg_t		sd_info;
	vertex_hdl_t		sd_vhdl;
	struct ioc3_soft_s     *sd_soft;
	thd_int_t		sd_tinfo;
	int		      (*sd_use_ithread)(intr_arg_t);
    } is_intr_info[MAX_IOC3_INTR_ENTS];

    /* number of entries active in the above array */
    int			    is_num_intrs;

    ioc3reg_t		    is_intr_bits_busy;	/* bits assigned */
    ioc3reg_t		    is_intr_ents_free;	/* which active entries are free */

    /* is_ir_lock must be held while
     * modifying sio_ie values, so
     * we can be sure that sio_ie is
     * not changing when we read it
     * along with sio_ir.
     */
    lock_t		    is_ir_lock;		/* SIO_IE[SC] mod lock */

#if RTINT_WAR
    ioc3reg_t is_ienb;
    char is_poll_state;
#endif
} ioc3_soft_t;

#if RTINT_WAR
# define POLL_INTR_MASK (SIO_IR_SA | SIO_IR_SB | SIO_IR_PP | SIO_IR_KBD_INT)
# define POLL_MODE	1
# define POLL_ACTIVE	2
# define TOUT_PENDING	4
#endif

#define sd_flags	sd_tinfo.thd_flags
#define sd_isync	sd_tinfo.thd_isync
#define sd_lat		sd_tinfo.thd_latstats
#define sd_thread	sd_tinfo.thd_ithread

#define ioc3_soft_set(v,i)	hwgraph_fastinfo_set((v), (arbitrary_info_t)(i))
#define ioc3_soft_get(v)	((ioc3_soft_t *)hwgraph_fastinfo_get(v))

/*
 * system global data
 */

int			console_device_attached;

static ioc3_soft_t     *the_console_ioc3;
ioc3_cfg_t	       *console_cfg;
ioc3_mem_t	       *console_mem;

/* =====================================================================
 *    Function Table of Contents
 */

void			ioc3_mlreset(ioc3_cfg_t *, ioc3_mem_t *);
void			ioc3_init(void);
int			ioc3_attach(vertex_hdl_t);
#if RTINT_WAR
void			ioc3_handle_intr(intr_arg_t, int);
#endif

int			ioc3_is_console(vertex_hdl_t);

static error_handler_f	ioc3_error_handler;

static void		ioc3_intrd(struct ioc3_intr_info *);
static void		ioc3_intrd_setup(struct ioc3_intr_info *);

static ulong_t	       *ioc3_subdev_ptr(vertex_hdl_t, int alloc);
int			ioc3_subdev_enabled(vertex_hdl_t, ioc3_subdev_t);
void			ioc3_subdev_enables(vertex_hdl_t, ulong_t);
void			ioc3_subdev_enable(vertex_hdl_t, ioc3_subdev_t);
void			ioc3_subdev_disable(vertex_hdl_t, ioc3_subdev_t);

static int		ioc3_intb_needed(vertex_hdl_t vhdl);

#if ULI
/* return the target cpu for the interrupt for an IOC3 */
cpuid_t
ioc3_intr_cpu_get(ioc3_soft_t *soft)
{
    return(soft->is_intrdest);
}
#endif

/* Platform specific early init code hands us the config and mem
 * pointers of the console ioc3, which we use later on to recognize
 * the console when it is attached
 */
void
ioc3_mlreset(ioc3_cfg_t *cfg, ioc3_mem_t *mem)
{
    console_cfg = cfg;
    console_mem = mem;

    /* we might want to call ioc3_pckm's earlyinit routine here,
       so it can be used as a console device */
}

/* The IOC3 hardware provides no atomic way to determine if interrupts
 * are pending since two reads are required to do so. The handler must
 * read the SIO_IR and the SIO_IES, and take the logical and of the
 * two. When this value is zero, all interrupts have been serviced and
 * the handler may return.
 *
 * This has the unfortunate "hole" that, if some other CPU or
 * some other thread or some higher level interrupt manages to
 * modify SIO_IE between our reads of SIO_IR and SIO_IE, we may
 * think we have observed SIO_IR&SIO_IE==0 when in fact this
 * condition never really occurred.
 *
 * To solve this, we use a simple spinlock that must be held
 * whenever modifying SIO_IE; holding this lock while observing
 * both SIO_IR and SIO_IE guarantees that we do not falsely
 * conclude that no enabled interrupts are pending.
 */

#if RTINT_WAR
static void
ioc3_poll(void *arg)
{
    lock_t	*lp = &((ioc3_soft_t *)arg)->is_ir_lock;
    int s;

    ioc3_handle_intr((intr_arg_t) arg, 0);

    s = mutex_spinlock_spl(lp, spl7);

    if (((ioc3_soft_t*)arg)->is_poll_state & POLL_ACTIVE) {
	ASSERT(((ioc3_soft_t*)arg)->is_poll_state & TOUT_PENDING);
	mutex_spinunlock(lp, s);
	prtimeout_nothrd(master_procid, ioc3_poll, arg, 1);
    }
    else {
	((ioc3_soft_t*)arg)->is_poll_state &= ~TOUT_PENDING;
	mutex_spinunlock(lp, s);
    }
}
#endif

void
ioc3_write_ireg(void *ioc3_soft, ioc3reg_t val, int which)
{
#if RTINT_WAR
    ioc3_soft_t		   *soft = (ioc3_soft_t *) ioc3_soft;
#endif
    ioc3_mem_t		   *mem = ((ioc3_soft_t *) ioc3_soft)->is_ioc3_mem;
    lock_t		   *lp = &((ioc3_soft_t *) ioc3_soft)->is_ir_lock;
    int			    s;

    s = mutex_spinlock_spl(lp, spl7);

    switch (which) {
      case W_IES:
#if RTINT_WAR
	/* store all enabled interrupts */
	soft->is_ienb |= val;

	/* If poll mode is not currently enabled and we are going to
	 * enable the RTINT, set up poll mode
	 */
	if (!(soft->is_poll_state & POLL_MODE) && (soft->is_ienb & SIO_IR_RT)) {
	    soft->is_poll_state |= POLL_MODE;
	    
	    /* disable all polled interrupts */
	    mem->sio_iec_ro = POLL_INTR_MASK;
	}

	if (soft->is_poll_state & POLL_MODE) {

	    /* if a serial, parallel or kbd/mouse interrupt is being
	     * enabled and one was not enabled previously, start the
	     * interrupt polling timeout.
	     */
	    if (!(soft->is_poll_state & POLL_ACTIVE) &&
		(soft->is_ienb & POLL_INTR_MASK)) {
		soft->is_poll_state |= POLL_ACTIVE;
		if (!(soft->is_poll_state & TOUT_PENDING)) {
		    /* prevent starting up parallel timeouts */
		    soft->is_poll_state |= TOUT_PENDING;
		    prtimeout_nothrd(master_procid, ioc3_poll, ioc3_soft, 1);
		}
	    }

	    /* don't actually enable the polled interrupts */
	    val &= ~POLL_INTR_MASK;
	}
#endif
	mem->sio_ies_ro = val;
	break;

      case W_IEC:
#if RTINT_WAR
	soft->is_ienb &= ~val;

	if (soft->is_poll_state & POLL_MODE) {

	    /* if a serial, parallel or kbd/mouse interrupt is currently
	     * active and none will be active after this operation, stop
	     * the polling timeout.
	     */
	    if ((soft->is_poll_state & POLL_ACTIVE) &&
		!(soft->is_ienb & POLL_INTR_MASK))
		soft->is_poll_state &= ~POLL_ACTIVE;

	    /* if we are disabling the RTINT, turn off poll mode. */
	    if (!(soft->is_ienb & SIO_IR_RT)) {
		soft->is_poll_state &= ~(POLL_MODE | POLL_ACTIVE);

		/* reenable the actual hardware interrupts */
		mem->sio_ies_ro = soft->is_ienb;
	    }
	}
#endif
	mem->sio_iec_ro = val;
	break;
    }

    mutex_spinunlock(lp, s);
}

static ioc3reg_t
ioc3_pending_intrs(ioc3_soft_t * ioc3_soft)
{
    ioc3_mem_t	*mem = ioc3_soft->is_ioc3_mem;
    lock_t	*lp = &ioc3_soft->is_ir_lock;
    int		s;
    ioc3reg_t	intrs;

    s = mutex_spinlock_spl(lp, spl7);
#if RTINT_WAR
    intrs = mem->sio_ir & ioc3_soft->is_ienb;
#else
    intrs = mem->sio_ir & mem->sio_ies_ro;
#endif
    mutex_spinunlock(lp, s);
    return intrs;
}

/*
 *    ioc3_init: called once during system startup or
 *	when a loadable driver is loaded.
 *
 *	The driver_register function should normally
 *	be in _reg, not _init.	But the ioc3 driver is
 *	required by devinit before the _reg routines
 *	are called, so this is an exception.
 */
void
ioc3_init(void)
{
#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "ioc3_init()\n");
#endif

    pciio_driver_register(IOC3_VENDOR_ID_NUM,
			  IOC3_DEVICE_ID_NUM,
			  "ioc3_",
			  CDL_PRI_HI);

    /* XXX- move this code to a different file. */
    {
	static nic_vmc_func	pci_ioc3_enet_vmc;

	nic_vmc_add("030-1155-", pci_ioc3_enet_vmc);
    }
}

/* XXX- move this function to a different file. */
static void
pci_ioc3_enet_vmc(vertex_hdl_t vhdl)
{
#if DEBUG
    cmn_err(CE_CONT, "%v looks like a PCI IOC3 Ethernet card\n", vhdl);
#endif
    ioc3_subdev_enables(vhdl, IOC3_SDB_ETHER | IOC3_SDB_KBMS);
}

/* Called for each IOC3 found by
 * the PCIIO infrastructure.
 */

int
ioc3_attach(vertex_hdl_t conn_vhdl)
{
    ioc3_cfg_t		   *cfg;
    ioc3_mem_t		   *mem;
    graph_error_t	    rc;
    vertex_hdl_t	    ioc3_vhdl;
    vertex_hdl_t	    econn_vhdl;
    vertex_hdl_t	    oconn_vhdl;
    ioc3_soft_t		   *soft;
    device_desc_t	    ioc3_dev_desc;
    pciio_intr_t	    intr;
    pciio_piomap_t	    cmap = 0;
    pciio_piomap_t	    rmap = 0;

#if IP30 && HWG_PERF_CHECK && !DEBUG
    unsigned long	    cval;
    extern void		    hwg_hprint(unsigned long, char *);
#endif

#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_attach()\n", conn_vhdl);
#endif

    /*
     * get PIO mappings through our "primary"
     * connection point to the IOC3's CFG and
     * MEM spaces.
     */

    cfg = (ioc3_cfg_t *) pciio_pio_addr
	(conn_vhdl, 0, PCIIO_SPACE_CFG, 0, sizeof(*cfg), &cmap, 0);

    if (!cfg) {
	cmn_err(CE_ALERT,
		"%v/" EDGE_LBL_IOC3
		": unable to get PIO mapping for my CFG space\n",
		conn_vhdl);
	return -1;
    }
    /* no need to store cmap, since we never piomap_free it. */

    mem = (ioc3_mem_t *) pciio_pio_addr
	(conn_vhdl, 0, PCIIO_SPACE_WIN(0), 0, sizeof(*mem), &rmap, 0);

    if (!mem) {
	if (cmap)
	    pciio_piomap_free(cmap);
	cmn_err(CE_ALERT,
		"%v/" EDGE_LBL_IOC3
		": unable to get PIO mapping for my MEM space\n",
		conn_vhdl);
	return -1;
    }
    /* no need to store rmap, since we never piomap_free it. */

    /*
     * Check for "host" and "guest" links, which will
     * get our "ether" and "other" connect points for
     * us when we are in dual-connect mode.
     */
    econn_vhdl = conn_vhdl;
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_HOST, &econn_vhdl);
    if ((rc != GRAPH_SUCCESS) || (econn_vhdl == GRAPH_VERTEX_NONE))
	econn_vhdl = conn_vhdl;

    oconn_vhdl = econn_vhdl;
    rc = hwgraph_traverse(econn_vhdl, EDGE_LBL_GUEST, &oconn_vhdl);
    if ((rc != GRAPH_SUCCESS) || (oconn_vhdl == GRAPH_VERTEX_NONE))
	oconn_vhdl = econn_vhdl;

    /*
     * Create the "ioc3" vertex which hangs off of
     * the connect points.
     * This code is slightly paranoid.
     */
    rc = hwgraph_path_add(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    (void) hwgraph_edge_add(econn_vhdl, ioc3_vhdl, EDGE_LBL_IOC3);
    (void) hwgraph_edge_add(oconn_vhdl, ioc3_vhdl, EDGE_LBL_IOC3);

    /*
     * Allocate the soft structure, fill it in a bit,
     * and attach it to the ioc3 vertex.
     */
    NEW(soft);

    spinlock_init(&soft->is_ir_lock, "is_ir_lock");

    soft->is_ioc3_vhdl = ioc3_vhdl;
    soft->is_econn_vhdl = econn_vhdl;
    soft->is_oconn_vhdl = oconn_vhdl;
    soft->is_ioc3_cfg = cfg;
    soft->is_ioc3_mem = mem;
    ioc3_soft_set(ioc3_vhdl, soft);

    /* if this is the console ioc3, take note of it
     */
    if (CHECK_IO_EQUIV(cfg, console_cfg) &&
	CHECK_IO_EQUIV(mem, console_mem)) {
	the_console_ioc3 = soft;
#if DEBUG && IOC3_DEBUG
	cmn_err(CE_CONT, "%v is the console IOC3!\n", ioc3_vhdl);
    } else {
	cmn_err(CE_CONT, "%v is not the console IOC3:\n"
		"\t0x%X vs 0x%X (cfg)\n"
		"\t0x%X vs 0x%X (mem)\n",
		ioc3_vhdl,
		console_cfg, cfg,
		console_mem, mem);
#endif
    }

    /* claim errors from the IOC3 PCI ports
     */
    pciio_error_register(econn_vhdl, ioc3_error_handler, soft);
    pciio_error_register(oconn_vhdl, ioc3_error_handler, soft);

    dprintf(("ioc3 init config base 0x%x mem base 0x%x\n",
	     cfg, mem));

    /* init the ioc3 */

    /* IP30 and SN0 boot proms allocate the PCI
     * space and set up the pci_addr fields.
     * Other systems need to set the base address.
     * This is handled automatically if the PCI infrastructure
     * is used.
     */

#if !defined(IP30) && !defined(SN0) && !defined(EVEREST)
    PCI_OUTW(&cfg->pci_addr,
	     K1_TO_PHYS((char *) mem));
#endif
    PCI_OUTW(&cfg->pci_scr,
	     PCI_INW(&cfg->pci_scr) |
	     PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE |
	     PCI_SCR_PAR_RESP_EN | PCI_SCR_SERR_EN |
	     PCI_SCR_DROP_MODE_EN);
    PCI_OUTW(&mem->sio_cr,
	     ((UARTA_BASE >> 3) << SIO_CR_SER_A_BASE_SHIFT) |
	     ((UARTB_BASE >> 3) << SIO_CR_SER_B_BASE_SHIFT) |
	     (0xf << SIO_CR_CMD_PULSE_SHIFT));

    /* XXX constants in header!! */

    /* plug some things in. XXX need to document this better! */
#ifdef IP22
    PCI_OUTW(PHYS_TO_K1(mem - 0x100000), 0x200);
#endif

#if IP30
    /*
     * set the latency timer to 1.2 us, see sec 2.8 of the
     * ISD PCI White Paper on PCI arbitration.
     * our PCI clock is running at 33MHz = 30ns/cycle
     * 1200ns / (30ns/cycle) = 40 cycles
     * other fields within the register are read only and ignored
     * on write so we don't need to do a read-modify-write
     */
    PCI_OUTW(&cfg->pci_lat, 40 << 8);
#else
    PCI_OUTW(&cfg->pci_lat, 0xff00);
#endif				/* IP30 */

    /* enable serial port mode select generic PIO pins as outputs
     */
    PCI_OUTW(&mem->gpcr_s, GPCR_UARTA_MODESEL | GPCR_UARTB_MODESEL);

    /* Clear and disable all interrupts */
    IOC3_WRITE_IEC(soft, ~0);
    PCI_OUTW(&mem->sio_ir, ~0);

    /*
     * Decode the IOC3 NIC (if we have one).
     * This will trigger any NIC callbacks.
     */

    /* XXX the following test is the WRONG way to make sure
     *			     that we don't look for a mfg. nic on basio and menet
     *	   boards - need to use nic bit which should be set only
     *	   on bridges that have physical PCI slots - pending doing
     *	   this correctly, the below code just avoids looking
     *	   on boards we know to not have mfg. nic on the ioc3
     *	   See bug #436355.
     */
    {
	char			name[MAXDEVNAME];

	vertex_to_name(conn_vhdl, name, MAXDEVNAME);
	if (!strstr(name, "baseio") && !strstr(name, "menet"))
	    (void) nic_ioc3_vertex_info(conn_vhdl,
					(nic_data_t) & mem->mcr,
					(__int32_t *) & mem->gpcr_s);
    }

    /*
     * If no devices are present that
     * need IOC3's second interrupt,
     * we are all done.
     */
    if (!ioc3_intb_needed(conn_vhdl)) {
	soft->is_intr = 0;
	soft->is_intrdest = cpuid();
	return 0;
    }
    /*
     * Alloc the ioc3 intr before attaching the subdevs, so the
     * cpu handling the ioc3 intr is known (for setmustrun on
     * the ioc3 ithreads).
     *
     * NOTE: If we are not dual-slot, and ethernet is enabled, we
     * want to pick up INTB; otherwise, we want INTA. The ethernet
     * driver will take care of routing its interrupt. For more
     * details on this rule, see the IOC3 spec.
     */
    /*
     * Modify the name field of the IOC3 device
     * descriptor. (XXX- IOC3 **UART**???)
     */
    ioc3_dev_desc = device_desc_dup(oconn_vhdl);
    device_desc_flags_set(ioc3_dev_desc,
	       (device_desc_flags_get(ioc3_dev_desc) | D_INTR_NOTHREAD));
    device_desc_intr_swlevel_set(ioc3_dev_desc,INTR_SWLEVEL_NOTHREAD_DEFAULT);
    device_desc_intr_name_set(ioc3_dev_desc, "IOC3 UART");
    device_desc_default_set(oconn_vhdl,ioc3_dev_desc);

    intr = pciio_intr_alloc
	(oconn_vhdl, ioc3_dev_desc,
	 (((oconn_vhdl == econn_vhdl) &&
	   (ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ether))) ?
	  PCIIO_INTR_LINE_B : PCIIO_INTR_LINE_A),
	 oconn_vhdl);

    if (intr == NULL)
	cmn_err(CE_PANIC,
		"%v: ioc3_attach:\n"
		"\tunable to allocate interrupt\n",
		oconn_vhdl);
    soft->is_intr = intr;

    pciio_intr_connect(intr,(intr_func_t)ioc3_intr,(intr_arg_t)soft,0);

    /* Save the cpu that's handling the ioc3 intr */
    soft->is_intrdest =
	cpuvertex_to_cpuid(pciio_intr_cpu_get(soft->is_intr));
    ASSERT(soft->is_intrdest != CPU_NONE);

    /* =============================================================
     *				  attach subdevices
     *
     * NB: as subdevs start calling pciio_register
     * and pciio calls all registered drivers, we
     * can stop explicitly calling subdev drivers.
     *
     * The drivers attached here have not been converted
     * to stand on their own. However, they *do* know
     * to call ioc3_subdev_enabled() to decide whether
     * to actually attach themselves.
     *
     * It would be nice if we could convert these
     * few remaining drivers over so they would
     * register as proper PCI device drivers ...
     */

#ifdef IOC3_PIO_MODE
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT();
#endif
    ns16550_attach(oconn_vhdl); /* PIO serial ports */
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT() - cval;
    hwg_hprint(cval, "ns16550_attach()");
#endif
#else
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT();
#endif
    ioc3_serial_attach(oconn_vhdl);	/* DMA serial ports */
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT() - cval;
    hwg_hprint(cval, "ioc3_serial_attach()");
#endif
#endif
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT();
#endif
    pckm_attach(oconn_vhdl);	/* Keyboard and Mouse */
#if IP30 && HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT() - cval;
    hwg_hprint(cval, "pckm_attach()");
#endif

    /* track that the console has been attached */
    if (soft == the_console_ioc3)
	console_device_attached = 1;

    return 0;
}

/*
 * ioc3_intr_connect:
 * arrange for interrupts for a subdevice
 * to be delivered to the right bit of
 * code with the right parameter.
 *
 * XXX- can we get rid of subdevice numbers
 * entirely, somehow?
 *
 * XXX- returning an error instead of panicing
 * might be a good idea (think bugs in loadable
 * ioc3 subdevices).
 */
void
ioc3_intr_connect(vertex_hdl_t conn_vhdl,
		  ioc3reg_t intrbits,
		  ioc3_intr_func_f *intr,
		  intr_arg_t info,
		  vertex_hdl_t owner_vhdl,
		  vertex_hdl_t intr_dev_vhdl,
		  int (*use_ithread)(intr_arg_t))
{
    graph_error_t	    rc;
    vertex_hdl_t	    ioc3_vhdl;
    ioc3_soft_t		   *soft;
    ioc3reg_t		    old, bits;
    int			    i;

    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    if (rc != GRAPH_SUCCESS) {
	cmn_err(CE_ALERT,
		"ioc3_intr_connect(%v): ioc3_attach not yet called",
		owner_vhdl);
	return;
    }
    soft = ioc3_soft_get(ioc3_vhdl);
    ASSERT(soft != NULL);

    /*
     * try to allocate a slot in the array
     * that has been marked free; if there
     * are none, extend the high water mark.
     */
    while (1) {
	bits = soft->is_intr_ents_free;
	if (bits == 0) {
	    i = atomicAddInt(&soft->is_num_intrs, 1) - 1;
	    ASSERT(i < MAX_IOC3_INTR_ENTS || (printf("i %d\n", i), 0));
	    break;
	}
	bits &= ~(bits - 1);	/* keep only the ls bit */
	old = atomicClearInt((int *) &soft->is_intr_ents_free, bits);
	if (bits & old) {
#if 0				/* XXX we don't have ffs in the kernel (yet) */
	    i = ffs(bits) - 1;
#else
	    {
		ioc3reg_t		shf;

		i = 31;
		if (shf = (bits >> 16))
		    bits = shf;
		else
		    i -= 16;
		if (shf = (bits >> 8))
		    bits = shf;
		else
		    i -= 8;
		if (shf = (bits >> 4))
		    bits = shf;
		else
		    i -= 4;
		if (shf = (bits >> 2))
		    bits = shf;
		else
		    i -= 2;
		if (shf = (bits >> 1))
		    bits = shf;
		else
		    i -= 1;
	    }
#endif
	    ASSERT(i < MAX_IOC3_INTR_ENTS || (printf("i %d\n", i), 0));
	    break;
	}
    }

    soft->is_intr_info[i].sd_bits = intrbits;
    soft->is_intr_info[i].sd_intr = intr;
    soft->is_intr_info[i].sd_info = info;
    soft->is_intr_info[i].sd_vhdl = owner_vhdl;
    soft->is_intr_info[i].sd_soft = soft;
    soft->is_intr_info[i].sd_use_ithread = use_ithread;

    /* make sure there are no bitmask overlaps */
    {
	ioc3reg_t		old;

	old = atomicSetInt((int *) &soft->is_intr_bits_busy, intrbits);
	if (old & intrbits) {
	    cmn_err(CE_CONT,
		    "%v: trying to share ioc3 intr bits 0x%X\n",
		    owner_vhdl, old & intrbits);
#if DEBUG && IOC3_DEBUG
	    {
		int			x;

		for (x = 0; x < i; x++)
		    if (intrbits & soft->is_intr_info[x].sd_bits) {
			cmn_err(CE_CONT,
				"%v: ioc3 intr bits 0x%X already call "
				"0x%X(0x%X, ...)\n",
				soft->is_intr_info[x].sd_vhdl,
				soft->is_intr_info[i].sd_bits,
				soft->is_intr_info[i].sd_intr,
				soft->is_intr_info[i].sd_info);
		    }
	    }
#endif
	    panic("ioc3_intr_connect: no IOC3 interrupt source sharing allowed");
	}
    }
    {
	char			thd_name[32];
	struct ioc3_intr_info  *iip = &soft->is_intr_info[i];
	device_driver_t driver;
	device_desc_t desc;
	int pri;

	desc = device_desc_dup(intr_dev_vhdl);
	pri = device_desc_intr_swlevel_get(desc);
	if (pri < 0) {
		extern int default_intr_pri;
		pri = default_intr_pri;
	}

	driver = device_driver_getbydev(intr_dev_vhdl);
	if (driver) {
		char prefix[16];

		device_driver_name_get(driver, prefix, sizeof prefix);
		sprintf(thd_name, "ioc3[%d]%s", i, prefix);
	} else {
		sprintf(thd_name, "ioc3[%d]", i);
	}
	thd_name[IT_NAMELEN-1] = '\0';

	atomicSetInt(&iip->sd_flags, THD_ISTHREAD | THD_REG);
	xthread_setup(thd_name,
		      pri,
		      &iip->sd_tinfo,
		      (xt_func_t *) ioc3_intrd_setup,
		      iip);
    }

#if DEBUG && (INTR_DEBUG || ATTACH_DEBUG || IOC3_DEBUG)
    printf("%v ioc3 bits 0x%X are 0x%X(0x%X)\n",
	   owner_vhdl, intrbits, intr, info);
#endif
}

/*
 * ioc3_intr_disconnect: turn off interrupt request
 * service for a specific service function and argument.
 * Scans the array for connections to the specified
 * function with the specified info and owner; turns off
 * the bits specified in intrbits. If this results in
 * an empty entry, logs it in the free entry map.
 */
void
ioc3_intr_disconnect(vertex_hdl_t conn_vhdl,
		     ioc3reg_t intrbits,
		     ioc3_intr_func_f *intr,
		     intr_arg_t info,
		     vertex_hdl_t owner_vhdl)
{
    graph_error_t	    rc;
    vertex_hdl_t	    ioc3_vhdl;
    ioc3_soft_t		   *soft;
    ioc3reg_t		    bits;
    int			    i, num_intrs;

    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    if (rc != GRAPH_SUCCESS) {
	cmn_err(CE_ALERT,
		"%v: ioc3_intr_connect: ioc3_attach not yet called",
		owner_vhdl);
	return;
    }
    soft = ioc3_soft_get(ioc3_vhdl);
    ASSERT(soft != NULL);

    num_intrs = soft->is_num_intrs;
    for (i = 0; i < num_intrs; ++i) {
	if ((soft->is_intr_info[i].sd_intr == intr) &&
	    (soft->is_intr_info[i].sd_info == info) &&
	    (soft->is_intr_info[i].sd_vhdl == owner_vhdl) &&
	    (bits = soft->is_intr_info[i].sd_bits & intrbits)) {
	    (void) atomicClearInt((int *) &soft->is_intr_info[i].sd_bits, bits);
	    (void) atomicClearInt((int *) &soft->is_intr_bits_busy, bits);
	    if (!(soft->is_intr_info[i].sd_bits)) {
		soft->is_intr_info[i].sd_intr = NULL;
		soft->is_intr_info[i].sd_info = NULL;
		soft->is_intr_info[i].sd_vhdl = GRAPH_VERTEX_NONE;
		(void) atomicSetInt((int *) &soft->is_intr_ents_free, 1 << i);
	    }
	}
    }
}

#if RTINT_WAR
void
ioc3_intr(intr_arg_t arg)
{
    ioc3_handle_intr(arg, 1);
}
#endif

/* Top level IOC3 interrupt handler. Farms out the interrupt to
 * the various IOC3 device drivers.
 */
void
#if RTINT_WAR
ioc3_handle_intr(intr_arg_t arg, int real_intr)
#else
ioc3_intr(intr_arg_t arg)
#endif
{
    ioc3_soft_t		   *soft;
    ioc3reg_t		    sio_ir;
    ioc3reg_t		    sio_mir;
    int			    x, num_intrs;
    int			    svcd = 0;

    soft = arg ? (ioc3_soft_t *) arg : the_console_ioc3;
    if (!soft)
	return;			/* polled but no console ioc3 registered */

    num_intrs = soft->is_num_intrs;

    while (sio_ir = ioc3_pending_intrs(soft)) {

#if RTINT_WAR
	if (!real_intr)
	    /* the realtime interrupt should never be serviced by the
	     * polling interrupt handler since it will probably be
	     * executing on the wrong cpu.
	     */
	    sio_ir &= ~SIO_IR_RT;

	else if (soft->is_poll_state & POLL_MODE)
	    /* similarly, polled interrupts must not be serviced in
	     * the real interrupt path or they will degrade realtime
	     * performance
	     */
	    sio_ir &= ~POLL_INTR_MASK;

	if (sio_ir == 0)
	    break;
#endif

	/* farm out the interrupt to the various drivers depending on
	 * which interrupt bits are set.
	 */
	for (x = 0; x < num_intrs; x++) {
	    struct ioc3_intr_info  *ii = &soft->is_intr_info[x];

	    if (sio_mir = sio_ir & ii->sd_bits) {
		if ((ii->sd_flags & THD_OK) &&
		    (ii->sd_use_ithread == 0 || ii->sd_use_ithread(ii->sd_info))) {
#ifdef ITHREAD_LATENCY
		    xthread_set_istamp(ii->sd_lat);
#endif
		    /* Mask any more occurences. */
		    /* The subdevice interrupt threads
		     * will hit their bits in sio_ies
		     * before they psema.
		     */
		    IOC3_WRITE_IEC(soft, ii->sd_bits);
		    vsema(&ii->sd_isync);
		}
		else
		    ii->sd_intr(ii->sd_info, sio_mir);
		sio_ir &= ~sio_mir;
		svcd++;
	    }
	}

	atomicAddInt((int *)&SYSINFO.intr_svcd, svcd-1);

	if (sio_ir)
	    cmn_err(CE_ALERT, "unknown IOC3 interrupt 0x%x\n", sio_ir);
    }

#if IOC3_PIO_MODE && P1IO6_PASSTHRU_INTR_KLUDGE
    /* P1 IO6 boards were built up without connecting the superio intr
     * line to the IOC3 passthru intr, thus IOC3_PENDING_INTRS above
     * never sees an intr. We must poll the 16550 in order to run the
     * console in PIO mode.
     */

    /* find the intr callback for the passthru lines and call it
     * regardless of whether the bit is actually pending
     */
    {
	int			x;

	for (x = 0; x < soft->is_num_intrs; x++) {
	    struct ioc3_intr_info  *ii = &soft->is_intr_info[x];

	    if (sio_mir = (ii->sd_bits & (SIO_IR_SA_INT | SIO_IR_SB_INT))) {
		IOC3_WRITE_IEC(soft, sio_mir);
		vsema(&ii->sd_isync);
	    }
	}
    }
#endif
}

int
ioc3_is_console(vertex_hdl_t conn_vhdl)
{
    graph_error_t	    rc;
    vertex_hdl_t	    ioc3_vhdl;
    ioc3_soft_t		   *soft;

    if (the_console_ioc3 == NULL)
	return 0;
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
    if (rc != GRAPH_SUCCESS) {
#if DEBUG && (ATTACH_DEBUG || IOC3_DEBUG)
	cmn_err(CE_ALERT, "%v ioc3_is_console: no IOC3 on this connect point", conn_vhdl);
#endif
	return 0;
    }
    soft = ioc3_soft_get(ioc3_vhdl);
    return (soft == the_console_ioc3);
}

/*ARGSUSED */
/* Called by pciio_error_handler
   ** via the registration in pciio_info
   ** if an error, report the error device and error address.
 */
int
ioc3_error_handler(void *einfo,
		   int error_code,
		   ioerror_mode_t mode,
		   ioerror_t *ioerror)
{
    ioc3_soft_t		   *soft = (ioc3_soft_t *) einfo;
    ioc3reg_t		    err;
    ioc3reg_t		    addr_l, addr_h;
    int			    master_id;
    char		    dev_name[25];
    iopaddr_t		    dev_addr;

#if DEBUG && ERROR_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_error_handler\n", soft->is_ioc3_vhdl);
#endif

    /* get status */
    err = PCI_INW(&soft->is_ioc3_cfg->pci_scr);
    /* decode error */
    if ((err & PCI_SCR_SIG_PAR_ERR) || (err & PCI_SCR_PAR_ERR))
	strcpy(dev_name, "parity error");
    if ((err & PCI_SCR_RX_SERR) || (err & PCI_SCR_SIG_SERR))
	strcpy(dev_name, "SERR");
    if (err & PCI_SCR_SIG_TAR_ABRT)
	strcpy(dev_name, "signalled target abort");
    if (err & PCI_SCR_RX_TAR_ABRT)
	strcpy(dev_name, "received target abort");
    if (err & PCI_SCR_SIG_MST_ABRT)
	strcpy(dev_name, "issued master abort");

    cmn_err(CE_NOTE, "IOC3 %s", dev_name);

    /* read error register */
    err = PCI_INW(&soft->is_ioc3_cfg->pci_err_addr_l);

    if (err & PCI_ERR_ADDR_VLD) {

	dprintf(("memory error occurred"));
	addr_l = err & PCI_ERR_ADDR_ADDR_MSK;
	addr_h = PCI_INW(&soft->is_ioc3_cfg->pci_err_addr_h);
	dev_addr = addr_h;
	dev_addr = (dev_addr << 32) | addr_l;

	master_id = (err & PCI_ERR_ADDR_MST_ID_MSK) >> 1;
	switch (master_id) {
	case IOC3_MST_ID_SA_TX:
	    strcpy(dev_name, "serial port A transmiter");
	    break;
	case IOC3_MST_ID_SA_RX:
	    strcpy(dev_name, "serial port A receiver");
	    break;
	case IOC3_MST_ID_SB_TX:
	    strcpy(dev_name, "serial port B transmitter");
	    break;
	case IOC3_MST_ID_SB_RX:
	    strcpy(dev_name, "serial port B receiver");
	    break;
	case IOC3_MST_ID_ECPP:
	    strcpy(dev_name, "parallel port");
	    break;
	case IOC3_MST_ID_ETX_DESC_READ:
	    strcpy(dev_name, "enet tx descriptor read");
	    break;
	case IOC3_MST_ID_ETX_BUF1_READ:
	    strcpy(dev_name, "enet tx buf1 read");
	    break;
	case IOC3_MST_ID_ETX_BUF2_READ:
	    strcpy(dev_name, "enet tx buf2 read");
	    break;
	case IOC3_MST_ID_ERX_DESC_READ:
	    strcpy(dev_name, "enet rx descriptor read");
	    break;
	case IOC3_MST_ID_ERX_BUF_WRITE:
	    strcpy(dev_name, "enet rx buffer write");
	    break;
	default:
	    strcpy(dev_name, "unknown address unit");
	    break;

	}

	cmn_err(CE_NOTE, "IOC3 Error ID %s Error Addr 0x%x",
		dev_name, dev_addr);

	PCI_OUTW(&soft->is_ioc3_cfg->pci_err_addr_l, 0x1);	/* rearm */
    }
    return IOERROR_HANDLED;
}

static void
ioc3_intrd(struct ioc3_intr_info *iip)
{
    ioc3_mem_t		   *mem = iip->sd_soft->is_ioc3_mem;
    ioc3reg_t		    sio_mir;
    extern int		    pending(ioc3_mem_t *);

    /* See which bits are pending
     * Don't use IOC3_PENDING_INTRS, since the bits we're interested
     * in are already masked.  Go directly to the status register.
     */
    sio_mir = (PCI_INW(&mem->sio_ir)) & iip->sd_bits;

    /* Call the handler
     * NOTE: each handler must re-enable it's own bits.				 This is
     *	because some handlers represent more than one bit, and may
     *	not want to reenable all of them.
     */
    iip->sd_intr(iip->sd_info, sio_mir);

    ipsema(&iip->sd_isync);
    /* NOTREACHED */
}

static void
ioc3_intrd_setup(struct ioc3_intr_info *iip)
{
    xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *) ioc3_intrd, iip);
    atomicSetInt(&iip->sd_flags, THD_INIT);
    ipsema(&iip->sd_isync);
    /* NOTREACHED */
}

#if SN0
vertex_hdl_t
ioc3_console_vhdl_get(void)
{
    if (the_console_ioc3)
	return the_console_ioc3->is_ioc3_vhdl;

    return GRAPH_VERTEX_NONE;
}
#endif

/* =====================================================================
 *    IOC3 Subdevice management
 */

/* ioc3_subdev_ptr:
 * Get the "subdev enable" data for this vertex,
 * which should be the HOST vertex in a dual-slot
 * mode IOC3. NULL is retured if no subdev spec
 * has been made.
 */
static ulong_t	       *
ioc3_subdev_ptr(vertex_hdl_t vhdl, int alloc)
{
    graph_error_t	    rc;
    arbitrary_info_t	    ainfo = 0;

    hwgraph_traverse(vhdl, EDGE_LBL_HOST, &vhdl);
    rc = hwgraph_info_get_LBL(vhdl, INFO_LBL_SUBDEVS, &ainfo);
    if (alloc && (rc != GRAPH_SUCCESS)) {
	ulong_t		       *subdevs;

	NEW(subdevs);
	*subdevs = IOC3_STD_SUBDEVS;
#if DEBUG && IOC3_DEBUG
	cmn_err(CE_CONT, "%v: ioc3_subdev_ptr:\n"
		"\tallocated %d bytes for subdev map at 0x%X, initial value 0x%X\n",
		vhdl, sizeof(*subdevs), subdevs, *subdevs);
#endif
	hwgraph_info_add_LBL(vhdl, INFO_LBL_SUBDEVS,
			     (arbitrary_info_t) subdevs);

	/* reentrancy protection: someone else may have
	 * set it at the same time. read back whatever
	 * got into the graph.
	 */
	ainfo = 0;
	rc = hwgraph_info_get_LBL(vhdl, INFO_LBL_SUBDEVS, &ainfo);
	ASSERT(rc == GRAPH_SUCCESS);

	if (ainfo != (arbitrary_info_t) subdevs)
	    DEL(subdevs);
    }
    return (ulong_t *) ainfo;
}

/*
 * ioc3_subdev_enabled:
 * return nonzero if the specified subdevice is
 * enabled on the IOC3 connected to this vertex.
 */
int
ioc3_subdev_enabled(vertex_hdl_t vhdl, int subdev)
{
    ulong_t		   *subdevp;
    ulong_t		    subdevs;
    ulong_t		    rv;

    subdevp = ioc3_subdev_ptr(vhdl, 0);
    subdevs = subdevp ? *subdevp : IOC3_STD_SUBDEVS;
    rv = subdevs & (1 << subdev);
#if DEBUG && IOC3_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_subdev_enabled:\n"
	    "\tsubdevp=0x%X subdevs=0x%X subdev=%d, returning 0x%X\n",
	    vhdl, subdevp, subdevs, subdev, rv);
#endif
    return rv;
}

/*
 * ioc3_subdev_enables:
 * specify all subdevice enables for this vertex.
 */
void
ioc3_subdev_enables(vertex_hdl_t vhdl, ulong_t mask)
{
    ulong_t		   *subdevp;

    subdevp = ioc3_subdev_ptr(vhdl, 1);
#if DEBUG && IOC3_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_subdev_enables:\n"
	    "\tsubdevp=0x%X setting mask=0x%X\n",
	    vhdl, subdevp, mask);
#endif
    if (subdevp)
	*subdevp = mask;
}

/*
 * ioc3_subdev_enable:
 * Turn on the subdev enable bit for this device.
 */
void
ioc3_subdev_enable(vertex_hdl_t vhdl, int subdev)
{
    ulong_t		   *subdevp;

    subdevp = ioc3_subdev_ptr(vhdl, 1);
#if DEBUG && IOC3_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_subdev_enable:\n"
	    "\tsubdevp=0x%X enabling subdev %d\n",
	    vhdl, subdevp, subdev);
#endif
    if (subdevp)
	atomicSetUint64(subdevp, 1 << subdev);
}

/*
 * ioc3_subdev_disable:
 * Turn off the subdev enable bit for this device.
 */
void
ioc3_subdev_disable(vertex_hdl_t vhdl, int subdev)
{
    ulong_t		   *subdevp;

    subdevp = ioc3_subdev_ptr(vhdl, 1);
#if DEBUG && IOC3_DEBUG
    cmn_err(CE_CONT, "%v: ioc3_subdev_enable:\n"
	    "\tsubdevp=0x%X disabling subdev %d\n",
	    vhdl, subdevp, subdev);
#endif
    if (subdevp)
	atomicClearUint64(subdevp, 1 << subdev);
}

static int
ioc3_intb_needed(vertex_hdl_t vhdl)
{
    ulong_t		   *subdevp;

    subdevp = ioc3_subdev_ptr(vhdl, 0);
    return ((subdevp == NULL) ||
	    ((*subdevp & IOC3_INTB_SUBDEVS) != 0));
}

ioc3_mem_t	       *
ioc3_mem_ptr(void *arg)
{
    ioc3_soft_t		   *ioc3_soft;

    ioc3_soft = (ioc3_soft_t *) arg;
    return ioc3_soft->is_ioc3_mem;
}
