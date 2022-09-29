#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/content_common.c,v 1.3 1995/10/03 03:06:11 ack Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "dlog.h"
#include "cldmgr.h"
#include "global.h"
#include "drive.h"

#define PREAMBLEMAX	3
#define QUERYMAX	1
#define CHOICEMAX	2
#define ACKMAX		3
#define POSTAMBLEMAX	3
#define DLOG_TIMEOUT	3600

bool_t
Media_prompt_change( drive_t *drivep )
{
	fold_t fold;
	char question[ 100 ];
	char *preamblestr[ PREAMBLEMAX ];
	size_t preamblecnt;
	char *querystr[ QUERYMAX ];
	size_t querycnt;
	char *choicestr[ CHOICEMAX ];
	size_t choicecnt;
	char *ackstr[ ACKMAX ];
	size_t ackcnt;
	char *postamblestr[ POSTAMBLEMAX ];
	size_t postamblecnt;
	ix_t doix;
	ix_t dontix;
	ix_t responseix;
	ix_t sigintix;

retry:
	preamblecnt = 0;
	fold_init( fold, "change media dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	assert( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	/* query: ask if media changed or declined
	 */
	sprintf( question,
		 "please change media in "
		 "drive %u\n",
		 drivep->d_index );
	querycnt = 0;
	querystr[ querycnt++ ] = question;
	assert( querycnt <= QUERYMAX );
	choicecnt = 0;
	dontix = choicecnt;
	choicestr[ choicecnt++ ] = "media change declined";
	doix = choicecnt;
	choicestr[ choicecnt++ ] = "media changed";
	assert( choicecnt <= CHOICEMAX );
	sigintix = IXMAX - 1;

	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,           /* hilitestr */
				       IXMAX,       /* hiliteix */
				       0,           /* defaultstr */
				       doix,        /* defaultix */
				       DLOG_TIMEOUT,
				       dontix,		/* timeout ix */
				       sigintix,	/* sigint ix */
				       dontix,		/* sighup ix */
				       dontix );	/* sigquit ix */
	ackcnt = 0;
	if ( responseix == doix ) {
		ackstr[ ackcnt++ ] = "examining new media\n";
	} else if ( responseix == dontix ) {
		ackstr[ ackcnt++ ] = "media change aborted\n";
	} else {
		assert( responseix == sigintix );
		ackstr[ ackcnt++ ] = "keyboard interrupt\n";
	}

	assert( ackcnt <= ACKMAX );
	dlog_multi_ack( ackstr,
			ackcnt );

	postamblecnt = 0;
	fold_init( fold, "end dialog", '-' );
	postamblestr[ postamblecnt++ ] = "\n";
	postamblestr[ postamblecnt++ ] = fold;
	postamblestr[ postamblecnt++ ] = "\n\n";
	assert( postamblecnt <= POSTAMBLEMAX );
	dlog_end( postamblestr,
		  postamblecnt );

	if ( responseix == sigintix ) {
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		sleep( 1 ); /* to allow main thread to begin dialog */
		mlog( MLOG_NORMAL | MLOG_BARE,
		      "" ); /* to block until main thread dialog complete */
		sleep( 1 ); /* to allow main thread to request children die */
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		mlog( MLOG_DEBUG,
		      "retrying media change dialog\n" );
		goto retry;
	}

	return responseix == doix;
}
