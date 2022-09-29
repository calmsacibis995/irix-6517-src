/*	LoadICE.c - Edit 7

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - LoadICE application: A ROM management utility for the PromICE
	
*/

#include <stdio.h>
#include <string.h>
#include <signal.h>

#ifdef MSDOS
#include <process.h>
#endif

#include "piconfig.h"
#include "pistruct.h"

char *cright="Copyright (C) 1990-1995 Grammar Engine, Inc.";
char *liver="LoadICE Version 3.1f";

#ifdef ANSI
extern void piinit(void);
extern void pishell(void);
extern void pi_beep(void);
extern void pcexit(void);
void licatch(int sig);
void liclick(int sig);
void main(int argc,char **argv)

#else
extern void piinit();
extern void pishell();
extern void pi_beep();
extern void pcexit();
void licatch();
void liclick();
void main(argc,argv)
int argc;
char **argv;
#endif

	{
	char *tp;
	
	piinit();

	setbuf(stdout, NULL);

	if (pxdisp&PXVH)
		printf("\n%s\n%s\n",liver,cright);

	if (argc > 1)
		{
		tp = pxcline;
		while (--argc)
			{
			strcpy(tp, *++argv);
			tp += strlen(tp);
			*tp++ = ' ';
			}
		*--tp = '\0';
		}

	signal(SIGINT,licatch);
	signal(SIGALRM, liclick);
	alarm(PIC_ARM);

	pishell();

	if (pxdisp&PXVH)
		printf("\n\nExiting LoadICE - exit code %d\n",pxexitv);

	exit(pxexitv);
	}

/* `licatch` - signal catcher to terminate the application */

#ifdef ANSI
void licatch(int sig)
#else
void licatch()
#endif
	{
	if (piflags&PiER)
		piflags &= ~PiUP;
	pcexit();

	printf("\n\nTerminating LoadICE - exit code %d\n",pxexitv);

	exit(pxexitv);
	}

/* `liclick` - signal catcher to kick PromICE */

#ifdef ANSI
void liclick(int sig)
#else
void liclick()
#endif
	{
	if (!pibusy)
		pikick();
	alarm(PIC_ARM);
	}
