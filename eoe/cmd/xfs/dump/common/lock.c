#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/lock.c,v 1.1 1995/06/21 22:47:15 cbullis Exp $"

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "qlock.h"
#include "mlog.h"

static qlockh_t lock_qlockh = QLOCKH_NULL;

bool_t
lock_init( void )
{
	/* initialization sanity checks
	 */
	assert( lock_qlockh == QLOCKH_NULL );

	/* allocate a qlock
	 */
	lock_qlockh = qlock_alloc( QLOCK_ORD_CRIT );

	return BOOL_TRUE;
}

void
lock( void )
{
	qlock_lock( lock_qlockh );
}

void
unlock( void )
{
	qlock_unlock( lock_qlockh );
}
