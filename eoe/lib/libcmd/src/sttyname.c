/*
 * sttyname.c
 *
 *      Brief description of purpose.
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.4 $"


/*LINTLIBRARY*/
/*
 * sttyname(s): return "/dev/X" (where X is a relative pathname
 * under /dev), which is the name of the tty character special
 * file associated with the stat structure s, or NULL if the
 * pathname cannot be found.
 *
 * sttyname() tries to find the tty file by matching major/minor
 * device, file system ID, and inode numbers of the file
 * descriptor parameter to those of files in the /dev directory.
 *
 * It attempts to find a match on major/minor device numbers,
 * file system ID, and inode numbers, but failing to match on
 * all three, settles for just a match on device numbers and
 * file system ID.
 *
 * To achieve higher performance and more flexible functionality,
 * sttyname first looks for a map file in /tmp.  The map file reflects
 * the configuration of the /dev/directory, and includes information
 * from the configuration file, /etc/ttysrch.  /etc/ttysrch identifies
 * directories in /dev to search first, directories to ignore, and 
 * directories for which a partial match may be acceptable.
 * This further improves performance by allowing an entry which
 * matches major/minor and file system ID, but not inode number
 * without searching the entire /dev tree. 
 *
 * If /tmp/ttymap does not exist, sttyname recursively searches /dev.
 */

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <malloc.h>
#include <ttymap.h>


static int	srch_dir(const char *, const int, int, const int,
			 const struct stat *, const time_t);
static int	search_ttymap(const struct stat *, char *);
static int	chk_file(const char *, const struct stat *,
			 const struct stat *, int);
static char 	*getnm(const char *, const char *);
static int	getflags(const char *, const struct tty_map *, const char *);

static char *rbuf=NULL;		/* matched file name */

char *
sttyname(struct stat *fsb)	/* fsb is what we are searching for	*/
{
	char path[ MAX_DEV_PATH ];	/* work area to concat paths	*/
	
/*
 *	do we need to search anything at all? 
 */
	if ( !S_ISCHR(fsb->st_mode) )
		return(0);

/*
 *	Check ttymap
 */			
	if( search_ttymap( fsb, path ) ) 
		return( rbuf );
/*
 *	ttymap was broken, do it the long way, search /dev
 */
	strcpy( path, DEV );
	(void) srch_dir( path, DEVLEN, DEVFLG, 0, fsb, (time_t)0 );

	return(rbuf);
}


/*
 * srch_dir() searches directory path and all directories under it up
 * to depth directories deep for a file described by a stat structure
 * fsb.  It puts the answer into rbuf.  If a match is found on device
 * number only, the search continues.
 *
 * srch_dir() returns 1 if a match (on device and inode) is found,
 * or 0 otherwise.
 *
 */

static int
srch_dir(
	const char *path,	/* current path name		*/
	const int plen,		/* current path len		*/
	int pflags,		/* current path flags		*/
	const int depth,	/* current depth (/dev = 0) 	*/
	const struct stat *fsb,	/* stat buf for the sought file	*/
	const time_t date	/* date from ttymap		*/
	)
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

	last_comp = (char *)(path + plen);
	*last_comp++ = '/';

	pass = 1;	/* pass 1: chk ino's from dir entries	*/
	if( !(pflags & MATCH_INO) )
		pass=2;	/* assume we are checking clone directory */
again:
/* 	skip first two entries ('.' and '..')
 *
 */
	(void) readdir(dirp);
	(void) readdir(dirp);
/* 
 *	search the directory
 */
	while ( direntp = readdir(dirp) ) {	/* assignment intended	*/
		if( pass == 1 && direntp->d_ino != fsb->st_ino )
			continue;
/*
 *		if the file name (path + "/" + d_name + NULL) 
 *		would be too long, then skip it 
 */
		len = (int)strlen(direntp->d_name);

		if ( (plen + len) > (MAX_DEV_PATH-2) )
			continue;

		strcpy(last_comp, direntp->d_name);

		if( stat( path, &tsb ) )
			continue;

		if( ( found = chk_file( path, &tsb, fsb, pflags ) ) )
			break;
/*
 * 		if file is a directory and we are not too deep, recurse
 */
		if ( S_ISDIR( tsb.st_mode ) 
		  && (found=srch_dir(path, /* assignment intended */
		  plen+len+1, pflags, depth+1, fsb, date)) )
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
static char *
getnm(const char *buf, const char *end)
{
	while( buf < end  && *buf++ ) 
		;
	if( buf < end )
		return( (char *)buf );
	return( NULL );
}
static int
search_ttymap(const struct stat *fsb, char *path)
{
	int fd;

	struct dent *fsid;
	size_t nfsid;
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

	if( (fd = open( MAP, O_RDONLY )) < 0 ) 
		return( 0 );

	if( fstat( fd, &sb ) < 0 ) {
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
		if( !stat( names, &sb ) 
		  && chk_file( names, &sb, fsb, flgs ) ) {
			(void) munmap( addr, size );
			return( 1 );
		}
	} while ( names=getnm( names, nmend ) ); /* assignment intended */


/*
 *	Didn't find a match in ttymap, run through dirmap
 *	looking for new directories
 */
chk_dirmap:

	for ( dp = dirmap; (char *)dp < dirend; dp++ ) {
		names = (char *)addr + ttymap->name_tbl + dp->nm_offset;
		nmend = names + dp->len;
	
		if( nmend > ttyend ) 
			goto addr_err;

		strncpy( path, names, dp->len );
		path[dp->len] = '\0';
		if( srch_dir( path, dp->len, dp->flags,
		  dp->depth, fsb, ttymap->date ) )
			break;
	};
	(void) munmap( addr, size );
	return( 1 );
addr_err:
	(void) munmap( addr, size );
	return( 0 );

}
static int				
chk_file(
	const char *path,	/* file to check	*/
	const struct stat *tsb,	/* stat buffer for path	*/
	const struct stat *fsb,	/* stat buffer we want	*/
	int pflags		/* flags for matching	*/
	)
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
		if (!rbuf && !(rbuf=malloc(MAX_DEV_PATH)))
			return(-1);
		strcpy(rbuf, path);
	}
	return( !pflags );
}
static int
getflags(const char *name, const struct tty_map *ttymap, const char *ttyend)
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
		for( p=(char *)name+DEVLEN; *p; p++ )
			if ( *p == '/' ) 
				return( MATCH_ALL );
	}
	return( dlong->flags );
}
