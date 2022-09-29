/*
 * realtime copy command
 *
 *	This utility copies a file to the real time partition on
 *	an XFS file system.
 *	
 *	The syntax is :
 *
 *		rtcp [-e extsize]  [ -p ] sourcefile destfile 
 *
 *	The "-e" option is used to set the extent size of the destination
 *	real time file. The "-p" option is used if the size of the source
 *	file is not an even multiple of the block size of the destination
 *	file system. When "-p" is specified rtcp will pad the destination
 *	file to a size which is an even multiple of the file system block
 *	size. This is necessary since the real time file is created using
 *	direct I/O and the minimum I/O is the file system block size.
 *
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <libgen.h>
#include <fmtmsg.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>

#define ISDIR(A)        ((A.st_mode & S_IFMT) == S_IFDIR)

int rtcp( char *, char *, int);
int xfsrtextsize( char *path);
int	pflag;

void
usage()
{
	printf("rtcp [-e extsize]  [ -p ] sourcefile destfile \n");
	exit(2);
}


/*
 * main entry
 */
main(argc, argv)
register char *argv[];
{
	register int c, i, r, errflg = 0;
	struct stat s1, s2;
	int		eflag;
	int		extsize = - 1;
	extern char *optarg;
	extern int optind;

	/*
	 * Check for options:
	 * 	cp [-e extsize ] file1 file2
	 */
	while ((c = getopt(argc, argv, "pe:")) != EOF) {
		switch (c) {
		case 'e':
			eflag = 1;
			extsize = atoi(optarg);
			break;
		case 'p':
			pflag = 1;
			break;
		default:
			errflg++;
		}
	}

	/*
	 * Check for sufficient arguments
	 * or a usage error.
	 */
	argc -= optind;
	argv  = &argv[optind];

	if (argc < 2) {
		printf("must specify files to copy \n");
		usage();
	}

	if (errflg) {
		usage();
	}

	/*
	 * If there is more than a source and target,
	 * the last argument (the target) must be a directory
	 * which really exists.
	 */
	if (argc > 2) {
		if (stat(argv[argc-1], &s2) < 0) {
			printf("stat of directory failed \n");
			exit(2);
		}

		if (!ISDIR(s2)) {
			printf("last file is not directory \n");
			usage();
		}
	}

	/*
	 * Perform a multiple argument rtcp by
	 * multiple invocations of rtcp().
	 */
	r = 0;
	for (i=0; i<argc-1; i++)
		r += rtcp(argv[i], argv[argc-1],extsize);

	/*
	 * Show errors by nonzero exit code.
	 */
	exit(r?2:0);
}

int
rtcp( char *source, char *target, int fextsize)
{
	int		fromfd, tofd, readct, writect, iosz, reopen;
	int		remove = 0, rtextsize;
	char 		*sp, *fbuf, *ptr;
	char		tbuf[ PATH_MAX ];
	struct	stat	s1, s2;
	struct fsxattr	fsxattr;
	struct dioattr	dioattr;


        /*
         * While source or target have trailing /, remove them
         * unless only "/".
         */
        sp = source + strlen(source);
        if (sp) {
                while (*--sp == '/' && sp > source)
                        *sp = '\0';
        }
        sp = target + strlen(target);
        if (sp) {
                while (*--sp == '/' && sp > target)
                        *sp = '\0';
        }

	if ( stat(source, &s1) ) {
		printf("Could not stat %s.\n", source);
		return( -1);
	}

	/*
	 * check for a real time partition
	 */
	sprintf(tbuf,"%s",target);
	if ( stat(target, &s2) ) {
		if (!ISDIR(s2)) {
			/* take out target file name
			 */
			ptr = strrchr(tbuf, '/');
			if ( ptr) {
				*ptr = '\0';
			} else {
				sprintf(tbuf,".");
			}
		} 
	}

	if ( (rtextsize = xfsrtextsize( tbuf ))  <= 0 ) {
		printf("%s does not have an XFS real "
			"time partition.\n",
			tbuf);
		return( -1 );
	}

	/* check if target is a directory.
	 */
	sprintf(tbuf,"%s",target);
	if ( !stat(target, &s2) ) {
		if (ISDIR(s2)) {
			sprintf(tbuf,"%s/%s",target, basename(source));
		} 
	}
	
	if ( stat(tbuf, &s2) ) {
		/*
		 * create the file if it does not exist
		 */
		if ( (tofd = open(tbuf, O_RDWR | O_CREAT| O_DIRECT, 0666)) < 0 ) {
			printf("Open of %s failed.\n",tbuf);
			return( -1 );
		}
		remove = 1;
		
		/*
		 * mark the file as a real time file
		 */
		fsxattr.fsx_xflags = XFS_XFLAG_REALTIME;
		if (fextsize != -1 )
			fsxattr.fsx_extsize = fextsize;
		else
			fsxattr.fsx_extsize = 0;

		if ( fcntl( tofd, F_FSSETXATTR, &fsxattr) ) { 
			printf("Set attributes on %s failed.\n",tbuf);
			close( tofd );
			unlink( tbuf );
			return( -1 );
		}
	} else {
		/*
		 * open existing file
		 */
		if ( (tofd = open(tbuf, O_RDWR | O_DIRECT)) < 0 ) {
			printf("Open of %s failed.\n", tbuf);
			return( -1 );
		}
		
		if ( fcntl( tofd, F_FSGETXATTR, &fsxattr) ) {
			printf("Get attributes of %s failed.\n",tbuf);
			close( tofd );
			return( -1 );
		}

		/*
		 * check if the existing file is already a real time file
		 */
		if ( !(fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ) {
			printf("%s is not a real time file.\n",tbuf);
			return( -1 );
		}
		
		/*
		 * check for matching extent size
		 */
		if ( (fextsize != -1) && (fsxattr.fsx_extsize != fextsize) ) {
			printf("%s file extent size is %d, instead of %d.\n",
				tbuf,
				fsxattr.fsx_extsize,
				fextsize);
			return( -1 );
		}
	}

	/*
	 * open the source file
	 */
	reopen = 0;
	if ( (fromfd = open(source, O_RDONLY | O_DIRECT)) < 0 ) {
		printf("Open of %s source failed.\n", source);
		close( tofd );
		if (remove)
			unlink( tbuf );
		return( -1 );
	}

	fsxattr.fsx_xflags = 0;
	fsxattr.fsx_extsize = 0;
	if ( fcntl( fromfd, F_FSGETXATTR, &fsxattr) ) {
		reopen = 1;
	} else {
		if (! (fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ){
			printf("%s is not a real time file. \n",source);
			reopen = 1;
		}
	}

	if (reopen) {
		close( fromfd );
		if ( (fromfd = open(source, O_RDONLY )) < 0 ) {
			printf("Open of %s source failed.\n", source);
			close( tofd );
			if (remove)
				unlink( tbuf );
			return( -1 );
		}
	}

	/*
	 * get direct I/O parameters
	 */
	if ( fcntl( tofd, F_DIOINFO, &dioattr) ) {
		printf("Could not get direct i/o information.\n");
		close( fromfd );
		close( tofd );
		if ( remove ) 
			unlink( tbuf );
		return( -1 );
	}

	if ( rtextsize % dioattr.d_miniosz ) {
		printf("File extent size %d is not a multiple of %d.\n",
			rtextsize,
			dioattr.d_miniosz);
		close( fromfd );
		close( tofd );
		if ( remove )
			unlink( tbuf );
		return( -1 );
	}

	/*
	 * Check that the source file size is a multiple of the
	 * file system block size.
	 */
	if ( s1.st_size % dioattr.d_miniosz ) {
		printf("The size of %s is not a multiple of %d.\n",
			source, dioattr.d_miniosz);
		if ( pflag ) {
			printf("%s will be padded to %lld bytes.\n",
				tbuf,
				(((s1.st_size / dioattr.d_miniosz) + 1)  *
					dioattr.d_miniosz) );
				
		} else {
			printf("Use the -p option to pad %s "
				"to a size which is a multiple of %d bytes.\n",
				tbuf, dioattr.d_miniosz);
			close( fromfd );
			close( tofd );
			if ( remove )
				unlink( tbuf );
			return( -1 );
		}
	}

	iosz =  dioattr.d_miniosz;
	fbuf = memalign( dioattr.d_mem, iosz);
	bzero (fbuf, iosz);

	/*
	 * read the entire source file
	 */
	while ( ( readct = read( fromfd, fbuf, iosz) ) != 0 ) {
		/*
		 * if there is a read error - break
		 */
		if (readct < 0 ) {
			break;
		}

		/*
		 * if there is a short read, pad to a block boundary
	 	 */
		if ( readct != iosz ) {
			if ( (readct % dioattr.d_miniosz)  != 0 )  {
				readct = ( (readct/dioattr.d_miniosz) + 1 ) *
					 dioattr.d_miniosz;
			}
		}

		/*
		 * write to target file	
		 */
		writect = write( tofd, fbuf, readct);

		if ( writect != readct ) {
			printf("Write error. \n");
			close(fromfd);
			close(tofd);
			free( fbuf );
			return( -1 );
		}

		bzero( fbuf, iosz);
	}

	close(fromfd);
	close(tofd);
	free( fbuf );
	return( 0 );
}

/*
 * Determine the real time extent size of the XFS file system 
 */
int
xfsrtextsize( char *path)
{
        int 		fd, rval, rtextsize;
        xfs_fsop_geom_t geo;

        fd = open( path, O_RDONLY );
        if ( fd < 0 ) {
               printf("Could not open %s.\n",path);
               return -1;
        }
        rval = syssgi( SGI_XFS_FSOPERATIONS,
                       fd,
                       XFS_FS_GEOMETRY,
                       (void *)0, &geo );
	close(fd);

	rtextsize  = geo.rtextsize * geo.blocksize;

        if ( rval < 0 ) {
                return -1;
        } else {
                return rtextsize;
        }
}
