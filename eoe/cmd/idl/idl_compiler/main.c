/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: main.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:06  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:48:19  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:20  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:28  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      MAIN.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Mainline for the IDL compiler.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <driver.h>             /* Main driver defs */

long main (
#ifdef PROTO
    int argc, char **argv
#endif
);


long main
#ifdef PROTO
(
    int  argc,
    char **argv
)
#else
(argc, argv)
    int  argc;
    char **argv;
#endif
{
    if (!DRIVER_main(argc, argv))
        exit(pgm_error);

    exit (pgm_ok);
}
