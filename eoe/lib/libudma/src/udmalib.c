
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

#ident	"libdisk/diskheader.c $Revision: 1.3 $ of $Date: 1999/03/04 03:16:40 $"

#include <stdlib.h>

#include "udmalib.h"
#include "udmadefs.h"

struct bus_entry_s {
	int		 (*bus_open)(dmaid_t *);
	int		 (*bus_close)(dmaid_t *);
	void		*(*bus_allocbuf)(dmaid_t *, int);
	int		 (*bus_freebuf)(dmaid_t *, void*);
	int		 (*bus_freeparms)(dmaid_t *, udmaprm_t *);
	udmaprm_t	*(*bus_mkparms)(dmaid_t *, void*, void *, int);
	int		 (*bus_start)(dmaid_t *, void *, udmaprm_t *);
};
typedef struct bus_entry_s bus_entry_t;

#ifdef UNIVERSE

bus_entry_t bus_funcs[] = {
	{
		universe_vme_open,
		universe_vme_close,
		universe_vme_allocbuf,
		universe_vme_freebuf,
		universe_vme_freeparms,
		universe_vme_mkparms,
		universe_vme_start
	}
};

char *mach_tag = "Origin-based VME";

#else

bus_entry_t bus_funcs[] = {
	{
		vme_open,
		vme_close,
		vme_allocbuf,
		vme_freebuf,
		vme_freeparms,
		vme_mkparms,
		vme_start
	}
};

char *mach_tag = "Challenge-based VME";

#endif

#define MAXBUS	(sizeof(bus_funcs)/sizeof(bus_funcs[0]))

udmaid_t *
dma_open(int bus, int adap)
{
	register dmaid_t *dp;


	if( (bus < 0) || (bus > MAXBUS) )
		return NULL;

	if( (dp = malloc(sizeof(dmaid_t))) == NULL )
		return NULL;

	dp->d_bus = bus;
	dp->d_adap = adap;

	if( (*bus_funcs[dp->d_bus].bus_open)(dp) ) {
		(void)free(dp);
		return NULL;
	}

	return (udmaid_t *)dp;
}

int
dma_close(udmaid_t *udp)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return 1;
	
	if( (*bus_funcs[dp->d_bus].bus_close)(dp) )
		return 1;

	(void)free(dp);

	return 0;
}

void *
dma_allocbuf(udmaid_t *udp, int size)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return NULL;
	
	return (*bus_funcs[dp->d_bus].bus_allocbuf)(dp,size);
}

int
dma_freebuf(udmaid_t *udp, void *bp)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return 1;

	return (*bus_funcs[dp->d_bus].bus_freebuf)(dp,bp);
}

udmaprm_t *
dma_mkparms(udmaid_t *udp, void *dinfo, void *iobuf, int size)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return NULL;

	return (*bus_funcs[dp->d_bus].bus_mkparms)(dp,dinfo,iobuf,size);
}

int
dma_freeparms(udmaid_t *udp, udmaprm_t *dparms)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return 1;

	return (*bus_funcs[dp->d_bus].bus_freeparms)(dp,dparms);
}


int
dma_start(udmaid_t *udp, void *busaddr, udmaprm_t *dparms)
{
	register dmaid_t *dp = udp;


	if( dp->d_bus > MAXBUS )
		return 1;

	return (*bus_funcs[dp->d_bus].bus_start)(dp,busaddr,dparms);
}
