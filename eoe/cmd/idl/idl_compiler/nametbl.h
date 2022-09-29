/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nametbl.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:41  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:59  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:49:40  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:16  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:57  devrcs
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
**      NAMETBL.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Header file for Name Table module, NAMETBL.C
**
**  VERSION: DCE 1.0
**
*/


#ifndef  nametable_incl
#define nametable_incl

/*
** IDL.H needs the definition of STRTAB_str_t, so put it first.
*/

typedef char * NAMETABLE_id_t;
#define NAMETABLE_NIL_ID ((NAMETABLE_id_t) NULL)

#ifdef MSDOS
typedef int  STRTAB_str_t ;
#define STRTAB_NULL_STR  ((STRTAB_str_t) 0)
#else
typedef NAMETABLE_id_t  STRTAB_str_t ;
#define STRTAB_NULL_STR  ((STRTAB_str_t) NULL)
#endif


#include <nidl.h>

#define NAMETABLE_id_too_long         1
#define NAMETABLE_no_space            2
#define NAMETABLE_different_casing    3
#define NAMETABLE_string_to_long      4
#define NAMETABLE_bad_id_len          5
#define NAMETABLE_bad_string_len      6

/*
 * This constant needs to be arbitrarily large since derived names added to
 * the nametable can get arbitrarily large, e.g. with nested structures.
 */
#define max_string_len                2048

NAMETABLE_id_t NAMETABLE_add_id(
#ifdef PROTO
    char *id
#endif
);

NAMETABLE_id_t NAMETABLE_lookup_id(
#ifdef PROTO
    char *id
#endif
);

void NAMETABLE_id_to_string(
#ifdef PROTO
    NAMETABLE_id_t NAMETABLE_id,
    char **str_ptr
#endif
);

boolean NAMETABLE_add_binding(
#ifdef PROTO
    NAMETABLE_id_t id,
    char * binding
#endif
);

char* NAMETABLE_lookup_binding(
#ifdef PROTO
    NAMETABLE_id_t identifier
#endif
);

boolean NAMETABLE_add_tag_binding(
#ifdef PROTO
    NAMETABLE_id_t id,
    char * binding
#endif
);

char* NAMETABLE_lookup_tag_binding(
#ifdef PROTO
    NAMETABLE_id_t identifier
#endif
);

char* NAMETABLE_lookup_local(
#ifdef PROTO
    NAMETABLE_id_t identifier
#endif
);

void  NAMETABLE_push_level(
#ifdef PROTO
    void
#endif
);

void  NAMETABLE_pop_level(
#ifdef PROTO
    void
#endif
);

void  NAMETABLE_set_temp_name_mode (
#ifdef PROTO
    void
#endif
);

void  NAMETABLE_set_perm_name_mode (
#ifdef PROTO
    void
#endif
);

void  NAMETABLE_clear_temp_name_mode (
#ifdef PROTO
    void
#endif
);

STRTAB_str_t   STRTAB_add_string(
#ifdef PROTO
    char *string
#endif
);

void  STRTAB_str_to_string(
#ifdef PROTO
    STRTAB_str_t str,
    char **strp
#endif
);

void  NAMETABLE_init(
#ifdef PROTO
    void
#endif
);

#ifdef DUMPERS
void  NAMETABLE_dump_tab(
#ifdef PROTO
    void
#endif
);

#endif
void  STRTAB_init(
#ifdef PROTO
    void
#endif
);

NAMETABLE_id_t NAMETABLE_add_derived_name(
#ifdef PROTO
    NAMETABLE_id_t id,
    char *matrix
#endif
);

NAMETABLE_id_t NAMETABLE_add_derived_name2(
#ifdef PROTO
    NAMETABLE_id_t id1,
    NAMETABLE_id_t id2,
    char *matrix
#endif
);



#endif
