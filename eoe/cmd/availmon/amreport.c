/*
 * amreport.c
 *
 * availmon local/site log viewing system
 */

#ident "$Revision: 1.23 $"

/*
 * To do list:
 *
 * 1. a better data structure for event_t, eventname, shorteventname
 *    and event code should be defined so that the handling routine
 *    can be simpler and easy to read.
 *
 *    one good advantage is that event code does not have special
 *    ordering, but event_t can have special order for easy processing
 *
 * 2. statname and availstat_t's nstat, stuptime, stdntime is another
 *    ugly implementation of processing and showing stats.  A better
 *    data structure should be defined.
 *
 * 3. since registration and deregistration are events, there is no
 *    need to have regist in arm_t.  replace regist with bad or duplicate
 *    for bad or duplicate reports (currently use regist).
 *
 * 4. if -f and/or -t are defined, total elapsed time, uptime, and
 *    downtime should be changed accordingly.  the stats shown are
 *    based on the epoches included.
 *
 * 5. currently, -f/-t and resend can be used for locallog only. it
 *    would be better to enhance it for sitelog also.
 *
 * 6. currently, diag info is not shown because it is too lengthy.
 *    it would be nice to be able to handle multiple-page display.
 *
 * 7. amreport used curses, so it is for dump terminal.  if necessary,
 *    it can have a gui option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <curses.h>
#include <term.h>
#include <fcntl.h>
#include <signal.h>
#include <values.h>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "defs.h"

#include <sys/syssgi.h>

#define MAXHOSTS	1024 /* no. of distinct hosts that can be monitored */
#define LINELEN		256
#define DASH		'-'
#define EQUALS		'='
#define PIPE		'|' /* the field separator for availlog records */
#define REPORT_WIDTH	79
#define NREASONS	7
#define MAXEVENTCODE	26
#define MINEVENTCODE	-19
#define NSTATS		16
#define PAGERTYPE	6

typedef enum {
    /* --- */	ev_unknown,
    /* -19 */   ev_livenotification,
    /* -18 */	ev_systemid_change,
    /* -17 */	ev_pwer_cycle,
    /* -16 */	ev_reset,
    /* -15 */	ev_nmi,
    /* -14 */	ev_software_panic,
    /* -13 */	ev_status_report,
    /* -12 */	ev_software_error,
    /* -11 */	ev_hardware_error,
    /* -10 */	ev_no_error,
    /*  -9 */	ev_registration,
    /*  -8 */	ev_deregistration,
    /*  -7 */	ev_power_failure,
    /*  -6 */	ev_system_off,
    /*  -5 */	ev_interrupt,
    /*  -4 */	ev_hardware_panic,
    /*  -3 */	ev_panic,
    /*  -2 */	ev_reboot_unconfigured,
    /*  -1 */	ev_reboot_timeout,
    /*   0 */	ev_reboot_unknown,
    /*   1 */	ev_reboot_environment,
    /*   2 */	ev_reboot_upgrade_hw,
    /*   3 */	ev_reboot_replace_hw,
    /*   4 */	ev_reboot_install_sw,
    /*   5 */	ev_reboot_scheduled,
    /*   6 */	ev_reboot_sw_problem,
    /*   7 */	ev_reboot_other,
    /*   8 */	ev_reboot_pad1,
    /*   9 */	ev_reboot_pad2,
    /*  10 */	ev_reboot_pad3,
    /*  11 */	ev_reboot_admin,
    /*  12 */	ev_reboot_hw_upgrade,
    /*  13 */	ev_reboot_sw_upgrade,
    /*  14 */	ev_reboot_fix_hw,
    /*  15 */	ev_reboot_install_patch,
    /*  16 */	ev_reboot_fix_sw,
    /*  17 */	ev_reboot_pad4,
    /*  18 */	ev_reboot_pad5,
    /*  19 */	ev_single_timeout,
    /*  20 */	ev_single_unconfigured,
    /*  21 */	ev_single_admin,
    /*  22 */	ev_single_hw_upgrade,
    /*  23 */	ev_single_sw_upgrade,
    /*  24 */	ev_single_fix_hw,
    /*  25 */	ev_single_install_patch,
    /*  26 */	ev_single_fix_sw,
    /* --- */	ev_num
} event_t;

typedef struct {
    /* int	majorversion;	* version of Avail mon */
    /* int	minorversion;	* version of Avail mon */
    time_t 	prevstarttime;
    time_t 	eventtime;
    time_t 	starttime;
    time_t     	lasttick;
    int		noprevstart;
    int 	uptime;
    int 	dntime;
    int		bound;
    event_t	event;
    int		regist;		/* should get rid of this hack */
    int		sysidlen;
    char	*sysid;
    int		hostnamelen;
    char 	*hostname;
    int		summarylen;
    char	*summary;
} amr_t;

typedef struct {
    char	*sysid;
    char	*hostname;
    int		nev[ev_num];
    int		nsr[ev_num];
    int		evuptime[ev_num];
    int		evdntime[ev_num];
    int		nstat[NSTATS];
    int		stuptime[NSTATS];
    int		stdntime[NSTATS];
    int		unschedules;
    int		unschedulesuptime;
    int		unschedulesdntime;
    int		sruptime[ev_num];
    int		srdntime[ev_num];
    int		service;
    int		serviceuptime;
    int		servicedntime;
    int		upleast;	/* in minutes */
    int		upmost;		/* in minutes */
    int		dnleast;	/* in minutes */
    int		dnmost;		/* in minutes */
    int		thisepoch;	/* in minutes */
    int		total;		/* in minutes, should get rid of this one */
				/* because total is the same as stuptime[15] */
    int		possibleuptime;	/* in minutes */
    time_t	lastboot;	/* since 1970 */
    time_t	thebeginning;	/* since 1970 */
    int		nepochs;
    int		ndowns;
} availstat_t;

char	cmd[MAX_STR_LEN];
char	*tfile = NULL;
char	file[MAXPATHLEN];

amr_t	*amrs; 			/* space will be dynamically allocated */
int	*amrmarks;		/* amr marks */
int	statmarks[MAXHOSTS+1];	/* stat marks */
amr_t	*samrs[MAXHOSTS+1]; 	/* an array of pointers into amrs, which mark
		    		 *  the start position of the  amrs array
				 *  sorted by hostname */
int		nsamrs; 	/* no. of sorted groups */
availstat_t	availstats[MAXHOSTS+1];
time_t		now;

/*
 * following used for options processing
 */
int	sitelog = 0;
int	locallog = 0;
int	printversion = 0;
int	printflag = 0;
int	statflag = 0;
int	eventlistflag = 0;
int	detailflag = 0;
time_t	fromtime = 0, totime = 0;
char	fromdate[MAX_STR_LEN] = "", todate[MAX_STR_LEN] = "";
int	countthisepoch = 1;
int	singlereport = 0;
int	listmachines = 0;
int	dupepochs = 0;
char	machinename[MAXHOSTS+1];

int	systemidlen;
char	systemid[MAXSYSIDSIZE + 1];
int	lhostnamelen;
char	lhostname[MAXHOSTNAMELEN+1];
int	nreports;
char	*cfile = NULL;

char	*usage =
	"Usage: %s [{ -l <locallogfile> | -s <sitelogfile> }] [-c]\n"
	"       %s [{ -l <locallogfile> | -s <sitelogfile> }] [-p] [-r] [-e] [-d] [-c]\n"
	"       %s [-l <locallogfile>] [-f <fromtime>] [-t <totime>] [-e] [-d] [-c]\n"
	"       %s { -1 | -v }\n";

int	errmsg = -1;
int	outistty = 1;		/* zero if output is a not a tty */

char	*eventname[] = {
	/* --- */  "Unknown reasons",
	/* -19 */  "Real-time Notification of critical Messages",
	/* -18 */  "Serial number/hostname change",
	/* -17 */  "Power cycle",
	/* -16 */  "System reset",
	/* -15 */  "NMI",
	/* -14 */  "Panic due to software fault",
	/* -13 */  "Status report",
	/* -12 */  "Software error",
	/* -11 */  "Hardware error",
	/* -10 */  "No error",
	/*  -9 */  "Registration",
	/*  -8 */  "Deregistration",
	/*  -7 */  "Power failure",
	/*  -6 */  "System off",
	/*  -5 */  "Interrupt",
	/*  -4 */  "Panic due to hardware fault",
	/*  -3 */  "Panic",
	/*  -2 */  "Controlled shutdown: shutdown reason unknown (not configured)",
	/*  -1 */  "Controlled shutdown: timed-out while waiting for operator to enter stop reason",
	/*   0 */  "Controlled shutdown: system shutdown, cause unknown",
	/*   1 */  "Controlled shutdown: environmental reasons (move system, planned power outage, ...)",
	/*   2 */  "Controlled shutdown: upgrade/add hardware or peripherals",
	/*   3 */  "Controlled shutdown: replace failed hardware component",
	/*   4 */  "Controlled shutdown: install software that requires reboot",
	/*   5 */  "Controlled shutdown: scheduled reboot/Routine maintenance",
	/*   6 */  "Controlled shutdown: software problem",
	/*   7 */  "Controlled shutdown: other unclassified reasons",
	/*   8 */  "",
	/*   9 */  "",
	/*  10 */  "",
	/*  11 */  "Controlled shutdown: administrative",
	/*  12 */  "Controlled shutdown: hardware upgrade",
	/*  13 */  "Controlled shutdown: software upgrade",
	/*  14 */  "Controlled shutdown: fix/replace hardware",
	/*  15 */  "Controlled shutdown: install patch",
	/*  16 */  "Controlled shutdown: fix software problem",
	/*  17 */  "",
	/*  18 */  "",
	/*  19 */  "Single-user mode: timed-out while waiting for operator to enter stop reason",
	/*  20 */  "Single-user mode: shutdown reason unknown (not configured)",
	/*  21 */  "Single-user mode: administrativ",
	/*  22 */  "Single-user mode: hardware upgrade",
	/*  23 */  "Single-user mode: software upgrade",
	/*  24 */  "Single-user mode: fix/replace hardware",
	/*  25 */  "Single-user mode: install patch",
	/*  26 */  "Single-user mode: fix software problem",
	/* --- */  ""
};

char	*shorteventname[] = {
	/* --- */  "Unknown",
	/* -19 */  "Live Event",
	/* -18 */  "SysID change",
	/* -17 */  "Power cycle",
	/* -16 */  "System reset",
	/* -15 */  "NMI",
	/* -14 */  "Panic (S/W)",
	/* -13 */  "Status report",
	/* -12 */  "Software error",
	/* -11 */  "Hardware error",
	/* -10 */  "No error",
	/*  -9 */  "Registration",
	/*  -8 */  "Deregistration",
	/*  -7 */  "Power failure",
	/*  -6 */  "System off",
	/*  -5 */  "Interrupt",
	/*  -4 */  "Panic (H/W)",
	/*  -3 */  "Panic",
	/*  -2 */  "Controlled",
	/*  -1 */  "Controlled",
	/*   0 */  "Controlled",
	/*   1 */  "Controlled",
	/*   2 */  "Controlled",
	/*   3 */  "Controlled",
	/*   4 */  "Controlled",
	/*   5 */  "Controlled",
	/*   6 */  "Controlled",
	/*   7 */  "Controlled",
	/*   8 */  "",
	/*   9 */  "",
	/*  10 */  "",
	/*  11 */  "Controlled",
	/*  12 */  "Controlled",
	/*  13 */  "Controlled",
	/*  14 */  "Controlled",
	/*  15 */  "Controlled",
	/*  16 */  "Controlled",
	/*  17 */  "",
	/*  18 */  "",
	/*  19 */  "Single-User",
	/*  20 */  "Single-User",
	/*  21 */  "Single-User",
	/*  22 */  "Single-User",
	/*  23 */  "Single-User",
	/*  24 */  "Single-User",
	/*  25 */  "Single-User",
	/*  26 */  "Single-User",
	/* --- */  ""
};

char	*statname[] = {
	"Unscheduled ....................",
	"  panic due to hardware ........",
	"  panic due to software ........",
	"  panic due to unknown reason ..",
	"  reset action .................",
	"  power interruption ...........",
	"Service action .................",
	"  fix/replace hardware .........",
	"  upgrade hardware .............",
	"  upgrade software .............",
	"  fix software .................",
	"  install patch ................",
	"  administrative: singe-user ...",
	"  administrative: reboot .......",
	"  unknown ......................",
	"Total .........................."
};

/*
 * Forward declarations.
 */
void	getlocalreports(FILE *, amr_t *);
void	getsitereports(FILE *, amr_t *);
void	collect_stats(amr_t *amrs, int n, availstat_t *s);
char	*extracthostname(char *iname, char *hname);
void	prettyshow_centered(char *s, char c, int spaces);
void	aggregatestats(availstat_t *as, int n, availstat_t *result,
		       char *upleasthost, char *upmosthost,
		       char *dnleasthost, char *dnmosthost);
void	sort_by_hosts(amr_t *amrs, int *n);
int	getword(char *line, int wno, char *word);
void	fillhostinfo(amr_t *amr, int hostlen, char *host);
char	getchoice(char *prompt, char *choices);
void	showstatline(availstat_t *s);
void	showstat(availstat_t *s);
void	showamrline(amr_t *amr);
char	showamrs(int begin, int end);
void	showamr(amr_t *amr);
void	sendamrs(int begin, int end);
void	sendamr(amr_t *amr);
char	*pretty_minutes(int m, char *s);
void	prettyprint_centered(char *s, char c, int spaces);
void	printstatlist(int all);
void	printstatline(availstat_t *s);
void	printstat(availstat_t *s);
void	printamrlist(int mach);
void	printamrline(amr_t *amr);
void	printamrs(int mach);
void	printamr(amr_t *amr);


void
errorexit(char *s, char *msg)
{
    if (msg)
	fprintf(stderr, "%s:%s\n", s, msg);
    else
	perror(s);

    if (tfile) unlink(tfile);
    exit(-1);
}

void
repaint()
{
    clearok(curscr,TRUE);
    wrefresh(curscr);
}

int
getkey()
{
    int c;

    while ((c = getch()) == 12)
	repaint();

    return c;
}

void
getret()
{
    int c;

    do {
	c = getkey();
	printw("[%o]<%c>\n", c, c);
    } while (c != '\n' && c != KEY_ENTER);
}

/*
 * copy gtime from touch.c originally,
 * but then feel it is too ugly, so rewrite it
 */
time_t
gtime(char *date)
{
    int		i, y, t;
    int		d, h, m;
    size_t	len;
    struct tm	tm;
    long	nt;
    int		c = 0;		/* entered century value */
    int		s = 0;		/* entered seconds value */
    int		point = 0;	/* relative byte position of decimal point */
    int		ssdigits = 0;	/* count seconds digits include decimal pnt */
    int		noyear = 1;	/* 0 means year is entered; 1 means none */

    /*
     * mmddhhmm is a total of 8 bytes min
     * [[cc]yy]mmddhhmm[.ss] is a total of 15 bytes max
     */
    len = strlen(date);
    if ((len < 8) || (len > 15))
	return(-1);
    /*
     * determine the decimal point position, if any.
     * check if there are more than one decimal point.
     * check if all the rest are digits.
     */
    for (i=0; i<len; i++) {
	if (date[i] == '.') {
	    if (point) {
		return(-1);
	    } else {
		point = i;
	    }
	} else if (!isdigit(date[i]))
	    return(-1);
    }
    switch(len) {
    case 8:
    case 10:
    case 12:
	if (point)
	    return(-1);
	tm.tm_sec = 0;
	break;
    case 11:
    case 13:
    case 15:
	if (!point || ((len - point) != 3))
	    return(-1);
	t = (date[point + 1] - '0') * 10 + date[point + 2] - '0';
	if (t > 61)
	    return(-1);
	else
	    tm.tm_sec = t;
	date[point] = '\0';
	len -= 3;
	break;
    default:
	return(-1);
    }

    if (len == 12) {
	/*
	 * ccyymmddhhmm is the input
	 */
	y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
	    (date[2] - '0') * 10 + date[3] - '0';
	if ((y < 1969) || (y > 2068))
	    return(-1);
	tm.tm_year = y - 1900;
	date += 4;
	len -= 4;
    } else if (len == 10) {
	/*
	 * yymmddhhmm is the input
	 */
	/* detemine yy -- year    number */
	y = (date[0] - '0') * 10 + date[1] - '0';
	if (y >= 69)
	    tm.tm_year = y;		/* 19th century */
	else
	    tm.tm_year = y + 100;	/* 20th century */
	date += 2;
	len -= 2;
    } else {
	(void) time(&nt);
	 tm.tm_year = localtime(&nt)->tm_year;
    }

    t = (date[0] - '0') * 10 + date[1] - '0';
    if (t > 12)
	return(-1);
    else
	tm.tm_mon = t - 1;
    t = (date[2] - '0') * 10 + date[3] - '0';
    if (t > 31)
	return(-1);
    else
	tm.tm_mday = t;
    t = (date[4] - '0') * 10 + date[5] - '0';
    if(t > 24)
	return(-1);
    else
	tm.tm_hour = t;
    t = (date[6] - '0') * 10 + date[7] - '0';
    if (t > 59)
	return(-1);
    else
	tm.tm_min = t;
    tm.tm_isdst = -1;
    return(mktime(&tm));
}

void
processoptions(int argc, char **argv)
{
    int c;
    int errflg = 0;

    /* process online options */
    while ((c = getopt(argc, argv, "1cdeprvl:s:f:t:M")) != EOF)
	switch (c) {
	case '1':
	    singlereport = 1;
	    break;
	case 'M':
	    listmachines = 1 ;
	    if ((optind == argc) || (argv[optind][0] == '-')) {
		/* no file was specified */ 
		strcpy(machinename, "");
	    } else {
		strcpy(machinename, argv[optind]);
		optind++;
	    }
	    break;
	case 's':
	    sitelog = 1;
	    if (locallog) {
		errflg++;
		break;
	    }
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else {
		strcpy(file, optarg);
	    }
	    break;
	case 'l':
	    locallog = 1;
	    if (sitelog) {
		errflg++;
		break;
	    }
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else {
		strcpy(file, optarg);
	    }
	    break;
	case 'f':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else {
		strcpy(fromdate, optarg);
		if ((fromtime = gtime(fromdate)) == -1) {
		    fprintf(stderr, "Error: %s: incorrect time in -f option.\n", cmd);
		    exit(-1);
		}
	    }
	    break;
	case 't':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else {
		strcpy(todate, optarg);
		if ((totime = gtime(todate)) == -1) {
		    fprintf(stderr, "Error: %s: incorrect time in -t option.\n", cmd);
		    exit(-1);
		}
	    }
	    break;
	case 'c':
	    countthisepoch = 0;
	    break;
	case 'p':
	    printflag = statflag = eventlistflag = 1;
	    break;
	case 'r':
	    printflag = statflag = 1;
	    break;
	case 'e':
	    printflag = eventlistflag = 1;
	    break;
	case 'd':
	    printflag = detailflag = 1;
	    break;
	case 'v':
	    printversion = 1;
	    break;
	case '?':
	default:
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, usage, cmd, cmd, cmd, cmd);
	exit(-1);
    } else if (!locallog && !sitelog && !singlereport) {
	/* no option was given, default to -l /var/adm/avail/availlog */
	locallog = 1;
	strcpy(file, "/var/adm/avail/availlog");
	return;
    }
    if (fromtime && fromtime > now) {
	fprintf(stderr, "Error: %s: bad -f time (later than current time).\n",
		cmd);
	exit(-1);
    }
    if (totime && totime > now) {
	fprintf(stderr, "Error: %s: bad -t time (later than current time).\n",
		cmd);
	exit(-1);
    }
    if (fromtime && totime && (fromtime >= totime)) {
	fprintf(stderr, "Error: %s: bad -f and -t time specifications.\n", cmd);
	exit(-1);
    }
}

void
printlist()
{
    int i;

    printf("\n\n");
    printf("%-35s %-20s\n", "Machine Name", "No. of reports");
    printf("%-35s %-20s\n", "------------", "--------------");
    for (i = 0; i < nsamrs; i++)
	printf("%-40s %d\n", samrs[i]->hostname, samrs[i+1] - samrs[i]);
    printf("\n\n");
}

main(int argc, char *argv[])
{
    FILE	*fp, *fp1;
    char	linebuf[MAX_LINE_LEN];
    int		i, j;
    char	hostname[MAXHOSTNAMELEN+1];
    amr_t	*amr, amr1;
    struct termio tty;

    outistty = (ioctl(1, TCGETA, &tty) == 0);
    strcpy(cmd, basename(argv[0]));
    now = time(0);
    /* sets file & format */
    processoptions(argc, argv);

    if (printversion) {
	printf("Availmon version %s\n", AMRVERSIONNUM);
	return(0);
    }
    if (singlereport) {
	if (sitelog || locallog) {
	    fprintf(stderr, usage, cmd, cmd, cmd, cmd);
	    exit(-1);
	}
	getsitereports(stdin, &amr1);
	amr1.dntime = (amr1.starttime - amr1.prevstarttime + 30)/60
		      - amr1.uptime;
	printamr(&amr1);
	return(0);
    }

    fprintf(stderr, "Using %s format with file %s ... \n",
	    (locallog ? "local log" : "site log"), file);
    if (listmachines)
	fprintf(stderr, "Listing machine %s ... \n", machinename);

    if ((fp = fopen(file, "r")) == NULL)
	errorexit(file, NULL);

    fprintf(stderr, "Counting Reports ... ");
    nreports = countreports(fp);
    fprintf(stderr, "done.\n");

    if (nreports <= 0) {
	fprintf(stderr, "%s: no reports gathered (is %s in %s format?)\n",
		cmd, file, locallog? "local log": "site log");
	exit(-1);
    }
    fprintf(stderr, "Total of %d reports.\n", nreports);

    if ((amrs = (amr_t *) calloc(nreports + 1, sizeof(amr_t))) == NULL)
	errorexit("calloc", "can't allocate space for amr's");

    if ((amrmarks = (int *) calloc(nreports + 1, sizeof(int))) == NULL)
	errorexit("calloc", "can't allocate space for amr marks");

    fprintf(stderr, "Reading reports ...");
    if (locallog) {
	if (fp1 = fopen("/var/adm/avail/.save/serialnum", "r")) {
	    fscanf(fp1, "%s", systemid);
	    fclose(fp1);
	} else if (fp1 = fopen("/var/adm/avail/.save/sysid", "r")) {
	    fscanf(fp1, "%s", systemid);
	    fclose(fp1);
	} else {
	    strcpy(systemid, "unknown");
	}
	systemidlen = strlen(systemid);
	fillhostinfo(NULL, lhostnamelen, lhostname);

	getlocalreports(fp, amrs);
	fprintf(stderr, "done.\n");
	if (nreports == 0) {
	    fprintf(stderr, "%s: no reports gathered (check -f/-t option)\n",
		    cmd);
	    exit(-1);
	}
	nsamrs = 1;
	samrs[0] = &amrs[0];
	samrs[1] = &amrs[nreports];
    } else {
	getsitereports(fp, amrs);
	fprintf(stderr, "done.\nSorting reports ...");
	sort_by_hosts(amrs, &nreports);
	fprintf(stderr, "done.\n");
    }

    if (listmachines) {
	if (machinename[0] == NULL) {
	    printlist();
	    exit(0);
	} else {
	    for (j = 0; j < nsamrs; j++)
		if (strcmp(extracthostname(samrs[j]->hostname,
					   hostname), machinename) == 0)
		    break;
	    if (j == nsamrs) {
		printf("Cannot find <%s>\n", machinename);
		printlist();
		exit(-1);
	    }
	    i = samrs[j] - &amrs[0];
	    nreports = i + samrs[j+1] - samrs[j];
	}
    } else 
	i = 0;

    if (errmsg && printflag == 0) {
	printf("errmsg=%d\n", errmsg);
	printf("Press <enter> to display summary ... ");
	fgets(linebuf, MAX_LINE_LEN, stdin);
    }

    if (printflag == 0) {
	/*
	 * enter curse display session
	 */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	if (LINES < 23 || COLS < 80) {
	    endwin();
	    fprintf(stderr, "Error: %s: This program needs a screen size at"
		    " least 23 by 80.\n", cmd);
	    exit(1);
	}
    }

    if (locallog) {
	collect_stats(amrs, nreports, &availstats[0]);
	availstats[0].sysid = systemid;

	if (printflag) {
	    if (statflag)
		printstatlist(0);
	    if (eventlistflag)
		printamrlist(0);
	    if (detailflag)
		printamrs(0);
	} else {
	    statmarks[0] = 1;
	    showstats();
	}
    } else {
	char	upleasthost[MAXHOSTNAMELEN+1], upmosthost[MAXHOSTNAMELEN+1];
	char	dnleasthost[MAXHOSTNAMELEN+1], dnmosthost[MAXHOSTNAMELEN+1];
	int	k, dup = 0;

	for (i = 0; i < nsamrs; i++) {
	    collect_stats(samrs[i],
			  samrs[i+1] - samrs[i],  /* pointer arithmetic */
			  &availstats[i]);
	}
	aggregatestats(availstats, nsamrs, &availstats[nsamrs],
		       upleasthost, upmosthost, dnleasthost, dnmosthost);

	for (i = 0, j = 0; i < nreports; i++)
	    if (amrs[i].regist == 1) {
		dup = 1;
		for (k = 0; k < nsamrs; k++)
		    if (samrs[k] > &amrs[j])
			break;
		for (; k <= nsamrs; k++)
		    samrs[k]--;
	    } else if (dup)
		amrs[j++] = amrs[i];
	    else
		j++;
	nreports = j;

	if (printflag) {
	    if (statflag)
		printstatlist(1);
	    if (eventlistflag)
		for (i = 0; i < nsamrs; i++)
		    printamrlist(i);
	    if (detailflag)
		for (i = 0; i < nsamrs; i++)
		    printamrs(i);
	} else
	    showoverall(upleasthost, upmosthost, dnleasthost, dnmosthost);
    }

    if (printflag == 0) {
	/*
	 * exit curse display session
	 */
	endwin();
	printf("\n");
    }
    if (cfile)
	unlink(cfile);
}

char *
extracthostname(char *iname, char *hname)
{
    char	*p;
    int		len;

    if ((p = strchr(iname, '.')) == NULL)
	p = iname + strlen(iname);
    len = p - iname;
    bcopy(iname, hname, len);
    hname[len] = NULL;
    return(hname);
}

int
countreports(FILE *fp)
{
    int nreports, bad= 1, t;
    char linebuf[MAX_LINE_LEN];
    char word[LINELEN], word1[LINELEN];

    nreports = 0;
    if (locallog) {
	while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL)
	    if ((strncmp("EVENT|", linebuf, 6) == 0) ||
		(strncmp("STOP|", linebuf, 5) == 0))
		nreports++;
	if (nreports == 0) {
	    time_t insttime;

	    if (fseek(fp, 0L, SEEK_SET) != 0)
		errorexit("fseek error", "when rewinding file");
	    fgets(linebuf, MAX_LINE_LEN, fp);
	    if (getword(linebuf, 1, word) == -1)
		errorexit("getword error", "when extracting installation time");
	    insttime = atol(word);
	    printf("\nNo event recorded since installation of availmon at %s",
		   ctime(&insttime));
	    printf("(100%% Availability)\n");
	    exit(0);
	}
    } else {
	while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL)
	    if ((strncmp("SERIALNUM|", linebuf, 10) == 0) ||
		(strncmp("SYSID|", linebuf, 6) == 0))
		nreports++;
    }
    return nreports;
}

void
initamr(amr_t *amr, int local)
{
    amr->prevstarttime = 0;
    amr->noprevstart = 0;
    amr->eventtime = -1;
    amr->event = ev_interrupt;
    amr->lasttick = -1;
    amr->uptime = -1;
    amr->dntime = 0;
    amr->bound = -1;
    amr->regist = 0;
    amr->summarylen = 0;
    amr->summary = NULL;
    if (local) {
	amr->sysidlen = systemidlen;
	amr->sysid = systemid;
	amr->hostnamelen = lhostnamelen;
	amr->hostname = lhostname;
    } else {
	amr->hostnamelen = 0;
	amr->hostname = NULL;
    }
}

void
getlocalreports(FILE *fp, amr_t *amr)
{
    int		bad = 1, inputsum = 0, t, t1, sumlen = 0, haslivediag = 0;
    int		garbage = 0, nrpts = 0;
    char	linebuf[MAX_LINE_LEN], linebuf1[MAX_LINE_LEN], sumbuf[BUFSIZ];
    char	word[MAX_LINE_LEN], word1[MAX_LINE_LEN];
    amr_t	*amr1;

    rewind(fp);

    sumbuf[0] = '\0';
    /*
     * skipped events before from date
     */
    if (fromtime) {
	while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL)
	    if (strncmp("START|", linebuf, 6) == 0) {
		if (getword(linebuf, 1, word) == -1) {
		    bad = 1;
		    fprintf(stderr, "Error: %s: cannot get time in START:\n%s",
			    cmd, linebuf);
		    continue;
		} else {
		    if (bad) {
			errmsg++;
			bad = 0;
		    }
		    strcpy(linebuf1, linebuf);
		    t = atol(word);
		    if (t > fromtime)
			goto start;
		}
	    } else if (bad) {
		continue;
	    } else if (strncmp("EVENT|", linebuf, 6) == 0) {
		if (getword(linebuf, 2, word1) == -1) {
		    bad = 1;
		    fprintf(stderr, "Error: %s: cannot get time in EVENT:\n%s",
			    cmd, linebuf);
		    continue;
		}
		t1 = atol(word1);
		if (t1 > fromtime) {
		    initamr(amr, 1);
		    amr->prevstarttime = t;
		    sumlen = 0;
		    sumbuf[0] = '\0';
		    inputsum = 0;
		    garbage = 0;
		    haslivediag = 0;
		    goto start1;
		}
	    } else if (strncmp("STOP|", linebuf, 5) == 0) {
		if (getword(linebuf, 1, word1) == -1) {
		    bad = 1;
		    fprintf(stderr, "Error: %s: cannot get time in STOP:\n%s",
			    cmd, linebuf);
		    continue;
		}
		t1 = atol(word1);
		if (t1 > fromtime) {
		    initamr(amr, 1);
		    amr->prevstarttime = t;
		    sumlen = 0;
		    sumbuf[0] = '\0';
		    inputsum = 0;
		    garbage = 0;
		    haslivediag = 0;
		    goto start1;
		}
	    }
    }

    while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL)
	if (strncmp("START|", linebuf, 6) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in START:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    if (bad) {
		errmsg++;
		bad = 0;
	    } else {
		amr->starttime = t;
		t1 = amr->event;
		if (((t1 <= ev_deregistration) && (t1 >= ev_status_report)) ||
		    (t1 == ev_systemid_change) || (t1 == ev_livenotification))
		    amr->dntime = 0;
		else
		    amr->dntime = (amr->starttime - amr->prevstarttime + 30)/60
				  - amr->uptime;
		if (sumlen) {
		    amr->summary = malloc(sumlen + 1);
		    strcpy(amr->summary, sumbuf);
		    amr->summarylen = sumlen;
		}
		amr++;
		nrpts++;
	    }
	start:
	    if (totime && (totime < t))
		goto end;

	    initamr(amr, 1);
	    amr->prevstarttime = t;
	    sumlen = 0;
	    sumbuf[0] = '\0';
	    inputsum = 0;
	    garbage = 0;
	    haslivediag = 0;
	} else if (bad) {
	    continue;
	} else if (strncmp("SUMMARY|", linebuf, 8) == 0) {
	    inputsum = !inputsum;
	} else if (inputsum) {
	    t = strlen(linebuf);
	    if (sumlen + t < BUFSIZ) {
		strcat(sumbuf, linebuf);
		sumlen += t;
	    }
	} else if (strncmp("EVENT|", linebuf, 6) == 0) {
	start1:
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get event code in EVENT:\n%s",
			cmd, linebuf);
		continue;
	    }
	    if (getword(linebuf, 2, word1) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in EVENT:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    t1 = atol(word1);
	    if (haslivediag) {
		if (sumlen) {
		    amr->summary = malloc(sumlen + 1);
		    strcpy(amr->summary, sumbuf);
		    amr->summarylen = sumlen;
		}
		amr1 = amr;
		amr++;
		nrpts++;
		initamr(amr, 1);
		if (amr1->event >= ev_single_timeout) {
		    amr->noprevstart = 1;
		    amr1->starttime = t1;
		    amr->prevstarttime = t1;
		} else {
		    amr->noprevstart = 0;
		    amr->prevstarttime = amr1->prevstarttime;
		}
		sumlen = 0;
		sumbuf[0] = '\0';
		inputsum = 0;
	    }
	    if (totime && (totime < t1))
		goto end;
	    if (t > MAXEVENTCODE || t < MINEVENTCODE)
		t = amr->event = ev_unknown;
	    else
		t = amr->event = (event_t)(t - MINEVENTCODE + 1);
	    if (((t <= ev_deregistration) && (t >= ev_status_report)) ||
		(t == ev_systemid_change) || (t == ev_livenotification)) {
		haslivediag = 1;
		amr->uptime = 0;
		amr->noprevstart = 1;
	    } else if (t >= ev_single_timeout)
		haslivediag = 1;
	    else
		haslivediag = 0;
	    amr->eventtime = t1;
	    if ((t >= ev_reboot_unconfigured) || (t == ev_panic) ||
		(t == ev_software_panic) || ( t == ev_hardware_panic)) {
		if (amr->eventtime > 0)
		    amr->uptime = (amr->eventtime - amr->prevstarttime + 30)/60;
	    }
	} else if (strncmp("STOP|", linebuf, 5) == 0) {
	    if (getword(linebuf, 2, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get stop code in STOP:\n%s",
			cmd, linebuf);
		continue;
	    }
	    if (getword(linebuf, 1, word1) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in STOP:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    t1 = atol(word1);
	    if (haslivediag) {
		if (sumlen) {
		    amr->summary = malloc(sumlen + 1);
		    strcpy(amr->summary, sumbuf);
		    amr->summarylen = sumlen;
		}
		amr1 = amr;
		amr++;
		nrpts++;
		initamr(amr, 1);
		if (amr1->event >= ev_single_timeout) {
		    amr->noprevstart = 1;
		    amr1->starttime = t1;
		    amr->prevstarttime = t1;
		} else {
		    amr->noprevstart = 0;
		    amr->prevstarttime = amr1->prevstarttime;
		}
		sumlen = 0;
		sumbuf[0] = '\0';
		inputsum = 0;
	    }
	    if (totime && (totime < t1))
		goto end;
	    if (t > MAXEVENTCODE || t < MINEVENTCODE)
		t = amr->event = ev_unknown;
	    else
		t = amr->event = (event_t)(t - MINEVENTCODE + 1);
	    if (((t <= ev_deregistration) && (t >= ev_status_report)) ||
		(t == ev_systemid_change) || (t == ev_livenotification)) {
		haslivediag = 1;
		amr->uptime = 0;
		amr->noprevstart = 1;
	    } else if (t >= ev_single_timeout) {
		haslivediag = 1;
	    } else
		haslivediag = 0;
	    amr->eventtime = t1;
	    if ((t >= ev_reboot_unconfigured) || (t == ev_panic) ||
		(t == ev_software_panic) || ( t == ev_hardware_panic)) {
		if (amr->eventtime > 0)
		    amr->uptime = (amr->eventtime - amr->prevstarttime + 30)/60;
	    }
	} else if (strncmp("UPTIME|", linebuf, 7) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get time in UPTIME:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    if (t >= 0)
		amr->lasttick = amr->prevstarttime + t * 60;
	    if (amr->uptime < 0) {
		amr->uptime = t;
	    }
	} else if (strncmp("LASTTICK|", linebuf, 9) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get time in UPTIME:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    amr->lasttick = t;
	    if (amr->uptime < 0) {
		amr->uptime = (t - amr->prevstarttime + 30) / 60;
	    }
	} else if (strncmp("BOUND|", linebuf, 6) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get bound in BOUND:\n%s",
			cmd, linebuf);
		continue;
	    }
	    amr->bound = atol(word);

	/*
	 * Moved garbage check from before UPTIME to here since the value
	 * of LASTTICK is not being read-in if the garbage flag is set
	 */

	} else if (garbage) {
	    continue;
	} else if (strncmp("UNAME|", linebuf, 6) == 0) {
	    continue;
	} else if (strncmp("VERSIONS|", linebuf, 9) == 0) {
	    garbage = 1;
	} else if (strncmp("HINV|", linebuf, 5) == 0) {
	    garbage = 1;
	} else if (strncmp("GFXINFO|", linebuf, 8) == 0) {
	    garbage = 1;
	} else if (strncmp("REGISTRATION|", linebuf, 13) == 0) {
	    amr->regist = 1;
	} else if (strncmp("DEREGISTRATION|", linebuf, 15) == 0) {
	    amr->regist = -1;
	} else if (strncmp("OLDSERIALNUM|", linebuf, 13) == 0) {
	    continue;
	} else if (strncmp("OLDHOSTNAME|", linebuf, 12) == 0) {
	    continue;
	} else {
	    errmsg++;
	    fprintf(stderr, "Error: %s: garbage in local log file:\n%s",
		    cmd, linebuf);
	}
end:
    if (bad)
	errmsg++;
    else if (sumlen) {
	amr->summary = malloc(sumlen + 1);
	strcpy(amr->summary, sumbuf);
	amr->summarylen = sumlen;
    }
    nreports = nrpts;
    fclose(fp);
}

void
getsitereports(FILE *fp, amr_t *amr)
{
    int		linenum = 0, oldsysid = 0;
    int		bad = 1, inputsum = 0, t, len, sumlen = 0, garbage = 0;
    char	linebuf[MAX_LINE_LEN], sumbuf[BUFSIZ];
    char	word[MAX_LINE_LEN], word1[MAX_LINE_LEN];

    rewind(fp);

    sumbuf[0] = '\0';
    while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL) {
	linenum++;
	if ((strncmp("SYSID|", linebuf, 6) == 0) ||
	    (strncmp("SERIALNUM|", linebuf, 10) == 0)) {
	    if (bad) {
		errmsg++;
		bad = 0;
	    } else {
		if (singlereport)
		    return;
		t = amr->event;
		if (((t <= ev_deregistration) && (t >= ev_status_report)) ||
		    (t == ev_systemid_change) || (t == ev_livenotification))
		    amr->dntime = 0;
		else
		    amr->dntime = (amr->starttime - amr->prevstarttime + 30)/60
				  - amr->uptime;
		if (sumlen) {
		    amr->summary = malloc(sumlen + 1);
		    strcpy(amr->summary, sumbuf);
		    amr->summarylen = sumlen;
		}
		amr++;
	    }
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get serial number:\n%s",
			cmd, linebuf);
		continue;
	    }
	    initamr(amr, 0);
	    len = strlen(word);
	    amr->sysidlen = len;
	    amr->sysid = malloc(len + 1);
	    strcpy(amr->sysid, word);
	    sumlen = 0;
	    sumbuf[0] = '\0';
	    inputsum = 0;
	    garbage = 0;
	    oldsysid = 1;
	} else if (bad) {
	    continue;
	} else if (strncmp("SUMMARY|", linebuf, 8) == 0) {
	    inputsum = !inputsum;
	} else if (inputsum) {
	    len = strlen(linebuf);
	    if (sumlen + len < BUFSIZ) {
		strcat(sumbuf, linebuf);
		sumlen += len;
	    }
	} else if (garbage) {
	    continue;
	} else if (strncmp("HOSTNAME|", linebuf, 9) == 0) {
	    len = strlen(linebuf);
	    linebuf[len - 1] = '\0';
	    amr->hostnamelen = len - 9;
	    amr->hostname = malloc(len - 9);
	    strcpy(amr->hostname, linebuf + 9);
	    oldsysid = 0;
	} else if (strncmp("AMRVERSION|", linebuf, 11) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get version number in AMRVERSION:\n%s",
			cmd, linebuf);
		continue;
	    }
	} else if (strncmp("PREVSTART|", linebuf, 10) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in PREVSTART:\n%s",
			cmd, linebuf);
		continue;
	    } else if (strstr(linebuf, "|NOTMULTIUSER"))
		amr->noprevstart = 1;
	    amr->prevstarttime = atol(word);
	} else if (strncmp("EVENT|", linebuf, 6) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get event code in EVENT:\n%s",
			cmd, linebuf);
		continue;
	    }
	    if (getword(linebuf, 2, word1) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in EVENT:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    if (t > MAXEVENTCODE || t < MINEVENTCODE)
		t = amr->event = ev_unknown;
	    else
		t = amr->event = (event_t)(t - MINEVENTCODE + 1);
	    if (((t <= ev_deregistration) && (t >= ev_status_report)) ||
		(t == ev_systemid_change) || (t == ev_livenotification)) {
		amr->uptime = 0;
		amr->noprevstart = 1;
	    }
	    amr->eventtime = atol(word1);
 	    if ((t >= ev_reboot_unconfigured) || (t == ev_panic) ||
		(t == ev_software_panic) || ( t == ev_hardware_panic) ||
		(t == ev_deregistration)) {
		if (amr->eventtime > 0)
		    amr->uptime = (amr->eventtime - amr->prevstarttime + 30)/60;
	    }
	} else if (strncmp("STOP|", linebuf, 5) == 0) {
	    if (getword(linebuf, 2, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get stop code in STOP:\n%s",
			cmd, linebuf);
		continue;
	    }
	    if (getword(linebuf, 1, word1) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in STOP:\n%s",
			cmd, linebuf);
		continue;
	    }
	    t = atol(word);
	    if (t > MAXEVENTCODE || t < MINEVENTCODE)
		t = amr->event = ev_unknown;
	    else
		t = amr->event = (event_t)(t - MINEVENTCODE + 1);
	    if (((t <= ev_deregistration) && (t >= ev_status_report)) ||
		(t == ev_systemid_change) || (t == ev_livenotification)) {
		amr->uptime = 0;
		amr->noprevstart = 1;
	    }
	    amr->eventtime = atol(word1); 
 	    if ((t >= ev_reboot_unconfigured) || (t == ev_panic) ||
		(t == ev_software_panic) || ( t == ev_hardware_panic) ||
		(t == ev_deregistration)) {
		if (amr->eventtime > 0)
		    amr->uptime = (amr->eventtime - amr->prevstarttime + 30)/60;
	    }
	} else if (strncmp("UPTIME|", linebuf, 7) == 0) {
	    if (amr->uptime < 0) {
		if (getword(linebuf, 1, word) == -1) {
		    fprintf(stderr, "Error: %s: cannot get time in UPTIME:\n%s",
			    cmd, linebuf);
		    continue;
		}
		amr->uptime = atol(word);
	    }
	} else if (strncmp("LASTTICK|", linebuf, 9) == 0) {
	    if (amr->uptime < 0) {
		if (getword(linebuf, 1, word) == -1) {
		    fprintf(stderr, "Error: %s: cannot get time in UPTIME:\n%s",
			    cmd, linebuf);
		    continue;
		}
		t = atol(word);
		amr->uptime = (t - amr->prevstarttime + 30) / 60;
	    }
	} else if (strncmp("START|", linebuf, 6) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		bad = 1;
		fprintf(stderr, "Error: %s: cannot get time in START:\n%s",
			cmd, linebuf);
		continue;
	    }
	    amr->starttime = atol(word);
	} else if (strncmp("REGISTRATION|", linebuf, 13) == 0) {
	    amr->regist = 1;
	    if (amr->prevstarttime == 0)
		amr->prevstarttime = amr->starttime;
	} else if (strncmp("DEREGISTRATION|", linebuf, 15) == 0) {
	    amr->regist = -1;
	    amr->starttime = amr->eventtime;
	} else if (strncmp("BOUNDS|", linebuf, 7) == 0) {
	    if (getword(linebuf, 2, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get bound in BOUND:\n%s",
			cmd, linebuf);
		continue;
	    }
	    amr->bound = atol(word);
	} else if (strncmp("STATUSINTERVAL|", linebuf, 15) == 0) {
	    if (getword(linebuf, 1, word) == -1) {
		fprintf(stderr, "Error: %s: cannot get status interval in STATUSINTERVAL:\n%s",
			cmd, linebuf);
		continue;
	    }
	} else if (strncmp("AMRVERSION|", linebuf, 11) == 0) {
	    continue;
	} else if (strncmp("UNAME|", linebuf, 6) == 0) {
	    continue;
	} else if (strncmp("STATUSINTERVAL|", linebuf, 15) == 0) {
	    continue;
	} else if (strncmp("OLDSERIALNUM|", linebuf, 13) == 0) {
	    continue;
	} else if (strncmp("OLDHOSTNAME|", linebuf, 12) == 0) {
	    continue;
	} else if (strncmp("VERSIONS|", linebuf, 9) == 0) {
	    garbage = 1;
	} else if (strncmp("HINV|", linebuf, 5) == 0) {
	    garbage = 1;
	} else if (strncmp("GFXINFO|", linebuf, 8) == 0) {
	    garbage = 1;
	} else if (strncmp("ICRASH|", linebuf, 7) == 0) {
	    garbage = 1;
	} else if (strncmp("SYSLOG|", linebuf, 7) == 0) {
	    garbage = 1;
	} else if (oldsysid && (*linebuf == 'K')) {
	    continue;
	} else {
	    errmsg++;
	    garbage = 1;
	    fprintf(stderr, "Error: %s: garbage in site log file:\nline %d:%s",
		    cmd, linenum, linebuf);
	}
    }

    if (bad)
	errmsg++;
    else {
	amr->dntime = (amr->starttime - amr->prevstarttime + 30)/60
	    - amr->uptime;
	if (sumlen) {
	    amr->summary = malloc(sumlen + 1);
	    strcpy(amr->summary, sumbuf);
	    amr->summarylen = sumlen;
	}
    }
    fclose(fp);
}

/*
 * words are numbered 0,1,2 and are in format 0|1|2\n (vertical bar is
 * the field separator)
 */
int
getword(char *line, int wno, char *word)
{
    int i = 0;
    char *p, *p1;

    if (wno) {
	i = wno;
	for (p = line; *p; p++)
	    if (*p == '|')
		if (--i == 0)
		    break;
    }
    if (i)
	return(-1);	/* not enough fields */
    p1 = ++p;
    for (; *p; p++)
	if (*p == '|') {
	    *p = NULL;
	    strcpy(word, p1);
	    *p = PIPE;
	    return(0);
	}
    if (*(--p) == '\n') {
	    *p = NULL;
	    strcpy(word, p1);
	    *p = '\n';
    } else
	strcpy(word, p1);
    return(0);
}

/*
 * old getword implementation
 */
int
getword1(char *line, int wno, char *word)
{
    int i;
    char *p1, *p2;

    for (i = 0, p1 = line; i < wno; i++)
	if ((p2 = strchr(p1, PIPE)) == NULL)
	    break;
	else
	    p1 = ++p2; /* start looking from next field */

    if (i != wno)
	return -1;
    /*
     * we have looked at wno-1 separators (wno-1 words),  p1 points
     * at beginnig of wno'th word; find end of wno'th word 
     */
    if ((p2 = strchr(p1, PIPE)) == NULL) { /* was the last word in line */
	strcpy(word, p1);
	i = strlen(word);
	word[i - 1] = '\0';	/* get rid of '\n' */
    } else {
	*p2 = NULL; /* will repair after copying */
	strcpy(word, p1);
	*p2 = PIPE;
    }
    return 0;
}

void
fillhostinfo(amr_t *amr, int hostlen, char *host)
{
    struct hostent *hent;
    char	hostname[MAXHOSTNAMELEN + 1];
    char	**p;
    int		i;

    if (gethostname (hostname, MAXHOSTNAMELEN) != 0) {
	perror("gethostname");
	exit(-1);
    }
    i = 0;
    while (1) { 
	hent = gethostbyname(hostname);
	if (hent != NULL)
	    break;
	if (h_errno == TRY_AGAIN) {	
	    printf("TRY_AGAIN for gethostbyname()\n");
	    i++;
	    if (i > 3) {
		herror(hostname);
		errorexit(hostname, "Failed after 3 tries");
	    }
	    continue;
	} else { /* some irrecorverable error */
	    herror(hostname);
	    exit(-1);
	}
    }

    if (amr) {
	amr->hostnamelen = strlen(hent->h_name) + 1;
	amr->hostname = malloc(amr->hostnamelen);
	strcpy(amr->hostname, hent->h_name);
    } else {
	hostlen = strlen(hent->h_name) + 1;
	strcpy(host, hent->h_name);
    }
}

int
compare_by_hostname(const void *amr1, const void *amr2)
{
    return(strcmp(((amr_t *) amr1)->hostname, ((amr_t *) amr2)->hostname));
}

int
compare_by_prev_starts(const void *amr1, const void *amr2)
{
    if (((amr_t *) amr1)->prevstarttime < ((amr_t *) amr2)->prevstarttime)
	return -1;
    else if (((amr_t *) amr1)->prevstarttime > ((amr_t *) amr2)->prevstarttime)
	return 1;
    else if (((amr_t *) amr1)->starttime < ((amr_t *) amr2)->starttime)
	return -1;
    else if (((amr_t *) amr1)->starttime > ((amr_t *) amr2)->starttime)
	return 1;
    else if (((amr_t *) amr1)->regist == 2)
	return -1;
    else if (((amr_t *) amr2)->regist == 2)
	return 1;
    else {
	fprintf(stderr, "Warning: Duplicate records with the same prestart"
		" time and start time");
	printamr((amr_t *) amr1);
	printamr((amr_t *) amr2);
	((amr_t *) amr1)->regist = 2;
	dupepochs++;
	return 0;
    }
}

void
setindex(amr_t *amrs, int n)
{
    int		i, j;

    for (i = 0, j = 1; i < n - 1; i++)
	if (strcmp(amrs[i].hostname, amrs[i+1].hostname) != 0)
	    samrs[j++] = &amrs[i+1];
    samrs[j] = &amrs[n]; /* this pointer is not valid, but marks just past
			  * the end of the amrs array
			  */
    nsamrs = j;
}

void
sort_by_hosts(amr_t *amrs, int *n)
{
    int		i, j, dup;
    amr_t	*b, *e;

    qsort(amrs, *n, sizeof(amr_t), compare_by_hostname);
    samrs[0] = &amrs[0];
    setindex(amrs, *n);

    /* make sure that records for a given host are sorted */
    for (i = 0; i < nsamrs;  i++)
	qsort(samrs[i], samrs[i+1] - samrs[i] /* pointer arithmetic */,	
	      sizeof(amr_t), compare_by_prev_starts);

    if (dupepochs) {
	dup = 0;
	for (i = 0, j = 0; i < *n; i++)
	    if (amrs[i].regist == 2)
		dup = 1;
	    else if (dup)
		amrs[j++] = amrs[i];
	    else
		j++;
	*n = j;
	setindex(amrs, *n);
    }
}

char
getchoice(char *prompt, char *choices)
{
    char	c;

    if (!outistty) 	/* non-interactive invocation */
	return(choices[0]);

    printw("\n\n");
    while (1) {
	printw("%s [%s] ? ", prompt, choices);
	c = getkey();
	if (c == '\n' || c == ' ')
	    return(choices[0]);
	if (strchr (choices, c))
	    return c;
	beep();
    }
}

void
initstat(availstat_t *s)
{
    int		i;

    for (i = 0; i < ev_num; i++)
	s->nev[i] = s->evuptime[i] = s->evdntime[i] = 0;
    for (i = 0; i < NSTATS; i++)
	s->nstat[i] = s->stuptime[i] = s->stdntime[i] = 0;
    s->total = 0;
    s->upleast = MAXINT;
    s->upmost = 0;
    s->dnleast = MAXINT;
    s->dnmost = 0;
    s->thisepoch = 0;
    s->possibleuptime = 0;
    s->nepochs = 0;
}


void
collect_stats(amr_t *amrs, int n, availstat_t *s)
{
    int		i, u = 0, d = 0, ev, offtime = 0, t, pre = -1, lessdntime = 0;
    FILE	tmp;
    amr_t	*amr;

    initstat(s);
    s->sysid = amrs[0].sysid;
    s->hostname = amrs[0].hostname;
    for (i = 0; i < n; i++) {
	ev = amrs[i].event;
	s->nev[ev]++;
	if (ev <= ev_registration && ev >= ev_status_report)
	    continue;
	if (pre >= 0) {
	    if (amrs[i].regist <= 0) {
		if (amrs[i].prevstarttime < amrs[pre].starttime)
		    offtime += amrs[i].prevstarttime - amrs[pre].starttime;
	    } else if (amrs[i].regist == 1) {
		if (amrs[pre].regist == -1) {
		    offtime += amrs[i].prevstarttime - amrs[pre].eventtime;
		} else {
		    fprintf(stderr, "Error: re-registration without "
			    "deregistration first.\n");
		    tmp = *stdout;
		    *stdout = *stderr;
		    printamr(&amrs[i]);
		    *stdout = tmp;
		    if (amrs[i].prevstarttime < amrs[pre].starttime)
			offtime += amrs[i].prevstarttime - amrs[pre].starttime;
		}
		pre = -1;
		continue;
	    } else 	/* if (amrs[i].regist == 2) ---> duplicate */
		continue;
	} else if (amrs[i].regist > 0)
	    continue;
	if (amrs[i].uptime < 0) {
	    t = amrs[i].starttime - amrs[i].prevstarttime - 60;
	    amrs[i].uptime = (t < 30) ? 0 : ((t + 30) / 60);
	    amrs[i].dntime = 1;
	}
	if (amrs[i].regist < 0)
	    amrs[i].dntime = 0;
	if (amrs[i].regist <= 0) {
	    u++;
	    s->evuptime[ev] += amrs[i].uptime;
	    s->total += amrs[i].uptime;
	    if (amrs[i].uptime < s->upleast)
		s->upleast = amrs[i].uptime;
	    if (amrs[i].uptime > s->upmost)
		s->upmost = amrs[i].uptime;
	}
	if (amrs[i].regist >= 0) {
	    d++;
	    s->evdntime[ev] += amrs[i].dntime;
	    if (amrs[i].dntime < s->dnleast)
		s->dnleast = amrs[i].dntime;
	    if (amrs[i].dntime > s->dnmost)
		s->dnmost = amrs[i].dntime;
	}
	pre = i;
    }

    /*
     * Fix for Bug # 668431 (sri), amreport shows bogus numbers :
     *
     *  Problem : If the last event recorded is a live event which
     *            doesn't have a START time, then, the value of 
     *            s->lastboot is being set to 0.  This is resulting
     *            in a huge time value (Jan 1 1970).
     *  Fix     : Added a check to see if the last event recorded
     *            is a live event.  In such cases, we take the
     *            PREVSTART value as s->lastboot.
     *            Since the event is a live event, machine actually
     *            booted up at PREVSTART time.
     *  Testing : Tested it on customer provided availlog files and
     *            some of my own combinations.
     *   
     */

    if (amrs[n-1].regist >= 0) {
	s->lastboot = amrs[n-1].starttime;
	if ( s->lastboot <= 0 ) {
	    switch(amrs[n-1].event) {
		case ev_registration:
		    if ( amrs[n-1].eventtime >= 0 ) {
			s->lastboot = amrs[n-1].eventtime;
		    } else { 
			s->lastboot = amrs[n-1].prevstarttime;
		    } 
		    break;
		case ev_livenotification:
		case ev_no_error:
		case ev_hardware_error:
		case ev_software_error:
		case ev_status_report:
		    s->lastboot = amrs[n-1].prevstarttime;
		    break;
	    }
	}
    } else {
	s->lastboot = amrs[n-1].eventtime;
    }

    amr = &amrs[0];
    if (fromtime && fromtime > amr->prevstarttime) {
	s->thebeginning = fromtime;
	if (amr->eventtime > 0) {
	    if (fromtime > amr->eventtime) {
		s->evdntime[amr->event] -=
		    (fromtime - amr->eventtime + 30) / 60;
		s->evuptime[amr->event] -= amr->uptime;
		s->total -= amr->uptime;
	    } else {
		s->evuptime[amr->event] -=
		    (fromtime - amr->prevstarttime + 30) / 60;
		s->total -= (fromtime - amr->prevstarttime + 30) / 60;
	    }
	} else if (fromtime - amr->prevstarttime > amr->uptime * 60) {
	    s->evdntime[amr->event] -=
		(fromtime - amr->prevstarttime + 30) / 60 - amr->uptime;
	    s->evuptime[amr->event] -= amr->uptime;
	    s->total -= amr->uptime;
	} else {
	    s->evuptime[amr->event] -=
		(fromtime - amr->prevstarttime + 30) / 60;
	    s->total -= (fromtime - amr->prevstarttime + 30) / 60;
	}
    } else {
	s->thebeginning = amr->prevstarttime;
    }

    amr = &amrs[n - 1];
    if (totime) {
	if (amr->regist < 0) {
	    if (amr->eventtime < totime) {
		s->possibleuptime = (s->lastboot - s->thebeginning - offtime
					+ 30) / 60;
		s->nepochs = u;
	    } else {
		s->possibleuptime = (totime - s->thebeginning - offtime + 30)
				    / 60;
		s->nepochs = u;
	    }
	} else {
	    s->possibleuptime = (totime - s->thebeginning - offtime + 30) / 60;
	    s->nepochs = u;
	    if (amr->eventtime > 0) {
		if (totime > amr->eventtime) {
		    s->evdntime[amr->event] -=
			(amr->starttime - totime + 30) / 60;
		} else {
		    s->evdntime[amr->event] -= amr->dntime;
		    s->evuptime[amr->event] -=
			(amr->eventtime - totime + 30) / 60;
		    s->total -= (amr->eventtime - totime + 30) / 60;
		}
	    } else if (totime - amr->prevstarttime > amr->uptime * 60) {
		s->evdntime[amr->event] -= (amr->starttime - totime + 30) / 60;
	    } else {
		s->evdntime[amr->event] -= amr->dntime;
		s->evuptime[amr->event] -=
		    amr->uptime - (totime - amr->prevstarttime + 30) / 60;
		s->total -=
		    amr->uptime - (totime - amr->prevstarttime + 30) / 60;
	    }
	}
    } else if (countthisepoch && amr->regist >= 0) {
	s->thisepoch = (now - s->lastboot + 30)/60;
	if (amr->regist > 0)
	    amr->uptime = s->thisepoch;
	s->total += s->thisepoch;
	if (s->thisepoch < s->upleast)
	    s->upleast = s->thisepoch;
	if (s->thisepoch > s->upmost)
	    s->upmost = s->thisepoch;
	s->nepochs = u + 1; /* +1 for thisepoch */
	s->possibleuptime = (now - s->thebeginning - offtime + 30) / 60;
    } else {
	s->possibleuptime = (s->lastboot - s->thebeginning - offtime + 30) / 60;
	s->nepochs = u;
    }
    s->ndowns = d;

    s->nstat[1] = s->nev[ev_hardware_panic];
    s->nstat[2] = s->nev[ev_software_panic];
    s->nstat[3] = s->nev[ev_panic];
    s->nstat[4] = s->nev[ev_interrupt] + s->nev[ev_system_off] + s->nev[ev_nmi]
		+ s->nev[ev_reset] + s->nev[ev_pwer_cycle];
    s->nstat[5] = s->nev[ev_power_failure];
    s->nstat[0] = s->nstat[1] + s->nstat[2] + s->nstat[3] + s->nstat[4]
		+ s->nstat[5];
    s->nstat[7] = s->nev[ev_reboot_replace_hw] + s->nev[ev_reboot_fix_hw]
		+ s->nev[ev_single_fix_hw];
    s->nstat[8] = s->nev[ev_reboot_upgrade_hw] + s->nev[ev_reboot_hw_upgrade]
		+ s->nev[ev_single_hw_upgrade];
    s->nstat[9] = s->nev[ev_reboot_install_sw] + s->nev[ev_reboot_sw_upgrade]
		+ s->nev[ev_single_sw_upgrade];
    s->nstat[10] = s->nev[ev_reboot_sw_problem] + s->nev[ev_reboot_fix_sw]
		+ s->nev[ev_single_fix_sw];
    s->nstat[11] = s->nev[ev_reboot_install_patch]
		+ s->nev[ev_single_install_patch];
    s->nstat[12] = s->nev[ev_single_admin];
    s->nstat[13] = s->nev[ev_reboot_environment] + s->nev[ev_reboot_scheduled]
		+ s->nev[ev_reboot_admin];
    s->nstat[14] = s->nev[ev_reboot_unconfigured] + s->nev[ev_reboot_timeout]
		+ s->nev[ev_reboot_unknown] + s->nev[ev_reboot_other]
		+ s->nev[ev_single_timeout] + s->nev[ev_single_unconfigured];
    s->nstat[6] = s->nstat[7] + s->nstat[8] + s->nstat[9] + s->nstat[10]
		+ s->nstat[11] + s->nstat[12] + s->nstat[13] + s->nstat[14];
    s->nstat[15] = s->nstat[0] + s->nstat[6];

    s->stuptime[1] = s->evuptime[ev_hardware_panic];
    s->stuptime[2] = s->evuptime[ev_software_panic];
    s->stuptime[3] = s->evuptime[ev_panic];
    s->stuptime[4] = s->evuptime[ev_interrupt] + s->evuptime[ev_system_off]
		+ s->evuptime[ev_nmi] + s->evuptime[ev_reset]
		+ s->evuptime[ev_pwer_cycle];
    s->stuptime[5] = s->evuptime[ev_power_failure];
    s->stuptime[0] = s->stuptime[1] + s->stuptime[2] + s->stuptime[3]
		+ s->stuptime[4] + s->stuptime[5];
    s->stuptime[7] = s->evuptime[ev_reboot_replace_hw]
		+ s->evuptime[ev_reboot_fix_hw] + s->evuptime[ev_single_fix_hw];
    s->stuptime[8] = s->evuptime[ev_reboot_upgrade_hw]
		+ s->evuptime[ev_reboot_hw_upgrade]
		+ s->evuptime[ev_single_hw_upgrade];
    s->stuptime[9] = s->evuptime[ev_reboot_install_sw]
		+ s->evuptime[ev_reboot_sw_upgrade]
		+ s->evuptime[ev_single_sw_upgrade];
    s->stuptime[10] = s->evuptime[ev_reboot_sw_problem]
		+ s->evuptime[ev_reboot_fix_sw] + s->evuptime[ev_single_fix_sw];
    s->stuptime[11] = s->evuptime[ev_reboot_install_patch]
		+ s->evuptime[ev_single_install_patch];
    s->stuptime[12] = s->evuptime[ev_single_admin];
    s->stuptime[13] = s->evuptime[ev_reboot_environment]
		+ s->evuptime[ev_reboot_scheduled]
		+ s->evuptime[ev_reboot_admin];
    s->stuptime[14] = s->evuptime[ev_reboot_unconfigured]
		+ s->evuptime[ev_reboot_timeout]
		+ s->evuptime[ev_reboot_unknown] + s->evuptime[ev_reboot_other]
		+ s->evuptime[ev_single_timeout]
		+ s->evuptime[ev_single_unconfigured];
    s->stuptime[6] = s->stuptime[7] + s->stuptime[8] + s->stuptime[9]
		+ s->stuptime[10] + s->stuptime[11] + s->stuptime[12]
		+ s->stuptime[13] + s->stuptime[14];
    s->stuptime[15] = s->stuptime[0] + s->stuptime[6];

    s->stdntime[1] = s->evdntime[ev_hardware_panic];
    s->stdntime[2] = s->evdntime[ev_software_panic];
    s->stdntime[3] = s->evdntime[ev_panic];
    s->stdntime[4] = s->evdntime[ev_interrupt] + s->evdntime[ev_system_off]
		+ s->evdntime[ev_nmi] + s->evdntime[ev_reset]
		+ s->evdntime[ev_pwer_cycle];
    s->stdntime[5] = s->evdntime[ev_power_failure];
    s->stdntime[0] = s->stdntime[1] + s->stdntime[2] + s->stdntime[3]
		+ s->stdntime[4] + s->stdntime[5];
    s->stdntime[7] = s->evdntime[ev_reboot_replace_hw]
		+ s->evdntime[ev_reboot_fix_hw] + s->evdntime[ev_single_fix_hw];
    s->stdntime[8] = s->evdntime[ev_reboot_upgrade_hw]
		+ s->evdntime[ev_reboot_hw_upgrade]
		+ s->evdntime[ev_single_hw_upgrade];
    s->stdntime[9] = s->evdntime[ev_reboot_install_sw]
		+ s->evdntime[ev_reboot_sw_upgrade]
		+ s->evdntime[ev_single_sw_upgrade];
    s->stdntime[10] = s->evdntime[ev_reboot_sw_problem]
		+ s->evdntime[ev_reboot_fix_sw] + s->evdntime[ev_single_fix_sw];
    s->stdntime[11] = s->evdntime[ev_reboot_install_patch]
		+ s->evdntime[ev_single_install_patch];
    s->stdntime[12] = s->evdntime[ev_single_admin];
    s->stdntime[13] = s->evdntime[ev_reboot_environment]
		+ s->evdntime[ev_reboot_scheduled]
		+ s->evdntime[ev_reboot_admin];
    s->stdntime[14] = s->evdntime[ev_reboot_unconfigured]
		+ s->evdntime[ev_reboot_timeout]
		+ s->evdntime[ev_reboot_unknown] + s->evdntime[ev_reboot_other]
		+ s->evdntime[ev_single_timeout]
		+ s->evdntime[ev_single_unconfigured];
    s->stdntime[6] = s->stdntime[7] + s->stdntime[8] + s->stdntime[9]
		+ s->stdntime[10] + s->stdntime[11] + s->stdntime[12]
		+ s->stdntime[13] + s->stdntime[14];
    s->stdntime[15] = s->stdntime[0] + s->stdntime[6];
}

void
aggregatestats(availstat_t *as, int n, availstat_t *result, char *upleasthost,
	       char *upmosthost, char *dnleasthost, char *dnmosthost)
{
    int i, j;

    initstat(result);
    for (i = 0; i < n; i++) {
	for (j = 0; j < ev_num; j++) {
	    result->nev[j] += as[i].nev[j];
	    result->evuptime[j] += as[i].evuptime[j];
	    result->evdntime[j] += as[i].evdntime[j];
	}
	for (j = 0; j < NSTATS; j++) {
	    result->nstat[j] += as[i].nstat[j];
	    result->stuptime[j] += as[i].stuptime[j];
	    result->stdntime[j] += as[i].stdntime[j];
	}
	if (as[i].nepochs) {
	    if (as[i].upleast < result->upleast) {
		result->upleast = as[i].upleast;
		strcpy(upleasthost, as[i].hostname);
	    }
	    if (as[i].upmost > result->upmost) {
		result->upmost = as[i].upmost;
		strcpy(upmosthost, as[i].hostname);
	    }
	}
	if (as[i].ndowns) {
	    if (as[i].dnleast < result->dnleast) {
		result->dnleast = as[i].dnleast;
		strcpy(dnleasthost, as[i].hostname);
	    }
	    if (as[i].dnmost > result->dnmost) {
		result->dnmost = as[i].dnmost;
		strcpy(dnmosthost, as[i].hostname);
	    }
	}
	result->total += as[i].total;
	result->possibleuptime += as[i].possibleuptime;
	result->nepochs += as[i].nepochs;
	result->ndowns += as[i].ndowns;
    }
}

#define highlight(s)	standout(); printw(s); standend()
#define printkey(k, c)	standout(); printw(k); standend(); printw(c)

int
showoverall(char *upleasthost, char *upmosthost, char *dnleasthost,
	    char *dnmosthost)
{
    int		i;
    availstat_t	*s = &availstats[nsamrs];
    float	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64];

  loop:
    clear();

    ftotal = s->total;
    fpossible = s->possibleuptime;
    strcpy(temp, "OVERALL SUMMARY");

    prettyshow_centered(temp, EQUALS, 1);
    printw("Machines Recorded .............. %d\n", nsamrs);

    printw("                                  Count  Downtime   MTBI  Availability\n");
    for (i = 0; i < NSTATS; i++) {
	if (s->nstat[i]) {
	    printw("%s %5d   %6d  %7d", statname[i], s->nstat[i],
		s->stdntime[i], s->possibleuptime / s->nstat[i]);
	    if (i == 0 || i == 6 || i == 15)
		printw("      %3.2f\n",
		    (fpossible - s->stdntime[i]) / fpossible * 100.0);
	    else
		printw("\n");
	}
    }
    /*
     * print out count, uptime, and downtime for each event
     *
    for (i = 1; i < ev_num; i++)
	if (s->nev[i])
	    printw("%4d %6d %5d  %s\n", s->nev[i], s->evuptime[i],
		s->evdntime[i], eventname[i]);
   */

    printw("\nAverage Uptime ................. %s\n",
	   pretty_minutes(s->total/s->nepochs, temp));
    printw("Least Uptime ................... %s\n", pretty_minutes(s->upleast, temp));
    printw("      Recorded at .............. %s\n", upleasthost);
    printw("Most Uptime .................... %s\n", pretty_minutes(s->upmost, temp));
    printw("      Recorded at .............. %s\n", upmosthost);

    if (s->ndowns) {
	printw("Average Downtime ............... %s\n",
	       pretty_minutes((s->possibleuptime - s->total) / s->ndowns, temp));
	printw("Least Downtime ................. %s\n", pretty_minutes(s->dnleast, temp));
	printw("      Recorded at .............. %s\n", dnleasthost);
	printw("Most Downtime .................. %s\n", pretty_minutes(s->dnmost, temp));
	printw("      Recorded at .............. %s\n", dnmosthost);
    }

    printw("Availability ................... %3.2f%% based on %d machines\n",
	   (ftotal/fpossible) * 100.0, nsamrs);

    prettyshow_centered("=", EQUALS, 0); /* print 80 char '=' line */
    refresh();

    move(LINES - 1, 0);
    printkey("<SPACE>", ":stat list  ");
    printkey("<RETURN>", ":stat list  ");
    printkey("V", ":stat list  ");
    printkey("Q", ":quit  ");
    refresh();

    while (1)
	switch (getkey()) {
	case KEY_ENTER:
	case '\n':
	case ' ':
	case 'V':
	case 'v':
	    if (showstatlist() == 'q')
		return 'q';
	    else
		goto loop;
	case 'Q':
	case 'q':
	    return 'q';
	default:
	    beep();
	}
}

#define STATTOP	2

int
showstatlist()
{
    int		i, j;
    availstat_t	*s = &availstats[0];
    availstat_t	*stat_b, *stat_e, *stat_t, *stat;
    int		row, entry, begin, end, num;
    int		top, numshow, perc;

    stat_t = stat_b = &availstats[0];
    stat_e = &availstats[nsamrs];
    top = begin = 0;
    num = end = nsamrs;
    numshow = LINES - 7;
    if (numshow > num)
	numshow = num;

  loop:
    clear();
    standout();
    printw("SERIAL NUMBER   HOSTNAME              UNSCHEDULED      SEVICE"
	   "          TOTAL   \n\n");
    standend();
    refresh();

    move(LINES - 2, 0);
    printkey("A", ":mark all  ");
    printkey("C", ":unmark all  ");
    printkey("M", ":mark  ");
    printkey("U", ":unmark  ");
    printkey("V", ":view marked entries\n");
    printkey("J", ":up  ");
    printkey("K", ":down  ");
    printkey("<UP>", ":up  ");
    printkey("<DOWN>", ":down  ");
    printkey("N", ":next page  ");
    printkey("P", ":prev page  ");
    printkey("B", ":back  ");
    printkey("Q", ":quit  ");
    refresh();

  show:
    for (i = 0, stat = stat_t; i < numshow && stat < stat_e; i++, stat++) {
	move(i + STATTOP, 0);
	showstatline(stat);
    }

    if (num <= numshow)
	printw("\n-- ALL --");
    else if (stat == stat_e)
	printw("\n-- BOTTOM --");
    else {
	perc = (stat - stat_b) * 100 / num;
	if (stat_t == stat_b)
	    printw("\n-- TOP %d%%", perc);
	else
	    printw("\n-- %d%%", perc);
    }

    row = STATTOP;
    entry = top;
    move(row, 0);
    while (1)
	switch (getkey()) {
	case 'A':
	case 'a':
	    move(STATTOP, 0);
	    for (j = begin; j < end; j++)
		statmarks[j] = 1;
	    goto show;
	case 'C':
	case 'c':
	    move(STATTOP, 0);
	    for (j = begin; j < end; j++)
		statmarks[j] = 0;
	    goto show;
	case 'M':
	case 'm':
	    statmarks[entry] = 1;
	    showstatline(&availstats[entry]);
	    row++;
	    entry++;
	    if (row >= numshow + STATTOP) {
		row = STATTOP;
		entry = top;
		move(row, 0);
	    }
	    break;
	case 'U':
	case 'u':
	    statmarks[entry] = 0;
	    showstatline(&availstats[entry]);
	    row++;
	    entry++;
	    if (row >= numshow + STATTOP) {
		row = STATTOP;
		entry = top;
		move(row, 0);
	    }
	    break;
	case 'V':
	case 'v':
	    if (showstats() == 'q')
		return 'q';
	    else
		goto loop;
	case 'K':
	case 'k':
	case KEY_UP:
	    row--;
	    entry--;
	    if (row < STATTOP) {
		row = numshow + STATTOP - 1;
		entry = top + numshow - 1;
	    }
	    move(row, 0);
	    break;
	case 'J':
	case 'j':
	case KEY_DOWN:
	    row++;
	    entry++;
	    if (row >= numshow + STATTOP) {
		row = STATTOP;
		entry = top;
	    }
	    move(row, 0);
	    break;
	case ' ':
	case '\n':
	case KEY_ENTER:
	case 'N':
	case 'n':
	    if (num > numshow) {
		if (top + numshow >= end) {
		    top = begin;
		    stat_t = stat_b;
		} else {
		    top += numshow;
		    stat_t += numshow;
		}
		goto loop;
	    } else
		break;
	case 'P':
	case 'p':
	    if (num > numshow) {
		if (top == begin) {
		    top = end - numshow;
		    stat_t = stat_e - numshow;
		} else if (top - numshow <= begin) {
		    top = begin;
		    stat_t = stat_b;
		} else {
		    top -= numshow;
		    stat_t -= numshow;
		}
		goto loop;
	    } else
		break;
	case 'B':
	case 'b':
	    return 'b';
	case 'Q':
	case 'q':
	    return 'q';
	default:
	    beep();
	}
}

void
showstatline(availstat_t *s)
{
    int		hilite;
    float	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64];

    hilite = statmarks[s - availstats];
    if (hilite)
	standout();

    ftotal = s->total;
    fpossible = s->possibleuptime;

    strcpy(temp, s->hostname); temp[20] = '\0';
    printw("%-12s %-20s  ", s->sysid, temp);
    if (s->nstat[0])
	printw("%5d  %6.2f%% ", s->nstat[0],
	       (fpossible - s->stdntime[0]) / fpossible * 100.0);
    else
	printw("%5d  %6.2f%% ", 0, 100.0);
    if (s->nstat[6])
	printw("%5d  %6.2f%% ", s->nstat[6],
	       (fpossible - s->stdntime[6]) / fpossible * 100.0);
    else
	printw("%5d  %6.2f%% ", 0, 100.0);
    if (s->nstat[15])
	printw("%5d  %6.2f%%\n", s->nstat[15],
	       (fpossible - s->stdntime[15]) / fpossible * 100.0);
    else
	printw("%5d  %6.2f%%\n", 0, 100.0);

    if (hilite)
	standend();
}

int
showstats()
{
    int		i, begin, end;

    begin = 0;
    end = nsamrs;

    for (i = begin; i < end; i++) {
	if (statmarks[i] == 0)
	    continue;
      loop:
	clear();
	showstat(&availstats[i]);

	if (locallog) {
	    move(LINES - 1, 0);
	    printkey("<SPACE>", ":amr list  ");
	    printkey("<RETURN>", ":amr list  ");
	    printkey("V", ":amr list  ");
	    printkey("Q", ":quit  ");
	    refresh();

	    while (1)
		switch (getkey()) {
		case KEY_ENTER:
		case '\n':
		case ' ':
		case 'V':
		case 'v':
		    if (showamrlist(0) == 'q')
			return 'q';
		    else
			goto loop;
		case 'Q':
		case 'q':
		    return 'q';
		default:
		    beep();
		}
	} else {
	    move(LINES - 2, 0);
	    printkey("<SPACE>", ":next  ");
	    printkey("<RETURN>", ":next  ");
	    printkey("N", ":next  ");
	    printkey("V", ":amr list  ");
	    printkey("P", ":previous  ");
	    printkey("B", ":back  ");
	    printkey("Q", ":quit  ");
	    refresh();

	    while (1)
		switch (getkey()) {
		case KEY_ENTER:
		case '\n':
		case ' ':
		case 'N':
		case 'n':
		    goto out;
		case 'P':
		case 'p':
		    for (i--; i >= begin; i--)
			if (statmarks[i])
			    goto loop;
		    return 'b';
		case 'V':
		case 'v':
		    if (showamrlist(i) == 'q')
			return 'q';
		    else
			goto loop;
		case 'B':
		case 'b':
		    return 'b';
		case 'Q':
		case 'q':
		    return 'q';
		default:
		    beep();
		}

	  out:
	    if (i == end - 1)
		return 'b';
	}
    }
    return 'b';
}

void
showstat(availstat_t *s)
{
    int		i;
    float	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64];

    ftotal = s->total;
    fpossible = s->possibleuptime;
    sprintf(temp, "SUMMARY for %s", s->hostname);
    prettyshow_centered(temp, EQUALS, 1);
    printw("                                  Count  Downtime   MTBI  Availability\n");
    for (i = 0; i < NSTATS; i++) {
	if (s->nstat[i]) {
	    printw("%s %5d   %6d  %7d", statname[i], s->nstat[i],
		s->stdntime[i], s->possibleuptime / s->nstat[i]);
	    if (i == 0 || i == 6 || i == 15)
		printw("      %3.2f\n",
		   (fpossible - s->stdntime[i]) / fpossible * 100.0);
	    else
		printw("\n");
	}
    }

    /*
     * print out count, uptime, and downtime for each event
     *
    for (i = 0; i < ev_num; i++)
	if (s->nev[i])
	    printw("%4d %6d %5d  %s\n", s->nev[i], s->evuptime[i],
		s->evdntime[i], eventname[i]);
    */

    printw("\n%s %s\n", "Average Uptime .................",
	   pretty_minutes(s->total/s->nepochs, temp));

    printw("Least Uptime ................... %s %s\n", pretty_minutes(s->upleast, temp),
	   (s->thisepoch == s->upleast) ? "(current epoch)":"");
    printw("Most Uptime .................... %s %s\n", pretty_minutes(s->upmost, temp),
	   (s->thisepoch == s->upmost) ? "(current epoch)":"");
    if (s->ndowns) {
	printw("Average Downtime ............... %s\n",
	       pretty_minutes((s->possibleuptime - s->total) / s->ndowns, temp));
	printw("Least Downtime ................. %s %s\n", pretty_minutes(s->dnleast, temp),
	       (s->thisepoch == s->dnleast) ? "(current epoch)":"");
	printw("Most Downtime .................. %s %s\n", pretty_minutes(s->dnmost, temp),
	       (s->thisepoch == s->dnmost) ? "(current epoch)":"");
    }
    /* printw("Availability ................... %3.2f%%\n", (ftotal/fpossible) * 100.0); */
    printw("\nLogging started at ............. %s", ctime(&(s->thebeginning)));
    i = s - availstats;
    if ((samrs[i + 1] - 1)->regist < 0)
	printw("Deregistered at ................ %s", ctime(&(s->lastboot)));
    else {
	printw("Last boot at ................... %s", ctime(&(s->lastboot)));
	if (countthisepoch)
	    printw("System has been up for ......... %s\n",
		   pretty_minutes(s->thisepoch, temp));
    }

    prettyshow_centered("=", EQUALS, 0); /* print 80 char '=' line */
    refresh();
}

void
prettyshow_centered(char *s, char c, int spaces)
{
    int slen, lflen, rflen, i;
    char obuf[REPORT_WIDTH], *p;

    slen = strlen(s);
    if (slen >= (REPORT_WIDTH - 5)) {
	printw("<%s>\n", s);
	return;
    }
    lflen = (REPORT_WIDTH - (spaces ? 2 : 0) - slen)/2;
    rflen = (REPORT_WIDTH - (spaces ? 2 : 0) - slen) - lflen;
    p = obuf;
    for (i = 0; i < lflen; i++)
	*p++ = c;
    if (spaces) {
	sprintf(p, " %s ", s);
	p += (slen + 2);
    } else {
	sprintf(p, "%s", s);
	p += slen;
    }

    for (i = 0; i < rflen; i++)
	*p++ = c;
    *p = NULL;
    printw("%s\n", obuf);
}

void
waitforenter(char *msg)		/* debugging: to stop output stream */
{
    char linebuf[LINELEN+1], c;

    if (!outistty) return;
    printw("%s", msg);
    refresh();
    getret();
    /* fgets(linebuf, LINELEN, stdin); */
}

#define AMRTOP	3

int
showamrlist(int mach)
{
    int		row, i, j, entry, begin, end, num;
    int		top, numshow, perc;
    char	hostname[MAXHOSTNAMELEN+1];
    amr_t	*amr, *amrb, *amre, *amrt;

    amrt = amrb = samrs[mach];
    amre = samrs[mach + 1];
    top = begin = amrb - amrs;
    end = amre - amrs;
    num = end - begin;
    if (num == 0) {
	clear();
	printw("\nNo shutdown report\n\nPress any key to continue.\n");
	getkey();
	return 'b';
    }
    numshow = LINES - 4 - AMRTOP;
    if (numshow > num)
	numshow = num;

  loop:
    clear();
    prettyshow_centered(amrs[begin].hostname, DASH, 1);
    standout();
    printw("       Start Time              Incident Time         UpTm  DnTm"
	   "  Reason        ");
    standend();
    refresh();

    move(LINES - 2, 0);
    printkey("A", ":mark all  ");
    printkey("C", ":unmark all  ");
    printkey("M", ":mark  ");
    printkey("U", ":unmark  ");
    printkey("V", ":view  ");
    printkey("S", ":send\n");
    printkey("J", ":up  ");
    printkey("K", ":down  ");
    printkey("<UP>", ":up  ");
    printkey("<DOWN>", ":down  ");
    printkey("N", ":next page  ");
    printkey("P", ":prev page  ");
    printkey("B", ":back  ");
    printkey("Q", ":quit  ");
    refresh();

  show:
    for (i = 0, amr = amrt; i < numshow && amr < amre; i++, amr++) {
	move(i + AMRTOP, 0);
	showamrline(amr);
    }

    if (num <= numshow)
	printw("\n-- ALL --");
    else if (amr == amre)
	printw("\n-- BOTTOM --");
    else {
	perc = (amr - amrb) * 100 / num;
	if (amrt == amrb)
	    printw("\n-- TOP %d%%", perc);
	else
	    printw("\n-- %d%%", perc);
    }

    row = AMRTOP;
    entry = top;
    move(row, 0);
    while (1)
	switch (getkey()) {
	case 'A':
	case 'a':
	    move(AMRTOP, 0);
	    for (j = begin; j < end; j++)
		amrmarks[j] = 1;
	    goto show;
	case 'C':
	case 'c':
	    move(AMRTOP, 0);
	    for (j = begin; j < end; j++)
		amrmarks[j] = 0;
	    goto show;
	case 'M':
	case 'm':
	    amrmarks[entry] = 1;
	    showamrline(&amrs[entry]);
	    row++;
	    entry++;
	    if (row >= numshow + AMRTOP) {
		row = AMRTOP;
		entry = top;
		move(row, 0);
	    }
	    break;
	case 'U':
	case 'u':
	    amrmarks[entry] = 0;
	    showamrline(&amrs[entry]);
	    row++;
	    entry++;
	    if (row >= numshow + AMRTOP) {
		row = AMRTOP;
		entry = top;
		move(row, 0);
	    }
	    break;
	case 'V':
	case 'v':
	    if (showamrs(begin, end) == 'q')
		return 'q';
	    else
		goto loop;
	case 'S':
	case 's':
	    sendamrs(begin, end);
	    goto loop;
	case 'K':
	case 'k':
	case KEY_UP:
	    row--;
	    entry--;
	    if (row < AMRTOP) {
		row = numshow + AMRTOP - 1;
		entry = top + numshow - 1;
	    }
	    move(row, 0);
	    break;
	case 'J':
	case 'j':
	case KEY_DOWN:
	    row++;
	    entry++;
	    if (row >= numshow + AMRTOP) {
		row = AMRTOP;
		entry = top;
	    }
	    move(row, 0);
	    break;
	case ' ':
	case '\n':
	case KEY_ENTER:
	case 'N':
	case 'n':
	    if (num > numshow) {
		if (top + numshow >= end) {
		    top = begin;
		    amrt = amrb;
		} else {
		    top += numshow;
		    amrt += numshow;
		}
		goto loop;
	    } else
		break;
	case 'P':
	case 'p':
	    if (num > numshow) {
		if (top == begin) {
		    top = end - numshow;
		    amrt = amre - numshow;
		} else if (top - numshow <= begin) {
		    top = begin;
		    amrt = amrb;
		} else {
		    top -= numshow;
		    amrt -= numshow;
		}
		goto loop;
	    } else
		break;
	case 'B':
	case 'b':
	    return 'b';
	case 'Q':
	case 'q':
	    return 'q';
	default:
	    beep();
	}
}

void
showamrline(amr_t *amr)
{
    int		hilite;
    char	tmp1[30], tmp2[30];
    time_t	t;

    hilite = amrmarks[amr - amrs];
    if (hilite)
	standout();

    if (amr->noprevstart) {
	tmp1[0] = '\0';
    } else {
	strcpy(tmp1, ctime(&(amr->prevstarttime)));
	tmp1[24] = '\0';
    }
    if (amr->eventtime > 0) {
	tmp2[0] = ' ';
	strcpy(tmp2 + 1, ctime(&(amr->eventtime)));
	tmp2[25] = '\0';
    } else {
	tmp2[0] = '*';
	t = amr->prevstarttime + amr->uptime * 60;
	strcpy(tmp2 + 1, ctime(&t));
	tmp2[25] = '\0';
    }
    printw("%-24s %-25s ", tmp1, tmp2);
    if ((amr->event > ev_deregistration || amr->event < ev_status_report)
	 && amr->event != ev_livenotification)
	printw("%6d %5d  ", amr->uptime, amr->dntime);
    else
	printw("     -     -  ");
    printw("%s\n", shorteventname[amr->event]);

    if (hilite)
	standend();
}

char
showamrs(int begin, int end)
{
    int i;

    for (i = begin; i < end; i++) {
	if (amrmarks[i] == 0)
	    continue;

      loop:
	clear();
	showamr(&amrs[i]);

	move(LINES - 2, 0);
	printkey("<SPACE>", ":next  ");
	printkey("<RETURN>", ":next  ");
	printkey("N", ":next  ");
	printkey("P", ":previous  ");
	printkey("B", ":back  ");
	printkey("Q", ":quit  ");
	refresh();

	while (1)
	    switch (getkey()) {
	    case KEY_ENTER:
	    case '\n':
	    case ' ':
	    case 'N':
	    case 'n':
		goto out;
	    case 'P':
	    case 'p':
		for (i--; i >= begin; i--)
		    if (amrmarks[i])
			goto loop;
		return 'b';
	    case 'B':
	    case 'b':
		return 'b';
	    case 'Q':
	    case 'q':
		return 'q';
	    default:
		beep();
	    }

      out:
	if (i == end - 1)
	    return 'b';
    }
    return 'b';
}

void
showamr(amr_t *amr)
{
    char	s[128], hostname[MAXHOSTNAMELEN+1];
    time_t	t;

    prettyshow_centered(amr->hostname, DASH, 1);
    printw("Internet address ....... %s\n", amr->hostname);
    printw("Reason for shutdown .... %s\n", eventname[amr->event]);
    printw("Start time ............. %s", ctime(&(amr->prevstarttime)));
    printw("Incident time .......... ");
    if (amr->eventtime > 0) {
	printw("%s", ctime(&(amr->eventtime)));
    } else {
	t = amr->prevstarttime + amr->uptime * 60;
	printw("around %s", ctime(&t));
    }
    if ((amr->event > ev_deregistration || amr->event < ev_status_report) 
	 && amr->event != ev_livenotification) {
	printw("Re-start time .......... %s", ctime(&(amr->starttime)));
	printw("Uptime ................. %s\n", pretty_minutes(amr->uptime, s));
	printw("Downtime ............... %s\n", pretty_minutes(amr->dntime, s));
    }
    if (amr->summarylen)
	printw("Incident summary ....... \n%s\n", amr->summary);
    prettyshow_centered("-", DASH, 0); /* print 80 char '-' line */
    refresh();
}

/*
 * Edit the email list configuration file
 */
void
editaddresses()
{
    int		pid, s, t;
    FILE	*fp, *fp1;
    char	*edit = "/usr/bin/vi";
    void	(*sig)(), (*scont)(), signull();

    sig = sigset(SIGINT, SIG_IGN);

    /* fork an editor on the message */
    pid = fork();
    if (pid == 0) {
	/* CHILD - exec the editor. */
	if (sig != SIG_IGN)
	    signal(SIGINT, SIG_DFL);
	execl(edit, edit, cfile, 0);
	perror(edit);
	exit(1);
    }
    if (pid == -1) {
	/* ERROR - issue warning and get out. */
	perror("fork");
	unlink(cfile);
	goto out;
    }
    /* PARENT - wait for the child (editor) to complete. */
    while (wait(&s) != pid)
	;

    /* Check the results. */
    if ((s & 0377) != 0)
	printf("Fatal error in \"%s\"\n", edit);

  out:
    sigset(SIGINT, sig);
}

int
getaddresses()
{
    int		len;
    FILE	*fp, *fp1;
    char	line[MAX_LINE_LEN];

    if (cfile == NULL) {
	if ((cfile = tempnam(NULL, "amrc")) == NULL) {
	    fprintf(stderr, "Error: cannot create temp file name\n");
	    exit(1);
	}
	if ((fp = fopen(cfile, "w")) == NULL) {
	    fprintf(stderr, "Error: cannot create temp file\n");
	    exit(1);
	}
	if ((fp1 = fopen(amcs[amc_autoemaillist].filename, "r")) == NULL) {
	    fprintf(stderr, "Error: cannot open file autoemail.list\n");
	    exit(1);
	}
	len = strlen(sendtypestr[PAGERTYPE]);
	while (fgets(line, MAX_LINE_LEN, fp1)) {
	    if (strncmp(line, sendtypestr[PAGERTYPE], len) == 0)
		continue;
	    else
		fputs(line, fp);
	}
	fclose(fp);
	fclose(fp1);
    }
loop:
    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr, "Error: cannot open temp file\n");
	exit(1);
    }
    clear();
    while (fgets(line, MAX_LINE_LEN, fp))
	printw("%s", line);
    fclose(fp);
    move(LINES - 1, 0);
    printkey("S", ":send  ");
    printkey("E", ":edit autoemail.list  ");
    printkey("C", ":cancel  ");

    while (1)
	switch (getkey()) {
	case 'C':
	case 'c':
	    return(0);
	case 'E':
	case 'e':
	    editaddresses();
	    goto loop;
	case 'S':
	case 's':
	    return(1);
	default:
	    beep();
	}
}

void
sendamrs(int begin, int end)
{
    int		i, count = 0, days = 0, waitinput = 1;
    char	c;
    time_t	t;
    amr_t	*amr;

    if (locallog == 0) {
	move(LINES - 3, 0);
	standout();
	printw("Only using locallog can send old availmon reports."
	       "  Press any key to continue.");
	standout();
	getkey();
	return;
    }
    for (i = begin; i < end; i++)
	if (amrmarks[i])
	    count++;
    if (count == 0) {
	move(LINES - 3, 0);
	printw("No entry marked. Send reports since how many days ago? (30) ");
	
	while (waitinput) {
	    switch(c = getkey()) {
	    case KEY_ENTER:
	    case 87:			/* enter key */
	    case '\n':
		waitinput = 0;
		if (count == 0)
		    days = 30;
		break;
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		printw("%c", c);
		days = days * 10 + c - '0';
		count++;
		break;
	    case KEY_BACKSPACE:
	    case 8:			/* backspace key */
	    case KEY_DC:
	    case 74:			/* delete key */
		printw("%c%c%c", c, ' ', c);
		days /= 10;
		count--;
		break;
	    default:
		beep();
	    }
	}
	/* get the from time */
	t = time(0) - days * 86400;
	for (amr = &amrs[begin]; amr < &amrs[end]; amr++) {
	    if (amr->prevstarttime > t) {
		break;
	    } else if (amr->eventtime > t) {
		break;
	    } else if (amr->starttime > t) {
		break;
	    }
	}
	if (getaddresses())
	    for ( ; amr < &amrs[end]; amr++)
		sendamr(amr);
    } else {

	if (getaddresses())
	    for (i = begin; i < end; i++) {
		if (amrmarks[i])
		    sendamr(&amrs[i]);
	    }
    }
}

void
sendamr(amr_t *amr)
{
    char	*tfn, *tfn1, fname[MAX_LINE_LEN], crashdir[MAX_LINE_LEN] = "";
    char	line[MAX_LINE_LEN], syscmd[MAX_LINE_LEN];
    time_t	etime;
    FILE	*fp, *fp1;
    struct stat	statbuf;

    if (((tfn = tempnam(NULL, "amra")) == NULL) ||
	((tfn1 = tempnam(NULL, "amrd")) == NULL)) {
	fprintf(stderr, "Error: cannot create temp file name\n");
	exit(1);
    }
    if (((fp = fopen(tfn, "w")) == NULL) ||
	((fp1 = fopen(tfn1, "w")) == NULL)) {
	fprintf(stderr, "Error: cannot create temp file\n");
	exit(1);
    }
    fprintf(fp, "SERIALNUM|%s\n", amr->sysid);
    fprintf(fp1, "SERIALNUM|%s\n", amr->sysid);
    fprintf(fp, "HOSTNAME|%s\n", amr->hostname);
    fprintf(fp1, "HOSTNAME|%s\n", amr->hostname);
    fprintf(fp, "AMRVERSION|%s\n", AMRVERSIONNUM);
    fprintf(fp1, "AMRVERSION|%s\n", AMRVERSIONNUM);
    fprintf(fp, "RESEND|\n");
    fprintf(fp1, "RESEND|\n");
    fprintf(fp, "PREVSTART|%d|%s", amr->prevstarttime,
	    ctime(&(amr->prevstarttime)));
    fprintf(fp1, "PREVSTART|%d|%s", amr->prevstarttime,
	    ctime(&(amr->prevstarttime)));
    if (amr->eventtime > 0) {
	etime = amr->eventtime;
	strcpy(line, ctime(&etime));
    } else {
	etime = -1;
	strcpy(line, "unknown\n");
    }
    fprintf(fp, "EVENT|%d|%d|%s", amr->event + MINEVENTCODE - 1, etime, line);
    fprintf(fp1, "EVENT|%d|%d|%s", amr->event + MINEVENTCODE - 1, etime, line);

    if (amr->lasttick > 0) {
	fprintf(fp, "LASTTICK|%d|%s", amr->lasttick, ctime(&(amr->lasttick)));
	fprintf(fp1, "LASTTICK|%d|%s", amr->lasttick, ctime(&(amr->lasttick)));
    } else {
	fprintf(fp, "LASTTICK|-1|unknown\n");
	fprintf(fp1, "LASTTICK|-1|unknown\n");
    }
    switch (amr->event) {
    case ev_no_error:
    case ev_hardware_error:
    case ev_software_error:
    case ev_systemid_change:
	break;
    default:
	fprintf(fp, "START|%d|%s", amr->starttime, ctime(&(amr->starttime)));
	fprintf(fp1, "START|%d|%s", amr->starttime, ctime(&(amr->starttime)));
    }
    if (amr->summarylen) {
	if (amr->event == ev_systemid_change) {
	    fprintf(fp, "%s", amr->summary);
	    fprintf(fp1, "%s", amr->summary);
	}
	fprintf(fp, "SUMMARY|Begin\n%sSUMMARY|End\n", amr->summary);
	fprintf(fp1, "SUMMARY|Begin\n%sSUMMARY|End\n", amr->summary);
    }
    fclose(fp);
    if (amr->bound >= 0) {
	if (fp = fopen("/etc/config/savecore.options", "r")) {
	    while (fscanf(fp, "%s ", fname) != EOF)
		if (fname[0] == '/') {
		    stat(fname, &statbuf);
		    if (S_ISDIR(statbuf.st_mode))
			strcpy(crashdir, fname);
		}
	    fclose(fp);
	}
	if (crashdir[0] == '\0')
	    strcpy(crashdir, "/var/adm/crash");
	sprintf(fname, "%s/analysis.%d", crashdir, amr->bound);
	if (fp = fopen(fname, "r")) {
	    fprintf(fp1, "ICRASH|Begin\n");
	    while (fgets(line, MAX_LINE_LEN, fp))
		fputs(line, fp1);
	    fprintf(fp1, "ICRASH|End\n");
	    fclose(fp);
	}
	sprintf(fname, "%s/syslog.%d", crashdir, amr->bound);
	if (fp = fopen(fname, "r")) {
	  fprintf(fp1, "SYSLOG|Begin\n");
	    while (fgets(line, MAX_LINE_LEN, fp))
		fputs(line, fp1);
	    fprintf(fp1, "SYSLOG|End\n");
	    fclose(fp);
	}
    }

    fclose(fp1);
    switch (amr->event) {
    case ev_no_error:
    case ev_hardware_error:
    case ev_software_error:
	sprintf(syscmd, "/usr/etc/amnotify -c %s -d %s ; rm -f %s %s",
		cfile, tfn1, tfn, tfn1);
	break;
    default:
	sprintf(syscmd, "/usr/etc/amnotify -c %s -a %s -d %s ; rm -f %s %s",
		cfile, tfn, tfn1, tfn, tfn1);
    }
    system(syscmd);
}

char *
pretty_minutes(int m, char *s)
{
    int		days, hrs, mins, tm;
    char	tempd[32];
    char	temph[32];
    char	tempm[32];

    tm = m;
    days = m / (60 * 24);
    m %= (60 * 24);
    hrs = m / 60;
    m %= 60;
    mins = m;

    s[0] = tempd[0] = temph[0] = tempm[0] = NULL;
    if (days)
	sprintf(tempd, "%d day%s", days, (days == 1) ? "" : "s");
    if (hrs)
	sprintf(temph, "%s%d hr%s", (days ? " " : "") /* space */,
		hrs, (hrs == 1)? "":"s");
    if (mins)
	sprintf(tempm, "%s%d min%s", ((days||hrs) ? " " : "") /* space */,
		mins, (mins == 1)? "":"s");
    if (days || hrs) /* no point in printing minutes by itself again */
	sprintf(s, "%d minutes (%s%s%s)", tm, tempd, temph, tempm); 
    else 
	sprintf(s, "%d minute%s", tm, (tm == 1) ? "" : "s");
    return(s);
}

void
prettyprint_centered(char *s, char c, int spaces)
{
    int slen, lflen, rflen, i;
    char obuf[REPORT_WIDTH], *p;

    slen = strlen(s);
    if (slen >= (REPORT_WIDTH - 5)) {
	printf("%s\n", s);
	return;
    }
    lflen = (REPORT_WIDTH - (spaces ? 2 : 0) - slen)/2;
    rflen = (REPORT_WIDTH - (spaces ? 2 : 0) - slen) - lflen;
    p = obuf;
    for (i = 0; i < lflen; i++)
	*p++ = c;
    if (spaces) {
	sprintf(p, " %s ", s);
	p += (slen + 2);
    } else {
	sprintf(p, "%s", s);
	p += slen;
    }

    for (i = 0; i < rflen; i++)
	*p++ = c;
    *p = NULL;
    printf("%s\n", obuf);
}

void
printstatlist(int all)
{
    availstat_t	*s = &availstats[nsamrs];
    availstat_t	*stat_b, *stat_e, *stat;
    float	ftotal, fpossible;

    stat_b = &availstats[0];
    stat_e = &availstats[nsamrs];

    if (all) {
	printf("SERIAL NUMBER   HOSTNAME              UNSCHEDULED      SEVICE"
	       "          TOTAL\n\n");
	ftotal = s->total;
	fpossible = s->possibleuptime;
	printf("%-33s  ", "Overall Statistics");
	if (s->nstat[0])
	    printf("%5d  %6.2f%% ", s->nstat[0],
	           (fpossible - s->stdntime[0]) / fpossible * 100.0);
	else
	    printf("%5d  %6.2f%% ", 0, 100.0);
	if (s->nstat[6])
	    printf("%5d  %6.2f%% ", s->nstat[6],
	       (fpossible - s->stdntime[6]) / fpossible * 100.0);
	else
	    printf("%5d  %6.2f%% ", 0, 100.0);
	if (s->nstat[15])
	    printf("%5d  %6.2f%%\n\n", s->nstat[15],
	       (fpossible - s->stdntime[15]) / fpossible * 100.0);
	else
	    printf("%5d  %6.2f%%\n\n", 0, 100.0);

	for (stat = stat_b; stat < stat_e; stat++)
	    printstatline(stat);
	printf("\n");
    }
    for (stat = stat_b; stat < stat_e; stat++)
	printstat(stat);
    printf("\n");
}


void
printstatline(availstat_t *s)
{
    float	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64];

    ftotal = s->total;
    fpossible = s->possibleuptime;

    strcpy(temp, s->hostname); temp[20] = '\0';
    printf("%-12s %-20s  ", s->sysid, temp);
    if (s->nstat[0])
	printf("%5d  %6.2f%% ", s->nstat[0],
	       (fpossible - s->stdntime[0]) / fpossible * 100.0);
    else
	printf("%5d  %6.2f%% ", 0, 100.0);
    if (s->nstat[6])
	printf("%5d  %6.2f%% ", s->nstat[6],
	       (fpossible - s->stdntime[6]) / fpossible * 100.0);
    else
	printf("%5d  %6.2f%% ", 0, 100.0);
    if (s->nstat[15])
	printf("%5d  %6.2f%%\n", s->nstat[15],
	       (fpossible - s->stdntime[15]) / fpossible * 100.0);
    else
	printf("%5d  %6.2f%%\n", 0, 100.0);
}

void
printstat(availstat_t *s)
{
    int		i;
    float	ftotal, fpossible;
    char	temp[MAXHOSTNAMELEN+64];

    ftotal = s->total;
    fpossible = s->possibleuptime;
    sprintf(temp, "SUMMARY for %s", s->hostname);
    prettyprint_centered(temp, EQUALS, 1);
    printf("                                  Count  Downtime   MTBI  Availability\n");
    for (i = 0; i < NSTATS; i++) {
	if (s->nstat[i]) {
	    printf("%s %5d   %6d  %7d", statname[i], s->nstat[i],
		s->stdntime[i], s->possibleuptime / s->nstat[i]);
	    if (i == 0 || i == 6 || i == 15)
		printf("      %3.2f\n",
		   (fpossible - s->stdntime[i]) / fpossible * 100.0);
	    else
		printf("\n");
	}
    }

    /*
     * print out count, uptime, and downtime for each event
     *
    for (i = 0; i < ev_num; i++)
	if (s->nev[i])
	    printf("%4d %6d %5d  %s\n", s->nev[i], s->evuptime[i],
		s->evdntime[i], eventname[i]);
    */

    printf("\n%s %s\n", "Average Uptime .................",
	   pretty_minutes(s->total/s->nepochs, temp));

    printf("Least Uptime ................... %s %s\n", pretty_minutes(s->upleast, temp),
	   (s->thisepoch == s->upleast) ? "(current epoch)":"");
    printf("Most Uptime .................... %s %s\n", pretty_minutes(s->upmost, temp),
	   (s->thisepoch == s->upmost) ? "(current epoch)":"");
    if (s->ndowns) {
	printf("Average Downtime ............... %s\n",
	       pretty_minutes((s->possibleuptime - s->total) / s->ndowns, temp));
	printf("Least Downtime ................. %s %s\n", pretty_minutes(s->dnleast, temp),
	       (s->thisepoch == s->dnleast) ? "(current epoch)":"");
	printf("Most Downtime .................. %s %s\n", pretty_minutes(s->dnmost, temp),
	       (s->thisepoch == s->dnmost) ? "(current epoch)":"");
    }
    /* printf("Availability ................... %3.2f%%\n", (ftotal/fpossible) * 100.0); */
    printf("\nLogging started at ............. %s", ctime(&(s->thebeginning)));
    i = s - availstats;
    if ((samrs[i + 1] - 1)->regist < 0)
	printf("Deregistered at ................ %s", ctime(&(s->lastboot)));
    else {
	printf("Last boot at ................... %s", ctime(&(s->lastboot)));
	if (countthisepoch)
	    printf("System has been up for ......... %s\n",
		   pretty_minutes(s->thisepoch, temp));
    }

    prettyprint_centered("=", EQUALS, 0); /* print 80 char '=' line */
}

void
printamrlist(int mach)
{
    int		begin;
    char	hostname[MAXHOSTNAMELEN+1];
    amr_t	*amr, *amrb, *amre;

    amrb = samrs[mach];
    amre = samrs[mach + 1];
    begin = amrb - amrs;

    prettyprint_centered(amrs[begin].hostname, DASH, 1);
    printf("       Start Time              Incident Time         UpTm  DnTm"
	   "  Reason        \n");

    for (amr = amrb; amr < amre; amr++)
	printamrline(amr);
    printf("\n");
}

void
printamrline(amr_t *amr)
{
    char	tmp1[30], tmp2[30];
    time_t	t;

    if (amr->noprevstart) {
	tmp1[0] = '\0';
    } else {
	strcpy(tmp1, ctime(&(amr->prevstarttime)));
	tmp1[24] = '\0';
    }
    if (amr->eventtime > 0) {
	tmp2[0] = ' ';
	strcpy(tmp2 + 1, ctime(&(amr->eventtime)));
	tmp2[25] = '\0';
    } else {
	tmp2[0] = '*';
	t = amr->prevstarttime + amr->uptime * 60;
	strcpy(tmp2 + 1, ctime(&t));
	tmp2[25] = '\0';
    }
    printf("%-24s %-25s ", tmp1, tmp2);
    if ((amr->event > ev_deregistration || amr->event < ev_status_report)
	 && amr->event != ev_livenotification)
	printf("%6d %5d  ", amr->uptime, amr->dntime);
    else
	printf("     -     -  ");
    printf("%s\n", shorteventname[amr->event]);
}

void
printamrs(int mach)
{
    int		begin;
    char	hostname[MAXHOSTNAMELEN+1];
    amr_t	*amr, *amrb, *amre;

    amrb = samrs[mach];
    amre = samrs[mach + 1];
    begin = amrb - amrs;

    prettyprint_centered(amrs[begin].hostname, DASH, 1);
    printf("       Start Time              Incident Time         UpTm  DnTm"
	   "  Reason        \n");

    for (amr = amrb; amr < amre; amr++)
	printamr(amr);
    printf("\n");
}

void
printamr(amr_t *amr)
{
    char	s[128], hostname[MAXHOSTNAMELEN+1];
    time_t	t;

    printf("\nSerial number .......... %s\n", amr->sysid);
    printf("Internet address ....... %s\n", amr->hostname);
    if (amr->regist > 0) {
	printf("Started at ............. %s", ctime(&(amr->prevstarttime)));
	printf("Registered at .......... %s", ctime(&(amr->starttime)));
    } else {
	printf("Reason for shutdown .... %s\n", eventname[amr->event]);
	printf("Start time ............. %s", ctime(&(amr->prevstarttime)));
	printf("Incident time .......... ");
	if (amr->eventtime > 0)
	    printf("%s", ctime(&(amr->eventtime)));
	else {
	    t = amr->prevstarttime + amr->uptime * 60;
	    printf("around %s", ctime(&t));
	}
	if ((amr->event > ev_deregistration || amr->event < ev_status_report) 
	     && amr->event != ev_livenotification) {
	    printf("Re-start time .......... %s", ctime(&(amr->starttime)));
	    printf("Uptime ................. %s\n", pretty_minutes(amr->uptime,
								   s));
	    printf("Downtime ............... %s\n", pretty_minutes(amr->dntime,
								   s));
	}
	if (amr->summarylen)
	    printf("Incident summary ....... \n%s\n", amr->summary);
    }
}
