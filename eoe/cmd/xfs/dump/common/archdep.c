#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/archdep.c,v 1.22 1995/07/25 21:01:24 cbullis Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "archdep.h"
#include "util.h"
#include "cleanup.h"
#include "mlog.h"
#include "namreg.h"

extern char *homedir;

/* template for the name of the tmp file containing the simulation tree
 */
#define NAMETEMPLATE	"xfssimmap"

/* node for internal simulation
 */
struct node {
	ino64_t n_ino;
	namreg_ix_t n_namreg_ix;
	intgen_t n_lnkcnt;
	struct node *n_parp;
};

typedef struct node node_t;

static void ino2path_init( fshandle_t, char * );
static char * ino2path( ino64_t, char *, size_t );
static intgen_t ino2path_count_cb( void *, fshandle_t, xfs_bstat_t * );
static intgen_t ino2path_init_cb( void *, fshandle_t, xfs_bstat_t * );
static void ino2path_init_recurse( node_t * );
static char * ino2path_recurse( node_t *, char *, size_t );
static node_t * ino2path_i2n( ino64_t );
static void ino2path_abort_cleanup( void *, void * );

fshandle_t
archdep_getfshandle( char *mntpnt )
{
	int fd;
	fd = open( mntpnt, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "unable to open %s: %s\n",
		      mntpnt,
		      strerror( errno ));
		return ( fshandle_t )FSHANDLE_NULL;
	}

	/* create the ino2path abstraction
	 */
	ino2path_init( ( fshandle_t )fd, mntpnt );

	return ( fshandle_t )fd;
}


archdep_getdents_context_t *
archdep_getdents_context_alloc( fshandle_t fshandle, xfs_bstat_t *statp )
{
	archdep_getdents_context_t *contextp;
	char pathbuf[ 257 ];
	char *pathp;

	/* allocate a context
	 */
	contextp = ( archdep_getdents_context_t * )
		   calloc( 1, sizeof( archdep_getdents_context_t ));
	assert( contextp );


	/* first get a pathname to the directory
	 */
	assert( ( statp->bs_mode & S_IFMT ) == S_IFDIR );
	pathp = ino2path( statp->bs_ino, pathbuf, sizeof( pathbuf ));

	/* if can't find path, must be a new directory not present
	 * when the simulator was initialized. print a warning, and
	 * pretend it is an empty directory.
	 */
	if ( ! pathp ) {
		mlog( MLOG_NORMAL,
		      "getdents sim: directory ino %llu not in sim map\n",
		      statp->bs_ino );
		contextp->gd_dirp = 0;
		return contextp;
	}

	/* prepare the kernel's readdir() context
	 */
	contextp->gd_dirp = opendir( pathp );
	assert( contextp->gd_dirp );
	contextp->gd_cached = BOOL_FALSE;

	return contextp;
}

void
archdep_getdents_context_free( archdep_getdents_context_t *contextp )
{
	intgen_t rval;

	if ( contextp->gd_dirp ) {
		rval = closedir( contextp->gd_dirp );
		assert( rval == 0 );
	}

	free( contextp );
}

#ifdef OBSOLETE
intgen_t
archdep_getdents( archdep_getdents_context_t *contextp,
		  char *bufp,
		  size_t bufsz )
{
	char debuf[ 2 * sizeof( dirent_t ) + NAME_MAX + 1 ];
						/* temp. hold system dirents */
	dirent_t *dep = ( dirent_t * )debuf;
	size_t donesz; /* number of bytes placed in caller's buffer */

	donesz = 0;

	/* if dir not in simulator map,
	 * just return 0 (pretend no dirents ).
	 */
	if ( ! contextp->gd_dirp ) {
		return 0;
	}

	/* if one in the cache, use it first
	 */
	if ( contextp->gd_cached ) {
		size_t namesz;
		size_t recsz;
		register archdep_getdents_entry_t *p;
		register char *cp;
		register char *ep;

		/* overlay a getdents_t on the caller's buffer
		 */
		p = ( archdep_getdents_entry_t * )bufp;

		/* check if the remaining space in the caller's buffer
		 * is sufficient
		 */
		namesz = contextp->gd_cachedsz;
		recsz = sizeof( archdep_getdents_entry_t )
			+
			namesz
			+
			1
			-
			sizeof( p->ge_name );
		recsz = ( recsz + ARCHDEP_GETDENTS_ALIGN - 1 )
			&
			~( ARCHDEP_GETDENTS_ALIGN - 1 );
		if ( bufsz < recsz ) {
			return -1;
		}

		mlog( MLOG_NITTY,
		      "cached dirent ino = %llu len == %d name == %s\n",
		      contextp->gd_cachedino,
		      contextp->gd_cachedsz,
		      contextp->gd_cachedname );

		/* copy the entry into the caller's buffer
		 */
		p->ge_ino = contextp->gd_cachedino;
		p->ge_sz = recsz;
		( void )strcpy( p->ge_name, contextp->gd_cachedname );

		/* null the padding (not necessary, good for debugging)
		 */
		cp = p->ge_name
		     +
		     contextp->gd_cachedsz
		     +
		     1;
		ep = ( char * )p + recsz;
		for ( ; cp < ep ; cp++ ) {
			*cp = 0;
		}

		bufsz -= recsz;
		donesz += recsz;
		bufp += recsz;

		contextp->gd_cached = BOOL_FALSE;
	}

	/* loop until we've exhausted the directory, or used up the caller's
	 * buffer
	 */
	errno = 0; /* to detect when readdir_r returns zero due to error */
	while ( ( dep = readdir_r( contextp->gd_dirp, dep )) != 0 ) {
		size_t namesz;
		size_t recsz;
		register archdep_getdents_entry_t *p;
		register char *cp;
		register char *ep;

		assert( dep == ( dirent_t * )debuf );

		/* skip "." and ".."
		 */
		if ( *( dep->d_name + 0 ) == '.'
		     &&
		     ( *( dep->d_name + 1 ) == 0
		       ||
		       ( *( dep->d_name + 1 ) == '.'
			 &&
			 *( dep->d_name + 2 ) == 0 ))) {
			errno = 0;
			continue;
		}

		/* overlay a getdents_t on the caller's buffer
		 */
		p = ( archdep_getdents_entry_t * )bufp;

		/* check if the remaining space in the caller's buffer
		 * is sufficient. if not, cache the entry and return.
		 */
		namesz = ( size_t )strlen( dep->d_name );
		recsz = sizeof( archdep_getdents_entry_t )
			+
			namesz
			+
			1
			-
			sizeof( p->ge_name );
		recsz = ( recsz + ARCHDEP_GETDENTS_ALIGN - 1 )
			&
			~( ARCHDEP_GETDENTS_ALIGN - 1 );
		if ( bufsz < recsz ) {
			contextp->gd_cachedino = ( ino64_t )dep->d_ino;
			assert( namesz < sizeof( contextp->gd_cachedname ));
			contextp->gd_cachedsz = namesz;
			strcpy( contextp->gd_cachedname, dep->d_name );
			contextp->gd_cached = BOOL_TRUE;
			if ( donesz == 0 ) {
				return -1;
			} else {
				errno = 0;
				break;
			}
		}

		mlog( MLOG_NITTY,
		      "dirent ino = %d len == %d name == %s\n",
		      dep->d_ino,
		      dep->d_reclen,
		      dep->d_name );

		/* copy the entry into the caller's buffer
		 */
		p->ge_ino = ( ino64_t )dep->d_ino;
		p->ge_sz = recsz;
		( void )strcpy( p->ge_name, dep->d_name );

		/* null the padding (not necessary, good for debugging)
		 */
		cp = p->ge_name
		     +
		     strlen( dep->d_name )
		     +
		     1;
		ep = ( char * )p + recsz;
		for ( ; cp < ep ; cp++ ) {
			*cp = 0;
		}

		bufsz -= recsz;
		donesz += recsz;
		bufp += recsz;
		errno = 0;
	}
	if ( errno ) {
		mlog( MLOG_NORMAL,
		      "getdir_r sets errno to %s\n",
		      strerror( errno ));
	}
	assert( errno == 0 );

	assert (donesz < ( size_t )INTGENMAX );
	return ( intgen_t )donesz;
}
#endif /* OBSOLETE */

archdep_getbmap_context_t *
archdep_getbmap_context_alloc( fshandle_t fshandle, xfs_bstat_t *statp )
{
	archdep_getbmap_context_t *contextp;
	char pathbuf[ 257 ];
	char *pathp;

	contextp = ( archdep_getbmap_context_t * )
		   calloc( 1, sizeof( archdep_getbmap_context_t ));
	assert( contextp );

	assert( ( statp->bs_mode & S_IFMT ) == S_IFREG );

	/* first get a pathname to the file
	 */
	pathp = ino2path( statp->bs_ino, pathbuf, sizeof( pathbuf ));

	/* if there is no path, return NULL. we will simulate this
	 * file by assuming it has no extents.
	 */
	if ( ! pathp ) {
		mlog( MLOG_DEBUG,
		      "WARNING: bmap sim: no pathname(s) for ino %llu\n",
		      statp->bs_ino );
		contextp->gb_fd = -1;
		return contextp;
	}

	/* open the file for reading
	 */
	contextp->gb_fd = open( pathp, O_RDONLY );
	assert( contextp->gb_fd > 0 );

	return contextp;
}

void
archdep_getbmap_context_free( archdep_getbmap_context_t *ctxp )
{
	intgen_t rval;
	if ( ctxp->gb_fd != -1 ) {
		rval = close( ctxp->gb_fd );
		assert( rval == 0 );
	}
	free( ( void * )ctxp );
}

intgen_t
archdep_getbmap( archdep_getbmap_context_t *ctxp, getbmap_t *bmapp )
{
	intgen_t rval;

	if ( ctxp->gb_fd == -1 ) {
		( void )memset( ( void * )bmapp, 0, sizeof( *bmapp ));
		return 0;
	}
	rval = fcntl( ctxp->gb_fd, F_GETBMAP, bmapp );
	return rval;
}

intgen_t
archdep_read( archdep_getbmap_context_t *contextp,
	      off64_t off,
	      char *bufp,
	      size_t sz )
{
	off64_t new_off;
	intgen_t nread;

	new_off = lseek64( contextp->gb_fd, off, SEEK_SET );

	if ( new_off == ( off64_t )( -1 )) {
		return -1;
	}

	nread = read( contextp->gb_fd, bufp, sz );
	return nread;
}

intgen_t
archdep_readlink( fshandle_t fshandle,
		  xfs_bstat_t *statp,
		  char *bufp,
		  size_t bufsz )
{
	char pathbuf[ 257 ];
	char *pathp;
	intgen_t rval;

	/* first get a pathname to the file
	 */
	pathp = ino2path( statp->bs_ino, pathbuf, sizeof( pathbuf ));

	/* if we can't find a path to this ino, return -1; the caller will
	 * handle it.
	 */
	if ( ! pathp ) {
		mlog( MLOG_DEBUG,
		      "WARNING: readlink sim: no pathname(s) for ino %llu\n",
		      statp->bs_ino );
		return -1;
	}

	/* call readlink(2) to get the link
	 */
	assert( bufsz <= INTGENMAX ); /* readlink() proto wrong in unistd.h! */
	rval = readlink( pathp, ( void * )bufp, ( int )bufsz );

	return rval;
}

/* below is the ino - to - pathname map, needed to simulated ino-oriented
 * system calls not yet implemented
 */
static node_t *i2p_nodep;
static size_t i2p_len;
static namreg_t *i2p_namregp;
static node_t *i2p_rootp;
static char *i2p_mntpnt;
static intgen_t i2p_fd;

static void
ino2path_init( fshandle_t fshandle, char *mntpnt )
{
	size_t ix;
	intgen_t stat;
	stat64_t statbuf;
	char namebuf[ 32 ];
	intgen_t rval;

	mlog( MLOG_VERBOSE,
	      "initializing ino-based syscall simulator ...\n" );

	/* save the mount point: it will be prepended to all
	 * pathnames.
	 */
	i2p_mntpnt = mntpnt;

	/* use a namreg abstraction to hold the directory entry names
	 */
	i2p_namregp = namreg_init( BOOL_FALSE,
				   BOOL_FALSE,
				   homedir );

	/* use bigstat_iter() to count the number of inos
	 */
	mlog( MLOG_VERBOSE,
	      "phase 1: counting inos\n" );
	i2p_len = 0;
	rval = bigstat_iter( fshandle,
			      BIGSTAT_ITER_ALL,
			      0,
			      ino2path_count_cb,
			      0,
			      &stat );
	assert( ! rval );

	/* allocate space for that many inos
	i2p_nodep = ( node_t * )calloc( i2p_len, sizeof( node_t ));
	assert( i2p_nodep );
	 */

	/* create the tmp file, first unlinking any older version laying around
	 */
	sprintf( namebuf, NAMETEMPLATE );
	assert( strlen( namebuf ) < sizeof( namebuf ));
	rval = unlink( namebuf );
	assert( rval == 0 || errno == ENOENT );
	i2p_fd = open( namebuf, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
	if ( i2p_fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "creat of simulator file %s failed: %s\n",
		      namebuf,
		      strerror( errno ));
		assert( 0 );
	}

	( void )cleanup_register( ino2path_abort_cleanup,
				  ( void * )homedir,
				  0 );

	/* mmap(2) the sim tmp file: NOTE: problem here with BIG files.
	 */
	assert(( int64_t )i2p_len * ( int64_t )sizeof( node_t ) <= UINTGENMAX );
	i2p_nodep = ( node_t * )mmap( 0,
				      i2p_len * sizeof( node_t ),
				      PROT_READ | PROT_WRITE,
				      MAP_SHARED | MAP_AUTOGROW,
				      i2p_fd,
				      0 );
	if ( i2p_nodep == ( node_t * )-1 ) {
		mlog( MLOG_NORMAL,
		      "mmap of sim file %s failed: %s\n",
		      namebuf,
		      strerror( errno ));
		assert( 0 );
	}

	/* fill in the ino field of each node_t
	 */
	mlog( MLOG_VERBOSE,
	      "phase 2: initializing %d nodes\n",
	      i2p_len );
	ix = 0;
	rval = bigstat_iter( fshandle,
			      BIGSTAT_ITER_ALL,
			      0,
			      ino2path_init_cb,
			      ( void * )&ix,
			      &stat );
	assert( ! rval );

	/* if the second bigstat pass came up with fewer inos,
	 * the remaining nodes aren't initialized: don't use them
	 */
	if ( ix < i2p_len ) {
		i2p_len = ix;
	}


	/* stat the mount point, to get the root ino
	 */
	rval = stat64( mntpnt, &statbuf );
	assert( ! rval );
	i2p_rootp = ino2path_i2n( statbuf.st_ino );
	assert( i2p_rootp );
	assert( i2p_rootp->n_ino == statbuf.st_ino );
	i2p_rootp->n_lnkcnt = 1;

	/* cd to the mount point
	 */
	rval = chdir( mntpnt );
	assert( ! rval );

	/* recursively descend the fs tree, identifying the parent of
	 * each inode.
	 */
	mlog( MLOG_VERBOSE,
	      "phase 3: building tree (%d nodes)\n",
	      i2p_len );
	ino2path_init_recurse( i2p_rootp );

	/* return to the original directory
	 */
	rval = chdir( homedir );
	assert( ! rval );

	mlog( MLOG_VERBOSE,
	      "initialization of ino-based syscall simulator complete\n" );
}

/* builds a null terminated pathname to the ino in the caller-supplied
 * buffer. returns a pointer to somewhere in the buffer, not necessarily
 * the beginning.
 */
static char *
ino2path( ino64_t ino, char *bufp, size_t bufsz  )
{
	node_t *np;
	char *p;

	mlog( MLOG_NITTY,
	      "simulator: mapping in %llu to pathname\n",
	      ino );

	/* find the ino in the map. return 0 if not found
	 */
	np = ino2path_i2n( ino );
	if ( ! np ) {
		return 0;
	}

	/* null-terminate the caller-supplied buffer
	 */
	bufp[ bufsz - 1 ] = 0;

	/* ascend up the tree, building the pathname in reverse.
	 * the recursion function returns the head of the pathname,
	 * which may be anywhere within the caller-supplied buffer.
	 */
	p = ino2path_recurse( np, bufp, bufsz - 1 );

	mlog( MLOG_NITTY,
	      "simulator: ino %llu is %s\n",
	      ino,
	      p );

	return p;
}

static char *
ino2path_recurse( node_t *np, char *bufp, size_t bufsz )
{
	intgen_t rval;
	size_t namsz;

	/* if this node has no links, return NULL: there is no
	 * corresponding pathname.
	 */
	if ( np->n_lnkcnt == 0 ) {
		return 0;
	}

	/* if this is the root, we are done: prepend the mount point,
	 * unless the mntpnt is "/", and at least one path element is
	 * already present in the buffer.
	 */
	if ( np == i2p_rootp ) {
		assert( bufsz >= strlen( i2p_mntpnt ));
		if ( strcmp( i2p_mntpnt, "/" ) || bufp[ bufsz ] == 0 ) {
			( void )memcpy( ( void * )&bufp[ bufsz
							 -
							 strlen( i2p_mntpnt ) ],
					( void * )i2p_mntpnt,
					strlen( i2p_mntpnt ));
			bufsz -= strlen( i2p_mntpnt );
		}
		return bufp + bufsz;
	}

	/* load the node name into beginning of the buffer
	 */
	rval = namreg_get( i2p_namregp, np->n_namreg_ix, bufp, bufsz );
	assert( rval >= 0 );
	namsz = ( size_t )rval;
	assert( namsz <= bufsz );

	/* move the name to the tail of the buffer
	 */
	( void )memmove( ( void * )( bufp + bufsz - namsz ),
			 ( void * )bufp,
			 namsz );
	bufsz -= namsz;

	/* pre-pend a 'slash'
	 */
	assert( bufsz >= 1 );
	bufp[ bufsz - 1 ] = '/';
	bufsz--;

	/* ascend up the tree, building the pathname in reverse.
	 * the recursion function returns the head of the pathname,
	 * which may be anywhere within the caller-supplied buffer.
	 */
	assert( np->n_parp );
	return ino2path_recurse( np->n_parp, bufp, bufsz );
}

static intgen_t
ino2path_count_cb( void *arg1, fshandle_t fshandle, xfs_bstat_t *statp )
{
	i2p_len++;
	return 0;
}

static intgen_t
ino2path_init_cb( void *arg1, fshandle_t fshandle, xfs_bstat_t *statp )
{
	size_t ix;
	node_t *p;

	ix = *( size_t * )arg1;
	if ( ix >= i2p_len ) {
		/* ran out of room: more inos became active */
		return 1;
	}
	p = i2p_nodep + ix;
	p->n_ino = statp->bs_ino;
	p->n_namreg_ix = NAMREG_IX_NULL;
	ix++;
	( * ( size_t * )arg1 )++;
	return 0;
}

static void
ino2path_init_recurse( node_t *parp )
{
	DIR *dirp;
	dirent_t *direntp;
	intgen_t rval;

	/* open the current directory
	 */
	dirp = opendir( "." );
	assert( dirp );

	/* process each directory entry
	 */
	errno = 0;
	while ( ( direntp = readdir( dirp )) != 0 ) {
		stat64_t statbuf;
		node_t *np;
		ino64_t ino;
		bool_t isdir;
		intgen_t rval;

		/* skip "." and ".."
		 */
		if ( ! strcmp( direntp->d_name, "." )) {
			continue;
		}
		if ( ! strcmp( direntp->d_name, ".." )) {
			continue;
		}

		/* stat64() for the ino and type
		 */
		assert( errno == 0 );
		rval = lstat64( direntp->d_name, &statbuf );
		assert( ! rval );
		ino = statbuf.st_ino;
		isdir = ( statbuf.st_mode & S_IFMT ) == S_IFDIR
			?
			BOOL_TRUE
			:
			BOOL_FALSE;

		/* look the ino up in the map. if not there, must be
		 * a recent creation, so skip.
		 */
		np = ino2path_i2n( ino );
		if ( ! np ) {
			continue;
		}

		/* if already linked, forget it. only need one path to a
		 * given ino for simulation.
		 */
		np->n_lnkcnt++;
		if ( np->n_lnkcnt > 1 ) {
			assert( ! isdir );
			continue;
		}
		np->n_parp = parp;
		np->n_namreg_ix = namreg_add( i2p_namregp,
					      direntp->d_name,
					      strlen( direntp->d_name ));

		/* recurse for directories
		 */
		if ( isdir ) {
			rval = chdir( direntp->d_name );
			assert( ! rval );
			ino2path_init_recurse( np );
			rval = chdir( ".." );
			assert( ! rval );
		}

		/* clear errno to detect if readdir() sets it
		 */
		errno = 0;
	}
	assert( errno == 0 );

	rval = closedir( dirp );
	assert( rval == 0 );
}

/* binary search
 */
static node_t *
ino2path_i2n( ino64_t ino )
{
	register node_t *p;
	register node_t *lop;
	register node_t *hip;

	p = i2p_nodep + i2p_len / 2;
	lop = i2p_nodep - 1;
	hip = i2p_nodep + i2p_len;

	for ( ; ; ) {
		if ( p->n_ino == ino ) {
			return p;
		}
		if ( p->n_ino < ino ) {
			lop = p;
			p = hip - ( hip - p ) / 2;
			if ( p >= hip ) {
				return 0;
			}
		} else {
			hip = p;
			p = lop + ( p - lop ) / 2;
			if ( p <= lop ) {
				return 0;
			}
		}
	}

	/* not reached */
	assert( 0 );
	return 0;
}

static void
ino2path_abort_cleanup( void *arg1, void *arg2 )
{
	( void )chdir( ( char * )arg1 );
	( void )unlink( NAMETEMPLATE );
}
