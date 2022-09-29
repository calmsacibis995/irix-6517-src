/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
 *	Regents of the University of California.  All rights reserved.
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
 * Entry point, initialization, miscellaneous routines.
 */

#include "less.h"
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <locale.h>
#include <limits.h>

char	cmd_label[16] = "UX:????";
char	*cmd_name;

int	ispipe;
int	is_tty;
char	*current_file, *previous_file, *current_name, *next_name;
off64_t	prev_pos;
int	any_display;		/* have done output to tty */
int	wait_on_error = 1;	/* pause after error messages */
int	initcmd_tried;		/* flag for using the initial command */
int	scroll_count;		/* number of lines to scroll */
int	ac;
char	**av;
int	curr_ac;
int	quitting;

static char *save(char *);
static void cat_file( int );


/*
 * Edit a new file.
 * Filename "-" means standard input.
 * No filename means the "current" file, from the command line.
 */
int
edit(register char *filename, int pos_ac)
{
	register int f;
	register char *m;
	off64_t initial_pos;
	static int didpipe;
	char message[ PATH_MAX + 128 ], *p;

	if(pos_ac == -1)
		pos_ac = curr_ac;
	initial_pos = NULL_POSITION;
	if (filename == NULL || *filename == '\0') {
		if (pos_ac >= ac) {
			error(gettxt(MSG(curfile), "No current file"));
			return(0);
		}
		filename = save(av[pos_ac]);
	}
	else if (strcmp(filename, "#") == 0) {
		if (previous_file == NULL || *previous_file == '\0') {
			error(gettxt(MSG(prevfile), "No previous file"));
			return(0);
		}
		filename = save(previous_file);
		initial_pos = prev_pos;
	} else
		filename = save(filename);

	/* use standard input. */
	if (!strcmp(filename, "-")) {
		if (didpipe) {
			error(gettxt(MSG(ttyview),
				     "Standard input may only be viewed once"));
			return(0);
		}
		f = 0;
	}
	else if ((m = bad_file(filename, message, sizeof(message))) != NULL) {
		error(m);
		free(filename);
		return(0);
        } else if (strcmp(message, "S_IFIFO") == 0) {
                raw_mode(0);
                if ((f = open(filename, O_RDONLY, 0)) < 0) {
                        raw_mode(1);
                        (void)sprintf(message, "%s: %s",
                                filename, strerror(errno));
                        error(message);
                        free(filename);
                        return(0);
                }
                raw_mode(1);
	} else if ((f = open(filename, O_RDONLY, 0)) < 0) {
		(void)sprintf(message, "%s: %s", filename, strerror(errno));
		error(message);
		free(filename);
		return(0);
	}

	if (isatty(f)) {
		/*
		 * Not really necessary to call this an error,
		 * but if the control terminal (for commands)
		 * and the input file (for data) are the same,
		 * we get weird results at best.
		 */
		error(gettxt(MSG(ttyin), "Cannot take input from a terminal"));
		if (f > 0)
			(void)close(f);
		(void)free(filename);
		return(0);
	}

	/*
	 * We are now committed to using the new file.
	 * Close the current input file and set up to use the new one.
	 */
	if (file > 0)
		(void)close(file);
	if (previous_file != NULL)
		free(previous_file);
	previous_file = current_file;
	current_file = filename;
	pos_clear();
	prev_pos = position(TOP);
	ispipe = (f == 0);
	if (ispipe) {
		didpipe = 1;
		current_name = "stdin";
	} else
		current_name = (p = strrchr(filename, '/')) ? p + 1 : filename;
	if ((pos_ac+1) >= ac)
		next_name = NULL;
	else
		next_name = av[pos_ac + 1];
	file = f;
	ch_init(cbufs, 0);
	init_mark();
	initcmd_tried = 0;	/* retry any initial commands */

	if (is_tty) {
		int no_display = !any_display;
		any_display = 1;
		if (no_display && errmsgs > 0) {
			/*
			 * We displayed some messages on error output
			 * (file descriptor 2; see error() function).
			 * Before erasing the screen contents,
			 * display the file name and wait for a keystroke.
			 */
			error(filename);
		}
		/*
		 * Indicate there is nothing displayed yet.
		 */
		if (initial_pos != NULL_POSITION)
			jump_loc(initial_pos);
		clr_linenum();
	}
	return(1);
}

/*
 * Edit the next file in the command line list.
 */
void
next_file(int n)
{
	int new_ac;

	/* Beginning at the nth file try for an editable file.
	 */
	wait_on_error--;
	for (new_ac = curr_ac + n; new_ac < ac; new_ac++) {

		if (edit(av[new_ac],new_ac)) {
			curr_ac = new_ac;
			wait_on_error++;
			return;
		}
	}
	wait_on_error++;

	/* No editable files.
	 */
	if (wait_at_eof || position(TOP) == NULL_POSITION)
		quit();
	error(gettxt(MSG(nextnfile), "No (N-th) next file"));
}

/*
 * Edit the previous file in the command line list.
 */
void
prev_file(int n)
{
	int new_ac;

	/* Beginning at the nth previous file try for an editable file.
	 */
	wait_on_error--;
	for (new_ac = curr_ac - n; new_ac >= 0; new_ac--) {

		if (edit(av[new_ac],new_ac)) {
			wait_on_error++;
			curr_ac = new_ac;
			return;
		}
	}
	wait_on_error++;
	error(gettxt(MSG(prevnfile), "No (N-th) previous file"));
}


/*
 * For non-tty processing. Parsing initcmd and get G command portion.
 * This function doesn't handle init commands from -p option, but does
 * only +#####... option. (-#####... and +/xxxxx... doesn't make sense
 * to this function.) Should be called after option().
 */
static int
initline( char *initcmd )
{
  int i;
  int pos_G = 0;
  int pos_digit = 0;
  char digit[ 32 ];

  if ( initcmd == NULL ) return 0;

  for ( i=strlen(initcmd)-1; 0<=i; i-- ) {
	if ( pos_G && !isdigit(initcmd[i]) ) {
		pos_digit = i + 1;
		break; 
	}
	if ( initcmd[i] == 'G' ) pos_G = i; 
  }
  if ( pos_G == 0 || pos_G <= pos_digit ||
	!((pos_G - pos_digit) < sizeof(digit)) ) return 0;

  strncpy( digit, initcmd + pos_digit, pos_G - pos_digit );
  digit[ pos_G - pos_digit ] = '\0';

  return atoi(digit);
}

/*
 * copy a file directly to standard output; used if stdout is not a tty.
 * the only processing is to squeeze multiple blank input lines.
 */
static void
cat_file( int skplines )
{
	register int c, empty;
	register int numline = 1;

	/* Handle only positive value because non-tty 
	 * processing doesn't handle -p option.
	 */
	skplines = (skplines-2)>0 ? skplines-2 : 0;

	if (squeeze) {
		empty = 0;
		while ((c = ch_forw_get()) != EOI)
			if (c != '\n') {
				putchr(c);
				empty = 0;
			}
			else if (empty < 2) {
				putchr(c);
				++empty;
			}
	}
	else while ((c = ch_forw_get()) != EOI)
	     {
		if ( ((c == '\n')? numline++ : numline) < skplines )
			continue;
		putchr(c);
	     }

	flush();
}

main(int argc, char **argv)
{
	int envargc, argcnt, sz;
	int mode;
	char *env_var;
	char **envargv;
	int  skplines = 0;

	cmd_name = argv[0];

	/* I18N
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)strcpy(cmd_label + 3, "more");
	(void)setlabel(cmd_label);

	/* Process environment variables.
	 */
	if ((env_var = getenv("LINES"))) {
		sc_height = atoi(env_var);	/* stays < 0 on failure */
		if (sc_height > LIMIT_SC_HEIGHT)
			sc_height = LIMIT_SC_HEIGHT; 
	}
	if ((env_var = getenv("COLUMNS")))
		sc_width = atoi(env_var);	/* stays < 0 on failure */

	if (env_var = getenv("MORE")) {

		/* Convert MORE string to argument vector.
		 */
		sz = 5;
		if ((envargv = malloc((sz + 1) * sizeof(char *))) == NULL) {
			error(gettxt(MSG(memory), "Cannot allocate memory"));
			exit(1);
		}
		envargc = 0;
		envargv[envargc++] = "more";
		envargv[envargc++] = strtok(env_var, " \t");
		while (envargv[envargc++] = strtok(NULL, " \t")) {
			if (sz != envargc)
				continue;
			sz += 5;
			if ((envargv = realloc(envargv,
					(sz + 1) * sizeof(char *)) ) == NULL) {
				error(gettxt(MSG(memory),
					     "Cannot allocate memory"));
				exit(1);
			}
		}
		envargc--;
		(void)option(envargc, envargv);
		free(envargv);
	}

	/* Command line arguments override environment variables.
	 */
	argcnt = option(argc, argv);
	argv += argcnt;
	argc -= argcnt;

	/*
	 * Set up list of files to be examined.
	 */
	ac = argc;
	av = argv;
	curr_ac = 0;

	/*
	 * Set up terminal, etc.
	 */
	is_tty = isatty(1);
	if (!is_tty) {
		/*
		 * Output is not a tty.
		 * Just copy the input file(s) to output.
		 */

		skplines = (p_option)? 0 : initline( initcmd );

		if (ac < 1) {
			(void)edit("-",-1);
			cat_file( skplines );
		} else {
			do {
				(void)edit((char *)NULL,curr_ac);
				if (file >= 0)
					cat_file( skplines );
			} while (++curr_ac < ac);
		}
		exit(errmsgs != 0);
	}

	/* If we cannot read stderr (our user input stream) then
	 * we quit (as per XPG4).
	 */
	if ((mode = fcntl(2, F_GETFL)) < 0 || (mode & O_ACCMODE) == O_WRONLY) {
		error(gettxt(MSG(stderr), "Cannot read user commands"));
		exit(errmsgs != 0);
	}

	raw_mode(1);
	get_term();
	open_getchr();
	init();
	init_signals(1);

	/* select the first file to examine. */
	if (tagoption) {
		/*
		 * A -t option was given; edit the file selected by the
		 * "tags" search, and search for the proper line in the file.
		 */
		if (!tagfile || !edit(tagfile,-1) || tagsearch())
			quit();
	}
	else if (ac < 1)
		(void)edit("-",-1);	/* Standard input */
	else {
		/*
		 * Try all the files named as command arguments.
		 * We are simply looking for one which can be
		 * opened without error.
		 */
		do {
			(void)edit((char *)NULL,curr_ac);
		} while (file < 0 && ++curr_ac < ac);
	}

	if (file >= 0)
		commands();
	quit();
	/*NOTREACHED*/
}

/*
 * Copy a string to a "safe" place
 * (that is, to a buffer allocated by malloc).
 */
static char *
save(char *s)
{
	char *p;

	p = malloc((u_int)strlen(s)+1);
	if (p == NULL)
	{
		error(gettxt(MSG(memory), "Cannot allocate memory"));
		quit();
	}
	return(strcpy(p, s));
}

/*
 * Exit the program.
 */
void
quit()
{
	/*
	 * Put cursor at bottom left corner, clear the line,
	 * reset the terminal modes, and exit.
	 */
	quitting = 1;
	lower_left();
	clear_eol();
	deinit();
	flush();
	raw_mode(0);
	exit(errmsgs != 0);
}
