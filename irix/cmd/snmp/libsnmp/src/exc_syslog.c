/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Syslog exception interface.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include "exception.h"
#include "nstrings.h"

void
syslog_vplog(int level, int error, const char *format, va_list ap)
{
	int cc;
	char buf[BUFSIZ];

	cc = vnsprintf(buf, sizeof buf, format, ap);
	if (buf[cc-1] == '\n')
		--cc;
	if (0 < error && error < sys_nerr) {
		cc += nsprintf(buf + cc, sizeof buf - cc, ": %s",
			       sys_errlist[error]);
	}
	buf[cc] = '.';
	buf[cc+1] = '\0';
	syslog(level, buf);
}

void
syslog_vperr(int error, const char *format, va_list ap)
{
	syslog_vplog(LOG_ERR, error, format, ap);
}

void
exc_openlog(char *progname, int options, int facility)
{
	exc_progname = progname;
	exc_vperr = syslog_vperr;
	exc_vplog = syslog_vplog;
	openlog(progname, options, facility);
}
