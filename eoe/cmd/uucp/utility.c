/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.4 $"

#include "uucp.h"


static void logError(char *, char *, int, int, char*, int);

#define TY_ASSERT	1
#define TY_ERROR	2

/*
 *	produce an assert error message
 * input:
 *	s1 - string 1
 *	s2 - string 2
 *	i1 - integer 1 (usually errno)
 *	file - __FILE of calling module
 *	line - __LINE__ of calling module
 */
void
assert(s1, s2, i1, file, line)
char *s1, *s2, *file;
{
	logError(s1, s2, i1, TY_ASSERT, file, line);
}


/*
 *	produce an assert error message
 * input: -- same as assert
 */
void
errent(s1, s2, i1, file, line)
char *s1, *s2, *file;
{
	logError(s1, s2, i1, TY_ERROR, file, line);
}

#define EFORMAT	"%sERROR (%.9s)  pid: %d (%s) %s %s (%d) [FILE: %s, LINE: %d]\n"

static
void
logError(s1, s2, i1, type, file, line)
char *s1, *s2, *file;
{
	register FILE *errlog;
	char text[BUFSIZ];
	int pid;

	if (Debug)
		errlog = stderr;
	else {
		errlog = fopen(ERRLOG, "a");
		(void) chmod(ERRLOG, 0666);
	}
	if (errlog == NULL)
		return;

	pid = getpid();

	(void) fprintf(errlog, EFORMAT, type == TY_ASSERT ? "ASSERT " : " ",
	    Progname, pid, timeStamp(), s1, s2, i1, file, line);

	(void) fclose(errlog);
	(void) sprintf(text, " %sERROR %.100s %.100s (%.9s)",
	    type == TY_ASSERT ? "ASSERT " : " ",
	    s1, s2, Progname);
	if (type == TY_ASSERT)
	    systat(Rmtname, SS_ASSERT_ERROR, text, Retrytime);
	return;
}


/* timeStamp - create standard time string
 * return
 *	pointer to time string
 */

char *
timeStamp()
{
	register struct tm *tp;
	time_t clock;
	static char str[20];

	(void) time(&clock);
	tp = localtime(&clock);
	(void) sprintf(str, "%d/%d-%d:%2.2d:%2.2d", tp->tm_mon + 1,
	    tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
	return(str);
}
