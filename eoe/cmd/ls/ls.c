/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.66 $"

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
/*
 *	Copyright (c) 1986, 1987, 1988, Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
/*
 * 	list file or directory
 */
/***************************************************************************
* Command: ls
* Inheritable Privileges: P_DACREAD,P_MACREAD
*       Fixed Privileges: None
* Notes: lists contents of directories
*
***************************************************************************/

#include	<sys/param.h>
#include	<sys/acl.h>
#include	<sys/euc.h>
#include	<sys/mac_label.h>
#include	<sys/mkdev.h>
#include	<sys/stat.h>
#include	<sys/capability.h>
#include	<sys/mac.h>
#include	<ctype.h>
#include	<curses.h>
#include	<deflt.h>
#include	<dirent.h>
#include	<errno.h>
#include	<getwidth.h>
#include	<grp.h>
#include	<invent.h>
#include	<libgen.h>
#include	<limits.h>
#include	<locale.h>
#include	<mls.h>
#include	<paths.h>
#include 	<proj.h>
#include	<pfmt.h>
#include	<pwd.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<term.h>
#include	<termios.h>
#include	<time.h>
#include	<unistd.h>

#define	BFSIZE	16

/* Date and time formats */

#define FORMAT1	 " %b %e  %Y "
#define FORMAT1ID ":321"
#define FORMAT2  " %b %e %H:%M "        /*
					 * b --- abbreviated month name
					   e --- day number
					   Y --- year in the form ccyy
					   H --- hour (24 hour version)
					   M --- minute   */
#define FORMAT2ID ":322"

#define DEFFILE "mac"	/* /etc/default file containing the default LTDB lvl */
#define LTDB_DEFAULT 2  /* default LID for non-readable LTDB */

typedef struct	{
	char	*lnamep;	/* file name */
	char	ltype;  	/* filetype */
	char	aclindicator;	/* indication of ACL existence on file */
	char	lnum_len;	/* length of lnum */
	char	lsize_len;	/* length of lsize */
	char	lblocks_len;	/* length of lblocks */
	char	lnl_len;	/* length of lnl */
	char	lmaj_len;	/* length of major dev number */
	char	lmin_len;	/* length of minor dev number */
	ino64_t	lnum;		/* inode number of file */
	off64_t	lsize;  	/* filesize or major/minor dev numbers */
	blkcnt64_t lblocks;	/* number of file blocks */
	mode_t	lflags; 	/* 0777 bits used as r,w,x permissions */
	nlink_t	lnl;    	/* number of links to file */
	uid_t	luid;		/* userid */
	gid_t	lgid;		/* groupid */
	long	lprojid;	/* project id */
	timespec_t lmtime;	/* timestamp */
	char	*flinkto;	/* symbolic link contents */
	char	*label;		/* file label string */
	char	*cap;		/* capability set */
	char	*acl;		/* access control list */
	char	*dacl;		/* directory default access control list */
} lbuf_t;

typedef struct dchain {
	char		*dc_name;	/* path name */
	struct dchain	*dc_next;	/* next directory in the chain */
} dchain_t;

static dchain_t	*dfirst;	/* start of the dir chain */
static dchain_t	*dtemp;		/* temporary - used for linking */
static char	*curdir;	/* the current directory */

static int	nfiles = 0;	/* number of flist entries in current use */
static int	nargs = 0;	/* number of flist entries used for arguments */
static int	maxfils = 0;	/* number of flist/lbuf entries allocated */
static int	maxn = 0;	/* number of flist entries with lbufs assigned */
#define	QUANTN	128		/* allocation growth quantum */
static int 	firstIsDirFlag = 0;

static lbuf_t	*nxtlbf;	/* pointer to next lbuf to be assigned */
static lbuf_t	**flist;	/* pointer to list of lbuf pointers */

static int	Aflg;		/* 4.3BSD ls option: like -a but don't list . and .. */
static int	Dflg;		/* Access Control List(s) */
static int	Hflg;		/* -H option prevents link chasing with -F */
static int	Mflg;		/* Mandatory Access Control label */
static int	Pflg;		/* Capability (Privilege) set */
static int	Sflg;		/* show string-based special files as strings */
static int	acl_enabled;	/* Access Control Lists supported */
static int	aflg, bflg, cflg, dflg, fflg, gflg, iflg, jflg, lflg, mflg;
static int	nflg, oflg, pflg, qflg, sflg, tflg, uflg, xflg, zflg, hflg;
static int	Cflg, Fflg, Rflg, Lflg, Zflg;
static int	rflg = 1;	/* initialized to 1 for special use in compare() */
static mode_t	flags;
static int	err = 0;	/* Contains return code */
static int	do_lnum_len, do_lsize_len, do_lblocks_len, do_lnl_len;
static int	lnum_len, lsize_len, lblocks_len, lnl_len, lmaj_len, lmin_len;

#define	DFL_NUM_LEN	5
#define	DFL_SIZE_LEN	6
#define	DFL_BLOCKS_LEN	4
#define	DFL_NL_LEN	4
#define	DFL_MAJOR_LEN	3
#define	DFL_MINOR_LEN	3

/*
 * print level information
 *	-1: don't print level
 *	LVL_ALIAS: print alias name
 *	LVL_FULL: print fully qualified level name
 */
static int	lvlformat;
#define	SHOWLVL()	(lvlformat != -1)

static uid_t	lastuid	= (uid_t)-1;
static gid_t	lastgid = (gid_t)-1;
static int	lastprojid = -1;

static int	statreq;    /* is > 0 if any of sflg, (n)lflg, tflg are on */

#define LS_UID		0
#define LS_GID		1
#define LS_PRID		2

char	option_str[] = "-RadC1xmnloghrtucpFbqisfLAHMDPSj";

static char	dotp[] = ".";
/* assumed 15 = max. length of user/group name */
static char	tbufu[BFSIZE], tbufg[BFSIZE], tbufp[BFSIZE];  

/* total number of blocks of files in a directory */
static blkcnt64_t	tblocks; 
static timespec_t	year;
static timespec_t	now;

static int	colwidth;
static int	num_cols = 80;
static int	filewidth;
static int	fixedwidth;
static int	lvlwidth;
static int	normwidth;
static int	extrawidth;
static int	nomocore;
static int	need_new_col;		/* need a new column no matter what */
static int	curcol;

static struct winsize	win;

static char	time_buf[50];	/* array to hold day and time */

static eucwidth_t	wp;
static int		difset1;
static int		difset2;
static int		difset3;

#define	STRCHUNK	(32 * 1024)
static char	**strspace;
static int	strspacelen;
static int	strcurblock = -1;
static int	strcuroffset = STRCHUNK;

static const char nomem[] = ":312:Out of memory: %s\n";
static const char usage[] = ":805:Usage: %s %s [files]\n";

extern int _pw_stayopen;
extern int _getpwent_no_shadow;

static int argcompare(const void *pp1, const void *pp2);
static void column(void);
static int compare(const void *pp1, const void *pp2);
static int getname(id_t uid, char *buf, int type);
static lbuf_t *gstat(char *file, int argfl);
static void ls_select(int *pairp);
static char *makename(char *dir, char *file);
static void morestrspace(void);
static void new_line(void);
static void pdirectory(char *name, int title, int lp);
static void pem(lbuf_t **slp, lbuf_t **lp, int tot_flag);
static void pentry(lbuf_t *ap);
static void pmode(mode_t aflag);
static void pprintf(char *s1, char *s2, char *s3);
static void rddir(char *dir);
static void resetstr(void);
static char *savestr(char *s);
static int setcount(char *s);
static int timespeccmp(timespec_t *t1, timespec_t *t2);
static int numlen(long long l, int minlen);
static int devlen(dev_t d, char *majorlen, char *minorlen);


/*
 * Procedure:     main
 *
 * Restrictions:
 *		fopen:	P_MACREAD
 *		lvlout:	P_MACREAD
 */
int
main(int argc, char **argv)
{
	int		amino;
	char		*bname = basename(argv[0]);
	int		c;
	char		*clptr;
	lbuf_t		*ep;
	int		i;
	char		*labelstate;
	lbuf_t		lb;
	int		opterr = 0;
	struct timeval	t;
	int		width;
	cap_value_t caps[2] = {CAP_DAC_READ_SEARCH, CAP_MAC_READ};

	/*
	 * Request both DAC and MAC read override capabilities.
	 * Don't sweat it if they're not available.
	 */
	(void)cap_acquire(2, caps);

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	getwidth(&wp);
	difset1 = wp._eucw1 - wp._scrw1;
	difset2 = wp._eucw2 - wp._scrw2;
	difset3 = wp._eucw3 - wp._scrw3;

	gettimeofday(&t, NULL);
	lb.lmtime.tv_sec = t.tv_sec;
	lb.lmtime.tv_nsec = t.tv_usec * 1000L;
	year = lb.lmtime;
	year.tv_sec -= 6L*30L*24L*60L*60L;	/* 6 months ago */
	now = lb.lmtime;
	now.tv_sec += 60L;
	if (!strcmp(bname, "lc")) {
		(void)setlabel("UX:lc");
		Cflg = 1;
		mflg = 0;
	} else
		(void)setlabel("UX:ls");

	if (isatty(1)) {
		Cflg = 1;
		mflg = 0;
	}

	while ((c=getopt(argc, argv, option_str)) != EOF) {
		switch(c) {
		case 'R':
			Rflg++;
			statreq++;
			break;
		case 'a':
			aflg++;
			break;
		case 'A':
			Aflg++;
			break;
		case 'd':
			dflg++;
			break;
		case 'C':
			Cflg = 1;
			mflg = 0;
			lflg = 0;
			break;
		case '1':
			Cflg = 0;
			break;
		case 'x':
			xflg = 1;
			Cflg = 1;
			mflg = 0;
			lflg = 0;
			break;
		case 'm':
			Cflg = 0;
			lflg = 0;
			mflg = 1;
			break;
		case 'n':
			nflg++;
			/* FALLTHROUGH */
		case 'l':
			Cflg = 0;
			lflg++;
			statreq++;
			break;
		case 'o':
			oflg++;
			lflg++;
			statreq++;
			break;
		case 'g':
			gflg++;
			lflg++;
			statreq++;
			break;
		case 'h':
			hflg++;
			break;
		case 'r':
			rflg = -1;
			break;
		case 't':
			tflg++;
			statreq++;
			break;
		case 'u':
			uflg++;
			break;
		case 'c':
			cflg++;
			break;
		case 'p':
			pflg++;
			statreq++;
			break;
		case 'F':
			Fflg++;
			statreq++;
			break;
		case 'b':
			bflg = 1;
			qflg = 0;
			break;
		case 'q':
			qflg = 1;
			bflg = 0;
			break;
		case 'i':
			iflg++;
			break;
		case 'j':
			jflg++;
			lflg++;
			statreq++;
			break;
		case 's':
			sflg++;
			statreq++;
			break;
		case 'f':
			fflg++;
			break;
		case 'L':
			Lflg++;
			Hflg=0;
			break;
		case 'H':
			Hflg++;
			Lflg=0;
			break;
		case 'M':
			Mflg++;
			break;
		case 'D':
			Dflg++;
			break;
		case 'P':
			Pflg++;
			break;
		case 'S':
			Sflg++;
			Lflg++;
			break;
		case 'z':
			zflg++;
			statreq++;
			break;
		case 'Z':
			Zflg++;
			statreq++;
			break;
		case '?':
			opterr++;
			break;
		}
	}
	if (opterr) {
		pfmt(stderr, MM_ACTION, usage, bname, option_str);
		exit(2);
	}

	/*
	 * The z and Z options are mutually exclusive.
	 */
	if (zflg && Zflg) {
		pfmt(stderr, MM_ERROR,
			":806:-%c and -%c are mutually exclusive\n", 'z', 'Z');
		pfmt(stderr, MM_ACTION, usage, bname, option_str);
		exit(2);
	}

	acl_enabled = sysconf(_SC_ACL) > 0;
	lvlformat = -1;

	if (fflg) {
		aflg++;
		lflg = 0;
		sflg = 0;
		tflg = 0;
		if (!(zflg || Zflg))
			statreq = 0;
	}

	fixedwidth = 2;
	if (pflg || Fflg)
		fixedwidth++;
	if (iflg)
		fixedwidth += DFL_NUM_LEN + 1;
	if (sflg)
		fixedwidth += DFL_BLOCKS_LEN + 1;

	if (lflg) {
		if (!gflg && !oflg)
			gflg = oflg = 1;
		else if (gflg && oflg)
			gflg = oflg = 0;
		Cflg = mflg = 0;
		_pw_stayopen = 1;
		_getpwent_no_shadow = 1;
	}

	if (Cflg || mflg) {
		if ((clptr = getenv("COLUMNS")) != NULL)
			num_cols = atoi(clptr);
		else if (ioctl(1, TIOCGWINSZ, &win) != -1)
			num_cols = (win.ws_col == 0 ? 80 : win.ws_col);	
		if (num_cols < 20 || num_cols > 512)
			/* assume it is an error */
			num_cols = 80;
	}

	do_lnum_len = iflg && !mflg;
	do_lblocks_len = sflg && !mflg;
	do_lsize_len = do_lnl_len = lflg;

	lnum_len = DFL_NUM_LEN;
	lsize_len = DFL_SIZE_LEN;
	lblocks_len = DFL_BLOCKS_LEN;
	lnl_len = DFL_NL_LEN;
	lmaj_len = DFL_MAJOR_LEN;
	lmin_len = DFL_MINOR_LEN;

	/* If env variable is on then show security labels */
	
	labelstate = getenv("LABELFLAG");
	if (labelstate && strcasecmp(labelstate, "on") == 0)
		Mflg++;

	lvlwidth = 0;		/* initialize width of level */

	if ((amino = (argc - optind)) == 0) {
		/*
		 * case when no names are given in ls-command and current 
		 * directory is to be used 
		 */
		argv[optind] = dotp;
	}

	for (i = 0; i < (amino ? amino : 1); i++) {
		if (SHOWLVL() || Cflg || mflg) {
			width = strlen(argv[optind]);
			width -= setcount(argv[optind]);
			if (width > filewidth)
				filewidth = width;
		}
		if ((ep = gstat((*argv[optind] ? argv[optind] : dotp), 1)) ==
		    NULL) {
			if (nomocore)
				exit(2);
			err = 2;
			optind++;
			continue;
		}
		ep->lnamep = *argv[optind] ? argv[optind] : dotp;
		optind++;
		nargs++;	/* count good arguments stored in flist */

		/*
		 * if lvlout fails, lwidth is -1, less than lvlwidth 
		 *
		 * if ltdb_lvl is > 0 then the LTDB is at a level which
                 *    is unreadable by this process at its current level,
                 *    change level to the level of the LTDB and issue lvlout
                 */
		if (Mflg && (Cflg || mflg) && ep->label) {
			width += strlen(ep->label) + 3;
			if (width > filewidth)
				filewidth = width;
		}
		if (Pflg && (Cflg || mflg) && ep->cap) {
			width += strlen(ep->cap) + 3;
			if (width > filewidth)
				filewidth = width;
		}
		if (Dflg && (Cflg || mflg)) {
			if (ep->acl)
				width += strlen(ep->acl) + 3;
			if (ep->dacl)
				width += strlen(ep->dacl) + 3;
			if (ep->acl && ep->dacl)
				width -= 2;
			if (width > filewidth)
				filewidth = width;
		}
		if (do_lnum_len && ep->lnum_len > lnum_len)
			lnum_len = ep->lnum_len;
		if (do_lsize_len) {
			if (ep->lsize_len > lsize_len)
				lsize_len = ep->lsize_len;
			if (ep->lmaj_len > lmaj_len)
				lmaj_len = ep->lmaj_len;
			if (ep->lmin_len > lmin_len)
				lmin_len = ep->lmin_len;
		}
		if (do_lblocks_len && ep->lblocks_len > lblocks_len)
			lblocks_len = ep->lblocks_len;
		if (do_lnl_len && ep->lnl_len > lnl_len)
			lnl_len = ep->lnl_len;
	}
	extrawidth = (lnum_len - DFL_NUM_LEN) + (lsize_len - DFL_SIZE_LEN) +
		     (lblocks_len - DFL_BLOCKS_LEN) + (lnl_len - DFL_NL_LEN);
	colwidth = fixedwidth + filewidth + extrawidth;

	if (SHOWLVL()) {
		/*
	 	 * An additional two spaces are taken into account to give at
	 	 * least two spaces between the level and the next file name.
	 	 * Originally, two spaces were kept between file names.
	 	 */
		normwidth = colwidth;
		colwidth = normwidth + lvlwidth + 2;
	}
	qsort(flist, nargs, sizeof(lbuf_t *), argcompare);
	for (i = 0; i < nargs; i++) {
		if ((flist[i]->ltype == 'd' && dflg == 0) || fflg)
			break;
	}
	pem(&flist[0], &flist[i], 0);
	for (; i < nargs; i++) {
		if (i == 0 && amino > 1)
			firstIsDirFlag = 1;
		pdirectory(flist[i]->lnamep, amino > 1, nargs);
		if (nomocore)
			exit(2);
		/* -R: print subdirectories found */
		while (dfirst) {
			/* take off first dir on dfirst & print it */
			dtemp = dfirst;
			dfirst = dfirst->dc_next;
			pdirectory(dtemp->dc_name, 1, nargs);
			if (nomocore)
				exit(2);
			free(dtemp->dc_name);
			free(dtemp);
		}
	}
	exit(err);
	/*NOTREACHED*/
}


/*
 * Procedure:     pdirectory
 *
 * Restrictions:
 *		gettxt:	none
 *		pfmt:	none
 *
 * Notes: print the directory name, labelling it if title is
 * nonzero, using lp as the place to start reading in the dir.
 */
static void
pdirectory(char *name, int title, int lp)
{
	lbuf_t		*ap;
	dchain_t	*dp;
	int		j;
	char		*pname;
	static char	*sep;
	static char	*total;

	filewidth = 0;
	curdir = name;
	if (title) {
		if (firstIsDirFlag)
			firstIsDirFlag = 0;
		else
			putchar('\n');
		if (!sep)
			sep = gettxt(":325", ":");
		pprintf(name, "", sep);
		new_line();
	}
	nfiles = lp;
	rddir(name);
	if (nomocore)
		return;
	if (fflg == 0)
		qsort(&flist[lp], nfiles - lp, sizeof(lbuf_t *), compare);
	if (Rflg) {
		for (j = nfiles - 1; j >= lp; j--) {
			ap = flist[j];
			if (ap->ltype == 'd' &&
			    strcmp(ap->lnamep, ".") != 0 &&
			    strcmp(ap->lnamep, "..") != 0) {
				dp = malloc(sizeof(*dp));
				if (dp == NULL)
					pfmt(stderr, MM_ERROR, nomem,
						strerror(errno));
				pname = makename(curdir, ap->lnamep);
				dp->dc_name = strdup(pname);
				if (dp->dc_name == NULL) {
					pfmt(stderr, MM_ERROR, nomem,
						strerror(errno));
					free(dp);
				} else {
					dp->dc_next = dfirst;
					dfirst = dp;
				}
			}
		}
	}
	if (lflg || sflg) {
		if (!total)
			total = gettxt(":326", "total %llu");
		curcol += printf(total, tblocks);
		need_new_col++;
	}
	pem(&flist[lp], &flist[nfiles], 0);
	resetstr();
}

/*
 * pem: print 'em.  Print a list of files (e.g. a directory) bounded
 * by slp and lp.
 */
static void
pem(lbuf_t **slp, lbuf_t **lp, int tot_flag)
{
	int	col;
	lbuf_t	**ep;
	int	ncols;
	int	nrows;
	int	row;

	if (Cflg || mflg)
		ncols = colwidth > num_cols ? 1 : num_cols / colwidth;
	if (ncols <= 1 || mflg || xflg || !Cflg) {
		for (ep = slp; ep < lp; ep++)
			pentry(*ep);
		new_line();
		return;
	}
	/* otherwise print -C columns */
	if (tot_flag)
		slp--;
	nrows = (lp - slp - 1) / ncols + 1;
	for (row = 0; row < nrows; row++) {
		for (col = row == 0 && tot_flag; col < ncols; col++) {
			ep = slp + (nrows * col) + row;
			if (ep < lp)
				pentry(*ep);
		}
		new_line();
	}
}

/* 
   Return a number formated in human format.  Stolen from top. 
*/
static char	*number(off64_t bytes)
{
#define	NSTR	4
  static	char	bufs[NSTR][16];
  static	int	index = 0;
  char		*p;
  char		tag = 'b';
  double	amt;
  off64_t	kilo = 1024;
  off64_t	mega = (1024*kilo);
  off64_t	giga = (1024*mega);
  off64_t	tera = (1024*giga);
  
  p =  bufs[index];
  index = (index + 1) % NSTR;
  
  if (bytes < kilo) {
    sprintf(p, "%lld%c", bytes, tag);
    return (p);
  } 

  amt = (double)bytes;

  if (bytes >= tera) {
    amt /= tera;
    tag = 'T';
  } 
  else if (bytes >= giga) {
    amt /= giga;
    tag = 'G';
  } 
  else if (bytes >= mega) {
    amt /= mega;
    tag = 'M';
  }
  else {
    amt /= kilo;
    tag = 'K';
  }
  if (amt >= 10) {
    sprintf(p, "%4.0f%c", amt, tag);
  } 
  else {
    sprintf(p, "%4.1f%c", amt, tag);
  }
  return (p);
}





/*
 * Procedure:     pentry
 *
 * Restrictions:
 *		cftime:	none
 *		lvlout:	P_MACREAD
 * Notes:
 * print one output entry;
 * if uid/gid is not found in the appropriate
 * file (passwd/group), then print uid/gid instead of 
 * user/group name;
 */
static void
pentry(lbuf_t *ap)  
{
	char		buf[BUFSIZ];
	char		*dmark = "";	/* Used if -p or -F option active */
	static char	*id1;
	static char	*id2;
	char		*midstr;
	lbuf_t		*p;
	int		show_linkto;	/* boolean: show link string */

	p = ap;
	column();
	if (iflg) {
		if (mflg)
			curcol += printf("%llu ", p->lnum);
		else
			curcol += printf("%*llu ", lnum_len, p->lnum);
	}
	if (sflg) {
		if (mflg)
			curcol += printf("%lld ",
				(p->ltype != 'b' && p->ltype != 'c') ?
					p->lblocks : 0LL);
		else
			curcol += printf("%*lld ", lblocks_len,
				(p->ltype != 'b' && p->ltype != 'c') ?
					p->lblocks : 0LL);
	}
	if (lflg) {
		putchar(p->ltype);
		curcol++;
		pmode(p->lflags);
		curcol += printf("%c", p->aclindicator);
		curcol += printf("%*d ", lnl_len, p->lnl);
		if (oflg) {
			if (!nflg && getname(p->luid, tbufu, LS_UID) == 0)
				curcol += printf("%-8.8s ", tbufu);
			else
				curcol += printf("%-8u ", p->luid);
		}
		if (gflg) {
			if (!nflg && getname(p->lgid, tbufg, LS_GID) == 0)
				curcol += printf("%-9.9s", tbufg);
			else
				curcol += printf("%-9u", p->lgid);
		}
		if (jflg) {
			if (!nflg && getname(p->lprojid, tbufp, LS_PRID) == 0)
				curcol += printf("%-9.9s", tbufp);
			else
				curcol += printf("%-9d", p->lprojid);
		}
		if ((p->ltype == 'b' || p->ltype == 'c') &&
		    (!Sflg || (p->flinkto == NULL)))
			curcol += printf(" %*d,%*d",
				lmaj_len, major((dev_t)p->lsize),
				lmin_len, minor((dev_t)p->lsize));
		else{
		  if (hflg || getenv("HUMAN_BLOCKS") 
		      && !getenv("POSIXLY_CORRECT")) {
		      curcol += printf(" %7s", number(p->lsize));
		  } else {
		    curcol += printf(" %*lld", lsize_len, p->lsize);
		  }		  
		}
		if (timespeccmp(&p->lmtime, &year) < 0 ||
		    timespeccmp(&p->lmtime, &now) > 0) {
			if (!id1)
				id1 = gettxt(FORMAT1ID, FORMAT1); 
			cftime(time_buf, id1, &p->lmtime.tv_sec);
			curcol += printf("%s", time_buf);
		} else {
			if (!id2)
				id2 = gettxt(FORMAT2ID, FORMAT2); 
			cftime(time_buf, id2, &p->lmtime.tv_sec);
			curcol += printf("%s", time_buf);
		}
	}

	/*
	 * prevent both "->" and trailing marks
	 * from appearing
	 */
	if (pflg && p->ltype == 'd')
		dmark = "/";

	show_linkto = (Sflg || lflg) && p->flinkto;

	if (Fflg && !show_linkto) {
		if (p->ltype == 'd')
			dmark = "/";
		else if (p->ltype == 'l')
			dmark = "@";
		else if (p->ltype == 'p')
			dmark = "|";
		else if (p->ltype == 'S')
			dmark = "=";
		else if (p->lflags & (S_IXUSR|S_IXGRP|S_IXOTH))
			dmark = "*";
		else
			dmark = "";
	}

	midstr = "";
	if (show_linkto) {
		midstr = " -> ";
		strcpy(buf, p->flinkto);
		free(p->flinkto);
		p->flinkto = NULL;
		dmark = buf;
	}

	if (qflg || bflg)
		pprintf(p->lnamep, midstr, dmark);
	else
		curcol += printf("%s%s%s", p->lnamep, midstr, dmark) -
			  setcount(p->lnamep);

	if (Mflg) {
		if (ap->label) {
			curcol += printf(" [%s]", ap->label);
			free (ap->label);
		} else
			curcol += printf(" []");
	}
	if (Pflg) {
		if (ap->cap) {
			curcol += printf(" [%s]", ap->cap);
			free (ap->cap);
		} else
			curcol += printf(" []");
	}
	if (Dflg) {
		curcol += printf(" [");
		if (ap->acl) {
			curcol += printf("%s", ap->acl);
			free (ap->acl);
			ap->acl = NULL;
		}
		if (ap->dacl) {
			curcol += printf("/%s", ap->dacl);
			free (ap->dacl);
			ap->dacl = NULL;
		}
		curcol += printf("]");
	}
}

/*
 * print various r,w,x permissions 
 */
static void
pmode(mode_t aflag)
{
        /* these arrays are declared static to allow initializations */
	static int	m0[] = { 1, S_IRUSR, 'r', '-' };
	static int	m1[] = { 1, S_IWUSR, 'w', '-' };
	static int	m2[] = { 3, S_ISUID|S_IXUSR, 's', S_IXUSR,
				 'x', S_ISUID, 'S', '-' };
	static int	m3[] = { 1, S_IRGRP, 'r', '-' };
	static int	m4[] = { 1, S_IWGRP, 'w', '-' };
	static int	m5[] = { 3, S_ISGID|S_IXGRP, 's', S_IXGRP,
				 'x', S_ISGID, 'L', '-'};
	static int	m6[] = { 1, S_IROTH, 'r', '-' };
	static int	m7[] = { 1, S_IWOTH, 'w', '-' };
	static int	m8[] = { 3, S_ISVTX|S_IXOTH, 't', S_IXOTH,
				 'x', S_ISVTX, 'T', '-'};
        static int	*m[] = { m0, m1, m2, m3, m4, m5, m6, m7, m8 };
	int		**mp;

	flags = aflag;
	for (mp = &m[0]; mp < &m[sizeof(m)/sizeof(m[0])]; mp++)
		ls_select(*mp);
}

static void
ls_select(int *pairp)
{
	int	n;

	n = *pairp++;
	while (n-- > 0) {
		if ((flags & *pairp) == *pairp) {
			pairp++;
			break;
		} else
			pairp += 2;
	}
	putchar(*pairp);
	curcol++;
}

/*
 * column: get to the beginning of the next column.
 */
static void
column(void)
{
	if (curcol == 0)
		return;
	if (need_new_col) {
		putchar('\n');
		curcol = 0;
		need_new_col = 0;
		return;
	}
	if (mflg) {
		putchar(',');
		curcol++;
		if (curcol + colwidth + 2 > num_cols) {
			putchar('\n');
			curcol = 0;
			return;
		}
		putchar(' ');
		curcol++;
		return;
	}
	if (Cflg == 0) {
		putchar('\n');
		curcol = 0;
		return;
	}
	if ((curcol / colwidth + 2) * colwidth > num_cols) {
		putchar('\n');
		curcol = 0;
		return;
	}
	do {
		putchar(' ');
		curcol++;
	} while (curcol % colwidth);
}

static void
new_line(void)
{
	if (curcol) {
		putchar('\n');
		curcol = 0;
	}
}

/*
 * Procedure:     rddir
 *
 * Restrictions:
 *		opendir:	none
 *		lvlout:		P_MACREAD
 * Notes:
 * read each filename in directory dir and store its
 * status in flist[nfiles] 
 * use makename() to form pathname dir/filename;
 *
 * If the LTDB is at a level which is unreadable by the current process
 *  (lttb_lvl > 0) ... attempt to set the P_SETPLEVEL privilege and
 *  change the process level to the level of the LTDB.
 */
static void
rddir(char *dir)
{
	dirent64_t	*dentry;
	DIR		*dirf;
	lbuf_t		*ep;
	int		width;

	if ((dirf = opendir(dir)) == NULL) {
		fflush(stdout);
		pfmt(stderr, MM_ERROR, ":327:Cannot access directory %s: %s\n",
			dir, strerror(errno));
		err = 2;
		return;
	}
	tblocks = 0;
	lnum_len = DFL_NUM_LEN;
	lsize_len = DFL_SIZE_LEN;
	lblocks_len = DFL_BLOCKS_LEN;
	lnl_len = DFL_NL_LEN;
	lmaj_len = DFL_MAJOR_LEN;
	lmin_len = DFL_MINOR_LEN;
	while (dentry = readdir64(dirf)) {
		/*
		 * check for directory items '.', '..', 
		 * and items without valid inode-number;
		 */
		if (aflg == 0 && dentry->d_name[0] == '.' &&
		    (Aflg == 0 ||
		     (dentry->d_name[1] == '\0' ||
		      (dentry->d_name[1] == '.' && dentry->d_name[2] == '\0'))))
			continue;
		if (SHOWLVL() || Cflg || mflg) {
			width = strlen(dentry->d_name);
			width -= setcount(dentry->d_name);
			if (width > filewidth)
				filewidth = width;
		}
		ep = gstat(makename(dir, dentry->d_name), 0);
		if (ep == NULL) {
			if (nomocore)
				return;
			continue;
		}
		ep->lnum = dentry->d_ino;
		if (do_lnum_len)
			ep->lnum_len = numlen(ep->lnum, DFL_NUM_LEN);
		ep->lnamep = savestr(dentry->d_name);
		if (ep->lnamep == NULL) {
			if (nomocore)
				return;
			continue;
		}
		if (Mflg && (Cflg || mflg) && ep->label) {
			width += strlen(ep->label) + 3;
			if (width > filewidth)
				filewidth = width;
		}
		if (do_lnum_len && ep->lnum_len > lnum_len)
			lnum_len = ep->lnum_len;
		if (do_lsize_len) {
			if (ep->lsize_len > lsize_len)
				lsize_len = ep->lsize_len;
			if (ep->lmaj_len > lmaj_len)
				lmaj_len = ep->lmaj_len;
			if (ep->lmin_len > lmin_len)
				lmin_len = ep->lmin_len;
		}
		if (do_lblocks_len && ep->lblocks_len > lblocks_len)
			lblocks_len = ep->lblocks_len;
		if (do_lnl_len && ep->lnl_len > lnl_len)
			lnl_len = ep->lnl_len;
	}
	(void)closedir(dirf);
	extrawidth = (lnum_len - DFL_NUM_LEN) + (lsize_len - DFL_SIZE_LEN) +
		     (lblocks_len - DFL_BLOCKS_LEN) + (lnl_len - DFL_NL_LEN);
	colwidth = fixedwidth + filewidth + extrawidth;
	if (SHOWLVL()) {
		/*
		 * An additional two spaces are taken into account
		 * to give at least two spaces between the level
		 * and the next file name.
		 * Originally, two spaces were kept between file names.
		 */
		normwidth = colwidth;
		colwidth = normwidth + lvlwidth + 2;
	}
}

void
get_mac(char *file, lbuf_t *rep)
{
	mac_t label;

	if (!Mflg)
		return;
	
	label = mac_get_file (file);
	if (label == (mac_t) NULL)
		rep->label = (char *) NULL;
	else
		rep->label = mac_to_text (label, (size_t *) NULL);
	mac_free (label);
}

void
get_cap(char *file, lbuf_t *rep)
{
	cap_t capset;

	if (!Pflg)
		return;

	capset = cap_get_file (file);
	if (capset == (cap_t) NULL)
		rep->cap = (char *) NULL;
	else
		rep->cap = cap_to_text (capset, (size_t *) NULL);
	cap_free (capset);
}

void
get_acl(char *file, lbuf_t *rep)
{
	acl_t acl;
	acl_t dacl;
	char *buffer;

	rep->aclindicator = ' ';
	rep->acl = NULL;
	rep->dacl = NULL;

	if (!acl_enabled)
		return;

	if (!lflg && !Dflg)
		return;

	acl = acl_get_file (file, ACL_TYPE_ACCESS);
	dacl = acl_get_file (file, ACL_TYPE_DEFAULT);

	if (acl_valid (acl) == 0) {
		rep->aclindicator = '+';
		if (Dflg) { 
			buffer = acl_to_short_text (acl, (ssize_t *) NULL);
			if (buffer) {
				rep->acl = strdup(buffer);
				acl_free(buffer);
			}
		}
	}

	if (acl_valid (dacl) == 0) {
		rep->aclindicator = '+';
		if (Dflg) {
			buffer = acl_to_short_text (dacl, (ssize_t *) NULL);
			if (buffer) {
				rep->dacl = strdup(buffer);
				acl_free(buffer);
			}
		}
	}
}

/*
 * Procedure:     gstat
 *
 * Restrictions:
 *		readlink(2):	none
 *		stat(2):	none
 *		acl(2):		none
 * Notes:
 * get status of file and recomputes tblocks;
 * argfl = 1 if file is a name in ls-command and  = 0
 * for filename in a directory whose name is an
 * argument in the command;
 * stores a pointer in flist[nfiles] and
 * returns that pointer;
 * returns NULL if failed;
 */
static lbuf_t *
gstat(char *file, int argfl)
{
	char		buf[BUFSIZ];
	int		bufsize = BUFSIZ;
	int		cc;
	char		*devname;
	lbuf_t		*rep;
	struct stat64	statb;
	struct stat64	statb1;
	int		(*statf)(const char *, struct stat64 *) = lstat64;

	if (nomocore)
		return NULL;
	if (nfiles >= maxfils) { 
		/*
		 * all flist/lbuf pair assigned files
		 * time to get some more space
		 */
		maxfils += QUANTN;
		if ((flist = realloc(flist,
				     maxfils * sizeof(lbuf_t *))) == NULL ||
		    (nxtlbf = malloc(QUANTN * sizeof(lbuf_t))) == NULL) {
			pfmt(stderr, MM_ERROR, nomem, strerror(errno));
			nomocore = 1;
			return NULL;
		}
	}

	/*
	 * nfiles is reset to nargs for each directory
	 * that is given as an argument
	 * maxn is checked to prevent the assignment of an lbuf
	 * to a flist entry that already has one assigned.
	 */
	if (nfiles >= maxn) {
		rep = nxtlbf++;
		flist[nfiles++] = rep;
		maxn = nfiles;
	} else
		rep = flist[nfiles++];
	rep->lflags = (mode_t)0;
	rep->flinkto = NULL;

	get_mac(file, rep);
	get_cap(file, rep);
	get_acl(file, rep);

	if (argfl || statreq) {
		if ((*statf)(file, &statb) < 0) {
			pfmt(stderr, MM_ERROR, ":5:Cannot access %s: %s\n",
				file, strerror(errno));
			nfiles--;
			return NULL;
		}
		/*
		 * Need to make sure the stat() is necessary
		 * because the symlink may point to an automounted
		 * filesystem.
		 */
		if (!Hflg && (statb.st_mode & S_IFMT) == S_IFLNK &&
		    (Lflg || !lflg) && stat64(file, &statb1) == 0 &&
		    (Lflg || (!lflg && (statb1.st_mode & S_IFMT) == S_IFDIR)))
			statb = statb1;
		rep->lnum = statb.st_ino;
		if (do_lnum_len)
			rep->lnum_len = numlen(rep->lnum, DFL_NUM_LEN);
		rep->lsize = statb.st_size;
		if (do_lsize_len) {
			rep->lsize_len = numlen(rep->lsize, DFL_SIZE_LEN);
			rep->lmaj_len = rep->lmin_len = 0;
		}
		rep->lblocks = statb.st_blocks;
		if (do_lblocks_len)
			rep->lblocks_len = numlen(rep->lblocks, DFL_BLOCKS_LEN);
		switch (statb.st_mode & S_IFMT) {
		case S_IFDIR:
			rep->ltype = 'd';
			if (Sflg) {
				devname = filename_to_devname(file, buf, &bufsize);
				if (devname)
					rep->flinkto = strdup(devname);
			}
			break;

		case S_IFCHR:
		case S_IFBLK:
			if ((statb.st_mode & S_IFMT) == S_IFCHR)
				rep->ltype = 'c';
			else
				rep->ltype = 'b';

			if (Sflg) {
				devname = filename_to_devname(file, buf, &bufsize);
				if (devname)
					rep->flinkto = strdup(devname);
			}
			if (!Sflg || !rep->flinkto) {
				rep->lsize = (off_t)statb.st_rdev;
				if (do_lsize_len)
					rep->lsize_len =
						devlen((dev_t)rep->lsize,
							&rep->lmaj_len,
							&rep->lmin_len);
			}
			break;
		case S_IFIFO:
			rep->ltype = 'p';
			break;
		case S_IFSOCK:
			rep->ltype = 'S';
			break;
		case S_IFNAM:
                        /* 'st_rdev' field is really the subtype */
                        switch (statb.st_rdev) {
                        case S_INSEM:
                               rep->ltype = 's';
                               break;
                        case S_INSHD:
                               rep->ltype = 'm';
                               break;
			default:
			       rep->ltype = '-';
			       break;
			}
			break;
		case S_IFLNK:
			rep->ltype = 'l';
			if (lflg) {
				cc = readlink(file, buf, BUFSIZ);
				if (cc < 0)
					break;
				/*
				 * follow the symbolic link
				 * to generate the appropriate
				 * Fflg marker for the object
				 * eg, /bin -> /sym/bin/
				 */
				if (!Hflg && (Fflg || pflg) &&
				    stat64(file, &statb1) >= 0) {
					switch (statb1.st_mode & S_IFMT) {
					case S_IFDIR:
						buf[cc++] = '/';
						break;
					default:
						if ((statb1.st_mode & ~S_IFMT) &
						    (S_IXUSR|S_IXGRP|S_IXOTH))
							buf[cc++] = '*';
						break;
					}
				}
				buf[cc] = '\0';
				rep->flinkto = strdup(buf);
				break;
			}
			/*
			 * ls /sym behaves differently from ls /sym/
			 * when /sym is a symbolic link. This is fixed
			 * when explicit arguments are specified.
			 */
			if (!argfl || stat64(file, &statb1) < 0)
				break;
			if ((statb1.st_mode & S_IFMT) == S_IFDIR) {
				statb = statb1;
				rep->ltype = 'd';
				rep->lsize = statb1.st_size;
				if (do_lsize_len)
					rep->lsize_len = numlen(rep->lsize,
								DFL_SIZE_LEN);
			}
			break;
		default:
			rep->ltype = '-';
			break;
		}
		rep->lflags = statb.st_mode & ~S_IFMT;

		/* mask ISARG and other file-type bits */
	
		rep->luid = statb.st_uid;
		rep->lgid = statb.st_gid;
		rep->lprojid = statb.st_projid;
		rep->lnl = statb.st_nlink;
		if (do_lnl_len)
			rep->lnl_len = numlen(rep->lnl, DFL_NL_LEN);
		if (uflg)
			rep->lmtime = statb.st_atim;
		else if (cflg)
			rep->lmtime = statb.st_ctim;
		else
			rep->lmtime = statb.st_mtim;
		if (rep->ltype != 'b' && rep->ltype != 'c')
			tblocks += rep->lblocks;
	}
        return rep;
}

/* returns pathname of the form dir/file;
 *  dir is a null-terminated string;
 */
static char *
makename(char *dir, char *file) 
{
					/* path + '/' + filename + '\0' */
	static char	dfile[PATH_MAX + MAXNAMLEN + 2];
	char		*dp;
	char		*fp;

	dp = dfile;
	fp = dir;
	while (*fp)
		*dp++ = *fp++;
	if (dp > dfile && dp[-1] != '/')
		*dp++ = '/';
	fp = file;
	while (*fp)
		*dp++ = *fp++;
	*dp = '\0';
	return dfile;
}

/*
 * get name from passwd/group file for a given uid/gid/projid
 * and store it in buf; lastuid is set to uid;
 * returns -1 if uid is not in file
 */
static int
getname(id_t id, char *buf, int type)
{
	struct group	*gp;
	struct passwd	*pp;

	switch (type) {
	      case LS_UID:
		if (id == lastuid)
			return (0);

		pp = getpwuid((uid_t)id);
		if (!pp) {
			lastuid = -1;
			return -1;
		}
		strcpy(buf, pp->pw_name);
		lastuid = id;
		break;

	      case LS_GID:
		if (id == lastgid)
			return (0);

		gp = getgrgid((gid_t)id);
		if (!gp) {
			lastgid = -1;			
			return -1;
		}
		strcpy(buf, gp->gr_name);
		lastgid = id;
		break;

	      case LS_PRID:
		if (id == lastprojid)
			return (0);
		if (projname((prid_t) id, buf, BFSIZE) == 0)
			return -1;
		if (buf[0] == '\0')
			return -1;
		lastprojid = (int)id;
		break;
		
	}
	return (0);
}

/* return >0 if item pointed by pp2 should appear first */
static int
argcompare(const void *pp1, const void *pp2)
{
	lbuf_t	*p1;
	lbuf_t	*p2;
	int	t;

	p1 = *(lbuf_t **)pp1;
	p2 = *(lbuf_t **)pp2;
	if (dflg == 0) {
		/*
		 * compare two names in ls-command one of which is file
		 * and the other is a directory;
		 * this portion is not used for comparing files within
		 * a directory name of ls-command.
		 */
		if (p1->ltype == 'd' && p2->ltype != 'd')
			return 1;
                if (p1->ltype != 'd' && p2->ltype == 'd')
			return -1;
	}
	if (tflg && (t = timespeccmp(&p2->lmtime, &p1->lmtime)))
		return rflg * t;
        return rflg * strcmp(p1->lnamep, p2->lnamep);
}

/* return >0 if item pointed by pp2 should appear first */
static int
compare(const void *pp1, const void *pp2)
{
	lbuf_t	*p1;
	lbuf_t	*p2;
	int	t;

	p1 = *(lbuf_t **)pp1;
	p2 = *(lbuf_t **)pp2;
	if (tflg && (t = timespeccmp(&p2->lmtime, &p1->lmtime)))
		return rflg * t;
        return rflg * strcmp(p1->lnamep, p2->lnamep);
}

/*
 * This is a little weird, but don't do the printable character processing
 * on the second string passed in.  This allows us to print " -> " without
 * converting the spaces to \040.
 */
static void
pprintf(char *s1, char *s2, char *s3)
{
	int	c;
	int	cc;
	int	i;
	char	*s;

	for (s = s1, i = 0; i < 3; i++) {
		while (c = (unsigned char)*s++) {
			if ((i != 1) &&
			    (!ISPRINT(c, wp) || (bflg && (c == ' ')))) { 
				if (qflg)
					c = '?';
				else if (bflg) {
					curcol += 3;
					putchar('\\');
					cc = '0' + ((c >> 6) & 07);
					putchar(cc);
					cc = '0' + ((c >> 3) & 07);
					putchar(cc);
					c = '0' + (c & 07);
				}
			}
			curcol++;
			putchar(c);
		}
		if (i == 0)
			s = s2;
		else
			s = s3;
	}
	curcol -= setcount(s1);
}

static int
setcount(char *s)
{
	int	cnt = 0;

	while (*s) {
		if (NOTASCII(*s)) {
			cnt += ISSET2(*s) ?
				difset2 + 1 :
				ISSET3(*s) ?
					difset3 + 1 :
					difset1;
			if (ISSET2(*s))
				s += wp._eucw2 + 1;
			else if (ISSET3(*s))
				s += wp._eucw3 + 1;
			else
				s += wp._eucw1;
		} else
			s++;
	}
	return cnt;
}

static void
morestrspace(void)
{
	char	*cp;

	strspacelen++;
	strspace = realloc(strspace, sizeof(*strspace) * strspacelen);
	cp = malloc(STRCHUNK);
	if (cp == NULL) {
		pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		nomocore = 1;
		err = 2;
		strspacelen--;
		return;
	}
	strspace[strspacelen - 1] = cp;
}

static char *
savestr(char *s)
{
	int len = strlen(s) + 1;
	char *p;

	if (STRCHUNK - strcuroffset < len) {
		if (strspacelen == strcurblock + 1) {
			morestrspace();
			if (nomocore)
				return NULL;
		}
		strcurblock++;
		strcuroffset = 0;
	}
	p = strspace[strcurblock] + strcuroffset;
	strcpy(p, s);
	strcuroffset += len;
	return p;
}

static void
resetstr(void)
{
	if (strspace == NULL)
		return;

	strcurblock = strcuroffset = 0;
}

static int
timespeccmp(timespec_t *t1, timespec_t *t2)
{
	time_t	s;
	long	n;

	if (s = t1->tv_sec - t2->tv_sec)
		return s > 0 ? 1 : -1;
	if (n = t1->tv_nsec - t2->tv_nsec)
		return n > 0 ? 1 : -1;
	return 0;
}

static int
numlen(long long l, int minlen)
{
	int i;
	static long long *v = NULL;
	long long val;

	if (!v) {
		v = malloc(20 * sizeof(*v));
		if (v == NULL)
			pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		for (i = 0, val = 1; i < 19; i++) {
			v[i] = val - 1LL;
			val *= 10LL;
		}
		v[19] = LONGLONG_MAX;
	}
	if (l <= v[minlen])
		return minlen;
	for (i = minlen + 1; i < 20; i++) {
		if (l <= v[i])
			return i;
	}
	/* shouldn't get here */
	return minlen;
}

static int
devlen(dev_t d, char *majorlen, char *minorlen)
{
	*majorlen = numlen(major(d), DFL_MAJOR_LEN);
	*minorlen = numlen(minor(d), DFL_MINOR_LEN);
	return *majorlen + *minorlen + 1;
}
