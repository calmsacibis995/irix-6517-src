/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/diskheader.c $Revision: 1.1 $ of $Date: 1997/12/20 02:44:28 $"

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/usrdma.h>

#include "udmalib.h"
#include "udmadefs.h"
#include "vme_dma_engine.h"

struct universe_vmeinfo_s {
	int			uv_bufcnt;
	vme_dma_engine_handle_t	uv_engine;

};
typedef struct universe_vmeinfo_s universe_vmeinfo_t;

struct dmaparms_s {
	vme_dma_engine_buffer_t		d_buf;
	unsigned int			d_size;
	vme_dma_engine_dir_t		d_dir;
	vme_dma_engine_throttle_t	d_throttle;
	vme_dma_engine_release_t	d_release;
	vmeio_am_t			d_addrmod;
};
typedef struct dmaparms_s dmaparms_t;

/* NOTE: These routines are specific to IP27, Tundra Universe 2
 * They provide backward compatibility to the usrdma API from the 
 * vme_dma_engine API.
 */

int
universe_vme_open(dmaid_t *dp)
{
	universe_vmeinfo_t 	*uvp;
	char		 	devnm[PATH_MAX];

	if ((uvp = malloc(sizeof(universe_vmeinfo_t))) == NULL)
		return 1;

	(void)sprintf(devnm,"/hw/vme/%d/dma_engine", dp->d_adap);

	dp->d_businfo = uvp;

	if ((uvp->uv_engine = 
	     vme_dma_engine_handle_alloc(devnm, VME_DMA_ENGINE_DEBUG)) == 0)
		return 1;

	return 0;
}

int
universe_vme_close(dmaid_t *dp)
{
	universe_vmeinfo_t 	*uvp;

	uvp = dp->d_businfo;

	/* XXX may want to deallocate all the users buffers 
	 * associated with this DMA engine at this point.
	 */
	if (uvp->uv_bufcnt)
		return 1;

	vme_dma_engine_handle_free(uvp->uv_engine);
	(void)free(uvp);

	dp->d_businfo = NULL;

	return 0;
}

void *
universe_vme_allocbuf(dmaid_t *dp, int size)
{
	universe_vmeinfo_t 	*uvp;
	vme_dma_engine_buffer_t bp;

	uvp = dp->d_businfo;
	
	bp = vme_dma_engine_buffer_alloc(uvp->uv_engine, 0, size);

	uvp->uv_bufcnt++;

	return (void *)bp;
}

int 
universe_vme_freebuf(dmaid_t *dp, void *bp)
{
	universe_vmeinfo_t 	*uvp;

	uvp = dp->d_businfo;

	vme_dma_engine_buffer_free(uvp->uv_engine, bp);

	uvp->uv_bufcnt--;

	return 0;
}

#define VME_A24ND64PBAMOD 0x38
#define VME_A24SD64BAMOD  0x3c
#define VME_A32ND64PBAMOD 0x08
#define VME_A32SD64BAMOD  0x0c

/* ARGSUSED */
udmaprm_t *
universe_vme_mkparms(dmaid_t *dp, void *dinfo, void *iobuf, int size)
{
	vmeparms_t	*vp;
	dmaparms_t	*dmap;

	vp = dinfo;

	/* check out the address modifier */
	if (vp->vp_addrmod & 0xc0)
		return NULL;

	/* check out datum size */
	if (vp->vp_datumsz > VME_DS_DBLWORD)
		return NULL;

	/* check block mode */

	if ((dmap = malloc(sizeof(dmaparms_t))) == NULL)
		return NULL;

	dmap->d_buf	= (vme_dma_engine_buffer_t)iobuf;
	dmap->d_size	= size;
	dmap->d_dir 	= (vp->vp_dir ? VME_DMA_ENGINE_DIR_WRITE :
					VME_DMA_ENGINE_DIR_READ);
	dmap->d_throttle= (vp->vp_throt ? VME_DMA_ENGINE_THROTTLE_2048 :
					  VME_DMA_ENGINE_THROTTLE_256);
	dmap->d_release	= (vp->vp_release ? VME_DMA_ENGINE_RELEASE_DEMAND :
					    VME_DMA_ENGINE_RELEASE_DONE);

	switch (vp->vp_addrmod) {
	case VME_A16NPAMOD: /* 0x29 */
		dmap->d_addrmod = VMEIO_AM_A16 | VMEIO_AM_N;
		break;

	case VME_A16SAMOD: /* 0x2d */
		dmap->d_addrmod = VMEIO_AM_A16 | VMEIO_AM_S;
		break;

	case VME_A24NPAMOD: /* 0x39 */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_N | VMEIO_AM_SINGLE;
		break;

	case VME_A24SAMOD: /* 0x3d */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_S | VMEIO_AM_SINGLE;
		break;

	case VME_A32NPAMOD: /* 0x09 */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_N | VMEIO_AM_SINGLE;
		break;

	case VME_A32SAMOD: /* 0x0d */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_S | VMEIO_AM_SINGLE;
		break;

	case VME_A24NPBAMOD: /* 0x3b */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_N | VMEIO_AM_BLOCK;
		break;

	case VME_A24SBAMOD: /* 0x3f */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_S | VMEIO_AM_BLOCK;
		break;

	case VME_A32NPBAMOD: /* 0x0b */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_N | VMEIO_AM_BLOCK;
		break;

	case VME_A32SBAMOD: /* 0x0f */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_S | VMEIO_AM_BLOCK;
		break;

	case VME_A24ND64PBAMOD: /* 0x38 */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_N | VMEIO_AM_BLOCK | VMEIO_AM_D64;
		break;

	case VME_A24SD64BAMOD: /* 0x3c */
		dmap->d_addrmod = VMEIO_AM_A24 | VMEIO_AM_S | VMEIO_AM_BLOCK | VMEIO_AM_D64;
		break;

	case VME_A32ND64PBAMOD: /* 0x08 */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_N | VMEIO_AM_BLOCK | VMEIO_AM_D64;
		break;

	case VME_A32SD64BAMOD: /* 0x0c */
		dmap->d_addrmod = VMEIO_AM_A32 | VMEIO_AM_S | VMEIO_AM_BLOCK | VMEIO_AM_D64;
		break;

	default:
		return NULL;
	}

	switch (vp->vp_datumsz) {
	case VME_DS_BYTE:
		dmap->d_addrmod |= VMEIO_AM_D8;
		break;

	case VME_DS_HALFWORD:
		dmap->d_addrmod |= VMEIO_AM_D16;
		break;

	case VME_DS_WORD:
		dmap->d_addrmod |= VMEIO_AM_D32;
		break;

	case VME_DS_DBLWORD:
		dmap->d_addrmod |= VMEIO_AM_D64;
		break;

	default:
		return NULL;
	}

	if (vp->vp_block) {
		switch (vp->vp_addrmod) {
		case VME_A24NPBAMOD:
		case VME_A24SBAMOD:
		case VME_A32NPBAMOD:
		case VME_A32SBAMOD:
		case VME_A24ND64PBAMOD:
		case VME_A24SD64BAMOD:
		case VME_A32ND64PBAMOD:
		case VME_A32SD64BAMOD:
			break;
		default:
			return NULL;
		}
	}
	else {
		switch (vp->vp_addrmod) {
		case VME_A16NPAMOD:
		case VME_A16SAMOD:
		case VME_A24NPAMOD:
		case VME_A24SAMOD:
		case VME_A32NPAMOD:
		case VME_A32SAMOD:
			break;
		default:
			return NULL;
		}
	}

	return (udmaprm_t *)dmap;
}

/* ARGSUSED */
int
universe_vme_freeparms(dmaid_t *dp, udmaprm_t *dmap)
{
	(void)free(dmap);

	return 0;
}


int 
universe_vme_start(dmaid_t *dp, void *busaddr, udmaprm_t *dmamap)
{
	universe_vmeinfo_t	*uvp;
	dmaparms_t *dmap = (dmaparms_t *)dmamap;
	vme_dma_engine_transfer_t transfer;

	uvp = dp->d_businfo;

	if ((transfer = vme_dma_engine_transfer_alloc(uvp->uv_engine,
						      dmap->d_buf,
						      (iopaddr_t)busaddr,
						      dmap->d_size,
						      dmap->d_dir,
						      dmap->d_addrmod,
						      dmap->d_throttle,
						      dmap->d_release)) == 0)
		return 1;

	if (vme_dma_engine_schedule(uvp->uv_engine, transfer) == -1)
		return 1;

	if (vme_dma_engine_commit(uvp->uv_engine, 
				  VME_DMA_ENGINE_COMMIT_BLOCK, 
				  VME_DMA_ENGINE_WAIT_SPIN) == -1)
		return 1;

	return 0;
}
