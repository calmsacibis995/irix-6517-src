/*
 *	Panel - a curses-based interface for drawing a large number of
 *		values in a readable manner on a screen.  Includes functions
 *		to except input commands to show or hide various values
 *		being displayed.
 *
 *	J. M. Barton	04/06/89
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#define	PERFORMANCE
#include <curses.h>
#include "osview.h"

/*
 * Internal table type for our own display areas.
 */

typedef struct locReg {
	int		flags;
	int		len;
	void		(*fillfunc)(struct locReg *);
	int		xpos;
	int		ypos;
} locReg_t;

#define	up		1
#define	down		2
#define	left		3
#define	right		4
#define	home		5
#define	back		6
#define	forward		7

#define	logtocur(lx)	((lx*FIELDSIZE)+MARGIN)

static Reg_t		*displaylist;		/* The display list */
static int		current_x = MARGIN;	/* screen x position */
static int		logical_x;		/* logical column position */
static int		current_y;		/* screen y position */
static int		curses_on = 0;		/* if curses started up */
static Reg_t		*highlighted;	/* if something is highlighted */
static char		*header = "";		/* header string */
static void		(*updatestart)(void);	/* called before update */
static void		(*updateend)(void);	/* called after update */
static int		intrchar;		/* user's interrupt character */
static int		suspchar;		/* job control character */
static int		pwaitval;		/* wait interval */
static int		numcols;		/* number of logical columns */
static int		nlines;		/* actual # of lines on display */
static int		nscreens;	/* actual # screens to display */
static int		curscreen;	/* Current screen # */
static int		curcat = RF_HDRSYS;	/* Current category */

static int	pstrobekey(void);
static void	piupdate(int);
static void	pnupdate(void);
static void	predraw(int);
static void	predraw1(Reg_t *);
static void	playout(void);
static void	pupdate(int);
static void	ponhead(Reg_t *);
static void	poffhead(Reg_t *);
static void	psafemove(int);
static int	pchkstate(Reg_t *, int);
static void	psettty(int);
static void	phighlight(Reg_t *);
#ifdef DEBUG
static void	pchain(void);
#endif

/*==========>
**
**	The following routines make up the output arm portion of
**	the panel library.
**
<==========*/

/*
 * Redraw the screen.
 */
static void
predraw(int fill)
{
	clear();
	pupdate(fill);
	refresh();
	move(current_y, current_x);
}

/*
 * Redraw the given element.
 */
static void
predraw1(Reg_t *rp)
{
	if (!(rp->flags & RF_VISIBLE) ||
	    !(rp->flags & RF_ON))
		return;

	if (rp->flags & RF_SUPRESS) {
		move(rp->ypos, rp->xpos - 1);
		addch('*');
	}

	if (rp->flags & RF_BOLD)
		attron(A_BOLD);
	else
		attroff(A_BOLD);

	if (rp == highlighted)
		standout();

	move(rp->ypos, rp->xpos);
	printw("%-*.*s", TEXTSIZE, TEXTSIZE, rp->name);
	rp->format[TEXTSIZE-1] = '\0';
	move(rp->ypos, rp->xpos + (TEXTSIZE - strlen(rp->format)));
	if ((rp->flags & RF_BOLD) != (rp->oflags & RF_BOLD)) {
		printw("%*s", strlen(rp->format), "");
		move(rp->ypos, rp->xpos + (TEXTSIZE - strlen(rp->format)));
	}
	rp->oflags = rp->flags;

	addstr(rp->format);

	if (rp == highlighted)
		standend();

	attroff(A_BOLD);
}

/*
 * Layout the data on the available window space.
 */
static void
playout(void)
{
	Reg_t	*rp;
	int	curx;
	int	cury;
	int	incat	= 0;

	nlines = (lines == 0 ? LINES : (lines > LINES ? LINES : lines));
	if (categ)	/* leave room for category bar */
		nlines--;

	clear();
	rp = displaylist;
	numcols = 0;
	nscreens = 0;
	while (rp) {
		for (curx = 0; curx < COLS-FIELDSIZE-1; curx += FIELDSIZE) {
			/* leave room for status bar */
			for (cury = 1; cury < nlines;) {
				if (rp == 0)
					goto done;
				if (!(rp->flags & RF_ON)) {
					rp = rp->next;
					continue;
				}
				if (categ) {
					if (rp->flags & RF_HEADER)
						incat = rp->flags & curcat;
					if (!incat) {
						rp->flags &= ~RF_VISIBLE;
						rp = rp->next;
						continue;
					}
				}
				if (nscreens != curscreen) {
					rp->flags &= ~RF_VISIBLE;
				} else {
					rp->xpos = curx + MARGIN;
					rp->ypos = cury;
					rp->flags |= RF_VISIBLE;
				}
				rp = rp->next;
				cury++;
			}
			numcols++;
		}
		nscreens++;
	}
    done:
	while (rp != 0) {
		rp->flags &= ~RF_VISIBLE;
		rp = rp->next;
	}
}

/*
 * Update the information in each field by calling the fill function.
 */
static void
pupdate(int fill)
{
	Reg_t	*rp;

	piupdate(fill);
	if (fill && updatestart)
		(*updatestart)();
	for (rp = displaylist; rp != 0; rp = rp->next) {
		if (fill && rp->fillfunc)
			(*rp->fillfunc)(rp);
		predraw1(rp);
	}
	if (categ)
		pnupdate();
	if (fill && updateend)
		(*updateend)();
}

/*
 * Build up a screen image from an initialization structure.  Then go
 * into automatic mode, calling update functions and handling input until
 * killed off by the user.
 */
void
pbegin(	initReg_t	*irp,
	void		(*onupdate)(void),
	void		(*offupdate)(void),
	char		*title,
	int		wtime)
{
	Reg_t	**rp;
	Reg_t	*nrp;

	/*
	 * Keep the user on his toes ...
	 */
	printf("Initializing ...");
	fflush(stdout);

	/*
	 * Set things up.
	 */
	updatestart = onupdate;
	updateend = offupdate;

	rp = &displaylist;
	while (irp->flags != 0) {
		*rp = (Reg_t *) calloc(1, sizeof(Reg_t));
		(*rp)->flags = irp->flags;
		(*rp)->fillfunc = irp->fillfunc;
		(*rp)->fillarg = irp->fillarg;
		strncpy((*rp)->name, irp->name, TEXTSIZE);
		(*rp)->format[0] = '\0';
		if ((*rp)->flags & RF_HEADER) {
			if (nrp = (*(irp->expandfunc))(*rp)) {
				if ((*rp)->flags & RF_SUPRESS)
					poffhead(*rp);
				rp = &(nrp->next);
			} else {
				rp = &((*rp)->next);
			}
		}
		else
			rp = &((*rp)->next);
		irp++;
	}
	header = title;

	/*
	 * Create the world.
	 */
#ifdef DEBUG
	{
	   char		bbuf[BUFSIZ];

		pchain();
		printf("[Hit Return to continue]");
		fflush(stdout);
		fgets(bbuf, BUFSIZ, stdin);
	}
#endif

	initscr();
	curses_on = 1;
	pwaitval = wtime;
	psettty(1);
	playout();
	predraw(1);

	/*
	 * Now enter main processing loop, not too interesting.
	 */
	while (!pstrobekey()) {
		pupdate(1);
		move(current_y, current_x);
		refresh();
	}

	/*
	 * User typed something to make us exit.  Bye bye.
	 */
	pbyebye();
}

/*
 * Fast way out for routines with a low tolerance to error.
 */
void
pbyebye(void)
{
	if (curses_on) {
		move(categ ? nlines : nlines-1, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	exit(2);
}

/*==========>
**
**	The following routines make up the input arm portion of
**	the panel library.
**
<==========*/

/*
 * Set up the input handling to be reasonable.
 */
void
psettty(int total)
{
	struct termio	tb;

	if (total) {
		raw();
		noecho();
		keypad(stdscr, TRUE);
		leaveok(stdscr, FALSE);
		ioctl(0, TCGETA, &tb);
		intrchar = tb.c_cc[VINTR];
		suspchar = tb.c_cc[VSUSP];
	}
	else {
		ioctl(0, TCGETA, &tb);
	}
	tb.c_cc[VTIME] = 10;	/* one second poll */
	tb.c_cc[VMIN] = 0;
	ioctl(0, TCSETA, &tb);
}

/*
 * Wait for input events.  The caller may safely assume that we
 * will leave things in such a state that an interrupt (generated
 * by a timer, for instance) will cause strobekeys to return 0,
 * allowing a new update and wait scan.
 */
static int
pstrobekey(void)
{
	Reg_t	*rp;
	int	c;
	time_t	tremain;
	time_t	tlast;
	time_t	tstart;

	tremain = (time_t) pwaitval;
	tstart = time(0);

    loop:
	c = getch();
	tlast = time(0) - tstart;

	switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
		if (!categ)
			break;

		switch (c) {
		case '0': curcat = RF_HEADER; break;
		case '1': curcat = RF_HDRSYS; break;
		case '2': curcat = RF_HDRCPU; break;
		case '3': curcat = RF_HDRMEM; break;
		case '4': curcat = RF_HDRNET; break;
		case '5': curcat = RF_HDRMSC; break;
		}
		curscreen = 0;
		playout();
		predraw(0);
		move(current_y, current_x);
		phighlight(pfind(current_x, current_y));
		break;
	case KEY_IC:
	case KEY_IL:
	case KEY_NPAGE:
	case 'I':
		if ((rp = pfind(current_x, current_y)) != 0)
			if (pchkstate(rp, 1)) {
				playout();
				tlast = tremain;
			}
		break;
	case KEY_DC:
	case KEY_DL:
	case KEY_PPAGE:
	case 'D':
		if ((rp = pfind(current_x, current_y)) != 0)
			if (pchkstate(rp, 0)) {
				playout();
				tlast = tremain;
			}
		break;
	case 'C':
	case KEY_CLEAR:
		/* clear counts */
		updatelast++;
		break;
	case KEY_UP:
	case 'k':
		psafemove(up);
		break;
	case KEY_DOWN:
	case '\n':
	case '\r':
	case 'j':
		psafemove(down);
		break;
	case KEY_LEFT:
	case KEY_BACKSPACE:
	case 'h':
	case '\b':
		psafemove(left);
		break;
	case KEY_RIGHT:
	case 'l':
	case '\t':
	case ' ':
		psafemove(right);
		break;
	case '\f':
		predraw(1);
		break;
	case '\002':
		psafemove(back);
		break;
	case '\006':
		psafemove(forward);
		break;
	case KEY_HOME:
		psafemove(home);
		break;
	case KEY_BREAK:
	case 'q':
		return(1);
	default:
		if (c == intrchar)
			return(1);
		else if (c == suspchar) {
			reset_shell_mode();
			kill(getpid(), SIGTSTP);
			reset_prog_mode();
			psettty(0);
		}
		/*
		 * Ignore what we don't care about.
		 */
		break;
	}
	if (tlast < tremain)
		goto loop;
	return(0);
}

/*==========>
**
**	The following routines make up the utilities portion of
**	the panel library.
**
<==========*/

/*
 * Move due to character input, but only within the screen boundaries.
 */
static void
psafemove(int way)
{
	switch (way) {
	case up:
		if (current_y <= 0)
			current_y = 0;
		else
			current_y--;
		break;
	case down:
		if (current_y >= nlines-1)
			current_y = nlines-1;
		else
			current_y++;
		break;
	case left:
		if (logical_x <= 0)
			logical_x = 0;
		else
			logical_x--;
		current_x = logtocur(logical_x);
		break;
	case right:
		if (logical_x >= numcols)
			logical_x = numcols;
		else
			logical_x++;
		current_x = logtocur(logical_x);
		break;
	case home:
		logical_x = 0;
		current_x = logtocur(logical_x);
		current_y = 0;
		break;
	case back:
		if (curscreen <= 0) 
			curscreen = 0;
		else
			curscreen--;
		playout();
		predraw(0);
		break;
	case forward:
		if (curscreen >= nscreens) 
			curscreen = nscreens;
		else
			curscreen++;
		playout();
		predraw(0);
		break;
	}
	move(current_y, current_x);
	phighlight(pfind(current_x, current_y));
}

/*
 * Expand or contract the given element.  Return 0 if nothing
 * happened, 1 otherwise.
 */
static int
pchkstate(Reg_t *rp, int shr)
{
	if (!(rp->flags & RF_HEADER))
		return(0);
	if (!(rp->flags & RF_SUPRESS)) {
		if (shr)
			return(0);
		else {
			poffhead(rp);
			return(1);
		}
	}
	else {
		if (!shr)
			return(0);
		else {
			ponhead(rp);
			return(1);
		}
	}
	/*NOTREACHED*/
}

/*
 * Turn off an element, and any followers.  Must call playout() after
 * calling this function and predraw() to get good data.
 */
static void
poffhead(Reg_t *rp)
{
	if (rp->flags & RF_HEADER) {
		rp->flags |= RF_SUPRESS;
		rp = rp->next;
		while (rp != 0 && !(rp->flags & RF_HEADER)) {
			rp->flags &= ~RF_ON;
			rp = rp->next;
		}
	}
}

/*
 * Turn on an element, and any followers.  Same update rules as above.
 */
static void
ponhead(Reg_t *rp)
{
	if (rp->flags & RF_HEADER) {
		rp->flags &= ~RF_SUPRESS;
		rp = rp->next;
		while (rp != 0 && !(rp->flags & RF_HEADER)) {
			rp->flags |= RF_ON;
			rp = rp->next;
		}
	}
}

/*
 * Given the current screen location, find a matching element.
 */
Reg_t *
pfind(int x, int y)
{
	Reg_t	*rp;

	for (rp = displaylist; rp != 0; rp = rp->next) {
		if (!(rp->flags & RF_ON) || !(rp->flags & RF_VISIBLE))
			continue;
		if (rp->ypos == y && rp->xpos <= x &&
			x < (rp->xpos+TEXTSIZE))
			return(rp);
	}
	return(0);
}

/*
 * Allocate a new Reg_t after the given one and initialize it.
 */
Reg_t *
pfollow(Reg_t *rp)
{
	Reg_t	*nrp;

	nrp = (Reg_t *) calloc(1, sizeof(*nrp));
	*nrp = *rp;
	rp->next = nrp;
	if (rp->flags & RF_HEADER) {
		if (rp->flags & RF_SUPRESS)
			nrp->flags &= ~(RF_ON|RF_SUPRESS);
		nrp->flags &= ~RF_HEADER;
	}
	return(nrp);
}

/*
 * Routines to highlight and un-highlight the given field.
 */
static void
phighlight(Reg_t *rp)
{
	Reg_t	*orp;

	if (rp != 0 && highlighted == rp)
		return;
	if (highlighted != 0) {
		orp = highlighted;
		highlighted = 0;
		predraw1(orp);
	}
	if (rp != 0) {
		highlighted = rp;
		predraw1(rp);
	}
	move(current_y, current_x);
}

#ifdef DEBUG
static void
pchain(void)
{
	Reg_t	*rp;
	int	i;

	for (i = 0, rp = displaylist; rp != 0; rp = rp->next, i++) {
		fprintf(stderr, "%2d: <%d,%d> \"%s\" \"%s\" '%#x' %#x\n", i,
			rp->xpos,
			rp->ypos, rp->name, rp->format, rp->flags, rp->fillarg);
	}
}
#endif

/*
 * Init/print header
 */
static void
piupdate(int fill)
{
	static struct tm	*curtime;
	static char		nn[12];
	static int		cursamp;
	time_t			tme;
	char			hdrstr[160];

	if (fill) {
		cursamp++;
		time(&tme);
		curtime = localtime(&tme);
		if (nn[0] == '\0') {
			struct utsname	ubuf;

			uname(&ubuf);
			strncpy(nn, ubuf.nodename, 11);
		}
	}
	sprintf(hdrstr, "%.35s%12s %0.2d:%0.2d:%0.2d %0.2d/%0.2d/%0.2d "
			"#%-7dint=%ds", header, nn, curtime->tm_hour,
			curtime->tm_min, curtime->tm_sec, curtime->tm_mon+1,
			curtime->tm_mday, curtime->tm_year%100, cursamp,
			pwaitval);
	move(0, MARGIN);
	standout();
	printw("%.*s", COLS-MARGIN, hdrstr);
	standend();
}

/*
 * Init/print footer
 */
static void
pnupdate(void)
{
	move(nlines, MARGIN);
	standout();
	printw("1:Sys 2:Cpu 3:Mem 4:Net 5:Other 0:All");
	if (nscreens > 0)
		printw(" (^f:forw ^b:back)");
	standend();
}
