/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodunmar.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:13  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:49  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:38  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:07  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:07  devrcs
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
**      nodunmar.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for nodunmar.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef NODUNMAR_H
#define NODUNMAR_H

#include <marshall.h>

extern BE_mn_t BE_unmarshall_node_names;

void CSPELL_unmarshall_node_decl
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    boolean varying
#endif
);

void CSPELL_unmarshall_node_pointees
(
#ifdef PROTO
    FILE *fid,
    BE_call_side_t call_side,
    AST_parameter_n_t *flattened_type
#endif
);

void CSPELL_unmarshall_node
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be unmarshalled */
    BE_call_side_t call_side,
    boolean declare_static,
    boolean in_aux
#endif
);

void BE_gen_node_unmarshalling
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
#endif
);

#endif
