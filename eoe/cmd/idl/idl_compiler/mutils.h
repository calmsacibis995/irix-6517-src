/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: mutils.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:40:34  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:47  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:49:22  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:05  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/13  19:09:40  harrow
 * 	Add option to enable generation of repas/xmit as free calls for
 * 	[in]-only parameters in addtion to [out] or [in,out] types.
 * 	[1992/05/13  11:39:08  harrow]
 * 
 * Revision 1.1  1992/01/19  03:02:55  devrcs
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
**  NAME:
**
**      mutils.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header file for mutils.c
**
**  VERSION: DCE 1.0
*/

#ifndef MUTILS_H
#define MUTILS_H

/*
 * 'when' values for BE_spell_alloc_ref_pointers
 */
typedef enum
{
    BE_arp_before,    /* Pre-pass over fixed-size parameters */
    BE_arp_during,    /* Unmarshalling [in...] conformant parameters */
    BE_arp_after      /* [out] only conformant parameters, i.e. arrays */
} BE_arp_flags_t;

char *BE_scalar_type_suffix
(
#ifdef PROTO
    AST_type_n_t *type
#endif
);

void BE_spell_pointer_init
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *oper
#endif
);

void BE_spell_to_xmits
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_free_xmits
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_assign_xmits
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_from_xmits
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_type_frees
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params,
    boolean in_only
#endif
);

void BE_spell_from_locals
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_free_nets
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_assign_nets
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_to_locals
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param
#endif
);

void BE_spell_free_locals
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params,
    boolean in_only
#endif
);

void BE_spell_ctx_to_wires
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_call_side_t call_side
#endif
);

void BE_spell_ctx_from_wires
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_call_side_t call_side
#endif
);

void BE_spell_ptrs_to_array_init
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params
#endif
);

void BE_spell_ptrs_to_conf_init
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params
#endif
);

void BE_spell_alloc_open_outs
(
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *oper
#endif
);

void BE_spell_alloc_ref_pointer
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params,
    BE_arp_flags_t when,
    boolean in_node
#endif
);

void BE_spell_alloc_ref_pointers
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *params,
    BE_arp_flags_t when,
    boolean in_node
#endif
);

boolean BE_ool_in_flattened_list
(
#ifdef PROTO
    AST_parameter_n_t *p_flat
#endif
);

#endif
