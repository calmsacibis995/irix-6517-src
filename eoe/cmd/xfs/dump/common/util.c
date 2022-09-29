#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/util.c,v 1.11 1999/08/23 23:02:54 nathans Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dirent.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"

extern size_t pgsz;

intgen_t
write_buf( char *bufp,
	   size_t bufsz,
	   void *contextp,
	   gwbfp_t get_write_buf_funcp,
	   wfp_t write_funcp )
{
	char *mbufp;	/* buffer obtained from manager */
	size_t mbufsz;/* size of buffer obtained from manager */

	while ( bufsz ) {
		int rval;

		assert( bufsz > 0 );
		mbufp = ( *get_write_buf_funcp )( contextp, bufsz, &mbufsz );
		assert( mbufsz <= bufsz );
		if ( bufp ) {
			(void)memcpy( ( void * )mbufp, ( void * )bufp, mbufsz );
		} else {
			(void)memset( ( void * )mbufp, 0, mbufsz );
		}
		rval = ( * write_funcp )( contextp, mbufp, mbufsz );
		if ( rval ) {
			return rval;
		}
		if ( bufp ) {
			bufp += mbufsz;
		}
		bufsz -= mbufsz;
	}

	return 0;
}

intgen_t
read_buf( char *bufp,
	  size_t bufsz, 
	  void *contextp,
	  rfp_t read_funcp,
	  rrbfp_t return_read_buf_funcp,
	  intgen_t *statp )
{
	char *mbufp;		/* manager's buffer pointer */
	size_t mbufsz;	/* size of manager's buffer */
	intgen_t nread;

	nread = 0;
	*statp = 0;
	while ( bufsz ) {
		mbufp = ( * read_funcp )( contextp, bufsz, &mbufsz, statp );
		if ( *statp ) {
			break;
		}
		assert( mbufsz <= bufsz );
		if ( bufp ) {
			( void )memcpy( (void *)bufp, (void *)mbufp, mbufsz );
			bufp += mbufsz;
		}
		bufsz -= mbufsz;
		nread += ( intgen_t )mbufsz;
		( * return_read_buf_funcp )( contextp, mbufp, mbufsz );
	}

	return nread;
}

char
*strncpyterm( char *s1, char *s2, size_t n )
{
	char *rval;

	if ( n < 1 ) return 0;
	rval = strncpy( s1, s2, n );
	s1[ n - 1 ] = 0;
	return rval;
}

void
stat64_to_xfsbstat( xfs_bstat_t *dstp, stat64_t *srcp )
{
	memset( ( void * )dstp, 0, sizeof( *dstp ));

	dstp->bs_ino = srcp->st_ino;
	dstp->bs_mode = srcp->st_mode;
	dstp->bs_nlink = srcp->st_nlink;
	dstp->bs_uid = srcp->st_uid;
	dstp->bs_gid = srcp->st_gid;
	dstp->bs_rdev = srcp->st_rdev;
	dstp->bs_blksize = ( int32_t )srcp->st_blksize;
	dstp->bs_size = srcp->st_size;
	dstp->bs_atime.tv_sec = srcp->st_atim.tv_sec;
#if defined( BANYAN ) || defined( SIXONE )
	dstp->bs_atime.tv_nsec = ( int32_t )srcp->st_atim.tv_nsec;
#else /* BANYAN || SIXONE */
	dstp->bs_atime.tv_nsec = ( long )srcp->st_atim.tv_nsec;
#endif /* BANYAN || SIXONE */
	dstp->bs_mtime.tv_sec = srcp->st_mtim.tv_sec;
#if defined( BANYAN ) || defined( SIXONE )
	dstp->bs_mtime.tv_nsec = ( int32_t )srcp->st_mtim.tv_nsec;
#else /* BANYAN || SIXONE */
	dstp->bs_mtime.tv_nsec = ( long )srcp->st_mtim.tv_nsec;
#endif /* BANYAN || SIXONE */
	dstp->bs_ctime.tv_sec = srcp->st_ctim.tv_sec;
#if defined( BANYAN ) || defined( SIXONE )
	dstp->bs_ctime.tv_nsec = ( int32_t )srcp->st_ctim.tv_nsec;
#else /* BANYAN || SIXONE */
	dstp->bs_ctime.tv_nsec = ( long )srcp->st_ctim.tv_nsec;
#endif /* BANYAN || SIXONE */
	dstp->bs_blocks = srcp->st_blocks;
}

intgen_t
bigstat_iter( jdm_fshandle_t *fshandlep,
	      intgen_t fsfd,
	      intgen_t selector,
	      ino64_t start_ino,
	      intgen_t ( * fp )( void *arg1,
				 jdm_fshandle_t *fshandlep,
				 intgen_t fsfd,
				 xfs_bstat_t *statp ),
	      void * arg1,
	      intgen_t *statp,
	      bool_t ( pfp )( int ),
	      xfs_bstat_t *buf,
	      size_t buflenin )
{
	size_t buflenout;
	ino64_t lastino;
	intgen_t saved_errno;

	/* stat set with return from callback func
	 */
	*statp = 0;

	/* NOT COOL: open will affect root dir's access time
	 */

	mlog( MLOG_NITTY,
	      "bulkstat iteration initiated: start_ino == %llu\n",
	      start_ino );

	/* quirk of the interface: I want to play in terms of the
	 * ino to begin with, and ino 0 is not used. so, ...
	 */
	if ( start_ino > 0 ) {
		lastino = start_ino - 1;
	} else {
		lastino = 0;
	}
	mlog( MLOG_NITTY + 1,
	      "calling bulkstat\n" );
	while (! syssgi( SGI_FS_BULKSTAT,
			 fsfd,
			 &lastino,
			 buflenin,
			 buf,
			 &buflenout )) {
		xfs_bstat_t *p;
		xfs_bstat_t *endp;

		if ( buflenout == 0 ) {
			mlog( MLOG_NITTY + 1,
			      "bulkstat returns buflen %d\n",
			      buflenout );
			return 0;
		}
		mlog( MLOG_NITTY + 1,
		      "bulkstat returns buflen %d ino %llu\n",
		      buflenout,
		      buf->bs_ino );
		for ( p = buf, endp = buf + buflenout ; p < endp ; p++ ) {
			intgen_t rval;

#ifdef SGI_FS_BULKSTAT_SINGLE
			if ( (!p->bs_nlink || !p->bs_mode) && p->bs_ino != 0 ) {
				/* inode being modified, get synced data */
				mlog( MLOG_NITTY + 1,
				      "ino %llu needed second bulkstat\n",
				      p->bs_ino );
				syssgi( SGI_FS_BULKSTAT_SINGLE,
					fsfd, &p->bs_ino, p);
			}
#endif

			if ( ( p->bs_mode & S_IFMT ) == S_IFDIR ) {
				if ( ! ( selector & BIGSTAT_ITER_DIR )){
					continue;
				}
			} else {
				if ( ! ( selector & BIGSTAT_ITER_NONDIR )){
					continue;
				}
			}
			rval = ( * fp )( arg1, fshandlep, fsfd, p );
			if ( rval ) {
				*statp = rval;
				return 0;
			}
			if ( pfp ) ( pfp )( PREEMPT_PROGRESSONLY );
		}

		if ( pfp && ( pfp )( PREEMPT_FULL )) {
			return EINTR;
		}

		mlog( MLOG_NITTY + 1,
		      "calling bulkstat\n" );
	}

	saved_errno = errno;

	mlog( MLOG_NORMAL,
	      "syssgi( SGI_FS_BULKSTAT ) on fsroot failed: %s\n",
	      strerror( saved_errno ));

	return saved_errno;
}

/* ARGSUSED */
intgen_t
bigstat_one( jdm_fshandle_t *fshandlep,
	     intgen_t fsfd,
	     ino64_t ino,
	     xfs_bstat_t *statp )
{
#ifdef SGI_FS_BULKSTAT_SINGLE
	assert( ino > 0 );
	return syssgi( SGI_FS_BULKSTAT_SINGLE, fsfd, &ino, statp );
#else
	ino64_t lastino;
	intgen_t count = 0;
	intgen_t rval;

	assert( ino > 0 );
	lastino = ino - 1;
	rval = syssgi( SGI_FS_BULKSTAT, fsfd, &lastino, 1, statp, &count );
	if ( count == 0 || lastino != ino ) {
		return EINVAL;
	}
	return rval;
#endif
}

/* calls the callback for every entry in the directory specified
 * by the stat buffer. supplies the callback with a file system
 * handle and a stat buffer, and the name from the dirent.
 *
 * NOTE: does NOT invoke callback for "." or ".."!
 *
 * if the callback returns non-zero, returns 1 with cbrval set to the
 * callback's return value. if syscall fails, returns -1 with errno set.
 * otherwise returns 0.
 *
 * caller may supply a dirent buffer. if not, will malloc one
 */
intgen_t
diriter( jdm_fshandle_t *fshandlep,
	 intgen_t fsfd,
	 xfs_bstat_t *statp,
	 intgen_t ( *cbfp )( void *arg1,
			     jdm_fshandle_t *fshandlep,
			     intgen_t fsfd,
			     xfs_bstat_t *statp,
			     char *namep ),
	 void *arg1,
	 intgen_t *cbrvalp,
	 char *usrgdp,
	 size_t usrgdsz )
{
	size_t gdsz;
#ifdef BANYAN
	dirent64_t *gdp;
#else /* BANYAN */
	dirent_t *gdp;
#endif /* BANYAN */
	intgen_t fd;
	intgen_t gdcnt;
	intgen_t scrval;
	intgen_t cbrval;

	if ( usrgdp ) {
#ifdef BANYAN
		assert( usrgdsz >= sizeof( dirent64_t ) + MAXPATHLEN );
#else /* BANYAN */
		assert( usrgdsz >= sizeof( dirent_t ) + MAXPATHLEN );
#endif /* BANYAN */
		gdsz = usrgdsz;
#ifdef BANYAN
		gdp = ( dirent64_t * )usrgdp;
#else /* BANYAN */
		gdp = ( dirent_t * )usrgdp;
#endif /* BANYAN */
	} else {
		gdsz = pgsz;
#ifdef BANYAN
		gdp = ( dirent64_t * )malloc( gdsz );
#else /* BANYAN */
		gdp = ( dirent_t * )malloc( gdsz );
#endif /* BANYAN */
		assert( gdp );
	}

	/* open the directory
	 */
	fd = jdm_open( fshandlep, statp, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "WARNING: unable to open directory ino %llu: %s\n",
		      statp->bs_ino,
		      strerror( errno ));
		*cbrvalp = 0;
		if ( ! usrgdp ) {
			free( ( void * )gdp );
		}
		return -1;
	}
	assert( ( statp->bs_mode & S_IFMT ) == S_IFDIR );

	/* lots of buffering done here, to achieve OS-independence.
	 * if proves to be to much overhead, can streamline.
	 */
	scrval = 0;
	cbrval = 0;
	for ( gdcnt = 1 ; ; gdcnt++ ) {
#ifdef BANYAN
		dirent64_t *p;
#else /* BANYAN */
		dirent_t *p;
#endif /* BANYAN */
		intgen_t nread;
		register size_t reclen;

		assert( scrval == 0 );
		assert( cbrval == 0 );

#ifdef BANYAN
		nread = getdents64( fd, gdp, gdsz );
#else /* BANYAN */
		nread = getdents( fd, gdp, gdsz );
#endif /* BANYAN */
		
		/* negative count indicates something very bad happened;
		 * try to gracefully end this dir.
		 */
		if ( nread < 0 ) {
			mlog( MLOG_NORMAL,
			      "WARNING: unable to read dirents (%d) for "
			      "directory ino %llu: %s\n",
			      gdcnt,
			      statp->bs_ino,
			      strerror( errno ));
			nread = 0; /* pretend we are done */
		}

		/* no more directory entries: break;
		 */
		if ( nread == 0 ) {
			break;
		}

		/* translate and invoke cb each entry: skip "." and "..".
		 */
		for ( p = gdp,
		      reclen = ( size_t )p->d_reclen
		      ;
		      nread > 0
		      ;
		      nread -= ( intgen_t )reclen,
		      assert( nread >= 0 ),
#ifdef BANYAN
		      p = ( dirent64_t * )( ( char * )p + reclen ),
#else /* BANYAN */
		      p = ( dirent_t * )( ( char * )p + reclen ),
#endif /* BANYAN */
		      reclen = ( size_t )p->d_reclen ) {
			xfs_bstat_t statbuf;

			assert( scrval == 0 );
			assert( cbrval == 0 );

			/* skip "." and ".."
			 */
			if ( *( p->d_name + 0 ) == '.'
			     &&
			     ( *( p->d_name + 1 ) == 0
			       ||
			       ( *( p->d_name + 1 ) == '.'
				 &&
				 *( p->d_name + 2 ) == 0 ))) {
				continue;
			}

			/* use bigstat
			 */
			scrval = bigstat_one( fshandlep,
					      fsfd,
#ifdef BANYAN
					      p->d_ino,
#else /* BANYAN */
					      ( ino64_t )p->d_ino,
#endif /* BANYAN */
					      &statbuf );
			if ( scrval ) {
				mlog( MLOG_NORMAL,
				      "WARNING: could not stat "
				      "dirent %s ino %llu: %s\n",
				      p->d_name,
				      ( ino64_t )p->d_ino,
				      strerror( errno ));
				scrval = 0;
				continue;
			}

			/* if getdents64() not yet available, and ino
			 * occupied more than 32 bits, warn and skip.
			 */
#ifndef BANYAN
			if ( statbuf.bs_ino > ( ino64_t )INOMAX ) {
				mlog( MLOG_NORMAL,
				      "WARNING: unable to process dirent %s: "
				      "ino %llu too large\n",
				      p->d_name,
				      ( ino64_t )p->d_ino );
				continue;
			}
#endif /* ! BANYAN */

			/* invoke the callback
			 */
			cbrval = ( * cbfp )( arg1,
					     fshandlep,
					     fsfd,
					     &statbuf,
					     p->d_name );

			/* abort the iteration if the callback returns non-zero
			 */
			if ( cbrval ) {
				break;
			}
		}

		if ( scrval || cbrval ) {
			break;
		}
	}

	( void )close( fd );
	if ( ! usrgdp ) {
		free( ( void * )gdp );
	}

	if ( scrval ) {
		*cbrvalp = 0;
		return -1;
	} else if ( cbrval ) {
		*cbrvalp = cbrval;
		return 1;
	} else {
		*cbrvalp = 0;
		return 0;
	}
}

char *
ctimennl( const time_t *clockp )
{
	char *p = ctime( clockp );

	if ( p && strlen( p ) > 0 ) {
		p[ strlen( p ) - 1 ] = 0;
	}

	return p;
}

int 
cvtnum( int blocksize, char *s )
{
	int i;
	char *sp;

	i = (int)strtoll(s, &sp, 0);
	if (i == 0 && sp == s)
		return -1;
	if (*sp == '\0')
		return i;
	if (*sp == 'b' && blocksize && sp[1] == '\0')
		return i * blocksize;
	if (*sp == 'k' && sp[1] == '\0')
		return 1024 * i;
	if (*sp == 'm' && sp[1] == '\0')
		return 1024 * 1024 * i;
	return -1;
}

bool_t
isinxfs( char *path )
{
	intgen_t fd;
	xfs_fsop_geom_t geo;
	intgen_t rval;
	
	fd = open( path, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "WARNING: could not open %s "
		      "to determine fs type: assuming non-xfs\n",
		      path );
		return BOOL_FALSE;
	}
	rval = syssgi( SGI_XFS_FSOPERATIONS,
		       fd,
		       XFS_FS_GEOMETRY,
		       (void *)0, &geo );
	if ( rval < 0 ) {
		return BOOL_FALSE;
	} else {
		return BOOL_TRUE;
	}
}

void
fold_init( fold_t fold, char *infostr, char c )
{
	size_t infolen;
	size_t dashlen;
	size_t predashlen;
	size_t postdashlen;
	char *p;
	char *endp;
	ix_t cnt;

	assert( sizeof( fold_t ) == FOLD_LEN + 1 );

	infolen = strlen( infostr );
	if ( infolen > FOLD_LEN - 4 ) {
		infolen = FOLD_LEN - 4;
	}
	dashlen = FOLD_LEN - infolen - 3;
	predashlen = dashlen / 2;
	postdashlen = dashlen - predashlen;

	p = &fold[ 0 ];
	endp = &fold[ sizeof( fold_t ) - 1 ];

	assert( p < endp );
	*p++ = ' ';
	for ( cnt = 0 ; cnt < predashlen && p < endp ; cnt++, p++ ) {
		*p = c;
	}
	assert( p < endp );
	*p++ = ' ';
	assert( p < endp );
	assert( p + infolen < endp );
	strcpy( p, infostr );
	p += infolen;
	assert( p < endp );
	*p++ = ' ';
	assert( p < endp );
	for ( cnt = 0 ; cnt < postdashlen && p < endp ; cnt++, p++ ) {
		*p = c;
	}
	assert( p <= endp );
	*p = 0;
}
