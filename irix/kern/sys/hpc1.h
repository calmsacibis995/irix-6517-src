/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/hpc1.h: $Revision: 1.2 $"

#ifndef __SYS_HPC1_H__
#define __SYS_HPC1_H__

/* 
 * SCSI Channel #0 Control on Hollywood HPC-1 #1.
 */
#define HPC1_SCSI_BC2		0xbfb00088  /* SCSI byte count register */
#define HPC1_SCSI_BUFFER_PTR2	0xbfb0008c  /* SCSI current buffer pointer */
#define HPC1_SCSI_BUFFER_NBDP2	0xbfb00090  /* SCSI next buffer pointer */
#define HPC1_SCSI_CONTROL2	0xbfb00094  /* SCSI control register */

/* wd33c93 chip registers accessed through HPC-1 */
#define HPC1_SCSI_REG_A2	0xbfb00122  /* address register (b) */
#define HPC1_SCSI_REG_D2	0xbfb00126  /* data register (b) */

/* dma fifo control registers on HPC-1 #1 */
#define HPC1_SCSI_DMACFG2	0xbfb00098  /* SCSI pointer register */
#define HPC1_SCSI_PIOCFG2	0xbfb0009c  /* SCSI fifo data register */

/* 
 * SCSI Channel #1 Control on Hollywood HPC-1 #2.
 */
#define HPC1_SCSI_BC3		0xbf980088  /* SCSI byte count register */
#define HPC1_SCSI_BUFFER_PTR3	0xbf98008c  /* SCSI current buffer pointer */
#define HPC1_SCSI_BUFFER_NBDP3	0xbf980090  /* SCSI next buffer pointer */
#define HPC1_SCSI_CONTROL3	0xbf980094  /* SCSI control register */

/* wd33c93 chip registers accessed through HPC-1 */
#define HPC1_SCSI_REG_A3	0xbf980122  /* address register (b) */
#define HPC1_SCSI_REG_D3	0xbf980126  /* data register (b) */

/* dma fifo control registers on HPC-1 #2 */
#define HPC1_SCSI_DMACFG3	0xbf980098  /* SCSI pointer register */
#define HPC1_SCSI_PIOCFG3	0xbf98009c  /* SCSI fifo data register */

/*
 * Control register defines on Hollywood HPC-1.
 */
#define HPC1_SCSI_RESET		0x01		/* reset WD33C93 chip */
#define HPC1_SCSI_FLUSH		0x02		/* flush SCSI buffers */
#define HPC1_SCSI_TO_MEM	0x10		/* transfer in */
#define HPC1_SCSI_STARTDMA	0x80		/* start SCSI dma transfer */

/* descriptor structure for Hollywood HPC-1 SCSI / HPC interface */
#define HPC1_BCNT_OFFSET	0
#define HPC1_CBP_OFFSET		4
#define HPC1_NBP_OFFSET		8
#define HPC1_EOX_VALUE		0x80000000

#ifdef LANGUAGE_C
#ifdef	_MIPSEB
typedef struct hpc1_scsi_descr {
	unsigned fill:19, bcnt:13;
	unsigned eox:1, efill:3, cbp:28;
	unsigned nfill:4, nbp:28;
} hpc1_scdescr_t;
#else	/* _MIPSEL */
typedef struct hpc1_scsi_descr {
	unsigned bcnt:13, fill:19;
	unsigned cbp:28, efill:3, eox:1;
	unsigned nbp:28, nfill:4;
} hpc1_scdescr_t;
#endif	/* _MIPSEL */
#endif

#define	HPC1_ENDIAN_OFFSET	0xc0
#define	HPC1_LIO_0_OFFSET	0x1c0

#endif	/* __SYS_HPC1_H__ */
