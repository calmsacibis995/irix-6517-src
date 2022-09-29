/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/show.c	1.2"
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define BACKSLASH '\\'

FILE	*devtty;
int	doattributes = 1;
char	*progname;
void	usage(void);
void	done(void);
int	getcommand(void);
void	show(FILE *);

main(int argc, char **argv)
{
    FILE    *file;
    int     c;
    int     useidl = 1, usenonl = 1;
    extern int getopt();
    extern int optind;
    extern char *getenv();

    progname = argv[0];
    devtty = fopen("/dev/tty", "r");
    if (devtty == NULL)
	{
	(void) fprintf (stderr, "%s: cannot open /dev/tty\n", progname);
	perror("/dev/tty");
	exit(1);
	}

    while ((c = getopt (argc, argv, "ina")) != EOF)
	switch (c)
	    {
	    case 'i': useidl = 0; break;
	    case 'n': usenonl = 0; break;
	    case 'a': doattributes = 0; break;
	    case '?': usage();
	    }
    (void) signal (SIGINT, done);	/* die gracefully */

    /* initialize curses */
    if (newterm(getenv("TERM"), stdout, devtty) == NULL)
	{
	char *term = getenv("TERM");
	(void) fprintf (stderr, "%s: screen initialization failed, TERM=%s\n",
	    progname, term ? term : "");
	exit(1);
	}
    noecho ();				/* turn off tty echo */
    cbreak ();				/* enter cbreak mode */
    if (usenonl)			/* allow more optimizations */
	nonl ();
    if (useidl)				/* allow insert/delete line */
	idlok (stdscr, TRUE);

    if (argc == optind)
	show(stdin);
    else
	for ( ; optind < argc; optind++)
	    if (strcmp(argv[optind], "-") == 0)
		show(stdin);
	    else if ((file = fopen(argv[optind], "r")) == NULL)
		{
		(void) fprintf (stderr, "%s: cannot open '%s'\n",
		    progname, argv[optind]);
		perror (argv[optind]);
		}
	    else
		{
		show(file);
		fclose(file);
		}
    done();
}

void
show(FILE *file)
{
    int y, x, newy, newx, line, reading = 1;
    chtype c1, c2;

    while (reading)
	{					/* for each screen full */
	(void) move (0, 0);
	werase (stdscr);
	for (line = 0; line < LINES;)
	    {
	    getyx(stdscr,y,x);
	    switch ((char) (c1 = (chtype) getc (file)))
		{
		case BACKSLASH:
		    if (doattributes)
			c2 = (chtype) getc (file);
		    else
			c2 = BACKSLASH;
		    switch ((char) c2)
			{
			int newrow, newcol;
			case EOF: addch (c1); clrtobot (); reading = 0; goto ref;
			case 'O': attron (A_STANDOUT); break;
			case 'U': attron (A_UNDERLINE); break;
			case 'R': attron (A_REVERSE); break;
			case 'K': attron (A_BLINK); break;
			case 'D': attron (A_DIM); break;
			case 'B': attron (A_BOLD); break;
			case 'I': attron (A_INVIS); break;
			case 'P': attron (A_PROTECT); break;
			case 'A': attron (A_ALTCHARSET); break;
			case 'G':
				newrow = getc(file) - '0';
				newrow = newrow * 10 + getc(file) - '0';
				newcol = getc(file) - '0';
				newcol = newcol * 10 + getc(file) - '0';
				move(newrow, newcol);
				break;
			case '-': attrset (0); break;
			case '\f': addch (c1); line = LINES; break;
			case BACKSLASH: addch(c2); break;
			case '\n':
			    if (line == LINES-1) goto ref;
			    /* no break */
			default:
			    if (addch (c1) == ERR)
				{
				ungetc((int) c2,file);
				goto ref;
				}
			    if (addch (c2) == ERR) goto ref;
			    break;
			}
		    break;
		case EOF: clrtobot (); reading = 0; goto ref;
		case '\f': line = LINES; break;
		case '\n': if (line == LINES-1) goto ref; /* no break */
		default: if (addch (c1) == ERR) goto ref; break;
		}
	    getyx(stdscr,newy,newx);
	    if (newy != y)
		line++;
	    }
    ref:
	stdscr->_use_idl = 2;
	(void) refresh ();		/* sync screen */
	if (getcommand())
	    break;
	}
}

void
usage(void)
{
    (void) fprintf (stderr, "usage: %s [-ina] files\n",
	progname);
    exit(2);
}

int
getcommand(void)
{
    int c;

    while ((c = getch()) == '\f')	/* repaint if asked to */
	{
	clearok(curscr,TRUE);
	wrefresh(curscr);
	}
    if (c == 'q' || c == 'Q')
	done ();
    if (c == 'n' || c == 'N')
	return 1;
    else
	return 0;
}

/*
 * Clean up and exit.
 */
void
done(void)
{
	(void) move(LINES-1,0);		/* to lower left corner */
	clrtoeol();			/* clear bottom line */
	(void) refresh();		/* flush out everything */
	endwin();			/* curses cleanup */
	exit(0);
}
