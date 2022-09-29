/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: message.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:18  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:22  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:48:45  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:37  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:50  devrcs
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
**  NAME:
**
**      message.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Constant definitions for international message support.
**
**  VERSION: DCE 1.0
**
*/

void message_print(
#ifdef __STDC__
    long msgid, ...
#endif
);

void message_open(
#ifdef PROTO
    char *image_name
#endif
);

void message_close(
#ifdef PROTO
    void
#endif
);
