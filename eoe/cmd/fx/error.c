#include "fx.h"
#include <stdarg.h>
extern int errno;

extern char fulldrivename[];
extern int nomenus;	/* see fx.c, -c option */
extern char *script; /* see fx.c, -s option */

/* NOTE: the output of these routines is parsed by the GUI disk setup tool!
 * (in particular, the "fx:  {Warning|Error}: " stuff.
*/


void
argerr(char *msg, ...)
{
	va_list arg;

	prompt(": ");
	printf("Error:  ");
	va_start(arg, msg);
	vprintf(msg, arg);
	va_end(arg);
	printf("\n");
	flushoutp();
	mpop();
}

errwarn(char *msg, ...)
{
	va_list arg;

	prompt(": ");
	printf("Warning:  ");
	va_start(arg, msg);
	vprintf(msg, arg);
	va_end(arg);
	printf("\n");
	flushoutp();
	return -1;
}

void
scerrwarn(char *msg, ...)
{
	register int x;
	char msgbuf[512];
	int len;
	va_list arg;

	x = errno;

	prompt(": ");
	if(x)
		sprintf(msgbuf,"Error:  %s:  ", strerror(x));
	else
		sprintf(msgbuf,"Error:  ");
	va_start(arg, msg);
	vsprintf(msgbuf+strlen(msgbuf), msg, arg);
	va_end(arg);
	len = strlen(msgbuf);
	if(msgbuf[len-1] != '\n') {
		msgbuf[len] = '\n';
		msgbuf[len+1] = '\0';
	}
	printf("%s", msgbuf);
	logmsg(msgbuf, 0, 0, 0);
	flushoutp();

	errno = x;
	if(nomenus && !script)
		exit(1);
}

/* like perror, but to stdout, and allows formatted messages */
void
err_fmt(char *msg, ...)
{
	int saverr = errno;	/* in case error during printf */
	va_list arg;

	va_start(arg, msg);
	vprintf(msg, arg);
	va_end(arg);

	if(saverr)
		printf(": %s\n", strerror(saverr));
	else
		printf("\n");
	flushoutp();
}


#ifndef _STANDALONE
#include <time.h>
extern FILE *logfd;

void
logmsg(char *fmt, ...)
{
	time_t t;
	va_list arg;

	if(logfd == NULL)
		return;
	(void)time(&t);
	fprintf(logfd, "%s at %.19s: ", fulldrivename, ctime(&t));
	va_start(arg, fmt);
	vfprintf(logfd, fmt, arg);
	va_end(arg);
	fflush(logfd);
}
#else
void
logmsg(char *fmt, ...)
{
}
#endif
