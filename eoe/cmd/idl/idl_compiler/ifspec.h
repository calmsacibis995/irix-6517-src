/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ifspec.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:44  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:51  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:59  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:43  devrcs
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
**      ifspec.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for ifspec.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef IFSPEC_H
#define IFSPEC_H

extern void CSPELL_interface_def(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *ifp,
    BE_output_k_t kind,
    boolean generate_mepv
#endif
);

#endif
