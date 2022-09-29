/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/utmpadmin/RCS/viutmp.c,v 1.4 1996/09/19 16:42:23 pcr Exp $ */
/* 
 * viutmp.c: Utmp file editor with locking
 *
 * $Log: viutmp.c,v $
 * Revision 1.4  1996/09/19 16:42:23  pcr
 * FIX #424625 Year 2000
 *
 * Revision 1.3  1995/04/10  17:02:41  jwag
 * fix for utmpx - pid was wrong, host, sid were missing
 *
 * Revision 1.2  1994/07/26  07:11:57  bowen
 * 	Fixed a typo in the usage message.
 *
 * Revision 1.1  1993/12/18  06:34:44  bowen
 * Initial revision
 *
 * Revision 1.11  1991/06/24  03:59:54  christos
 * Apollo fixes
 *
 * Revision 1.10  1991/05/02  17:38:16  root
 * Removed local dependencies.
 *
 * Revision 1.9  1990/05/26  16:18:18  root
 * Fixed for hpux 7.0
 *
 * Revision 1.8  90/03/07  17:31:03  christos
 * Added system V support
 * 
 * Revision 1.7  90/02/01  20:12:01  christos
 * Added rt support.
 * 
 * Revision 1.6  89/12/06  08:16:07  christos
 * Fixed typo that would not let us change the
 * hour
 * 
 * Revision 1.5  89/11/21  05:04:49  christos
 * We don't need to write the utmp
 * file if no changes were made.
 * Also better signal handling.
 * 
 * Revision 1.4  89/11/19  20:37:04  christos
 * Fixed so that time can be set,
 * and changed separator to |
 * so that unix:0.0 does not get
 * misinterpeted.
 * 
 * Revision 1.3  89/04/06  06:08:35  christos
 * Fixed bug on the sun, to truncate
 * the existing utmp file so that we
 * can remove entries
 * 
 * Revision 1.2  89/04/06  06:06:08  christos
 * Added SUID stuff to run as a
 * mortal, on the suns
 * 
 * Revision 1.1  89/04/06  05:51:05  christos
 * Initial revision
 * 
 */
#ifndef lint
static char rcsid[] = "$Id: viutmp.c,v 1.4 1996/09/19 16:42:23 pcr Exp $";
#endif /* lint */

/*
 * If invoked as viutmp, viwtmp, viutmpx(vixutmp), or viwtmpx(xwtmp) 
 * [4.x names in parens], viutmp.c selects path and record-formats 
 * appropriate for viewing the default system utmp files with a minimum
 * of typing.  Multiple soft links to one binary are a convenient way
 * to invoke the program as different names.
 *
 * To view other utmp- or utmpx-format files, invoke as `viutmp' with 
 * the required -f <utmpfilename> arg; to force `viutmp' to interpret
 * the records as utmpx structs, the -x option must be used.  [Note that
 * <utmpfilename> must include the path if it isn't in the cwd.]
 * 
 * I've used this program as a RO method of inspecting utmp, xutmp,
 * and utmpx-format files: I've never modified any of the entries,
 * although the code to do it looks complete.  Paul Close grabbed this
 * program off the net, and I hacked it to understand both 4.x and 5.x
 * struct formats and default directories, and added the -x and -f 
 * options (patterned after their last(1) counterparts).  I assume
 * no responsibility for this code or the results of its execution.
 * It's a handy little tool: use or modify it as you wish.  -- JFB
 */


/*
 * for the suns, the /etc/utmp file is publicly writeable, 
 * so this does not need to run suid, but since we cannot
 * write in / the rename call won't work, so we resort to
 * just exclusively open utmp and write the data!
 * If something happens in the middle we might mangle /etc/utmp
 * but this is not the end of the world!
 */
#define NOSUID
#define SEP	'|'
#define USG
typedef void sigret_t;

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <limits.h>

#include <utmpx.h>	/* #includes utmp.h */

#define VTPATH		"/var/tmp/"

#define NIL(a)	((a *) 0)
#define NEW(a)	((a *) Malloc(sizeof(a)))
#define public
#define PERROR strerror(errno)

/*
 * {u,w}tmp files are composed of utmp structs; {u,w}tmpx files contain
 * utmpx (extended) structs.  Assume the latter format if argv[0] ==
 * "viutmpx" or "viwtmpx"; else use the older format.
 */
#define NFMT	1
#define XFMT	2
#define ISXFMT	(ffmt == XFMT)
#define ISNFMT	(ffmt == NFMT)

int ffmt = -1;

typedef struct utlist_t {
    union {
	struct utmp n_data;
	struct utmpx x_data;
    }ut_data;
    struct utlist_t *next;
} utlist_t;
#define ndata	ut_data.n_data
#define xdata	ut_data.x_data

utlist_t *ut = NIL(utlist_t);

static unique_fname(char *newfname);
static time_t get_long(char **str, char *prom, char sep, int minx, int maxx);
static time_t set_time(char **str);
static char *show_time(time_t ti);
static void zeropad(char *dest, char *src, int len);
static int	usage(char *);

char	utmp_fname[]	= UTMP_FILE;
char	utmpx_fname[]	= UTMPX_FILE;
char	wtmp_fname[]	= WTMP_FILE;
char	wtmpx_fname[]	= WTMPX_FILE;
char	*utmp_file = (char *)&utmpx_fname[0];

char	pname[NAME_MAX+1];	/* progname (argv[0] after stripping path) */
char	temp_fname[NAME_MAX+1];	/* generated temp filename stored here */

char	buf[BUFSIZ];
extern  time_t timelocal(struct tm *);

static sigret_t		bye_bye(int);
static char		*Malloc(unsigned);

#define UTMP_PNAME	"viutmp"
#define WTMP_PNAME	"viwtmp"
#define UTMPX_PNAME	"viutmpx"
#define WTMPX_PNAME	"viwtmpx"

static char *state[] = {
    "empty", "run level", "boot time", "old time", "new time",
    "init process", "login process", "user process", "dead process",
    "accounting" 
};

int fd;

public 
/*ARGSUSED*/
main(argc, argv)
int 	argc;
char *	argv[];
{
    char *cp, *scp;
    int fu, c;
    FILE *ft;
    char *editor;
    char *faddr = NULL;		/* addr of utmp[x] field to be changed */
    char *fmt;
    struct stat s1buf, s2buf;
    struct utmp um;
    struct utmpx umx;
    struct utlist_t *up; 
    long tmptime;
    int line, i;
    int utsize;
    int errflg = 0;

    up = NIL(utlist_t);
    (void) signal(SIGHUP, bye_bye);
    (void) signal(SIGINT, bye_bye);
    (void) signal(SIGQUIT, bye_bye);
    (void) signal(SIGTERM, bye_bye);
    (void) setbuf(stderr, NIL(char));
    (void) umask(0);

    cp = strrchr(argv[0], '/');
    if (!cp)	/* running from current directory */
	strncpy(&(pname[0]), argv[0], NAME_MAX);
    else	/* cp is pointing to last '/' in path */
	strncpy(&(pname[0]), cp+1, NAME_MAX);

    /*
     * The binary utmp file records are converted into character format
     * and dumped into "temp_fname" temporary file, the generated name
     * incorporates the runtime process id and is therefore unique.  The
     * file is unlinked before exit; in order to keep a copy of the
     * textfile write it to another file from vi.
     */
    unique_fname(temp_fname);

    /* 
     * if path-stripped argv[0] == viutmp, viwtmp, viutmpx(vixutmp),
     * or viwtmpx(vixwtmp) [4.0 name in parens] prepend "/var/adm/"
     * (or "/etc" for 4.0) utmp directory path, and select the correct
     * record-format.
     */
    if (argc == 1) {  /* viutmp, viutmpx/vixutmp, viwtmp, or viwtmpx/vixwtmp */
	if (strcmp(pname, WTMP_PNAME) == 0) {
	    utmp_file = &wtmp_fname[0];
	    ffmt = NFMT;
	} else if (strcmp(pname, WTMPX_PNAME) == 0) {
	    utmp_file = &wtmpx_fname[0];
	    ffmt = XFMT;
	} else if (strcmp(pname, UTMP_PNAME) == 0) {
	    utmp_file = &utmp_fname[0];
	    ffmt = NFMT;
	} else if (strcmp(pname, UTMPX_PNAME) == 0) {
	    utmp_file = &utmpx_fname[0];
	    ffmt = XFMT;
	} else {	/* illegal executable name */
	    usage(pname);
	}
    } else if (strcmp(pname, UTMP_PNAME)) {    /* only generic name accepted */
	(void) fprintf(stderr, "%s: only '%s' format accepts options\n",
		pname, UTMP_PNAME);
	usage(NULL);
    } else {
	/* this format requires an alternate utmp filename (-f opt);
	 * the records in that file are assumed to be of 'struct utmp'
	 * format unless the -x flag is specified.
	 */
	ffmt = NFMT;
	utmp_file[0] = '\0';
	while ((c = getopt(argc, argv, "xf:")) != EOF) {
	    switch(c) {
	    case 'x':	/* interpret file contents as utmpx structs */
		ffmt = XFMT;
		break;
	    case 'f':	/* -f <utmpfname> */
		if (utmp_file[0])	/* already saw -f option */
		    errflg++;
	 	else
		    utmp_file = (char *)optarg;
		break;
	    case '?':
		errflg++;
		break;
	    default:
    	        (void) fprintf(stderr,
		    "%s: ??getopts returned bad option `%c'(==0x%x)??\n",
		    pname, (char)c, c);
		exit(-3);
	    }
	}

	/*
	 * if errflg is set, getopts() found an error and already 
	 * displayed a message
	 */
	if (errflg)
	    usage(NULL);

	if (utmp_file[0] == '\0') {
    	    (void) fprintf(stderr,
		"%s:  requires a utmp file, including path:\n",
		pname);
	    usage(NULL);
	}
    }

#if VERBOSE
    (void) fprintf(stderr, "%s for release 5.x or later\n", pname);
#endif

    if (ffmt == -1 || (ffmt != NFMT && ffmt != XFMT)) {
	(void) fprintf(stderr, "!!??internal error: invalid ffmt (%d)\n", ffmt);
	exit(7);
    }
    utsize = (ISXFMT ? sizeof(struct utmpx) : sizeof(struct utmp));

    (void) fprintf(stderr, "\t%s: examine file `%s' using %s entry format\n",
	pname, utmp_file, (ISXFMT ? "utmpx" : "utmp"));

    if ( (fd = open(temp_fname, O_WRONLY|O_CREAT|O_EXCL, 0644)) == -1 ) {
	if (errno == EEXIST) {
	    (void) fprintf(stderr, "%s: tempfile \"%s\" is busy.\n",
		pname, temp_fname);
	    exit(1);
	}
	(void) fprintf(stderr, "%s: cannot open tempfile \"%s\" (%s).\n",
	    pname, temp_fname, PERROR);
	exit(1);
    }
    if ((ft = fdopen(fd, "w")) == NIL(FILE)) {
	(void) fprintf(stderr, "%s: cannot fdopen tempfile \"%s\" (%s).\n",
	    pname, temp_fname, PERROR);
	bye_bye(1);
    }

    fprintf(stderr, "\topening utmp file \"%s\"\n", utmp_file);
    sleep(3);
    if ((fu = open(utmp_file, O_RDONLY)) == -1) {
	(void) fprintf(stderr, "%s: cannot open \"%s\" (%s).\n",
	    pname, utmp_file, PERROR);
	bye_bye(1);
    }

    if (ISXFMT) {	/* file contains extended-format entries */
	while ( read(fu, (char *) &umx, utsize) == utsize ) {
	    (void) fprintf(ft,
		    "%.*s%c%.*s%c%.*s%c%d%c%s%c%d%c%d%c%s%csid%d%c%.*s\n",
		    sizeof(umx.ut_user), umx.ut_user, SEP, 
		    sizeof(umx.ut_id),   umx.ut_id,   SEP, 
		    sizeof(umx.ut_line), umx.ut_line, SEP,
		    umx.ut_pid, SEP,
		    umx.ut_type >= 0 && umx.ut_type <= UTMAXTYPE ?
			    state[umx.ut_type] : "*error*",  SEP,
		    umx.ut_exit.e_termination, SEP, 
		    umx.ut_exit.e_exit, SEP,
		    show_time(umx.ut_xtime), SEP,
		    umx.ut_session, SEP,
		    umx.ut_syslen, umx.ut_host);
	}
    } else {	/* file contains unextended-format (struct utmp) records */
	while ( read(fu, (char *) &um, utsize) == utsize ) {
	    (void) fprintf(ft, "%.*s%c%.*s%c%.*s%c%d%c%s%c%d%c%d%c%s\n",
		    sizeof(um.ut_user), um.ut_user, SEP, 
		    sizeof(um.ut_id),   um.ut_id,   SEP, 
		    sizeof(um.ut_line), um.ut_line, SEP,
		    um.ut_pid, SEP,
		    (um.ut_type >= 0 && um.ut_type <= UTMAXTYPE ?
			    state[um.ut_type] : "*?!badtype!?*"),  SEP,
		    um.ut_exit.e_termination, SEP, 
		    um.ut_exit.e_exit, SEP,
		    show_time(um.ut_time));
	}
    }

    (void) fclose(ft); (void) close(fu);
    if (stat(temp_fname, &s1buf) < 0) {
	(void) fprintf(stderr,
	    "%s: can't stat temp file \"%s\" (%s), %s unchanged\n",
	    pname, temp_fname, PERROR, utmp_file);
	bye_bye(1);
    }
    editor = getenv("EDITOR");
    if (editor == 0)
	    editor = "vi";
    (void) sprintf(buf, "%s %s", editor, temp_fname);
    if (system(buf) != 0) {
	(void) fprintf(stderr, "%s: could not execute editor (%s).\n",
	    pname, PERROR);
	bye_bye(1);
    }

    /* 
     * sanity checks 
     */
    if (stat(temp_fname, &s2buf) < 0) {
	(void) fprintf(stderr,
	    "%s: can't stat temp file \"%s\" (%s); %s unchanged\n",
	    pname, temp_fname, PERROR, utmp_file);
	bye_bye(1);
    }
    if (s1buf.st_size == 0) {
	(void) fprintf(stderr, "%s: bad temp file \"%s\"; %s unchanged\n",
	    pname, temp_fname, utmp_file);
	bye_bye(1);
    }
    if (s1buf.st_mtime == s2buf.st_mtime) {
	(void) fprintf(stdout, "%s: no changes made\n", utmp_file);
	bye_bye(0);
    }

    if ((ft = fopen(temp_fname, "r")) == NIL(FILE)) {
	(void) fprintf(stderr,
	    "%s: can't reopen temp file \"%s\";  %s unchanged\n",
	    pname, temp_fname, utmp_file);
	bye_bye(1);
    }

    for (line = 1; fgets(buf, sizeof(buf) - 1, ft) != NIL(char); line++) {

	/*
	 * Ignore empty lines
	 */
	if (strchr(buf, '\n') == NIL(char)) 
	    continue;

	/*
	 * allocate record
	 */
	if ( ut == NIL(utlist_t) ) 
	    ut = up = NEW(utlist_t);
	else {
	    up->next = NEW(utlist_t);
	    up = up->next;
	}
	up->next = NIL(utlist_t);

	/*
	 * Get user name
	 */
	scp = buf;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing user name", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	if (ISXFMT)
	    zeropad(up->xdata.ut_user, scp, sizeof(up->xdata.ut_user));
	else
	    zeropad(up->ndata.ut_user, scp, sizeof(up->ndata.ut_user));
	/*
	 * get id
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing id", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	if (ISXFMT)
	    zeropad(up->xdata.ut_id, scp, sizeof(up->xdata.ut_id));
	else
	    zeropad(up->ndata.ut_id, scp, sizeof(up->ndata.ut_id));
	/*
	 * get tty
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing tty", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	if (ISXFMT)
	    zeropad(up->xdata.ut_line, scp, sizeof(up->xdata.ut_line));
	else
	    zeropad(up->ndata.ut_line, scp, sizeof(up->ndata.ut_line));

	/*
	 * get pid
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing process id", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	if (ISXFMT) {
	    fmt = "%d";
	    faddr = (char *)&(up->xdata.ut_pid);
	} else {
	    fmt = "%hd";
	    faddr = (char *)&(up->ndata.ut_pid);
	}
	if (sscanf(scp, fmt, faddr) != 1) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "bad process id", line, utmp_file);
	    bye_bye(1);
	}

	/*
	 * get entry type
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing entry type", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	for (i = 0; i <= UTMAXTYPE; i++)
	    if (strcmp(scp, state[i]) == 0)
		break;
	if (i != UTMAXTYPE + 1) {
	    if (ISXFMT)
		up->xdata.ut_type = i;
	    else
		up->ndata.ut_type = i;
	} else {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "bad entry type", line, utmp_file);
	    bye_bye(1);
	}

	/*
	 * get termination status
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing termination status", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';

	faddr = (char *)(ISXFMT ? &(up->xdata.ut_exit.e_termination) :
			&(up->ndata.ut_exit.e_termination));
	if (sscanf(scp, "%hd", faddr) != 1) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "bad termination status", line, utmp_file);
	    bye_bye(1);
	}

	/*
	 * get exit status
	 */
	scp = cp;
	if ((cp = strchr(scp, SEP)) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing exit status", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';

	faddr = (char *)(ISXFMT ? &(up->xdata.ut_exit.e_exit) :
			&(up->ndata.ut_exit.e_exit)); 
	if (sscanf(scp, "%hd", faddr) != 1) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "bad exit status", line, utmp_file);
	    bye_bye(1);
	}

	/*
	 * get logintime
	 */
	scp = cp;
	if ((cp = strchr(scp, ISXFMT ? SEP : '\n')) == NIL(char)) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, "missing login time", line, utmp_file);
	    bye_bye(1);
	}
	*cp++ = '\0';
	if ( (tmptime = set_time(&scp)) == -1 ) {
	    (void) fprintf(stderr,
		"%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		pname, scp, line, utmp_file);
	    bye_bye(1);
	}
	if (ISXFMT) {
	    up->xdata.ut_tv.tv_sec = tmptime;
	    up->xdata.ut_tv.tv_usec = 0;
	} else
	    up->ndata.ut_time = tmptime;
	if (ISXFMT) {
	    /* still have session and host name! */
	    scp = cp;
	    if ((cp = strchr(scp, SEP)) == NIL(char)) {
	        (void) fprintf(stderr,
		    "%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		    pname, "missing session id", line, utmp_file);
	        bye_bye(1);
	    }
	    *cp++ = '\0';
	    if (sscanf(scp, "sid%ld", &up->xdata.ut_session) != 1) {
	        (void) fprintf(stderr,
		    "%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		    pname, "bad session id", line, utmp_file);
	        bye_bye(1);
	    }

	    /* host name */
	    scp = cp;
	    if ((cp = strchr(scp, '\n')) == NIL(char)) {
	        (void) fprintf(stderr,
		    "%s: Syntax error (%s) at line %d; %s unchanged.\n", 
		    pname, "missing host name", line, utmp_file);
	        bye_bye(1);
	    }
	    *cp++ = '\0';
	    i = strlen(scp);
	    memcpy(up->xdata.ut_host, scp, i+1);
	    up->xdata.ut_syslen = i+1;
	}
    }
    (void) fclose(ft);

#ifdef NOSUID
    if ( (fd = open(utmp_file, O_WRONLY|O_EXCL|O_TRUNC, 0666)) == -1 ) {
	(void) fprintf(stderr, "%s: cannot open `%s' (%s).\n",
	    pname, utmp_file, PERROR);
	bye_bye(1);
    }
#else
    if ( (fd = open(temp_fname, O_WRONLY, 0666)) == -1 ) {
	(void) fprintf(stderr, "%s: cannot open `%s' (%s).\n",
	    pname, temp_fname, PERROR);
	bye_bye(1);
    }
#endif

    for ( line = 0, up = ut; up != NIL(utlist_t); up = up->next, line++ ) {
	if (ISXFMT)
	    faddr = (char *)&(up->xdata);
	else
	    faddr = (char *)&(up->ndata);
	if ( write(fd, (char *) faddr, utsize) != utsize ) {
	    (void) fprintf(stderr, "%s: write failed (%s); %s unchanged.\n",
		pname, PERROR, utmp_file);
	    bye_bye(1);
	}
    }
    
    (void) close(fd);
    (void) unlink(temp_fname);	/* else open() will fail in next run */


    (void) fprintf(stdout, "%s:  `%s\' contains %d entries.\n", pname,
				utmp_file, line);
    exit(0);

} /* end main */


char gudpnames[] = "\tviutmp\n\tviutmpx\n\tviwtmp\n\tviwtmpx";

/* usage(): if "progname"  != NULL, the invoking name of the executable
 * file is invalid.  Else opt or arg error.
 */
static
usage(char *progname)
{
    if (progname)
	(void) fprintf(stderr, "\"%s\": invalid program name\n", progname);

    (void) fprintf(stderr, "\nusage:\n%s\n", gudpnames);
    (void) fprintf(stderr, "\tviutmp -f UTMPFNAME [-x]\n");
    exit(1);
}

/* bye_bye():
 *	Mop up and exit
 */
static sigret_t
bye_bye(int sig)
{
    if (sig == 0)
	(void) close(fd);

    (void) unlink(temp_fname);
    exit(sig);
}

/* zeropad():
 *	Copy src to dest with zeropadding to len
 */
static void
zeropad(char *dest, char *src, int len)
{
    int i, slen = strlen(src);

    for ( i = 0; i < len; i++ ) 
	if ( i < slen )
	   dest[i] = src[i];
	else
	   dest[i] = 0;
} /* zeropad */

static char *
show_time(time_t ti)
{
    static char buffer[BUFSIZ];
    struct tm *t;

    if (ti != 0) {
	t = localtime(&ti);
	(void) sprintf(buffer, "%.2d:%.2d:%.2d,%.2d/%.2d/%.2d",
	    t->tm_hour, t->tm_min, t->tm_sec,
	    t->tm_mon + 1, t->tm_mday, t->tm_year%100);
    }
    else
	buffer[0] = '\0';

    return(buffer);
} /* show_time */

static time_t
set_time(char **str)
{
    struct tm t, *tn;
    time_t sec;

    (void) time(&sec);
    tn = localtime(&sec);

    t = *tn;

    if (*str[0] == '\0')
	return((time_t) 0);

    if ((t.tm_hour = get_long(str, "hour", ':', 0, 23)) == -1) 
	return((time_t) -1);
    
    if ((t.tm_min = get_long(str, "minute", ':', 0, 59)) == -1) 
	return((time_t) -1);

    if ((t.tm_sec = get_long(str, "second", ',', 0, 59)) == -1) 
	return((time_t) -1);

    if ((t.tm_mon = get_long(str, "month", '/', 1, 12)) == -1) 
	return((time_t) -1);
    else
	t.tm_mon--;

    if ((t.tm_mday = get_long(str, "day", '/', 1, 31)) == -1) 
	return((time_t) -1);

    if ((t.tm_year = get_long(str, "year", '\0', 0, 100)) == -1) 
	return((time_t) -1);

    return(timelocal(&t));

} /* set_time */

static time_t 
get_long(char **str, char *prom, char sep, int minx, int maxx)
{
    static char buffer[BUFSIZ];
    char *ptr;
    int val;

    if ((ptr = strchr(*str, sep)) == NIL(char)) {
	(void) sprintf(buffer, "missing %s", prom);
	*str = buffer;
	return((time_t) -1);
    }
    *ptr++ = '\0';
    if (sscanf(*str, "%d", &val) != 1) {
	(void) sprintf(buffer, "bad %s `%s'", prom, ptr);
	*str = buffer;
	return((time_t) -1);
    }
    *str = ptr;
    if (val < minx || val > maxx) {
	(void) sprintf(buffer, "out of range %s `%d'", prom, val);
	*str = buffer;
	return((time_t) -1);
    }
    
    return((time_t) val);

} /* get_long */


#ifndef sun
/* timelocal():
 *	We don't have it and it is hard to do
 */
/*ARGSUSED*/
time_t 
timelocal(struct tm *tm)
{
    return(time(NIL(time_t)));
} /* end timelocal */
#endif

static char *
Malloc(unsigned n)
{
    char *ptr;

    if ((ptr = malloc(n)) == NIL(char)) {
	(void) fprintf(stderr, "Out of memory.\n");
	exit(1);
    }
    return(ptr);

} /* Malloc */

static unique_fname(char *newfname)
{
    pid_t p = getpid();

    sprintf(newfname, "%sut%d", VTPATH, p);
#if VERBOSE
    fprintf(stderr, "unique_fname: generated fname = \"%s\"\n", newfname);
#endif
    return(0);

}
