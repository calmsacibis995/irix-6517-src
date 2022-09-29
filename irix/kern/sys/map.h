#ifndef __SYS_MAP_H__
#define __SYS_MAP_H__

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/map.h	10.2"*/
#ident	"$Revision: 3.25 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/sema.h>

/*
 *	struct map	X[]	.m_size		.m_addr
 *			---	------------	-----------
 *			[0]	mapsize(X)	unused
 *				# X[] unused
 *			[1]	map lock *	mapwant sv_t *
 *				map access	wait for free map space
 *
 *	  mapstart(X)->	[2]	# units		unit number
 *			 :	    :		  :
 *			[ ]	    0
 */

struct map
{
	unsigned long m_size;	/* number of units available */
	unsigned long m_addr;	/* address of first available unit */
};

#define	mapstart(X)		&X[2]		/* start of map array */

#define	mapsize(X)		X[0].m_size	/* number of empty slots */
						/* remaining in map array */
#define	mapdata(X)		{(X)-3, 0} , {0, 0}

#define	mapinit(X, Y, A, B)	X[0].m_size = (Y)-3;\
				X[1].m_size = (unsigned long) A;\
				X[1].m_addr = (unsigned long) B

#define maplock(X)		(((lock_t *) X[1].m_size))

#define mapout(X)		((sv_t *) X[1].m_addr)

#define	mapdefine(mapsize, nfree, start, maplock, mapout) \
	{ (mapsize)-3, 0 } ,\
	{ (unsigned long) maplock, (unsigned long) mapout },\
	{ (nfree), (start) },\
	{ 0, 0 }

extern uint malloc(struct map *, int);
extern uint malloc_wait(struct map *, int);
extern void mfree(struct map *, int, unsigned int);
extern ulong_t rmalloc(struct map *, size_t);
extern ulong_t rmalloc_wait(struct map *, size_t);
extern struct map *rmallocmap(ulong_t);
extern void rmfree(struct map *, size_t, ulong_t);
extern void rmfreemap(struct map *);

#define BITSPERWORD	(NBBY * sizeof(int))
#define BITSPERLONG	(NBBY * sizeof(long))

/*
 * bitmaps are managed either with m_lowbit OR the array of power-of-two
 * starting bits m_startb
 * We mark maps using the starb array by setting m_lowbit to -1;
 */
#define SPTROTORS  10
#define SPT_MAXCOLORS 8
struct bitmap {
	__psunsigned_t m_unit0;	/* value of unit[0] */
	char	*m_map;		/* the bitmap itself */
	uint_t	m_lock;		/* map protections */
	int	m_size;		/* map units available */
	int	m_lowbit;	/* low-unit mark (inclusive) */
	int	m_highbit;	/* high-unit mark (exclusive) */
	int	m_rotor;	/* bitmap rotor */
	int	m_count;	/* current count */
	int	m_startb[SPTROTORS];
#if R4000
	struct bitmap *m_color[SPT_MAXCOLORS];	/* array of color bitmaps */
#endif
};

struct bitmap_wait {
	int	waiters;
	sv_t	wait;
};

struct sysbitmap {
	uint_t	m_gen;			/* generation number */
	struct bitmap sptmap;		/* allocation map */
	struct bitmap stale_sptmap;	/* stale system virtual space */
	struct bitmap temp_sptmap;	/* in-transit system virtual space */
	struct bitmap aged_sptmap;	/* map for clock-aging */
	sema_t sptmaplock;		/* sema for manipulating maps */
	struct bitmap_wait spt_wait;
};

#define MAP_LOCK	0x1	/* normal access requires two bits to lock */
#define MAP_URGENT	0x2	/* urgency bit asserted */

extern void mergebitmaps(struct sysbitmap *, struct bitmap *, struct bitmap *);
extern void bmapswtch(struct bitmap *, struct bitmap *);
extern int clean_stale_sptmap(struct sysbitmap *, int);
extern void sptgetsizes(struct sysbitmap *, int *, int *, int *, int *);

extern struct sysbitmap sptbitmap;	/* Map for system virtual space. */

extern void sptfree(struct sysbitmap *, int, __psunsigned_t);
extern __psunsigned_t sptalloc(struct sysbitmap *, int, int, int, int);
extern int sptwait(struct sysbitmap *, int);
extern void sptwakeup(struct sysbitmap *);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_MAP_H__  */
