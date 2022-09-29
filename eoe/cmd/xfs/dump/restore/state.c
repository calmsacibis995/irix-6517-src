#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/state.c,v 1.4 1995/06/21 22:43:03 cbullis Exp $"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "stream.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "fs.h"
#include "openutil.h"
#include "state.h"

extern size_t pgsz;
extern size_t pgmask;

state_t *
state_create( char *pathname, size_t streamcnt, bool_t cumulative )
{
	intgen_t fd;
	state_t *statep;

	/* sanity checks
	 */
	assert( sizeof( state_t ) <= pgsz );
	assert( streamcnt <= STREAM_MAX );

	/* create a new state file.
	 */
	fd = open_erwp( pathname );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "creation of %s failed: %s\n",
		      pathname,
		      strerror( errno ));
		return 0;
	}

	/* mmap the file. must be a full page
	 */
	statep = ( state_t * ) mmap( 0,
				     pgsz,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED | MAP_AUTOGROW,
				     fd,
				     0 );
	if ( statep == ( state_t * )-1 ) {
		mlog( MLOG_DEBUG,
		      "mmap of %s failed: %s\n",
		      pathname,
		      strerror( errno ));
		( void )close( fd );
		( void )unlink( pathname );
		return 0;
	}

	/* zero the file
	 */
	( void )memset( ( void * )statep, 0, sizeof( *statep ));

	/* init
	 */
	statep->s_streamcnt = streamcnt;
	statep->s_cumulative = cumulative;

	return statep;
}

state_t *
state_open( char *pathname )
{
	intgen_t fd;
	state_t *statep;
	size_t streamix;

	/* sanity checks
	 */
	assert( sizeof( state_t ) <= pgsz );

	/* open existing state file.
	 */
	fd = open_rwp( pathname );
	if ( fd < 0 ) {
		if ( errno != ENOENT ) {
			mlog( MLOG_DEBUG,
			      "open of %s failed: %s\n",
			      pathname,
			      strerror( errno ));
		}
		return 0;
	}

	/* mmap the file. must be a full page
	 */
	statep = ( state_t * ) mmap( 0,
				     pgsz,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED | MAP_AUTOGROW,
				     fd,
				     0 );
	if ( statep == ( state_t * )-1 ) {
		mlog( MLOG_DEBUG,
		      "mmap of %s failed: %s\n",
		      pathname,
		      strerror( errno ));
		( void )close( fd );
		return 0;
	}

	/* adjust the interrupted streamcnt
	 */
	statep->s_interruptedstreamcnt = 0;
	for ( streamix = 0 ; streamix < statep->s_streamcnt ; streamix++ ) {
		if ( statep->s_stream[ streamix ].ss_interrupted ) {
			statep->s_interruptedstreamcnt++;
		}
	}

	return statep;
}
