/* frcEagle01.h - Force EAGLE-01C module header */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,07jan93,ccc  fixed comment about SCSI_DMA, now works.
01b,22oct92,ccc  added definiton of INCLUDE_SCSI_BOOT, INCLUDE_DOSFS for SCSI.
01a,20oct92,caf  created.
*/

/*
This file contains the configuration parameters for the Force EAGLE-01C module.
*/


#ifndef INCfrcEagle01h
#define	INCfrcEagle01h

/* includes */

#include "drv/scsi/mb87030.h"

/* defines */

/* interrupt vectors */

#define	INT_VEC_FDC	(INT_VEC_FGA + FGA_INT_LOCAL1)	/* FDC              */
#define	INT_VEC_LANCE	(INT_VEC_FGA + FGA_INT_LOCAL6)	/* LANCE ethernet   */
#define	INT_VEC_SCSI	(INT_VEC_FGA + FGA_INT_LOCAL7)	/* SCSI             */
#define	INT_VEC_SCSI_DMA (INT_VEC_FGA + FGA_INT_DMANORM) /* SCSI DMA        */

/* interrupt levels */

#define	INT_LVL_FDC	2			/* floppy disc controller   */
#define	INT_LVL_LANCE	3			/* LANCE ethernet           */
#define	INT_LVL_SCSI	2			/* SCSI                     */

#define	FRC40_FDC_BASE_ADRS     ((char *) 0xff803800) /* wd1772 disk ctlr   */

/* LANCE Ethernet */

#define	FRC40_LANCE_BASE_ADRS	((char *) 0xfef80000) /* Am7990 LANCE       */

#define	LN_POOL_ADRS		0xfef00000	/* dedicated memory pool    */
#define	LN_POOL_SIZE		0x10000		/* size of ln memory pool   */
#define	LN_DATA_WIDTH		2		/* word access only         */
#define	LN_PADDING		FALSE		/* no padding for RAP       */
#define	LN_RING_BUF_SIZE	0		/* use default size         */

#undef	DEFAULT_BOOT_LINE
#define	DEFAULT_BOOT_LINE \
"ln(0,0)host:/usr/vw/config/frc40/vxWorks h=90.0.0.3 e=90.0.0.50 u=target"

#define	INCLUDE_LN 			    	/* include LANCE Ethernet   */
#define	IO_ADRS_LN	FRC40_LANCE_BASE_ADRS	/* LANCE I/O address        */
#define	INT_VEC_LN	INT_VEC_LANCE		/* LANCE interrupt vector   */
#define	INT_LVL_LN	INT_LVL_LANCE		/* LANCE interrupt level    */

/* PIT */

#define	FRC_PIT1_READ		0x40		/* 1=From SCSI, 0=To SCSI   */
#define	FRC_PIT1_FLOPPY		0x80		/* 1=Floppy, 0=SCSI access  */

/* SCSI */

#define	FRC40_SCSI_DMA_CR       ((char *) 0xff802c00)   /* SCSI DMA control */
#define	FRC_SCSI_DMA_CR         FRC40_SCSI_DMA_CR
#define	FRC_SCSI_DMA_READ	0x01
#define	FRC40_SPC_BASE_ADRS	((UINT8 *) 0xff803400)	/* base ads         */
#define	FRC40_SPC_REG_OFFSET	1			/* reg offset       */
#define	FRC40_SPC_CLK_PERIOD	125			/* clock (nsec)     */
#define	FRC40_SPC_PARITY	SPC_DATA_PARITY_HIGH	/* input parity     */

#if	FALSE			/* change FALSE to TRUE for SCSI interface */
#define	INCLUDE_SCSI		/* include FRC-40 SCSI interface */
#define	INCLUDE_SCSI_BOOT	/* include ability to boot from SCSI */
#define	INCLUDE_DOSFS		/* file system to be used */
#define	INCLUDE_SCSI_DMA
#endif	/* FALSE/TRUE */

#endif	/* INCfrcEagle01h */
