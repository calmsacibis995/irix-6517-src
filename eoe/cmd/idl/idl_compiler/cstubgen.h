/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cstubgen.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:52  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:00  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:12  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:21  devrcs
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
**      cstubgen.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for cstubgen.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef CSTUBGEN_H
#define CSTUBGEN_H

#include <clihandl.h>

extern BE_handle_info_t BE_handle_info;

void CSPELL_test_status (
#ifdef PROTO
    FILE *fid
#endif
);

void CSPELL_test_transceive_status
(
#ifdef PROTO
    FILE *fid
#endif
);

void BE_gen_cstub(
#ifdef PROTO
    FILE *fid,                      /* Handle for emitted C text */
    AST_interface_n_t *p_interface, /* Ptr to AST interface node */
    language_k_t language,          /* Language stub is to interface to */
    char header_name[],             /* Name of header file to be included in stub */
    boolean bugs[],                 /* List of backward compatibility "bugs" */
    boolean generate_cepv           /* generate cepv if true */
#endif
);

#endif
