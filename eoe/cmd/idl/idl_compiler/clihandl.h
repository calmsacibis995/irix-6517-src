/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: clihandl.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:30  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:22  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:34  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:13  devrcs
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
**      clihandl.h
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Header file for clihandl.c
**
**  VERSION: DCE 1.0
**
*/
#ifndef CLIHANDL_H
#define CLIHANDL_H

#include <commstat.h>

typedef enum {
    BE_parm_handle_t_k,    /* Operation has first parameter of type handle_t */
    BE_parm_user_handle_k, /* Operation has first parm with [handle] attrib */
    BE_context_handle_k,   /* Operation has an [in] context handle parameter */
    BE_impl_handle_t_k,    /* No handle parm. Implicit handle_t handle */
    BE_impl_user_handle_k, /* No handle parm. Implicit [handle] handle */
    BE_auto_handle_k,      /* No handle parm. [auto_handle] interface */
    BE_rep_as_handle_t_k,  /* First parm handle_t with [rep_as] passed by val */
    BE_rep_as_handle_t_p_k /* First parm handle_t with [rep_as] passed by ref */
} BE_handle_type_k_t;

typedef struct {
    BE_handle_type_k_t handle_type;  /* Type of handle for operation */
    char *assoc_name;             /* Ptr to name to be used for assoc handle */
    char *type_name;                /* Ptr to name of [handle] type */
    char *user_handle_name;         /* Ptr to name of [handle] object */
    char deref_assoc;         /* '*' if handle must be dereferenced, else ' ' */
    char deref_generic;      /* '*' if handle must be dereferenced, else ' ' */
    boolean auto_handle_idempotent_op;  /* Only used if op is [auto_handle]
                                           TRUE if op is [idempotent] */
    NAMETABLE_id_t rep_as_name; /* Name of handle_t param to which [rep_as]
                                        is attached */
    NAMETABLE_id_t rep_as_type; /* Type of handle param which has [rep_as] */
} BE_handle_info_t;

extern char assoc_handle_ptr[];

void BE_setup_client_handle(
#ifdef PROTO
    FILE *fid,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation,
    BE_handle_info_t *p_handle_info
#endif
);

void CSPELL_call_start(
#ifdef PROTO
    FILE *fid,
    BE_handle_info_t *p_handle_info,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation,
    unsigned long op_num,            /* Number of current operation */
    BE_stat_info_t *p_comm_stat_info,
    BE_stat_info_t *p_fault_stat_info
#endif
);

void CSPELL_auto_handle_statics
(
#ifdef PROTO
    FILE * fid
#endif
);

void CSPELL_restart_logic
(
#ifdef PROTO
    FILE * fid,
    AST_operation_n_t *p_operation,
    boolean uses_packet
#endif
);

void CSPELL_binding_free_if_needed
(
#ifdef PROTO
    FILE *fid,
    BE_handle_info_t *p_handle_info
#endif
);

#endif

