/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: astp_gbl.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:37:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:26  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:09  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:26  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:38  devrcs
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
**
**      ASTP_BLD_GLOBALS.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Defines global variables used by the parser and abstract
**      sytax tree (AST) builder modules.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <astp.h>

/*
 * External variables defined here, exported in ASTP.H
 * Theses externals are shared between the AST builder modules
 */

/*
 *  Interface Attributes
 */

int             interface_pointer_class = 0;    /* Default class for          */
                                                /* undecorated pointers       */
                                                /* Values are 0, ASTP_REF,    */
                                                /* ASTP_UNIQUE, or ASTP_PTR   */

/*
 *  Operation, Parameter, Type Attributes
 */

AST_type_n_t        *ASTP_transmit_as_type = NULL;


/*
 *  Interface just parsed
 */
AST_interface_n_t *the_interface = NULL;

/*
 * List head for saved context for field
 * attributes forward referenced parameters.
 */
ASTP_field_ref_ctx_t *ASTP_field_ref_ctx_list = NULL;

/*
 * List head for referenced struct/union tags.
 */
ASTP_tag_ref_n_t *ASTP_tag_ref_list = NULL;


/*
 *  Control for parser
 */
boolean ASTP_parsing_main_idl = TRUE;

/*
 *  Builtin in constants
 */

AST_constant_n_t    *zero_constant_p = NULL;

/*
 * Builtin base types
 */
AST_type_n_t    *ASTP_char_ptr = NULL,
                *ASTP_boolean_ptr = NULL,
                *ASTP_byte_ptr = NULL,
                *ASTP_void_ptr = NULL,
                *ASTP_handle_ptr = NULL,
                *ASTP_short_float_ptr = NULL,
                *ASTP_long_float_ptr = NULL,
                *ASTP_small_int_ptr = NULL,
                *ASTP_short_int_ptr = NULL,
                *ASTP_long_int_ptr = NULL,
                *ASTP_hyper_int_ptr = NULL,
                *ASTP_small_unsigned_ptr = NULL,
                *ASTP_short_unsigned_ptr = NULL,
                *ASTP_long_unsigned_ptr = NULL,
                *ASTP_hyper_unsigned_ptr = NULL;

/* Default tag for union */
NAMETABLE_id_t  ASTP_tagged_union_id;


