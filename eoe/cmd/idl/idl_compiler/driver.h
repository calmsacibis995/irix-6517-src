/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: driver.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:23  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:55  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:37  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:27  devrcs
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
**      driver.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions for main "driver" module.
**
**  VERSION: DCE 1.0
*/

boolean DRIVER_main(
#ifdef PROTO
    int argc,
    char **argv
#endif
);

void nidl_terminate (
#ifdef PROTO
    void
#endif
);

