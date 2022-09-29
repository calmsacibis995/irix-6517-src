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

#ident "$Revision: 1.4 $"

/* 
 * Since the VME subsystem is in use by two different groups of users,
 * the device driver writers and the user level VME users, the VME
 * support files should try to avoid as much kernel data structure
 * dependency as possible.  XXX Try to hide all the data member
 * details in this file This file is to be used only by the kernel and
 * SGI internal user libraries, namely, user level dma engine library.  
 */

#ifndef __SYS_VME_UNIVERSE_H__
#define __SYS_VME_UNIVERSE_H__

/* All registers of the Universe chip are 32 bits wide */
typedef volatile __uint32_t universe_reg_t;

/* -----------------------------------------------------------------------

  Addresses of all Universe registers 

  ------------------------------------------------------------------------ */

/* Universe PCI specific register definitions */
#define	UNIVERSE_PCI_ID		(0x0)	  /* Configuration ID 	            */
#define	UNIVERSE_PCI_CSR	(0x4)	  /* Command/status	            */
#define	UNIVERSE_PCI_CLASS	(0x8)	  /* Configuration Class 	    */
#define	UNIVERSE_PCI_MISC0	(0xc)	  /* Miscellaneous 0	            */
#define	UNIVERSE_PCI_BS		(0x10)	  /* Config Base address	    */
#define	UNIVERSE_PCI_MISC1	(0x3c)	  /* Miscellaneous 1	            */

/* Universe PCI slave image registers */
#define	UVLS_BASE		(0x100)	  /* Start of local slave image     */

/* Special Cycle control register */
#define	UVSPL_SCYC_CTL		(0x170)   /* Special cycle control register */
#define	UVSPL_SCYC_ADDR		(0x174)	  /* Special cycle address register */
#define	UVSPL_SCYC_EN		(0x178)	  /* Special cycle Enable register  */
#define	UVSPL_SCYC_CMP		(0x17c)   /* Special cycle Compare register */
#define	UVSPL_SCYC_SWP		(0x180)   /* Special cycle Swap register    */
#define	UVSPL_LMISC		(0x184)   /* Special cycle Misc register    */

/* PCI slave images */
#define	UNIVERSE_PCISIMG_SPEC	(0x188)   /* Special PCI slave image        */

/* PCIbus error */
#define UNIVERSE_L_CMDERR       (0x18c)   /* PCIbus command error log */
#define UNIVERSE_L_AERR         (0x190)   /* PCIbus address error log */

/* DMA Engine */
#define	UNIVERSE_DMA_CTL	(0x200)   /* DMA Control	           */
#define	UNIVERSE_DMA_VA 	(0x210)	  /* VMEbus address	           */
#define	UNIVERSE_DMA_CPP	(0x218)	  /* Command packet pointer        */
#define	UNIVERSE_DMA_LLUE	(0x224)	  /* DMA linked list update enable */
#define	UNIVERSE_DMA_TBC	(0x204)	  /* Transfer byte count           */
#define	UNIVERSE_DMA_LA	        (0x208)	  /* PCIbus lower address	   */
#define	UNIVERSE_DMA_GCS	(0x220)	  /* General cmd/status register   */

/* Interrupt on PCIbus */
#define	UNIVERSE_LINT_EN        (0x300)	  /* Enable                        */
#define	UNIVERSE_LINT_STAT      (0x304)	  /* Status                        */
#define	UNIVERSE_LINT_MAP0	(0x308)	  /* mapping betn VME<->LCL intr   */
#define	UNIVERSE_LINT_MAP1	(0x30c)	  /* mapping betn LCL<->LCL intr   */

/* Interrupt on VMEbus -- not used on SGI platforms */
#define	UVINT_VME_ENBL		(0x310)	
#define	UVINT_VME_STAT		(0x314)
#define	UVINT_VME_MAP0		(0x318)
#define	UVINT_VME_MAP1		(0x31c)

#define	UVINT_STATID		(0x320)	/* 31:24 holds the vector returned 
					 * by universe as part of iack cycle */

#define	UVINT_VIRQ1_STATID	(0x324)	/* VME IRQ 1 Status/IACK vector Value */
#define	UVINT_VIRQ2_STATID	(0x328)	/* VME IRQ 2 Status/IACK vector value */
#define	UVINT_VIRQ3_STATID	(0x32c)	/* VME IRQ 3 Status/IACK vector value */
#define	UVINT_VIRQ4_STATID	(0x330)	/* VME IRQ 4 Status/IACK vector value */
#define	UVINT_VIRQ5_STATID	(0x334)	/* VME IRQ 5 Status/IACK vector value */
#define	UVINT_VIRQ6_STATID	(0x338)	/* VME IRQ 6 Status/IACK vector value */
#define	UVINT_VIRQ7_STATID	(0x33c)	/* VME IRQ 7 Status/IACK vector value */

/* Miscellaneous control registers */
#define	UNIVERSE_MAST_CTL	(0x400)	/* Master control register 	*/
#define	UNIVERSE_MISC_CTL	(0x404)	/* Misc Control register	*/
#define	UNIVERSE_MISC_STAT	(0x408)	/* Misc Status register		*/
#define	UNIVERSE_USER_AM	(0x40c) /* Defines values of User AM	*/

/* Universe VME Slave image control registers */
#define	UNIVERSE_VMESIMG_BASE	(0xf00)	/* Start of VME slave image 	*/


/* VMEbus error */
#define UNIVERSE_V_AMERR        (0xf88) /* VMEbus addr modifier error log */
#define UNIVERSE_V_AERR         (0xf8c) /* VMEbus address error log */

/* VMEbus CSR registers */
#define UNIVERSE_VCSR_CLR       (0xff4) /* VMEbus CSR Bit Clear         */
#define UNIVERSE_VCSR_SET       (0xff8) /* VMEbus CSR Bit Set           */

/*
 * IO graph specific definitions for VME Bus 
 */
#define	EDGE_ROOT_VMEBUS	"vme"
extern	vertex_hdl_t		root_vmebus;


#define	VME_IRQMAX		8

/* This file defines the various data structures used to manage the
 * VME Bus subsystem provided via Newbridge's Universe  PCI<-> VME
 * Controller.
 */
#define	UNIV_VALID		0xFEEDDEAD



/* ------------------------------------------------------------------------
  
                               DMA Engine 
 
   ------------------------------------------------------------------------ */

#if _KERNEL

/* Little endian version */
typedef struct universe_dma_ctl_s {
	universe_reg_t l2v:       1,
                       reserved0: 7,
                       vdw:       2,
                       reserved1: 3, 
	               vas:       3,
	               pgm:       2,
	               super:     2,
                       reserved2: 3,
                       vct:       1,
                       ld64en:    1,
	               reserved3: 7;
} universe_dma_ctl_t;

#else

/* Big endian version */
typedef struct universe_dma_ctl_s {
	universe_reg_t ld64en:    1,
	               reserved3: 7,
                       pgm:       2,
	               super:     2,
                       reserved2: 3,
                       vct:       1,
                       vdw:       2,
                       reserved1: 3, 
	               vas:       3,
		       l2v:       1,
                       reserved0: 7;
} universe_dma_ctl_t;

#endif 

#define UNIVERSE_DMA_CTL_L2V_READ             (0x0)
#define UNIVERSE_DMA_CTL_L2V_WRITE            (0x1)
#define UNIVERSE_DMA_CTL_VDW_8                (0x0)
#define UNIVERSE_DMA_CTL_VDW_16               (0x1)
#define UNIVERSE_DMA_CTL_VDW_32               (0x2)
#define UNIVERSE_DMA_CTL_VDW_64               (0x3)
#define UNIVERSE_DMA_CTL_VAS_A16              (0x0)
#define UNIVERSE_DMA_CTL_VAS_A24              (0x1)
#define UNIVERSE_DMA_CTL_VAS_A32              (0x2)
#define UNIVERSE_DMA_CTL_VAS_USER1            (0x6)
#define UNIVERSE_DMA_CTL_VAS_USER2            (0x7)
#define UNIVERSE_DMA_CTL_PGM_DATA             (0x0)
#define UNIVERSE_DMA_CTL_PGM_PROGRAM          (0x1)
#define UNIVERSE_DMA_CTL_SUPER_N              (0x0)
#define UNIVERSE_DMA_CTL_SUPER_S              (0x1)
#define UNIVERSE_DMA_CTL_VCT_SINGLE           (0x0)
#define UNIVERSE_DMA_CTL_VCT_BLOCK            (0x1)
#define UNIVERSE_DMA_CTL_LD64EN_OFF           (0x0)
#define UNIVERSE_DMA_CTL_LD64EN_ON            (0x1)

typedef universe_reg_t universe_dma_tbc_t;
typedef universe_reg_t universe_dma_la_t;
typedef universe_reg_t universe_dma_va_t;

#if _KERNEL
typedef struct universe_dma_cpp_s {
	universe_reg_t addr_31_3:  29,
	               reserved:    1,
	               processed:   1,
	               null:        1;
} universe_dma_cpp_t;
#else 
typedef struct universe_dma_cpp_s {
	universe_reg_t reserved0:   6,
	               processed:   1,
                       null:        1,
	               reserved1:  24;

} universe_dma_cpp_t;
#endif 

#define UNIVERSE_DMA_CPP_ADDR_SHIFT       (3)
#define UNIVERSE_DMA_CPP_PROCESSED_CLEAR  (0x0)
#define UNIVERSE_DMA_CPP_PROCESSED_NO     (0x0)
#define UNIVERSE_DMA_CPP_PROCESSED_YES    (0x1)
#define UNIVERSE_DMA_CPP_NULL_CONTINUE    (0x0)
#define UNIVERSE_DMA_CPP_NULL_FINAL       (0x1)

typedef struct universe_dma_gcs_s {
	universe_reg_t go:        1,
	               stop_req:  1,
	               halt_req:  1,
	               reserved0: 1,
                       chain:     1,
	               reserved1: 3,
	               von:       4,
		       voff:      4,
	               act:       1,
	               stop:      1,
	               halt:      1,
                       reserved2: 1,
                       done:      1,
                       lerr:      1,
                       verr:      1,
                       perr:      1,
                       reserved3: 1,
	               int_stop:  1,
	               int_halt:  1,
	               reserved4: 1,
	               int_done:  1,
	               int_lerr:  1,
	               int_verr:  1,
	               int_m_err: 1;
} universe_dma_gcs_t;

#define UNIVERSE_DMA_GCS_GO_ENABLE           (0x1)
#define UNIVERSE_DMA_GCS_CHAIN_DIRECT        (0x0)
#define UNIVERSE_DMA_GCS_CHAIN_LINKEDLIST    (0x1)
#define UNIVERSE_DMA_GCS_VON_UNTILDONE       (0x0)
#define UNIVERSE_DMA_GCS_VON_256B            (0x1)
#define UNIVERSE_DMA_GCS_VON_512B            (0x2)
#define UNIVERSE_DMA_GCS_VON_1024B           (0x3)
#define UNIVERSE_DMA_GCS_VON_2048B           (0x4)
#define UNIVERSE_DMA_GCS_VON_4096B           (0x5)
#define UNIVERSE_DMA_GCS_VON_8192B           (0x6)
#define UNIVERSE_DMA_GCS_VON_16384B          (0x7)
#define UNIVERSE_DMA_GCS_ACT_NOTACTIVE       (0x0)
#define UNIVERSE_DMA_GCS_ACT_ACTIVE          (0x1)
#define UNIVERSE_DMA_GCS_STOP_NOTSTOPPED     (0x0)
#define UNIVERSE_DMA_GCS_STOP_STOPPED        (0x1)
#define UNIVERSE_DMA_GCS_HALT_NOTHALTED      (0x0)
#define UNIVERSE_DMA_GCS_HALT_HALTED         (0x1)
#define UNIVERSE_DMA_GCS_DONE_NOTCOMPLETED   (0x0)
#define UNIVERSE_DMA_GCS_DONE_COMPLETED      (0x1)
#define UNIVERSE_DMA_GCS_DONE_CLEAR          UNIVERSE_DMA_GCS_DONE_COMPLETED
#define UNIVERSE_DMA_GCS_LERR_NOERROR        (0x0)
#define UNIVERSE_DMA_GCS_LERR_ERROR          (0x1)
#define UNIVERSE_DMA_GCS_LERR_CLEAR          UNIVERSE_DMA_GCS_LERR_ERROR  
#define UNIVERSE_DMA_GCS_VERR_NOERROR        (0x0)
#define UNIVERSE_DMA_GCS_VERR_ERROR          (0x1)
#define UNIVERSE_DMA_GCS_VERR_CLEAR          UNIVERSE_DMA_GCS_VERR_ERROR
#define UNIVERSE_DMA_GCS_PERR_NOERROR        (0x0)
#define UNIVERSE_DMA_GCS_PERR_ERROR          (0x1)
#define UNIVERSE_DMA_GCS_PERR_CLEAR          UNIVERSE_DMA_GCS_PERR_ERROR 
#define UNIVERSE_DMA_GCS_INT_STOP_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_STOP_DISABLED   (0x0)
#define UNIVERSE_DMA_GCS_INT_STOP_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_HALT_DISABLED   (0x0)
#define UNIVERSE_DMA_GCS_INT_HALT_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_DONE_DISABLED   (0x0)
#define UNIVERSE_DMA_GCS_INT_DONE_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_LERR_DISABLED   (0x0)
#define UNIVERSE_DMA_GCS_INT_LERR_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_VERR_DISABLED   (0x0)
#define UNIVERSE_DMA_GCS_INT_VERR_ENABLED    (0x1)
#define UNIVERSE_DMA_GCS_INT_M_ERR_DISABLED  (0x0)
#define UNIVERSE_DMA_GCS_INT_M_ERR_ENABLED   (0x1)

typedef struct universe_dma_llue_s {
	universe_reg_t update:    1,
                       reserved: 31;
} universe_dma_llue_t;

#define UNIVERSE_DMA_LLUE_UPDATE_ON          (1)
#define UNIVERSE_DMA_LLUE_UPDATE_OFF         (0)

typedef struct universe_dma_engine_regs_s {
	universe_dma_ctl_t    ctl;
	universe_dma_tbc_t    tbc;
	universe_dma_la_t     la;
	universe_reg_t        reserved0; 
	universe_dma_va_t     va;
	universe_reg_t        reserved1;
#if _KERNEL
	universe_dma_cpp_t    cpp; 
#else
	universe_reg_t        cpp; 
#endif
	universe_reg_t        reserved2;
	universe_dma_gcs_t    gcs;
	universe_dma_llue_t   llue;
} universe_dma_engine_regs_t;

#define UNIVERSE_DMA_ENGINE_REGS_SIZE (sizeof(universe_dma_engine_regs_t))

/* 
 * Linked list command packet object
 */
typedef struct universe_dma_engine_linked_list_packet_s {
	universe_dma_ctl_t   ctl;	/* Dma Control Parameters */
	universe_dma_tbc_t   tbc;	/* DMA transfer Count     */
	universe_dma_la_t    pciaddr;	/* DMA PCI Bus address    */
	universe_reg_t       reserved0; 
	universe_dma_va_t    vmeaddr;	/* DMA VME Bus address    */
	universe_reg_t       reserved1;
	universe_dma_cpp_t   cpp;       /* DMA command packet ptr */
	universe_reg_t       reserved2; 
} * universe_dma_engine_linked_list_packet_t;

#define UNIVERSE_DMA_ENGINE_LINKED_LIST_PACKET_SIZE (                     \
        sizeof(struct universe_dma_engine_linked_list_packet_s))

/* XXX Tunable parameters */
/* XXXXX Bringup setting */
#define UNIVERSE_DMA_ENGINE_LINKED_LIST_MAX_PACKETS (256) 

#define UNIVERSE_DMA_ENGINE_LINKED_LIST_SIZE (                            \
        UNIVERSE_DMA_ENGINE_LINKED_LIST_MAX_PACKETS *                     \
        UNIVERSE_DMA_ENGINE_LINKED_LIST_PACKET_SIZE)

/* Values for dma_flags */
#define	UNDMA_ACTIVE		1	/* DMA Currently active */
#define	UNDMA_CHMODE		2	/* DMA In Chain Mode */


/* ------------------------------------------------------------------------
  
                    Common knowledge across system call

   ------------------------------------------------------------------------ */

/* 
 * Convention used by the mapping call 
 */
#define UNIVERSE_DMA_ENGINE_REGS_OFFSET        (0)
#define UNIVERSE_DMA_ENGINE_LINKED_LIST_OFFSET (16384)

/*
 * One buffer -- system's view
 * Passed out by an ioctl call
 */
#define UNIVERSE_DMA_ENGINE_BUF_MAX_ITEMS 64

typedef struct universe_dma_engine_buffer_internal_item_s {
	/* void *       addr; */
	unsigned int addr;
	/* size_t       byte_count; */
	unsigned int byte_count; 
} universe_dma_engine_buffer_internal_item_t;

/* XXX a pointer to the type more aesthetic ? */
typedef struct universe_dma_engine_buffer_internal_s {
	unsigned int         num_of_items;
#if _KERNEL
#ifndef _ABI64
	unsigned int	     pad;
#endif
	pciio_dmamap_t	     pci_dmamap;
#else
	unsigned long long    pci_dmamap;
#endif
	universe_dma_engine_buffer_internal_item_t alens[UNIVERSE_DMA_ENGINE_BUF_MAX_ITEMS];
} universe_dma_engine_buffer_internal_t;

/* Command for ioctl */
#define UDE_IOCTL_BUFPREP          (0)
#define UDE_IOCTL_BUFTEAR          (1)

#endif /* __SYS_VME_UNIVERSE_H__ */





