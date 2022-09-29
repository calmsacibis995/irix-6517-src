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
 **************************************************************************
 *
 * This is based on #ident  "io/pci_intf.c: $Revision: 1.1 $" from banyan
 *
 * $Id: pci_intf.c,v 1.1 1997/06/05 23:39:30 philw Exp $
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/dmamap.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/vmereg.h>
#include <sys/pio.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <bstring.h>
#include <string.h>
#include <arcs/hinv.h>
#include <libsk.h>
#include <libsc.h>

#include <pci/PCI_defs.h>
#include <pci/bridge.h>
#include <pci_intf.h>

static int verbose_pci=0;

#ifdef DEBUGCARD
#include "../../../IP32/include/DBCsim.h"
#include "../../../IP32/include/DBCuartsim.h"
#else
#define DPRINTF		printf
#endif

#if defined(IP32)

#ifdef SIM_PCICFG
static unsigned char		*init_pci_config_space(void);
#define FIRST_CFG_SLOT		init_pci_config_space()
#else
#define FIRST_CFG_SLOT		((1 << 31) | (1 << CFG1_DEVICE_SHIFT))
#endif
#define PCI_CFG_SIZE		(1 << CFG1_DEVICE_SHIFT)
#define BASE_IO_MAP_ADDR	PCI_FIRST_IO_ADDR
#define FIRST_DEV_NUM		1
#define LAST_DEV_NUM		3

#else /* not IP32 */

#define	FIRST_CFG_SLOT		BRIDGE_TYPE0_CFG_DEV0
#define	PCI_CFG_SIZE		BRIDGE_TYPE0_CFG_OFF
#define	BASE_IO_MAP_ADDR	BRIDGE_DEVIO0
#define FIRST_DEV_NUM		0
#define	LAST_DEV_NUM		7

#endif

static __uint32_t debugnum;

/*
 * This structure is used for holding on to per registration info
 */
struct pci_reg {
	struct pci_reg	*preg_next;	/* maintain the list */
	pci_record_t	*preg_record;	/* info provided to us */
};
typedef	struct pci_reg pci_reg_t;

pci_reg_t	*pci_reg_first = NULL;

/*
 * storage for interrupt info
 */
struct pci_dev_intr {
	struct pci_dev_intr	*di_next;
	pci_intr_t		*di_intr;
};
typedef struct pci_dev_intr pci_dev_intr_t;

/*
 * Structure used for listing found devices
 */
struct pci_dev {
	struct pci_dev	*pd_next;	/* maintain the list */
	void		*pd_bus;	/* bus where we are located */
	unsigned char	*pd_cfg;	/* pointer to config space */
	__uint32_t	pd_mem_size;	/* address size needed */
	pci_ident_t	pd_id;
	pci_dev_intr_t	*pd_intr;
	edt_t		*pd_edt;	/* created edt for later reference */
	pci_reg_t	*pd_reg;	/* driver currently attached */
};
typedef struct pci_dev pci_dev_t;

pci_dev_t	*pci_dev_first = NULL;
COMPONENT *pci_node=NULL;		/* my cfg node.  Hmm, need more than 1 */

static id_match(pci_ident_t *sp, pci_ident_t *dp);
static pci_reg_t *find_reg(pci_ident_t *ip);

/*
 * Routine used to register a particular PCI card
 * The pci_record_t must be allocated and preserved by the caller.
 */
pci_register(pci_record_t *pr_p)
{
	pci_reg_t	*rp;

	/* check to make sure that this driver is nice */
	if ((pr_p->pci_attach == NULL) || (pr_p->pci_detach == NULL)) {
		printf("pci_register: no attach/detach functions defined\n");
		return -1;
	}

	/* get a pci_reg record and fill it */
	if ((rp = malloc(sizeof(pci_reg_t))) == NULL) {
		printf("pci_register: malloc failed\n");
		return -1;
	}
	bzero(rp, sizeof(pci_reg_t));
	rp->preg_record = pr_p;

	/* add it to the list */
	rp->preg_next = pci_reg_first;
	pci_reg_first = rp;

	if (verbose_pci)
		DPRINTF("pci_register: registering vendor 0x%x device 0x%x\n",
			rp->preg_record->pci_id.vendor_id,
			rp->preg_record->pci_id.device_id);

	
	/* do not call attach_devs now.  The attach function
	 * will be called from pci_startup
	 */
	return 0;
}

/*
 * Allow the driver to be un-registered
 */
pci_unregister(pci_record_t *pr_p, __int32_t reasons)
{
	pci_dev_t	*dp;
	pci_reg_t	*rp, *lrp = NULL;

	/* first check to see if everyone is done */
	dp = pci_dev_first;
	while (dp) {
		if (dp->pd_reg) {
		    if (dp->pd_reg->preg_record == pr_p) {
			if (reasons & PCI_FORCE_DETACH) {
			    (*(pr_p->pci_detach))(pr_p, dp->pd_edt, reasons);
			    dp->pd_reg = NULL;
			}
			if (dp->pd_reg) {
			    return -1;		/* Still have someone in use */
		        }
		    }
		}
		dp = dp->pd_next;
	}

	/* Now find the registration */
	rp = pci_reg_first;
	while (rp) {
		if (rp->preg_record == pr_p)
			break;
		lrp = rp;
		rp = rp->preg_next;
	}
	/* and remove it */
	if (rp) {
		if (lrp) {
			lrp->preg_next = rp->preg_next;
		} else {
			pci_reg_first = rp->preg_next;
		}
	}
	return 0;
}

static
id_match(pci_ident_t *sp, pci_ident_t *dp)
{
	int retval = 0;

	if ((sp->vendor_id == dp->vendor_id) &&
	    (sp->device_id == dp->device_id)) {
		if ((sp->revision_id == dp->revision_id) ||
			(sp->revision_id == REV_MATCH_ANY) ||
			(dp->revision_id == REV_MATCH_ANY))
		retval = 1;
	}

	return retval;
}

static pci_reg_t *
find_reg(pci_ident_t *ip)
{
	pci_reg_t *rp;

	if (verbose_pci)
		DPRINTF("find_reg: find match for 0x%x 0x%x\n",
			ip->vendor_id, ip->device_id);

	rp = pci_reg_first;
	while (rp != NULL) {

		if (verbose_pci)
			DPRINTF("check 0x%x 0x%x\n",
				rp->preg_record->pci_id.vendor_id,
				rp->preg_record->pci_id.device_id);

		if (id_match(ip, &rp->preg_record->pci_id))
			break;
		rp = rp->preg_next;
	}
	if (verbose_pci) 
		if (rp == NULL)
			DPRINTF("no match\n");
		else
			DPRINTF("match\n");

	return rp;
}

#define	SWAP_BYTES(_s)	((((_s) & 0xFF) << 8) | (((_s) >> 8) & 0xFF))
#define SWAP_WORD(_w)	((((_w) & 0xFF) << 24) | (((_w) & 0xFF00) << 8) | \
			 (((_w) >> 8) & 0xFF00) | (((_w) >> 24) & 0xFF))
#define SWIZZLE_BYTEPTR(a) swizzle_byteptr(a)

u_int
swizzle_byteptr(u_int src)
{

	switch(src & 0x3) {
	case 0:
		return ((src & 0xfc) | 0x3);

	case 1:
		return ((src & 0xfc) | 0x2);

	case 2:
		return ((src & 0xfc) | 0x1);

	case 3:
		return (src & 0xfc);
	}
	/* not reached */
	ASSERT(0);
	return 0;
}


static __uint32_t
addr_size(__uint32_t addr)
{
	__uint32_t	size;
	int		cnt = 21;

	size = 1;
	while (((addr & 1) == 0) && (--cnt > 0)) {
		size = size << 1;
		addr = addr >> 1;
	}
	return size;
}

static __uint32_t
io_slot_delta(int slot, __uint32_t addrsize)
{
	__uint32_t retval;

#if defined(IP32)
	/*
	 * On Moosehead, keep cards at least one page from each other,
	 * and round up anything larger than one page to the nearest page.
	 */
	if (addrsize <= PCI_IO_MAP_INCR) {
		retval = PCI_IO_MAP_INCR;
	} else {
		retval = ((addrsize + PCI_IO_MAP_INCR - 1) / PCI_IO_MAP_INCR) *
			  PCI_IO_MAP_INCR;
	}
#else
	retval = BRIDGE_DEVIO_OFF;
	if (slot < 2)
		retval += BRIDGE_DEVIO_OFF;
#endif
	return retval;
}

#if defined(IP32)
/* 
 * This should be in a header file somewhere.
 */
#define PCI_CONFIG_ADDR_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CONFIG_DATA_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_DATA)))
#define PCI_ERROR_FLAGS_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_ERROR_FLAGS)))
#endif

/*
 * Read a value from a configuration register.
 * For Moosehead, we assume the system is big endien, the system PCI bridge
 * does not do address or data swizzling, and the device is little endien.
 */
u_int32_t
pci_get_cfg(volatile unsigned char *bus_addr, int cfg_reg)
{
	u_int32_t	tmp=0;
	volatile unsigned char *cp;

#if defined(IP32)
	u_int32_t	pci_err_flags;

	/*
	 * Read the 32 bit data now.  Decide which part of it to return
	 * in the switch stmt. XXX This function should really return a seperate
	 * return value.
	 */
#ifdef SIM_PCICFG
	cp = bus_addr + (cfg_reg & 0xfc);
	tmp = *((u_int32_t *) cp);
	if (verbose_pci)
		DPRINTF("pci_get_cfg: reading 0x%x got 0x%x\n", cp, tmp);
#else
	cp = bus_addr + (cfg_reg & 0xfc);
	*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
	tmp = *(PCI_CONFIG_DATA_PTR);
	pci_err_flags = *PCI_ERROR_FLAGS_PTR;
	if (pci_err_flags & PERR_MASTER_ABORT) {
		*PCI_ERROR_FLAGS_PTR &= ~(PERR_MASTER_ABORT);	/* clear bit */
		return -1;
	}
#endif

#else	/* not IP32 */
	cp = bus_addr + cfg_reg;
#endif

	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_COMMAND:
	case PCI_CFG_STATUS:
		/*
		 * Reads of these registers will return 16 bits in the lower
		 * 16 bits of the 32 bit return value.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)      /* even addr: take lower 16 bits */
			tmp = tmp & 0xffff;
		else				/* odd addr: take upper 16 bits */
			tmp = tmp >> 16;
		break;
#else
		tmp = (u_int32_t)*(u_int16_t *)cp;
		tmp = SWAP_BYTES(tmp);

		cp = (volatile unsigned char *)((unsigned int)cp ^ 2);
		tmp = (u_int32_t)*(u_int16_t *)cp;
#endif
		break;

	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
	case PCI_CFG_HEADER_TYPE:
	case PCI_CFG_BIST:
		/*
		 * Reads of these registers will return 8 bits in the lower
		 * 8 bits of the 32 bit return value.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0) {	/* bits 0-7 */
			tmp = tmp & 0xff;
		} else if ((cfg_reg & 0x3) == 1) { /* bits 8-15 */
			tmp = (tmp & 0xff00) >> 8;
		} else if ((cfg_reg & 0x3) == 2) { /* bits 16-32 */
			tmp = (tmp & 0xff0000) >> 16;
		} else {			   /* bits 24-31 */
			tmp = (tmp & 0xff000000) >> 24;
		}
#else
		cp = (volatile unsigned char *)
			((__psunsigned_t)cp + SWIZZLE_BYTEPTR(cfg_reg));
		tmp = *cp;
#endif
		break;
		   
	case PCI_CFG_BASE_ADDR_0:
	case PCI_CFG_BASE_ADDR_1:
	case PCI_CFG_BASE_ADDR_2:
	case PCI_CFG_BASE_ADDR_3:
	case PCI_CFG_BASE_ADDR_4:
	case PCI_CFG_BASE_ADDR_5:
		/* reads of these registers will return 32 bits */
#if defined(IP32)
		/* done.  no swapping of data */
#else
		tmp = *(u_int32_t *)cp;
		tmp = SWAP_WORD(tmp);
#endif
		break;

	default:
		if (cfg_reg < PCI_CFG_VEND_SPECIFIC)
			cmn_err(CE_WARN, "pci_get_cfg: accessing 0x%x",
				cfg_reg);
#if defined(IP32)
		/* done.  no swapping of data */
#else
		tmp = *(u_int32_t *)cp;
#endif
	}

	return tmp;
}

/*
 * See comments at pci_get_cfg.
 */		   
int
pci_put_cfg(volatile unsigned char *bus_addr, int cfg_reg, __int32_t val)
{
	u_int32_t	tmp = (u_int32_t)val;
	volatile unsigned char *cp;

#if defined(IP32)
	/*
	 * Read the 32 bit data now.  Merge it with the correct bits
	 * passed in by val.  If the read succeeds, then the device is there.
	 * XXX what about errors from the read? 
	 */
#if SIM_PCICFG
	cp = bus_addr + (cfg_reg & 0xfc);
	tmp = *((u_int32_t *) cp);
#else
	cp = bus_addr + (cfg_reg & 0xfc);
	*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
	tmp = *(PCI_CONFIG_DATA_PTR);
#endif

#else   /* not IP32 */
	cp = bus_addr + cfg_reg;
#endif

	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_HEADER_TYPE:
		cmn_err(CE_WARN, "pci_put_cfg: writing to reserved reg 0x%x",
			cfg_reg);
		break;

	case PCI_CFG_COMMAND:
	case PCI_CFG_STATUS:
		/*
		 * The lower 16 bits of val will be written to the appropriate
		 * place in the config register.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)	/* even addr: put in lower 16 bits */
			tmp = (tmp & 0xffff0000) | (val & 0xffff);
		else				/* odd addr: put in upper 16 bits */
			tmp = (tmp & 0xffff) | (val << 16);
#ifdef SIM_PCICFG
		*((u_int32_t *) cp) = tmp;
#else
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = tmp;
#endif
#else
		tmp = SWAP_BYTES(tmp);
		cp = (volatile unsigned char *)((__psunsigned_t) cp ^ 2);
		*(u_int16_t *) cp = (u_int16_t) tmp;	
#endif
		break;

	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
	case PCI_CFG_BIST:
		/*
		 * The lower 8 bits of val will be written to the appropriate
		 * place in the config register.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)	/* put in bits 0-7 */
			tmp = (tmp & 0xffffff00) | (val & 0xff);
		else if ((cfg_reg & 0x3) == 1)	/* put in bits 8-15 */
			tmp = (tmp & 0xffff00ff) | ((val & 0xff) << 8);
		else if ((cfg_reg & 0x3) == 2)	/* put in bits 16-23 */
			tmp = (tmp & 0xff00ffff) | ((val & 0xff) << 16);
		else				/* put in bits 8-15 */
			tmp = (tmp & 0x00ffffff) | ((val & 0xff) << 24);
#ifdef SIM_PCICFG
		*((u_int32_t *) cp) = tmp;
#else
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = tmp;
#endif
#else
		cp = (volatile unsigned char *)
			((__psunsigned_t) cp + SWIZZLE_BYTEPTR(cfg_reg));
		*cp = (u_char) tmp;
#endif
		break;
		   
	case PCI_CFG_BASE_ADDR_0:
	case PCI_CFG_BASE_ADDR_1:
	case PCI_CFG_BASE_ADDR_2:
	case PCI_CFG_BASE_ADDR_3:
	case PCI_CFG_BASE_ADDR_4:
	case PCI_CFG_BASE_ADDR_5:

#if defined(IP32)
		/*
		 * All 32 bits of val are put into the register.  Wasted
		 * the read above...hmmm
		 */
#ifdef SIM_PCICFG
		if (val == 0xffffffff)
			*((u_int32_t *) cp) = 0x80;
		else
			*((u_int32_t *) cp) = val;
#else
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = val;
#endif

#else	/* not IP32 */
		tmp = SWAP_WORD(tmp);
		*(u_int32_t *)cp = tmp;
#endif
		break;
		   
	default:
		if (cfg_reg < PCI_CFG_VEND_SPECIFIC)
			cmn_err(CE_WARN, "pci_put_cfg: accessing 0x%x",
				cfg_reg);
#if defined(IP32)
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = val;
#else
		*(u_int32_t *)cp = tmp;
#endif
	}

	return 0;
}


/* template for PCI bus controller */
static COMPONENT pci_adapter_tmpl = {
	AdapterClass,			/* Class */
	PCIAdapter,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0,				/* Affinity */
	0,				/* ConfigurationDataSize */
	8,				/* IdentifierLength */
#ifdef IP32
	"MACE-PCI"			/* Identifier */
#else
	"SGI-PCI"			/* Identifier */
#endif
};

/*
 * Routine used to get a bus on-line
 */
pci_startup(COMPONENT *system_node)
{
	void *bus_addr=0;
	volatile unsigned char *cfg_p, *up;
	pci_dev_t *dp, *bus_dp[LAST_DEV_NUM + 1];
	pci_reg_t *rp;
	edt_t	*ep;
	int	slot;
	u_int16_t tmp_s;
	__uint32_t addr;

#ifdef IP32
	init_pci();
#endif

	if (verbose_pci)
		DPRINTF("pci_startup: parent node %s at 0x%x\n",
			system_node->Identifier, system_node);

	/* first add myself to the cfg. tree.  The code from banyan assumes
	 * only 1 PCI bus, so I do so here.  For Moosehead, it works fine.
	 */
	pci_node = AddChild(system_node, &pci_adapter_tmpl, (void *) NULL);

	bzero(bus_dp, (LAST_DEV_NUM + 1) * sizeof(pci_dev_t *));

	/*
	 * step thru the config slots for this bridge and see what we have
	 * Assign config addresses and IO mappings as we find the cards.
	 */
	cfg_p = (unsigned char *) FIRST_CFG_SLOT;
	up = (unsigned char *) BASE_IO_MAP_ADDR;

	for (slot = FIRST_DEV_NUM; slot <= LAST_DEV_NUM; slot++) {
		if (verbose_pci)
			DPRINTF("pci_startup: checking config addr 0x%x slot %d\n",
				cfg_p, slot);
		tmp_s = (u_int16_t)pci_get_cfg(cfg_p, PCI_CFG_VENDOR_ID);
		if (verbose_pci)
			DPRINTF("pci_startup: vendor id 0x%X\n", tmp_s);

		/*
		 * if non zero then we have a device
		 * According to PCI 2.1, 0xffff is an invalid vendor id
		 * Also, Moosehead returns -1 if the config read failed.
		 */
		if (tmp_s && (tmp_s != 0xffff)) {
		    /* allocate and add to this bus set */
		    dp = malloc(sizeof(pci_dev_t));
		    bzero(dp, sizeof(pci_dev_t));
		    bus_dp[slot] = dp;

		    dp->pd_bus = bus_addr;
		    dp->pd_cfg = (unsigned char *)cfg_p;

		    /* get the id info */
		    dp->pd_id.vendor_id = tmp_s;
		    dp->pd_id.device_id = 
			(u_int16_t)pci_get_cfg(cfg_p, PCI_CFG_DEVICE_ID);
		    dp->pd_id.revision_id = 
			(unsigned char)pci_get_cfg(cfg_p, PCI_CFG_REV_ID);

                    /* disable memory,io, and bus master */
                    pci_put_cfg(cfg_p, PCI_CFG_COMMAND, 0);
                      
		    ep = malloc(sizeof(edt_t));
		    bzero(ep, sizeof(edt_t));
		    dp->pd_edt = ep;

		    ep->e_base3 = (caddr_t) cfg_p;
		    ep->e_base = (caddr_t) up;

		    /*
		     * XXX I still need to deal with bridges.  For now,
		     * just skip them.
		     * Bit3 is going to change from IBM to DEC
		     * so the vendor and device id list has to be
		     * expanded.
		     * XXX also, this does not do natural alignment.
		     */
		    if (dp->pd_id.vendor_id == 0x1014 &&
			dp->pd_id.device_id == 0x0022) {
			    /* do nothing */
		    } else {
			    int cbase;
			    for (cbase = PCI_CFG_BASE_ADDR_0;
				 cbase <= PCI_CFG_BASE_ADDR_5;
				 cbase += 4) {
				    addr = pci_put_cfg(cfg_p, cbase, 0xffffffff);
				    addr = pci_get_cfg(cfg_p, cbase);
				    if (addr & 1)
					    continue; /* don't bother with IO space */
				    if ((addr & 0xfffffff0) == 0)
					    continue; /* doesn't need any MEM space */
				    dp->pd_mem_size = addr_size(addr & ~0xf);
				    pci_put_cfg(cfg_p, cbase, ((int)up) | PCI_IO);
				    up += io_slot_delta(slot, dp->pd_mem_size);
			    }
		    }

		    if (verbose_pci)
			    DPRINTF("pci_startup: found slot %d vend 0x%x, 0x%x, %d size 0x%x\n",
				    slot,
				    dp->pd_id.vendor_id,
				    dp->pd_id.device_id,
				    dp->pd_id.revision_id,
				    dp->pd_mem_size);
		}
		cfg_p += PCI_CFG_SIZE;
	}


	/*
	 * Now see if there is an attach function for the devices that
	 * we found.  If there is, call it.
	 */
	for (slot = FIRST_DEV_NUM; slot <= LAST_DEV_NUM; slot++) {
	    if (dp = bus_dp[slot]) {
		if (rp = find_reg(&dp->pd_id)) {
		    pci_record_t *pr_p = rp->preg_record;

		    if ( !(pr_p->pci_flags & PCI_NO_AUTO_ATTACH) ) {
			    (*(pr_p->pci_attach))(pr_p, dp->pd_edt, pci_node);
		    }
		}
		dp->pd_next = pci_dev_first;
		pci_dev_first = dp;
	    }
	}
	return 0;
}

/*
 * Shutdown a pci bus
 */
pci_shutdown(void *bus_addr, __int32_t reasons)
{
	pci_dev_t *dp, *ldp = NULL;
	pci_record_t *pr_p;

	dp = pci_dev_first;
	while (dp) {
		if (dp->pd_bus == bus_addr) {
		    if (dp->pd_reg) {
			pr_p = dp->pd_reg->preg_record;
			(*(pr_p->pci_detach))(pr_p, dp->pd_edt, reasons);
			free(dp->pd_reg);
			dp->pd_reg = NULL;
		    }
		    if (ldp) {
			ldp->pd_next = dp->pd_next;
		    } else {
			pci_dev_first = dp->pd_next;
		    }
		} else {
		    ldp = dp;
		}
		dp = dp->pd_next;
	}
	return 0;
}

/*
 * Setup an interrupt for this particular PCI driver
 */
pci_setup_intr(edt_t *ep, pci_intr_t *ip)
{
	pci_dev_t *dp;
	pci_dev_intr_t *dip;

	/* find the correct pci_dev */
	dp = pci_dev_first;
	while (dp) {
		if (dp->pd_edt == ep)
			break;
		dp = dp->pd_next;
	}

	/* add the interrupt for this device */
	if (dp) {
		dip = malloc(sizeof(pci_dev_intr_t));
		dip->di_next = dp->pd_intr;
		dip->di_next = dip;
		dip->di_intr = ip;
	}

	return 0;
}

/*
 * Remove the interrupt to get it out of the setup
 */
pci_remove_intr(edt_t *ep, pci_intr_t *ip)
{
	pci_dev_t *dp;
	pci_dev_intr_t *dip, *ldip = NULL;

	/* find the correct pci_dev */
	dp = pci_dev_first;
	while (dp) {
		if (dp->pd_edt == ep)
			break;
		dp = dp->pd_next;
	}

	/* find and remove interrupt */
	if (dp) {
		dip = dp->pd_intr;
		while (dip) {
			if (dip->di_intr == ip)
				break;
			ldip = dip;
			dip = dip->di_next;
		}
		if (dip) {
			if (ldip) {
				ldip->di_next = dip->di_next;
			} else {
				dp->pd_intr = dip->di_next;
			}
			free(dip);
		}
	}

	return 0;
}

#ifdef SIM_PCICFG
uint config_data1[] = {0x81789004, 0x0, 0x1000, 0, 1};
int config_data1_size = sizeof(config_data1) / sizeof(uint);
uint config_data2[] = {0x81789004, 0x0, 0x1000, 0, 1};
int config_data2_size = sizeof(config_data2) / sizeof(uint);
char *pci_config_space;

/*
 * Malloc a piece of memory and fool the rest of the code into thinking
 * that it is the configuration spaces of all the possible PCI devices
 * on the bus.  Most of the spaces will have FFFF, which is invalid
 * vendor ID.  But one one slot, put in the values for the Adaptec.
 */
static unsigned char *
init_pci_config_space()
{
	int config_size, i;
	uint *wordp;

	config_size = ((LAST_DEV_NUM - FIRST_DEV_NUM + 1) * PCI_CFG_SIZE);

	pci_config_space = malloc(config_size);
	if (pci_config_space == NULL) {
		printf("init_pci_config_space: could not allocate %d bytes\n",
		       config_size);
		return NULL;
	} else if (verbose_pci) {
		DPRINTF("config space at 0x%x for 0x%x bytes\n",
			pci_config_space, config_size);
	}

	/*
	 * The data should be 0xffff, but I'll keep it at 0 for now.
	 */
	for (i=0; i < config_size; i++)
		pci_config_space[i] = 0;

	wordp = (uint *) pci_config_space;
	for (i=0; i < config_data1_size; i++)
		wordp[i] = config_data1[i];

	wordp += PCI_CFG_SIZE/4;
	for (i=0; i < config_data2_size; i++)
		wordp[i] = config_data2[i];

	return pci_config_space;
}

#endif /* SIM_PCICFG */
