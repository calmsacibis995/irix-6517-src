/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodesupp.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:03  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:33  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:52  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:04  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      nodesupp.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for nodesupp.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef NODESUPP_H
#define NODESUPP_H

void CSPELL_pa_routine_name
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,
    BE_call_side_t call_side,
    BE_marshalling_k_t to_or_from_wire,
    boolean varying
#endif
);

void BE_pointed_at_routines
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
#endif
);

void BE_clone_anonymous_pa_types
(
#ifdef PROTO
    AST_interface_n_t *ifp
#endif
);

#endif
