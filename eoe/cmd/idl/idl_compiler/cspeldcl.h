/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cspeldcl.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:43  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:44  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:58  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:56  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:18  devrcs
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
**      cspeldcl.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for cspeldcl.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef CSPELDCL_H
#define CSPELDCL_H

extern void CSPELL_constant_val (
#ifdef PROTO
    FILE *fid, AST_constant_n_t *cp
#endif
);

extern void CSPELL_labels (
#ifdef PROTO
    FILE *fid, AST_case_label_n_t *tgp
#endif
);

extern void CSPELL_parameter_list (
#ifdef PROTO
    FILE        *fid,
    AST_parameter_n_t *pp
#endif
);

extern void CSPELL_finish_synopsis (
#ifdef PROTO
    FILE *fid, AST_parameter_n_t *paramlist
#endif
);

#endif
