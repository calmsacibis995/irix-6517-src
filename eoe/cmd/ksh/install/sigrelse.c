
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * See if sigrelse is needed to exit from signal handler
 */

#include	<sys/types.h>
#include	<signal.h>
#ifdef SIGBLOCK
# define sigrelse(s)	sigsetmask(sigblock(0)&~(1<<((s)-1)))
#endif

int handler();
int timeout();

main()
{
	int parent;
	int status;
	if((parent=fork())==0)
	{
		signal(SIGQUIT,handler);
		signal(SIGALRM,timeout);
		/* set a timeout */
		alarm(5);
		/* send a SIQUIT to myself */
		kill(getpid(),SIGQUIT);
		pause();
		exit(0);
	}
	else if(parent < 0)
		exit(1);
	wait(&status);
	if((status&077)==SIGQUIT)
	{
#ifdef SIGBLOCK
		printf("#define sigrelease(s)	sigsetmask(sigblock(0)&~(1<<((s)-1)))\n");
		printf("#define sig_begin()	(sigblock(0),sigsetmask(0))\n");
#else
		printf("#define sig_begin()\n");	
#   ifdef sigrelse
		printf("#define sigrelease(s)\n");	
#   else
		printf("#define sigrelease	sigrelse\n");
#   endif sigrelease
#endif
		exit(0);
	}
	exit(1);
}

handler(sig)
{
	/* reset the signal handler to default */
	signal(sig, SIG_DFL);
	sigrelse(sig);
	kill(getpid(),SIGQUIT);
	pause();
	exit(0);
}

timeout(sig)
{
	exit(1);
}
