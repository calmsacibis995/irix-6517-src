/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) ttydrvr.c:3.2 5/30/92 20:18:57" };
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>

#ifndef ZILOG
#include <setjmp.h>			/* everyone else left them alone */
#else					/* Zilog chose to change some names */
#include <setret.h>
#define setjmp(x) setret(x)
#define longjmp(x,y) longret(x,y)
typedef ret_buf jmp_buf;
#endif

#include "ttytest.h"

#define TIMEOUT -11
#define TIME 2

char list[] = "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\n";
struct ttdata mydata = {0L,0L,0L};
int ractive = 1;
int thepipe;
jmp_buf env;
jmp_buf tmo_env;

void rstopper(),rstarter(),render();
void timer();
long atol();

main(argc,argv)
int argc;
char **argv;
{
    int nwrote;
    int c,lastc;
    int done = 0;
    register int index = 0;	/* index to expected char sequence */

    if(argc < 2)  {
	fprintf(stderr,"%s: usage fdes\n",argv[0]);
	exit(1);
    }
    thepipe = (int)atol(argv[1]);
    nwrote = 1;
    write(thepipe,&nwrote,sizeof nwrote);
    signal(SIGHUP,rstopper);
    signal(SIGINT,rstarter);
    signal(SIGTERM,render);
    signal(SIGALRM,timer);
    if(setjmp(env))
	pause();
    for(;;)  {
	if((nwrote = write(1,list,SEQLEN)) < 0)  { /* write tty should be 1 */
	    mydata.errors = mydata.num_rd = -1;
	    write(thepipe,&mydata,sizeof mydata);
	    exit(1);
	}
	mydata.num_wt += nwrote;
	done = 0;
	lastc = -1;
	while(!done  && (c = tgetc(0)) >= 0)  {
	    if(lastc == 'p')  
		done = 1;
	    lastc = c;
	    mydata.num_rd++;
	    if(c != list[index])  {
		mydata.errors++;
		if(islower(c))
		    index = c - 'a';
		else if(c == '\n')
		    index++;
	    }
	    index++;
	    index %= SEQLEN;
	}
    }
#ifdef LINT
    exit(0);
#endif
}


void rstopper(sig)
int sig;
{
    signal(sig,rstopper);		/* reset the signal */
    tflush(0);
    tflush(1);
    write(thepipe,&mydata,sizeof mydata);
    mydata.errors = mydata.num_rd = mydata.num_wt = 0;
    longjmp(env,1);
}

void rstarter(sig)
int sig;
{
    signal(sig,rstarter);
    return;
}

void render(sig)
int sig;
{
    tflush(0);
    tflush(1);
    exit(0);
}

tgetc(file)
int file;
{
    char c;
    int nread;

    if(setjmp(tmo_env))
	return TIMEOUT;
    alarm(TIME);
    for(;;)  {
	nread = read(file,&c,sizeof c);
	if(nread == 0)
	    continue;
	if(nread < 0)  {
	    alarm(0);
	    return EOF;
	}
	alarm(0);
	break;
    }
    return (c & 0xff);
}


void timer(sig)
int sig;
{
    signal(sig,timer);
    longjmp(tmo_env,1);
}
