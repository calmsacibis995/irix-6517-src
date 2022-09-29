#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/mlog.c,v 1.11 1997/08/14 20:38:54 prasadb Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <task.h>

#include "types.h"
#include "qlock.h"
#include "stream.h"
#include "mlog.h"
#include "cldmgr.h"
#include "getopt.h"

extern char *progname;
extern void usage( void );

#ifdef DUMP
static FILE *mlog_fp = stderr;
#endif /* DUMP */
#ifdef RESTORE
static FILE *mlog_fp = stdout;
#endif /* RESTORE */

intgen_t mlog_level_ss[ MLOG_SS_CNT ] = {
	MLOG_VERBOSE,
	MLOG_VERBOSE,
	MLOG_VERBOSE,
	MLOG_VERBOSE,
#ifdef RESTORE
	MLOG_VERBOSE,
#endif /* RESTORE */
	MLOG_VERBOSE
};

intgen_t mlog_showlevel = BOOL_FALSE;

intgen_t mlog_showss = BOOL_FALSE;

intgen_t mlog_timestamp = BOOL_FALSE;

static intgen_t mlog_sym_lookup( char * );

static size_t mlog_streamcnt;

static char mlog_levelstr[ 3 ]; 

#define MLOG_SS_NAME_MAX	10

static char mlog_ssstr[ MLOG_SS_NAME_MAX + 2 ];

static char mlog_tsstr[ 10 ];

struct mlog_sym {
	char *sym;
	intgen_t level;
};

typedef struct mlog_sym mlog_sym_t;

char *mlog_ss_names[ MLOG_SS_CNT ] = {
	"general",	/* MLOG_SS_GEN */
	"proc",		/* MLOG_SS_PROC */
	"drive",	/* MLOG_SS_DRIVE */
	"media",	/* MLOG_SS_MEDIA */
	"inventory",	/* MLOG_SS_INV */
#ifdef DUMP
	"inomap"	/* MLOG_SS_INOMAP */
#endif /* DUMP */
#ifdef RESTORE
	"tree"		/* MLOG_SS_TREE */
#endif /* RESTORE */
};

static mlog_sym_t mlog_sym[ ] = {
	"0",		MLOG_SILENT,
	"1",		MLOG_VERBOSE,
	"2",		MLOG_TRACE,
	"3",		MLOG_DEBUG,
	"4",		MLOG_NITTY,
	"5",		MLOG_NITTY + 1,
	"silent",	MLOG_SILENT,
	"verbose",	MLOG_VERBOSE,
	"trace",	MLOG_TRACE,
	"debug",	MLOG_DEBUG,
	"nitty",	MLOG_NITTY,
};

static qlockh_t mlog_qlockh;

bool_t
mlog_init1( intgen_t argc, char *argv[ ] )
{
	char **suboptstrs;
	ix_t ssix;
	ix_t soix;
	size_t vsymcnt;
	intgen_t c;

	/* initialize stream count. will be updated later by call to
	 * mlog_tell_streamcnt( ), after drive layer has counted drives
	 */
	mlog_streamcnt = 1;

	/* prepare an array of suboption token strings. this will be the
	 * concatenation of the subsystem names with the verbosity symbols.
	 * this array of char pts must be null terminated for getsubopt( 3 ).
	 */
	vsymcnt = sizeof( mlog_sym ) / sizeof( mlog_sym[ 0 ] );
	suboptstrs = ( char ** )calloc( MLOG_SS_CNT + vsymcnt + 1,
					sizeof( char * ));
	assert( suboptstrs );
	for ( soix = 0 ; soix < MLOG_SS_CNT ; soix++ ) {
		assert( strlen( mlog_ss_names[ soix ] ) <= MLOG_SS_NAME_MAX );
			/* unrelated, but opportunity to chk */
		suboptstrs[ soix ] = mlog_ss_names[ soix ];
	}
	for ( ; soix < MLOG_SS_CNT + vsymcnt ; soix++ ) {
		suboptstrs[ soix ] = mlog_sym[ soix - MLOG_SS_CNT ].sym;
	}
	suboptstrs[ soix ] = 0;

	/* set all of the subsystem log levels to -1, so we can see which
	 * subsystems where explicitly called out. those which weren't will
	 * be given the "general" level.
	 */
	for ( ssix = 0 ; ssix < MLOG_SS_CNT ; ssix++ ) {
		mlog_level_ss[ ssix ] = -1;
	}
	mlog_level_ss[ MLOG_SS_GEN ] = MLOG_VERBOSE;

	/* get command line options
	 */
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		char *options;

		switch ( c ) {
		case GETOPT_VERBOSITY:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				fprintf( stderr,
					 "%s: -%c argument missing\n",
					 progname,
					 optopt );
				usage( );
				return BOOL_FALSE;
			}
			options = optarg;
			while ( *options ) {
				intgen_t suboptix;
				char *valstr;

				suboptix = getsubopt( &options, 
						      suboptstrs,
						      &valstr );
				if ( suboptix < 0 ) {
					fprintf( stderr,
						 "%s: -%c argument invalid\n",
						 progname,
						 optopt );
					usage( );
					return BOOL_FALSE;
				}
				assert( ( ix_t )suboptix
					<
					MLOG_SS_CNT + vsymcnt );
				if ( suboptix < MLOG_SS_CNT ) {
					if ( ! valstr ) {
						fprintf( stderr,
							 "%s: -%c subsystem "
							 "subargument "
							 "%s requires a "
							 "verbosity value\n",
							 progname,
							 optopt,
						mlog_ss_names[ suboptix ] );
						usage( );
						return BOOL_FALSE;
					}
					ssix = ( ix_t )suboptix;
					mlog_level_ss[ ssix ] =
						    mlog_sym_lookup( valstr );
				} else {
					if ( valstr ) {
						fprintf( stderr,
							 "%s: -%c argument "
							 "does not require "
							 "a value\n",
							 progname,
							 optopt );
						usage( );
						return BOOL_FALSE;
					}
					ssix = MLOG_SS_GEN;
					mlog_level_ss[ ssix ] =
				    mlog_sym_lookup( suboptstrs[ suboptix ] );
				}
				if ( mlog_level_ss[ ssix ] < 0 ) {
					fprintf( stderr,
						 "%s: -%c argument "
						 "invalid\n",
						 progname,
						 optopt );
					usage( );
					return BOOL_FALSE;
				}
			}
			break;
		case GETOPT_SHOWLOGLEVEL:
			mlog_showlevel = BOOL_TRUE;
			break;
		case GETOPT_SHOWLOGSS:
			mlog_showss = BOOL_TRUE;
			break;
		case GETOPT_TIMESTAMP:
			mlog_timestamp = BOOL_TRUE;
			break;
		}
	}

	free( ( void * )suboptstrs );

	/* give subsystems not explicitly called out the "general" verbosity
	 */
	for ( ssix = 0 ; ssix < MLOG_SS_CNT ; ssix++ ) {
		if ( mlog_level_ss[ ssix ] < 0 ) {
			assert( mlog_level_ss[ ssix ] == -1 );
			assert( mlog_level_ss[ MLOG_SS_GEN ] >= 0 );
			mlog_level_ss[ ssix ] = mlog_level_ss[ MLOG_SS_GEN ];
		}
	}

	/* prepare a string for optionally displaying the log level
	 */
	mlog_levelstr[ 0 ] = 0;
	mlog_levelstr[ 1 ] = 0;
	mlog_levelstr[ 2 ] = 0;
	if ( mlog_showlevel ) {
		mlog_levelstr[ 0 ] = ':';
	}

#ifdef DUMP
	/* note if dump going to stdout. if so, can't
	 * send mlog output there. since at compile time
	 * mlog_fd set to stderr, see if we can switch
	 * to stdout.
	 */
	if ( optind >= argc ||  strcmp( argv[ optind ], "-" )) {
		mlog_fp = stdout;
	}
#endif /* DUMP */

	mlog_qlockh = QLOCKH_NULL;

	return BOOL_TRUE;
}

bool_t
mlog_init2( void )
{
	/* allocate a qlock
	 */
	mlog_qlockh = qlock_alloc( QLOCK_ORD_MLOG );

	return BOOL_TRUE;
}

void
mlog_tell_streamcnt( size_t streamcnt )
{
	mlog_streamcnt = streamcnt;
}

void
mlog_lock( void )
{
	qlock_lock( mlog_qlockh );
}

void
mlog_unlock( void )
{
	qlock_unlock( mlog_qlockh );
}

void
mlog( intgen_t levelarg, char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	mlog_va( levelarg, fmt, args );
	va_end( args );
}

void
mlog_va( intgen_t levelarg, char *fmt, va_list args )
{
	intgen_t level;
	ix_t ss;

	level = levelarg & MLOG_LEVELMASK;
	ss = ( ix_t )( ( levelarg & MLOG_SS_MASK ) >> MLOG_SS_SHIFT );

	assert( ss < MLOG_SS_CNT );
	if ( level > mlog_level_ss[ ss ] ) {
		return;
	}
	
	if ( ! ( levelarg & MLOG_NOLOCK )) {
		mlog_lock( );
	}

	if ( ! ( levelarg & MLOG_BARE )) {
		intgen_t streamix;
		streamix = stream_getix( get_pid() );

		if ( mlog_showss ) {
			sprintf( mlog_ssstr, ":%s", mlog_ss_names[ ss ] );
		} else {
			mlog_ssstr[ 0 ] = 0;
		}

		if ( mlog_timestamp ) {
			time_t now = time( 0 );
			struct tm *tmp = localtime( &now );
			sprintf( mlog_tsstr,
				 ":%02d.%02d.%02d",
				 tmp->tm_hour,
				 tmp->tm_min,
				 tmp->tm_sec );
			assert( strlen( mlog_tsstr ) < sizeof( mlog_tsstr ));
		} else {
			mlog_tsstr[ 0 ] = 0;
		}

		if ( mlog_showlevel ) {
			mlog_levelstr[ 0 ] = ':';
			if ( level > 9 ) {
				mlog_levelstr[ 1 ] = '?';
			} else {
				mlog_levelstr[ 1 ] = ( char )
						     ( level
						       +
						       ( intgen_t )'0' );
			}
		} else {
			mlog_levelstr[ 0 ] = 0;
		}
		if ( streamix != -1 && mlog_streamcnt > 1 ) {
			fprintf( mlog_fp,
				 "%s%s%s%s: "
#ifdef DUMP
				 "drive "
#endif /* DUMP */
#ifdef RESTORE
				 "drive "
#endif /* RESTORE */
				 "%d: ",
				 progname,
				 mlog_tsstr,
				 mlog_ssstr,
				 mlog_levelstr,
				 streamix );
		} else {
			fprintf( mlog_fp,
				 "%s%s%s%s: ",
				 progname,
				 mlog_tsstr,
				 mlog_ssstr,
				 mlog_levelstr );
		}
		if ( levelarg & MLOG_NOTE ) {
			fprintf( mlog_fp,
				 "NOTE: " );
		}
		if ( levelarg & MLOG_WARNING ) {
			fprintf( mlog_fp,
				 "WARNING: " );
		}
		if ( levelarg & MLOG_ERROR ) {
			fprintf( mlog_fp,
				 "ERROR: " );
		}
	}

	vfprintf( mlog_fp, fmt, args );
	fflush( mlog_fp );

	if ( ! ( levelarg & MLOG_NOLOCK )) {
		mlog_unlock( );
	}
}

static intgen_t
mlog_sym_lookup( char *sym )
{
	mlog_sym_t *p = mlog_sym;
	mlog_sym_t *ep = mlog_sym
			 +
			 sizeof( mlog_sym ) / sizeof( mlog_sym[ 0 ] );

	for ( ; p < ep ; p++ ) {
		if ( ! strcmp( sym, p->sym )) {
			return p->level;
		}
	}

	return -1;
}
