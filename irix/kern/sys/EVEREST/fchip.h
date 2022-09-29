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


#ifndef _SYS_EVEREST_FCHIP_H_
#define _SYS_EVEREST_FCHIP_H_

#if defined(_LANGUAGE_C) && defined(_KERNEL)
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/reg.h>
#include <sys/dmamap.h>
#include <sys/pio.h>
#endif

#define FCHIP_VERSION_NUMBER	0x0000
#define FCHIP_MASTER_ID		0x0008
#define FCHIP_INTR_MAP		0x0010
#define FCHIP_FIFO_DEPTH	0x0018
#define FCHIP_FCI_ERROR_CMND	0x0020
#define FCHIP_TLB_BASE		0x0028
#define FCHIP_MODE		0x0030
/*
   This table is from pault and defines all the bits in the fchip
   mode register.  The Value column is how graphics wants these
   bits set.
   
   Bit Number(s)           Name                                    Value
   -------------------------------------------------------------------------------
   0                       Order Read Response Mode                0
   1                       Command Overlap                         1
   2                       DMA Read Buffer Size                    1 (4 cache lines)
   [5:3]                   Prefetch Count                          0
   [8:6]                   Maximum Outstanding IBus DMA Reads      2 (4 reads)
   9                       Enable EOF                              1
   10                      Unordered DMA Mode                      1
   11                      Enable RD_NXT32                         1
   12                      Enable Quarter DMA Reads                1
*/
#define     FCHIP_MODE_ORDER_READ_RESP	0x1
#define     FCHIP_MODE_CMD_OVERLAP	0x2
#define     FCHIP_MODE_DMA_RD_4LINES	0x4	/* only on Rev 2 or newer */
#define	    FCHIP_MODE_PREFETCH_CNT(x)  ((x & 7) << 3)
#define     FCHIP_MODE_MAX_DMA_RD(x)    ((x & 3) << 6)
#define     FCHIP_MODE_ENABLE_EOF       (1 << 9)
#define     FCHIP_MODE_UNORDERED_DMA    (1 << 10)
#define     FCHIP_MODE_RD_NXT32         (1 << 11)
#define     FCHIP_MODE_QRTR_DMA_RD      (1 << 12)

#define FCHIP_ORDER_READ_RESP	FCHIP_MODE	/* XXX obselete name */
#define FCHIP_DMA_TIMEOUT	0x0038
#define FCHIP_INTR_MASK		0x0040
#define FCHIP_INTR_SET_MASK	0x0048
#define FCHIP_INTR_RESET_MASK	0x0050
#define FCHIP_SW_FCI_RESET	0x0058
#define FCHIP_IBUS_ERROR_CMND	0x0060
#define FCHIP_TLB_FLUSH		0x0068
#define FCHIP_ERROR		0x0070
#define FCHIP_ERROR_CLEAR	0x0078

/* FCHIP TLB related registers used mostly by POD */
#define	FCHIP_TLB_IO0		0x00200
#define	FCHIP_TLB_IO1		0x00208
#define	FCHIP_TLB_IO2		0x00210
#define	FCHIP_TLB_IO3		0x00218
#define	FCHIP_TLB_IO4		0x00220
#define	FCHIP_TLB_IO5		0x00228
#define	FCHIP_TLB_IO6		0x00230
#define	FCHIP_TLB_IO7		0x00238

#define	FCHIP_TLB_EBUS0		0x00240
#define	FCHIP_TLB_EBUS1		0x00248
#define	FCHIP_TLB_EBUS2		0x00250
#define	FCHIP_TLB_EBUS3		0x00258
#define	FCHIP_TLB_EBUS4		0x00260
#define	FCHIP_TLB_EBUS5		0x00268
#define	FCHIP_TLB_EBUS6		0x00270
#define	FCHIP_TLB_EBUS7		0x00278


#if defined(_LANGUAGE_C) && defined(_KERNEL)
/* PROTOTYPE DECLARATION */
extern void	fchip_tlbflushall(__psunsigned_t);
extern void	fchip_tlbflush(dmamap_t *);
extern void	fchip_intr(eframe_t *, void *);
extern void	fchip_init(__psunsigned_t);
extern void	fchip_fcg_init(__psunsigned_t);
extern int	pio_mapfix_fci(piomap_t *);

#endif	/* _LANGUAGE_C && _KERNEL */

#endif
