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
 *
 *	@(#)less.h	8.1 (Berkeley) 6/6/93
 */

#define	NULL_POSITION	((off64_t)(-1))

#define	EOI		(0)
#define	READ_INTR	(-2)

/* Special chars used to tell put_line() to do something special */
#define	UL_CHAR		'\201'		/* Enter underline mode */
#define	UE_CHAR		'\202'		/* Exit underline mode */
#define	BO_CHAR		'\203'		/* Enter boldface mode */
#define	BE_CHAR		'\204'		/* Exit boldface mode */

#define	CARAT_CHAR(c)		(((c) == '\177') ? '?' : ((c) | 0100))

#define SS2		0x8e
#define SS3		0x8f

#define	TOP			(0)
#define	BOTTOM			(-1)
#define	BOTTOM_PLUS_ONE		(-2)
#define	CURRENT			(2)	/* current line is 3rd line on screen */
#define	CURRENT_PLUS_ONE	(3)

#define	A_INVALID		-1

#define	A_AGAIN_SEARCH		1
#define	A_B_LINE		2
#define	A_B_SCREEN		3
#define	A_B_SCROLL		4
#define	A_B_SEARCH		5
#define	A_DIGIT			6
#define	A_EXAMINE		7
#define	A_FREPAINT		8
#define	A_F_LINE		9
#define	A_F_SCREEN		10
#define	A_F_SCROLL		11
#define	A_F_SEARCH		12
#define	A_GOEND			13
#define	A_GOLINE		14
#define	A_GOMARK		15
#define	A_HELP			16
#define	A_NEXT_FILE		17
#define	A_PERCENT		18
#define	A_PREFIX		19
#define	A_PREV_FILE		20
#define	A_QUIT			21
#define	A_REPAINT		22
#define	A_SETMARK		23
#define	A_STAT			24
#define	A_VISUAL		25
#define	A_TAGFILE		26
#define	A_FILE_LIST		27
#define	A_REVERSE_SEARCH	28
#define	A_SHELL			29
#define	A_REPEAT		30

#define MSG(msg)	_SGI_DMMX_xpg_more_ ## msg

#define CMDBUFLEN	120
#define LIMIT_SC_HEIGHT 1000000


/* Common includes
 */
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <fmtmsg.h>


/* ch */
extern int file;
extern int ch_addbuf(int);
extern off64_t ch_tell(void);
extern int ch_seek(off64_t);
extern int ch_end_seek(void);
extern int ch_beg_seek(void);

extern off64_t ch_length(void);
extern void ch_init(int, int);
extern int ch_forw_get(void);
extern int ch_back_get(void);

/* command */
extern void commands(void);

/* decode */
extern void noprefix(void);
extern int cmd_decode(int);
extern int cmd_search(char *, char *);

/* help */
extern void help(void);

/* input */
extern off64_t forw_line(off64_t);
extern off64_t back_line(off64_t);

/* line */
extern void prewind(void);
extern int pappend(int);
extern off64_t forw_raw_line(off64_t);
extern off64_t back_raw_line(off64_t);

/* linenum */
extern char *line;
extern int currline(int);
extern void add_lnum(int, off64_t);
extern void clr_linenum(void);
extern int find_linenum(off64_t);

/* main */
extern int any_display;
extern int wait_on_error;
extern char *current_file;
extern char **av;
extern int ispipe;
extern int curr_ac;
extern int initcmd_tried;
extern char *current_name;
extern char *next_name;
extern char *cmd_name;
extern char cmd_label[];
extern void quit(void);
extern int edit(register char *,int);
extern void next_file(int);
extern void prev_file(int);
extern int  is_tty;


/* option */
extern int p_option;
extern int make_printable;
extern int fold;
extern int print_help;
extern int massage;
extern int tagoption;
extern int squeeze;
extern int tabstop;
extern int top_scroll;
extern int caseless;
extern int cbufs;
extern int wait_at_eof;
extern int linenums;
extern char *initcmd;
extern int option(int, char **);

/* os */
extern int reading;
extern char *bad_file(char *, char *, u_int);
extern int iread(int, char *, int);
extern char *uxglob(char *);
extern void lsystem(char *);

/* output */
extern int errmsgs;
extern int cmdstack;
extern void putchr(int);
extern void putstr(char *);
extern void put_line(void);
extern void purge(void);
extern void flush(void);
extern void message(char *s);
extern void error(char *);
extern void ierror(char *);

/* position */
extern int overdrawn;
extern off64_t position(int);
extern void pos_clear(void);
extern void add_forw_pos(off64_t);
extern void add_back_pos(off64_t);
extern void copytable(void);
extern int onscreen(off64_t);

/* prim */
extern int back_scroll;
extern int hit_eof;
extern int screen_trashed;
extern void repaint(void);
extern void prepaint(off64_t);
extern int get_back_scroll(void);
extern void jump_loc(off64_t pos);
extern void jump_back(int);
extern void jump_forw(void);
extern int search(int, char *, int, int);
extern void forward(int, int);
extern void backward(int, int);
extern void init_mark(void);
extern void lastmark(void);
extern void setmark(int);
extern void gomark(int);
extern void raw_mode(int);
extern void get_term(void);
extern void jump_percent(int);
extern void eof_check(void);
extern void forw(int, off64_t, int);
extern void back(int, off64_t, int);

/* screen */
extern int ul_width, ue_width;
extern int bo_width, be_width;
extern int so_width, se_width;
extern int auto_wrap, ignaw;
extern int retain_below;
extern int erase_char, kill_char, werase_char;
extern int sc_width, sc_height;
extern int sc_window;
extern int ac;
extern int quitting;
extern int scroll_count;
extern int short_file;
extern void lower_left(void);
extern void clear_eol(void);
extern void clear_scr(void);
extern void bell(void);
extern void so_enter(void);
extern void so_exit(void);
extern void init(void);
extern void deinit(void);
extern void home(void);
extern void ul_enter(void);
extern void ul_exit(void);
extern void bo_enter(void);
extern void bo_exit(void);
extern void putbs(void);
extern void backspace(void);
extern void add_line(void);

/* signal */
extern int sigs;
extern void init_signals(int);
extern void psignals(void);
extern void winchsig();

/* tags */
extern char *tagfile;
extern void findtag(char *);
extern int tagsearch(void);

/* ttyin */
extern int getchr(void);
extern void open_getchr(void);


