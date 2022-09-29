/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.14 $
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sm_log.h>

static int logon;
static int loglevel = 0;

/*
 * Initialize log ops.
 */
void
sm_openlog(int on, int upto, const char *ident, int logopt, int facility)
{
	errno = 0;
	logon = on;
	loglevel = upto;
	if (on != SM_LOGON_SYSLOG)
		return;

	openlog(ident, logopt, facility);
}

void
sm_loglevel(int level)
{
	loglevel = level;
}

/*
 * Log errors to syslog or stderr
 */
/*VARARGS2*/
void
sm_log(int level, int issyserr, const char *format, ...)
{
    	va_list args;

	if (loglevel < level)
		return;
	if (level > LOG_DEBUG) {
		if (level != loglevel)
			return;
		level = 0;
	}

    	va_start(args, format);
    	if (logon == SM_LOGON_STDERR) {
	    	vfprintf(stderr, format, args);
		if (issyserr == SM_ISSYSERR)
	    		fprintf(stderr, ": %s\n", strerror(errno));
		else
			fputc('\n', stderr);

	} else if (issyserr == SM_ISSYSERR) {
		/* Ugly, but best suited for smt */
		char form[128];

		strncpy(form, format, sizeof(form)-1);
		form[sizeof(form)-1] = 0;
		vsyslog(level, strncat(form,": %m",sizeof(form)-1), args);
	} else {
		vsyslog(level, format, args);
	}
    	va_end(args);
}

#define MAX_DISPLAY	256

void
sm_hexdump(int level, const char *packet, int length)
{
#ifdef DEBUG
	char buf[MAX_DISPLAY*4+1];
	register char *cp;
	register int count;

	if ((loglevel < level) || (!packet))
		return;

	SNMPDEBUG((LOG_DBGSNMP, 0, "dumping %d bytes @%x:", length, packet));
	if (length > MAX_DISPLAY) {
		SNMPDEBUG((LOG_DBGSNMP, 0, "dumping %d of %d bytes @%x:",
			MAX_DISPLAY, length, packet));
		length = MAX_DISPLAY;
	}

	cp = buf;
	sprintf(cp, "\n");
	cp++;
	for (count = 0; count < length; count++) {
    		sprintf(cp, "%02X ", packet[count]);
		cp += 3;
    		if ((count % 16) == 15) {
			sprintf(cp, "\n");
			cp++;
		}
	}
	*cp = '\0';

	sm_log(level, 0, "%s\n", buf);
#endif
}
