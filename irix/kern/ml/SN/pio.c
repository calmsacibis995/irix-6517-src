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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/pio.h>
#include <sys/vmereg.h>
#include <sys/sysmacros.h>
#include <sys/SN/addrs.h>
#include <sys/SN/xtalkaddrs.h>
/*#include <sys/XTALK/bridge.h> */
#include <sys/PCI/univ_vmestr.h>

piomap_t piomaplisthd;	/* A dummy piomap use as dlist head */
mutex_t	 piomaplock;

#define	PIO_MAPLOCK(x)		mutex_spinlock(&x)
#define	PIO_MAPUNLOCK(x,s)	mutex_spinunlock(&x, s)

extern int pio_mapfix(piomap_t *);

void
pio_init()
{
	piomaplisthd.pio_next = &piomaplisthd;
	piomaplisthd.pio_prev = &piomaplisthd;
	spinlock_init(&piomaplock, "piomaplock");
}
	
piomap_t *
pio_mapalloc(uint bus, uint adap, iospace_t *iospace, int flag, char *name)
{
	int	 i;
	piomap_t *piomap = (piomap_t*)kern_malloc(sizeof(piomap_t));

	if (piomap == 0) {
		ASSERT(0);
		return piomap;
	}

	/* fills the handle */
	piomap->pio_bus		= bus;
	piomap->pio_adap	= adap;
	piomap->pio_type	= iospace->ios_type;
	piomap->pio_iopaddr	= iospace->ios_iopaddr;
	piomap->pio_size	= iospace->ios_size;
	piomap->pio_flag	= flag;
	piomap->pio_reg		= PIOREG_NULL;
	piomap->pio_next	= (piomap_t*)0;
	piomap->pio_prev	= (piomap_t*)0;
	pio_seterrf(piomap, 0);	/* Attach the error function */
	
	for (i = 0; i < 7; i++)
		if (name[i])
			piomap->pio_name[i] = name[i];

	/*
	 * Attempt to fix map it even if the flag was not PIOMAP_FIXED.
	 * If the mapping is hard wired/coded, it is promoted to PIOMAP_FIXED.
	 */
	if (pio_mapfix(piomap)) {
		(void)kern_free((void*)piomap);
		return (piomap_t *)0;
	}

	/* insert piomap to piomaplist */
	i = PIO_MAPLOCK(piomaplock);
	piomap->pio_next = piomaplisthd.pio_next;
	piomap->pio_prev = &piomaplisthd;
	piomaplisthd.pio_next->pio_prev = piomap;
	piomaplisthd.pio_next = piomap;
	PIO_MAPUNLOCK(piomaplock, i);

	return piomap;
}

void
pio_mapfree(piomap_t *piomap)
{
	/* delete from the dlist */
	int	s;

	s = PIO_MAPLOCK(piomaplock);
	if (piomap->pio_next)
		piomap->pio_next->pio_prev = piomap->pio_prev;
	if (piomap->pio_prev)
		piomap->pio_prev->pio_next = piomap->pio_next;
	PIO_MAPUNLOCK(piomaplock, s);

	switch (piomap->pio_bus) {
	case ADAP_NULL:
	default:
		cmn_err(CE_NOTE, "Spurious piomap at 0x%x.\n", piomap);
		ASSERT(0);
		break;
	case ADAP_VME:
		pio_mapfree_vme(piomap);
		break;
	case ADAP_GFX:
			/* TBD */
	case ADAP_SCSI:
			/* TBD */
	case ADAP_LOCAL:		/* EPC for Everest */
			/* TBD */
		break;
	}

	kern_free(piomap);
}


/*
 * The mapping for the piomap must be valid for pio_mapaddr() to work.
 * Driver use pio_mapaddr() to obtain kv address only for PIOMAP_FIXED piomaps.
 * Otherwise pio_mapaddr() is called from pio_"access" functions only.
 */
caddr_t
pio_mapaddr(piomap_t *piomap, iopaddr_t addr)
{
	/* pio_vaddr should be consistent for fixed maps but
	 * won't be for non-fixed.  Care must be taken to call
	 * pio_map before pio_mapaddr() for non-fixed maps.
	 */
	return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
}



/*
 * These are not exported and are called from pio_"access" functions only.
 *
 * pio_maplock	- called to secure some h/w mapping resource.
 * pio_map	- to actually do the mapping.
 * pio_mapunlock- release the h/w mapping resource.
 * pio_mapfix	- try fix map the piomap.
 */

void
pio_maplock(piomap_t *piomap)
{
	switch (piomap->pio_bus) {
	case ADAP_NULL:
	default:
		cmn_err(CE_NOTE, "Spurious piomap at 0x%x.\n", piomap);
		ASSERT(0);
		break;
	case ADAP_VME:
		pio_maplock_vme(piomap);
		break;
	case ADAP_GFX:
			/* TBD */
	case ADAP_SCSI:
			/* TBD */
	case ADAP_LOCAL:		/* EPC for Everest */
			/* TBD */
		break;
	}
}

void
pio_mapunlock (piomap_t *piomap)
{
	switch (piomap->pio_bus) {
	case ADAP_NULL:
	default:
		cmn_err(CE_NOTE, "Spurious piomap at 0x%x.\n", piomap);
		ASSERT(0);
		break;
	case ADAP_VME:
		pio_mapunlock_vme(piomap);
		break;
	case ADAP_GFX:
			/* TBD */
	case ADAP_SCSI:
			/* TBD */
	case ADAP_LOCAL:		/* EPC for Everest */
			/* TBD */
		break;
	}
}

ulong
pio_map(piomap_t *piomap, iopaddr_t iopaddr, int size)
{
	/* do some simple range checking */
	if( (iopaddr < piomap->pio_iopaddr) || 
	    (iopaddr >= piomap->pio_iopaddr + piomap->pio_size) )
		return 0;

	switch (piomap->pio_bus) {
	case ADAP_NULL:
	default:
		cmn_err(CE_NOTE, "Spurious piomap at 0x%x.\n", piomap);
		ASSERT(0);
		return 0;
	case ADAP_VME:
		return pio_map_vme(piomap, iopaddr, size);
	case ADAP_GFX:
			/* TBD */
	case ADAP_SCSI:
			/* TBD */
	case ADAP_LOCAL:		/* EPC for Everest */
			/* TBD */
		return 0;
	}
}


int
pio_mapfix(piomap_t *piomap)
{
	int err;

	switch (piomap->pio_bus) {
	case ADAP_NULL:
	default:
		cmn_err(CE_NOTE, "Spurious piomap at 0x%x.\n", piomap);
		ASSERT(0);
		err = 1;
		break;
	case ADAP_VME:
		err = pio_mapfix_vme(piomap);
		break;
	case ADAP_GFX:
			/* TBD */
	case ADAP_SCSI:
			/* TBD */
	case ADAP_LOCAL:		/* EPC for Everest */
			/* TBD */
		err = 1;
		break;
	}

	return err;
}


/* There is no pio_mapunfix() since once fixed, the map stays forever. */


/*
 * The piomap access interface functions.
 */

int
pio_badaddr(piomap_t* piomap, iopaddr_t iopaddr, int len)
{
	int retval;
	if (piomap->pio_flag != PIOMAP_FIXED) {
		pio_maplock(piomap);
		(void)pio_map(piomap, iopaddr, len);
	}
	retval = badaddr((volatile void *)pio_mapaddr(piomap, iopaddr), len);
	if (piomap->pio_flag != PIOMAP_FIXED)
		pio_mapunlock(piomap);
	return retval;
}

int
pio_badaddr_val(piomap_t* piomap, iopaddr_t iopaddr, int len, void *ptr)
{
	int retval;
	if (piomap->pio_flag != PIOMAP_FIXED){
		pio_maplock(piomap);
		(void)pio_map(piomap, iopaddr, len);
	}
	retval = badaddr_val((volatile void *)pio_mapaddr(piomap, iopaddr),
				len, ptr);
	if (piomap->pio_flag != PIOMAP_FIXED)
		pio_mapunlock(piomap);
	return retval;
}

int
pio_wbadaddr(piomap_t* piomap, iopaddr_t iopaddr, int len)
{
	int retval;
	if (piomap->pio_flag != PIOMAP_FIXED) {
		pio_maplock(piomap);
		(void)pio_map(piomap, iopaddr, len);
	}
	retval = wbadaddr((volatile void *)pio_mapaddr(piomap, iopaddr), len);
	if (piomap->pio_flag != PIOMAP_FIXED)
		pio_mapunlock(piomap);
	return retval;
}

int
pio_wbadaddr_val(piomap_t* piomap, iopaddr_t iopaddr, int len, int value)
{
	int retval;
	if (piomap->pio_flag != PIOMAP_FIXED) {
		pio_maplock(piomap);
		(void)pio_map(piomap, iopaddr, len);
	}
	retval = wbadaddr_val((volatile void *)pio_mapaddr(piomap, iopaddr),
				len, (void *)((char *)&value+sizeof(int)-len));
	if (piomap->pio_flag != PIOMAP_FIXED)
		pio_mapunlock(piomap);
	return retval;
}


/* support routines for pio_bcopyin */
static int
bcopyb(register char *src, register char *dest, int count)
{
	register int i;

	for( i = 0 ; i < count ; i++ )
		*dest++ = *src++;

	return count;
}

static int
bcopyh(register short *src, register short *dest, int count)
{
	register int i;

	/* make sure counts and alignments are correct */
	if( (count & 1) || ((__psunsigned_t)src & 1) || 
		((__psunsigned_t)dest & 1) )
		return -1;

	for( i = 0 ; i < count/2 ; i++ )
		*dest++ = *src++;

	return count;
}

static int
bcopyw(register int *src, register int *dest, register int count)
{
	register int i;

	/* make sure counts and alignments are correct */
	if( (count & 3) || ((__psunsigned_t)src & 3) || 
		((__psunsigned_t)dest & 3) )
		return -1;

	for( i = 0 ; i < count/4 ; i++ )
		*dest++ = *src++;

	return count;
}

static int
bcopyd(register long long *src, register long long *dest, int count)
{
	register int i;

	/* make sure counts and alignments are correct */
	if( (count & 7) || ((__psunsigned_t)src & 7) || 
		((__psunsigned_t)dest & 7) )
		return -1;

	for( i = 0 ; i < count/8 ; i++ )
		*dest++ = *src++;

	return count;
}

static int
bcopyn(void *src, void *dst, int count, int itmsz)
{
	switch( itmsz ) {
		case 1:
			return bcopyb(src, dst, count);
		case 2:
			return bcopyh(src, dst, count);
		case 4:
			return bcopyw(src, dst, count);
		case 8:
			return bcopyd(src, dst, count);
	}

	return -1;
	
}

/* ARGSUSED */
int
pio_bcopyout(piomap_t	*piomap,
	     iopaddr_t	iopaddr,
	     void *	kvaddr,
	     int	size,
	     int	itmsz,
	     int	flag)
{
	void	*dst;
	int	 cnt;
	ulong	 totcnt, msize; 

	/* basic range checking */
	if( (iopaddr < piomap->pio_iopaddr) || 
	    (iopaddr >= piomap->pio_iopaddr + piomap->pio_size) )
		return 0;

	/* the simple case */
	if( piomap->pio_flag == PIOMAP_FIXED ) {
		dst = pio_mapaddr(piomap, iopaddr);
		return bcopyn(kvaddr, dst, size, itmsz);
	}

	/* allocate and lock the mapping */
	pio_maplock(piomap);

	for( totcnt = msize = cnt = 0 ; (totcnt < size) || (cnt != msize) ; 
		totcnt += cnt ) {

		msize = pio_map(piomap, iopaddr, size);

		if( msize == 0 )
			break;

		ASSERT(msize <= size);

		dst = pio_mapaddr(piomap, iopaddr);
		cnt = bcopyn(kvaddr, dst, msize, itmsz);

		iopaddr += cnt;
		kvaddr = (void *)((__psunsigned_t)kvaddr + cnt);
	}

	/* release the map register */
	pio_mapunlock(piomap);

	return totcnt;
}


/* ARGSUSED */
int
pio_bcopyin(piomap_t	*piomap,
	    iopaddr_t	iopaddr,
	    void	*kvaddr,
	    int		size,
	    int		itmsz,
	    int		flag)
{
	void	*src;
	int	 cnt;
	ulong	 totcnt, msize; 

	/* basic range checking */
	if( (iopaddr < piomap->pio_iopaddr) || 
	    (iopaddr >= piomap->pio_iopaddr + piomap->pio_size) )
		return 0;

	/* the simple case */
	if( piomap->pio_flag == PIOMAP_FIXED ) {
		src = pio_mapaddr(piomap, iopaddr);
		return bcopyn(src, kvaddr, size, itmsz);
	}

	/* allocate and lock the mapping */
	pio_maplock(piomap);

	for( totcnt = msize = cnt = 0 ; (totcnt < size) || (cnt != msize) ; 
		totcnt += cnt ) {

		msize = pio_map(piomap, iopaddr, size);

		if( msize == 0 )
			break;

		ASSERT(msize <= size);

		src = pio_mapaddr(piomap, iopaddr);
		cnt = bcopyn(src, kvaddr, msize, itmsz);

		iopaddr += cnt;
		kvaddr = (void *)((__psunsigned_t)kvaddr + cnt);
	}

	/* release the map register */
	pio_mapunlock(piomap);

	return totcnt;
}


/*
 * pio_ioaddr:
 * Given the bystype, ioaddr and adap, findout if there is any board which
 * uses this address, and return the corresponding piomap
 *
 * NOTE: adap should be internal adapter. External adapter can have 
 * different values for the same internal adapter. So We use internal
 * adapter for consistency.
 */
piomap_t *
pio_ioaddr(int bustype, iobush_t iadap, iopaddr_t addr, piomap_t *p)
{
	piomap_t *pmap = piomaplisthd.pio_next;
	iospace_t 	*ios;
	iopaddr_t	addr1;
	int		s;

	s = PIO_MAPLOCK(piomaplock);

	if (p){
		for (; pmap != &piomaplisthd; pmap = pmap->pio_next)
			if (pmap == p)
				break;
		if (pmap == &piomaplisthd){
			PIO_MAPUNLOCK(piomaplock, s);
			return 0;
		}
		pmap = pmap->pio_next;
	}

	for (; pmap != &piomaplisthd; pmap = pmap->pio_next){
		if ((pmap->pio_bus == bustype) && 
		    (unvme_xtoiadap(pmap->pio_adap) == iadap)){

			switch(pmap->pio_type){
			case VME_A16S:
			case VME_A16NP: addr1 = 0xffff;
					break;
			case VME_A24S:
			case VME_A24NP: addr1 = 0xffffff;
					break;
			case VME_A32S:
			case VME_A32NP: addr1 = 0xffffffff;
					break;
			default:	addr1 = 0;
			}
			addr1 &= addr;

#define	PG_LOWER(x)	(io_ctob(io_btoct(x)))	/* bump to low page boundary */
#define	PG_UPPER(x)	(io_ctob(io_btoc(x)))	/* bump to hi page boundary */

			ios = &pmap->pio_iospace;
			if ((addr1 >= PG_LOWER(ios->ios_iopaddr)) && 
		    	(addr1 < PG_UPPER(ios->ios_iopaddr + ios->ios_size)))
		    		break;
		}
	}
	PIO_MAPUNLOCK(piomaplock, s);

	if (pmap == &piomaplisthd)
		return 0;
	
	return pmap;
}

