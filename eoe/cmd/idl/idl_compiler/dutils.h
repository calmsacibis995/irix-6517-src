/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dutils.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:13  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:32  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:46:10  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:47  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:29  devrcs
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
**      dutils.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for dutils.c
**
**  VERSION: DCE 1.0
*/

#ifndef DUTILS_H
#define DUTILS_H

NAMETABLE_id_t BE_new_local_var_name
(
#ifdef PROTO
    char *root
#endif
);

char *BE_get_name
(
#ifdef PROTO
    NAMETABLE_id_t id
#endif
);

int BE_required_alignment
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

int BE_resulting_alignment
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

struct BE_ptr_init_t *BE_new_ptr_init
(
#ifdef PROTO
    NAMETABLE_id_t pointer_name,
    AST_type_n_t *pointer_type,
    NAMETABLE_id_t pointee_name,
    AST_type_n_t *pointee_type,
    boolean heap
#endif
);

AST_type_n_t *BE_get_type_node
(
#ifdef PROTO
    AST_type_k_t kind
#endif
);

AST_type_n_t *BE_pointer_type_node
(
#ifdef PROTO
    AST_type_n_t *type
#endif
);

AST_type_n_t *BE_slice_type_node
(
#ifdef PROTO
    AST_type_n_t *type
#endif
);

char *BE_first_element_expression
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

char *BE_count_expression
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

char *BE_size_expression
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

void BE_declare_surrogates
(
#ifdef PROTO
    AST_operation_n_t *oper,
    AST_parameter_n_t *param
#endif
);

void BE_declare_server_surrogates
(
#ifdef PROTO
    AST_operation_n_t *oper
#endif
);

int BE_num_elts
(
#ifdef PROTO
    AST_parameter_n_t *param
#endif
);

char *BE_A_expression
(
#ifdef PROTO
    AST_parameter_n_t *param,
    int dim
#endif
);

char *BE_B_expression
(
#ifdef PROTO
    AST_parameter_n_t *param,
    int dim
#endif
);

char *BE_Z_expression
(
#ifdef PROTO
    AST_parameter_n_t *param,
    int dim
#endif
);

AST_parameter_n_t *BE_create_recs
(
#ifdef PROTO
    AST_parameter_n_t *params,
    BE_side_t side
#endif
);

#ifdef DEBUG_VERBOSE

void traverse(
#ifdef PROTO
    AST_parameter_n_t *list,
    int indent
#endif
);

void traverse_blocks(
#ifdef PROTO
BE_param_blk_t *block
#endif
);

#endif


#endif
