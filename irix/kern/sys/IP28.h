/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/*
 * IP28.h -- cpu board specific defines for IP28
 *
 */

#ifndef __SYS_IP28_H__
#define	__SYS_IP28_H__

#if 0				/* make 1 to turn on this code */

#define _IP28_DMA_HOTEL		1

/*
 * use an IP-stype mask and bits to determine if memory is in the
 * same region or not.  In IP networking, you determine if a dstIP
 * and a src IP are on the same net via
 * same-net = ((src & mask) == (dst & mask))
 *
 * DMA HOTEL is an area of memory protected from random
 * T5 cache flushes by changing the T5 PAL so that *ALL* cache flushed
 * to a bank of memory are ignored
 *
 * The current definition is the third 8 Meg of memory.
 * by appropriate changing of the DMA_HOTEL_M and DMA_HOTEL_N
 * we could change this to the first M meg or memory,
 * or the first M meg of each N-meg bank of memory
 * and it needn't be just the first M meg, could be the 3rd M meg of every
 * N-meg bank or whatever.
 */
#define DMA_HOTEL_WIDTH 8
#define DMA_HOTEL_QUANTUM 1024*1024
#define DMA_HOTEL_M (DMA_HOTEL_WIDTH*DMA_HOTEL_QUANTUM) /* defined in T5 PAL */
#define DMA_HOTEL_SIZE DMA_HOTEL_M
#define DMA_HOTEL_N (-DMA_HOTEL_QUANTUM)
#define DMA_HOTEL_MASK	DMA_HOTEL_N
/*
 * The following definition of DMA_HOTEL_N  defines the range of all
 * memory.  i.e. first 8 meg of all memory.  To make it the first 8 meg
 * of each 64-meg bank, change DMA_NOTEL_N to be 64-meg
 */
#define DMA_HOTEL_BASE PHYS_RAMBASE+16*DMA_HOTEL_QUANTUM /* defined in T5 PAL */


#define IN_DMA_HOTEL(a)	\
	(((a) & DMA_HOTEL_MASK) == (DMA_HOTEL_BASE & DMA_HOTEL_MASK))

#define MBUF_HOTEL_SIZE 1*DMA_HOTEL_QUANTUM
#define MBUF_HOTEL_BASE 0
#define MBUF_HOTEL	\
	((__psunsigned_t)(PHYS_TO_K1(DMA_HOTEL_BASE + MBUF_HOTEL_BASE)))

#endif /* on/off */

#endif /* __SYS_IP28_H__ */
