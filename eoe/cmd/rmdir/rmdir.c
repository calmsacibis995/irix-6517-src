/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)rmdir:rmdir.c	1.10"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/rmdir/RCS/rmdir.c,v 1.6 1996/06/11 01:57:39 danc Exp $"
/*
 * Rmdir(1) removes directory.
 * If -p option is used, rmdir(1) tries to remove the directory
 * and it's parent directories.  It exits with code 0 if the WHOLE
 * given path is removed and 2 if part of path remains.  
 * Results are printed except when -s is used.
 *
 * Internationalized by frank@ceres.esd.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

/* externals */

extern int opterr, optind, errno, rmdir(), rmdirp(), setuid();
/* extern char *optarg, *malloc(), *getenv();	*/
extern void exit(), free();

/* locals */

char	cmd_label[] = "UX:rmdir";

/*
 * main entry
 */
int
main(argc,argv)
int argc;
char **argv;
{
	int c, pflag, sflag, errflg, rc;
        char *ptr, *remain, *msg, *path, *xpg;
	unsigned int pathlen;
	int	errcode = 0;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	pflag = sflag = 0;
	errflg = 0;

	/*
	 * set effective uid, euid, to be same as real
	 * uid, ruid.  Rmdir(2) checks search & write
	 * permissions for euid, but for compatibility
	 * the check must be done using ruid.
	 */
	if(setuid((int)getuid()) == -1) {
	    _sgi_nl_error(SGINL_SYSERR, cmd_label,
		gettxt(_SGI_DMMX_setuidfailed, "setuid(2) failed"));
	    exit(1);
	}

	while((c = getopt(argc, argv, "ps")) != EOF) {
	    switch (c) {
		case 'p':
		    pflag++;
		    break;
		case 's':
		    sflag++;
		    break;
		case '?':
		    errflg++;
		    break;
	    }
	}
        if(argc < 2 || errflg) {
	    (void)_sgi_nl_usage(SGINL_USAGE, cmd_label,
		gettxt(_SGI_DMMX_usage_rmdir, "rmdir [-ps] dirname ..."));
	    exit(2);
	}
	argc -= optind;
	argv = &argv[optind];

        while(argc--) {
	    ptr = *argv++;				/* next argv */
	    if(pflag) {
		pathlen = (unsigned)strlen(ptr);
		path = malloc(pathlen + 4);
		remain=malloc(pathlen + 4);
		if( !path || !remain) {
		    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_outofmem, "Out of memory"));
		    exit(2);
		}
		strcpy(path,ptr);

		/*
		 * rmdirp() removes directory and parents
		 * rc != 0 implies only part of path removed
		 */
		if( !(rc = rmdirp(path, remain))) {
		    if( (atoi(xpg = getenv("_XPG")) == NULL && !sflag) )  
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_pathremoved,
				"%s: Whole path removed"),
			    ptr);
		}
		else {
		    if(!sflag) {
			switch(rc) {
			    case -1:
				if(errno == EEXIST)
				    msg = gettxt(_SGI_DMMX_dirnotempty,
					"Directory not empty");
				else
				    msg = strerror(oserror());
				errcode++;
				break;
			    case -2:
				msg = gettxt(_SGI_DMMX_cantrmdotdot,
				    "Can not remove . or ..");
				break;
			    case -3:
				msg = gettxt(_SGI_DMMX_cantrmcwd,
				    "Can not remove current directory");
				break;
			}
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_notremoved,
				"%s: %s not removed"),
			    ptr,
			    remain);
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label, "%s", msg);
		    }
		}
		free(path);
		free(remain);
		continue;
	    }
	    /*
	     * No -p option. Remove only one directory
	     */
	    if(rmdir(ptr) == -1) {
		errcode++;
		switch(errno) {
		    case EEXIST:
			msg = gettxt(_SGI_DMMX_dirnotempty,
			    "Directory not empty");
			break;
		    case ENOTDIR:
			msg = gettxt(_SGI_DMMX_patcompnotdir,
			    "Path component not a directory");
			break;
		    case ENOENT:
			msg = gettxt(_SGI_DMMX_dirnotexist,
			    "Directory does not exist");
			break;
		    case EACCES:
			msg = gettxt(_SGI_DMMX_nowrperm,
			    "Search or write permission needed");
			break;
		    case EBUSY:
			msg = gettxt(_SGI_DMMX_mountpoint,
			    "Directory is a mount point or in use");
			break;
		    case EINVAL:
			msg = gettxt(_SGI_DMMX_cantrmcwddot,
			    "Can't remove current directory or ..");
			break;
		    default:
			msg = strerror(oserror());
			break;
		}
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label, "%s: %s", ptr, msg);
		continue;
	    }
        }
        exit(errcode?2:0);
}

