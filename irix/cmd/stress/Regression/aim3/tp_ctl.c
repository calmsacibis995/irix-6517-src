/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) tp_ctl.c:3.2 5/30/92 20:18:52" };
#endif

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "testerr.h"
#include "tp_test.h"

/*
 * tp_ctl is the controlling interface to the tape exerciser.  it expects
 * the arguments:
 * cmd  --  indicates what the exerciser is to do next.  currently this is 
 * one of the following  CONTINUE or KILLTAPE.  KILLTAPE stops the exerciser.
 * CONTINUE basically has no effect.
 * rn  --  a pointer to character indicates the name of the character special
 * file for the tape drive.
 * results  --  a pointer to long where tp_ctl returns the number of bytes it
 * was able to write to the tape since it's last invocation
 *
 * tp_ctl returns less than 0 if it encounters an error of some kind
 */

void tp_tmout();

tp_ctl(cmd,rn,results)
int cmd;
char *rn;
long *results;
{
    static int pid = -1;		/* the process id of the exerciser */
    static int fdes[2] = {-1,-1};	/* pipe to the process */
    struct stat stbuf;
    int tapedes;
    long tmp;

    signal(SIGALRM,tp_tmout);
    if(pid < 0 && cmd != KILLTAPE)  {
	TPDEBUG(fprintf(stderr, "tp_ctl() initialising\n"));
	alarm(5);
	if((tapedes = open(rn,2)) < 0)  {
	    alarm(0);
	    TPDEBUG(fprintf(stderr, "tp_ctl() couldn't open tape: %s\n",rn));
	    TPDEBUG(perror(rn));
	    return BADTAPE;
	}
	alarm(0);
	if(fstat(tapedes,&stbuf) < 0)  {
	    close(tapedes);
	    TPDEBUG(fprintf(stderr, "tp_ctl() fstat complained about the tape\n"));
	    return BADTAPE;
	}
	if((stbuf.st_mode & S_IFMT) != S_IFCHR)  {	/* not char special */
	    close(tapedes);
	    TPDEBUG(fprintf(stderr, "tp_ctl() tape wasn't a character device\n"));
	    return BADTAPE;
	}
	close(tapedes);
	if(pipe(fdes) < 0)  {
	    TPDEBUG(fprintf(stderr, "tp_ctl() can't create a pipe\n"));
	    return BADPIPE;
	}
	if((pid = fork()) < 0)  {
	    close(fdes[0]);
	    close(fdes[1]);
	    TPDEBUG(fprintf(stderr, "tp_ctl() can't fork()\n"));
	    return BADPROC;
	}
	if(pid == 0)  {			/* the child */
	    close(1);			/* close stdout for pipe */
	    if(dup(fdes[1]) != 1)  {	/* copy the write end of pipe */
		tmp = -1;
		write(fdes[1],&tmp,sizeof tmp);  /* indicate error to dad */
		exit(1);
	    }
	    close(fdes[1]);		/* close the copy of pipe write */
	    close(fdes[0]);		/* close the read end of pipe */
	    execl("./tp_drvr","tp_drvr",rn,(char *)0);
	    tmp = -1;
	    write(1,&tmp,sizeof tmp);	/* we had a problem with execl */
	    exit(1);
	}
	close(fdes[1]);
	fdes[1] = -1;
	read(fdes[0],&tmp,sizeof tmp);
	if(tmp < 0)  {			/* child encountered an error */
	    wait((int *)0);
	    close(fdes[0]);
	    fdes[0] = -1;
	    TPDEBUG(fprintf(stderr, "tp_ctl() child exited prematurely\n"));
	    return BADPROC;
	}
	return 1;
    }
    if(pid < 0)  {
	TPDEBUG(fprintf(stderr, "tp_ctl() attempt to kill non-existant child\n"));
	return BADPROC;
    }
    if(kill(pid,SIGHUP) == 0)
	read(fdes[0],results,sizeof *results);
    else
	*results = -1;
    if(cmd == KILLTAPE)  {
	kill(pid,SIGTERM);
	close(fdes[0]);
	fdes[0] = fdes[1] = -1;
	pid = -1;
	wait((int *)0);
    }
    return 1;
}

void tp_tmout(sig)
int sig;
{
    signal(sig,tp_tmout);
}
