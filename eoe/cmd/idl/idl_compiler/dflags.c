/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dflags.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:59  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:11  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:41  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:24  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:01  devrcs
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
**      dflags.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines to set various decoration flags on parameters
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <bounds.h>
#include <dflags.h>

/*
 * The offset pointer is required if there are any pointers or if there
 * are any parameters which might be marshalled out-of-packet, i.e. arrays
 */
#define op_type(type) ((type)->kind == AST_array_k ||\
                       (type)->kind == AST_pointer_k)

/*
 * set_sync
 *
 * Passed a parameter and a sync-mp-before-this-next-parameter boolean, sets
 * BE_SYNC_MP as appropriate.  Returns a new synchronization boolean
 */
static boolean set_sync
#ifdef PROTO
(
    AST_parameter_n_t *param,
    boolean sync_this
)
#else
(param, sync_this)
    AST_parameter_n_t *param;
    boolean sync_this;
#endif
{
    BE_arm_t *arm;
    boolean sync_next = false;

    if (sync_this) BE_PI_Flags(param) |= BE_SYNC_MP;

    if (param->type->kind == AST_disc_union_k)
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
            sync_next |= BE_flag_sync(arm->flat, false);
    else sync_next = (param->type->kind == AST_array_k &&
                      !(BE_PI_Flags(param) & BE_ARRAY_HEADER));

    return sync_next;
}

/*
 * BE_flag_sync
 *
 * Calls set_sync on each parameter of the list passed.
 */
boolean BE_flag_sync
#ifdef PROTO
(
    AST_parameter_n_t *params,
    boolean sync_next
)
#else
(params, sync_next)
    AST_parameter_n_t *params;
    boolean sync_next;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
        sync_next = set_sync(param, sync_next);

    return sync_next;
}

/*
 * BE_flag_advance_mp
 *
 * Passed a parameter list and a boolean, sets BE_ADVANCE_MP on the parameters
 * in the list if the marshalling pointer should be advanced after marshalling
 * the parameter or if 'force' is non-zero
 */
void BE_flag_advance_mp
#ifdef PROTO
(
    AST_parameter_n_t *params,
    BE_side_t side,
    BE_direction_t direction,
    boolean force
)
#else
(params, side, direction, force)
    AST_parameter_n_t *params;
    BE_side_t side;
    BE_direction_t direction;
    boolean force;
#endif
{
    AST_parameter_n_t *param;
    BE_arm_t *arm;

    for (param = params; param; param = param->next)
    {
        if (param->type->kind == AST_disc_union_k ||
            param->type->kind == AST_pointer_k ||
            param->next != NULL ||
            force) BE_PI_Flags(param) |= BE_ADVANCE_MP;

        if (param->type->kind == AST_disc_union_k)
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                BE_flag_advance_mp(arm->flat, side, direction,
                                   force | param->next != NULL);
    }
}

/*
 * BE_set_alignment
 *
 * Passed a parameter and an initial alignment, sets BE_ALIGN_MP as
 * appropriate.  Returns the resulting alignment for the parameter.
 */
int BE_set_alignment
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int initial_alignment
)
#else
(param,initial_alignment)
    AST_parameter_n_t *param;
    int initial_alignment;
#endif
{
    AST_parameter_n_t *pp;
    BE_arm_t *arm;
    int res, alignment;

    if (param->type->kind == AST_disc_union_k)
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
            BE_flag_alignment(arm->flat, initial_alignment);
    else
    {
        alignment = BE_required_alignment(param);

        if (initial_alignment < alignment) BE_PI_Flags(param) |= BE_ALIGN_MP;
    }

    return (res = BE_resulting_alignment(param)) ? res : initial_alignment;
}

/*
 * BE_flag_alignment
 *
 * Passed an initial alignment and a parameter list, calls set_alignment
 * on each parameter in the list passed.  Returns the resulting alignment.
 */
int BE_flag_alignment
#ifdef PROTO
(
    AST_parameter_n_t *params,
    int alignment
)
#else
(params, alignment)
    AST_parameter_n_t *params;
    int alignment;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
        alignment = BE_set_alignment(param, alignment);

    return alignment;
}

/*
 * set_check_buffer
 *
 * Passed an overestimate of the number of bytes on the wire before the
 * parameter passed, sets BE_CHECK_BUFFER on the parameter as appropriate.
 * Returns a new overestimate.
 */
static int set_check_buffer
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int count,
    long * current_alignment
)
#else
(param, count, current_alignment)
    AST_parameter_n_t *param;
    int count;
    long * current_alignment;
#endif
{
    BE_arm_t *arm;
    int this_arm_count;
    int longest_arm_count;
    long temp_alignment;
    long worst_alignment;

    if (param->type->kind == AST_disc_union_k)
    {
        longest_arm_count = 0;
        worst_alignment = 8;
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
        {
            temp_alignment = *current_alignment;
            this_arm_count = BE_flag_check_buffer(arm->flat, count,
                &temp_alignment);
            if (longest_arm_count < this_arm_count)
                longest_arm_count = this_arm_count;
            if (worst_alignment > temp_alignment);
                worst_alignment = temp_alignment;
        }

        *current_alignment = worst_alignment;
        /*
         * Doesn't need to be a += operation, since the size of the preceeding
         * count value was added in during the call to
         * BE_flag_check_buffer.
         */
        count = longest_arm_count;
    }
    else
    {
        count += BE_wire_bound(param, current_alignment);
        if (count > MIN_BUFF_SIZE) BE_PI_Flags(param) |= BE_CHECK_BUFFER;
    }

    return count;
}

/*
 * BE_flag_check_buffer
 *
 * Passed an overestimate of the number of bytes on the wire before the
 * parameter list passed, calls set_check_buffer for each element of the list.
 * Returns a new overestimate.
 */
int BE_flag_check_buffer
#ifdef PROTO
(
    AST_parameter_n_t *params,
    int count,
    long * current_alignment
)
#else
(params, count, current_alignment)
    AST_parameter_n_t *params;
    int count;
    long * current_alignment;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
        count = set_check_buffer(param, count, current_alignment);

    return count;
}

/*
 * BE_any_pointers
 *
 * Returns non-zero if there are any pointers, arrays containing pointers,
 * unions containing pointers, etc. ad infinitum in the parameter list passed.
 */
boolean BE_any_pointers
#ifdef PROTO
(
    AST_parameter_n_t *params
)
#else
(params)
    AST_parameter_n_t *params;
#endif
{
    AST_parameter_n_t *param;
    BE_arm_t *arm;

    for (param = params; param; param = param->next)
    {
        if ( (param->type->kind == AST_pointer_k)
             && ( ! AST_IGNORE_SET(param) ) ) return true;

        else if (param->type->kind == AST_array_k &&
            BE_any_pointers(BE_Array_Info(param)->flat_elt)) return true;

        else if (param->type->kind == AST_disc_union_k)
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                if (BE_any_pointers(arm->flat)) return true;
    }

    return false;
}

/*
 * op_required
 *
 * Returns non-zero if the offset pointer must be maintained for the
 * parameter list passed
 */
static boolean op_required
#ifdef PROTO
(
    AST_parameter_n_t *params
)
#else
(params)
    AST_parameter_n_t *params;
#endif
{
    AST_parameter_n_t *param;
    BE_arm_t *arm;

    for (param = params; param; param = param->next)
    {
        if (op_type(param->type)) return true;
        else if (param->type->kind == AST_disc_union_k)
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                if (op_required(arm->flat)) return true;
    }
    return false;
}

#if 0 /* should eventually be used to control the boolean buffer_changed local */
/*
 * any_complex_arrays
 *
 * Returns non-zero if there are any arrays of non-scalars in the list passed
 */
static boolean any_complex_arrays
#ifdef PROTO
(
    AST_parameter_n_t *params
)
#else
(params)
    AST_parameter_n_t *params;
#endif
{
    AST_parameter_n_t *param;
    BE_arm_t *arm;

    for (param = params; param; param = param->next)
    {
        if (param->type->kind == AST_array_k &&
            param->type->type_structure.array->element_type == AST_structure_k ||
            param->type->type_structure.array->element_type == AST_disc_union_k)
            return true;
        else if (param->type->kind == AST_disc_union_k)
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                if (any_complex_arrays(arm->flat)) return true;
    }
    return false;
}
#endif

/*
 * BE_any_ref_pointers
 *
 * Returns non-zero if any parameter in the list passed is or contains a [ref]
 * pointer, where "contains" applies to array elements (and fields of
 * structures), but not to unions arms or pointer referents
 */
boolean BE_any_ref_pointers
#ifdef PROTO
(
    AST_parameter_n_t *params
)
#else
(params)
    AST_parameter_n_t *params;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
    {
        if (param->type->kind == AST_pointer_k && AST_REF_SET(param))
            return true;
        else if (param->type->kind == AST_array_k &&
                 BE_any_ref_pointers(BE_Array_Info(param)->flat_elt))
            return true;
    }

    return false;
}

/*
 * BE_set_operation_flags
 *
 * Sets BE_MAINTAIN_OP if appropriate
 */
void BE_set_operation_flags
#ifdef PROTO
(
    AST_operation_n_t *oper
)
#else
(oper)
    AST_operation_n_t *oper;
#endif
{
    if (AST_HAS_IN_PIPES_SET(oper) || AST_HAS_OUT_PIPES_SET(oper) ||
        AST_HAS_IN_OOLS_SET(oper) || AST_HAS_OUT_OOLS_SET(oper) ||
        op_required(BE_Flat_Params(oper)))
        oper->be_info.oper->flags |= BE_MAINTAIN_OP;

#if 0
    if (any_complex_arrays(BE_Recs(oper)))
        oper->be_info.oper->flags |= BE_BUFF_BOOL;
#endif
}
