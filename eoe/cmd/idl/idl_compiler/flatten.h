/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: flatten.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:39:31  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:01  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:46:51  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:16  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/13  19:03:51  harrow
 * 	Add flag necessary to fix [transmit_as] on a field of an [out_of_line] structure.
 * 	[1992/05/13  11:26:21  harrow]
 * 
 * Revision 1.1  1992/01/19  03:02:36  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      flatten.h
**
**  FACILITY:
**
**      Remote Procedure Call
**
**  ABSTRACT:
**
**  Header file for flatten.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef FLATTEN_H
#define FLATTEN_H

typedef unsigned long BE_fflags_t;

/* Flags to BE_flatten_parameter */
#define BE_F_PA_RTN       0x0001     /* constructing a pointed_at rtn */
#define BE_F_OOL_RTN      0x0002     /* constructing an ool un/marshalling rtn */
#define BE_F_POINTEE      0x0004     /* flattening a pointee */
#define BE_F_PIPE_ROUTINE 0x0008     /* flattening pipe push/pull routine */
#define BE_F_ARRAYIFIED_ELT 0x0010     /* flattening element of arrayified pointer */
#define BE_F_UNDER_OOL    0x0020     /* within an OOL type and not in the
                                     ool un/marshalling routine for that type */

void BE_flatten_parameters
(
#ifdef PROTO
    AST_operation_n_t *oper,
    BE_side_t side
#endif
);

void BE_flatten_field_attrs
(
#ifdef PROTO
    AST_parameter_n_t *params
#endif
);

AST_parameter_n_t *BE_flatten_parameter
(
#ifdef PROTO
    AST_parameter_n_t *param,
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    BE_side_t side,
    boolean top_level,
    AST_parameter_n_t **open_array,
    BE_fflags_t flags
#endif
);

AST_parameter_n_t *BE_yank_pointers
(
#ifdef PROTO
    AST_parameter_n_t *list
#endif
);

BE_ool_i_t *BE_new_ool_info
(
#ifdef PROTO
    AST_parameter_n_t *param,
    AST_type_n_t *type,
    NAMETABLE_id_t name,
    boolean any_pointers,
    boolean use_P_rtn,
    BE_call_rec_t *calls_before,
    boolean top_level
#endif
);

void BE_propagate_orecord_info
(
#ifdef PROTO
    AST_parameter_n_t *flat
#endif
);

#endif
