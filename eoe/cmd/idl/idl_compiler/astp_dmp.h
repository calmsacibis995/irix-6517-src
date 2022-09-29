/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: astp_dmp.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:37:53  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:23  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:04  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:23  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:01  devrcs
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
**  NAME
**      ASTP_DMP.H
**
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Header file for the AST Builder Dumper module, ASTP_DMP.C
**
**  VERSION: DCE 1.0
**
*/

#ifndef ASTP_DMP_H
#define ASTP_DMP_H
#ifdef DUMPERS
#include <nidl.h>
#include <ast.h>

/*
 * Exported dump routines
 */

void AST_dump_interface
(
#ifdef PROTO
    AST_interface_n_t *if_n_p
#endif
);

void AST_dump_operation
(
#ifdef PROTO
    AST_operation_n_t *operation_node_ptr,
    int indentation
#endif
);

void AST_dump_parameter
(
#ifdef PROTO
    AST_parameter_n_t *parameter_node_ptr,
    int indentation
#endif
);

void AST_dump_nametable_id
(
#ifdef PROTO
    char   *format_string,
    NAMETABLE_id_t id
#endif
);

void AST_dump_parameter
(
#ifdef PROTO
    AST_parameter_n_t *param_node_ptr,
    int     indentation
#endif
);

void AST_dump_type(
#ifdef PROTO
    AST_type_n_t *type_n_p,
    char *format,
    int indentation
#endif
);


void AST_dump_constant
(
#ifdef PROTO
    AST_constant_n_t *constant_node_ptr,
    int indentation
#endif
);

void AST_enable_hex_dump();


#endif /* Dumpers */
#endif /* ASTP_DMP_H */
