/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "sys/IPMHSIM.h: $Revision: 1.2 $"

/*
 * IPMHSIM.h -- cpu board specific defines for IPMHSIM
 *
 * The IP22 is equipped with an R4000, MC, HPC3, and INT2 chip.  HPC3 and MC
 * definitions are contained in header files that are included here.
 *
 * The IP24 cpu board is architecturally the same as the IP22 with the
 * following exceptions:
 *   1. Uses an IOC1 chip containing the following parts:
 *        INT3      - replaces INT2
 *        VTI85CX30 - replaces Z85230
 *        VTI8042   - replaces Intel 8242
 *   2. Sys_ID register identifies IP24 cpu board and ioc1 presence.
 *   3. Other register name and offset changes as noted in this file and
 *      sys/hpc3.h.
 */

#ifndef __SYS_IPMHSIM_H__
#define	__SYS_IPMHSIM_H__

#define LIO_0_MASK_ADDR	0x1fbd9000	/* Local IO register 0 mask (b) */
#define LIO_1_MASK_ADDR	0x1fbd9010	/* Local IO register 1 mask (b) */

#define LIO_0_ISR_ADDR	0x1fbd9100	/* Local IO interrupt status (b,ro) */
#define LIO_1_ISR_ADDR	0x1fbd9110	/* Local IO interrupt status (b,ro) */

/* LIO 0 status bits */
#define LIO_FIFO	0x01		/* FIFO full / GIO-0  interrupt */
#define LIO_SCSI_0	0x02		/* SCSI controller 0 interrupt */
#define LIO_SCSI_1	0x04		/* SCSI controller 1 interrupt */
#define LIO_CENTR	0x20		/* Parallel port interrupt */
#define LIO_GIO_1	0x40		/* IP22: GE/GIO-1/2nd-HPC interrupt */
#define LIO_LIO2	0x80		/* LIO2 interrupt */

/* LIO 1 status bits */
#define LIO_LIO3	0x08		/* LIO3 interrupt */
#define LIO_HPC3	0x10		/* HPC3 interrupt */
#define LIO_AC		0x20		/* ACFAIL interrupt */

/* Mapped LIO interrupts.  These interrupts may be mapped to either 0, or 1 */
#define	LIO_PASSWD	0x02		/* Pasword checking enable */
#define	LIO_KEYBD_MOUSE	0x10		/* Keyboard/Mouse interrupt */
#define	LIO_DUART	0x20		/* Duart interrupt */


/* LIO 0 mask bits */
#define LIO_FIFO_MASK		0x01	/* FIFO full/GIO-0 interrupt mask */
#define LIO_SCSI_0_MASK		0x02	/* SCSI 0 interrupt mask */
#define LIO_SCSI_1_MASK		0x04	/* SCSI 1 interrupt mask */
#define LIO_LIO2_MASK		0x80	/* LIO2 interrupt mask */

/* LIO 1 mask bits */
#define LIO_MASK_BIT0_UNUSED	0x01	/* IP22: Unused */
#define LIO_MASK_BIT2_UNUSED	0x04	/* IP22: Unused */
#define LIO_LIO3_MASK		0x08	/* LIO3 interrupt */
#define LIO_HPC3_MASK		0x10	/* HPC3 interrupt */

/* Mapped interrupt mask bits */
#define LIO_DUART_MASK		0x20	/* Duart interrupts mask*/

/* Local I/O vector numbers for setlclvector().
 *	- local 1 starts at 8
 *	- local 2 starts at 16 (VME register)
 */
#define VECTOR_SCSI		1
#define VECTOR_SCSI1		2
#define VECTOR_DUART		3
#define VECTOR_GDMA		4		/* mc dma complete */
#define VECTOR_LCL2		7

#define VECTOR_LCL3		11
#define VECTOR_HPCDMA		12

#define VECTOR_KBDMS		20
#define VECTOR_DRAIN0		22		/* IP22: fifo not full */
#define VECTOR_DRAIN1		23		/* IP22: fifo not full */

/* clear timer 2 bits (ws) */
#define	TIMER_ACK_ADDR		PHYS_TO_K1(0x1fbb0000)
#define ACK_TIMER0	0x1	/* write strobe to clear timer 0 */
#define ACK_TIMER1	0x2	/* write strobe to clear timer 1 */

/* wd33c93 chip registers accessed through HPC3 */

#define MHSIM_CPU_FREQ		240
#define RESTART_ADDR		PHYS_TO_K0(0x400)

/*
 * MHSIM I/O Registers
 */
#define MHSIM_SCSI0A_ADDR 		PHYS_TO_K1(0x1fbc0000)
#define MHSIM_SCSI0D_ADDR 		PHYS_TO_K1(0x1fbc8000)

#define MHSIM_DUART1B_DATA		PHYS_TO_K1(0x1fbd0000)
#define MHSIM_DUART_INTR_ENBL_ADDR	PHYS_TO_K1(0x1fbd0010)
#define MHSIM_DUART_INTR_STATE_ADDR	PHYS_TO_K1(0x1fbd0020)
#define MHSIM_DUART_SYNC_WR_ADDR	PHYS_TO_K1(0x1fbd0030)

#define MHSIM_DUART_RX_INTR_STATE	0x1
#define MHSIM_DUART_TX_INTR_STATE	0x2

#define MHSIM_GETTIMEOFDAY_ADDR		PHYS_TO_K1(0x1fbe0000)
#define MHSIM_MEMSIZE_ADDR		PHYS_TO_K1(0x1fbe0010)
#define MHSIM_SC_SIZE_ADDR		PHYS_TO_K1(0x1fa00056)

/*
 * Support for MHSIM's version of Sabledsk
 */
#define MHSIM_DISK_MAX_TRANSFER_SIZE	(1000 * 1024)
#define MHSIM_DISK_DATA			PHYS_TO_K1(0x1f100000)
#define MHSIM_DISK_DISKNUM		PHYS_TO_K1(0x1fbf0000)
#define MHSIM_DISK_SECTORNUM		PHYS_TO_K1(0x1fbf0010)
#define MHSIM_DISK_SECTORCOUNT		PHYS_TO_K1(0x1fbf0020)
#define MHSIM_DISK_STATUS		PHYS_TO_K1(0x1fbf0030)
#define MHSIM_DISK_BYTES_TRANSFERRED	PHYS_TO_K1(0x1fbf0040)
#define MHSIM_DISK_PROBE_UNIT		PHYS_TO_K1(0x1fbf0050)
#define MHSIM_DISK_SIZE			PHYS_TO_K1(0x1fbf0060)
#define MHSIM_DISK_OPERATION		PHYS_TO_K1(0x1fbf0070)

#define MHSIM_DISK_NOP			0x0
#define MHSIM_DISK_READ			0x1
#define MHSIM_DISK_WRITE		0x2
#define MHSIM_DISK_PROBE		0x3

/*
 * Relevant portion of mc.h
 */

#define	CAUSE_BERRINTR	CAUSE_IP7	/* bus error interrupt */
#define	CPU_ERR_STAT	0x1fa000ec
#define	GIO_ERR_STAT	0x1fa000fc
#define	CPU_ERR_ADDR	0x1fa000e4
#define	GIO_ERR_ADDR	0x1fa000f4
#define	CPUCTRL0	0x1fa00004
#define	CPUCTRL0_R4K_CHK_PAR_N	0x04000000	/* R4000 not to check parity

/* CPU error status, CPU_ERR_STAT */
#define	CPU_ERR_STAT_RD		0x00000100	/* read parity error */
#define	CPU_ERR_STAT_PAR	0x00000200	/* CPU parity error */
#define	CPU_ERR_STAT_ADDR	0x00000400	/* memory bus error. bad
						   address */
#define	CPU_ERR_STAT_SYSAD_PAR	0x00000800	/* sysad parity error */
#define	CPU_ERR_STAT_SYSCMD_PAR	0x00001000	/* syscmd parity error */
#define	CPU_ERR_STAT_BAD_DATA	0x00002000	/* bad data identifier */

#define	CPU_ERR_STAT_PAR_MASK	0x00001f00	/* parity error mask */
#define	CPU_ERR_STAT_RD_PAR	(CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)
#define	ERR_STAT_B0		0x00000001
#define	ERR_STAT_B1		0x00000002
#define	ERR_STAT_B2		0x00000004
#define	ERR_STAT_B3		0x00000008
#define	ERR_STAT_B4		0x00000010
#define	ERR_STAT_B5		0x00000020
#define	ERR_STAT_B6		0x00000040
#define	ERR_STAT_B7		0x00000080
/*
 * Mode argument to perr_save_info()
 */

#define PERC_CACHE_SYSAD 0	/* cache exception, from SysAD		*/
#define PERC_CACHE_LOCAL 1	/* cache exception, from cache		*/
				/* (may be due to late detection of 	*/
				/* a SysAD error)			*/
#define PERC_IBE_EXCEPTION 2	/* Instruction fetch bus error exception */
#define PERC_DBE_EXCEPTION 3	/* Data fetch bus error exception	*/
#define PERC_BE_INTERRUPT 4	/* MC bus error interrupt		*/


/*
 * ECC error handler defines.
 * Define an additional exception frame for the ECC handler.
 * Save 3 more registers on this frame: C0_CACHE_ERR, C0_TAGLO,
 * and C0_ECC.
 * Call this an ECCF_FRAME.
 */
#define ECCF_CACHE_ERR	0
#define ECCF_TAGLO	1
#define ECCF_ECC	2
#define ECCF_ERROREPC	3
#define ECCF_PADDR	4
#define ECCF_CES_DATA	5
#ifndef _MEM_PARITY_WAR
#define ECCF_SIZE	(6 * sizeof(k_machreg_t))
#else /* _MEM_PARITY_WAR */
#define ECCF_STACK_BASE 6
#define ECCF_BUSY	7
#define ECCF_ECCINFO	8
#define ECCF_SECOND_PHASE	9
#define	ECCF_CPU_ERR_STAT	10
#define	ECCF_CPU_ERR_ADDR	11
#define	ECCF_GIO_ERR_STAT	12
#define	ECCF_GIO_ERR_ADDR	13
#define	ECCF_SIZE	(14 * sizeof(k_machreg_t))
#endif /* _MEM_PARITY_WAR */

/* Location of the eframe and stack for the ecc handler.
 * Since the R4000 'replaces' KUSEG with an unmapped, uncached space
 * corresponding to physical memory when a cache error occurs, these are
 * the actual addresses used.
 */
#define CACHE_ERR_EFRAME	(0x1000 - EF_SIZE)
#define CACHE_ERR_ECCFRAME	(CACHE_ERR_EFRAME - ECCF_SIZE)

#ifdef _MEM_PARITY_WAR
#define ECCF_ADDR(x) (CACHE_ERR_ECCFRAME + ((x) * 4))

/* pointer to top of cache error exception stack */
#define CACHE_ERR_STACK_BASE	ECCF_ADDR(ECCF_STACK_BASE)
#define CACHE_ERR_STACK_BASE_P  (*((long *) PHYS_TO_K1(CACHE_ERR_STACK_BASE)))

#define CACHE_ERR_STACK_SIZE	(1 * NBPP)

/* pointer to cache error log structure */
#define CACHE_ERR_ECCINFO	ECCF_ADDR(ECCF_ECCINFO)
#define CACHE_ERR_ECCINFO_P	(*((long *) PHYS_TO_K1(CACHE_ERR_ECCINFO)))
#endif /* _MEM_PARITY_WAR */

#define CACHE_ERR_CES_DATA	ECCF_ADDR(ECCF_CES_DATA)
#define CACHE_ERR_CES_DATA_P	(*((long *) PHYS_TO_K1(CACHE_ERR_CES_DATA)))

/* 4 arguments on the stack. */
#define CACHE_ERR_SP		(CACHE_ERR_ECCFRAME - 4 * sizeof(int))


/* Mask to remove SW bits from pte. Note that the high-order
 * address bits are overloaded with SW bits, which limits the
 * physical addresses to 32 bits
 */
#define	TLBLO_HWBITS		0x03ffffff	/* 20 bit ppn, plus CDVG */
#define TLBLO_HWBITSHIFT	6		/* A shift value, for masking */
#define TLBLO_PFNTOKDMSHFT	6		/* tlblo pfn to direct mapped */

#ifdef LANGUAGE_C

/* chip interface structure for IP22 / HPC / WD33C93 */
typedef struct scuzzy {
	volatile unsigned char	*d_addr;	/* address register */
	volatile unsigned char	*d_data;	/* data register */
	unsigned char d_vector;	/* hardware interrupt vector */
} scuzzy_t;

/* Map up to 256Kb per transfer.  This is sufficient
 * for almost all disk and tape acesses.  Things like scanners or
 * printers often transfer far more than a Mb per cmd, so they will
 * have to re-map anyway, no matter how many we allocate.
 */
#define NSCSI_DMA_PGS 64

/* this saves a lot of hard to read and type code in various places,
	and also ensures that we get the register size correct */
#define K1_LIO_0_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_0_MASK_ADDR))
#define K1_LIO_1_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_1_MASK_ADDR))
#define K1_LIO_2_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_2_MASK_ADDR))
#define K1_LIO_0_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_0_ISR_ADDR))
#define K1_LIO_1_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_1_ISR_ADDR))
#define K1_LIO_2_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_2_3_ISR_ADDR))

#define PHYS_RAMBASE	0x00000000
#define K0_RAMBASE	PHYS_TO_K0(PHYS_RAMBASE)

/* function prototypes for IP22 functions */

extern int spl1(void);
extern int splnet(void);
extern int spl3(void);
extern int spllintr(void);
extern int splhintr(void);
extern int splretr(void);

struct eframe_s;
extern void setlclvector(int, void (*)(int,struct eframe_s *), int);
extern void setgiovector(int, int, void (*)(int, struct eframe_s *), int);
extern void setgioconfig(int, int);

extern void sethpcdmaintr(int, void (*)(int,struct eframe_s *), int);
extern void hpcdma_intr(int, struct eframe_s *);

extern int is_fullhouse(void), board_rev(void);

#endif /* LANGUAGE_C */

#ifdef _STANDALONE
#if _MIPS_SIM == _ABI64
#define SR_PROMBASE     SR_CU0|SR_CU1|SR_KX
#else
#define SR_PROMBASE     SR_CU0|SR_CU1
#endif
#endif /* _STANDALONE */


#endif /* __SYS_IPMHSIM_H__ */
