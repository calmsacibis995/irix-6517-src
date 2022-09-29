/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: inpipes.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:52  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:59  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:07  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:45  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990,1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      inpipes.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for inpipes.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef INPIPES_H
#define INPIPES_H

void CSPELL_caller_in_pipe_decls
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *p_operation
#endif
);

void CSPELL_marshall_in_pipe
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,
    char *pipe_struct_name,
    int pipe_num,
    char *iovec_name,
    boolean do_ool
#endif
);

void CSPELL_marshall_in_pipes
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *p_operation
#endif
);

void CSPELL_pipe_pull_routine
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_pipe_type
#endif
);

#endif
