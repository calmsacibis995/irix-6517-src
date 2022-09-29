#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/dlog.c,v 1.13 1997/08/14 20:38:41 prasadb Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bstring.h>
#include <string.h>
#include <assert.h>
#include <task.h>

#include "types.h"
#include "mlog.h"
#include "dlog.h"
#include "getopt.h"

extern bool_t miniroot;
extern pid_t parentpid;

static int dlog_ttyfd = -1;
static bool_t dlog_allowed_flag = BOOL_FALSE;
static bool_t dlog_timeouts_flag = BOOL_FALSE;
static char *promptstr = " -> ";

static bool_t promptinput( char *buf,
			   size_t bufsz,
			   ix_t *exceptionixp,
			   time_t timeout,
			   ix_t timeoutix,
			   ix_t sigintix,
			   ix_t sighupix,
			   ix_t sigquitix,
			   char *fmt, ... );
static void dlog_string_query_print( void *ctxp, char *fmt, ... );
static void sighandler( int );

bool_t
dlog_init( int argc, char *argv[ ] )
{
	intgen_t c;

	/* can only call once
	 */
	assert( dlog_ttyfd == -1 );

	/* initially allow dialog, use stdin fd
	 */
	dlog_ttyfd = 0; /* stdin */
	dlog_allowed_flag = BOOL_TRUE;
	dlog_timeouts_flag = BOOL_TRUE;

	/* look for command line option claiming the operator knows
	 * what's up.
	 */
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
		case GETOPT_FORCE:
			dlog_ttyfd = -1;
			dlog_allowed_flag = BOOL_FALSE;
			break;
		case GETOPT_NOTIMEOUTS:
			dlog_timeouts_flag = BOOL_FALSE;
			break;
		}
	}
#ifdef RESTORE
	/* look to see if restore source coming in on
	 * stdin. If so , try to open /dev/tty for dialogs.
	 */
	if ( optind < argc && ! strcmp( argv[ optind ], "-" )) {
		dlog_ttyfd = open( "/dev/tty", O_RDWR );
		if ( dlog_ttyfd < 0 ) {
			perror("/dev/tty");
			dlog_ttyfd = -1;
			dlog_allowed_flag = BOOL_FALSE;
		}
	}
#endif /* RESTORE */

#ifdef CHKSTDIN
	/* if stdin is not a tty, don't allow dialogs
	 */
	if ( dlog_allowed_flag ) {
		stat_t statbuf;
		int rval;

		assert( dlog_ttyfd >= 0 );
		rval = fstat( dlog_ttyfd, &statbuf );
		if ( rval ) {
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "could not fstat stdin (fd %d): %s (%d)\n",
			      dlog_ttyfd,
			      strerror( errno ),
			      errno );
		} else {
			mlog( MLOG_DEBUG,
			      "stdin mode 0x%x\n",
			      statbuf.st_mode );
		}
	}
#endif /* CHKSTDIN */

	return BOOL_TRUE;
}

bool_t
dlog_allowed( void )
{
	return dlog_allowed_flag;
}

void
dlog_desist( void )
{
	dlog_allowed_flag = BOOL_FALSE;
	dlog_ttyfd = -1;
}

intgen_t
dlog_fd( void )
{
	return dlog_ttyfd;
}

void
dlog_begin( char *preamblestr[ ], size_t preamblecnt )
{
	size_t ix;

	mlog_lock( );
	for ( ix = 0 ; ix < preamblecnt ; ix++ ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      preamblestr[ ix ] );
	}
}

void
dlog_end( char *postamblestr[ ], size_t postamblecnt )
{
	size_t ix;

	for ( ix = 0 ; ix < postamblecnt ; ix++ ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      postamblestr[ ix ] );
	}
	mlog_unlock( );
}

ix_t
dlog_multi_query( char *querystr[ ],
		  size_t querycnt,
		  char *choicestr[ ],
		  size_t choicecnt,
		  char *hilitestr,
		  size_t hiliteix,
		  char *defaultstr,
		  ix_t defaultix,
		  time_t timeout,
		  ix_t timeoutix,
		  ix_t sigintix,
		  ix_t sighupix,
		  ix_t sigquitix )
{
	size_t ix;
	char buf[ 100 ];
	char *prepromptstr;

	/* sanity
	 */
	assert( dlog_allowed_flag );
	assert( choicecnt < 9 );

	/* display query description strings
	 */
	for ( ix = 0 ; ix < querycnt ; ix++ ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      querystr[ ix ] );
	}

	/* display the choices: NOTE: display is 1-based, code intfs 0-based!
	 */
	for ( ix = 0 ; ix < choicecnt ; ix++ ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      "%u: %s",
		      ix + 1,
		      choicestr[ ix ] );
		if ( ix == hiliteix ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "%s",
			      hilitestr ?  hilitestr : " *" );
		}
		if ( ix == defaultix ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "%s",
			      defaultstr ?  defaultstr : " (default)" );
		}
		if ( dlog_timeouts_flag
		     &&
		     timeoutix != IXMAX
		     &&
		     ix == timeoutix ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      " (timeout in %u sec)",
			      timeout );
		}
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      "\n" );
	}

	/* read the tty until we get a proper answer or are interrupted
	 */
	prepromptstr = "";
	for ( ; ; ) {
		ix_t exceptionix;
		bool_t ok;

		/* prompt and accept input
		 */
		ok = promptinput( buf,
				  sizeof( buf ),
				  &exceptionix,
				  timeout,
				  timeoutix,
				  sigintix,
				  sighupix,
				  sigquitix,
				  prepromptstr,
				  choicecnt );
		if ( ok ) {
			if ( ! strlen( buf )) {
				return defaultix;
			}
			if ( strlen( buf ) != 1 ) {
				prepromptstr =
				    "please enter a single "
				    "digit response (1 to %d)";
				continue;
			}
			if ( buf[ 0 ] < '1'
			     ||
			     buf[ 0 ] >= '1' + ( u_char_t )choicecnt ) {
				prepromptstr =
				      "please enter a single digit "
				      "between 1 and %d inclusive ";
				continue;
			}
			return ( size_t )( buf[ 0 ] - '1' );
		} else {
			return exceptionix;
		}
	}
	/* NOTREACHED */
}

void
dlog_multi_ack( char *ackstr[ ], size_t ackcnt )
{
	size_t ix;

	for ( ix = 0 ; ix < ackcnt ; ix++ ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      ackstr[ ix ] );
	}
}

ix_t
dlog_string_query( dlog_ucbp_t ucb, /* user's print func */
		   void *uctxp,	  /* user's context for above */
		   char *bufp,	  /* typed string returned in */
		   size_t bufsz,	  /* buffer size */
		   time_t timeout,  /* secs b4 giving up */
		   ix_t timeoutix,
		   ix_t sigintix,
		   ix_t sighupix,
		   ix_t sigquitix,
		   ix_t okix )
{
	ix_t exceptionix;
	bool_t ok;

	/* call the caller's callback with his context, print context, and
	 * print operator
	 */
	( * ucb )( uctxp, dlog_string_query_print, 0 );

	/* if called for, print the timeout and a newline.
	 * if not, print just a newline
	 */
	if ( dlog_timeouts_flag && timeoutix != IXMAX ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      " (timeout in %u sec)\n",
		      timeout );
	} else {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      "\n" );
	}

	/* prompt and accept input
	 */
	ok = promptinput( bufp,
			  bufsz,
			  &exceptionix,
			  timeout,
			  timeoutix,
			  sigintix,
			  sighupix,
			  sigquitix,
			  "" );
	if ( ok ) {
		return okix;
	} else {
		return exceptionix;
	}
}

void
dlog_string_ack( char *ackstr[ ], size_t ackcnt )
{
	dlog_multi_ack( ackstr, ackcnt );
}

/* ok that this is a static, since used under mutual exclusion lock
 */
static int dlog_signo_received;

static void
sighandler( int signo )
{
	dlog_signo_received = signo;
}

/* ARGSUSED */
static void
dlog_string_query_print( void *ctxp, char *fmt, ... )
{
	va_list args;

	assert( ! ctxp );

	va_start( args, fmt );
	mlog_va( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE, fmt, args );
	va_end( args );
}

static bool_t
promptinput( char *buf,
	     size_t bufsz,
	     ix_t *exceptionixp,
	     time_t timeout,
	     ix_t timeoutix,
	     ix_t sigintix,
	     ix_t sighupix,
	     ix_t sigquitix,
	     char *fmt,
	     ... )
{
	va_list args;
	u_intgen_t alarm_save;
	SIG_PF sigalrm_save;
	SIG_PF sigint_save;
	SIG_PF sighup_save;
	SIG_PF sigterm_save;
	SIG_PF sigquit_save;
	intgen_t nread;
	pid_t pid = get_pid( );

	/* display the pre-prompt
	 */
	va_start( args, fmt );
	mlog_va( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE, fmt, args );
	va_end( args );

	/* display standard prompt
	 */
#ifdef NOTYET
	if ( dlog_timeouts_flag && timeoutix != IXMAX ) {
		mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
		      "(timeout in %d sec)",
		      timeout );
	}
#endif /* NOTYET */
	mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE, promptstr );

	/* set up signal handling
	 */
	dlog_signo_received = -1;
	if ( dlog_timeouts_flag && timeoutix != IXMAX ) {
		if ( pid == parentpid && ! miniroot ) {
			( void )sigrelse( SIGALRM );
		}
		sigalrm_save = sigset( SIGALRM, sighandler );
		alarm_save = alarm( ( u_intgen_t )timeout );
	}
	if ( sigintix != IXMAX ) {
		if ( pid == parentpid && ! miniroot ) {
			( void )sigrelse( SIGINT );
		}
		sigint_save = sigset( SIGINT, sighandler );
	}
	if ( sighupix != IXMAX ) {
		if ( pid == parentpid && ! miniroot ) {
			( void )sigrelse( SIGHUP );
		}
		sighup_save = sigset( SIGHUP, sighandler );
		if ( pid == parentpid && ! miniroot ) {
			( void )sigrelse( SIGTERM );
		}
		sigterm_save = sigset( SIGTERM, sighandler );
	}
	if ( sigquitix != IXMAX ) {
		if ( pid == parentpid && ! miniroot ) {
			( void )sigrelse( SIGQUIT );
		}
		sigquit_save = sigset( SIGQUIT, sighandler );
	}

	/* wait for input, timeout, or interrupt
	 */
	assert( dlog_ttyfd >= 0 );
	nread = read( dlog_ttyfd, buf, bufsz - 1 );

	/* restore signal handling
	 */
	if ( sigquitix != IXMAX ) {
		( void )sigset( SIGQUIT, sigquit_save );
		if ( pid == parentpid && ! miniroot ) {
			( void )sighold( SIGQUIT );
		}
	}
	if ( sighupix != IXMAX ) {
		( void )sigset( SIGHUP, sighup_save );
		if ( pid == parentpid && ! miniroot ) {
			( void )sighold( SIGHUP );
		}
		( void )sigset( SIGTERM, sigterm_save );
		if ( pid == parentpid && ! miniroot ) {
			( void )sighold( SIGTERM );
		}
	}
	if ( sigintix != IXMAX ) {
		( void )sigset( SIGINT, sigint_save );
		if ( pid == parentpid && ! miniroot ) {
			( void )sighold( SIGINT );
		}
	}
	if ( dlog_timeouts_flag && timeoutix != IXMAX ) {
		( void )alarm( alarm_save );
		( void )sigset( SIGALRM, sigalrm_save );
		if ( pid == parentpid && ! miniroot ) {
			( void )sighold( SIGALRM );
		}
	}
	
	/* check for timeout or interrupt
	 */
	if ( nread < 0 ) {
		if ( dlog_signo_received == SIGALRM ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "timeout\n" );
			*exceptionixp = timeoutix;
		} else if ( dlog_signo_received == SIGINT ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "keyboard interrupt\n" );
			*exceptionixp = sigintix;
		} else if ( dlog_signo_received == SIGHUP ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "hangup\n" );
			*exceptionixp = sighupix;
		} else if ( dlog_signo_received == SIGTERM ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "terminate\n" );
			*exceptionixp = sighupix;
		} else if ( dlog_signo_received == SIGQUIT ) {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "keyboard quit\n" );
			*exceptionixp = sigquitix;
		} else {
			mlog( MLOG_NORMAL | MLOG_NOLOCK | MLOG_BARE,
			      "unknown signal during dialog: %d\n",
			      dlog_signo_received );
			*exceptionixp = sigquitix;
		}
		return BOOL_FALSE;
	} else if ( nread == 0 ) {
		*exceptionixp = timeoutix;
		if ( bufsz > 0 ) {
			buf[ 0 ] = 0;
		}
		return BOOL_FALSE;
	} else {
		assert( dlog_signo_received == -1 );
		assert( ( size_t )nread < bufsz );
		assert( buf[ nread - 1 ] == '\n' );
		buf[ nread - 1 ] = 0;
		*exceptionixp = 0;
		return BOOL_TRUE;
	}
}
