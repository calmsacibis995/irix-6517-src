/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: localvar.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:03  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:48:14  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:47  devrcs
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
**      localvar.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Declarations of procedures in  localvar.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef LOCALVAR_H
#define LOCALVAR_H

void BE_declare_local_var (
#ifdef PROTO
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    AST_operation_n_t *rp,
    char *comment,
    boolean volatility
#endif
);

NAMETABLE_id_t BE_declare_short_local_var (
#ifdef PROTO
    NAMETABLE_id_t name,
    AST_type_n_t *type,
    AST_operation_n_t *rp,
    boolean volatility
#endif
);

void CSPELL_local_var_decls (
#ifdef PROTO
    FILE *fid,
    AST_operation_n_t *rp
#endif
);


#endif
