/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1996, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(IP22) || defined(EVEREST)
#ident  "io/pci_intf.c: $Revision: 1.11 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/pci_intf.h>
#include <sys/pci_kern.h>

#ifdef EVEREST
#include <sys/immu.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#endif

#define	get_bus(_x)	((_x) & 0xffff)
int pcidevflag = 0;
int pcidebug = 1;

#ifdef DEBUG
extern int pcidebug;
# define DBG(s) {if(pcidebug) {s;} }
#else
# define DBG(s)
#endif

#ifndef EVEREST
void
pciedtinit(struct edt *e) {
	pci_startup(e->e_base);
}
#else
#define	PCI_GIO_LWINSIZE	0x200000
#define	PCI_GIO_BASE		0x1f400000
#define	PCI_GIO_PROBE_TO	0x200		/* 32/25 usecs at 25/33 Mhz */

#define DANG_IRS_DISBLALL ( \
			    DANG_IRS_CERR | \
			    DANG_IRS_WGFL | \
			    DANG_IRS_WGFH | \
			    DANG_IRS_WGPERR | \
			    DANG_IRS_GIO0 | \
			    DANG_IRS_GIO1 | \
			    DANG_IRS_GIO2 | \
			    DANG_IRS_GIOSTAT | \
			    DANG_IRS_DMASYNC | \
			    DANG_IRS_GFXDLY | \
			    DANG_IRS_GIOTIMEOUT)

#define	GIO_SID_MASK		0xff

typedef struct {
	unchar	gioid;
	unchar	winnum;
	unchar	slot;
	unchar	padap;
	void	*swin;
	void	*lwin;
	unsigned long long *dang;
	unsigned char	   *gio;
	void	(*intr_f)(void *);
	void	*intr_arg;
} pci_adap_t;

#define PCI_MAX	16
pci_adap_t pciadap[PCI_MAX];

void
pciedtinit(struct edt *e) {
	int	unit = 0;
	uint	dang_indx;
	uint	slot, adap, window;
	pci_adap_t *ppa = pciadap;
	unsigned long long *dang;

#if defined(MULTIKERNEL)
	/* All I/O boards belong ONLY to the golden cell */
	if (evmk_cellid != evmk_golden_cellid)
    		return;
#endif /* MULTIKERNEL */
	for (dang_indx = 0; dang_indx < DANG_MAX_INDX; dang_indx++) {
		if ((dang_id[dang_indx] & GIO_SID_MASK) != 0x21)
			continue;
		bzero(ppa, sizeof(pci_adap_t));

		slot = ppa->slot = DANG_INX2SLOT(dang_indx);
		adap = ppa->padap = DANG_INX2ADAP(dang_indx);
		window = ppa->winnum = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
		ppa->dang = ppa->swin = (void *)DANG_INX2BASE(dang_indx);

		ppa->lwin = iospace_alloc(PCI_GIO_LWINSIZE,
					  LWIN_PFN(window, adap) +
					  ((PCI_GIO_BASE & GIO_BUS_MASK) >> IO_PNUMSHFT));

		dang = ppa->dang;

		/* Set to 32-bit pipelined mode */
		EV_SET_REG(dang+DANG_UPPER_GIO_ADDR,GIO_UPPER(PCI_GIO_BASE));
		EV_SET_REG(dang+DANG_MIDDLE_GIO_ADDR,GIO_MIDDLE(PCI_GIO_BASE));

		EV_SET_REG(dang + DANG_GIO64, 0); 

		/* Disable DANG interrupts, for now */
		EV_SET_REG(dang + DANG_INTR_ENABLE, DANG_IRS_DISBLALL);

		EV_SET_REG(dang + DANG_PIPELINED, 1);
		EV_SET_REG(dang + DANG_BIG_ENDIAN, 1);

		/* Clear DANG errors */
		EV_GET_REG(dang + DANG_PIO_ERR_CLR);        /* Clear any PIO error */
		EV_GET_REG(dang + DANG_DMAM_ERR_CLR);       /* Clear DMA master error */
		EV_GET_REG(dang + DANG_DMAS_ERR_CLR);       /* Clear DMA slave error */
		EV_GET_REG(dang + DANG_DMAM_COMPLETE_CLR);  /* Clear DMAM complete */

		/* Clear PIO timeout error and set PIO timeout value */
		EV_SET_REG(dang + DANG_CLR_TIMEOUT, 1);
		EV_SET_REG(dang + DANG_MAX_GIO_TIMEOUT, PCI_GIO_PROBE_TO);

		/* Program DANG so that GIO/FC registers are accessable from large window */

		pci_startup(ppa->lwin);

		ppa++;
		unit++;
	}
}

pci_badaddr(void *addr, int size, void *vp) {
	return ((__psint_t)addr & 0x40000) ? 0 : 1;
}

void
p_dang_do_intr(eframe_t *ep, void *arg) {
	pci_adap_t *ppa = (pci_adap_t *)arg;

	if (ppa->intr_f) {
		(*ppa->intr_f)(ppa->intr_arg);
	} else {
		printf("p_dang: no interrupt routine\n");
	}
}

static void *
find_dang(void *addr) {
	__psint_t base = (__psint_t)addr;
	pci_adap_t *ppa = pciadap;
	void *dang = NULL;

	base &= ~0x1fffff;
	while (ppa->dang) {
		if ((__psint_t)ppa->lwin == base) {
			dang = ppa->dang;
			break;
		}
		ppa++;
	}
	return dang;
}

static
clear_dang(void *dang_in) {
	evreg_t dang_resp;
	volatile unsigned long long *dang = dang_in;

	if ((dang_resp = EV_GET_REG(dang + DANG_INTR_STATUS)) &
			DANG_IRS_GIOTIMEOUT) {
		EV_SET_REG(dang + DANG_CLR_TIMEOUT, 1);
	}
	return (dang_resp & DANG_IRS_GIOTIMEOUT);
}

pci_dang_read(void *addr, int width) {
	void *dang = NULL;
	int val;

	dang = find_dang(addr);
	while (clear_dang(dang))
		;
	do {
	  switch (width) {
	    case 1:
		val = *(uchar_t *)addr;
		break;
	    case 2:
		val = *(ushort_t *)addr;
		break;
	    case 4:
		val = *(uint_t *)addr;
		break;
	  }
	} while (clear_dang(dang));

	return val;
}

void
pci_dang_write(void *addr, int val, int width) {
	void *dang = NULL;

	dang = find_dang(addr);
	while (clear_dang(dang))
		;
	do {
	  switch (width) {
	    case 1:
		*(uchar_t *)addr = val;
		break;
	    case 2:
		*(ushort_t *)addr = val;
		break;
	    case 4:
		*(uint_t *)addr = val;
		break;
	  }
	} while (clear_dang(dang));
}

void
pci_dang_intr(void *busaddr, void (*rtn)(), void *arg) {
	unsigned long long *dang;
	pci_adap_t *ppa = pciadap;
	int i;

	while (ppa->dang) {
	    ppa->intr_f = rtn;
	    ppa->intr_arg = arg;
	    dang = ppa->dang;
	    i = dang_intr_conn((evreg_t*)(dang+DANG_INTR_GIO_2), EVINTR_ANY,
			   (EvIntrFunc *)p_dang_do_intr, ppa);
	    if (i) 
		EV_SET_REG(dang + DANG_INTR_ENABLE, DANG_IRS_GIO2 | DANG_IRS_ENABLE);
	    else
		printf("fcdbg_init() : failed to allocate interrupt for GIO_INTR_2 for address 0x%x\n", busaddr);
	    ppa++;
	}
}
#endif /* EVEREST */

/* ARGSUSED */
int
pciopen(dev_t *devp, int flag, int otyp, struct cred *crp) {
	int error = 0;

	DBG(printf("pciopen(%x,%x) bus %d.\n", *devp, flag, get_bus(*devp)));

	DBG(if(error)printf("pciopen errno %d\n", error));
	return error;
}

/* ARGSUSED */
int
pciclose(dev_t dev, int flag, int otyp, struct cred *crp) {
	int error = 0;

	DBG(printf("pciclose(%x,%x) bus %d.\n", dev, flag, get_bus(dev)));

	return error;
}

/* ARGSUSED */
int
pciioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *crp, int *rvalp) {
	int error = 0;

	switch (cmd) {
	    case PCI_ATTACH:
	    case PCI_DETACH:
	      {
		pci_ident_t	idp;
		pci_reg_t	*rp;
		int		count;

		if (copyin((caddr_t)arg, (caddr_t)&idp, sizeof(idp))) {
			error = EFAULT;
			break;
		}
		if ((rp = pci_find_reg(&idp)) == NULL) {
			error = EINVAL;
			break;
		}
		if (cmd == PCI_ATTACH) {
			count = pci_attach_devs(rp, PCI_SLOT_ANY);
		} else {
			count = pci_detach_devs(rp, PCI_SLOT_ANY, 0);
		}
		*rvalp = count;
	      }
	      break;

	    case PCI_UNREGISTER:
	      {
		pci_ident_t	idp;
		pci_reg_t	*rp;
		int		count;

		if (copyin((caddr_t)arg, (caddr_t)&idp, sizeof(idp))) {
			error = EFAULT;
			break;
		}
		if ((rp = pci_find_reg(&idp)) == NULL) {
			error = EINVAL;
			break;
		}
		count = pci_detach_devs(rp, PCI_SLOT_ANY, 0);
		if (rp->preg_record == NULL) {
			error = EINVAL;
			break;
		}
		if (pci_unregister(rp->preg_record, 0)) {
			error = EBUSY;
			break;
		}
		*rvalp = count;
	      }
	      break;

	    case PCI_STARTUP:
	    case PCI_SHUTDOWN:

	    default:
		error = EINVAL;
		break;
	}
	return error;
}

#endif /* IP22 || EVEREST */
