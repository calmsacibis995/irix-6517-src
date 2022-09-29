#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/stream.c,v 1.5 1996/04/23 01:20:46 doucette Exp $"

#include <sys/types.h>
#include <ulocks.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "stream.h"
#include "lock.h"

#define PROCMAX	( STREAM_MAX * 2 + 1 )

struct spm {
	bool_t s_busy;
	pid_t s_pid;
	intgen_t s_ix;
};

typedef struct spm spm_t;

static spm_t spm[ STREAM_MAX * 3 ];

void
stream_init( void )
{
	/* REFERENCED */
	intgen_t rval;

	rval = ( intgen_t )usconfig( CONF_INITUSERS, PROCMAX );
	assert( rval >= 0 );
	( void )memset( ( void * )spm, 0, sizeof( spm ));

}

void
stream_register( pid_t pid, intgen_t streamix )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	assert( streamix < STREAM_MAX );

	lock( );
	for ( ; p < ep ; p++ ) {
		if ( ! p->s_busy ) {
			p->s_busy = BOOL_TRUE;
			break;
		}
	}
	unlock();

	assert( p < ep );
	if ( p >= ep ) return;

	p->s_pid = pid;
	p->s_ix = streamix;
}

void
stream_unregister( pid_t pid )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->s_pid == pid ) {
			p->s_pid = 0;
			p->s_ix = -1;
			p->s_busy = BOOL_FALSE;
			break;
		}
	}
	unlock();

	assert( p < ep );
}

intgen_t
stream_getix( pid_t pid )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	for ( ; p < ep ; p++ ) {
		if ( p->s_pid == pid ) {
			break;
		}
	}

	return ( p >= ep ) ? -1 : p->s_ix;
}

size_t
stream_cnt( void )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );
	size_t ixmap = 0;
	size_t ixcnt;
	size_t bitix;

	assert( sizeof( ixmap ) * NBBY >= STREAM_MAX );
	
	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->s_busy ) {
			ixmap |= ( size_t )1 << p->s_ix;
		}
	}
	unlock();

	ixcnt = 0;
	for ( bitix = 0 ; bitix < STREAM_MAX ; bitix++ ) {
		if ( ixmap & ( ( size_t )1 << bitix )) {
			ixcnt++;
		}
	}

	return ixcnt;
}
