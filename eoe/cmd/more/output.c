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
 * High level routines dealing with the output to the screen.
 */

#include "less.h"
#include <stdlib.h>


int errmsgs;	/* Count of messages displayed by error() */

/* display the line which is in the line buffer. */
void
put_line()
{
	register char *p;
	register int c;
	register int column;

	if (sigs)
	{
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed = 1;
		return;
	}

	if (line == NULL)
		line = "";

	column = 0;
	for (p = line;  *p != '\0';  p++)
	{
		switch (c = *p)
		{
		case UL_CHAR:
                        	ul_enter();
                        	column += ul_width;
                        	break;
		case UE_CHAR:
                                ul_exit();
                                column += ue_width;
                        	break;
		case BO_CHAR:
                                bo_enter();
                                column += bo_width;
                        	break;
		case BE_CHAR:
                                bo_exit();
                                column += be_width;
                        	break;
		case '\t':
			do {
				putchr(' ');
				column++;
			} while ((column % tabstop) != 0);
			break;
		case '\b':
			putbs();
			column--;
			break;
		default:
			if ( c == 0200 ) {

                             /* 0200 might be '\0' or '\0200' before.
                                If you don't put -u (massage) option,
                                it also might be '\r' before.

                                line buffer (line) contains the same number
                                of characters (normal width) as screen width
                                (normal width), unless it doesn't contain '\n'.
                                Each line buffer may contain '\0200's and is
                                processed by this put_line() function, but they
                                are not printed.

				'\0' and '\0200' are printed as just '^'
				in printable mode. This behavior is the 
				same behavior as original XPG4 more in C locale.
				With this fix, '\r' also gets printed as '^'  
				in printable mode. '\r' can be printed as
				'^M' with -u option in printable mode (-r option).
			     */	
                                if ( !make_printable ) 
                                      break;

				putchr('^');
				column++;
			}
			else
			if ( iscntrl(c) && make_printable  &&
			     (MB_CUR_MAX>1 ? (c!=SS2 && c!=SS3):1) ) 
 			{ 
				putchr('^');
				putchr(CARAT_CHAR(c & 0177));		
				column += 2;
			} else {
				putchr(c);
				column++;
			}
		}
	}
	if (column < sc_width || !auto_wrap || ignaw || !fold)
		putchr('\n');
}

static char obuf[1024];
static char *ob = obuf;

/*
 * Flush buffered output.
 */
void
flush()
{
	register int n;

	n = ob - obuf;
	if (n == 0)
		return;
	if (write(1, obuf, n) != n)
		screen_trashed = 1;
	ob = obuf;
}

/*
 * Purge any pending output.
 */
void
purge()
{
	ob = obuf;
}

/*
 * Output a character.
 */
void
putchr(int c)
{
	if (ob >= &obuf[sizeof(obuf)])
		flush();
	*ob++ = c;
}

/*
 * Output a string.
 */
void
putstr(register char *s)
{
	while (*s != '\0')
		putchr(*s++);
}

int cmdstack;

/*
 * Output a message in the lower left corner of the screen.
 * Wait for carriage return if wait_on_error is set.
 */
void
message(char *s)
{
	int ch;
	static char *return_to_continue;
	static int rtc_len;

	if (!any_display) {
		/*
		 * Nothing has been displayed yet.  Output this message on
		 * error output (file descriptor 2) and don't wait for a
		 * keystroke to continue.
		 *
		 * This has the desirable effect of producing all error
		 * messages on error output if standard output is directed
		 * to a file.  It also does the same if we never produce
		 * any real output; for example, if the input file(s) cannot
		 * be opened.  If we do eventually produce output, code in
		 * edit() makes sure these messages can be seen before they
		 * are overwritten or scrolled away.
		 */
		(void)write(2, s, strlen(s));
		(void)write(2, "\n", 1);
		return;
	}

	lower_left();
	clear_eol();
	so_enter();
	if (s) {
		putstr(s);
		putstr("  ");
	}
	if (!return_to_continue) {
		return_to_continue = gettxt(MSG(return),
					    "(press RETURN)");
		rtc_len = strlen(return_to_continue);
	}
	putstr(return_to_continue);
	so_exit();

	if (wait_on_error && (ch = getchr()) != '\n') {
		if (ch == 'q')
			quit();
		cmdstack = ch;
	}
	lower_left();

	if (strlen(s) + rtc_len + so_width + se_width + 1 > sc_width)
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		repaint();
	flush();
}

/* Update error count and output message.
 */
void
error(char *s)
{
	++errmsgs;
	message(s);
}

void
ierror(char *s)
{
	static char *intr_to_abort;

	if (!intr_to_abort)
		intr_to_abort = gettxt(MSG(abort), "... (interrupt to abort)");
	lower_left();
	clear_eol();
	so_enter();
	putstr(s);
	putstr(intr_to_abort);
	so_exit();
	flush();
}

#ifdef	DEBUG
/* DEBUG
 * Print a null terminated string highlighting control and other
 * special characters.
 */
void
dumpLine(char *line)
{
	char *p;

	if (!getenv("TRACE")) return;
	for (p = line;  *p != '\0';  p++) {
		switch (*p) {
		case UL_CHAR:
			printf("<ul>");
			break;
		case UE_CHAR:
			printf("<ue>");
			break;
		case BO_CHAR:
			printf("<bo>");
			break;
		case BE_CHAR:
			printf("<be>");
			break;
		case '\t':
			printf("<\\t>");
			break;
		case '\b':
			printf("<\\b>");
			break;
		default:
			if (*p & 0200)
				printf("<^%c>", *p & 0177);
			else
				printf("%c", *p);
			break;
		}
	}
	printf("\n");
}
#endif	/* DEBUG */
