/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	tty.c    routines for interacting with user's terminal
 *
 *  SCCS
 *
 *	@(#)tty.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines which directly interact with the user's
 *	terminal.
 *
 */

#include "autoconfig.h"

#include <stdarg.h>			/* Use system supplied varargs */

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#  include <sys/stat.h>
#  if HAVE_TERMIO
#    include <termio.h>		/* For terminal handling */
#  else
#    include <sgtty.h>		/* For terminal handling */
#    include <sys/file.h>	/* For ioctl flags */
#  endif
#endif

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "config.h"		/* Configuration file */
#include "errors.h"		/* Error codes */
#include "finfo.h"		/* File information structure */
#include "flags.h"		/* Command line flags */
#include "bruinfo.h"		/* Current invocation information */


/*
 *	External bru variables.
 */

static FILE *ttyout;		/* Stream for writing requests */
static FILE *ttyin;		/* Stream for reading responses */

extern struct cmd_flags flags;	/* Command line flags */
extern struct bru_info info;	/* Current invocation information */

/*
 *	External bru functions.
 */

extern VOID bru_error ();	/* Report an error to user */
extern VOID sig_push ();	/* Push current signal state */
extern VOID sig_pop ();		/* Pop previous signal stat */
extern VOID sig_done ();	/* Set for unconditional exit via "done" */
extern VOID done ();		/* Clean up and exit with status */
extern VOID s_sleep ();		/* Sleep for specified number of seconds */


#define tty_putc(a) s_putc(a,ttyout)
#define tty_getc() s_getc(ttyin)
#define tty_rewind s_rewind
extern int s_getc ();		/* Get character from a stream */
extern int s_putc ();		/* Write character to stream */
extern VOID s_fflush ();	/* Invoke library fflush function */
extern VOID s_rewind ();	/* Switch between read/write (YECH!) */
extern FILE *s_fopen ();	/* Open a stream */
extern int s_vfprintf ();	/* Invoke library vfprintf function */
extern int s_ioctl ();		/* Request to control device */
extern int s_fileno ();		/* Convert FILE pointer to file descriptor */

/*
 *	Locally defined functions.
 */

static VOID tty_open ();		/* Open the terminal file */
static VOID tty_inflush ();		/* Flush tty input stream */
char *tty_read ();			/* Read from tty stream into buffer */

VOID tty_fflush ()
{
    if (ttyout != NULL) {
	s_fflush (ttyout);
    }
}
  
/*VARARGS1*/

VOID tty_printf (char *str, ...)
{
    va_list args;

    /*	this can get called from signal handlers and other places before
	the tty is tty_open()'ed.  Either open it here, or ignore the
	call.  Since bru crashed in these cases before, there was obviously
	never any output, so I chose to ignore the call.  Olson, 11/88
    */
    if(ttyout) {
	va_start (args, str);
	(VOID) s_vfprintf (ttyout, str, args);
	va_end (args);
    }
}



/*
 *  FUNCTION
 *
 *	confirmed    confirm an action if confirmation enabled
 *
 *  SYNOPSIS
 *
 *	BOOLEAN confirmed (format, fip)
 *	char *format;
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a printf style format string representing
 *	action to be taken, and pointer to a file information structure,
 *	confirms the action and returns TRUE or FALSE according to
 *	"yes" or "no" response.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin confirmed
 *	    Default result is confirm action
 *	    If confirmation requested and terminal available then
 *		Print action to be taken
 *		Print confirmation request
 *		Get answer from user
 *		Determine result
 *	    End if
 *	    Return result
 *	End confirmed
 *
 */

BOOLEAN confirmed (format, fip)
char *format;
struct finfo *fip;
{
    auto char answer [NAMESIZE];
    register BOOLEAN result;

    DBUG_ENTER ("confirmed");
    result = TRUE;
    if (flags.wflag) {
	if (ttyout == NULL) {
	    tty_open ();
	}
	tty_printf (format, fip -> fname);
	tty_printf (": please confirm [y/n] ");
	(VOID) tty_read (answer, sizeof (answer));
	result = (answer[0] == 'y' || answer[0] == 'Y');
    }
    DBUG_RETURN (result);
}


/*
 *  FUNCTION
 *
 *	response    get response from user to a prompt
 *
 *  SYNOPSIS
 *
 *	char response (prompt, dfault)
 *	char *prompt;
 *	char dfault;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a prompt for user, and a default response,
 *	prompts user and returns a single character indicating the
 *	response.  If the response is simply a carriage return, the
 *	the default is returned.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin response
 *	    Save previous signal handling state
 *	    Set up to exit on interrupt or quit
 *	    Issue prompt to user
 *	    Read the answer
 *	    Pop the previous signal handling state
 *	    If no explicit answer given then
 *		Result is the default
 *	    End if
 *	    Return the answer
 *	End response
 *
 */

char response (prompt, dfault)
char *prompt;
char dfault;
{
    SIGTYPE prevINT;
    SIGTYPE prevQUIT;
    auto char answer [2];
    
    DBUG_ENTER ("response");
    sig_push (&prevINT, &prevQUIT);
    sig_done ();
    if (ttyout == NULL) {
	tty_open ();
    }
    tty_printf ("%s: %s  [default: %c] >> ", info.bru_name, prompt, dfault);
    (VOID) tty_read (answer, sizeof (answer));
    sig_pop (&prevINT, &prevQUIT);
    if (answer[0] == EOS) {
	answer[0] = dfault;
    }
    DBUG_RETURN (answer[0]);
}



/*
 *  FUNCTION
 *
 *	tty_read    read a string from the user's terminal
 *
 *  SYNOPSIS
 *
 *	char *tty_read (bufp, tbufsize)
 *	char *bufp;
 *	int tbufsize;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a buffer in which to store a response string,
 *	and the size of the buffer, reads from the tty up to the
 *	limit imposed by the buffer.  Then flushes the rest of
 *	the input.
 *
 *	The rewind call is necessary to allow mixed reads/writes
 *	on the terminal.  Systems which don't allow this use
 *	two separate streams.
 *
 *	Returns pointer to the terminating null character in buffer
 *	or returns NULL if too much input was read for available
 *	buffer.
 *
 *	Note that it is not an error to provide a NULL pointer for
 *	the buffer.  In this case, all input is discarded up to the
 *	next newline and a NULL is returned.  This is useful in
 *	those situations where operation is suspended until the
 *	user issues an acknowledgement.
 *
 *	The -B option (background) is provided for cases where the prefered
 *	action is to simply give up rather than attempting to interact with
 *	the operator.  This might occur, for example, when bru is
 *	used in a shell script that must be run at night with no operator
 *	present, and the script must not hang waiting for input.
 *	In this case, bru just issues an appropriate message and exits
 *	with an error status.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin tty_read
 *	    If abort rather than interact with operator
 *		Finish interaction line with a newline
 *		Issue appropriate error message
 *		Clean up and exit
 *	    Else
 *		Flush any pending output to terminal
 *		Flush any pending input if possible
 *		Rewind the terminal (huh!)
 *		If no buffer or illegal buffer size then
 *		    Flush any input
 *		    Return will be NULL
 *		Else
 *		    While more input and place to put it
 *			Squirrel the character away in buffer
 *		    End while
 *		    Terminate the string in the buffer
 *		    If last character was not end of input then
 *			Remember we overran the buffer
 *			Flush any remaining input on line
 *		    End if
 *		Endif
 *		Rewind the terminal
 *	    End if
 *	    Return result
 *	End tty_read
 *
 */


char *tty_read (bufp, tbufsize)
char *bufp;
int tbufsize;
{
    register char ch;

    DBUG_ENTER ("tty_read");
    if (flags.Bflag) {
	tty_printf ("\n");
    	bru_error (ERR_BACKGND);
	done ();
    } else {
	tty_fflush ();
	tty_inflush ();
	tty_rewind (ttyout);
	if (bufp == NULL || tbufsize < 1) {
	    while (tty_getc () != '\n') {;}
	    bufp = NULL;
	} else {
	    while (((ch = tty_getc ()) != '\n') && --tbufsize) {
		*bufp++ = ch;
	    }
	    *bufp = EOS;
	    if (ch != '\n') {
		bufp = NULL;
		while (tty_getc () != '\n') {;}
	    }
	}
	tty_rewind (ttyin);
    }
    DBUG_RETURN (bufp);
}


/*
 *  FUNCTION
 *
 *	tty_inflush    flush the tty input queue if possible
 *
 *  SYNOPSIS
 *
 *	static VOID tty_inflush
 *
 *  DESCRIPTION
 *
 *	This function is called to flush the input queue, if
 *	possible, before each interactive prompt.  This insures
 *	that no stray typeaheads will cause unintentional results.
 *	This function can be null without any serious adverse
 *	affects on functionality.
 *
 *	Under BSD 4.2, be sure to flush only the input queue.
 */

static VOID tty_inflush ()
{
    DBUG_ENTER ("tty_inflush");

#ifdef TCFLSH
    (VOID) s_ioctl (s_fileno (ttyin), TCFLSH, 0);
#else
#ifdef TIOCFLUSH
    {
	int tflags;
	tflags = FREAD;
	(VOID) s_ioctl (s_fileno (ttyin), (int) TIOCFLUSH, (int) &tflags);
    }
#endif
#endif

    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	tty_open    open the tty file for possible interaction with user
 *
 *  SYNOPSIS
 *
 *	static VOID tty_open ()
 *
 *  DESCRIPTION
 *
 *	Opens the user's terminal file for possible interaction later.
 *	Not every invocation of bru will require interaction.  Failure
 *	to open the prefered file will result in a warning and
 *	execution will continue using standard error instead.
 *
 *	This function is generally called only once per bru
 *	invocation, when it is discovered that the current tty pointer
 *	is NULL.
 *
 *	Note that if the -B option has been specified, there is to
 *	be no interaction with the operator.  Thus we simply allow the
 *	interaction stream to default to stderr so that the message
 *	we need an answer to will get written to wherever the errors
 *	are going.
 *
 */

static VOID tty_open ()
{
    DBUG_ENTER ("tty_open");
    if (!flags.Bflag && ttyout == NULL && ttyin == NULL) {
	if ((ttyout = s_fopen (info.bru_tty, "r+")) == NULL)
	    bru_error (ERR_TTYOPEN, info.bru_tty);
	ttyin = ttyout;
    }
    if (ttyout == NULL) {
	ttyout = stderr;
	ttyin = ttyout;
	info.bru_tty = "stderr";
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	tty_newmedia    prompt for new media and device name
 *
 *  SYNOPSIS
 *
 *	VOID tty_newmedia (volume, dfault, newname, newnamesize)
 *	int volume;
 *	char *dfault;
 *	char *newname;
 *	int newnamesize;
 *
 *  DESCRIPTION
 *
 *	Rings the terminal bell and prompts the user for a new media
 *	for the given volume number.  If dfault is non-null then
 *	the default is also shown.  Any input from the user is placed
 *	in the buffer pointed to by newname, of size newnamesize.
 *
 *	There is a 1 second delay after sending out the BELL character
 *	because some terminals or systems seem to require a long
 *	time to process it and may drop characters in the meantime.
 *
 */

VOID tty_newmedia (volume, dfault, newname, newnamesize)
int volume;
char *dfault;
char *newname;
int newnamesize;
{
    DBUG_ENTER ("tty_newmedia");
    if (ttyout == NULL) {
	tty_open ();
    }
#ifdef LATTICE		/* Lattice bug, report it... */
    tty_putc (BELL);
#else
    (VOID) tty_putc (BELL);
#endif
    tty_fflush ();
    s_sleep ((UINT) 1);
    tty_printf ("%s: load volume %d and enter device",
    	info.bru_name, volume);
    if (dfault != NULL) {
	tty_printf (" [default: %s]", dfault);
    }
    tty_printf (" >> ");
    (VOID) tty_read (newname, newnamesize);
    DBUG_VOID_RETURN;
}
