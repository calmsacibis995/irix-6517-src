/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* errorutils.c                                                              */
/* This is a generic errorexit function that gets called from all availmon   */
/* programs.  The input parameters that this function takes are              */
/*                                                                           */
/* exitcode    -   If the program wants to exit because of a catastrophic    */
/*                 error, the value of exitcode should not be 0.             */
/* sysloglevel -   If stderr is not a tty which happens in case any program  */
/*                 gets exec'd from another program (like DSM), and the      */
/*                 calling program closes stderr, then the message gets      */
/*                    logged to SYSLOG.  The sysloglevel determines the      */
/*                    priority level of the message that gets logged in      */
/*                    SYSLOG.  This sysloglevel has 2 conditions :           */
/*                    - If the stderr of the program is a tty, then          */
/*                      sysloglevel is ignored.                              */
/*                    - If sysloglevel > debug_level, then sysloglevel       */
/*                      is ignored.                                          */
/*                    In all other cases, message will be logged to SYSLOG.  */
/* format      -   specifies the format for the message.                     */
/* ...         -   Variable arguments.                                       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "amdefs.h"
#include <syslog.h>

#ifdef DEBUG
int   debug_level = LOG_DEBUG;
#else
int   debug_level = LOG_ERR;
#endif

void errorexit(char *progname, int exitcode, int sysloglevel, char *format, ...)
{

    va_list  args;
    int      nobytes = 0;
    char     errString[MAX_LINE_LEN];

    va_start(args, format);
    nobytes += vsnprintf(errString, MAX_LINE_LEN, format, args);
    va_end(args);

    if ( isatty(fileno(stderr)) ) {
	fprintf(stderr, "%s\n", errString);
    } else {
	if ( sysloglevel >= 0 ) {
	    openlog(progname, LOG_PID, LOG_LOCAL0);
	    setlogmask(LOG_UPTO(debug_level));
	    syslog(sysloglevel, errString);
	    closelog();
	}
    }

    if ( exitcode != 0 ) {
	exit(exitcode);
    }

    return;
}
