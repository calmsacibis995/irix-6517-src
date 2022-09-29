/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_UNIV_VMEREG_H
#define __SYS_UNIV_VMEREG_H

/*
 * This file defines the registers available in Newbridge's Universe
 * controller, and the various bitfields in there.
 * All universe register names have UV prefix standing form UniverseVme
 *
 * Registers in Universe can be divided into 7 groups.
 *
 * 1. PCI Specific registers, (prefix: UVPCI_)
 * 2. Local slave image specific register (prefix: UVLS_)
 * 3. Special cycle generation control registers. (cmp & swap ) (prefix: UVSPL_)
 * 4. DMA Module registers.	(prefix:  UVDMA_ )
 * 5. Interrupt module registers.(prefix:  UVINT_)
 * 6. Miscellaneous registers. ( prefix:  UVMISC_)
 *	Misc regs include the master control, misc control and a few error
 *	logging registers. 
 * 7. VME Bus slave image specific registers.( prefix:  UVVME_ )
 * 
 * Each of these set of registers are separated via a set of lines 
 * with '======================='
 *
 * Bit field Defintion Style:
 * ================
 * Whenever a register has a field size > 1 bit, there are a pair of 
 * definitions for that field. One is a mask, other is the right shift.
 * If a field is only one bit wide, then the single definition indicates
 * the result of setting the bit. Result of clearing the bit is the counter
 * case, which is "often" mentioned in the comment.
 *
 * 
 */

/* Some Common VME specific Bit field values. Same set of values are used
 * in a number of reigsters. As part of each of the field descriptions,
 * there is a list of fields to which it defines the values.
 */

/* Values for field LSIMG_CTL_VDW, DMA_CTL_VDW */
#define	VME_DATA_8BIT		0	/* VME bus width is 8 bits */
#define	VME_DATA_16BIT		1	/* VME bus width is 16 bits */
#define	VME_DATA_32BIT		2	/* VME bus width is 32 bits */
#define	VME_DATA_64BIT		3	/* VME bus width is 64 bits */

/* Value for field LSIMG_CTL_VAS & VMEIMG_CTL_VAS & DMA_CTL_VAS */
#define	VMEVAS_A16		0
#define	VMEVAS_A24		1
#define	VMEVAS_A32		2
#define	VMEVAS_A64		4
#define	VMEVAS_CSR		5
#define	VMEVAS_USR1	6
#define	VMEVAS_USR2	7

/* Values for field LSIMG_CTL_LAS & VMEIMG_CTL_LAS */
#define	CTL_LBUS_MEMMODE	0x0	/* PCI Address space is Memory */
#define	CTL_LBUS_IOMODE		0x1	/* PCI Address space is IO */
#define	CTL_LBUS_TYPE1		0x2	/* PCI address space is type 1 */


/* Alignment requirement for Different Images */
#define	KB			(1024)
#define	MB			(1024*KB)
#define	GB			(1024*MB)

#define	UN_ALIGN_4K		((iopaddr_t)4*KB-1)	/*  4KB alignment */
#define	UN_ALIGN_64K		((iopaddr_t)64*KB-1)	/* 64KB alignment */
#define	UN_ALIGN_64M		((iopaddr_t)64*MB-1)	/* 64MB Alignment */
#define	UN_ALIGN_4GB		((iopaddr_t)4*GB-1)	/*  4GB Alignment */

#define	UVPCI_REGSPACE		(4*KB)	/* PCI space for Universe regs */

/* Address of All Universe registers */

/* Universe PCI specific register definitions */
#define	UVPCI_ID		0x0		/* Configuration ID 	*/
#define	UVPCI_CSR		0x4		/* Command/status	*/
#define	UVPCI_CLASS		0x8		/* Configuration Class 	*/
#define	UVPCI_MISC0		0xc		/* Miscellaneous 0	*/
#define	UVPCI_BS		0x10		/* Config Base address	*/
#define	UVPCI_MISC1		0x3c		/* Miscellaneous 1	*/


/* Universe Local slave image registers	*/
#define	UVLS_BASE		0x100	/* Start of local slave image 	*/

/* Local slave image 4 register definitions
 * This differs from the other slave images, in that this is the one which
 * support VME 64bit mode, and has only 4GB alignment. Hence there is one
 * less register (missing UVLS_UTO_OFS) than the other images, which is the
 * reason for separate definition of the register.
 */
#define	UVLS_IMG4_CTL		0x150	/* Local slave image 4 control regr */
#define	UVLS_IMG4_BS		0x154	/* Local slave image 4 base regr */
#define	UVLS_IMG4_BD		0x158	/* Local slave image 4 bound regr */
#define	UVLS_IMG4_TO		0x15c	/* only ONE translation offs reg */

/* Special Cycle control register	*/
#define	UVSPL_SCYC_CTL		0x170	/* Special cycle control register */
#define	UVSPL_SCYC_ADDR		0x174	/* Special cycle address register */
#define	UVSPL_SCYC_EN		0x178	/* Special cycle Enable  register */
#define	UVSPL_SCYC_CMP		0x17c	/* Special cycle Compare register */
#define	UVSPL_SCYC_SWP		0x180	/* Special cycle Swap  register */
#define	UVSPL_LMISC		0x184	/* Special cycle Misc register */

#define	UVLS_SLSI		0x188	/* Special Local slave image regr */

/* DMA Engine Control Registers */
#define	UVDMA_CTL		0x200	/* DMA Control			*/
#define	UVDMA_TBC		0x204	/* Transfer byte count 		*/
#define	UVDMA_LLA		0x208	/* Local bus lower address	*/
#define	UVDMA_LUA		0x20c	/* Local bus upper address	*/
#define	UVDMA_VLA		0x210	/* VME bus lower address	*/
#define	UVDMA_VUA		0x214	/* VME Bus upper address	*/
#define	UVDMA_LCPP		0x218	/* Lower cmd pkt pointer	*/
#define	UVDMA_UCPP		0x21c	/* Upper cmd pkt pointer	*/
#define	UVDMA_GCSR		0x220	/* General cmd/status register	*/
#define	UVDMA_LLUE		0x224	/* DMA linked list update enabl	*/


/* Interrupt control block registers */
#define	UVINT_LCL_ENBL		0x300	/* Local Interrupt Enable */
#define	UVINT_LCL_STAT		0x304	/* Local Interrupt Status,
					 * write 1 to clear bits */
#define	UVINT_LCL_MAP0		0x308	/* Setup mapping betn VME<->LCL intr */
#define	UVINT_LCL_MAP1		0x30c	/* Setup mapping betn LCL<->LCL intr */

/* Next 4 registers are never used in the SGI configuration. These
 * map the Local (aka PCI) interrupts to VME interrupts. We dont plan
 * to use NEWBRIDGE universe chip in this configuration.
 */
#define	UVINT_VME_ENBL		0x310	
#define	UVINT_VME_STAT		0x314
#define	UVINT_VME_MAP0		0x318
#define	UVINT_VME_MAP1		0x31c

#define	UVINT_STATID		0x320	/* 31:24 holds the vector returned 
					 * by universe as part of iack cycle */

#define	UVINT_VIRQ1_STATID	0x324	/* VME IRQ 1 Status/IACK vector Value */
#define	UVINT_VIRQ2_STATID	0x328	/* VME IRQ 2 Status/IACK vector value */
#define	UVINT_VIRQ3_STATID	0x32c	/* VME IRQ 3 Status/IACK vector value */
#define	UVINT_VIRQ4_STATID	0x330	/* VME IRQ 4 Status/IACK vector value */
#define	UVINT_VIRQ5_STATID	0x334	/* VME IRQ 5 Status/IACK vector value */
#define	UVINT_VIRQ6_STATID	0x338	/* VME IRQ 6 Status/IACK vector value */
#define	UVINT_VIRQ7_STATID	0x33c	/* VME IRQ 7 Status/IACK vector value */


/* Miscellaneous control registers */
#define	UVMISC_MAST_CTL		0x400	/* Master control register 	*/
#define	UVMISC_MISC_CTL		0x404	/* Misc Control register	*/
#define	UVMISC_MISC_STAT	0x408	/* Misc Status register		*/
#define	UVMISC_USER_AM		0x40c	/* Defines values of User AM	*/

#define	UVMISC_L_CMDERR		0x410	/* Local cmd errlog register	*/
#define	UVMISC_L_LAERR		0x414	/* Local loaddr errlog register */
#define	UVMISC_L_UAERR		0x418	/* Local upraddr errlog register*/
#define	UVMISC_V_AMERR		0x41c	/* VME AM errlog register	*/
#define	UVMISC_V_LAERR		0x420	/* VME loaddr errlog register	*/
#define	UVMISC_V_UAERR		0x424	/* VME upraddr errlog register	*/


/* Universe VME Slave image control registers */
#define	UVVME_BASE		0xf00	/* Start of VME slave image 	*/

/* VME slave image 4 register definitions
 * This differs from the other slave images, in that this is the one which
 * support VME 64bit mode, and has only 4GB alignment. Hence there is one
 * less register (missing UVVME_UTO_OFS) than the other images, which is the
 * reason for separate definition of the register.
 */
#define	UVVME_IMG4_CTL		0xf50
#define	UVVME_IMG4_BS		0xf54
#define	UVVME_IMG4_BD		0xf58
#define	UVVME_IMG4_TO		0xf5c	/* only ONE translation offs reg */

/* Bit field description for each of the register defined above
 * follows. Individual groups are demarkated with rows of '===============
 */

/* =========================================================================
 * =========================================================================
 * =========================================================================
 * PCI Specific Registers field description.
 * Just including the PCI definition register should be sufficient.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */

/* Field Definitions for PCI Configuration Space Control Register */
#define	UVPCI_DEVMASK		(0xffff)
#define	UVPCI_DEVSHFT		(16)
#define	UVPCI_DEVID		0	/* Universe Device Id */
#define	UVPCI_VENDORID		0x10e3	/* Universe Device Vendor ID */
#define	UVPCI_VENMASK		(0xffff)

#define	UVPCI_CLRDPE		(1 << 31) /* Clear Parity error detected */
#define	UVPCI_CLRSERR		(1 << 30) /* Clear SERR asserted */
#define	UVPCI_CLRRMA		(1 << 29) /* Clear Rcvd Master Abort */
#define	UVPCI_CLRRTA		(1 << 28) /* Clear Received Target Abort */
#define	UVPCI_CLRSTA		(1 << 27) /* Clear Signalled Target Abort */
#define	UVPCI_CLRDPD		(1 << 24) /* Clear Data parity detected */
#define	UVPCI_SERRENBL		(1 << 8)  /* Enable SERR# Driver */
#define	UVPCI_PRESENBL		(1 << 6)  /* Enable Parity error response */
#define	UVPCI_BMENBL		(1 << 2)  /* Enable universe to be bus master */
#define	UVPCI_MSENBL		(1 << 1)  /* Enable Memory Space */
#define	UVPCI_IOENBL		(1 << 0)  /* Enable I/O Space */

#define	UVPCI_CLRSTAT		(UVPCI_CLRDPE|UVPCI_CLRSERR|UVPCI_CLRRMA|\
				UVPCI_CLRRTA|UVPCI_CLRSTA|UVPCI_CLRDPD)
#define	UVPCI_DEFENBL		(UVPCI_SERRENBL|UVPCI_PRESENBL|\
				UVPCI_BMENBL|UVPCI_MSENBL)

/* Field definition for PCI configuration Misc 0 Register */
#define	UVPCI_TMRMASK		(0x1f)	/* Latency Timer Mask */
#define	UVPCI_TMRSHFT		(11)	/* Shift for Laterncy timer field */
#define	UVPCI_TMRVAL		(4)	/* Value of Timer */



/* =========================================================================
 * =========================================================================
 * =========================================================================
 * Local Slave Image Registers fields description.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */

/* Universe Local Slave Image Registers. 
 *
 * Universe supports 5 local slave (aka PCI slave) images.
 * and a Special local slave image register. 
 *
 * Each of the 5 local slave images, have 5 control registers 
 * 	Local slave Control : has various control parameters
 *	Local slave base  : has the base address of the slave image.
 *	Local slave bound : has the size of the slave image.
 *	Local slave LTO	: Holds the lower 32 bits of the 52 bit 
 *				translation offset
 *	Local slave UTO : Holds the upper 20 bits of translation offset.
 * 
 * Local slave image 0 supports 4k granularity. i.e. the address programmed
 *	to LS_IMG0 base registers could be at any 4k boundary.
 * Local slave image 1-3 support only 64k granularity. i.e. the address 
 * 	programmed to the base registers in these need to be at 64k boundary.
 * Local slave image 4 is an A32/A64 capable image. 
 *	A64 mode (enabled by default) is set by setting DACEN field to 1
 * 	In A64 mode, it only allows access beyond 4GB limit at 4GB boundary.
 *	In A32 mode, it only allows 64k resolution. 
 *	
 * Special local slave image register is one of its kind!!!
 *
 * Addressing and control bits for each of these registers is similar.
 * except for the added DACEN in slave image 4, which is reserved in others.
 */

#define	UVLS_CTL_OFS		0x00	/* Local slave control reg offset */
#define	UVLS_BS_OFS		0x04	/* Local slave base reg offset	*/
#define	UVLS_BD_OFS		0x08	/* Local slave bound reg offset	*/
#define	UVLS_LTO_OFS		0x0c	/* Local slave lower translation offs */
#define	UVLS_UTO_OFS		0x10	/* Local slave upper translation offs */
#define	UVLS_IMG_SZ		0x14	/* Size of each local slave image */

/* Macro used to generate the address of the required 
 * local slave image register 
 * Usage: x = UVLS_ADDR(slave-image-no, register-type)
 */
#define	UVLS_ADDR(_i, _r)	(UVLS_BASE +  (UVLS_IMG_SZ * (_i)) + _r )


/* Common definition for the bits in Local slave control register  */
#define	LSIMG_CTL_ENABLE	(1 << 31)	/* Enable slave image	*/
#define	LSIMG_CTL_PWENBL	(1 << 30)	/* Enable posted write 	*/
#define	LSIMG_CTL_VDW_MASK	(3)		/* VME data width field mask */
#define	LSIMG_CTL_VDW_SHFT	(22)		/* VME data width field shift */
#define	LSIMG_CTL_VAS_MASK	(7)		/* VME Addrspace type mask   */
#define	LSIMG_CTL_VAS_SHFT	(16)		/* VME Addrspace type shift  */
#define	LSIMG_CTL_PGM_MASK	(3)		/* VME Pgm/data mode mask    */
#define	LSIMG_CTL_PGM_SHFT	(14)		/* VME Pgm/data mode shift   */
#define	LSIMG_CTL_SUPER_MASK	(3)		/* VME Sup/np mode mask */
#define	LSIMG_CTL_SUPER_SHFT	(12)		/* VME Sup/np mode mask */
#define	LSIMG_CTL_VCT		(1 << 8)	/* VME cycle type mask	*/
#define	LSIMG_CTL_DACEN		(1 << 7)	/* VME DAC enable	*/
#define	LSIMG_CTL_LAS_MASK	0x00000003	/* Local bus space type mask */


/* Values for IMG_CTL_VDW -> Refer to VME_DATA_* macros */
/* Values for IMG_CTL_VAS -> Refer to VMEVAS* macros */

/* Values for field LSIMG_CTL_PGM */
#define	VME_MODE_DATA		0	/* VME in Data Mode */
#define	VME_MODE_PGM		1	/* VME in Program Mode */

/* Values for field LSIMG_CTL_SUPER */
#define	VME_MODE_NONPRIV	0	/* VME in NP Mode */
#define	VME_MODE_SUPER		1	/* VME in Supervisory Mode */

/* Values for field LSIMG_CTL_VCT */
#define	VME_CYC_SINGLE		0	/* Only single cycles on VMEBUS */
#define	VME_CYC_BOTH		1	/* Both single and block cycles */

#define	LSIMG_A32ENBL		(LSIMG_CTL_ENABLE|LSIMG_CTL_PWENBL|\
				(VME_DATA_64BIT << LSIMG_CTL_VDW_SHFT)| \
				(VMEVAS_A32 << LSIMG_CTL_VAS_SHFT) | \
				(LSIMG_CTL_VCT))

#define PCISIMG_A32NENBL        LSIMG_A32ENBL
#define PCISIMG_A32SENBL        (LSIMG_A32ENBL | \
                                (VME_MODE_SUPER << LSIMG_CTL_SUPER_SHFT))


/* Different bit fields in Special Local slave register	*/
#define	SLSI_IMGEN		(1 << 31) /* Enable SLSI */
#define	SLSI_PWEN		(1 << 30) /* Enable Posted write for SLSI */
#define	SLSI_VDW_MASK		0xf 	   /* SLSI VME Data width */
#define	SLSI_VDW_SHFT		20 	   /* SLSI VME Data width */
#define	SLSI_PGM_MASK		0xf 	   /* SLSI Program mask */
#define	SLSI_PGM_SHFT		12 	   /* SLSI Program mask */
#define	SLSI_SUPER_MASK		0xf 	   /* SLSI Supervisory/non mode */
#define	SLSI_SUPER_SHFT		8 	   /* SLSI Supervisory/non mode */
#define	SLSI_BS_MASK		(0x3f) 	   /* SLSI Base Address	64MB aligned */
#define	SLSI_BS_SHFT		(2)	   /* SLSI Base Address Shift */
#define	SLSI_LAS_MASK		0x3	   /* SLSI Loacl addr space mask */

#define	SLSI_BAS_ALIGN		26	   /* Shift needed to align base */

/* Values for field SLSI_VDW 
 * Each bit corresponds to a 16MB region, and value of 0 in each case 
 * corresponds to a 16bit datawidth. 
 */
#define	SLSI_REG_16BIT		0x0	/* All Regions are 16 bit */
#define	SLSI_REG_32BIT		(0xf << SLSI_VDW_SHFT)/* 32 bit width */
#define	SLSI_REG0_32BIT		(1 << SLSI_VDW_SHFT)
#define	SLSI_REG1_32BIT		(2 << SLSI_VDW_SHFT)
#define	SLSI_REG2_32BIT		(4 << SLSI_VDW_SHFT)
#define	SLSI_REG3_32BIT		(8 << SLSI_VDW_SHFT)

/* Convenience macro to get the mask to set a A24 region to 32bit mode. */
#define	SLSI_REGVAL_32BIT(_r)	(1 << (SLSI_VDW_SHFT + _r))

/* =========================================================================
 * =========================================================================
 * =========================================================================
 * Special Cycle Control Register bit field definitions
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */

 

/* Bit definitions for UVSPL_SCYC_CTL */
#define	SCYC_CTL_DISABLE	0
#define	SCYC_CTL_RMW		0x1	/* Do RMW operation */
#define	SCYC_CTL_ADOH		0x2	/* Generate Address only cycle */

/* Bit definitions for UVSPL_LMISC */
#define	LMISC_CRT_MASK		0xf0000000	/* Coupled request timer */
#define	LMISC_CRT_SHFT		(28)		/* Coupled request timer */
#define	LMISC_CWT_MASK		0x0f000000	/* Coupled window timer */
#define	LMISC_CWT_SHFT		(24)		/* Coupled window timer */
/* Coupled Request Timer: CRT Amount of time to hold the VMEbus, waiting
 * for a PCI master to generate a Retry. 0 => Infinity, 1 => 128us ..
 */
#define	LMISC_CRT_VAL		1	/* 128 usecs 	*/

/* Coupled Window Timer: CWT - Amount of Time Universe VME master module
 * Holds VME Bus on seeing a coupled transfer. This prevents repeated
 * acquisition of VME bus during successive PIO read cycles 
 */
#define	LMISC_CWT_VAL		2	/* 32 Local Clocks */

/* Timeout in the above cases is in terms of usecs in case of CRT, and 
 * number of local clocks in case of CWT. In either of these cases, 
 * value of 0 imples "disable", which has different meaning for both.
 * For CRT: 0 => Infinite. Reset value is 1 => 128usec.
 * For CWT: 0 => releaase after first coupled transaction, reset value = 0
 */

#define	LMISC_CRT_128us		1
#define	LMISC_CRT_256us		2
#define	LMISC_CRT_512us		3
#define	LMISC_CRT_1024us	4
#define	LMISC_CRT_2048us	5
#define	LMISC_CRT_4096us	6

#define	LMISC_CWT_16CLK		1
#define	LMISC_CWT_32CLK		2
#define	LMISC_CWT_64CLK		3
#define	LMISC_CWT_128CLK	4
#define	LMISC_CWT_256CLK	5
#define	LMISC_CWT_512CLK	6

/* =========================================================================
 * =========================================================================
 * =========================================================================
 * DMA Engine registers.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */


/* Bit fields in UVDMA_CTL */
#define	DMA_CTL_L2V		(1 << 31) /* xfer Local to VME 		*/
#define	DMA_CTL_V2L		0	  /* xfer VME to local 		*/
#define	DMA_CTL_VDW_MASK	(3)	  /* VME xfer width Mask	*/
#define	DMA_CTL_VDW_SHFT	(22)	  /* VME Xfer width shift	*/
#define	DMA_CTL_VAS_MASK	(7)	  /* VME aspace type mask	*/
#define	DMA_CTL_VAS_SHFT	16	  /* Shift value for VAS	*/
#define	DMA_CTL_PGM		(3)	  /* VME Pgm/data mode Mask	*/
#define	DMA_CTL_PGM_SHFT	(14)	  /* VME Pgm/data mode shift 	*/
#define	DMA_CTL_SUPER		(3)	  /* VME Sup/NP mode	*/
#define	DMA_CTL_SUPER_SHFT	(12)
#define	DMA_CTL_VCT_BLK		(1 << 8)   /* VME Cycle type :block*/
#define	DMA_CTL_LD64EN		(1 << 7)   /* enbl 64bit local bus xaction */

/* Defines for field DMA_CTL_VDW_ -> Refer to VME_DATA_* macros */
/* Defines for field DMA_CTL_VAS_ -> Refer to VMEVAS_* macros */


/* Bit fields in UVDMA_GCSR	*/ 
/* Command portion of the DMA General Command Status Register */
#define	DMA_GCSR_GO		(1 << 31)	/* DMA go bit. 1 to enable  */
#define	DMA_GCSR_STOP		(1 << 30)	/* 1 = Stop dma */
#define	DMA_GCSR_HALT		(1 << 29)	/* Halt after current pkt   */
#define	DMA_GCSR_CHAIN		(1 << 27)	/* In DMA Cmd chaining mode */
#define	DMA_GCSR_VON_MASK	(0xf)		/* VME Aligned count mask   */
#define	DMA_GCSR_VON_SHFT	20		/* VME Aligned count shift  */
#define	DMA_GCSR_VOFF_MASK	(0xf)		/* VME DMA OFF timer mask   */
#define	DMA_GCSR_VOFF_SHFT	16		/* VME DMA OFF timer shift  */

/* Status portion of DMA GCSR	*/
#define	DMA_GCSR_ACTIVE		(1 << 15)	/* 1 => DMA Active	*/
#define	DMA_GCSR_STOPPED	(1 << 14)	/* 1->DMA stopped, Wr 1 to clr*/
#define	DMA_GCSR_HALTED		(1 << 13)	/* DMA halted. Wr 1 to clr */
#define	DMA_GCSR_DONE		(1 << 11)

#define	DMA_GCSR_LERR		(1 << 10)	/* DMA Loca bus error	*/ 
#define	DMA_GCSR_VERR		(1 << 9)	/* DMA VME  bus error	*/ 
#define	DMA_GCSR_MEERR		(1 << 8)	/* DMA master enabl error */ 
#define	DMA_GCSR_ERR		(DMA_GCSR_LERR|DMA_GCSR_VERR|DMA_GCSR_MEERR)

/* Interrupt setup portion of DMA GCSR */
#define	DMA_GCSR_INTSTOP	(1 << 6)	/* interrupt when stopped */
#define	DMA_GCSR_INTHALT	(1 << 5)	/* Interrupt when halted */
#define	DMA_GCSR_INTDONE	(1 << 3)	/* Interrupt when done	*/
#define	DMA_GCSR_INTLERR	(1 << 2)	/* Interrupt on Local error */
#define	DMA_GCSR_INTVERR	(1 << 1)	/* Interrupt on VME   error */
#define	DMA_GCSR_INTMERR	(1)		/* Intr on mast enbl error */


/* Bit fields for DMA_LLUE */
#define	DMA_LLUE_READY		(1 << 31)	/* Should be read as 1, 
						before updating linked list */

/* =========================================================================
 * =========================================================================
 * =========================================================================
 * Interrupt Module registers.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */


/* Convenience macros to get the register address for VME IRQ status */
#define	UVINT_VIRQ_STAT(_l)	(UVINT_VIRQ1_STATID + (_l) * 4)
#define	INT_VIRQ_ERROR		(0x100)	/* Error while acquiring STAT/ID */

/* Bit fields in UVINT_LCL_ENBL/UVINT_LCL_STAT  */
#define	INTLCL_VOWN_BIT		0x1
#define	INTLCL_VIRQ_BIT		0xfe		/* Mask for all IRQs 	*/
#define	INTLCL_VIRQL_BIT(x)	(1 << (x+1))	/* Bit for level x 	*/

#define	INTLCL_DMA_BIT		(1 << 8)  /* Bit for local DMA int 	*/
#define	INTLCL_LERR_BIT		(1 << 9)  /* Bit for local error int 	*/
#define	INTLCL_VERR_BIT		(1 << 10) /* Bit for VME error intr 	*/

#define	INTLCL_VSWIACK_BIT	(1 <<12)  /* Bit for VME SW IACK Intr 	*/

/* I dont know much about this bit yet. This's what the document sez..
 * A zero-to-one transition will cause the Local software interrupt to
 * be asserted. Subsequent zeroing of this will cause the interrupt to be 
 * masked, but will not clear the LCL software Interrupt status bit.
 * SO, what clears the LCL software status bit ????    - Bhanu
 */
#define	INTLCL_LSWINT_BIT 	(1 << 13) /* Bit for Local SW Intr 	*/

/* Fatal Interrupts */
#define	INTLCL_SYSFAIL_BIT 	(1 << 14) /* Bit for SYSFAIL interrupt 	*/
#define	INTLCL_ACFAIL_BIT 	(1 << 15) /* Bit for ACFAIL Interrupt	*/


/* Macro to generate the value to be written into UVINT_LCL_MAP0
 * to map a VME interrupt level to a local interrupt level 
 */
#define	INTMAP_VME2LOCAL(_vl, _ll)	((_ll) << ( (_vl) * 4))

/* Macros to map other Universe interrupts to local interrupts
 * Return value from this macro needs to be written into 
 * UVINT_LCL_MAP1 register.
 */

#define	INTMAP_LDMA_LCL(_ll)		(_ll)
#define	INTMAP_LERR_LCL(_ll)		(_ll << 4)
#define	INTMAP_LVERR_LCL(_ll)		(_ll << 8)
#define	INTMAP_VSWIACK_LCL(_ll)		(_ll << 16)
#define	INTMAP_LSWINT_LCL(_ll)		(_ll << 20)
#define	INTMAP_SYSFAIL_LCL(_ll)		(_ll << 24)
#define	INTMAP_ACFAIL_LCL(_ll)		(_ll << 28)


/* =========================================================================
 * =========================================================================
 * =========================================================================
 * Miscellaneous Control registers.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */


/* Bit fields in UVMISC_MAST_CTL register	*/

#define	MASTCTL_MRTRY_MASK	(0xf)     /* Max number of retry mask	    */
#define	MASTCTL_MRTRY_SHFT	28 	  /* bits for posted wrt xfer cnt */

#define	MASTCTL_PWON_MASK	(0xf) 	  /* bits for posted wrt xfer cnt */
#define	MASTCTL_PWON_SHFT	24 	  /* bits for posted wrt xfer cnt */

#define	MASTCTL_VRL_MASK	(3)	  /* VME Request level mask	*/
#define	MASTCTL_VRL_SHFT	22	  /* Bit shifts to set VME req level */
#define	MASTCTL_VRM_FAIR	(1 << 21) /* VME in Fair(0=demand) req mode */
#define	MASTCTL_VREL_ROR	(1 << 20) /* VME in ROR(0=RWD) rls mode */
#define	MASTCTL_VOWN		(1 << 19) /* =1 =>Acquire&hold VME bus 	*/
#define	MASTCTL_VOWN_ACK	(1 << 18) /* VME Bus owned due to VOWN == 1 */
#define	MASTCTL_LABS_64		(1 << 12) /* Local aligned burst size=64byte */

#define	MASTCTL_BUSNO_MASK	0xff	  /* Mask for Bus PCI Bus number */


#define	MASTCTL_DEFAULT		((3 << MASTCTL_VRL_SHFT)|MASTCTL_LABS_64)

/* Bit fields in UVMISC_MISC_CTL  register */
/* Universe AUto ID sequence: Universe participation in VME64 Auto ID
 * mechanism is controlled by the VME64AUTO bit. When this bit is detected
 * high, the universe uses the VME_SW_Int mechanism to generate VXIRQ2 on
 * VME, then releases VXSYSFAIL. Access to the CR/CSR is enabled when the 
 * level 2 interrupt acknowledge cycle completes.
 */
#define	MISCCTL_VMEA64_AUTO	(1 << 16) /* =1 Initiates Auto ID sequence */
#define	MISCCTL_SYSCON		(1 << 17) /* =1 => is VME Bus syscontroller */
#define	MISCCTL_RESCIND		(1 << 18) /* Rescinding DTACK Enable ??    */
#define	MISCCTL_ENGBI		(1 << 19) /* Enable global BI mode initiator */
#define	MISCCTL_BI		(1 << 20) /* =1 => In Bi bode.  */
#define	MISCCTL_SWVMERST	(1 << 22) /* SW driven VME Bus reset */
#define	MISCCTL_SWLRST		(1 << 23) /* SW driven Local bus reset */
#define	MISCCTL_VARBTO_MASK	(3 << 24) /* VME bus arbitration Timeout 
					   * 01=> 16us, 10=256us	*/
#define	MISCCTL_VARB_PRI	(1 << 26) /* VME Arbitration mode 
					   * 0 = Roundrobin, 1 Priority */

#define	MISCCTL_VBTO_SHFT	27	  /* Bit shift for Bus timeout value */
#define	MISCCTL_VBTO_MASK	(0xf) 	  /* VME Bus timeout value 
					   * 1   2   3    4    5    6    7 
					   * 16, 32, 64, 128, 356, 512, 1024 us
					   */
#define	MISCCTL_DEFAULT		((4 << MISCCTL_VBTO_SHFT)|MISCCTL_VARB_PRI|\
				MISCCTL_SYSCON)

/* Bit fields in UVMISC_MISC_STAT register */
/* All bits in this register are READONLY */
#define	MISCSTAT_RXFE		(1 << 17) /* Rx FIFO Empty */
#define	MISCSTAT_TXFE		(1 << 18) /* Tx FIFO Empty */
#define	MISCSTAT_DY4ADONE	(1 << 19) /* DY4 AUto ID done */
#define	MISCSTAT_MYBBSY		(1 << 21) /* Universe BBSY */
/* Following three are power up options */
#define	MISCSTAT_DY4AENBL	(1 << 27) /* Enable DY4 Auto ID */
#define	MISCSTAT_LCL64		(1 << 30) /* =1 if Local Bus size is 64	*/
#define	MISCSTAT_BIGENDIAN	(1 << 31) /* =1 => Local bus is Bigendian */

/* Bit fields in UVMISC_USER_AM */
#define	MISCAM_USERAM2_MASK	(0x3f << 18) /* UserAM 2 mask */
#define	MISCAM_USERAM1_MASK	(0x3f << 26) /* UserAM 1 mask */


/* Bit fields in UVMISC_L_CMDERR Register */
#define	LCMDERR_LCMD_MASK	(0xf)
#define	LCMDERR_LCMD_SHFT	24
#define	LCMDERR_LCMD_OVRRUN	(1 << 27)
#define	LCMDERR_LCMD_VALID	(1 << 23)

/* Bit fields in UVMISC_V_AMERR : the AM Code error log */
#define	LCMDERR_AMERR_AM_MASK	0x3f	/* Mask for the Address modifier code */
#define	LCMDERR_AMERR_AMSHFT	26	/* Bit shift to reach the field */

#define	LCMDERR_AMERR_IACK	(1 << 25) /* True if error on IACK 	   */
#define	LCMDERR_AMERR_MERR	(1 << 24) /* Multiple errors occuered	   */
#define	LCMDERR_AMERR_VSTAT	(1 << 23) /* Set if VME Error log is valid */
#define	LCMDERR_AMERR_ENBLOG	(1 << 23) /* Reenable VME Error Logging	*/

/* =========================================================================
 * =========================================================================
 * =========================================================================
 * VME Bus Slave Image Control registers.
 * =========================================================================
 * =========================================================================
 * =========================================================================
 */

/* Universe VME Slave Image Registers. 
 * VME Slave image registers define the VME bus Addresses that would be 
 * mapped into system memory.
 * 
 * Universe supports 5 VME slave images.
 *
 * Each of the 5 local slave images, have 5 control registers 
 * 	VME slave Control : has various control parameters
 *	VME slave base  : has the base address of the slave image.
 *	VME slave bound : has the size of the slave image.
 *	VME slave LTO	: Holds the lower 32 bits of the 52 bit 
 *				translation offset
 *	VME slave UTO : Holds the upper 20 bits of translation offset.
 * 
 * VME slave image 0 supports 4k granularity. i.e. the address programmed
 *	to VME_IMG0 base registers could be at any 4k boundary.
 * VME slave image 1-3 support only 64k granularity. i.e. the address 
 * 	programmed to the base registers in these need to be at 64k boundary.
 * VME slave image 4 is an A32/A64 capable image. 
 *	A64 mode (enabled by default) is set by setting DACEN field to 1
 * 	In A64 mode, it only allows access beyond 4GB limit at 4GB boundary.
 *	In A32 mode, it only allows 64k resolution. 
 *	
 */

#define	UVVME_CTL_OFS		0x00	/* VME slave control reg offset */
#define	UVVME_BS_OFS		0x04	/* VME slave base reg offset	*/
#define	UVVME_BD_OFS		0x08	/* VME slave bound reg offset	*/
#define	UVVME_LTO_OFS		0x0c	/* VME slave lower translation offs */
#define	UVVME_UTO_OFS		0x10	/* VME slave upper translation offs */
#define	UVVME_IMG_SZ		0x14	/* Size of each VME slave image */

/* Macro used to generate the address of the required 
 * VME slave image register 
 * Usage: x = UVVME_ADDR(slave-image-no, register-type)
 */
#define	UVVME_ADDR(_i, _r)	(UVVME_BASE +  (UVVME_IMG_SZ * (_i)) + _r )


/* Common definition for the bits in VME slave control register UVVME_CTL_OFS */
#define	VMEIMG_CTL_ENABLE	(1 << 31) /* Enable slave image	*/
#define	VMEIMG_CTL_PWENBL	(1 << 30) /* Enable posted write 	*/
#define	VMEIMG_CTL_PREN		(1 << 29) /* Enable Prefetch for Read */

#define	VMEIMG_CTL_PGM_MASK	(3)	  /* Program/Data mode Mask  */
#define	VMEIMG_CTL_PGM_SHFT	(22)	  /* Program/Data mode Shift
					   * 0      1      2       3 
					   * Rsvd, data, Program, Both
					   */
#define	VMEIMG_CTL_SUPER_MASK	(3)	  /* SP/NP mode mask */
#define	VMEIMG_CTL_SUPER_SHFT	(20)	  /* SP/NP Mode shift */

#define	VMEIMG_CTL_VAS_MASK	(7)	  /* VME address space mask */
#define	VMEIMG_CTL_VAS_SHFT	(16)	  /* VME address space shift */

#define	VMEIMG_CTL_LD64EN	(1 << 7)  /* Enable 64 bit Loca bus trans */
#define	VMEIMG_CTL_LLRMW	(1 << 6)  /* Enable Local bus lock of VME RMW */
#define	VMEIMG_CTL_LAS_MASK	(3)	  /* Mask for Local addr space to use */


/* Defines for field IMG_CTL_VDW_ -> Refer to VME_DATA_* macros */
/* Defines for field IMG_CTL_VAS_ -> Refer to VMEVAS_* macros */

/* Values for field VMEIMG_CTL_PGM */
#define	VMEPGM_MODE_DATA		(1 << VMEIMG_CTL_PGM_SHFT)
#define	VMEPGM_MODE_PGM			(2 << VMEIMG_CTL_PGM_SHFT  )
#define	VMEPGM_MODE_BOTH		(3 << VMEIMG_CTL_PGM_SHFT)

/* Values for field VMEIMG_CTL_SUPER */
#define	VMESUP_MODE_NP			(1 << VMEIMG_CTL_SUPER_SHFT)
#define	VMESUP_MODE_SUPER		(2 << VMEIMG_CTL_SUPER_SHFT)
#define	VMESUP_MODE_BOTH		(3 << VMEIMG_CTL_SUPER_SHFT)

/* Values for field VMEIMG_CTL_VAS - Refer to VMEVAS_ macros */

#endif /* __SYS_UNIV_VMEREG_H */
