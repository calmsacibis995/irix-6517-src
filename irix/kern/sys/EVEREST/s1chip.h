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
 * s1chip.h -- S1 chip register location definitions.
 */

#ident "$Revision: 1.15 $"

#include "sys/reg.h"
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/io4.h"

#ifndef __SYS_S1CHIP_H__
#define __SYS_S1CHIP_H__

#ifdef _LANGUAGE_C

#include "sys/types.h"

#define S1_GET(_r, _i, _reg) \
        (evreg_t)load_double((long long *)(SWIN_BASE((_r), (_i)) + (_reg)))
#define S1_SET(_r, _i, _reg, _value) \
        store_double((long long *)(SWIN_BASE((_r),(_i)) + (_reg)), (long long)(_value))
#endif /* _LANGUAGE_C */


/*
 * Overall S1 address map
 */

#define	S1_CTRL_BASE		0x0000	/* SCSI protocol controller base */
#define	S1_CTRL_OFFSET		0x0800	/* SCSI protocol controller offset */

#define	S1_DMA_BASE		0x2000	/* internal registers */
#define	S1_DMA_OFFSET		0x0800	/* internal register offset 0,1,2 */

#define	S1_GENERAL_OFFSET	0x4000	/* General S1 registers */

/*
 * Per Slice (DMA 0,1,2) addresses (64 bit endian independant accesses)
 */

#define	S1_DMA_WRITE_OFF	0x000	/* offset for writes (starts dma) */
#define	S1_DMA_READ_OFF		0x008	/* offset for reads (starts dma) */

#define	S1_DMA_TRANS_LOW	0x010	/* address of list (low order 32) */
#define	S1_DMA_TRANS_HIGH	0x018	/* address of list (high order 8) */

#define	S1_DMA_FLUSH		0x020	/* flush the channel */

#define	S1_DMA_CHAN_RESET	0x028	/* reset the channel (slice) */

/*
 * Scsi Interface Programming register map (DMAPGM)
 */

#define	S1_INTF_R_SEQ_REGS	0x4000	/* dma interface read sequence regs */

#define	S1_INTF_R_OP_BR_0	0x4100	/* read branch 0 address reg */
#define	S1_INTF_R_OP_BR_1	0x4108	/* read branch 1 address reg */

#define	S1_INTF_W_SEQ_REGS	0x4200	/* dma interface write sequence regs */

#define	S1_INTF_W_OP_BR_0	0x4300	/* write branch 0 address reg */
#define	S1_INTF_W_OP_BR_1	0x4308	/* write branch 1 address reg */

#define	S1_INTF_CONF		0x4318	/* interface configuration register */

/*
 * uP interface register map
 */

#define	S1_UP_R_SEQ_REGS	0x4400	/* uP read sequence regs */

#define	S1_UP_R_OP_BR_0		0x4500	/* uP read branch 0 reg */
#define	S1_UP_R_OP_BR_1		0x4508	/* uP read branch 0 reg */
#define	S1_UP_R_END		0x4510	/* uP read end pointer */

#define	S1_UP_W_SEQ_REGS	0x4600	/* uP write sequence regs */

#define	S1_UP_W_OP_BR_0		0x4700	/* uP write branch 0 reg */
#define	S1_UP_W_OP_BR_1		0x4708	/* uP write branch 0 reg */
#define	S1_UP_W_END		0x4710	/* uP write end pointer */

#define	S1_UP_CONF		0x4718	/* uP interface configuration reg */


/*
 * S1 Internal register map
 */
#define	S1_DRIVERS_CONF		0x4800	/* Scsi bus drivers configuration */
#define	S1_STATUS_CMD		0x6000	/* S1 status and command register */

#define	S1_DMA_INTERRUPT	0x7000	/* Channel interrupt reg */
#define	S1_DMA_INT_OFFSET	0x0008	/* Channel interrupt offset 0,1,2 */

#define	S1_INTERRUPT		0x7018	/* S1 chip interrupt reg */

#define	S1_PIO_CLEAR		0x7020	/* PIO clear register */

#define	S1_IBUS_ERR_STAT	0x7028	/* Ibus error status reg */

#define	S1_IBUS_LOG_LOW		0x7030	/* Ibus error log (low 32 bits) */
#define	S1_IBUS_LOG_HIG		0x7038	/* Ibus error log (high 32 bits) */


/*
 * S1 Ibus error register defines (from pg 101 of rev 1.3 of S1 spec)
 */
#define	S1_IERR_OVR_INTERRUPT	(1<<26)	/* interrupt overrun */
#define	S1_IERR_OVR_DMA_READ	(1<<25)	/* DMA read response overrun */
#define	S1_IERR_OVR_WRITE_REQ	(1<<24)	/* PIO write request overrun */
#define	S1_IERR_OVR_PIO_READ	(1<<23)	/* PIO read response overrun */
#define	S1_IERR_OVR_SURPRISE	(1<<22)	/* surprise overrun */

#define	S1_IERR_OVR_CH_2	(1<<21)	/* overrun on channel 2 */
#define	S1_IERR_OVR_CH_1	(1<<20)	/* overrun on channel 1 */
#define	S1_IERR_OVR_CH_0	(1<<19)	/* overrun on channel 0 */
#define	S1_IERR_OVR_CH(_chan)	(1<<(19+(_chan)))

#define	S1_IERR_OVR_TRANSLATION	(1<<18)	/* DMA translation overrun */
#define	S1_IERR_OVR_OUT_CMD	(1<<17)	/* outgoing command overrun */
#define	S1_IERR_OVR_OUT_DATA	(1<<16)	/* outgoing data overrun */
#define	S1_IERR_OVR_IN_CMD	(1<<15)	/* incoming command overrun */
#define	S1_IERR_OVR_IN_DATA	(1<<14)	/* incoming data overrun */

#define	S1_IERR_INTERRUPT	(1<<13)	/* interrupt error */
#define	S1_IERR_DMA_READ	(1<<12)	/* DMA read response error */
#define	S1_IERR_WRITE_REQ	(1<<11)	/* PIO write request error */
#define	S1_IERR_PIO_READ	(1<<10)	/* PIO read response error */
#define	S1_IERR_SURPRISE	(1<<9)	/* surprise error */

#define	S1_IERR_ERR_CH_2	(1<<8)	/* error in channel 2 */
#define	S1_IERR_ERR_CH_1	(1<<7)	/* error in channel 1 */
#define	S1_IERR_ERR_CH_0	(1<<6)	/* error in channel 0 */
#define	S1_IERR_ERR_CH(_chan)	(1<<(6+(_chan)))

#define	S1_IERR_TRANSLATION	(1<<5)	/* error in DMA translation */
#define	S1_IERR_OUT_CMD		(1<<4)	/* error on outgoing command */
#define	S1_IERR_OUT_DATA	(1<<3)	/* error on outgoing data */
#define	S1_IERR_IN_CMD		(1<<2)	/* error on incoming command */
#define	S1_IERR_IN_DATA		(1<<1)	/* error on incoming data */

#define	S1_IERR_ERR_ALL(_chan) \
( S1_IERR_INTERRUPT     | S1_IERR_DMA_READ     | \
  S1_IERR_WRITE_REQ     | S1_IERR_PIO_READ     | S1_IERR_SURPRISE        | \
  S1_IERR_TRANSLATION   | S1_IERR_OUT_CMD      | S1_IERR_OUT_DATA        | \
  S1_IERR_IN_CMD        | S1_IERR_IN_DATA      | S1_IERR_ERR_CH(_chan) )

#define	S1_IERR_OVR_ALL(_chan) \
( S1_IERR_OVR_INTERRUPT | S1_IERR_OVR_DMA_READ | S1_IERR_OVR_WRITE_REQ   | \
  S1_IERR_OVR_PIO_READ  | S1_IERR_OVR_SURPRISE | S1_IERR_OVR_TRANSLATION | \
  S1_IERR_OVR_OUT_CMD   | S1_IERR_OVR_OUT_DATA | S1_IERR_OVR_IN_CMD      | \
  S1_IERR_OVR_IN_DATA   | S1_IERR_OVR_CH(_chan) )


/*
 * S1 Status and Command Register defines
 */
#define	S1_SC_INT_CNTRL_2	(1 << 22)	/* interrupt from cntrl 2 */
#define	S1_SC_INT_CNTRL_1	(1 << 21)	/* interrupt from cntrl 1 */
#define	S1_SC_INT_CNTRL_0	(1 << 20)	/* interrupt from cntrl 0 */
#define	S1_SC_INT_CNTRL(_chan)	(1 << (20 + _chan))
#define	S1_SC_INT_CNTRL_ANY	(S1_SC_INT_CNTRL_2 | S1_SC_INT_CNTRL_1 | \
					S1_SC_INT_CNTRL_0)

#define	S1_SC_DMA_2	(1 << 19)	/* dma slice 2 active */
#define	S1_SC_DMA_1	(1 << 18)	/* dma slice 1 active */
#define	S1_SC_DMA_0	(1 << 17)	/* dma slice 0 active */
#define	S1_SC_DMA(_chan)	(1 << (17 + _chan))

#define	S1_SC_IBUS_ERR	(1 << 16)	/* IBus error detected */
#define	S1_SC_PIO_DROP	(1 << 15)	/* PIO Drop Mode active */
#define	S1_SC_INV_PIO	(1 << 14)	/* Invalid PIO */
#define	S1_SC_M_WDATA	(1 << 13)	/* Missing Write data */
#define	S1_SC_M_WDATA_O	(1 << 12)	/* Missing Write data overrun */

#define	S1_SC_R_ERR_2	(1 << 11)	/* SCSI PIO Read Error on SCSI 2 */
#define	S1_SC_R_ERR_1	(1 << 10)	/* SCSI PIO Read Error on SCSI 1 */
#define	S1_SC_R_ERR_0	(1 << 9)	/* SCSI PIO Read Error on SCSI 0 */
#define	S1_SC_R_ERR(_chan)	(1 << (9 + _chan))

#define	S1_SC_D_ERR_2	(1 << 8)	/* SCSI DMA Error on SCSI 2 */
#define	S1_SC_D_ERR_1	(1 << 7)	/* SCSI DMA Error on SCSI 1 */
#define	S1_SC_D_ERR_0	(1 << 6)	/* SCSI DMA Error on SCSI 0 */
#define	S1_SC_D_ERR(_chan)	(1 << (6 + _chan))

#define	S1_SC_BIG_END	(1 << 5)	/* SCSI Protocol Big Endian */
#define	S1_SC_DATA_16	(1 << 4)	/* 8/16 bit mode */

#define	S1_SC_S_RESET_2	(1 << 3)	/* SCSI Bus reset for controller 2 */
#define	S1_SC_S_RESET_1	(1 << 2)	/* SCSI Bus reset for controller 1 */
#define	S1_SC_S_RESET_0	(1 << 1)	/* SCSI Bus reset for controller 0 */
#define	S1_SC_S_RESET(_chan)	(1 << (1 + _chan))

#define S1_SC_ERR_MASK(_chan) \
( S1_SC_PIO_DROP   | S1_SC_INV_PIO       | S1_SC_M_WDATA | \
  S1_SC_M_WDATA_O  | S1_SC_R_ERR(_chan)  | S1_SC_D_ERR(_chan) )


/* general info about the S1 chips */

#define	S1_MAX_CHANNELS		3

/* types of chips attached to an S1 */

#define	S1_H_ADAP_95A		1
#define	S1_H_ADAP_93B		2

#ifdef _LANGUAGE_C
/* Prototype definitions */
#if _STANDALONE
void s1_init(unchar, unchar, unchar);
void s1_reset(int);
int scip_diag(u_char, u_char, u_char);
#else
void s1_init(unchar, unchar, unchar, int);
#endif /* _STANDALONE */

extern void s1chip_intr(eframe_t *, void *);

extern int  scsidma_flush(dmamap_t *, int);

#endif /* _LANGUAGE_C */

#endif /* __SYS_S1CHIP_H__ */
