/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: be_pvt.h,v $
 * Revision 1.3  1993/09/13 16:11:00  jwag
 * split out typedef changes from ksgen changes
 *
 * Revision 1.2  1993/09/08  20:15:18  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:38:05  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:42  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:43:35  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:42  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/24  14:08:56  harrow
 * 	Add support for bug 4, ship holes for arrays of ref pointers when
 * 	contained within other types.
 * 	[1992/03/19  15:13:29  harrow]
 * 
 * Revision 1.1  1992/01/19  03:02:05  devrcs
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
**  NAME:
**
**      be_pvt.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Backend private global definitions
**
**  VERSION: DCE 1.0
**
*/

#ifndef BE_PVT_H
#define BE_PVT_H

#include <nametbl.h>

/*
 * Some backend private globals
 */
extern struct AST_type_n_t *BE_ulong_int_p, *BE_ushort_int_p;
extern struct AST_type_n_t *BE_pointer_p, *BE_function_p;
extern struct AST_type_n_t *BE_hyper_null_p, *BE_long_null_p, *BE_short_null_p;

extern boolean BE_bug_array_no_ref_hole;
extern boolean BE_space_opt, BE_bug_array_align, BE_bug_array_align2, BE_bug_boolean_def;
#ifdef DUMPERS
extern boolean BE_dump_debug, BE_dump_flat, BE_dump_mnode, BE_dump_mool,
               BE_dump_recs, BE_dump_sends, BE_dump_unode, BE_dump_uool;
#endif
#ifdef __sgi
extern boolean BE_ksgen;
extern boolean BE_kstypes;
#endif

/*
 * The runtime will give us at least this many bytes
 */
#define MIN_BUFF_SIZE 880
/*
 * The maximum alignment required by any type
 */
#define RPC_MAX_ALIGNMENT 8

typedef enum {
    BE_client_stub_k,
    BE_server_stub_k,
    BE_server_aux_k,
    BE_client_aux_k
} BE_output_k_t;

typedef enum {BE_client_side, BE_server_side, BE_both_sides} BE_side_t;
typedef enum {BE_in, BE_out} BE_direction_t;
typedef enum {BE_caller, BE_callee} BE_call_side_t;
typedef enum {BE_pipe_push_k, BE_pipe_pull_k, BE_pipe_alloc_k}
                                                BE_pipe_routine_k_t;
typedef enum {BE_marshalling_k, BE_unmarshalling_k} BE_marshalling_k_t;

typedef union be_info_t {
    struct BE_pipe_i_t    *pipe;
    struct BE_type_i_t    *type;
    struct BE_param_i_t   *param;
    struct BE_field_i_t   *field;
    struct BE_oper_i_t    *oper;
#ifndef ultrix
    void                  *other;  /* "other" since "default" is C keyword */
#else
    char                  *other;
#endif
} be_info_t;

typedef struct BE_local_var_t
{
    NAMETABLE_id_t name;
    char *comment;
    boolean volatility;
    struct AST_type_n_t *type;
    struct BE_local_var_t *next;
} BE_local_var_t;

#ifdef DEBUG_VERBOSE
#  define debug(x) if (BE_dump_debug) printf x
#else
#  ifndef VMS
#     define debug(x)
#  else
#     define debug(x) if(0) printf x
#  endif
#endif

#endif
