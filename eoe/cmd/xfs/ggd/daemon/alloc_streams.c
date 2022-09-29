#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/alloc_streams.c,v 1.7 1997/02/07 20:10:44 kanoj Exp $"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <time.h>
#include <grio.h>
#include <sys/mkdev.h>
#include <sys/fs/xfs_inum.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/sysmp.h>
#include "ggd.h"
#include "griotab.h"

/*
 * alloc_streams.c
 *
 *
 */

int totalstreamcount;
int remainingstreamcount;

extern int license_streamcount( void );


/*
 * init_stream_count
 *
 *
 * RETURNS:
 *	1 if streams were allocated
 *	0 if no steams were alloated
 */
int
init_stream_count( void )
{
	int 		count = 0, ret;

	count = license_streamcount();

	if (count == UNLIMITED_GRIO_STREAMS) {
		printf("Total number of licensed grio streams is unlimited.\n");
	} else {
		printf("Total number of licensed grio streams is %d.\n",count);
	}

	if (count) {
		totalstreamcount = count;
		remainingstreamcount = count;
		ret = 0;
	} else {
		ret = 1;
	}

	return( ret );
}

/*
 * can_allocate_stream
 *
 * RETURNS:
 *	number of remaining unused streams
 */
int
can_allocate_stream( void )
{
	return(remainingstreamcount);
}

/*
 * allocate_stream
 *
 *
 * RETURNS:
 *	none
 */
void
allocate_stream( void )
{
	remainingstreamcount--;
	if (remainingstreamcount < 0 ) {
		dbgprintf(1,("ggd stream count too low. %d \n",
			remainingstreamcount));
		abort();
	}
}

/*
 * deallocate_stream
 *
 * RETURNS:
 *	none
 */
void
deallocate_stream( void )
{
	remainingstreamcount++;
	if (remainingstreamcount > totalstreamcount) {
		dbgprintf(1,("ggd stream count too high. %d, %d \n",
			remainingstreamcount,
			totalstreamcount));
		abort();
	}
}
