/* frcEagle02.h - Force EAGLE-02 module header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,20oct92,caf  created.
*/

/*
This file contains the configuration parameters for the Force EAGLE-02 module.
*/


#ifndef INCfrcEagle02h
#define	INCfrcEagle02h

/* defines */

/* interrupt vectors */

#define	INT_VEC_VSB	(INT_VEC_FGA + FGA_INT_LOCAL1)	/* VSB             */
#define	INT_VEC_LANCE	(INT_VEC_FGA + FGA_INT_LOCAL7)	/* LANCE Ethernet  */

/* interrupt levels */

#define	INT_LVL_LANCE		3		/* LANCE Ethernet          */
#define	INT_LVL_VSB		6		/* VSB interrupts          */

/* LANCE Ethernet */

#define	FRC40_LANCE_BASE_ADRS	((char *) 0xfef80000)	/* Am7990 LANCE    */

#define	LN_POOL_ADRS		0xfef00000	/* dedicated memory pool   */
#define	LN_POOL_SIZE		0x10000		/* size of ln memory pool  */
#define	LN_DATA_WIDTH		2		/* word access only        */
#define	LN_PADDING		FALSE		/* no padding for RAP      */
#define	LN_RING_BUF_SIZE	0		/* use default size        */

#undef	DEFAULT_BOOT_LINE
#define	DEFAULT_BOOT_LINE \
"ln(0,0)host:/usr/vw/config/frc40/vxWorks h=90.0.0.3 e=90.0.0.50 u=target"

#define	INCLUDE_LN 			    	/* include LANCE Ethernet  */
#define	IO_ADRS_LN	FRC40_LANCE_BASE_ADRS	/* LANCE I/O address       */
#define	INT_VEC_LN	INT_VEC_LANCE		/* LANCE interrupt vector  */
#define	INT_LVL_LN	INT_LVL_LANCE		/* LANCE interrupt level   */

/* EAGLE-02 defines */

/* what the base address should be set to (note: -1 will keep it useless)  */

#define	FRC40_E002_IOC_BASE_ADRS	0xfec00000
#define	FRC40_E002_FLASH_BASE_ADRS	0xfe800000
#define	FRC40_E002_VSB1_BASE_ADRS	0xfd000000
#define	FRC40_E002_CSR_BASE_ADRS	0xfec00200
#define	FRC40_E002_VSB2_BASE_ADRS	0xffffffff	/* -1              */
#define	FRC40_E002_LANCE_BASE_ADRS	0xfef80000
#define	FRC40_E002_SRAM_BASE_ADRS	LN_POOL_ADRS

/* IOC chip selects */

#define	FRC40_E002_IOCCS_FLASH		0
#define	FRC40_E002_IOCCS_VSB1		1
#define	FRC40_E002_IOCCS_CSR		2
#define	FRC40_E002_IOCCS_VSB2		3
#define	FRC40_E002_IOCCS_SRAM		4
#define	FRC40_E002_IOCCS_LANCE		5
#define	FRC40_E002_IOCCS_UNUSD1		6
#define	FRC40_E002_IOCCS_UNUSD2		7

/* definitions for device sizes as the IOC wants it */

#define	FRC40_E002_VSB1_SIZE		0x00800000	/* 16 mb           */
#define	FRC40_E002_CSR_SIZE		0x00000020	/* 64 bytes        */
#define	FRC40_E002_VSB2_SIZE		0x00000000	/* 0 bytes         */
#define	FRC40_E002_SRAM_SIZE		0x00008000	/* 64 kb           */
#define	FRC40_E002_LANCE_SIZE		0x00000020	/* 64 bytes        */

#endif	/* INCfrcEagle02h */
