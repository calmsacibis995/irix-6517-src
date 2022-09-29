/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) tp_drvr.c:3.3 5/30/92 20:24:30" };
#endif

#include <stdio.h>
#include <signal.h>

#ifndef ZILOG				/* Zilog decided to change */
#include <setjmp.h>
#else					/* the name of the setjmp stuff */
#include <setret.h>			/* setret is SO MUCH BETTER */
#define setjmp(x) setret(x)		/* sarcasm obviously dripping */
#define longjmp(x) longret(x)		/* but we can get around this */
#endif				

#define MWRITE 32

/*
 * this is the tape driver it assumes that file descriptor 0 is connected to
 * a tape drive that is opened for read and write, and that file descriptor 1
 * is connected to the write end of a pipe
 */
char buff[32768];			/* the buffer for reads & writes */
long count = 0;				/* count of bytes written to tape */
jmp_buf Environ;
int fd;

int stopper();
int ender();

main(argc,argv)
int argc;
char **argv;
{
    int n_read,n_wrote;
    int index;
    char *rn;

    if(argc < 2)
	count = -1;
    write(1,&count,sizeof count);	/* let parent process proceed */
    if(argc < 2)
	exit(1);
    rn = argv[1];
    for(index = 0;index < sizeof buff;index++)
	buff[index] = 'a';
    signal(SIGHUP,stopper);
    signal(SIGTERM,ender);
    setjmp(Environ);
    for(count = 0;;)  {
	if((fd = open(rn,1)) < 0)  {
	    count = -1;
	    write(1,&count,sizeof count);
	    exit(1);
	}
	for(index = 0;index < MWRITE;)  {
	    buff[0]++;
	    if((n_wrote = write(fd,buff,sizeof buff)) != sizeof buff)  {
		count = -1;
		write(1,&count,sizeof count);
		exit(1);
	    }
	    index++;
	    count += n_wrote;
	}
	close(fd);
	if((fd = open(rn,0)) < 0)  {
	    count = -1;
	    write(1,&count,sizeof count);
	    exit(1);
	}
	for(index = 0;index < MWRITE;)  {
	    if((n_read = read(fd,buff,sizeof buff)) < 0)  {
		count = -1;
		write(1,&count,sizeof count);
		exit(1);
	    }
	    index++;
	}
	close(fd);
    }
#ifdef LINT
    exit(0);
#endif
}

stopper(sig)
int sig;
{
    signal(sig,stopper);
    write(1,&count,sizeof count);
    close(fd);
    longjmp(Environ,0);
}

ender(sig)
int sig;
{
    signal(sig,SIG_IGN);
    close(fd);
    close(1);				/* close the pipe */
    exit(0);
}
