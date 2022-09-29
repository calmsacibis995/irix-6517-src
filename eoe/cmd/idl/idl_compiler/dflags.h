/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dflags.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:02  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:15  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:46  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:28  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:25  devrcs
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
**      dflags.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header for dflags.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef DFLAGS_H
#define DFLAGS_H

boolean BE_flag_sync
(
#ifdef PROTO
    AST_parameter_n_t *params,
    boolean sync_next
#endif
);

void BE_flag_advance_mp
(
#ifdef PROTO
    AST_parameter_n_t *params,
    BE_side_t side,
    BE_direction_t direction,
    boolean force
#endif
);

int BE_set_alignment
(
#ifdef PROTO
    AST_parameter_n_t *param,
    int initial_alignment
#endif
);

int BE_flag_alignment
(
#ifdef PROTO
    AST_parameter_n_t *params,
    int alignment
#endif
);

int BE_flag_check_buffer
(
#ifdef PROTO
    AST_parameter_n_t *params,
    int count,
    long * current_alignment
#endif
);

boolean BE_any_pointers
(
#ifdef PROTO
    AST_parameter_n_t *params
#endif
);

boolean BE_any_ref_pointers
(
#ifdef PROTO
    AST_parameter_n_t *params
#endif
);

void BE_set_operation_flags
(
#ifdef PROTO
    AST_operation_n_t *oper
#endif
);

#endif
