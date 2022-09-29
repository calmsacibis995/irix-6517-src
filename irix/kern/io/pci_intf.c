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

#if defined(IP22) || defined(EVEREST)
#ident  "io/pci_intf.c: $Revision: 1.54 $"

#define	PCI_DEBUG	1

#ifdef EVEREST
#define MLDTEST_DRV 1
#endif

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
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
#include <sys/PCI/PCI_defs.h>

#ifdef IP22
#define	TYPE0_CFG_SLOT0		0x80000
#define PCI_CFG_SIZE		0x40000
#define	DEVICE_IO_0		0x100000
#define	FIRST_DEV_NUM		1
#define LAST_DEV_NUM		2

#elif defined(EVEREST)
#define	TYPE0_CFG_SLOT0		0x80000
#define PCI_CFG_SIZE		0x40000
#define	DEVICE_IO_0		0x100000
#define	FIRST_DEV_NUM		1
#define LAST_DEV_NUM		2

#else

#include <sys/PCI/bridge.h>
#include <sys/PCI/pcibr.h>

#define	TYPE0_CFG_SLOT0		BRIDGE_TYPE0_CFG_DEV0
#define	PCI_CFG_SIZE		BRIDGE_TYPE0_CFG_OFF
#define	DEVICE_IO_0		BRIDGE_DEVIO0
#define	FIRST_DEV_NUM		0
#define LAST_DEV_NUM		7
#endif

#include <sys/pci_intf.h>
#include <sys/pci_kern.h>

mutex_t		pci_reg_lock;
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
	int		pd_slot;
	pci_dev_intr_t	*pd_intr;
	edt_t		*pd_edt;	/* created edt for later reference */
	pci_reg_t	*pd_reg;	/* driver currently attached */
};
typedef struct pci_dev pci_dev_t;

mutex_t		pci_dev_lock;
pci_dev_t	*pci_dev_first = NULL;

extern char pci_no_auto[];

static id_match(pci_ident_t *sp, pci_ident_t *dp);

/*
 * Routine used to register a particular PCI card
 * The pci_record_t must be allocated and preserved by the caller.
 */
pci_register(pci_record_t *pr_p)
{
	pci_reg_t	*rp;

	/* check to make sure that this driver is nice */
	if ((pr_p->pci_attach == NULL) || (pr_p->pci_detach == NULL)) {
		return -1;
	}

	/* get a pci_reg record and fill it */
	if ((rp = kmem_zalloc(sizeof(pci_reg_t), KM_CACHEALIGN)) == NULL) {
		return -1;
	}
	rp->preg_record = pr_p;

	mutex_lock(&pci_reg_lock, PRIBIO);
	/* add it to the list */
	rp->preg_next = pci_reg_first;
	pci_reg_first = rp;
	mutex_unlock(&pci_reg_lock);

	/* then if we have devices already recognized start them up */
	if ( !(pr_p->pci_flags & PCI_NO_AUTO_ATTACH) && !*pci_no_auto) {
		pci_attach_devs(rp, PCI_SLOT_ANY);
	}
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
	mutex_lock(&pci_dev_lock, PRIBIO);
	dp = pci_dev_first;
	while (dp) {
		if (dp->pd_reg) {
		    if (dp->pd_reg->preg_record == pr_p) {
			if (reasons & PCI_FORCE_DETACH) {
			    (*(pr_p->pci_detach))(pr_p, dp->pd_edt, reasons);
			    dp->pd_reg = NULL;
			}
			if (dp->pd_reg) {
			    mutex_unlock(&pci_dev_lock);
			    return -1;		/* Still have someone in use */
		        }
		    }
		}
		dp = dp->pd_next;
	}
	mutex_unlock(&pci_dev_lock);

	/* Now find the registration */
	mutex_lock(&pci_reg_lock, PRIBIO);
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
	mutex_unlock(&pci_reg_lock);
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

pci_attach_devs(pci_reg_t *rp, int slot)
{
	pci_dev_t *dp;
	pci_record_t *pr_p = rp->preg_record;
	int	count = 0;
#if PCI_DEBUG
	printf("pci_attach_devs: trying to attach vend 0x%x dev 0x%x rev 0x%x\n",
	       pr_p->pci_id.vendor_id, pr_p->pci_id.device_id,
	       pr_p->pci_id.revision_id);
#endif
	mutex_lock(&pci_dev_lock, PRIBIO);
	dp = pci_dev_first;
	while (dp != NULL) {
#if PCI_DEBUG
	    printf("known dev: vend 0x%x dev 0x%x rev 0x%x\n",
		   dp->pd_id.vendor_id, dp->pd_id.device_id,
		   dp->pd_id.revision_id);
#endif
	    if (id_match(&pr_p->pci_id, &dp->pd_id)) {
		if (((slot == PCI_SLOT_ANY) || (dp->pd_slot == slot)) &&
				dp->pd_reg == NULL) {
		    (*(pr_p->pci_attach))(pr_p, dp->pd_edt);
		    dp->pd_reg = rp;
		    count++;
		}
	    }
	    dp = dp->pd_next;
	}
	mutex_unlock(&pci_dev_lock);
	return count;
}

pci_detach_devs(pci_reg_t *rp, int slot, int reasons)
{
	pci_dev_t *dp;
	pci_record_t *pr_p = rp->preg_record;
	int	count = 0;
#if PCI_DEBUG
	printf("detach_devs: trying to detach vend 0x%x dev 0x%x rev 0x%x\n",
	       pr_p->pci_id.vendor_id, pr_p->pci_id.device_id,
	       pr_p->pci_id.revision_id);
#endif
	mutex_lock(&pci_dev_lock, PRIBIO);
	dp = pci_dev_first;
	while (dp != NULL) {
#if PCI_DEBUG
	    printf("known dev: vend 0x%x dev 0x%x rev 0x%x\n",
		   dp->pd_id.vendor_id, dp->pd_id.device_id,
		   dp->pd_id.revision_id);
#endif
	    if (id_match(&pr_p->pci_id, &dp->pd_id)) {
		if (((slot == PCI_SLOT_ANY) || (dp->pd_slot == slot)) &&
				dp->pd_reg != NULL) {
		    (*(pr_p->pci_detach))(pr_p, dp->pd_edt, reasons);
		    dp->pd_reg = NULL;
		    count++;
		} else {
#if DEBUG
		    printf("trying to detach unattached dev: vend 0x%x dev 0x%x rev 0x%x\n",
			   dp->pd_id.vendor_id, dp->pd_id.device_id,
			   dp->pd_id.revision_id);
#endif
		}
	    }
	    dp = dp->pd_next;
	}
	mutex_unlock(&pci_dev_lock);
	return count;
}

pci_reg_t *
pci_find_reg(pci_ident_t *ip)
{
	pci_reg_t *rp;

	mutex_lock(&pci_reg_lock, PRIBIO);
	rp = pci_reg_first;
	while (rp != NULL) {
		if (id_match(ip, &rp->preg_record->pci_id))
			break;
		rp = rp->preg_next;
	}
	mutex_unlock(&pci_reg_lock);
	return rp;
}

#define	MAX_ADDR_SIZE	0x00100000

static __uint32_t
addr_size(__uint32_t addr)
{
	/* keep only the "base address" part */
	if (addr & 1)	addr &= ~3;	/* PCI I/O space */
	else		addr &= ~15;	/* PCI MEM space */

	/* keep just the LS bit */
	addr &= ~(addr - 1);


	/* if nothing set, or too big, set to max. */
	if ((addr < 1) || (addr > MAX_ADDR_SIZE))
	    addr = MAX_ADDR_SIZE;
	return addr;
}

/* ARGSUSED */
static __uint32_t
io_slot_delta(int slot, __uint32_t addrsize)
{
	__uint32_t retval;

#ifdef IP22
	retval = 0x80000;
#elif defined(EVEREST)
	retval = 0;
#else	/* all other machines */
	retval = BRIDGE_DEVIO_OFF;
	if (slot < 2)
		retval += BRIDGE_DEVIO_OFF;
#endif

	return retval;
}
#ifdef MLDTEST_DRV
int mldtd_data[16] = {0xBADD10A9, 0, 0xff01, 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0, 0, 0, 0};
mldtd_getcfg(int word) {
	return mldtd_data[(word >>2)];
}
void
mldtd_putcfg(int word, int val) {
	mldtd_data[(word >>2)] = val;
}
#endif
/*
 * Read a value from a configuration register.
 */
u_int32_t
pci_get_cfg(volatile unsigned char *bus_addr, int cfg_reg)
{
	/* SGI PCI devices require us to use 32 bit accesses.
	 * Everyone else is required "by law" to allow them.
	 */
	int		byte = cfg_reg & 3;
	int		word = cfg_reg &~ 3;
	u_int32_t	data;

#if defined(MLDTEST_DRV) && defined(EVEREST)
	if (((__psint_t)bus_addr & 0x40000) == 0)
		data = mldtd_getcfg(word);
	else
#endif /* MLDTEST_DRV */
	data = *(volatile u_int32_t *)(bus_addr + word);

	/* byte within longword is LITTLEENDIAN chosen. */
	if (byte) data >>= byte * 8;

	/* mask to correct data size based on register */
	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_COMMAND:
	case PCI_CFG_STATUS:
		data &= 0xFFFF;		/* 16 bit */
		break;

	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
	case PCI_CFG_HEADER_TYPE:
	case PCI_CFG_BIST:
		data &= 0xFF;		/* 8 bit */
		break;
		   
	case PCI_CFG_BASE_ADDR_0:
	case PCI_CFG_BASE_ADDR_1:
		break;			/* 32 bit */
		   
	default:
		/* unknown */
		if (cfg_reg < PCI_CFG_VEND_SPECIFIC)
			cmn_err(CE_WARN, "pci_get_cfg: accessing 0x%x",
				cfg_reg);

		if (byte & 1)
			data &= 0xFF;
		else if (byte & 2)
			data &= 0xFFFF;
		break;
	}
	return data;
}


/*
 * Write a value into a configuration register
 */
int
pci_put_cfg(volatile unsigned char *bus_addr, int cfg_reg, __int32_t val)
{
	int		word;
	int		byte;
	int		shft;
	int		mask;
	int		data;

	/* SGI PCI devices require us to use 32 bit accesses.
	 * Everyone else is required "by law" to allow them.
	 * So we write smaller things by doing 32-bit RMW
	 * cycles.
	 *
	 * UNFORTUNATELY, the existance of RW1C
	 * bits in small registers makes it tricky to update
	 * certain other registers, so we have to check
	 * for some special cases.
	 *
	 * As long as we are switching on the register,
	 * we can warn about some read-only registers.
	 */
#if defined(MLDTEST_DRV) && defined(EVEREST)
	if (((__psint_t)bus_addr & 0x40000) == 0) {
		mldtd_putcfg(cfg_reg, val);
		return 0;
	}
#endif /* MLDTEST_DRV */

	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_HEADER_TYPE:
		cmn_err(CE_WARN, "pci_put_cfg: writing to read-only reg 0x%x",
			cfg_reg);
		break;

	case PCI_CFG_COMMAND:
		/* The PCI Command register shares space with the STATUS
		 * register. To "not modify" the STATUS register, we
		 * write all zeros to it. COMMAND is in the lower
		 * half of the word, STATUS is in the upper half, so
		 * we don't have to modify "val" at all for this.
		 * But, we mask off the top bits anyway, just in
		 * case some caller does something silly.
		 */
		*(volatile u_int32_t *)(bus_addr + cfg_reg) = val & 0xFFFF;
		break;
		   
	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
		/*
		 * These registers share a longword with the
		 * BIST register, and we do not want to restart
		 * the BIST as a side effect, so do the RMW but
		 * filter out the "Start BIST" bit.
		 */
		word = cfg_reg &~ 3;
		byte = cfg_reg & 3;
		shft = 8 * byte;
		mask = 0xFF << shft;
		data = *(volatile u_int32_t *)(bus_addr + word);
		data &= ~(1<<30);
		val = (~mask & data) | (mask & (val << shft));
		*(volatile u_int32_t *)(bus_addr + word) = val;
		break;

	default:
		/* Other registers: 32-bit store, or a Normal RMW cycle
		 * with size determined from the alignment.
		 * Any vendor or card specific gotchas found need to be
		 * hacked in above, or managed by that card's driver.
		 */
		word = cfg_reg &~ 3;
		byte = cfg_reg & 3;
		if (byte != 0) {
			shft = 8 * byte;
			mask = (byte & 1) ? 0xFF : 0xFFFF;
			mask <<= shft;
			data = *(volatile u_int32_t *)(bus_addr + word);
			val = (~mask & data) | (mask & (val << shft));
		}
		*(volatile u_int32_t *)(bus_addr + word) = val;
		break;
	}
	return 0;
}

/*
 * Routine used to get a bus on-line
 */
pci_startup(void *bus_addr)
{
	volatile unsigned char *cfg_p, *up;
	pci_dev_t *dp, *bus_dp[LAST_DEV_NUM + 1];
	pci_reg_t *rp;
	edt_t	*ep;
	int	slot;
	u_int16_t tmp_s;
	/* REFERENCED */
	int	val;

	if (*pci_no_auto) {
		if ((*pci_no_auto >= '0') &&
				(*pci_no_auto <= '9')) {
			*pci_no_auto -= '0';
		}
	}
#ifdef PCI_DEBUG
	printf("pci_s: entered 0x%x\n", bus_addr);
#endif

	/*
	 * Step thru the config slots for this bridge and see what we have.
	 * Assign config addresses and IO mappings as we find the cards.
	 */
	cfg_p = (unsigned char *)bus_addr + TYPE0_CFG_SLOT0;
	up = (unsigned char *)bus_addr + DEVICE_IO_0;

	for (slot = FIRST_DEV_NUM; slot <= LAST_DEV_NUM; slot++) {
		bus_dp[slot] = NULL;
#ifdef IP22
		if (badaddr_val((void *)cfg_p,sizeof(int),&val) != 0) {
			cfg_p += PCI_CFG_SIZE;
			continue;
		}
#elif defined(EVEREST)
#ifndef MLDTEST_DRV
		if (pci_badaddr((void *)cfg_p,sizeof(int),&val) != 0) {
			cfg_p += PCI_CFG_SIZE;
			continue;
		}
#endif /* MLDTEST_DRV */
#endif
		tmp_s = (u_int16_t)pci_get_cfg(cfg_p, PCI_CFG_VENDOR_ID);
#ifdef PCI_DEBUG
		printf("pci_s: checking 0x%x\n", cfg_p);
#endif

		/*
		 * if non zero then we have a device
		 * According to PCI 2.1, 0xffff is an invalid vendor id
		 * Also, Moosehead returns -1 if the config read failed.
		 */
		if (tmp_s && (tmp_s != 0xffff)) {
		    /* allocate and add to this bus set */
		    dp = kern_malloc(sizeof(pci_dev_t));
		    bzero(dp, sizeof(pci_dev_t));
		    bus_dp[slot] = dp;

		    dp->pd_bus = bus_addr;
		    dp->pd_cfg = (unsigned char *)cfg_p;
		    dp->pd_slot = slot;

		    /* get the id info */
		    dp->pd_id.vendor_id = tmp_s;
		    dp->pd_id.device_id = 
			(u_int16_t)pci_get_cfg(cfg_p, PCI_CFG_DEVICE_ID);
		    dp->pd_id.revision_id = 
			(unsigned char)pci_get_cfg(cfg_p, PCI_CFG_REV_ID);
		    pci_put_cfg(cfg_p, PCI_CFG_BASE_ADDR_0, 0xffffffff);
		    dp->pd_mem_size =
			addr_size(pci_get_cfg(cfg_p, PCI_CFG_BASE_ADDR_0));

		    pci_put_cfg(cfg_p, PCI_CFG_COMMAND,
				(PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE |
				 PCI_CMD_BUS_MASTER | PCI_CMD_MEMW_INV_ENAB));

		    ep = kern_malloc(sizeof(edt_t));
		    bzero(ep, sizeof(edt_t));
		    dp->pd_edt = ep;

		    ep->e_base3 = (caddr_t)cfg_p;
		    ep->e_base = (caddr_t)up;
#ifdef EVEREST

		    pci_put_cfg(cfg_p, PCI_CFG_BASE_ADDR_0,
				(0x1f500000 | ((__psunsigned_t)ep->e_base & 0xf0000)));
		    pci_put_cfg(cfg_p, PCI_CFG_BASE_ADDR_1,
				(0x1f500001 | ((__psunsigned_t)ep->e_base & 0xf0000)));

		    /* setup the byte count register on the board */
		    *(__uint32_t *)((__psint_t)bus_addr + 0x40000) = 256;
#else	/* CPU addresses appear unchanged on PCI bus */
		    pci_put_cfg(cfg_p, PCI_CFG_BASE_ADDR_0,
				((__psunsigned_t)ep->e_base & 0x3ffffffc));
		    pci_put_cfg(cfg_p, PCI_CFG_BASE_ADDR_1,
				(((__psunsigned_t)ep->e_base + dp->pd_mem_size)
					& 0x3ffffffc) | 1);
#endif
#ifdef PCI_DEBUG
		    printf("pci_startup: cfg 0x%x found vend 0x%x dev 0x%x rev %d size %d\n",
			dp->pd_cfg, dp->pd_id.vendor_id,
			dp->pd_id.device_id, dp->pd_id.revision_id,
			dp->pd_mem_size);
		    
		    printf("pci_startup: slot: %d e_base: 0x%x e_base3: 0x%x\n",
				slot, ep->e_base, ep->e_base3);
		    printf("pci_startup: base_addr1: 0x%x base_addr0: 0x%x\n",
			pci_get_cfg(cfg_p, PCI_CFG_BASE_ADDR_1),
			pci_get_cfg(cfg_p, PCI_CFG_BASE_ADDR_0));
#endif
		    up += io_slot_delta(slot, dp->pd_mem_size);

		    /* add it to the inventory */
		    add_to_inventory(INV_PCI, dp->pd_id.vendor_id,
			     0, slot, dp->pd_id.device_id);
		} else {
		    bus_dp[slot] = NULL;
		}
		cfg_p += PCI_CFG_SIZE;
	}

	/* go thru this bus and find sizes */
	for (slot = FIRST_DEV_NUM; slot <= LAST_DEV_NUM; slot++) {
		if (dp = bus_dp[slot]) {
		    ep = dp->pd_edt;
#if 0
		    ep->e_base = (caddr_t)up;
#endif
		}
	}

	/*
	 * Now put this bus set on the full list.
	 * Call the attach functions of the devices that were found.
	 */
	for (slot = FIRST_DEV_NUM; slot <= LAST_DEV_NUM; slot++) {
	    if (dp = bus_dp[slot]) {
		dp->pd_next = pci_dev_first;
		pci_dev_first = dp;
		if (rp = pci_find_reg(&dp->pd_id)) {
		    pci_record_t *pr_p = rp->preg_record;

		    if ( !(pr_p->pci_flags & PCI_NO_AUTO_ATTACH) &&
				!*pci_no_auto) {
			    (*(pr_p->pci_attach))(punifdef: 798: dangling #endif.
r_p, dp->pd_edt);
		    }
		}
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
			kern_free(dp->pd_reg);
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

static void
pci_playout_intr(pci_dev_intr_t *dip)
{
	while (dip) {
		pci_intr_t *ip = dip->di_intr;

		if (ip && ip->pci_intr) {
			(*ip->pci_intr)(ip->intr_arg);
		}
		dip = dip->di_next;
	}
#if 0	/* HACK Alert */
	clear_bridge_tout((void *)0x9200000000000000);
#endif
}

/*
 * Setup an interrupt for this particular PCI driver
 */
pci_dev_intr_t *dip_first = NULL;

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
		dip = kmem_alloc(sizeof(pci_dev_intr_t), KM_CACHEALIGN);
		dip->di_next = dip_first;
		dip_first = dip;
		dp->pd_intr = dip;
		dip->di_intr = ip;
	}
#if defined(IP22) && defined(MCCHIP) 
	/* This first one may require some changes, since, by default,
	 * VECTOR_GIO0 is non-threaded for fifo full interrupts.
	 */
        setgiovector(GIO_INTERRUPT_0, GIO_SLOT_0, (void(*)())pci_playout_intr,
                     (__psint_t)dip_first);
        setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, (void(*)())pci_playout_intr,
                     (__psint_t)dip_first);
        setgiovector(GIO_INTERRUPT_2, GIO_SLOT_0, (void(*)())pci_playout_intr,
                     (__psint_t)dip_first);
#endif

#ifdef EVEREST
	if (dp)
		pci_dang_intr(dp->pd_edt->e_base,  pci_playout_intr, dip_first);
#endif
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
		dip = dip_first;
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
				dip_first = dip->di_next;
			}
			kern_free(dip);
		}
	}
#if defined(IP22) && defined(MCCHIP)
	/* This first one may require some changes, since, by default,
	 * VECTOR_GIO0 is non-threaded for fifo full interrupts.
	 */
        setgiovector(GIO_INTERRUPT_0, GIO_SLOT_0, (void(*)())pci_playout_intr,
                        (__psint_t)dip_first);
        setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, (void(*)())pci_playout_intr,
                        (__psint_t)dip_first);
        setgiovector(GIO_INTERRUPT_2, GIO_SLOT_0, (void(*)())pci_playout_intr,
                        (__psint_t)dip_first);

#endif
	return 0;
}

#endif
