/*	@(#)subr.c	1.3 90/01/03 NFS Rev 2 Testsuite
 * 	1.6 Lachman ONC Test Suite source
 *
 * Useful subroutines shared by all tests
 */

#include <sys/param.h>
#ifndef major
#include <sys/types.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include "tests.h"
#include "subr.h"

/*
 * perform miscellaneous initializations
 *
 * initialize the random number generator
 */
void
initialize( void )
{
	srand48( time( NULL ) );
}

int
write_buffer( int fd, char *buffer, int len )
{
	int resid = len;
	int byte_cnt;

	/*
	 * write the output buffer until it has all been
	 * written or a zero-byte write occurs, or an error
	 * occurs
	 */
	while ( resid && ((byte_cnt = write( fd, buffer, resid )) > 0) ) {
		resid -= byte_cnt;
		buffer += byte_cnt;
	}
	return( (byte_cnt >= 0) ? (len - resid) : -1 );
}

int
make_fill_pattern( char *buffer, char *name, int size )
{
	int buflen = 0;
	char filler[FILLSTR_LEN + FILL_LEN];
	int status = 1;
	int i;

	/*
	 * generate the file filler
	 */
	for ( i = 0; i < FILL_LEN; i++ ) {
		filler[i] = (char)GEN_NUM( (int)'A', (int)'z' );
	}
	filler[FILL_LEN] = NULL;
	if ( sprintf( buffer, FILE_FMT, name, size, INITSTRING, filler ) == NULL ) {
		error( "make_fill_pattern: unable to fill output buffer for file %s",
			name );
		status = 0;
	} else {
		/*
		 * calculate the buffer length and make sure that the file
		 * size is at least large enough to hold one buffer
		 */
		buflen = strlen( buffer );
		while ( size < buflen ) {
			size = buflen;
			if ( sprintf( buffer, FILE_FMT, name, size, filler ) == NULL ) {
				error(
				"make_fill_pattern: unable to fill output buffer for file %s",
				name );
				status = 0;
				break;
			}
			buflen = strlen( buffer );
		}
	}
	return( status );
}

/*
 * construct a file of the requested size and name
 * the contents will be based on the file name
 * the contents will be a repetition of a string containing a 6 digit
 * generation number followed by the file name
 * the first 8 bytes in the file will contain the file size
 */
int
build_file( char *name, int size )
{
	int fd;
	int file_built = 0;
	int byte_cnt;
	char buffer[BUFLEN];
	int buflen = 0;

	/*
	 * 1. attempt to open the file;
	 *    if there is an error, report it and return 0.
	 * 2. generate the file filler;
	 *    if there is an error, report it and return 0 to the caller;
	 *    the filler is composed as follows:
	 *        <name> <size> <INITSTRING><random filler>;
	 *    name is the name of the file, size is the file character
	 *    representation of the file size, INITSTRING is the initialization
	 *    string, and the random filler is a randomly generated filler,
	 *    FILL_LEN bytes in length, each byte being between 'A' and 'z';
	 *    name and size and INITSTRING are separated by white space;
	 *    INITSTRING and the random filler form a singlr character string
	 *    which is FILLSTR_LEN + FILL_LEN bytes in length;
	 *    ensure that the file size is large enough to hold at least one
	 *    filler record
	 * 3. write the buffer to the file enough times to create a file of
	 *    the specified size.
	 *
	 */
	if ( size <= 0 ) {
		error( "build_file: %s: bad file size: %d", name, size );
	} else if ( (fd = open( name, O_WRONLY | O_TRUNC | O_CREAT, FILEMODE )) <
		0 ) {
			error( "build_file: %s: open", name );
	} else {
		if ( !make_fill_pattern( buffer, name, size ) ) {
			error( "build_file: unable to make fill pattern for file %s",
				name );
		} else {
			buflen = strlen( buffer );
			size = MAX( buflen, size );
			while ( size ) {
				byte_cnt = write_buffer( fd, buffer, MIN(buflen,size) );
				if ( byte_cnt < 0 ) {
					error( "build_file: write error on %s", name );
					break;
				} else if (byte_cnt == 0 ) {
					error( "build_file: %s: zero-byte write, %d bytes remain",
						name, size );
					break;
				} else if ( byte_cnt < MIN(buflen,size) ) {
					error( "build_file: %s: incomplete write, %d bytes remain",
						name, size );
					break;
				}
				size -= MIN( buflen, size );
			}
		}
		if ( close( fd ) < 0 ) {
			error( "build_file: close error on %s", name );
		} else if ( !size ) {
			file_built = 1;
		}
	}
	return( file_built );
}

/*
 * verify the contents of a buffer against name, size, and INITSTRING
 */
static int
check_buffer( char *buf, char *name, int size )
{
	int buffer_good = 0;
	char tmp_buf[BUFLEN];
	int compare_len;
	char *filename;
	char *initstr;
	char *sizetok;

	compare_len = MIN( strlen(buf), NONRAND_LEN(name) );
	if ( !make_fill_pattern( tmp_buf, name, size ) ) {
		error( "check_buffer: unable to make fill pattern for file %s", name );
	} else if ( strncmp( buf, tmp_buf, compare_len ) != 0 ) {
		error( "check_buffer: %s: incorrect fill pattern", name );
		fprintf( stderr, "    file pattern: %s\n", buf );
		fprintf( stderr, "expected pattern: %s\n", tmp_buf );
		filename = strtok( buf, " " );
		sizetok = strtok( NULL, " " );
		initstr = strtok( NULL, " " );
		if ( filename && (strcmp( filename, name ) != 0) ) {
			fprintf( stderr, "check_buffer: name mismatch\n" );
		} else if ( sizetok && (atoi(sizetok) != size) ) {
			fprintf( stderr, "check_buffer: size mismatch\n" );
		} else if ( initstr && (strcmp( initstr, INITSTRING ) != 0) ) {
			fprintf( stderr, "check_buffer: initstring mismatch\n" );
		} else {
			fprintf( stderr, "unknown buffer mismatch\n" );
		}
	} else {
		buffer_good = 1;
	}
	return( buffer_good );
}

/*
 * verify the contents of the named file
 */
int
check_file( char *name )
{
	int good_file = 0;
	int fd;
	int byte_cnt;
	char buffer[BUFLEN];
	char *bp;
	int buflen = 0;
	int resid_size;
	int resid;
	struct stat statbuf;

	if ( (fd = open( name, O_RDONLY )) < 0 ) {
		error( "check_file: %s: open", name );
	} else if ( fstat( fd, &statbuf ) ) {
		error( "check_file: %s: fstat", name );
		if ( close( fd ) < 0 ) {
			error( "check_file: %s: close", name );
		}
	} else if ( (byte_cnt = read( fd, buffer, BUFLEN )) < 0 ) {
		error( "check_file: %s: read", name );
		if ( close( fd ) < 0 ) {
			error( "check_file: %s: close", name );
		}
	} else if ( byte_cnt == 0 ) {
		error( "check_file: %s: empty file", name );
		if ( close( fd ) < 0 ) {
			error( "check_file: %s: close", name );
		}
	} else if ( !check_buffer( buffer, name, statbuf.st_size ) ) {
		error( "check_file: %s: incorrect file contents", name );
		if ( close( fd ) < 0 ) {
			error( "check_file: %s: close", name );
		}
	} else {
		buflen = FILL_PAT_LEN( name );
		resid_size = statbuf.st_size - buflen;
		if ( lseek( fd, buflen, SEEK_SET ) != buflen ) {
			error( "check_file: %s: lseek", name );
			if ( close( fd ) < 0 ) {
				error( "check_file: %s: close", name );
			}
		} else {
			while( resid_size ) {
				byte_cnt = read( fd, buffer, MIN(buflen,resid_size) );
				if ( byte_cnt == -1 ) {
					error( "check_file: %s: read", name );
					if ( close( fd ) < 0 ) {
						error( "check_file: %s: close", name );
					}
					break;
				} else if ( byte_cnt == 0 ) {
					error( "check_file: %s: unexpected EOF", name );
					if ( close( fd ) < 0 ) {
						error( "check_file: %s: close", name );
					}
					break;
				} else if ( !check_buffer( buffer, name, statbuf.st_size ) ) {
					error( "check_file: %s: bad file contents", name );
					break;
				}
				resid_size -= byte_cnt;
			}
			if ( close( fd ) < 0 ) {
				error( "check_file: %s: close", name );
			} else if ( !resid_size ) {
				good_file = 1;
			}
		}
	}
	return( good_file );
}

/*
 * given a file name, determine the level it is on
 */
int
get_level( char *file )
{
	char *dotp;
	char *tok;
	int lev = -1;
	char c;

	dotp = strpbrk( file, "." );
	if ( dotp ) {
		c = *dotp;
		tok = strtok( file, "." );
		if ( tok && (strlen( tok ) != 0) ) {
			lev = atoi( tok );
			*dotp = c;
		} else {
			*dotp = c;
			error( "get_level: % not of proper format", file );
		}
	} else {
		error( "get_level: % not of proper format", file );
	}
	return( lev );
}

/*
 * build the directory information file for the specified level
 * this file contains the level number, the number of regular files the
 * directory is supposed to contain (excluding the dir. info. file), and
 * the number of subdirectories in the directory
 */
int
build_dirinfo(
	int lev,
	char *fname,
	char *dname,
	int fid,
	int did,
	int files,
	int dirs
)
{
	int file_built = 0;
	int fd;
	char buffer[BUFLEN];
	char *bp;
	int buflen;
	int byte_cnt;
	char name[MAXPATHLEN];

	/*
	 * construct the file name
	 * it is composed of the level and the string DIRINFO: <lev>.DIRINFO
	 */
	sprintf( name, "%d.DIRINFO", lev );
	/*
	 * open the file for writing, creating it if it does not exist,
	 * truncating it otherwise
	 * once the file is open, fill in the output buffer
	 */
	if ( (fd = open( name, O_WRONLY | O_CREAT | O_TRUNC, 0600 )) < 0 ) {
		error( "build_dirinfo: %s: open", name );
	} else if ( sprintf( buffer, DIRINFO_FMT, name, lev, fname, dname, fid,
		did, files, dirs ) == NULL ) {
			error( "build_dirinfo: unable to fill output buffer for file %s",
				name );
	} else {
		/*
		 * write the output buffer until it has all been written or
		 * a zero-byte write occurs, or an error occurs
		 */
		for (
			buflen = strlen( buffer ), bp = buffer;
			buflen && ((byte_cnt = write( fd, bp, buflen )) > 0);
			buflen -= byte_cnt, bp += byte_cnt
		) {
			;
		}
		if ( buflen ) {
			if ( byte_cnt < 0 ) {
				error( "build_dirinfo: %s: incomplete write, %d bytes remain: ",
					name, buflen );
			} else {
				error( "build_dirinfo: %s: zero-byte write, %d bytes remain",
					name, buflen );
			}
			if ( close( fd ) < 0 ) {
				error( "build_dirinfo: %s: close", name );
			}
		} else {
			file_built = 1;
			if ( close( fd ) < 0 ) {
				error( "build_dirinfo: %s: close", name );
			}
		}
	}
	return( file_built );
}

/*
 * get the directory information file contents: returns file and dir count
 * verify in the process that the file is the correct file
 */
int
get_dirinfo(
	int lev,
	char *fname,
	char *dname,
	int *fid,
	int *did,
	int *files,
	int *dirs
)
{
	int file_read = 0;
	int fd;
	char buffer[BUFLEN];
	int buflen;
	int byte_cnt;
	int items;
	char infoname[MAXPATHLEN];
	char name[MAXPATHLEN];
	int filelev;

	/*
	 * construct the file name
	 * it is composed of the level and the string DIRINFO: <lev>.DIRINFO
	 */
	sprintf( infoname, "%d.DIRINFO", lev );
	/*
	 * open the file for writing, creating it if it does not exist,
	 * truncating it otherwise
	 * once the file is open, fill in the output buffer
	 */
	if ( (fd = open( infoname, O_RDONLY, 0600 )) < 0 ) {
		error( "get_dirinfo: %s: open", infoname );
	} else if ( (byte_cnt = read( fd, buffer, BUFLEN )) < 0 ) {
		error( "get_dirinfo: %s: read", infoname );
		if ( close( fd ) < 0 ) {
			error( "get_dirinfo: %s: close", name );
		}
	} else if ( byte_cnt == 0 ) {
		error( "get_dirinfo: %s: empty file", name );
		if ( close( fd ) < 0 ) {
			error( "get_dirinfo: %s: close", name );
		}
	} else if ( (items = sscanf( buffer, DIRINFO_FMT, name, &filelev, fname,
		dname, fid, did, files, dirs )) < DIRINFO_ITEMS ) {
			error(
			"get_dirinfo: %s: unable to get contents, %d of %d items read",
			infoname, items, DIRINFO_ITEMS );
			if ( close( fd ) < 0 ) {
				error( "get_dirinfo: %s: close", name );
			}
	} else if ( (strcmp( name, infoname ) != 0) || (filelev != lev) ){
		error(
			"get_dirinfo: %s: invalid contents: read %s, expected %s, read %d, expected %d",
			infoname, name, infoname, filelev, lev );
		if ( close( fd ) < 0 ) {
			error( "get_dirinfo: %s: close", name );
		}
	} else if ( close( fd ) < 0 ) {
		error( "get_dirinfo: %s: close", name );
	} else {
		file_read = 1;
	}
	return( file_read );
}

/*
 * strip leading and trailing spaces
 */
static void
strip_spaces( char *str )
{
	char *cp;

	if ( str ) {
		cp = str + strlen(str) - 1;
		while ( (cp >= str) && (*cp == ' ') ) {
			*cp = NULL;
			cp--;
		}
		cp = str;
		while ( *cp == ' ' ) {
			cp++;
		}
		if ( (*cp != NULL) && (cp != str) ) {
			while ( *cp != NULL ) {
				*str = *cp;
				str++;
				cp++;
			}
			*str = NULL;
		}
	}
}

/*
 * Build a directory tree "lev" levels deep
 * with a number of files in each directory between "minfiles" and "maxfiles"
 * each file of size between "minsize" and "maxsize"
 * and a fan out between "mindirs" and "maxdirs".  Starts at the current
 * directory.
 * "fname" and "dname" are the base of the names used for
 * files and directories.
 */
int
dirtree(
	int lev,
	int minfiles,
	int maxfiles,
	int minsize,
	int maxsize,
	int mindirs,
	int maxdirs,
	char *fname,
	char *dname,
	int *totfiles,
	int *totdirs
)
{
	int fd;
	int f, d;
	int dirs, files;
	char name[MAXPATHLEN];

	if (lev-- <= 0) {
		return;
	}
	if ( minfiles > maxfiles ) {
		error( "dirtree: minfiles > maxfiles" );
		exit( 1 );
	}
	if ( mindirs > maxdirs ) {
		error( "dirtree: mindirs > maxdirs" );
		exit( 1 );
	}
	/*
	 * generate the number of directories and files for this level
	 */
	if ( mindirs == maxdirs ) {
		dirs = mindirs;
	} else if ( lev == 0 ) {
		dirs = 0;
	} else {
		dirs = GEN_NUM( mindirs, maxdirs );
	}
	if ( minfiles == maxfiles ) {
		files = minfiles;
	} else {
		files = GEN_NUM( minfiles, maxfiles );
	}
	if ( Verbose ) {
		printf( "dirtree: directory level %d, %d dirs, %d files\n", lev, dirs,
			files );
	}
	/*
	 * build the directory information file for this level
	 */
	if ( !build_dirinfo( lev, fname, dname, files-1, dirs-1, files, dirs ) ) {
		error("dirtree: build_dirinfo failed for level %d", lev);
		exit(1);
	}
	/*
	 * build all of the regular files
	 * file names are unique, consisting of the level number, a base name
	 * and a file number
	 */
	for ( f = 0; f < files; f++) {
		MAKE_FILE_NAME(name, lev, fname, f);
		strip_spaces( name );
		if ( !build_file( name, GEN_NUM( minsize, maxsize ) ) ) {
			error("dirtree: build_file %s failed", name);
			exit(1);
		}
		(*totfiles)++;
	}
	/*
	 * make the directories, cd to each, and make the recursive call to
	 * dirtree
	 */
	for ( d = 0; d < dirs; d++) {
		sprintf(name, "%d.%s%d", lev, dname, d);
		if (mkdir(name, DIRMODE) < 0) {
			error("dirtree: mkdir %s failed", name);
			exit(1);
		}
		(*totdirs)++;
		if (chdir(name) < 0) {
			error("dirtree: chdir %s failed", name);
			exit(1);
		}
		dirtree(lev, minfiles, maxfiles, minsize, maxsize, mindirs, maxdirs,
			fname, dname, totfiles, totdirs);
		if (chdir("..") < 0) {
			error("dirtree: chdir .. failed");
			exit(1);
		}
	}
}

/*
 * Remove a directory tree starting at the current directory.
 * "fname" and "dname" are the base of the names used for
 * files and directories to be removed - don't remove anything else!
 * "files" and "dirs" are used with fname and dname to generate
 * the file names to remove.
 *
 * This routine will fail if after removing known files,
 * the directory is not empty.
 *
 * This is used to test the unlink function and to clean up after tests.
 */
int
rmdirtree(
	int lev,
	int maxfiles,
	int maxdirs,
	int *totfiles,		/* total removed */
	int *totdirs,		/* total removed */
	int ignore
)
{
	int f, d;
	int files, dirs;
	char name[MAXPATHLEN];
	char fbase[MAXPATHLEN];
	char dbase[MAXPATHLEN];
	int fid;
	int did;

	if (lev-- <= 0) {
		return;
	}
	/*
	 * get the directory information to get the real file and dir counts
	 */
	if ( !get_dirinfo( lev, fbase, dbase, &fid, &did, &files, &dirs ) ) {
		error("rmdirtree: get_dirinfo failed for level %d", lev);
		exit(1);
	}
	if ( (files > maxfiles) || (dirs > maxdirs) ) {
		error(
		"rmdirtree: WARNING: level %d file or dir count greater than maximum",
		lev);
	}
	for ( f = 0; f < files; f++) {
		sprintf(name, "%d.%s%d", lev, fbase, f);
		if (unlink(name) < 0 && !ignore) {
			error("rmdirtree: unlink %s failed", name);
			exit(1);
		}
		(*totfiles)++;
	}
	for ( d = 0; d < dirs; d++) {
		sprintf(name, "%d.%s%d", lev, dbase, d);
		if (chdir(name) < 0) {
			if (ignore)
				continue;
			error("rmdirtree: chdir %s failed", name);
			exit(1);
		}
		rmdirtree(lev, maxfiles, maxdirs, totfiles, totdirs, ignore);
		if (chdir("..") < 0) {
			error("rmdirtree: chdir .. failed");
			exit(1);
		}
		if (rmdir(name) < 0) {
			error("rmdirtree: rmdir %s failed", name);
			exit(1);
		}
		(*totdirs)++;
	}
}

/*
 * validate a directory tree
 */
int
checktree(
	int lev,			/* current level */
	int maxfiles,		/* max expected files */
	int maxdirs,		/* max expected dirs */
	char *fname,		/* base file name */
	char *dname,		/* base dir name */
	int *totfiles,		/* total valid files */
	int *totdirs,		/* total dirs */
	int *toterror,		/* total number of errors */
	int ignore			/* do not exit on file errors */
)
{
	int f, d;
	int files, dirs;
	char name[MAXPATHLEN];
	char fbase[MAXPATHLEN];
	char dbase[MAXPATHLEN];
	char curdir[MAXPATHLEN];
	int tree_valid = 1;
	int fid;
	int did;

	if (lev-- <= 0) {
		return;
	}
	/*
	 * get the directory information to get the real file and dir counts
	 */
	if ( !get_dirinfo( lev, fbase, dbase, &fid, &did, &files, &dirs ) ) {
		error("checktree: get_dirinfo failed for level %d", lev);
		if ( getwd( curdir ) ) {
			fprintf( stderr, "Current directory: %s\n", curdir );
		}
		exit(1);
	}
	if ( Verbose ) {
		printf( "checktree: directory level %d, %d dirs, %d files\n", lev, dirs,
			files );
	}
	/*
	 * check against what is expected to be there
	 */
	if ( (maxfiles >= 0) && (maxdirs >= 0) ) {
		if ( (files > maxfiles) || (dirs > maxdirs) ) {
			error(
		"checktree: WARNING: level %d file or dir count greater than maximum",
			lev);
		}
	}
	/*
	 * check all of the regular files on this level
	 */
	for ( f = 0; f < files; f++) {
		sprintf(name, "%d.%s%d", lev, fname, f);
		if ( !check_file( name ) ) {
			(*toterror)++;
			tree_valid = 0;
			error("checktree: check of %s failed", name);
			if ( !ignore ) {
				exit(1);
			}
			continue;
		}
		(*totfiles)++;
	}
	/*
	 * check each directory
	 * this is done recursively
	 */
	for ( d = 0; d < dirs; d++) {
		sprintf(name, "%d.%s%d", lev, dname, d);
		if (chdir(name) < 0) {
			(*toterror)++;
			tree_valid = 0;
			error("checktree: chdir %s failed", name);
			if (ignore)
				continue;
			exit(1);
		}
		tree_valid &= checktree(lev, maxfiles, maxdirs, fname, dname,
			totfiles, totdirs, toterror, ignore);
		if (chdir("..") < 0) {
			error("checktree: chdir .. failed");
			exit(1);
		}
		(*totdirs)++;
	}
}

/* VARARGS */
int
error(str, ar1, ar2, ar3, ar4, ar5, ar6, ar7, ar8, ar9)
	char *str;
{
	char *ret, *getwd(), path[MAXPATHLEN];

	if ((ret = getwd(path)) == NULL)
		fprintf(stderr, "%s: getwd failed: %s\n", Progname, path);
	else
		fprintf(stderr, "%s: (%s) ", Progname, path);

	fprintf(stderr, str, ar1, ar2, ar3, ar4, ar5, ar6, ar7, ar8, ar9);
	if (errno) {
		perror(" ");
		errno = 0;
	} else {
		fprintf(stderr, "\n");
	}
	fflush(stderr);
	if (ret == NULL)
		exit(1);
}

/*
 * Set up and move to a test directory
 */
int
testdir( char *dir )
{
	struct stat statb;
	char str[MAXPATHLEN];
	extern char *getenv();

	/*
	 *  If dir is non-NULL, use that dir.  If NULL, first
	 *  check for env variable NFSTESTDIR.  If that is not
	 *  set, use the compiled-in TESTDIR.
	 */
	if (dir == NULL)
		if ((dir = getenv("NFSTESTDIR")) == NULL)
			dir = TESTDIR;

	if (stat(dir, &statb) == 0) {
		sprintf(str, "rm -r %s", dir);
		if (system(str) != 0) {
			error("testdir: can't remove old test directory %s", dir);
			exit(1);
		}
	}

	if (mkdir(dir, DIRMODE) < 0) {
		error("testdir: can't create test directory %s", dir);
		exit(1);
	}
	if (chdir(dir) < 0) {
		error("testdir: can't chdir to test directory %s", dir);
		exit(1);
	}
}

/*
 * Move to a test directory
 */
int
mtestdir( char *dir )
{
	struct stat statb;
	char str[MAXPATHLEN];
	char *getenv();

	/*
	 *  If dir is non-NULL, use that dir.  If NULL, first
	 *  check for env variable NFSTESTDIR.  If that is not
	 *  set, use the compiled-in TESTDIR.
	 */
	if (dir == NULL)
		if ((dir = getenv("NFSTESTDIR")) == NULL)
			dir = TESTDIR;

	if (chdir(dir) < 0) {
		error("testdir: can't chdir to test directory %s", dir);
		return(-1);
	}
	return(0);
}
