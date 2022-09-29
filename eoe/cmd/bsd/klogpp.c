/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* #ident	"$Revision: 1.6 $" */

/*
** klogpp
**	This program read from standard input, writes to standard output trying
**	to convert standard device name formats to include the mounted
**	filesystem.  Input line size defined by MAXINPUTLINE, can currently
**	be upto 2k characters in length.
**	This program is usually invoked as a filter via syslogd.conf.
**	Assumptions:
**		The input line has no newline (e.g.) null terminated string.
**		The output is expected to come back the same way.
**		This filter is invoked for each line, it doesn't hang
**		   around.
*/
#include	<sys/types.h>
#include	<stdio.h>
#include	<mntent.h>
#include	<sys/stat.h>
#include	<varargs.h>
#include	<limits.h>
#include	"klogppre.h"

/*
** Good and bad error returns.
*/
#define	EXIT_OK		0	/* A OKAY!				*/
#define	EXIT_BAD	1	/* Generally bad error			*/
#define	EXIT_NOMTAB	2	/* Cannot open MOUNTED (/etc/mtab)	*/
#define	EXIT_NOMEM	3	/* Cannot allocate memory		*/
#define	EXIT_INPERR	4	/* Error on input			*/

/*
** Define for setmntent() for read only
*/
#define	SME_RO	"r"

#define	DISK_PATHNAME	"/dev/dsk/"	/* path to disk devices	*/

/*
** max input line, must be >= than size configured in syslogd
*/
#define	MAXINPUTLINE	2048
#define	MAXDEVICENAME	PATH_MAX	/* max device name	*/

#define	equal(a,b)	(!strcmp((a),(b)))

/*
** This table holds the addresses of the compiled regcmp expressions
** imported via klogppre.h
*/
caddr_t	validdevnames[] =
{
	harddisk,
	floppy,
	logvol,
	NULL
};

/*
** The structure holds the info from the parsed /etc/mtab file that
** we care about. The entries are dynamically allocated and placed on
** a linked list with mtinfo_head as the head of the list.
*/
typedef struct mtinfo
{
	caddr_t		mti_name;	/* device name			*/
	dev_t		mti_dev;	/* dev number			*/
	struct mtinfo	*mti_next;	/* pointer to next entry	*/
	char		mti_namearea[ NAME_MAX ];
} mtinfo_t;

caddr_t	Pgmname;	/* Program name for error messages	*/

/*
** function prototypes: In this file, all others that need to be.
**   (warn and error have no prototypes because they use variable number
**    of args).
*/
caddr_t	cvtomfs( char * );
caddr_t	matchmtinfo( mtinfo_t *, caddr_t );
void	fillmtinfo( mtinfo_t ** );
void	warn();
void	error();

extern caddr_t	strchr();
extern caddr_t	regex();

/*
** cvttomfs
**   Takes the input 'line' and translates, if present, the standard
**   device name format to the actual mounted filesystem.
**   The mounted filesystem, if found, is parenthetically added following
**   the standard device name.
*/
cvttomfs( line )
caddr_t	line;
{
	static caddr_t	newline = NULL;
	static mtinfo_t	*mthead = NULL;
	register caddr_t	*vdnptr;
	caddr_t		retptr;
	caddr_t		mtfs;
	char		ret0[ MAXINPUTLINE ];
	char		ret1[ MAXINPUTLINE ];
	char		devname[ PATH_MAX ];

	fillmtinfo( &mthead );	/* setup mtab info table	*/

	/*
	** Check for a regular expression match
	*/
	for ( vdnptr = validdevnames; *vdnptr != NULL; vdnptr++ )
	{
		retptr = regex( *vdnptr, line, ret0, ret1 );
		if ( retptr != NULL )
			break;
	}
	if ( retptr == NULL )
	{
		printf( "%s", line );	/* no match	*/
		return;
	}

	strcpy( devname, DISK_PATHNAME );
	strcat( devname, ret1 );

	mtfs = matchmtinfo( mthead, devname );
	if ( mtfs == NULL )
	{
		printf( "%s", line );	/* no match	*/
		return;
	}

	printf( "%s%s (%s):%s", ret0, ret1, mtfs, retptr );

}

/*
** matchmtinfo
**   given the head of the mount table info and a device name, we
**   see if there is a match - based on major/minor device number.
*/
caddr_t
matchmtinfo( mthead, devname )
mtinfo_t		*mthead;
register caddr_t	devname;
{
	register mtinfo_t	*mtinfo;
	register dev_t		devt;
	struct stat		statbuf;

	if ( stat( devname, &statbuf ) < 0 )
	{
		return( NULL );
	}
	devt = statbuf.st_rdev;

	for ( mtinfo = mthead; mtinfo != NULL;
	      mtinfo = mtinfo->mti_next )
	{
		if ( devt == mtinfo->mti_dev )
			  return ( mtinfo->mti_name );
	}
	return ( NULL );
}

/*
** fillmtinfo
**   fill the mount table info from the /etc/mtab (MOUNTED) file,
*/
void
fillmtinfo( mthead )
register mtinfo_t	**mthead;
{
	struct stat		statbuf;
	register FILE		*fp;
	register mtinfo_t	*mtinfo;
	register struct mntent	*mntent;

	/*
	** open up /etc/mtab and parse it:
	** get an entry
	**	skip entry if not EFS type
	**	if current mtinfo entry is null, allocate one
	**	  and link it onto the list.
	**	otherwise free current name space
	**	allocate name space for new name
	**	save name and major/minor device number
	*/
	fp = setmntent( MOUNTED, SME_RO );
	if ( fp == NULL )
	{
		error( EXIT_NOMTAB, "Cannot open `%s'", MOUNTED );
	}

	while ( ( mntent = getmntent( fp ) ) != NULL )
	{
		if ( ! equal( FSID_EFS, mntent->mnt_type ) )
			continue;

		if ( stat( mntent->mnt_fsname, &statbuf ) < 0 )
		{
			warn( "Cannot stat %s (ignored)",
				mntent->mnt_fsname );
			continue;
		}

		mtinfo = (mtinfo_t *)
				malloc( sizeof (mtinfo_t) );
		if ( mtinfo == NULL )
			error( EXIT_NOMEM, "Out of memory, Cannot parse %s", MOUNTED );

		mtinfo->mti_next = *mthead;
		*mthead = mtinfo;

		mtinfo->mti_name = mtinfo->mti_namearea;
		strncpy( mtinfo->mti_name, mntent->mnt_dir, NAME_MAX );
		mtinfo->mti_dev = statbuf.st_rdev;
	}
	endmntent( fp );
}

void
warn( str, va_alist )
caddr_t	str;
va_dcl
{
	va_list	args;

	va_start( args );

	fprintf( stderr, "%s: ", Pgmname );
	vfprintf( stderr, str, args );
	fflush( stderr );
}

void
error( exitcode, str, va_alist )
int	exitcode;
caddr_t	str;
va_dcl
{
	va_list	args;

	va_start( args );

	fprintf( stderr, "%s: ", Pgmname );
	vfprintf( stderr, str, args );
	fflush( stderr );
	exit( exitcode );
}

main( argc, argv )
int	argc;
caddr_t	argv[];
{
	register int	len;
	char		buf[ MAXINPUTLINE ];

	Pgmname = argv[ 0 ];

	len = read( fileno( stdin ), buf, MAXINPUTLINE - 1 );
	if ( len < 0 )
		error( EXIT_INPERR, "Input error" );

	buf [ len ] = '\0';
	cvttomfs( buf );
	fflush( stdout );
	exit(0);
}
