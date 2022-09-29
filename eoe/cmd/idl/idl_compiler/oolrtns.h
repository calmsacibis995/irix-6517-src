/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: oolrtns.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:18  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:58  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:51  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:08  devrcs
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
**      oolrtns.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Prototypes for routines supporting out_of_line marshalling
**
**  VERSION: DCE 1.0
**
*/

boolean BE_type_has_pointers
(
#ifdef PROTO
    AST_type_n_t *p_type
#endif
);

void BE_ool_spell_varying_info
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *flattened_type
#endif
);

void CSPELL_ool_routine_name
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,
    BE_call_side_t call_side,
    BE_marshalling_k_t to_or_from_wire,
    boolean varying,
    boolean pointees
#endif
);

void CSPELL_marshall_ool_type_decl
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type,
    boolean varying,          /* TRUE if rtn is for varying array */
    boolean has_pointers,     /* TRUE if the type includes any pointers */
    boolean pointee_routine,  /* TRUE for routines that marshall pointees */
    boolean v1_stringifiable  /* TRUE if v1_array && !v1_string */
#endif
);

void CSPELL_unmarshall_ool_type_decl
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type,
    boolean varying,          /* TRUE if rtn is for varying array */
    boolean has_pointers,     /* TRUE if the type includes any pointers */
    boolean pointee_routine,  /* TRUE for routines that unmarshall pointees */
    boolean v1_stringifiable  /* TRUE if v1_array && !v1_string */
#endif
);

void BE_out_of_line_routines
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
#endif
);
