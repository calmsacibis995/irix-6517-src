/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Exception raising and handling.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "exception.h"
/* #include "heap.h" */
#include "nstrings.h"

char	*exc_progname;
int	exc_autofail;
int	exc_defer;
int	exc_error;
int	exc_level = LOG_ERR;
char	*exc_message;

void	stdio_vperr(int, const char *, va_list);
void	(*exc_vperr)(int, const char *, va_list) = stdio_vperr;

void	stdio_vplog(int, int, const char *, va_list);
void	(*exc_vplog)(int, int, const char *, va_list) = stdio_vplog;

static int
exc_vnsprintf(char *buf, int bufsize, const char *format, va_list ap)
{
	int cc;

	cc = vnsprintf(buf, bufsize, format, ap);
	if (buf[cc-1] == '\n')
		buf[--cc] = '\0';
	return cc;
}

void
exc_vraise(int error, const char *format, va_list ap)
{
	int cc;
	char buf[BUFSIZ];

	if (exc_defer) {
		--exc_defer;	/* let a higher layer raise */
		return;
	}

	if (exc_autofail) {
		(*exc_vperr)(error, format, ap);
		exit(error ? error : -1);
		/* NOTREACHED */
	}

	cc = exc_vnsprintf(buf, sizeof buf, format, ap);
	exc_error = error;
	if (0 < error && error < sys_nerr)
		cc += (int) sprintf(buf + cc, ": %s", sys_errlist[error]);
	if (exc_message)
		free(exc_message);
	exc_message = (cc > 0) ? (char *)strndup(buf, cc) : (char *)0;
}

void
exc_raise(int error, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	exc_vraise(error, format, ap);
	va_end(ap);
}

void
exc_perror(int error, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	(*exc_vperr)(error, format, ap);
	va_end(ap);
}

void
exc_errlog(int level, int error, const char *format, ...)
{
	va_list ap;

	if (level <= exc_level) {
		va_start(ap, format);
		(*exc_vplog)(level, error, format, ap);
		va_end(ap);
	}

	if (exc_autofail && level < LOG_ERR)
		exit(error ? error : -1);
}


void
stdio_vperr(int error, const char *format, va_list ap)
{
	char buf[BUFSIZ];

	if (exc_progname)
		fprintf(stderr, "%s: ", exc_progname);
	(void) exc_vnsprintf(buf, sizeof buf, format, ap);
	fputs(buf, stderr);
	if (0 < error && error < sys_nerr)
		fprintf(stderr, ": %s", sys_errlist[error]);
	fputs(".\n", stderr);
}

void
stdio_vplog(int level, int error, const char *format, va_list ap)
{
	static char ebuf[20];
	static char *plevel[] = {
		"emergency",
		"alert",
		"critical",
		"error",
		"warning",
		"notice",
		"info",
		"debug",
		ebuf
	};

	if (exc_progname)
		fprintf(stderr, "%s: ", exc_progname);
	if (level > LOG_DEBUG) {
		(void) sprintf(ebuf, "level %d", level);
		level = LOG_DEBUG + 1;
	}
	fputs(plevel[level], stderr);
	if (format) {
		char buf[BUFSIZ];

		(void) exc_vnsprintf(buf, sizeof buf, format, ap);
		fprintf(stderr, ": %s", buf);
	}
	if (0 < error && error < sys_nerr)
		fprintf(stderr, ": %s", sys_errlist[error]);
	fputs(".\n", stderr);
}
