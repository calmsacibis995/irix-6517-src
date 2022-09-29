/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

/*
 * iomap.c - dma mapping, pio, and scsi dma support
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/dmamap.h>
#include <sys/cmn_err.h>
#include <sys/edt.h>
#include <sys/pio.h>

/*ARGSUSED*/
int
readadapters(uint_t bustype)
{
	return(0);		/* SN0 does the same thing */
}

/*ARGSUSED*/
piomap_t*
pio_mapalloc(uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
#ifdef DEBUG
	cmn_err(CE_WARN,"pio_mapalloc: called on IP30!\n");
#endif
	return(0);		/* Use new infrastructure, return failure! */
}

/*ARGSUSED*/
caddr_t 
pio_mapaddr(piomap_t* piomap, iopaddr_t addr)
{
#ifdef DEBUG
	cmn_err(CE_WARN,"pio_mapaddr: called on IP30!\n");
#endif
	return(0);		/* Use new infrastructure, return failure! */
}

/*ARGSUSED*/
void
pio_mapfree(piomap_t* piomap)
{
#ifdef DEBUG
	cmn_err(CE_WARN,"pio_free: called on IP30!\n");
#endif
}
