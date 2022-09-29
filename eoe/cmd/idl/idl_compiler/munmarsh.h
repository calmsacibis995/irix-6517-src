/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: munmarsh.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:29  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:39  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:49:10  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:55  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:53  devrcs
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
**      <filename>
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for munmarsh.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef MUNMARSH_H
#define MUNMARSH_H

#include <marshall.h>

void BE_u_start_buff
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags
#endif
);

void BE_u_end_buff
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags
#endif
);

void BE_u_match_routine_mode
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags,
    boolean routine_mode_required,
    boolean *p_routine_mode
#endif
);

void BE_spell_array_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm,
    boolean *p_routine_mode
#endif
);

void BE_spell_array_hdr_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_start_unmarshall
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_end_unmarshall
(
#ifdef PROTO
    FILE *fid,
    BE_mn_t *nm
#endif
);

void BE_spell_scalar_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_pointer_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_open_record_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_union_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm,
    boolean *p_routine_mode
#endif
);

void BE_spell_conformant_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *mn
#endif
);

void BE_spell_null_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

void BE_spell_ool_unmarshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *nm
#endif
);

#endif
