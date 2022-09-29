#ifndef __IDE_PCI_H__
#define __IDE_PCI_H__
/*
 * d_pci.h
 *
 * 	IDE header for PCI tests
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "ide/godzilla/include/d_pci.h:  $Revision: 1.6 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* used in pci_config.c */
#define	IOC3_PCI_ID_DEF			0x310A9
#define	IOC3_PCI_SLOT			0x2
#define	IOC3_PCI_LAT_TMR_MASK		0xff00
#define	IOC3_PCI_ADDR_BASE_MASK		0xfff00000

/* used in pci_mem.c */
#define	IOC3_INT_OUT_MASK		0x4007ffff
#define	IOC3_PPCR_A_MASK		0x1ffff
#define	IOC3_PPCR_B_MASK		0x1ffff
#define	IOC3_PPBR_H_A_MASK		0xffffffff
#define	IOC3_PPBR_H_B_MASK		0xffffffff
#define	IOC3_SBBR_H_MASK		0xffffffff
#define	IOC3_SBBR_L_MASK		0xfffff001
#define	IOC3_SRCIR_A_MASK		0xff8
#define	IOC3_SRCIR_B_MASK		0xff8
#define	IOC3_SRPIR_A_MASK		0xff8
#define	IOC3_SRPIR_B_MASK		0xff8
#define	IOC3_STPIR_A_MASK		0xff8
#define	IOC3_STPIR_B_MASK		0xff8
#define	IOC3_STCIR_A_MASK		0xff8
#define	IOC3_STCIR_B_MASK		0xff8
#define	IOC3_SRTR_A_MASK		0x3ff
#define	IOC3_SRTR_B_MASK		0x3ff
#define	IOC3_ERBR_H_MASK		0xffffffff
#define	IOC3_ERBR_L_MASK		0xfffff000
#define	IOC3_ERPIR_MASK			0xff8
#define	IOC3_ERCIR_MASK			0xff8
#define	IOC3_ERBAR_MASK			0xffff0000
#define	IOC3_ETBR_H_MASK		0xffffffff
#define	IOC3_ETBR_L_MASK		0xffffc001
#define	IOC3_ETPIR_MASK			0xff80
#define	IOC3_ETCIR_MASK			0xff80
#define	IOC3_ERTR_MASK			0x7ff
#define	IOC3_EBIR_MASK			0xffffdf1f
#define	IOC3_EMAR_H_MASK		0xffff
#define	IOC3_EMAR_L_MASK		0xfffffffe
#define	IOC3_EHAR_H_MASK		0xffffffff
#define	IOC3_EHAR_L_MASK		0xffffffff
#define	IOC3_MIDR_MASK			0xffff

/* constants used in pci_intr.c */
#define	PCI_INTR_TEST_SERIAL_A		0x0
#define	PCI_INTR_TEST_SERIAL_B		0x1
#define	PCI_INTR_TEST_PARALLEL		0x2
#define	PCI_INTR_TEST_KEYBOARD		0x3
#define	PCI_INTR_TEST_REALTIME		0x4
#define	PCI_INTR_TEST_LASTDEVICE	0x4

#define PCI_DEVICE_SIZE	0x200000

/* structure definitions used in pci_mem test */
typedef	struct	_IOC3_Regs {
	char		*name;		/* name of the register */
	ioc3reg_t	offset;		/* address offset */
	__uint32_t	mask;		/* read-write mask */
} IOC3_Regs;

#define	SIO_REG_RD(add, mask, val) {					\
	val = (uchar_t) (*(uchar_t *)(PHYS_TO_K1(BRIDGE_BASE+add))) & mask;	\
}

#define	SIO_REG_WR(add, mask, val) {				\
	(*(uchar_t *)(PHYS_TO_K1(BRIDGE_BASE+add))) = val & mask;		\
}

#endif /* C || C++ */

#endif	/* __IDE_PCI_H__ */
