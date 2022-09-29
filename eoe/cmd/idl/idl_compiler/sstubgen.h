/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sstubgen.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  22:11:46  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:38:24  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  19:08:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:46  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:03:16  devrcs
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
**      sstubgen.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Prototypes for routines in sstubgen.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef SSTUBGEN_H
#define SSTUBGEN_H

extern void BE_gen_sstub
(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    language_k_t language,
    char header_name[],
    boolean bugs[],
    boolean generate_mepv
#endif
);

#endif
