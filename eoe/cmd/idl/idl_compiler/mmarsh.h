/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: mmarsh.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:23  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:31  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:49:00  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:46  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:52  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      mmarsh.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for mmarsh.c
**
**  VERSION: DCE 1.0
**
*/

#ifndef MMARSH_H
#define MMARSH_H

#include <marshall.h>

void BE_m_align_mp
(
#ifdef PROTO
    FILE *fid,
    char *mp,
    AST_type_n_t *type,
    BE_mflags_t flags
#endif
);

void BE_m_start_buff
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags
#endif
);

void BE_m_end_buff
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags
#endif
);

void BE_spell_start_marshall
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags    /*  BE_M_MAINTAIN_OP */
#endif
);

void BE_spell_start_block
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags,
    int num_elt
#endif
);

void BE_spell_end_block
(
#ifdef PROTO
    FILE *fid,
    BE_mflags_t flags
#endif
);

void BE_spell_slot_length
(
#ifdef PROTO
    FILE *fid,
    int slot_num
#endif
);

void BE_m_flip_routine_mode
(
#ifdef PROTO
    FILE *fid,
    boolean old_routine_mode
#endif
);

void BE_spell_scalar_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

void BE_spell_null_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

void BE_spell_varying_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags
#endif
);

void BE_spell_array_hdr_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

void BE_spell_array_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num,
    boolean *p_routine_mode,
    int total_slots
#endif
);

void BE_spell_open_record_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

void BE_spell_union_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num,
    boolean *p_routine_mode,
    int total_slots
#endif
);

void BE_spell_pointer_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

void BE_spell_ool_marshall
(
#ifdef PROTO
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    int slot_num
#endif
);

#endif
