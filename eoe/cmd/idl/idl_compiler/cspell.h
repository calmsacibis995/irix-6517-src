/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cspell.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:48  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:52  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:10  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:05  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:20  devrcs
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
**      cspell.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Definitions of routines declared in cspell.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef CSPELL_H
#define CSPELL_H

void CSPELL_std_include(
#ifdef PROTO
    FILE *fid,
    char header_name[],
    BE_output_k_t filetype,
    int op_count
#endif
);

void spell_name(
#ifdef PROTO
    FILE *fid,
    NAMETABLE_id_t name
#endif
);

void CSPELL_var_decl(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *type,
    NAMETABLE_id_t name
#endif
);

void CSPELL_typed_name(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *type,
    NAMETABLE_id_t name,
    AST_type_n_t *in_typedef,
    boolean in_struct,
    boolean spell_tag
#endif
);

void CSPELL_function_def_header(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *oper,
    NAMETABLE_id_t name
#endif
);

void CSPELL_cast_exp(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *tp
#endif
);

void CSPELL_type_exp_simple(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *tp
#endif
);

boolean CSPELL_scalar_type_suffix(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *tp
#endif
);

#endif
