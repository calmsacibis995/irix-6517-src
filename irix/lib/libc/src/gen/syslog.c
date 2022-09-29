/*
 * SYSLOG -- print message on log file
 *
 * This routine looks a lot like printf, except that it outputs to the
 * log file instead of the standard output.  Also:
 *	adds a timestamp,
 *	prints the module name in front of the message,
 *	has some other formatting types (or will sometime),
 *	adds a newline on the end of the message.
 *
 * The output of this routine is intended to be read by syslogd(8).
 *
 * Author: Eric Allman
 * Modified to use UNIX domain IPC by Ralph Campbell
 */

#define _BSD_SIGNALS
#ifdef __STDC__
	#pragma weak syslog = _syslog
#ifndef _LIBC_ABI
	#pragma weak vsyslog = _vsyslog
#endif
	#pragma weak openlog = _openlog
	#pragma weak closelog = _closelog
	#pragma weak setlogmask = _setlogmask
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <strings.h>
#include <stropts.h>
#include <sys/log.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>	/* get waitpid() prototype */
#include <unistd.h> 	/* get getpid()/fork() prototype */
#include <stdlib.h> 	/* get write() prototype */
#include <fcntl.h>

#define	LOGNAME	"/dev/log"
#define	CONSOLE	"/dev/console"

static int	LogFile = -1;		/* fd for log */
static int	LogStat = 0;		/* status bits, set by openlog() */
static char	*LogTag = "syslog";	/* string to tag the entry with */
static int	LogFacility = LOG_USER;	/* default facility code */
static int	LogMask = 0xff;		/* mask of priorities to be logged */
static int	LogConsole = 0;		/* need to log to console ? */

#define _TBUF_LEN	4096
#define	_FMT_LEN	1024

void
syslog(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

static const char longmsg[] = "overly long syslog message detected, truncating\n";
static const char truncmsg[] = " [...truncated]";
#if 0
static const char attackmsg[] = "overly long syslog message detected, but handled\n";
static char abortmsg[] = "overly long syslog message, integrity compromised, aborting\n";
#endif

void
vsyslog(int pri, register const char *fmt, va_list ap)
{
	register unsigned int cnt;
	register char *p, *t;
	time_t now;
	register char ch;
	int plen;
	int tbuf_left, fmt_left;
	int saved_errno;
	pid_t pid;
	char fmt_cpy[_FMT_LEN];
#ifndef _LIBC_ABI
	char ttmp[32];
#endif
	char *stdp;
	int status;
	char tbuf[_TBUF_LEN];
	struct log_ctl hdr;
	struct strbuf dat;
	struct strbuf ctl;

#define _DEC() { \
		if (plen >= tbuf_left) \
			plen = tbuf_left - 1; \
		p += plen;	\
		tbuf_left -= plen; \
	     } 

	saved_errno = errno;

	/* see if we should just throw out this message */
	if ((u_int)LOG_FAC(pri) >= LOG_NFACILITIES ||
	    !(LOG_MASK(LOG_PRI(pri)) & LogMask) || 
	    (pri &~ (LOG_PRIMASK|LOG_FACMASK)))
		return;
	if (LogFile < 0)
		openlog(LogTag, LogStat | LOG_NDELAY, 0);

	/* set default facility if none specified */
	if ((pri & LOG_FACMASK) == 0)
		pri |= LogFacility;

	tbuf_left = sizeof(tbuf);
	p = tbuf;

	/* build the message */
	(void)time(&now);
	plen = sprintf(tbuf, "<%d>%.15s ", pri,
#ifndef _LIBC_ABI
		/* WARNING: use asctime_r here since ctime dirties
		   static data (cbuf[26]) that we don't want to dirty! */
		asctime_r(localtime(&now), ttmp) + 4);
#else
		/* ABI can't use asctime_r here since its not part of
		 * time.h header in ABI mode.
		 */
		asctime(localtime(&now)) + 4);
#endif
	_DEC();
	if (LogStat & LOG_PERROR)
		stdp = p;
	if (LogTag) {
		plen = snprintf(p, tbuf_left, "%s", LogTag);
		_DEC();
	}
	if (LogStat & LOG_PID) {
		plen = snprintf(p, tbuf_left, "[%d]", getpid());
		_DEC();
	}
	if (LogTag && *LogTag) {
		if (tbuf_left > 1) {
			*p++ = ':';
			tbuf_left--;
		}
		if (tbuf_left > 1) {
			*p++ = ' ';
			tbuf_left--;
		}
	}

	/* substitute error message for %m */
	for (t = fmt_cpy, fmt_left = _FMT_LEN; ch = *fmt; ++fmt) {
		if (ch == '%' && fmt[1] == 'm') {
			++fmt;
			plen = snprintf(t, fmt_left, "%s",
			    strerror(saved_errno));
			if (plen >= fmt_left)
				plen = fmt_left - 1;
			t += plen;
			fmt_left -= plen;
		} else {
			if (fmt_left > 1) {
				*t++ = ch;
				fmt_left--;
			}
		}
	}
	*t = '\0';

	plen = vsnprintf(p, tbuf_left, fmt_cpy, ap);
	_DEC();
	cnt = (int)(ptrdiff_t)(p - tbuf);

	/*
	 * now that we have snprintf() to cover our flanks, don't dump core,
	 * but warn the administrator that something odd may be happening.
	 */
	if (cnt > LOG_MAXPS) {
		write(LogFile,longmsg,sizeof(longmsg));
	}
#if 0
	if (plen > 2048) {
		write(LogFile,attackmsg,sizeof(attackmsg));
	}
	if (plen > 4000) {
		write(LogFile,abortmsg,sizeof(abortmsg));
		abort();
	}
#endif

	/* output to stderr if requested */
	if (LogStat & LOG_PERROR) {
		struct iovec iov[2];
		iov[0].iov_base = stdp;
		iov[0].iov_len = cnt - (int)(ptrdiff_t)(stdp - tbuf);
		iov[1].iov_base = "\n";
		iov[1].iov_len = 1;
		(void)writev(2, iov, 2);
	}
	hdr.level = 0;
	hdr.flags = SL_CONSOLE;
	hdr.pri = pri;
	ctl.maxlen = sizeof(struct log_ctl);
	ctl.len = sizeof(struct log_ctl);
	ctl.buf = (char*)&hdr;
	dat.maxlen = sizeof(tbuf);
	if (cnt > LOG_MAXPS) {
		dat.len = LOG_MAXPS;
		bcopy(truncmsg, tbuf+LOG_MAXPS-sizeof(truncmsg),
			sizeof(truncmsg));
	} else
		dat.len = cnt;
	dat.buf = tbuf;
	(void)putmsg(LogFile, &ctl, &dat, 0);

	if (!LogConsole)
		return;

	/* output the message to the console */
	pid = fork();
	if (pid == -1)
		return;
	if (pid == 0) {
		int fd;
		sigset_t sigs;
		struct iovec iov[2];

		(void)signal(SIGALRM, SIG_DFL);
		sigfillset(&sigs);
		sigdelset(&sigs, SIGALRM);
		sigprocmask(SIG_SETMASK, &sigs, NULL);
		(void)alarm((u_int)5);
		if ((fd = open(CONSOLE, O_WRONLY, 0)) < 0)
			_exit(0);
		(void)alarm((u_int)0);
		p = strchr(tbuf, '>') + 1;
		iov[0].iov_base = p;
		iov[0].iov_len = cnt - (int)(ptrdiff_t)(p - tbuf);
		iov[1].iov_base = "\r\n";
		iov[1].iov_len = 2;
		(void)writev(fd, iov, 2);
		(void)close(fd);
		_exit(0);
	}
	if (!(LogStat & LOG_NOWAIT))
		while ((waitpid(pid,&status,0) < 0) && (errno == EINTR))
			;
#undef _DEC
}


/* Allow other routines to tell if openlog has been called. */
int _using_syslog = 0;	

/*
 * OPENLOG -- open system log
 */
void
openlog(const char *ident, int logstat, int logfac)
{
	_using_syslog = 1;
	if (ident != NULL)
		LogTag = (char *)ident;
	LogStat = logstat;
	if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0)
		LogFacility = logfac;
	if (LogFile >= 0)
		return;
	if (LogStat & LOG_NDELAY) {
		LogFile = open(LOGNAME, O_WRONLY);
		fcntl(LogFile, F_SETFD, FD_CLOEXEC);
	}
}

/*
 * CLOSELOG -- close the system log
 */
void
closelog(void)
{
	(void) close(LogFile);
	LogFile = -1;
	LogConsole = 0;
}

/*
 * SETLOGMASK -- set the log mask level
 */

int
setlogmask(int pmask)
{
	int omask;

	omask = LogMask;
	if (pmask != 0)
		LogMask = pmask;
	return (omask);
}
