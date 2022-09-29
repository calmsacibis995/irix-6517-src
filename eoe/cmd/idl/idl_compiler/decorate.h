/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: decorate.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:57  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:08  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:35  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:21  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:23  devrcs
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
**      decorate.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header file for decorate.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef DECORATE_H
#define DECORATE_H

typedef unsigned long BE_dflags_t;

/* Flags to decorate_parameter */
#define BE_D_PARRAY       0x0001     /* pointed-at or out_of_line array */
#define BE_D_PARRAYM      0x0002     /* pointed-at, marshalling pass only */
#define BE_D_OARRAYM      0x0004     /* out_of_line, marshalling pass only */
#define BE_D_OARRAYU      0x0008     /* out_of_line, unmarshalling pass only */

void BE_decorate_parameter
(
#ifdef PROTO
    AST_operation_n_t *oper,
    AST_parameter_n_t *param,
    BE_side_t side,
    BE_dflags_t flags
#endif
);

void BE_decorate_operation
(
#ifdef PROTO
    AST_operation_n_t *oper,
    BE_side_t side
#endif
);

#endif
