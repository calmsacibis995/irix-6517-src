/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)syslogd.c	5.13 (Berkeley) 5/26/86";
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * NLOGS   -- the maximum number of simultaneous log files.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 */

/*
 * Define this to test for "out of disk space".  Creating the file
 * /tmp/<logfile> will cause <logfile> to appear to be out of space.
 *
 * #define DUMMY_NOSPC
 */

#define	NLOGS		50		/* max number of log files */
#define	MAXLINE		1024		/* maximum line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define MARKCOUNT	10		/* ratio of minor to major marks */

#define _BSD_SIGNALS
#define SSS_SUPPORT 1

#include <errno.h>
#include <stdio.h>
#include <utmp.h>
#include <ctype.h>
#include <signal.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef SSS_SUPPORT
#include <sys/un.h>
#endif
#include <sys/file.h>
#include <sys/msgbuf.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/statvfs.h>

#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <paths.h>
#include <sys/strlog.h>
#include <stropts.h>
#include <pfmt.h>
#include <cap_net.h>
#include <t6net.h>

#define	_PATH_KLOG	"/dev/klog"
#define	_PATH_LOGCONF	"/etc/syslog.conf"

#define	strcpyn(a, b, n)	strncpy(a, b, n)

int	flog;
char	*malloc();

char	*LogName =	_PATH_LOG;
char	*ConfFile =	_PATH_LOGCONF;
char	*ctty =		_PATH_CONSOLE;

#define FDMASK(fd)	(1 << (fd))

#define	dprintf		if (Debug) printf

#define UNAMESZ		8	/* length of a login name */
#define MAXUNAMES	20	/* maximum number of user names */
#define MAXFNAME	200	/* max file pathname length */

#define NOPRI		0x10	/* the "no priority" priority */
#define	LOG_MARK	(LOG_NFACILITIES << 3)	/* mark "facility" */

#define FORK_TIMEOUT	120	/* number of seconds between */
				/* failed fork attempts      */
#define FREEDISK	10240	/* amount of free disk space, in bytes, */
				/* required to restart logging to a full */
				/* partition */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define NOCOPY		0x004	/* don't suppress duplicate messages */
#define ADDDATE		0x008	/* add a date to the message */
#define MARK		0x010	/* this message is a mark */
#define DO_CONS		0x020	/* send this to the console */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	short	f_status;		/* status of filed */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	char	*f_filter;		/* path of message filter or NULL */
	union {
		char	f_uname[MAXUNAMES][UNAMESZ+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN+1];
			struct sockaddr_in	f_addr;
			unsigned short	f_nfail1;
			unsigned short	f_nfail2;
			time_t	        f_till;
			int		f_nmissed;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXFNAME];
#ifdef SSS_SUPPORT
                struct {
                        struct sockaddr_un su_addr; /* UNIX socket - destination address */
                        int su_addrlen;             /* UNIX socket - address len */
                } f_us;                 /* UNIX sockets support structure */
#endif
	} f_un;
};

#ifdef SSS_SUPPORT
#define fus_su  f_un.f_us.su_addr
#define fus_sal f_un.f_us.su_addrlen
#endif

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_DOCONS	7		/* special stub for lfmt -c */
#ifdef SSS_SUPPORT
#define F_UNIXSOCK      8               /* UNIX socket */
#endif
/* values for f_status */
#define S_OK		0		/* operational */
#define S_NOSPC		1		/* no space available */

#define FW_MSG1	"sendto <%s>: Failed %d times"
#define FW_MSG2	"sendto <%s>: No msgs. will be sent for %d seconds"
#define FW_MSG3	"sendto <%s>: Closing connection due to persistent network problems (last %d sends failed)"
#define FW_MSG4 "sendto <%s>: last %d messages lost"

#define FW_MAXFAIL1		5
#define FW_MAXFAIL2		5
#define FW_SILENT_INTERVAL	60 	/* seconds */

#ifdef SSS_SUPPORT
char	*TypeNames[9] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"DOCONS",
        "UNIXSOCK"
};
#else
char	*TypeNames[8] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"DOCONS"
};
#endif

struct code {
	char	*c_name;
	int	c_val;
};

struct code	PriNames[] = {
	"panic",	LOG_EMERG,
	"emerg",	LOG_EMERG,
	"alert",	LOG_ALERT,
	"crit",		LOG_CRIT,
	"err",		LOG_ERR,
	"error",	LOG_ERR,
	"warn",		LOG_WARNING,
	"warning",	LOG_WARNING,
	"notice",	LOG_NOTICE,
	"info",		LOG_INFO,
	"debug",	LOG_DEBUG,
	"none",		NOPRI,
	NULL,		-1
};

struct code	FacNames[] = {
	"kern",		LOG_KERN,
	"user",		LOG_USER,
	"mail",		LOG_MAIL,
	"daemon",	LOG_DAEMON,
	"auth",		LOG_AUTH,
	"security",	LOG_AUTH,
	"mark",		LOG_MARK,
	"syslog",	LOG_SYSLOG,
	"lpr",		LOG_LPR,
	"news",		LOG_NEWS,
	"uucp",		LOG_UUCP,
	"cron",		LOG_CRON,
	"local0",	LOG_LOCAL0,
	"local1",	LOG_LOCAL1,
	"local2",	LOG_LOCAL2,
	"local3",	LOG_LOCAL3,
	"local4",	LOG_LOCAL4,
	"local5",	LOG_LOCAL5,
	"local6",	LOG_LOCAL6,
	"local7",	LOG_LOCAL7,
	NULL,		-1
};

struct filed	Files[NLOGS];
extern void reapchild();

int	Debug;			/* debug flag */
#ifdef SSS_SUPPORT
int	sssTagOn = 1;		/* SSS tag enabled flag, default value is "on" */
#endif
char	LocalHostName[MAXHOSTNAMELEN+1];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	InetInuse = 0;		/* non-zero if INET sockets are being used */
int	LogPort;		/* port number for INET connections */
char	PrevLine[MAXLINE + 1];	/* copy of last line to supress repeats */
char	PrevHost[MAXHOSTNAMELEN+1];		/* previous host */
int	PrevFlags;
int	PrevPri;
int	PrevCount = 0;		/* number of times seen */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20;	/* interval between marks in minutes */
int	MarkSeq = 0;		/* mark sequence number */

int	finet, inetm;

static int redo_host_lookup;	/* gethostbyname failed during init() */

/* forward declarations */
void usage();
void opendevlog(), init(), untty(), logerror();
void cfline(), flushmsg(), wallmsg(), logmsg();
void domark();
void printline(), printlfmt(), printsys();

/* Flags set by signal handlers to indicate a need to call the routine */
static int need_inetbind;
static int need_init;
static int need_domark;

void
inithandler()
{
	need_init = 1;
}

void
bindhandler()
{
	need_inetbind = 1;
}

void
domarkhandler()
{
	need_domark = 1;
}


/* 
 * Initialize the syslog port used to listen for messages from other hosts
 * and to forward messages to other hosts. If we can't bind within the 
 * specified time limit (approx. 2 * MAXBINDTIMEOUT), stop trying and 
 * start the mark timer.
 */
#define MAXBINDTIMEOUT	160

void
tryinetbind(first)
	int first;
{
	struct servent *sp;
	struct sockaddr_in sin;
	static int timeout = 5;

	need_inetbind = 0;
	dprintf("tryinetbind %d\n", first);

	sp = getservbyname("syslog", "udp");
	if (sp == NULL)
		goto retry;
	sin.sin_family = AF_INET;
	sin.sin_port = sp->s_port;
	sin.sin_addr.s_addr = INADDR_ANY;
	bzero(sin.sin_zero, sizeof sin.sin_zero);
	if (cap_bind(finet, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
retry:
		if (timeout < MAXBINDTIMEOUT) {
			signal(SIGALRM, bindhandler);
			timeout *= 2;
			alarm(timeout);
			return;
		}
		/* Give up -- fall through ... */
	} else {
		inetm = FDMASK(finet);
		InetInuse = 1;
		LogPort = sp->s_port;
	}
	if (!first) {
		if (MarkInterval > 0) {
			(void) signal(SIGALRM, domarkhandler);
			(void) alarm(MarkInterval * 60 / MARKCOUNT);
		}
		init();		/* reconfigure forwarding ports */
	}
}

main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	register char *p;
	int fklog, klogm, len;
	struct sockaddr_in frominet;
	char line[MSG_BSIZE + 1];
	extern void die();
	int oerrno;
	int ch;

#ifdef SSS_SUPPORT
	while ((ch = getopt(argc, argv, "dsf:m:p:")) != EOF)
#else
	while ((ch = getopt(argc, argv, "df:m:p:")) != EOF)
#endif
		switch((char)ch) {
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg);	/* * 60? */
			break;
		case 'p':		/* path */
			LogName = optarg;
			break;
#ifdef SSS_SUPPORT
		case 's':		/* turn off SSS tag for output */
			sssTagOn = 0;
			break;
#endif
		case '?':
		default:
			usage();
		}
	if (argc -= optind)
		usage();

	if (!Debug) {
		if (fork())
			exit(0);
		for (i = 0; i < 10; i++)
			(void) close(i);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		untty();
	} else
		setlinebuf(stdout);

	(void) gethostname(LocalHostName, sizeof LocalHostName);
	if (p = index(LocalHostName, '.')) {
		*p++ = '\0';
		LocalDomain = p;
	}
	else
		LocalDomain = "";
	(void) signal(SIGTERM, die);
	(void) signal(SIGINT, Debug ? die : SIG_IGN);
	(void) signal(SIGQUIT, Debug ? die : SIG_IGN);
	(void) signal(SIGCHLD, reapchild);
	if (MarkInterval > 0) {
		(void) signal(SIGALRM, domarkhandler);
		(void) alarm(MarkInterval * 60 / MARKCOUNT);
	}

	opendevlog();
	finet = socket(AF_INET, SOCK_DGRAM, 0);
	if (finet != -1 && tsix_on (finet) == -1) {
		close(finet);
		finet = -1;
	}
	if (finet >= 0) {
		tryinetbind(1);
	}
	if ((fklog = open(_PATH_KLOG, O_RDONLY)) >= 0)
		klogm = FDMASK(fklog);
	else {
		dprintf("can't open %s (%d)\n", _PATH_KLOG,  errno);
		klogm = 0;
	}

	dprintf("off & running....\n");
	dprintf("unix=%d, inet=%d, klog=%d\n", flog, finet, fklog);

	init();
	(void) signal(SIGHUP, inithandler);

	for (;;) {
		int nfds, readfds = FDMASK(flog) | inetm | klogm;

		errno = 0;
		dprintf("readfds = %#x\n", readfds);
		nfds = select(20, (fd_set *) &readfds, (fd_set *) NULL,
				  (fd_set *) NULL, (struct timeval *) NULL);
		oerrno = errno;
		if (Debug) {
			if (nfds > 0) {
				printf("got a message (%d, %#x)\n",
				    nfds, readfds);
			} else {
				printf("select returned %d  %s\n",
				    nfds, nfds < 0 ? strerror(errno) : "");
			}
		}
		/* Check if a signal came in */
		if (need_inetbind)
			tryinetbind(0);
		if (need_init)
			init();
		if (need_domark) {
			domark();

			/* A previous gethostbyname failed to get an answer;
			 * try again in case the name server is now available.
			 */
			if (redo_host_lookup)
				init();
		}
		if (nfds <= 0) {
			if (nfds < 0 && oerrno != EINTR) {
				errno = oerrno;
				logerror("select");
			}
			continue;
		}

		if (readfds & klogm) {
			i = read(fklog, line, sizeof(line) - 1);
			if (i > 0) {
				line[i] = '\0';
				printsys(line);
			} else if (i < 0 && errno != EINTR) {
				logerror("klog");
				fklog = -1;
				klogm = 0;
			}
		}
		if (readfds & FDMASK(flog)) {
			struct log_ctl hdr;
			struct strbuf dat, ctl;
			int flags = 0;

			dat.maxlen = MAXLINE;
			dat.buf = line;
			ctl.maxlen = sizeof(struct log_ctl);
			ctl.buf = (caddr_t)&hdr;

			i = getmsg(flog, &ctl, &dat, &flags);

			if (i == 0 && dat.len > 0) {
				line[dat.len] = '\0';
				if ((hdr.pri & LOG_FACMASK) == LOG_LFMT)
					printlfmt(&hdr, line);
				else
					printline(LocalHostName, line);
			} else if (i < 0 && errno != EINTR) {
				close(flog);
				opendevlog();
			}
		}
		if (readfds & inetm) {
			len = sizeof frominet;
			i = recvfrom(finet, line, MAXLINE, 0, &frominet, &len);
			if (i > 0) {
				extern char *cvthname();

				line[i] = '\0';
				printline(cvthname(&frominet), line);
			} else if (i < 0 && errno != EINTR)
				logerror("recvfrom inet");
		}
	}
}

void
usage()
{
#ifdef SSS_SUPPORT
	fprintf(stderr, "usage: syslogd [-d] [-mmarkinterval] [-ppath] [-fconffile] [-s]\n");
#else
	fprintf(stderr, "usage: syslogd [-d] [-mmarkinterval] [-ppath] [-fconffile]\n");
#endif
	exit(1);
}

void
untty(void)
{
	int i;

	if (!Debug) {
		i = open(_PATH_TTY, O_RDWR);
		if (i >= 0) {
			(void) ioctl(i, (int) TIOCNOTTY, (char *)0);
			(void) close(i);
		}
	}
}

static int
cap_open(const char *file, int flags, ...)
{
	static const cap_value_t open_caps[] = {CAP_DAC_WRITE, CAP_MAC_WRITE};
	va_list ap;
	cap_t ocap;
	int r;

	ocap = cap_acquire(2, open_caps);
	va_start(ap, flags);
	if (flags & O_CREAT)
		r = open(file, flags, va_arg(ap, mode_t));
	else
		r = open(file, flags);
	va_end(ap);
	cap_surrender(ocap);
	return(r);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(hname, msg)
	char *hname;
	char *msg;
{
	register char *p, *q;
	register int c;
	char line[MAXLINE + 1];
	int pri;

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
repeat:
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
		if (pri <= 0 || pri >= (LOG_NFACILITIES << 3))
			pri = DEFUPRI;
	}

	/* don't allow users to log kernel messages */
	if ((pri & LOG_PRIMASK) == LOG_KERN)
		pri |= LOG_USER;

	q = line;

	while ((c = *p++ & 0177) != '\0' && c != '\n' &&
	    q < &line[sizeof(line) - 1]) {
		if (iscntrl(c)) {
			*q++ = '^';
			*q++ = c ^ 0100;
		} else
			*q++ = c;
	}
	*q = '\0';

#ifdef SSS_SUPPORT
	logmsg(pri, line, hname, 0, 0);
#else
	logmsg(pri, line, hname, 0);
#endif
	/*
	 * Named pipes don't preserve record boundaries, so more than
	 * one message can be retrieved in a single read.
	 * Skip duplicate line feeds and go print the next message
	 */
	if (c == '\n') {
		while (*p == '\n') ++p;
		if (*p != '\0') {
			pri = DEFUPRI;
			goto repeat;
		}
	}
}

/*
 * Message is in funny lfmt format
 */
void
printlfmt(hdr, msg)
	struct log_ctl *hdr;
	char *msg;
{
	long flags = 0;
	char lfmt[MAXLINE];
	char *lp, *nl;
	char *sev, *lab;
	int i;

	for (i = 0; i < sizeof(long); i++)
		flags = (flags << 8)|*msg++;
	
	if (flags & MM_CONSOLE)
		flags = DO_CONS;
	else
		flags = 0;

	lab = msg;
	while (*msg++ != '\0')
		;
	sev = msg;
	while (*msg++ != '\0')
		;

	strcpy(lfmt, ctime(&hdr->ttime)+4);
	lfmt[16] = '\0';
	if (*lab) {
		strcat(lfmt, lab);
		strcat(lfmt, ": ");
	}
	if (*sev) {
		strcat(lfmt, sev);
		strcat(lfmt, ": ");
	}
	lp = lfmt+strlen(lfmt);

	while (nl = strchr(msg, '\n')) {
		*nl = '\0';
		if (*msg) {
			strcat(lfmt, msg);
#ifdef SSS_SUPPORT
			logmsg(hdr->pri&LOG_PRIMASK, lfmt, LocalHostName, 0, 0);
#else
			logmsg(hdr->pri&LOG_PRIMASK, lfmt, LocalHostName, 0);
#endif
		}
		msg = nl+1;
		*lp = '\0';
	}
	if (*msg) {
		strcat(lfmt, msg);
#ifdef SSS_SUPPORT
		logmsg(hdr->pri&LOG_PRIMASK, lfmt, LocalHostName, flags, 0);
#else
		logmsg(hdr->pri&LOG_PRIMASK, lfmt, LocalHostName, flags);
#endif

	}
}
/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(msg)
	char *msg;
{
	register char *p, *q;
	register int c;
	char line[MAXLINE + 1];
#ifdef SSS_SUPPORT
	char line2[MAXLINE + 33];
        char *ctime_result;
	char *lp2;
	char *q2;
        char *p2;
#endif
	int pri, flags;
	char *lp;
	time_t now;

	(void) time(&now);
	if (*msg == '\n') {
		while (*++msg == '\n')
			;
		if (*msg == '\0')
			return;
	}
#ifdef SSS_SUPPORT
	ctime_result = ctime(&now);
	lp  = line + sprintf(line, "%.15s unix: ", ctime_result + 4);
	lp2 = line2 + sprintf(line2, "%.15s unix: ", ctime_result + 4);
        p2 = 0;
#else
	lp = line + sprintf(line, "%.15s unix: ", ctime(&now) + 4);
#endif
	for (p = msg; *p != '\0'; ) {
		flags = SYNC_FILE;	/* fsync file after write */
		pri = DEFSPRI;
		if (*p == '<') {
			pri = 0;
			while (isdigit(*++p))
				pri = 10 * pri + (*p - '0');
			if (*p == '>')
				++p;
			if (pri <= 0 || pri >= (LOG_NFACILITIES << 3))
				pri = DEFSPRI;
#ifdef SSS_SUPPORT
	                p2 = p;
			if(!sssTagOn && p[0] == '|' && p[1] == '$' && p[2] == '(') {
				p += 3;
				while(*p != '\0' && *p != ')') p++;
				if(*p == ')') p++;
				else p2 = 0;
			} else p2 = 0;
#endif
		} else {
			/* kernel printf's come out on console */
			flags |= IGN_CONS;
#ifdef SSS_SUPPORT
			p2 = 0;
#endif
		}
		q = lp;
		while (*p != '\0' && (c = *p++) != '\n' &&
		    q < &line[MAXLINE])
			*q++ = c;
		*q = '\0';
#ifdef SSS_SUPPORT
		if(p2) {
			q2 = lp2;
			while (*p2 != '\0' && (c = *p2++) != '\n' &&
			    q2 < &line2[MAXLINE])
				*q2++ = c;
			*q2 = '\0';
		}
		logmsg(pri, line, LocalHostName, flags, p2 ? line2 : 0);
#else
		logmsg(pri, line, LocalHostName, flags);
#endif
	}
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
#ifdef SSS_SUPPORT
logmsg(pri, msg, from, flags, msg2)
	int pri;
	char *msg, *msg2, *from;
	int flags;
#else
logmsg(pri, msg, from, flags)
	int pri;
	char *msg, *from;
	int flags;
#endif
{
	register struct filed *f;
	register int l;
	int fac, prilev;
	time_t now;
	int omask;
	struct iovec iov[7];
	register struct iovec *v = iov;
	char line[MAXLINE + 1];
	char *msgbase;
	int msglen;
	char prilev_char[4];
	char fac_string[LOG_NFACILITIES]="ABCDEFGHIJKLMNOPQRSTUVWX";
	int didconsole;
	char spc;

	didconsole = 0;

#ifdef SSS_SUPPORT
	dprintf("logmsg: pri %o, flags %x, from %s, msg %s, msg2 %s\n", pri, flags, from, msg, msg2);
#else
	dprintf("logmsg: pri %o, flags %x, from %s, msg %s\n", pri, flags, from, msg);
#endif
	omask = sigblock(sigmask(SIGHUP)|sigmask(SIGALRM));

	/*
	 * Check to see if msg looks non-standard.
	 */
	if (strlen(msg) < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	if (!(flags & NOCOPY)) {
		if (flags & (ADDDATE|MARK))
			flushmsg();
		else if (!strcmp(msg + 16, PrevLine + 16)
			 && PrevFlags == flags
			 && PrevPri == pri
			 && !strcmp(from, PrevHost)) {
			/* we found a match, update the time */
			(void) strncpy(PrevLine, msg, 15);
			PrevCount++;
			(void) sigsetmask(omask);
			return;
		} else {
			/* new line, save it */
			flushmsg();
			(void) strncpy(PrevLine, msg, sizeof(PrevLine)-1);
			(void) strncpy(PrevHost, from, sizeof(PrevHost)-1);
			PrevFlags = flags;
			PrevPri = pri;
		}
	}


	/* extract facility and priority level */
	fac = (pri & LOG_FACMASK) >> 3;
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	prilev = pri & LOG_PRIMASK;
	
	(void) time(&now);
	if (flags & ADDDATE)
		v->iov_base = ctime(&now) + 4;
	else
		v->iov_base = msg;
	v->iov_len = 15;
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	sprintf(prilev_char, "%1d%c:", prilev, fac_string[fac] );
	v->iov_base = prilev_char;
	v->iov_len = 3;
	v++;

	v->iov_base = from;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;
	if (flags & ADDDATE)
		v->iov_base = msg;
	else
		v->iov_base = msg + 16;
	v->iov_len = strlen(v->iov_base);
	msgbase = v->iov_base;
	msglen = v->iov_len;
	v++;

	
	/* log the message to the particular outputs */
	if (!Initialized) {
		int cfd = open(ctty, O_WRONLY);

		if (cfd >= 0) {
			v->iov_base = "\r\n";
			v->iov_len = 2;
			(void) writev(cfd, iov, 6);
			(void) close(cfd);
		}
		untty();
		(void) sigsetmask(omask);
		return;
	}
	for (f = Files; f < &Files[NLOGS]; f++) {
		char newmsg[MAXLINE+1];

		/* skip messages that are incorrect priority */
		if (f->f_pmask[fac] < prilev || f->f_pmask[fac] == NOPRI)
			continue;
		if (f->f_filter) {
			dprintf("Filtering to %s\n", f->f_filter);
			v[-1].iov_base = newmsg;
			v[-1].iov_len = MAXLINE;
			if (filtermsg(f->f_filter, msgbase, msglen, newmsg,
					&v[-1].iov_len,fac,prilev,from) == 0) {
				/* filter didn't want message logged */
				dprintf("filter IGNORED message\n");
				v[-1].iov_base = msgbase;
				v[-1].iov_len = msglen;
				continue;
			}
		}

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < (MarkInterval * 60 / 2))
			continue;

		dprintf("Logging to %s", TypeNames[f->f_type]);
		f->f_time = now;
		switch (f->f_type) {
		case F_UNUSED:
			dprintf("\n");
			break;
#ifdef SSS_SUPPORT
                case F_UNIXSOCK:
                        dprintf("%s\n",f->fus_su.sun_path);
			(void) sprintf(line, "<%d><%.15s> %s", pri, iov[0].iov_base, msg2 ?
				       ((flags & ADDDATE) ? msg2 : msg2+16) : (char*)iov[5].iov_base);
			l = strlen(line);
			if (l > MAXLINE) l = MAXLINE;
                        if(sendto(f->f_file, line, l, 0, (struct sockaddr*) &f->fus_su, f->fus_sal) != l) {
                          dprintf("sendto error - %s\n",strerror(errno)); l = 0;
                        }
                        break;
#endif
		case F_FORW:
			dprintf(" %s\n", f->f_un.f_forw.f_hname); 

			/*
			 * multihop:  If we're forwarding an already-
			 * forwarded msg, substitute the fromhost
			 * for the date field.
			 */
			if (strcmp(from, LocalHostName) != 0)
				(void) sprintf(line, "<%d>%s %s",
				pri,
				iov[3].iov_base,
				iov[5].iov_base);
			else
				(void) sprintf(line, "<%d>%.15s %s", pri,
				iov[0].iov_base, iov[5].iov_base);

			l = strlen(line);
			if (l > MAXLINE)
				l = MAXLINE;
			if (f->f_un.f_forw.f_nfail1 >= FW_MAXFAIL1) {
				time_t now;

				now = time(0);
				if (now < f->f_un.f_forw.f_till) {
					/*
					 * Not enough time elapsed since last
					 * f_nfail1 errors.
					 */
					f->f_un.f_forw.f_nmissed++;
					break;
				}
				f->f_un.f_forw.f_nfail1 = 0;
			}
			if (sendto(f->f_file, line, l, 0,
			    &f->f_un.f_forw.f_addr,
			    sizeof f->f_un.f_forw.f_addr) != l) {
				/* 
				 * if error is operational error, then
				 * ignore a few of them.
				 */
				int e = errno;

				if ((e == ENETDOWN) || (e == ENETUNREACH) ||
					(e == ENETRESET))
				{
					char emsg[MAXHOSTNAMELEN+100];

					f->f_un.f_forw.f_nmissed++;
					f->f_un.f_forw.f_nfail1++;
					if (f->f_un.f_forw.f_nfail1 < FW_MAXFAIL1)
						break;

					sprintf(emsg, FW_MSG1,
					      f->f_un.f_forw.f_hname, FW_MAXFAIL1);
					logerror(emsg);

					f->f_un.f_forw.f_nfail2++;
					if (f->f_un.f_forw.f_nfail2 < FW_MAXFAIL2)
					{
						time_t now;
					
						now = time(0);
						f->f_un.f_forw.f_till = now + FW_SILENT_INTERVAL;
						sprintf(emsg, FW_MSG2,
							f->f_un.f_forw.f_hname,
							FW_SILENT_INTERVAL);
						errno = e;
						logerror(emsg);
						break;
					}
					sprintf(emsg, FW_MSG3,
						f->f_un.f_forw.f_hname,
							FW_MAXFAIL1 * FW_MAXFAIL2);
					errno = e;
					logerror(emsg);
				}
				(void) close(f->f_file);
				f->f_type = F_UNUSED;
				errno = e;
				if ((e != ENETDOWN) && (e != ENETUNREACH) &&
				     (e != ENETRESET))
					logerror("sendto");
			} else {
				char emsg[MAXHOSTNAMELEN+100];

				/* every successful send resets fail windows */
				f->f_un.f_forw.f_nfail1 = 0;
				f->f_un.f_forw.f_nfail2 = 0;
				f->f_un.f_forw.f_till = 0;
				if (f->f_un.f_forw.f_nmissed) {
					sprintf(emsg, FW_MSG4,
						f->f_un.f_forw.f_hname,
						f->f_un.f_forw.f_nmissed);
					f->f_un.f_forw.f_nmissed = 0;
					errno = 0;
					logerror(emsg);
				}
			}
			break;

		case F_DOCONS:
			if (!(flags & DO_CONS) || didconsole) {
				dprintf(" (ignored)\n");
				break;
			}
			/* FALL THROUGH */
		case F_CONSOLE:
			if ((flags & IGN_CONS) || didconsole) {
				dprintf(" (ignored)\n");
				break;
			}
			didconsole = 1;
			f->f_file = cap_open(f->f_un.f_fname, O_WRONLY);
			/* FALL THROUGH */

		case F_TTY:
		case F_FILE:
			dprintf(" %s\n", f->f_un.f_fname);
			if (f->f_type != F_FILE) {
				v->iov_base = "\r\n";
				v->iov_len = 2;
			} else {
				v->iov_base = "\n";
				v->iov_len = 1;
			}
			
			/*
			 * Prior to doing the write, see if we are in
			 * an error condition.
			 */
			switch (f->f_status) {
			case S_OK:
			default:
				break;
			case S_NOSPC:
				/*
				 * If we are in a low disk space situation,
				 * check to see if there is any free space
				 * available on the partition.  There has
				 * to be at least FREEDISK bytes available
				 * to continue.
				 */
				if (chkdiskspace(f)) {
					char nbuf[MAXFNAME + 32];
					f->f_file = cap_open(f->f_un.f_fname,
						O_WRONLY|O_APPEND|O_CREAT,
						0644);
					if (f->f_file < 0) {
						f->f_type = F_UNUSED;
						logerror(f->f_un.f_fname);
					} else {
						sprintf(nbuf,
						    "%s: logging reenabled",
						    f->f_un.f_fname);
						f->f_status = S_OK;
						errno = 0;
						logerror(nbuf);
					}
				}
				break;
			}
			
			/*
			 * Check the status again.  If it's not S_OK,
			 * we're going to skip and go on.
			 */
			if (f->f_status != S_OK) {
				dprintf("\n");
				break;	/* end of case */
			}

#ifdef DUMMY_NOSPC
			spc = forcediskspace(f);
#else /* DUMMY_NOSPC */
			spc = 1;
#endif /* DUMMY_NOSPC */
			
			/*
			 * Otherwise, attempt to write out the log data
			 */
#ifdef DUMMY_NOSPC
			if (spc) {
				spc = (writev(f->f_file, iov, 7) >= 0);
			} else {
				errno = ENOSPC;
			}
			if (!spc) {
#else /* DUMMY_NOSPC */
			if (writev(f->f_file, iov, 7 ) < 0) {
#endif /* DUMMY_NOSPC */
				int e = errno;
				(void) close(f->f_file);
				f->f_file = -1;

				/*
				 * Check for EIO on TTY's due to vhangup() XXX
				 */
				if (e == EIO && f->f_type != F_FILE) {
					f->f_file = cap_open(f->f_un.f_fname, O_WRONLY|O_APPEND|O_CREAT, 0644);
					if (f->f_file < 0) {
						f->f_type = F_UNUSED;
						logerror(f->f_un.f_fname);
					}
				} else if (e == ENOSPC &&
						f->f_type == F_FILE) {
					/*
					 * Set the type to disk full
					 */
					f->f_status = S_NOSPC;
					errno = e;
					logerror(f->f_un.f_fname);
				} else {
					f->f_type = F_UNUSED;
					errno = e;
					logerror(f->f_un.f_fname);
				}
			} else if (flags & SYNC_FILE)
				(void) fsync(f->f_file);
			if (f->f_type == F_CONSOLE || f->f_type == F_DOCONS) {
				close(f->f_file);
				f->f_file = -1;
			}
			break;

		case F_USERS:
		case F_WALL:
			dprintf("\n");
			v->iov_base = "\r\n";
			v->iov_len = 2;
			wallmsg(f, iov);
			break;
		}
		if (f->f_filter) {
			v[-1].iov_base = msgbase;
			v[-1].iov_len = msglen;
		}
	}
	(void) sigsetmask(omask);
}


static int ftimeout;		/* if 1, filter didn't respond in time */

static void
filter_timeout()
{
	ftimeout = 1;
	dprintf("filter timeout\n");
}

filtermsg(filter, msg, len, newmsg, newlen,facility, priority, fromstr)
	char *filter;
	char *msg;
	int len;
	char *newmsg;
	int *newlen;	/* in/out parameter */
	int facility;
	int priority;
	char *fromstr;
{
	int ip[2], op[2];
	int pid, n, wpid;
	union wait status;
	char *whyfail;
	char msgbuf[128];
	SIG_PF oldreap;
	SIG_PF oldalarm;
	int omask;
	struct itimerval it, oit;
#ifdef SSS_SUPPORT
	static int fork_restart_time = 0;
#else
	static fork_restart_time = 0;
#endif

	/*
	 * If a previous fork attempt failed due to EAGAIN, wait for the
	 * timeout period to expire before trying to run a filter again.
	 * This way we don't flood SYSLOG with messages about syslogd
	 * not being able to fork.  Just return the message unfiltered.
	 */

	if (fork_restart_time)
		if (fork_restart_time > time(NULL)) {
			strncpy(newmsg, msg, *newlen-1);
			*newlen = strlen(newmsg);
			return 1;
		} else
			fork_restart_time = 0;

	/*
	 * Don't let reapchild() reap 'em -- we want to get the exit status.
	 */
	oldreap = signal(SIGCHLD, SIG_DFL);

	timerclear(&it.it_interval);
	timerclear(&it.it_value);
	(void) setitimer(ITIMER_REAL, &it, &oit);
	oldalarm = signal(SIGALRM, filter_timeout);

	/* Enable SIGALRM */
	omask = sigsetmask(sigblock(0) & ~sigmask(SIGALRM));

	/*
	 * Create two pipes, fork, and run the filter.
	 */
	ip[0] = ip[1] = op[0] = op[1] = -1;
	if (pipe(ip) < 0 || pipe(op) < 0)
		goto sysfail;
	pid = fork();
	if (pid < 0)
		goto sysfail;
	if (pid == 0) {
		char envstr[128];
		int i;

		for ( i = 0; FacNames[i].c_name; i++ )
			if ((FacNames[i].c_val >> 3) == facility)
				break;

		if (FacNames[i].c_name)
			sprintf(envstr, "FACILITY=%s", FacNames[i].c_name);
		else
			sprintf(envstr, "FACILITY=%d", facility);
		putenv(strdup(envstr));

		for ( i = 0; PriNames[i].c_name; i++ )
			if (PriNames[i].c_val == priority)
				break;

		if (PriNames[i].c_name)
			sprintf(envstr, "PRIORITY=%s", PriNames[i].c_name);
		else
			sprintf(envstr, "PRIORITY=%d", priority);
		putenv(strdup(envstr));

		sprintf(envstr, "FROM=%s", fromstr);
		putenv(strdup(envstr));

		(void) close(ip[1]);
		(void) close(op[0]);
		dup2(ip[0], 0);
		dup2(op[1], 1);
		dup2(op[1], 2);
		setpgrp();	/* so we can kill entire process group if
				 * process is too slow. */
		for (n = getdtablehi(); --n > 2; )
			(void) close(n);
		execlp(filter, filter, 0);
		exit(-1);
	}
	dprintf("filter: child %d\n", pid);

	/* Parent writes msg to ip, and waits up to 8 seconds for new msg */
	if (write(ip[1], msg, len) < 0)
		goto sysfail;
	(void) close(ip[1]);
	(void) close(op[1]);	/* close write ends */

	/* If filter exits without writing anything into the pipe, that
	 * means that the filter didn't want this particular message
	 * logged.  (If the filter is 'bad', or hung up, then the alarm
	 * will go off, and we'll get back -1).
	 */

	ftimeout = 0;
	alarm(8);
	n = read(op[0], newmsg, *newlen);
	if (n < 0) {
		if (ftimeout)
			whyfail = "too slow";
		else
			whyfail = strerror(errno);
		/* Don't go to fail or sysfail yet, still need to do the wait */
	}

	/*
	 * Child should have always started by this point, since the
	 * read will block until the alarm expires or the filter completes.
	 */
	ftimeout = 0;
	alarm(2);
	wpid = waitpid(pid, (int *)&status, 0);
	alarm(0);

	if (n < 0)
		goto fail;
	else if (wpid == -1) {
		whyfail = msgbuf;
		if (ftimeout) {
			strcpy(whyfail, "filter took too long to exit");
			kill(-pid, SIGKILL);	/* kill entire process group */
			sleep(1);
			reapchild();
		} else
			sprintf(whyfail, "wait for filter failed: %s",
				strerror(errno));
		goto fail;
	} else if (status.w_retcode) {
		whyfail = msgbuf;
		sprintf(whyfail, "exit status 0x%x", status.w_retcode);
		goto fail;
	} else if (status.w_termsig) {
		whyfail = msgbuf;
		sprintf(whyfail, "killed by signal %d", status.w_termsig);
		goto fail;
	}

	newmsg[n] = '\0';
	*newlen = n;
	goto out;

sysfail:
	if (errno == EAGAIN)
		fork_restart_time = time(NULL) + FORK_TIMEOUT;

	whyfail = strerror(errno);
fail:
	*newlen = sprintf(newmsg, "%s [filter %s failed: %s]",
			  msg, filter, whyfail);
	n = 1;	/* we have something to log */
out:
	(void) close(ip[0]);
	(void) close(ip[1]);
	(void) close(op[0]);
	(void) close(op[1]);
	(void) signal(SIGALRM, oldalarm);
	(void) setitimer(ITIMER_REAL, &oit, NULL);
	(void) signal(SIGCHLD, oldreap);
	(void) sigsetmask(omask);
	return n;
}


/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(f, iov)
	register struct filed *f;
	struct iovec *iov;
{
	register char *p;
	register int i;
	int ttyf, len;
	FILE *uf;
	static int reenter = 0;
	register struct utmp *ut;
	time_t now;
	char greetings[200];
	int pid;

	if (reenter++)
		return;

	(void) time(&now);
	(void) sprintf(greetings,
	    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		iov[2].iov_base, ctime(&now));
	len = strlen(greetings);

	/* scan the (automatically opened) user login file */
	while (ut = getutent()) {
		/* is this slot used? */
		if (ut->ut_name[0] == '\0')
			continue;
		if (ut->ut_type != USER_PROCESS)
			continue;
		/*
		 * Don't allow someone to send to other than /dev devices
		 * (covers a security hole if ut_line is a filename).
		 */
		if (index(ut->ut_line, '/') != NULL)
			continue;

		/* should we send the message to this user? */
		if (f->f_type == F_USERS) {
			for (i = 0; i < MAXUNAMES; i++) {
				if (!f->f_un.f_uname[i][0]) {
					i = MAXUNAMES;
					break;
				}
				if (strncmp(f->f_un.f_uname[i], ut->ut_name,
				    UNAMESZ) == 0)
					break;
			}
			if (i >= MAXUNAMES)
				continue;
		}

		/* compute the device name */
		p = "/dev/12345678";
		strcpyn(&p[5], ut->ut_line, UNAMESZ);

		/*
		 * Might as well fork instead of using nonblocking I/O
		 * and doing notty().
		 */
		if ((pid = fork()) == 0) {
			if (f->f_type == F_WALL) {
				iov[0].iov_base = greetings;
				iov[0].iov_len = len;
				iov[1].iov_len = 0;
			}
			(void) sigsetmask(0);
			(void) signal(SIGALRM, SIG_DFL);
			(void) alarm(30);
			/* open the terminal */
			ttyf = cap_open(p, O_WRONLY);
			if (ttyf >= 0)
				(void) writev(ttyf, iov, 6);
			exit(0);
		}
		dprintf("wallmsg: child %d\n", pid);
		/* avoid having them all pile up at once */
		sleep(1);
	}
	/* close the user login file */
	(void) endutent();
	reenter = 0;
}

void
reapchild()
{
	union wait status;
	int pid;

	while ((pid=wait3((int *)&status, WNOHANG, (struct rusage *) NULL)) > 0)
		dprintf("reaped %d\n", pid);
}

/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr_in *f;
{
	struct hostent *hp;
	register char *p;
	extern char *inet_ntoa();

	dprintf("cvthname(%s)\n", inet_ntoa(f->sin_addr));

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address\n");
		return ("???");
	}
	hp = gethostbyaddr(&f->sin_addr, sizeof(struct in_addr), f->sin_family);
	if (hp == 0) {
		dprintf("Host name for your address (%s) unknown\n",
			inet_ntoa(f->sin_addr));
		return (inet_ntoa(f->sin_addr));
	}
	if ((p = index(hp->h_name, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hp->h_name);
}

void
domark()
{
	need_domark = 0;
	dprintf("domark\n");

	if ((++MarkSeq % MARKCOUNT) == 0)
#ifdef SSS_SUPPORT
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK, 0);
#else
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
#endif
	else
		flushmsg();
	alarm(MarkInterval * 60 / MARKCOUNT);
}

void
flushmsg()
{
	if (PrevCount == 0)
		return;
	if (PrevCount > 1)
		(void) sprintf(PrevLine+16, "last message repeated %d times", PrevCount);
	PrevCount = 0;
#ifdef SSS_SUPPORT
	logmsg(PrevPri, PrevLine, PrevHost, PrevFlags|NOCOPY, 0);
#else
	logmsg(PrevPri, PrevLine, PrevHost, PrevFlags|NOCOPY);
#endif
	PrevLine[0] = '\0';
}

/*
 * Print syslogd errors some place.
 */
void
logerror(type)
	char *type;
{
	char buf[MAXFNAME + 80];
	int mask = LOG_SYSLOG;

	switch (errno) {
	case 0:
		(void) sprintf(buf, "syslogd: %s", type);
		mask |= LOG_ERR;
		break;
	case ENOSPC: 
		(void) sprintf(buf, "syslogd: %s: %s", type, strerror(errno));
		mask |= LOG_ALERT;
		break;
	default:
		(void) sprintf(buf, "syslogd: %s: %s", type, strerror(errno));
		mask |= LOG_ERR;
		break;
	}

	dprintf("%s\n", buf);
#ifdef SSS_SUPPORT
	logmsg(mask, buf, LocalHostName, ADDDATE, 0);
#else
	logmsg(mask, buf, LocalHostName, ADDDATE);
#endif
}

void
die(sig)
{
	char buf[100];

	if (sig) {
		dprintf("syslogd: going down on signal %d\n", sig);
		flushmsg();
		(void) sprintf(buf, "going down on signal %d", sig);
		logerror(buf);
	}
	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */

void
init(void)
{
	register int i;
	register FILE *cf;
	register struct filed *f;
	register char *p;
	char cline[MAXLINE], cbuffer[MAXLINE];
	int omask;
	int restarting;

	/* Can be restarting or rebinding (name lookup) */
	restarting = (need_init || !Initialized) ? 1 : 0;

	need_init = 0;

	dprintf("init\n");

	omask = sigblock(sigmask(SIGHUP)|sigmask(SIGALRM));

	/* flush any pending output */
	flushmsg();

	/*
	 *  Close all open log files.
	 */
	for (f = Files; f < &Files[NLOGS]; f++) {
		if (f->f_filter) {
			free(f->f_filter);
			f->f_filter = NULL;
		}
		if (f->f_type == F_FILE || f->f_type == F_TTY
#ifdef SSS_SUPPORT
		    || f->f_type == F_FORW || f->f_type == F_UNIXSOCK)
#else
 		    || f->f_type == F_FORW)
#endif
			(void) close(f->f_file);
		f->f_type = F_UNUSED;
	}

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		cfline("*.ERR\t/dev/console", &Files[0]);
		cfline("*.PANIC\t*", &Files[1]);
#ifdef SSS_SUPPORT
		cfline("*.DEBUG\t@@/tmp/.eventmond.events.sock", &Files[2]);
#endif
		(void) sigsetmask(omask);
		return;
	}
	setbuffer(cf, cbuffer, sizeof cbuffer);

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	redo_host_lookup = 0;
	f = Files;
	while (fgets(cline, sizeof cline, cf) != NULL && f < &Files[NLOGS]) {
		/* check for end-of-section */
		if (cline[0] == '\n' || cline[0] == '#')
			continue;

		/* strip off trailing whitespace & newline character */
		p = index(cline, '\n');
		if (p)
			do
				*p-- = '\0';
			while (p >= cline && isspace(*p));
		
		cfline(cline, f++);
	}
	if( f < &Files[NLOGS]) {
		/* special dummy entry for lfmt -c */
		cfline("*.DEBUG\t/dev/console", f);
		f->f_type = F_DOCONS;
	}
	else
		dprintf("More than %d configuration lines in config file, rest ignored\n", NLOGS);

	/* close the configuration file */
	(void) fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f < &Files[NLOGS]; f++) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("(%d) ", f->f_file);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
				printf("'%s'", f->f_un.f_fname);
				break;

			case F_FORW:
				printf("'%s'", f->f_un.f_forw.f_hname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("'%s', ", f->f_un.f_uname[i]);
				break;
			}
			if (f->f_filter)
				printf(" '%s'\n", f->f_filter);
			printf("\n");
		}
	}

	(void) sigsetmask(omask);

	if (0 == restarting)
		return;

#ifdef SSS_SUPPORT
	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE, 0);
#else
	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
#endif
	dprintf("syslogd: restarted\n");
}


void
cfline(line, f)
	char *line;
	register struct filed *f;
{
	register char *p;
	register char *q;
	register int i;
	char *bp;
	int pri;
	struct hostent *hp;
	char buf[MAXLINE];

	dprintf("cfline(%s)\n", line);

	errno = 0;	/* keep sys_errlist stuff out of logerror messages */

	/* clear out file entry */
	bzero((char *) f, sizeof *f);
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = NOPRI;

	if (isspace(*line)) {	/* leading whitespace is not permitted */
		logerror("invalid line: leading whitespace is not permitted");
		return;
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !index("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (index(", ;", *q))
			q++;

		/* decode priority name */
		pri = decode(buf, PriNames);
		if (pri < 0) {
			char xbuf[200];

			(void) sprintf(xbuf, "unknown priority name \"%s\"", buf);
			logerror(xbuf);
			return;
		}

		/* scan facilities */
		while (*p && !index("\t.;", *p)) {
			int i;

			for (bp = buf; *p && !index("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
			else {
				i = decode(buf, FacNames);
				if (i < 0) {
					char xbuf[200];

					(void) sprintf(xbuf, "unknown facility name \"%s\"", buf);
					logerror(xbuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

get_action:
	/* skip to action part */
	while (isspace(*p))
		p++;

	switch (*p)
	{
	case '@':
#ifdef SSS_SUPPORT
                if(*(p+1) == '@') {
                  p++;
                  bzero((char *)&f->fus_su,sizeof(f->fus_su));
		  (void) strncpy(f->fus_su.sun_path, ++p, sizeof(f->fus_su.sun_path)-1);
                  f->fus_su.sun_family = AF_UNIX;
		  f->f_file = socket(AF_UNIX, SOCK_DGRAM, 0);
		  if (f->f_file < 0) {
		        logerror("Can't create UNIX socket");
			break;
		  }
                  f->fus_sal = strlen(f->fus_su.sun_path)+sizeof(f->fus_su.sun_family);
	          f->f_type = F_UNIXSOCK;
		  break;
                }
#endif
		if (!InetInuse)
			break;
		(void) strncpy(f->f_un.f_forw.f_hname, ++p, sizeof(f->f_un.f_forw.f_hname)-1);
		hp = gethostbyname(p);
		if (hp == NULL) {
			char buf[200];

			(void) sprintf(buf, "can't forward to '%s': %s",
				p, hstrerror(h_errno));
			errno = 0;
			logerror(buf);
			redo_host_lookup = 1;
			break;
		}
		bzero((char *) &f->f_un.f_forw.f_addr,
			 sizeof f->f_un.f_forw.f_addr);
		f->f_un.f_forw.f_addr.sin_family = AF_INET;
		f->f_un.f_forw.f_addr.sin_port = LogPort;
		bcopy(hp->h_addr, (char *) &f->f_un.f_forw.f_addr.sin_addr,
			hp->h_length);
		f->f_file = socket(AF_INET, SOCK_DGRAM, 0);
		if (f->f_file < 0) {
			logerror("socket");
			break;
		}
		f->f_type = F_FORW;
		f->f_un.f_forw.f_nfail1 = 0;
		f->f_un.f_forw.f_nfail2 = 0;
		f->f_un.f_forw.f_till = 0;
		f->f_un.f_forw.f_nmissed = 0;
		break;

	case '/':
		(void) strncpy(f->f_un.f_fname, p, sizeof(f->f_un.f_fname)-1);
		if ((f->f_file = cap_open(p, O_WRONLY|O_APPEND|O_CREAT,0644)) < 0) {
			f->f_file = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			untty();
		}
		else
			f->f_type = F_FILE;
		if (strcmp(p, ctty) == 0)
			close(f->f_file),	/* keep closed between writes */
			f->f_file = -1,
			f->f_type = F_CONSOLE;
		break;

	case '|':
		if (f->f_filter) {	/* catch multiple filters */
			errno = EINVAL;
			logerror(p);
			break;
		}
		/* skip white space between | and program name */
		do {
			p++;
		} while (isspace(*p));
		/* find end of program */
		for (q = p; !isspace(*q); q++)
			continue;
		*q = '\0';
		f->f_filter = strdup(p);
		if (f->f_filter == NULL) {
			logerror(p);
			break;
		}
		p = q+1;
		goto get_action;

	case '*':
		f->f_type = F_WALL;
		break;

	case '\0':
		if (f->f_filter) {	/* no action for filter process */
			char buf[100];
			errno = EINVAL;
			(void) sprintf(buf, "%s <no action>", f->f_filter);
			logerror(buf);
			free(f->f_filter);
			f->f_filter = NULL;
		}
		break;		/* leave f->f_type set to F_UNUSED */
	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void) strncpy(f->f_un.f_uname[i], p, UNAMESZ);
			if ((q - p) > UNAMESZ)
				f->f_un.f_uname[i][UNAMESZ] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || isspace(*q))
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */

decode(name, codetab)
	char *name;
	struct code *codetab;
{
	register struct code *c;
	register char *p;
	char buf[40];

	if (isdigit(*name))
		return (atoi(name));

	(void) strncpy(buf, name, sizeof(buf)-1);
	for (p = buf; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

void
opendevlog()
{
	struct strioctl strio;

	flog = open(_PATH_LOG, O_RDWR);
	if (flog < 0) {
		logerror("open of " _PATH_LOG);
		die(0);
	}
	strio.ic_cmd = I_CONSLOG;
	strio.ic_timout = 0;
	strio.ic_len = 0;
	strio.ic_dp = 0;

	if (ioctl(flog,I_STR,&strio) < 0)
		logerror("set I_CONSLOG");
}

/*
 * Return 1 if there is more than FREEDISK bytes available on the disk
 * partition represented by the 'filed', 0 otherwise.
 */
int
chkdiskspace(struct filed *f)
{
#ifdef DUMMY_NOSPC
	if (!forcediskspace(f))
		return 0;
#else /* DUMMY_NOSPC */
	struct statvfs64 s;
	__int64_t sz;
	int bsize;
	
	if (statvfs64(f->f_un.f_fname, &s) < 0) {
		return 0;
	}
	
	/*
	 * Compute the size
	 */
	bsize = s.f_frsize ? s.f_frsize : s.f_bsize;
	sz = s.f_bfree * bsize;
	if (sz >= FREEDISK)
		return 1;
	else
		return 0;
#endif /* DUMMY_NOSPC */
}

#ifdef DUMMY_NOSPC
/*
 * Decide if we're forcing a disk no space error.
 */
int
forcediskspace(struct filed *f)
{
	char buf[MAXFNAME + 32];
	struct stat st;
	
	sprintf(buf, "/tmp%s", f->f_un.f_fname);
	if (stat(buf, &st) < 0)
		return 1;
	else
		return 0;
}
#endif /* DUMMY_NOSPC */
