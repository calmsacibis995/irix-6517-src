#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/scan/RCS/main.c,v 1.3 1996/04/23 01:21:15 doucette Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dirent.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stkchk.h"
#include "exit.h"
#include "types.h"
#include "stream.h"
#include "cldmgr.h"
#include "jdm.h"
#include "util.h"
#include "getopt.h"
#include "mlog.h"
#include "qlock.h"
#include "lock.h"
#include "dlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"

/* structure definitions used locally ****************************************/

#define MINSTACKSZ	0x02000000
#define MAXSTACKSZ	0x08000000

typedef enum {
	ACTION_NEXT,
	ACTION_PREV,
	ACTION_QUIT,
	ACTION_TERM,
	ACTION_REWIND,
	ACTION_ERROR
} action_t;

/* declarations of externally defined global symbols *************************/

static rlim64_t minstacksz;
static rlim64_t maxstacksz;

/* forward declarations of locally defined global functions ******************/

void usage( void );

/* forward declarations of locally defined static functions ******************/

static intgen_t content_scan( void );
static action_t action_dialog( bool_t termallowedpr );
static bool_t write_terminator( drive_t *drivep );
static void display_mfile_hdr( drive_t *drivep );
static bool_t set_rlimits( void );

/* definition of locally defined global variables ****************************/

intgen_t version = 1;
intgen_t subversion = 0;
char *progname = 0;			/* used in all error output */
size_t pgsz;
size_t pgmask;
char *homedir;
pid_t parentpid;
bool_t miniroot = BOOL_TRUE;
bool_t pipeline = BOOL_FALSE;


/* definition of locally defined static variables *****************************/


/* definition of locally defined global functions ****************************/

int
main( int argc, char *argv[] )
{
	global_hdr_t *gwhdrtemplatep;
	intgen_t exitcode;
	bool_t ok;
	int c;
	rlim64_t tmpstacksz;

	/* record the command name used to invoke
	 */
	progname = argv[ 0 ];

	/* mlog init phase 1
	 */
	ok = mlog_init1( argc, argv );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	/* scan the command line for the stacksz options.
	 */
	minstacksz = MINSTACKSZ;
	maxstacksz = MAXSTACKSZ;
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
                case GETOPT_MINSTACKSZ:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return EXIT_ERROR;
			}
			errno = 0;
			tmpstacksz = strtoull( optarg, 0, 0 );
			if ( tmpstacksz == ULONGLONG_MAX
			     ||
			     errno == ERANGE ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
				      "-%c argument (%s) invalid\n",
				      optopt,
				      optarg );
				usage( );
				return EXIT_ERROR;
			}
			minstacksz = tmpstacksz;
			break;
                case GETOPT_MAXSTACKSZ:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return EXIT_ERROR;
			}
			errno = 0;
			tmpstacksz = strtoull( optarg, 0, 0 );
			if ( tmpstacksz == ULONGLONG_MAX
			     ||
			     errno == ERANGE ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
				      "-%c argument (%s) invalid\n",
				      optopt,
				      optarg );
				usage( );
				return EXIT_ERROR;
			}
			maxstacksz = tmpstacksz;
			break;
		}
	}

	/* sanity check resultant stack size limits
	 */
	if ( minstacksz > maxstacksz ) {
		mlog( MLOG_NORMAL
		      |
		      MLOG_ERROR
		      |
		      MLOG_NOLOCK
		      |
		      MLOG_PROC,
		      "specified minimum stack size is larger than maximum: "
		      "min is 0x%llx,  max is 0x%llx\n",
		      minstacksz,
		      maxstacksz );
		return EXIT_ERROR;
	}

	/* intitialize the stream manager
	 */
	 stream_init( );

	/* set the memory limits to their appropriate values.
	 */
	ok = set_rlimits( );
	if ( ! ok ) {
		return EXIT_ERROR;
	}


	/* initialize the spinlock allocator
	 */
	ok = qlock_init( BOOL_TRUE );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	/* initialize message logging
	 */
	ok = mlog_init2( );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	/* ask the system for the true vm page size, which must be used
	 * in all mmap calls
	 */
	pgsz = ( size_t )getpagesize( );
	mlog( MLOG_DEBUG | MLOG_PROC,
	      "getpagesize( ) returns %u\n",
	      pgsz );
	assert( ( intgen_t )pgsz > 0 );
	pgmask = pgsz - 1;

	/* initialize the critical region lock
	 */
	lock_init( );

	/* Get the parent's pid. will be used in signal handling
	 * to differentiate parent from children.
	 */
	parentpid = getpid( );
	if ( parentpid <= 1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to determine current directory: %s\n",
		      strerror( errno ));
		return EXIT_ERROR;
	}
	mlog( MLOG_DEBUG | MLOG_PROC,
	      "parent pid is %d\n",
	      parentpid );

	/* get the current working directory: this is where we will dump
	 * core, if necessary. some tmp files may be placed here as well.
	 */
	homedir = getcwd( 0, MAXPATHLEN );
	if ( ! homedir ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to determine current directory: %s\n",
		      strerror( errno ));
		return EXIT_ERROR;
	}

	/* initialize the stack checking abstraction
	 */
	stkchk_init( STREAM_MAX * 2 + 1 );
	stkchk_register( );

	/* select and instantiate a drive manager
	 */
	ok = drive_init1( argc, argv, BOOL_TRUE );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	mlog( MLOG_VERBOSE,
	      "version %d.%d\n",
	      version,
	      subversion );

	/* build a global write header template
	 */
	gwhdrtemplatep = global_hdr_alloc( argc, argv );
	if ( ! gwhdrtemplatep ) {
		return EXIT_ERROR;
	}

	/* initialize operator dialog capability
	 */
	ok = dlog_init( argc, argv );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	/* tell mlog how many streams there are. the format of log messages
	 * depends on whether there are one or many.
	 */
	mlog_tell_streamcnt( 1 );

	/* complete drive initialization
	 */
	ok = drive_init2( argc,
			  argv,
			  gwhdrtemplatep );
	if ( ! ok ) {
		return EXIT_ERROR;
	}
	ok = drive_init3( );
	if ( ! ok ) {
		return EXIT_ERROR;
	}

	/* commence scanning
	 */
	exitcode = content_scan( );

	return exitcode;
}

static intgen_t
content_scan( void )
{
	drive_t *drivep = drivepp[ 0 ];
	drive_ops_t *dop = drivep->d_opsp;

	for ( ; ; ) {
		intgen_t rval;
		bool_t termallowedpr;
		action_t action;
		bool_t ok;
		intgen_t stat;
		intgen_t count;

		rval = ( * dop->do_begin_read )( drivep );

		if ( ! rval ) {
			( dop->do_end_read )( drivep );
		}

		termallowedpr = BOOL_FALSE;

		switch( rval ) {
		case 0:
			termallowedpr = BOOL_TRUE;
			display_mfile_hdr( drivep );
			break;
		case DRIVE_ERROR_BLANK:
			mlog( MLOG_NORMAL,
			      "BLANK\n" );
			break;
		case DRIVE_ERROR_FOREIGN:
			mlog( MLOG_NORMAL,
			      "FOREIGN\n" );
			break;
		case DRIVE_ERROR_EOD:
			mlog( MLOG_NORMAL,
			      "EOD\n" );
			break;
		case DRIVE_ERROR_EOM:
			mlog( MLOG_NORMAL,
			      "EOM\n" );
			break;
		case DRIVE_ERROR_MEDIA:
			mlog( MLOG_NORMAL,
			      "MEDIA\n" );
			break;
		case DRIVE_ERROR_FORMAT:
			mlog( MLOG_NORMAL,
			      "FORMAT\n" );
			break;
		case DRIVE_ERROR_VERSION:
			mlog( MLOG_NORMAL,
			      "VERSION\n" );
			break;
		case DRIVE_ERROR_CORRUPTION:
			mlog( MLOG_NORMAL,
			      "CORRUPTION\n" );
			break;
		case DRIVE_ERROR_DEVICE:
			mlog( MLOG_NORMAL,
			      "DEVICE\n" );
			break;
		case DRIVE_ERROR_INVAL:
			mlog( MLOG_NORMAL,
			      "INVAL\n" );
			break;
		case DRIVE_ERROR_CORE:
			mlog( MLOG_NORMAL,
			      "CORE\n" );
			break;
		case DRIVE_ERROR_STOP:
			mlog( MLOG_NORMAL,
			      "STOP\n" );
			break;
		default:
			mlog( MLOG_NORMAL,
			      "unknown return from do_begin_read\n" );
			break;
		}

		action = action_dialog( termallowedpr );
		switch( action ) {
		case ACTION_NEXT:
			continue;
		case ACTION_QUIT:
			return EXIT_NORMAL;
		case ACTION_TERM:
			ok = write_terminator( drivep );
			if ( ! ok ) {
				return EXIT_ERROR;
			} else {
				continue;
			}
		case ACTION_REWIND:
			rval = ( * dop->do_rewind )( drivep );
			if ( rval ) {
				return EXIT_ERROR;
			} else {
				continue;
			}
		case ACTION_PREV:
			stat = 0;
			count = ( * dop->do_bsf )( drivep, 1, &stat );
			if ( stat ) {
				mlog( MLOG_NORMAL,
				      "backspace failed: %d\n",
				      stat );
				return EXIT_ERROR;
			} else if ( count != 1 ) {
				mlog( MLOG_NORMAL,
				      "backspace error (count %d)\n",
				      count );
			} else {
				continue;
			}
		default:
			return EXIT_ERROR;
		}
	}
	/* NOTREACHED */
}

#define PREAMBLEMAX	3
#define QUERYMAX	2
#define CHOICEMAX	6
#define ACKMAX		2
#define POSTAMBLEMAX	5

static action_t
action_dialog( bool_t termallowedpr )
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
	ix_t responseix;
	ix_t nextix;
	ix_t previx;
	ix_t quitix;
	ix_t termix;
	ix_t rewindix;
	action_t action;

	preamblecnt = 0;
	fold_init( fold, "action dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	assert( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	/* query: ask if overwrite ok
	 */
	sprintf( question,
		 "please select action to be taken\n" );
	querycnt = 0;
	querystr[ querycnt++ ] = question;
	assert( querycnt <= QUERYMAX );
	choicecnt = 0;
	nextix = choicecnt;
	choicestr[ choicecnt++ ] = "next media file";
	previx = choicecnt;
	choicestr[ choicecnt++ ] = "prev media file";
	if ( termallowedpr ) {
		termix = choicecnt;
		choicestr[ choicecnt++ ] = "write terminator "
					   "(DANGER: not recommended)";
	} else {
		termix = IXMAX;
	}
	rewindix = choicecnt;
	choicestr[ choicecnt++ ] = "rewind";
	quitix = choicecnt;
	choicestr[ choicecnt++ ] = "quit";
	assert( choicecnt <= CHOICEMAX );

	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,		/* hilitestr */
				       IXMAX,		/* hiliteix */
				       0,		/* defaultstr */
				       nextix,		/* defaultix */
				       0,		/* timeout */
				       IXMAX,		/* timeout ix */
				       quitix,		/* sigint ix */
				       quitix,		/* sighup ix */
				       quitix );	/* sigquit ix */

	ackcnt = 0;
	if ( responseix == nextix ) {
		ackstr[ ackcnt++ ] = "moving to next media file\n";
		action = ACTION_NEXT;
	} else if ( responseix == previx ) {
		ackstr[ ackcnt++ ] = "moving to prev media file\n";
		action = ACTION_PREV;
	} else if ( responseix == termix ) {
		ackstr[ ackcnt++ ] = "overwriting with terminator\n";
		action = ACTION_TERM;
	} else if ( responseix == rewindix ) {
		ackstr[ ackcnt++ ] = "rewind\n";
		action = ACTION_REWIND;
	} else {
		ackstr[ ackcnt++ ] = "quiting\n";
		action = ACTION_QUIT;
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

	return action;
}

static bool_t
write_terminator( drive_t *drivep )
{
	drive_ops_t *dop = drivep->d_opsp;
	off64_t ncommitted;
	global_hdr_t *grhdrp;
	global_hdr_t *gwhdrp;
	drive_hdr_t *dwhdrp;
	media_hdr_t *mwhdrp;
	intgen_t status;
	intgen_t rval;

	/* copy last read hdr into write hdr;
	 */
	grhdrp = drivep->d_greadhdrp;
	gwhdrp = drivep->d_gwritehdrp;
	*gwhdrp = *grhdrp;

	/* modify the write header to indicate a terminator
	 */
	dwhdrp = drivep->d_writehdrp;
	mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
	MEDIA_TERMINATOR_SET( mwhdrp );
	
	status = 0;
	rval = ( * dop->do_bsf )( drivep, 0, &status );
	assert( rval == 0 );
	if ( status == DRIVE_ERROR_DEVICE ) {
		return BOOL_FALSE;
	}

	rval = ( * dop->do_begin_write )( drivep );
	switch( rval ) {
	case 0:
		( void )( * dop->do_end_write )( drivep, &ncommitted );
		return BOOL_TRUE;
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_DEVICE:
		return BOOL_TRUE;
	default:
		return BOOL_FALSE;
	}
}

static void
display_mfile_hdr( drive_t *drivep )
{
	global_hdr_t *grhdrp = drivep->d_greadhdrp;
	drive_hdr_t *drhdrp = drivep->d_readhdrp;
	media_hdr_t *mrhdrp = ( media_hdr_t * )drhdrp->dh_upper;
	content_hdr_t *crhdrp = ( content_hdr_t * )mrhdrp->mh_upper;
	content_inode_hdr_t *scrhdrp =
				( content_inode_hdr_t * )crhdrp->ch_specific;
	char *mftstr;
	char dateline[ 28 ];
	char level_string[ 2 ];
	char *dump_string_uuid;
	char *media_string_uuid;
	char *fs_string_uuid;
	u_int32_t uuid_stat;

	assert( scrhdrp->cih_level >= 0 );
	assert( scrhdrp->cih_level < 10 );
	level_string[ 0 ] = ( char )( '0' + ( u_char_t )scrhdrp->cih_level );
	level_string[ 1 ] = 0;

	uuid_to_string( &grhdrp->gh_dumpid, &dump_string_uuid, &uuid_stat );
	assert( uuid_stat == uuid_s_ok );
	uuid_to_string( &mrhdrp->mh_mediaid, &media_string_uuid, &uuid_stat );
	assert( uuid_stat == uuid_s_ok );
	uuid_to_string( &crhdrp->ch_fsid, &fs_string_uuid, &uuid_stat );
	assert( uuid_stat == uuid_s_ok );

	mlog( MLOG_BARE | MLOG_NORMAL | MLOG_MEDIA,
	      "examining media file %u\n",
	      mrhdrp->mh_mediafileix );
	mlog( MLOG_BARE | MLOG_NORMAL | MLOG_MEDIA,
	      "magic: %s\n",
	      grhdrp->gh_magic );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "session time: %s",
#ifdef BANYAN
	      ctime_r( &grhdrp->gh_timestamp, dateline ));
#else /* BANYAN */
	      ctime_r( &grhdrp->gh_timestamp, dateline, sizeof( dateline )));
#endif /* BANYAN */
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "dump host ip addr: 0x%llx\n",
	      grhdrp->gh_ipaddr );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "session id: %s\n",
	      dump_string_uuid );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "hostname: %s\n",
	      grhdrp->gh_hostname );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "session label: \"%s\"\n",
	      grhdrp->gh_dumplabel );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "stream count: %u\n",
	      drhdrp->dh_drivecnt );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "stream index: %u\n",
	      drhdrp->dh_driveix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "drive strategy id: %u\n",
	      drhdrp->dh_strategyid );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media label: \"%s\"\n",
	      mrhdrp->mh_medialabel );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "previous media label: \"%s\"\n",
	      mrhdrp->mh_prevmedialabel );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media id: %s\n",
	      media_string_uuid );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media ix "
	      "(0-based index of this media object within the dump stream): "
	      "%u\n",
	      mrhdrp->mh_mediaix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media file ix "
	      "(0-based index of this file within this media object): "
	      "%u\n",
	      mrhdrp->mh_mediafileix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "dump file ix "
	      "(0-based index of this file within this dump stream): "
	      "%u\n",
	      mrhdrp->mh_dumpfileix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "dump media file ix "
	      "(0-based index of file within dump stream and media object): "
	      "%u\n",
	      mrhdrp->mh_dumpmediafileix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "dump media ix "
	      "(0-based index of this dump within the media object): "
	      "%u\n",
	      mrhdrp->mh_dumpmediaix );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media strategy id: %d\n",
	      mrhdrp->mh_strategyid );
	if ( MEDIA_TERMINATOR_CHK( mrhdrp )) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      "TERMINATOR\n" );
	}
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "mount point: %s\n",
	      crhdrp->ch_mntpnt );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "fs device: %s\n",
	      crhdrp->ch_fsdevice );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "fs type: %s\n",
	      crhdrp->ch_fstype );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "file system id: %s\n",
	      fs_string_uuid );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "content strategy id: %d\n",
	      crhdrp->ch_strategyid );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "volume: %s\n",
	      crhdrp->ch_fsdevice );
	switch( scrhdrp->cih_mediafiletype ) {
	case CIH_MEDIAFILETYPE_DATA:
		mftstr = "DATA";
		break;
	case CIH_MEDIAFILETYPE_INVENTORY:
		mftstr = "INVENTORY";
		break;
	case CIH_MEDIAFILETYPE_INDEX:
		mftstr = "INDEX";
		break;
	default:
		mftstr = "unknown";
		break;
	}
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "media file type: %s\n",
	      mftstr );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "dump attributes:" );
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_SUBTREE ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " SUBTREE" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_INDEX ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " INDEX" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_INVENTORY ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " INVENTORY" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_INCREMENTAL ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " INCREMENTAL" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_RETRY ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " RETRY" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_RESUME ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " RESUME" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_INOMAP ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " INOMAP" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_DIRDUMP ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " DIRDUMP" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_FILEHDR_CHECKSUM ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " FILEHDR_CHECKSUM" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_EXTENTHDR_CHECKSUM ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " EXTENTHDR_CHECKSUM" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_DIRENTHDR_CHECKSUM ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " DIRENTHDR_CHECKSUM" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_DIRENTHDR_GEN ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " DIRENTHDR_GEN" );
	}
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_EXTATTR ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " EXTATTR" );
	}
#ifdef EXTATTR
	if ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_EXTATTRHDR_CHECKSUM ) {
		mlog( MLOG_BARE | MLOG_NORMAL,
		      " EXTATTRHDR_CHECKSUM" );
	}
#endif /* EXTATTR */
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "\n" );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "level: %s%s\n",
	      level_string,
	      ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_RESUME )
	      ?
	      " resumed"
	      :
	      "" );
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "incr base time: %s",
#ifdef BANYAN
	      ctime_r( &scrhdrp->cih_last_time, dateline ));
#else /* BANYAN */
	      ctime_r( &scrhdrp->cih_last_time, dateline, sizeof( dateline )));
#endif /* BANYAN */
	mlog( MLOG_BARE | MLOG_NORMAL,
	      "resume base time: %s",
#ifdef BANYAN
	      ctime_r( &scrhdrp->cih_resume_time, dateline ));
#else /* BANYAN */
	      ctime_r( &scrhdrp->cih_resume_time, dateline, sizeof( dateline )));
#endif /* BANYAN */

	free( ( void * )dump_string_uuid );
	free( ( void * )media_string_uuid );
	free( ( void * )fs_string_uuid );
}

void
usage( void )
{
}

#ifdef DUMP
static bool_t
set_rlimits( void )
#endif /* DUMP */
#ifdef RESTORE
static bool_t
set_rlimits( size64_t *vmszp )
#endif /* RESTORE */
{
	struct rlimit64 rlimit64;
#ifdef RESTORE
	size64_t vmsz;
#endif /* RESTORE */
	/* REFERENCED */
	intgen_t rval;

	assert( minstacksz <= maxstacksz );

	rval = getrlimit64( RLIMIT_VMEM, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_VMEM org cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
#ifdef RESTORE
	vmsz = ( size64_t )rlimit64.rlim_cur;
#endif /* RESTORE */
	
	assert( minstacksz <= maxstacksz );
	rval = getrlimit64( RLIMIT_STACK, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_STACK org cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
	if ( rlimit64.rlim_cur < minstacksz ) {
		if ( rlimit64.rlim_max < minstacksz ) {
			mlog( MLOG_DEBUG
			      |
			      MLOG_NOLOCK
			      |
			      MLOG_PROC,
			      "raising stack size hard limit "
			      "from 0x%llx to 0x%llx\n",
			      rlimit64.rlim_max,
			      minstacksz );
			rlimit64.rlim_cur = minstacksz;
			rlimit64.rlim_max = minstacksz;
			( void )setrlimit64( RLIMIT_STACK, &rlimit64 );
			rval = getrlimit64( RLIMIT_STACK, &rlimit64 );
			assert( ! rval );
			if ( rlimit64.rlim_cur < minstacksz ) {
				mlog( MLOG_NORMAL
				      |
				      MLOG_WARNING
				      |
				      MLOG_NOLOCK
				      |
				      MLOG_PROC,
				      "unable to raise stack size hard limit "
				      "from 0x%llx to 0x%llx\n",
				      rlimit64.rlim_max,
				      minstacksz );
			}
		} else {
			mlog( MLOG_DEBUG
			      |
			      MLOG_NOLOCK
			      |
			      MLOG_PROC,
			      "raising stack size soft limit "
			      "from 0x%llx to 0x%llx\n",
			      rlimit64.rlim_cur,
			      minstacksz );
			rlimit64.rlim_cur = minstacksz;
			( void )setrlimit64( RLIMIT_STACK, &rlimit64 );
			rval = getrlimit64( RLIMIT_STACK, &rlimit64 );
			assert( ! rval );
			if ( rlimit64.rlim_cur < minstacksz ) {
				mlog( MLOG_NORMAL
				      |
				      MLOG_WARNING
				      |
				      MLOG_NOLOCK
				      |
				      MLOG_PROC,
				      "unable to raise stack size soft limit "
				      "from 0x%llx to 0x%llx\n",
				      rlimit64.rlim_cur,
				      minstacksz );
			}
		}
	} else if ( rlimit64.rlim_cur > maxstacksz ) {
		mlog( MLOG_DEBUG
		      |
		      MLOG_NOLOCK
		      |
		      MLOG_PROC,
		      "lowering stack size soft limit "
		      "from 0x%llx to 0x%llx\n",
		      rlimit64.rlim_cur,
		      maxstacksz );
		rlimit64.rlim_cur = maxstacksz;
		( void )setrlimit64( RLIMIT_STACK, &rlimit64 );
		rval = getrlimit64( RLIMIT_STACK, &rlimit64 );
		assert( ! rval );
		if ( rlimit64.rlim_cur > maxstacksz ) {
			mlog( MLOG_NORMAL
			      |
			      MLOG_WARNING
			      |
			      MLOG_NOLOCK
			      |
			      MLOG_PROC,
			      "unable to lower stack size soft limit "
			      "from 0x%llx to 0x%llx\n",
			      rlimit64.rlim_cur,
			      maxstacksz );
		}
	}
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_STACK new cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );

	rval = getrlimit64( RLIMIT_DATA, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_DATA org cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
	
	rval = getrlimit64( RLIMIT_FSIZE, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_FSIZE org cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
	rlimit64.rlim_cur = rlimit64.rlim_max;
	( void )setrlimit64( RLIMIT_FSIZE, &rlimit64 );
	rlimit64.rlim_cur = RLIM64_INFINITY;
	( void )setrlimit64( RLIMIT_FSIZE, &rlimit64 );
	rval = getrlimit64( RLIMIT_FSIZE, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_FSIZE now cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
	
	rval = getrlimit64( RLIMIT_CPU, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_CPU cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );
	rlimit64.rlim_cur = rlimit64.rlim_max;
	( void )setrlimit64( RLIMIT_CPU, &rlimit64 );
	rval = getrlimit64( RLIMIT_CPU, &rlimit64 );
	assert( ! rval );
	mlog( MLOG_NITTY | MLOG_NOLOCK | MLOG_PROC,
	      "RLIMIT_CPU now cur 0x%llx max 0x%llx\n",
	      rlimit64.rlim_cur,
	      rlimit64.rlim_max );

#ifdef RESTORE
	*vmszp = vmsz;
#endif /* RESTORE */
	return BOOL_TRUE;
}
