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

/*
 * wd95a.h -- Western Digital 95a chip and associated data structures.
 */

#ident "$Revision: 1.3 $"

#ifndef _SYS_WD95A_
#define _SYS_WD95A_

/*
 * register definitions - Setup Mode
 */

#define	WD95A_S_CTL	0		/* control register */
#define	WD95A_S_SCNF	2		/* scsi configuration */
#define	WD95A_S_OWNID	4		/* Own ID register */
#define	WD95A_S_TIMOUT	6		/* Sel & Resel timout period */
#define	WD95A_S_SLEEP	8		/* Sleep Countdown register */
#define	WD95A_S_TIMER	10		/* scsi timeout countdown residue */

#define	WD95A_S_CDBSIZ1	12		/* Special CDB Size reg (grp 3,4) */
#define	WD95A_S_CDBSIZ2	14		/* Special CDB Size reg (grp 6,7) */
#define	WD95A_S_IDFLAG0	16		/* SCSI-ID specific flags (0..7) */
#define	WD95A_S_IDFLAG1	18		/* SCSI-ID specific flags (8..15) */

#define	WD95A_S_DMACNF	20		/* DMA Configuration */
#define	WD95A_S_DMATIM	22		/* DMA Timing control */

#define	WD95A_S_TEST0	24		/* Factory Test Register */

#define	WD95A_S_SC0	26		/* SCSI Low-level control */
#define	WD95A_S_SC1	28		/* SCSI Low-level control */
#define	WD95A_S_SC2	30		/* SCSI Low-level control */
#define	WD95A_S_SC3	32		/* SCSI Low-level control */

#define	WD95A_S_CSADR	34		/* WCS Address Register */
#define	WD95A_S_CSPRT0	36		/* WCS port for window 0 */
#define	WD95A_S_CSPRT1	38		/* WCS port for window 1 */
#define	WD95A_S_CSPRT2	40		/* WCS port for window 2 */
#define	WD95A_S_CSPRT3	42		/* WCS port for window 3 */
#define	WD95A_S_SQSEL	44		/* WCS Sel reponse addr */
#define	WD95A_S_SQRSL	46		/* WCS ReSel reponse addr */
#define	WD95A_S_SQDMA	48		/* WCS Start addr for DMA req */

#define	WD95A_S_DPRADD	50		/* DPR address Pointer */
#define	WD95A_S_DPRTC	52		/* DPR Transfer Counter */

#define	WD95A_S_PLR	54		/* Physical:Logical Block Ratio */
#define	WD95A_S_PSIZE0	56		/* Physical Block Size */
#define	WD95A_S_PSIZE1	58		/* Physical Block Size */

#define	WD95A_S_PING	60		/* Mirroring Ping-pong value */
#define	WD95A_S_CMPIX	62		/* Data Compare Index */
#define	WD95A_S_CMPVAL	64		/* Data Compare Value Register */
#define	WD95A_S_CMPMASK	66		/* Data Compare Mask Register */

#define	WD95A_S_BBDL	68		/* Buffered BD[0..7] */
#define	WD95A_S_BBDH	70		/* Buffered BD[8..15] */

#define	WD95A_S_CVER	72		/* Chip/Design Version */
#define	WD95A_S_TEST1	74		/* Factory Test Register */

#define	WD95A_S_OFFSET	76		/* REQ/ACK offset counter */

/*
 * register definitions - Normal Mode
 */

#define	WD95A_N_CTL	0		/* Control Register */
#define	WD95A_N_CTLA	2		/* Control Register, Auxiliary */

#define	WD95A_N_ISR	4		/* Interrupt Status Register */
#define	WD95A_N_STOPU	6		/* Unexpected Stop Status Register */
#define	WD95A_N_UEI	8		/* Unexpected Event Intr Register */
#define	WD95A_N_ISRM	10		/* ISR Mask Register */
#define	WD95A_N_STOPUM	12		/* STOPU Mask Register */
#define	WD95A_N_UEIM	14		/* UEI Mask Register */

#define	WD95A_N_RESPONSE 16		 /* Automatic Response Control */
#define	WD95A_N_SQINT	18		/* WCS Sequencer Interrupt Addr */
#define	WD95A_N_SQADR	20		/* Current addr WCS is executing */

#define	WD95A_N_STC	22		/* SCSI Transfer Control Register */
#define	WD95A_N_SPW	24		/* SCSI Pulse Width Control Reg */

#define	WD95A_N_DESTID	26		/* Destination ID Register */
#define	WD95A_N_SRCID	28		/* Source ID Register */
#define	WD95A_N_FLAG	30		/* Flag Register */

#define	WD95A_N_TC_0_7	32		/* Transfer Count 0..7 */
#define	WD95A_N_TC_8_15	34		/* Transfer Count 8..15 */
#define	WD95A_N_TC_16_23 36		/* Transfer Count 16..23 */

#define	WD95A_N_DATA	38		/* Data Access Port */
#define	WD95A_N_SR	40		/* Status Register */
#define	WD95A_N_FIFOS	42		/* Fifo Status Register */

#define	WD95A_N_PBR	44		/* Physical Block Residue */
#define	WD95A_N_BR0	46		/* Byte Residue */
#define	WD95A_N_BR1	48		/* Byte Residue */
#define	WD95A_N_PINGR	50		/* PING Residue */
#define	WD95A_N_LRCR0	52		/* LRC Residue Register (Low) */
#define	WD95A_N_LRCR1	54		/* LRC Residue Register (High) */
#define	WD95A_N_ODDR	56		/* Odd-Byte Reconnect Register */

#define	WD95A_N_DPR	64		/* address of first DPR */
#define	W95_DPR_R(_reg)	(64 + (_reg << 1))

/* Control register definition */
#define	W95_CTL_SETUP	(1 << 0)	/* change between setup and normal */
#define	W95_CTL_ABORT	(1 << 1)	/* Abort any operation */
#define	W95_CTL_KILL	(1 << 2)	/* Kill any operation */
#define	W95_CTL_LDTC	(1 << 3)	/* load transfer count */
#define	W95_CTL_LDTCL	(1 << 4)	/* load transfer count last */
#define	W95_CTL_RSTO	(1 << 5)	/* reset the scsi bus */

/* SCNF - SCSI Configuration Register */
#define	W95_SCNF_LRCCE	(1 << 0)	/* LRC Checking Enable */
#define	W95_SCNF_SCSIL	(1 << 1)	/* start low-level micro bus control */
#define	W95_SCNF_SPDEN	(1 << 2)	/* Enable Checking for parity on bus */
#define	W95_SCNF_SPUE	(1 << 3)	/* SCSI Pullup Enable */
#define	W95_SCNF_RSTFM	(1 << 4)	/* RSTF output Mask */
#define	W95_SCNF_SCLK	(7 << 5)	/* LRC Checking Enable */

#define	W95_SCNF_CLK_SHFT	5	/* number of bits to shift a value */

/* DMACNF - DMA Config register */
#define	W95_DCNF_BURST	(1 << 0)	/* burst/single cycle DMA */
#define	W95_DCNF_MASTER	(1 << 1)	/* Master/slave DMA */
#define	W95_DCNF_PGEN	(1 << 2)	/* Generate Parity */
#define	W95_DCNF_DPEN	(1 << 3)	/* check for odd parity on dma data */
#define	W95_DCNF_DRQAH	(1 << 4)	/* Active high for DRQ Signal */
#define	W95_DCNF_DACAH	(1 << 5)	/* Active high for DACK Signal */
#define	W95_DCNF_DMA16	(1 << 6)	/* Transfer size 8/16 */
#define	W95_DCNF_SWAP	(1 << 7)	/* Swap high and low bytes on dma */

/* DMATIM - dma timing register */
#define	W95_DTIM_DCLKH	(7 << 0)	/* DMA read/write high pulse width */
#define	W95_DTIM_DCLKL	(7 << 3)	/* DMA read/write low pulse width */
#define	W95_DTIM_DSRW	(1 << 6)	/* DACK setup time from leading edge */
#define	W95_DTIM_DMAOE	(1 << 7)	/* DMA output enable */

#define	W95_DTIM_CH_SHFT	0	/* bits to shift width value (high) */
#define	W95_DTIM_CL_SHFT	3	/* bits to shift width value (low) */

/* SC0, SC1 - low level control regiseters */

#define	W95_SC0_SELO	(1 << 0)	/* SEL output value */
#define	W95_SC0_SELI	(1 << 1)	/* SEL input value */
#define	W95_SC0_BSYO	(1 << 2)	/* BSY output value */
#define	W95_SC0_BSYI	(1 << 3)	/* BSY input value */
#define	W95_SC0_RSTI	(1 << 5)	/* RST input value */
#define	W95_SC0_SDP	(1 << 6)	/* SCSI parity for SD 0..7 */
#define	W95_SC0_SDP1	(1 << 7)	/* SCSI parity for SD 8..15 */

#define	W95_SC1_REQ	(1 << 0)	/* REQ output/input value on bus */
#define	W95_SC1_ACK	(1 << 1)	/* ACK output/input value on bus */
#define	W95_SC1_MSG	(1 << 2)	/* MSG output/input value on bus */
#define	W95_SC1_CD	(1 << 3)	/* C/D output/input value on bus */
#define	W95_SC1_IO	(1 << 4)	/* I/O output/input value on bus */
#define	W95_SC1_ATNL	(1 << 5)	/* ATN output/input value on bus */
#define	W95_SC1_TGS	(1 << 6)	/* Target group sel (REQ,MSG,CD,IO) */
#define	W95_SC1_IGS	(1 << 7)	/* Init. group sel (REQ,MSG,CD,IO) */

/* CVER - version and bus type reg */
#define	W95_CVER_CVER	(0xf)
#define	W95_CVER_CV(_val)	((_val) & W95_CVER_CVER)
#define	W95_CVER_DSENS	(1 << 4)	/* Differential without SE drive */
#define	W95_CVER_SE	(1 << 5)	/* Single ended bus */

/* CTLA - Control Register, Auxiliary */
#define	W95_CTLA_MRM	(1 << 0)	/* Mirror-Restore Mode */
#define	W95_CTLA_ATNHD	(1 << 1)	/* Halt data transfer on ATN */
#define	W95_CTLA_LRCGS	(1 << 2)	/* LRC generation/strip */
#define	W95_CTLA_PIO	(1 << 3)	/* Polled IO of Fifo enabled */
#define	W95_CTLA_PPEN	(1 << 4)	/* Ping/pong striping enable */
#define	W95_CTLA_PAS	(1 << 5)	/* DMA port A enable */
#define	W95_CTLA_PBS	(1 << 6)	/* DMA port B enable */
#define	W95_CTLA_BYTEC	(1 << 7)	/* TC pipeline values are byte/block */

/* ISR - Interrupt status register */
#define	W95_ISR_STOPWCS	(1 << 0)	/* WCS executed STOP instruction */
#define	W95_ISR_INTWCS	(1 << 1)	/* WCS instruction with INT set */
#define	W95_ISR_STOPU	(1 << 2)	/* WCS stopped unexpectedly */
#define	W95_ISR_UEI	(1 << 3)	/* Unexpected Exception inter */
#define	W95_ISR_BUSYI	(1 << 4)	/* TC trans from busy to empty */
#define	W95_ISR_VBUSYI	(1 << 5)	/* TC trans from very busy to busy */
#define	W95_ISR_SREJ	(1 << 6)	/* micro wrote to SQADR while inhib. */
#define	W95_ISR_INT0	(1 << 7)	/* INT output line asserted by ESBC */

/* STOPU - Unexpected stop status register */
#define	W95_STPU_ABORTI	(1 << 0)	/* Abort Issued by micro */
#define	W95_STPU_SCSIT	(1 << 1)	/* SCSI timeout during sel or resel */
#define	W95_STPU_PARE	(1 << 2)	/* Parity Error */
#define	W95_STPU_LRCE	(1 << 3)	/* LRC Error */
#define	W95_STPU_TCUND	(1 << 4)	/* Transfer-count pipeline underrun */
#define	W95_STPU_SOE	(1 << 5)	/* SCSI offset error */

/* UEI - Unexpected interrupt status register */
#define	W95_UEI_FIFOE	(1 << 0)	/* FIFO overflow/underflow */
#define	W95_UEI_ATNI	(1 << 1)	/* ATN asserted */
#define	W95_UEI_UDISC	(1 << 2)	/* Unexpected SCSI bus disconnect */
#define	W95_UEI_TCOVR	(1 << 3)	/* TC pipeline Overrun */
#define	W95_UEI_USEL	(1 << 4)	/* Unexpected Selection */
#define	W95_UEI_URSEL	(1 << 5)	/* Unexpected Reselection */
#define	W95_UEI_UPHAS	(1 << 6)	/* Unexpected SCSI phase */
#define	W95_UEI_RSTINTL	(1 << 7)	/* Reset received on SCSI bus */

/* ISRM - Interrupt mask register */
#define	W95_ISRM_STOPWCSM (1 << 0)	/* WCS executed Stop instruction */
#define	W95_ISRM_INTWCSM  (1 << 1)	/* WCS instruction with INT */
#define	W95_ISRM_STOPUM	(1 << 2)	/* WCS stopped unexpectedly */
#define	W95_ISRM_UEIM	(1 << 3)	/* Unexpected Exception intr. */
#define	W95_ISRM_BUSYIM	(1 << 4)	/* TC pipeline becomes empty */
#define	W95_ISRM_VBUSYIM  (1 << 5)	/* TC pipeline becomes not full */
#define	W95_ISRM_SREJM	(1 << 6)	/* write to SQADR while inhibited */

/* STOPUM - Unexpected stop mask register */
#define	W95_STPUM_ABORTM (1 << 0)	/* Abort Issued by micro */
#define	W95_STPUM_SCSITM (1 << 1)	/* SCSI timeout during sel or resel */
#define	W95_STPUM_PAREM	(1 << 2)	/* Parity Error */
#define	W95_STPUM_LRCEM	(1 << 3)	/* LRC Error */
#define	W95_STPUM_TCUNDM (1 << 4)	/* Transfer-count pipeline underrun */
#define	W95_STPUM_SOEM	(1 << 5)	/* SCSI offset error */

/* UEIM - Unexpected interrupt mask register */
#define	W95_UEIM_FIFOEM	(1 << 0)	/* FIFO overflow/underflow */
#define	W95_UEIM_ATNIM	(1 << 1)	/* ATN asserted */
#define	W95_UEIM_UDISCM	(1 << 2)	/* Unexpected SCSI bus disconnect */
#define	W95_UEIM_TCOVRM	(1 << 3)	/* TC pipeline Overrun */
#define	W95_UEIM_USELM	(1 << 4)	/* Unexpected Selection */
#define	W95_UEIM_URSELM	(1 << 5)	/* Unexpected Reselection */
#define	W95_UEIM_UPHASM	(1 << 6)	/* Unexpected SCSI phase */
#define	W95_UEIM_RSTINTM (1 << 7)	/* Reset received on SCSI bus */

/* RESPONSE - Automatic response control register */
#define	W95_RESP_SELL	(1 << 0)	/* Enable low-level sel resp */
#define	W95_RESP_SELH	(1 << 1)	/* Enable high-level sel resp */
#define	W95_RESP_RSELL	(1 << 2)	/* Enable low-level resel resp */
#define	W95_RESP_RSELH	(1 << 3)	/* Enable high-level resel resp */
#define	W95_RESP_AUTOR	(1 << 4)	/* Automatic reconnect on DMA req */
#define	W95_RESP_AUTOD	(1 << 5)	/* pause data trans at logical blk */ 
#define	W95_RESP_ALLRR	(1 << 6)	/* Auto low-level response reset */

/* STC - SCSI trasfer control register */
#define	W95_STC_OFF_M	0x3f		/* synchronous transfer offset */
#define	W95_STC_WSCSI	(1 << 7)	/* enable wide SCSI trans for data */

/* SPW -  SCSI Synchronous clock Pulse width */
#define	W95_SPW_SCLKA	(7 << 0)	/* clock assertion pulse width */
#define	W95_SPW_SCLKA_SFT 0		/* bit position for shift */
#define	W95_SPW_SCLKN	(7 << 4)	/* clock negation pulse width */
#define	W95_SPW_SCLKN_SFT 4		/* bit position for shift */

/* SRCID - source ID register */
#define	W95_SRCID_M	0xf		/* mask for source id */
#define	W95_SRCID_SIV	(1 << 4)	/* valid se/reselection */

/* SR - status register */
#define	W95_SR_BUSY	(1 << 0)	/* ESBC is busy */
#define	W95_SR_VBUSY	(1 << 1)	/* ESBC is very busy */
#define	W95_SR_ACTIVE	(1 << 2)	/* WCS is running */

#define	WCS_NOWHERE	0x7f		/* don't do anything */

#endif /* _SYS_WD95A_ */
