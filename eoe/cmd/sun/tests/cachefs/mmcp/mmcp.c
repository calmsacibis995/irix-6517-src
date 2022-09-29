/*	Copyright (c) 1984 AT&T	*/ /*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * frank@ceres.esd.sgi.com
 *	Internationalization done on Oct 27 1992
 */
#ident	"$Revision: 1.1 $"

/*
 * Combined mv/mmcp/ln command:
 *	mv file1 file2
 *	mv dir1 dir2
 *	mv file1 ... filen dir1
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <locale.h>
#include <fmtmsg.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

#define FTYPE(A)	(A.st_mode)
#define FMODE(A)	(A.st_mode)
#define FUSER(A)	(A.st_uid)
#define FGROUP(A)	(A.st_gid)
#define	IDENTICAL(A,B)	(A.st_dev==B.st_dev && A.st_ino==B.st_ino)
#define ISBLK(A)	((A.st_mode & S_IFMT) == S_IFBLK)
#define ISCHR(A)	((A.st_mode & S_IFMT) == S_IFCHR)
#define ISDIR(A)	((A.st_mode & S_IFMT) == S_IFDIR)
#define ISFIFO(A)	((A.st_mode & S_IFMT) == S_IFIFO)
#define ISREG(A)	((A.st_mode & S_IFMT) == S_IFREG)
#define ISLNK(A)	(((A).st_mode & S_IFMT) == S_IFLNK)

#define EQ(x,y)		!strcmp(x,y)
#define MODEBITS	(~S_IFMT)

#define FBUFSIZ		8192

long	actime, modtime;

char	*basename();

char	*cmd;
int	cpy;
int	mve;
int	lnk;
char	symlnk;
char	iflag;
char	rflag;
char	pflag;
int	silent;

/* cmd strings */

static	char cmd_mv[] = "mv";
static	char cmd_mvdir[] = "mvdir";
static	char cmd_ln[] = "ln";
static	char cmd_cp[] = "mmcp";

static	char cmd_label[16] = "UX:??";

static	char xmsgbuf[PATH_MAX+80];	/* buffer for message */

/* some often used strings */

static	char x_directory[] = "directory";

/*
 * compose message string
 */
struct xxtext {
	char	*xxcat_txt;	/* %s or message */
	char	*xxdef_txt;	/* default message */
};

char *compmsg(xtp)
struct xxtext *xtp;
{
	for(xmsgbuf[0] = 0; xtp->xxcat_txt; xtp++) {
	    strcat(xmsgbuf, (xtp->xxdef_txt)?
		gettxt(xtp->xxcat_txt, xtp->xxdef_txt) : xtp->xxcat_txt);
	    strcat(xmsgbuf, " ");
	}
	return(xmsgbuf);
}

/*
 * Display usage message.
 */
static char opt_cp[] = "mmcp [-irp]";
static char opt_mv[] = "mv [-fi]";
static char opt_ln[] = "ln [-fis]";
static char xx_MMX_usage[] = _SGI_DMMX_usagespc;
static char xx_usage[] = "      ";

static struct xxtext xxx_txt[] = {
	{ _SGI_DMMX_Usage,	"Usage:"	},
	{ opt_mv,		0		},
	{ _SGI_DMMX_file1,	"f1"		},
 	{ _SGI_DMMX_file2,	"f2"		},
	{ 0,			0,		}
};
static struct xxtext xxx_txt2[] = {
	{ xx_MMX_usage,		xx_usage	},
	{ opt_mv,		0		},
	{ _SGI_DMMX_file1toN,	"f1 ... f2"	},
	{ _SGI_DMMX_dir,	"dir"		},
	{ 0,			0,		}
};
static struct xxtext mv_txt3[] = {
	{ xx_MMX_usage,		xx_usage	},
	{ opt_mv,		0		},
	{ _SGI_DMMX_dir1,	"dir1"		},
	{ _SGI_DMMX_dir2,	"dir2"		},
	{ 0,			0		}
};

usage()
{
	if(cpy)
	    xxx_txt[1].xxcat_txt = xxx_txt2[1].xxcat_txt = opt_cp;
	if(lnk)
	    xxx_txt[1].xxcat_txt = xxx_txt2[1].xxcat_txt = opt_ln;
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(xxx_txt));
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(xxx_txt2));
	if(mve)
	    _sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(mv_txt3));
	exit(2);
}

void
Error1(s)
{
	_sgi_nl_error(SGINL_SYSERR2, cmd_label, "%s", s);
}

void
CannotOpen(f)
char *f;
{
	_sgi_nl_error(SGINL_SYSERR2, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"), f);
}

void
BadWrite(f)
char *f;
{
	_sgi_nl_error(SGINL_SYSERR2, cmd_label,
	    gettxt(_SGI_DMMX_CannotWrite, "bad write to %s"), f);
}

/*
 * main entry
 */
main(argc, argv)
register char *argv[];
{
	register int c, i, r, errflg = 0;
	struct stat s1, s2;
	extern char *optarg;
	extern int optind;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);		/* will be changed later */

	/*
	 * Determine command invoked (mv, mmcp, or ln)
	 */
	if (cmd = strrchr(argv[0], '/'))
		++cmd;
	else
		cmd = argv[0];

	/*
	 * Set flags based on command.
	 */
	if (EQ(cmd, cmd_mv) || EQ(cmd, cmd_mvdir))
	  	mve = 1;
	else if (EQ(cmd, cmd_ln))
		lnk = 1;
	else if (EQ(cmd, cmd_cp))
		cpy = 1;
	else {
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_ERROR, "%s",
		    gettxt(_SGI_DMMX_whoami,
			"I don't know who I am."));
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_FIX, "%s",
		    gettxt(_SGI_DMMX_cpmvln,
			"Cmd name must be 'mmcp', 'mv' or 'ln'"));
		return(1);
	}
	(void)strcpy(cmd_label + 3, cmd);
	(void)setlabel(cmd_label);

	/*
	 * Check for options:
	 * 	mmcp [-ipr] file1 [file2 ...] target
	 *	ln [-sif] file1 [file2 ...] target
	 *	mv [-if] file1 [file2 ...] target
	 *	mv [-if] dir1 target
	 */
	while ((c = getopt(argc, argv, (cpy ? "ipr" : ( lnk ? "sif" : "if"))))
		  != EOF) {
		switch (c) {
		case 'f':
			silent++;
			break;
		case 's':
			symlnk = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'r':
			rflag = 1;
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
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_insuffargs, "Insufficient arguments"));
		usage();
	}
	if (errflg != 0)
		usage();

	/*
	 * If there is more than a source and target,
	 * the last argument (the target) must be a directory
	 * which really exists.
	 */
	if (argc > 2) {
		if (stat(argv[argc-1], &s2) < 0) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_notfound, "%s not found."),
			    argv[argc-1]);
			exit(2);
		}

		if (!ISDIR(s2)) {
		    if (!rflag) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_Targetmusbe,
				"Target %s must be %s."),
			    argv[argc-1],
			    gettxt(_SGI_DMMX_directory, x_directory));
			usage();
		    }
		}
	}

	/*
	 * Perform a multiple argument mv|mmcp|ln by
	 * multiple invocations of move().
	 */
	r = 0;
	for (i=0; i<argc-1; i++)
		r += move(argv[i], argv[argc-1]);

	/*
	 * Show errors by nonzero exit code.
	 */
	exit(r?2:0);
}

static int query(omsg, yn)
char *omsg;
int yn;			/* yes or no default */
{
	register int i;
	char ibuf[MAX_INPUT];
	char obuf[PATH_MAX+80];

	_sgi_sfmtmsg(obuf, 0, cmd_label, MM_INFO, "%s", omsg);
	do {
	    fprintf(stdout, "%s", obuf);
	    i = _sgi_nl_query(stdin, ibuf, yn,
		_SGI_DMMX_query, _SGI_DMMX_querydef,
		_SGI_DMMX_yes, _SGI_DMMX_no);
	} while(i < 0);
	return(i);
}

move(source, target)
char *source, *target;
{
	register int c, i, ovrwrt = 0;
	char *sp;
	int sourceexists, targetexists;
	int from, to, ct, oflg;
	struct utimbuf times;
	struct stat s1, s2, s3;
	char fbuf[FBUFSIZ];
	char buf[PATH_MAX];
	char *inbuf, *outbuf;

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

	/*
	 * Make sure source file exists.
	 *
	 * You can move or link a dangling symbolic link.
	 * You can also symbolically link to a non-existent file.
	 */
	sourceexists = (cpy ? stat(source, &s1) : lstat(source, &s1)) >= 0;
	if (!sourceexists && !symlnk) {
		Error1(source);
		return(1);
	}

	actime = s1.st_atime;
	modtime = s1.st_mtime;

	/*
         * Make sure source file is not a directory,
	 * we don't move() directories, but we do move symlinks,
	 * and mmcp -r can copy directory trees.
	 */
	if (ISDIR(s1) && !symlnk && !mve && !rflag) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    cpy ?
			gettxt(_SGI_DMMX_Cannotcopy, "Cannot copy %s %s.")
		    :
			gettxt(_SGI_DMMX_Cannotlink, "Cannot link %s %s"),
		    gettxt(_SGI_DMMX_directory, x_directory),
		    source);
		return(1);
	}

	/*
	 * If it is a move command and we don't have write access
	 * to the source's parent then fail.
	 */
	if (mve && accs_parent(source, 2) == -1) {
		Error1(source);
		return(1);
	}

	/*
	 * If stat fails, then the target doesn't exist,
	 * we will create a new target with default file type of regular.
 	 */
	FTYPE(s2) = (rflag ? FTYPE(s1) : S_IFREG);

	targetexists = (stat(target, &s2) >= 0
			|| (!cpy && lstat(target, &s2) >= 0));
	if (targetexists) {
		/*
		 * If target is a directory,
		 * make complete name of new file
		 * within that directory.
		 */
		if (ISDIR(s2)) {
			sprintf(buf, "%s/%s", target, basename(source));
			target = buf;

			/*
			 * Reset expectations on type of target
			 */
			FTYPE(s2) = (rflag ? FTYPE(s1) : S_IFREG);
		}

		/*
		 * If filenames for the source and target are
		 * the same and the inodes are the same, it is
		 * an error.
		 */
		if (stat(target, &s2) >= 0
		    || (!cpy && lstat(target, &s2) >= 0)) {
			if (sourceexists && IDENTICAL(s1,s2)) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_areidentical,
					"%s and %s are identical"),
				    source,
				    target);
				return(1);
			}

			/*
			 * Prevent super-user from overwriting a directory
			 * structure with file or symlink of same name.
			 */
			if (!cpy && ISDIR(s2)) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_cannot_owr,
					"cannot overwrite %s %s"),
				    gettxt(_SGI_DMMX_directory, x_directory),
				    target);
				return(1);
			}

			/* if not silent, get confirmation from user */
			if (!silent && iflag && (!ISDIR(s2) || !cpy)) {
				sprintf(xmsgbuf,
				    gettxt(_SGI_DMMX_overwrite,
					"overwrite %s? "),
				    target);
				/* save if we decide to overwrite or not */
				ovrwrt = query(xmsgbuf, SGINL_DEFNO);
				if( ! ovrwrt)
					return(1);
			}

			/*
			 * Because copy doesn't unlink target,
			 * treat it separately.
			 */
			if (cpy)
				goto skip;

			/*
			 * If the user does not have access to
			 * the target, ask them -- if it is not
			 * silent and user invoked command
			 * interactively. Avoid this if the target
			 * is actually a symlink.
			 */
			if (lstat(target, &s3) >= 0
			    && !ISLNK(s3)
			    && access(target, 2) < 0
			    && isatty(fileno(stdin))
			    && !iflag
			    && !symlnk
			    && !silent) {
				sprintf(xmsgbuf,
				    gettxt(_SGI_DMMX_file_mode,
					"%s: %o mode? "),
				    target,
				    FMODE(s2) & MODEBITS);
				i = query(xmsgbuf, SGINL_DEFNO);
				if( !i)
					return(1);
			}

			/*
			 * Attempt to unlink the target if request is a 
			 * regular link.  If a symlink, then unlink the
			 * target only if ovrwrt has been set.
			 */
			if (lnk && (!symlnk || ovrwrt || silent) &&
				unlink(target) < 0) {
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				  gettxt(_SGI_DMMX_Cannotunlink,
					"Cannot unlink %s"),
				  target);
				return(1);
			}

		}
	}
skip:
	/*
	 * Either the target doesn't exist,
	 * or this is a copy command ...
	 */
	if (ISDIR(s1) && rflag) {		/* do recursive copy */
	    return rcopy(source, target);
	}

	if (lnk) {
	    register int result = 0;
	    if (symlnk) {
		if (symlink(source, target) < 0) {
		    if (errno == EEXIST)
		    	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_CannotCreat,
			    "Cannot create %s: File exists"),
			    target);
		    else
			Error1(target);
		    result = 1;
		}
	    } else if (link(source,target) < 0) {
		if (errno == EXDEV)
			goto diff_filsys;
		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
		    gettxt(_SGI_DMMX_Cannotlinkto, "Cannot link %s to %s"),
		    target,
		    source);
		result = 1;
	    }
	    return(result);
	}

	if (mve) {
	    if (rename(source, target) == 0)
		return (0);
            /*
	     * Can't rename directories across devices, for example but we
	     * want to fall through and do the copy for a simple file
	     */
	    if (errno != EXDEV) {
		Error1((errno == ENOENT && !targetexists) ? target : source);
		return (1);
	    }
	    /*
	     * if source is a symbolic link, do the 'copy' now.
	     */
	    if (ISLNK(s1)) {
		char lbuf[MAXPATHLEN];
		register int len;
		register int result = 1;

		len = readlink(source,lbuf,sizeof(lbuf)-1);
		if (len >= sizeof(lbuf)) {
		    lbuf[sizeof(lbuf)-1] = '\0';
		    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_syml_2long,
			    "Symbolic link %s -> %s too long"),
			source,
			lbuf);
		} else if (len < 0) {
		    Error1(source);
#ifdef sgi
		} else if (unlink(target) < 0 && errno != ENOENT) {
#else
		} else if (unlink(target) < 0) {
#endif
		    Error1(target);
		} else if (lbuf[len] = '\0',
			   symlink(lbuf, target) < 0) {
		    Error1(target);
#ifdef sgi
		} else if (unlink(source) < 0) {
		    Error1(source);
#endif
		} else {
		    result = 0;
		}
		return (result);
	    }
	    if (!ISREG(s1)) {
diff_filsys:
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_diff_filsys,
			"%s %s: different file systems"),
		    source,
		    target);
		return (1);
	    }
	}


	/*
	 * Attempt to open source for copy.
	 */
	if ((from = open(source, 0)) < 0) {
		CannotOpen(source);
		return (1);
	}

	/*
	 * If we are copying to complete a cross-device mv, and the
	 * target is a symbolic link, then remove the link before
	 * trying to copy the file.
	 */
	if (!cpy) {
		struct stat ls;
		if (lstat(target, &ls) >=0 && ISLNK(ls)) {
			if (unlink(target) < 0) {
				Error1(target);
				return (1);
			}
		}
	}
	/*
	 * Save a flag to indicate target existed.
	 */
	oflg = access(target, 0) == -1;

	/*
	 * Attempt to create a target.
	 */
	if (silent) {
		/*
		 * Without this, cross device moves where we are non-superuser
		 * and the target is not writable fail.
		 */
		(void)unlink(target);
	}
#if _ORIGINAL
	if ((to = creat(target, 0666)) < 0) {
		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
		    gettxt(_SGI_DMMX_CannotCreat, "Cannot create %s"), target);
		close(from);
		return (1);
	}

	/*
	 * Block copy from source to target.
	 */

	/*
	 * If we created a target, or are mv'ing,
	 * set target permissions to the source
	 * before any copying so that any partially copied
	 * file will have the source's permissions (at most)
	 * or umask permissions whichever is the most restrictive.
	 */

	if (oflg || mve || pflag)
		chmod(target, FMODE(s1));

	while ((ct = read(from, fbuf, sizeof fbuf)) != 0) {
		int wc;

		if (ct < 0 || (wc = write(to, fbuf, ct)) != ct) {
			if (ct < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_CannotRead,
					"bad read from %s"),
				    source);
			else if (wc < 0)
				BadWrite(target);
			else {
				/*
				 * Might be out of space
				 *
				 * Try to finish write
				 */
				int resid = ct-wc;

				if ((wc = write(to, fbuf+wc, resid)) == resid)
					/* worked on second attempt? */
					continue;
				else
				if (wc < 0)
					BadWrite(target);
				else
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_bad_copy,
					"bad copy to %s"),
				    target);
			}
			/*
			 * If target is a regular file,
			 * unlink the bad file.
			 */
			if (ISREG(s2))
				unlink(target);
			return (1);
		}
	}
#else /* _ORIGINAL */
	if ((to = open(target, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
		    gettxt(_SGI_DMMX_CannotCreat, "Cannot create %s"), target);
		close(from);
		return (1);
	}

	/*
	 * Block copy from source to target.
	 */

	/*
	 * If we created a target, or are mv'ing,
	 * set target permissions to the source
	 * before any copying so that any partially copied
	 * file will have the source's permissions (at most)
	 * or umask permissions whichever is the most restrictive.
	 */

	if (oflg || mve || pflag)
		chmod(target, FMODE(s1));

	/*
	 * mmap the from file
	 */
	if ( (inbuf = (char *)mmap( NULL, s1.st_size, PROT_READ, MAP_SHARED, from,
		(off_t)0 )) == (char *)-1 ) {
			perror( source );
			if (ISREG(s2))
				unlink(target);
			return( 1 );
	/*
	 * mmap the to file
	 */
	} else if ( (outbuf = (char *)mmap( NULL, s1.st_size,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_AUTOGROW, to,
		(off_t)0 )) == (char *)-1 ) {
			perror( target );
			if (ISREG(s2))
				unlink(target);
			return( 1 );
	}
	/*
	 * copy the source to the destination
	 */
	bcopy( inbuf, outbuf, s1.st_size );
	/*
	 * unmap the files
	 */
	munmap( (void *)inbuf, s1.st_size );
	munmap( (void *)outbuf, s1.st_size );
#endif /* _ORIGINAL */

	/*
	 * If it was a move, leave times alone.
	 */
	if (mve || pflag)
		setimes(target);

	(void)close(from);
	if (close(to) < 0) {
		BadWrite(target);
		if (ISREG(s2))
			unlink(target);
		return (1);
	}

	/*
	 * Attempt to unlink the source.
	 */
	/* unlink source iff rename failed and we decided to copy */
	if (!cpy && unlink(source) < 0) {
		Error1(source);

		/*
		 * If we can't unlink the source, assume we lack permission.
		 * Remove the target, as we may have copied it erroneously:
		 *  (1)	from an NFS filesystem, running as root.  In this case
		 *	the call to accs_parent(source) succeeds based on the
		 *	client credentials, but unlink(source) has just failed
		 *	because the server uid for client root is usually -2.
		 *  (2) because after the accs_parent(source) succeeded, we
		 *	lost a race to someone who removed write permission
		 *	from the source directory.
		 */
		if (stat(source, &s1) == 0)
			(void) unlink(target);
		return(1);
	}

	/*
	 * If it was a move, sets its owner to the source
	 */
	if (mve || pflag)
		chown(target, FUSER(s1), FGROUP(s1));
	return(0);
}


accs_parent(name, amode)
register char *name;
register int amode;
{
	register char c, *p, *q;
	char buf[PATH_MAX];

	p = q = buf;

	/*
	 * Copy name into buf and set 'q' to point to the last
	 * delimiter within name.
	 */
	while (c = *p++ = *name++)
		if (c == '/')
			q = p-1;

	/*
	 * If the name had no '\' or was "\" then leave it alone,
	 * otherwise null the name at the last delimiter.
	 */
	if (q == buf && *q == '/')
		q++;
	*q = '\0';

	/*
	 * Find the access of the parent.
	 * If no parent specified, use dot.
	 */
	return access(buf[0] ? buf : ".", amode);
}

/* recursive copy */
int
rcopy(from, to)
char *from, *to;
{
	register DIR *fold;
	register struct direct *dp;
	int errs = 0;
	struct stat t_sbuf, f_sbuf;
	int made_dir = 0;
	char fromname[MAXPATHLEN+1];

	fold = opendir(from);
	if (fold == 0) {
		CannotOpen(from);
		return (1);
	}

	if (stat(to, &t_sbuf) < 0) {
		made_dir = 1;
		FMODE(f_sbuf) = 0;
		if (0 > stat(from, &f_sbuf))
			Error1(from);
		if (mkdir(to, FMODE(f_sbuf)|0700) < 0) {
			Error1(to);
			return(1);
		}
	} else if (!ISDIR(t_sbuf)) {
		usage();
	}

	for (;;) {
		dp = readdir(fold);
		if (dp == 0) {
			closedir(fold);
			if (made_dir) {
				if (0 > chmod(to, FMODE(f_sbuf)))
					Error1(to);
				else
					if(pflag)
						setimes(to);
			}
			return (errs);
		}
		if (dp->d_ino == 0
		    || !strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

		if (strlen(from) + 1 + dp->d_namlen >= sizeof(fromname)-1) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_Name2long, "%s/%s: Name too long"),
			    from,
			    dp->d_name);
			errs++;
			continue;
		}
		(void) sprintf(fromname, "%s/%s", from, dp->d_name);

		errs += move(fromname, to);
	}
}

char *
basename(name)
register char *name;
{
	register char *p;

	/*
	 * Return just the file name given the complete path.
	 */
	p = name;

	/*
	 * While there are characters left,
	 * set name to start after last /.
	 */
	while(*p)
	    if(*p++ == '/' && *p)
		name = p;
	return(name);
}

setimes(to)
char *to;
{
        struct utimbuf times;

        times.actime = actime;
        times.modtime = modtime;
        utime(to, &times);
}

