/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved. 					*/

#ident	"@(#)rm:rm.c	1.21.1.5"

/***************************************************************************
 * Command: rm
 *
 * Inheritable Privileges: P_MACREAD,P_MACWRITE,P_DACREAD,
 *			   P_DACWRITE,P_COMPAT,P_FILESYS
 *       Fixed Privileges: None
 *
 * Notes: rm [-fir] file ...
 *
 *
 ***************************************************************************/



#include	<sys/types.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include	<dirent.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<locale.h>
#include	<pfmt.h>
#include	<sgi_nl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<msgs/uxsgicore.h>

#define	DIRECTORY	((buffer.st_mode&S_IFMT) == S_IFDIR)
#define	SYMLINK		((buffer.st_mode&S_IFMT) == S_IFLNK)
#define WRITEPERM       ((buffer.st_mode&S_IAMB)&S_IWUSR)
#define	FAIL		-1
int	maxfiles;			/* Maximum number of open files */
long	maxpathlen;
#define	NAMESIZE	MAXNAMLEN + 1	/* "/" + (file name size) */
static	int	errcode;
static	int interactive, recursive, silent; /* flags for command line options */

static	void	rm(char *);
static	void	undir(char *, dev_t, ino64_t);
static	int	query(char *, int);
static	int	mypath(dev_t, ino64_t);

static const char badopen[] = ":4:Cannot open %s: %s\n";
static const char nomem[] = ":312:Out of memory: %s\n";
static const char cmd_label[] = "UX:rm";
static  char xmsgbuf[1024];     /* buffer for message */


/*
 * Procedure:     main
 *
 * Restrictions:
 *                setlocale: none
 *                getopt: none
 *                pfmt: none
 */
main(int argc, char *argv[])
{
	int	errflg = 0;
	int	c;
	int	arglen;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel(cmd_label);
	while ((c = getopt(argc, argv, "fRri")) != EOF) {
		switch (c) {
		case 'f':
			silent = 1;
			if (interactive)		/* Ignore if set */
				interactive = 0;
			break;
		case 'i':
			interactive = 1;
			if (silent)			/* Ignore if set */
				silent = 0;
			break;
		case 'r':
		case 'R':
			recursive = 1;
			break;
		case '?':
			errflg = 1;
			break;
		}
	}
	/* 
	 * For BSD compatibility allow '-' to delimit the end 
	 * of options, but not if -- was given as the last option.
	 */
	if (optind < argc && optind > 1 && !strcmp(argv[optind], "-") &&
	    strcmp(argv[optind - 1], "--"))
		optind++;
	argc -= optind;
	argv = &argv[optind];
	if ((argc < 1 && !silent) || errflg) {
		if (!errflg)
			(void)pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
		(void)pfmt(stderr, MM_ACTION,
			":433:Usage: rm [-fir] file ...\n");
		exit(2);
	}
	maxfiles = getdtablesize();
	/*
	 * XXX really should check per argument
	 */
	maxpathlen = pathconf("/", _PC_PATH_MAX);
	if (maxpathlen < 0)
		maxpathlen = PATH_MAX;
	while (argc-- > 0) {
		if (!strcmp(*argv, "..")) {
			pfmt(stderr, MM_ERROR,
				"uxsgicore:81:cannot remove ..\n");
			argv++;
			errcode++;
			continue;
		}
		/*
		 * Check for /.. at the end of a shell-expanded argument;
		 * in -r mode this causes removal ABOVE the specified
		 * target directory!
		 */
		arglen = strlen(*argv);
		if ((arglen >= 3) && !strncmp(*argv + arglen - 3, "/..", 3)) {
			*(*argv + arglen - 3) = '\0';
			pfmt(stderr, MM_ERROR,
				"uxsgicore:82:cannot remove files above %s\n",
				*argv);
			argv++;
			errcode++;
			continue;
		}
		rm(*argv);
		argv++;
	}
	return errcode ? 2 : 0;
}

/*
 * Procedure:     rm
 *
 * Restrictions:
 *                lstat(2): none
 *                pfmt: none
 *                strerror: none
 *                access(2): none
 *                isatty: none
 *                unlink(2): none
 */
static void
rm(char *path)
{
	struct stat64 buffer;

	/* 
	 * Check file to see if it exists.
	 */
	if (lstat64(path, &buffer) == FAIL) {
		if (!silent) {
			pfmt(stderr, MM_ERROR, ":5:Cannot access %s: %s\n",
				path, strerror(errno));
			++errcode;
		}
		return;
	}
	/*
	 * If it's a directory, remove its contents.
	 */
	if (DIRECTORY) {
		undir(path, buffer.st_dev, buffer.st_ino);
		return;
	}
	/*
	 * If interactive, ask for acknowledgement.
	 */
	if (interactive) {
                sprintf(xmsgbuf,
			gettxt(_SGI_MMX_rm_filrm,"File %s. Remove ? "),
			path);
                if (!query(xmsgbuf, SGINL_DEFNO))
			return;
	} else if (!silent) {
		/* 
		 * If not silent, and stdin is a terminal, and there's
		 * no write access, and the file isn't a symbolic link,
		 * ask for permission.
		 */
		if (!SYMLINK && access(path, W_OK) == FAIL &&
		    isatty(fileno(stdin))) {
                        sprintf(xmsgbuf,
                                gettxt(_SGI_MMX_rm_modrm,
					"%s: %o mode. Remove ? "),
				path, buffer.st_mode & 0777);
			/*
			 * If permission isn't given, skip the file.
			 */
                        if (!query(xmsgbuf, SGINL_DEFNO))
				return;
		}
	}
	/*
	 * If the unlink fails, inform the user if interactive or not silent.
	 */
	if (unlink(path) == FAIL) {
		if (!silent || interactive) 
			pfmt(stderr, MM_ERROR, ":436:%s not removed: %s.\n",
				path, strerror(errno));
		if (!silent || (errno != ENOENT))
			++errcode;
	}
}

/*
 * Procedure:     undir
 *
 * Restrictions:
 *                pfmt: none
 *                opendir: none
 *                strerror: none
 *                sprintf: none
 *                rmdir(2): none
 */
static void
undir(char *path, dev_t dev, ino64_t ino)
{
	char		*newpath;
	DIR		*name;
	dirent64_t	*direct;
	int		isinpath;
	struct stat64	buffer;
	struct rlimit	fdlim;
	static int	flim_check = 0;

	/*
	 * If "-r" wasn't specified, trying to remove directories
	 * is an error.
	 */
	if (!recursive) {
		pfmt(stderr, MM_ERROR, ":437:%s not removed: directory\n",
			path);
		++errcode;
		return;
	}
	/*
	 * If interactive and this file isn't in the path of the
	 * current working directory, ask for acknowledgement.
	 */
	isinpath = mypath(dev, ino);
        /*
	 * print this at the end and also at the beginning : sgi: rags
	 */
	if (interactive && !isinpath) {
		sprintf(xmsgbuf,
			gettxt(_SGI_MMX_rm_dirrm_con, "Directory %s. Remove contents? "),
			path);
		/*
		 * If the answer is no, skip the directory.
		 */
		if (!query(xmsgbuf, SGINL_DEFNO))
			return;
	}
	/*
	 * Open the directory for reading.
	 */
	if ((name = opendir(path)) == NULL) {
		pfmt(stderr, MM_ERROR, badopen, path, strerror(errno));
		errcode++;
		return;	
	}
	/*
	 * Read every directory entry.
	 */
	while ((direct = readdir64(name)) != NULL) {
		/*
		 * Ignore "." and ".." entries.
 		 */
		if (!strcmp(direct->d_name, ".") ||
		    !strcmp(direct->d_name, ".."))
			continue;
		/*
		 * Try to remove the file.
		 */
		newpath = (char *)malloc((strlen(path) + NAMESIZE + 1));
		if (newpath == NULL) {
			pfmt(stderr, MM_ERROR, nomem, strerror(errno));
			exit(1);
		}
		/*
		 * Limit the pathname length so that a clear error message
		 * can be provided.
		 */
		if (strlen(path) + strlen(direct->d_name) + 2 > maxpathlen) {
			pfmt(stderr, MM_ERROR, ":438:Path too long (%d/%d).\n",
				strlen(path) + strlen(direct->d_name) + 2,
				maxpathlen);
			exit(2);
		}
		sprintf(newpath, "%s/%s", path, direct->d_name);
		/*
		 * If we need more file descriptors, and haven't tried
		 * to raise our limit yet, do so.
		 */
		if (name->dd_fd >= maxfiles - 1 && !flim_check) {
			if (getrlimit(RLIMIT_NOFILE, &fdlim) == 0 &&
			    fdlim.rlim_cur < fdlim.rlim_max &&
			    (fdlim.rlim_cur = fdlim.rlim_max) &&
			    setrlimit(RLIMIT_NOFILE, &fdlim) == 0)
				maxfiles = fdlim.rlim_cur;
			flim_check = 1;
		}
		/*
		 * If a spare file descriptor is available, just call the
		 * "rm" function with the file name; otherwise close the
		 * directory and reopen it when the child is removed.
		 */
		if (name->dd_fd >= maxfiles - 1) {
			closedir(name);
			rm(newpath);
			if ((name = opendir(path)) == NULL) {
				pfmt(stderr, MM_ERROR, badopen, path,
					strerror(errno));
				errcode++;
				return;
			}
		} else
			rm(newpath);
		free(newpath);
	}
	/*
	 * Close the directory we just finished reading.
	 */
	closedir(name);
	/*
	 * The contents of the directory have been removed.  If the
	 * directory itself is in the path of the current working
	 * directory, don't try to remove it.
	 * When the directory itself is the current working directory, mypath()
	 * has a return code == 2.
	 */
	switch (isinpath) {
	case 2:
		pfmt(stderr, MM_ERROR,
			"uxsgicore:83:Cannot remove the current working "
			"directory\n\t%s\n",
			path);
		++errcode;
		return;
	case 1:
		pfmt(stderr, MM_ERROR,
			":439:Cannot remove any directory in the path\n"
			"\tof the current working directory\n\t%s\n",
			path);
		return;
	case 0:
                /*
		 * check if directory has write perms
		 */
                lstat64(path, &buffer);
                if ((interactive || !WRITEPERM) && !silent) {
                        sprintf(xmsgbuf,
				gettxt(_SGI_MMX_rm_dirrm,
					"Directory %s. Remove ? "), path);
                        /*
                         * If the answer is no, skip the directory.
                         */
                        if (!query(xmsgbuf, SGINL_DEFNO))
                                return;
                }
		break;
	}
	if (rmdir(path) == FAIL) {
		if (errno != EEXIST)
			pfmt(stderr, MM_ERROR,
				":440:Cannot remove directory %s: %s\n",
				path, strerror(errno));
		else
			pfmt(stderr, MM_ERROR,
				"uxsgicore:949:Cannot remove directory %s: "
				"Directory not empty\n",
				path);
		++errcode;
	}
}

/*
 * Procedure:     query
 *
 */
static int
query(char *omsg, int yn)
{
        int	i;
        char	ibuf[MAX_INPUT];

        do {
		/* print label if they want it */
		pfmt(stderr, MM_NOGET|MM_INFO, "");
		fprintf(stderr, "%s", omsg);
		i = _sgi_nl_query_fd(stdin, ibuf, yn,
			_SGI_MMX_query, _SGI_MMX_querydef,
			_SGI_MMX_yes, _SGI_MMX_no, stderr);
        } while (i < 0 && !(feof(stdin)||ferror(stdin)));
        return ((feof(stdin)||ferror(stdin)) ? yn : i);
}

/*
 * Procedure:     mypath
 *
 * Restrictions:
 *               pfmt: none
 *               strerror: none
 *               lstat(2): none
 */
static int
mypath(dev_t dev, ino64_t ino)
{
	typedef struct di {
		dev_t	d;
		ino64_t	i;
	} di_t;
	static di_t	*dip = (di_t *)0;
	int		i;

	if (!dip) {
		struct stat64	buffer;
		int		j;
		dev_t		lastdev = (dev_t)-1;
		ino64_t		lastino = (ino64_t)-1;
		char		*path;

		path = (char *)malloc(maxpathlen);
		if (path == NULL) {
			pfmt(stderr, MM_ERROR, nomem, strerror(errno));
			exit(1);
		}
		for (i = 0; ; i++) {
			/*
			 * Starting from ".", walk toward the root, looking at
			 * each directory along the way.
			 */
			strcpy(path, ".");
			for (j = 0; j < i; j++) {
				if (j == 0)
					strcpy(path, "..");
				else
					strcat(path,"/..");
			}
			lstat64(path, &buffer);
			dip = realloc(dip, (i + 1) * sizeof(*dip));
			if (dip == NULL) {
				pfmt(stderr, MM_ERROR, nomem, strerror(errno));
				exit(1);
			}
			if ((buffer.st_dev == lastdev &&
			     buffer.st_ino == lastino) ||
			    strlen(path) + 3 > maxpathlen) {
				dip[i].d = (dev_t)-1;
				dip[i].i = (ino64_t)-1;
				free(path);
				break;
			} else {
				dip[i].d = buffer.st_dev;
				dip[i].i = buffer.st_ino;
			}
			/*
			 * Save the current dev and ino, and loop again to go
			 * back another level.
			 */
			lastdev = buffer.st_dev;
			lastino = buffer.st_ino;
		}
	}
	for (i = 0; ; i++) {
		if (dip[i].d == dev && dip[i].i == ino)
			return i ? 1 : 2;
		if (dip[i].d == (dev_t)-1 && dip[i].i == (ino64_t)-1)
			return 0;
	}
}
