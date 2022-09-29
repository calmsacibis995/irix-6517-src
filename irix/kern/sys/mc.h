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

#ident "sys/mc.h: $Revision: 1.36 $"

/*
 * defines for the MC memory controller used by IP20/IP22
 */

#ifndef __SYS_MC_H__
#define	__SYS_MC_H__

#define MCCHIP		1		/* We have an MC chip in the machine */

#ifdef	_MIPSEB

#define	CPUCTRL0	0x1fa00004	/* CPU control 0 (w, rw) */
#define	CPUCTRL1	0x1fa0000c	/* CPU control 1 (w, rw) */
#define	DOGC		0x1fa00014	/* watchdog timer (w, ro) */
#define	DOGR		DOGC		/* watchdog timer clear	(w, w) */
#define	SYSID		0x1fa0001c	/* system ID register (w, ro) */
#define	RPSS_DIVIDER	0x1fa0002c	/* RPSS divider register (w, rw) */
#define	EEROM		0x1fa00034	/* R4000 EEROM interface (b, rw) */
#define	CTRLD		0x1fa00044	/* refresh counter preload value (h, rw)
					 */
#define	REF_CTR		0x1fa0004c	/* refresh counter (h, ro) */
#define	GIO64_ARB	0x1fa00084	/* GIO64 arbitration parameters (h, rw)
					 */
#define	CPU_TIME	0x1fa0008c	/* arbiter CPU time period (h, rw) */
#define	LB_TIME		0x1fa0009c	/* arbiter long burst time period 
					   (h, rw) */
#define	MEMCFG0		0x1fa000c4	/* memory size configuration register 0
					   (w, rw) */
#define	MEMCFG1		0x1fa000cc	/* memory size configuration register 1
					   (w, rw) */
#define	CPU_MEMACC	0x1fa000d4	/* CPU main memory access configuration
					   parameters (w, rw) */
#define	GIO_MEMACC	0x1fa000dc	/* GIO main memory access configuration
					   parameters (w, rw) */
#define	CPU_ERR_ADDR	0x1fa000e4	/* CPU error address (w, ro) */
#define	CPU_ERR_STAT	0x1fa000ec	/* CPU error status (w, rw) */
#define	GIO_ERR_ADDR	0x1fa000f4	/* GIO error address (w, ro) */
#define	GIO_ERR_STAT	0x1fa000fc	/* GIO error status (w, rw) */
#define	SYS_SEMAPHORE	0x1fa00104	/* system semaphore (1 bit, rw) */
#define	LOCK_MEMORY	0x1fa0010c	/* lock GIO out of memory (1 bit, rw) */
#define	EISA_LOCK	0x1fa00114	/* lock EISA bus (1 bit, rw) */
#define	DMA_GIO_MASK	0x1fa00154	/* mask to translate GIO64 address
					   (w, rw) */
#define	DMA_GIO_SUB	0x1fa0015c	/* substitution bits for translating
					   GIO64 address (w, rw) */
#define	DMA_CAUSE	0x1fa00164	/* DMA interrupt cause (w, rw) */
#define	DMA_CTL		0x1fa0016c	/* DMA control, (w, rw) */
#define	DMA_TLB_HI_0	0x1fa00184	/* DMA TLB entry 0 high (w, rw) */
#define	DMA_TLB_LO_0	0x1fa0018c	/* DMA TLB entry 0 low (w, rw) */
#define	DMA_TLB_HI_1	0x1fa00194	/* DMA TLB entry 1 high (w, rw) */
#define	DMA_TLB_LO_1	0x1fa0019c	/* DMA TLB entry 1 low (w, rw) */
#define	DMA_TLB_HI_2	0x1fa001a4	/* DMA TLB entry 2 high (w, rw) */
#define	DMA_TLB_LO_2	0x1fa001ac	/* DMA TLB entry 2 low (w, rw) */
#define	DMA_TLB_HI_3	0x1fa001b4	/* DMA TLB entry 3 high (w, rw) */
#define	DMA_TLB_LO_3	0x1fa001bc	/* DMA TLB entry 3 low (w, rw) */
#define	RPSS_CTR	0x1fa01004	/* RPSS 100 nsec counter (w, ro) */
#define	DMA_MEMADR	0x1fa02004	/* DMA memory address (w, rw) */
#define	DMA_MEMADRD	0x1fa0200c	/* DMA memory address and set defaults
					   parameters (w, w) */
#define	DMA_SIZE	0x1fa02014	/* DMA line count and width (w, rw) */
#define	DMA_STRIDE	0x1fa0201c	/* DMA line zoom and stride (w, rw) */
#define	DMA_GIO_ADR	0x1fa02024	/* DMA GIO64 address, do not start DMA
					   (w, rw) */
#define	DMA_GIO_ADRS	0x1fa0202c	/* DMA GIO64 address and strat DMA 
					   (w, w) */
#define	DMA_MODE	0x1fa02034	/* DMA mode (w, rw) */
#define	DMA_COUNT	0x1fa0203c	/* DMA zoom count and byte count (w, rw)
					 */
#define	DMA_STDMA	0x1fa02044	/* start virtual DMA (w, rw) */
#define	DMA_RUN		0x1fa0204c	/* virtual DMA is running (w, ro) */
#define DMA_MEMADRDS	0x1fa02074	/* DMA memory address, set defaults
					   parameters, and start DMA (w, w)*/
#define	SEMAPHORE_0	0x1fa10004	/* semaphore 0 (1 bit, rw) */
#define	SEMAPHORE_1	0x1fa11004	/* semaphore 1 (1 bit, rw) */
#define	SEMAPHORE_2	0x1fa12004	/* semaphore 2 (1 bit, rw) */
#define	SEMAPHORE_3	0x1fa13004	/* semaphore 3 (1 bit, rw) */
#define	SEMAPHORE_4	0x1fa14004	/* semaphore 4 (1 bit, rw) */
#define	SEMAPHORE_5	0x1fa15004	/* semaphore 5 (1 bit, rw) */
#define	SEMAPHORE_6	0x1fa16004	/* semaphore 6 (1 bit, rw) */
#define	SEMAPHORE_7	0x1fa17004	/* semaphore 7 (1 bit, rw) */
#define	SEMAPHORE_8	0x1fa18004	/* semaphore 8 (1 bit, rw) */
#define	SEMAPHORE_9	0x1fa19004	/* semaphore 9 (1 bit, rw) */
#define	SEMAPHORE_10	0x1fa1a004	/* semaphore 10 (1 bit, rw) */
#define	SEMAPHORE_11	0x1fa1b004	/* semaphore 11 (1 bit, rw) */
#define	SEMAPHORE_12	0x1fa1c004	/* semaphore 12 (1 bit, rw) */
#define	SEMAPHORE_13	0x1fa1d004	/* semaphore 13 (1 bit, rw) */
#define	SEMAPHORE_14	0x1fa1e004	/* semaphore 14 (1 bit, rw) */
#define	SEMAPHORE_15	0x1fa1f004	/* semaphore 15 (1 bit, rw) */

#else	/* little endian */

#define	CPUCTRL0	0x1fa00000	/* CPU control 0 (w, rw) */
#define	CPUCTRL1	0x1fa00008	/* CPU control 1 (w, rw) */
#define	DOGC		0x1fa00010	/* watchdog timer (w, ro) */
#define	DOGR		DOGC		/* watchdog timer clear	(w, w) */
#define	SYSID		0x1fa00018	/* system ID register (w, ro) */
#define	RPSS_DIVIDER	0x1fa00028	/* RPSS divider register (w, rw) */
#define	EEROM		0x1fa00030	/* R4000 EEROM interface (b, rw) */
#define	CTRLD		0x1fa00040	/* refresh counter preload value (h, rw)
					 */
#define	REF_CTR		0x1fa00048	/* refresh counter (h, ro) */
#define	GIO64_ARB	0x1fa00080	/* GIO64 arbitration parameters (h, rw)
					 */
#define	CPU_TIME	0x1fa00088	/* arbiter CPU time period (h, rw) */
#define	LB_TIME		0x1fa00098	/* arbiter long burst time period 
					   (h, rw) */
#define	MEMCFG0		0x1fa000c0	/* memory size configuration register 0
					   (w, rw) */
#define	MEMCFG1		0x1fa000c8	/* memory size configuration register 1
					   (w, rw) */
#define	CPU_MEMACC	0x1fa000d0	/* CPU main memory access configuration
					   parameters (w, rw) */
#define	GIO_MEMACC	0x1fa000d8	/* GIO main memory access configuration
					   parameters (w, rw) */
#define	CPU_ERR_ADDR	0x1fa000e0	/* CPU error address (w, ro) */
#define	CPU_ERR_STAT	0x1fa000e8	/* CPU error status (w, rw) */
#define	GIO_ERR_ADDR	0x1fa000f0	/* GIO error address (w, ro) */
#define	GIO_ERR_STAT	0x1fa000f8	/* GIO error status (w, rw) */
#define	SYS_SEMAPHORE	0x1fa00100	/* system semaphore (1 bit, rw) */
#define	LOCK_MEMORY	0x1fa00108	/* lock GIO out of memory (1 bit, rw) */
#define	EISA_LOCK	0x1fa00110	/* lock EISA bus (1 bit, rw) */
#define	DMA_GIO_MASK	0x1fa00150	/* mask to translate GIO64 address
					   (w, rw) */
#define	DMA_GIO_SUB	0x1fa00158	/* substitution bits for translating
					   GIO64 address (w, rw) */
#define	DMA_CAUSE	0x1fa00160	/* DMA interrupt cause (w, rw) */
#define	DMA_CTL		0x1fa00168	/* DMA control, (w, rw) */
#define	DMA_TLB_HI_0	0x1fa00180	/* DMA TLB entry 0 high (w, rw) */
#define	DMA_TLB_LO_0	0x1fa00188	/* DMA TLB entry 0 low (w, rw) */
#define	DMA_TLB_HI_1	0x1fa00190	/* DMA TLB entry 1 high (w, rw) */
#define	DMA_TLB_LO_1	0x1fa00198	/* DMA TLB entry 1 low (w, rw) */
#define	DMA_TLB_HI_2	0x1fa001a0	/* DMA TLB entry 2 high (w, rw) */
#define	DMA_TLB_LO_2	0x1fa001a8	/* DMA TLB entry 2 low (w, rw) */
#define	DMA_TLB_HI_3	0x1fa001b0	/* DMA TLB entry 3 high (w, rw) */
#define	DMA_TLB_LO_3	0x1fa001b8	/* DMA TLB entry 3 low (w, rw) */
#define	RPSS_CTR	0x1fa01000	/* RPSS 100 nsec counter (w, ro) */
#define	DMA_MEMADR	0x1fa02000	/* DMA memory address (w, rw) */
#define	DMA_MEMADRD	0x1fa02008	/* DMA memory address and set defaults
					   parameters (w, w) */
#define	DMA_SIZE	0x1fa02010	/* DMA line count and width (w, rw) */
#define	DMA_STRIDE	0x1fa02018	/* DMA line zoom and stride (w, rw) */
#define	DMA_GIO_ADR	0x1fa02020	/* DMA GIO64 address, do not start DMA
					   (w, rw) */
#define	DMA_GIO_ADRS	0x1fa02028	/* DMA GIO64 address and strat DMA 
					   (w, w) */
#define	DMA_MODE	0x1fa02030	/* DMA mode (w, rw) */
#define	DMA_COUNT	0x1fa02038	/* DMA zoom count and byte count (w, rw)
					 */
#define	DMA_STDMA	0x1fa02040	/* start virtual DMA (w, rw) */
#define	DMA_RUN		0x1fa02048	/* virtual DMA is running (w, ro) */
#define DMA_MEMADRDS	0x1fa02070	/* DMA memory address, set defaults
					   parameters, and start DMA (w, w)*/
#define	SEMAPHORE_0	0x1fa10000	/* semaphore 0 (1 bit, rw) */
#define	SEMAPHORE_1	0x1fa11000	/* semaphore 1 (1 bit, rw) */
#define	SEMAPHORE_2	0x1fa12000	/* semaphore 2 (1 bit, rw) */
#define	SEMAPHORE_3	0x1fa13000	/* semaphore 3 (1 bit, rw) */
#define	SEMAPHORE_4	0x1fa14000	/* semaphore 4 (1 bit, rw) */
#define	SEMAPHORE_5	0x1fa15000	/* semaphore 5 (1 bit, rw) */
#define	SEMAPHORE_6	0x1fa16000	/* semaphore 6 (1 bit, rw) */
#define	SEMAPHORE_7	0x1fa17000	/* semaphore 7 (1 bit, rw) */
#define	SEMAPHORE_8	0x1fa18000	/* semaphore 8 (1 bit, rw) */
#define	SEMAPHORE_9	0x1fa19000	/* semaphore 9 (1 bit, rw) */
#define	SEMAPHORE_10	0x1fa1a000	/* semaphore 10 (1 bit, rw) */
#define	SEMAPHORE_11	0x1fa1b000	/* semaphore 11 (1 bit, rw) */
#define	SEMAPHORE_12	0x1fa1c000	/* semaphore 12 (1 bit, rw) */
#define	SEMAPHORE_13	0x1fa1d000	/* semaphore 13 (1 bit, rw) */
#define	SEMAPHORE_14	0x1fa1e000	/* semaphore 14 (1 bit, rw) */
#define	SEMAPHORE_15	0x1fa1f000	/* semaphore 15 (1 bit, rw) */

#endif	/* _MIPSEB */

/* CPU control register 0, CPUCTRL0 */
#define	CPUCTRL0_REFS_MASK	0x0000000f	/* mask for the REFS field */
#define	CPUCTRL0_RFE		0x00000010	/* enable memory refresh */
#define	CPUCTRL0_GPR		0x00000020	/* enable parity error reporting
						   on GOI64 transactions */
#define	CPUCTRL0_MPR		0x00000040	/* enable parity reporting on
						   main memory */
#define	CPUCTRL0_CPR		0x00000080	/* enable parity error reporting
						   on CPU bus transactions */
#define	CPUCTRL0_DOG		0x00000100	/* enable watchdog timer */
#define	CPUCTRL0_SIN		0x00000200	/* system initialization */
#define	CPUCTRL0_GRR		0x00000400	/* graphics reset */
#define	CPUCTRL0_EN_LOCK	0x00000800	/* enable EISA to lock memory
						   from the CPU */
#define	CPUCTRL0_CMD_PAR	0x00001000	/* enable parity error reporting
						   on syscmd bus */
#define	CPUCTRL0_INT_EN		0x00002000	/* enable interrupt writes from
						   MC */
#define	CPUCTRL0_SNOOP_EN	0x00004000	/* enable snoop logic for I/O
						   request, if implemented */
#define	CPUCTRL0_PROM_WR_EN	0x00008000	/* enable CPU writes to the
						   boot PROM */
#define	CPUCTRL0_WR_ST		0x00010000	/* warm reset without killing
						   memory refresh */
#define	CPUCTRL0_LITTLE		0x00040000	/* configure the MC to run in
						   little endian mode */
#define	CPUCTRL0_WRST		0x00080000	/* warm reset */
#define	CPUCTRL0_BAD_PAR	0x02000000	/* generate bad parity on data
						   written by CPU to memory */
#define	CPUCTRL0_R4K_CHK_PAR_N	0x04000000	/* R4000 not to check parity
						   on memory read data */
#define	CPUCTRL0_BACK2		0x08000000	/* enable back to back GIO64
						   write with no dead tristate
						   cycles between MC and MUX */

#define CPU_CONFIG		CPUCTRL0
#define CONFIG_GRRESET		CPUCTRL0_GRR

/* CPU control register 1, CPUCTRL1 */
#define	CPUCTRL1_ABORT_EN	0x00000010	/* enable GIO bus time outs */
#define	CPUCTRL1_HPC_FX		0x00001000	/* HPC endianess is fixed */
#define	CPUCTRL1_HPC_LITTLE	0x00002000	/* HPC is little endian */
#define	CPUCTRL1_EXP0_FX	0x00004000	/* EXP0 endianess is fixed */
#define	CPUCTRL1_EXP0_LITTLE	0x00008000	/* EXP0 is little endian */
#define	CPUCTRL1_EXP1_FX	0x00010000	/* EXP1 endianess is fixed */
#define	CPUCTRL1_EXP1_LITTLE	0x00020000	/* EXP1 is little endian */

/* system ID register, SYSID */
#define	SYSID_CHIP_REV_MASK	0x0000000f	/* MC chip revision mask */
#define	SYSID_EISA_PRESENT	0x00000010	/* EISA bus is present */

/* R4000 configuration EEROM interface, EEROM */
#define	EEROM_CS	0x00000002	/* EEROM chip select. active high */
#define	EEROM_SCK	0x00000004	/* serial EEROM clock */
#define	EEROM_SO	0x00000008	/* data to serial EEROM */
#define	EEROM_SI	0x00000010	/* data from serial EEROM */

/* arbitration parameters, GIO64_ARB */
#define	GIO64_ARB_HPC_SIZE_64	0x00000001	/* width of data transfer to
						   HPC is 64 bits */
#define	GIO64_ARB_GRX_SIZE_64	0x00000002	/* width of data transfer to
						   graphics is 64 bits */
#define	GIO64_ARB_EXP0_SIZE_64	0x00000004	/* width of data transfer to
						   GIO64 slot 0 is 64 bits */
#define	GIO64_ARB_EXP1_SIZE_64	0x00000008	/* width of data transfer to
						   GIO64 slot 1 is 64 bits */
#define	GIO64_ARB_EISA_SIZE	0x00000010	/* width of data transfer to the
						   EISA bus is 64 bits */
#define	GIO64_ARB_HPC_EXP_SIZE_64 0x00000020 	/* width of data transfer to
						   second HPC is 64 bits */
#define	GIO64_ARB_GRX_RT	0x00000040	/* graphics is a real time
						   device */
#define	GIO64_ARB_EXP0_RT	0x00000080	/* GIO64 slot 0 is a real time
						   device */
#define	GIO64_ARB_EXP1_RT	0x00000100	/* GIO64 slot 1 is a real time
						   device */
#define	GIO64_ARB_EISA_MST	0x00000200	/* EISA bus can be a GIO64 bus
						   master */
#define	GIO64_ARB_1_GIO		0x00000400	/* there is only one pipelined
						   GIO64 bus */
#define	GIO64_ARB_GRX_MST	0x00000800	/* graphics can be a GIO64 bus
						   master */
#define	GIO64_ARB_EXP0_MST	0x00001000	/* GIO64 slot 0 can be a GIO64
						   bus master */
#define	GIO64_ARB_EXP1_MST	0x00002000	/* GIO64 slot 1 can be a GIO64
						   bus master */
#define	GIO64_ARB_EXP0_PIPED	0x00004000	/* GIO64 slot 0 is a pipelined
						   device */
#define	GIO64_ARB_EXP1_PIPED	0x00008000	/* GIO64 slot 1 is a pipelined
						   device */

/* memory configuration registers, MEMCFG0, MEMCFG1 */
#define	MEMCFG_4MRAM		0x0000	/* 4 MB of RAM */
#define	MEMCFG_8MRAM		0x0100	/* 8 MB of RAM */
#define	MEMCFG_16MRAM		0x0300	/* 16 MB of RAM */
#define	MEMCFG_32MRAM		0x0700	/* 32 MB of RAM */
#define	MEMCFG_64MRAM		0x0f00	/* 64 MB of RAM */
#define	MEMCFG_128MRAM		0x1f00	/* 128 MB of RAM */
#define	MEMCFG_DRAM_MASK	0x1f00	/* DRAM mask */

#define	MEMCFG_ADDR_MASK	0x00ff	/* ADDR mask */
#define	MEMCFG_VLD		0x2000	/* bank is valid */
#define	MEMCFG_BNK		0x4000	/* 2 subbanks */

#define MEMACC_BIGALIAS		0x20000000	/* alias at 512MB (rev D) */

#define	ERR_STAT_B0		0x00000001
#define	ERR_STAT_B1		0x00000002
#define	ERR_STAT_B2		0x00000004
#define	ERR_STAT_B3		0x00000008
#define	ERR_STAT_B4		0x00000010
#define	ERR_STAT_B5		0x00000020
#define	ERR_STAT_B6		0x00000040
#define	ERR_STAT_B7		0x00000080

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

/* GIO error status, GIO_ERR_STAT */
#define	GIO_ERR_STAT_RD		0x00000100	/* read parity error */
#define	GIO_ERR_STAT_WR		0x00000200	/* write parity error */
#define	GIO_ERR_STAT_TIME	0x00000400	/* GIO bus timed out */
#define	GIO_ERR_STAT_PROM	0x00000800	/* write to PROM when PROM_EN
						   not set */
#define	GIO_ERR_STAT_ADDR	0x00001000	/* parity error on address
						   cycle */
#define	GIO_ERR_STAT_BC		0x00002000	/* parity error on byte count
						   cycle */
#define GIO_ERR_STAT_PIO_RD	0x00004000	/* read data parity on pio */
#define GIO_ERR_STAT_PIO_WR	0x00008000	/* write data parity on pio */

#define	CPU_TIME_DEFAULT	0x002b		/* ~1.3 us, GIO delay, 33MHz */
#define	LB_TIME_DEFAULT		0x034f		/* ~25.4us, GIO burst, 33MHz */

#ifdef R4000
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
#define ECCF_ADDR(x) (CACHE_ERR_ECCFRAME + ((x) * sizeof(k_machreg_t)))

/* pointer to top of cache error exception stack */
#define CACHE_ERR_STACK_BASE	ECCF_ADDR(ECCF_STACK_BASE)
#define CACHE_ERR_STACK_BASE_P  (*((__psunsigned_t *) PHYS_TO_K1(CACHE_ERR_STACK_BASE)))

#define CACHE_ERR_STACK_SIZE	(1 * NBPP)

/* pointer to cache error log structure */
#define CACHE_ERR_ECCINFO	ECCF_ADDR(ECCF_ECCINFO)
#define CACHE_ERR_ECCINFO_P	(*((__psunsigned_t *) PHYS_TO_K1(CACHE_ERR_ECCINFO)))
#endif /* _MEM_PARITY_WAR */

#define CACHE_ERR_CES_DATA	ECCF_ADDR(ECCF_CES_DATA)
#define CACHE_ERR_CES_DATA_P	(*((__psunsigned_t *) PHYS_TO_K1(CACHE_ERR_CES_DATA)))

/* 4 arguments on the stack. */
#define CACHE_ERR_SP		(CACHE_ERR_ECCFRAME - 4 * sizeof(k_machreg_t))

#elif R10000
/* scratch area and ptr to ECC frame/stack */
#define CACHE_TMP_EMASK		0x3e00
#define CACHE_TMP_EFRAME1	0x2000
#define CACHE_TMP_EFRAME2	0x2200
#endif /* R4000/R10000 */

#ifdef _MEM_PARITY_WAR
#ifdef _KERNEL
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

#endif /* _KERNEL */
#endif /* _MEM_PARITY_WAR */

/* Mask to remove SW bits from pte. Note that the high-order
 * address bits are overloaded with SW bits, which limits the
 * physical addresses to 32 bits
 */
#if R4000
#define	TLBLO_HWBITS		0x03ffffff	/* 20 bit ppn, plus CDVG */
#define TLBLO_PFNTOKDMSHFT	6		/* tlblo pfn to direct mapped */
#endif
#define TLBLO_HWBITSHIFT	6		/* A shift value for masking */

/* Local IO Segment */
#define LIO_ADDR        0x1f000000
#define LIO_GFX_SIZE    0x00400000      /* 4 Meg for graphics boards */
#define LIO_GIO_SIZE    0x00600000      /* 6 Meg for GIO Slots */

#define PHYS_RAMBASE            SEG0_BASE
#define K0_RAMBASE              PHYS_TO_K0(PHYS_RAMBASE)
#define K1_RAMBASE              PHYS_TO_K1(PHYS_RAMBASE)
#define PHYS_TO_K0_RAM(x)       PHYS_TO_K0((x)+PHYS_RAMBASE)
#define PHYS_TO_K1_RAM(x)       PHYS_TO_K1((x)+PHYS_RAMBASE)
#define SYMMON_STACK            PHYS_TO_K0_RAM(0x6000)
#define SYMMON_STACK_ADDR(x)	SYMMON_STACK
#if _MIPS_SIM == _ABI64
#define SYMMON_STACK_SIZE	0x2000
#else
#define SYMMON_STACK_SIZE	0x1000
#endif

#if IP20 || IP22
#define PROM_STACK              PHYS_TO_K1_RAM(0x800000)
#define PROM_BSS                PHYS_TO_K1_RAM(0x740000)
#endif

#if IP26 || IP28
#define PROM_STACK              PHYS_TO_K1_RAM(0x1000000)
#define PROM_BSS                PHYS_TO_K1_RAM(0x0f00000)
#endif

#define PROM_CHILD_STACK        (PROM_BSS-8)

/*
 * There are two discontiguous segments of physical memory on the IP20, 22,
 * and IP26.  The physical address space looks like this:
 * 
 *	0x20000000	0x2fffffff	physical memory segment 1 (256 MB)
 *	0x18000000	0x1fffffff	I/O space		  (128 MB)
 *	0x08000000	0x17ffffff	physical memory segment 0 (256 MB)
 *	0x00000000	0x07ffffff	EISA and I/O space        (128 MB)
 *
 * Note that even though segment 1 is 256 MB big, the board can only hold
 * up to 384 MB of total ram.
 *
 * With the rev D MC used on IP26 and IP28, segment 1 is increased:
 * 
 *	0x20000000	0x3fffffff	physical memory segment 1 (512 MB)
 *
 * Note that the max memory of the IP26 is 640MB since the 64MB SIMMs
 * must be 256MB aligned, and we need memory in segment 0 which is only
 * 128MB aligned.
 *
 * The following define the two physical memory segments
 */

#if IP20 || IP22
#define SEG0_BASE	0x08000000		/* base addr of seg 0 */
#define SEG0_SIZE	(256*1024*1024)		/* max  size of seg 0 */
#define SEG1_BASE	0x20000000		/* base addr of seg 1 */
#define SEG1_SIZE	(128*1024*1024)		/* max  size of seg 1 */
#elif IP26
#define SEG0_BASE	0x08000000		/* base addr of seg 0 */
#define SEG0_SIZE	(256*1024*1024)		/* max  size of seg 0 */
#define SEG1_BASE	0x20000000		/* base addr of seg 1 */
#define SEG1_SIZE	(512*1024*1024)		/* max  size of seg 1 */
#elif IP28
#define SEG0_BASE	0x20000000		/* base addr of seg 0 */
#define SEG0_SIZE	(768*1024*1024)		/* max  size of seg 0 */
#endif

/* Convert K0/K1 address to MC physical address.  This is usefull for
 * converting 64 bit KDM addresses to 32 bit physical addresses some
 * of the I/O hardware expects.
 */
#if _MIPS_SIM == _ABI64
#ifndef SEG1_BASE
#define KDM_TO_MCPHYS(addr)	((__uint32_t)((__psunsigned_t)(addr) & 0x7fffffff))
#else
#define KDM_TO_MCPHYS(addr)	((__uint32_t)((__psunsigned_t)(addr) & 0x3fffffff))
#endif /* !SEG1_BASE */
#define KDM_TO_IOPHYS(addr)	KDM_TO_MCPHYS(addr)
#define IOPHYS_TO_K0(addr)	PHYS_TO_K0(addr)
#define IOPHYS_TO_K1(addr)	PHYS_TO_K1(addr)

#else
#define KDM_TO_MCPHYS(addr)	((__uint32_t)((__psunsigned_t)(addr) & 0x1fffffff))
#define KDM_TO_IOPHYS(addr)	(__uint32_t)(addr)
#define IOPHYS_TO_K0(addr)	(addr)
#define IOPHYS_TO_K1(addr)	(addr)
#endif

/* Size definitions */
#define SYMMON_PDASIZE          512     /* size per CPU */

#if R4000 || R10000
#define	CAUSE_BERRINTR	CAUSE_IP7	/* bus error interrupt */
#define	SR_BERRBIT	SR_IBIT7	/* bus error interrupt enable */
#endif

#ifdef LANGUAGE_C
/*
 * Graphics DMA channel descriptor array element.
 * Used only with pre-MR hardware (MC rev A).
 */

/* no PIC (IP12) chip so no need to use PIC bit placement of GDMA_LAST and
 * PICGDMA_GTOH, thus we can use more than 12 bits to specify length, thus
 * we can handle 16K as well as 4K pages.
 */
#define GDMA_FULLPG     0x40000000      /* Full page (4k/16k) DMA */
#define GDMA_LAST     (GDMA_FULLPG>>1)  /* Marks last desc array entry */
#define PICGDMA_GTOH    (GDMA_LAST>>1)  /* DMA Mode: 1 = Gfx to Host */
 
#define GDMA_HTOG	0x0
#define GDMA_GTOH	0x2

typedef struct gdmada {
        long *bufaddr;
        long dmactl;
        long grxaddr;
#ifdef  _MIPSEB
        unsigned short stride;
        unsigned short linecount;
#else   /* _MIPSEL */
        unsigned short linecount;
        unsigned short stride;
#endif  /* _MIPSEL */
        long *nextdesc;
} gdmada_t;

#ifndef MCREGWBUG
#define writemcreg(reg,val) (*(uint *)(PHYS_TO_COMPATK1(reg)) = (val))
#else

/*
 * Write to an MC register.
 * This is needed for MC rev A parts only.
 *
 * Ex: writemcreg (DMA_MODE, 0x50);
 */
extern void writemcreg(unsigned int, unsigned int);

#endif	/* MCREGWBUG */

int is_sysad_parity_enabled(void);
void disable_sysad_parity(void);
void enable_sysad_parity(void);

#if !defined(_STANDALONE)
/*
 * The lclvec data structures for fast forward architecture machines.
 * Allows changes to be made in one place, instead of each independent
 * machine's .c file.
 *
 * NOTE: intentionally doesn't declare function prototypes, since it
 * shouldn't be exported beyond the irix ism.
 */
struct eframe_s;
typedef void lcl_intr_func_t(__psint_t, struct eframe_s *);

#if defined(__XTHREAD_H__)
typedef struct lclvec_s {
	lcl_intr_func_t	*isr;	/* interrupt service routine */
	__psint_t	arg;	/* arg to pass. */
	uint		bit;	/* corresponding bit in sr */
	thd_int_t	lcl_tinfo; /* Info used by interrupt thread */
} lclvec_t;

extern lclvec_t lcl0vec_tbl[];
extern lclvec_t lcl1vec_tbl[];
#if !IP20
extern lclvec_t lcl2vec_tbl[];
#endif /* !IP20 */

#define lcl_flags	lcl_tinfo.thd_flags
#define lcl_isync	lcl_tinfo.thd_isync
#define lcl_lat		lcl_tinfo.thd_latstats
#define lcl_thread	lcl_tinfo.thd_ithread
#endif /* __XHTREAD_H__ */

extern void setlclvector(int, lcl_intr_func_t *, __psint_t);
extern void setgiovector(int, int, lcl_intr_func_t *, __psint_t);
extern void setgioconfig(int, int);
#endif /* !_STANDALONE */

#endif /* LANGUAGE_C */

#endif /* __SYS_MC_H__ */
