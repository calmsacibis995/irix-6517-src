/* vmechip2.h - VMEbus Interface Controller */

/* Copyright 1991-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,12jan93,ccc  added Miscellaneous Control Register for new chips.
01d,17jun92,ccc  fixed typo, SPR 1353.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01a,17jun91,ccc	 written.
*/

#ifdef	DOC
#define INCvmechip2h
#endif	/* DOC */

#ifndef	INCvmechip2h
#define	INCvmechip2h

/*
 * This file contains constants for the mv167 VMEbus Interface controller.
 * The macros VMECHIP2_BASE_ADRS and VC2BUS_BASE_ADRS must be defined
 * when including this header.

 * The registers are listed in ascending (numerical) order; the definitions
 * for each register are started with a header eg.
 */

#ifdef	_ASMLANGUAGE
#define CASTINT
#else
#define CASTINT (int *)
#endif	/* _ASMLANGUAGE */

/*
 * on-board access, register definitions
 * these registers MUST BE WRITTEN 4-BYTE WRITES ONLY
 * they can be read as byte, two-bytes or 4-bytes.
 */

#define	VMECHIP2_REG_INTERVAL	1

#ifndef VMECHIP2_ADRS	/* to permit alternative board addressing */
#define VMECHIP2_ADRS(reg)   (CASTINT (VMECHIP2_BASE_ADRS + \
				       (reg * VMECHIP2_REG_INTERVAL)))
#define VC2GCSR_ADRS(reg)    (CASTINT (VMECHIP2_BASE_ADRS + 0x100 + \
				       (reg * VMECHIP2_REG_INTERVAL)))
#endif	/* VMECHIP2_ADRS */

#ifndef	VC2BUS_ADRS	/* to permit alternative board addressing */
#define	VC2BUS_ADRS(reg)	(CASTINT (VC2BUS_BASE_ADRS + (reg)))
#endif	/* VC2BUS_ADRS */

/* WRITES MUST BE 4-BYTE WRITES ONLY */
/*
 *		VMEbus Slave Ending Address Register 1		0x00	31-16
 *		VMEbus Slave Starting Address Register 1	0x00	15-00
 */
#define VMECHIP2_VSAR1		VMECHIP2_ADRS(0x00)
/*
 *		VMEbus Slave Ending Address Register 2		0x04	31-16
 *		VMEbus Slave Starting Address Register 2	0x04	15-00
 */
#define	VMECHIP2_VSAR2		VMECHIP2_ADRS(0x04)
/*
 *		VMEbus Slave Address Translation Address Reg 1	0x08	31-16
 *		VMEbus Slave Adrs Translation Select Reg 1	0x08	15-00
 */
#define	VMECHIP2_VSATR1		VMECHIP2_ADRS(0x08)
/*
 *		VMEbus Slave Adrs Translation Address Reg 2	0x0c	31-16
 *		VMEbus Slave Adrs Translation Select Reg 2	0x0c	15-00
 */
#define	VMECHIP2_VSATR2		VMECHIP2_ADRS(0x0c)
/*
 *		VMEbus Slave Write Post and Snoop Control Reg 2	0x10	31-24
 *		VMEbus Slave Address Modifier Select Reg 2	0x10	23-16
 *		VMEbus Slave Write Post and Snoop Control Reg 1	0x10	15-08
 *		VMEbus Slave Address Modifier Select Reg 1	0x10	07-00
 */
#define	VMECHIP2_VSAMSR		VMECHIP2_ADRS(0x10)
/*
 *		Local Bus Slave Ending Address Reg 1		0x14	31-16
 *		Local Bus Slave Starting Address Reg 1		0x14	16-00
 */
#define	VMECHIP2_LBSAR1		VMECHIP2_ADRS(0x14)
/*
 *		Local Bus Slave Ending Address Reg 2		0x18	31-16
 *		Local Bus Slave Starting Address Reg 2		0x18	15-00
 */
#define	VMECHIP2_LBSAR2		VMECHIP2_ADRS(0x18)
/*
 *		Local Bus Slave Ending Address Reg 3		0x1c	31-16
 *		Local Bus Slave Starting Address Reg 3		0x1c	15-00
 */
#define	VMECHIP2_LBSAR3		VMECHIP2_ADRS(0x1c)
/*
 *		Local Bus Slave Ending Address Reg 4		0x20	31-16
 *		Local Bus Slave Starting Address Reg 4		0x20	15-00
 */
#define	VMECHIP2_LBSAR4		VMECHIP2_ADRS(0x20)
/*
 *		Local Bus Slave Address Translation Adrs Reg 1	0x24	31-16
 *		Local Bus Slave Address Translation Sel Reg 1	0x24	15-00
 */
#define	VMECHIP2_LBSATR1	VMECHIP2_ADRS(0x24)
/*
 *		Local Bus Slave Attribute Reg 4			0x28	31-24
 *		Local Bus Slave Attribute Reg 3			0x28	23-16
 *		Local Bus Slave Attribute Reg 2			0x28	15-08
 *		Local Bus Slave Attribute Reg 1			0x28	07-00
 */
#define	VMECHIP2_LBSAR		VMECHIP2_ADRS(0x28)
/*
 *		VMEbus Slave GCSR Group Address Reg		0x2c	31-24
 *		VMEbus Slave GCSR Board Address Reg		0x2c	23-20
 *		Local Bus To VMEbus Enable Control Reg		0x2c	19-16
 *		Local Bus To VMEbus I/O Control Reg		0x2c	15-08
 *		ROM Control Reg					0x2c	07-00
 */
#define	VMECHIP2_LBTVCR		VMECHIP2_ADRS(0x2c)
/*
 *		EPROM Decoder, SRAM and DMA Control Register	0x30	23-16
 *		Local Bus To VMEbus Requester Control Reg	0x30	15-08
 *		DMAC Control Register 1				0x30	07-00
 */
#define	VMECHIP2_DMACR1		VMECHIP2_ADRS(0x30)
/*
 *		DMAC Control Register 2				0x34	15-08
 *		DMAC Control Register 2				0x34	07-00
 */
#define	VMECHIP2_DMACR2		VMECHIP2_ADRS(0x34)
/*
 *		DMAC Local Bus Address Counter			0x38	31-00
 */
#define	VMECHIP2_DMACLBAC	VMECHIP2_ADRS(0x38)
/*
 *		DMAC VMEbus Address Counter			0x3c	31-00
 */
#define	VMECHIP2_DMACVAC	VMECHIP2_ADRS(0x3c)
/*
 *		DMAC Byte Counter				0x40	31-00
 */
#define	VMECHIP2_DMACBC		VMECHIP2_ADRS(0x40)
/*
 *		Table Address Counter				0x44	31-00
 */
#define	VMECHIP2_TAC		VMECHIP2_ADRS(0x44)
/*
 *		VMEbus Interrupter Control Register		0x48	31-24
 *		VMEbus Interrupter Vector Register		0x48	23-16
 *		MPU Status and DMA Interrupt Count Register	0x48	15-08
 *		DMAC Status Register				0x48	07-00
 */
#define	VMECHIP2_ICR		VMECHIP2_ADRS(0x48)
/*
 *		VMEbus Arbiter Timeout Control Register		0x4c	31-24
 *		DMAC Ton/Toff Timers and VMEbus Global Timeout	0x4c	23-16
 *		VME Access, Local Bus and Watchdog Timeout	0x4c	15-08
 *		Prescaler Control Register			0x4c	07-00
 */
#define	VMECHIP2_TIMEOUTCR	VMECHIP2_ADRS(0x4c)
/*
 *		Tick Timer 1 Compair Register			0x50	31-00
 */
#define	VMECHIP2_TTCOMP1	VMECHIP2_ADRS(0x50)
/*
 *		Tick Timer 1 Counter				0x54	31-00
 */
#define	VMECHIP2_TTCOUNT1	VMECHIP2_ADRS(0x54)
/*
 *		Tick Timer 2 Compair Register			0x58	31-00
 */
#define	VMECHIP2_TTCOMP2	VMECHIP2_ADRS(0x58)
/*
 *		Tick Timer 2 Counter				0x5c	31-00
 */
#define	VMECHIP2_TTCOUNT2	VMECHIP2_ADRS(0x5c)
/*
 *		Board Control Register				0x60	31-24
 *		Watchdog Timer Control Register			0x60	23-16
 *		Tick Timer 2 Control Register			0x60	15-08
 *		Tick Timer 1 Control Register			0x60	07-00
 */
#define	VMECHIP2_TIMERCR	VMECHIP2_ADRS(0x60)
/*
 *		Prescaler Counter				0x64	31-00
 */
#define	VMECHIP2_PRESCALE_CNT	VMECHIP2_ADRS(0x64)
/*
 *		Local Bus Interrupter Status Reg (24-31)	0x68	31-24
 *		Local Bus Interrupter Status Reg (16-23)	0x68	23-16
 *		Local Bus Interrupter Status Reg (8-15)		0x68	15-08
 *		Local Bus Interrupter Status Reg (0-7)		0x68	07-00
 */
#define	VMECHIP2_LBISR		VMECHIP2_ADRS(0x68)
/*
 *		Local Bus Interrupter Enable Reg (24-31)	0x6c	31-24
 *		Local Bus Interrupter Enable Reg (16-23)	0x6c	24-16
 *		Local Bus Interrupter Enable Reg (8-15)		0x6c	15-08
 *		Local Bus Interrupter Enable Reg (0-7)		0x6c	07-00
 */
#define	VMECHIP2_LBIER		VMECHIP2_ADRS(0x6c)
/*
 *		Software Interrupt Set Register (8-15)		0x70	15-08
 */
#define	VMECHIP2_SISR		VMECHIP2_ADRS(0x70)
/*
 *		Interrupt Clear Register (24-31)		0x74	31-24
 *		Interrupt Clear Register (16-23)		0x74	23-16
 *		Interrupt Clear Register (08-15)		0x74	15-08
 */
#define	VMECHIP2_ICLR		VMECHIP2_ADRS(0x74)
/*
 *		Interrupt Level Register 1 (24-31)		0x78	31-24
 *		Interrupt Level Register 1 (16-23)		0x78	23-16
 *		Interrupt Level Register 1 (8-15)		0x78	15-08
 *		Interrupt Level Register 1 (0-7)		0x78	07-00
 */
#define	VMECHIP2_ILR1		VMECHIP2_ADRS(0x78)
/*
 *		Interrupt Level Register 2 (24-31)		0x7c	31-24
 *		Interrupt Level Register 2 (16-23)		0x7c	23-16
 *		Interrupt Level Register 2 (8-15)		0x7c	15-08
 *		Interrupt Level Register 2 (0-7)		0x7c	07-00
 */
#define	VMECHIP2_ILR2		VMECHIP2_ADRS(0x7c)
/*
 *		Interrupt Level Register 3 (24-31)		0x80	31-24
 *		Interrupt Level Register 3 (16-23)		0x80	23-16
 *		Interrupt Level Register 3 (8-15)		0x80	15-08
 *		Interrupt Level Register 3 (0-7)		0x80	07-00
 */
#define	VMECHIP2_ILR3		VMECHIP2_ADRS(0x80)
/*
 *		Interrupt Level Register 4 (24-31)		0x84	31-24
 *		Interrupt Level Register 4 (16-23)		0x84	23-16
 *		Interrupt Level Register 4 (8-15)		0x84	15-08
 *		Interrupt Level Register 4 (0-7)		0x84	07-00
 */
#define	VMECHIP2_ILR4		VMECHIP2_ADRS(0x84)
/*
 *		Vector Base Register				0x88	31-24
 *		I/O Control Register 1				0x88	23-16
 *		I/O Control Register 2				0x88	15-08
 *		I/O Control Register 3				0x88	07-00
 */
#define	VMECHIP2_IOCR		VMECHIP2_ADRS(0x88)
/*
 *		Miscellaneous Control Register			0x8c    07-00
 */
#define	VMECHIP2_MISCCR		VMECHIP2_ADRS(0x8c)

/* These Global Status and Control Register are 16-bit registers
 * accessible from both the VMEbus and the local bus.
 * They are byte accessible registers.
 */

#define	VC2GCSR_REVISION	VC2GCSR_ADRS(0x02)  /* VMEchip2 Rev Reg */
#define	VC2GCSR_ID		VC2GCSR_ADRS(0x03)  /* VMEchip2 ID Reg */
#define	VC2GCSR_LM_SIG		VC2GCSR_ADRS(0x06)  /* VMEchip2 LM/SIG Reg */
#define	VC2GCSR_BSCR		VC2GCSR_ADRS(0x07)  /* Board Stat/Cont Reg */
#define	VC2GCSR_GPR0H		VC2GCSR_ADRS(0x0a)  /* General Purpose Reg 0 */
#define	VC2GCSR_GPR0L		VC2GCSR_ADRS(0x0b)  /* General Purpose Reg 0 */
#define	VC2GCSR_GPR1H		VC2GCSR_ADRS(0x0e)  /* General Purpose Reg 1 */
#define	VC2GCSR_GPR1L		VC2GCSR_ADRS(0x0f)  /* General Purpose Reg 1 */
#define	VC2GCSR_GPR2H		VC2GCSR_ADRS(0x12)  /* General Purpose Reg 2 */
#define	VC2GCSR_GPR2L		VC2GCSR_ADRS(0x13)  /* General Purpose Reg 2 */
#define	VC2GCSR_GPR3H		VC2GCSR_ADRS(0x16)  /* General Purpose Reg 3 */
#define	VC2GCSR_GPR3L		VC2GCSR_ADRS(0x17)  /* General Purpose Reg 3 */
#define	VC2GCSR_GPR4H		VC2GCSR_ADRS(0x1a)  /* General Purpose Reg 4 */
#define	VC2GCSR_GPR4L		VC2GCSR_ADRS(0x1b)  /* General Purpose Reg 4 */
#define	VC2GCSR_GPR5H		VC2GCSR_ADRS(0x1e)  /* General Purpose Reg 5 */
#define	VC2GCSR_GPR5L		VC2GCSR_ADRS(0x1f)  /* General Purpose Reg 5 */

#define	VC2BUS_REVISION		VC2BUS_ADRS(0x00)  /* Revision Reg */
#define	VC2BUS_ID		VC2BUS_ADRS(0x01)  /* ID Reg */
#define	VC2BUS_LM_SIG		VC2BUS_ADRS(0x02)  /* LM/SIG Reg */
#define	VC2BUS_BSCR		VC2BUS_ADRS(0x03)  /* Board Stat/Cont Reg */
#define VC2BUS_GPR0H		VC2BUS_ADRS(0x04)  /* General Purpose Reg 0 */
#define	VC2BUS_GPR0L		VC2BUS_ADRS(0x05)  /* General Purpose Reg 0 */
#define	VC2BUS_GPR1H		VC2BUS_ADRS(0x06)  /* General Purpose Reg 1 */
#define	VC2BUS_GPR1L		VC2BUS_ADRS(0x07)  /* General Purpose Reg 1 */
#define	VC2BUS_GPR2H		VC2BUS_ADRS(0x08)  /* General Purpose Reg 2 */
#define	VC2BUS_GPR2L		VC2BUS_ADRS(0x09)  /* General Purpose Reg 2 */
#define	VC2BUS_GPR3H		VC2BUS_ADRS(0x0a)  /* General Purpose Reg 3 */
#define	VC2BUS_GPR3L		VC2BUS_ADRS(0x0b)  /* General Purpose Reg 3 */
#define	VC2BUS_GPR4H		VC2BUS_ADRS(0x0c)  /* General Purpose Reg 4 */
#define	VC2BUS_GPR4L		VC2BUS_ADRS(0x0d)  /* General Purpose Reg 4 */
#define	VC2BUS_GPR5H		VC2BUS_ADRS(0x0e)  /* General Purpose Reg 5 */
#define	VC2BUS_GPR5L		VC2BUS_ADRS(0x0f)  /* General Purpose Reg 5 */

/* NOW LET'S DEFINE THE BITS FOR THESE REGISTERS */

/************************************************************************
* VMEbus Slave Write Post and Snoop Control Reg 2	0x10	31-24	*
* VMEbus Slave Address Modifier Select Reg 2		0x10	23-16	*
* VMEbus Slave Write Post and Snoop Control Reg 1	0x10	15-08	*
* VMEbus Slave Address Modifier Select Reg 1		0x10	07-00	*
*************************************************************************/
#define	VSAMSR2_SNP_INHIBIT	0x00	   /* Snoop Inhibited		26-25 */
#define	VSAMSR2_SNP_WSD		(1 << 25)  /* Write-Sink Data		26-25 */
#define	VSAMSR2_SNP_WI		(2 << 25)  /* Write-Invalidate		26-25 */
#define	VSAMSR2_WP		(1 << 24)  /* Write Posting Enabled	24 */
#define	VSAMSR2_SUP		(1 << 23)  /* 2nd map to supervi	23 */
#define	VSAMSR2_USR		(1 << 22)  /* 2nd map to user 		22 */
#define	VSAMSR2_A32		(1 << 21)  /* 2nd map to extend access	21 */
#define	VSAMSR2_A24		(1 << 20)  /* 2nd map to stanard access	20 */
#define	VSAMSR2_D64		(1 << 19)  /* 2nd map to D64 block acc  19 */
#define	VSAMSR2_BLK		(1 << 18)  /* 2nd map to block access	18 */
#define	VSAMSR2_PGM		(1 << 17)  /* 2nd map to program access	17 */
#define	VSAMSR2_DAT		(1 << 16)  /* 2nd map to data access	16 */
#define	VSAMSR1_SNP_INHIBIT	0x00	   /* Snoop Inhibited		10-9 */
#define	VSAMSR1_SNP_WSD		(1 << 9)   /* Write-Sink Data		10-9 */
#define	VSAMSR1_SNP_WI		(2 << 9)   /* Write-Invalidate		10-9 */
#define	VSAMSR1_WP		(1 << 8)   /* Write Posting Enabled	 8 */
#define	VSAMSR1_SUP		(1 << 7)   /* 1st map to supervi	 7 */
#define	VSAMSR1_USR		(1 << 6)   /* 1st map to user	 	 6 */
#define	VSAMSR1_A32		(1 << 5)   /* 1st map to extended access 5 */
#define	VSAMSR1_A24		(1 << 4)   /* 1st map to standard access 4 */
#define	VSAMSR1_D64		(1 << 3)   /* 1st map to D64 block acces 3 */
#define	VSAMSR1_BLK		(1 << 2)   /* 1st map to block access	 2 */
#define	VSAMSR1_PGM		(1 << 1)   /* 1st map to program access	 1 */
#define	VSAMSR1_DAT		0x01	   /* 1st map to data access	 0 */

/************************************************************************
* Local Bus Slave Attribute Register 4			0x28	31-24	*
* Local Bus Slave Attribute Register 3			0x28	23-16	*
* Local Bus Slave Attribute Register 2			0x28	15-08	*
* Local Bus Slave Attribute Register 1			0x28	07-00	*
*************************************************************************/
#define	LBSAR4_D16		(1 << 31)  /* D16 data transfers	31 */
#define	LBSAR4_D32		0x00	   /* D32 data transfers	31 */
#define	LBSAR4_WP_ENABLE	(1 << 30)  /* Write posting enabled	30 */
#define	LBSAR4_WP_DISABLE	0x00 	   /* Write posting disabled	30 */
/*	The following are for the Address Modifier equates		29-24 */
#define	LBSAR4_AM_STD_SUP_ASCENDING	VME_AM_STD_SUP_ASCENDING << 24
#define	LBSAR4_AM_STD_SUP_PGM		VME_AM_STD_SUP_PGM << 24
#define	LBSAR4_AM_STD_SUP_DATA		VME_AM_STD_SUP_DATA << 24
#define	LBSAR4_AM_STD_USR_ASCENDING	VME_AM_STD_USR_ASCENDING << 24
#define LBSAR4_AM_STD_USR_PGM		VME_AM_STD_USR_PGM << 24
#define	LBSAR4_AM_STD_USR_DATA		VME_AM_STD_USR_DATA << 24
#define	LBSAR4_AM_SUP_SHORT_IO		VME_AM_SUP_SHORT_IO << 24
#define	LBSAR4_AM_USR_SHORT_IO		VME_AM_USR_SHORT_IO << 24
#define	LBSAR4_AM_EXT_SUP_ASCENDING	VME_AM_EXT_SUP_ASCENDING << 24
#define	LBSAR4_AM_EXT_SUP_PGM		VME_AM_EXT_SUP_PGM << 24
#define	LBSAR4_AM_EXT_SUP_DATA		VME_AM_EXT_SUP_DATA << 24
#define	LBSAR4_AM_EXT_USR_ASCENDING	VME_AM_EXT_USR_ASCENDING << 24
#define	LBSAR4_AM_EXT_USR_PGM		VME_AM_EXT_USR_PGM << 24
#define	LBSAR4_AM_EXT_USR_DATA		VME_AM_EXT_USR_DATA << 24

#define	LBSAR3_D16		(1 << 23)  /* D16 data transfers	23 */
#define	LBSAR3_D32		0x00	   /* D32 data transfers	23 */
#define	LBSAR3_WP_ENABLE	(1 << 22)  /* Write posting enabled	22 */
#define	LBSAR3_WP_DISABLE	0x00	   /* Write posting disabled	22 */
/*	The following are for the Address Modifier equates		21-16 */
#define	LBSAR3_AM_STD_SUP_ASCENDING	VME_AM_STD_SUP_ASCENDING << 16
#define	LBSAR3_AM_STD_SUP_PGM		VME_AM_STD_SUP_PGM << 16
#define	LBSAR3_AM_STD_SUP_DATA		VME_AM_STD_SUP_DATA << 16
#define	LBSAR3_AM_STD_USR_ASCENDING	VME_AM_STD_USR_ASCENDING << 16
#define	LBSAR3_AM_STD_USR_PGM		VME_AM_STD_USR_PGM << 16
#define	LBSAR3_AM_STD_USR_DATA		VME_AM_STD_USR_DATA << 16
#define	LBSAR3_AM_SUP_SHORT_IO		VME_AM_SUP_SHORT_IO << 16
#define	LBSAR3_AM_USR_SHORT_IO		VME_AM_USR_SHORT_IO << 16
#define LBSAR3_AM_EXT_SUP_ASCENDING	VME_AM_EXT_SUP_ASCENDING << 16
#define	LBSAR3_AM_EXT_SUP_PGM		VME_AM_EXT_SUP_PGM << 16
#define	LBSAR3_AM_EXT_SUP_DATA		VME_AM_EXT_SUP_DATA << 16
#define	LBSAR3_AM_EXT_USR_ASCENDING	VME_AM_EXT_USR_ASCENDING << 16
#define	LBSAR3_AM_EXT_USR_PGM		VME_AM_EXT_USR_PGM << 16
#define LBSAR3_AM_EXT_USR_DATA		VME_AM_EXT_USR_DATA << 16

#define	LBSAR2_D16		(1 << 15)  /* D16 data transfers	15 */
#define	LBSAR2_D32		0x00	   /* D32 data transfers	15 */
#define	LBSAR2_WP_ENABLE	(1 << 14)  /* Write posting enabled	14 */
#define	LBSAR2_WP_DISABLE	0x00	   /* Write posting disabled	14 */
/*	The following are for the Address Modifier equates		13-8 */
#define	LBSAR2_AM_STD_SUP_ASCENDING	VME_AM_STD_SUP_ASCENDING << 8
#define	LBSAR2_AM_STD_SUP_PGM		VME_AM_STD_SUP_PGM << 8
#define	LBSAR2_AM_STD_SUP_DATA		VME_AM_STD_SUP_DATA << 8
#define	LBSAR2_AM_STD_USR_ASCENDING	VME_AM_STD_USR_ASCENDING << 8
#define	LBSAR2_AM_STD_USR_PGM		VME_AM_STD_USR_PGM << 8
#define	LBSAR2_AM_STD_USR_DATA		VME_AM_STD_USR_DATA << 8
#define	LBSAR2_AM_SUP_SHORT_IO		VME_AM_SUP_SHORT_IO << 8
#define	LBSAR2_AM_USR_SHORT_IO		VME_AM_USR_SHORT_IO << 8
#define	LBSAR2_AM_EXT_SUP_ASCENDING	VME_AM_EXT_SUP_ASCENDING << 8
#define	LBSAR2_AM_EXT_SUP_PGM		VME_AM_EXT_SUP_PGM << 8
#define	LBSAR2_AM_EXT_SUP_DATA		VME_AM_EXT_SUP_DATA << 8
#define	LBSAR2_AM_EXT_USR_ASCENDING	VME_AM_EXT_USR_ASCENDING << 8
#define	LBSAR2_AM_EXT_USR_PGM		VME_AM_EXT_USR_PGM << 8
#define	LBSAR2_AM_EXT_USR_DATA		VME_AM_EXT_USR_DATA << 8

#define	LBSAR1_D16		(1 << 7)  /* D16 data transfers		 7 */
#define	LBSAR1_D32		0x00	  /* D32 data transfers		 7 */
#define	LBSAR1_WP_ENABLE	(1 << 6)  /* Write posting enabled	 6 */
#define	LBSAR1_WP_DISABLE	0x00	  /* Write posting disabled	 6 */
/*	The following are for the Address Modifier equates		 5-0 */
#define	LBSAR1_AM_STD_SUP_ASCENDING	VME_AM_STD_SUP_ASCENDING
#define	LBSAR1_AM_STD_SUP_PGM		VME_AM_STD_SUP_PGM
#define	LBSAR1_AM_STD_SUP_DATA		VME_AM_STD_SUP_DATA
#define	LBSAR1_AM_STD_USR_ASCENDING	VME_AM_STD_USR_ASCENDING
#define	LBSAR1_AM_STD_USR_PGM		VME_AM_STD_USR_PGM
#define	LBSAR1_AM_STD_USR_DATA		VME_AM_STD_USR_DATA
#define	LBSAR1_AM_SUP_SHORT_IO		VME_AM_SUP_SHORT_IO
#define	LBSAR1_AM_USR_SHORT_IO		VME_AM_USR_SHORT_IO
#define	LBSAR1_AM_EXT_SUP_ASCENDING	VME_AM_EXT_SUP_ASCENDING
#define	LBSAR1_AM_EXT_SUP_PGM		VME_AM_EXT_SUP_PGM
#define	LBSAR1_AM_EXT_SUP_DATA		VME_AM_EXT_SUP_DATA
#define	LBSAR1_AM_EXT_USR_ASCENDING	VME_AM_EXT_USR_ASCENDING
#define	LBSAR1_AM_EXT_USR_PGM		VME_AM_EXT_USR_PGM
#define	LBSAR1_AM_EXT_USR_DATA		VME_AM_EXT_USR_DATA

/************************************************************************
 * VMEbus Slave GCSR Group Address Reg			0x2c	31-24	*
 * VMEbus Slave GCSR Board Address Reg			0x2c	23-20	*
 * Local Bus To VMEbus Enable Control Reg		0x2c	19-16	*
 * Local Bus to VMEbus I/O Control Reg			0x2c	15-08	*
 * ROM Control Reg					0x2c	07-00	*
 ************************************************************************/

#define	LBTVCR_EN4		(1 << 19)  /* 4th VMEbus decoder enable	19 */
#define	LBTVCR_EN3		(1 << 18)  /* 3rd VMEbus decoder enable	18 */
#define	LBTVCR_EN2		(1 << 17)  /* 2nd VMEbus decoder enable	17 */
#define	LBTVCR_EN1		(1 << 16)  /* 1st VMEbus decoder enable	16 */
#define	LBTVCR_I2EN		(1 << 15)  /* Enable F page decoder	15 */
#define	LBTVCR_I2WP		(1 << 14)  /* Page F Write Post Enable	14 */
#define	LBTVCR_I2SUP		(1 << 13)  /* Supervisor AM for Page F	13 */
#define	LBTVCR_I2USR		0x00	   /* User AM for Page F	13 */
#define	LBTVCR_I2PROG		(1 << 12)  /* Program AM for Page F	12 */
#define	LBTVCR_I2DATA		0x00	   /* Data AM for Page F	12 */
#define	LBTVCR_I1EN		(1 << 11)  /* Short I/O Enabled		11 */
#define	LBTVCR_I1D16		(1 << 10)  /* D16 data for Short I/O	10 */
#define	LBTVCR_I1D32		0x00	   /* D32 data for Short I/O	10 */
#define	LBTVCR_I1WP		(1 << 9)   /* Write Post for Short I/O	 9 */
#define	LBTVCR_I1SUP		(1 << 8)   /* Supvisor AM for Short I/O	 8 */
#define	LBTVCR_I1USR		0x00	   /* User AM for Short I/O	 8 */
#define	LBTVCR_8MBIT		0x00	   /* 8 MBit Chips		 7-6 */
#define	LBTVCR_4MBIT		(1 << 6)   /* 4 MBit Chips		 7-6 */
#define	LBTVCR_2MBIT		(2 << 6)   /* 2 MBit Chips		 7-6 */
#define	LBTVCR_1MBIT		(3 << 6)   /* 1 MBit Chips		 7-6 */

/* B ROMs SPEED DEFINED FOR 25MHz BUS CLOCK */
#define	LBTVCR_B365NS		0x00	   /* 365 ns B EPROM Access	 5-3 */
#define	LBTVCR_B325NS		(1 << 3)   /* 325 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B285NS		(2 << 3)   /* 285 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B245NS		(3 << 3)   /* 245 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B205NS		(4 << 3)   /* 205 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B165NS		(5 << 3)   /* 165 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B125NS		(6 << 3)   /* 125 ns B EPROM Access 	 5-3 */
#define	LBTVCR_B85NS		(7 << 3)   /* 85 ns B EPROM Access 	 5-3 */

/* A ROMs SPEED DEFINED FOR 25MHz BUS CLOCK */
#define	LBTVCR_A365NS		0x00	   /* 365 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A325NS		0x01	   /* 325 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A285NS		0x02	   /* 285 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A245NS		0x03	   /* 245 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A205NS		0x04	   /* 205 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A165NS		0x05	   /* 165 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A125NS		0x06	   /* 125 ns A EPROM Access 	 2-0 */
#define	LBTVCR_A85NS		0x07	   /* 85 ns A EPROM Access 	 2-0 */

/************************************************************************
 * EPROM Decoder, SRAM and DMA Control Register		0x30	23-16	*
 * Local Bus To VMEbus Requester Control Reg		0x30	15-08	*
 * DMAC Control Register 1				0x30	07-00	*
 ************************************************************************/

#define	DMACR1_ROM0_ON		(1 << 20)  /* Map EPROM to $00000000	20 */
#define	DMACR1_ROM0_OFF		0x00	   /* No Map EPROM to $00000000	20 */
#define	DMACR1_SNOOP_INHIBIT	0x00	   /* Snoop Inhibited		19-18 */
#define	DMACR1_SINK_DATA	(1 << 18)  /* Sink Data			19-18 */
#define	DMACR1_INVALIDATE	(2 << 18)  /* Invalidate		19-18 */
#define	DMACR1_165NS		0x00	   /* 165 ns SRAM Access Time	17-16 */
#define	DMACR1_125NS		(1 << 16)  /* 125 ns SRAM Access Time	17-16 */
#define	DMACR1_85NS		(2 << 16)  /* 85 ns SRAM Access Time	17-16 */
#define	DMACR1_45NS		(3 << 16)  /* 45 ns SRAM Access Time	17-16 */
#define	DMACR1_ROBIN		(1 << 15)  /* Arbiter in Round Robin	15 */
#define	DMACR1_PRIORITY		0x00	   /* Arbiter in Priority	15 */
#define	DMACR1_DHB		(1 << 14)  /* VMEbus acquired from DWB	14 */
#define	DMACR1_DWB		(1 << 13)  /* VMEbus held		13 */
#define	DMACR1_LVFAIR		(1 << 11)  /* Requester in fair mode	11 */
#define	DMACR1_LVRWD		(1 << 10)  /* Release When Done		10 */
#define	DMACR1_LVROR		0x00	   /* Relase On Request		10 */
#define	DMACR1_LVREQ_L0		0x00	   /* Request Level 0		 9-8 */
#define	DMACR1_LVREQ_L1		(1 << 8)   /* Request Level 1		 9-8 */
#define	DMACR1_LVREQ_L2		(2 << 8)   /* Request Level 2		 9-8 */
#define	DMACR1_LVREQ_L3		(3 << 8)   /* Request Level 3		 9-8 */
#define	DMACR1_DHALT		(1 << 7)   /* Halt at end of Command	 7 */
#define	DMACR1_DEN		(1 << 6)   /* DMAC enabled		 6 */
#define	DMACR1_DTBL		(1 << 5)   /* Command chaining mode	 5 */
#define	DMACR1_DFAIR		(1 << 4)   /* Operates in fair mode	 4 */
#define	DMACR1_TIMER_BRX	0x00	   /* Release with Timer & BRx	 3-2 */
#define	DMACR1_TIMER		(1 << 2)   /* Release with Timer	 3-2 */
#define	DMACR1_BRX		(2 << 2)   /* Release with BRx		 3-2 */
#define DMACR1_TIMER_OR_BRX	(3 << 2)   /* Release with Timer or BRx	 3-2 */
#define	DMACR1_DREQ_L0		0x00	   /* VMEbus request Level 0	 1-0 */
#define	DMACR1_DREQ_L1		0x01	   /* VMEbus request Level 1	 1-0 */
#define	DMACR1_DREQ_L2		0x02	   /* VMEbus request Level 2	 1-0 */
#define	DMACR1_DREQ_L3		0x03	   /* VMEbus request Level 3	 1-0 */

/************************************************************************
 * DMAC Control Register 2				0x34	15-08	*
 * DMAC Control Register 2				0x34	07-00	*
 ************************************************************************/

#define	DMACR2_INTE		(1 << 15)  /* Interrupt Enabel		15 */
#define	DMACR2_SNOOP_INHIBIT	0x00	   /* Snoop Inhibited		14-13 */
#define	DMACR2_SINK_DATA	(1 << 13)  /* Sink Data			14-13 */
#define	DMACR2_INVALIDATE	(2 << 13)  /* Invalidate		14-13 */
#define	DMACR2_VINC		(1 << 11)  /* Increment Address on DMA	11 */
#define	DMACR2_LINC		(1 << 10)  /* Increment Local Addr DMA	10 */
#define	DMACR2_TO_VME		(1 << 9)   /* Transfer to VMEbus	 9 */
#define DMACR2_TO_LOCAL		0x00	   /* Transfer to Local bus	 9 */
#define	DMACR2_D16		(1 << 8)   /* D16 on the VMEbus		 8 */
#define	DMACR2_D32		0x00	   /* D32 on the VMEbus		 8 */
#define	DMACR2_NO_BLOCK		0x00	   /* Block Transfers Disabled	 7-6 */
#define	DMACR2_D32_BLOCK	(1 << 6)   /* D32 Block Transfer VMEbus	 7-6 */
#define	DMACR2_D64_BLOCK	(3 << 6)   /* D64 Block Transfer VMEbus	 7-6 */
#define	DMACR2_AM_STD_SUP_ASCENDING	VME_AM_STD_SUP_ASCENDING
#define	DMACR2_AM_STD_SUP_PGM		VME_AM_STD_SUP_PGM
#define	DMACR2_AM_STD_SUP_DATA		VME_AM_STD_SUP_DATA
#define	DMACR2_AM_STD_USR_ASCENDING	VME_AM_STD_USR_ASCENDING
#define	DMACR2_AM_STD_USR_PGM		VME_AM_STD_USR_PGM
#define	DMACR2_AM_STD_USR_DATA		VME_AM_STD_USR_DATA
#define	DMACR2_AM_SUP_SHORT_IO		VME_AM_SUP_SHORT_IO
#define	DMACR2_AM_USR_SHORT_IO		VME_AM_USR_SHORT_IO
#define	DMACR2_AM_EXT_SUP_ASCENDING	VME_AM_EXT_SUP_ASCENDING
#define	DMACR2_AM_EXT_SUP_PGM		VME_AM_EXT_SUP_PGM
#define	DMACR2_AM_EXT_SUP_DATA		VME_AM_EXT_SUP_DATA
#define	DMACR2_AM_EXT_USR_ASCENDING	VME_AM_EXT_USR_ASCENDING
#define	DMACR2_AM_EXT_USR_PGM		VME_AM_EXT_USR_PGM
#define	DMACR2_AM_EXT_USR_DATA		VME_AM_EXT_USR_DATA

/************************************************************************
 * VMEbus Interrupter Control Register			0x48	31-24	*
 * VMEbus Interrupter Vector Register			0x48	23-16	*
 * MPU Status and DMA Interrupt Count Register		0x48	15-08	*
 * DMAC Status Register					0x48	07-00	*
 ************************************************************************/

#define	ICR_IRQ1_INTERRUPTER	0x00	   /* Intrptr connected to IRQ1	30-29 */
#define	ICR_IRQ1_TIMER1		(1 << 29)  /* Timer 1 connected to IRQ1	30-29 */
#define	ICR_IRQ1_TIMER2		(3 << 29)  /* Timer 2 connected to IRQ2	30-29 */
#define	ICR_IRQ_CLEAR		(1 << 28)  /* Clear VMEbus IRQ		28 */
#define	ICR_IRQ_STATUS		(1 << 27)  /* IRQ not acknowledged	27 */
#define	ICR_IRQ_L1		(1 << 24)  /* Generate VME IRQ level 1	26-24 */
#define	ICR_IRQ_L2		(2 << 24)  /*		       level 2	26-24 */
#define	ICR_IRQ_L3		(3 << 24)  /*		       level 3	26-24 */
#define	ICR_IRQ_L4		(4 << 24)  /*		       level 4	26-24 */
#define	ICR_IRQ_L5		(5 << 24)  /*		       level 5	26-24 */
#define	ICR_IRQ_L6		(6 << 24)  /*		       level 6	26-24 */
#define	ICR_IRQ_L7		(7 << 24)  /*		       level 7	26-24 */
#define	ICR_MCLR		(1 << 11)  /* Clear MPU status bits	11 */
#define	ICR_MLBE		(1 << 10)  /* MPU received a TEA	10 */
#define	ICR_MLPE		(1 << 9)   /* MPU -TEA + Parity error	 9 */
#define	ICR_MLOB		(1 << 8)   /* MPU -TEA + Off board	 8 */
#define	ICR_MLTO		(1 << 7)   /* MPU -TEA + Local Time Out	 7 */
#define	ICR_DLBE		(1 << 6)   /* DMAC received a TEA	 6 */
#define	ICR_DLPE		(1 << 5)   /* DMAC -TEA + Parity Error	 5 */
#define	ICR_DLOB		(1 << 4)   /* DMAC -TEA + Off Board	 4 */
#define	ICR_DLTO		(1 << 3)   /* DMAC -TEA + Locl Time Out	 3 */
#define	ICR_TBL			(1 << 2)   /* DMAC error reading command 2 */
#define	ICR_VME			(1 << 1)   /* DMAC got VMEbus BERR	 1 */
#define	ICR_DONE		1	   /* DMAC finished		 0 */

/************************************************************************
 * VMEbus Arbiter Timeout Control Register		0x4c	31-24	*
 * DMAC Ton/Toff Timers and VMEbus Global Timeout	0x4c	23-16	*
 * VME Access, Local Bus and Watchdog Timeout		0x4c	15-08	*
 * Prescaler Control Register				0x4c	07-00	*
 ************************************************************************/

#define	TIMEOUTCR_ARBTO		(1 << 24)  /* Enable Grant Timeout	24 */
#define	TIMEOUTCR_OFF_0US	0x00	   /* DMAC off VMEbus for 0us	23-21 */
#define	TIMEOUTCR_OFF_16US	(1 << 21)  /*			16us	23-21 */
#define	TIMEOUTCR_OFF_32US	(2 << 21)  /*			32us	23-21 */
#define	TIMEOUTCR_OFF_64US	(3 << 21)  /*			64us	23-21 */
#define	TIMEOUTCR_OFF_128US	(4 << 21)  /*			128us	23-21 */
#define	TIMEOUTCR_OFF_256US	(5 << 21)  /*			256us	23-21 */
#define	TIMEOUTCR_OFF_512US	(6 << 21)  /*			512us	23-21 */
#define	TIMEOUTCR_OFF_1024US	(7 << 21)  /*			1024us	23-21 */
#define	TIMEOUTCR_ON_16US	0x00	   /* DMAC on VMEbus for 16us	20-18 */
#define	TIMEOUTCR_ON_32US	(1 << 18)  /*			32us	20-18 */
#define	TIMEOUTCR_ON_64US	(2 << 18)  /*			64us	20-18 */
#define	TIMEOUTCR_ON_128US	(3 << 18)  /*			128us	20-18 */
#define	TIMEOUTCR_ON_256US	(4 << 18)  /*			256us	20-18 */
#define	TIMEOUTCR_ON_512US	(5 << 18)  /*			512us	20-18 */
#define	TIMEOUTCR_ON_1024US	(6 << 18)  /*			1024us	20-18 */
#define	TIMEOUTCR_ON_DONE	(7 << 18)  /* DMAC on VMEbus until done	20-18 */
#define	TIMEOUTCR_VGTO_8US	0x00	   /* VMEbus global timeout 8us	17-16 */
#define	TIMEOUTCR_VGTO_64US	(1 << 16)  /*			 16us	17-16 */
#define	TIMEOUTCR_VGTO_256US	(2 << 16)  /*			256us	17-16 */
#define	TIMEOUTCR_VGTO_DISABLE	(3 << 16)  /* global timeout disabled	17-16 */
#define	TIMEOUTCR_VATO_64US	0x00	   /* VMEbus timeout    64us	15-14 */
#define	TIMEOUTCR_VATO_1MS	(1 << 14)  /*			 1ms	15-14 */
#define	TIMEOUTCR_VATO_32MS	(2 << 14)  /*			32ms	15-14 */
#define	TIMEOUTCR_VATO_DISABLE	(3 << 14)  /* access timeout disabled	15-14 */
#define	TIMEOUTCR_LBTO_8US	0x00	   /* Local Bus Timeout	8us	13-12 */
#define	TIMEOUTCR_LBTO_64US	(1 << 12)  /*			64us	13-12 */
#define	TIMEOUTCR_LBTO_256US	(2 << 12)  /*			256us	13-12 */
#define	TIMEOUTCR_LBTO_DISABLE	(3 << 12)  /* Local Timeout Disabled	13-12 */
#define	TIMEOUTCR_WDTO_512US	0x00	   /* Watchdog Timeout	512us	11-08 */
#define	TIMEOUTCR_WDTO_1MS	(1 << 8)   /* Watchdog Timeout	1ms	11-08 */
#define	TIMEOUTCR_WDTO_2MS	(2 << 8)   /* Watchdog Timeout	2ms	11-08 */
#define	TIMEOUTCR_WDTO_4MS	(3 << 8)   /* Watchdog Timeout	4ms	11-08 */
#define	TIMEOUTCR_WDTO_8MS	(4 << 8)   /* Watchdog Timeout	8ms	11-08 */
#define	TIMEOUTCR_WDTO_16MS	(5 << 8)   /* Watchdog Timeout	16ms	11-08 */
#define	TIMEOUTCR_WDTO_32MS	(6 << 8)   /* Watchdog Timeout	32ms	11-08 */
#define	TIMEOUTCR_WDTO_64MS	(7 << 8)   /* Watchdog Timeout	64ms	11-08 */
#define	TIMEOUTCR_WDTO_128MS	(8 << 8)   /* Watchdog Timeout	128ms	11-08 */
#define	TIMEOUTCR_WDTO_256MS	(9 << 8)   /* Watchdog Timeout	256ms	11-08 */
#define	TIMEOUTCR_WDTO_512MS	(10 << 8)  /* Watchdog Timeout	512ms	11-08 */
#define	TIMEOUTCR_WDTO_1S	(11 << 8)  /* Watchdog Timeout	1s	11-08 */
#define	TIMEOUTCR_WDTO_4S	(12 << 8)  /* Watchdog Timeout	4s	11-08 */
#define	TIMEOUTCR_WDTO_16S	(13 << 8)  /* Watchdog Timeout	16s	11-08 */
#define	TIMEOUTCR_WDTO_32S	(14 << 8)  /* Watchdog Timeout	32s	11-08 */
#define	TIMEOUTCR_WDTO_64S	(15 << 8)  /* Watchdog Timeout	64s	11-08 */

/************************************************************************
 * Board Control Register				0x60	31-24	*
 * Watchdog Timer Control Register			0x60	23-16	*
 * Tick Timer 2 Control Register			0x60	15-08	*
 * Tick Timer 1 Control Register			0x60	07-00	*
 ************************************************************************/

#define	TIMERCR_SCON		(1 << 30)  /* Board is System Cont	30 */
#define	TIMERCR_SFFL		(1 << 29)  /* VMEbus SYSFAIL is assertd	29 */
#define	TIMERCR_BRFLI		(1 << 28)  /* Board Fail is asserted	28 */
#define	TIMERCR_PURS		(1 << 27)  /* Power-Up Reset		27 */
#define	TIMERCR_CPURS		(1 << 26)  /* Clear Power-Up Reset	26 */
#define	TIMERCR_BDFLO		(1 << 25)  /* Assert the BRDFAIL Signal	25 */
#define	TIMERCR_RSWE		(1 << 24)  /* Enable Reset Switch	24 */
#define	TIMERCR_RSWD		0x00	   /* Disable Reset Switch	24 */
#define	TIMERCR_SRST		(1 << 23)  /* Assert SYSRESET on VMEbus	23 */
#define	TIMERCR_WDCS		(1 << 22)  /* Clear watchdog timeout	22 */
#define	TIMERCR_WDCC		(1 << 21)  /* Reset watchdog counter	21 */
#define	TIMERCR_WDTO		(1 << 20)  /* Watchdog Timeout occurd	20 */
#define	TIMERCR_WDBFE		(1 << 19)  /* Set Board Fail on WD TO	19 */
#define	TIMERCR_WDSYSRESET	(1 << 18)  /* Set SYSRESET on WD TO	18 */
#define	TIMERCR_WDLRESET	0x00	   /* Set LRESET on WD TO	18 */
#define	TIMERCR_WDRSE		(1 << 17)  /* RESET on Watchdog Timeout	17 */
#define	TIMERCR_WDEN		(1 << 16)  /* Enable Watchdog Timeout	16 */
#define	TIMERCR_WDDIS		0x00	   /* Disable Watchdog Timeout	16 */
#define	TIMERCR_TT2_COVF	(1 << 10)  /* Clear Over Flow Counter	10 */
#define	TIMERCR_TT2_COC		(1 << 9)   /* Clear On Compare		 9 */
#define	TIMERCR_TT2_EN		(1 << 8)   /* Enable Counter		 8 */
#define	TIMERCR_TT1_COVF	(1 << 2)   /* Clear Over Flow Counter	 2 */
#define	TIMERCR_TT1_COC		(1 << 1)   /* Clear On Compare		 1 */
#define	TIMERCR_TT1_EN		1	   /* Enable Counter		 0 */

/************************************************************************
 * Local Bus Interrupter Status Register		0x68	31-00	*
 ************************************************************************/

#define	LBISR_ACF		(1 << 31)  /* VMEbus ACFAIL Interrupt	31 */
#define	LBISR_AB		(1 << 30)  /* ABORT Switch Interrupt	30 */
#define	LBISR_SYSF		(1 << 29)  /* VMEbus SYSFAIL Interrupt	29 */
#define	LBISR_MWP		(1 << 28)  /* VMEbus master write post	28 */
#define	LBISR_PE		(1 << 27)  /* External Inter (parity)	27 */
#define	LBISR_VI1E		(1 << 26)  /* VMEbus IRQ1 edge Intr	26 */
#define	LBISR_TIC2		(1 << 25)  /* Tick Timer 2 Interrupt	25 */
#define	LBISR_TIC1		(1 << 24)  /* Tick Timer 1 Interrupt	24 */
#define	LBISR_VIA		(1 << 23)  /* VMEbus Inter Ack Inter	23 */
#define	LBISR_DMA		(1 << 22)  /* DMAC Interrupt		22 */
#define	LBISR_SIG3		(1 << 21)  /* GCSR SIG3 interrupt	21 */
#define	LBISR_SIG2		(1 << 20)  /* GCSR SIG2 interrupt	20 */
#define	LBISR_SIG1		(1 << 19)  /* GCSR SIG1 interrupt	19 */
#define	LBISR_SIG0		(1 << 18)  /* GCSR SIG0 interrupt	18 */
#define	LBISR_LM1		(1 << 17)  /* GCSR LM1 interrupt	17 */
#define	LBISR_LM0		(1 << 16)  /* GCSR LM0 interrupt	16 */
#define	LBISR_SW7		(1 << 15)  /* Software 7 interrupt	15 */
#define	LBISR_SW6		(1 << 14)  /* Software 6 interrupt	14 */
#define	LBISR_SW5		(1 << 13)  /* Software 5 interrupt	13 */
#define	LBISR_SW4		(1 << 12)  /* Software 4 interrupt	12 */
#define	LBISR_SW3		(1 << 11)  /* Software 3 interrupt	11 */
#define	LBISR_SW2		(1 << 10)  /* Software 2 interrupt	10 */
#define	LBISR_SW1		(1 << 9)   /* Software 1 interrupt	 9 */
#define	LBISR_SW0		(1 << 8)   /* Software 0 interrupt	 8 */
#define	LBISR_VME7		(1 << 6)   /* VMEbus IRQ7 interrupt	 6 */
#define	LBISR_VME6		(1 << 5)   /* VMEbus IRQ6 interrupt	 5 */
#define	LBISR_VME5		(1 << 4)   /* VMEbus IRQ5 interrupt	 4 */
#define	LBISR_VME4		(1 << 3)   /* VMEbus IRQ4 interrupt	 3 */
#define	LBISR_VME3		(1 << 2)   /* VMEbus IRQ3 interrupt	 2 */
#define	LBISR_VME2		(1 << 1)   /* VMEbus IRQ2 interrupt	 1 */
#define	LBISR_VME1		1	   /* VMEbus IRQ1 interrupt	 0 */

/************************************************************************
 * Local Bus Interrupter Enable Register		0x6c	31-00	*
 ************************************************************************/

#define	LBIER_EACF		(1 << 31)  /* Enable VMEbus ACFAIL IRQ	31 */
#define	LBIER_EAB		(1 << 30)  /* Enable ABORT switch IRQ	30 */
#define	LBIER_ESYSF		(1 << 29)  /* Enable VMEbus SYSFAIL IRQ	29 */
#define	LBIER_EMWP		(1 << 28)  /* Enable VMEbus WPE IRQ	28 */
#define	LBIER_EPE		(1 << 27)  /* Enable Parity Error IRQ	27 */
#define	LBIER_EVI1E		(1 << 26)  /* Enable VMEbus IRQ1 IRQ	26 */
#define	LBIER_ETIC2		(1 << 25)  /* Enable Tick Timer 2 IRQ	25 */
#define	LBIER_ETIC1		(1 << 24)  /* Enable Tick Timer 1 IRQ	24 */

#define	LBIER_EVIA		(1 << 23)  /* Enable VME Intptr Ack IRQ	23 */
#define	LBIER_EDMA		(1 << 22)  /* Enable DMAC IRQ		22 */
#define	LBIER_ESIG3		(1 << 21)  /* Enable GCSR SIG3 IRQ	21 */
#define	LBIER_ESIG2		(1 << 20)  /* Enable GCSR SIG2 IRQ	20 */
#define	LBIER_ESIG1		(1 << 19)  /* Enable GCSR SIG1 IRQ	19 */
#define	LBIER_ESIG0		(1 << 18)  /* Enable GCSR SIG0 IRQ	18 */
#define	LBIER_ELM1		(1 << 17)  /* Enable GCSR LM1 IRQ	17 */
#define	LBIER_ELM0		(1 << 16)  /* Enable GCSR LM0 IRQ	16 */

#define	LBIER_ESW7		(1 << 15)  /* Enable Software 7 Inter	15 */
#define	LBIER_ESW6		(1 << 14)  /* Enable Software 6 Inter	14 */
#define	LBIER_ESW5		(1 << 13)  /* Enable Software 5 Inter	13 */
#define	LBIER_ESW4		(1 << 12)  /* Enable Software 4 Inter	12 */
#define	LBIER_ESW3		(1 << 11)  /* Enable Software 3 Inter	11 */
#define	LBIER_ESW2		(1 << 10)  /* Enable Software 2 Inter	10 */
#define	LBIER_ESW1		(1 << 9)   /* Enable Software 1 Inter	 9 */
#define	LBIER_ESW0		(1 << 8)   /* Enable Software 0 Inter	 8 */

#define	LBIER_EIRQ7		(1 << 6)   /* Enable VMEbus IRQ7 Inter	 6 */
#define	LBIER_EIRQ6		(1 << 5)   /* Enable VMEbus IRQ6 Inter	 5 */
#define	LBIER_EIRQ5		(1 << 4)   /* Enable VMEbus IRQ5 Inter	 4 */
#define	LBIER_EIRQ4		(1 << 3)   /* Enable VMEbus IRQ4 Inter	 3 */
#define	LBIER_EIRQ3		(1 << 2)   /* Enable VMEbus IRQ3 Inter	 2 */
#define	LBIER_EIRQ2		(1 << 1)   /* Enable VMEbus IRQ2 Inter	 1 */
#define	LBIER_EIRQ1		1	   /* Enable VMEbus IRQ1 Inter	 0 */

/************************************************************************
 * Software Interrupt Set Register			0x70	15-08	*
 ************************************************************************/

#define	SISR_SSW7		(1 << 15)  /* Set Software 7 Interrupt	15 */
#define	SISR_SSW6		(1 << 14)  /* Set Software 6 Interrupt	14 */
#define	SISR_SSW5		(1 << 13)  /* Set Software 5 Interrupt	13 */
#define	SISR_SSW4		(1 << 12)  /* Set Software 4 Interrupt	12 */
#define	SISR_SSW3		(1 << 11)  /* Set Software 3 Interrupt	11 */
#define	SISR_SSW2		(1 << 10)  /* Set Software 2 Interrupt	10 */
#define	SISR_SSW1		(1 << 9)   /* Set Software 1 Interrupt	 9 */
#define	SISR_SSW0		(1 << 8)   /* Set Software 0 Interrupt	 8 */

/************************************************************************
 * Interrupt Clear Register				0x74	31-08	*
 ************************************************************************/

#define	ICLR_CACF		(1 << 31)  /* Clr VMEbus ACFAIL Inter	31 */
#define	ICLR_CAB		(1 << 30)  /* Clr ABORT Switch Inter	30 */
#define	ICLR_CSYSF		(1 << 29)  /* Clr VMEbus SYSFAIL Inter	29 */
#define	ICLR_CMWP		(1 << 28)  /* Clr VME Master Write Post	28 */
#define	ICLR_CPE		(1 << 27)  /* Clr Ext Inter (PARITY)	27 */
#define	ICLR_CVI1E		(1 << 26)  /* Clr VME IRQ1 Edge Inter	26 */
#define	ICLR_CTIC2		(1 << 25)  /* Clr Tick Timer 2 Inter	25 */
#define	ICLR_CTIC1		(1 << 24)  /* Clr Tick Timer 1 Inter	24 */

#define	ICLR_CVIA		(1 << 23)  /* Clr VMEbus Interrupt Ack	23 */
#define	ICLR_CDMA		(1 << 22)  /* Clr DMA Controller Inter	22 */
#define	ICLR_CSIG3		(1 << 21)  /* Clr GCSR SIG3 Interrupt	21 */
#define	ICLR_CSIG2		(1 << 20)  /* Clr GCSR SIG2 Interrupt	20 */
#define	ICLR_CSIG1		(1 << 19)  /* Clr GCSR SIG1 Interrupt	19 */
#define	ICLR_CSIG0		(1 << 18)  /* Clr GCSR SIG0 Interrupt	18 */
#define	ICLR_CLM1		(1 << 17)  /* Clr GCSR LM1 Interrupt	17 */
#define	ICLR_CLM0		(1 << 16)  /* Clr GCSR LM0 Interrupt	16 */

#define	ICLR_CSW7		(1 << 15)  /* Clr Software 7 Interrupt	15 */
#define	ICLR_CSW6		(1 << 14)  /* Clr Software 6 Interrupt	14 */
#define	ICLR_CSW5		(1 << 13)  /* Clr Software 5 Interrupt	13 */
#define	ICLR_CSW4		(1 << 12)  /* Clr Software 4 Interrupt	12 */
#define	ICLR_CSW3		(1 << 11)  /* Clr Software 3 Interrupt	11 */
#define	ICLR_CSW2		(1 << 10)  /* Clr Software 2 Interrupt	10 */
#define	ICLR_CSW1		(1 << 9)   /* Clr Software 1 Interrupt	 9 */
#define	ICLR_CSW0		(1 << 8)   /* Clr Software 0 Interrupt	 8 */

/************************************************************************
 * Interrupt Level Register 1				0x78	31-00	*
 ************************************************************************/

#define	ILR1_ACF_LEVEL1		(1 << 28)  /* ACFAIL Inter on Level 1	30-28 */
#define	ILR1_ACF_LEVEL2		(2 << 28)  /* ACFAIL Inter on Level 2	30-28 */
#define	ILR1_ACF_LEVEL3		(3 << 28)  /* ACFAIL Inter on Level 3	30-28 */
#define	ILR1_ACF_LEVEL4		(4 << 28)  /* ACFAIL Inter on Level 4	30-28 */
#define	ILR1_ACF_LEVEL5		(5 << 28)  /* ACFAIL Inter on Level 5	30-28 */
#define	ILR1_ACF_LEVEL6		(6 << 28)  /* ACFAIL Inter on Level 6	30-28 */
#define	ILR1_ACF_LEVEL7		(7 << 28)  /* ACFAIL Inter on Level 7	30-28 */

#define ILR1_AB_LEVEL1		(1 << 24)  /* ABORT Inter on Level 1	26-24 */
#define ILR1_AB_LEVEL2		(2 << 24)  /* ABORT Inter on Level 2	26-24 */
#define ILR1_AB_LEVEL3		(3 << 24)  /* ABORT Inter on Level 3	26-24 */
#define ILR1_AB_LEVEL4		(4 << 24)  /* ABORT Inter on Level 4	26-24 */
#define ILR1_AB_LEVEL5		(5 << 24)  /* ABORT Inter on Level 5	26-24 */
#define ILR1_AB_LEVEL6		(6 << 24)  /* ABORT Inter on Level 6	26-24 */
#define ILR1_AB_LEVEL7		(7 << 24)  /* ABORT Inter on Level 7	26-24 */

#define	ILR1_SYSF_LEVEL1	(1 << 20)  /* SYSFAIL Inter on Lvl 1	22-20 */
#define	ILR1_SYSF_LEVEL2	(2 << 20)  /* SYSFAIL Inter on Lvl 2	22-20 */
#define	ILR1_SYSF_LEVEL3	(3 << 20)  /* SYSFAIL Inter on Lvl 3	22-20 */
#define	ILR1_SYSF_LEVEL4	(4 << 20)  /* SYSFAIL Inter on Lvl 4	22-20 */
#define	ILR1_SYSF_LEVEL5	(5 << 20)  /* SYSFAIL Inter on Lvl 5	22-20 */
#define	ILR1_SYSF_LEVEL6	(6 << 20)  /* SYSFAIL Inter on Lvl 6	22-20 */
#define	ILR1_SYSF_LEVEL7	(7 << 20)  /* SYSFAIL Inter on Lvl 7	22-20 */

#define	ILR1_WPE_LEVEL1		(1 << 16)  /* Master Write Post Lvl 1	18-16 */
#define	ILR1_WPE_LEVEL2		(2 << 16)  /* Master Write Post Lvl 2	18-16 */
#define	ILR1_WPE_LEVEL3		(3 << 16)  /* Master Write Post Lvl 3	18-16 */
#define	ILR1_WPE_LEVEL4		(4 << 16)  /* Master Write Post Lvl 4	18-16 */
#define	ILR1_WPE_LEVEL5		(5 << 16)  /* Master Write Post Lvl 5	18-16 */
#define	ILR1_WPE_LEVEL6		(6 << 16)  /* Master Write Post Lvl 6	18-16 */
#define	ILR1_WPE_LEVEL7		(7 << 16)  /* Master Write Post Lvl 7	18-16 */

#define	ILR1_PE_LEVEL1		(1 << 12)  /* Parity Error on Level 1	14-12 */
#define	ILR1_PE_LEVEL2		(2 << 12)  /* Parity Error on Level 2	14-12 */
#define	ILR1_PE_LEVEL3		(3 << 12)  /* Parity Error on Level 3	14-12 */
#define	ILR1_PE_LEVEL4		(4 << 12)  /* Parity Error on Level 4	14-12 */
#define	ILR1_PE_LEVEL5		(5 << 12)  /* Parity Error on Level 5	14-12 */
#define	ILR1_PE_LEVEL6		(6 << 12)  /* Parity Error on Level 6	14-12 */
#define	ILR1_PE_LEVEL7		(7 << 12)  /* Parity Error on Level 7	14-12 */

#define	ILR1_IRQ1E_LEVEL1	(1 << 8)   /* VMEbus IRQ1 edge on Lvl 1	10-8 */
#define	ILR1_IRQ1E_LEVEL2	(2 << 8)   /* VMEbus IRQ1 edge on Lvl 2	10-8 */
#define	ILR1_IRQ1E_LEVEL3	(3 << 8)   /* VMEbus IRQ1 edge on Lvl 3	10-8 */
#define	ILR1_IRQ1E_LEVEL4	(4 << 8)   /* VMEbus IRQ1 edge on Lvl 4	10-8 */
#define	ILR1_IRQ1E_LEVEL5	(5 << 8)   /* VMEbus IRQ1 edge on Lvl 5	10-8 */
#define	ILR1_IRQ1E_LEVEL6	(6 << 8)   /* VMEbus IRQ1 edge on Lvl 6	10-8 */
#define	ILR1_IRQ1E_LEVEL7	(7 << 8)   /* VMEbus IRQ1 edge on Lvl 7	10-8 */

#define	ILR1_TICK2_LEVEL1	(1 << 4)   /* Tick Timer 2 Inter Lvl 1	 6-4 */
#define	ILR1_TICK2_LEVEL2	(2 << 4)   /* Tick Timer 2 Inter Lvl 2	 6-4 */
#define	ILR1_TICK2_LEVEL3	(3 << 4)   /* Tick Timer 2 Inter Lvl 3	 6-4 */
#define	ILR1_TICK2_LEVEL4	(4 << 4)   /* Tick Timer 2 Inter Lvl 4	 6-4 */
#define	ILR1_TICK2_LEVEL5	(5 << 4)   /* Tick Timer 2 Inter Lvl 5	 6-4 */
#define	ILR1_TICK2_LEVEL6	(6 << 4)   /* Tick Timer 2 Inter Lvl 6	 6-4 */
#define	ILR1_TICK2_LEVEL7	(7 << 4)   /* Tick Timer 2 Inter Lvl 7	 6-4 */

#define	ILR1_TICK1_LEVEL1	1	   /* Tick Timer 1 Inter Lvl 1	 6-4 */
#define	ILR1_TICK1_LEVEL2	2	   /* Tick Timer 1 Inter Lvl 2	 6-4 */
#define	ILR1_TICK1_LEVEL3	3	   /* Tick Timer 1 Inter Lvl 3	 6-4 */
#define	ILR1_TICK1_LEVEL4	4	   /* Tick Timer 1 Inter Lvl 4	 6-4 */
#define	ILR1_TICK1_LEVEL5	5	   /* Tick Timer 1 Inter Lvl 5	 6-4 */
#define	ILR1_TICK1_LEVEL6	6	   /* Tick Timer 1 Inter Lvl 6	 6-4 */
#define	ILR1_TICK1_LEVEL7	7	   /* Tick Timer 1 Inter Lvl 7	 6-4 */

/************************************************************************
 * Interrupt Level Register 2				0x7c	31-00	*
 ************************************************************************/

#define	ILR2_VIA_LEVEL1		(1 << 28)  /* VMEbus Intrptr Ack Lvl 1	30-28 */
#define	ILR2_VIA_LEVEL2		(2 << 28)  /* VMEbus Intrptr Ack Lvl 2	30-28 */
#define	ILR2_VIA_LEVEL3		(3 << 28)  /* VMEbus Intrptr Ack Lvl 3	30-28 */
#define	ILR2_VIA_LEVEL4		(4 << 28)  /* VMEbus Intrptr Ack Lvl 4	30-28 */
#define	ILR2_VIA_LEVEL5		(5 << 28)  /* VMEbus Intrptr Ack Lvl 5	30-28 */
#define	ILR2_VIA_LEVEL6		(6 << 28)  /* VMEbus Intrptr Ack Lvl 6	30-28 */
#define	ILR2_VIA_LEVEL7		(7 << 28)  /* VMEbus Intrptr Ack Lvl 7	30-28 */

#define	ILR2_DMA_LEVEL1		(1 << 24)  /* DMA Controller Int Lvl 1	26-24 */
#define	ILR2_DMA_LEVEL2		(2 << 24)  /* DMA Controller Int Lvl 2	26-24 */
#define	ILR2_DMA_LEVEL3		(3 << 24)  /* DMA Controller Int Lvl 3	26-24 */
#define	ILR2_DMA_LEVEL4		(4 << 24)  /* DMA Controller Int Lvl 4	26-24 */
#define	ILR2_DMA_LEVEL5		(5 << 24)  /* DMA Controller Int Lvl 5	26-24 */
#define	ILR2_DMA_LEVEL6		(6 << 24)  /* DMA Controller Int Lvl 6	26-24 */
#define	ILR2_DMA_LEVEL7		(7 << 24)  /* DMA Controller Int Lvl 7	26-24 */

#define	ILR2_SIG3_LEVEL1	(1 << 20)  /* GCSR SIG3 Int on Level 1	22-20 */
#define	ILR2_SIG3_LEVEL2	(2 << 20)  /* GCSR SIG3 Int on Level 2	22-20 */
#define	ILR2_SIG3_LEVEL3	(3 << 20)  /* GCSR SIG3 Int on Level 3	22-20 */
#define	ILR2_SIG3_LEVEL4	(4 << 20)  /* GCSR SIG3 Int on Level 4	22-20 */
#define	ILR2_SIG3_LEVEL5	(5 << 20)  /* GCSR SIG3 Int on Level 5	22-20 */
#define	ILR2_SIG3_LEVEL6	(6 << 20)  /* GCSR SIG3 Int on Level 6	22-20 */
#define	ILR2_SIG3_LEVEL7	(7 << 20)  /* GCSR SIG3 Int on Level 7	22-20 */

#define	ILR2_SIG2_LEVEL1	(1 << 16)  /* GCSR SIG2 Int on Level 1	18-16 */
#define	ILR2_SIG2_LEVEL2	(2 << 16)  /* GCSR SIG2 Int on Level 2	18-16 */
#define	ILR2_SIG2_LEVEL3	(3 << 16)  /* GCSR SIG2 Int on Level 3	18-16 */
#define	ILR2_SIG2_LEVEL4	(4 << 16)  /* GCSR SIG2 Int on Level 4	18-16 */
#define	ILR2_SIG2_LEVEL5	(5 << 16)  /* GCSR SIG2 Int on Level 5	18-16 */
#define	ILR2_SIG2_LEVEL6	(6 << 16)  /* GCSR SIG2 Int on Level 6	18-16 */
#define	ILR2_SIG2_LEVEL7	(7 << 16)  /* GCSR SIG2 Int on Level 7	18-16 */

#define	ILR2_SIG1_LEVEL1	(1 << 12)  /* GCSR SIG1 Int on Level 1	14-12 */
#define	ILR2_SIG1_LEVEL2	(2 << 12)  /* GCSR SIG1 Int on Level 2	14-12 */
#define	ILR2_SIG1_LEVEL3	(3 << 12)  /* GCSR SIG1 Int on Level 3	14-12 */
#define	ILR2_SIG1_LEVEL4	(4 << 12)  /* GCSR SIG1 Int on Level 4	14-12 */
#define	ILR2_SIG1_LEVEL5	(5 << 12)  /* GCSR SIG1 Int on Level 5	14-12 */
#define	ILR2_SIG1_LEVEL6	(6 << 12)  /* GCSR SIG1 Int on Level 6	14-12 */
#define	ILR2_SIG1_LEVEL7	(7 << 12)  /* GCSR SIG1 Int on Level 7	14-12 */

#define	ILR2_SIG0_LEVEL1	(1 << 8)   /* GCSR SIG0 Int on Level 1	10-8 */
#define	ILR2_SIG0_LEVEL2	(2 << 8)   /* GCSR SIG0 Int on Level 2	10-8 */
#define	ILR2_SIG0_LEVEL3	(3 << 8)   /* GCSR SIG0 Int on Level 3	10-8 */
#define	ILR2_SIG0_LEVEL4	(4 << 8)   /* GCSR SIG0 Int on Level 4	10-8 */
#define	ILR2_SIG0_LEVEL5	(5 << 8)   /* GCSR SIG0 Int on Level 5	10-8 */
#define	ILR2_SIG0_LEVEL6	(6 << 8)   /* GCSR SIG0 Int on Level 6	10-8 */
#define	ILR2_SIG0_LEVEL7	(7 << 8)   /* GCSR SIG0 Int on Level 7	10-8 */

#define	ILR2_LM1_LEVEL1		(1 << 4)   /* GCSR LM1 Int on Level 1	6-4 */
#define	ILR2_LM1_LEVEL2		(2 << 4)   /* GCSR LM1 Int on Level 2	6-4 */
#define	ILR2_LM1_LEVEL3		(3 << 4)   /* GCSR LM1 Int on Level 3	6-4 */
#define	ILR2_LM1_LEVEL4		(4 << 4)   /* GCSR LM1 Int on Level 4	6-4 */
#define	ILR2_LM1_LEVEL5		(5 << 4)   /* GCSR LM1 Int on Level 5	6-4 */
#define	ILR2_LM1_LEVEL6		(6 << 4)   /* GCSR LM1 Int on Level 6	6-4 */
#define	ILR2_LM1_LEVEL7		(7 << 4)   /* GCSR LM1 Int on Level 7	6-4 */

#define	ILR2_LM0_LEVEL1		1	   /* GCSR LM0 Int on Level 1	2-0 */
#define	ILR2_LM0_LEVEL2		2	   /* GCSR LM0 Int on Level 2	2-0 */
#define	ILR2_LM0_LEVEL3		3	   /* GCSR LM0 Int on Level 3	2-0 */
#define	ILR2_LM0_LEVEL4		4	   /* GCSR LM0 Int on Level 4	2-0 */
#define	ILR2_LM0_LEVEL5		5	   /* GCSR LM0 Int on Level 5	2-0 */
#define	ILR2_LM0_LEVEL6		6	   /* GCSR LM0 Int on Level 6	2-0 */
#define	ILR2_LM0_LEVEL7		7	   /* GCSR LM0 Int on Level 7	2-0 */

/************************************************************************
 * Interrupt Level Register 3				0x80	31-00	*
 ************************************************************************/

#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */
#define	ILR3_SW7_LEVEL7		(7 << 28)  /* Software Inter 7 on Lvl 7	30-28 */

#define	ILR3_SW6_LEVEL1		(1 << 24)  /* Software Inter 6 on Lvl 1	26-24 */
#define	ILR3_SW6_LEVEL2		(2 << 24)  /* Software Inter 6 on Lvl 2	26-24 */
#define	ILR3_SW6_LEVEL3		(3 << 24)  /* Software Inter 6 on Lvl 3	26-24 */
#define	ILR3_SW6_LEVEL4		(4 << 24)  /* Software Inter 6 on Lvl 4	26-24 */
#define	ILR3_SW6_LEVEL5		(5 << 24)  /* Software Inter 6 on Lvl 5	26-24 */
#define	ILR3_SW6_LEVEL6		(6 << 24)  /* Software Inter 6 on Lvl 6	26-24 */
#define	ILR3_SW6_LEVEL7		(7 << 24)  /* Software Inter 6 on Lvl 7	26-24 */

#define	ILR3_SW5_LEVEL1		(1 << 20)  /* Software Inter 5 on Lvl 1	22-20 */
#define	ILR3_SW5_LEVEL2		(2 << 20)  /* Software Inter 5 on Lvl 2	22-20 */
#define	ILR3_SW5_LEVEL3		(3 << 20)  /* Software Inter 5 on Lvl 3	22-20 */
#define	ILR3_SW5_LEVEL4		(4 << 20)  /* Software Inter 5 on Lvl 4	22-20 */
#define	ILR3_SW5_LEVEL5		(5 << 20)  /* Software Inter 5 on Lvl 5	22-20 */
#define	ILR3_SW5_LEVEL6		(6 << 20)  /* Software Inter 5 on Lvl 6	22-20 */
#define	ILR3_SW5_LEVEL7		(7 << 20)  /* Software Inter 5 on Lvl 7	22-20 */

#define	ILR3_SW4_LEVEL1		(1 << 16)  /* Software Inter 4 on Lvl 1	18-16 */
#define	ILR3_SW4_LEVEL2		(2 << 16)  /* Software Inter 4 on Lvl 2	18-16 */
#define	ILR3_SW4_LEVEL3		(3 << 16)  /* Software Inter 4 on Lvl 3	18-16 */
#define	ILR3_SW4_LEVEL4		(4 << 16)  /* Software Inter 4 on Lvl 4	18-16 */
#define	ILR3_SW4_LEVEL5		(5 << 16)  /* Software Inter 4 on Lvl 5	18-16 */
#define	ILR3_SW4_LEVEL6		(6 << 16)  /* Software Inter 4 on Lvl 6	18-16 */
#define	ILR3_SW4_LEVEL7		(7 << 16)  /* Software Inter 4 on Lvl 7	18-16 */

#define	ILR3_SW3_LEVEL1		(1 << 12)  /* Software Inter 3 on Lvl 1	14-12 */
#define	ILR3_SW3_LEVEL2		(2 << 12)  /* Software Inter 3 on Lvl 2	14-12 */
#define	ILR3_SW3_LEVEL3		(3 << 12)  /* Software Inter 3 on Lvl 3	14-12 */
#define	ILR3_SW3_LEVEL4		(4 << 12)  /* Software Inter 3 on Lvl 4	14-12 */
#define	ILR3_SW3_LEVEL5		(5 << 12)  /* Software Inter 3 on Lvl 5	14-12 */
#define	ILR3_SW3_LEVEL6		(6 << 12)  /* Software Inter 3 on Lvl 6	14-12 */
#define	ILR3_SW3_LEVEL7		(7 << 12)  /* Software Inter 3 on Lvl 7	14-12 */

#define	ILR3_SW2_LEVEL1		(1 << 8)   /* Software Inter 2 on Lvl 1	10-8 */
#define	ILR3_SW2_LEVEL2		(2 << 8)   /* Software Inter 2 on Lvl 2	10-8 */
#define	ILR3_SW2_LEVEL3		(3 << 8)   /* Software Inter 2 on Lvl 3	10-8 */
#define	ILR3_SW2_LEVEL4		(4 << 8)   /* Software Inter 2 on Lvl 4	10-8 */
#define	ILR3_SW2_LEVEL5		(5 << 8)   /* Software Inter 2 on Lvl 5	10-8 */
#define	ILR3_SW2_LEVEL6		(6 << 8)   /* Software Inter 2 on Lvl 6	10-8 */
#define	ILR3_SW2_LEVEL7		(7 << 8)   /* Software Inter 2 on Lvl 7	10-8 */

#define	ILR3_SW1_LEVEL1		(1 << 4)   /* Software Inter 1 on Lvl 1	6-4 */
#define	ILR3_SW1_LEVEL2		(2 << 4)   /* Software Inter 1 on Lvl 2	6-4 */
#define	ILR3_SW1_LEVEL3		(3 << 4)   /* Software Inter 1 on Lvl 3	6-4 */
#define	ILR3_SW1_LEVEL4		(4 << 4)   /* Software Inter 1 on Lvl 4	6-4 */
#define	ILR3_SW1_LEVEL5		(5 << 4)   /* Software Inter 1 on Lvl 5	6-4 */
#define	ILR3_SW1_LEVEL6		(6 << 4)   /* Software Inter 1 on Lvl 6	6-4 */
#define	ILR3_SW1_LEVEL7		(7 << 4)   /* Software Inter 1 on Lvl 7	6-4 */

#define	ILR3_SW0_LEVEL1		1	   /* Software Inter 0 on Lvl 1	2-0 */
#define	ILR3_SW0_LEVEL2		2	   /* Software Inter 0 on Lvl 2	2-0 */
#define	ILR3_SW0_LEVEL3		3	   /* Software Inter 0 on Lvl 3	2-0 */
#define	ILR3_SW0_LEVEL4		4	   /* Software Inter 0 on Lvl 4	2-0 */
#define	ILR3_SW0_LEVEL5		5	   /* Software Inter 0 on Lvl 5	2-0 */
#define	ILR3_SW0_LEVEL6		6	   /* Software Inter 0 on Lvl 6	2-0 */
#define	ILR3_SW0_LEVEL7		7	   /* Software Inter 0 on Lvl 7	2-0 */

/************************************************************************
 * Interrupt Level Register 4				0x84	31-00	*
 ************************************************************************/

#define	ILR4_VIRQ7_LEVEL1	(1 << 24)  /* VMEbus IRQ7 on Level 1	26-24 */
#define	ILR4_VIRQ7_LEVEL2	(2 << 24)  /* VMEbus IRQ7 on Level 2	26-24 */
#define	ILR4_VIRQ7_LEVEL3	(3 << 24)  /* VMEbus IRQ7 on Level 3	26-24 */
#define	ILR4_VIRQ7_LEVEL4	(4 << 24)  /* VMEbus IRQ7 on Level 4	26-24 */
#define	ILR4_VIRQ7_LEVEL5	(5 << 24)  /* VMEbus IRQ7 on Level 5	26-24 */
#define	ILR4_VIRQ7_LEVEL6	(6 << 24)  /* VMEbus IRQ7 on Level 6	26-24 */
#define	ILR4_VIRQ7_LEVEL7	(7 << 24)  /* VMEbus IRQ7 on Level 7	26-24 */

#define	ILR4_VIRQ6_LEVEL1	(1 << 20)  /* VMEbus IRQ6 on Level 1	22-20 */
#define	ILR4_VIRQ6_LEVEL2	(2 << 20)  /* VMEbus IRQ6 on Level 2	22-20 */
#define	ILR4_VIRQ6_LEVEL3	(3 << 20)  /* VMEbus IRQ6 on Level 3	22-20 */
#define	ILR4_VIRQ6_LEVEL4	(4 << 20)  /* VMEbus IRQ6 on Level 4	22-20 */
#define	ILR4_VIRQ6_LEVEL5	(5 << 20)  /* VMEbus IRQ6 on Level 5	22-20 */
#define	ILR4_VIRQ6_LEVEL6	(6 << 20)  /* VMEbus IRQ6 on Level 6	22-20 */
#define	ILR4_VIRQ6_LEVEL7	(7 << 20)  /* VMEbus IRQ6 on Level 7	22-20 */

#define	ILR4_VIRQ5_LEVEL1	(1 << 16)  /* VMEbus IRQ5 on Level 1	18-16 */
#define	ILR4_VIRQ5_LEVEL2	(2 << 16)  /* VMEbus IRQ5 on Level 2	18-16 */
#define	ILR4_VIRQ5_LEVEL3	(3 << 16)  /* VMEbus IRQ5 on Level 3	18-16 */
#define	ILR4_VIRQ5_LEVEL4	(4 << 16)  /* VMEbus IRQ5 on Level 4	18-16 */
#define	ILR4_VIRQ5_LEVEL5	(5 << 16)  /* VMEbus IRQ5 on Level 5	18-16 */
#define	ILR4_VIRQ5_LEVEL6	(6 << 16)  /* VMEbus IRQ5 on Level 6	18-16 */
#define	ILR4_VIRQ5_LEVEL7	(7 << 16)  /* VMEbus IRQ5 on Level 7	18-16 */

#define	ILR4_VIRQ4_LEVEL1	(1 << 12)  /* VMEbus IRQ4 on Level 1	14-12 */
#define	ILR4_VIRQ4_LEVEL2	(2 << 12)  /* VMEbus IRQ4 on Level 2	14-12 */
#define	ILR4_VIRQ4_LEVEL3	(3 << 12)  /* VMEbus IRQ4 on Level 3	14-12 */
#define	ILR4_VIRQ4_LEVEL4	(4 << 12)  /* VMEbus IRQ4 on Level 4	14-12 */
#define	ILR4_VIRQ4_LEVEL5	(5 << 12)  /* VMEbus IRQ4 on Level 5	14-12 */
#define	ILR4_VIRQ4_LEVEL6	(6 << 12)  /* VMEbus IRQ4 on Level 6	14-12 */
#define	ILR4_VIRQ4_LEVEL7	(7 << 12)  /* VMEbus IRQ4 on Level 7	14-12 */

#define	ILR4_VIRQ3_LEVEL1	(1 << 8)   /* VMEbus IRQ3 on Level 1	10-8 */
#define	ILR4_VIRQ3_LEVEL2	(2 << 8)   /* VMEbus IRQ3 on Level 2	10-8 */
#define	ILR4_VIRQ3_LEVEL3	(3 << 8)   /* VMEbus IRQ3 on Level 3	10-8 */
#define	ILR4_VIRQ3_LEVEL4	(4 << 8)   /* VMEbus IRQ3 on Level 4	10-8 */
#define	ILR4_VIRQ3_LEVEL5	(5 << 8)   /* VMEbus IRQ3 on Level 5	10-8 */
#define	ILR4_VIRQ3_LEVEL6	(6 << 8)   /* VMEbus IRQ3 on Level 6	10-8 */
#define	ILR4_VIRQ3_LEVEL7	(7 << 8)   /* VMEbus IRQ3 on Level 7	10-8 */

#define	ILR4_VIRQ2_LEVEL1	(1 << 4)   /* VMEbus IRQ2 on Level 1	6-4 */
#define	ILR4_VIRQ2_LEVEL2	(2 << 4)   /* VMEbus IRQ2 on Level 2	6-4 */
#define	ILR4_VIRQ2_LEVEL3	(3 << 4)   /* VMEbus IRQ2 on Level 3	6-4 */
#define	ILR4_VIRQ2_LEVEL4	(4 << 4)   /* VMEbus IRQ2 on Level 4	6-4 */
#define	ILR4_VIRQ2_LEVEL5	(5 << 4)   /* VMEbus IRQ2 on Level 5	6-4 */
#define	ILR4_VIRQ2_LEVEL6	(6 << 4)   /* VMEbus IRQ2 on Level 6	6-4 */
#define	ILR4_VIRQ2_LEVEL7	(7 << 4)   /* VMEbus IRQ2 on Level 7	6-4 */

#define	ILR4_VIRQ1_LEVEL1	1	   /* VMEbus IRQ1 on Level 1	2-0 */
#define	ILR4_VIRQ1_LEVEL2	2	   /* VMEbus IRQ1 on Level 2	2-0 */
#define	ILR4_VIRQ1_LEVEL3	3	   /* VMEbus IRQ1 on Level 3	2-0 */
#define	ILR4_VIRQ1_LEVEL4	4	   /* VMEbus IRQ1 on Level 4	2-0 */
#define	ILR4_VIRQ1_LEVEL5	5	   /* VMEbus IRQ1 on Level 5	2-0 */
#define	ILR4_VIRQ1_LEVEL6	6	   /* VMEbus IRQ1 on Level 6	2-0 */
#define	ILR4_VIRQ1_LEVEL7	7	   /* VMEbus IRQ1 on Level 7	2-0 */

/************************************************************************
 * Vector Base Register					0x88	31-24	*
 * I/O Control Register 1				0x88	23-16	*
 * I/O Control Register 2				0x88	15-08	*
 * I/O Control Register 3				0x88	07-00	*
 ************************************************************************/

#define	IOCR_MEIN		(1 << 23)  /* Enable Interrupts		23 */
#define	IOCR_SYSFL		(1 << 22)  /* SYSFAIL VMEbus Status	22 */
#define	IOCR_ACFL		(1 << 21)  /* ACFAIL VMEbus Status	21 */
#define	IOCR_ABRTL		(1 << 20)  /* ABORT Switch Status	20 */
#define	IOCR_GPOEN3		(1 << 19)  /* GPIO3 Set as Output	19 */
#define	IOCR_GPIEN3		0x00	   /* GPIO3 Set as Input	19 */
#define	IOCR_GPOEN2		(1 << 18)  /* GPIO2 Set as Output	18 */
#define	IOCR_GPIEN2		0x00	   /* GPIO2 Set as Input	18 */
#define	IOCR_GPOEN1		(1 << 17)  /* GPIO1 Set as Output	17 */
#define	IOCR_GPIEN1		0x00	   /* GPIO1 Set as Input	17 */
#define	IOCR_GPOEN0		(1 << 16)  /* GPIO0 Set as Output	16 */
#define	IOCR_GPIEN0		0x00	   /* GPIO0 Set as Input	16 */

#define	IOCR_GPIOO3_HIGH	(1 << 15)  /* GPIO3 set to HIGH		15 */
#define	IOCR_GPIOO3_LOW		0x00	   /* GPIO3 set to LOW		15 */
#define	IOCR_GPIOO2_HIGH	(1 << 14)  /* GPIO2 set to HIGH		14 */
#define	IOCR_GPIOO2_LOW		0x00	   /* GPIO2 set to LOW		14 */
#define	IOCR_GPIOO1_HIGH	(1 << 13)  /* GPIO1 set to HIGH		13 */
#define	IOCR_GPIOO1_LOW		0x00	   /* GPIO1 set to LOW		13 */
#define	IOCR_GPIOO0_HIGH	(1 << 12)  /* GPIO0 set to HIGH		12 */
#define	IOCR_GPIOO0_LOW		0x00	   /* GPIO0 set to LOW		12 */
#define	IOCR_GPIOI3		(1 << 11)  /* GPIO3 Status		11 */
#define	IOCR_GPIOI2		(1 << 10)  /* GPIO2 Status		10 */
#define	IOCR_GPIOI1		(1 << 9)   /* GPIO1 Status		 9 */
#define	IOCR_GPIOI0		(1 << 8)   /* GPIO0 Status		 8 */

#define	IOCR_GPI7		(1 << 7)   /* GPI7 Status		 7 */
#define	IOCR_GPI6		(1 << 6)   /* GPI6 Status		 6 */
#define	IOCR_GPI5		(1 << 5)   /* GPI5 Status		 5 */
#define	IOCR_GPI4		(1 << 4)   /* GPI4 Status		 4 */
#define	IOCR_GPI3		(1 << 3)   /* GPI3 Status		 3 */
#define	IOCR_GPI2		(1 << 2)   /* GPI2 Status		 2 */
#define	IOCR_GPI1		(1 << 1)   /* GPI1 Status		 1 */
#define	IOCR_GPI0		1	   /* GPI0 Status		 0 */

/**********************************************************************
 * Miscellaneous Control Register			0x8c	07-00 *
 **********************************************************************/

#define	MISCCR_DISSRAM		(1 << 5)   /* Disable SRAM decoder       5 */

/****************************************************************
 * VMEchip2 LM/SIG Register				0x06	*
 ****************************************************************/

#define	LM_SIG_LM3		0x80	  /* Location Monitor 3		7 */
#define	LM_SIG_LM2		0x40	  /* Location Monitor 2		6 */
#define	LM_SIG_LM1		0x20	  /* Location Monitor 1		5 */
#define	LM_SIG_LM0		0x10	  /* Location Monitor 0		4 */
#define	LM_SIG_SIG3		0x08	  /* SIG3			3 */
#define	LM_SIG_SIG2		0x04	  /* SIG2			2 */
#define	LM_SIG_SIG1		0x02	  /* SIG1			1 */
#define	LM_SIG_SIG0		0x01	  /* SIG0			0 */

/****************************************************************
 * VMEchip2 Board Status/Control Register		0x07	*
 ****************************************************************/

#define	BSCR_RST		0x80	  /* Allow VMEbus master reset	7 */
#define	BSCR_ISF		0x40	  /* Inhibit SYSFAIL to VMEbus	6 */
#define	BSCR_BF			0x20	  /* Board Fail active		5 */
#define	BSCR_SCON		0x10	  /* Board is System Controller	4 */
#define	BSCR_SYSFL		0x08	  /* Board Driving SYSFAIL	3 */


/****************************************************************
 * Local Bus Interrupter Summary (2-65 of '167 manual)		*
 * These are values for the interrupt vector related to various	*
 * interrupts generated locally.				*
 ****************************************************************/

/* These have the upper 4 bits defined by the contents of
 * vector base register 0
 */
#define	LBIV_SOFTWARE0		0x08
#define	LBIV_SOFTWARE1		0x09
#define	LBIV_SOFTWARE2		0x0a
#define	LBIV_SOFTWARE3		0x0b
#define	LBIV_SOFTWARE4		0x0c
#define	LBIV_SOFTWARE5		0x0d
#define	LBIV_SOFTWARE6		0x0e
#define	LBIV_SOFTWARE7		0x0f

/* These have the upper 4 bits defined by the contents of
 * vector base register 1
 */
#define	LBIV_GCSR_LM0		0x00
#define	LBIV_GCSR_LM1		0x01
#define	LBIV_GCSR_SIG0		0x02
#define	LBIV_GCSR_SIG1		0x03
#define	LBIV_GCSR_SIG2		0x04
#define	LBIV_GCSR_SIG3		0x05
#define	LBIV_DMAC		0x06
#define	LBIV_VIA		0x07
#define	LBIV_TT1		0x08
#define	LBIV_TT2		0x09
#define	LBIV_VIRQ1_ES		0x0a
#define	LBIV_PARITY_ERROR	0x0b
#define	LBIV_VMWPE		0x0c
#define	LBIV_VME_SYSFAIL	0x0d
#define	LBIV_ABORT		0x0e
#define	LBIV_VME_ACFAIL		0x0f

#endif	/* INCvmechip2h */
