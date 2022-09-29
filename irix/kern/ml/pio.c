/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
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
#include <sys/pio.h>

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

/*
 * PIO functions for IP17.
 * They aren't really necessary for these architectures, but are provided
 * for driver compatibility with architectures like Everest where they
 * are required.
 */

/* ARGSUSED */
int
pio_bcopyin(piomap_t *map, iopaddr_t addr, void *dest, int count, int itmsz, 
	int flags)
{
	iopaddr_t off;

	/* bounds check */
	if( (addr < map->pio_iopaddr) ||
	    (addr >= map->pio_iopaddr + map->pio_size) )
	    return 0;

	off = addr - map->pio_iopaddr;
	if( off + count > map->pio_size )
		count = map->pio_size - off;

	return bcopyn((void *)pio_mapaddr(map, addr), dest, count, itmsz);
}

/* ARGSUSED */
int
pio_bcopyout(piomap_t *map, iopaddr_t addr, void *src, int count, int itmsz,
	int flags)
{
	iopaddr_t off;

	/* bounds check */
	if( (addr < map->pio_iopaddr) ||
	    (addr >= map->pio_iopaddr + map->pio_size) )
	    return 0;

	off = addr - map->pio_iopaddr;
	if( off + count > map->pio_size )
		count = map->pio_size - off;

	return bcopyn(src, (void *)pio_mapaddr(map, addr), count, itmsz);
}

#if !IP20 && !IP30
void
pio_orb_rmw(piomap_t *map, iopaddr_t addr, unsigned char data)
{
	orb_rmw(pio_mapaddr(map, addr), data);
}

void
pio_orh_rmw(piomap_t *map, iopaddr_t addr, unsigned short data)
{
	orh_rmw(pio_mapaddr(map, addr), data);
}

void
pio_orw_rmw(piomap_t *map, iopaddr_t addr, unsigned long data)
{
	orw_rmw(pio_mapaddr(map, addr), data);
}

void
pio_andb_rmw(piomap_t *map, iopaddr_t addr, unsigned char data)
{
	andb_rmw(pio_mapaddr(map, addr), data);
}

void
pio_andh_rmw(piomap_t *map, iopaddr_t addr, unsigned short data)
{
	andh_rmw(pio_mapaddr(map, addr), data);
}

void
pio_andw_rmw(piomap_t *map, iopaddr_t addr, unsigned long data)
{
	andw_rmw(pio_mapaddr(map, addr), data);
}
#endif /* VME or EISA */

int
pio_badaddr(piomap_t *map, iopaddr_t addr, int len)
{
	return badaddr(pio_mapaddr(map,addr),len);
}

int
pio_badaddr_val(piomap_t *map, iopaddr_t addr, int len, void *ret)
{
	return badaddr_val(pio_mapaddr(map,addr),len,ret);
}

int
pio_wbadaddr(piomap_t *map, iopaddr_t addr, int len)
{
	return wbadaddr(pio_mapaddr(map,addr),len);
}

int
pio_wbadaddr_val(piomap_t *map, iopaddr_t addr, int len, int val)
{
	return wbadaddr_val(pio_mapaddr(map,addr),len,
		(void *)((char *)&val+sizeof(int)-len));
}

