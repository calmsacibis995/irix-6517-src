/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
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

#ident "$Revision: 1.7 $"

/* 
 * This file is to be used for the modules inside this directory, namely,
 * universe chip device driver, universe dma engine device driver, user 
 * level PIO device driver.
 */

#ifndef __VME_UNIVERSE_PRIVATE_H__
#define __VME_UNIVERSE_PRIVATE_H__

#include "vmeio_private.h"

/* ------------------------------------------------------------------------
  
                     Universe -- hardware view
   
   ------------------------------------------------------------------------ */

/* 
 * Workaround for Chip Revision Number:  The rev # isn't bumped after
 * a hardware bug is fixed                     
 */
#define UNIVERSE_REVNUM_WAR     (1) 

/* 
 * Workaround for A24/A32 VMEbus Error Address Register: Universe passes
 * 32bit PCIbus address onto VMEbus during A16/A24 transactions.  When
 * error happens on VMEbus, the VMEbus error address register records
 * this PCIbus address, as opposed to A16/A24 address.
 */
#define UNIVERSE_V_AERR_WAR     (1)

/* 
 * Workaround for interrupt line connection between Universe and Bridge 
 */
#define XIOVME_INTRLINE_WAR     (1)


#define UNIVERSE_REV_1          (1)

#define	UNIVERSE_VENDOR_ID	(0x10e3)   /* Universe Device Vendor ID     */
#define	UNIVERSE_DEVICE_ID	(0)	   /* Universe Device Id            */

#define UNIVERSE_REGISTERS_SIZE (4*1024)

/* XXX Belong to some common header file */
#define	KB			(1024)
#define	MB			(1024*KB)
#define	GB			(1024*MB)

#define	ALIGN_4K		((iopaddr_t)4*KB-1)	/*  4KB alignment */
#define	ALIGN_64K		((iopaddr_t)64*KB-1)	/* 64KB alignment */
#define	ALIGN_64M		((iopaddr_t)64*MB-1)	/* 64MB Alignment */
#define	ALIGN_4GB		((iopaddr_t)4*GB-1)	/*  4GB Alignment */

#define ADDR_32_MASK            (0xffffffff)
#define ADDR_24_MASK            (0xffffff)
#define ADDR_16_MASK            (0xffff)

/* -------------------------------------------------------------------------

   Access the address of each register

   ------------------------------------------------------------------------- */

#define UNIVERSE_REG_ADDR(base, r)    (universe_reg_t *)((caddr_t)(base) +   \
							 (r))
#define	UNIVERSE_REG_GET(base, r)     (*(universe_reg_t *)((caddr_t)(base) + \
							   (r)))
#define	UNIVERSE_REG_SET(base, r, v)  (*(universe_reg_t *)((caddr_t)(base) + \
							   (r)) = (v))

/* ------------------------------------------------------------------------

   PCI Specific Registers field description.
   XXX should be specified in some PCI common file?

  ------------------------------------------------------------------------- */

/* PCI Configuration Space Control */
typedef struct universe_pci_csr_s {
	universe_reg_t d_pe:       1,
	               s_serr:     1,
		       r_ma:       1,
	               r_ta:       1,
	               s_ta:       1,
	               devsel:     2,
	               dp_d:       1,
		       tfbbc:      1,
		       reserved0:  13,
		       mfbbc:      1,
	               serr_en:    1,
	               wait:       1,
	               peresp:     1,
	               vgaps:      1,
	               mwi_en:     1,
	               sc:         1,
	               bm:         1,
	               ms:         1,
	               ios:        1;
} universe_pci_csr_t;

#define UNIVERSE_PCI_CSR_D_PE_CLEAR           (0x1)
#define UNIVERSE_PCI_CSR_S_SERR_CLEAR         (0x1)
#define UNIVERSE_PCI_CSR_R_MA_CLEAR           (0x1)
#define UNIVERSE_PCI_CSR_R_TA_CLEAR           (0x1)
#define UNIVERSE_PCI_CSR_S_TA_CLEAR           (0x1)
#define UNIVERSE_PCI_CSR_DEVSEL_MEDIUM        (0x1)
#define UNIVERSE_PCI_CSR_DP_D_CLEAR           (0x1)
#define UNIVERSE_PCI_CSR_TFBBC_DISABLED       (0x0)
#define UNIVERSE_PCI_CSR_TFBBC_ENABLED        (0x1)
#define UNIVERSE_PCI_CSR_MFBBC_DISABLED       (0x0)
#define UNIVERSE_PCI_CSR_MFBBC_ENABLED        (0x1)
#define UNIVERSE_PCI_CSR_SERR_EN_DISABLE      (0x0)
#define UNIVERSE_PCI_CSR_SERR_EN_ENABLE       (0x1)
#define UNIVERSE_PCI_CSR_WAIT_0               (0x0)
#define UNIVERSE_PCI_CSR_PERESP_DISABLE       (0x0)
#define UNIVERSE_PCI_CSR_PERESP_ENABLE        (0x1)
#define UNIVERSE_PCI_CSR_VGAPS_DISABLE        (0x0)
#define UNIVERSE_PCI_CSR_VGAPS_ENABLE         (0x1)
#define UNIVERSE_PCI_CSR_MWI_EN_DISABLED      (0x0)
#define UNIVERSE_PCI_CSR_MWI_EN_ENABLED       (0x1)
#define UNIVERSE_PCI_CSR_SC_DISABLED          (0x0)
#define UNIVERSE_PCI_CSR_SC_ENABLED           (0x1)
#define UNIVERSE_PCI_CSR_BM_DISABLED          (0x0)
#define UNIVERSE_PCI_CSR_BM_ENABLED           (0x1)
#define UNIVERSE_PCI_CSR_MS_DISABLED          (0x0)
#define UNIVERSE_PCI_CSR_MS_ENABLED           (0x1)
#define UNIVERSE_PCI_CSR_IOS_DISABLED         (0x0)
#define UNIVERSE_PCI_CSR_IOS_ENABLED          (0x1)

typedef struct universe_pci_class_s {
	universe_reg_t base: 8,
                       sub:  8,
		       prog: 8,
		       rid:  8;
} universe_pci_class_t;

#define UNIVERSE_PCI_CLASS_RID_0             (0x0)   
#define UNIVERSE_PCI_CLASS_RID_1             (0x1)

/* PCI_MISC0 */
typedef struct universe_pci_misc0_s {
	universe_reg_t bist:      1,
		       sbist:     1,
		       reserved0: 2,
		       ccode:     4,
		       mfunct:    1,
		       layout:    7,
		       ltimer:    5,
		       reserved1: 11;
} universe_pci_misc0_t;

#define UNIVERSE_PCI_MISC0_LTIMER_8CLKS  (1)
#define UNIVERSE_PCI_MISC0_LTIMER_16CLKS (2)
#define UNIVERSE_PCI_MISC0_LTIMER_24CLKS (3)
#define UNIVERSE_PCI_MISC0_LTIMER_32CLKS (4)
#define UNIVERSE_PCI_MISC0_LTIMER_40CLKS (5)
#define UNIVERSE_PCI_MISC0_LTIMER_48CLKS (6)
#define UNIVERSE_PCI_MISC0_LTIMER_56CLKS (7)
#define UNIVERSE_PCI_MISC0_LTIMER_64CLKS (8)
#define UNIVERSE_PCI_MISC0_LTIMER_72CLKS (9)
/* ... */


/* ------------------------------------------------------------------------
  
                         VMEbus Standard Registers

   ------------------------------------------------------------------------ */

/* VMEbus CSR Bit Clear */

typedef struct universe_vcsr_clr_s {
	universe_reg_t reset:     1,
	               sysfail:   1,
	               fail:      1,
	               reserved0: 29;
} universe_vcsr_clr_t;

#define UNIVERSE_VCSR_CLR_SYSFAIL_NOTASSERTED    (0)
#define UNIVERSE_VCSR_CLR_SYSFAIL_ASSERTED       (1)
#define UNIVERSE_VCSR_CLR_SYSFAIL_NEGATE         (1)

/* ------------------------------------------------------------------------
  
                                 PIO Module 

   ------------------------------------------------------------------------ */

/* PCI slave image control register */
typedef struct universe_pcisimg_ctl_s {
	universe_reg_t en:         1,
                       pwen:       1,
                       reserved0:  6,
                       vdw:        2,
                       reserved1:  3,
                       vas:        3,
		       pgm:        2,
		       super:      2,
	               reserved2:  3,
	               vct:        1,
	               reserved3:  7,
	               las:        1;
} universe_pcisimg_ctl_t;

#define UNIVERSE_PCISIMG_CTL_EN_OFF      (0x0)
#define UNIVERSE_PCISIMG_CTL_EN_ON       (0x1)

#define UNIVERSE_PCISIMG_CTL_PWEN_OFF    (0x0)
#define UNIVERSE_PCISIMG_CTL_PWEN_ON     (0x1)

#define UNIVERSE_PCISIMG_CTL_VDW_8       (0x0)
#define UNIVERSE_PCISIMG_CTL_VDW_16      (0x1)
#define UNIVERSE_PCISIMG_CTL_VDW_32      (0x2)
#define UNIVERSE_PCISIMG_CTL_VDW_64      (0x3)

#define UNIVERSE_PCISIMG_CTL_VAS_A16     (0x0)
#define UNIVERSE_PCISIMG_CTL_VAS_A24     (0x1)
#define UNIVERSE_PCISIMG_CTL_VAS_A32     (0x2)
#define UNIVERSE_PCISIMG_CTL_VAS_USER1   (0x6)
#define UNIVERSE_PCISIMG_CTL_VAS_USER2   (0x7)

#define	UNIVERSE_PCISIMG_CTL_PGM_DATA	 (0x0)
#define	UNIVERSE_PCISIMG_CTL_PGM_PGM	 (0x1)

#define	UNIVERSE_PCISIMG_CTL_SUPER_N	 (0x0)
#define	UNIVERSE_PCISIMG_CTL_SUPER_S 	 (0x1)

#define UNIVERSE_PCISIMG_CTL_VCT_SINGLE  (0x0)
#define UNIVERSE_PCISIMG_CTL_VCT_BLOCK   (0x1)

#define UNIVERSE_PCISIMG_CTL_LAS_MEM     (0x0)
#define UNIVERSE_PCISIMG_CTL_LAS_IO      (0x1)
#define UNIVERSE_PCISIMG_CTL_LAS_CONFIG  (0x2)

typedef struct universe_pcisimg_s {
	universe_pcisimg_ctl_t   ctl;
	universe_reg_t           bs;
	universe_reg_t           bd;
	universe_reg_t           to;
} universe_pcisimg_t;

extern unsigned universe_pcisimg_offset[];

/* PCI special slave image -- hardware */
typedef struct universe_pcisimg_spec_s {
	universe_reg_t en:        1,
		       pwen:      1,
                       reserved0: 6,
	               vdw3:      1,
	               vdw2:      1,
	               vdw1:      1,
	               vdw0:      1,
		       reserved1: 4,
		       pgm3:      1,
		       pgm2:      1,
		       pgm1:      1,
		       pgm0:      1,
		       super3:    1,
		       super2:    1,
		       super1:    1,
		       super0:    1,
		       bs:        6,
		       las:       2;
} universe_pcisimg_spec_t;

#define UNIVERSE_PCISIMG_SPEC_EN_OFF      (0x0)
#define UNIVERSE_PCISIMG_SPEC_EN_ON       (0x1)

#define UNIVERSE_PCISIMG_SPEC_PWEN_OFF    (0x0)
#define UNIVERSE_PCISIMG_SPEC_PWEN_ON     (0x1)

#define UNIVERSE_PCISIMG_SPEC_VDW_16      (0x0)
#define UNIVERSE_PCISIMG_SPEC_VDW_32      (0x1)

#define	UNIVERSE_PCISIMG_SPEC_PGM_DATA    (0x0)
#define	UNIVERSE_PCISIMG_SPEC_PGM_PGM     (0x1)

#define	UNIVERSE_PCISIMG_SPEC_SUPER_N     (0x0)
#define	UNIVERSE_PCISIMG_SPEC_SUPER_S     (0x1)

#define UNIVERSE_PCISIMG_SPEC_LAS_MEM     (0x0)
#define UNIVERSE_PCISIMG_SPEC_LAS_IO      (0x1)
#define UNIVERSE_PCISIMG_SPEC_LAS_CONFIG  (0x2)

#define UNIVERSE_PCISIMG_SPEC_BS_SHFT     (26)

/*
 * This's the way the 4 quadrant A16/A24 space would look.
 *
pci_vbase ------------> ________________________________
                        |                               |
                        |                               |
                        |       A24S  D32               |
                        |                               |
pci_vbase+0x0ff0000     |_______________________________|
                        |                               |
                        |       A16S  D32               |
pci_vbase+0x1000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24S  D16               |
                        |                               |
pci_vbase+0x1ff0000     |_______________________________|
                        |                               |
                        |       A16S  D16               |
pci_vbase+0x2000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24N  D32               |
                        |                               |
pci_vbase+0x2ff0000     |_______________________________|
                        |                               |
                        |       A16N  D32               |
pci_vbase+0x3000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24N  D16               |
                        |                               |
pci_vbase+0x3ff0000     |_______________________________|
                        |                               |
                        |       A16N  D16               |
pci_vbase+0x4000000     |_______________________________|
 *
 *
 */


#define UNIVERSE_PCISIMG_SPEC_A24S_BS (0)
#define UNIVERSE_PCISIMG_SPEC_A24N_BS (0x1000000)

#define UNIVERSE_PCISIMG_SPEC_A16S_BS (0xff0000)
#define UNIVERSE_PCISIMG_SPEC_A16N_BS (0x1ff0000)

#define UNIVERSE_PCISIMG_SPEC_A24S_D32_BS (0)
#define UNIVERSE_PCISIMG_SPEC_A24S_D32_BD (0x0FF0000)

#define UNIVERSE_PCISIMG_SPEC_A24S_D16_BS (0x1000000)
#define UNIVERSE_PCISIMG_SPEC_A24S_D16_BD (0x1FF0000)

#define UNIVERSE_PCISIMG_SPEC_A24N_D32_BS (0x2000000)
#define UNIVERSE_PCISIMG_SPEC_A24N_D32_BD (0x2FF0000)

#define UNIVERSE_PCISIMG_SPEC_A24N_D16_BS (0x3000000)
#define UNIVERSE_PCISIMG_SPEC_A24N_D16_BD (0x3FF0000)

#define UNIVERSE_PCISIMG_SPEC_A16S_D32_BS (0x0ff0000)
#define UNIVERSE_PCISIMG_SPEC_A16S_D32_BD (0x1000000)

#define UNIVERSE_PCISIMG_SPEC_A16S_D16_BS (0x1ff0000)
#define UNIVERSE_PCISIMG_SPEC_A16S_D16_BD (0x2000000)

#define UNIVERSE_PCISIMG_SPEC_A16N_D32_BS (0x2ff0000)
#define UNIVERSE_PCISIMG_SPEC_A16N_D32_BD (0x3000000)

#define UNIVERSE_PCISIMG_SPEC_A16N_D16_BS (0x3ff0000)
#define UNIVERSE_PCISIMG_SPEC_A16N_D16_BD (0x4000000)

#define UNIVERSE_NUM_OF_PCISIMGS          (8)
#define UNIVERSE_PCISIMG_ADDR_RES         (0x10000)
#define UNIVERSE_PCISIMG_ADDR_ALIGN(addr) ((addr) & 0xffff0000)

#define	UNIVERSE_PIO_A16_START		  (0)
#define	UNIVERSE_PIO_A16_END		  (0xffff)

#define	UNIVERSE_PIO_A24_START		  (0x800000)
#define	UNIVERSE_PIO_A24_END		  (0xfeffff)

#define	UNIVERSE_PIO_A32_START		  (0x0)
#define	UNIVERSE_PIO_A32_END		  (0x80000000)

typedef unsigned int universe_pcisimg_num_t;

/* -----------------------------------------------------------------------
 
                           DMA module 

   ----------------------------------------------------------------------- */

/* VME slave image control register */
typedef struct universe_vmesimg_ctl_s {
	universe_reg_t en:         1,
	               pwen:       1,
	               pren:       1,
	               reserved0:  5,
	               pgm:        2,
	               super:      2,
	               reserved1:  1,
	               vas:        3,
	               reserved2:  8,
	               ld64en:     1,
	               llrmw:      1,
	               reserved3:  5,
	               las:        1;
} universe_vmesimg_ctl_t;

#define UNIVERSE_VMESIMG_CTL_EN_ON       (0x1)
#define UNIVERSE_VMESIMG_CTL_EN_OFF      (0x0)

#define UNIVERSE_VMESIMG_CTL_PWEN_ON     (0x1)
#define UNIVERSE_VMESIMG_CTL_PWEN_OFF    (0x0)

#define UNIVERSE_VMESIMG_CTL_PREN_ON     (0x1)
#define UNIVERSE_VMESIMG_CTL_PREN_OFF    (0x0)

#define	UNIVERSE_VMESIMG_CTL_PGM_DATA	 (0x1)
#define	UNIVERSE_VMESIMG_CTL_PGM_PGM	 (0x2)
#define	UNIVERSE_VMESIMG_CTL_PGM_BOTH    (0x3)

#define	UNIVERSE_VMESIMG_CTL_SUPER_N	 (0x1)
#define	UNIVERSE_VMESIMG_CTL_SUPER_S 	 (0x2)
#define	UNIVERSE_VMESIMG_CTL_SUPER_BOTH	 (0x3)

#define UNIVERSE_VMESIMG_CTL_VAS_A16     (0x0)
#define UNIVERSE_VMESIMG_CTL_VAS_A24     (0x1)
#define UNIVERSE_VMESIMG_CTL_VAS_A32     (0x2)
#define UNIVERSE_VMESIMG_CTL_VAS_USER1   (0x6)
#define UNIVERSE_VMESIMG_CTL_VAS_USER2   (0x7)

#define UNIVERSE_VMESIMG_CTL_LD64EN_ON   (0x1)
#define UNIVERSE_VMESIMG_CTL_LD64EN_OFF  (0x0)

#define UNIVERSE_VMESIMG_CTL_LLRMW_ON    (0x1)
#define UNIVERSE_VMESIMG_CTL_LLRMW_OFF   (0x0)

#define UNIVERSE_VMESIMG_CTL_LAS_MEM     (0x0)
#define UNIVERSE_VMESIMG_CTL_LAS_IO      (0x1)
#define UNIVERSE_VMESIMG_CTL_LAS_CFG     (0x2)

typedef struct universe_vmesimg_s {
	universe_vmesimg_ctl_t   ctl;
	universe_reg_t           bs;
	universe_reg_t           bd;
	universe_reg_t           to;
} universe_vmesimg_t;

extern unsigned universe_vmesimg_offset[];

#define UNIVERSE_NUM_OF_VMESIMGS           (8)

#define UNIVERSE_VMESIMG_ADDR_RES          (0x10000)
#define UNIVERSE_VMESIMG_ADDR_ALIGN(addr)  ((addr) & (0xffff0000))
#define UNIVERSE_VMESIMG_ADDR_TO_BS(addr)  (((addr) & 0xffff0000) &           \
					    ADDR_32_MASK)
#define UNIVERSE_VMESIMG_ADDR_TO_BD(addr)  ((((addr) + 0xffff) &              \
					     0xffff0000) &                    \
					    ADDR_32_MASK)
#define UNIVERSE_VMESIMG_A32_TO            (0xc0000000)  
#define UNIVERSE_VMESIMG_A24_TO            (0x40000000)
#define UNIVERSE_DMA_A24_ENTRY_SIZE        (UNIVERSE_VMESIMG_ADDR_RES)
#define UNIVERSE_DMA_A24_ENTRIES           (VMEIO_DMA_A24_SIZE/             \
					     UNIVERSE_DMA_A24_ENTRY_SIZE)

#define UNIVERSE_DMA_A24_COUNT_TO_ENTRY(byte_count)                           \
     (((byte_count) + UNIVERSE_DMA_A24_ENTRY_SIZE - 1) /                      \
      UNIVERSE_DMA_A24_ENTRY_SIZE + 1)

#define UNIVERSE_DMA_A24_INDEX_TO_ADDR(index) (VMEIO_DMA_A24_START +          \
					       (index) *                      \
					       UNIVERSE_DMA_A24_ENTRY_SIZE) 
#define UNIVERSE_DMA_A24_ADDR_TO_INDEX(addr)  (((addr) -                      \
						VMEIO_DMA_A24_START) /        \
					       UNIVERSE_DMA_A24_ENTRY_SIZE) 
                                            
typedef int universe_vmesimg_num_t;

/* -----------------------------------------------------------------------

                           Interrupt module

 * ----------------------------------------------------------------------- */

/* Bit layout in universe_lint_en and universe_lint_stat */
#define UNIVERSE_LINT_ACFAIL       (0x8000)
#define UNIVERSE_LINT_SYSFAIL      (0x4000)
#define UNIVERSE_LINT_SW_INT       (0x2000)
#define UNIVERSE_LINT_SW_IACK      (0x1000)
/*	reserved		   (0x0800) */
#define UNIVERSE_LINT_VERR         (0x0400)
#define UNIVERSE_LINT_LERR         (0x0200)
#define UNIVERSE_LINT_DMA          (0x0100)
#define UNIVERSE_LINT_VIRQ7        (0x0080)
#define UNIVERSE_LINT_VIRQ6        (0x0040)
#define UNIVERSE_LINT_VIRQ5        (0x0020)
#define UNIVERSE_LINT_VIRQ4        (0x0010)
#define UNIVERSE_LINT_VIRQ3        (0x0008)
#define UNIVERSE_LINT_VIRQ2        (0x0004)
#define UNIVERSE_LINT_VIRQ1        (0x0002)
#define UNIVERSE_LINT_VOWN         (0x0001)

#define	UNIVERSE_LINT_VIRQ(n)	   (1<<(n))

#define	UNIVERSE_LINT_OTHER	   (0xf700)
#define	UNIVERSE_LINT_ALL	   (0xF7FE)

/*
 * Access functions on PCI interrupt status register
 */

#define UNIVERSE_INTRSRC_ACFAIL(intr_status) (                           \
        (intr_status) & UNIVERSE_ACFAIL_BIT)
#define UNIVERSE_INTRSRC_SYSFAIL(intr_status) (                          \
        (intr_status) & UNIVERSE_SYSFAIL_BIT)
#define UNIVERSE_INTRSRC_VERR(intr_status) (                             \
        (intr_status) & UNIVERSE_VERR_BIT)
#define UNIVERSE_INTRSRC_LERR(intr_status) (                             \
        (intr_status) & UNIVERSE_LERR_BIT)
#define UNIVERSE_INTRSRC_DMA(intr_status) (                              \
        (intr_status) & UNIVERSE_DMA_BIT)

/* Convenience macros to get the register address for VME IRQ status */
#define	UNIVERSE_VIRQ_STATID(level) (                                    \
        UVINT_VIRQ1_STATID + ((level) - 1) * sizeof(universe_reg_t))

#define	UNIVERSE_ACFAIL_BIT 	(1 << 15) /* ACFAIL */
#define	UNIVERSE_SYSFAIL_BIT 	(1 << 14) /* SYSFAIL */
#define	UNIVERSE_LSWINT_BIT 	(1 << 13) /* Local SW */
#define	UNIVERSE_VSWIACK_BIT	(1 << 12) /* Bit for VME SW IACK Intr 	*/
#define	UNIVERSE_VERR_BIT	(1 << 10) /* Bit for VME error intr 	*/
#define	UNIVERSE_LERR_BIT	(1 << 9)  /* Bit for PCIbus error int 	*/
#define	UNIVERSE_DMA_BIT	(1 << 8)  /* Bit for local DMA int 	*/
#define	UNIVERSE_VIRQ_BITS	(0xfe)	  /* Mask for all IRQs 	*/
#define	UNIVERSE_VOWN_BIT	(0x1)

#define	UNIVERSE_VIRQL_BIT(x)	(1 << (x+1)) /* Bit for level x 	*/


/* UNIVERSE -> PCI Interrupt Map 0 */
#define	UNIVERSE_INTRMAP_VME_TO_PCI(level, line)  ((line) << ((level) * 4))

/* VIRQ Status/Id */
typedef struct universe_virq_statid_s {
	universe_reg_t reserved: 23,
                       err:       1,
	               statid:    8;
} universe_virq_statid_t;

#define UNIVERSE_VIRQ_STATID_ERR    (0x1)  /* Error while acquiring STAT/ID */

/* ------------------------------------------------------------------------
   
                       Error Management Module

   ------------------------------------------------------------------------ */

typedef struct universe_l_cmderr_s {
	universe_reg_t cmderr:     4,   
		       m_err:      1,
		       reserved0:  3,
		       l_stat:     1,
		       reserved:  23;
} universe_l_cmderr_t; 

#define UNIVERSE_L_CMDERR_LSTAT_INVALID (0)
#define UNIVERSE_L_CMDERR_LSTAT_VALID   (1)
#define UNIVERSE_L_CMDERR_LSTAT_CLEAR   (UNIVERSE_L_CMDERR_LSTAT_VALID)

typedef struct universe_v_amerr_s {
	universe_reg_t amerr:      6,   
	               iack:       1, 
		       m_err:      1,
		       v_stat:     1,
		       reserved:  23;
} universe_v_amerr_t; 

#define UNIVERSE_V_AMERR_VSTAT_INVALID (0)
#define UNIVERSE_V_AMERR_VSTAT_VALID   (1)
#define UNIVERSE_V_AMERR_VSTAT_CLEAR   (UNIVERSE_V_AMERR_VSTAT_VALID)

typedef universe_reg_t universe_v_aerr_t;

/* ------------------------------------------------------------------------
   
                          Misc. control registers

   ------------------------------------------------------------------------ */

/* LMISC */
typedef struct universe_lmisc_s {
	universe_reg_t reserved0: 5,
	               cwt:       3,
	               reserved1: 24;
} universe_lmisc_t;

#define UNIVERSE_LMISC_CWT_0    (0x0)
#define UNIVERSE_LMISC_CWT_16   (0x1)
#define UNIVERSE_LMISC_CWT_32   (0x2)
#define UNIVERSE_LMISC_CWT_64   (0x3)
#define UNIVERSE_LMISC_CWT_128  (0x4)
#define UNIVERSE_LMISC_CWT_256  (0x5)
#define UNIVERSE_LMISC_CWT_512  (0x6)

/* MAST_CTL */
typedef struct universe_mast_ctl_s {
	universe_reg_t maxrtry:        4,
		       pwon:           4,
		       vrl:            2,
		       vrm:            1,
		       vrel:           1,
		       vown:           1,
		       vown_ack:       1,
		       reserved0:      2,
		       reserved1:      2,
		       pabs:           2,
		       reserved2:      4,
		       bus_no:         8;
} universe_mast_ctl_t;

/*  Maximum number of retries on PCIbus */
#define UNIVERSE_MAST_CTL_MAXRTRY_INFINITE  (0x0)  /* infinite retries */
#define UNIVERSE_MAST_CTL_MAXRTRY_64TMS     (0x1)  /* 64 retries       */
#define UNIVERSE_MAST_CTL_MAXRTRY_512TMS    (0x8)  /* 512 retries      */

/* VMEbus request level */
#define UNIVERSE_MAST_CTL_VRL_0             (0x0)
#define UNIVERSE_MAST_CTL_VRL_1             (0x1)
#define UNIVERSE_MAST_CTL_VRL_2             (0x2)
#define UNIVERSE_MAST_CTL_VRL_3             (0x3)

/* VMEbus ownership */
#define UNIVERSE_MAST_CTL_VOWN_REL          (0x0)
#define UNIVERSE_MAST_CTL_VOWN_ACQ          (0x1)

/* VMEbus ownership acknowledgement */
#define UNIVERSE_MAST_CTL_VOWN_ACK_NO       (0x0)
#define UNIVERSE_MAST_CTL_VOWN_ACK_YES      (0x1)

/* PCIbus aligned burst size */
#define UNIVERSE_MAST_CTL_PABS_32B          (0x0)  /* 32 bytes  */
#define UNIVERSE_MAST_CTL_PABS_64B          (0x1)  /* 64 bytes  */
#define UNIVERSE_MAST_CTL_PABS_128B         (0x2)  /* 128 bytes */

/* MISC_CTL */
typedef struct universe_misc_ctl_s {
	universe_reg_t vbto:        4,
		       reserved0:   1,
		       varb:        1,
		       varbto:      2,
		       sw_lrst:     1,
		       sw_srst:     1,
		       reserved1:   1,
		       bi:          1,
		       egnbi:       1,
		       rescind:     1,
		       syscon:      1,
		       v64auto:     1,
		       reserved2:  16;
} universe_misc_ctl_t;

#define UNIVERSE_MISC_CTL_VBTO_OFF            (0x0)  /* Never timed out */
#define UNIVERSE_MISC_CTL_VBTO_16US           (0x1)  /* 16 us           */
#define UNIVERSE_MISC_CTL_VBTO_32US           (0x2)  /* 32 us           */
#define UNIVERSE_MISC_CTL_VBTO_64US           (0x3)  /* 64 us           */
#define UNIVERSE_MISC_CTL_VBTO_128US          (0x4)  /* 128 us          */
#define UNIVERSE_MISC_CTL_VBTO_256US          (0x5)  /* 256 us          */
#define UNIVERSE_MISC_CTL_VBTO_512US          (0x6)  /* 512 us          */
#define UNIVERSE_MISC_CTL_VBTO_1024US         (0x7)  /* 1024 us         */

#define UNIVERSE_MISC_CTL_VARB_RR           (0x0)  /* Round Robin     */
#define UNIVERSE_MISC_CTL_VARB_PRI          (0x1)  /* Priority        */

#define UNIVERSE_MISC_CTL_SYSCON_ON         (0x1)  
#define UNIVERSE_MISC_CTL_SYSCON_OFF        (0x0) 

#define UNIVERSE_MISC_CTL_VARBTO_OFF        (0x0)  
#define UNIVERSE_MISC_CTL_VARBTO_16         (0x1)  /* 16 us           */
#define UNIVERSE_MISC_CTL_VARBTO_256        (0x2)  /* 256 us          */


/* MISC_STAT */
typedef struct universe_misc_stat_s {
	universe_reg_t endian:      1,
		       lclsize:     1,
		       reserved0:   2,
		       dy4auto:     1,
		       reserved1:   5,
		       mybbsy:      1,
		       reserved2:   1,
		       dy4done:     1,
		       txfe:        1,
		       rxfe:        1,
		       reserved3:   1,
		       dy4autoid:   8,
		       reserved4:   8;
} universe_misc_stat_t;

#define	UNIVERSE_MISC_STAT_ENDIAN_LITTLE    (0) 
#define	UNIVERSE_MISC_STAT_ENDIAN_BIG       (1) 
#define	UNIVERSE_MISC_STAT_LCLSIZE_32       (0) 
#define	UNIVERSE_MISC_STAT_LCLSIZE_64       (1) 
#define	UNIVERSE_MISC_STAT_DY4AUTO_DISABLE  (0) 
#define	UNIVERSE_MISC_STAT_DY4AUTO_ENABLE   (1) 
#define	UNIVERSE_MISC_STAT_MYBBSY_YES       (0) 
#define	UNIVERSE_MISC_STAT_MYBBSY_NO        (1) 
#define	UNIVERSE_MISC_STAT_DY4DONE_NO       (0) 
#define	UNIVERSE_MISC_STAT_DY4DONE_YES      (1) 
#define	UNIVERSE_MISC_STAT_TXFE_NOTEMPTY    (0) 
#define	UNIVERSE_MISC_STAT_TXFE_EMPTY       (1) 
#define	UNIVERSE_MISC_STAT_RXFE_NOTEMPTY    (0) 
#define	UNIVERSE_MISC_STAT_RXFE_EMPTY       (1) 

/* ------------------------------------------------------------------------
   
                     Atomic Operation Module

   ------------------------------------------------------------------------ */

/* This structure would be primarily used by the users who need to 
 * use the compare and swap support provided by Universe. 
 */
typedef struct univ_splcyc_reg_s {
	universe_reg_t	scyc_ctl;	/* Control values of spl cycle 	     */
	universe_reg_t	scyc_laddr;	/* Local bus address for spl cycle   */
	universe_reg_t	scyc_enabl;	/* Enable bits for spl cycle 	     */
	universe_reg_t	scyc_cmpdata;	/* Data to compare  		     */
	universe_reg_t	scyc_swapdata;	/* Data to be written back 	     */
} un_splcyc_t;


/* Bit definitions for UVSPL_SCYC_CTL */
#define	SCYC_CTL_DISABLE	0
#define	SCYC_CTL_RMW		0x1	/* Do RMW operation */
#define	SCYC_CTL_ADOH		0x2	/* Generate Address only cycle */

/* ------------------------------------------------------------------------
   
                    Universe Chip -- Register Map

   ------------------------------------------------------------------------ */


typedef struct universe_chip_s {
	universe_reg_t              pci_id;
	universe_pci_csr_t          pci_csr;
	universe_pci_class_t        pci_class;
	universe_pci_misc0_t        pci_misc0;
	universe_reg_t              reserved_0[60];
	universe_pcisimg_t          pcisimg_0;
	universe_reg_t              reserved_1;
	universe_pcisimg_t          pcisimg_1;
	universe_reg_t              reserved2;
	universe_pcisimg_t          pcisimg_2;
	universe_reg_t              reserved_3;
	universe_pcisimg_t          pcisimg_3;
	universe_reg_t              reserved_4[15];
	universe_pcisimg_spec_t     pcisimg_spec;      /* 0x188 */
	universe_l_cmderr_t         l_cmderr;          /* 0x18c */
	universe_reg_t              l_aerr;            /* 0x190 */
	universe_reg_t              reserved_5[91];
	universe_reg_t              lint_en;
	universe_reg_t              lint_stat;
	universe_reg_t              lint_map0;
	universe_reg_t              lint_map1;
	universe_reg_t              reserved_6[60];
	universe_mast_ctl_t         mast_ctl;
	universe_misc_ctl_t         misc_ctl;          
	universe_misc_stat_t        misc_stat;         /* 0x408 */
	universe_reg_t              reserved_7[735];   
	universe_v_amerr_t          v_amerr;           /* 0xf88 */
	universe_reg_t              v_aerr;
	universe_reg_t              reserved_8[25];
	universe_vcsr_clr_t         vcsr_clr;
	universe_reg_t              vcsr_set;
	universe_reg_t              vcsr_bs;
} universe_chip_t;

/* ------------------------------------------------------------------------
  
                     Universe -- software view 
   
   ------------------------------------------------------------------------ */



/* 
 * Policy on VME Bridge resources
 * -- The last of the regular PCI images is for unfixed mappings
 */

#define UNIVERSE_PCISIMG_UNFIXED_7       (7)
#define UNIVERSE_NUM_OF_PCISIMGS_FIXED   (7)

typedef struct universe_state_s * universe_state_t;

/* 
 * Piomap for the Universe adapter(an instance of VMEbus provider)
 * Inherited members:
 *   -- members of VME piomap superclass
 * Universe specific members:
 *   -- PCI slave image number
 *   -- information of PCIbus which acts as the bridg between system 
 *      interconnect and VMEbus
 */

struct universe_piomap_s {
	struct vmeio_piomap_s  vme_piomap; 
	pciio_piomap_t         pci_piomap; 
	iopaddr_t              pci_addr; 
#define                        uv_dev                vme_piomap.dev 
#define                        uv_ctlr               vme_piomap.ctlr 
#define                        uv_am                 vme_piomap.am
#define                        uv_vmeaddr            vme_piomap.vmeaddr 
#define                        uv_mapsz              vme_piomap.mapsz 
#define                        uv_kvaddr             vme_piomap.kvaddr 
#define                        uv_flags              vme_piomap.flags 
	universe_pcisimg_num_t image_num; 
	universe_state_t       universe_state;  
};

typedef struct universe_piomap_s * universe_piomap_t;

/* A list to record the valid PIO maps */
typedef struct universe_piomap_list_item_s {
	struct universe_piomap_s *            piomap;
	struct universe_piomap_list_item_s *  next;
} * universe_piomap_list_item_t;

typedef struct universe_piomap_list_s {
	sema_t                        lock[1];
	universe_piomap_list_item_t   head;
} * universe_piomap_list_t;

#define	universe_piomaps_lock(piomaps)	        psema(piomaps->lock, PZERO)
#define	universe_piomaps_unlock(piomaps)	vsema(piomaps->lock)

/* Universe specific DMA map */
struct universe_dmamap_s {
	/* Inherited VMEbus DMA map */
	struct vmeio_dmamap_s  vme_dmamap; 
#define                        ud_dev               vme_dmamap.dev
#define                        ud_am                vme_dmamap.am
#define                        ud_vmeaddr           vme_dmamap.vmeaddr
#define                        ud_mapsz             vme_dmamap.mapsz
#define                        ud_flags             vme_dmamap.flags
	pciio_dmamap_t         pci_dmamap; 
	iopaddr_t              pciaddr;
	universe_vmesimg_num_t image_num;  
	universe_state_t       universe_state; 
};

typedef struct universe_dmamap_s * universe_dmamap_t;

/* Universe specific interrupt map */
struct universe_intr_s {
	struct vmeio_intr_s vme_intr;
#define                     ui_dev                  vme_intr.dev
#define                     ui_level                vme_intr.level
#define                     ui_vec                  vme_intr.vec
#define                     ui_func                 vme_intr.func
#define                     ui_arg                  vme_intr.arg
	thd_int_t	    ui_tinfo;	
	vertex_hdl_t        ui_cpu;
	unsigned	    ui_flags;	
	universe_state_t    ui_universe_state;  
};

typedef struct universe_intr_s * universe_intr_t;

/* ------------------------------------------------------------------------
  
                        universe_state_t 

   ------------------------------------------------------------------------ */

typedef __uint32_t universe_image_users_t;

/* PCIbus slave image */
typedef struct universe_pcisimg_state_s {
	universe_image_users_t  users;        /* Use count                */
	pciio_piomap_t          pci_piomap;   /* Preallocated PCIbus PIO 
						 resources                */
	vmeio_am_t              am;        
	caddr_t	                kv_base;      /* Starting Virtual address */ 
	universe_reg_t          pci_base;     /* PCIbus Base address      */
	universe_reg_t          pci_bound;    /* Highest PCIbus address   */
	universe_reg_t          to;           /* Translation offset       */
} universe_pcisimg_state_t;

typedef struct universe_pcisimg_spec_state_s {
	universe_image_users_t  users;        /* Use count                */
	pciio_piomap_t          pci_piomap;   /* Preallocated PCIbus 
					         PIO resources            */
	universe_reg_t          pci_base;     /* PCI Base address         */
	universe_reg_t          regval;       /* PCI SLSI Register Value  */
	caddr_t                 kv_base;      /* CPU addressible 
					         base address             */
} universe_pcisimg_spec_state_t;

/* VMEbus slave image */
typedef struct universe_vmesimg_state_s {
	universe_image_users_t  users;      /* Use count                  */
	universe_reg_t          to;         /* Cached TO                  */
} universe_vmesimg_state_t;

typedef enum universe_dma_engine_state_e {	
	UDE_FREE,
	UDE_BUSY
} universe_dma_engine_state_t;

typedef struct universe_dma_engine_pci_dmamap_item_s {
	pciio_dmamap_t               		       pci_dmamap;
	struct universe_dma_engine_pci_dmamap_item_s * next;
	struct universe_dma_engine_pci_dmamap_item_s * last;
} * universe_dma_engine_buffer_item_t;

/* DMA engine */
typedef struct universe_dma_engine_s {
	lock_t                  lock;
	vertex_hdl_t            vertex;
        caddr_t                 ll_kvaddr; /* Starting addr of linked list */
	universe_dma_engine_state_t state;
	universe_dma_engine_buffer_item_t pci_dmamaps;
} universe_dma_engine_t;

typedef struct universe_intr_info_s {
	universe_state_t	state;
	universe_reg_t		ibits;
} * universe_intr_info_t;

typedef struct vmebus_intr_info_s {
	vertex_hdl_t            cpu; 
	unsigned                users;
} vmebus_intr_info_t;

/* Software state of the Universe bridge */
typedef struct universe_state_s {
        vertex_hdl_t                    vertex; 
        vertex_hdl_t                    pci_conn; 
        lock_t                          lock; 
        universe_chip_t *               chip;
	unsigned int                    rev;          /* Chip revision       */
#if UNIVERSE_REVNUM_WAR
	unsigned int                    board_rev;    /* Borad revision      */
#endif /* UNIVERSE_REVNUM_WAR */
	int			        ctlr;         /* Index from ioconfig */

	/* PIO */
        universe_pcisimg_state_t        pcisimgs_fixed[UNIVERSE_NUM_OF_PCISIMGS_FIXED];	                                           
	universe_pcisimg_state_t        pcisimg_unfixed; 
        universe_pcisimg_spec_state_t   pcisimg_spec_state;  

	struct universe_piomap_list_s * piomaps; 
        lock_t                          unfixlock;  

	/* DMA */
        universe_vmesimg_state_t        vmesimgs[UNIVERSE_NUM_OF_VMESIMGS];  
	struct map *                    dma_a24_map;
	struct universe_dmamap_list_s * dmamaps; 

        /* Interrupt */
	universe_intr_info_t            iinfo[8];   
	vmebus_intr_info_t              vmebus_intrs[8];
	struct universe_intr_s *        intr_table[VMEIO_INTR_TABLE_SIZE]; 

	/* User level handles */
	universe_dma_engine_t           dma_engine; 	
	vertex_hdl_t                    usrvme;  

} * universe_state_t;


/* ------------------------------------------------------------------------
  
                         Function prototypes
			 
   Note: We export those functions to avoid indirection on the platforms with
         Universe as the only VMEbus provider.    

   ------------------------------------------------------------------------ */

extern void 
universe_provider_startup(vertex_hdl_t);  /* Universe vertex */ 
extern void 
universe_provider_shutdown(vertex_hdl_t); /* Universe vertex */
extern universe_piomap_t 
universe_piomap_alloc(vertex_hdl_t,
		      device_desc_t,
		      vmeio_am_t,
		      iopaddr_t,
		      size_t,
		      size_t,
		      unsigned);
extern caddr_t 
universe_piomap_addr(universe_piomap_t,
		     iopaddr_t,
		     size_t);
extern void
universe_piomap_done(universe_piomap_t);
extern void 
universe_piomap_free(universe_piomap_t);  

size_t
universe_pio_bcopyin(universe_piomap_t,
		     iopaddr_t,
		     caddr_t,
		     size_t,
		     int,             
		     unsigned int);
size_t
universe_pio_bcopyout(universe_piomap_t,
		      iopaddr_t,
		      caddr_t,
		      size_t,
		      int,
		      unsigned int);

extern universe_dmamap_t
universe_dmamap_alloc(vertex_hdl_t, 
		      device_desc_t,
		      vmeio_am_t,
		      size_t,       
		      unsigned);
extern iopaddr_t
universe_dmamap_addr(universe_dmamap_t, 
		     paddr_t,           
		     size_t);           
extern alenlist_t
universe_dmamap_list(universe_dmamap_t,  
		     alenlist_t,         
		     unsigned);          
extern void
universe_dmamap_done(universe_dmamap_t); 
extern void
universe_dmamap_free(universe_dmamap_t);

extern universe_intr_t
universe_intr_alloc(vertex_hdl_t,
		    device_desc_t,
		    vmeio_intr_vector_t,
		    vmeio_intr_level_t,
		    vertex_hdl_t,
		    unsigned);
extern void
universe_intr_free(universe_intr_t);
extern int
universe_intr_connect(universe_intr_t,
		      intr_func_t,
		      intr_arg_t,
		      void *);
extern void 
universe_intr_disconnect(universe_intr_t);
extern void 
universe_intr_block(universe_intr_t); 
extern void
universe_intr_unblock(universe_intr_t);
extern vertex_hdl_t
universe_intr_cpu_get(universe_intr_t);
extern int
universe_error_devenable(vertex_hdl_t vme_conn, int err_code);

extern int
universe_compare_and_swap(universe_piomap_t piomap,
			  iopaddr_t iopaddr,
			  __uint32_t old,
			  __uint32_t new);
 
extern int
universe_probe(char *, iospace_t *, int *);

extern int
universe_driver_register(vertex_hdl_t, 
			 vmeio_am_t,
			 iopaddr_t,
			 vmeio_probe_func_t,
			 vmeio_probe_arg_t,
			 vmeio_probe_arg_t,
			 char *,
			 unsigned);

#endif /* __VME_UNIVERSE_PRIVATE_H__ */








