/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: bounds.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:16  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:02  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:02  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:03  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:10  devrcs
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
**      bounds.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for bounds.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef BOUNDS_H
#define BOUNDS_H

int BE_pkt_size(
#ifdef PROTO
    AST_parameter_n_t *param,
    long * current_alignment
#endif
);

int BE_wire_size(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

int BE_list_size(
#ifdef PROTO
    AST_parameter_n_t *list,
    boolean tile
#endif
);

char *BE_list_size_exp(
#ifdef PROTO
    AST_parameter_n_t *list
#endif
);

int BE_wire_bound(
#ifdef PROTO
    AST_parameter_n_t *param,
    long * current_alignment
#endif
);

#endif
