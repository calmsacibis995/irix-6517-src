
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include "mkfp.h"

extern void flushoutp(void);

char *progname = NULL;


void
fpMessage(char *msg, ...)
{
    va_list arg;

    va_start(arg, msg);
    vprintf(msg, arg);
    va_end(arg);
    printf("\n");

    flushoutp();
}


void
fpError(char *msg, ...)
{
    va_list arg;

    if (progname)
	printf("%s: Error - ", progname);
    else
	printf("Error - ");

    va_start(arg, msg);
    vprintf(msg, arg);
    va_end(arg);
    printf("\n");

    flushoutp();
}


void
fpWarn(char *msg, ...)
{
    va_list arg;

    if (progname)
	printf("%s: Warning - ", progname);
    else
	printf("Warning - ");

    va_start(arg, msg);
    vprintf(msg, arg);
    va_end(arg);
    printf("\n");

    flushoutp();
}



/* like perror, but to stdout, and allows formatted messages */
void
fpSysError(char *msg, ...)
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

    logmsg(msg, 0, 0, 0);

    flushoutp();
}


extern FILE *logfd;

void
logmsg(char *fmt, ...)
{
    time_t t;
    va_list arg;

    if(logfd == NULL)
        return;

    (void)time(&t);
    va_start(arg, fmt);
    vfprintf(logfd, fmt, arg);
    fprintf(logfd, " : [ %.19s]\n", ctime(&t));
    va_end(arg);

    fflush(logfd);
}


void
scerrwarn(char *msg, ...)
{
    register int x;
    char msgbuf[512];
    int len;
    va_list arg;

    x = errno;

    if(x)
        sprintf(msgbuf,"Error: %s -- ", strerror(x));
    else
        sprintf(msgbuf,"Error: -- ");

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
}
