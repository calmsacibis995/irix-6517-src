/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodmarsh.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:08  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:41  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:59  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:05  devrcs
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
**      nodmarsh.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for nodmarsh.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef NODMARSH_H
#define NODMARSH_H

void CSPELL_marshall_node_decl
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *array_desc,
    boolean varying
#endif
);

void CSPELL_return_on_status (
#ifdef PROTO
    FILE *fid
#endif
);

void CSPELL_get_node_number
(
#ifdef PROTO
    FILE *fid,
    char *field_name,             /* The name of the pointer field */
    char *node_number_access,     /* Text to be used to access
                                      "last node number used" */
    boolean in_operation,         /* FALSE if in marshalling routine */
                                  /* TRUE if in operation stub */
    BE_call_side_t call_side
#endif
);

void CSPELL_marshall_node
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean declare_static,
    boolean in_aux
#endif
);

void BE_gen_node_marshalling
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
#endif
);

#endif
