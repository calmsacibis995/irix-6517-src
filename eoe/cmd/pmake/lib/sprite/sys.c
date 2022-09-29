/* 
 * sys.c --
 *
 *	Miscellaneous user-level run-time library routines for the Sys module.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#if !defined(lint) && defined(keep_rcsid)
static char rcsid[] = "Header: sys.c,v 1.1 87/09/14 00:32:36 deboor Exp $ SPRITE (Berkeley)";
#endif not lint


#include <stdio.h>
#include "sprite.h"
#include "sys.h"
#include "varg.h"




/*
 * ----------------------------------------------------------------------------
 *
 * Sys_Panic --
 *
 *      Print a formatted string and then,depending on the panic level,
 *	abort to the debugger or continue.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The process may be put into the debug state.
 *
 * ----------------------------------------------------------------------------
 */

/*VARARGS2*/
void
Sys_Panic(level, format, Varg_Args)
    Sys_PanicLevel      level;	/* Severity of the error. */
    char 	*format;	/* Contains literal text and format control
                                 * sequences indicating how elements of
                                 * Varg_Alist are to be printed.  See the
                                 * Io_Print manual page for details. */
    Varg_Decl;                  /* Variable number of values to be formatted
                                 * and printed. */
{
    Varg_List args;

    Varg_Start(args);

    if (level == SYS_WARNING) {
        fprintf(stderr, "Warning: ");
    } else {
        fprintf(stderr, "Fatal Error: ");
    }

    fprintf(stderr, format, args);
    fflush(stderr);

    if (level == SYS_FATAL) {
	abort();
    }
}
