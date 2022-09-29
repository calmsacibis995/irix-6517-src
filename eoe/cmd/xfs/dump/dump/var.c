#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/var.c,v 1.4 1995/06/21 22:52:05 cbullis Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "types.h"
#include "fs.h"
#include "openutil.h"
#include "mlog.h"

#define VAR_PATH	"/var"
#define VAR_MODE	0755
#define VAR_OWNER	0
#define VAR_GROUP	0

#define XFSDUMP_PATH	"/var/xfsdump"
#define XFSDUMP_MODE	0755
#define XFSDUMP_OWNER	0
#define XFSDUMP_GROUP	0

static char *var_path = VAR_PATH;
static char *xfsdump_path = XFSDUMP_PATH;

static void var_skip_recurse( char *, void ( * )( ino64_t ));

void
var_create( void )
{
	intgen_t rval;

	mlog( MLOG_DEBUG,
	      "creating directory %s\n",
	      XFSDUMP_PATH );

	/* first make /var
	 */
	rval = mkdir( var_path, VAR_MODE );
	if ( rval && errno != EEXIST ) {
		mlog( MLOG_NORMAL,
		      "unable to create %s: %s\n",
		      var_path,
		      strerror( errno ));
		return;
	}
	if ( rval == 0 ) {
		rval = chown( var_path, VAR_OWNER, VAR_GROUP );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "unable to chmown %s: %s\n",
			      var_path,
			      strerror( errno ));
		}
	}

	/* next make /var/xfsdump
	 */
	rval = mkdir( xfsdump_path, XFSDUMP_MODE );
	if ( rval && errno != EEXIST ) {
		mlog( MLOG_NORMAL,
		      "unable to create %s: %s\n",
		      xfsdump_path,
		      strerror( errno ));
		return;
	}
	if ( rval == 0 ) {
		rval = chown( xfsdump_path, XFSDUMP_OWNER, XFSDUMP_GROUP );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "unable to chmown %s: %s\n",
			      xfsdump_path,
			      strerror( errno ));
		}
	}
}


void
var_skip( uuid_t *dumped_fsidp, void ( *cb )( ino64_t ino ))
{
	uuid_t fsid;
	u_int32_t uuidstat;
	intgen_t rval;

	/* see if the fs uuid's match
	 */
	rval = fs_getid( var_path, &fsid );
	if ( rval ) {
		mlog( MLOG_NORMAL,
		      "unable to determine uuid of fs containing %s: "
		      "%s\n",
		      var_path,
		      strerror( errno ));
		return;
	}

	if ( ! uuid_equal( dumped_fsidp, &fsid, &uuidstat )) {
		assert( uuidstat == uuid_s_ok );
		return;
	}
	assert( uuidstat == uuid_s_ok );

	/* traverse the xfsdump directory, getting inode numbers of it
	 * and all of its children, and reporting those to the callback.
	 */
	var_skip_recurse( xfsdump_path, cb );
}

static void
var_skip_recurse( char *base, void ( *cb )( ino64_t ino ))
{
	stat64_t statbuf;
	DIR *dirp;
	dirent_t *direntp;
	intgen_t rval;

	rval = lstat64( base, &statbuf );
	if ( rval ) {
		mlog( MLOG_NORMAL,
		      "unable to get status of %s: %s\n",
		      base,
		      strerror( errno ));
		return;
	}

	mlog( MLOG_DEBUG,
	      "excluding %s from dump\n",
	      base );

	( * cb )( statbuf.st_ino );

	if ( ( statbuf.st_mode & S_IFMT ) != S_IFDIR ) {
		return;
	}

	dirp = opendir( base );
	if ( ! dirp ) {
		mlog( MLOG_NORMAL,
		      "unable to open directory %s\n",
		      base );
		return;
	}

	while ( ( direntp = readdir( dirp )) != NULL ) {
		char *path;

		/* skip "." and ".."
		 */
		if ( *( direntp->d_name + 0 ) == '.'
		     &&
		     ( *( direntp->d_name + 1 ) == 0
		       ||
		       ( *( direntp->d_name + 1 ) == '.'
			 &&
			 *( direntp->d_name + 2 ) == 0 ))) {
			continue;
		}

		path = open_pathalloc( base, direntp->d_name, 0 );
		var_skip_recurse( path, cb );
		free( ( void * )path );
	}

	closedir( dirp );
}
