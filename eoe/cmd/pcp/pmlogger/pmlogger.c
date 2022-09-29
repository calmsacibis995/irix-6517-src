/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: pmlogger.c,v 2.77 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"


#define CONFIGDIR "/var/pcp/config/pmlogger/"

char		*configfile = NULL;
FILE		*fconfig;		/* config read from here */
__pmLogCtl	logctl;
int		exit_samples = -1;       /* number of samples 'til exit */
long		exit_bytes = -1;         /* number of bytes 'til exit */
long		vol_bytes = 0;		/* total in earlier volumes */
struct timeval  exit_time;               /* time interval 'til exit */
int		vol_switch_samples = -1; /* number of samples 'til vol switch */
long		vol_switch_bytes = -1;   /* number of bytes 'til vol switch */
struct timeval	vol_switch_time;         /* time interval 'til vol switch */
int		vol_samples_counter = 0; /* Counts samples - reset for new vol*/
int		vol_switch_afid = -1;    /* afid of event for vol switch */
int		parse_done = 0;
int		primary = 0;		/* Non-zero for primary pmlogger */
char	    	*archBase;		/* base name for log files */
char		*pmcd_host = NULL;
struct timeval	epoch;
int		archive_version = PM_LOG_VERS02; /* Type of archive to create */
int		linger = 0;		/* linger with no tasks/events */
int		rflag = 0;		/* report sizes */
struct timeval	delta = { 60, 0 };	/* default logging interval */

static int 	    pmcdfd;		/* comms to pmcd */
static int	    ctx;		/* handle correspondong to ctxp below */
static __pmContext  *ctxp;		/* pmlogger has just this one context */
static fd_set	    fds;		/* file descriptors mask for select */
static int	    numfds;		/* number of file descriptors in mask */

extern struct timeval	last_stamp;
extern int		ctlfd, clientfd;

static int	rsc_fd = -1;	/* recording session control see -x */
static int	rsc_replay = 0;
static time_t	rsc_start;
static char	*rsc_prog = "<unknown>";
static char	*folio_name = "<unknown>";
static char	*dialog_title = "PCP Archive Recording Session";

extern int	errno;

void
run_done(int sts, char *msg)
{
#ifdef PCP_DEBUG
    if (msg != NULL)
    	fprintf(stderr, "pmlogger: %s, exiting\n", msg);
    else
    	fprintf(stderr, "pmlogger: End of run time, exiting\n");
#endif

    /* write the last time stamp */
    if (last_stamp.tv_sec != 0) {
	__pmTimeval	tmp;
	tmp.tv_sec = (__int32_t)last_stamp.tv_sec;
	tmp.tv_usec = (__int32_t)last_stamp.tv_usec;
	__pmLogPutIndex(&logctl, &tmp);
    }

    exit(sts);
}

/*ARGSUSED*/
static void
run_done_callback(int i, void *j)
{
  run_done(0, NULL);
}

/*ARGSUSED*/
static void
vol_switch_callback(int i, void *j)
{
  newvolume(VOL_SW_TIME);
}

static int
maxfd(void)
{
    int	max = ctlfd;
    if (clientfd > max)
	max = clientfd;
    if (pmcdfd > max)
	max = pmcdfd;
    if (rsc_fd > max)
	max = rsc_fd;
    return max;
}

/*
 * tolower_str - convert a string to all lowercase
 */
static void 
tolower_str(char *str)
{
  char *s = str;
  while(*s){
    *s = tolower(*s);
    s++;
  }
}

/*
 * ParseSize - parse a size argument given in a command option
 *
 * The size can be in one of the following forms:
 *   "40"    = sample counter of 40
 *   "40b"   = byte size of 40
 *   "40Kb"  = byte size of 40*1024 bytes = 40 kilobytes
 *   "40Mb"  = byte size of 40*1024*1024 bytes = 40 megabytes
 *   time-format = time delta in seconds
 *
 */
static int
ParseSize(char *size_arg, int *sample_counter, long *byte_size, 
          struct timeval *time_delta)
{
  double x = 0; /* the size number */
  char *ptr = NULL;

  *sample_counter = -1;
  *byte_size = -1;
  time_delta->tv_sec = -1;
  time_delta->tv_usec = -1;
  
  x = strtod(size_arg, &ptr);

  /* must be positive */
  if (x <= 0) {
    return -1;
  }

  if (*ptr == '\0') {
    /* we have consumed entire string as a long */
    /* => we have a sample counter */
    *sample_counter = x;
    return 1;
  }

  if (ptr != size_arg) {
    /* we have a number followed by something else */

    tolower_str(ptr);

    /* chomp off plurals */
    {
      int len = strlen(ptr);
      if (ptr[len-1] == 's')
        ptr[len-1] = '\0';
    }

    /* if bytes */
    if (strcmp(ptr, "b") == 0 ||
        strcmp(ptr, "byte") == 0) {
      *byte_size = (long)x;
      return 1;
    }  

    /* if kilobytes */
    if (strcmp(ptr, "k") == 0 ||
        strcmp(ptr, "kb") == 0 ||
        strcmp(ptr, "kilobyte") == 0) {
      *byte_size = (long)(x*1024);
      return 1;
    }

    /* if megabytes */
    if (strcmp(ptr, "m") == 0 ||
        strcmp(ptr, "mb") == 0 ||
        strcmp(ptr, "megabyte") == 0) {
      *byte_size = (long)(x*1024*1024);
      return 1;
    }
  }
  
  /* Doesn't fit pattern above, try a time interval */
  {
    char *interval_err;

    if (pmParseInterval(size_arg, time_delta, &interval_err) >= 0) {
      return 1;
    }
  }
  
  /* Doesn't match anything, return an error */
  return -1;  
}/*ParseSize*/

/* time manipulation */
static void
tsub(struct timeval *a, struct timeval *b)
{
    a->tv_usec -= b->tv_usec;
    if (a->tv_usec < 0) {
        a->tv_usec += 1000000;
        a->tv_sec--;
    }
    a->tv_sec -= b->tv_sec;
    if (a->tv_sec < 0) {
        /* clip negative values at zero */
        a->tv_sec = 0;
        a->tv_usec = 0;
    }
}

static char *
do_size(double d)
{
    static char nbuf[100];

    if (d < 10 * 1024)
	sprintf(nbuf, "%ld bytes", (long)d);
    else if (d < 10.0 * 1024 * 1024)
	sprintf(nbuf, "%.1f Kbytes", d/1024);
    else if (d < 10.0 * 1024 * 1024 * 1024)
	sprintf(nbuf, "%.1f Mbytes", d/(1024 * 1024));
    else
	sprintf(nbuf, "%ld Mbytes", (long)d/(1024 * 1024));
    
    return nbuf;
}

/*
 * add text identified by p to the malloc buffer at bp[0] ... bp[nchar -1]
 * return the length of the result or -1 for an error
 */
static int
add_msg(char **bp, int nchar, char *p)
{
    int		add_len;

    if (nchar < 0 || p == NULL)
	return nchar;

    add_len = (int)strlen(p);
    if (nchar == 0)
	add_len++;
    if ((*bp = realloc(*bp, nchar+add_len)) == NULL)
	return -1;
    if (nchar == 0)
	strcpy(*bp, p);
    else
	strcat(&(*bp)[nchar-1], p);

    return nchar+add_len;
}


/*
 * generate dialog/message when launching application wishes to break
 * its association with pmlogger
 *
 * cmd is one of the following:
 *	D	detach pmlogger and let it run forever
 *	Q	terminate pmlogger
 *	?	display status
 *	X	fatal error or application exit ... user must decide
 *		to detach or quit
 */
void
do_dialog(char cmd)
{
    FILE	*msgf;
    time_t	now;
    char	*msg;
    static char	lbuf[100+MAXPATHLEN];
    double	archsize;
    char	*q;
    char	*p = NULL;
    int		nchar;

    /*
     * flush archive buffers so size is accurate
     */
    fflush(logctl.l_tifp);
    fflush(logctl.l_mdfp);
    fflush(logctl.l_mfp);

    time(&now);
    now -= rsc_start;
    if (now == 0)
	/* hack is close enough! */
	now = 1;

    archsize = vol_bytes + ftell(logctl.l_mfp);

    nchar = add_msg(&p, 0, "");
    p[0] = '\0';

    sprintf(lbuf, "PCP recording for the archive folio \"%s\" and the host\n", folio_name);
    nchar = add_msg(&p, nchar, lbuf);
    sprintf(lbuf, "\"%s\" has been in progress for %d %s\n",
	pmcd_host,
	now < 240 ? now : now/60, now < 240 ? "seconds" : "minutes");
    nchar = add_msg(&p, nchar, lbuf);
    nchar = add_msg(&p, nchar, "and in that time the pmlogger process has created an\n");
    nchar = add_msg(&p, nchar, "archive of ");
    q = do_size(archsize);
    nchar = add_msg(&p, nchar, q);
    nchar = add_msg(&p, nchar, ".\n");
    if (rsc_replay) {
	nchar = add_msg(&p, nchar, "\nThis archive may be replayed with the following command:\n");
	sprintf(lbuf, "        $ pmafm %s replay\n", folio_name);
	nchar = add_msg(&p, nchar, lbuf);
    }

    if (cmd == 'D') {
	nchar = add_msg(&p, nchar, "\nThe application that launched pmlogger has asked pmlogger\n");
	nchar = add_msg(&p, nchar, "to continue independently and the PCP archive will grow at\n");
	nchar = add_msg(&p, nchar, "the rate of ");
	q = do_size((archsize * 3600) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per hour or ");
	q = do_size((archsize * 3600 * 24) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per day.\n");
    }

    if (cmd == 'X') {
	nchar = add_msg(&p, nchar, "\nThe application that launched pmlogger has exited and you\n");
	nchar = add_msg(&p, nchar, "must decide if the PCP recording session should be terminated\n");
	nchar = add_msg(&p, nchar, "or continued.  If recording is continued the PCP archive will\n");
	nchar = add_msg(&p, nchar, "grow at the rate of ");
	q = do_size((archsize * 3600) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per hour or ");
	q = do_size((archsize * 3600 * 24) / now);
	nchar = add_msg(&p, nchar, q);
	nchar = add_msg(&p, nchar, " per day.\n");
    }

    if (cmd == 'Q') {
	nchar = add_msg(&p, nchar, "\nThe application that launched pmlogger has terminated\n");
	nchar = add_msg(&p, nchar, "this PCP recording session.\n");
    }

    if (cmd != 'Q') {
	nchar = add_msg(&p, nchar, "\nAt any time this pmlogger process may be terminated with the\n");
	nchar = add_msg(&p, nchar, "following command:\n");
	nchar = add_msg(&p, nchar, "        $ kill -INT");
	sprintf(lbuf, " %d\n", getpid());
	nchar = add_msg(&p, nchar, lbuf);
    }

    if (cmd == 'X')
	nchar = add_msg(&p, nchar, "\nTerminate this PCP recording session now?\n");

    if (nchar > 0) {
	if ((msg = tmpnam(NULL)) == NULL ||
	    (msgf = fopen(msg, "w")) == NULL) {
	    fprintf(stderr, "\nError: failed create temporary message file for recording session dialog\n");
	    fprintf(stderr, "Reason? %s\n", strerror(errno));
	    goto failed;
	}
	    
	fputs(p, msgf);
	fclose(msgf);

	if (cmd == 'X')
	    sprintf(lbuf, "/usr/bin/X11/xconfirm -c -header \"%s - %s\" -file %s -icon question -B Yes -b No 2>/dev/null",
		dialog_title, rsc_prog, msg);
	else
	    sprintf(lbuf, "/usr/bin/X11/xconfirm -c -header \"%s - %s\" -file %s -icon info -b Close 2>/dev/null",
		dialog_title, rsc_prog, msg);

	if ((msgf = popen(lbuf, "r")) == NULL) {
	    fprintf(stderr, "\nError: failed to start command for recording session dialog\n");
	    fprintf(stderr, "Command: \"%s\"\n", msg);
	    goto failed;
	}

	if (fgets(lbuf, sizeof(lbuf), msgf) == NULL) {
	    fprintf(stderr, "\n%s: xconfirm(1) failed for recording session dialog\n",
		    cmd == '?' ? "Warning" : "Error");
failed:
	    fprintf(stderr, "Dialog:\n");
	    fputs(p, stderr);
	    strcpy(lbuf, "Yes");
	}
	else {
	    /* strip at first newline */
	    for (q = lbuf; *q && *q != '\n'; q++)
		;
	    *q = '\0';
	}

	pclose(msgf);
	unlink(msg);
    }
    else {
	fprintf(stderr, "Error: failed to create recording session dialog message!\n");
	fprintf(stderr, "Reason? %s\n", strerror(errno));
	strcpy(lbuf, "Yes");
    }

    free(p);

    if (cmd == 'Q' || (cmd == 'X' && strcmp(lbuf, "Yes") == 0)) {
	run_done(0, "Recording session terminated");
	/*NOTREACHED*/
    }

    if (cmd != '?') {
	/* detach, silently go off to the races ... */
	close(rsc_fd);
	FD_CLR(rsc_fd, &fds);
	rsc_fd = -1;
    }
}


int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			errflag = 0;
    char		local[MAXHOSTNAMELEN];
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*logfile = "pmlogger.log";
				    /* default log (not archive) file name */
    char		*endnum;
    int			i;
    task_t		*tp;
    optcost_t		ocp;
    extern char		*optarg;
    extern int		optind;
    extern int		pmDebug;
    extern void		init_ports(void);
    fd_set		readyfds;
    char		*p;
    char		*runtime = NULL;

    __pmSetAuthClient();

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    /*
     * Warning:
     *		If any of the pmlogger options change, make sure the
     *		corresponding changes are made to pmnewlog when pmlogger
     *		options are passed through from the control file
     */
    while ((c = getopt(argc, argv, "c:D:h:l:Ln:Prs:T:t:v:V:x:?")) != EOF) {
	switch (c) {

	case 'c':		/* config file */
	    if (access(optarg, F_OK) == 0)
		configfile = optarg;
	    else {
		/* does not exist as given, try the standard place */
		configfile = (char *)malloc(strlen(CONFIGDIR)+strlen(optarg)+1);
		if (configfile == NULL) {
		    __pmNoMem("config file name", strlen(CONFIGDIR)+strlen(optarg)+1, PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		strcpy(configfile, CONFIGDIR);
		strcat(configfile, optarg);
		if (access(configfile, F_OK) != 0) {
		    /* still no good, error handling happens below */
		    free(configfile);
		    configfile = optarg;
		}
	    }
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'h':		/* hostname for PMCD to contact */
	    pmcd_host = optarg;
	    break;

	case 'l':		/* log file name */
	    logfile = optarg;
	    break;

	case 'L':		/* linger if not primary logger */
	    linger = 1;
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'P':		/* this is the primary pmlogger */
	    primary = 1;
	    break;

	case 'r':		/* report sizes of pmResult records */
	    rflag = 1;
	    break;

	case 's':		/* exit size */
	    if (ParseSize(optarg, &exit_samples, &exit_bytes, 
		&exit_time) < 0) {
	      fprintf(stderr, "%s: illegal size argument '%s' for -s\n", 
                      pmProgname, optarg);
	      errflag++;
            }
	    if (exit_time.tv_sec > 0) {
	      __pmAFregister(&exit_time, NULL, run_done_callback);
            }
	    break;

	case 'T':		/* end time */
	    runtime = optarg;
            break;

	case 't':		/* change default logging interval */
	    if (pmParseInterval(optarg, &delta, &p) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmProgname);
		fputs(p, stderr);
		free(p);
		errflag++;
	    }
	    break;

	case 'v':		/* volume switch after given size */
	    if (ParseSize(optarg, &vol_switch_samples, &vol_switch_bytes,
                &vol_switch_time) < 0) {
	      fprintf(stderr, "%s: illegal size argument '%s' for -v\n", 
                      pmProgname, optarg);
	      errflag++;
            }
	    if (vol_switch_time.tv_sec > 0) {
	      vol_switch_afid = __pmAFregister(&vol_switch_time, NULL, 
                                           vol_switch_callback);
            }
	    break;

        case 'V': 
	    archive_version = (int)strtol(optarg, &endnum, 10);
            if (*endnum != '\0' ||
		(archive_version != PM_LOG_VERS01 &&
                 archive_version != PM_LOG_VERS02)) {
                fprintf(stderr, "%s: -V requires a version number of "
                        "%d or %d\n", pmProgname, 
                        PM_LOG_VERS01, PM_LOG_VERS02); 
		errflag++;
            }
	    break;

	case 'x':		/* recording session control fd */
	    rsc_fd = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || rsc_fd < 0) {
		fprintf(stderr, "%s: -x requires a non-negative numeric argument\n", pmProgname);
		errflag++;
	    }
	    time(&rsc_start);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr,
"Usage: %s [options] archive\n\
\n\
Options:\n\
  -c configfile file to load configuration from\n\
  -h host	metrics source is PMCD on host\n\
  -l logfile	redirect diagnostics and trace output\n\
  -L		linger, even if not primary logger instance and nothing to log\n\
  -n pmnsfile   use an alternative PMNS\n\
  -P		execute as primary logger instance\n\
  -r		report record sizes and archive growth rate\n\
  -s endsize	terminate after endsize has been accumulated\n\
  -t interval   default logging interval [default 60.0 seconds]\n\
  -T endtime	terminate at given time\n\
  -v volsize	switch log volumes after volsize has been accumulated\n\
  -V version    generate version 1 or 2 archives (default is 2)\n\
  -x fd		control file descriptor for application launching pmlogger\n\
		via pmRecordControl(3)\n",
			pmProgname);
	exit(1);
    }

    if (primary && pmcd_host != NULL) {
	fprintf(stderr, "%s: -P and -h are mutually exclusive ... use -P only when running\n%s on the same (local) host as the PMCD to which it connects.\n", pmProgname, pmProgname);
	exit(1);
    }

    __pmOpenLog("pmlogger", logfile, stderr, &sts);

    /* base name for archive is here ... */
    archBase = argv[optind];

    if (pmcd_host == NULL || strcmp(pmcd_host, "localhost") == 0) {
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	pmcd_host = local;
    }

    /* initialise access control */
    if (__pmAccAddOp(OP_LOG_ADV) < 0 ||
	__pmAccAddOp(OP_LOG_MAND) < 0 ||
	__pmAccAddOp(OP_LOG_ENQ) < 0) {
	fprintf(stderr, "%s: access control initialisation failed\n", pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, pmcd_host)) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, pmcd_host, pmErrStr(ctx));
	exit(1);
    }

    /*
     * discover fd for comms channel to PMCD ... 
     */
    ctxp = __pmHandleToPtr(ctx);
    pmcdfd = ctxp->c_pmcd->pc_fd;

    if (configfile != NULL) {
	if ((fconfig = fopen(configfile, "r")) == NULL) {
	    fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmProgname, configfile, strerror(errno));
	    exit(1);
	}
    }
    else {
	fconfig = stdin;
	configfile = "<stdin>";
    }

    __pmOptFetchGetParams(&ocp);
    ocp.c_scope = 1;
    __pmOptFetchPutParams(&ocp);

    /* prevent early timer events ... */
    __pmAFblock();

    if (yyparse() != 0)
	exit(1);
    fclose(fconfig);

#ifdef PCP_DEBUG
    fprintf(stderr, "Config parsed\n");
#endif

    fprintf(stderr, "Starting %slogger for host \"%s\"\n",
	primary ? "primary " : "", pmcd_host);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	char	*p;
	fprintf(stderr, "optFetch Cost Parameters: pmid=%d indom=%d fetch=%d scope=%d\n",
		ocp.c_pmid, ocp.c_indom, ocp.c_fetch, ocp.c_scope);

	fprintf(stderr, "\nAfter loading config ...\n");
	for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	    fprintf(stderr, " state: %sin log, %savail, %s, %s",
		PMLC_GET_INLOG(tp->t_state) ? "" : "not ",
		PMLC_GET_AVAIL(tp->t_state) ? "" : "un",
		PMLC_GET_MAND(tp->t_state) ? "mand" : "adv",
		PMLC_GET_ON(tp->t_state) ? "on" : "off");
	    fprintf(stderr, " delta: %d msec", 1000 * tp->t_delta.tv_sec + tp->t_delta.tv_usec);
	    fprintf(stderr, " numpmid: %d\n", tp->t_numpmid);
	    for (i = 0; i < tp->t_numpmid; i++) {
		sts = pmNameID(tp->t_pmidlist[i], &p);
		if (sts < 0)
		    fprintf(stderr, "  %s (%s):", pmIDStr(tp->t_pmidlist[i]), "<noname>");
		else {
		    fprintf(stderr, "  %s (%s):", pmIDStr(tp->t_pmidlist[i]), p);
		    free(p);
		}
		putc('\n', stderr);
	    }
	    __pmOptFetchDump(stderr, tp->t_fetch);
	}
    }
#endif

    if (!primary && tasklist == NULL && !linger) {
	fprintf(stderr, "Nothing to log, and not the primary logger instance ... good-bye\n");
	exit(1);
    }

    if ((sts = __pmLogCreate(pmcd_host, archBase, archive_version, &logctl)) < 0) {
	fprintf(stderr, "__pmLogCreate: %s\n", pmErrStr(sts));
	exit(1);
    }
    else {
	/*
	 * try and establish $TZ from the remote PMCD ...
	 * Note the label record has been set up, but not written yet
	 */
	char		*name = "pmcd.timezone";
	pmID		pmid;
	pmResult	*resp;

#if defined(sgi)
	gettimeofday(&epoch, NULL, NULL);
#else
	gettimeofday(&epoch, NULL);
#endif
	sts = pmUseContext(ctx);
	if (sts >= 0)
	    sts = pmLookupName(1, &name, &pmid);
	if (sts >= 0) {
	    sts = pmFetch(1, &pmid, &resp);
	}
	if (sts >= 0 && resp->vset[0]->numval > 0) {
	    strcpy(logctl.l_label.ill_tz, resp->vset[0]->vlist[0].value.pval->vbuf);
	    /* prefer to use remote time to avoid clock drift problems */
	    epoch = resp->timestamp;		/* struct assignment */
	    pmFreeResult(resp);
	    pmNewZone(logctl.l_label.ill_tz);
        }
#ifdef PCP_DEBUG
    	else if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, 
			"main: Could not get timezone from host %s\n",
			pmcd_host);
	}
#endif
    }

    /* do ParseTimeWindow stuff for -T */
    if (runtime) {
        struct timeval res_end;    /* time window end */
        struct timeval start;
        struct timeval end;
        struct timeval last_delta;
        char *err_msg;             /* parsing error message */
        time_t now;
        struct timeval now_tv;

        time(&now);
        now_tv.tv_sec = now;
        now_tv.tv_usec = 0; 

        start = now_tv;
        end.tv_sec = INT_MAX;
        end.tv_usec = INT_MAX;
        sts = __pmParseTime(runtime, &start, &end, &res_end, &err_msg);
        if (sts < 0) {
	    fprintf(stderr, "%s: illegal -T argument\n%s", pmProgname, err_msg);
            exit(1);
        }

        last_delta = res_end;
        tsub(&last_delta, &now_tv);
	__pmAFregister(&last_delta, NULL, run_done_callback);

        last_stamp = res_end;
    }

    fprintf(stderr, "Archive basename: %s\n", archBase);

#ifdef MALLOC_AUDIT
    _malloc_reset_();
#if defined(HAVE_ATEXIT)
    atexit(_malloc_audit_);
#endif
#endif

    /* detach yourself from the launching process */
    setpgid(getpid(), 0);

    /* set up control port */
    init_ports();
    FD_ZERO(&fds);
    FD_SET(ctlfd, &fds);
    FD_SET(pmcdfd, &fds);
    if (rsc_fd != -1)
	FD_SET(rsc_fd, &fds);
    numfds = maxfd() + 1;

    if ((sts = do_preamble()) < 0)
	fprintf(stderr, "Warning: problem writing archive preamble: %s\n",
	    pmErrStr(sts));

    sts = 0;		/* default exit status */

    parse_done = 1;	/* enable callback processing */
    __pmAFunblock();

#if defined(__NUTC__)
    /* kick-start the AF itimer for the "log on once" case */
    kill(getpid(), SIGALRM);
#endif

    for ( ; ; ) {
	int		nready;
	extern int	control_req(void);
	extern int	client_req(void);

	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(numfds, &readyfds, NULL, NULL, NULL);

	if (wantflush) {
	    /*
	     * flush request via SIGUSR1
	     */
	    fflush(logctl.l_tifp);
	    fflush(logctl.l_mdfp);
	    fflush(logctl.l_mfp);
	    wantflush = 0;
	}

	if (nready > 0) {
	    /* block signals to simplify IO handling */
	    __pmAFblock();

	    /* handle request on control port */
	    if (FD_ISSET(ctlfd, &readyfds)) {
		if (control_req()) {
		    /* new client has connected */
		    FD_SET(clientfd, &fds);
		    if (clientfd >= numfds)
			numfds = clientfd + 1;
		}
	    }
	    if (clientfd >= 0 && FD_ISSET(clientfd, &readyfds)) {
		/* process request from client, save clientfd in case client
		 * closes connection, resetting clientfd to -1
		 */
		int	fd = clientfd;

		if (client_req()) {
		    /* client closed connection */
		    FD_CLR(fd, &fds);
		    numfds = maxfd() + 1;
		}
	    }
	    if (pmcdfd >= 0 && FD_ISSET(pmcdfd, &readyfds)) {
		/*
		 * do not expect this, given synchronous commumication with the
		 * pmcd ... either pmcd has terminated, or bogus PDU ...
		 */
		__pmPDU		*pb;
		__pmPDUHdr	*php;
		sts = __pmGetPDU(pmcdfd, PDU_BINARY, TIMEOUT_NEVER, &pb);
		if (sts <= 0) {
		    if (sts < 0)
			fprintf(stderr, "Error: __pmGetPDU: %s\n", pmErrStr(sts));
		    disconnect(sts);
		    /*NOTREACHED*/
		}
		else {
		    php = (__pmPDUHdr *)pb;
		    fprintf(stderr, "Error: Unsolicited %s PDU from PMCD\n",
			__pmPDUTypeStr(php->type));
		    disconnect(PM_ERR_IPC);
		    /*NOTREACHED*/
		}
	    }
	    if (rsc_fd >= 0 && FD_ISSET(rsc_fd, &readyfds)) {
		/*
		 * some action on the recording session control fd
		 * end-of-file means launcher has quit, otherwise we
		 * expect one of these commands
		 *	V<number>\n	- version
		 *	F<folio>\n	- folio name
		 *	P<name>\n	- launcher's name
		 *	R\n		- launcher can replay
		 *	D\n		- detach from launcher
		 *	Q\n		- quit pmlogger
		 */
		char	rsc_buf[MAXPATHLEN];
		char	*rp = rsc_buf;
		char	myc;
		int	fake_x = 0;

		for (rp = rsc_buf; ; rp++) {
		    if (read(rsc_fd, &myc, 1) <= 0) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL2)
			    fprintf(stderr, "recording session control: eof\n");
#endif
			if (rp != rsc_buf) {
			    *rp = '\0';
			    fprintf(stderr, "Error: incomplete recording session control message: \"%s\"\n", rsc_buf);
			}
			fake_x = 1;
			break;
		    }
		    if (rp >= &rsc_buf[MAXPATHLEN]) {
			fprintf(stderr, "Error: absurd recording session control message: \"%100.100s ...\"\n", rsc_buf);
			fake_x = 1;
			break;
		    }
		    if (myc == '\n') {
			*rp = '\0';
			break;
		    }
		    *rp = myc;
		}

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL2) {
		    if (fake_x == 0)
			fprintf(stderr, "recording session control: \"%s\"\n", rsc_buf);
		}
#endif

		if (fake_x)
		    do_dialog('X');
		else if (strcmp(rsc_buf, "Q") == 0 ||
		         strcmp(rsc_buf, "D") == 0 ||
			 strcmp(rsc_buf, "?") == 0)
		    do_dialog(rsc_buf[0]);
		else if (rsc_buf[0] == 'F')
		    folio_name = strdup(&rsc_buf[1]);
		else if (rsc_buf[0] == 'P')
		    rsc_prog = strdup(&rsc_buf[1]);
		else if (strcmp(rsc_buf, "R") == 0)
		    rsc_replay = 1;
		else if (rsc_buf[0] == 'V' && rsc_buf[1] == '0') {
		    /*
		     * version 0 of the recording session control ...
		     * this is all we grok at the moment
		     */
		    ;
		}
		else {
		    fprintf(stderr, "Error: illegal recording session control message: \"%s\"\n", rsc_buf);
		    do_dialog('X');
		}
	    }

	    __pmAFunblock();
	}
	else if (nready < 0 && errno != EINTR)
	    perror("select");
    }

    /*NOTREACHED*/
}


int
newvolume(int vol_switch_type)
{
    FILE	*newfp;
    int		nextvol = logctl.l_curvol + 1;
    time_t	now;
    static char *vol_sw_strs[] = {
       "SIGHUP", "pmlc request", "sample counter",
       "sample byte size", "sample time"
    };

    vol_samples_counter = 0;
    vol_bytes += ftell(logctl.l_mfp);
    if (exit_bytes != -1) {
        if (vol_bytes >= exit_bytes) 
	    run_done(0, "Byte limit reached");
    }

    /* 
     * If we are not part of a callback but instead a 
     * volume switch from "pmlc" or a "SIGHUP" then
     * get rid of pending volume switch in event queue
     * as it will now be wrong, and schedule a new
     * volume switch event.
     */
    if (vol_switch_afid >= 0 && vol_switch_type != VOL_SW_TIME) {
      __pmAFunregister(vol_switch_afid);
      vol_switch_afid = __pmAFregister(&vol_switch_time, NULL,
                                   vol_switch_callback);
    }

    if ((newfp = __pmLogNewFile(archBase, nextvol)) != NULL) {
	if (logctl.l_state == PM_LOG_STATE_NEW) {
	    /*
	     * nothing has been logged as yet, force out the label records
	     */
	    gettimeofday(&last_stamp, NULL);
	    logctl.l_label.ill_start.tv_sec = (__int32_t)last_stamp.tv_sec;
	    logctl.l_label.ill_start.tv_usec = (__int32_t)last_stamp.tv_usec;
	    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
	    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);
	    logctl.l_label.ill_vol = PM_LOG_VOL_META;
	    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
	    logctl.l_label.ill_vol = 0;
	    __pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
	    logctl.l_state = PM_LOG_STATE_INIT;
	}
#if 0
	if (last_stamp.tv_sec != 0) {
	    __pmTimeval	tmp;
	    tmp.tv_sec = (__int32_t)last_stamp.tv_sec;
	    tmp.tv_usec = (__int32_t)last_stamp.tv_usec;
	    __pmLogPutIndex(&logctl, &tmp);
	}
#endif
	fclose(logctl.l_mfp);
	logctl.l_mfp = newfp;
	logctl.l_label.ill_vol = logctl.l_curvol = nextvol;
	__pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
	fflush(logctl.l_mfp);
	time(&now);
	fprintf(stderr, "New log volume %d, via %s at %s",
		nextvol, vol_sw_strs[vol_switch_type], ctime(&now));
	return nextvol;
    }
    else
	return -errno;
}


void
disconnect(int sts)
{
    time_t  now;

    time(&now);
    if (sts != 0)
	fprintf(stderr, "%s: Error: %s\n", pmProgname, pmErrStr(sts));
    fprintf(stderr, "%s: Lost connection to PMCD on \"%s\" at %s",
	    pmProgname, ctxp->c_pmcd->pc_name, ctime(&now));
#if CAN_RECONNECT
    if (primary) {
	fprintf(stderr, "This is fatal for the primary logger.");
	exit(1);
    }
    close(pmcdfd);
    FD_CLR(pmcdfd, &fds);
    pmcdfd = -1;
    numfds = maxfd() + 1;
    ctxp->c_pmcd->pc_fd = -1;
#else
    exit(1);
#endif
}

#if CAN_RECONNECT
int
reconnect(void)
{
    int	    sts;

    sts = pmReconnectContext(ctx);
    if (sts >= 0) {
	time_t      now;
	time(&now);
	fprintf(stderr, "%s: re-established connection to PMCD on \"%s\" at %s\n",
		pmProgname, ctxp->c_pmcd->pc_name, ctime(&now));
	pmcdfd = ctxp->c_pmcd->pc_fd;
	FD_SET(pmcdfd, &fds);
	numfds = maxfd() + 1;
    }
    return sts;
}
#endif


