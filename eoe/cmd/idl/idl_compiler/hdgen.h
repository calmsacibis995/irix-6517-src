/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: hdgen.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:49  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:34  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:38  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:48  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:42  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef HDGEN_H
#define HDGEN_H
/*
**
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      hdgen.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Function prototypes for hdgen.c
**
**  VERSION: DCE 1.0
**
*/

void BE_gen_c_header(
#ifdef PROTO
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *ifp, /* Ptr to AST interface node */
    boolean bugs[],         /* List of backward compatibility "bugs" */
    boolean cepv_opt        /* -cepv option present */
#endif
);

void CSPELL_type_def
(
#ifdef PROTO
    FILE *fid,
    AST_type_n_t *tp,
    boolean spell_tag
#endif
);

char *mapchar
(
#ifdef PROTO
    AST_constant_n_t *cp,   /* Constant node with kind == AST_char_const_k */
    boolean warning_flag    /* TRUE => log warning on nonportable escape char */
#endif
);

#endif
