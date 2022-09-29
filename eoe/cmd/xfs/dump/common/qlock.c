#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/qlock.c,v 1.4 1997/08/14 20:39:00 prasadb Exp $"

#include <sys/types.h>
#include <errno.h>
#include <ulocks.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <task.h>


#include "types.h"
#include "qlock.h"
#include "mlog.h"

struct qlock {
	ix_t ql_ord;
		/* ordinal position of this lock
		 */
	pid_t ql_owner;
		/* who owns this lock
		 */
	ulock_t ql_uslockh;
		/* us lock handle
		 */
};

typedef struct  qlock qlock_t;
	/* internal qlock
	 */

#define QLOCK_SPINS			0x1000
	/* how many times to spin on lock before sleeping for it
	 */

#define QLOCK_THRDCNTMAX			256
	/* arbitrary limit on number of threads supported
	 */

static size_t qlock_thrdcnt;
	/* how many threads have checked in
	 */

typedef size_t ordmap_t;
	/* bitmap of ordinals. used to track what ordinals have
	 * been allocated.
	 */

#define ORDMAX					( 8 * sizeof( ordmap_t ))
	/* ordinals must fit into a wordsize bitmap
	 */

static ordmap_t qlock_ordalloced;
	/* to enforce allocation of only one lock to each ordinal value
	 */

struct thrddesc {
	pid_t td_pid;
	ordmap_t td_ordmap;
};
typedef struct thrddesc thrddesc_t;
static thrddesc_t qlock_thrddesc[ QLOCK_THRDCNTMAX ];
	/* holds the ordmap for each thread
	 */

#define QLOCK_ORDMAP_SET( ordmap, ord )	( ordmap |= 1U << ord )
	/* sets the ordinal bit in an ordmap
	 */

#define QLOCK_ORDMAP_CLR( ordmap, ord )	( ordmap &= ~( 1U << ord ))
	/* clears the ordinal bit in an ordmap
	 */

#define QLOCK_ORDMAP_GET( ordmap, ord )	( ordmap & ( 1U << ord ))
	/* checks if ordinal set in ordmap
	 */

#define QLOCK_ORDMAP_CHK( ordmap, ord )	( ordmap & (( 1U << ord ) - 1U ))
	/* checks if any bits less than ord are set in the ordmap
	 */

static usptr_t *qlock_usp;
	/* pointer to shared arena from which locks are allocated
	 */

static char *qlock_arenaroot = "xfsrestoreqlockarena";
	/* shared arena file name root
	 */

/* REFERENCED */
static bool_t qlock_inited = BOOL_FALSE;
	/* to sanity check initialization
	 */

/* forward declarations
 */
static void qlock_ordmap_add( pid_t pid );
static ordmap_t *qlock_ordmapp_get( pid_t pid );
static ix_t qlock_thrdix_get( pid_t pid );

bool_t
qlock_init( bool_t miniroot )
{
	char arenaname[ 100 ];
	/* REFERENCED */
	intgen_t nwritten;
	intgen_t rval;

	/* sanity checks
	 */
	assert( ! qlock_inited );

	/* initially no threads checked in
	 */
	qlock_thrdcnt = 0;

	/* initially no ordinals allocated
	 */
	qlock_ordalloced = 0;

	/* if miniroot, fake it
	 */
	if ( miniroot ) {
		qlock_inited = BOOL_TRUE;
		qlock_usp = 0;
		return BOOL_TRUE;
	}

	/* generate the arena name
	 */
	nwritten = sprintf( arenaname,
			    "/tmp/%s.%d",
			    qlock_arenaroot,
			    get_pid() );
	assert( nwritten > 0 );
	assert( ( size_t )nwritten < sizeof( arenaname ));

	/* configure shared arenas to automatically unlink on last close
	 */
	rval = usconfig( CONF_ARENATYPE, ( u_intgen_t )US_SHAREDONLY );
	if ( rval ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to configure shared arena for auto unlink: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* allocate a shared arena for the locks
	 */
	qlock_usp = usinit( arenaname );
	if ( ! qlock_usp ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to allocate shared arena for thread locks: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* now say we are initialized
	 */
	qlock_inited = BOOL_TRUE;

	/* add the parent thread to the thread list
	 */
	if ( ! qlock_thrdinit( )) {
		qlock_inited = BOOL_FALSE;
		return BOOL_FALSE;
	}

	return BOOL_TRUE;
}

bool_t
qlock_thrdinit( void )
{
	intgen_t rval;

	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* add thread to shared arena
	 */
	rval = usadd( qlock_usp );
	if ( rval ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to add thread to shared arena: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* add thread to ordmap list
	 */
	qlock_ordmap_add( get_pid() );

	return BOOL_TRUE;
}

qlockh_t
qlock_alloc( ix_t ord )
{
	qlock_t *qlockp;

	/* sanity checks
	 */
	assert( qlock_inited );

	/* verify the ordinal is not already taken, and mark as taken
	 */
	assert( ! QLOCK_ORDMAP_GET( qlock_ordalloced, ord ));
	QLOCK_ORDMAP_SET( qlock_ordalloced, ord );

	/* allocate lock memory
	 */
	qlockp = ( qlock_t * )calloc( 1, sizeof( qlock_t ));
	assert( qlockp );

	/* allocate a us lock: bypass if miniroot
	 */
	if ( qlock_usp ) {
		qlockp->ql_uslockh = usnewlock( qlock_usp );
		assert( qlockp->ql_uslockh );
	}

	/* assign the ordinal position
	 */
	qlockp->ql_ord = ord;

	return ( qlockh_t )qlockp;
}

void
qlock_lock( qlockh_t qlockh )
{
	qlock_t *qlockp = ( qlock_t * )qlockh;
	pid_t pid;
	ix_t thrdix;
	ordmap_t *ordmapp;
	/* REFERENCED */
	bool_t lockacquired;
	
	/* sanity checks
	 */
	assert( qlock_inited );

	/* bypass if miniroot
	 */
	if ( ! qlock_usp ) {
		return;
	}

	/* get the caller's pid and thread index
	 */
	pid = get_pid();

	thrdix = qlock_thrdix_get( pid );

	/* get the ordmap for this thread
	 */
	ordmapp = qlock_ordmapp_get( pid );

	/* assert that this lock not already held
	 */
	if ( QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_NOLOCK,
		      "lock already held: thrd %d pid %d ord %d map %x\n",
		      thrdix,
		      pid,
		      qlockp->ql_ord,
		      *ordmapp );
	}
	assert( ! QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord ));

	/* assert that no locks with a lesser ordinal are held by this thread
	 */
	if ( QLOCK_ORDMAP_CHK( *ordmapp, qlockp->ql_ord )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_NOLOCK,
		      "lock ordinal violation: thrd %d pid %d ord %d map %x\n",
		      thrdix,
		      pid,
		      qlockp->ql_ord,
		      *ordmapp );
	}
	assert( ! QLOCK_ORDMAP_CHK( *ordmapp, qlockp->ql_ord ));

	/* acquire the us lock
	 */
	lockacquired = uswsetlock( qlockp->ql_uslockh, QLOCK_SPINS );
	assert( lockacquired );

	/* verify lock is not already held
	 */
	assert( ! qlockp->ql_owner );

	/* add ordinal to this threads ordmap
	 */
	QLOCK_ORDMAP_SET( *ordmapp, qlockp->ql_ord );

	/* indicate the lock's owner
	 */
	qlockp->ql_owner = pid;
}

void
qlock_unlock( qlockh_t qlockh )
{
	qlock_t *qlockp = ( qlock_t * )qlockh;
	pid_t pid;
	ordmap_t *ordmapp;
	/* REFERENCED */
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );

	/* bypass if miniroot
	 */
	if ( ! qlock_usp ) {
		return;
	}

	/* get the caller's pid
	 */
	pid = get_pid();

	/* get the ordmap for this thread
	 */
	ordmapp = qlock_ordmapp_get( pid );

	/* verify lock is held by this thread
	 */
	assert( QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord ));
	assert( qlockp->ql_owner == pid );

	/* clear lock owner
	 */
	qlockp->ql_owner = 0;

	/* clear lock's ord from thread's ord map
	 */
	QLOCK_ORDMAP_CLR( *ordmapp, qlockp->ql_ord );
	
	/* release the us lock
	 */
	rval = usunsetlock( qlockp->ql_uslockh );
	assert( ! rval );
}

qsemh_t
qsem_alloc( ix_t cnt )
{
	usema_t *usemap;

	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* allocate a us semaphore
	 */
	usemap = usnewsema( qlock_usp, ( intgen_t )cnt );
	assert( usemap );

	return ( qsemh_t )usemap;
}

void
qsem_free( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;

	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* free the us semaphore
	 */
	usfreesema( usemap, qlock_usp );
}

void
qsemP( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* "P" the semaphore
	 */
	rval = uspsema( usemap );
	if ( rval != 1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to \"P\" semaphore: "
		      "rval == %d, errno == %d (%s)\n",
		      rval,
		      errno,
		      strerror( errno ));
	}
	assert( rval == 1 );
}

void
qsemV( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* "V" the semaphore
	 */
	rval = usvsema( usemap );
	if ( rval != 0 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to \"V\" semaphore: "
		      "rval == %d, errno == %d (%s)\n",
		      rval,
		      errno,
		      strerror( errno ));
	}
	assert( rval == 0 );
}

bool_t
qsemPwouldblock( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if equal to zero, no tokens left. if less than zero, other thread(s)
	 * currently waiting.
	 */
	if ( rval <= 0 ) {
		return BOOL_TRUE;
	} else {
		return BOOL_FALSE;
	}
}

size_t
qsemPavail( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if greater or equal to zero, no one is blocked and that is the number
	 * of resources available. if less than zero, absolute value is the
	 * number of blocked threads.
	 */
	if ( rval < 0 ) {
		return 0;
	} else {
		return ( size_t )rval;
	}
}

size_t
qsemPblocked( qsemh_t qsemh )
{
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if greater or equal to zero, no one is blocked. if less than zero,
	 * absolute value is the number of blocked threads.
	 */
	if ( rval < 0 ) {
		return ( size_t )( 0 - rval );
	} else {
		return 0;
	}
}

qbarrierh_t
qbarrier_alloc( void )
{
	barrier_t *barrierp;

	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	/* allocate a us barrier
	 */
	barrierp = new_barrier( qlock_usp );
	assert( barrierp );

	return ( qbarrierh_t )barrierp;
	
}

void
qbarrier( qbarrierh_t qbarrierh, size_t thrdcnt )
{
	barrier_t *barrierp = ( barrier_t * )qbarrierh;

	/* sanity checks
	 */
	assert( qlock_inited );
	assert( qlock_usp );

	barrier( barrierp, thrdcnt );
}

/* internal ordinal map abstraction
 */
static void
qlock_ordmap_add( pid_t pid )
{
	assert( qlock_thrdcnt < QLOCK_THRDCNTMAX );
	qlock_thrddesc[ qlock_thrdcnt ].td_pid = pid;
	qlock_thrddesc[ qlock_thrdcnt ].td_ordmap = 0;
	qlock_thrdcnt++;
}

static thrddesc_t *
qlock_thrddesc_get( pid_t pid )
{
	thrddesc_t *p;
	thrddesc_t *endp;

	for ( p = &qlock_thrddesc[ 0 ],
	      endp = &qlock_thrddesc[ qlock_thrdcnt ]
	      ;
	      p < endp
	      ;
	      p++ ) {
		if ( p->td_pid == pid ) {
			return p;
		}
	}

	assert( 0 );
	return 0;
}

static ordmap_t *
qlock_ordmapp_get( pid_t pid )
{
	thrddesc_t *p;
	p = qlock_thrddesc_get( pid );
	return &p->td_ordmap;
}

static ix_t
qlock_thrdix_get( pid_t pid )
{
	thrddesc_t *p;
	p = qlock_thrddesc_get( pid );
	assert( p >= &qlock_thrddesc[ 0 ] );
	return ( ix_t )( p - &qlock_thrddesc[ 0 ] );
}
