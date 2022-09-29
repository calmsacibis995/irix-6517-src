/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Routines to manipulate the "line buffer".
 * The line buffer holds a line of output as it is being built
 * in preparation for output to the screen.
 * We keep track of the PRINTABLE length of the line as it is being built.
 */

#include "less.h"
#include <sys/types.h>
#include <ctype.h>

#define BACKSPACE	'\b'

static char linebuf[1024];	/* Buffer which holds the current output line */
static char *curr;		/* Pointer into linebuf */
static int column;		/* Printable length, accounting for
				   backspaces, etc. */
static int prev_backspace;	/* Have we seen a backspace which could lead
				   into a highlighting mode ? */

/*
 * A ridiculously complex state machine takes care of backspaces.  The
 * complexity arises from the attempt to deal with all cases, especially
 * involving long lines with underlining, boldfacing or whatever.  There
 * are still some cases which will break it.
 *
 * There are four states:
 *	LN_NORMAL is the normal state (not in underline mode).
 *	LN_UNDERLINE means we are in underline mode.  We expect to get
 *		either a sequence like "_\bX" or "X\b_" to continue
 *		underline mode, or anything else to end underline mode.
 *	LN_BOLDFACE means we are in boldface mode.  We expect to get sequences
 *		like "X\bX\b...X\bX" to continue boldface mode, or anything
 *		else to end boldface mode.
 */
static int ln_state;		/* Currently in normal/underline/bold/etc mode? */
#define	LN_NORMAL	0	/* Not in underline, boldface mode */
#define	LN_UNDERLINE	0x1
#define	LN_BOLDFACE	0x2

char *line;			/* Pointer to the current line.
				   Usually points to linebuf. */

/*
 * Rewind the line buffer.
 */
void
prewind()
{
	line = curr = linebuf;
	ln_state = LN_NORMAL;
	column = 0;
	prev_backspace = 0;
}


/* Increment column width and return if we ran over */
#define ADDTOBUF(count) \
	if (((column += (count)) > sc_width) && fold) return (1)

/* Return if increment is too big */
#define TRYADDTOBUF(count) \
	if (((column + (count)) > sc_width) && fold) return (1)

/*
 * Append character to buffer if there is room.
 * Process backspace highlight and underline sequences.
 */
int
pappend(int c)
{
	if (curr > linebuf + sizeof(linebuf) - 12)
		/*
		 * Almost out of room in the line buffer.
		 * Don't take any chances.
		 * {{ Linebuf is supposed to be big enough that this
		 *    will never happen, but may need to be made 
		 *    bigger for wide screens or lots of backspaces. }}
		 */
		return(1);

	if (c == BACKSPACE && massage) {

		if (prev_backspace || curr == linebuf) {
			prev_backspace = 0;
		} else {
			/* Ensure we have room on screen to start and
			 * end a mode.
			 * XXX assumes bo/be are same width as ul/ue.
			 */
			TRYADDTOBUF(bo_width + be_width);
			prev_backspace = 1;
		}

		*curr++ = BACKSPACE;
		return (0);

	}

	if (!prev_backspace || c == '\0') {

		/* Two cases:
		 * 1. We are not treating backspace sequences specially.
		 *
		 * 2. The previous character was not a backspace and
		 *    the current char is not a backspace or
		 *    we have the null terminator.
		 */

		/* Terminate current mode.
		 */
		switch (ln_state) {
		case LN_BOLDFACE	:
			*curr++ = BE_CHAR;
			column += be_width;
			ln_state &= ~LN_BOLDFACE;
			break;
		case LN_UNDERLINE	:
			*curr++ = UE_CHAR;
			column += ue_width;
			ln_state &= ~LN_UNDERLINE;
			break;
		case LN_UNDERLINE|LN_BOLDFACE :
			*curr++ = BE_CHAR;
			*curr++ = UE_CHAR;
			column += be_width + ue_width;
			ln_state &= ~(LN_BOLDFACE|LN_UNDERLINE);
			break;
		}

		if (!iscntrl(c)) {			/* common case */

			ADDTOBUF(1);
			*curr++ = c;

		} else if (c == '\0') {			/* terminator */

			*curr = '\0';
			prev_backspace = 0;

		} else if (c == '\t' && massage) {	/* tab padding */

			ADDTOBUF(tabstop - (column % tabstop));
			*curr++ = '\t';

		} else if (make_printable) {		/* make it printable */

		/*** I18NCAT_8BIT_CLEAN ****/

			ADDTOBUF(2);          
			*curr++ = c; 

		} else {				/* print as is */

			ADDTOBUF(1);
			*curr++ = c;
		}

		return (0);

	}

	/* The previous character was a backspace and
	 * there is a non-backspace char before it and
	 * the current char which is not a backspace or a null.
	 *
	 * X \b X	Bold mode
	 * X \b _	Underline
	 * _ \b X	Underline
	 *
	 * Multiple overstrikes condense (X \b X \b X \b _ \b _).
	 */
	if (c == curr[-2]) {

		/* Bold char.
		 * Start bold sequence if not already in one.
		 */
		if (!(ln_state & LN_BOLDFACE)) {
			ln_state |= LN_BOLDFACE;

			/* Try to avoid mode changes.
			 */
			if (curr - linebuf < 5 || curr[-3] != BE_CHAR) {
				/* X.\b.X -> bo.X */
				curr--;
				curr[-1] = BO_CHAR;
				column += bo_width;
			} else {
				/* bo.Y.be.X.\b.X -> bo.Y.X */
				curr -= 3;
				column -= be_width;
			}
		} else {
			/* bo.X.\b.X -> bo.X */
			curr -= 2;
		}
	} else if (c == '_' || curr[-2] == '_') {

		/* Underline char.
		 * Start underline sequence if not already in one.
		 */
		if (!(ln_state & LN_UNDERLINE)) {
			ln_state |= LN_UNDERLINE;
			if (c == '_')
				c = curr[-2];

			/* Try to avoid mode changes.
			 */
			if (curr - linebuf < 5 || curr[-3] != UE_CHAR) {
				/* X.\b._ -> ul.X */
				curr--;
				curr[-1] = UL_CHAR;
				column += ul_width;
			} else {
				/* ul.Y.ue.X.\b._ -> ul.Y.X */
				curr -= 3;
				column -= ue_width;
			}
		} else {
			/* ul.X.\b._ -> ul.X */
			curr -= 2;
		}
	}

	/* Save the highlighted character.
	 */
	*curr++ = c;
	prev_backspace = 0;

	return (0);
}

/*
 * Analogous to forw_line(), but deals with "raw lines":
 * lines which are not split for screen width.
 * {{ This is supposed to be more efficient than forw_line(). }}
 */
off64_t
forw_raw_line(off64_t curr_pos)
{
	register char *p;
	register int c;
	off64_t new_pos;

	if (curr_pos == NULL_POSITION || ch_seek(curr_pos) ||
		(c = ch_forw_get()) == EOI)
		return (NULL_POSITION);

	p = linebuf;

	for (;;)
	{
		if (c == '\n' || c == EOI)
		{
			new_pos = ch_tell();
			break;
		}
		if (p >= &linebuf[sizeof(linebuf)-1])
		{
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 * {{ The line buffer is supposed to be big
			 *    enough that this never happens. }}
			 */
			new_pos = ch_tell() - 1;
			break;
		}
		*p++ = c;
		c = ch_forw_get();
	}
	*p = '\0';
	line = linebuf;
	return (new_pos);
}

/*
 * Analogous to back_line(), but deals with "raw lines".
 * {{ This is supposed to be more efficient than back_line(). }}
 */
off64_t
back_raw_line(off64_t curr_pos)
{
	register char *p;
	register int c;
	off64_t new_pos;

	if (curr_pos == NULL_POSITION || curr_pos <= (off64_t)0 ||
		ch_seek(curr_pos-1))
		return (NULL_POSITION);

	p = &linebuf[sizeof(linebuf)];
	*--p = '\0';

	for (;;)
	{
		c = ch_back_get();
		if (c == '\n')
		{
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI)
		{
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			new_pos = (off64_t)0;
			break;
		}
		if (p <= linebuf)
		{
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		*--p = c;
	}
	line = p;
	return (new_pos);
}
