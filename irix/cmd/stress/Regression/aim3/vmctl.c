/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) vmctl.c:3.2 5/30/92 20:19:00" };
#endif

#include <stdio.h>
#include <signal.h>
#include "vmtest.h"
#include "testerr.h"

/*
 * vmctl provides the interface to the memory exerciser.
 * the first parameter, results is a pointer to long and is used for the
 * exerciser to write information into.  this information is the number of
 * bytes used in the test.
 * the second parameter, cmd is what the exerciser is supposed to do.
 * this currently can be either die or continue.
 */
vmctl(results,cmd)
long *results;
int cmd;
{
    static int pid = -1;
    static int fdes[2];
    long tmp;
#ifdef DEBUGON
    char dbbuf[10];
#endif

    if(pid == -1 && cmd != KILLMEM)  {	/* starting the test */
	if(pipe(fdes) < 0)  {
	    VMDEBUG(fprintf(stderr, "vmctl: couldn't create pipe\n"));
	    return BADPIPE;
	}
	if((pid = fork()) < 0)  {
	    pid = -1;
	    close(fdes[0]);
	    close(fdes[1]);
	    VMDEBUG(fprintf(stderr, "vmctl: couldn't fork()\n"));
	    return BADPROC;
	}
	if(pid == 0)  {			/* then this is the child */
	    close(fdes[0]);
	    close(1);
	    dup(fdes[1]);
	    close(fdes[1]);
#ifdef DEBUGON
	    sprintf(dbbuf,"%ld",debug);
	    execl("./vmdrvr","vmdrvr",dbbuf,0);
#else
	    execl("./vmdrvr","vmdrvr",0);
#endif
	    tmp = BADPROC;		/* to get here something is wrong */
	    write(1,&tmp,sizeof tmp);
	    exit(1);
	}
	close(fdes[1]);
	read(fdes[0],&tmp,sizeof tmp);	/* wait for child to get the memory */
	*results = tmp;
	if(tmp <= 0)  {			/* something's gone wrong */
	    VMDEBUG(fprintf(stderr, "vmctl: couldn't execl(vmdrvr)\n"));
	    wait(0L);			/* put zombie to rest */
	    return (int)tmp;
	}
	return 0;
    }
    if(kill(pid,SIGHUP) == 0)
	read(fdes[0],results,sizeof *results);
    else
	*results = -1;
    if(*results <= 0)  {
	cmd = KILLMEM;
    }
    switch(cmd)  {
    case KILLMEM:
	VMDEBUG(fprintf(stderr, "killing the vmem exerciser\n"));
	if(pid >= 0)  {
	    kill(pid,SIGKILL);
	    wait((int *)0);
	    pid = -1;
	    close(fdes[0]);
	    fdes[0] = fdes[1] = -1;
	}
	break;
    default:				/* including CONTINUE */
	VMDEBUG(fprintf(stderr, "continuing with the vmem exerciser\n"));
	kill(pid,SIGINT);
	break;
    }
    return 1;
}
