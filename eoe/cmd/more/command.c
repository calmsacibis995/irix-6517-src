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

#include "less.h"
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "pathnames.h"

#define	NO_MCA		0
#define	MCA_DONE	1
#define	MCA_MORE	2

#define	CMD_RESET	cp = cmdbuf	/* reset command buffer to empty */
#define	CMD_EXEC	lower_left(); flush()

static char cmdbuf[CMDBUFLEN];	/* Buffer for holding a multi-char command */
static char *cp;		/* Pointer into cmdbuf */
static int cmd_col;		/* Current column of the multi-char command */
static int longprompt;		/* if stat command instead of prompt */
static int mca;			/* The multicharacter command (action) */
static int last_mca;		/* The previous mca */
static int number;		/* The number typed by the user */
static int wsearch;		/* Search for matches (1) or non-matches (0) */
static char *initcmd_cc;	/* Pointer for processing an initial cmd */

static void editfile(void);
static void showlist(void);
static void shellcmd(void);
static int cmd_erase(void);
static void exec_mca(void);
static int prompt(void);
static int getcc(void);

/* backspace in command buffer. */
static int
cmd_erase()
{
	/*
	 * backspace past beginning of the string: this usually means
	 * abort the command.
	 */
	if (cp == cmdbuf)
		return(1);

	/* erase an extra character, for the carat. */
	if (iscntrl(*--cp)) {
		backspace();
		--cmd_col;
	}

	backspace();
	--cmd_col;
	return(0);
}

/* set up the display to start a new multi-character command. */
void
start_mca(int action, char *prompt)
{
	lower_left();
	clear_eol();
	putstr(prompt);
	cmd_col = strlen(prompt);
	mca = action;
}

/*
 * process a single character of a multi-character command, such as
 * a number, or the pattern of a search command.
 */
static int
cmd_char(int c)
{
	if (c == erase_char)
		return(cmd_erase());
	/* in this order, in case werase == erase_char */
	if (c == werase_char) {
		if (cp > cmdbuf) {
			while (isspace(cp[-1]) && !cmd_erase());
			while (!isspace(cp[-1]) && !cmd_erase());
			while (isspace(cp[-1]) && !cmd_erase());
		}
		return(cp == cmdbuf);
	}
	if (c == kill_char) {
		while (!cmd_erase());
		return(1);
	}
	/*
	 * No room in the command buffer, or no room on the screen;
	 * {{ Could get fancy here; maybe shift the displayed line
	 * and make room for more chars, like ksh. }}
	 */
	if (cp >= &cmdbuf[sizeof(cmdbuf)-1] || cmd_col >= sc_width-3)
		bell();
	else {
		*cp++ = c;
		if ( iscntrl(c) && make_printable &&
                     (MB_CUR_MAX>1 ? (c!=SS2 && c!=SS3):1) ) 
		{
			putchr('^');
			cmd_col++;

		/**** I18NCAT_8BIT_CLEAN ****/

			c = CARAT_CHAR(c & 0177);
		}
		putchr(c);
		cmd_col++;
	}
	return(0);
}

static int
prompt()
{
	off64_t len, pos;
	char pbuf[ PATH_MAX + 256 ];

	/*
	 * if nothing is displayed yet, display starting from line 1;
	 */
	if (position(TOP) == NULL_POSITION) {
		if (forw_line((off64_t)0) == NULL_POSITION)
			return(0);

		/* see below */
		if (!initcmd || initcmd_tried)
			jump_back(1);
	}
	else if (screen_trashed)
		repaint();

	/* If there's an initial command sequence that we haven't tried
	 * arrange for it to be read as input.
	 */
	if (initcmd && !initcmd_tried) {
		initcmd_cc = initcmd;
		initcmd_tried = 1;
	}

	/* if not pausing and we've hit EOF on the last file, quit. */
	if (!wait_at_eof && hit_eof && curr_ac + 1 >= ac)
		quit();

	/* select the proper prompt and display it. */
	lower_left();
	clear_eol();
	if (longprompt) {
		so_enter();
		putstr(current_name);
		putstr(":");
		if (!ispipe) {
			_sgi_sfmtmsg(pbuf, 0, cmd_label, MM_INFO,
				     gettxt(MSG(fileno), " file %d/%d"),
				     curr_ac + 1, ac);
			putstr(pbuf);
		}
		if (linenums) {
			_sgi_sfmtmsg(pbuf, 0, cmd_label, MM_INFO,
				     gettxt(MSG(lineno), " line %d"),
				     currline(BOTTOM));
			putstr(pbuf);
		}
		if ((pos = position(BOTTOM)) != NULL_POSITION) {
			_sgi_sfmtmsg(pbuf, 0, cmd_label, MM_INFO,
				     gettxt(MSG(byteno), " byte %lld"), pos);
			putstr(pbuf);
			if (!ispipe && (len = ch_length())) {
				(void)sprintf(pbuf, "/%lld %lld%%",
					      len, ((100 * pos) / len));
				putstr(pbuf);
			}
		}
		so_exit();
		longprompt = 0;
	}
	else {
		so_enter();
		putstr(current_name);
		if (hit_eof) {
			putstr(gettxt(MSG(end), ": END"));
			if (next_name) {
				_sgi_sfmtmsg(pbuf, 0, cmd_label, MM_INFO,
					     gettxt(MSG(pausenext),
						" (next file: %s)"), next_name);
				putstr(pbuf);
			}
		}
		else if (!ispipe &&
		    (pos = position(BOTTOM)) != NULL_POSITION &&
		    (len = ch_length())) {
			(void)sprintf(pbuf, " (%lld%%)", ((100 * pos) / len));
			putstr(pbuf);
		}
		so_exit();
	}
	return(1);
}

/* get command character. */
static int
getcc()
{
	int ch;

	/* Check for saved command sequence.
	 */
	if (initcmd_cc) {
		if (*initcmd_cc)
			return(*initcmd_cc++);
		initcmd_cc = 0;
		if (mca && mca != A_DIGIT)
			return('\n');
	}

	/* left over from error() routine. */
	if (cmdstack) {
		ch = cmdstack;
		cmdstack = NULL;
		return(ch);
	}
	if (cp > cmdbuf && position(TOP) == NULL_POSITION) {
		/*
		 * Command is incomplete, so try to complete it.
		 * There are only two cases:
		 * 1. We have "/string" but no newline.  Add the \n.
		 * 2. We have a number but no command.  Treat as #g.
		 * (This is all pretty hokey.)
		 */
		if (mca != A_DIGIT)
			/* Not a number; must be search string */
			return('\n');
		else
			/* A number; append a 'g' */
			return('g');
	}
	return(getchr());
}

/* execute a multicharacter command. */
static void
exec_mca()
{
	register char *p;

	*cp = '\0';
	CMD_EXEC;
	switch (mca) {
	case A_F_SEARCH:
		(void)search(1, cmdbuf, number, wsearch);
		break;
	case A_B_SEARCH:
		(void)search(0, cmdbuf, number, wsearch);
		break;
	case A_EXAMINE:
		for (p = cmdbuf; isspace(*p); ++p);

		if (!edit(uxglob(p),-1))
			errmsgs--;	/* not a real error (XPG4) */
		break;
	case A_TAGFILE:
		for (p = cmdbuf; isspace(*p); ++p);
		findtag(p);
		if (tagfile == NULL)
			break;
		if (edit(tagfile,-1))
			(void)tagsearch();
		break;
	case A_SHELL:
		shellcmd();
		break;
	}
}

/* add a character to a multi-character command. */
static int
mca_char(int c)
{
	switch (mca) {
	case 0:			/* not in a multicharacter command. */
	case A_PREFIX:		/* in the prefix of a command. */
		return(NO_MCA);
	case A_DIGIT:
		/*
		 * Entering digits of a number.
		 * Terminated by a non-digit.
		 */
		if (!isascii(c) || !isdigit(c) &&
		    c != erase_char && c != kill_char && c != werase_char) {
			/*
			 * Not part of the number.
			 * Treat as a normal command character.
			 */
			*cp = '\0';
			number = atoi(cmdbuf);
			CMD_RESET;
			mca = 0;
			return(NO_MCA);
		}
		break;
	}

	/*
	 * Any other multicharacter command
	 * is terminated by a newline.
	 */
	if (c == '\n' || c == '\r') {
		exec_mca();
		return(MCA_DONE);
	}

	/* append the char to the command buffer. */
	if (cmd_char(c))
		return(MCA_DONE);

	return(MCA_MORE);
}

/*
 * Main command processor.
 * Accept and execute commands until a quit command, then return.
 */
void
commands()
{
	register int c;
	register int action;
	int	last_action;
	int	last_number;
	char	*last_cp;

	number = 0;
	action = A_INVALID;
	last_mca = 0;
	scroll_count = sc_height / 2;

	for (;;) {
		mca = 0;
		last_number = number;
		number = 0;

		/*
		 * See if any signals need processing.
		 */
		if (sigs) {
			psignals();
			if (quitting)
				quit();
		}
		/*
		 * Display prompt and accept a character.
		 */
		last_cp = cp;
		CMD_RESET;
		if (!prompt()) {
			next_file(1);
			continue;
		}
		noprefix();
		c = getcc();

again:		if (sigs)
			continue;

		/*
		 * If we are in a multicharacter command, call mca_char.
		 * Otherwise we call cmd_decode to determine the
		 * action to be performed.
		 */
		if (mca)
			switch (mca_char(c)) {
			case MCA_MORE:
				/*
				 * Need another character.
				 */
				c = getcc();
				goto again;
			case MCA_DONE:
				/*
				 * Command has been handled by mca_char.
				 * Start clean with a prompt.
				 */
				continue;
			case NO_MCA:
				/*
				 * Not a multi-char command
				 * (at least, not anymore).
				 */
				break;
			}

		last_action = action;
		action = cmd_decode(c);
repeat_last:
		/* decode the command character and decide what to do. */
		switch (action) {

		case A_DIGIT:		/* first digit of a number */
			start_mca(A_DIGIT, ":");
			goto again;
		case A_F_SCREEN:	/* forward one screen */
			CMD_EXEC;
			if (number <= 0 && (number = sc_window) <= 0)
				number = sc_height - 1;
			forward(number, 1);
			break;
		case A_B_SCREEN:	/* backward one screen */
			CMD_EXEC;
			if (number <= 0 && (number = sc_window) <= 0)
				number = sc_height - 1;
			backward(number, 1);
			break;
		case A_F_LINE:		/* forward N (default 1) line */
			CMD_EXEC;
			forward(number <= 0 ? 1 : number, 0);
			break;
		case A_B_LINE:		/* backward N (default 1) line */
			CMD_EXEC;
			backward(number <= 0 ? 1 : number, 0);
			break;
		case A_F_SCROLL:	/* forward N lines */
			CMD_EXEC;
			if (number > 0)
				scroll_count = number;
			forward(scroll_count, 0);
			break;
		case A_B_SCROLL:	/* backward N lines */
			CMD_EXEC;
			if (number > 0)
				scroll_count = number;
			backward(scroll_count, 0);
			break;
		case A_FREPAINT:	/* flush buffers and repaint */
			if (!ispipe) {
				ch_init(0, 0);
				clr_linenum();
			}
			/* FALLTHROUGH */
		case A_REPAINT:		/* repaint the screen */
			CMD_EXEC;
			repaint();
			break;
		case A_GOLINE:		/* go to line N, default 1 */
			CMD_EXEC;
			if (number <= 0)
				number = 1;
			jump_back(number);
			break;
		case A_PERCENT:		/* go to percent of file */
			CMD_EXEC;
			if (number < 0)
				number = 0;
			else if (number > 100)
				number = 100;
			jump_percent(number);
			break;
		case A_GOEND:		/* go to line N, default end */
			CMD_EXEC;
			if (number <= 0)
				jump_forw();
			else
				jump_back(number);
			break;
		case A_STAT:		/* print file name, etc. */
			longprompt = 1;
			continue;
		case A_QUIT:		/* exit */
			quit();
		case A_F_SEARCH:	/* search for a pattern */
		case A_B_SEARCH:
			if (number <= 0)
				number = 1;
			start_mca(action, (action==A_F_SEARCH) ? "/" : "?");
			last_mca = mca;
			wsearch = 1;
			c = getcc();
			if (c == '!') {
				/*
				 * Invert the sense of the search; set wsearch
				 * to 0 and get a new character for the start
				 * of the pattern.
				 */
				start_mca(action, 
				    (action == A_F_SEARCH) ? "!/" : "!?");
				wsearch = 0;
				c = getcc();
			}
			action = A_AGAIN_SEARCH;	/* for REPEAT */
			goto again;
		case A_REVERSE_SEARCH:		/* reverse last search */
			if (last_mca == A_F_SEARCH)
				last_mca = A_B_SEARCH;
			else
				last_mca = A_F_SEARCH;
			/* FALLTHROUGH */
		case A_AGAIN_SEARCH:		/* repeat previous search */
			if (number <= 0)
				number = 1;
			if (wsearch)
				start_mca(last_mca, 
				    (last_mca == A_F_SEARCH) ? "/" : "?");
			else
				start_mca(last_mca, 
				    (last_mca == A_F_SEARCH) ? "!/" : "!?");
			CMD_EXEC;
			(void)search(mca == A_F_SEARCH, (char *)NULL,
			    number, wsearch);
			break;
		case A_HELP:			/* help */
			lower_left();
			clear_eol();
			putstr(gettxt(MSG(help), "help"));
			CMD_EXEC;
			help();
			break;
		case A_TAGFILE:			/* tag a new file */
			CMD_RESET;
			start_mca(A_TAGFILE, "Tag: ");
			c = getcc();
			goto again;
		case A_FILE_LIST:		/* show list of file names */
			CMD_EXEC;
			showlist();
			repaint();
			break;
		case A_EXAMINE:			/* edit a new file */
			CMD_RESET;
			start_mca(A_EXAMINE, "Examine: ");
			c = getcc();
			goto again;
		case A_VISUAL:			/* invoke the editor */
			if (ispipe) {
				error(gettxt(MSG(ttyedit),
					     "Cannot edit standard input"));
				break;
			}
			CMD_EXEC;
			editfile();
			ch_init(0, 0);
			clr_linenum();
			break;
		case A_SHELL:			/* run command in subshell */
			CMD_RESET;
			start_mca(A_SHELL, "Command: ");
			c = getcc();
			goto again;
		case A_REPEAT:			/* repeat last command */
			number = last_number;
			action = last_action;
			if (last_cp != cmdbuf && action != A_AGAIN_SEARCH)
				initcmd_cc = cmdbuf;
			goto repeat_last;
			/* NOTREACHED */
			break;
		case A_NEXT_FILE:		/* examine next file */
			if (number <= 0)
				number = 1;
			next_file(number);
			break;
		case A_PREV_FILE:		/* examine previous file */
			if (number <= 0)
				number = 1;
			prev_file(number);
			break;
		case A_SETMARK:			/* set a mark */
			lower_left();
			clear_eol();
			start_mca(A_SETMARK, "mark: ");
			c = getcc();
			if (c == erase_char || c == kill_char)
				break;
			setmark(c);
			break;
		case A_GOMARK:			/* go to mark */
			lower_left();
			clear_eol();
			start_mca(A_GOMARK, "goto mark: ");
			c = getcc();
			if (c == erase_char || c == kill_char)
				break;
			gomark(c);
			break;
		case A_PREFIX:
			/*
			 * The command is incomplete (more chars are needed).
			 * Display the current char so the user knows what's
			 * going on and get another character.
			 */
			if (mca != A_PREFIX)
				start_mca(A_PREFIX, "");
			if (iscntrl(c)) {
				putchr('^');
				c = CARAT_CHAR(c);
			}
			putchr(c);
			c = getcc();
			goto again;
		default:
			if (print_help)
				message(gettxt(MSG(hint),
					"Type 'h' for help"));
			else
				bell();
			break;
		}
	}
}

static void
editfile()
{
	static int dolinenumber;
	static char *editor;
	int c;
	int len;
	char *cmd;
	char buf[ PATH_MAX * 2 + 32 ];

	if (editor == NULL) {
		editor = getenv("EDITOR");
		/* pass the line number to vi */
		if (editor == NULL || *editor == '\0') {
			editor = _PATH_VI;
			dolinenumber = 1;
		}
		else {
			/* check for vi, or ex editor in $EDITOR */
			len = strlen(editor);
			if (len == 2) {
				dolinenumber = (strcmp(editor, "vi") == 0
						|| strcmp(editor, "ex") == 0);
			}
			else if (len > 2) {
				cmd = editor + len - 3;
				dolinenumber = (strcmp(cmd, "/vi") == 0
						|| strcmp(cmd, "/ex") == 0);
			}
			else
				dolinenumber = 0;
		}
	}

	/* Can't set long values for EDITOR on shells. The size of the buf
	   is safe enough.
	*/
	if (dolinenumber && (c = currline(CURRENT)))
		(void)sprintf(buf, "%s +%d %s", editor, c, current_file);
	else
		(void)sprintf(buf, "%s %s", editor, current_file);
	lsystem(buf);
}

static void
showlist()
{
	register int indx, width;
	int len;
	char *p;

	if (ac <= 0) {
		error(gettxt(MSG(files), "No files provided as arguments."));
		return;
	}
	for (width = indx = 0; indx < ac;) {
		p = strcmp(av[indx], "-") ? av[indx] : "stdin";
		len = strlen(p) + 1;
		if (curr_ac == indx)
			len += 2;
		if (width + len + 1 >= sc_width) {
			if (!width) {
				if (curr_ac == indx)
					putchr('[');
				putstr(p);
				if (curr_ac == indx)
					putchr(']');
				++indx;
			}
			width = 0;
			putchr('\n');
			continue;
		}
		if (width)
			putchr(' ');
		if (curr_ac == indx)
			putchr('[');
		putstr(p);
		if (curr_ac == indx)
			putchr(']');
		width += len;
		++indx;
	}
	putchr('\n');
	message("");
}

static void
shellcmd()
{
	static char	shellbuf[160];
	char		tempbuf[160];
	char		*outp = tempbuf;
	char		*inp = cmdbuf;
	int		left = sizeof(tempbuf) - 1;

	while ((*outp = *inp++) && left) {

		switch (*outp) {
		case '%' :
			if (ispipe) {
				error(gettxt(MSG(files), 
					     "No files provided as arguments"));
					return;
			}
			if ((left -= strlen(current_file)) < 0) {
				error(gettxt(MSG(cmdlength),
					     "Command too long"));
				return;
			}
			strcpy(outp, current_file);
			outp += strlen(current_file);
			break;
		case '!' :
			if (!shellbuf[0]) {
				error(gettxt(MSG(prevcmd),
					     "No previous command"));
				return;
			}
			if ((left -= strlen(shellbuf)) < 0) {
				error(gettxt(MSG(cmdlength),
					     "Command too long"));
				return;
			}
			strcpy(outp, shellbuf);
			outp += strlen(shellbuf);
			break;
		case '//' :
			if (*inp == '%' || *inp == '!')
				*outp++ = *inp++;
			left--;
			break;
		default :
			outp++;
			left--;
			break;
		}
	}

	*outp = '\0';
	strcpy(shellbuf, tempbuf);
	lsystem(shellbuf);
	message("");
}
