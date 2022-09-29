/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: genpipes.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:41  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:17  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:15  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:32  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:39  devrcs
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
**      genpipes.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for genpipes.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef GENPIPES_H
#define GENPIPES_H

#define BE_FINISHED_WITH_PIPES -32767

void BE_spell_pipe_struct_name
(
#ifdef PROTO
    AST_parameter_n_t *p_parameter,
    char pipe_struct_name[]
#endif
);

void CSPELL_init_server_pipes
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *p_operation,
    long *p_first_pipe      /* ptr to index and direction of first pipe */
#endif
);

void CSPELL_pipe_support_header
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_pipe_type,
    BE_pipe_routine_k_t push_or_pull,
    boolean in_header
#endif
);

void BE_gen_pipe_routines
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface
#endif
);

void BE_gen_pipe_routine_decls
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface
#endif
);

void CSPELL_pipe_base_cast_exp
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type
#endif
);

void CSPELL_pipe_base_type_exp
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type
#endif
);

void BE_undec_piped_arrays
(
#ifdef PROTO
    AST_parameter_n_t *flat
#endif
);

#endif
