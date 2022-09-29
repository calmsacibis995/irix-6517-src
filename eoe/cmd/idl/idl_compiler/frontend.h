/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: frontend.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:36  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:10  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:03  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:25  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:37  devrcs
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
**      frontend.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Functions exported by frontend.c.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>               /* IDL common defs */
#include <ast.h>                /* Abstract Syntax Tree defs */
#include <nametbl.h>            /* Nametable defs */

boolean FE_main(
#ifdef PROTO
    boolean             *cmd_opt,
    void                **cmd_val,
    STRTAB_str_t        idl_sid,
    AST_interface_n_t   **int_p
#endif
);


AST_interface_n_t   *FE_parse_import(
#ifdef PROTO
    STRTAB_str_t new_input
#endif
);
