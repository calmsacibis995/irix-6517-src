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
#ident	"$Revision: 1.68 $"

/*
 * Combined mv/cp/ln command:
 *	mv file1 file2
 *	mv dir1 dir2
 *	mv file1 ... filen dir1
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <locale.h>
#include <fmtmsg.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <ftw.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/attributes.h>
#include <sys/statfs.h>
#include <sys/syssgi.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/mman.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>


#define FTYPE(A)	(A.st_mode)
#define FMODE(A)	(A.st_mode)
#define FUSER(A)	(A.st_uid)
#define FGROUP(A)	(A.st_gid)
#define	IDENTICAL(A,B)	(A.st_dev==B.st_dev && A.st_ino==B.st_ino)
#define ISBLK(A)	((A.st_mode & S_IFMT) == S_IFBLK)
#define ISCHR(A)	((A.st_mode & S_IFMT) == S_IFCHR)
#define ISDIR(A)	((A.st_mode & S_IFMT) == S_IFDIR)
#define ISFIFO(A)	((A.st_mode & S_IFMT) == S_IFIFO)
#define ISREG(A)	(((A).st_mode & S_IFMT) == S_IFREG)
#define ISLNK(A)	(((A).st_mode & S_IFMT) == S_IFLNK)

#define EQ(x,y)		!strcmp(x,y)
#define MODEBITS	(~S_IFMT)

#define BUFFER_SIZE	(1<<16)

#define BUFSIZE		1024
#define min(x, y) ((x) < (y) ? (x) : (y))

int	fstyp_xfs;
struct getbmap	*outmap = NULL;
int		outmap_size = 0;

#define FS_XFS	1
#define FS_HUH	0

struct	dioattr	from_dio,
		to_dio;
int	argv_blksz_dio=0;	/* Default is MAX Block size */
int	do_dio=0;		/* Default = Don't do DIRECT_IO */
int	verbose=0;
int	mmap_in = 0;
int	pagesize;
int	cpy_attribs = 0;
int	cpy_holes = 1;
int	try_prealloc = 0;
int	do_rtime = 0;
int	rtextsize = 0;
int	rt_size = -1;
int	do_pad = 0;

char	*basename();
void	xdev_quit();
int	xdev_state = 0;

char	*cmd;
int	cpy;
int	mve;
int	lnk;
char	symlnk;
char	iflag;
char	rflag;
char	Rflag;
char	fflag;
char	pflag;
int	silent;

/* cmd strings */

static	char cmd_mv[] = "mv";
static	char cmd_ln[] = "ln";
static	char cmd_cp[] = "cp";

static	char cmd_label[16] = "UX:??";

static	char xmsgbuf[PATH_MAX+80];	/* buffer for message */

/* some often used strings */

static	char x_directory[] = "directory";
static	char x_file[] = "file";

/*
 * compose message string
 */
struct xxtext {
	char	*xxcat_txt;	/* %s or message */
	char	*xxdef_txt;	/* default message */
};

int read_fd_bmap(int, int, struct stat64 *, struct stat64 *);
int nftw_cp(const char *, const struct stat64 *, int flag, struct FTW *);
int nftw_rm(const char *, const struct stat64 *, int flag, struct FTW *);
char xdev_target[MAXPATHLEN+1];
char xdev_source[MAXPATHLEN+1];

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
 *
 * For cp the options mean:
 * -a	copy user attributes
 * -M	memory map input instead of doing read *** disabled ***
 * -D	use direct I/O
 * -A	preallocate space for output file	*** disabled ***
 * -t   create output file in realtime partition
 * -b blk	specify I/O block size to use
 * -e extsize	realtime extent size requested
 * -P		pad realtime output to extent boundary
 */
static char opt_cp[] = "cp [-aDfirRp] [-b size] [-tP -[e size]]";
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

void
usage(exitflg)
{
	if(cpy)
	    xxx_txt[1].xxcat_txt = xxx_txt2[1].xxcat_txt = opt_cp;
	if(lnk)
	    xxx_txt[1].xxcat_txt = xxx_txt2[1].xxcat_txt = opt_ln;
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(xxx_txt));
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(xxx_txt2));
	if(mve)
	    _sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO, compmsg(mv_txt3));
	if(exitflg)
	    exit(2);
}

void
Error1(s)
char *s;
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
	struct stat64 s1, s2;
	extern char *optarg;
	extern int optind;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);		/* will be changed later */

	/*
	 * Determine command invoked (mv, cp, or ln)
	 */
	if (cmd = strrchr(argv[0], '/'))
		++cmd;
	else
		cmd = argv[0];

	/*
	 * Set flags based on command.
	 */
	if (EQ(cmd, cmd_mv))
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
			"Cmd name must be 'cp', 'mv' or 'ln'"));
		return(1);
	}
	(void)strcpy(cmd_label + 3, cmd);
	(void)setlabel(cmd_label);

	/*
	 * Check for options:
	 * 	cp [-fiprR] file1 [file2 ...] target
	 *	ln [-sif] file1 [file2 ...] target
	 *	mv [-if] file1 [file2 ...] target
	 *	mv [-if] dir1 target
	 */
	while ((c = getopt(argc, argv, (cpy ? "fiprae:RPDtb:v" : ( lnk ? "sif" : "if"))))
		  != EOF) {
		switch (c) {
		case 'a':
			cpy_attribs = 1;
			break;
#ifdef NOTYET
		case 'M':
			mmap_in = 1;
			pagesize = sysconf(_SC_PAGESIZE);
			break;
		case 'A':
			try_prealloc = 1;
			break;
#endif
		case 'D':
			do_dio=1;
			break;
		case 'P':
			do_pad=1;
			break;
		case 'b':
			if( (argv_blksz_dio=atoi(argv[optind-1])) <= 0 )
				argv_blksz_dio=0;
			break;
		case 'e':
			if( (rt_size=atoi(argv[optind-1])) <= 0 )
				rt_size=0;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			if(cpy) 
				fflag = 1;
			else
				silent++;
			if(mve && iflag)
				iflag=0;
			break;
		case 's':
			symlnk = 1;
			break;
		case 'i':
			iflag = 1;
			if(mve && silent)
				silent=0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 't':
			do_rtime = 1;
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
		usage(1);
	}
	if (errflg != 0)
		usage(1);

	/* Real time or direct I/O do not mix with hole copying and
	 * memory mapped I/O.
	 */
	if (do_rtime || do_dio) {
		cpy_holes = 0;
		mmap_in = 0;
	}

	/*
	 * If there is more than a source and target,
	 * the last argument (the target) must be a directory
	 * which really exists.
	 */
	if (argc > 2) {
		if (stat64(argv[argc-1], &s2) < 0) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_notfound, "%s not found."),
			    argv[argc-1]);
			exit(2);
		}

		if (!ISDIR(s2)) {
		    if (!rflag && !Rflag) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_Targetmusbe,
				"Target %s must be %s."),
			    argv[argc-1],
			    gettxt(_SGI_DMMX_directory, x_directory));
			usage(1);
		    }
		}
	}

	/*
	 * Initialize global for XFS filesystem type so we can
	 * special case code for it.
	 */
	fstyp_xfs = sysfs(GETFSIND, FSID_XFS);


	/*
	 * Perform a multiple argument mv|cp|ln by
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
	    fprintf(stderr, "%s", obuf);
	    i = _sgi_nl_query(stdin, ibuf, yn,
		_SGI_DMMX_query, _SGI_DMMX_querydef,
		_SGI_DMMX_yes, _SGI_DMMX_no);
	} while (i < 0 && !feof(stdin));
	return(feof(stdin) ? yn : i);
}

move(source, target)
char *source, *target;
{
	register int c, i, ovrwrt = 0;
	char *sp;
	int sourceexists, targetexists;
	int from, to,     oflg;
	int ct, wc, wc_b4;
	struct utimbuf times;
	struct stat64 s1, s2, s3;
	void *fbuf;
	off64_t len, cnt, siz, pos;
#ifdef NOTYET
	off64_t mmap_pos, mmap_size;
	int do_prealloc;
	void *mmap_buf;
#endif
	int nextents, extent;
	int rtextsize;
	flock64_t space;
	struct fsxattr  fsxattr;
	unsigned	blksz_dio;
	unsigned	dio_mem;
	unsigned	dio_max;
	unsigned	dio_min;

	char buf[PATH_MAX+1];
	mode_t newmode;

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
	if (symlnk)
		sourceexists = 0;
	else if (cpy && !Rflag)
		sourceexists = stat64(source, &s1) >= 0;
	else
		sourceexists = lstat64(source, &s1) >= 0;
	if (!sourceexists && !symlnk) {
		Error1(source);
		return(1);
	}

	times.actime = s1.st_atime;
	times.modtime = s1.st_mtime;

	/*
         * Make sure source file is not a directory,
	 * we don't move() directories, but we do move symlinks,
	 * and cp -r can copy directory trees.
	 */
	if (!symlnk && ISDIR(s1) && !mve && !rflag && !Rflag) {
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
	if (mve && accs_parent(source, W_OK|EFF_ONLY_OK) == -1) {
		Error1(source);
		return(1);
	}

	/*
	 * If stat fails, then the target doesn't exist,
	 * we will create a new target with default file type of regular.
 	 */
	FTYPE(s2) = ((rflag||Rflag) ? FTYPE(s1) : S_IFREG);

	targetexists = (stat64(target, &s2) >= 0
			|| (!cpy && lstat64(target, &s2) >= 0));

	if (cpy && do_rtime) {
		char	*ptr;

		if (targetexists) {
			ptr = target;
		} else {
			strcpy(buf, target);
			ptr = strrchr(buf, '/');
			if (ptr) {
				*ptr = '\0';
			} else {
				strcpy(buf, ".");
			}
			ptr = buf;
		}

		if ((rtextsize = xfsrtextsize( ptr )) <= 0 ) {
			Error1("no XFS real time partition for destination");
			return(1);
		}

		if ((rt_size != -1) && (rt_size % rtextsize)) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
			    gettxt(_SGI_MMX_badextsize, _SGI_SMMX_badextsize),
			    rt_size, rtextsize);
			return(1);
		}
	}

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
			FTYPE(s2) = ((rflag||Rflag) ? FTYPE(s1) : S_IFREG);
		}

		/*
		 * If filenames for the source and target are
		 * the same and the inodes are the same, it is
		 * an error.
		 */
		if (stat64(target, &s2) >= 0
		    || (!cpy && lstat64(target, &s2) >= 0)) {
			if (sourceexists && IDENTICAL(s1,s2)) {
				if (!silent) {
				  _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
						gettxt(_SGI_DMMX_areidentical,
						       "%s and %s are identical"),
						source, target);
				  return(1);
				} else {
				/*
				 * Since this is already the desired state
				 * and we are silent, return 0 without
				 * mentioning an error.
				 */
				  return(0);
				}
			}
			if(mve && ISDIR(s1) && !ISDIR(s2)) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_cannot_owr,
					"cannot overwrite %s %s"),
				    gettxt(_SGI_DMMX_file, x_file),
				    target);
				return(1);
			}

			/*
			 * Prevent super-user from overwriting a directory
			 * structure with file or symlink of same name.
			 */
			if (lnk && ISDIR(s2) || mve && !ISDIR(s1) && ISDIR(s2)) {
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
			if (lstat64(target, &s3) >= 0
			    && !ISLNK(s3)
			    && access(target, W_OK|EFF_ONLY_OK) < 0
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
			 * Do not unlink target if regular link and not
			 * silent and interactive.
			 */
			if (lnk && !symlnk && !silent && !iflag) {
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				  gettxt(_SGI_DMMX_Notunlink,
					"May not unlink existing %s %s"),
				  target,"");
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
	if (ISDIR(s1) && (rflag || Rflag)) {		/* do recursive copy */
	    return rcopy(source, target, &times);
	}

	if (lnk) {
	    register int result = 0;
	    static int (*linkfunc)(const char *, const char *);
	    if (!symlnk && !linkfunc) {
		char *xpg = getenv("_XPG");
		if (xpg && atoi(xpg) > 0)
		    linkfunc = linkfollow;
		else
		    linkfunc = link;
	    }
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
	    } else if ((*linkfunc)(source,target) < 0) {
		if (errno == EXDEV) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_diff_filsys,
				"%s %s: different file systems"),
			    source, target);
			return (1);
		}
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
	    if (ISDIR(s1)) {
		int ret;
		mode_t mask = umask(0);

		/* Check if target exists */
		if(stat64(target, &s2) >= 0) {
			if(rmdir(target) < 0) {
				if(errno == EEXIST)
					errno = ENOTEMPTY;
				Error1(target);
				return(1);
			}
		} 
		strcpy(xdev_target,target);
		strcpy(xdev_source,source);

		/* Simulate a cp command with -pR flags */
		mve = 0;
		cpy = pflag = Rflag = 1;
		xdev_state = 1;

		signal(SIGINT,  xdev_quit);
		signal(SIGQUIT, xdev_quit);
		signal(SIGTERM, xdev_quit);

		/* Walk the source tree for the copy to target */
		if((ret = nftw64(source, nftw_cp, OPEN_MAX, 
				FTW_DEPTH|FTW_PHYS|FTW_MOUNT)) != 0 ) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_partdest,
			    "Warning - incomplete copy to target tree: %s"),target);
			mve = 1;
			cpy = pflag = Rflag = 0;
			umask(mask);
			return(1);
		}
		umask(mask);
		mve = 1;
		cpy = pflag = Rflag = 0;

		/* Walk the source tree for removal */

		xdev_state = 2;
		if((ret = nftw64(source, nftw_rm, OPEN_MAX, 
				FTW_DEPTH|FTW_PHYS|FTW_MOUNT)) != 0 ) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_partsrc,
			    "Warning - incomplete removal of source tree: %s"),source);
			return(1);
		}
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		return(0);
	    }
	    if(!ISREG(s1))
	    {
	    	int create_error = 0;
	    	int rm_error = 0;
	    	int mod_error = 0;

	    	if(accs_parent(target, W_OK|EFF_ONLY_OK) == -1) {
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				gettxt(_SGI_DMMX_CannotAccess,
				"Cannot access %s"), target);
			return(1);
	    	}

	    	switch(FMODE(s1)&S_IFMT)
	    	{
	    		case S_IFIFO:
	    			if(mkfifo(target,FMODE(s1)) < 0)
	    				++create_error;
	    			else if(unlink(source) < 0)
	    				++rm_error;
	    			break;
	    		case S_IFCHR:			/* character special */
	    		case S_IFBLK:			/* block special */
	    		case S_IFSOCK:			/* socket */
	    			if(mknod(target,FMODE(s1),s1.st_rdev) < 0)
	    				++create_error;
	    			else if(unlink(source) < 0)
	    				++rm_error;
	    			break;
	    		default:
	    			++create_error;
	    			break;
	    	}
	    	if( rm_error ) {
	    		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
	    			gettxt(_SGI_DMMX_Cannotunlink,
				"Cannot unlink %s"), target);
	    		return (1);
	    	}
	    	if( create_error ) {
	    		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
	    			gettxt(_SGI_DMMX_CannotCreat, 
	    			"Cannot create %s"), target);
	    		return (1);
	    	}
		if(utime(target, &times) < 0)
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_utime,
			    "%s: Cannot update target times"),target);
		if(chown(target, FUSER(s1), FGROUP(s1)) < 0) {
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_chown,
			    "%s: Cannot change target ownership to uid(%d) gid(%d)"),
			    target, FUSER(s1), FGROUP(s1));
			if(chmod(target, FMODE(s1)&(~(S_ISUID|S_ISGID))) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_chmod,
				    "%s: Cannot change target mode to (%o)"),
				    target, FMODE(s1));
		}
		else if(chmod(target, FMODE(s1)) < 0)
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_chmod,
			    "%s: Cannot change target mode to (%o)"),
			    target, FMODE(s1));
	    	return(0);
	    }
	}
	if(!S_ISREG(s1.st_mode) && cpy && Rflag)
	{
		int create_error = 0;
		int syml = 0;

		switch(FMODE(s1)&S_IFMT)
		{
			case S_IFIFO:
				if(mkfifo(target,FMODE(s1)) < 0)
					++create_error;
				break;
			case S_IFCHR:			/* character special */
			case S_IFBLK:			/* block special */
			case S_IFSOCK:			/* socket */
				if(mknod(target,FMODE(s1),s1.st_rdev) < 0)
					++create_error;
				break;
			case S_IFLNK:			/* symbolic link */
			{
				char lbuf[MAXPATHLEN];
				register int len;

				++syml;

				len = readlink(source,lbuf,sizeof(lbuf)-1);
				if (len >= sizeof(lbuf)) 
				{
				    lbuf[sizeof(lbuf)-1] = '\0';
				    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					gettxt(_SGI_DMMX_syml_2long,
					"Symbolic link %s -> %s too long"),
					source,lbuf);
				    ++create_error;
				} 
				else if (len < 0) 
				    ++create_error;
				else if (lbuf[len] = '\0', symlink(lbuf, target) < 0)
				    ++create_error;
				break;
			}
			default:
				++create_error;
				break;
		}
		if( create_error )
		{
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				gettxt(_SGI_DMMX_CannotCreat, 
				"Cannot create %s"), target);
			return (1);
		}
		if(!syml && pflag)
		{
			if(utime(target, &times) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_utime,
				    "%s: Cannot update target times"),target);
			if(chown(target, FUSER(s1), FGROUP(s1)) < 0) {
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_chown,
				    "%s: Cannot change target ownership to uid(%d) gid(%d)"),
				    target, FUSER(s1), FGROUP(s1));
				if(chmod(target,FMODE(s1)&(~(S_ISUID|S_ISGID))) < 0)
					_sgi_nl_error(SGINL_SYSERR2, cmd_label,
					    gettxt(_SGI_DMMX_xpg_mv_chmod,
					    "%s: Cannot change target mode to (%o)"),
					    target, FMODE(s1));
			}
			else if(chmod(target,FMODE(s1)) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_chmod,
				    "%s: Cannot change target mode to (%o)"),
				    target, FMODE(s1));
		}
		return(0);
	}

	/*
	 * Attempt to open source for copy.
	 */
	if ((from = open(source, 0)) < 0) {
		CannotOpen(source);
		return (1);
	}

	if (do_rtime) do_dio = 1;
	if(!do_dio || !cpy) do_dio=0;
	else {
		if( (fcntl(from,F_SETFL,0 | FDIRECT)) == EINVAL ) {
			if(verbose) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_nodiosupp,
				           _SGI_SMMX_nodiosupp), source);
			}
			do_dio=0;
			close(from);
			from =  open(source, 0);
		}
		else {
			if( (fcntl(from,F_DIOINFO, &from_dio)) == -1 ) {
				if(verbose) {
					_sgi_nl_error(SGINL_SYSERR, cmd_label,
					gettxt(_SGI_DMMX_dioerror,
					       _SGI_SMMX_dioerror), source);
				}
				do_dio=0;
				close(from);
				from = open(source,0);
			}
		}
	}

	/*
	 * If we are copying to complete a cross-device mv, and the
	 * target is a symbolic link, then remove the link before
	 * trying to copy the file.
	 */
	if (!cpy) {
		struct stat64 ls;
		if (lstat64(target, &ls) >=0 && ISLNK(ls)) {
			if (unlink(target) < 0) {
				Error1(target);
				return (1);
			}
		}
	}
	/*
	 * Save a flag to indicate target existed.
	 */
	if(!mve && !pflag)
		oflg = access(target, F_OK|EFF_ONLY_OK);
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
again:
	if ((to = creat(target, 0666)) < 0) 
	{
		if(!oflg && fflag && ISREG(s1))
		{
			if(unlink(target) < 0)
			{
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				  gettxt(_SGI_DMMX_Cannotunlink,
					"Cannot unlink %s"),
				  target);
				return(1);
			}
			oflg = 1;
			goto again;
		}
		else
		{
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_CannotCreat, "Cannot create %s"), target);
			close(from);
			return (1);
		}
	}
	if(do_dio) {
		if((fcntl(to,F_SETFL,FDIRECT)) == EINVAL) {
			if(verbose) {
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				    gettxt(_SGI_DMMX_nodiosupp,
				           _SGI_SMMX_nodiosupp), target);
			}
			do_dio=0;
			close(from);
			from = open(source,0);
		}
		else {
			if( (fcntl(to,F_DIOINFO, &to_dio)) == -1 ) {
				if(verbose) {
					_sgi_nl_error(SGINL_SYSERR, cmd_label,
					gettxt(_SGI_DMMX_dioerror,
					       _SGI_SMMX_dioerror), target);
				}
				do_dio=0;
				close(from);
				from = open(source,0);
			}
		}
	}

	/*
	 * Block copy from source to target.
	 */


	/*
	 * First work out the extent map - this will return everything
	 * in one lump if there are no holes, or we are not running XFS.
	 */
	nextents = read_fd_bmap(from, to, &s1, &s2);
#ifdef NOTYET
	if (get_fs_type(to) == FS_XFS) {
		do_prealloc = try_prealloc;
	} else {
		do_prealloc = 0;
	}
#endif

	/*
	 * If we created a target, or are mv'ing,
	 * set target permissions to the source
	 * before any copying so that any partially copied
	 * file will have the source's permissions (at most)
	 * or umask permissions whichever is the most restrictive.
	 */

	if (oflg || mve || pflag) {
		newmode = FMODE(s1);
		if (cpy && oflg && !pflag) {
			mode_t mask = umask(0);
			umask(mask);
			newmode &= ~mask;
		}
		if(chmod(target, newmode)<0)
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_chmod,
			    "%s: Cannot change target mode to (%o)"),
			    target, newmode);
	}

	if( !do_dio) {
		if (argv_blksz_dio) {
			blksz_dio = argv_blksz_dio;
		} else {
			blksz_dio=BUFFER_SIZE;
		}
		dio_max = blksz_dio;
		dio_min = 4096;
	}
	else { /* Hola ... */

		dio_mem=from_dio.d_mem > to_dio.d_mem ? from_dio.d_mem : to_dio.d_mem ;
		dio_min=from_dio.d_miniosz > to_dio.d_miniosz ? from_dio.d_miniosz : to_dio.d_miniosz ;
		dio_max=from_dio.d_maxiosz < to_dio.d_maxiosz ? from_dio.d_maxiosz : to_dio.d_maxiosz ;
		if(verbose>1) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_diosizes,
					_SGI_SMMX_diosizes), dio_min,dio_max);
		}

		if(argv_blksz_dio == 0)	blksz_dio=dio_max;
		else {
			blksz_dio = argv_blksz_dio;

			if( blksz_dio%dio_min != 0 ||
			    blksz_dio         <  dio_min ||
			    blksz_dio         >  dio_max ) {
				if(verbose) {
					_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
						gettxt(_SGI_DMMX_diousemax,
						       _SGI_SMMX_diousemax));
				}
				blksz_dio=dio_max;
			}
		}

		if (do_rtime) {
			fsxattr.fsx_xflags = XFS_XFLAG_REALTIME;
			if (rt_size != -1) 
				fsxattr.fsx_extsize = rt_size;
			else
				fsxattr.fsx_extsize = rtextsize;

			if (fcntl(to, F_FSSETXATTR, &fsxattr) < 0) {
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_rtiofail,
				           _SGI_SMMX_rtiofail), target);
			}
		}
	}

	/* Make sure the buffer size we use is sensible */
	if ((s1.st_size > BUFFER_SIZE) && !argv_blksz_dio) {
		while ((blksz_dio > s1.st_size) && (blksz_dio > dio_min)) {
			blksz_dio >>= 1;
		}
	}

	if (s1.st_size < blksz_dio) {
		while ((blksz_dio > s1.st_size) && (blksz_dio > dio_min)) {
			blksz_dio >>= 1;
		}
		if (blksz_dio < s1.st_size) {
			blksz_dio <<= 1;
		}
	}

	if (do_dio) {
		fbuf = (char *)memalign(dio_mem,blksz_dio);
	} else {
		fbuf= malloc(blksz_dio);
	}

	if(verbose>1) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_diousing, _SGI_SMMX_diousing),
		    blksz_dio);
	}

	for (extent = 0; extent < nextents; extent++) {
		pos = outmap[extent].bmv_offset;
		if (cpy_holes && (outmap[extent].bmv_block == -1)) {
			space.l_whence = 0;
			space.l_start = pos;
			space.l_len = outmap[extent].bmv_length;
			if (fcntl(to, F_UNRESVSP64, &space) < 0) {
				Error1("Truncating extent");
			}
			lseek64(to, outmap[extent].bmv_length, SEEK_CUR);
			lseek64(from, outmap[extent].bmv_length, SEEK_CUR);
			continue;
		}

#ifdef NOTYET
		if (do_prealloc) {
			space.l_whence = 1;
			space.l_start = 0;
			space.l_len = outmap[extent].bmv_length;
			if (fcntl(to, F_RESVSP64, &space) < 0) {
				Error1("reserving file space");
			}
		}
#endif

		for (cnt = outmap[extent].bmv_length; cnt > 0;
		     cnt -= ct, pos += ct) {
			if (!do_dio) {
				ct = min(blksz_dio, cnt);
			} else {
				ct = blksz_dio;
			}

#ifdef NOTYET
			if (mmap_in) {
				mmap_pos = pos & ~(pagesize-1);
				mmap_size = (ct + (pos - mmap_pos) +
						pagesize - 1) & ~(pagesize-1);
				mmap_buf = mmap64(NULL, mmap_size, PROT_READ,
						  MAP_SHARED, from, mmap_pos);

				if (mmap_buf == MAP_FAILED) {
					perror("on mmap call");
					exit(1);
				}
				fbuf = (void *)((char *)(mmap_buf) + pos -
								mmap_pos);
			} else {
				ct = read(from, fbuf, ct);
			}
#else
			ct = read(from, fbuf, ct);
#endif
			if (ct == 0) {
				/* EOF, stop trying to read */
				extent = nextents;
				break;
			}


			/* Ensure we do direct I/O to correct block
			 * boundaries.
			 */
			if (do_dio && (ct != blksz_dio)) {
				wc = ct + dio_min - (ct % dio_min);
				if (do_pad) {
					bzero(((char *)fbuf) + wc - ct,
					      wc - ct);
				}
			} else {
				wc = ct;
			}
			wc_b4 = wc;
			if (ct < 0 || ((wc = write(to, fbuf, wc)) != wc_b4)) {
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

					if ((wc = write(to, ((char *)fbuf)+wc,
							resid)) == resid) {
						/* worked on second attempt? */
						continue;
					}
					else
					if (wc < 0) {
						BadWrite(target);
					} else {
					_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					    gettxt(_SGI_DMMX_bad_copy,
						"bad copy to %s"),
					    target);
					}
				}
				/*
				 * If target is a regular file,
				 * unlink the bad file.
				 */
				if (ISREG(s2))
					unlink(target);
#ifdef NOTYET
				if (!mmap_in) {
					free(fbuf);
				} else {
					munmap(mmap_buf, mmap_size);
				}
#else
				free(fbuf);
#endif
				return (1);
			}
#ifdef NOTYET
			if (mmap_in) {
				munmap(mmap_buf, mmap_size);
			}
#endif
		}
	}
	if(do_dio && !do_pad)	ftruncate64(to,s1.st_size);
#ifdef NOTYET
	if (!mmap_in) {
		free(fbuf);
	}
#else
	free(fbuf);
#endif

	if (cpy_attribs) {
		clone_attribs(from, to);
	}

	(void)close(from);
	if (close(to) < 0) {
		BadWrite(target);
		if (ISREG(s2))
			unlink(target);
		return (1);
	}
	/*
	 * If it was a move, leave times alone.
	 */
	if (mve || pflag) {
		if(utime(target, &times) < 0)
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_utime,
			    "%s: Cannot update target times"),target);
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
		if (stat64(source, &s1) == 0)
			(void) unlink(target);
		return(1);
	}

	/*
	 * If it was a move, sets its owner to the source
	 */
	if (mve || pflag) {
		if(chown(target, FUSER(s1), FGROUP(s1)) < 0) {
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_chown,
			    "%s: Cannot change target ownership to uid(%d) gid(%d)"),
			    target, FUSER(s1), FGROUP(s1));
			newmode &= ~(S_ISUID|S_ISGID);
		}
	}
	if (oflg || mve || pflag)
		chmod(target, newmode);	/* Do this now: chown strips S_ISUID/S_ISGID */
	return(0);
}


accs_parent(name, amode)
register char *name;
register int amode;
{
	register char c, *p, *q;
	char buf[PATH_MAX+1];

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
rcopy(from, to, timep)
char *from, *to;
struct utimbuf *timep;
{
	register DIR *fold;
	register dirent64_t *dp;
	int errs = 0;
	struct stat64 t_sbuf, f_sbuf;
	int made_dir = 0;
	char fromname[MAXPATHLEN+1];

	fold = opendir(from);
	if (fold == 0) {
		CannotOpen(from);
		return (1);
	}

	if (stat64(to, &t_sbuf) < 0) {
		mode_t mask = umask(0);
		umask(mask);
		if(pflag)
			mask=0;
		made_dir = 1;
		FMODE(f_sbuf) = 0;
		if (stat64(from, &f_sbuf) < 0)
			Error1(from);
		if (mkdir(to, FMODE(f_sbuf)&(~mask)|S_IRWXU) < 0) {
			Error1(to);
			closedir(fold);
			return(1);
		}
	} else if (!ISDIR(t_sbuf)) {
		usage(0);
	}

	for (;;) {
		dp = readdir64(fold);
		if (dp == NULL) {
			closedir(fold);
			if (made_dir) {
				mode_t mask = umask(0);
				umask(mask);
				if(pflag)
					mask=0;
				if (chmod(to, FMODE(f_sbuf)&(~mask)) < 0) {
					_sgi_nl_error(SGINL_SYSERR2, cmd_label,
					    gettxt(_SGI_DMMX_xpg_mv_chmod,
					    "%s: Cannot change target mode to (%o)"),
					    to, FMODE(f_sbuf)&(~mask));
					errs++;
				} else if (pflag) {
					if(utime(to, timep) < 0)
						_sgi_nl_error(SGINL_SYSERR2, cmd_label,
						    gettxt(_SGI_DMMX_xpg_mv_utime,
						    "%s: Cannot update target times"),to);
					if(chown(to, FUSER(f_sbuf), FGROUP(f_sbuf))<0)
						_sgi_nl_error(SGINL_SYSERR2, cmd_label,
						    gettxt(_SGI_DMMX_xpg_mv_chown,
						    "%s: Cannot change target ownership to uid(%d) gid(%d)"),
						    to, FUSER(f_sbuf), FGROUP(f_sbuf) );
				}
			}
			return (errs);
		}
		if (dp->d_ino == 0
		    || !strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

		if (strlen(from) + 1 + strlen(dp->d_name) >= sizeof(fromname)-1) {
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

int 
nftw_cp(const char *path, const struct stat64 *snftw, int nftw_flag, struct FTW *Snftw)
{
	char target_path[MAXPATHLEN+1];
	char dir_path[MAXPATHLEN+1];
	char nftw_path[MAXPATHLEN+1];
	char *b;
	int c;
	struct utimbuf times;

	/* Determine full destination path */
	strcpy(target_path,xdev_target);

	strcpy(nftw_path,path);
	b = &nftw_path[strlen(nftw_path)-1];
	for(c=0; c < (Snftw->level); c++) {
		while(*b != '/' && *b != 0)--b;
		if(*b != 0)--b;
	}
	if( (strlen(target_path)+strlen(b)+1) > MAXPATHLEN) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_Name2long, "%s/%s: Name too long"),
		    target_path,b);
		return(1);
	}
	strcat(target_path,++b);

        switch(nftw_flag)
        {
                case FTW_SL:    /* symbolic link */
                case FTW_SLN:   /* symbolic link that points to nonexistent file */
                case FTW_F:     /* file */

			/* Creat directories up to target if needed */

			strcpy(dir_path,target_path);
			b = &dir_path[strlen(dir_path)-1];
			while(*b != '/' && *b != 0) *b-- = 0;
			*b = 0;
			if(mkdirp(dir_path,0777) < 0) {
				Error1(dir_path);
				return(1);
			}
			return move(path,target_path);

                case FTW_DP:    /* directory pre visited */
                case FTW_D:     /* directory */
			strcpy(dir_path,target_path);
			if(mkdirp(dir_path,0777) < 0) {
				Error1(dir_path);
				return(1);
			}
			times.actime = snftw->st_atime;
			times.modtime = snftw->st_mtime;
			if(utime(dir_path, &times) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_utime,
				    "%s: Cannot update target times"),dir_path);
			if(chown(dir_path, snftw->st_uid, snftw->st_gid) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_chown,
				    "%s: Cannot change target ownership to uid(%d) gid(%d)"),
				    dir_path, snftw->st_uid, snftw->st_gid);
			if(chmod(dir_path,snftw->st_mode) < 0)
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_chmod,
				    "%s: Cannot change target mode to (%o)"),
				    dir_path, snftw->st_mode);
                        return(0);

                case FTW_DNR:   /* directory without read permission */
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_dirnoread,
			    "Cannot read directory: %s"),path);
                        return(1);
                case FTW_NS:    /* unknown type, stat failed */
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_cannotstat,
			    "Cannot stat: %s"),path);
                        return(1);
        }
	return(1);
}
int 
nftw_rm(const char *path, const struct stat64 *snftw, int nftw_flag, struct FTW *Snftw)
{
        switch(nftw_flag)
        {
                case FTW_SL:    /* symbolic link */
                case FTW_SLN:   /* symbolic link that points to nonexistent file */
                case FTW_F:     /* file */
			if(unlink(path) < 0) {
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				  gettxt(_SGI_DMMX_Cannotunlink,
					"Cannot unlink %s"), path);
			}
			return(0);

                case FTW_DP:    /* directory pre visited */
                case FTW_D:     /* directory */
			if(rmdir(path) < 0) {
				if(errno == EEXIST)
					errno = ENOTEMPTY;
				_sgi_nl_error(SGINL_SYSERR2, cmd_label,
				    gettxt(_SGI_DMMX_xpg_mv_rmdir,
				    "Cannot remove directory: %s"),path);
			}
			return(0);

                case FTW_DNR:   /* directory without read permission */
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_xpg_mv_dirnoread,
			    "Cannot read directory: %s"),path);
                        return(1);
                case FTW_NS:    /* unknown type, stat failed */
			_sgi_nl_error(SGINL_SYSERR2, cmd_label,
			    gettxt(_SGI_DMMX_cannotstat,
			    "Cannot stat: %s"),path);
                        return(1);
        }
	return(1);
}

void
xdev_quit(sig)
{
	if(xdev_state == 1)
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_xpg_mv_partdest,
		    "Warning - incomplete copy to target tree: %s"),xdev_target);
	else 
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_xpg_mv_partsrc,
		    "Warning - incomplete removal of source tree: %s"),xdev_source);
	exit(1);
}



int
get_fs_type(int fd)
{
	struct statfs	buf;

	if (fstatfs(fd, &buf, sizeof(buf), 0) < 0) {
		return(-1);
	}

	if (buf.f_fstyp == fstyp_xfs) return (FS_XFS);
	return(FS_HUH);
}


/*
 * Attribute cloning code - most of this is here because attr_copy does not
 * let us pick and choose which attributes we want to copy.
 */

attr_multiop_t	attr_ops[ATTR_MAX_MULTIOPS];

/*
 * Grab attributes specified in attr_ops from source file and write them
 * out on the destination file. 
 */

static int
attr_replicate(int src_fd, int dst_fd, int count)
{
	int	j, k;

	if (attr_multif(src_fd, attr_ops, count, 0) < 0) {
		return(-1);
	}
	for (k = 0; k < count; k++) {
		if (attr_ops[k].am_error) {
			printf("Error %d getting attribute\n",
				attr_ops[k].am_error);
			break;
		}
		attr_ops[k].am_opcode = ATTR_OP_SET;
	}
	if (attr_multif(dst_fd, attr_ops, k, 0) < 0) {
		perror("on attr_multif set");
	}
	for (j = 0; j < k; j++) {
		if (attr_ops[j].am_error) {
			printf("Error %d setting attribute\n",
				attr_ops[j].am_error);
			return(-1);
		}
	}

	return(0);
}

/*
 * Copy all the attributes specified from src to dst.
 */

static int
attr_clone_copy(int src_fd, int dst_fd, char *list_buf,
                      char *attr_buf, int buf_len, int flags)
{
        attrlist_t *alist;
        attrlist_ent_t *attr;
        attrlist_cursor_t cursor;
        int space, i, j, k;
	char	*ptr;

        bzero((char *)&cursor, sizeof(cursor));
        do {
                if (attr_listf(src_fd, list_buf, BUFSIZE, flags, &cursor) < 0) {
			perror("on attr_listf");
                        return(1);
		}

                alist = (attrlist_t *)list_buf;

		space = buf_len;
		ptr = attr_buf;
                for (j = 0, i = 0; i < alist->al_count; i++) {
                        attr = ATTR_ENTRY(list_buf, i);
			if (space < attr->a_valuelen) {
				attr_replicate(src_fd, dst_fd, j);
				j = 0;
				space = buf_len;
				ptr = attr_buf;
			}
			attr_ops[j].am_opcode = ATTR_OP_GET;
			attr_ops[j].am_attrname = attr->a_name;
			attr_ops[j].am_attrvalue = ptr;
			attr_ops[j].am_length = (int) attr->a_valuelen;
			attr_ops[j].am_flags = flags;
			attr_ops[j].am_error = 0;
			j++;
			ptr += attr->a_valuelen;
			space -= attr->a_valuelen;
                }

		if (j) {
			attr_replicate(src_fd, dst_fd, j);
		}
        } while (alist->al_more);
        return(0);
}


int	clone_attribs(
	int	in_fd,
	int	out_fd)
{
	char	list_buf[BUFSIZE];
	char	*attr_buf;

	if ((get_fs_type(in_fd) != FS_XFS) || (get_fs_type(out_fd) != FS_XFS)) {
		return(0);
	}

	attr_buf = malloc(ATTR_MAX_VALUELEN * 2);
	attr_clone_copy(in_fd, out_fd, list_buf,
                      attr_buf, ATTR_MAX_VALUELEN * 2, 0);
	free(attr_buf);
	return(0);
}

/************
int	calculate_rt_size(int in_fd, int out_fd)
{
	struct fsxattr	fsx_src, fsx_dst;
	int	rt_src = 0;
	int	rt_dst = 0;

	if (!fcntl(in_fd, F_FSGETXATTR, &fsx_src) &&
	    (fsx_src.fsx_xflags & XFS_XFLAG_REALTIME)) {
		rt_src = fsx_src.fsx_extsize;
		printf("input rt extsize = %d\n", fsx_src.fsx_extsize);
	}

	if (!fcntl(out_fd, F_FSGETXATTR, &fsx_dst) &&
	    (fsx_dst.fsx_xflags & XFS_XFLAG_REALTIME)) {
		rt_dst = fsx_dst.fsx_extsize;
		printf("output rt extsize = %d\n", fsx_dst.fsx_extsize);
	}

	return(0);
}
***********/

/*
 * Determine the real time extent size of the XFS file system
 */
int
xfsrtextsize(char *path)
{
        int             fd, rval, rtextsize;
        xfs_fsop_geom_t geo;

        fd = open( path, O_RDONLY );
        if ( fd < 0 ) {
               Error1(path);
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


/*
 * Read in block map of the input file, coalesce contiguous
 * extents into a single range, keep all holes. Convert from 512 byte
 * blocks to bytes.
 */

#define MAPSIZE	128
#define	OUTMAP_SIZE_INCREMENT	MAPSIZE

int	read_fd_bmap(int fd, int out, struct stat64 *sin, struct stat64 *sout)
{
	int		i, cnt;
	int		nextents;
	struct getbmap	map[MAPSIZE];
	struct fsxattr	fsx;

#define	BUMP_CNT	\
	if (++cnt >= outmap_size) { \
		outmap_size += OUTMAP_SIZE_INCREMENT; \
		outmap = realloc(outmap, outmap_size*sizeof(*outmap)); \
		if (outmap == NULL) { \
			Error1("realloc for XFS extent map"); \
			exit(1); \
		} \
	}

	/*	Initialize the outmap array.  It always grows - never shrinks.
	 *	Left-over memory allocation is saved for the next files.
	 */
	if (outmap_size == 0) {
		outmap_size = OUTMAP_SIZE_INCREMENT; /* Initial size */
		outmap = malloc(outmap_size*sizeof(*outmap));
		if (!outmap) {
			Error1("malloc for XFS extent map");
			exit(1);
		}
	}

	outmap[0].bmv_block = 0;
	outmap[0].bmv_offset = 0;
	outmap[0].bmv_length = sin->st_size;

	/*
	 * If a non regular file is involved then forget holes
	 */

	if (!ISREG(*sin) || !ISREG(*sout))
		return(1);

	/*
	 * If the file length takes less blocks than the file
	 * has allocated then we probably do not have holes,
	 * don't bother looking for them.
	 */

	if (BTOBB(sin->st_size) <= sin->st_blocks) {
		return(1);
	}

	if (!cpy_holes || (get_fs_type(fd) != FS_XFS) ||
				(get_fs_type(out) != FS_XFS)) {
		return(1);
	}


	outmap[0].bmv_length = 0;

	map[0].bmv_offset = 0;
	map[0].bmv_block = 0;
	map[0].bmv_entries = 0;
	map[0].bmv_count = MAPSIZE;
	map[0].bmv_length = -1;

	cnt = 0;

	do {
		if (fcntl(fd, F_GETBMAP, map) < 0) {
			Error1("reading XFS extents");
			exit(1);
		}

		/* Concatenate extents together and replicate holes into
		 * the output map.
		 */
		for (i = 0; i < map[0].bmv_entries; i++) {
			if (map[i + 1].bmv_block == -1) {
				BUMP_CNT;
				outmap[cnt] = map[i+1];
			} else if (outmap[cnt].bmv_block == -1) {
				BUMP_CNT;
				outmap[cnt] = map[i+1];
			} else {
				outmap[cnt].bmv_length += map[i + 1].bmv_length;
			}
		}
	} while (map[0].bmv_entries == (MAPSIZE-1));

	for (i = 0; i <= cnt; i++) {
		outmap[i].bmv_offset = BBTOB(outmap[i].bmv_offset);
		outmap[i].bmv_length = BBTOB(outmap[i].bmv_length);
	}

	outmap[cnt].bmv_length = sin->st_size - outmap[cnt].bmv_offset;

	return(cnt+1);
}
