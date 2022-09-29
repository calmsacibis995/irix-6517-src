#ident	"lib/libsk/ml/bridge.c:  $Revision: 1.87 $"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * bridge.c - bridge support
 */
/*
 * NOTE:
 * This file needs to be shared between the RACER and SN0 projects.
 * At this time, (10/17/95), since it is not yet decided  as to how
 * to do it, I will do the following:
 * Add routines with different names, if they have a functionality
 * that is different from the RACER routines. If there is a common
 * functionality, I will try to use ifdef SN0.
 * For example, SN0 needs a init_xbow_discover, which disables
 * interrupts during discovery. This will be different from the
 * general 'init_xbow' routine. Also, the SN0 registers reside
 * in the HUB's IO Space and not PHYS_TO_K1. So, every 
 * assignment statement will be different.
 * We also need to decide if we can come up with a common set of
 * utility routines that both of us can use. One example would be:
 * have routines, init_intr_handler, init_heart_bridge, init_hub_bridge
 * and have init_bridge call them in the right order.
 * At this stage, it is important to read the comments to understand
 * what is going on, rather than the code.
 *                                      -- Srinivasa Prasad
 *                                         sprasad@engr.sgi.com
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#if IP30
#include <standcfg.h>
#include <sys/nic.h>
static void real_init_bridge(bridge_t *bridge);
#endif
#include <sys/PCI/PCI_defs.h>
#ifdef SN0
#include <sys/PCI/bridge.h>
#include <sys/SN/xbow.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
extern int sk_sable ;
static void kl_bridge_set_ihdlr(bridge_t *bp);
static void kl_disable_bridge_intrs(bridge_t *bp);
void kl_enable_bridge_intrs(bridge_t *, __uint32_t) ;
#endif

static void bridge_clearerrors(bridge_t *bridge);

/* Make it easy to turn printf's on and off. */
#ifdef DEBUG
#define	DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

static void size_bridge_ssram(bridge_t *);
static void init_bridge_rrb(bridge_t *);

#ifdef IP30

static int	num_bridges;		/* XXX why nbridge and num_bridges? */

/*
 * Bridge ASIC "early" initialization
 *	init_bridge is called for each widget that the caller thinks
 *	is a bridge, to do early initialization. We want to count the
 *	bridges, but only initialize the BaseIO bridge (which will
 *	always be the first one through the gate).
 *
 *	Expansion bridges will be initialized when the "install"
 *	entry points are being called.
 */
void
init_bridge(widget_cfg_t *widget)
{
	if (!num_bridges)
		real_init_bridge((bridge_t *)widget);
	num_bridges++;
}

/*
 * Bridge ASIC initialization
 *	real_init_bridge sets up the registers for the specified
 *	bridge ASIC. It is called for the BaseIO bridge during early
 *	initialization time, and for Expansion bridges when device
 *	driver "install" routines are being called.
 *
 *	Do not use malloc.
 *	Do not use printf.
 */
static void
real_init_bridge(bridge_t *bridge)
{
    static void		init_bridge_pci(bridge_t *bridge);
    int			vec, wid;

    /* set target ID and address for Bridge interrupts */
    wid = bridge->b_wid_control & WIDGET_WIDGET_ID;
    vec = WIDGET_ERRVEC(wid);
    bridge->b_wid_int_upper = INT_DEST_UPPER(vec);
    bridge->b_wid_int_lower = INT_DEST_LOWER;

    /* specify interrupt bit number for errors */
    bridge->b_int_host_err = vec;

#ifdef HEART_CHIP
    /* turn off the ADD512 bit in DIRMAP.XXX-should be on when mapping ready*/
    /* set heart as target register */
    bridge->b_dir_map = HEART_ID << BRIDGE_DIRMAP_W_ID_SHFT;
#else
    /* turn off the ADD512 bit in DIRMAP. */
    bridge->b_dir_map &= ~BRIDGE_DIRMAP_ADD512;
#endif

    /* disable all interrupts and clear all pending interrupts */
    bridge->b_int_enable = 0x0;
    bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

    init_bridge_rrb(bridge);
    if (bridge->b_wid_stat & BRIDGE_STAT_PCI_GIO_N)
	init_bridge_pci(bridge);
    size_bridge_ssram(bridge);

#if _PAGESZ == 4096
    bridge->b_wid_control &= ~BRIDGE_CTRL_PAGE_SIZE;	/* 4K page size */
#else
    bridge->b_wid_control |= BRIDGE_CTRL_PAGE_SIZE;	/* 16K page size */
#endif	/* _PAGESZ */
    bridge->b_wid_control;		/* inval addr bug war */

    /* clear anything hanging about */
    bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

    /* enable bridge error reporting */
    bridge->b_int_enable |=
	(BRIDGE_IRR_CRP_GRP |
	 BRIDGE_IRR_RESP_BUF_GRP |
	 BRIDGE_IRR_REQ_DSP_GRP |
	 BRIDGE_IRR_LLP_GRP |
	 BRIDGE_IRR_SSRAM_GRP |
	 BRIDGE_IRR_PCI_GRP |
	 BRIDGE_IRR_GIO_GRP);

    /* don't want an interrupt on every LLP micropacket retry */
    bridge->b_int_enable &= ~(BRIDGE_ISR_LLP_REC_SNERR |
	BRIDGE_ISR_LLP_REC_CBERR | BRIDGE_ISR_LLP_TX_RETRY);

#ifdef QL_SCSI_CTRL_WAR
    /* Really a QL rev A issue, but all newer hearts have newer QLs */
    if (heart_rev() == HEART_REV_A)
    	bridge->b_int_enable &= ~BRIDGE_IMR_PCI_MST_TIMEOUT;
#endif	/* QL_SCSI_CTRL_WAR */
#ifdef BRIDGE1_TIMEOUT_WAR			/* only on rev A bridges */
    if (XWIDGET_REV_NUM(bridge->b_wid_id) == 1) {
    	bridge->b_int_enable &=
		~(BRIDGE_IMR_XREAD_REQ_TIMEOUT | BRIDGE_IMR_UNEXP_RESP);
    	bridge->b_arb = 0x2ff08;
    	bridge->b_pci_bus_timeout = 0x0fffff;
    }
#endif	/* BRIDGE1_TIMEOUT_WAR */
}

#endif	/* IP30 */


/*
 * Read Response Buffer allocation
 */

void
init_bridge_rrb(bridge_t *bridge)
{
    /* disable and deallocate all of this bridge's buffers */
    bridge->b_even_resp = 0;
    bridge->b_odd_resp = 0;
}

int
alloc_bridge_rrb(void *bus_base, int bus_slot, int virtual, int howmany)
{
	bridge_t	       *bridge = (bridge_t *)bus_base;
	volatile bridgereg_t   *rrbp;
	bridgereg_t		rrb;
	unsigned		shift;
	unsigned		field;
	unsigned		got = 0;

	if (bus_slot & 1)
		rrbp = &bridge->b_odd_resp;
	else
		rrbp = &bridge->b_even_resp;

	rrb = *rrbp;

	field = 0x8|(virtual?0x4:0)|(bus_slot>>1);

	for (shift = 0; (got < howmany) && (shift < 32); shift += 4) {
		if (rrb & (8<<shift)) {
			/* take into account previously allocated RRBs */
			if ((rrb & (3<<shift)) == (bus_slot>>1))
				got++;
			continue;
		}
		rrb &= ~(0xF << shift);
		rrb |= field << shift;
		got ++;
	}
	*rrbp = rrb;
	return got;
}

int
free_bridge_rrb(void *bus_base, int bus_slot, int virtual, int howmany)
{
	bridge_t	       *bridge = (bridge_t *)bus_base;
	volatile bridgereg_t   *rrbp;
	bridgereg_t		rrb;
	unsigned		shift;
	unsigned		match;

	if (bus_slot & 1)
		rrbp = &bridge->b_odd_resp;
	else
		rrbp = &bridge->b_even_resp;

	rrb = *rrbp;

	match = 0x8|(virtual?4:0)|(bus_slot>>1);

	for (shift = 0; shift < 32; shift += 4) {
		if ((rrb & (0xF<<shift)) != match)
			continue;
		rrb &= ~(0xF<<shift);
		if (!--howmany)
			break;
	}
	*rrbp = rrb;
	return howmany;
}

/* set bridge interrupt vector for INT(bus_slot) to vector,
 * and enable the interrupt in bridge */
void
set_bridge_vector(void *bus_base, int bus_slot, int vector)
{
    bridge_t   *bridge = (bridge_t *)bus_base;
    int		imask = 1<<bus_slot;

    bridge->b_int_enable &= ~imask;
    bridge->b_int_addr[bus_slot].addr =
	(bridge->b_int_addr[bus_slot].addr &~ BRIDGE_INT_ADDR_FLD) | vector;
    bridge->b_int_enable |= imask;
}

/* disable bridge interrupts at INT(bus_slot)
 */
void
disable_bridge_intr(void *bus_base, int bus_slot)
{
    bridge_t	*bridge = (bridge_t *)bus_base;
    int		imask = 1<<bus_slot;

    bridge->b_int_enable &= ~imask;
    flushbus();
}

/* indexes into bridge_ssram for the word after the SSRAM, which
 * aliases back onto the word at zero (except when it is out of range).
 */

#define	SSRAM_64K	(( 64 * 1024) / 8)		/*  32k by 16 or 18 */
#define	SSRAM_128K	((128 * 1024) / 8)		/*  64k by 16 or 18 */
#define	SSRAM_512K	((512 * 1024) / 8)		/* 256k by 16 or 18 */
/*
 * size a Bridge ASIC's external SSRAM and determine whether it has parity
 * protection or not, then set its control register accordingly
 */
static void
size_bridge_ssram(bridge_t *bridge)
{
	bridgereg_t		cntrl;
	bridge_ate_p		bridge_ssram = bridge->b_ext_ate_ram;
	bridge_ate_p		ssram_last_addr;
	bridgereg_t		ssram_size;
	bridge_ate_p		ptr;

	bridgereg_t		old_int_enable;

	/* assume maximum external SSRAM size */
	cntrl = bridge->b_wid_control;
	cntrl &= ~(BRIDGE_CTRL_SSRAM_SIZE_MASK|
		   BRIDGE_CTRL_SS_PAR_EN|
		   BRIDGE_CTRL_SS_PAR_BAD);

#ifndef IP30_RPROM
	cntrl |= BRIDGE_CTRL_SSRAM_512K; /* preshifted */
	bridge->b_wid_control = cntrl;
	bridge->b_wid_control;		/* inval addr bug war */

	/*
	 * rev 1.1 Bridge spec section 10.1.3, write to SSRAM must be in
	 * 64-bits. don't need to clobber the data lines afterward
	 * since there're only 16 data lines into the SSRAM and the upper
	 * 48 bits of data (all zeros) should clear the data lines
	 */

#if 0	/* useful for verifying the "real" size */
	ssram_size = 512 << 10;
	bridge_ssram[0] = ssram_size;
	while ((ssram_size >>= 1) > 0x800)
		bridge_ssram[ssram_size/8] = ssram_size;
	bridge_ssram[0];	/* force read for visibility */
#endif

	bridge_ssram[0] = 0x1111;
	bridge_ssram[SSRAM_128K] = 0x2222;
	bridge_ssram[SSRAM_64K] = 0x4444;

	switch (bridge_ssram[0]) {
	case 0x1111:
		ssram_size = BRIDGE_CTRL_SSRAM_512K;
		ssram_last_addr = &bridge_ssram[SSRAM_512K];
		break;

	case 0x2222:
		ssram_size = BRIDGE_CTRL_SSRAM_128K;
		ssram_last_addr = &bridge_ssram[SSRAM_128K];
		break;

	case 0x4444:
		ssram_size = BRIDGE_CTRL_SSRAM_64K;
		ssram_last_addr = &bridge_ssram[SSRAM_64K];
		break;

	/* internal SSRAM only, getting garbage back */
	default:
		ssram_size = BRIDGE_CTRL_SSRAM_1K;
		break;
	}

	cntrl &= ~BRIDGE_CTRL_SSRAM_SIZE_MASK;
	cntrl |= ssram_size;	/* preshifted */

	/*
	 * If we have no external SSRAM, we are done.
	 */
	if (ssram_size == BRIDGE_CTRL_SSRAM_1K) {
		bridge->b_wid_control = cntrl;
		bridge->b_wid_control;		/* inval addr bug war */
		return;
	}

	/*
	 * Check to see whether we are using x18 SSRAMs that
	 * provide real parity checking, or x16 SSRAMS that just
	 * tie the parity bit either high or low, by writing
	 * 0x0000 and 0x0101 with bad parity, and verifying that
	 * we get a parity error indication in Bridge when we
	 * read them back. If both patterns give us parity error
	 * indications, we will use SSRAM parity checking in the
	 * bridge; otherwise, it gets turned back off.
	 *
	 * XXX- shouldn't we also verify that we do *not* get
	 * parity errors when we write these patterns without
	 * the SS_PAR_BAD bit turned on?
	 */
	bridge->b_wid_control =
		(cntrl |
		 BRIDGE_CTRL_SS_PAR_EN |
		 BRIDGE_CTRL_SS_PAR_BAD);
	bridge->b_wid_control;		/* inval addr bug war */

	/*
	 * Errors do not appear in IntStatus unless they
	 * are enabled, so enable the bit. We may need to
	 * tell other code to ignore the SSRAM parity errors.
	 */
	old_int_enable = bridge->b_int_enable;
	bridge->b_int_enable = old_int_enable | BRIDGE_ISR_SSRAM_PERR;

	/* clear out any old stale errors */
	bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

	bridge_ssram[0] = 0x0000;	/* writes bad parity */
	/*
	 * forces read, discards data.  cannot just read the location
	 * since we don't want the processor to get a DBE exception
	 */
	if (badaddr(bridge_ssram, sizeof(__uint64_t))) {
		bridge->b_ram_perr;	/* forces read, discards data */
		bridge->b_int_rst_stat = BRIDGE_IRR_SSRAM_GRP;
		bridge->b_wid_tflush;
		flushbus();

		bridge_ssram[0] = 0x0101;	/* write bad parity again */
		if (badaddr(bridge_ssram, sizeof(__uint64_t))) {
			bridge->b_ram_perr;	/* forces read as above */
			bridge->b_int_rst_stat = BRIDGE_IRR_SSRAM_GRP;
			bridge->b_wid_tflush;
			flushbus();

			cntrl |= BRIDGE_CTRL_SS_PAR_EN;
		}
	}

	/*
	 * Write final bridge configuration control, with correct size,
	 * parity enabled if we want it, and bad parity forcing turned
	 * off if we had it on.
	 */
	bridge->b_wid_control = cntrl;
	bridge->b_wid_control;		/* inval addr bug war */

	/*
	 * Restore BRIDGE_ISR_SSRAM_PERR to previous state.
	 */
	bridge->b_int_enable = old_int_enable;
		
	/*
	 * Initialize the SSRAM (including parity, if present).
	 * Skip when running SABLE? Probably not a good idea.
	 */
	for (ptr = bridge_ssram; ptr < ssram_last_addr; ptr++)
		*ptr = 0x0;
#else /* IP30_RPROM */
	/*
	 * for RPROM skip the probing during ext. ssram sizing
	 * and just assume no ext ssram
	 */
	cntrl &= ~BRIDGE_CTRL_SSRAM_SIZE_MASK;
	cntrl |= BRIDGE_CTRL_SSRAM_1K;	/* preshifted */

	bridge->b_wid_control = cntrl;
	bridge->b_wid_control;		/* inval addr bug war */
#endif /* IP30_RPROM */
}

/*
 * One of our PCI devices, the IOC3, only responds to 32-bit
 * accesses over the PCI bus. These routines will read and
 * write the 32-bit word containing the desired offset,
 * and take care of the byteswap; it is up to the caller to
 * extract the data they want, and do get/mask/insert/put
 * sequences when writing anything less than a 32-bit word.
 *
 * If this gets to be a burden, we can insert a layer above
 * the get_word and put_word routines that handles extraction
 * and get/modify/put sequences. Currently, it is not much
 * of a burden at all.
 *
 * Bear in mind that when you get a word, lower-address PCI bytes are
 * in the more-significant position. So, for instance, the 16-bit
 * VendorID register is at 0x00, so it will show up in the upper half
 * of the word read from 0x00.
 *
 * Doing it this way makes the values of the various 16 and 32 bit
 * registers come out right.
 */
unsigned
pci_get_cfg_word(volatile unsigned char *cfg, unsigned offset)
{
    return *(__uint32_t *)(cfg + (offset & ~3));
}

void
pci_put_cfg_word(volatile unsigned char *cfg, unsigned offset, unsigned value)
{
    *(__uint32_t *)(cfg + (offset & ~3)) = value;
}

/*
 * Probe the PCI vendor and part number,
 * return zero if a fault occurred.
 */
unsigned
pci_get_vendpart_nofault(bridge_t *bridge, volatile unsigned char *cfg)
{
    unsigned	rv;
    bridgereg_t	oie = bridge->b_int_enable;

    bridge->b_int_enable = oie | BRIDGE_ISR_PCI_MST_TIMEOUT;
#ifndef IP30_RPROM
    if (badaddr_val(cfg, sizeof(__uint32_t), (volatile void *)&rv)) {
	rv = 0;
    }
#else
    rv = *(volatile __int32_t *)cfg;
    *(volatile __int32_t *)cfg = (__int32_t)rv;
#endif /* !IP30_RPROM */
    bridge->b_int_enable = oie;

    return rv;
}

#if IP30

/*
 * Bridge/PCI "discovery" code for IP30.
 */

static bridge_t	       *bridges[MAX_XBOW_PORTS];
static int		nbridge;

static COMPONENT bridgetmpl = {
	AdapterClass,			/* Class */
	PCIAdapter,			/* Type  */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	0				/* Identifier */
};

int
bridge_install(COMPONENT *parent,slotinfo_t *bslot)
{
    bridge_t	       *bridge;
    int			slot;
    slotinfo_t		cslot;
    bridgereg_t		pci_gio_bit;
    bridgereg_t		wid;
    COMPONENT		bridge_comp = bridgetmpl;
    COMPONENT	       *self;

    /*
     * the bridge control register wid field has been set to the
     * correct xbow port earlier at init_xbow
     * later bus_slot is assigned the correct value by heart_do_slots()
     */
    wid = bslot->bus_slot;
    bridge = (bridge_t *)bslot->mem_base;

    /* do early for nofault */
    bridges[nbridge] = bridge;		/* remember bridges for nofault */
    nbridge++;

    /*
     * don't want to initialize the BaseIO bridge twice
     * and set nic name for bridge.
     */
    if (wid != BRIDGE_ID) {
	char *buf, *name_str, *nbuf;
	nic_data_t mcr;
	int n;

	real_init_bridge(bridge);
	buf = malloc(1024);
	if (buf) {
	    mcr = (nic_data_t)&bridge->b_nic;
	    slot = cfg_get_nic_info(mcr, buf);
	    if (slot == NIC_OK || slot == NIC_DONE) {
	        if (n = nic_item_info_get(buf, "Name:", &name_str)) {
		    nbuf = malloc(n + 1);

		    bcopy(name_str, nbuf, n);
		    nbuf[n] = '\0';
		    bridge_comp.IdentifierLength = n;
		    bridge_comp.Identifier = nbuf;
	        }
	    } /* if NIC_OK || NIC_DONE */
	}
	free(buf);
    }

    self = AddChild(parent, &bridge_comp, (void *)0);
    self->Key = wid;
    if (bridge_comp.Identifier)
	free(bridge_comp.Identifier);

DPRINTF(("bridge%d ID=%d at 0x%X\n", nbridge-1, wid, bridge));

    pci_gio_bit = bridge->b_wid_stat & BRIDGE_STAT_PCI_GIO_N;

DPRINTF(("bridge%d: scanning %s slots\n",
       nbridge-1, pci_gio_bit ? "PCI" : "GIO"));
    if (pci_gio_bit) {
	for (slot = 0; slot < 8; ++slot) {
	    unsigned		mfgr;
	    unsigned		part;
	    unsigned		rev;
	    volatile unsigned char *cfg;
	    volatile unsigned char *mem;

	    unsigned		pciwbase;
	    unsigned		pcidbase;

	    unsigned		devreg;

	    cfg = bridge->b_type0_cfg_dev[slot].c;
	    mem = 0;

	    DPRINTF(("bridge%d pci slot%d ...\n", nbridge-1, slot));

#ifdef IP30_RPROM
	    if (slot > 3) /* XXX dont do RAD at 3? */
	    	continue;
#endif
	    mfgr = pci_get_vendpart_nofault(bridge, cfg);
	    part = mfgr >> 16;
	    mfgr = mfgr & 0xFFFF;
	    if (!mfgr) {
		DPRINTF(("bridge%d pci slot%d empty\n", nbridge-1, slot));
		continue;
	    }

	    rev = 0xFF & pci_get_cfg_word(cfg, PCI_CFG_REV_ID);

	    DPRINTF(("bridge%d: I see vend=%x part=%x rev=%x in PCI slot %d\n",
		     nbridge-1, mfgr, part, rev, slot));

	    devreg = bridge->b_device[slot].reg;
	    pciwbase = (devreg & BRIDGE_DEV_OFF_MASK) << 20;
	    pcidbase = pci_get_cfg_word(cfg, PCI_CFG_BASE_ADDR(0));
	    if (pcidbase && (pcidbase != 0xFFFFFFFFu)) {
		if (pcidbase & 1)
		    pcidbase &= 0xFFFFFFFCu;
		else
		    pcidbase &= 0x3FFFFFF0u;
		mem = bridge->b_devio(slot).c + pcidbase - pciwbase;
	    } else {
		mem = 0;
	    }

	    cslot.id.bus = BUS_PCI;
	    cslot.id.mfgr = mfgr;
	    cslot.id.part = part;
	    cslot.id.rev = rev;
	    cslot.cfg_base = cfg;
	    cslot.mem_base = mem;
	    cslot.bus_base = (void *)bridge;
	    cslot.bus_slot = slot;

	    slot_register(self,&cslot);
	}
    /*
     * For non BaseIO PCI and GIO Bridge cards we have done minimal
     * amount of HW initialization and no need setup child device
     * structures ... etc
     */
    } else if (pci_gio_bit == 0) {	/* we are GIO */

	self->Type = GIOAdapter; /* backpatch our COMPONENT Type */

	/* that's it, dont do anymore */
DPRINTF(("bridge%d: GIO bus not probing for device\n", nbridge-1));

    }
DPRINTF(("bridge%d complete\n", nbridge-1));
    return 0;
}

#define	IOC3_ID		((IOC3_DEVICE_ID_NUM << 16) | IOC3_VENDOR_ID_NUM)

/*
 * init_bridge_pci: set up the PCI bus.
 */
static void
init_bridge_pci(bridge_t *bridge)
{
    int			slot;
    unsigned		pci_as_i32;
    unsigned		pci_as_m20;
    unsigned		pci_as_m30;
    unsigned		pci_hi_i32;
    unsigned		pci_hi_m20;
    unsigned		pci_hi_m30;

    pci_as_m20 = 0x00000010;
    pci_hi_m20 = 0x000FFFFF;
    pci_as_m30 = 0x00100000;
    pci_hi_m30 = 0x3FFFFFFF;
    pci_as_i32 = 0x00000010;
    pci_hi_i32 = 0xFFFFFFFF;

    for (slot = 0; slot < 8; ++slot) {
	unsigned		pci_id;
	volatile unsigned char *cfg;
	int			window;
	unsigned		align;
	unsigned		pciobits;
	unsigned		pcimbits;
	unsigned		mask;
	unsigned		size;
	unsigned		pcibase;
	unsigned		pcilimit;
	unsigned	       *pci_as_p;
	unsigned	       *pci_hi_p;

	cfg = bridge->b_type0_cfg_dev[slot].c;

#ifdef IP30_RPROM	/* If building recovery prom, */
	if (slot > 3)	/*XXX scrach the RAD at 3? */
	    continue;
#endif
	pci_id = pci_get_vendpart_nofault (bridge, cfg);
	if (pci_id == 0x0)
	    continue;

	/* Prealign I/O and MEM spaces to 2M for
	 * the first two slots, and 1M for the rest,
	 * so the first window lands at the beginning
	 * of a DevIO(x) register and maximizes the
	 * chances of mapping all this device's windows
	 * with one DevIO(x) window.
	 */
	align = (slot < 2) ? 0x200000 : 0x100000;
	pci_as_m30 += align-1;
	pci_as_i32 += align-1;
	pci_as_m30 &= ~(align-1);
	pci_as_i32 &= ~(align-1);

	/*
	 * Disconnect all of device except config space.
	 * Writing all zeros to status causes no status
	 * bits to change.
	 * NOTE: Our console I/O goes through PCI, so
	 * do not call printf until we have turned the
	 * command word decode enables back on.
	 */
	pci_put_cfg_word(cfg, PCI_CFG_COMMAND, 0);

	for (window = 0; window < 6; ++window) { 

	    /* Check what kind of space the device wants,
	     * and see if anything is already set up there.
	     */
	    pci_put_cfg_word(cfg, PCI_CFG_BASE_ADDR(window), 0x00000000);
	    pciobits = pci_get_cfg_word(cfg, PCI_CFG_BASE_ADDR(window));

	    if (pciobits & 1) {
		/* will allocate from PCI I/O space */
		mask = ~3;
		pci_as_p = &pci_as_i32;
		pci_hi_p = &pci_hi_i32;
	    } else {
		/* will allocate from PCI MEM space */
		mask = ~15;
		pci_as_p = ((pciobits & 7) == 2) ? &pci_as_m20 : &pci_as_m30;
		pci_hi_p = ((pciobits & 7) == 2) ? &pci_hi_m20 : &pci_hi_m30;
	    }

	    /* find out how much space the device wants */
	    pci_put_cfg_word(cfg, PCI_CFG_BASE_ADDR(window), 0xFFFFFFFF);
	    pcimbits = pci_get_cfg_word(cfg, PCI_CFG_BASE_ADDR(window)) & mask;
	    pci_put_cfg_word(cfg, PCI_CFG_BASE_ADDR(window), pciobits);
	    if (pcimbits == 0)
		break;
	    size = pcimbits & -pcimbits;

	    /* we will align to at least a page boundary,
	     * making it easier to mmap individual windows
	     * from userland under IRIX.
	     */
	    align = (size < _PAGESZ) ? _PAGESZ : size;

	    /* Find block in which we are allocating */
	    pcibase = *pci_as_p;
	    pcilimit = *pci_hi_p;

	    /* Round up to next alignment limit;
	     * If this would wrap around 2^32
	     * then the result will be zero.
	     * Bail if the result is off the
	     * end of the allocation block.
	     */
	    pcibase += align-1;
	    pcibase &= ~(align-1);

	    if ((pcibase > pcilimit) || (pcibase == 0))
		break;

	    /* If there isn't enough space left in the
	     * block to allow this mapping, bail.
	     */
	    if ((pcilimit - pcibase) <= size)
		break;

	    /* Update the address of the next
	     * free location in this space.
	     */
	    *pci_as_p = pcibase + size;

	    /* tell device where this window is */
	    pci_put_cfg_word(cfg, PCI_CFG_BASE_ADDR(window), pcibase);
	    pcibase = pci_get_cfg_word(cfg, PCI_CFG_BASE_ADDR(window));

	    /* The *FIRST* window gets mapped via Bridge's Device(x) regsiter.
	     * This only makes *one* window available. Other windows of
	     * the same type might be available, if they manage to get allocated
	     * sufficiently close to the original window, but we make no guarantees;
	     * other windows should be accessed via other means.
	     */
	    if (window == 0) {
		unsigned		pciwsize;
		unsigned		pciwbase;

		pciwsize = (slot < 2) ? 0x200000 : 0x100000;
		pciwbase = pcibase &~ (pciwsize - 1);

		/* tell bridge where device lives on PCI */
		bridge->b_device[slot].reg = BRIDGE_DEV_COH | BRIDGE_DEV_ERR_LOCK_EN
		    | ((pciobits & 1) ? 0 : BRIDGE_DEV_DEV_IO_MEM)
		    | (BRIDGE_DEV_OFF_MASK & (pciwbase >> 20));
	    }

	    DPRINTF(("bridge pci_cfg 0x%x pci slot %d BASE%d is 0x%x\n",
		     cfg, slot, window, pcibase));

	    /*
	     * workaround for IOC3 bug.  when writing to PCI_CFG_BASE_ADDR(1),
	     * the value written will be latched into PCI_CFG_BASE_ADDR(0)
	     */
	    if (pci_id == IOC3_ID)
		break;

	    /* If this was a "64-bit" address register,
	     * force the upper half to all-zeros.
	     */
	    if ((pciobits & 7) == 4) {
		window ++;
		pci_put_cfg_word(cfg, PCI_CFG_BASE_ADDR(window), 0);
	    }
	}

	/* mapping the expansion rom seems to kill
	 * console I/O on the IOC3.
	 */
	if (pci_id != IOC3_ID) {

	    /* if the device has an expansion ROM,
	     * allocate space for it.
	     */
	    pci_put_cfg_word(cfg, PCI_EXPANSION_ROM, 0xFFFFF000);
	    pcimbits = pci_get_cfg_word(cfg, PCI_EXPANSION_ROM);
	    if (pcimbits != 0) {
		/* allocate PCI MEM address space */
		size = pcimbits & -pcimbits;
		align = (size < _PAGESZ) ? _PAGESZ : size;
		pcibase = pci_as_m30;
		pcilimit = pci_hi_m30;

		/* Round up to next alignment limit;
		 * If this would wrap around 2^32
		 * then the result will be zero.
		 * If the aligned result is within
		 * the allocation block, and there
		 * is enough space, nail the rom there.
		 */
		pcibase += align-1;
		pcibase &= ~(align-1);
		if ((pcibase <= pcilimit) &&
		    (pcibase != 0) &&
		    ((pcilimit - pcibase) > size)) {
		    pci_as_m30 = pcibase + size;

		    /* tell ROM where to sit, but do NOT
		     * enable the decode: that might disable
		     * decode of the BASE areas. ROM decode
		     * gets enabled and disabled as needed
		     * by the particular driver.
		     */
		    pci_put_cfg_word(cfg, PCI_EXPANSION_ROM, pcibase);
		    pcibase = pci_get_cfg_word(cfg, PCI_EXPANSION_ROM);
		}
	    }
	}

	/* give the device, by default, one RRB. */
	(void)alloc_bridge_rrb((void *)bridge, slot, 0, 1);

	/* now let the device answer to MEM and I/O, if it wishes. */
	/* XXX- also clears out any RW1C bits in Status. */
	pci_put_cfg_word(cfg, PCI_CFG_COMMAND,
			 pci_get_cfg_word(cfg, PCI_CFG_COMMAND)
			 | PCI_CMD_IO_SPACE
			 | PCI_CMD_MEM_SPACE
			 | PCI_CMD_BUS_MASTER);
    }
}

/* XXX check mfg id on SGI stuff! */
static slotdrvr_id_t bridgeids[] = {
    { BUS_XTALK, MFGR_ANY, BRIDGE_WIDGET_PART_NUM, REV_ANY },
    {BUS_NONE, 0, 0, 0}
};
drvrinfo_t bridge_drvrinfo = { bridgeids, "bridge" };

/*
 * Configure the DMA for chan req/grant by
 * 	1. allocating the xtalk rrb's
 * 	2. setup DIRMAP to map the upper 2GB to base of sys mem
 */
void
bridge_dma_config(int xport, int chan, int rrbs)
{
	bridge_t *bridge = (bridge_t *)K1_MAIN_WIDGET(xport);

	bridge = (bridge_t *)K1_MAIN_WIDGET(xport);
	(void)alloc_bridge_rrb((void *)bridge, chan, 0, rrbs);

	/*
	 * b_dir_map.w_id should already be initialized with Heart ID
	 * the add512 bit is coordinated with subtracting
	 * SYSTEM_MEMORY_BASE from padd in bridge_dma_trans()
	 */
	bridge->b_dir_map |= BRIDGE_DIRMAP_ADD512;
	/* read back to flush */
	bridge->b_dir_map;
}
#endif	/* IP30 */

#ifndef IP30_RPROM
extern struct reg_desc widget_llp_confg_desc[]; 

struct reg_desc widget_id_desc[] = {
	{WIDGET_REV_NUM,	-WIDGET_REV_NUM_SHFT,	"REV",	"0x%x",	NULL},
	{WIDGET_PART_NUM,	-WIDGET_PART_NUM_SHFT,	"PART",	"0x%x",	NULL},
	{WIDGET_MFG_NUM,	-WIDGET_MFG_NUM_SHFT,	"MFGT",	"0x%x",	NULL},
	{0,0,NULL,NULL,NULL}
};

static struct reg_values pci_gio_values[] = {
	{0,	"GIO"},
	{1,	"PCI"},
	{0,	NULL}
};

struct reg_desc widget_stat_desc[] = {
	{WIDGET_LLP_REC_CNT,	-24,	"LLP_REC_CNT",	"%d",	NULL},
	{WIDGET_LLP_TX_CNT,	-16,	"LLP_TX_CNT",	"%d",	NULL},
	{BRIDGE_STAT_FLASH_SELECT,-6,	"FLASH_SEL",	"%d",	NULL},
	{BRIDGE_STAT_PCI_GIO_N,  -5,	"MODE",		NULL,	pci_gio_values},
	{WIDGET_PENDING,	  0,	"PENDING",	"%d",	NULL},
	{0,0,NULL,NULL,NULL}
};

static struct reg_values ssram_values[] = {
	{0,	"1K"},
	{1,	"64K"},
	{2,	"128K"},
	{3,	"512K"},
	{0,	NULL}
};

static struct reg_values endian_values[] = {
	{0,	"LITTLE"},
	{1,	"BIG"},
	{0,	NULL}
};

struct reg_desc bridge_wid_ctrl_desc[] = {
	{BRIDGE_CTRL_FLASH_WR_EN,    0,	"FLASH_WR_EN",	NULL,	NULL},
	{BRIDGE_CTRL_EN_CLK50,	0,	"EN_CLK50",	NULL,	NULL},
	{BRIDGE_CTRL_EN_CLK40,	0,	"EN_CLK40",	NULL,	NULL},
	{BRIDGE_CTRL_EN_CLK33,	0,	"EN_CLK33",	NULL,	NULL},
	{BRIDGE_CTRL_RST_MASK,	-24,	"RST_MSK",	"0x%x",	NULL},
	{BRIDGE_CTRL_IO_SWAP,	0,	"IO_SWP",	NULL,	NULL},
	{BRIDGE_CTRL_MEM_SWAP,	0,	"MEM_SWP",	NULL,	NULL},
	{BRIDGE_CTRL_PAGE_SIZE,	0,	"PAGE_SZ",	NULL,	NULL},
	{BRIDGE_CTRL_SS_PAR_BAD,0,	"SS_PAR_BAD",	NULL,	NULL},
	{BRIDGE_CTRL_SS_PAR_EN,	0,	"SS_PAR_EN",	NULL,	NULL},
	{BRIDGE_CTRL_SSRAM_SIZE_MASK,-17,"SS_RAM",	NULL,	ssram_values},
	{BRIDGE_CTRL_F_BAD_PKT,	0,	"F_BAD_PKT",	NULL,	NULL},
	{BRIDGE_CTRL_LLP_XBAR_CRD_MASK,-12,"LLP_XBAR_CRD","0x%x",NULL},
	{BRIDGE_CTRL_SYS_END,	-9,	"ENDIAN",	NULL,	endian_values},
	{BRIDGE_CTRL_MAX_TRANS_MASK, -4,"MAX_TRANS",	"%d",	NULL},
	{BRIDGE_CTRL_WIDGET_ID_MASK,  0,"WID_ID",	"0x%x",	NULL},
	{0,0,NULL,NULL,NULL}
};

static struct reg_values packet_type[] = {
	{ 0x0,	"RD REQ" },
	{ 0x1,	"RD RESP" },
	{ 0x2,	"WR REQ W/ RESP" },
	{ 0x3,	"WR RESP" },
	{ 0x4,	"WR REQ W/O RESP" },
	{ 0x5,	"RESERVED" },
	{ 0x6,	"FETCH AND OP" },
	{ 0x7,	"RESERVED" },
	{ 0x8,	"STORE AND OP" },
	{ 0x9,	"RESERVED" },
	{ 0xa,	"RESERVED" },
	{ 0xb,	"RESERVED" },
	{ 0xc,	"RESERVED" },
	{ 0xd,	"RESERVED" },
	{ 0xe,	"SPECIAL PKT REQ" },
	{ 0xf,	"SPECIAL PKT RESP" },
	{ 0,	NULL },
};

static struct reg_values data_size[] = {
	{ 0x0,	"DOUBLE WORD" },
	{ 0x1,	"QUARTER CACHELINE" },
	{ 0x2,	"FULL CACHELINE" },
	{ 0x3,	"RESERVED" },
	{ 0,	NULL },
};

struct reg_desc widget_err_cmd_word_desc[] = {
	{WIDGET_DIDN,		-28,	"DIDN",		"0x%x",	NULL},
	{WIDGET_SIDN,		-24,	"SIDN",		"0x%x",	NULL},
	{WIDGET_PACTYP,		-20,	"PACTYPE",	NULL,	packet_type},
	{WIDGET_TNUM,		-15,	"TNUM",		"0x%x",	NULL},
	{WIDGET_COHERENT,	0,	"COHERENT",	NULL,	NULL},
	{WIDGET_DS,		-12,	"DS",		NULL,	data_size},
	{WIDGET_GBR,		0,	"GBR",		NULL,	NULL},
	{WIDGET_VBPM,		0,	"VBPM",		NULL,	NULL},
	{WIDGET_ERROR,		0,	"ERROR",	NULL,	NULL},
	{WIDGET_BARRIER,	0,	"BARRIER",	NULL,	NULL},
	{0,			0,	NULL,		NULL,	NULL}
};

struct reg_desc bridge_resp_buf_err_desc[] = {
	{0x7L << 52,		-52,	"DEV_NUM",	"0x%x",		NULL},
	{0xfL << 48,		-48,	"BUFF_NUM",	"0x%x",		NULL},
	{0xffffffffffffL,	-0,	"ADDR",		"0x%Lx",	NULL},
	{0,0,NULL,NULL,NULL}
};

struct reg_desc bridge_ssram_err_desc[] = {
	{0x7L << 25,	-25,	"PCI_DEV",	"0x%x",	NULL},
	{0x1L << 24,	0,	"ACC_SRC",	NULL,	NULL},
	{0x1L << 23,	0,	"DS0",		NULL,	NULL},
	{0x1L << 22,	0,	"DS1",		NULL,	NULL},
	{0x1L << 21,	0,	"DS2",		NULL,	NULL},
	{0x1L << 20,	0,	"DS3",		NULL,	NULL},
	{0x1L << 19,	0,	"DS4",		NULL,	NULL},
	{0x1L << 18,	0,	"DS5",		NULL,	NULL},
	{0x1L << 17,	0,	"DS6",		NULL,	NULL},
	{0x1L << 16,	0,	"DS7",		NULL,	NULL},
	{0xffffL,	3,	"ADDR",		"0x%x",	NULL},
	{0,0,NULL,NULL,NULL}
};

struct reg_desc bridge_pci_err_desc[] = {
	{0x1L << 52,		0,	"DEV_MASTER",	NULL,		NULL},
	{0x1L << 51,		0,	"VDEV",		NULL,		NULL},
	{0x7L << 48,		-48,	"DEV_NUM",	"0x%x",		NULL},
	{0xffffffffffffL,	-0,	"ADDR",		"0x%Lx",	NULL},
	{0,0,NULL,NULL,NULL}
};

struct reg_desc bridge_isr_desc[] = {
	{BRIDGE_ISR_MULTI_ERR,	0,	"MULTI_ERR",		NULL,	NULL},
	{BRIDGE_ISR_PMU_ESIZE_FAULT,0,	"PMU_ESIZE_FAULT",	NULL,	NULL},
	{BRIDGE_ISR_UNEXP_RESP,	0,	"UNEXP_RESP",		NULL,	NULL},
	{BRIDGE_ISR_BAD_XRESP_PKT,0,	"BAD_XRESP_PKT",	NULL,	NULL},
	{BRIDGE_ISR_BAD_XREQ_PKT,0,	"BAD_XREQ_PKT",		NULL,	NULL},
	{BRIDGE_ISR_RESP_XTLK_ERR,0,	"RESP_XTLK_ERR",	NULL,	NULL},
	{BRIDGE_ISR_REQ_XTLK_ERR,0,	"REQ_XTLK_ERR",		NULL,	NULL},
	{BRIDGE_ISR_INVLD_ADDR,	0,	"INVLD_ADDR",		NULL,	NULL},
	{BRIDGE_ISR_UNSUPPORTED_XOP,0,	"UNSUPPORTED_XOP",	NULL,	NULL},
	{BRIDGE_ISR_XREQ_FIFO_OFLOW,0,	"XREQ_FIFO_OFLOW",	NULL,	NULL},
	{BRIDGE_ISR_LLP_REC_SNERR,0,	"LLP_REC_SNERR",	NULL,	NULL},
	{BRIDGE_ISR_LLP_REC_CBERR,0,	"LLP_REC_CBERR",	NULL,	NULL},
	{BRIDGE_ISR_LLP_RCTY,	0,	"LLP_REC_CNT_OFLOW",	NULL,	NULL},
	{BRIDGE_ISR_LLP_TX_RETRY,0,	"LLP_TX_RETRY",		NULL,	NULL},
	{BRIDGE_ISR_LLP_TCTY,	0,	"LLP_TX_CNT_OFLOW",	NULL,	NULL},
	{BRIDGE_ISR_SSRAM_PERR,	0,	"SSRAM_PERR",		NULL,	NULL},
	{BRIDGE_ISR_PCI_ABORT,	0,	"PCI_ABORT",		NULL,	NULL},
	{BRIDGE_ISR_PCI_PARITY,	0,	"PCI_PARITY",		NULL,	NULL},
	{BRIDGE_ISR_PCI_SERR,	0,	"PCI_SERR",		NULL,	NULL},
	{BRIDGE_ISR_PCI_PERR,	0,	"PCI_PERR",		NULL,	NULL},
	{BRIDGE_ISR_PCI_MST_TIMEOUT,0,	"PCI_MST_TIMEOUT",	NULL,	NULL},
	{BRIDGE_ISR_PCI_RETRY_CNT,0,	"PCI_RETRY_CNT",	NULL,	NULL},
	{BRIDGE_ISR_XREAD_REQ_TIMEOUT,0,"XREAD_REQ_TIMEOUT",	NULL,	NULL},
	{BRIDGE_ISR_GIO_B_ENBL_ERR,0,	"GIO_B_ENBL_ERR",	NULL,	NULL},
#if IP30
	{BRIDGE_ISR_INT(7),	0,	"AC_FAIL/7",		NULL,	NULL},
	{BRIDGE_ISR_INT(6),	0,	"PWR_SWITCH/6",		NULL,	NULL},
	{BRIDGE_ISR_INT(5),	0,	"PASSWD/5",		NULL,	NULL},
#else
	{BRIDGE_ISR_INT(7),	0,	"INT_7",		NULL,	NULL},
	{BRIDGE_ISR_INT(6),	0,	"INT_6",		NULL,	NULL},
	{BRIDGE_ISR_INT(5),	0,	"INT_5",		NULL,	NULL},
#endif	/* IP30 */
	{BRIDGE_ISR_INT(4),	0,	"INT_4",		NULL,	NULL},
	{BRIDGE_ISR_INT(3),	0,	"INT_3",		NULL,	NULL},
	{BRIDGE_ISR_INT(2),	0,	"INT_2",		NULL,	NULL},
	{BRIDGE_ISR_INT(1),	0,	"INT_1",		NULL,	NULL},
	{BRIDGE_ISR_INT(0),	0,	"INT_0",		NULL,	NULL},
	{0,0,NULL,NULL,NULL}
};

void
print_bridge_status(bridge_t *bridge, int clear)
{
	bridgereg_t		bridge_ctrl;
	bridgereg_t		bridge_isr;

	bridge_isr = bridge->b_int_status;
	if (!bridge_isr)
		return;

	bridge_ctrl = bridge->b_wid_control;

	/* print widget ID in case we have multiple Bridges in the system */
	printf("Bridge(id=0x%x) ISR: %R\n",
		bridge_ctrl & BRIDGE_CTRL_WIDGET_ID_MASK,
		bridge_isr, bridge_isr_desc);

	if (bridge_isr & BRIDGE_IRR_CRP_GRP) {
		bridgereg_t aux_err_cmd_word;

		aux_err_cmd_word = bridge->b_wid_aux_err;
		printf("  Aux error command word: %R\n",
			aux_err_cmd_word, widget_err_cmd_word_desc);
	}

	if (bridge_isr & BRIDGE_IRR_RESP_BUF_GRP) {
		__uint64_t resp_buf_err;
		bridgereg_t resp_buf_err_lower;
		bridgereg_t resp_buf_err_upper;

		resp_buf_err_upper = bridge->b_wid_resp_upper;
		resp_buf_err_lower = bridge->b_wid_resp_lower;

		resp_buf_err =
			(((__uint64_t)resp_buf_err_upper << 32) |
			 resp_buf_err_lower);

		printf("  Response buffer error: %LR\n",
			resp_buf_err, bridge_resp_buf_err_desc);
	}

	if (bridge_isr & BRIDGE_IRR_REQ_DSP_GRP) {
		__uint64_t err_addr;
		bridgereg_t err_addr_lower;
		bridgereg_t err_addr_upper;
		bridgereg_t err_cmd_word;

		err_cmd_word = bridge->b_wid_err_cmdword;
		printf("  Error command word: %R\n",
			(__uint64_t)err_cmd_word, widget_err_cmd_word_desc);

		err_addr_upper = bridge->b_wid_err_upper;
		err_addr_lower = bridge->b_wid_err_lower;
		err_addr = ((__uint64_t)err_addr_upper << 32) | err_addr_lower;
		printf("  Error address: 0x%Lx\n", err_addr);
	}

	if (bridge_isr & BRIDGE_IRR_SSRAM_GRP) {
		bridgereg_t ssram_err;

		ssram_err = bridge->b_ram_perr;
		printf("  SSRAM parity error: %R\n",
			ssram_err, bridge_ssram_err_desc);
	}

	if (bridge_isr & BRIDGE_IRR_PCI_GRP) {
		__uint64_t pci_err;
		bridgereg_t pci_err_lower ;
		bridgereg_t pci_err_upper;

		pci_err_upper = bridge->b_pci_err_upper;
		pci_err_lower = bridge->b_pci_err_lower;
		pci_err = ((__uint64_t)pci_err_upper << 32) | pci_err_lower;
		printf("  PCI/GIO error: %LR\n",
			pci_err, bridge_pci_err_desc);
	}

	if (clear)
		bridge_clearerrors(bridge);
}

void
dump_widget_regs(widget_cfg_t *wp)
{
	widgetreg_t reg;

	printf(" wid_id: %R\n", wp->w_id, widget_id_desc);
	printf(" wid_stat: %R\n", wp->w_status, widget_stat_desc);
	reg = wp->w_control;
	printf(" wid_control: %R\n", reg, bridge_wid_ctrl_desc);
	{
		__uint64_t err_addr;
		widgetreg_t err_addr_lower;
		widgetreg_t err_addr_upper;
		widgetreg_t err_cmd_word;

		err_cmd_word = wp->w_err_cmd_word;
		printf("  Error command word: %R\n",
			(ulong)err_cmd_word & 0xFFFFFFFFL,
			widget_err_cmd_word_desc);

		err_addr_upper = wp->w_err_upper_addr;
		err_addr_lower = wp->w_err_lower_addr;
		err_addr = ((__uint64_t)err_addr_upper << 32) | err_addr_lower;
		printf("  Error address: 0x%Lx\n", err_addr);
	}
	reg = wp->w_llp_cfg;
	printf("  LLP Configuration: %R\n", reg, widget_llp_confg_desc);

}

void
dump_bridge_regs(bridge_t *bridge)
{
	bridgereg_t		reg;

	printf("Bridge Widget Registers:\n");
	dump_widget_regs((widget_cfg_t *)bridge);

	reg = bridge->b_int_status;
	printf(" int_status: %R\n", reg, bridge_isr_desc);
	reg = bridge->b_int_enable;
	printf(" int_enable: %R\n", reg, bridge_isr_desc);

	printf("  Aux error command word: %R\n",
		(ulong) bridge->b_wid_aux_err & 0xFFFFFFFFL,
		widget_err_cmd_word_desc);

	{
		__uint64_t resp_buf_err;
		bridgereg_t resp_buf_err_lower;
		bridgereg_t resp_buf_err_upper;

		resp_buf_err_upper = bridge->b_wid_resp_upper;
		resp_buf_err_lower = bridge->b_wid_resp_lower;

		resp_buf_err =
			(((__uint64_t)resp_buf_err_upper << 32) |
			 resp_buf_err_lower);

		printf("  Response buffer error: %LR\n",
			resp_buf_err, bridge_resp_buf_err_desc);
	}

	reg = bridge->b_ram_perr;
	printf("  SSRAM parity error: %R\n",
		reg, bridge_ssram_err_desc);

	{
		__uint64_t pci_err;
		bridgereg_t pci_err_lower ;
		bridgereg_t pci_err_upper;

		pci_err_upper = bridge->b_pci_err_upper;
		pci_err_lower = bridge->b_pci_err_lower;
		pci_err = ((__uint64_t)pci_err_upper << 32) | pci_err_lower;
		printf("  PCI/GIO error: %LR\n",
			pci_err, bridge_pci_err_desc);
	}
}
#endif /* !IP30_RPROM */

static void
bridge_clearerrors(bridge_t *bridge)
{
	if (bridge->b_int_status) {
		bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;
		bridge->b_wid_tflush;		/* flush to bridge */
		flushbus();
	}
}

void
bridge_clearnofault(void)
{
#ifdef IP30
	int i;

	for (i = 0 ; i < nbridge; i++) {
		bridge_clearerrors(bridges[i]);
	}
#else
	/* XXX SN0? */
#endif
}

#ifdef QLREVA_STALE_RRB_WAR
/* Needed by the QL driver for rev A parts which sometimes will ask for
 * a buffer they do not use.  Code could be cleaner, but...
 */
void
bridge_inval_rrbs(__psunsigned_t addr)
{
	bridgereg_t rrb_save, rrb_xor;
	volatile unsigned int *rrbp;
	unsigned int shift, bitmask;
	bridge_t *bridge;
	int slot, to;

	bridge = (bridge_t *)(addr & ~0xefffff);
	addr &= 0xe00000;

	if (addr < BRIDGE_DEVIO2)
		slot = (addr - BRIDGE_DEVIO0) / BRIDGE_DEVIO_2MB;
	else
		slot = 2 + ((addr - BRIDGE_DEVIO2) / BRIDGE_DEVIO_1MB);

	/* Find this slots rrb xor mask and bitmask */
	rrbp = (slot & 1) ? &bridge->b_odd_resp : &bridge->b_even_resp;
	rrb_save = *rrbp;
	rrb_xor = 0;
	bitmask = 0;
	for (shift = 0; shift < 32; shift += 4) {
		if ((rrb_save & (0xb<<shift)) == ((0x8|(slot>>1))<<shift)) {
			rrb_xor |= 0x8 << shift;
			bitmask |= 1 << ((shift>>1)+(slot&1));
		}
	}

	if (!bitmask) return;		/* no RRBs for this slot */

	/* Check for any valid or in use RRBs */
	if ((bridge->b_resp_status & (bitmask | (bitmask << 16))) == 0)
		return;

#ifdef DEBUG
	printf("Outstanding RRBs 0x%x\n",bridge->b_resp_status);
#endif

	*rrbp = rrb_save ^ rrb_xor;	/* invalidate buffers */
	flushbus();

	/* Wait for RRB_INUSE to go away */
	for (to=128; (bridge->b_resp_status & bitmask) && to > 0; to--)
		;

	/* Clear RRBs */
	bridge->b_resp_clear = bitmask;

	/* Wait for clear to take effect */
	for (to=128;
	     (bridge->b_resp_status & (bitmask | (bitmask << 16))) && to > 0;
	     to--)
		;

	*rrbp = rrb_save;		/* restore buffers */
	flushbus();
}
#endif
