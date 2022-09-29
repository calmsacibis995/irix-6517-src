/*
 * Copyright 1995, Silicon Graphics, Inc.
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
#ident "$Revision: 1.4 $"

#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <curses.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include "evctr_util.h"

extern int	Cflag;

#define BOS (LINES-1)		/* Index of last line on screen */
enum cmode {ABSOLUTE, RATE} cmode;
static int	Full;

extern void quit(int);
extern void fail(char *, ...);

static int Cfirst = 1;

void
quit(int stat)
{
	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		(void)endwin();
	}
	exit(stat);
}

void
sig_winch(int sig)
{
	if (Cflag) {
		(void)endwin();
		initscr();
		Cfirst = 1;
	}

	/* re-arm in case we have System V signals */
	(void)signal(sig, sig_winch);
}

void
fail(char *fmt, ...)
{
	va_list args;

	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	va_start(args, fmt);
	fprintf(stderr, "ecstat: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

void
do_display(int interval)
{
    int		ystart = 1;
    int		y;
    int		x;
    int		ctr;

    move(ystart, 0);
    standout();
    if (cmode == ABSOLUTE)
	printw("%18s   Event Code and Name", "Absolute Count");
    else
	printw("%18s   Event Code and Name", "Delta Count/sec");
    getyx(stdscr, y, x);
    move(y, x);
    while (x < COLS-1) {
	addch(' ');
	x++;
    }
    standend();

    evctr_global_sample(0);

    if (ActiveChanged || Cfirst) {
	for (y = ystart+1; y < BOS; y++) {
	    move(y, 0);
	    clrtoeol();
	}
	Cfirst = 1;
    }

    y = ystart;
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {

	if (!Full && !Active[ctr])
	    continue;

	y++;
	if (y < BOS) {
	    move(y, 0);
	    if (Active[ctr]) {
		if (cmode == ABSOLUTE)
		    printw("%18llu", Count[ctr] * Mux[ctr]);
		else {
		    if (OldCount[ctr] != -1)
			printw("%18.2f", (double)((Count[ctr] - OldCount[ctr]) * Mux[ctr]) / interval);
		    else
			printw("%18s", "<initializing>");
		}
		addch(' ');
		if (Sem[ctr] != EVCTR_SEM_OK)
		    addch('?');
		else if (Mux[ctr] > 1)
		    addch('*');
		else
		    addch(' ');
		OldCount[ctr] = Count[ctr];
	    }
	    else if (Cfirst)
		printw("%18s  ", "<inactive>");
	    if (Cfirst)
		printw(" [%2d]  %s", ctr, EventDesc[ctr].text);
	}
    }
}

const char ctrl_l = 'L' & 037;

/*
 * Repeated periodic Curses display
 */
void
disp(int fflag, int rflag, int interval, int samples)
{
    time_t now;
    char tmb[80];
    struct timeval wait;
    fd_set rmask;
    struct termio  tb;
    int c, n;
    int intrchar;			/* user's interrupt character */
    int suspchar;			/* job control character */
    static char rtitle[] = "R: rate";
    static char atitle[]  = "A: absolute";
    static char host[20] = "";

    gethostname (host, sizeof(host));
    Full = fflag ? 1 : 0;
    cmode = rflag ? RATE : ABSOLUTE;

    (void) ioctl(0, TCGETA, &tb);
    intrchar = tb.c_cc[VINTR];
    suspchar = tb.c_cc[VSUSP];
    FD_ZERO(&rmask);

    /* now that everything else is ready, leave cooked mode
     */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    leaveok(stdscr, FALSE);
    nonl();
    intrflush(stdscr, FALSE);
    move(0, 0);

    (void)signal(SIGINT, quit);
    (void)signal(SIGTERM, quit);
    (void)signal(SIGHUP, quit);
    (void)signal(SIGWINCH, sig_winch);

    for (;;) {
	if (Cfirst) {
	    clear();
	    standout();
	    move(BOS, 0);
	    printw(
"F:AllCtrs E:EnabledCtrs R:Rate A:Absolute");
	    standend();
	    switch (cmode) {
		case RATE:
		    move(0, 80-sizeof(rtitle));
		    printw(rtitle);
		    break;
		case ABSOLUTE:
		    move(0, 80-sizeof(atitle));
		    printw(atitle);
		    break;
	    }
	}
	now = time(0);
	cftime(tmb, "%b %e %T", &now);

	move(0, 0);
	printw("Global Event Counters -- %s -- %s", tmb, host);

	do_display(interval);

	Cfirst = 0;
	move(0, 0);
	refresh();

	if (samples) {
	    if (samples == 1) {
		/*
		 * all done, so exit
		 */
		quit(0);
		/*NOTREACHED*/
	    }
	    samples--;
	}

	(void)gettimeofday(&wait, 0);
	wait.tv_sec = interval-1;
	wait.tv_usec = 1000000 - wait.tv_usec;
	if (wait.tv_usec >= 1000000) {
		wait.tv_sec++;
		wait.tv_usec -= 1000000;
	}
	if (wait.tv_sec < interval
	    && wait.tv_usec < 1000000/10)
		wait.tv_sec++;
	FD_SET(0, &rmask);
	n = select(1, &rmask, NULL, NULL, &wait);
	if (n < 0) {
	    if (errno == EAGAIN || errno == EINTR)
		    continue;
	    fail("select: %s\n", strerror(errno));
	    break;
	}
	else if (n == 1) {
	    c = getch();
	    if (c == intrchar || c == 'q' || c == 'Q') {
		quit(0);
		/*NOTREACHED*/
	    }
	    if (samples) samples++;
	    if (c == suspchar) {
		reset_shell_mode();
		kill(getpid(), SIGTSTP);
		reset_prog_mode();
	    }
	    else if (c == 'a' || c == 'A') {
		cmode = ABSOLUTE;
		Cfirst = 1;
	    }
	    else if (c == 'e' || c == 'E') {
		Full = 0;
		Cfirst = 1;
	    }
	    else if (c == 'f' || c == 'F') {
		Full = 1;
		Cfirst = 1;
	    }
	    else if (c == 'r' || c == 'R') {
		cmode = RATE;
		Cfirst = 1;
	    }
	    else if (c == ctrl_l) {
		Cfirst = 1;
	    }
	}
    }
}
