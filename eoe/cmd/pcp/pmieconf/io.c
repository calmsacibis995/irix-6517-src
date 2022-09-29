/*
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: io.c,v 1.1 1999/04/28 10:39:51 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <termio.h>

#define MINCOLS	80
#define MINROWS	24

static int	neols = -1;
static int	needresize;
static int	needscroll;
static int	skiprest;
static int	nrows;
static int	ncols;
static char	shortmsg[] = "more? (h=help) ";
static char	longmsg[] = \
	"[q or n to stop, y or <space> to go on, <enter> to step] more? ";
static struct termio	otty;

extern char	*pmProgname;


void setio(int reset)	{ neols = 0; skiprest = reset; }
void setscroll(void)	{ needscroll = 1; }
int  resized(void)	{ return needresize; }

/* looks after window resizing for the printing routine */
/*ARGSUSED*/
void
onwinch(int dummy)
{
    signal(SIGWINCH, onwinch);
    needresize = 1;
}

/* in interactive mode scrolling, if no more wanted skiprest is set */
static void
promptformore(void)
{
    int			i;
    int			ch;
    int			sts = 1;
    char		c;
    char		*prompt;
    static int		first = 1;
    struct termio	ntty;

    if (first) {
	if (ioctl(0, TCGETA, &otty) < 0) {
	    fprintf(stderr, "%s: TCGETA ioctl failed: %s\n", pmProgname,
		    strerror(errno));
	    exit(1);
	}
	first = 0;
    }

    /* put terminal into raw mode so we can read input immediately */
    memcpy(&ntty, &otty, sizeof(struct termio));
    ntty.c_cc[VMIN] = 1;
    ntty.c_cc[VTIME] = 1;
    ntty.c_lflag &= ~(ICANON | ECHO);
    if (ioctl(0, TCSETAW, &ntty) < 0) {
	fprintf(stderr, "%s: TCSETAW ioctl failed: %s\n", pmProgname,
		strerror(errno));
	exit(1);
    }

    prompt = shortmsg;
    while (sts == 1) {
	putchar('\r');
	for (i = 0; i < ncols-1; i++)
	    putchar(' ');
	putchar('\r');
	printf(prompt);
	fflush(stdout);

	if (read(0, &c, 1) != 1) {
	    sts = 1;
	    goto reset_tty;
	}
	ch = (int)c;

	switch(ch) {
	case 'n':	/* stop */
	case 'q':
	    setio(1);
	    sts = 0;
	    break;
	case 'y':	/* page down */
	case ' ':
	    neols = sts = 0;
	    break;
	case '\n':	/* step down */
	    neols = nrows;
	    sts = 0;
	    break;
	default:
	    prompt = longmsg;
	}
    }

reset_tty:
    if (ioctl(0, TCSETAW, &otty) < 0) {
	fprintf(stderr, "%s: reset TCSETAW ioctl failed: %s\n", pmProgname,
		strerror(errno));
	exit(1);
    }

    putchar('\r');
    for (i = 0; i < ncols-1; i++)
	putchar(' ');
    putchar('\r');
    fflush(stdout);
}

/*
 * generic printing routine which can pause at end of a screenful.
 * if this returns 1, the user has requested an end to this info,
 * so the caller must always observe the pprintf return value.
 */
void
pprintf(char *format, ...)
{
    char		*p;
    va_list		args;
    struct winsize	geom;
    static int		first = 1;

    if (first == 1) {	/* first time thru */
	first = 0;
	ioctl(0, TIOCGWINSZ, &geom);
	nrows = (geom.ws_row < MINROWS? MINROWS : geom.ws_row);
	ncols = (geom.ws_col < MINCOLS? MINCOLS : geom.ws_col);
    }

    if (skiprest)
	return;

    va_start(args, format);
    if (needscroll) {
	/*
	 * use the fact that i know we never print more than MINROWS at once
	 * to figure out how many lines we've done before doing the vfprintf
	 */
	if (neols >= nrows-1) {
	    promptformore();
	    if (skiprest) {
		va_end(args);
		return;
	    }
	}
	for (p = format; *p != '\0'; p++)
	    if (*p == '\n') neols++;
	vfprintf(stdout, format, args);
    }
    else
	vfprintf(stdout, format, args);
    va_end(args);
    if (needresize) {
	ioctl(0, TIOCGWINSZ, &geom);
	nrows = (geom.ws_row < MINROWS? MINROWS : geom.ws_row);
	ncols = (geom.ws_col < MINCOLS? MINCOLS : geom.ws_col);
#ifdef PMIECONF_DEBUG
	printf("debug - reset size: cols=%d rows=%d\n", ncols, nrows);
#endif
	needresize = 0;
    }
}

/* general error printing routine */
void
error(char *format, ...)
{
    va_list	args;
    FILE	*f;

    if (skiprest)
	return;
    va_start(args, format);
    if (needscroll) {
	f = stdout;
	fprintf(f, "  Error - ");
    }
    else {
	f = stderr;
	fprintf(f, "%s: error - ", pmProgname);
    }
    vfprintf(f, format, args);
    fprintf(f, "\n");
    neols++;
    va_end(args);
}
