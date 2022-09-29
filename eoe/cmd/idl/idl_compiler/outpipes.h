/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: outpipes.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:23  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:38:05  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:51:02  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:26  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:10  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      outpipes.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for outpipes.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef OUTPIPES_H
#define OUTPIPES_H

void CSPELL_caller_out_pipe_decls
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *p_operation
#endif
);

void CSPELL_unmarshall_out_pipe
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,
    char *pipe_struct_name,
    int pipe_num,
    char *mp_name,
    char *op_name,
    char *drep_name,
    char *elt_name,
    char *call_h_name,
    char *st_name,
    boolean do_ool,
    boolean *p_routine_mode
#endif
);

void CSPELL_unmarshall_out_pipes
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *p_operation
#endif
);

void CSPELL_pipe_push_routine
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_pipe_type
#endif
);

#endif
