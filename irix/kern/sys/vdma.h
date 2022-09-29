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
 * VDMA controller definitions
 */


#if IP20 || IP22 || IP26 || IP28

/*
 * DMA_CTL register bits(field)s
 */
#define VDMA_C_IE	(1<<4)		/* Enable VDMA complete interrupt */
#define VDMA_C_XLATE	(1<<8)		/* Enable virtual addr. translation */
#define VDMA_C_DECSLV	(1<<9)		/* Slave does true GIO decr. DMA */

/*
 * DMA_MODE register bits
 */
#define VDMA_M_GTOH     (1<<1)		/* transfer is from gfx to host */
#define VDMA_M_SYNC	(1<<2)		/* delay transfer until sync */
#define VDMA_M_FILL	(1<<3)		/* write constant data to memory */
#define VDMA_M_INCA	(1<<4)		/* memory address increments */
#define VDMA_M_SNOOP	(1<<5)		/* enable cache snoop (short burst) */
#define VDMA_M_LBURST	(1<<6)		/* Long burst transfers */

/*
 * DMA_RUN register bits
 * (also found in DMA_CAUSE)
 */
#define VDMA_R_PAGEFAULT	(1<<0)	/* Accessed non-resident page */
#define VDMA_R_UTLBMISS		(1<<1)	/* VDMA uTLB miss */ 
#define VDMA_R_CLEAN		(1<<2)	/* Wrote to read-only page */
#define VDMA_R_COMPLETE		(1<<3)	/* Last DMA completed successfully */
#define VDMA_R_BMEM		(1<<4)	/* Invalid memory address */
#define VDMA_R_BPTE		(1<<5)	/* Invalid PTE address */
#define VDMA_R_RUNNING		(1<<6)	/* DMA in progress (DMA_RUN only) */


#define VDMAREG(i) (*(volatile uint *)(PHYS_TO_COMPATK1(i)))

#if _LANGUAGE_C

/*
 * This structure contains the information needed
 * to set up the vdma controller for a transfer.
 */

typedef struct vdma_args {
        void *  memadr;		/* i/o buffer address */
        ushort  width;		/* line width in bytes */
        ushort  height;		/* number of lines to transfer */
        short   stride;		/* bytes from END of 1 line to start of next */
        ushort  yzoom;		/* y zoom factor (1023 max) */
        void *  gioaddr;	/* GIO slave address, or memory fill pattern */
        uint    mode;		/* mode register bits */
} vdma_args_t;

/*
 * vdma_state flags
 */

#define VDMA_INITTED 	0x01	/* Controller initialized by VdmaInit() */

/*
 * If MCdma is called with int_enab set to this magic cookie,
 * MCdma attempts to work around a REX bug which causes DMAs
 * from the REX to MC to stall. Since this work around needs to
 * poll for DMA completion there is no conflict over int_enab 
 * usage.
 */
#define REX_BUG		0x55

#ifndef _STANDALONE
extern void VdmaInit(void);
extern int vdma_set_tlb(caddr_t, int);
extern int MCdma(vdma_args_t *, int);
extern void MCIntClr(void);
extern int fakePICdma (gdmada_t *dmadap, int modes);
extern int vdma_fault(int);
#endif

#endif	/* _LANGUAGE_C */

#endif	/* IP20 || IP22 || IP26 || IP28 */

