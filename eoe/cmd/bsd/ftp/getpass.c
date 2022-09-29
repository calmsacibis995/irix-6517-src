/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)getpass.c	5.4 (Berkeley) 3/7/86";
#endif /* not lint */

#include <stdio.h>
#include <signal.h>
#ifdef sgi
#include <termio.h>

static struct termio ttyb;
#else
#include <sgtty.h>

static	struct sgttyb ttyb;
#endif
static	int flags;
static	FILE *fi;

#ifdef sgi
static void intfix()
#else
static intfix()
#endif
{
#ifdef sgi
	ttyb.c_lflag = flags;
	if (fi != NULL)
		(void) ioctl(fileno(fi), TCSETA, &ttyb);
#else
	ttyb.sg_flags = flags;
	if (fi != NULL)
		(void) stty(fileno(fi), &ttyb);
#endif
	exit(SIGINT);
}

char *
mygetpass(prompt)
char *prompt;
{
	register char *p;
	register c;
	static char pbuf[50+1];
#ifdef sgi
	void (*sig)();
#else
	int (*signal())();
	int (*sig)();
#endif

	if ((fi = fopen("/dev/tty", "r")) == NULL)
		fi = stdin;
	else
		setbuf(fi, (char *)NULL);
	sig = signal(SIGINT, intfix);
#ifdef sgi
	(void)ioctl(fileno(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(fileno(fi), TCSETAF, &ttyb);
#else
	(void) gtty(fileno(fi), &ttyb);
	flags = ttyb.sg_flags;
	ttyb.sg_flags &= ~ECHO;
	(void) stty(fileno(fi), &ttyb);
#endif
	fprintf(stderr, "%s", prompt); (void) fflush(stderr);
	for (p=pbuf; (c = getc(fi))!='\n' && c!=EOF;) {
		if (p < &pbuf[sizeof(pbuf)-1])
			*p++ = c;
	}
	*p = '\0';
	fprintf(stderr, "\n"); (void) fflush(stderr);
#ifdef sgi
	ttyb.c_lflag = flags;
	(void) ioctl(fileno(fi), TCSETA, &ttyb);
#else
	ttyb.sg_flags = flags;
	(void) stty(fileno(fi), &ttyb);
#endif
	(void) signal(SIGINT, sig);
	if (fi != stdin)
		(void) fclose(fi);
	return(pbuf);
}
