/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/ttyname.c	1.28"


/*LINTLIBRARY*/
/*
 * ttyname(f): return "/dev/X" (where X is a relative pathname
 * under /dev), which is the name of the tty character special
 * file associated with the file descriptor f, or NULL if the
 * pathname cannot be found.
 *
 * ttyname() tries to find the tty file by matching major/minor
 * device, file system ID, and inode numbers of the file
 * descriptor parameter to those of files in the /dev directory.
 *
 * It attempts to find a match on major/minor device numbers,
 * file system ID, and inode numbers, but failing to match on
 * all three, settles for just a match on device numbers and
 * file system ID.
 *
 * NOT PORTED TO IRIX vvvvvvv
 * To achieve higher performance and more flexible functionality,
 * ttyname first looks for a map file in /etc.  The map file reflects
 * the configuration of the /dev/directory, and includes information
 * from the configuration file, /etc/ttysrch.  /etc/ttysrch identifies
 * directories in /dev to search first, directories to ignore, and 
 * directories for which a partial match may be acceptable.
 * This further improves performance by allowing an entry which
 * matches major/minor and file system ID, but not inode number
 * without searching the entire /dev tree. 
 *
 * If /etc/ttymap does not exist, ttyname recursively searches /dev.
 *
 * SGI Enhancement:
 *  Don't ever return /dev/syscon or /dev/systty.  These devices are
 *  always links to either /dev/console or to a /dev/tty entry.
 */

#ifdef __STDC__
	#pragma weak ttyname = _ttyname
#ifndef  _LIBC_ABI
	#pragma weak ttyname_r = _ttyname_r
#endif /* _LIBC_ABI */
#endif

#include "synonyms.h"
#include <sys/types.h>
#include "shlib.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <malloc.h>
#include <ttymap.h>


static int srch_dir(const char *, const int, int, const int,
		const struct stat *, const time_t, char *, size_t);
#ifndef sgi
static int search_ttymap(const struct stat *, char *, char *, size_t);
static char *getnm(const char *,const char *);
static int getflags(const char *, const struct tty_map *, const char *);
#endif
static int chk_file(const char *, const struct stat *, 
		const struct stat *, int, char *, size_t);

static char *rbuf=NULL;		/* matched file name */

int
ttyname_r(int f, char *buf, size_t buflen)
{
	struct stat fsb;		/* what we are searching for	*/
	char path[ MAX_DEV_PATH ];	/* work area to concat paths	*/
	int rv;

/*
 *	do we need to search anything at all? 
 *	(is file descriptor a char special tty file?)
 */
	if (fstat(f, &fsb) < 0)
		return EBADF;
	if ( !isatty(f) || !S_ISCHR(fsb.st_mode) )
		return(ENOTTY);

/*
 *	Check ttymap
 */			
#ifndef sgi
	rv = search_ttymap( &fsb, path, buf, buflen );
	if (rv < 0)
		return NULL;
	else if (rv == 0) {
#else
	{
#endif
		/*
		 *	ttymap was broken, do it the long way, search /dev
		 */
		strcpy( path, DEV );
		rv = srch_dir( path, DEVLEN, DEVFLG, 0, &fsb, (time_t)0,
				buf, buflen );
		if (rv == 0)
			return ENOTTY; /* no match */
		else if (rv == -1)
			return ERANGE;
	}
	/* found a match */
	return(0);
}

char *
ttyname(int f)
{
	if (!rbuf && !(rbuf=malloc(MAX_DEV_PATH)))
		return NULL;

	if (ttyname_r(f, rbuf, MAX_DEV_PATH))
		return NULL;
	return rbuf;
}

/*
 * srch_dir() searches directory path and all directories under it up
 * to depth directories deep for a file described by a stat structure
 * fsb.  It puts the answer into buf.  If a match is found on device
 * number only, the search continues.
 *
 * srch_dir() returns 1 if a match (on device and inode) is found,
 * 	0 if no match
 *     -1 on error
 *
 */

static int
srch_dir(path, plen, pflags, depth, fsb, date, buf, buflen )
const char *path;		/* current path name		*/
const int plen;			/* current path len		*/
int pflags;			/* current path flags		*/
const int depth;		/* current depth (/dev = 0) 	*/
const struct stat *fsb;		/* stat buf for the sought file	*/
const time_t date;		/* date from ttymap		*/
char *buf;
size_t buflen;
{
	DIR *dirp;
	struct dirent *direntp;
	struct stat tsb;
	char *last_comp;
	int found = 0;
	int len;
	int pass;

	if ( depth > MAX_SRCH_DEPTH )
		return( 0 );
/*
 *	if we have a date from dirmap, stat the directory
 *	if the date is the same, skip this directory
 */
	if( date && ( stat( path, &tsb ) || tsb.st_mtime <= date ) )
		return( 0 );
		
	if ( !(dirp = opendir( path )) )
		return(0);

	last_comp = ((char *)path) + plen;
	*last_comp++ = '/';

	pass = 1;	/* pass 1: chk ino's from dir entries	*/
	if( !(pflags & MATCH_INO) )
		pass=2;	/* assume we are checking clone directory */
again:
#ifndef sgi
/* ACK! not all file systems guarentee . & .. are first! */
/* 	skip first two entries ('.' and '..')
 *
 */
	(void) readdir(dirp);
	(void) readdir(dirp);
#endif
/* 
 *	search the directory
 */
	while ( direntp = readdir(dirp) ) {	/* assignment intended	*/
		if( pass == 1 && direntp->d_ino != fsb->st_ino )
			continue;
#ifdef sgi
		if (direntp->d_name[0] == '.') {
			if (strcmp(direntp->d_name, ".") == 0 ||
			    strcmp(direntp->d_name, "..") == 0)
				continue;
		}
#endif
/*
 *		if the file name (path + "/" + d_name + NULL) 
 *		would be too long, then skip it 
 */
		len = (int)strlen(direntp->d_name);

		if ( (plen + len) > (MAX_DEV_PATH-2) )
			continue;

		strcpy(last_comp, direntp->d_name);
#ifdef sgi
/*
 * It is never correct for ttyname to return syscon or systty under Irix.
 */
		if (!(strcmp(path,"/dev/syscon")&&strcmp(path,"/dev/systty")))
			continue;
/*
 * Don't return SVR4-style pty names.
 */
		if (!(strncmp(path, "/dev/pts", strlen("/dev/pts"))))
			continue;
#endif

		if( stat( path, &tsb ) )
			continue;

		if( ( found = chk_file( path, &tsb, fsb, pflags,
				buf, buflen) ) )
			/* either a match or an error */
			break;
/*
 * 		if file is a directory and we are not too deep, recurse
 */
		if ( S_ISDIR( tsb.st_mode ) 
		  && (found=srch_dir(path, /* assignment intended */
		  plen+len+1, pflags, depth+1, fsb, date, buf, buflen)) )
				break;
	}
	if( pass == 1 && !found ) {
		pass = 2;
		rewinddir(dirp);
		goto again;
	}
	closedir(dirp);
	return(found);
}
#ifndef sgi
static char *
getnm( buf, end )
const char *buf;
const char *end;
{
	while( buf < end  && *buf++ ) 
		;
	if( buf < end )
		return((char *)buf );
	return( NULL );
}
static int
search_ttymap( fsb, path, buf, buflen )
const struct stat *fsb;
char *path;
char *buf;
size_t buflen;
{
	int fd;

	struct dent *fsid;
	uint nfsid;
	struct rent *maj_min;	
	uint nmaj_min;
	caddr_t addr;
	struct tty_map *ttymap;
	caddr_t ttyend;
	size_t size;
	struct stat sb;
	char *names;
	char *nmend;
	struct dirmap *dirmap;
	caddr_t dirend;
	struct dirmap *dp;
	int i;
	int rv = 0;

	if( (fd = open( MAP, O_RDONLY )) < 0 ) 
		return( 0 );

	if( fstat( fd, &sb ) < 0 || sb.st_size < sizeof(struct tty_map)) {
		(void) close( fd );
		return( 0 );
	}
	
	size = sb.st_size;
	addr = mmap( NULL, size, PROT_READ, MAP_PRIVATE, fd, (off_t)0 );
	(void) close( fd );

	if( !addr )
		return( 0 );

	ttymap = (struct tty_map *) addr;
	ttyend = addr + size;

	dirmap = (struct dirmap *)(addr + ttymap->dirmap_tbl);
	dirend = (char *)(dirmap + ttymap->dirmap_sz);

	if( dirend > ttyend )
		goto addr_err;


	fsid = (struct dent *)(addr + ttymap->dent_tbl);
	nfsid = ttymap->dent_sz;
	if( (caddr_t)(fsid + nfsid + 1) > ttyend ) {
		(void) munmap( addr, size );
		return( 0 );
	}
	for( i = 0; i < nfsid && fsid->dev < fsb->st_dev; i++ )
		fsid++;

	if( i >= nfsid || fsid->dev != fsb->st_dev )
		goto chk_dirmap;

	maj_min = (struct rent *)(addr + ttymap->rent_tbl 
	  + fsid->rent_offset);
	nmaj_min = fsid->nrent;
	if( (caddr_t)(maj_min + nmaj_min + 1) > ttyend )
		goto addr_err;

	for( i = 0; i < nmaj_min && maj_min->rdev < fsb->st_rdev; i++ )
		maj_min++;

	if( i >= nmaj_min || maj_min->rdev != fsb->st_rdev )
		goto chk_dirmap;

	names = (char *)addr + ttymap->name_tbl + maj_min->nm_offset;
	nmend = names + maj_min->nm_size;
	
	if( nmend > ttyend ) 
		goto addr_err;

	do {
		struct stat sb;
		uint flgs;

		flgs = getflags( names, ttymap, ttyend );
		if( !stat( names, &sb ) ) {
			rv = chk_file( names, &sb, fsb, flgs, buf, buflen);
			if (rv != 0) {
				/* either an error or a match */
				(void) munmap( addr, size );
				return( rv );
			}
		}
	} while ( names=getnm( names, nmend ) ); /* assignment intended */


/*
 *	Didn't find a match in ttymap, run through dirmap
 *	looking for new directories
 */
chk_dirmap:
	rv = 0;

	for ( dp = dirmap; (char *)dp < dirend; dp++ ) {
		names = (char *)addr + ttymap->name_tbl + dp->nm_offset;
		nmend = names + dp->len;
	
		if( nmend > ttyend ) 
			goto addr_err;

		strncpy( path, names, dp->len + 1 );
		rv = srch_dir( path, dp->len, dp->flags,
			  dp->depth, fsb, ttymap->date, buf, buflen );
		if (rv != 0)
			/* either a match or an error */
			break;
	};
	(void) munmap( addr, size );
	return( rv );
addr_err:
	(void) munmap( addr, size );
	return( 0 );

}
#endif

/*
 * Returns - -1 on error - this is only ERANGE.
 *	      0 if path doesn't match
 *	      1 if found match
 */
static int				
chk_file( path, tsb, fsb, pflags, buf, buflen ) 
const char *path;		/* file to check	*/
const struct stat *tsb;	/* stat buffer for path	*/
const struct stat *fsb;	/* stat buffer we want	*/
int pflags;		/* flags for matching	*/
char *buf;
size_t buflen;
{
/*
 *	is it a character special file?
 */
	if ( !S_ISCHR(tsb->st_mode) ) 
		return( 0 );
/*
 *	check dev, rdev, and ino 
 */
	if (tsb->st_dev == fsb->st_dev)
		pflags &= ~MATCH_FS;
	if (tsb->st_rdev == fsb->st_rdev)
		pflags &= ~MATCH_MM;
	if (tsb->st_ino == fsb->st_ino)
		pflags &= ~MATCH_INO;
/*
 *	Did we get enough of them for a partial match?
 */
	if ( !(pflags & (MATCH_MM | MATCH_FS)) ) {
		if (strlen(path) > buflen) {
			return (-1);
		}
		strcpy(buf, path);
	}
	return( !pflags );
}
#ifndef sgi
static int
getflags( name, ttymap, ttyend )
const char *name;
const struct tty_map *ttymap;
const char *ttyend;
{
	struct dirmap *dirmap, *dp, *de, *dlong;
	char *p, *plong;

/*
 *	search for dirmap name that fits name
 *	if found, use its flags, otherwise, return MATCH_ALL
 */

	dlong = NULL;
	dirmap = (struct dirmap *)((char *)ttymap + ttymap->dirmap_tbl);
	de = dirmap + ttymap->dirmap_sz;
	if( (char *)de > ttyend )
		return( MATCH_ALL );

	for( dp = dirmap; dp < de; dp++ ) {
		p = (char *)ttymap + ttymap->name_tbl + dp->nm_offset;
		if( p + dp->len > ttyend )
			return( MATCH_ALL );
		if( !strncmp( name, p, dp->len ) )
			if( !dlong || dp->len > dlong->len ) {
				dlong = dp;
				plong = p;
			}
	}
	if( !dlong || !dlong->flags )
		return( MATCH_ALL );
/*
 *	Don't use /dev flags beyond a path depth of 1
 */
	if( !strcmp( plong, DEV ) ) {
		for( p=(char *)((int)name+DEVLEN); *p; p++ )
			if ( *p == '/' ) 
				return( MATCH_ALL );
	}
	return( dlong->flags );
}
#endif
