/* :set ts=4 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.88 $"

/***************************************************************************
 * Command: ps
 *
 * Displays all sorts of useful information about a process. 
 *
 * SGI changes:
 *	1)	Removed all the security junk.
 *	2) 	Removed DIRSIZ and replaced it with MAXNAMLEN
 *	3)	Put in the notinteresting() routine to cut search time
 *	4)	Removed remote file system support (MAYBE)
 *	5)	Ignore syscon and systty entirely, they are always links
 *		to some other device
 *	6)	Redid all output with format strings
 *
 ***************************************************************************/

/* Overall scheme:
	main()		Process the args setting appropriate flags and saving
			away pids etc for later matching.
			Build the line out format fEntry[].
			Loop over entries in /proc using PIOCPSINFO.
			Select the interesting procs and pass them to prcom().
	FillFormat()	Builds a string of field names based on the flags.
	FormatSpecifier()
			Takes either the FillFormat() string or the -o string
			and creates the fEntry data.
	prcom()		Prints out info{} formatted according to fEntry[].
	info{}		Saves the current process entry's state.
	fEntry[]	Describes the line format as formatted fields.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ustat.h>
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <wchar.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/procfs.h>
#include <paths.h>
#include <locale.h>
#include <fmtmsg.h>
#include <stdarg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <grp.h>
#include <getopt.h>

#ifdef UNS
extern int _getpwent_no_shadow;
#endif

#define	TRUE	1
#define	FALSE	0

#define NTTYS	20	/* max ttys that can be specified with the -t option  */
#define SIZ 	30	/* max processes that can be specified with -p and -g */
#define ARGSIZ	256	/* size of buffer which holds args for -t, -p & -u */

#ifndef MAXLOGIN
#define MAXLOGIN	14	/* max number of char in userid */
#endif


/* format specifiers */
#define PS_RUSER	0	/* real user ID */
#define PS_USER		1	/* effective user ID */
#define PS_RGROUP	2	/* real group ID */
#define PS_GROUP	3	/* effective group ID */
#define PS_PID		4	/* pid */
#define PS_PPID		5	/* parent pid */
#define PS_PGID		6	/* process group id */
#define PS_PCPU		7	/* ratio of CPU time */
#define PS_VSZ		8	/* size of process */
#define PS_NICE		9	/* priority */
#define PS_ETIME	10	/* elapsed time */
#define PS_TIME		11	/* CPU time */
#define PS_TTY		12	/* controlling terminal */
#define PS_COMM		13	/* command */
#define PS_ARGS		14	/* command arguments */
#define PS_STIME	15	/* start time */
#define PS_FLAG		16	/* flags associated with the process */
#define PS_STATE	17	/* state of the process */
#define PS_WCHAN	18	/* wchan address process is waiting on */
#define PS_WNAME	19	/* wchan name process is waiting on */
#define PS_UTIL		20	/* processor utilisation */
#define PS_UID		21	/* user id */
#define PS_OPRI		22	/* old priority */
#define PS_PROC		23	/* processor */
#define PS_SZ		24	/* size in blocks */
#define PS_RSS		25	/* resident size */
#define PS_OTIME	26	/* old time display */
#define PS_PRI		27	/* priority */
#define PS_CLASS	28	/* class */
#define PS_SID		29	/* sid */
#define PS_BLANK	30	/* place a space */
#define PS_ADDR		31	/* address of the process */
#define PS_LABEL	32	/* label of the process */
#define PS_CELL		33	/* cell id of the process */
#define FORMAT_ENTRIES	34

struct format {
	short len;			/* format length */
	char spec;			/* format specifier */
	char d;				/* delimitor */
};

int fcount = 0;

#define FORMAT_MAX	50	/* maximum format entries */
struct format fEntry[FORMAT_MAX];


#define LEFT_ADJUST	0
#define RIGHT_ADJUST	1

struct formatname_s {
	char *name;			/* format name */
	int namelen;
	int adjust;			/* left or right justified */
	char spec;
	char *def;			/* default format name */
	int minlen;
} fname[FORMAT_ENTRIES] = {
	{ "ruser", 5, RIGHT_ADJUST, PS_RUSER, "RUSER", 8 },
	{ "user", 4, RIGHT_ADJUST, PS_USER, "USER", 8 },
	{ "rgroup", 6, RIGHT_ADJUST, PS_RGROUP, "RGROUP", 8 },
	{ "group", 5, RIGHT_ADJUST, PS_GROUP, "GROUP", 8 },
	{ "pid", 3, RIGHT_ADJUST, PS_PID, "PID", 10 },
	{ "ppid", 4, RIGHT_ADJUST, PS_PPID, "PPID", 10 },
	{ "pgid", 4, RIGHT_ADJUST, PS_PGID, "PGID", 10 },
	{ "pcpu", 4, RIGHT_ADJUST, PS_PCPU, "%CPU", 4 },
	{ "vsz", 3, LEFT_ADJUST, PS_VSZ, "VSZ", 6 },
	{ "nice", 4, RIGHT_ADJUST, PS_NICE, "NI", 2 },
	{ "etime", 5, RIGHT_ADJUST, PS_ETIME, "ELAPSED", 11 },
	{ "time", 4, RIGHT_ADJUST, PS_TIME, "TIME", 11 },
	{ "tty", 3, LEFT_ADJUST, PS_TTY, "TTY", 6 },
	{ "comm", 4, LEFT_ADJUST, PS_COMM, "COMMAND", 9 },
	{ "args", 4, LEFT_ADJUST, PS_ARGS, "COMMAND", 35 },
	{ "stime", 5, RIGHT_ADJUST, PS_STIME, "STIME", 8 },
	{ "flag", 4, RIGHT_ADJUST, PS_FLAG, "F", 3 },
	{ "state", 5, LEFT_ADJUST, PS_STATE, "S", 1 },
	{ "wchan", 5, RIGHT_ADJUST, PS_WCHAN, "WCHAN", 8 },
	{ "wname", 5, RIGHT_ADJUST, PS_WNAME, "WCHAN", 8 },
	{ "util", 4, RIGHT_ADJUST, PS_UTIL, "C", 2 },
	{ "cell", 4, RIGHT_ADJUST, PS_CELL, "CELL", 4 },
	{ "uid", 3, RIGHT_ADJUST, PS_UID, "UID", 5 },
	{ "opri", 4, RIGHT_ADJUST, PS_OPRI, "PRI", 3 },
	{ "cpu", 3, RIGHT_ADJUST, PS_PROC, "P", 2 },
	{ "sz", 2, RIGHT_ADJUST, PS_SZ, "SZ", 5 },
	{ "rss", 3, LEFT_ADJUST, PS_RSS, "RSS", 5 },
	{ "otime", 5, RIGHT_ADJUST, PS_OTIME, "TIME", 5 },
	{ "pri", 3, RIGHT_ADJUST, PS_PRI, "PRI", 3 },
	{ "class", 5, RIGHT_ADJUST, PS_CLASS, "CLS", 4 },
	{ "sid", 3, RIGHT_ADJUST, PS_SID, "SID", 10 },
	{ "blank", 5, RIGHT_ADJUST, PS_BLANK, "", 0 },
	{ "addr", 4, RIGHT_ADJUST, PS_ADDR, "ADDR", 8 },
	{ "label", 5, LEFT_ADJUST, PS_LABEL, "LABEL", 22},
};


/* Structure for storing user info */
struct udata {
	uid_t	uid;		/* numeric user id */
	char	name[MAXLOGIN];	/* login name, may not be null terminated */
};

/* udata and devl granularity for structure allocation */
#define UDQ	50

/* Pointer to user data */
struct udata *ud;
int	nud = 0;	/* number of valid ud structures */
int	maxud = 0;	/* number of ud's allocated */

struct udata uid_tbl[SIZ];	/* table to store selected uid's */
int	nut = 0;		/* counter for uid_tbl */
struct udata Uid_tbl[SIZ];	/* table to store selected uid's */
int	nUt = 0;		/* counter for uid_tbl */

char label[512];

struct prpsinfo info;	/* process information structure from /proc */
struct prcred cred;	/* process credentials structure from /proc */
int	trix_mac;	/* true if kernel has MAC configured */

int retcode = 1;	/* exit value - any success clears this flag */

int	lflg = 0;
int	eflg = 0;
int	uflg = 0;
int	Uflg = 0;
int	aflg = 0;
int	dflg = 0;
int	pflg = 0;
int	fflg = 0;
int	cflg = 0;
int	jflg = 0;
int	gflg = 0;
int	Gflg = 0;
int	sflg = 0;
int	tflg = 0;
int	Mflg = 0;
int	oflg = 0;
int	xflg = 0;
int	Xflg = 0;
int	Tflg = 0;
int	noflg = 0;
#ifdef _SHAREII
int	yflg;
#endif /* _SHAREII */
int	euid;

int xpg = 0;

int pgSize;
time_t tim;

int	errflg;
char	argbuf[ARGSIZ];
char	*parg;
char	*p1;			/* points to successive option arguments */
static char	stdbuf[BUFSIZ+8];
cell_t	cellid;

/*
 * /tmp/.ps_data/ps_data stores information for quick access by ps
 * and whodo.  To avoid synchronization problems when
 * reading and writing this file, a temporary file is
 * created at the level of /tmp/.ps_data, ownership and group set to known
 * ids, information stored in the temp file, and finally
 * the temp file is renamed to /tmp/.ps_data/ps_data. Note also that
 * update of ps_data depends on /etc/passwd, /unix and /dev.
 * The files are kept in /tmp/.ps_data instead of /tmp, because /tmp
 * has the sticky bit set.  This keeps the ps program from being able
 * to remove and rename files in /tmp without being setuid root.  So,
 * we created a subdirectory that is writable by root,sys for ps to
 * work in.
 *
 */

int	ndev;			/* number of devices */
int	maxdev;			/* number of devl structures allocated */


struct devl {				/* device list	 */
	char	dname[MAXNAMLEN];	/* device name	 */
	dev_t	dev;			/* device number */
} *devl;

char	*tty[NTTYS];	/* for t option */
int	ntty = 0;
pid_t	pid[SIZ];	/* for p option */
int	npid = 0;
pid_t	grpid[SIZ];	/* for g option */
int	ngrpid = 0;
pid_t	Grpid[SIZ];	/* for G option */
int	nGrpid = 0;
pid_t	sessid[SIZ];	/* for s option */
int	nsessid = 0;
pid_t	Cellids[SIZ];	/* for X option */
int	nCellids = 0;

static void usage(void);		/* print usage message and quit */
static void uconv(void);
void uconv_all(struct udata *uid_tbl, int *nut);
void getdev(void), getarg(void);
int gdev(const char *, const struct stat *, int,struct FTW *);
void FormatSpecifier(char *), FillFormat(void);
void getpasswd(void);
void wrdata(void);
void pswrite(int fd, char *bp, unsigned bs);
void prstime(timespec_t st, int len);
void prformattime(time_t tm, int len);

char	cmd_label[] = "UX:ps";

/*
 * error handling
 */
static void
err_nomem()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

static void
err_opt_c(s, c)
char *s;
int c;
{
	char *pstr= s;

	/* check the size of message text, message system functions
         * don't handle arbitrary sizes, use PATH_MAX for limit.
 	 */
	if (strlen(s) >= PATH_MAX) {
		*(pstr+PATH_MAX) = '\0';
	}

	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_ps_invpopt,
		"%s is an invalid non-numeric argument for -%c"),
	    s, c);
}

static void
err_tty()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_ps_notty, "can't find controlling terminal"));
}

/*
 * usage message
 */
static void
usage(void)
{
#ifdef _SHAREII
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_ps_usage3, "ps [ -edalfcjyxM ] [ -t termlist ] [ -u uidlist ] [ -o format ]"));
#else
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_ps_usage3, "ps [ -edalfcjxM ] [ -t termlist ] [ -u uidlist ] [ -o format ]"));
#endif /* _SHAREII */
	_sgi_nl_usage(SGINL_USAGESPC, cmd_label,
	    gettxt(_SGI_DMMX_ps_usage4, "   [ -U userlist ] [ -G grplist ] [ -p proclist ] [ -g grplist ]"));
	_sgi_nl_usage(SGINL_USAGESPC, cmd_label,
	    gettxt(_SGI_DMMX_ps_usage4, "   [ -s sidlist ] [ -X celllist ]"));
	exit(1);
}

/*
 * Procedure:     main
 *
 */
main(int argc, char **argv)
{
	register char	**ttyp = tty;
	char	*name;
	char	*p;
	int	c;
	uid_t	puid;		/* puid: process user id */
	uid_t	rpuid;		/* rpuid: process real user id */
	uid_t	pgid;		/* pgid: process real group id */
	pid_t	ppid;		/* ppid: parent process id */
	pid_t	ppgrp;		/* ppgrp: process group leader id */
	pid_t	psid;		/* psid: session id */
	int	i, found;
	struct ustat ustatb;
	struct stat statb;
	dev_t procdev;
	int	pgerrflg = 0;	/* err flg: non-numeric arg w/p & g options */
	unsigned	size;
	DIR *dirp;
	struct dirent *dentp;
	char	pname[100];
	int	pdlen;
	char	*cp;
	cap_t	ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;
#ifdef UNS
	_getpwent_no_shadow = 1;
#endif
	euid = geteuid();

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	/* set page size variable */
	pgSize = getpagesize();

	/* get the current time */
	tim = time((time_t *) 0);

	/* check to see if we need to be XPG compliant */
	if (p = getenv("_XPG")) {
		xpg = atoi(p);
	}

	label[0] = '\0';
	trix_mac = (sysconf(_SC_MAC) == 1);
	(void) setvbuf(stdout, stdbuf, _IOFBF, sizeof(stdbuf));
#ifdef _SHAREII	
	while ((c = getopt(argc, argv, "jlfcweadxAMyTt:p:g:u:n:s:o:G:U:X:")) != EOF)
#else  /* _SHAREII */		
	while ((c = getopt(argc, argv, "jlfcweadxAMTt:p:g:u:n:s:o:G:U:X:")) != EOF)
#endif /* _SHAREII */
		switch (c) {
		case 'l':		/* long listing */
			lflg++;
			break;
		case 'f':		/* full listing */
			fflg++;
			break;
		case 'j':
			jflg++;
			break;
		case 'T':
			Tflg++;
			break;
		case 'x': 		/* display the cell id */
		        /* verify that the kernel supports the syssgi option */
	  	        if (syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, 0,
				   &cellid) == -1) {
  		                fprintf(stderr, 
		       "Operating system version does not support -x option\n");
				exit(1);
			}
		        xflg++;
			break;
		case 'X':		/* cell ids */
		        /* verify that the kernel supports the syssgi option */
	  	        if (syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, 0,
				   &cellid) == -1) {
  		                fprintf(stderr, 
		       "Operating system version does not support -X option\n");
				exit(1);
			}
		        Xflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				if (nCellids >= SIZ)
					break;
				getarg();
				if (!num(parg)) {
					pgerrflg++;
					err_opt_c(parg, 'X');
				}
				Cellids[nCellids++] = (pid_t) atol(parg);
			} while (*p1);
			break;
		case 'c':
			/*
			 * Format output to reflect scheduler changes:
			 * high numbers for high priorities and don't
			 * print nice or p_cpu values.  'c' option only
			 * effective when used with 'l' or 'f' options.
			 */
			cflg++;
			break;
		case 'e':		/* list for every process */
		case 'A':
			eflg++;
			tflg = uflg = Uflg = pflg = Gflg = gflg = sflg = 0;
			Xflg = 0;
			break;
		case 'a':
			/*
			 * Same as 'e' except no process group leaders
			 * and no non-terminal processes.
			 */
			aflg++;
			break;
		case 'd':	/* same as e except no proc grp leaders */
			dflg++;
			break;
		case 'n':	/* no longer needed; retain as no-op */
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_ps_noopt, "-n option ignored"));
			break;
		case 'M':		/* print MAC labels */
			if (trix_mac)
				Mflg++; 
			break;
#ifdef _SHAREII
		case 'y':	/* print processes attached to lnode "uid" */
			yflg++;
			break;
#endif /* _SHAREII */			
		case 't':		/* terminals */
#define TSZ 30
			tflg++;
			p1 = optarg;
			do {
				parg = argbuf;
				if (ntty >= NTTYS)
					break;
				getarg();
				if ((p = (char *)malloc(TSZ)) == NULL) {
					err_nomem();
					exit(1);
				}
				size = TSZ;
				if (isdigit(*parg)) {
					(void) strcpy(p, "tty");
					size -= 3;
				} else
					*p = '\0';
				(void) strncat(p, parg, (int)size);
				*ttyp++ = p;
				ntty++;
			} while (*p1);
			break;
		case 'p':		/* proc ids */
			pflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				if (npid >= SIZ)
					break;
				getarg();
				if (!num(parg)) {
					pgerrflg++;
					err_opt_c(parg, 'p');
				}
				pid[npid++] = (pid_t)atol(parg);
			} while (*p1);
			break;
		case 's':		/* session */
			sflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				if (nsessid >= SIZ)
					break;
				getarg();
				if (!num(parg)) {
					pgerrflg++;
					err_opt_c(parg, 's');
				}
				sessid[nsessid++] = (pid_t)atol(parg);
			} while (*p1);
			break;
		case 'g':		/* proc group */
			gflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				if (ngrpid >= SIZ)
					break;
				getarg();
				if (!num(parg)) {
					pgerrflg++;
					err_opt_c(parg, 'g');
				}
				grpid[ngrpid++] = (pid_t)atol(parg);
			} while (*p1);
			break;
		case 'G':		/* proc real group */
			Gflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				if (nGrpid >= SIZ)
					break;
				getarg();
				if (!num(parg)) {
					pgerrflg++;
					err_opt_c(parg, 'G');
				}
				Grpid[nGrpid++] = (pid_t)atol(parg);
			} while (*p1);
			break;
		case 'u':		/* user name or number */
			uflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				getarg();
				if (nut < SIZ)
					(void) strncpy(uid_tbl[nut++].name,
					  parg, MAXLOGIN);
			} while (*p1);
			break;
		case 'U':		/* real user name or number */
			Uflg++;
			p1 = optarg;
			parg = argbuf;
			do {
				getarg();
				if (nUt < SIZ)
					(void) strncpy(Uid_tbl[nUt++].name,
					  parg, MAXLOGIN);
			} while (*p1);
			break;
		case 'o':		/* format instruction */
			oflg++;
			FormatSpecifier(optarg);
			break;
		default:			/* error on ? */
			errflg++;
			break;
		}
	if (trix_mac) {
		char *labelstate;

		/* If env variable is on then show security labels */
		labelstate = getenv("LABELFLAG");
		if (labelstate && strcasecmp(labelstate,"on") == 0)
			Mflg++;
	}
	if (errflg || (optind < argc) || pgerrflg)
		usage();

	if (tflg)
		*ttyp = 0;

	if (stat(_PATH_PROCFSPI, &statb) < 0) {
		perror(_PATH_PROCFSPI);
		exit(1);
	}
	procdev = statb.st_dev;
	if (stat("/", &statb) < 0) {
		perror("/");
		exit(1);
	}
	if ((procdev == statb.st_dev) || (ustat(procdev, &ustatb) < 0)) {
		fprintf(stderr, "%s is not mounted\n", _PATH_PROCFSPI);
		exit(1);
	}


	/*
	 * If an appropriate option has not been specified, use the
	 * current terminal as the default.
	 */
	if (!(aflg || eflg || dflg || uflg || tflg || pflg || gflg || sflg ||
	  Gflg || Uflg || Xflg)) {
		name = NULL;
		for (i = 2; i >= 0; i--)
			if (isatty(i)) {
				name = ttyname(i);
				break;
			}
		if (name == NULL) {
			err_tty();
			exit(1);
		}
		*ttyp++ = name + 5;
		*ttyp = 0;
		ntty++;
		tflg++;
		noflg++;
	}
	if (eflg) {
		tflg = uflg = Uflg = pflg = sflg = gflg = Gflg = 0;
		aflg = dflg = Xflg = 0;
	}
	if (aflg || dflg)
		tflg = 0;

	if (!readata()) {	/* get data from psfile */
		getdev();
#ifndef UNS
		getpasswd();
#endif
		wrdata();
	}

	uconv();

	if (!oflg) 
		FillFormat();

	/*
	 *  Get rid of trailing blanks at the end of the label line.
	 */
	cp = (label + (strlen(label) - 1)); /* point to last char b4 '\0' */
	while (*cp && (label < cp) && (*cp == ' '))
		cp--;

	*(cp + 1) = '\0';

	/* display header */
	{
		/* if the header is blank, dont print anything */
		int i, len = strlen(label);
		for (i = 0; i < len; i++)
			if (label[i] != ' ')
				break;
		if (i < len)
			printf("%s\n", label);
	}

	/*
	 * Determine which processes to print info about by searching
	 * the /proc/pinfo directory and looking at each process.
	 */
	if ((dirp = opendir(_PATH_PROCFSPI)) == NULL) {
		(void) fprintf(stderr,"Cannot open /proc/pinfo directory :%s\n",
			strerror(errno));
		exit(1);
	}

	(void) sprintf(pname, "%s%c", _PATH_PROCFSPI, '/');
	pdlen = strlen(pname);

	/* for each active process --- */
	ocap = cap_acquire(1, &cap_mac_read);
	while (dentp = readdir(dirp)) {
		int	procfd;		/* fd for /proc/pinfo/nnnnn */
		char   *plblstring;	/* process label string */
		int	mt;

		if (dentp->d_name[0] == '.')	/* skip . and .. */
			continue;

		plblstring = NULL;
		(void) strcpy(pname + pdlen, dentp->d_name);
		if (Mflg) {
			mac_t pl;	/* pname's MAC label */

			/*
			 * If you don't have privilege to get
			 * pname's label, then you don't have
			 * privilege to see the process.
			 */
			pl = mac_get_file (pname);
			if (pl == NULL)
				continue;
			plblstring = mac_to_text(pl, (size_t *) NULL);
			mac_free(pl);
			if (plblstring == NULL)
				continue;
		}
retry:
		if ((procfd = open(pname, O_RDONLY)) == -1) {
			mac_free(plblstring);
			continue;
		}

		/*
		 * Get the info structure for the process and close quickly.
		 */
		if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
			int	saverr = errno;

			(void) close(procfd);
			if (saverr == EACCES) {
				mac_free(plblstring);
				continue;
			}
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					gettxt(_SGI_DMMX_ps_nopsinfo,
					"PIOCPSINFO on %s: %s"),
					pname, strerror(saverr));
			mac_free(plblstring);
			continue;
		}

		/*
		 * read in the process creds 
		 */
		if (ioctl(procfd, PIOCCRED, (char *) &cred) == -1) {
			int	saverr = errno;

			(void) close(procfd);
			if (saverr == EACCES) {
				mac_free(plblstring);
				continue;
			}
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					gettxt(_SGI_DMMX_ps_nopsinfo,
					"PIOCCRED on %s: %s"),
					pname, strerror(saverr));
			mac_free(plblstring);
			continue;
		}

		if (!(mt = (info.pr_thds > 1 && Tflg)))
			(void) close(procfd);

		if (info.pr_state == 0) {		/* can't happen? */
			goto next_proc;
		}
		found = 0;
#ifdef _SHAREII
		if (yflg)
			puid = info.pr_shareuid;
		else
			puid = cred.pr_euid;
#else  /* _SHAREII */
		puid = cred.pr_euid;
#endif /* _SHAREII */		
		rpuid = info.pr_uid;
		ppid = info.pr_pid;
		pgid = info.pr_gid;
		ppgrp = info.pr_pgrp;
		psid = info.pr_sid;

		/*
		 * Omit process group leaders for 'a' and 'd' options.
		 */
		if ((ppid == psid) && (dflg || aflg)) {
			goto next_proc;
		}
		if (eflg || dflg)
			found++;
		else if (pflg && search(pid, npid, ppid))
			found++;
		else if (uflg && ufind(puid,1))
			found++;	/* puid in u option arg list */
		else if (Uflg && ufind(rpuid,0))
			found++;	/* puid in U option arg list */
		else if (gflg && search(grpid, ngrpid, ppgrp))
			found++;	/* grpid in g option arg list */
		else if (Gflg && search(Grpid, nGrpid, pgid))
			found++;	/* grpid in G option arg list */
		else if (sflg && search(sessid, nsessid, psid))
			found++;	/* sessid in s option arg list */
		else if (Xflg) {
		        (void) syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, 
				      info.pr_pid, &cellid); 
                        if (search(Cellids, nCellids, (pid_t) cellid))
			        found++;
		}
		        
		if (!found && !tflg && !aflg ) {
			goto next_proc;
		}

		if (xflg && !Xflg) {
		        /* make the syssgi call to set the cell id */
		        (void) syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, 
				      info.pr_pid, &cellid);
		}

		if (!mt) {
			if (prcom(puid, found, plblstring)) {
				printf("\n");
				retcode = 0;
			}
		} else {
			prthreadctl_t	ptc;

			ptc.pt_tid = 0;
			ptc.pt_flags = PTFD_GEQ | PTFS_ALL;
			ptc.pt_cmd = PIOCPSINFO;
			ptc.pt_data = (caddr_t)&info;

			if (ioctl(procfd, PIOCTHREAD, &ptc) < 0
			    || !prcom(puid, found, plblstring))
				goto next_proc;
			retcode = 0;
			ptc.pt_flags = PTFD_GTR | PTFS_ALL;
			for (;;) {
				printf("\n");
				if (ioctl(procfd, PIOCTHREAD, &ptc) < 0
				    || !prcom(puid, found, plblstring))
					break;
			}
		}
next_proc:
		if (mt) (void)close(procfd);
		mac_free(plblstring);
	}
	cap_surrender(ocap);

	(void) closedir(dirp);
	(void) fflush(stdout);
	exit(xpg ? 0 : retcode);
	/* NOTREACHED */
}

/*
 * Procedure:     readata
 *
 * Notes:
 * 		readata reads in the open devices (terminals) and stores 
 * 		info in the devl structure.
 */

static char	psdir[] = "/tmp/.ps_data";
static char	psfile[] = "/tmp/.ps_data/.ps_data";
static char	pstmpfile[] = "/tmp/.ps_data/.ps_XXXXXX";

int readata()
{
	struct stat sbuf1, sbuf2;
	int fd;

	fd = open(psfile, O_RDONLY);

	if(fd == -1)		/* Restore privs before returning error.*/
		return(0);

	if (fstat(fd, &sbuf1) < 0
		  || sbuf1.st_size == 0
	  || stat(_PATH_DEV, &sbuf2) == -1
		  || sbuf1.st_mtime <= sbuf2.st_mtime
		  || sbuf1.st_mtime <= sbuf2.st_ctime
	  || stat("/unix", &sbuf2) == -1
		  || sbuf1.st_mtime <= sbuf2.st_mtime
		  || sbuf1.st_mtime <= sbuf2.st_ctime
	  || stat(_PATH_PASSWD, &sbuf2) == -1
		  || sbuf1.st_mtime <= sbuf2.st_mtime
		  || sbuf1.st_mtime <= sbuf2.st_ctime) {
		(void) close(fd);
		return 0;
	}

	/* Read /dev data from psfile. */
	if (psread(fd, (char *) &ndev, sizeof(ndev)) == 0)  {
		(void) close(fd);
		return 0;
	}

	if ((devl = (struct devl *)malloc(ndev * sizeof(*devl))) == NULL) {
		err_nomem();
		exit(1);
	}
	if (psread(fd, (char *)devl, ndev * sizeof(*devl)) == 0)  {
		(void) close(fd);
		return 0;
	}

#ifndef UNS
	/* Read /etc/passwd data from psfile. */
	if (psread(fd, (char *) &nud, sizeof(nud)) == 0)  {
		(void) close(fd);
		return 0;
	}
	if ((ud = (struct udata *)malloc(nud * sizeof(*ud))) == NULL) {
		err_nomem();
		exit(1);
	}
	if (psread(fd, (char *)ud, nud * sizeof(*ud)) == 0)  {
		(void) close(fd);
		return 0;
	}
#endif

	(void) close(fd);
	return 1;
}

/*
 * Procedure:     getdev
 *
 * Notes:  getdev() uses ftw() to pass pathnames under /dev to gdev()
 * along with a status buffer.
 */

void getdev(void)
{
	int	rcode;

	ndev = 0;
	rcode = nftw(_PATH_DEV, gdev, 100, FTW_PHYS);

	switch(rcode) {

	case 0:	 return;		/* successful return, devl populated */

	case 1:
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_ftwproblem, "ftw() encountered problem: %s"), strerror(errno));
		exit(1);
	case -1:
		_sgi_nl_error(SGINL_SYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_ftwfailed, "ftw() failed: %s"), strerror(errno));
		exit(1);
	default:
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_ftwret, "ftw() unexpected return, rcode=%d: %s"), rcode, strerror(errno));
		exit(1);
	}
}


/* 
 * Procedure:	notinteresting
 *
 * Filters out directory names which don't normally contain
 * tty-like devices by comparing the initial path to a table
 * of known "uninteresting" strings.  Returns 1 if the string is
 * deemed uninteresting, 0 otherwise.
 */

char *excl[] = {
        "/dev/dsk",
        "/dev/rdsk",
        "/dev/mt",
        "/dev/rmt",
        "/dev/gro",
        "/dev/grin",
	"/dev/annex",
	"/dev/hl",
	"/dev/fd",
	"/dev/scsi",
	"/dev/hdsp",
	"/dev/vme",
	"/dev/input",
	"/dev/syscon",
	"/dev/systty",
	"/dev/tape",
	"/dev/xbmon",
        0
};

notinteresting(s)
char *s;
{
        register char **pref;

        for (pref = &excl[0]; *pref; pref++) {
                if (strncmp(s, *pref, strlen(*pref)) == 0)
                        return(1);
        }
        return(0);
}

/*
 * Procedure:     gdev
 *
 * gdev() puts device names and ID into the devl structure for character
 * special files in /dev.  The "/dev/" string is stripped from the name
 * and if the resulting pathname exceeds MAXNAMLEN in length then the highest
 * level directory names are stripped until the pathname is MAXNAMLEN or less.
 * For efficiency purposes, we disregard directories which we know will never
 * have tty-like files in them.
 */

int gdev(const char *objptr, const struct stat *objstatp, 
	 int numb,struct FTW *f)
{
	register int	i;
	int	leng, start;
	static struct devl ldevl[2];
	static int	lndev, consflg;
	struct stat	*statp,link_stat;

	statp = (struct stat *)objstatp;
	switch (numb) {
	case FTW_SL:
		/* Try to get the inode information of the file which
		 * this symbolic link references.
		 */
		if (stat(objptr,&link_stat))
			return(0);
		statp = &link_stat;
	case FTW_F:	
		if ((statp->st_mode & S_IFMT) == S_IFCHR) {
			/* Cut search path */
			if (notinteresting(objptr))
				return(0);

			/* Get more and be ready for syscon & systty. */
			while (ndev + lndev >= maxdev) {
				maxdev += UDQ;
				devl = (struct devl *) ((devl == NULL) ? 
				  malloc(sizeof(struct devl ) * maxdev) : 
				  realloc(devl, sizeof(struct devl ) * maxdev));
				if (devl == NULL) {
					err_nomem();
					exit(1);
				}
			}
#ifndef sgi
			/*
			 * Save systty & syscon entries if the console
			 * entry hasn't been seen.
			 */
			if (!consflg
			  && (strcmp("/dev/systty", objptr) == 0
			    || strcmp("/dev/syscon", objptr) == 0)) {
				(void) strncpy(ldevl[lndev].dname,
				  &objptr[5], MAXNAMLEN);
				ldevl[lndev].dev = statp->st_rdev;
				lndev++;
				return 0;
			}
#endif

			leng = strlen(objptr);
			/* Strip off /dev/ */
			if (leng < MAXNAMLEN + 4)
				(void) strcpy(devl[ndev].dname, &objptr[5]);
			else {
				start = leng - MAXNAMLEN - 1;

				for (i = start; i < leng && (objptr[i] != '/');
				  i++)
					;
				if (i == leng )
					(void) strncpy(devl[ndev].dname,
					  &objptr[start], MAXNAMLEN);
				else
					(void) strncpy(devl[ndev].dname,
					  &objptr[i+1], MAXNAMLEN);
			}
			devl[ndev].dev = statp->st_rdev;
			ndev++;
#ifndef sgi
			/*
			 * Put systty & syscon entries in devl when console
			 * is found.
			 */
			if (strcmp("/dev/console", objptr) == 0) {
				consflg++;
				for (i = 0; i < lndev; i++) {
					(void) strncpy(devl[ndev].dname,
					  ldevl[i].dname, MAXNAMLEN);
					devl[ndev].dev = ldevl[i].dev;
					ndev++;
				}
				lndev = 0;
			}
#endif
		}
		return 0;

	case FTW_D:
		if (notinteresting(objptr))
			f->quit = FTW_PRUNE;
	case FTW_DNR:
	case FTW_NS:
	case FTW_DP:
	case FTW_SLN:
		return 0;

	default:
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_gdeverr, "gdev() error, %d, encountered"), numb);
		return 1;
	}
}


/*
 * Procedure:     getpasswd
 *
 * Get the passwd file data into the ud structure.
 */

void
getpasswd(void)
{
	struct passwd *pw;

	ud = NULL;
	nud = 0;
	maxud = 0;

	while ((pw = getpwent()) != NULL) {
		while (nud >= maxud) {
			maxud += UDQ;
			ud = (struct udata *) ((ud == NULL) ? 
			  malloc(sizeof(struct udata ) * maxud) : 
			  realloc(ud, sizeof(struct udata ) * maxud));
			if (ud == NULL) {
				err_nomem();
				exit(1);
			}
		}
		/*
		 * Copy fields from pw file structure to udata.
		 */
		ud[nud].uid = pw->pw_uid;
		(void) strncpy(ud[nud].name, pw->pw_name, MAXLOGIN);
		nud++;
	}
	endpwent();
}

/*
 * Procedure:     wrdata
 *
 * Restrictions:
                 strerror: None
                 mktemp: None
                 open(2): None
                 chown(2): None
                 rename(2): None
                 unlink(2): None
*/
void
wrdata(void)
{
	int		fd;
	struct stat	statbuf;
	cap_t		ocap;
	cap_value_t	cap_chown = CAP_CHOWN;

	(void) umask(02);

	if (mktemp(pstmpfile) == (char *)NULL ||
	    pstmpfile[0] == '\0') {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_opnwr, "open() for write failed"));
		_sgi_nl_error(SGINL_SYSERR, cmd_label, "%s", psfile);
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_FIX,
		    gettxt(_SGI_DMMX_ps_notifySU, "Please notify your System Administrator"));
		return;
	}

	/*
	 * Make sure that the /tmp/.ps_data directory exists.
	 */
	if (stat(psdir, &statbuf) != 0) {
		if (errno == ENOENT) {
			/*
			 * Make the directory writable by only
			 * user root and group sys.  This allows
			 * the ps to muck with the directory entries
			 * since it is setgid sys, but it keeps
			 * others away.
			 */
			if (mkdir(psdir, 0775) != 0) {
				return;
			}
			ocap = cap_acquire(1, &cap_chown);
			if (chown(psdir, 0, 0) != 0) {
				cap_surrender(ocap);
				return;
			}
			cap_surrender(ocap);
		} else {
			return;
		}
	} else if (!(S_ISDIR(statbuf.st_mode))) {
		/*
		 * If the name is in use but it is not a directory,
		 * then it is probably a left over .ps_data file from
		 * when we used to keep it directly in /tmp.  Try to
		 * remove it and create the directory in its place.
		 * Make sure to get the owner and group permissions
		 * on the directory right as described above.
		 */
		if (remove(psdir) != 0) {
			return;
		}
		if (mkdir(psdir, 0775) != 0) {
			return;
		}
		ocap = cap_acquire(1, &cap_chown);
		if (chown(psdir, 0, 0) != 0) {
			cap_surrender(ocap);
			return;
		}
		cap_surrender(ocap);
	}

	if ((fd = open(pstmpfile, O_WRONLY|O_CREAT, 0664)) == -1) {
		/* only privileged user can write ps_data file */
		return;
	}

	/*
	 * Make owner root, group sys.
	 * This is for compatibility.
	 */
	ocap = cap_acquire(1, &cap_chown);
	(void)chown(pstmpfile, (uid_t)0, (gid_t)0);
	cap_surrender(ocap);

	/* write /dev data */
	pswrite(fd, (char *) &ndev, sizeof(ndev));
	pswrite(fd, (char *)devl, ndev * sizeof(*devl));

#ifndef UNS
	/* write /etc/passwd data */
	pswrite(fd, (char *) &nud, sizeof(nud));
	pswrite(fd, (char *)ud, nud * sizeof(*ud));
#endif

	(void) close(fd);
	if (rename(pstmpfile, psfile) == -1) {
		_sgi_nl_error(SGINL_SYSERR, cmd_label,
			gettxt(_SGI_DMMX_ps_rename, "rename() failed"));
		_sgi_ffmtmsg(stderr, 0, cmd_label, MM_FIX,
		    gettxt(_SGI_DMMX_ps_notifySU, "Please notify your System Administrator"));
		(void)unlink(pstmpfile);
	}
}

/*
 * getarg() finds the next argument in list and copies arg into argbuf.
 * p1 first pts to arg passed back from getopt routine.  p1 is then
 * bumped to next character that is not a comma or blank -- p1 NULL
 * indicates end of list.
 */

void getarg(void)
{
	char *parga;
	parga = argbuf;

	while(*p1 && *p1 != ',' && *p1 != ' ' && *p1 != '\t' && parga < (argbuf + ARGSIZ - 1))
		*parga++ = *p1++;
	*parga = '\0';

	while( *p1 && ( *p1 == ',' || *p1 == ' ' || *p1 == '\t') )
		p1++;
}

/*
 * gettty returns the user's tty number or ? if none.
 */
char *gettty(ip)
	register int	*ip;	/* where the search left off last time */
{
	register int	i;

	if (info.pr_ttydev != PRNODEV && *ip >= 0) {
		for (i = *ip; i < ndev; i++) {
			if (devl[i].dev == info.pr_ttydev) {
				*ip = i + 1;
				return devl[i].dname;
			}
		}
	}
	*ip = -1;
	return "?";
}

/*
 * Procedure:     prcom
 *
 * Restrictions:
                 lvlout: MACREAD
                 lvlproc: none
 *
 * Notes:
 * Print info about the process.
 */
prcom(puid, found, plblstring)
	uid_t	puid;
	int	found;
	char   *plblstring;
{
	register char	*cp;
	register char	*tp;
	long	tm;
	int	i, wcnt, length;
	wchar_t	wchar;
	register char	**ttyp, *str;
	int j;
	char buf[64];
	struct group *gr;
#ifdef UNS
	struct passwd *pw;
#endif

	if (info.pr_zomb && tflg && !found) 
		return 0;

#if NEVER
	/*
	 * SIDL - intermediate state in process creation 
	 * psinfo may not be consistent (skip it for now) -XXX
	 */
	if (info.pr_state==SIDL)
		return(1);
#endif

	/*
	 * Get current terminal.  If none ("?") and 'a' is set, don't print
	 * info.  If 't' is set, check if term is in list of desired terminals
	 * and print it if it is.
	 */
	i = 0;
	tp = gettty(&i);
	if (aflg && *tp == '?')
		return 0;
	if (tflg && !found) {
		int match = 0;

		/*
		 * Look for same device under different names.
		 */
		while (i >= 0 && !match) {
			for (ttyp = tty; (str = *ttyp) != 0 && !match; ttyp++)
				if (strcmp(tp, str) == 0)
					match = 1;
			if (!match)
				tp = gettty(&i);
		}
		if (!match)
			return 0;
		/*
		 * if -t was not specified (we're checking for terminal
		 * by default) require an effective uid match
		 */
		if (noflg && puid != euid)
		    return 0;
	}

	for ( j = 0; j < fcount; j++ ) {

		/* for each format entry, display */
		switch (fEntry[j].spec) {
		case PS_RUSER:
#ifdef UNS
			pw = getpwuid(cred.pr_ruid);
			if (pw) {
				printf("%*.*s ", fEntry[j].len,
				    fEntry[j].len, pw->pw_name);
			} else {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, cred.pr_ruid);
			}
#else
			if ((i = getunam(cred.pr_ruid)) >= 0)
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, 
				  ud[i].name);
			else {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, cred.pr_ruid);
			}
#endif
			break;

		case PS_USER:
#ifdef UNS
#ifdef _SHAREII
				if (yflg)
					pw = getpwuid(puid);
				else
					pw = getpwuid(cred.pr_euid);
#else /* _SHAREII */
				pw = getpwuid(cred.pr_euid);
#endif /* _SHAREII */
				if (pw) {
					printf("%*.*s ", fEntry[j].len,
					    fEntry[j].len, pw->pw_name);
				} else {
					sprintf(buf, "%%%dld ", fEntry[j].len);
					printf(buf, cred.pr_ruid);
				}
#else
#ifdef _SHAREII
				if (yflg)
					i = getunam(puid);
				else
					i = getunam(cred.pr_euid);
#else /* _SHAREII */
				i  = getunam(cred.pr_euid);
#endif /* _SHAREII */
				if (i >= 0)
					printf("%*.*s ", fEntry[j].len, fEntry[j].len, 
					  ud[i].name);
				else {
					sprintf(buf, "%%%dld ", fEntry[j].len);
					printf(buf, cred.pr_euid);
				}
#endif
			break;

		case PS_RGROUP:
			if (gr = getgrgid(cred.pr_rgid))
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, 
				  gr->gr_name);
			else {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, cred.pr_rgid);
			}
			break;

		case PS_GROUP:
			if (gr = getgrgid(cred.pr_egid))
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, 
				  gr->gr_name);
			else {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, cred.pr_egid);
			}
			break;

		case PS_PID:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, info.pr_pid);
			break;

		case PS_PPID:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, info.pr_ppid);
			break;

		case PS_PGID:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, info.pr_pgrp);
			break;

		case PS_PCPU:
			/* ok, ok, this is a hack job but the stats
			 * are not available in the kernel and at this point
			 * having ps wait a certain about of time to
			 * determine them isn't a good idea. */
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, 0);
			break;

		case PS_VSZ:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, info.pr_size * pgSize / 1024);
			break;

		case PS_NICE:
			if (info.pr_zomb) {
				sprintf(buf, "%%-%ds ", fEntry[j].len);
				printf(buf, "-");
				break;
			}
			if (strcmp(info.pr_clname, "TS") == 0) {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, info.pr_nice);
			}
#ifdef _SHAREII
			else if (strcmp(info.pr_clname, "SHR") == 0) {
				sprintf(buf, "%%%dld ", fEntry[j].len);
				printf(buf, info.pr_nice);
			}
#endif /* _SHAREII */				
			else if (strcmp(info.pr_clname, "WL") == 0) {
				sprintf(buf, "%%%d.%ds ", fEntry[j].len, fEntry[j].len);
				printf(buf, info.pr_clname);
			} else if (strcmp(info.pr_clname, "B") == 0) {
				sprintf(buf, "%%%d.%ds ", fEntry[j].len, fEntry[j].len);
				printf(buf, info.pr_clname);
			} else if (strcmp(info.pr_clname, "BC") == 0) {
				sprintf(buf, "%%%d.%ds ", fEntry[j].len, fEntry[j].len);
				printf(buf, info.pr_clname);
			} else {
				sprintf(buf, "%%%d.%ds ", fEntry[j].len, fEntry[j].len);
				printf(buf, info.pr_clname);
			}
			break;

		case PS_ETIME:
			if (info.pr_zomb) {
				sprintf(buf, "%%-%ds ", fEntry[j].len);
				printf(buf, "-");
				break;
			}
			if ((tm = tim - info.pr_start.tv_sec) < 0)
				tm = 0L;
			prformattime(tm,fEntry[j].len);
			break;

		case PS_TIME:
			tm = info.pr_time.tv_sec;
			if (info.pr_time.tv_nsec > 500000000)
				tm++;
			prformattime(tm,fEntry[j].len);
			break;

		case PS_TTY:
			sprintf(buf, "%%-%ds ", fEntry[j].len);
			if (info.pr_zomb) 
				printf(buf, "-");
			else
				printf(buf, tp);
			break;

		case PS_COMM:
			if (info.pr_zomb) {
				printf("%.*s ", fEntry[j].len, "<defunct>");
				break;
			}

			/* if not last, make sure to fill in spaces, if it
			 * is last, don't fill in spaces */
			wcnt = namencnt(info.pr_fname, 16, fEntry[j].len);
			if ((j + 1) < fcount)
				printf("%-*.*s ", fEntry[j].len, wcnt, info.pr_fname);
			else
				printf("%.*s ", wcnt, info.pr_fname);
			break;

		case PS_ARGS:
			if (info.pr_zomb) {
				printf("%.*s ", fEntry[j].len, "<defunct>");
				break;
			}
			for (cp = info.pr_psargs; cp < &info.pr_psargs[PRARGSZ]; ) {
				if (*cp == 0) 
					break;
				length = mbtowc(&wchar, cp, MB_LEN_MAX);
				if (length < 0 || !iswprint(wchar)) {
					sprintf(buf, " [ %.8s ]", info.pr_fname);
					printf("%.*s ", fEntry[j].len, buf);
					wcnt = 8;
					return 1;	
				}
				cp += length;
			}

			/* if not last, make sure to fill in spaces, if it
			 * is last, don't fill in spaces */
			if ((j + 1) < fcount) {
				wcnt = namencnt(info.pr_psargs, PRARGSZ, 
				  lflg ? fEntry[j].len : PRARGSZ);
				printf("%-*.*s ", (lflg ? fEntry[j].len : PRARGSZ), 
				  wcnt, info.pr_psargs);
			} else {
				wcnt = namencnt(info.pr_psargs, PRARGSZ, lflg ? 
				  fEntry[j].len : PRARGSZ);
				printf("%.*s", wcnt, info.pr_psargs);
			}
			break;

		case PS_STIME:
			if (info.pr_zomb) 
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, "-");
			else
				prstime(info.pr_start,fEntry[j].len);
			break;

		case PS_FLAG:
			sprintf(buf, "%%%dx ", fEntry[j].len);
				printf(buf, info.pr_flag & PR_FLAG_MASK); 
			break;

		case PS_STATE:
			i = 1;
			while (i < fEntry[j].len)
				printf(" "), i++;
			printf("%c ", info.pr_sname);
			break;

		case PS_WCHAN:
			if (info.pr_wchan && !info.pr_zomb) {
				printf("%*x ", fEntry[j].len,
				       info.pr_wchan);
			} else {
				i = 1;
				while (i < fEntry[j].len)
					putchar(' '), i++;
				printf("- ");
			}
			break;

		case PS_WNAME:
			if (info.pr_wchan && !info.pr_zomb) {
			    if (info.pr_wname[0])
				printf("%*.*s ",
				       fEntry[j].len, fEntry[j].len,
				       info.pr_wname);
			    else
				printf("%*x ", fEntry[j].len,
				       info.pr_wchan);
			} else {
				i = 1;
				while (i < fEntry[j].len)
					putchar(' '), i++;
				printf("- ");
			}
			break;

		case PS_UTIL:
			sprintf(buf, "%%%dd ", fEntry[j].len);
			printf(buf, info.pr_cpu & 0377);
			break;

		case PS_CELL:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, cellid);
			break;

		case PS_UID:
			sprintf(buf, "%%%dld ", fEntry[j].len);
			printf(buf, puid);
			break;

		case PS_ADDR:
			sprintf(buf, "%%%dlx ", fEntry[j].len);
			printf(buf, info.pr_addr);
			break;

		case PS_OPRI:
			if (strcmp(info.pr_clname, "WL") == 0) {
				sprintf(buf, "  w ");
				printf (buf, info.pr_oldpri);
			} else if (strcmp(info.pr_clname, "B") == 0) {
				sprintf(buf, "  b ");
				printf (buf, info.pr_oldpri);
			} else if (strcmp(info.pr_clname, "BC") == 0) {
				sprintf(buf, " bc ");
				printf (buf, info.pr_oldpri);
			} else {
				sprintf(buf, "%%%dd ", fEntry[j].len);
				printf(buf, info.pr_oldpri);
			}	
			break;

		case PS_PROC:
			if (info.pr_zomb) {
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, "-");
				break;
			}
			if ((int)info.pr_sonproc < 0) {
				i = 1;
				while (i < fEntry[j].len)
					putchar(' '), i++;
				printf("* ");
			} else {
				sprintf(buf, "%%%dd ", fEntry[j].len);
				printf(buf, info.pr_sonproc);
			}
			break;

		case PS_SZ:
			if (info.pr_zomb) {
				printf("%*.*s ", fEntry[j].len, fEntry[j].len, "-");
				break;
			}
			sprintf(buf, "%%%dd%c", fEntry[j].len, fEntry[j].d);
			printf(buf, info.pr_size);
			break;

		case PS_RSS:
			if (info.pr_zomb) {
				printf("%-*.*s ", fEntry[j].len, fEntry[j].len, "-");
				break;
			}
			sprintf(buf, "%%-%dd%c", fEntry[j].len, fEntry[j].d);
			printf(buf, info.pr_rssize);
			break;

		case PS_OTIME:
			i = fEntry[j].len - 5;
			while (i)
				putchar(' '), i--;
			tm = info.pr_time.tv_sec;
			if (info.pr_time.tv_nsec > 500000000)
				tm++;
			printf("%2ld:%.2ld ", tm / 60, tm % 60);
			break;

		case PS_CLASS:
			printf("%*.*s ", fEntry[j].len, fEntry[j].len,
			  (info.pr_zomb ? "-" : info.pr_clname));
			break;

		case PS_PRI:
			if (strcmp(info.pr_clname, "WL") == 0) 
				printf("  w ");
			else { 
				sprintf(buf, "%%%dd ", fEntry[j].len);
				printf(buf, info.pr_pri);
			}		
			break;

		case PS_SID:
			sprintf(buf, "%%%dd ", fEntry[j].len);
			printf(buf, info.pr_sid);
			break;

		case PS_BLANK:
			putchar(' ');
			break;
		case PS_LABEL:
			sprintf(buf, "%%-%ds ", fEntry[j].len);
			printf(buf, plblstring);
			break;
		}
	}

	return 1;
}

/*
 * Returns 1 if arg is found in array arr, of length num; 0 otherwise.
 */
int search(pid_t arr[], register int number, register pid_t arg)
{
	register int i;

        for (i = 0; i < number; i++)
		if (arg == arr[i])
			return 1;

	return 0;
}

/*
 * Procedure:     uconv
 *
 */
void
uconv(void) {
	uconv_all(uid_tbl, &nut);
	uconv_all(Uid_tbl, &nUt);
}

#ifdef UNS
void
uconv_all(struct udata *uid_tbl, int *nut)
{
	struct passwd *pw;
	uid_t pwuid;
	int i, j;

	for (i = 0; i < *nut; i++) {
		pw = getpwnam(uid_tbl[i].name);
		if (! pw && isdigit(*uid_tbl[i].name)) {
			pwuid = (uid_t)atol(uid_tbl[i].name);
			pw = getpwuid(pwuid);
		}
		if (pw) {
			uid_tbl[i].uid = pw->pw_uid;
			(void) strncpy(uid_tbl[i].name, pw->pw_name,
			  MAXLOGIN);
		} else {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_ps_unknuser, "unknown user %s"),
			    uid_tbl[i].name);
			for (j = i + 1; j < *nut; j++) {
				(void) strncpy(uid_tbl[j-1].name,
				  uid_tbl[j].name, MAXLOGIN);
			}
			*nut -= 1;
			if (*nut <= 0) 
				exit(1);
			i--;
		}
	}
			
}
#else
void
uconv_all(struct udata *uid_tbl, int *nut)
{
	uid_t pwuid;
	int found, i, j;

	/*
	 * Search name array for oarg.
	 */
	for (i = 0; i < *nut; i++) {
		found = -1;
		for (j = 0; j < nud; j++) {
			if (strncmp(uid_tbl[i].name, ud[j].name,
			  MAXLOGIN) == 0) {
				found = j;
				break;
			}
		}
		/*
		 * If not found and oarg is numeric, search number array.
		 */
		if (found < 0
		  && uid_tbl[i].name[0] >= '0'
		  && uid_tbl[i].name[0] <= '9') {
			pwuid = (uid_t)atol(uid_tbl[i].name);
			for (j = 0; j < nud; j++) {
				if (pwuid == ud[j].uid) {
					found = j;
					break;
				}
			}
		}

		/*
		 * If found, enter found index into tbl array.
		 */
		if (found != -1) {
			uid_tbl[i].uid = ud[found].uid;
			(void) strncpy(uid_tbl[i].name, ud[found].name,
			  MAXLOGIN);
		} else {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_ps_unknuser, "unknown user %s"),
			    uid_tbl[i].name);
			for (j = i + 1; j < *nut; j++) {
				(void) strncpy(uid_tbl[j-1].name,
				  uid_tbl[j].name, MAXLOGIN);
			}
			*nut -= 1;
			if (*nut <= 0) 
				exit(1);
			i--;
		}
	}
	return;
}
#endif

/*
 * For full command listing (-f flag) print user name instead of number.
 * Search table of userid numbers and if puid is found, return the
 * corresponding name.  Otherwise search /etc/passwd.
 */
int getunam(puid)
	register uid_t puid;
{
	register int i;

	for (i = 0; i < nud; i++)
		if (ud[i].uid == puid)
			return i;
	return -1;
}

/*
 * Return 1 if puid is in table, otherwise 0.
 */
int ufind(puid,flag)
	register uid_t puid;
{
	register int i;
	int count = (flag ? nut : nUt);

	for (i = 0; i < count; i++)
		if (flag && uid_tbl[i].uid == puid)
			return 1;
		else if (!flag && Uid_tbl[i].uid == puid)
			return 1;
	return 0;
}

/*
 * Procedure:     psread
 *
 * Restrictions:
                 read(2): None
                 unlink(2): None
 * Notes:
 * Special read; unlinks psfile on read error.
 */
int psread(fd, bp, bs)
	int fd;
	char *bp;
	unsigned int bs;
{
	int rbs;

	if ((rbs = read(fd, bp, bs)) != bs) {
		(void) unlink(psfile);
		return 0;
	}
	return 1;
}

/*
 * Procedure:     pswrite
 *
 * Restrictions:
                 write(2): None
                 unlink(2): None
 * Notes:
 * Special write; unlinks pstmpfile on write error.
 */
void
pswrite(fd, bp, bs)
int	fd;
char	*bp;
unsigned	bs;
{
	int	wbs;

	if ((wbs = write(fd, bp, bs)) != bs) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_ps_pswriteerr,
			"pswrite() error on write, wbs=%d, bs=%d"),
		    wbs, bs);
		(void) unlink(pstmpfile);
	}
}

/*
 * Procedure:     prstime
 *
 * Restrictions:
 *               cftime: MACREAD opens /usr/lib/locale/<language>/LC_TIME 
 *                       which should be at SYS_PUBLIC
 *               gettxt: None
 *               printf: None
 * Notes:
 * Print starting time of process unless process started more than 24 hours
 * ago, in which case the date is printed.
 */
void
prstime(timespec_t st, int len)
{
	char sttim[26];
	time_t starttime;

	starttime = st.tv_sec;
	if (st.tv_nsec > 500000000)
		starttime++;
	
	if (tim - starttime > 24*60*60) {
		cftime(sttim, gettxt(":724","%b %d"), &starttime);
		sttim[8] = '\0';
	} else {
		cftime(sttim, gettxt(":725","%H:%M:%S"), &starttime);
		sttim[8] = '\0';
	}
	printf("%*.*s ", len, len, sttim);
}


void
prformattime(time_t tm, int len)
{
	char buf[24];
	long l,m;

	l = tm % (24*60*60);
	m = l % (60*60);
	if (tm > 24*60*60)
		sprintf(buf, "%2ld-%.2ld:%.2ld:%.2ld", tm / (24*60*60), l / (60*60),
		  m / 60, m % 60 );
	else if (tm > 60*60)
		sprintf(buf, "   %2ld:%.2ld:%.2ld", l / (60*60), m / 60, m % 60 );
	else
		sprintf(buf, "      %2ld:%.2ld", m / 60, m % 60);

	printf("%*.*s ", len, len, buf);
}


/*
 * Returns true iff string is all numeric.
 */
int num(s)
	register char	*s;
{
	register int c;

	if (s == NULL)
		return 0;
	c = *s;
	do {
		if (!isdigit(c))
			return 0;
	} while (c = *++s);
	return 1;
}

/*
 * Function to compute the number of printable bytes in a multibyte
 * command string ("internationalization").
 */
int namencnt(cmd, eucsize, scrsize)
	register char *cmd; 
	int eucsize;
	int scrsize;
{
	register int eucwcnt = 0, scrwcnt = 0;
	register int neucsz, nscrsz;
	wchar_t  wchar;

	while (*cmd != '\0') {
		if ((neucsz = mbtowc(&wchar, cmd, MB_LEN_MAX)) < 0)
			return 8; /* default to use for illegal chars */
		if ((nscrsz = wcwidth(wchar)) == 0)
			return 8;
		if (eucwcnt + neucsz > eucsize || scrwcnt + nscrsz > scrsize)
			break;
		eucwcnt += neucsz;
		scrwcnt += nscrsz;
		cmd += neucsz;
	}
	return eucwcnt;
}


/*
 * Interpret format command line and output appropriate header
 */
void
FormatSpecifier(char *str)
{

	int i = 0, found, len, l, j;
	char id, buf[32], delimit;
	char *rstr = str;

	while (str && str[i]) {
		found = 0;

		/* if there are white spaces skip them */
		while (str[i] == ' ')
			i++;

		for (j = 0; j < FORMAT_ENTRIES && str[i]; j++) 
			if (!strncmp(fname[j].name, &str[i], fname[j].namelen)) {
				found++;
				i += fname[j].namelen;
				id = fname[j].spec;

				/* override the default name */
				if (str[i] == '=') {
					i++;
					len = 0;

					/* look for delimitor */
					while (str[i+len] != ',' && str[i+len] != '\0')
						len++;

					/* is it long enough */
					l = len < fname[j].minlen ? fname[j].minlen : len;

					/* create format string to fit requirements */
					sprintf(buf, "%%s%%%s%d.%ds ", 
					  (fname[j].adjust == LEFT_ADJUST ? "-" : ""), l, len);

					sprintf(label, buf, label, &str[i]);
					i += len;
					len = l;
					delimit = ' ';
				} else {
					len = fname[j].minlen;
					sprintf(buf, "%%s%%%s%d.%ds", 
				  	  (fname[j].adjust == LEFT_ADJUST ? "-" : ""), len, len);
					sprintf(label, buf, label, fname[j].def);
					if (str[i] == ':')
						strcat(label, ":"), i++, delimit = ':';
					else
						strcat(label, " "), delimit = ' ';
				}
				
				/* add entry */
				if (fcount == FORMAT_MAX)
					return;
				fEntry[fcount].spec = id;
				fEntry[fcount].len = len;
				fEntry[fcount].d = delimit;
				fcount++;

				/* if delimitor, go on to next */				
				if (str[i] == ',')
					i++;

				if (id == PS_LABEL && trix_mac)
					Mflg++;

				break;
			}
		if (!found) {
			err_opt_c(rstr, 'o');
			exit(1);
		}
	}
}


void
FillFormat(void)
{
	char format[512];

	format[0] = '\0';
	if (Mflg)
		strcat(format, "label,");
	if (!lflg && !fflg)
		strcat(format, "blank,");
	else if (lflg)
		strcat(format, "flag,state,");
	if (fflg)
		strcat(format, "user=UID,");
	else if (lflg)
		strcat(format, "uid,");
	strcat(format, "pid,");
	if (lflg || fflg)
		strcat(format, "ppid,");
	if (jflg)
		strcat(format, "pgid,sid,");
	if (xflg || Xflg)
	        strcat(format, "cell,");
	if (cflg)
		strcat(format, "class,pri,");
	else if (lflg || fflg) {
		if (!cflg)
			strcat(format, "util,");
		if (lflg)
			strcat(format, "opri,nice,");
	}
	if (lflg) {
		if (xpg)
			strcat(format, "addr,sz,wname,");
		else
			strcat(format, "cpu,sz:rss,wname,");
	}
	if (fflg)
		strcat(format, "stime,tty,otime,args=CMD");
	else 
		strcat(format, "tty,otime,comm=CMD");

	FormatSpecifier(format);	
}
