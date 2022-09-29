#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <task.h>

#include "types.h"
#include "mlog.h"

#define STACKFUDGE	( 8 * pgsz )
#define STACKMARGIN	( 8 * pgsz )

struct stkchk {
	pid_t sc_pid;
	char *sc_initsp;
	char *sc_minsp;
	rlim_t sc_sz;
};

typedef struct stkchk stkchk_t;

/* REFERENCED */
static size_t stkchk_pidcntmax = 0;

static stkchk_t *stkchk_list;

static size_t stkchk_pidcnt = 0;

static rlim_t get_stacksz( void );

extern size_t pgsz;

void
stkchk_init( size_t maxpidcnt )
{
	assert( maxpidcnt > 0 );
	assert( stkchk_pidcntmax == 0 );
	stkchk_list = ( stkchk_t * )calloc( maxpidcnt, sizeof( stkchk_t ));
	assert( stkchk_list );
	stkchk_pidcntmax = maxpidcnt;
	assert( stkchk_pidcnt == 0 );
}

void
stkchk_register( void )
{
	char dummy;
	pid_t pid;
	stkchk_t *stkchkp;

	assert( stkchk_pidcntmax );
	pid = get_pid( );
	assert( stkchk_pidcnt < stkchk_pidcntmax );
	stkchkp = &stkchk_list[ stkchk_pidcnt++ ];
	stkchkp->sc_pid = get_pid( );
	stkchkp->sc_initsp = &dummy;
	stkchkp->sc_sz = get_stacksz( );
	assert( ( rlim_t )stkchkp->sc_initsp >= stkchkp->sc_sz );
	stkchkp->sc_minsp = stkchkp->sc_initsp - stkchkp->sc_sz;
	mlog( MLOG_DEBUG | MLOG_PROC,
	      "stack pid %u: "
	      "sz 0x%lx min 0x%lx max 0x%lx\n",
	      pid,
	      stkchkp->sc_sz,
	      stkchkp->sc_minsp,
	      stkchkp->sc_initsp );
}

int
stkchk( void )
{
	char dummy = 1; /* keep lint happy */
	pid_t pid;
	size_t pidix;
	stkchk_t *stkchkp;

	assert( stkchk_pidcntmax );
	pid = get_pid( );

	for ( pidix = 0 ; pidix < stkchk_pidcnt ; pidix++ ) {
		stkchkp = &stkchk_list[ pidix ];
		if ( stkchkp->sc_pid == pid ) {
			break;
		}
	}
	assert( pidix < stkchk_pidcnt );

	if ( &dummy > stkchkp->sc_initsp ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_PROC,
		      "stack underrun pid %d "
		      "sp 0x%lx min 0x%lx max 0x%lx\n",
		      pid,
		      &dummy,
		      stkchkp->sc_minsp,
		      stkchkp->sc_initsp );
		      return 3;
	} else if ( &dummy < stkchkp->sc_minsp + STACKFUDGE ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_PROC,
		      "stack overrun pid %d "
		      "sp 0x%lx min 0x%lx max 0x%lx\n",
		      pid,
		      &dummy,
		      stkchkp->sc_minsp,
		      stkchkp->sc_initsp );
		      return 2;
	} else if ( &dummy < stkchkp->sc_minsp + STACKFUDGE + STACKMARGIN ) {
		mlog( MLOG_VERBOSE | MLOG_WARNING | MLOG_PROC,
		      "stack near overrun pid %d "
		      "sp 0x%lx min 0x%lx max 0x%lx\n",
		      pid,
		      &dummy,
		      stkchkp->sc_minsp,
		      stkchkp->sc_initsp );
		      return 1;
	}

	mlog( MLOG_DEBUG | MLOG_PROC,
	      "stack ok pid %d "
	      "sp 0x%lx min 0x%lx max 0x%lx\n",
	      pid,
	      &dummy,
	      stkchkp->sc_minsp,
	      stkchkp->sc_initsp );
	return 0;
}

static rlim_t
get_stacksz( void )
{
	struct rlimit rlimit;
	/* REFERENCED */
	intgen_t rval;

	rval = getrlimit( RLIMIT_STACK, &rlimit );
	assert( ! rval );
	return rlimit.rlim_cur;
}
