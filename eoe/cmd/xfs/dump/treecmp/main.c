#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/treecmp/RCS/main.c,v 1.2 1996/04/23 01:21:15 doucette Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>

#include "types.h"
#include "path.h"

#define CMPBUFSZ	( 4 * 4096 )

struct dent {
	struct dent *d_next;
	char d_name[ 1 ];
};

typedef struct dent dent_t;

struct dentbuf {
	size_t db_cnt;
	dent_t **db_array;
	dent_t *db_list;
};

typedef struct dentbuf dentbuf_t;

struct dentbuf_iter {
	size_t dbi_remaining;
	dent_t **dbi_nextp;
};

typedef struct dentbuf_iter dentbuf_iter_t;

#define MLOG_NORMAL	0
#define MLOG_VERBOSE	1
#define MLOG_TRACE	2

#define PATH_SZ		( MAXPATHLEN * 32 * 8 )

struct path {
	size_t p_sz;
	size_t p_len;
	char p_head[ PATH_SZ ];
	char *p_next;
};

typedef struct path path_t;

typedef u_int32_t sum_t;

static char *progname;

static intgen_t mlog_level = MLOG_NORMAL;
static bool_t cmp_sums = BOOL_FALSE;
static bool_t cmp_bytes = BOOL_FALSE;

static char *aroot;
static char *broot;

static path_t *apathp;
static path_t *bpathp;
static path_t *rpathp;

static intgen_t dircmp( intgen_t );

static dentbuf_t *dentbuf_create( char * );
static intgen_t dentcmp( const void *, const void * );
static dentbuf_iter_t *dentbuf_iter_create( dentbuf_t * );
static char *dentbuf_iter_next( dentbuf_iter_t * );
static void dentbuf_iter_destroy( dentbuf_iter_t * );
static void dentbuf_destroy( dentbuf_t * );

static path_t *path_create( char *base );
static void path_append( path_t *pathp, char *name );
static void path_peel( path_t *pathp );
static void path_print( path_t *pathp, intgen_t level, char *fmt, ... );

static intgen_t statcmp( stat64_t *, stat64_t *, bool_t *, bool_t * );
static sum_t sum( intgen_t, bool_t * );
static void cmp( int fda, int fdb );
static int cmpbuf( char *bufa, char *bufb, int cnt );

static void mlog( intgen_t, char *, ... );

static char * ctimennl( const time_t * );

int
main( intgen_t argc, char *argv[ ] )
{
	intgen_t c;
	char *homedir;
	intgen_t rval;

	progname = argv[ 0 ];

	if ( argc <= 1 ) {
		mlog( MLOG_NORMAL,
		      "usage: %s [-v <0-3>] [-s (cmp sums)] [-c (cmp bytes)] "
		      "<dir1> <dir2>\n",
		      progname );
		return 0;
	}

	while ( ( c = getopt( argc, argv, "v:sc" )) != EOF ) {
		switch ( c ) {
		case 'v':
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				fprintf( stderr,
					 "%s: -%c argument missing\n",
					 progname,
					 optopt );
				return 1;
			}
			mlog_level = atoi( optarg );
			break;
		case 's':
			cmp_sums = BOOL_TRUE;
			break;
		case 'c':
			cmp_bytes = BOOL_TRUE;
			break;
		}
	}

	homedir = getcwd( 0, MAXPATHLEN );
	assert(  homedir );
	aroot = path_reltoabs( argv[ optind ], homedir );
	broot = path_reltoabs( argv[ optind + 1 ], homedir );

	mlog( MLOG_VERBOSE,
	      "comparing %s to %s\n",
	      aroot,
	      broot );

	apathp = path_create( aroot );
	bpathp = path_create( broot );
	rpathp = path_create( "." );

	rval = dircmp( 0 );

	return (int)rval;
}

static intgen_t
dircmp( intgen_t l )
{
	stat64_t abuf;
	stat64_t bbuf;
	bool_t isdir;
	bool_t isreg;
	intgen_t afd;
	intgen_t bfd;
	dentbuf_t *dap;
	dentbuf_t *dbp;
	char *ca;
	char *cb;
	dentbuf_iter_t *diap;
	dentbuf_iter_t *dibp;
	/* REFERENCED */
	intgen_t rval;

	if ( l > 0 ) {
		path_print( rpathp, MLOG_TRACE, "checking" );
	}

	rval = lstat64( apathp->p_head, &abuf );
	assert( ! rval );

	rval = lstat64( bpathp->p_head, &bbuf );
	assert( ! rval );

	if ( l > 0 ) {
		rval = statcmp( &abuf, &bbuf, &isdir, &isreg );
		assert( ! rval );
	} else {
		isreg = BOOL_FALSE;
		isdir = BOOL_TRUE;
	}

	if ( ! isdir && ( ! ( cmp_sums || cmp_bytes ) || ! isreg )) {
		return 0;
	}

	afd = open( apathp->p_head, O_RDONLY );
	assert( afd >= 0 );

	bfd = open( bpathp->p_head, O_RDONLY );
	assert( bfd >= 0 );

	if ( isreg ) {
		if ( cmp_bytes ) {
			cmp( afd, bfd );
		} else if ( cmp_bytes ) {
			sum_t suma;
			sum_t sumb;
			bool_t readerror;
			readerror = 0;
			suma = sum( afd, &readerror );
			if ( readerror ) {
				path_print( apathp,
					    MLOG_NORMAL,
					    "read error" );
			}
			readerror = 0;
			sumb = sum( bfd, &readerror );
			if ( readerror ) {
				path_print( bpathp,
					    MLOG_NORMAL,
					    "read error" );
			}
			if ( suma != sumb ) {
				path_print( rpathp,
					    MLOG_NORMAL,
					    "checksums differ" );
			}
		}
			
		close( afd );
		close( bfd );
		return 0;
	}

	dap = dentbuf_create( apathp->p_head );
	dbp = dentbuf_create( bpathp->p_head );

	diap = dentbuf_iter_create( dap );
	dibp = dentbuf_iter_create( dbp );

	ca = dentbuf_iter_next( diap );
	cb = dentbuf_iter_next( dibp );
	for ( ; ; ) {
		intgen_t diff;

		if ( ! ca && ! cb ) {
			break;
		}
		if ( ca && ! cb ) {
			path_append( bpathp, ca );
			path_print( bpathp, MLOG_NORMAL, "missing" );
			path_peel( bpathp );
			ca = dentbuf_iter_next( diap );
			continue;
		}
		if ( ! ca && cb ) {
			path_append( apathp, cb );
			path_print( apathp, MLOG_NORMAL, "extra" );
			path_peel( apathp );
			cb = dentbuf_iter_next( dibp );
			continue;
		}
		diff = strcmp( ca, cb );
		if ( diff < 0 ) {
			path_append( bpathp, ca );
			path_print( bpathp, MLOG_NORMAL, "missing" );
			path_peel( bpathp );
			ca = dentbuf_iter_next( diap );
			continue;
		}
		if ( diff > 0 ) {
			path_append( apathp, cb );
			path_print( apathp, MLOG_NORMAL, "extra" );
			path_peel( apathp );
			cb = dentbuf_iter_next( dibp );
			continue;
		}
		path_append( apathp, ca );
		path_append( bpathp, cb );
		path_append( rpathp, ca );
		( void )dircmp( l + 1 );
		path_peel( apathp );
		path_peel( bpathp );
		path_peel( rpathp );
		ca = dentbuf_iter_next( diap );
		cb = dentbuf_iter_next( dibp );
	}

	dentbuf_iter_destroy( dibp );
	dentbuf_iter_destroy( diap );
	dentbuf_destroy( dap );
	dentbuf_destroy( dbp );

	close( bfd );

	close( afd );

	return 0;
}

static dentbuf_t *
dentbuf_create( char *dirname )
{

	DIR *dirp;
	dirent_t *dep;
	dentbuf_t *dbp;
	dent_t *dp;
	dent_t *rootp;
	dent_t *lastp;
	size_t cnt;
	dent_t **array;
	dent_t **ap;

	dbp = ( dentbuf_t * )malloc( sizeof( dentbuf_t ));
	assert( dbp );
	dirp = opendir( dirname );
	assert( dirp );

	cnt = 0;
	rootp = 0;
	lastp = 0;
	while ( ( dep = readdir( dirp )) != NULL ) {
		if ( *( dep->d_name + 0 ) == '.'
		     &&
		     ( *( dep->d_name + 1 ) == 0
		       ||
		       ( *( dep->d_name + 1 ) == '.'
			 &&
			 *( dep->d_name + 2 ) == 0 ))) {
			continue;
		}
		dp = ( dent_t * )malloc( sizeof( dent_t )
					 +
					 strlen( dep->d_name ));
		assert( dp );
		dp->d_next = 0;
		strcpy( dp->d_name, dep->d_name );
		if ( ! rootp ) {
			rootp = dp;
			lastp = dp;
		} else {
			lastp->d_next = dp;
			lastp = dp;
		}
		cnt++;
	}
	array = ( dent_t ** )calloc( cnt, sizeof( dent_t * ));
	assert( array );
	dbp->db_cnt = cnt;
	for ( dp = rootp, ap = &array[ 0 ]
	      ;
	      cnt > 0
	      ;
	      dp = dp->d_next, ap++, cnt-- ) {
		*ap = dp;
	}

	qsort( ( void * )array, dbp->db_cnt, sizeof( dent_t * ), dentcmp );
	dbp->db_array = array;
	dbp->db_list = rootp;

	closedir( dirp );
	return dbp;
}

static intgen_t
dentcmp( const void *ap, const void *bp )
{
	dent_t *adp = *( dent_t ** )ap;
	dent_t *bdp = *( dent_t ** )bp;
	return strcmp( adp->d_name, bdp->d_name );
}

static void
dentbuf_destroy( dentbuf_t *dbp )
{
	dent_t *dp;

	dp = dbp->db_list;
	while ( dp ) {
		register dent_t *olddp = dp;
		dp = dp->d_next;
		free( ( void * )olddp );
	}
	free( ( void * )dbp->db_array );
	free( ( void * )dbp );
}

static
dentbuf_iter_t *
dentbuf_iter_create( dentbuf_t *dbp )
{
	dentbuf_iter_t *dip;

	dip = ( dentbuf_iter_t * )malloc( sizeof( dentbuf_iter_t ));
	assert( dip );
	dip->dbi_remaining = dbp->db_cnt;
	dip->dbi_nextp = &dbp->db_array[ 0 ];

	return dip;
}

static char *
dentbuf_iter_next( dentbuf_iter_t *dip )
{
	if ( dip->dbi_remaining > 0 ) {
		dent_t *dp;

		dip->dbi_remaining--;
		dp = *dip->dbi_nextp;
		dip->dbi_nextp++;
		return dp->d_name;
	} else {
		return 0;
	}
}

static void
dentbuf_iter_destroy( dentbuf_iter_t *dip )
{
	free( ( void * )dip );
}

static intgen_t
statcmp( stat64_t * abufp, stat64_t *bbufp, bool_t *isdirp, bool_t *isregp )
{
	bool_t aisdir;
	bool_t bisdir;
	bool_t aissym;
	bool_t bissym;
	bool_t bothsym;

	aisdir = ( ( abufp->st_mode & S_IFMT ) == S_IFDIR ) ? BOOL_TRUE
							  : BOOL_FALSE;

	bisdir = ( ( bbufp->st_mode & S_IFMT ) == S_IFDIR ) ? BOOL_TRUE
							  : BOOL_FALSE;

	if ( aisdir ^ bisdir ) {
		if ( bisdir ) {
			path_print( bpathp,
				    MLOG_NORMAL,
				    "changed to dir" );
		} else {
			path_print( bpathp,
				    MLOG_NORMAL,
				    "changed to non-dir" );
		}
		*isdirp = BOOL_FALSE;
		return 0;
	}

	aissym = ( ( abufp->st_mode & S_IFMT ) == S_IFLNK ) ? BOOL_TRUE
							    : BOOL_FALSE;

	bissym = ( ( bbufp->st_mode & S_IFMT ) == S_IFLNK ) ? BOOL_TRUE
							    : BOOL_FALSE;

	bothsym = aissym && bissym;

	if ( ! bothsym && abufp->st_mode != bbufp->st_mode ) {
		path_print( bpathp,
			    MLOG_NORMAL,
			    "mode changed from %04x to %04x",
			    abufp->st_mode,
			    bbufp->st_mode );
	}

	if ( ! bothsym && abufp->st_mtime != bbufp->st_mtime ) {
		char ctimebuf[ 27 ];
		strcpy( ctimebuf, ctimennl( &abufp->st_mtime ));
		assert( strlen( ctimebuf ) < sizeof( ctimebuf ));
		path_print( bpathp,
			    MLOG_NORMAL,
			    "mtime changed from %s to %s",
			    ctimebuf,
			    ctimennl( &bbufp->st_mtime ));
	}

	if ( abufp->st_uid != bbufp->st_uid ) {
		path_print( bpathp,
			    MLOG_NORMAL,
			    "uid changed from %d to %d",
			    abufp->st_uid,
			    bbufp->st_uid );
	}

	if ( abufp->st_gid != bbufp->st_gid ) {
		path_print( bpathp,
			    MLOG_NORMAL,
			    "gid changed from %d to %d",
			    abufp->st_gid,
			    bbufp->st_gid );
	}

	if ( ! aisdir && abufp->st_size != bbufp->st_size ) {
		path_print( bpathp,
			    MLOG_NORMAL,
			    "size changed from %lld to %lld",
			    abufp->st_size,
			    bbufp->st_size );
	}

	if ( abufp->st_mode == bbufp->st_mode 
	     &&
	     ( abufp->st_mode & S_IFMT ) == S_IFREG ) {
		*isregp = BOOL_TRUE;
	} else {
		*isregp = BOOL_FALSE;
	}

	*isdirp = aisdir;
	return 0;
}

static sum_t
sum( intgen_t fd, bool_t *readerrorp )
{
	char buf[ PGSZ ]; /* PGSZ % sizeof( sum_t ) MUST be zero ! */
	intgen_t nread;
	sum_t accum;

	accum = 0;

	while ( ( nread = read( fd, ( void * )buf, sizeof( buf ))) > 0 ) {
		sum_t *sump = ( sum_t * )buf;
		size_t ix;
		size_t sumread = ( ( size_t )nread + sizeof( sum_t ) - 1 )
				 /
				 sizeof( sum_t );
		size_t remcnt = ( size_t )nread % sizeof( sum_t );
		if ( remcnt ) {
			remcnt = sizeof( sum_t ) - remcnt;
		}
		for ( ix = ( size_t )nread
		      ;
		      ix < ( size_t )nread + remcnt
		      ;
		      ix++ ) {
			buf[ ix ] = 0;
		}
		for ( ; sumread ; sump++, sumread-- ) {
			accum += *sump;
		}
	}

	if ( nread < 0 ) {
		*readerrorp = BOOL_TRUE;
	} else {
		*readerrorp = BOOL_FALSE;
	}
	return accum;
}

static void
mlog( intgen_t level, char *fmt, ... )
{
	va_list args;

	if ( level > mlog_level ) {
		return;
	}
	va_start( args, fmt );
	vfprintf( stdout, fmt, args );
	va_end( args );
}

static path_t *
path_create( char *base )
{
	path_t *pathp = ( path_t * )calloc( 1, sizeof( path_t ));
	size_t baselen = strlen( base );
	assert( pathp );
	pathp->p_sz = PATH_SZ;
	pathp->p_len = 0;
	pathp->p_next = pathp->p_head;
	assert( pathp->p_len + baselen < pathp->p_sz );
	strcpy( pathp->p_next, base );
	pathp->p_next += baselen;
	pathp->p_len += baselen;

	return pathp;
}

static void
path_append( path_t *pathp, char *name )
{
	size_t namelen = strlen( name );
	assert( pathp->p_len + namelen + 1 < pathp->p_sz );
	*pathp->p_next = '/';
	pathp->p_next++;
	pathp->p_len++;
	strcpy( pathp->p_next, name );
	pathp->p_next += namelen;
	pathp->p_len += namelen;
}

static void
path_peel( path_t *pathp )
{
	char *tp;
	size_t namelen;

	for ( tp = pathp->p_next - 1, namelen = 0
	      ;
	      tp >= pathp->p_head
	      ;
	      tp--, namelen++ ) {
		if ( *tp == '/' ) {
			break;
		}
	}
	assert( tp >= pathp->p_head );
	*tp = 0;
	pathp->p_len -= namelen + 1;
	pathp->p_next -= namelen + 1;
}

static void
path_print( path_t *pathp, intgen_t level, char *fmt, ... )
{
	va_list args;

	if ( level > mlog_level ) {
		return;
	}

	fputs( pathp->p_head, stderr );
	fputs( ": ", stderr );
	va_start( args, fmt );
	vfprintf( stderr, fmt, args );
	va_end( args );
	fputc( '\n', stderr );
}

static char *
ctimennl( const time_t *clockp )
{
	char *p = ctime( clockp );

	if ( p && strlen( p ) > 0 ) {
		p[ strlen( p ) - 1 ] = 0;
	}

	return p;
}

static void
cmp( int fda, int fdb )
{
	char bufa[ CMPBUFSZ ];
	char bufb[ CMPBUFSZ ];
	int cnt;

	for ( cnt = 0 ; ; cnt += ( int )sizeof( bufa )) {
		int nreada;
		int nreadb;
		int bufcnt;

		nreada = read( fda, bufa, sizeof( bufa ));
		if ( nreada < 0 ) {
			path_print( apathp,
				    MLOG_NORMAL,
				    "read error" );
			return;
		}
		nreadb = read( fdb, bufb, sizeof( bufb ));
		if ( nreadb < 0 ) {
			path_print( bpathp,
				    MLOG_NORMAL,
				    "read error" );
			return;
		}
		if ( nreada < nreadb ) {
			/*
			path_print( bpathp,
				    MLOG_NORMAL,
				    "longer" );
			*/
			return;
		}
		if ( nreada > nreadb ) {
			/*
			path_print( bpathp,
				    MLOG_NORMAL,
				    "shorter" );
			*/
			return;
		}
		bufcnt = cmpbuf( bufa, bufb, nreada );
		if ( bufcnt != nreada ) {
			path_print( rpathp,
				    MLOG_NORMAL,
				    "differ at byte %d\n",
				    cnt + bufcnt );
			return;
		}
		if ( nreada < ( int )sizeof( bufa )) {
			return;
		}
	}
}

static int
cmpbuf( char *bufa, char *bufb, int cnt )
{
	int c;

	for ( c = 0 ; c < cnt ; c++ ) {
		if ( *bufa++ != *bufb++ ) {
			break;
		}
	}

	return c;
}
