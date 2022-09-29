#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/namreg.c,v 1.7 1995/10/03 03:06:23 ack Exp $"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "lock.h"
#include "mlog.h"
#include "namreg.h"
#include "openutil.h"

/* structure definitions used locally ****************************************/

#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#define NAMREG_AVGLEN	10

/* persistent context for a namreg - placed in first page
 * of the namreg file by namreg_init if not a sync
 */
struct namreg_pers {
	off64_t np_appendoff;
};

typedef struct namreg_pers namreg_pers_t;

#define NAMREG_PERS_SZ	pgsz

/* transient context for a namreg - allocated by namreg_init()
 */
struct namreg_tran {
	char *nt_pathname;
	int nt_fd;
	bool_t nt_at_endpr;
};

typedef struct namreg_tran namreg_tran_t;


#ifdef NAMREGCHK

/* macros for manipulating namreg handles when handle consistency
 * checking is enabled.
 */
#define CHKBITCNT		2
#define	CHKBITSHIFT		( NBBY * sizeof( nrh_t ) - CHKBITCNT )
#define	CHKBITLOMASK		( ( 1 << CHKBITCNT ) - 1 )
#define	CHKBITMASK		( CHKBITLOMASK << CHKBITSHIFT )
#define CHKHDLCNT		CHKBITSHIFT
#define CHKHDLMASK		( ( 1 << CHKHDLCNT ) - 1 )
#define CHKGETBIT( h )		( ( h >> CHKBITSHIFT ) & CHKBITLOMASK )
#define CHKGETHDL( h )		( h & CHKHDLMASK )
#define CHKMKHDL( c, h )	( ( ( c << CHKBITSHIFT ) & CHKBITMASK )	\
				  |					\
				  ( h & CHKHDLMASK ))
#define HDLMAX			( ( off64_t )CHKHDLMASK )

#else /* NAMREGCHK */

#define HDLMAX			( ( ( off64_t )1			\
				    <<					\
				    ( ( off64_t )NBBY			\
				      *					\
				      ( off64_t )sizeof( nrh_t )))	\
				  -					\
				  ( off64_t )2 ) /* 2 to avoid NRH_NULL */

#endif /* NAMREGCHK */


/* declarations of externally defined global symbols *************************/

extern size_t pgsz;

/* forward declarations of locally defined static functions ******************/


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

static char *namregfile = "namreg";
static namreg_tran_t *ntp = 0;
static namreg_pers_t *npp = 0;


/* definition of locally defined global functions ****************************/

bool_t
namreg_init( char *hkdir, bool_t resume, u_int64_t inocnt )
{
#ifdef SESSCPLT
	if ( ntp ) {
		return BOOL_TRUE;
	}
#endif /* SESSCPLT */

	/* sanity checks
	 */
	assert( ! ntp );
	assert( ! npp );

	assert( sizeof( namreg_pers_t ) <= NAMREG_PERS_SZ );

	/* allocate and initialize context
	 */
	ntp = ( namreg_tran_t * )calloc( 1, sizeof( namreg_tran_t ));
	assert( ntp );

	/* generate a string containing the pathname of the namreg file
	 */
	ntp->nt_pathname = open_pathalloc( hkdir, namregfile, 0 );

	/* open the namreg file
	 */
	if ( resume ) {
		/* open existing file
		 */
		ntp->nt_fd = open( ntp->nt_pathname, O_RDWR );
		if ( ntp->nt_fd < 0 ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "could not find name registry file %s: "
			      "%s\n",
			      ntp->nt_pathname,
			      strerror( errno ));
			return BOOL_FALSE;
		}
	} else {
		/* create the namreg file, first unlinking any older version
		 * laying around
		 */
		( void )unlink( ntp->nt_pathname );
		ntp->nt_fd = open( ntp->nt_pathname,
				   O_RDWR | O_CREAT | O_EXCL,
				   S_IRUSR | S_IWUSR );
		if ( ntp->nt_fd < 0 ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "could not create name registry file %s: "
			      "%s\n",
			      ntp->nt_pathname,
			      strerror( errno ));
			return BOOL_FALSE;
		}

		/* reserve space for the backing store. try to use F_RESVSP64.
		 * if doesn't work, try F_RESVSP64. the former is faster, since
		 * it does not zero the space.
		 */
		{
		bool_t successpr;
		intgen_t fcntlcmd;
		intgen_t loglevel;
		size_t trycnt;

#ifndef F_RESVSP64
#define F_RESVSP64 0
#endif /* F_RESVSP64 */

		for ( trycnt = 0,
		      successpr = BOOL_FALSE,
		      fcntlcmd = F_RESVSP64,
#ifdef BANYAN
		      loglevel = MLOG_VERBOSE
#else /* BANYAN */
		      loglevel = MLOG_DEBUG
#endif /* BANYAN */
		      ;
		      ! successpr && trycnt < 2
		      ;
		      trycnt++,
		      fcntlcmd = F_ALLOCSP64,
		      loglevel = max( MLOG_NORMAL, loglevel - 1 )) {
			off64_t initsz;
			flock64_t flock64;
			intgen_t rval;

			if ( ! fcntlcmd ) {
				continue;
			}

			initsz = ( off64_t )NAMREG_PERS_SZ
				 +
				 ( ( off64_t )inocnt * NAMREG_AVGLEN );
			flock64.l_whence = 0;
			flock64.l_start = 0;
			flock64.l_len = initsz;
			rval = fcntl( ntp->nt_fd, fcntlcmd, &flock64 );
			if ( rval ) {
				mlog( loglevel | MLOG_NOTE,
				      "attempt to reserve %lld bytes for %s "
				      "using %s "
				      "failed: %s (%d)\n",
				      initsz,
				      ntp->nt_pathname,
				      fcntlcmd == F_RESVSP64
				      ?
				      "F_RESVSP64"
				      :
				      "F_ALLOCSP64",
				      strerror( errno ),
				      errno );
			} else {
				successpr = BOOL_TRUE;
			}
		}
		}
	}

	/* mmap the persistent descriptor
	 */
	assert( ! ( NAMREG_PERS_SZ % pgsz ));
	npp = ( namreg_pers_t * ) mmap( 0,
				        NAMREG_PERS_SZ,
				        PROT_READ | PROT_WRITE,
				        MAP_SHARED | MAP_AUTOGROW,
				        ntp->nt_fd,
				        ( off_t )0 );
	if ( npp == ( namreg_pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      ntp->nt_pathname,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* initialize persistent state
	 */
	if ( ! resume ) {
		npp->np_appendoff = ( off64_t )NAMREG_PERS_SZ;
	}

	/* initialize transient state
	 */
	ntp->nt_at_endpr = BOOL_FALSE;

	return BOOL_TRUE;
}

nrh_t
namreg_add( char *name, size_t namelen )
{
	off64_t oldoff;
	intgen_t nwritten;
	unsigned char c;
	nrh_t nrh;
	
	/* sanity checks
	 */
	assert( ntp );
	assert( npp );

	/* make sure file pointer is positioned to append
	 */
	if ( ! ntp->nt_at_endpr ) {
		off64_t newoff;
		newoff = lseek64( ntp->nt_fd, npp->np_appendoff, SEEK_SET );
		if ( newoff == ( off64_t )-1 ) {
			mlog( MLOG_NORMAL,
			      "lseek of namreg failed: %s\n",
			      strerror( errno ));
			assert( 0 );
			return NRH_NULL;
		}
		assert( npp->np_appendoff == newoff );
		ntp->nt_at_endpr = BOOL_TRUE;
	}

	/* save the current offset
	 */
	oldoff = npp->np_appendoff;

	/* write a one byte unsigned string length
	 */
	assert( namelen < 256 );
	c = ( unsigned char )( namelen & 0xff );
	nwritten = write( ntp->nt_fd, ( void * )&c, 1 );
	if ( nwritten != 1 ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		assert( 0 );
		return NRH_NULL;
	}

	/* write the name string
	 */
	nwritten = write( ntp->nt_fd, ( void * )name, namelen );
	if ( ( size_t )nwritten != namelen ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		assert( 0 );
		return NRH_NULL;
	}

	npp->np_appendoff += ( off64_t )( 1 + namelen );
	assert( oldoff <= HDLMAX );

#ifdef NAMREGCHK

	/* encode the lsb of the len plus the first character into the handle.
	 */
	nrh = CHKMKHDL( ( nrh_t )namelen + ( nrh_t )*name, ( nrh_t )oldoff );

#else /* NAMREGCHK */

	nrh = ( nrh_t )oldoff;

#endif /* NAMREGCHK */

	return nrh;
}

/* ARGSUSED */
void
namreg_del( nrh_t nrh )
{
	/* currently not implemented - grows, never shrinks
	 */
}

intgen_t
namreg_get( nrh_t nrh,
	    char *bufp,
	    size_t bufsz )
{
	off64_t newoff;
	intgen_t nread;
	size_t len;
	unsigned char c;
#ifdef NAMREGCHK
	nrh_t chkbit;
#endif /* NAMREGCHK */

	/* sanity checks
	 */
	assert( ntp );
	assert( npp );

	/* make sure we aren't being given a NULL handle
	 */
	assert( nrh != NRH_NULL );

	/* convert the handle into the offset
	 */
#ifdef NAMREGCHK

	newoff = ( off64_t )( size64_t )CHKGETHDL( nrh );
	chkbit = CHKGETBIT( nrh );

#else /* NAMREGCHK */

	newoff = ( off64_t )( size64_t )nrh;

#endif /* NAMREGCHK */

	/* do sanity check on offset
	 */
	assert( newoff <= HDLMAX );
	assert( newoff < npp->np_appendoff );
	assert( newoff >= ( off64_t )NAMREG_PERS_SZ );

	lock( );

	/* seek to the name
	 */
	newoff = lseek64( ntp->nt_fd, newoff, SEEK_SET );
	if ( newoff == ( off64_t )-1 ) {
		unlock( );
		mlog( MLOG_NORMAL,
		      "lseek of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

	/* read the name length
	 */
	c = 0; /* unnecessary, but keeps lint happy */
	nread = read( ntp->nt_fd, ( void * )&c, 1 );
	if ( nread != 1 ) {
		unlock( );
		mlog( MLOG_NORMAL,
		      "read of namreg failed: %s (nread = %d)\n",
		      strerror( errno ),
		      nread );
		return -3;
	}
	
	/* deal with a short caller-supplied buffer
	 */
	len = ( size_t )c;
	if ( bufsz < len + 1 ) {
		unlock( );
		return -1;
	}

	/* read the name
	 */
	nread = read( ntp->nt_fd, ( void * )bufp, len );
	if ( ( size_t )nread != len ) {
		unlock( );
		mlog( MLOG_NORMAL,
		      "read of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

#ifdef NAMREGCHK

	/* validate the checkbit
	 */
	assert( chkbit
		==
		( ( ( nrh_t )len + ( nrh_t )bufp[ 0 ] ) & CHKBITLOMASK ));

#endif /* NAMREGCHK */

	/* null-terminate the string if room
	 */
	bufp[ len ] = 0;

	ntp->nt_at_endpr = BOOL_FALSE;
	
	unlock( );

	return ( intgen_t )len;
}


/* definition of locally defined static functions ****************************/
