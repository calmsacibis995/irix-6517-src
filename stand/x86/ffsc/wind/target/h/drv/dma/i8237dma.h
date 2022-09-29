/* i8237Dma.h - Intel 8237 DMA controller header */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01a,30sep93,hdn  written.
*/

#ifndef __INCi8237Dmah
#define __INCi8237Dmah

#ifdef __cplusplus
extern "C" {
#endif


/* DMA chip-1 IO address */

#define DMA1_BASE		0x00

#define DMA1_ADDR(chan)		(DMA1_BASE + (chan << 1))
#define DMA1_COUNT(chan)	(DMA1_BASE + (chan << 1) + 1)
#define DMA1_STATUS		0x08
#define DMA1_CMD		0x08
#define DMA1_MASK_SINGLE	0x0a
#define DMA1_MODE		0x0b
#define DMA1_CLEAR_FF		0x0c
#define DMA1_CLEAR_ALL		0x0d
#define DMA1_CLEAR_MASK		0x0e
#define DMA1_MASK_ALL		0x0f

/* DMA chip-2 IO address */

#define DMA2_BASE		0xc0

#define DMA2_ADDR(chan)		(DMA2_BASE + ((chan - 4) << 2))
#define DMA2_COUNT(chan)	(DMA2_BASE + ((chan - 4) << 2) + 2)
#define DMA2_STATUS		0xd0
#define DMA2_CMD		0xd0
#define DMA2_MASK_SINGLE	0xd4
#define DMA2_MODE		0xd6
#define DMA2_CLEAR_FF		0xd8
#define DMA2_CLEAR_ALL		0xda
#define DMA2_CLEAR_MASK		0xdc
#define DMA2_MASK_ALL		0xde

/* mode register bit */

#define DMA_MODE_DEMAND		0x00
#define DMA_MODE_SINGLE		0x40
#define DMA_MODE_BLOCK		0x80
#define DMA_MODE_CASCADE	0xc0
#define DMA_MODE_DECREMENT	0x20
#define DMA_MODE_INCREMENT	0x00
#define DMA_MODE_AUTO_ENABLE	0x10
#define DMA_MODE_AUTO_DISABLE	0x00
#define DMA_MODE_WRITE		0x08
#define DMA_MODE_READ		0x04

/* mask register bit */

#define DMA_MASK_SET		0x04
#define DMA_MASK_RESET		0x00


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS dmaSetup (int direction, void *pBuf, UINT nBytes, UINT chan);

#else

STATUS dmaSetup ();

#endif  /* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCi8237Dmah */

