#ident "$Revision: 1.4 $"

#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/file.h>
#include "defs.h"

char cmd[MAXNAMELEN];
extern int errno;
void ticker();


main(int argc, char **argv)
{
    unsigned int sleeptime;
    int fd;

    strcpy(cmd, basename(argv[0]));
    if (argc != 3) {
	fprintf(stderr, "usage: %s <tick-file> <tick-duration>\n", cmd);
	exit(1);
    }
    if (fork()) {
	/* parent  -- just return */
	exit(0);
    } else {
	/* child -- go into ticker loop */
	ticker(argv[1], argv[2]);
    }
    /* NOT REACHED */
}

void
ticker(char *tickfile, char *s_sleeptime)
{
    struct tms t; /* not used */
    register time_t uptime, lastuptime;
    register int len;
    register int fd;
    register unsigned int sleeptime;
    int	statusinterval, statusint, statusduration;
    char	buf[32];	/* has to at most hold maxint */
    FILE	*fp;

    if ((fd = open(tickfile, O_WRONLY|O_CREAT|O_SYNC|O_TRUNC, 0644)) < 0) {
	fprintf(stderr, "%s:cannot open file %s\n", cmd, tickfile);
	perror(tickfile);
	exit(1);
    }
    if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
	fprintf(stderr, "%s: cannot get lock on %s\n", cmd, tickfile);
	if (errno == EWOULDBLOCK)
	    fprintf(stderr, "... are you running another instance of %s?\n", cmd);
	perror(tickfile);
	exit(1);
    }
    sleeptime = atoi(s_sleeptime);
    if ((fp = fopen(amcs[amc_statusinterval].filename, "r")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, amcs[amc_statusinterval].filename);
	fprintf(stderr, "Status reports will not be sent.\n");
	statusinterval = 0;
    } else {
	fscanf(fp, "%d", &statusinterval);
	fclose(fp);
    }

    if (statusinterval > 0) {
	lastuptime = (times(&t) / HZ) / 60 ;
	statusint = statusinterval * 1440;
	statusduration = lastuptime % statusint;
    }
    while (1) {
	sprintf(buf, "%d\n", time(0));
	len = strlen(buf);
	/* go to begining of file  to rewrite previous value */
	if (lseek(fd, 0L, SEEK_SET) == -1) {
	    fprintf(stderr, "%s: lseek on file %s failed\n",
		    cmd, tickfile);
	    perror(tickfile);
	    exit(1);
	}
	if (write(fd, buf, len) != len) {
	    fprintf(stderr, "%s: write to file %s failed\n", cmd, tickfile);
	    perror(tickfile);
	    exit(1);
	}
	if (statusinterval > 0) {
	    uptime = (times(&t) / HZ) / 60; /* time in minutes */
	    statusduration += uptime - lastuptime;
	    lastuptime = uptime;
	    if (statusduration >= statusint) {
		statusduration -= statusint;
		system("/usr/etc/amdiag STATUS &");
	    }
	}
	sleep(sleeptime);
    }
}
