/*
 *  file system buffer cache display for Irix
 *
 *  Converted from top(1) users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/*
 *  This file contains the routines that display information on the screen.
 *  Each section of the screen has two routines:  one for initially writing
 *  all constant and dynamic text, and one for only updating the text that
 *  changes.  The prefix "i_" is used on all the "initial" routines and the
 *  prefix "u_" is used for all the "updating" routines.
 *
 *  ASSUMPTIONS:
 *        None of the "i_" routines use any of the termcap capabilities.
 *        In this way, those routines can be safely used on terminals that
 *        have minimal (or nonexistant) terminal capabilities.
 *
 *        The routines are called in this order:
 */

#include "os.h"
#include <ctype.h>
#include <time.h>

#include "screen.h"		/* interface to screen package */
#include "layout.h"		/* defines for screen position layout */
#include "display.h"
#include "bv.h"
#include "boolean.h"
#include "machine.h"		/* we should eliminate this!!! */
#include "utils.h"

#ifdef DEBUG
FILE *debug;
#endif

/* imported from screen.c */
extern int overstrike;

static int last_hi = 0;		/* used in u_process and u_endscreen */
static int lastline = 0;
static int display_width = MAX_COLS;

#define lineindex(l) ((l)*display_width)

/* things initialized by display_init and used thruout */

/* buffer of proc information lines for display updating */
char *screenbuf = NULL;

static int num_sysstats;
static int num_datastats;
static int num_emptystats;
static int num_getstats;

int *sysbuf_stats, *lsysbuf_stats;
int *databuf_stats, *ldatabuf_stats;
int *databuf_stats, *ldatabuf_stats;
int *emptybuf_stats, *lemptybuf_stats;
int *getbuf_stats, *lgetbuf_stats;

static enum { OFF, ON, ERASE } header_status = ON;

static void summary_format();
static int string_count(char **);
static void line_update();

int
display_resize(void)
{
	register int lines;

	/* first, deallocate any previous buffer that may have been there */
	if (screenbuf != NULL)
	{
		free(screenbuf);
	}

	/* calculate the current dimensions */
	/* if operating in "dumb" mode, we only need one line */
	lines = smart_terminal ? screen_length - Header_lines : 1;

	/* we don't want more than MAX_COLS columns, since the machine-
	 * dependent modules make static allocations based on MAX_COLS
	 * and we don't want to run off the end of their buffers
	 */
	display_width = screen_width;
	if (display_width >= MAX_COLS)
	{
		display_width = MAX_COLS - 1;
	}

	/* now, allocate space for the screen buffer */
	screenbuf = (char *)malloc(lines * display_width);
	if (screenbuf == (char *)NULL)
	{
		/* oops! */
		return(-1);
	}

	/* return number of lines available */
	/* for dumb terminals, pretend like we can show any amount */
	return(smart_terminal ? lines : Largest);
}

int
display_init(void)
{
	int lines;

	/* call resize to do the dirty work */
	lines = display_resize();

	/* only do the rest if we need to */
	if (lines > -1)
	{
		num_sysstats = string_count(sysbuf_names);
		sysbuf_stats = (int *)malloc(num_sysstats * sizeof(int));
		lsysbuf_stats = (int *)malloc(num_sysstats * sizeof(int));

		num_datastats = string_count(databuf_names);
		databuf_stats = (int *)malloc(num_datastats * sizeof(int));
		ldatabuf_stats = (int *)malloc(num_datastats * sizeof(int));

		num_emptystats = string_count(emptybuf_names);
		emptybuf_stats = (int *)malloc(num_emptystats * sizeof(int));
		lemptybuf_stats = (int *)malloc(num_emptystats * sizeof(int));

		num_getstats = string_count(getbuf_names);
		getbuf_stats = (int *)malloc(num_getstats * sizeof(int));
		lgetbuf_stats = (int *)malloc(num_getstats * sizeof(int));
	}

	/* return number of lines available */
	return(lines);
}

void
i_timeofday(time_t *tod)
{
	extern long nticks;

	if (smart_terminal)
		Move_to(x_ticks, y_ticks);
	printf("tick: %10d", nticks);

	/*
	 *  Display the current time.
	 *  "ctime" always returns a string that looks like this:
	 *  
	 *	Sun Sep 16 01:03:52 1973
	 *      012345678901234567890123
	 *	          1         2
	 *
	 *  We want indices 11 thru 18 (length 8).
	 */

	if (smart_terminal)
	{
		Move_to(screen_width - 8, 0);
	}
	else
	{
		fputs("    ", stdout);
	}

	printf("%-8.8s\n", &(ctime(tod)[11]));
	lastline = 1;
}

static char sysbufstats_buffer[MAX_COLS];

/*
 *  i_sysbufs() - print the system buffers line
 *
 *  Assumptions:  cursor is at the beginning of the line on entry
 *		  lastline is valid
 */
void
i_sysbufs(void)
{
	/*
	 * clear the screen since this is first
	 */
	if (smart_terminal)
		clear();

	/* format and print the system buffer statistics summary */
	summary_format(sysbufstats_buffer, sysbuf_stats, sysbuf_names);
	fputs(sysbufstats_buffer, stdout);

	/* save the numbers for next time */
	memcpy(lsysbuf_stats, sysbuf_stats, num_sysstats * sizeof(int));
}

void
u_sysbufs(void)
{
	char new[MAX_COLS];

	/* see if any of the state numbers has changed */
	if (memcmp(lsysbuf_stats, sysbuf_stats, num_sysstats * sizeof(int)))
	{
		/* format and update the line */
		summary_format(new, sysbuf_stats, sysbuf_names);
		line_update(sysbufstats_buffer, new, x_sysstat, y_sysstat);
		memcpy(lsysbuf_stats, sysbuf_stats, num_sysstats * sizeof(int));
	}
}

static char databufstats_buffer[MAX_COLS];

void
i_databufs(void)
{

	/* format and print the data buffer statistics summary */
	summary_format(databufstats_buffer, databuf_stats, databuf_names);
	fputs(databufstats_buffer, stdout);

	/* save the numbers for next time */
	memcpy(ldatabuf_stats, databuf_stats, num_datastats * sizeof(int));
}

void
u_databufs(void)
{
	char new[MAX_COLS];

	/* see if any of the state numbers has changed */
	if (memcmp(ldatabuf_stats, databuf_stats, num_datastats * sizeof(int)))
	{
		/* format and update the line */
		summary_format(new, databuf_stats, databuf_names);
		line_update(databufstats_buffer, new, x_datastat, y_datastat);
		memcpy(ldatabuf_stats, databuf_stats,
				num_datastats * sizeof(int));
	}
}

static char emptybufstats_buffer[MAX_COLS];

void
i_emptybufs(void)
{
	fputs("\n", stdout);
	lastline++;

	/* format and print the empty buffer statistics summary */
	summary_format(emptybufstats_buffer, emptybuf_stats, emptybuf_names);
	fputs(emptybufstats_buffer, stdout);

	/* save the numbers for next time */
	memcpy(lemptybuf_stats, emptybuf_stats, num_emptystats * sizeof(int));
}

void
u_emptybufs(void)
{
	char new[MAX_COLS];

	/* see if any of the state numbers has changed */
	if (memcmp(lemptybuf_stats, emptybuf_stats, num_emptystats*sizeof(int)))
	{
		/* format and update the line */
		summary_format(new, emptybuf_stats, emptybuf_names);
		line_update(emptybufstats_buffer, new,
				x_emptystat, y_emptystat);
		memcpy(lemptybuf_stats, emptybuf_stats,
			num_emptystats * sizeof(int));
	}
}

static char getbufstats_buffer[MAX_COLS];

void
i_getbufs(void)
{
	fputs("\n", stdout);
	lastline++;

	/* format and print the get-buffer statistics summary */
	summary_format(getbufstats_buffer, getbuf_stats, getbuf_names);
	fputs(getbufstats_buffer, stdout);

	/* save the numbers for next time */
	memcpy(lgetbuf_stats, getbuf_stats, num_getstats * sizeof(int));
}

void
u_getbufs(void)
{
	char new[MAX_COLS];

	/* see if any of the state numbers has changed */
	if (memcmp(lgetbuf_stats, getbuf_stats, num_getstats * sizeof(int)))
	{
		/* format and update the line */
		summary_format(new, getbuf_stats, getbuf_names);
		line_update(getbufstats_buffer, new, x_getstat, y_getstat);
		memcpy(lgetbuf_stats, getbuf_stats,
				num_getstats * sizeof(int));
	}
}


char order_buffer[MAX_COLS];

void
i_order(void)
{
	static int old_size;
	int n;

	fputs("\n", stdout);
	lastline++;

	n = printf("Order: %s", display_order());

	n += display_devs();

	if (bst.bflags || bst.bvtype)
		n += printf("  Display flags: %s",
			    display_flags(bst.bflags, bst.bvtype, ", "));

	if (old_size > n)
		(void) clear_eol(old_size);
	old_size = n;
}

void
u_order(void)
{
}

/*
 *  *_message() - print the next pending message line, or erase the one
 *                that is there.
 *
 *  Note that u_message is (currently) the same as i_message.
 *
 *  Assumptions:  lastline is consistent
 */

/*
 *  i_message is funny because it gets its message asynchronously (with
 *	respect to screen updates).
 */

static char next_msg[MAX_COLS + 5];
static int msglen = 0;

/*
 * Invariant: msglen is always the length of the message currently displayed
 * on the screen (even when next_msg doesn't contain that message).
 */
void
i_message()
{
	while (lastline < y_message)
	{
		fputc('\n', stdout);
		lastline++;
	}

	if (next_msg[0] != '\0')
	{
		standout(next_msg);
		msglen = strlen(next_msg);
		next_msg[0] = '\0';
	}
	else if (msglen > 0)
	{
		(void) clear_eol(msglen);
		msglen = 0;
	}
}

void
u_message()
{
	i_message();
}

static int header_length;

/*
 *  *_header(text) - print the header for the buffer entries
 *
 *  Assumptions:  cursor is on the previous line and lastline is consistent
 */

void
i_header(void)
{
	extern char header_separate[];
	extern char header_aggregate[];

	if (header_status == ON)
	{
		putchar('\n');
		if (bst.separate)
		{
			header_length = strlen(header_separate);
			standout(header_separate);
		}
		else
		{
			header_length = strlen(header_aggregate);
			standout(header_aggregate);
		}
		lastline++;
	}
	else if (header_status == ERASE)
	{
		header_status = OFF;
	}
}

/*ARGSUSED*/
void
u_header(void)
{
	if (header_status == ERASE)
	{
		putchar('\n');
		lastline++;
		clear_eol(header_length);
		header_status = OFF;
	}
}

void
u_endscreen(int hi)
{
    register int screen_line = hi + Header_lines;
    register int i;

    if (smart_terminal)
    {
	if (hi < last_hi)
	{
	    /* need to blank the remainder of the screen */
	    /* but only if there is any screen left below this line */
	    if (lastline + 1 < screen_length)
	    {
		/* efficiently move to the end of currently displayed info */
		if (screen_line - lastline < 5)
		{
		    while (lastline < screen_line)
		    {
			putchar('\n');
			lastline++;
		    }
		}
		else
		{
		    Move_to(0, screen_line);
		    lastline = screen_line;
		}

		if (clear_to_end)
		{
		    /* we can do this the easy way */
		    putcap(clear_to_end);
		}
		else
		{
		    /* use clear_eol on each line */
		    i = hi;
		    while ((void) clear_eol(strlen(&screenbuf[lineindex(i++)])), i < last_hi)
		    {
			putchar('\n');
		    }
		}
	    }
	}
	last_hi = hi;

	/* move the cursor to a pleasant place */
	Move_to(x_idlecursor, y_idlecursor);
	lastline = y_idlecursor;
    }
    else
    {
	/* separate this display from the next with some vertical room */
	fputs("\n\n", stdout);
    }
}

void
display_header(int t)
{
	if (t)
	{
		header_status = ON;
	}
	else if (header_status == ON)
	{
		header_status = ERASE;
	}
}

/*VARARGS2*/
void
new_message(int type, char *msgfmt, char *a1, char *a2, char *a3)
{
    register int i;

    /* first, format the message */
    (void) sprintf(next_msg, msgfmt, a1, a2, a3);

    if (msglen > 0)
    {
	/* message there already -- can we clear it? */
	if (!overstrike)
	{
	    /* yes -- write it and clear to end */
	    i = strlen(next_msg);
	    if ((type & MT_delayed) == 0)
	    {
		type & MT_standout ? standout(next_msg) :
		                     fputs(next_msg, stdout);
		(void) clear_eol(msglen - i);
		msglen = i;
		next_msg[0] = '\0';
	    }
	}
    }
    else
    {
	if ((type & MT_delayed) == 0)
	{
	    type & MT_standout ? standout(next_msg) : fputs(next_msg, stdout);
	    msglen = strlen(next_msg);
	    next_msg[0] = '\0';
	}
    }
}

void
clear_message(void)
{
    if (clear_eol(msglen) == 1)
    {
	putchar('\r');
    }
}

int
readline(char *buffer, int size, int numeric)
{
    register char *ptr = buffer;
    register char ch;
    register char cnt = 0;
    register char maxcnt = 0;

    /* allow room for null terminator */
    size -= 1;

    /* read loop */
    while ((fflush(stdout), read(0, ptr, 1) > 0))
    {
	/* newline means we are done */
	if ((ch = *ptr) == '\n')
	{
	    break;
	}

	/* handle special editing characters */
	if (ch == ch_kill)
	{
	    /* kill line -- account for overstriking */
	    if (overstrike)
	    {
		msglen += maxcnt;
	    }

	    /* return null string */
	    *buffer = '\0';
	    putchar('\r');
	    return(-1);
	}
	else if (ch == ch_erase)
	{
	    /* erase previous character */
	    if (cnt <= 0)
	    {
		/* none to erase! */
		putchar('\7');
	    }
	    else
	    {
		fputs("\b \b", stdout);
		ptr--;
		cnt--;
	    }
	}
	/* check for character validity and buffer overflow */
	else if (cnt == size || (numeric && !isdigit(ch)) ||
		!isprint(ch))
	{
	    /* not legal */
	    putchar('\7');
	}
	else
	{
	    /* echo it and store it in the buffer */
	    putchar(ch);
	    ptr++;
	    cnt++;
	    if (cnt > maxcnt)
	    {
		maxcnt = cnt;
	    }
	}
    }

    /* all done -- null terminate the string */
    *ptr = '\0';

    /* account for the extra characters in the message area */
    /* (if terminal overstrikes, remember the furthest they went) */
    msglen += overstrike ? maxcnt : cnt;

    /* return either inputted number or string length */
    putchar('\r');
    return(cnt == 0 ? -1 : numeric ? atoi(buffer) : cnt);
}

/*
 *  *_buffers(line, thisline) - print one buffer usage line
 *
 *  Assumptions:  lastline is consistent
 */
void
i_buffers(int line, char *thisline)
{
	register char *p;
	register char *base;

	/* make sure we are on the correct line */
	while (lastline < y_buffers + line)
	{
		putchar('\n');
		lastline++;
	}

	/* truncate the line to conform to our current screen width */
	thisline[display_width] = '\0';

	/* write the line out */
	fputs(thisline, stdout);

	/* copy it in to our buffer */
	base = smart_terminal ? screenbuf + lineindex(line) : screenbuf;
	p = strecpy(base, thisline);

	/* zero fill the rest of it */
	memzero(p, display_width - (p - base));
}

void
u_buffers(int line, char *newline)
{
	register char *optr;
	register int screen_line = line + Header_lines;
	register char *bufferline;

	/* remember a pointer to the current line in the screen buffer */
	bufferline = &screenbuf[lineindex(line)];

	/* truncate the line to conform to our current screen width */
	newline[display_width] = '\0';

	/* is line higher than we went on the last display? */
	if (line >= last_hi)
	{
		/* yes, just ignore screenbuf and write it out directly */
		/* get positioned on the correct line */
		if (screen_line - lastline == 1)
		{
			putchar('\n');
			lastline++;
		}
		else
		{
			Move_to(0, screen_line);
			lastline = screen_line;
		}

		/* now write the line */
		fputs(newline, stdout);

		/* copy it in to the buffer */
		optr = strecpy(bufferline, newline);

		/* zero fill the rest of it */
		memzero(optr, display_width - (optr - bufferline));
	}
	else
	{
		line_update(bufferline, newline, 0, line + Header_lines);
	}
}

static int
string_count(char **pp)
{
	int cnt = 0;

	while (*pp++ != NULL) {
		cnt++;
	}
	return cnt;
}

static void
summary_format(char *str, int *numbers, char **names)
{
	int64_t  num;
	char *p;
	char *thisname;
	static char buffer[24];

	/* format each number followed by its string */
	p = str;
	while ((thisname = *names++) != NULL)
	{
		/* get the number to format */
		num = *numbers++;

		/* is this number in pages? */
		if (thisname[0] == 'P')
		{
			/* yes: format it as a memory value */
			p = strecpy(p, konvert(num * pagesize));

			/* skip over the P */
			p = strecpy(p, thisname+1);
		}
		else if (thisname[0] == 'K')
		{
			/* yes: format it as a memory value */
			p = strecpy(p, konvert(num));

			/* skip over the K */
			p = strecpy(p, thisname+1);
		}
		else if (thisname[0] == 'J')
		{
			/* yes: format it as a memory value */
			sprintf(buffer, "%6lld", num);
			p = strecpy(p, buffer);

			/* skip over the J */
			p = strecpy(p, thisname+1);
		}
		else if (thisname[0] == 'F')
		{
			/* format it as a float value */

			sprintf(buffer, "%5.1f", (float)num);
			p = strecpy(p, buffer);

			/* skip over the F */
			p = strecpy(p, thisname+1);
		}
		else
		{
			p = strecpy(p, itoa(num));
			p = strecpy(p, thisname);
		}
	}

	/* if the last two characters in the string are ", ", delete them */
	p -= 2;

	if (p >= str && p[0] == ',' && p[1] == ' ')
	{
		*p = '\0';
	}
}

static void
line_update(char * old, char * new, int start, int line)
{
    int ch;
    int diff;
    int newcol = start + 1;
    int lastcol = start;
    char cursor_on_line = No;
    char *current;

    /* compare the two strings and only rewrite what has changed */
    current = old;
#ifdef DEBUG
    fprintf(debug, "line_update, starting at %d\n", start);
    fputs(old, debug);
    fputc('\n', debug);
    fputs(new, debug);
    fputs("\n-\n", debug);
#endif

    /* start things off on the right foot		    */
    /* this is to make sure the invariants get set up right */
    if ((ch = *new++) != *old)
    {
	if (line - lastline == 1 && start == 0)
	{
	    putchar('\n');
	}
	else
	{
	    Move_to(start, line);
	}
	cursor_on_line = Yes;
	putchar(ch);
	*old = ch;
	lastcol = 1;
    }
    old++;
	
    /*
     *  main loop -- check each character.  If the old and new aren't the
     *	same, then update the display.  When the distance from the
     *	current cursor position to the new change is small enough,
     *	the characters that belong there are written to move the
     *	cursor over.
     *
     *	Invariants:
     *	    lastcol is the column where the cursor currently is sitting
     *		(always one beyond the end of the last mismatch).
     */
    do		/* yes, a do...while */
    {
	if ((ch = *new++) != *old)
	{
	    /* new character is different from old	  */
	    /* make sure the cursor is on top of this character */
	    diff = newcol - lastcol;
	    if (diff > 0)
	    {
		/* some motion is required--figure out which is shorter */
		if (diff < 6 && cursor_on_line)
		{
		    /* overwrite old stuff--get it out of the old buffer */
		    printf("%.*s", diff, &current[lastcol-start]);
		}
		else
		{
		    /* use cursor addressing */
		    Move_to(newcol, line);
		    cursor_on_line = Yes;
		}
		/* remember where the cursor is */
		lastcol = newcol + 1;
	    }
	    else
	    {
		/* already there, update position */
		lastcol++;
	    }
		
	    /* write what we need to */
	    if (ch == '\0')
	    {
		/* at the end--terminate with a clear-to-end-of-line */
		(void) clear_eol(strlen(old));
	    }
	    else
	    {
		/* write the new character */
		putchar(ch);
	    }
	    /* put the new character in the screen buffer */
	    *old = ch;
	}
	    
	/* update working column and screen buffer pointer */
	newcol++;
	old++;
	    
    } while (ch != '\0');

    /* zero out the rest of the line buffer -- MUST BE DONE! */
    diff = display_width - newcol;
    if (diff > 0)
    {
	memzero(old, diff);
    }

    /* remember where the current line is */
    if (cursor_on_line)
    {
	lastline = line;
    }
}
