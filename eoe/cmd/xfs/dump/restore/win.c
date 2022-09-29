#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/win.c,v 1.7 1997/04/22 19:33:41 prasadb Exp $"

#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "types.h"
#include "mlog.h"
#include "bag.h"
#include "qlock.h"

extern size_t pgsz;
extern size_t pgmask;

/* window descriptor
 */
struct win {
	off64_t w_off;
		/* offset from first segment of segment mapped by this window
		 */
	void *w_p;
		/* window virtual address
		 */
	size_t w_refcnt;
		/* reference count
		 */
	time_t w_lasttouched;
		/* time stamp of most refcnt decrement
		 */
	struct win *w_nextp;
		/* LRU list forward linkage
		 */
	struct win *w_prevp;
		/* LRU list backward linkage
		 */
	bagelem_t w_bagelem;
		/* bag element cookie
		 */
};

typedef struct win win_t;

/* forward declarations
 */
static void win_bag_insert( win_t *winp );
static void win_bag_remove( win_t *winp );
static win_t *win_bag_find_off( off64_t off );
static void critical_init( void );
static void critical_begin( void );
static void critical_end( void );

/* transient state
 */
struct tran {
	intgen_t t_fd;
		/* file descriptor of backing store to be windowed
		 */
	off64_t t_firstoff;
		/* offset of first seg within backing store (for mmap( ))
		 */
	size_t t_segsz;
		/* backing store segment / window size
		 */
	size_t t_winmax;
		/* maximum number of windows to allocate
		 */
	size_t t_wincnt;
		/* number of windows allocated
		 */
	win_t *t_lruheadp;
		/* LRU head (re-use from this end)
		 */
	win_t *t_lrutailp;
		/* LRU tail (put here when no refs)
		 */
	bag_t *t_bagp;
		/* context for bag abstraction
		 */
	qlockh_t t_qlockh;
		/* for establishing critical regions
		 */

};

typedef struct tran tran_t;

static tran_t *tranp = 0;

void
win_init( intgen_t fd,
	  off64_t firstoff,
	  size_t segsz,
	  size_t winmax )
{
	/* validate parameters
	 */
	assert( ( firstoff & ( off64_t )pgmask ) == 0 );
	assert( ( segsz & pgmask ) == 0 );

	/* allocate and initialize transient state
	 */
	assert( tranp == 0 );
	tranp = ( tran_t * )calloc( 1, sizeof( tran_t ));
	assert( tranp );

	tranp->t_fd = fd;
	tranp->t_firstoff = firstoff;
	tranp->t_segsz = segsz;
	tranp->t_winmax = winmax;

	/* create a bag 
	 */
	tranp->t_bagp = bag_alloc( );

	/* initialize critical region enforcer
	 */
	critical_init( );
}

void
win_map( off64_t off, void **pp )
{
	size_t offwithinseg;
	off64_t segoff;
	win_t *winp;

	critical_begin( );

	/* calculate offset within segment
	 */
	offwithinseg = ( size_t )( off % ( off64_t )tranp->t_segsz );

	/* calculate offset of segment
	 */
	segoff = off - ( off64_t )offwithinseg;

	/* see if segment already mapped. if ref cnt zero,
	 * remove from LRU list.
	 */
	winp = win_bag_find_off( segoff );
	if ( winp ) {
		if ( winp->w_refcnt == 0 ) {
			assert( tranp->t_lruheadp );
			assert( tranp->t_lrutailp );
			if ( tranp->t_lruheadp == winp ) {
				if ( tranp->t_lrutailp == winp ) {
					tranp->t_lruheadp = 0;
					tranp->t_lrutailp = 0;
				} else {
					tranp->t_lruheadp = winp->w_nextp;
					tranp->t_lruheadp->w_prevp = 0;
				}
			} else {
				if ( tranp->t_lrutailp == winp ) {
					tranp->t_lrutailp = winp->w_prevp;
					tranp->t_lrutailp->w_nextp = 0;
				} else {
					winp->w_prevp->w_nextp = winp->w_nextp;
					winp->w_nextp->w_prevp = winp->w_prevp;
				}
			}
			winp->w_prevp = 0;
			winp->w_nextp = 0;
		} else {
			assert( ! winp->w_prevp );
			assert( ! winp->w_nextp );
		}
		winp->w_refcnt++;
		*pp = ( void * )( ( char * )( winp->w_p ) + offwithinseg );
		critical_end( );
		return;
	}

	/* if LRU list not empty, re-use descriptor at head.
	 * if LRU list is empty, allocate a new descriptor if
	 * not too many already.
	 */
	if ( tranp->t_lruheadp ) {
		/* REFERENCED */
		intgen_t rval;
		assert( tranp->t_lrutailp );
		winp = tranp->t_lruheadp;
		tranp->t_lruheadp = winp->w_nextp;
		if ( tranp->t_lruheadp ) {
			tranp->t_lruheadp->w_prevp = 0;
		} else {
			tranp->t_lrutailp = 0;
		}
		win_bag_remove( winp );
		rval = munmap( winp->w_p, tranp->t_segsz );
		assert( ! rval );
		memset( ( void * )winp, 0, sizeof( win_t ));
	} else if ( tranp->t_wincnt < tranp->t_winmax ) {
		winp = ( win_t * )calloc( 1, sizeof( win_t ));
		assert( winp );
		tranp->t_wincnt++;
	} else {
		assert( tranp->t_wincnt == tranp->t_winmax );
		*pp = 0;
		critical_end( );
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "all map windows in use. Check virtual memory limits\n" );
		return;
	}

	/* map the window
	 */
	assert( tranp->t_segsz >= 1 );
	assert( tranp->t_firstoff
		<=
		OFF64MAX - segoff - ( off64_t )tranp->t_segsz + 1ll );
	assert( ! ( tranp->t_segsz % pgsz ));
	assert( ! ( ( tranp->t_firstoff + segoff ) % ( off64_t )pgsz ));
	winp->w_p = mmap64( 0,
			    tranp->t_segsz,
			    PROT_READ | PROT_WRITE,
			    MAP_SHARED | MAP_AUTOGROW,
			    tranp->t_fd,
			    ( off64_t )( tranp->t_firstoff + segoff ));
	assert( winp->w_p );
	assert( winp->w_p != ( void * )( -1 ));
	winp->w_off = segoff;
	assert( winp->w_refcnt == 0 );
	winp->w_refcnt++;
	win_bag_insert( winp );

	*pp = ( void * )( ( char * )( winp->w_p ) + offwithinseg );

	critical_end( );
}

void
win_unmap( off64_t off, void **pp )
{
	size_t offwithinseg;
	off64_t segoff;
	win_t *winp;

	critical_begin( );

	/* calculate offset within segment
	 */
	offwithinseg = ( size_t )( off % ( off64_t )tranp->t_segsz );

	/* convert offset within range into window's offset within range
	 */
	segoff = off - ( off64_t )offwithinseg;

	/* verify window mapped
	 */
	winp = win_bag_find_off( segoff );
	assert( winp );

	/* validate p
	 */
	assert( pp );
	assert( *pp );
	assert( *pp >= winp->w_p );
	assert( *pp < ( void * )( ( char * )( winp->w_p ) + tranp->t_segsz ));

	/* decrement the reference count. if zero, place at tail of LRU list.
	 */
	assert( winp->w_refcnt > 0 );
	winp->w_refcnt--;
	winp->w_lasttouched = time( 0 );
	assert( ! winp->w_prevp );
	assert( ! winp->w_nextp );
	if ( winp->w_refcnt == 0 ) {
		if ( tranp->t_lrutailp ) {
			assert( tranp->t_lruheadp );
			winp->w_prevp = tranp->t_lrutailp;
			tranp->t_lrutailp->w_nextp = winp;
			tranp->t_lrutailp = winp;
		} else {
			assert( ! tranp->t_lruheadp );
			assert( ! winp->w_prevp );
			tranp->t_lruheadp = winp;
			tranp->t_lrutailp = winp;
		}
		assert( ! winp->w_nextp );
	}

	/* zero the caller's pointer
	 */
	*pp = 0;

	critical_end( );
}

static void
win_bag_insert( win_t *winp )
{
	bag_insert( tranp->t_bagp,
		    &winp->w_bagelem,
		    ( size64_t )winp->w_off,
		    ( void * )winp );
}

static void
win_bag_remove( win_t *winp )
{
	off64_t off;
	win_t *p;

	off = 0; /* keep lint happy */
	p = 0; /* keep lint happy */
	bag_remove( tranp->t_bagp,
		    &winp->w_bagelem,
		    ( size64_t * )&off,
		    ( void ** )&p );
	assert( off == winp->w_off );
	assert( p == winp );
}

static win_t *
win_bag_find_off( off64_t winoff )
{
	bagelem_t *bagelemp;
	win_t *p;

	p = 0; /* keep lint happy */
	bagelemp = bag_find( tranp->t_bagp,
			     ( size64_t )winoff,
			     ( void ** )&p );
	if ( ! bagelemp ) {
		return 0;
	}
	assert( p );
	assert( bagelemp == &p->w_bagelem );
	return p;
}

static void
critical_init( void )
{
	tranp->t_qlockh = qlock_alloc( QLOCK_ORD_WIN );
}

static void
critical_begin( void )
{
	qlock_lock( tranp->t_qlockh );
}

static void
critical_end( void )
{
	qlock_unlock( tranp->t_qlockh );
}
