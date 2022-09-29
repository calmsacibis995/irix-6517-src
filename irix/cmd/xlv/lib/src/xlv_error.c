/**************************************************************************
 *                                                                        *
 *          Copyright (C) 1993, 1994 Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.13 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <tcl.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_error.h>
#include <xlv_utils.h>

int xlv_got_error = 0;

void
xlv_error (char *format, ...) {
	va_list	arg_ptr;

	xlv_got_error = 1;
	va_start (arg_ptr, format);
	fprintf (stderr, "Error: ");
	vfprintf (stderr, format, arg_ptr);
	fprintf (stderr, "\n");

	xlv_openlog("xlv", LOG_PID | LOG_NOWAIT | LOG_CONS | LOG_PERROR, 
		    LOG_USER);
	vsyslog(LOG_USER | LOG_ERR, format, arg_ptr);
	xlv_closelog();

	va_end (arg_ptr);
}

static int opencnt = 0;

/*
 * pass along all openlog calls, but only call closelog on the last
 * close of SYSLOG
 */
void
xlv_openlog(const char *ident, int logstat, int logfac)
{
	
	opencnt++;
	openlog(ident, logstat, logfac);
}

void
xlv_closelog(void)
{
	if (--opencnt)
		return;
	closelog();
}
		

int
xlv_encountered_errors (void) {
	return (xlv_got_error != 0);
}

extern char *xlv_error_text[];

char *
xlv_error_inq_text (unsigned error_code) {

        if (error_code >= XLV_STATUS_LAST) {
                ASSERT (0);
                error_code = XLV_STATUS_ILLEGAL_STATUS_CODE;
        }

        /*
         * Error codes start with 1.  But the array index is zero-based.
         */

        return (xlv_error_text [error_code-1]);
}


/*
 * Note that set_interp_result is not reentrant.  It returns a pointer
 * to static storage.
 */
 
static char	error_string[200];

void
set_interp_result_str (Tcl_Interp *interp,
		   char	   *format,
		   ...) {
        va_list arg_ptr;

        va_start (arg_ptr, format);
        vsprintf (error_string, format, arg_ptr);
        va_end (arg_ptr);

	interp->result = error_string;
}

void
set_interp_result (Tcl_Interp *interp,
		   unsigned error_code,
		   ...) {
	va_list arg_ptr;

	va_start (arg_ptr, error_code);	/* make arg_ptr point to 1st
					   unamed argument. */
	vsprintf (error_string, xlv_error_inq_text (error_code), arg_ptr);
	va_end (arg_ptr);

	interp->result = error_string;
}
