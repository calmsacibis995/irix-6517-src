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
 * Routines which deal with the characteristics of the terminal.
 * Uses termcap to be as terminal-independent as possible.
 *
 * {{ Someday this should be rewritten to use curses. }}
 */

#include "less.h"
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#ifdef TIOCGWINSZ
#include <sys/ioctl.h>
#else
/*
 * For the Unix PC (ATT 7300 & 3B1):
 * Since WIOCGETD is defined in sys/window.h, we can't use that to decide
 * whether to include sys/window.h.  Use SIGPHONE from sys/signal.h instead.
 */
#include <sys/signal.h>
#ifdef SIGPHONE
#include <sys/window.h>
#endif
#endif

extern int errno;

#define RETRY_TSET_COUNT 10

/*
 * Strings passed to tputs() to do various terminal functions.
 */
static char
	*sc_pad,		/* Pad string */
	*sc_home,		/* Cursor home */
	*sc_addline,		/* Add line, scroll down following lines */
	*sc_lower_left,		/* Cursor to last line, first column */
	*sc_move,		/* General cursor positioning */
	*sc_clear,		/* Clear screen */
	*sc_eol_clear,		/* Clear to end of line */
	*sc_s_in,		/* Enter standout (highlighted) mode */
	*sc_s_out,		/* Exit standout mode */
	*sc_u_in,		/* Enter underline mode */
	*sc_u_out,		/* Exit underline mode */
	*sc_b_in,		/* Enter bold mode */
	*sc_b_out,		/* Exit bold mode */
	*sc_backspace,		/* Backspace cursor */
	*sc_init,		/* Startup terminal initialization */
	*sc_deinit;		/* Exit terminal de-intialization */

int auto_wrap;			/* Terminal does \r\n when write past margin */
int ignaw;			/* Terminal ignores \n immediately after wrap */
				/* The user's erase and line-kill chars */
int retain_below;		/* Terminal retains text below the screen */
int erase_char, kill_char, werase_char;
int sc_width = -1;		/* Width of screen */
int sc_height = -1;		/* Height of screen */
int sc_window = -1;		/* window size for forward and backward */
int bo_width, be_width;		/* Printing width of boldface sequences */
int ul_width, ue_width;		/* Printing width of underline sequences */
int so_width, se_width;		/* Printing width of standout sequences */

/*
 * These two variables are sometimes defined in,
 * and needed by, the termcap library.
 */
extern short ospeed;	/* Terminal output baud rate */
extern char PC;		/* Pad character */

extern char *tgetstr();
extern char *tgoto();

/*
 * Change terminal to "raw mode", or restore to "normal" mode.
 * "Raw mode" means 
 *	1. An outstanding read will complete on receipt of a single keystroke.
 *	2. Input is not echoed.  
 *	3. On output, \n is mapped to \r\n.
 *	4. \t is NOT expanded into spaces.
 *	5. Signal-causing characters such as ctrl-C (interrupt),
 *	   etc. are NOT disabled.
 * It doesn't matter whether an input \n is mapped to \r, or vice versa.
 */
void
raw_mode(int on)
{
	struct termios s;
 static struct termios save_term;
	struct termios st;
	int    retry_tset;

	memset( (void *)&s,  0, sizeof(struct termios) );
	memset( (void *)&st, 0, sizeof(struct termios) );

        /* setting count for retrying tcsetattr() */
        retry_tset = RETRY_TSET_COUNT;

	if (on) {

		/*
		 * Get terminal modes.
		 */
		for ( ; tcgetattr(2, &s) < 0  &&  0 < retry_tset ; retry_tset -- ) { 

#ifdef _TCSET_DEBUG_ 
			fprintf( stderr, "ERROR(1): tcgetattr( %s ), try again (%d times)...\n", strerror(errno), retry_tset );
#endif
			;
		}

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		save_term   = s;
		ospeed      = cfgetospeed(&s);
		erase_char  = s.c_cc[VERASE];
		kill_char   = s.c_cc[VKILL];
		werase_char = s.c_cc[VWERASE]; 
		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag    &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
		s.c_oflag    |=  (OPOST|ONLCR|TAB3);
		s.c_cc[VMIN]  = 1;
		s.c_cc[VTIME] = 0;
	} 
	else {
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}


retry_tset:

	/* 
	 * Set terminal modes. 
	 */

	if ( tcsetattr(2, TCSADRAIN, &s) < 0 ) {

#ifdef _TCSET_DEBUG_
                fprintf( stderr, "ERROR(0): tcsetattr( %s ), try again (%d times)...\n", strerror(errno), retry_tset );
#endif
		if ( 0 < retry_tset ) {
			 retry_tset --;
			 goto retry_tset;
		}
	}
	else {  /* even tcsetattr returns positive value, we have to check all of settings */

		/* get tc attribute into st (status) */
	        for ( ; tcgetattr(2, &st) < 0  &&  0 < retry_tset ; retry_tset -- ) {

#ifdef _TCSET_DEBUG_
                        fprintf( stderr, "ERROR(2): tcgetattr( %s ), try again (%d times)...\n", strerror(errno), retry_tset );
#endif
                        ;
		}

		if ( on )
		{
			if ( ( st.c_lflag & ICANON || st.c_lflag & ECHO  || st.c_lflag & ECHOE  ||
		     	       st.c_lflag & ECHOK  || st.c_lflag & ECHONL )                     ||
			    !( st.c_oflag & OPOST  && st.c_oflag & ONLCR && st.c_oflag & TAB3 ) || 
			       st.c_cc[VMIN] != 1  || st.c_cc[VTIME] != 0 )
			{
#ifdef _TCSET_DEBUG_
	                        fprintf( stderr, "INVALID: tcsetattr(), try again (%d times)...\n", retry_tset );
				fprintf( stderr, " - st.c_lflag & ICANON   : %x (0)\n", st.c_lflag & ICANON );
				fprintf( stderr, " - st.c_lflag & ECHO     : %x (0)\n", st.c_lflag & ECHO   );
				fprintf( stderr, " - st.c_lflag & ECHOE    : %x (0)\n", st.c_lflag & ECHOE  );
				fprintf( stderr, " - st.c_lflag & ECHOK    : %x (0)\n", st.c_lflag & ECHOK  );
				fprintf( stderr, " - st.c_oflag & OPOST    : %x (1)\n", st.c_oflag & OPOST  );
				fprintf( stderr, " - st.c_oflag & ONLCR    : %x (1)\n", st.c_oflag & ONLCR  );
				fprintf( stderr, " - st.c_oflag & TAB3     : %x (1)\n", st.c_oflag & TAB3   );
				fprintf( stderr, " - st.c_cc[VMIN]         : %x (1)\n", st.c_cc[VMIN]       );
				fprintf( stderr, " - st.c_cc[VTIME]        : %x (0)\n", st.c_cc[VTIME]      );
#endif
				if ( 0 < retry_tset ) {
					 retry_tset --;
                			 s.c_lflag    &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
                			 s.c_oflag    |=  (OPOST|ONLCR|TAB3);
                			 s.c_cc[VMIN]  = 1;
                			 s.c_cc[VTIME] = 0;
					 goto retry_tset;
				}
			}
		} 
		/* good enough for another case */
	}


#ifdef _TCSET_DEBUG_M_

	tcgetattr(2, &st);

        fprintf( stderr, "---------------------------------------------------\n" );
	fprintf( stderr, " - st.c_iflag & IGNBRK  : %x\n", st.c_iflag & IGNBRK     );
        fprintf( stderr, " - st.c_iflag & BRKINT  : %x\n", st.c_iflag & BRKINT     );
        fprintf( stderr, " - st.c_iflag & IGNPAR  : %x\n", st.c_iflag & IGNPAR     );
        fprintf( stderr, " - st.c_iflag & PARMRK  : %x\n", st.c_iflag & PARMRK     );
        fprintf( stderr, " - st.c_iflag & INPCK   : %x\n", st.c_iflag & INPCK      );
        fprintf( stderr, " - st.c_iflag & ISTRIP  : %x\n", st.c_iflag & ISTRIP     );
        fprintf( stderr, " - st.c_iflag & INLCR   : %x\n", st.c_iflag & INLCR      );
        fprintf( stderr, " - st.c_iflag & IGNCR   : %x\n", st.c_iflag & IGNCR      );
        fprintf( stderr, " - st.c_iflag & ICRNL   : %x\n", st.c_iflag & ICRNL      );
        fprintf( stderr, " - st.c_iflag & IUCLC   : %x\n", st.c_iflag & IUCLC      );
        fprintf( stderr, " - st.c_iflag & IXON    : %x\n", st.c_iflag & IXON       );
        fprintf( stderr, " - st.c_iflag & IXANY   : %x\n", st.c_iflag & IXANY      );
        fprintf( stderr, " - st.c_iflag & IXOFF   : %x\n", st.c_iflag & IXOFF      );
        fprintf( stderr, " - st.c_iflag & IMAXBEL : %x\n", st.c_iflag & IMAXBEL    );
        fprintf( stderr, " - st.c_iflag & IBLKMD  : %x\n", st.c_iflag & IBLKMD     );
        fprintf( stderr, " - st.c_oflag & OPOST   : %x\n", st.c_oflag & OPOST      );
        fprintf( stderr, " - st.c_oflag & OLCUC   : %x\n", st.c_oflag & OLCUC      );
        fprintf( stderr, " - st.c_oflag & ONLCR   : %x\n", st.c_oflag & ONLCR      );
        fprintf( stderr, " - st.c_oflag & OCRNL   : %x\n", st.c_oflag & OCRNL      );
        fprintf( stderr, " - st.c_oflag & ONOCR   : %x\n", st.c_oflag & ONOCR      );
        fprintf( stderr, " - st.c_oflag & ONLRET  : %x\n", st.c_oflag & ONLRET     );
        fprintf( stderr, " - st.c_oflag & OFILL   : %x\n", st.c_oflag & OFILL      );
        fprintf( stderr, " - st.c_oflag & OFDEL   : %x\n", st.c_oflag & OFDEL      );
        fprintf( stderr, " - st.c_oflag & NLDLY   : %x\n", st.c_oflag & NLDLY      );
        fprintf( stderr, " - st.c_oflag & NL0     : %x\n", st.c_oflag & NL0        );
        fprintf( stderr, " - st.c_oflag & NL1     : %x\n", st.c_oflag & NL1        );
        fprintf( stderr, " - st.c_oflag & CRDLY   : %x\n", st.c_oflag & CRDLY      );
        fprintf( stderr, " - st.c_oflag & CR0     : %x\n", st.c_oflag & CR0        );
        fprintf( stderr, " - st.c_oflag & CR1     : %x\n", st.c_oflag & CR1        );
        fprintf( stderr, " - st.c_oflag & CR2     : %x\n", st.c_oflag & CR2        );
        fprintf( stderr, " - st.c_oflag & CR3     : %x\n", st.c_oflag & CR3        );
        fprintf( stderr, " - st.c_oflag & TABDLY  : %x\n", st.c_oflag & TABDLY     );
        fprintf( stderr, " - st.c_oflag & TAB0    : %x\n", st.c_oflag & TAB0       );
        fprintf( stderr, " - st.c_oflag & TAB1    : %x\n", st.c_oflag & TAB1       );
        fprintf( stderr, " - st.c_oflag & TAB2    : %x\n", st.c_oflag & TAB2       );
        fprintf( stderr, " - st.c_oflag & TAB3    : %x\n", st.c_oflag & TAB3       );
        fprintf( stderr, " - st.c_oflag & XTABS   : %x\n", st.c_oflag & XTABS      );
        fprintf( stderr, " - st.c_oflag & BSDLY   : %x\n", st.c_oflag & BSDLY      );
        fprintf( stderr, " - st.c_oflag & BS0     : %x\n", st.c_oflag & BS0        );
        fprintf( stderr, " - st.c_oflag & BS1     : %x\n", st.c_oflag & BS1        );
        fprintf( stderr, " - st.c_oflag & VTDLY   : %x\n", st.c_oflag & VTDLY      );
        fprintf( stderr, " - st.c_oflag & VT0     : %x\n", st.c_oflag & VT0        );
        fprintf( stderr, " - st.c_oflag & VT1     : %x\n", st.c_oflag & VT1        );
        fprintf( stderr, " - st.c_oflag & FFDLY   : %x\n", st.c_oflag & FFDLY      );
        fprintf( stderr, " - st.c_oflag & FF0     : %x\n", st.c_oflag & FF0        );
        fprintf( stderr, " - st.c_oflag & FF1     : %x\n", st.c_oflag & FF1        );
        fprintf( stderr, " - st.c_oflag & PAGEOUT : %x\n", st.c_oflag & PAGEOUT    );
        fprintf( stderr, " - st.c_oflag & WRAP    : %x\n", st.c_oflag & WRAP       );
        fprintf( stderr, " - st.c_cflag & CBAUD   : %x\n", st.c_cflag & CBAUD      );
        fprintf( stderr, " - st.c_cflag & CSIZE   : %x\n", st.c_cflag & CSIZE      );
        fprintf( stderr, " - st.c_cflag & CS5     : %x\n", st.c_cflag & CS5        );
        fprintf( stderr, " - st.c_cflag & CS6     : %x\n", st.c_cflag & CS6        );
        fprintf( stderr, " - st.c_cflag & CS7     : %x\n", st.c_cflag & CS7        );
        fprintf( stderr, " - st.c_cflag & CS8     : %x\n", st.c_cflag & CS8        );
        fprintf( stderr, " - st.c_cflag & CSTOPB  : %x\n", st.c_cflag & CSTOPB     );
        fprintf( stderr, " - st.c_cflag & CREAD   : %x\n", st.c_cflag & CREAD      );
        fprintf( stderr, " - st.c_cflag & PARENB  : %x\n", st.c_cflag & PARENB     );
        fprintf( stderr, " - st.c_cflag & PARODD  : %x\n", st.c_cflag & PARODD     );
        fprintf( stderr, " - st.c_cflag & HUPCL   : %x\n", st.c_cflag & HUPCL      );
        fprintf( stderr, " - st.c_cflag & CLOCAL  : %x\n", st.c_cflag & CLOCAL     );
        fprintf( stderr, " - st.c_cflag & RCV1EN  : %x\n", st.c_cflag & RCV1EN     );
        fprintf( stderr, " - st.c_cflag & XMT1EN  : %x\n", st.c_cflag & XMT1EN     );
        fprintf( stderr, " - st.c_cflag & LOBLK   : %x\n", st.c_cflag & LOBLK      );
        fprintf( stderr, " - st.c_cflag & XCLUDE  : %x\n", st.c_cflag & XCLUDE     );
        fprintf( stderr, " - st.c_cflag & CIBAUD  : %x\n", st.c_cflag & CIBAUD     );
        fprintf( stderr, " - st.c_cflag & PAREXT  : %x\n", st.c_cflag & PAREXT     );
        fprintf( stderr, " - st.c_cflag&NEW_RTSCTS: %x\n", st.c_cflag & CNEW_RTSCTS);
        fprintf( stderr, " - st.c_lflag & ISIG    : %x\n", st.c_lflag & ISIG       );
        fprintf( stderr, " - st.c_lflag & ICANON  : %x\n", st.c_lflag & ICANON     );
        fprintf( stderr, " - st.c_lflag & XCASE   : %x\n", st.c_lflag & XCASE      );
        fprintf( stderr, " - st.c_lflag & ECHO    : %x\n", st.c_lflag & ECHO       );
        fprintf( stderr, " - st.c_lflag & ECHOE   : %x\n", st.c_lflag & ECHOE      );
        fprintf( stderr, " - st.c_lflag & ECHOK   : %x\n", st.c_lflag & ECHOK      );
        fprintf( stderr, " - st.c_lflag & ECHONL  : %x\n", st.c_lflag & ECHONL     );
        fprintf( stderr, " - st.c_lflag & NOFLSH  : %x\n", st.c_lflag & NOFLSH     );
        fprintf( stderr, " - st.c_lflag & IEXTEN  : %x\n", st.c_lflag & IEXTEN     );
        fprintf( stderr, " - st.c_lflag & ITOSTOP : %x\n", st.c_lflag & ITOSTOP    );
        fprintf( stderr, " - st.c_lflag & TOSTOP  : %x\n", st.c_lflag & TOSTOP     );
        fprintf( stderr, " - st.c_lflag & ECHOCTL : %x\n", st.c_lflag & ECHOCTL    );
        fprintf( stderr, " - st.c_lflag & ECHOPRT : %x\n", st.c_lflag & ECHOPRT    );
        fprintf( stderr, " - st.c_lflag & ECHOKE  : %x\n", st.c_lflag & ECHOKE     );
        fprintf( stderr, " - st.c_lflag & DEFECHO : %x\n", st.c_lflag & DEFECHO    );
        fprintf( stderr, " - st.c_lflag & FLUSHO  : %x\n", st.c_lflag & FLUSHO     );
        fprintf( stderr, " - st.c_lflag & PENDIN  : %x\n", st.c_lflag & PENDIN     );
	fprintf( stderr, " - st.c_ospeed (Decimal): %d\n", st.c_ospeed             );
        fprintf( stderr, " - st.c_ispeed (Decimal): %d\n", st.c_ispeed             );
        fprintf( stderr, " - st.c_cc[    VINTR   ]: %d\n", st.c_cc[      0       ] );
        fprintf( stderr, " - st.c_cc[    VQUIT   ]: %d\n", st.c_cc[      1       ] );
        fprintf( stderr, " - st.c_cc[    VERASE  ]: %d\n", st.c_cc[      2       ] );
        fprintf( stderr, " - st.c_cc[    VKILL   ]: %d\n", st.c_cc[      3       ] );
        fprintf( stderr, " - st.c_cc[    VEOF    ]: %d\n", st.c_cc[      4       ] );
        fprintf( stderr, " - st.c_cc[    VEOL    ]: %d\n", st.c_cc[      5       ] );
        fprintf( stderr, " - st.c_cc[    VEOL2   ]: %d\n", st.c_cc[      6       ] );
        fprintf( stderr, " - st.c_cc[    VMIN    ]: %d\n", st.c_cc[      4       ] );
        fprintf( stderr, " - st.c_cc[    VTIME   ]: %d\n", st.c_cc[      5       ] );
        fprintf( stderr, " - st.c_cc[    VSWTCH  ]: %d\n", st.c_cc[      7       ] );
        fprintf( stderr, " - st.c_cc[    VSTART  ]: %d\n", st.c_cc[      8       ] );
        fprintf( stderr, " - st.c_cc[    VSTOP   ]: %d\n", st.c_cc[      9       ] );
        fprintf( stderr, " - st.c_cc[    VSUSP   ]: %d\n", st.c_cc[      10      ] );
        fprintf( stderr, " - st.c_cc[    VDSUSP  ]: %d\n", st.c_cc[      11      ] );
        fprintf( stderr, " - st.c_cc[    VREPRINT]: %d\n", st.c_cc[      12      ] );
        fprintf( stderr, " - st.c_cc[    VDISCARD]: %d\n", st.c_cc[      13      ] );
        fprintf( stderr, " - st.c_cc[    VWERASE ]: %d\n", st.c_cc[      14      ] );
        fprintf( stderr, " - st.c_cc[    VLNEXT  ]: %d\n", st.c_cc[      15      ] );
#endif

	noprefix();
	if (cmd_decode(kill_char))
		kill_char =  '\0';
	noprefix();
	if (cmd_decode(werase_char))
		werase_char = '\0';
}

/*
 * Get terminal capabilities via termcap.
 */
void
get_term()
{
	char termbuf[ 4096 ];
	char *sp;
	char *term;
	int hard;
#ifdef TIOCGWINSZ
	struct winsize w;
#else
#ifdef WIOCGETD
	struct uwdata w;
#endif
#endif
	static char sbuf[ 2048 ];

	/*
	 * Find out what kind of terminal this is.
	 */
 	if ((term = getenv("TERM")) == NULL)
 		term = "unknown";
 	if (tgetent(termbuf, term) <= 0)
 		(void)strcpy(termbuf, "dumb:co#80:hc:");

	/*
	 * Get size of the screen.
	 */
	if (sc_height < 0) {
#ifdef TIOCGWINSZ
		if (ioctl(2, TIOCGWINSZ, &w) == 0 && w.ws_row > 0)
			sc_height = w.ws_row;
		else
#else
#ifdef WIOCGETD
		if (ioctl(2, WIOCGETD, &w) == 0 && w.uw_height > 0)
			sc_height = w.uw_height/w.uw_vs;
		else
#endif
#endif
		sc_height = tgetnum("li");

		if (sc_height < 0)
			sc_height = 24;
	}
	hard = tgetflag("hc");

	if (sc_width < 0) {
#ifdef TIOCGWINSZ
		if (ioctl(2, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
			sc_width = w.ws_col;
		else
#ifdef WIOCGETD
		if (ioctl(2, WIOCGETD, &w) == 0 && w.uw_width > 0)
			sc_width = w.uw_width/w.uw_hs;
		else
#endif
#endif
			sc_width = tgetnum("co");

		if (sc_width < 0)
			sc_width = 80;

		ignaw = tgetflag("xn");
		auto_wrap = tgetflag("am");

	} else {
		/* value already set so assume no autowrap */
		ignaw = 1;
		auto_wrap = 0;
	}

	retain_below = tgetflag("db");


	/*
	 * Assumes termcap variable "sg" is the printing width of
	 * the standout sequence, the end standout sequence,
	 * the underline sequence, the end underline sequence,
	 * the boldface sequence, and the end boldface sequence.
	 */
	if ((so_width = tgetnum("sg")) < 0)
		so_width = 0;
	be_width = bo_width = ue_width = ul_width = se_width = so_width;

	/*
	 * Get various string-valued capabilities.
	 */
	sp = sbuf;

	sc_pad = tgetstr("pc", &sp);
	if (sc_pad != NULL)
		PC = *sc_pad;

	sc_init = tgetstr("ti", &sp);
	if (sc_init == NULL)
		sc_init = "";

	sc_deinit= tgetstr("te", &sp);
	if (sc_deinit == NULL)
		sc_deinit = "";

	sc_eol_clear = tgetstr("ce", &sp);
	if (hard || sc_eol_clear == NULL || *sc_eol_clear == '\0')
	{
		sc_eol_clear = "";
	}

	sc_clear = tgetstr("cl", &sp);
	if (hard || sc_clear == NULL || *sc_clear == '\0')
	{
		sc_clear = "\n\n";
	}

	sc_move = tgetstr("cm", &sp);
	if (hard || sc_move == NULL || *sc_move == '\0')
	{
		/*
		 * This is not an error here, because we don't 
		 * always need sc_move.
		 * We need it only if we don't have home or lower-left.
		 */
		sc_move = "";
	}

	sc_s_in = tgetstr("so", &sp);
	if (hard || sc_s_in == NULL)
		sc_s_in = "";

	sc_s_out = tgetstr("se", &sp);
	if (hard || sc_s_out == NULL)
		sc_s_out = "";

	sc_u_in = tgetstr("us", &sp);
	if (hard || sc_u_in == NULL)
		sc_u_in = sc_s_in;

	sc_u_out = tgetstr("ue", &sp);
	if (hard || sc_u_out == NULL)
		sc_u_out = sc_s_out;

	sc_b_in = tgetstr("md", &sp);
	if (hard || sc_b_in == NULL)
	{
		sc_b_in = sc_s_in;
		sc_b_out = sc_s_out;
	} else
	{
		sc_b_out = tgetstr("me", &sp);
		if (hard || sc_b_out == NULL)
			sc_b_out = "";
	}

	sc_home = tgetstr("ho", &sp);
	if (hard || sc_home == NULL || *sc_home == '\0')
	{
		if (*sc_move == '\0')
		{
			/*
			 * This last resort for sc_home is supposed to
			 * be an up-arrow suggesting moving to the 
			 * top of the "virtual screen". (The one in
			 * your imagination as you try to use this on
			 * a hard copy terminal.)
			 */
			sc_home = "|\b^";
		} else
		{
			/* 
			 * No "home" string,
			 * but we can use "move(0,0)".
			 */
			(void)strcpy(sp, tgoto(sc_move, 0, 0));
			sc_home = sp;
			sp += strlen(sp) + 1;
		}
	}

	sc_lower_left = tgetstr("ll", &sp);
	if (hard || sc_lower_left == NULL || *sc_lower_left == '\0')
	{
		/* set sc_lower_left to "\r" if not available vs moving
		 * all over the screen.
		 */
		sc_lower_left = "\r";
		/*
		if (*sc_move == '\0')
		{
			sc_lower_left = "\r";
		} else
		{
		*/
			/*
			 * No "lower-left" string, 
			 * but we can use "move(0,last-line)".
			 */
		/*
			(void)strcpy(sp, tgoto(sc_move, 0, sc_height-1));
			sc_lower_left = sp;
			sp += strlen(sp) + 1;
		}
		*/
	}

	/* Disable back_scroll and force repaint for any backward
	 * movement to make sure that prompt is always at bottom left
	 * vs adding a line at top of screen then scroll the display down.
	 */
	back_scroll = 0;

	/*
	 * To add a line at top of screen and scroll the display down,
	 * we use "al" (add line) or "sr" (scroll reverse).
	 */
	/*
	if ((sc_addline = tgetstr("al", &sp)) == NULL || 
		 *sc_addline == '\0')
		sc_addline = tgetstr("sr", &sp);

	if (hard || sc_addline == NULL || *sc_addline == '\0')
	{
		sc_addline = "";
	*/
		/* Force repaint on any backward movement */
	/*
		back_scroll = 0;
	}
	*/

	if (tgetflag("bs"))
		sc_backspace = "\b";
	else
	{
		sc_backspace = tgetstr("bc", &sp);
		if (sc_backspace == NULL || *sc_backspace == '\0')
			sc_backspace = "\b";
	}
}


/*
 * Below are the functions which perform all the 
 * terminal-specific screen manipulation.
 */

/*
 * Initialize terminal
 */
void
init()
{
	tputs(sc_init, sc_height, putchr);
}

/*
 * Deinitialize terminal
 */
void
deinit()
{
	tputs(sc_deinit, sc_height, putchr);
}

/*
 * Home cursor (move to upper left corner of screen).
 */
void
home()
{
	tputs(sc_home, 1, putchr);
}

/*
 * Add a blank line (called with cursor at home).
 * Should scroll the display down.
 */
void
add_line()
{
	tputs(sc_addline, sc_height, putchr);
}

int short_file;				/* if file less than a screen */

void
lower_left()
{
	if (short_file) {
		putchr('\r');
		flush();
	}
	else
		tputs(sc_lower_left, 1, putchr);
}

/*
 * Ring the terminal bell.
 */
void
bell()
{
	putchr('\7');
}

/*
 * Clear the screen.
 */
void
clear_scr()
{
	tputs(sc_clear, sc_height, putchr);
}

/*
 * Clear from the cursor to the end of the cursor's line.
 * {{ This must not move the cursor. }}
 */
void
clear_eol()
{
	tputs(sc_eol_clear, 1, putchr);
}

/*
 * Begin "standout" (bold, underline, or whatever).
 */
void
so_enter()
{
	tputs(sc_s_in, 1, putchr);
}

/*
 * End "standout".
 */
void
so_exit()
{
	tputs(sc_s_out, 1, putchr);
}

/*
 * Begin "underline" (hopefully real underlining, 
 * otherwise whatever the terminal provides).
 */
void
ul_enter()
{
	tputs(sc_u_in, 1, putchr);
}

/*
 * End "underline".
 */
void
ul_exit()
{
	tputs(sc_u_out, 1, putchr);
}

/*
 * Begin "bold"
 */
void
bo_enter()
{
	tputs(sc_b_in, 1, putchr);
}

/*
 * End "bold".
 */
void
bo_exit()
{
	tputs(sc_b_out, 1, putchr);
}

/*
 * Erase the character to the left of the cursor 
 * and move the cursor left.
 */
void
backspace()
{
	/* 
	 * Try to erase the previous character by overstriking with a space.
	 */
	tputs(sc_backspace, 1, putchr);
	putchr(' ');
	tputs(sc_backspace, 1, putchr);
}

/*
 * Output a plain backspace, without erasing the previous char.
 */
void
putbs()
{
	tputs(sc_backspace, 1, putchr);
}
