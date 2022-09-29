/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 *
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) ttyctl.c:3.2 5/30/92 20:18:56" };
#endif

#include "suite3.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>

#ifndef V7
#  ifdef SYS5
#    include <fcntl.h>
#  else
#    include <sys/file.h>
#    ifdef sun
#      include <fcntl.h>
#    endif
#  endif
#endif

#include "ttytest.h"
#include "testerr.h"

/*
 * This is the tty driver module for the multi user benchmark program
 * The interface to this module consists of the single function
 * ttyctl(nb,wn,rn,raw,data)
 * int nb;
 * char *wn,*rn;
 * int raw;
 * struct ttdata *data;
 * Nb is the new desired baud rate, wn is the name of the character
 * special file that will be the device to write to, and rn is the name of the
 * name of the character special to read from.
 * Raw is a boolean value.  If raw != 0 then run the tty in raw mode.
 * Data is a pointer to a place to store information from the reader process.
 */

#define READTTY  0
#define WRITETTY 1
#define RPROC	 0
#define WPROC	 1

ttyctl(nb,wn,rn,raw,data)
int nb;
char *wn,*rn;
int raw;
struct ttdata *data;
{
    static int pid = -1;	/* for reader & writer children */
    static int fildes[2] = {-1,-1};	/* pipe to the reader */
    static int ttydes[2] = {-1,-1};
    int n_read = 0;
    int fcarg = 0;

    if(ttydes[READTTY] == -1 && nb >= 0)  {/* starting the ttydrvr */
		/* open the ttys and get file descriptors for them */
	if(ttopen(rn,wn,ttydes) < 0) {	/* found a problem with the tty */
	    fprintf(stderr,
		"ttopen() returned error for rd_dev=%s and wrt_dev=%s\n",
		rn, wn);
	    return BADTTY;
	}
		/* initialise the ports according to the desired settings */
	if(termconf(nb,raw,ttydes) < 0)  {
	    close(ttydes[READTTY]);ttydes[READTTY] = -1;
	    close(ttydes[WRITETTY]);ttydes[WRITETTY] = -1;
	    fprintf(stderr, "termconf() returned error\n");
	    return BADIOCTL;
	}
	tflush(ttydes[READTTY]);
	tflush(ttydes[WRITETTY]);
		/* create a pipe to communicate with the reader process */
	if(pipe(fildes) < 0)  {			/* problem making a pipe */
	    close(ttydes[READTTY]);ttydes[READTTY] = -1;
	    close(ttydes[WRITETTY]);ttydes[WRITETTY] = -1;
	    return BADPIPE;
	}
		/* start the process that does the i/o */
	if((pid = procinit(fildes,ttydes)) < 0)  {
	    ttclean(ttydes,fildes[0],pid);
	    close(ttydes[WRITETTY]);ttydes[WRITETTY] = -1;
	    return BADPROC;
	}
	return GOOD;
    }
    if(ttydes[READTTY] < 0)
	return BADPROC;
    kill(pid,SIGHUP);
    n_read = read(fildes[0],data,sizeof *data);
    if(nb < 0)  {
	ttclean(ttydes,fildes[0],pid);
	return DEAD;
    }
    if(n_read != sizeof *data)  {
	TTDEBUG(fprintf(stderr, "ttyctl(): read returned %d\n",n_read));
	ttclean(ttydes,fildes[0],pid);
	return BADPIPE;
    }
    if(data->errors < 0 && data->num_rd < 0)  {
	ttclean(ttydes,fildes[0],pid);
	return BADTTY;
    }
    if(termconf(nb,raw,ttydes) < 0)  {
	ttclean(ttydes,fildes[0],pid);
	return BADTTY;
    }
    kill(pid,SIGINT);	/* tell the writer to come on again */
    return GOOD;
}

void tt_tmout();

ttopen(rn,wn,ttydes)
char *rn,*wn;
int ttydes[2];
{
    signal(SIGALRM,tt_tmout);
    if(ttydes[READTTY] >= 0)
	return 1;
    alarm(5);
    ttydes[WRITETTY] = open(wn,WFLAG);
    alarm(0);
    if(ttydes[WRITETTY] < 0)
	return(-1);
    alarm(5);
    ttydes[READTTY] = open(rn,RFLAG);
    alarm(0);
    if(ttydes[READTTY] < 0)
	return(-1);
    return 1;
}

void tt_tmout(sig)
int sig;
{
    signal(sig,tt_tmout);
}


procinit(fds,ttydes)
int fds[2];				/* the pipe to the reader */
int ttydes[2];				/* file descriptors for the ttys */
{
    int pid,res = -1;
    char buff[10];			/* space for ascii pipe end */

    /* first start up the read process */
    if((pid = fork()) < 0)  {	/* couldn't create the child */
	return -1;
    }
    if(pid == 0)  {			/* this is the child */
	if(close(0) < 0)  {		/* close its standard input */
	    perror("");
	    write(fds[1],&res,sizeof res);
	    exit(1);
	}
	if(dup(ttydes[READTTY]) != 0) {	/* to reconnect it to tty */
	    perror("");
	    write(fds[1],&res,sizeof res);
	    exit(1);
	}
	if(close(1) < 0)  {		/* close its standard output */
	    perror("");
	    write(fds[1],&res,sizeof res);
	    exit(1);
	}
	if(dup(ttydes[WRITETTY]) != 1)  {
	    write(fds[1],&res,sizeof res);
	    exit(1);
	}
	close(fds[0]);			/* child doesn't need this end */
	sprintf(buff,"%d",fds[1]);	/* string for the execl */
	execl("./ttydrvr","ttydrvr",buff,(char *)0);
	write(fds[1],&res,sizeof res);
	exit(1);
    }
    read(fds[0],&res,sizeof res);
    if(res < 0)  {
	wait((int *)0);
	TTDEBUG(fprintf(stderr,"the child died in an undignified fashion\n"));
	return BADPROC;
    }
    return pid;
}

termconf(nb,raw,ttys)
int nb,raw;
int ttys[2];
{
    int i;
    extern struct bpair baudlst[];
    ttyinf arg;				/* argument to stty */
    
    TTDEBUG(printf("termconf(%d,%d,%lx)\n",nb,raw,ttys));
    fflush(stdout);
    for(i = 0;baudlst[i].val >= 0;i++){	/* make certain nb is one of the */
	if(nb <= baudlst[i].val)	/* allowed speeds.  round the speed */
	    break;			/* down if it isn't */
    }
    if(baudlst[i].val < 0)
	return -1;
    if(nb < baudlst[i].val)
	i--;
    nb = baudlst[i].lbl;		/* pick the label for the baud rate */
    if(ioctl(ttys[READTTY],TERMGET,&arg) < 0)
	return -1;
    if(raw)				/* boss has requested raw output */
	setraw(&arg);
    else
	unsetraw(&arg);
    echoff(&arg);			/* we don't want any echoing */
    mapoff(&arg);			/* don't map carriage returns */
    setspeed(&arg,nb);
    if(ioctl(ttys[WRITETTY],TERMSET,&arg) < 0)
	return -1;
    if(ioctl(ttys[READTTY],TERMSET,&arg) < 0)
	return -1;
    return 1;
}

ttclean(ttydes,pend,pid)
int ttydes[2];				/* file descriptors for ttys */
int pend;				/* the read end of the pipe */
int pid;				/* process id of the child */
{
    close(pend);
    kill(pid,SIGTERM);
    wait((int *)0);
    close(ttydes[READTTY]);
    close(ttydes[WRITETTY]);
    ttydes[READTTY] = -1;
}
