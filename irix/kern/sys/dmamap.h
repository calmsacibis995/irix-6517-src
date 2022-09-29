#ifndef __SYS_DMAMAP_H__
#define __SYS_DMAMAP_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 3.26 $"

#include "sys/edt.h"

/*
 * Definitions for allocating, freeing, and using DMA maps
 */

/*
 * DMA map types
 */
#define	DMA_SCSI	0
#define	DMA_A24VME	1		/* Challenge/Onyx only 	*/
#define	DMA_A32VME	2		/* Challenge/Onyx only 	*/
#define	DMA_A64VME	3		/* SN0/Racer */

#define	DMA_EISA	4

#define	DMA_PCI32	5		/* SN0/Racer 	*/
#define	DMA_PCI64	6		/* SN0/Racer 	*/

/*
 * DMA map structure as returned by dma_mapalloc()
 */
typedef struct dmamap {
	int		dma_type;	/* Map type (see above) */
	int		dma_adap;	/* I/O adapter */
	int		dma_index;	/* Beginning map register to use */
	int		dma_size;	/* Number of map registers to use */
	paddr_t		dma_addr;	/* Corresponding bus addr for A24/A32 */
	caddr_t		dma_virtaddr;	/* Beginning virtual address that is mapped */
} dmamap_t;

#include "sys/buf.h"          /* For buf_t */
struct alenlist_s;

/*
 * Prototypes of exported functions
 */
extern dmamap_t	*dma_mapalloc(int, int, int, int);
extern void	dma_mapfree(dmamap_t *);
extern int	dma_map(dmamap_t *, caddr_t, int);
extern int	dma_map2(dmamap_t *, caddr_t, caddr_t, int);
extern paddr_t	dma_mapaddr(dmamap_t *, caddr_t);
extern int	dma_mapbp(dmamap_t *, buf_t *, int);
extern int	dma_map_alenlist(dmamap_t *, struct alenlist_s *, size_t);
extern uint	ev_kvtoiopnum(caddr_t);

/*
 * These variables are defined in master.d/kernel
 */
extern struct map *a24map[];
extern struct map *a32map[];

extern int a24_mapsize;
extern int a32_mapsize;

extern lock_t dmamaplock;
extern sv_t dmamapout;

#ifdef __cplusplus
}
#endif

/* standard flags values for pio_map routines,
 * including {xtalk,pciio}_dmamap calls.
 * NOTE: try to keep these in step with PIOMAP flags.
 */
#define DMAMAP_FIXED	0x1
#define DMAMAP_NOSLEEP	0x2
#define	DMAMAP_INPLACE	0x4

#define	DMAMAP_FLAGS	0x7

#endif /* __SYS_DMAMAP_H__ */
