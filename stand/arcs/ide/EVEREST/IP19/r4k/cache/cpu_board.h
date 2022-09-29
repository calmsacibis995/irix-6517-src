/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

/*
 * cpu_board.h -- cpu board specific defines
 */

/*
 * This first group of defines are for hardware devices that exist
 * on multiple pieces of hardware.  If the hardware is at the SAME
 * address on all the hardware, then a normal #define is used.  If
 * the SAME piece of hardware exists at different addresses, then
 * the normal #define is suffixed with the CPU board number of the
 * hardware.  For the M-series boxes (M500, M800, M1000) everything
 * is the same, so the lowest version is used.  In this case R2300.
 *
 * The current list of hardware is:
 *
 * 	M500 		R2300 board
 *	M800 		R2600 board
 *	M1000  		R2800 board
 * 	M120  		R2400 board
 *	M2000  		R3200 board
 *	RC62x0		R6300 board (CPU)
 *	M12		I2000 board
 *	RC62x0		R6350 board (MEMORY)
 *	RC62x0		R6360 board (IOA)
 *	M180		R2400 board
 *	Rx3230 (M20)	R3030 board
 *	Genesis		RB3125 board (CPU)
 *	Genesis-33Mhz	RB3133 board (CPU)
 *	Rx4230      	R4030 board
 */
 
#define BOOTPROM	0x1fc00000	/* Boot Prom address */
#define BOOTPROM_SIZE	0x00300000	/* Boot Prom address space size */

#define IDPROM_R2300	0x1ff00000	/* IDPROM on R2300 series */
#define IDPROM_R2400	0x1e000000	/* IDPROM on M120 */
#define IDPROM_R3200	0x1ff00000	/* IDPROM on M2000 series */
#define IDPROM_R6300	0x1e00f804	/* IDPROM on RC62x0 series */
#define IDPROM_R3030    0x1d000000	/* IDPROM on R3030 is in TOD_CLOCK chip*/

/* defines associated with the IDPROM - what is in the IDPROM and where */
#define IDPROM_BRDTYPE	0x1ff00003	/* cpu bd type, see below */
#define	IDPROM_REV	0x1ff00007	/* cpu bd revision */
#define	IDPROM_SN1	0x1ff0000b	/* cpu bd serial number, digit 1 */
#define	IDPROM_SN2	0x1ff0000f	/* cpu bd serial number, digit 2 */
#define	IDPROM_SN3	0x1ff00013	/* cpu bd serial number, digit 3 */
#define	IDPROM_SN4	0x1ff00017	/* cpu bd serial number, digit 4 */
#define	IDPROM_SN5	0x1ff0001b	/* cpu bd serial number, digit 5 */
#define IDPROM_CKSUM	0x1fffffff	/* cpu bd idprom checksum */

/* Only the offset into the IDPROM where things are */
#define ID_BRDTYPE_OFF	0x3		/* cpu bd type offset */
#define ID_REV_OFF	0x7		/* cpu bd revision offset */
#define ID_SN1_OFF	0xb		/* cpu bd serial no, digit 1 offset */
#define ID_SN2_OFF	0xf		/* cpu bd serial no, digit 2 offset */
#define ID_SN3_OFF	0x13		/* cpu bd serial no, digit 3 offset */
#define ID_SN4_OFF	0x17		/* cpu bd serial no, digit 4 offset */
#define ID_SN5_OFF	0x1b		/* cpu bd serial no, digit 5 offset */
/* Genesis only defines */
#define	ID_EA1_OFF	0x1f
#define	ID_EA2_OFF	0x23
#define	ID_EA3_OFF	0x27
#define	ID_EA4_OFF	0x2b
#define	ID_EA5_OFF	0x2f
#define	ID_EA6_OFF	0x33
/* offset for R3030 cpu board information (relative to IDPROM_R3030) */
#define	ID_REV_OFF_R3030	0x1f	/* cpu board revision level */
#define	ID_SN1_OFF_R3030	0x23	/* cpu board serial no., left most */
#define	ID_SN2_OFF_R3030	0x27 	/* cpu board serial no., 2nd char */
#define	ID_SN3_OFF_R3030	0x2b    /* cpu board serial no., 3rd char */
#define	ID_SN4_OFF_R3030	0x2f    /* cpu board serial no., 4th char */
#define	ID_SN5_OFF_R3030	0x33  	/* cpu board serial no., right most */

	/* R6000 CPU cache sizes in Kbytes, MEMORY sizes in Mbytes */
#define ID_ICACHE_OFF	0x33		/* i-cache size, in 2^^Kbytes,	*/
#define ID_DCACHE_OFF	0x37		/* d-cache size, in 2^^Kbytes,	*/
#define ID_SCACHE_OFF	0x3b		/* s-cache size, in 2^^Kbytes,	*/
#define ID_MEMSIZE_OFF	0x33		/* Memory size, in 2^^Mbytes	*/
#define ID_CHKSUM_OFF	0xfffff		/* cpu bd idprom checksum offset */

/* Possible values for cpu bd type in IDPROM */
#define BRDTYPE_R2300	1		/* M500 cpu board */
#define BRDTYPE_R2600	2		/* M800 cpu board */
#define BRDTYPE_R2800	3		/* M1000 cpu board */
#define BRDTYPE_R2400	4		/* M120 cpu/motherboard */
#define BRDTYPE_R3200	5		/* M2000 cpu board */
#define BRDTYPE_R6300	6		/* RC62x0 cpu board */
#define BRDTYPE_I2000	7		/* Jupiter cpu board */
#define BRDTYPE_I2000S	8		/* Sable simulating Jupiter */
#define BRDTYPE_M180	9		/* M180 cpu board */
#define BRDTYPE_R3030	10		/* Rx3230 (M20) cpu board */
#define BRDTYPE_RB3125	11		/* Genesis cpu board */
#define BRDTYPE_RB3133	BRDTYPE_RB3125	/* Genesis 33Mhz cpu board */
#define BRDTYPE_R4030	12		/* Fission */
#define BRDTYPE_R4230	BRDTYPE_R4030	/* Fission alias */

/*
 * The following BRDTYPEs are never used in machine_type, and they are never
 * used in master.d/machaddr arrays indexed by machine_type, so their values
 * are much higher to make lots of room for more machine_type BRDTYPEs.
 */
#define BRDTYPE_R6350	100		/* RC62x0 memory board */
#define BRDTYPE_R6360	101		/* RC62x0 IOA board */

/* Possible values for rev id in IDPROM */
#define REV_R2400_16	0x10		/* M120 16Mhz revision */
#define REV_R2400_12_5	0x20		/* M120 12.5Mhz revision */
#define REV_R3200_20	0x10		/* M2000 20Mhz revision */
#define REV_R3200_25	0x20		/* M2000 25Mhz revision */
#define REV_RB3125	REV_R3200_25	/* Genesis-25Mhz revision */
#define REV_RB3133	0x30		/* Genesis-33Mhz revision */

/*
 * This macro is used whenever we have set up arrays of pointers or
 * addresses for the different machines.  This merely returns the machine
 * dependent value that is wanted.
 */
#ifdef LANGUAGE_C
extern int machine_type;

#define MACHDEP(X)	X[machine_type-1]

/*
 * These macros will be used to run code based on machine type.
 * Generally these will be "if" statements.
 *
 * In general IS_R2300 will generically mean R2300, R2600, and
 * R2800 cpu boards.
 */

#define IS_R2300	((machine_type == BRDTYPE_R2300) || \
			  (machine_type == BRDTYPE_R2600) || \
			  (machine_type == BRDTYPE_R2800))
#define IS_R2400	((machine_type == BRDTYPE_R2400) || \
			  (machine_type == BRDTYPE_M180))
#define IS_R3200	(machine_type == BRDTYPE_R3200)
#define IS_R6300	(machine_type == BRDTYPE_R6300)
#define IS_R3030	(machine_type == BRDTYPE_R3030)
#define IS_I2000	((machine_type == BRDTYPE_I2000) || \
			 (machine_type == BRDTYPE_I2000S))
#define IS_12000S	(machine_type == BRDTYPE_I2000S)
#define IS_RB3125	(machine_type == BRDTYPE_RB3125)
#define IS_RB3133	(machine_type == BRDTYPE_RB3133)
#define IS_R4230	(machine_type == BRDTYPE_R4030)
#define IS_COMMON_SCSI	(IS_R2400 || IS_R3030 || IS_RB3125 || IS_R4230)
#ifdef SABLE
#define IS_SABLE (1)
#else
#define IS_SABLE (0)
#endif SABLE

#endif LANGUAGE_C

#define R6000_BASIC_SCACHE (256*1024)	/* fundamental byte size of one side */
#define R6000_2MB_MASK	0x06000000	/* bits 26:25 */

/* LED register addresses for the various machines */
#define	LED_REG_R2300	0x1e080003	/* LED register on R2300 series */
#define LED_REG_R2400	0x18080003	/* LED register on M120 */
#define	LED_REG_R3200	0x1e080003	/* LED register on M2000 series */
#define	LED_REG_R6300	0x1e02e003	/* LED register on RC62x0 series */

/* All machines low 6 bits are for the LED, and different meaning for upper */
#define	LED_MASK_R2300	0x3f		/* led bits - R2300 */
#define	LED_MASK_R2400	0xff		/* led bits - M120 has 8 led's */
#define	LED_MASK_R3200	0x3f		/* led bits - M2000 same as R2300 */

/* R2300 upper LED bit meanings */
#define	LED_LMEM_RUN	0x40		/* enable local memory - R2300 */
#define	LED_FPBD_RUN	0x80		/* enable fp board - R2300 */

/* R2400 has 8 bits for LED, instead of 6.  Want to do something different? */

/* R3200 upper LED bit meanings */
#define LED_RESET_VME	0x40		/* reset VME cards - M2000 */
#define LED_RESET_CPU	0x80		/* reset CPU - M2000 */

/*
 * The way these duart address were figured is slightly strange.
 * The DUART#_BASE #defines are the actual base address of the
 * duarts.  The DUART#_STR #defines are the #defines to use
 * for the device mapping structures use in SA and the kernel.
 * The difference is that the DUART#_STR #defines are 3 bytes
 * less than the base addresses.
 *
 * NOTE: There is NO second duart on the M2000.
 */

#define DUART0_BASE_R2300	0x1e008002	/* Duart 0 base on R2300 */
#define DUART0_BASE_R2400	0x18090003	/* Duart 0 base on M120 */
#define DUART0_BASE_R3200	0x1e008002	/* Duart 0 base on M2000 */
#define DUART0_BASE_R6300	0x1e02e203	/* Duart 0 base on RC62x0 */

#define DUART0_STR_R2300	(DUART0_BASE_R2300 - 3)
#define DUART0_STR_R2400	(DUART0_BASE_R2400 - 3)
#define DUART0_STR_R3200	(DUART0_BASE_R3200 - 3)
#define DUART0_STR_R6300	(DUART0_BASE_R6300 - 3)

#define DUART1_BASE_R2300	0x1e800000	/* Duart 1 base on R2300 */
#define DUART1_BASE_R2400	0x180a0003	/* Duart 1 on M120 */

#define DUART1_STR_R2300	(DUART1_BASE_R2300 - 3)
#define DUART1_STR_R2400	(DUART1_BASE_R2400 - 3)

#define	SCC_BASE_R3030		0x1b000000	/* scc on Pizazz */
#define SCC_BASE_RB3125		0x1e800000	/* scc on Genesis */
#define SCC_RB3125_CHB_PTR	0x1e800000
#define SCC_RB3125_CHB_DATA	0x1e800004
#define SCC_RB3125_CHA_PTR	0x1e800008
#define SCC_RB3125_CHA_DATA	0x1e80000c

/* Genesis Lance local buffer (1MB) starting address */
#define LANCE_BUFFER_RB3125	0x1f000000

/* M-series use MC14618 device, where M120 and M2000 use MK48T02 */

#define RT_CLOCK_ADDR_R2300	0x1e010003	/* MC14618 on R2300 */

#define TODC_CLOCK_ADDR_R2400	0x180b0000	/* MK48T02 on M120 */
                                                /*      BYTE access    */
#define TODC_CLOCK_ADDR_R3200	0x1e010000	/* MK48T02 on M2000 */
#define TODC_CLOCK_ADDR_R6300	0x1e02c000	/* MK48T02 on RC62x0 */
#define	TODC_CLOCK_ADDR_R3030	0x1d000000	/* MK48T02 on pizazz */

#define TODC_NVRAM_OFFSET	0x000007f0	/* For M120 and M2000 */
						/* use NVRAM in TODC  */
						/* for initial stack */

#define PT_CLOCK_ADDR_R2300	0x1e001003	/* I8254 on R2300 series */
#define PT_CLOCK_ADDR_R2400	0x180c0003	/* I8254 on M120 */
#define PT_CLOCK_ADDR_R3200	0x1e001003	/* I8254 on M2000 series */

#define TIM0_ACK_ADDR_R2300	0x1e200003	/* Timer 0 IACK on R2300 */
#define TIM0_ACK_ADDR_R2400	0x18050003	/* Timer 0 IACK on M120 */
#define TIM0_ACK_ADDR_R3200	0x1e200003	/* Timer 0 IACK on R3200 */

#define TIM1_ACK_ADDR_R2300	0x1e200007	/* Timer 1 IACK on R2300 */
#define TIM1_ACK_ADDR_R2400	0x18060003	/* Timer 1 IACK on M120 */
#define TIM1_ACK_ADDR_R3200	0x1e200007	/* Timer 1 IACK on R3200 */

/* 
 * For M-series and M2000, the SBE is the System Bus Error address
 * register.  Reading this while in the Bus Error handler gives you
 * the location of the read error.  In addition it clears the interrupt
 * for you.
 * 
 * On the M2000, the Write Error Address Register at WBE_ADDR_R3200 latches
 * the address used during a bus write error.  This location can be
 * read multiple times without change.
 * It must be written to (any value will do) to re-enable the latch
 * and clear the interrupt.
 *
 * On M120, when you enter the Bus Error handler, you first read the
 * FIR (Fault ID register), which tells you who cause the error (defines
 * below), then the FAR (Fault Address register) which gives you the
 * address of the read error, and also clears the interrupt for you.
 */

/* M-series and M2000 SBE location */
#define	SBE_ADDR		0x1e000800	/* system bus error address */

/* M2000 Write Error Addr Reg location */
#define	BWE_ADDR_R3200		0x1e000800	/* system write bus err addr */

/* M120 Fault register locations */
#define FAR			0x18030000	/* fault address register */
                                                /*   - WORD access        */

#define	FID			0x18040002	/* fault ID register	  */
					        /*   - HALFWORD access	  */

/* M120 FID register information */
#define	FID_IBUSMAST2	(1 << 15)	        /* IBus master		  */
#define	FID_IBUSMAST1	(1 << 14)
#define	FID_IBUSMAST0	(1 << 13)
#define	FID_IBUSMAST	(FID_IBUSMAST2 | FID_IBUSMAST1 | FID_IBUSMAST0)
#define	  IBUS_SHIFT	13
#define	  IBUS_PCAT4	0			/* PCAT level 4		  */
#define	  IBUS_PCAT3	1			/* PCAT level 3		  */
#define	  IBUS_PCAT2	2			/* PCAT level 2		  */
#define	  IBUS_PCAT1	3			/* PCAT level 1		  */
#define	  IBUS_CHAIN	4			/* 9516 chaining	  */
#define	  IBUS_PCATDMA	5			/* 9516 for PCAT	  */
#define	  IBUS_SCSI	6			/* 9516 for SCSI	  */
#define	  IBUS_LANCE	7			/* Lance		  */
#define	FID_IBUSVALIDB	(1 << 12)		/* caused by IBus	  */
#define   IBUSVALIDB_SHIFT	12
#define	FID_PROCBRD	(1 << 11)		/* caused by processor board */
#define	FID_IOPOEB	(1 << 10)		/* caused by IOP board	  */
#define   IOPOEB_SHIFT	10
#define FID_PROMFAULTB	(1 << 9)		/* caused by write to prom   */
                                                /*   (inverted)		  */
#define   PROMFAULTB_SHIFT	9
#define	FID_TIMEOUT	(1 << 7)		/* bus timeout		  */
#define	FID_MREAD	(1 << 6)		/* during read (write = 0)*/
#define	FID_ACCTYPE0B	(1 << 5)
#define	FID_ACCTYPE1B	(1 << 4)		/* access type (inverted) */
#define	FID_ACCTYPEB	(FID_ACCTYPE1B | FID_ACCTYPE0B)
#define   ACCTYPEB_SHIFT	4
#define   ACCTYPEB_WORD		0
#define   ACCTYPEB_HALFWORD	1
#define   ACCTYPEB_TRIBYTE	2
#define   ACCTYPEB_BYTE		3
#define	FID_PARER3B	(1 << 3)		/* parity error on bytes  */
#define   PARER3B_SHIFT		3
#define	FID_PARER2B	(1 << 2)		/*   (inverted)		  */
#define   PARER2B_SHIFT		2
#define	FID_PARER1B	(1 << 1)
#define   PARER1B_SHIFT		1
#define	FID_PARER0B	(1 << 0)
#define   PARER0B_SHIFT		0
#define	FID_PARERB	(FID_PARER0B | FID_PARER1B | FID_PARER2B | FID_PARER3B)
#define	FID_MASK	(FID_IBUSMAST | FID_PROCBRD | FID_TIMEOUT | FID_MREAD)
                                                /* mask for noninverted bits */
#define	FID_MASKB	(FID_IBUSVALIDB | FID_IOPOEB | FID_ACCTYPEB | FID_PARERB)
                                                /* mask for inverted bits */

/* M-series configuration register information */
#define	CPU_CONFIG	0x1e080006	/* cpu bd configuration register */
#define	CONFIG_NOCP1	0x01		/* coprocessor 1 not present */
#define	CONFIG_NOCP2	0x02		/* coprocessor 2 not present */
#define	CONFIG_POWERUP	0x04		/* power-up or cpu bd reset */
#define	CONFIG_VMEMEM	0x08		/* VME memory present */

/* M120 has 16 bit System Configuration register with this information */
#define	SCR		0x18000002	/* system configuration		*/
					/*   register - HALFWORD access	*/

#define	SCR_PAREN	(1 << 15)	/* enable memory parity error	*/
					/*   interrupt -RW		*/
#define	SCR_FORCEPAR	(1 << 14)	/* force parity error (for	*/
					/*   diagnostics) - RW		*/
#define SCR_SLOWUDCEN	(1 << 13)	/* slow UDC accesses		*/
#define	SCR_ATTC	(1 << 12)	/* AT terminal count - RW	*/
#define	SCR_NORSTPCATBUS	(1 << 11)	/* don't reset PCAT Bus - RW */
#define   NORSTPCATBUS_SHIFT	11
#define	SCR_EOP9516	(1 << 10)	/* end-of-process to 9516 - RW	*/
#define	SCR_SCSIHIN	(1 << 9)	/* SCSI chip HIN bit - RW	*/
#define	SCR_RSTSCSI	(1 << 8)	/* I Bus lockout request - RW	*/
#define	SCR_NOFPP	(1 << 7)	/* coprocessor 1 not present	*/
					/*   - RO			*/
#define   NOFPP_SHIFT	7
#define	SCR_NOFB0	(1 << 6)	/* frame buffer 0 not present	*/
					/*   - RO			*/
#define   NOFB0_SHIFT	6
#define	SCR_NOFB1	(1 << 5)	/* frame buffer 1 not present	*/
					/*   - RO			*/
#define   NOFB1_SHIFT	5
#define	SCR_COLDSTART	(1 << 4)	/* cold start reset - RO	*/
#define	SCR_NOBOOTLOCK	(1 << 3)	/* key switch is not in the lock position */
#define SCR_PTR1 	(1 << 2)	/* SCSI state machine byte pointer 1 */
#define SCR_PTR0 	(1 << 1)	/* SCSI state machine byte pointer 0 */
#define	SCR_KEY0	(1 << 0)	/* CPU board ID key bit 0	*/

/* Who uses these?
   DEMON for one! */
#define RESET_COPROCESSOR 0x80
#define RESET_LOCAL_MEMORY 0x40

/*
 * This next section will be M120 only information.
 *
 * NOTE: M2000 may eventually contain a LANCE chip, and should end up
 *       at the same address as the LANCE on M120.  Find out!
 *
 */

#define AMD_BASE_R2400	0x180e0000	/* Base for structure for 9516 */

#define	DMA_BASE	0x180e0002	/* 9516 DMA base		*/
					/*   - HALFWORD access		*/
#define	DMA_PTR		0x180e0006	/* 9516 DMA Pointer register	*/

#define	DMA_INTACK	0x180e000a	/* 9516 interrupt acknowledge	*/
#define DMA_BASE_RB3125	0x1e008000	/* DMA base addr for Genesis */

#define FUJI_BASE_R2400	0x180d0000	/* Base for structure for FUJI */
#define NCR_BASE_RB3125	0x1e002000	/* NCR scsi part base */

#define LANCE_BASE_R2400	0x180f0000	/* Lance base - M120 */
#define LANCE_BASE_R3200	0x180f0000	/* Lance base - M2000? */
#define	LANCE_BASE_R3030	0x1a000000	/* Pizzaz */

#define	LANCE_BASE_RB3125	0x1e004000	/* Genesis */

#define LANCE_RDP_OFFSET	0x00000002	/* Lance RDP offset */
#define LANCE_RAP_OFFSET	0x00000006	/* Lance RAP offset */

#define	ENET_RDP	0x180f0002	/* 7990 Lance register data	*/
					/*   port - HALFWORD access	*/
#define	ENET_RAP	0x180f0006	/* register address port	*/
					/*   - HALFWORD access		*/
/* 7990 Lance register data,  port - HALFWORD access	*/
#define	LANCE_RDP_RB3125 (LANCE_BASE_RB3125 + LANCE_RDP_OFFSET)
/* register address port  - HALFWORD access		*/
#define	LANCE_RAP_RB3125 (LANCE_BASE_RB3125 + LANCE_RAP_OFFSET)

/*
 * On M-series and M2000, we only had the VME IMR and ISR.  These are
 * analogous to those except that there are many more sources and masks
 * available.
 */

#define IMR		0x18020002	/* interrupt mask register,	*/
					/*   1 = enable - HALFWORD	*/
					/*   access			*/
#define ISR		0x18010002	/* interrupt status register,	*/
					/*   1 = pending - HALFWORD	*/
					/*   access			*/
#define	INT_MBUS	(1 << 15)	/* MBus (frame buffer)		*/
					/*   interrupt			*/
#define	INT_ENET	(1 << 14)	/* E-net controller interrupt	*/
#define	INT_SCSI	(1 << 13)	/* SCSI controller interrupt	*/
#define	INT_DMA		(1 << 12)	/* DMA device interrupt		*/
#define	INT_IRQ15	(1 << 7)	/* PC/AT interrupts 15-9, 7-3	*/
#define	INT_IRQ14	(1 << 6)
#define	INT_IRQ12	(1 << 5)
#define	INT_IRQ11	(1 << 10)
#define	INT_IRQ10	(1 << 9)
#define	INT_IRQ9	(1 << 8)
#define	INT_IRQ7	(1 << 4)
#define	INT_IRQ6	(1 << 3)
#define	INT_IRQ5	(1 << 2)
#define	INT_IRQ4	(1 << 1)
#define	INT_IRQ3	(1 << 0)
#define	INT_ATMASK	(INT_IRQ15 | INT_IRQ14 | INT_IRQ12 | INT_IRQ11 | INT_IRQ10 | INT_IRQ9 | INT_IRQ7 | INT_IRQ6 | INT_IRQ5 | INT_IRQ4 | INT_IRQ3)

/*
 * PC AT bus register information
 */
#define	PCAT		0x18070002	/* PCAT register - BYTE access	*/
#define	AT_FLOWTOMBUS	(1 << 15)
#define	AT_FLOWTHRU	(1 << 14)
#define	AT_9516DACK	(1 << 13)
#define	AT_ENDMARQST	(1 << 12)
#define	AT_ENALTRQST4	(1 << 11)
#define	AT_ENALTRQST3	(1 << 10)
#define	AT_ENALTRQST2	(1 << 9)
#define	AT_ENALTRQST1	(1 << 8)
#define	AT_IADDR	0xff
#define AT_IADDR_SHIFT	20		/* bit 20 of ibus address in bit 0*/
#define	AT_IADDR_MASK	0xfffff		/* 1 Meg window into MBus memory  */

#define	SCSI_BASE	0x180d0000	/* SCSI base - BYTE access	*/

#define	ENETPROM_BASE	0x1d000000	/* Ethernet PROM - BYTE access	*/
#define	ENETPROM_RB3125 ((IDPROM_R3200+ID_EA1_OFF) & ~3)

/*
 * M2000 specfic information - taken from M2000 software changes 3/14/88
 */

/*
 * M2000 CPU Configuration Register - expanded to 32 bits, and address
 * changed.  Note that the least significant 4 bits read back the state of
 * the CPU Board Control Register.
 */

#define CPU_CR_M2000	0x1e080004	/* 32 bits now */
#define CPU_CR_RB3125	0x1e080004	/* 32 bits now */

#define CR_SCCTIB	(1<<31)
#define CR_SCCRIB	(1<<30)
#define CR_SCCRLBKB	(1<<29)
#define CR_SCCLLBKB	(1<<28)
#define CR_SCCDSRB	(1<<27)
#define CR_SCCDRSOB	(1<<26)
#define CR_SCCDRSIB	(1<<25)
#define CR_BUFPARSEL	(1<<24)
#define CR_VMERMWB	(1<<23)		/* active low - asserted when the
						RMW flag is set */
#define CR_SYSCONENB	(1<<22)		/* active low - asserted when this
						CPU is VME controller */
#define CR_LED_5	(1<<21)		/* copy of LED output bit 5 */
#define CR_LED_4	(1<<20)		/* copy of LED output bit 4 */
#define CR_LED_3	(1<<19)		/* copy of LED output bit 3 */
#define CR_LED_2	(1<<18)		/* copy of LED output bit 2 */
#define CR_LED_1	(1<<17)		/* copy of LED output bit 1 */
#define CR_LED_0	(1<<16)		/* copy of LED output bit 0 */
#define CR_MIPSINTINB	(1<<15)		/* active low - asserted when MipsInt */
#define CR_DMARESETB	(1<<14)		/* genesis dma reset bit */
					/*	is active on the backplane */
					/* bit 14 unused */
#define CR_VMERESETINB	(1<<13)		/* active low - asserted when VmeReset
					/*	is active on the backplane */
#define CR_SCSIRESETB	(1<<12)		/* backplane slot number 4 */
#define CR_CLRBUFERRB	(1<<11)		/* backplane slot number 3 */
#define CR_SLOTID_2	(1<<10)		/* backplane slot number 2 */
#define CR_SLOTID_1	(1<<9)		/* backplane slot number 1 */
#define CR_SLOTID_0	(1<<8)		/* backplane slot number 0 */
#define CR_LMEMSELB	(1<<7)		/* active low - asserted to map private
						memory into low addresses */
#define CR_COLDSTART	(1<<6)
#define CR_LNCRESETB	(1<<5)		/* active high - CPU is VME master */
#define CR_FPPRES	(1<<4)		/* active high - FPU present */
/*
 * The next 4 are READ-ONLY in this register.  To initiate one of these
 * from software, write the appropriate bit in the CPU Control Register
 * described below
 */
#define CR_MIPSINT	(1<<3)		/* active high - interprocessor int. */
#define CR_SWWARMRESET	(1<<2)		/* active high - reset CPU and VME */
#define CR_SWMEMRESET	(1<<1)		/* active high - reset memory */
#define CR_SWVMERESET	(1<<0)		/* active high - reset VME */

/*
 * ESR is the Error status register - this replaces the the Private Memory
 * Status Register.  This is synonymous to the FID on M120.
 */

#define ESR		0x1e100002	/* byte access */

					/* bits 7-6 unused */
#define ESR_ACFAIL	(1<<5)		/* active high - power fail */
#define ESR_RD_PROTO	(1<<4)		/* active low - read protocol error */
#define ESR_RD_TIMEOUT	(1<<3)		/* active low - read timeout */
#define ESR_RD_BERR	(1<<2)		/* active low - read bus error */
#define ESR_WR_TIMEOUT	(1<<1)		/* active low - write timeout */
#define ESR_WR_BERR	(1<<0)		/* active low - write bus error */

/*
 * Note: The LED register is the same as m-series except that the reset
 * bits were moved out.  Bits 5-0 still control the LED's as on M-series
 */

/*
 * The CPU Control Register is used to reset some or all of the system
 * via software.  The low four bits in this register are replicated
 * READ-ONLY in the CPU Configuration Register in the same bit positions
 */

#define CPU_CON_M2000	0x1e080007	/* byte access */

#define CON_MIPSINT	(1<<3)		/* active high - interprocessor int. */
#define CON_SWWARMRESET (1<<2)		/* active high - reset CPU and VME */
#define CON_SWMEMRESET	(1<<1)		/* active high - reset memory */
#define CON_SWVMERESET	(1<<0)		/* active high - reset VME */

/*
 * Note: the I/O address register does not exist on M2000.  The slot number
 * information in the CPU configuration register replaces this function
 */

/*
 * The Memory Address Register is changed to provide independent mapping
 * of the memory from the private or VME sides.  The bits are defined as:
 *
 *		15-9	Private memory address (compare to address bits 31-25)
 *		   8	unused
 *		 7-1	VME memory address (compare to address bits 31-25)
 *		   0	unused
 */

#define MAR_M2000	0xXXXXXXXX	/* halfword access? */

/*
 * The Memory Control Register has had several bits removed.  The
 * definition of bits for M2000 are as follows:
 */

#define MCR_M2000	0xXXXXXXXX	/* haflword access? */

					/* bit 15 unused */
#define MCR_INTLEVEL_1B	(1<<14)
#define MCR_INTLEVEL_0B	(1<<13)
#define MCR_INTLEVEL	(MCR_INTLEVEL_1B | MCR_INTLEVEL_0B)
					/* interrupt level */
					/* bit 12-10 unused */
#define MCR_INWR_CKB	(1<<9)		/* high true - inhibit write of
						check bits */
#define MCR_INWR_DAB	(1<<8)		/* high true - inhibit write of
						data bits */
#define MCR_IN_ECC	(1<<7)		/* inhibit ECC correction */
#define MCR_EN_DBLECC	(1<<6)		/* enable double ECC error interrupt */
#define MCR_EN_SINECC	(1<<5)		/* enable single ECC error interrupt */
					/* bit 4 unused - was EnInt.TErr */
#define MCR_EN_SYN	(1<<3)		/* enable syndrome error latches */
					/* bit 2 unused - was En.PrivErr */
#define MCR_EN_MEMVME	(1<<1)		/* high true - enable memory transfers
						on VME bus */
#define MCR_EN_MEMPRIV	(1<<0)		/* high true - enable memory transfers
						on private bus */

/* 
 * The Memory Status Register has changed alot.  All error status is now in
 * this register.  The Even Syndrome, Odd Syndrome, Even Check and Odd Check
 * registers of the R2850 are not on the R3250.  Bit definitions are:
 */

#define MSR_M2000	0xXXXXXXXX	/* halfword access? */

#define	MSR_SYN_6	(1<<15)		/* syndrome bit 6 */
#define	MSR_SYN_5	(1<<14)		/* syndrome bit 5 */
#define	MSR_SYN_4	(1<<13)		/* syndrome bit 4 */
#define	MSR_SYN_3	(1<<12)		/* syndrome bit 3 */
#define	MSR_SYN_2	(1<<11)		/* syndrome bit 2 */
#define	MSR_SYN_1	(1<<10)		/* syndrome bit 1 */
#define	MSR_SYN_0	(1<<9)		/* syndrome bit 0 */
					/* bits 8-6 unused */
#define MSR_BANK_DEC_2	(1<<5)		/* bank decode bit 2 (addr bit 24) */
#define MSR_BANK_DEC_1	(1<<4)		/* bank decode bit 1 (addr bit 23) */
#define MSR_BANK_DEC_0	(1<<3)		/* bank decode bit 0 (addr bit 2)  */
#define MSR_DBL_BIT_ERR	(1<<2)		/* high true - double bit error */
#define MSR_SIN_BIT_ERR	(1<<1)		/* high true - single bit error */
#define MSR_VME_ERR	(1<<0)		/* vme error - 1=VME, 0=Private Bus */

/*
 * The following are R3030 (Pizazz) defines
 */
#define	VIDEO_AT_R3030	0x10000000	/* video and restricted AT bus */

/* #define	NCR_BASE_R3030	0x18000000	/* NCR53C94 low order 8 bits */

#define	SCSI_BASE_R3030	0x18000000	/* NCR53C94 low order 8 bits */
#define	KEYBOARD_BASE	0x19000000	/* i8042 keyboard ctrl low 8 bits*/

/* #define	RAMBO_BASE_R3030 0x1c000000	/* Mips Rambo DMA engine */
#define	RAMBO_BASE 	0x1c000000	/* Mips Rambo DMA engine */

#define	FLOPPY_BASE	0x1e000000	/* i82072 floppy low 8 bits */
#define	FLOPPY_TC	0x1e800000	/* floppy terminal count sig reg */
/*
 * The following are RB3125 defines
 */
#define	SCSI_BASE_RB3125 0x1e002000	/* NCR53C94 low order 8 bits */
#define	G_DMA_BASE 	 0x1e008000	/* Genesis discrete DMA */

#define SCSI_BASE_R4030EB 0xe0002000

#ifdef ARCS
#include "r4030def.h"
#endif ARCS
