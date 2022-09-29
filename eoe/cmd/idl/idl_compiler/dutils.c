/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dutils.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.2.4.3  1993/01/03  21:39:10  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:27  bbelch]
 *
 * Revision 1.2.4.2  1992/12/23  18:46:05  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:42  zeliff]
 * 
 * Revision 1.2.2.3  1992/05/13  19:12:54  harrow
 * 	Correct alignment following zero-length conformant array field in struct.
 * 	[1992/05/13  11:30:03  harrow]
 * 
 * Revision 1.2.2.2  1992/04/14  20:39:56  harrow
 * 	     Checkin fix from Terry Dineen that corrects reference expressions
 * 	     for array of arrays with [heap] attribute.  Allows stubs to be
 * 	     compiles under ANSI C.
 * 	     [1992/04/14  18:00:24  harrow]
 * 
 * Revision 1.2  1992/01/19  22:13:25  devrcs
 * 	     Dropping DCE1.0 OSF1_misc port archive
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      dutils.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Utilities for the decoration module
**
**  VERSION: DCE_V1.0.0-1
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <backend.h>
#include <localvar.h>

#define Append(list, item)\
    if (list == NULL) list = item;\
    else {list->last->next = item; list->last = list->last->next->last;}

/*
 * BE_new_local_var_name
 *
 * Returns a unique variable name based on the root name passed.
 * Name persists only until the next invocation.
 */
NAMETABLE_id_t BE_new_local_var_name
#ifdef PROTO
(
    char *root
)
#else
(root)
    char *root;
#endif
{
    static char var_name[38];
    int suffix = 0;

    do
    {
        sprintf(var_name, "IDL_%s_%d", root, suffix++);
    } while (NAMETABLE_lookup_id(var_name) != NAMETABLE_NIL_ID);

    return NAMETABLE_add_id(var_name);
}

/*
 * BE_get_name
 *
 * Returns a character string given a NAMETABLE_id_t
 */
char *BE_get_name
#ifdef PROTO
(
    NAMETABLE_id_t id
)
#else
(id)
    NAMETABLE_id_t id;
#endif
{
    char *retval;

    NAMETABLE_id_to_string(id, &retval);
    return retval;
}

/*
 * BE_required_alignment
 *
 * Returns the alignment required by the parameter passed
 */
int BE_required_alignment
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int this_arm, max;
    BE_arm_t *arm;

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        return AST_SMALL_SET(BE_Open_Array(param)) ? 2 : 4;

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
    {
        /*
         * If there are any conformance or variance words to marshall,
         * return 2 or 4; otherwise no alignment is required
         */
        if (AST_VARYING_SET(param) ||
            (AST_CONFORMANT_SET(param->type) &&
             !(BE_PI_Flags(param) & BE_IN_ORECORD)))
            return AST_SMALL_SET(param) ? 2 : 4;
        else return 1;
    }

    else switch (param->type->kind)
    {
        case AST_array_k:
            /*
             * Return the alignment required by the first element
             */
            return BE_required_alignment(BE_Array_Info(param)->flat_base_elt);

        case AST_disc_union_k:
            /*
             * Return the most stringent alignment required by any of the
             * arms.
             */
            max = 1;
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
            {
                if (arm->flat)
                {
                    this_arm = BE_required_alignment(arm->flat);
                    if (this_arm > max) max = this_arm;
                }
            }
            return max;

        default:
            /*
             *  Objects marked as non marshalled comm/fault status then any
             *  alignment they have should be ignored.  
             */
            if (AST_ADD_COMM_STATUS_SET(param) || 
                AST_ADD_FAULT_STATUS_SET(param)) return 0;
            return param->type->alignment_size;
    }
}

/*
 * BE_resulting_alignment
 *
 * Returns the alignment resulting after marshalling the parameter passed,
 * or zero if the parameter would have no effect on the wire.
 */
int BE_resulting_alignment
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int this_arm, min, elt_alignment;
    BE_arm_t *arm;
    AST_parameter_n_t *elt, *last;

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        return AST_SMALL_SET(BE_Open_Array(param)) ? 2 : 4;

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
    {
        /*
         * If there are any conformance or variance words to marshall,
         * return 2 or 4; otherwise no alignment is required
         */
        if (AST_VARYING_SET(param) ||
            (AST_CONFORMANT_SET(param->type) &&
             !(BE_PI_Flags(param) & BE_IN_ORECORD)))
            return AST_SMALL_SET(param) ? 2 : 4;
        else return 0;
    }

    /*
     * Assume nothing after marshalling the referents of pointers
     */
    else if ( (param->type->kind == AST_pointer_k &&
                        !(BE_PI_Flags(param) & BE_DEFER)) /* Single pointer */
           || (BE_PI_Flags(param) & BE_REF_PASS) ) /* In arrays or union arms */
                 return 1;

    else switch (param->type->kind)
    {
        case AST_array_k:

            /*
             * Find the alignment resulting from the last element
             */
            for (elt=BE_Array_Info(param)->flat_base_elt; elt; elt=elt->next)
                last = elt;

            elt_alignment = BE_resulting_alignment(last);

            /*
             * If the array is neither conformant nor varying or the
             * alignment bug (-bug 1) is in effect then return the resulting
             * element alignment.
             *
             * Otherwise, consider that the array may have no elements,
             * and return the worse of the resulting header alignment and
             * the resulting element alignment.
             */
            if ((!AST_CONFORMANT_SET(param->type) && !AST_VARYING_SET(param)) ||
                (BE_bug_array_align && AST_SMALL_SET(param)))
                return elt_alignment;

            else if (AST_CONFORMANT_SET(param->type) && !AST_VARYING_SET(param)
                        && (BE_PI_Flags(param) & BE_FIELD))
            {
                /* Conformant array field of structure. If zero-length,
                   alignment stays same as previous field - whatever that was */
                return 1;
            }

            else if (AST_SMALL_SET(param) && 2 < elt_alignment) return 2;

            else if (4 < elt_alignment) return 4;

            else return elt_alignment;

        case AST_disc_union_k:
            /*
             * Return the worst alignment resulting from any of the arms.
             */
            min = RPC_MAX_ALIGNMENT;
            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
            {
                if (arm->flat)
                {
                    for (elt = arm->flat; elt; elt = elt->next)
                        last = elt;

                    this_arm = BE_resulting_alignment(last);
                    if (this_arm < min) min = this_arm;
                }
                else
                    /*
                     * If the arm is NULL then nothing is shipped for it, and
                     * the resulting alignment is that of the previously shipped
                     * field.  Since that value is not known, return that we are
                     * totally unaligned.
                     */
                    return 1;
            }
            return min;

        default:
            /*
             *  Objects marked as non marshalled comm/fault status then any
             *  alignment they have should be ignored.  
             */
            if (AST_ADD_COMM_STATUS_SET(param) || 
                AST_ADD_FAULT_STATUS_SET(param)) return 0;
            return param->type->alignment_size;
    }
}

/*
 * BE_new_ptr_init
 *
 * Returns a pointer initialization record with fields as passed
 */
BE_ptr_init_t *BE_new_ptr_init
#ifdef PROTO
(
    NAMETABLE_id_t pointer_name,
    AST_type_n_t *pointer_type,
    NAMETABLE_id_t pointee_name,
    AST_type_n_t *pointee_type,
    boolean heap
)
#else
(pointer_name, pointer_type, pointee_name, pointee_type, heap)
    NAMETABLE_id_t pointer_name;
    AST_type_n_t *pointer_type;
    NAMETABLE_id_t pointee_name;
    AST_type_n_t *pointee_type;
    boolean heap;
#endif
{
    BE_ptr_init_t *new = (BE_ptr_init_t *)BE_ctx_malloc(sizeof(BE_ptr_init_t));

    new->heap = heap;
    new->pointer_name = pointer_name;
    new->pointer_type = pointer_type;
    new->pointee_name = pointee_name;
    new->pointee_type = pointee_type;

    new->next = NULL;
    new->last = new;

    return new;
}

/*
 * BE_get_type_node
 *
 * Allocates and returns a type node
 */
AST_type_n_t *BE_get_type_node
#ifdef PROTO
(
    AST_type_k_t kind
)
#else
(kind)
    AST_type_k_t kind;
#endif
{
    AST_type_n_t *new_type = (AST_type_n_t *)BE_ctx_malloc(sizeof(AST_type_n_t));

    new_type->fe_info = NULL;
    new_type->be_info.other = NULL;
    new_type->name = NAMETABLE_NIL_ID;
    new_type->defined_as = NULL;
    new_type->kind = kind;
    new_type->flags = 0;
    new_type->xmit_as_type = NULL;
    new_type->rep_as_type = NULL;

    return new_type;
}

/*
 * BE_pointer_type_node
 *
 * Returns an AST type node which is a pointer to the type passed
 */
AST_type_n_t *BE_pointer_type_node
#ifdef PROTO
(
    AST_type_n_t *type
)
#else
(type)
    AST_type_n_t *type;
#endif
{
    AST_type_n_t *new_type;
    AST_pointer_n_t *new_pointer;

    new_pointer = (AST_pointer_n_t *)BE_ctx_malloc(sizeof(AST_pointer_n_t));
    new_pointer->pointee_type = type;

    new_type = BE_get_type_node(AST_pointer_k);
    new_type->type_structure.pointer = new_pointer;
    new_type->ndr_size =
    new_type->alignment_size = 4;

    return new_type;
}

/*
 * BE_slice_type_node
 *
 * Returns an AST type node which is of the type resulting when the
 * major dimension of the array passed is removed.
 */
AST_type_n_t *BE_slice_type_node
#ifdef PROTO
(
    AST_type_n_t *type
)
#else
(type)
    AST_type_n_t *type;
#endif
{
    AST_type_n_t *new_type;
    AST_array_n_t *new_array;
    int index_count = type->type_structure.array->index_count;
    AST_type_n_t *et = type->type_structure.array->element_type;

    if (index_count == 1) return et;

    /*
       In flatten_array 2-d arrays of chars that are arrays of strings
       are converted to 2-d arrays of 1-d [string] arrays rather than
       1-d arrays of 1-d arrays for reasons explained in in comment there.
       Here, however, we need to view such artifacts as 1-d arrays whose
       slice type is the [string] array.
    */
    if (index_count == 2 && AST_STRING_SET(et)) return et;

    new_array = (AST_array_n_t *)BE_ctx_malloc(sizeof(AST_array_n_t));
    new_array->element_type = type->type_structure.array->element_type;
    new_array->index_count = index_count - 1;
    new_array->index_vec = type->type_structure.array->index_vec + 1;

    new_type = BE_get_type_node(AST_array_k);
    new_type->type_structure.array = new_array;

    return new_type;
}

/*
 * BE_first_element_expression
 *
 * Allocates and returns a first element expression for a flattened array
 * parameter
 */
char *BE_first_element_expression
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    char *first_elt_exp;
    int i, index_count, first_elt_exp_size;
    AST_type_n_t *elt_type;

    index_count = BE_Array_Info(param)->loop_nest_count;

    if (BE_PI_Flags(param) & BE_ARR_OF_STR)
        index_count--;

    /* How much space do we need for this expression? */
    first_elt_exp_size = strlen(BE_get_name(param->name))
                         + 2 * (index_count + 1);
    for (i=0; i<index_count; i++)
    {
        if (!AST_SMALL_SET(param) && AST_VARYING_SET(param))
           first_elt_exp_size += strlen(BE_get_name(BE_Array_Info(param)->A[i]));
        else first_elt_exp_size += 10;
    }

    /* Now form the expression:  &<name>[<name>]...[<name>]\0 */
    first_elt_exp = BE_ctx_malloc(first_elt_exp_size);
    sprintf(first_elt_exp, "&%s", BE_get_name(param->name));
    for (i=0; i<index_count; i++)
    {
        if (!AST_SMALL_SET(param) && AST_VARYING_SET(param))
            sprintf(first_elt_exp + strlen(first_elt_exp), "[%s]",
                BE_get_name(BE_Array_Info(param)->A[i]));
        else sprintf(first_elt_exp + strlen(first_elt_exp), "[%d]",
                param->type->type_structure.array->index_vec[i].lower_bound->value.int_val);
    }
    debug(("first element expression for %s is %s\n", BE_get_name(param->name),
           first_elt_exp));
    ASSERTION((first_elt_exp_size>strlen(first_elt_exp)));
    return first_elt_exp;
}

/*
 * BE_num_elts
 *
 * Returns the number of elements in a flattened fixed array
 */
int BE_num_elts
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int count = 1;
    int slice, index_count = param->type->type_structure.array->index_count;

    if (BE_PI_Flags(param) & BE_ARR_OF_STR) index_count--;

    for (slice = 0; slice < index_count; slice++)
        count *=
            param->type->type_structure.array->index_vec[slice].upper_bound->value.int_val -
            param->type->type_structure.array->index_vec[slice].lower_bound->value.int_val + 1;

    return count;
}

/*
 * BE_size_expression_size
 *
 * Get the string length for an expression for the number of elements to be
 * allocated in the flattened array parameter passed of one of the following
 * forms:
 *
 * 1.5.1 NDR:
 *   (upper_bound[0]-lower_bound[0]+1)*...*(upper_bound[n]-lower_bound[n]+1)
 *
 * 2.0 NDR:
 *   Z[0]*...*Z[n]
 */
static int BE_size_expression_size
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int slice, index_count, exp_size;

    index_count = BE_Array_Info(param)->loop_nest_count;

    exp_size = index_count * 5;     /* Space for punctuation */

    if ( AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type)
                              && (BE_PI_Flags(param) & BE_PARRAY) )
    {
        exp_size += strlen(BE_get_name(BE_Array_Info(param)->size_var));
        return exp_size;
    }
    for (slice = 0; slice < index_count; slice++)
    {
        /*
         * For 2.0 conformant arrays the size is just the product of the
         * Z variables
         */
        if (!AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type))
            exp_size += strlen(BE_get_name(BE_Array_Info(param)->Z[slice]));
        else
        {
            /*
             * Otherwise use the size_is if it exists
             */
            if (param->field_attrs && param->field_attrs->size_is_vec &&
                param->field_attrs->size_is_vec[slice].valid)
                exp_size += strlen(BE_get_name
                    (param->field_attrs->size_is_vec[slice].ref.p_ref->name));
            else
            {
                /*
                 * Otherwise make an expression like (upper-lower+1)
                 */
                if (param->field_attrs && param->field_attrs->max_is_vec &&
                    param->field_attrs->max_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                        (param->field_attrs->max_is_vec[slice].ref.p_ref->name))+2;
                else exp_size += 10;

                if (param->field_attrs && param->field_attrs->min_is_vec &&
                    param->field_attrs->min_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                        (param->field_attrs->min_is_vec[slice].ref.p_ref->name))+2;
                else exp_size += 10;
            }
        }
    }

    return exp_size;
}

/*
 * BE_count_expression_size
 *
 * Get the string length of an expression for the number of elements to be
 * marshalled in the flattened array parameter passed of one of the following
 * forms:
 *
 * 1.5.1 NDR:
 *   (upper_bound[0]-lower_bound[0]+1)*...*(upper_bound[n]-lower_bound[n]+1)
 *
 * 2.0 NDR:
 *   B[0]*...*B[n]
 */
static int BE_count_expression_size
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int slice, index_count, exp_size;
    AST_parameter_n_t *first, *last, *length;

    /*
     * [string0]: count expression is strlen()
     */

    if (BE_PI_Flags(param) & BE_STRING0)
    {
        return ( 17 + strlen(BE_get_name(param->name)) );
    }

    index_count = BE_Array_Info(param)->loop_nest_count;

    if (BE_PI_Flags(param) & BE_ARR_OF_STR) index_count--;

    exp_size = index_count * 5;     /* Space for punctuation */

    if (AST_SMALL_SET(param) && (BE_PI_Flags(param) & BE_PARRAY) )
    {
        if (AST_VARYING_SET(param))
            exp_size += strlen(BE_get_name(BE_Array_Info(param)->count_var));
        else INTERNAL_ERROR("BE_count_expression_size, AST_SMALL_SET, BE_PARRAY and not AST_VARYING_SET");
#if 0
/* BE_PARRAY and AST_SMALL_SET can only occur together for the OOL routine for
   a [v1_array], but AST_VARYING is always set when generating such a routine */
        else if (AST_CONFORMANT_SET(param->type))
            exp_size += strlen(BE_get_name(BE_Array_Info(param)->size_var));
        else
            exp_size += BE_size_expression_size(param);
#endif
        return exp_size;
    }

    for (slice = 0; slice < index_count; slice++)
    {
        /*
         * For 2.0 varying arrays the count is just the product of the
         * B variables (for varying arrays) or Z variable (for conformant
         * non-varying arrays)
         */
        if (!AST_SMALL_SET(param) && AST_VARYING_SET(param))
            exp_size += strlen(BE_get_name(BE_Array_Info(param)->B[slice]));
        else if (!AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type))
            exp_size += strlen(BE_get_name(BE_Array_Info(param)->Z[slice]));
        else
        {
            /*
             * Use the length_is field if it exists
             */
            if (param->field_attrs && param->field_attrs->length_is_vec &&
                param->field_attrs->length_is_vec[slice].valid)
                exp_size += strlen(
                    BE_get_name(param->field_attrs->length_is_vec[slice].ref.p_ref->name));
#if 0
/* [size_is] without [length_is] or [last_is] not permitted on [v1_array] */
            /*
             * Otherwise use the size_is field if it exists and there are no
             * first or last fields
             */
            else if (param->field_attrs && param->field_attrs->size_is_vec &&
                     param->field_attrs->size_is_vec[slice].valid &&
                     (!param->field_attrs->first_is_vec ||
                      !param->field_attrs->first_is_vec[slice].valid) &&
                     (!param->field_attrs->last_is_vec ||
                      !param->field_attrs->last_is_vec[slice].valid))
                exp_size +=strlen(BE_get_name
                      (param->field_attrs->size_is_vec[slice].ref.p_ref->name));
#endif
            /*
             * Otherwise make an expression like (upper-lower+1)
             */
            else
            {
                /*
                 * Upper bound: Use the last_is field if it exists
                 */
                if (param->field_attrs && param->field_attrs->last_is_vec &&
                    param->field_attrs->last_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                      (param->field_attrs->last_is_vec[slice].ref.p_ref->name));
#if 0
/* [max_is] without [length_is] or [last_is] not permitted on [v1_array] */
                /*
                 * Otherwise use the max_is field if it exists
                 */
                else if (param->field_attrs && param->field_attrs->max_is_vec &&
                    param->field_attrs->max_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                        (param->field_attrs->max_is_vec[slice].ref.p_ref->name)) + 2;
#endif
                /*
                 * Otherwise there is a fixed upper bound.  Use it.
                 */
                else
                    exp_size += 10;

#if 0
/* [first_is] and [min_is] not supported on [v1_array] */
                /*
                 * Lower bound: Use the first_is field for lower if it exists
                 */
                if (param->field_attrs && param->field_attrs->first_is_vec &&
                    param->field_attrs->first_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                      (param->field_attrs->first_is_vec[slice].ref.p_ref->name))+3;
                /*
                 * Otherwise use the min_is field if it exists
                 */
                else if (param->field_attrs && param->field_attrs->min_is_vec &&
                    param->field_attrs->min_is_vec[slice].valid)
                    exp_size += strlen(BE_get_name
                        (param->field_attrs->min_is_vec[slice].ref.p_ref->name))+3;
#endif

                /*
                 * Otherwise there is a fixed lower bound.  Use it.
                 */
                exp_size += 11;
            }
        }
    }
    return exp_size;
}

/*
 * BE_count_expression
 *
 * Allocate and return an expression for the number of elements to be marshalled
 * in the flattened array parameter passed of one of the following forms:
 *
 * 1.5.1 NDR:
 *   (upper_bound[0]-lower_bound[0]+1)*...*(upper_bound[n]-lower_bound[n]+1)
 *
 * 2.0 NDR:
 *   B[0]*...*B[n]
 */
char *BE_count_expression
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int   count_exp_size = BE_count_expression_size(param);
    char *count_exp = BE_ctx_malloc(count_exp_size);
                                   /* <num>*(<name>-<name>+1)\0 , 1 per dim */
    int slice, index_count;
    AST_parameter_n_t *first, *last, *length;

    index_count = BE_Array_Info(param)->loop_nest_count;
    if (BE_PI_Flags(param) & BE_ARR_OF_STR) index_count--;

    /*
     * [string0]: count expression is strlen()
     */

    if (BE_PI_Flags(param) & BE_STRING0)
    {
        sprintf(count_exp, "strlen((char *)%s)", BE_get_name(param->name));
        debug(("count expression for %s is %s\n", BE_get_name(param->name),
                count_exp));
        ASSERTION((count_exp_size > strlen(count_exp)));
        return count_exp;
    }

    *count_exp = '\0';

    if (AST_SMALL_SET(param) && (BE_PI_Flags(param) & BE_PARRAY) )
    {
        if (AST_VARYING_SET(param))
            strcat(count_exp, BE_get_name(BE_Array_Info(param)->count_var));
        else INTERNAL_ERROR("BE_count_expression, AST_SMALL_SET, BE_PARRAY and not AST_VARYING_SET");
#if 0
/* BE_PARRAY and AST_SMALL_SET can only occur together for the OOL routine for
   a [v1_array], but AST_VARYING is always set when generating such a routine */
        else if (AST_CONFORMANT_SET(param->type))
            strcat(count_exp, BE_get_name(BE_Array_Info(param)->size_var));
        else
            strcat(count_exp, BE_size_expression(param));
#endif
        ASSERTION((count_exp_size > strlen(count_exp)));
        return count_exp;
    }

    for (slice = 0; slice < index_count; slice++)
    {
        if (slice > 0) strcat(count_exp, "*");

        /*
         * For 2.0 varying arrays the count is just the product of the
         * B variables (for varying arrays) or Z variable (for conformant
         * non-varying arrays)
         */
        if (!AST_SMALL_SET(param) && AST_VARYING_SET(param))
            strcat(count_exp, BE_get_name(BE_Array_Info(param)->B[slice]));
        else if (!AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type))
            strcat(count_exp, BE_get_name(BE_Array_Info(param)->Z[slice]));
        else
        {
            /*
             * Use the length_is field if it exists
             */
            if (param->field_attrs && param->field_attrs->length_is_vec &&
                param->field_attrs->length_is_vec[slice].valid)
                strcat(count_exp,
                    BE_get_name(param->field_attrs->length_is_vec[slice].ref.p_ref->name));
#if 0
/* [size_is] without [length_is] or [last_is] not permitted on [v1_array] */
            /*
             * Otherwise use the size_is field if it exists and there are no
             * first or last fields
             */
            else if (param->field_attrs && param->field_attrs->size_is_vec &&
                     param->field_attrs->size_is_vec[slice].valid &&
                     (!param->field_attrs->first_is_vec ||
                      !param->field_attrs->first_is_vec[slice].valid) &&
                     (!param->field_attrs->last_is_vec ||
                      !param->field_attrs->last_is_vec[slice].valid))
                strcat(count_exp,
                    BE_get_name(param->field_attrs->size_is_vec[slice].ref.p_ref->name));
#endif
            /*
             * Otherwise make an expression like (upper-lower+1)
             */
            else
            {
                /*
                 * Upper bound: Use the last_is field if it exists
                 */
                if (param->field_attrs && param->field_attrs->last_is_vec &&
                    param->field_attrs->last_is_vec[slice].valid)
                sprintf(count_exp+strlen(count_exp), "(%s-",
                        BE_get_name(param->field_attrs->last_is_vec[slice].ref.p_ref->name));
#if 0
/* [max_is] without [length_is] or [last_is] not permitted on [v1_array] */
                /*
                 * Otherwise use the max_is field if it exists
                 */
                else if (param->field_attrs && param->field_attrs->max_is_vec &&
                    param->field_attrs->max_is_vec[slice].valid)
                sprintf(count_exp+strlen(count_exp), "(%s-",
                        BE_get_name(param->field_attrs->max_is_vec[slice].ref.p_ref->name));
#endif
                /*
                 * Otherwise there is a fixed upper bound.  Use it.
                 */
                else sprintf(count_exp+strlen(count_exp), "(%d-",
                    param->type->type_structure.array->index_vec[slice].upper_bound->value.int_val);

#if 0
/* [first_is] and [min_is] not supported on [v1_array] */
                /*
                 * Lower bound: Use the first_is field for lower if it exists
                 */
                if (param->field_attrs && param->field_attrs->first_is_vec &&
                    param->field_attrs->first_is_vec[slice].valid)
                sprintf(count_exp+strlen(count_exp), "%s+1)",
                        BE_get_name(param->field_attrs->first_is_vec[slice].ref.p_ref->name));
                /*
                 * Otherwise use the min_is field if it exists
                 */
                else if (param->field_attrs && param->field_attrs->min_is_vec &&
                    param->field_attrs->min_is_vec[slice].valid)
                sprintf(count_exp+strlen(count_exp), "%s+1)",
                        BE_get_name(param->field_attrs->min_is_vec[slice].ref.p_ref->name));
#endif
                /*
                 * Otherwise there is a fixed lower bound.  Use it.
                 */
                 sprintf(count_exp+strlen(count_exp), "%d+1)",
                        param->type->type_structure.array->index_vec[slice].lower_bound->value.int_val);
            }
        }
    }
    debug(("count expression for %s is %s\n", BE_get_name(param->name),
            count_exp));
    ASSERTION((count_exp_size > strlen(count_exp)));
    return count_exp;
}

/*
 * BE_size_expression
 *
 * Allocate and return an expression for the number of elements to be allocated
 * in the flattened array parameter passed of one of the following forms:
 *
 * 1.5.1 NDR:
 *   (upper_bound[0]-lower_bound[0]+1)*...*(upper_bound[n]-lower_bound[n]+1)
 *
 * 2.0 NDR:
 *   Z[0]*...*Z[n]
 */
char *BE_size_expression
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int   size_exp_size = BE_size_expression_size(param);
    char *size_exp = BE_ctx_malloc(size_exp_size);
                                  /* <num>*(<name>-<name>+1)\0 , 1 per dim*/
    int slice, index_count;

    index_count = BE_Array_Info(param)->loop_nest_count;
    *size_exp = '\0';

    if ( AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type)
                              && (BE_PI_Flags(param) & BE_PARRAY) )
    {
        strcat(size_exp, BE_get_name(BE_Array_Info(param)->size_var));
        ASSERTION((size_exp_size > strlen(size_exp)));
        return size_exp;
    }

    for (slice = 0; slice < index_count; slice++)
    {
        if (slice > 0) strcat(size_exp, "*");

        /*
         * For 2.0 conformant arrays the size is just the product of the
         * Z variables
         */
        if (!AST_SMALL_SET(param) && AST_CONFORMANT_SET(param->type))
            strcat(size_exp, BE_get_name(BE_Array_Info(param)->Z[slice]));
        else
        {
            /*
             * Otherwise use the size_is if it exists
             */
            if (param->field_attrs && param->field_attrs->size_is_vec &&
                param->field_attrs->size_is_vec[slice].valid)
                strcat(size_exp,
                    BE_get_name(param->field_attrs->size_is_vec[slice].ref.p_ref->name));
            else
            {
                /*
                 * Otherwise make an expression like (upper-lower+1)
                 */
                if (param->field_attrs && param->field_attrs->max_is_vec &&
                    param->field_attrs->max_is_vec[slice].valid)
                    sprintf(size_exp+strlen(size_exp), "(%s-",
                        BE_get_name(param->field_attrs->max_is_vec[slice].ref.p_ref->name));
                else sprintf(size_exp+strlen(size_exp), "(%d-",
                    param->type->type_structure.array->index_vec[slice].upper_bound->value.int_val);

                if (param->field_attrs && param->field_attrs->min_is_vec &&
                    param->field_attrs->min_is_vec[slice].valid)
                sprintf(size_exp+strlen(size_exp), "%s+1)",
                    BE_get_name(param->field_attrs->min_is_vec[slice].ref.p_ref->name));
                else sprintf(size_exp+strlen(size_exp), "%d+1)",
                    param->type->type_structure.array->index_vec[slice].lower_bound->value.int_val);
            }
        }
    }

    debug(("size expression for %s is %s\n", BE_get_name(param->name),
           size_exp));
    ASSERTION((size_exp_size > strlen(size_exp)));
    return size_exp;
}

/*
 * string_length_exp_size
 *
 * Returns the size of the string expression that will be generated by
 * the next call to string_length_exp.
 */

static int string_length_exp_size
#ifdef PROTO
(
    AST_parameter_n_t *string
)
#else
(string)
    AST_parameter_n_t *string;
#endif
{
    AST_type_n_t *elt_type;
    int exp_size;

    /* Calculate string size for expression */
    exp_size = strlen("rpc_ss_strsiz((idl_char *),)") + sizeof('\0')
                + strlen(BE_get_name(string->name));

    elt_type = string->type->type_structure.array->element_type;
    if (elt_type->rep_as_type)
        exp_size += (strlen("sizeof()")
                    + strlen(BE_get_name(elt_type->rep_as_type->type_name)));
    else if (elt_type->kind == AST_structure_k)
    {
        /*
         * Use the type name if it exists
         */
        if (elt_type->name != NAMETABLE_NIL_ID)
            exp_size += (strlen("sizeof()")
                        + strlen(BE_get_name(elt_type->name)));
        /*
         * otherwise use the tag name
         */
        else
            exp_size += (strlen("sizeof(struct )")
                        + strlen(BE_get_name
                            (elt_type->type_structure.structure->tag_name)));
    }
    else
        exp_size += sizeof('1');

    return exp_size;
}


/*
 * string_length_exp
 *
 * Returns an expression like: rpc_ss_strsiz((idl_char *)arg,sizeof(elt_type))
 * for a [string] array parameter passed.  Result only persists for one
 * invocation.
 */


static char *string_length_exp
#ifdef PROTO
(
    AST_parameter_n_t *string
)
#else
(string)
    AST_parameter_n_t *string;
#endif
{
    char *BE_sle_exp;
    AST_type_n_t *elt_type;
    int exp_size;

    exp_size = string_length_exp_size(string);
    BE_sle_exp = BE_ctx_malloc( exp_size );

    /*
     * rpc_ss_strsiz takes the size of each char in the string (in octets)
     * and returns the number of characters (not octets) in the string.
     */
    sprintf(BE_sle_exp, "rpc_ss_strsiz((idl_char *)%s,",
        BE_get_name(string->name), BE_get_name(string->name));

    elt_type = string->type->type_structure.array->element_type;

    /*
     * We really should use CSPELL_type_BE_sle_exp here, but we unfortunately don't
     * want to emit anything just now--all we want is an BE_sle_expression we can
     * emit later.  So this is a best current approximation thereof.
     *
     * If elt_type is a structure or has a rep_as_type attribute then pass
     * the size of the elt_type, otherwise the size is 1.
     */
    if (elt_type->rep_as_type)
        sprintf(BE_sle_exp+strlen(BE_sle_exp), "sizeof(%s)",
                BE_get_name(elt_type->rep_as_type->type_name));

    else if (elt_type->kind == AST_structure_k)
    {
        /*
         * Use the type name if it exists
         */
        if (elt_type->name != NAMETABLE_NIL_ID)
            sprintf(BE_sle_exp+strlen(BE_sle_exp), "sizeof(%s)",
                    BE_get_name(elt_type->name));
        /*
         * otherwise use the tag name
         */
        else sprintf(BE_sle_exp+strlen(BE_sle_exp), "sizeof(struct %s)",
                BE_get_name(elt_type->type_structure.structure->tag_name));
    }

    else
        sprintf(BE_sle_exp+strlen(BE_sle_exp), "1");

    sprintf(BE_sle_exp+strlen(BE_sle_exp), ")");

    ASSERTION((exp_size > strlen(BE_sle_exp)));
    return BE_sle_exp;
}

/*
 * BE_A_expression_size
 *
 * 2.0 NDR: Get string length for  an expression for the first element of the
 * array to be marshalled in the given dimension, relative to the lower
 * bound of the array, of the form "(first[n]-lower_bound[n])"
 */
static int BE_A_expression_size
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim
)
#else
(param, dim)
    AST_parameter_n_t *param;
    int dim;
#endif
{
    int exp_size;

    /*
     * Pointed-at marshalling: A[n] = IDL_A_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        exp_size = 1 + strlen(BE_get_name(BE_Array_Info(param)->A[dim]));
    /*
     * if there is a first_is value then A[n] := first_is[n] - lower_bound[n]
     */
    else if (param->field_attrs && param->field_attrs->first_is_vec &&
            param->field_attrs->first_is_vec[dim].valid)
    {
        exp_size = 3 + strlen(
            BE_get_name(param->field_attrs->first_is_vec[dim].ref.p_ref->name));
        if (param->field_attrs && param->field_attrs->min_is_vec &&
            param->field_attrs->min_is_vec[dim].valid)
            exp_size += ( 1 + strlen(BE_get_name
                      (param->field_attrs->min_is_vec[dim].ref.p_ref->name)) );
        else exp_size += 11;
    }
    /*
     * if there is no first_is value then A[n] := lower_bound[n]
     */
    else exp_size = 2;

    return exp_size;
}

/*
 * BE_A_expression
 *
 * 2.0 NDR: Allocate and fill in an expression for the first element of the
 * array to be marshalled in the given dimension, relative to the lower
 * bound of the array, of the form "(first[n]-lower_bound[n])"
 */
char *BE_A_expression
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim
)
#else
(param, dim)
    AST_parameter_n_t *param;
    int dim;
#endif
{
    int   exp_size = BE_A_expression_size(param,dim);
    char *a_exp = BE_ctx_malloc(exp_size); /* (<name>-<name>) */

    /*
     * Pointed-at marshalling: A[n] = IDL_A_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        strcpy(a_exp, BE_get_name(BE_Array_Info(param)->A[dim]));
    /*
     * if there is a first_is value then A[n] := first_is[n] - lower_bound[n]
     */
    else if (param->field_attrs && param->field_attrs->first_is_vec &&
            param->field_attrs->first_is_vec[dim].valid)
    {
        sprintf(a_exp, "(%s-",
            BE_get_name(param->field_attrs->first_is_vec[dim].ref.p_ref->name));
        if (param->field_attrs && param->field_attrs->min_is_vec &&
            param->field_attrs->min_is_vec[dim].valid)
            sprintf(a_exp + strlen(a_exp), "%s)",
                BE_get_name(param->field_attrs->min_is_vec[dim].ref.p_ref->name));
        else sprintf(a_exp + strlen(a_exp), "%d)",
            param->type->type_structure.array->index_vec[dim].lower_bound->value.int_val);
    }
    /*
     * if there is no first_is value then A[n] := lower_bound[n]
     */
    else strcpy(a_exp, "0");

    debug(("A expression[%d] for %s is %s\n", dim, BE_get_name(param->name),
           a_exp));

    ASSERTION((exp_size > strlen(a_exp)));
    return a_exp;
}

/*
 * BE_B_expression_sizes
 *
 * 2.0 NDR: Get string lengths for building an expression for the number of
 * elements of an array to be marshalled in the given dimension, relative to
 * the lower bound of the array, of the form "(last[n]-lower_bound[n])"
 */
static void BE_B_expression_sizes
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim,
    int *p_exp_size,
    int *p_lb_size,
    int *p_f_size,
    int *p_l_size
)
#else
(param, dim, p_exp_size, p_lb_size, p_f_size, p_l_size)
    AST_parameter_n_t *param;
    int dim;
    int *p_exp_size;
    int *p_lb_size;
    int *p_f_size;
    int *p_l_size;
#endif
{
    int exp_size;

    *p_lb_size = 1;
    *p_f_size = 1;
    *p_l_size = 1;

    /*
     * Pointed-at marshalling: B[n] = IDL_B_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        exp_size = 1 + strlen(BE_get_name(BE_Array_Info(param)->B[dim]));

    else if (AST_STRING_SET(param))
    {
        /*
         * Use the Z variable (which contains the value of the string length
         * expression) for conformant parameters without max or size
         */
        if (AST_CONFORMANT_SET(param->type) &&
            (!param->field_attrs ||
             ((!param->field_attrs->max_is_vec ||
               !param->field_attrs->max_is_vec[dim].valid) &&
              (!param->field_attrs->size_is_vec ||
               !param->field_attrs->size_is_vec[dim].valid))))
           exp_size = 1 + strlen(BE_get_name(BE_Array_Info(param)->Z[dim]));

        else exp_size = 1 + string_length_exp_size(param);
    }

    /*
     * Use the length-is if it exists
     */
    else if (param->field_attrs && param->field_attrs->length_is_vec &&
             param->field_attrs->length_is_vec[dim].valid)
        exp_size = 1 + strlen(
            BE_get_name(param->field_attrs->length_is_vec[dim].ref.p_ref->name));

    else
    {
#if 0
/* [min_is] not supported */
        /* figure out the lower bound */
        if (param->field_attrs && param->field_attrs->min_is_vec &&
                param->field_attrs->min_is_vec[dim].valid)
            *p_lb_size = 1 + strlen(
              BE_get_name(param->field_attrs->min_is_vec[dim].ref.p_ref->name));
        else
#endif
        *p_lb_size = 11;

        /* figure out the first value to be marshalled */
        if (param->field_attrs && param->field_attrs->first_is_vec &&
            param->field_attrs->first_is_vec[dim].valid)
            *p_f_size = 1 + strlen(BE_get_name
                       (param->field_attrs->first_is_vec[dim].ref.p_ref->name));
        else *p_f_size = *p_lb_size;

        /* figure out the last value to be marshalled */
        if (param->field_attrs && param->field_attrs->last_is_vec &&
            param->field_attrs->last_is_vec[dim].valid)
            *p_l_size = 1 + strlen(BE_get_name
                        (param->field_attrs->last_is_vec[dim].ref.p_ref->name));
        else if (param->field_attrs && param->field_attrs->max_is_vec &&
                 param->field_attrs->max_is_vec[dim].valid)
            *p_l_size = 1 + strlen(BE_get_name
                         (param->field_attrs->max_is_vec[dim].ref.p_ref->name));
        else if (param->field_attrs && param->field_attrs->size_is_vec &&
                 param->field_attrs->size_is_vec[dim].valid)
            *p_l_size = 4 + *p_lb_size + strlen(BE_get_name
                        (param->field_attrs->size_is_vec[dim].ref.p_ref->name));
        else *p_l_size = 11;

        /* B[i] := (last - first + 1) */
        exp_size = 5 + *p_l_size + *p_f_size;
    }

    *p_exp_size = exp_size;
}

/*
 * BE_B_expression
 *
 * 2.0 NDR: Allocate and fill in an expression for the number of elements of
 * an array to be marshalled in the given dimension, relative to the lower
 * bound of the array, of the form "(last[n]-lower_bound[n])"
 */
char *BE_B_expression
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim
)
#else
(param, dim)
    AST_parameter_n_t *param;
    int dim;
#endif
{
    char *b_exp, *lower_bound, *first, *last;
    int exp_size, lb_size, f_size, l_size;

    BE_B_expression_sizes( param, dim, &exp_size, &lb_size, &f_size, &l_size );

    b_exp = BE_ctx_malloc(exp_size); /* (<name> + <name> - 1 - <name> + 1) */
    lower_bound = MALLOC(lb_size);
    first = MALLOC(f_size);
    last = MALLOC(l_size);

    *b_exp = '\0';
    *lower_bound = '\0';
    *first = '\0';
    *last = '\0';

    /*
     * Pointed-at marshalling: B[n] = IDL_B_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        strcpy(b_exp, BE_get_name(BE_Array_Info(param)->B[dim]));

    else if (AST_STRING_SET(param))
    {
        /*
         * Use the Z variable (which contains the value of the string length
         * expression) for conformant parameters without max or size
         */
        if (AST_CONFORMANT_SET(param->type) &&
            (!param->field_attrs ||
             ((!param->field_attrs->max_is_vec ||
               !param->field_attrs->max_is_vec[dim].valid) &&
              (!param->field_attrs->size_is_vec ||
               !param->field_attrs->size_is_vec[dim].valid))))
           sprintf(b_exp, "%s", BE_get_name(BE_Array_Info(param)->Z[dim]));

        else strcpy(b_exp, string_length_exp(param));
    }

    /*
     * Use the length-is if it exists
     */
    else if (param->field_attrs && param->field_attrs->length_is_vec &&
             param->field_attrs->length_is_vec[dim].valid)
        strcpy(b_exp,
            BE_get_name(param->field_attrs->length_is_vec[dim].ref.p_ref->name));

    else
    {
#if 0
/* [min_is] not supported */
        /* figure out the lower bound */
        if (param->field_attrs && param->field_attrs->min_is_vec &&
                param->field_attrs->min_is_vec[dim].valid)
        strcpy(lower_bound,
            BE_get_name(param->field_attrs->min_is_vec[dim].ref.p_ref->name));
        else
#endif
        sprintf(lower_bound, "%d",
            param->type->type_structure.array->index_vec[dim].lower_bound->value.int_val);

        /* figure out the first value to be marshalled */
        if (param->field_attrs && param->field_attrs->first_is_vec &&
            param->field_attrs->first_is_vec[dim].valid)
            strcpy(first,
                BE_get_name(param->field_attrs->first_is_vec[dim].ref.p_ref->name));
        else strcpy(first, lower_bound);

        /* figure out the last value to be marshalled */
        if (param->field_attrs && param->field_attrs->last_is_vec &&
            param->field_attrs->last_is_vec[dim].valid)
            strcpy(last,
                BE_get_name(param->field_attrs->last_is_vec[dim].ref.p_ref->name));
        else if (param->field_attrs && param->field_attrs->max_is_vec &&
                 param->field_attrs->max_is_vec[dim].valid)
            strcpy(last,
                BE_get_name(param->field_attrs->max_is_vec[dim].ref.p_ref->name));
        else if (param->field_attrs && param->field_attrs->size_is_vec &&
                 param->field_attrs->size_is_vec[dim].valid)
            sprintf(last, "%s+%s-1",
                lower_bound,
                BE_get_name(param->field_attrs->size_is_vec[dim].ref.p_ref->name));
        else sprintf(last, "%d",
            param->type->type_structure.array->index_vec[dim].upper_bound->value.int_val);

        /* B[i] := (last - first + 1) */
        sprintf(b_exp, "(%s+1-%s)", last, first);
    }

    debug(("B expression[%d] for %s is %s\n", dim, BE_get_name(param->name),
           b_exp));

    ASSERTION((exp_size > strlen(b_exp)));
    ASSERTION((lb_size > strlen(lower_bound)));
    ASSERTION((f_size > strlen(first)));
    ASSERTION((l_size > strlen(last)));
    FREE(lower_bound);
    FREE(first);
    FREE(last);
    return b_exp;
}

/*
 * BE_Z_expression_size
 *
 * 2.0 NDR: Get string lengths for expressions for the number of elements to be
 * allocated in the dimension of the flattened array parameter passed of the
 * form "(upper_bound[n]-lower_bound[n]+1)"
 */
static int BE_Z_expression_size
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim
)
#else
(param, dim)
    AST_parameter_n_t *param;
    int dim;
#endif
{
    int exp_size;

    /*
     * Pointed-at marshalling: Z[n] = IDL_Z_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        exp_size = 1 + strlen(BE_get_name(BE_Array_Info(param)->Z[dim]));

    /*
     * Use size_is if it exists
     */
    else if (param->field_attrs && param->field_attrs->size_is_vec &&
             param->field_attrs->size_is_vec[dim].valid)
        exp_size = 1 + strlen(
            BE_get_name(param->field_attrs->size_is_vec[dim].ref.p_ref->name));

    /*
     * [string]: If there is no max_is, use an expression based on strlen()
     */
    else if (AST_STRING_SET(param) &&
             (!param->field_attrs || !param->field_attrs->max_is_vec ||
              !param->field_attrs->max_is_vec[dim].valid))
        exp_size = 1 + string_length_exp_size(param);

    else
    {
        if (param->field_attrs && param->field_attrs->max_is_vec &&
            param->field_attrs->max_is_vec[dim].valid)
        exp_size = 3 + strlen(
            BE_get_name(param->field_attrs->max_is_vec[dim].ref.p_ref->name));
        else exp_size = 13;

        if (param->field_attrs && param->field_attrs->min_is_vec &&
            param->field_attrs->min_is_vec[dim].valid)
            exp_size += ( 3 + strlen(BE_get_name
                      (param->field_attrs->min_is_vec[dim].ref.p_ref->name)) );
        else exp_size += 13;
    }

    return exp_size;
}

/*
 * BE_Z_expression
 *
 * 2.0 NDR: Allocate and fill in expressions for the number of elements to be
 * allocated in the dimension of the flattened array parameter passed of the
 * form "(upper_bound[n]-lower_bound[n]+1)"
 */
char *BE_Z_expression
#ifdef PROTO
(
    AST_parameter_n_t *param,
    int dim
)
#else
(param, dim)
    AST_parameter_n_t *param;
    int dim;
#endif
{
    int exp_size = BE_Z_expression_size(param,dim);
    char *z_exp = BE_ctx_malloc(exp_size);           /* (<name>-<name>+1) */

    /*
     * Pointed-at marshalling: Z[n] = IDL_Z_m
     */
    if (BE_PI_Flags(param) & BE_PARRAY)
        strcpy(z_exp, BE_get_name(BE_Array_Info(param)->Z[dim]));

    /*
     * Use size_is if it exists
     */
    else if (param->field_attrs && param->field_attrs->size_is_vec &&
             param->field_attrs->size_is_vec[dim].valid)
        strcpy(z_exp,
            BE_get_name(param->field_attrs->size_is_vec[dim].ref.p_ref->name));

    /*
     * [string]: If there is no max_is, use an expression based on strlen()
     */
    else if (AST_STRING_SET(param) &&
             (!param->field_attrs || !param->field_attrs->max_is_vec ||
              !param->field_attrs->max_is_vec[dim].valid))
        strcpy(z_exp, string_length_exp(param));

    else
    {
        if (param->field_attrs && param->field_attrs->max_is_vec &&
            param->field_attrs->max_is_vec[dim].valid)
        sprintf(z_exp, "(%s-",
            BE_get_name(param->field_attrs->max_is_vec[dim].ref.p_ref->name));
        else sprintf(z_exp, "(%d-",
            param->type->type_structure.array->index_vec[dim].upper_bound->value.int_val);

        if (param->field_attrs && param->field_attrs->min_is_vec &&
            param->field_attrs->min_is_vec[dim].valid)
        sprintf(z_exp+strlen(z_exp), "%s+1)",
            BE_get_name(param->field_attrs->min_is_vec[dim].ref.p_ref->name));
        else sprintf(z_exp+strlen(z_exp), "%d+1)",
            param->type->type_structure.array->index_vec[dim].lower_bound->value.int_val);
    }
    debug(("Z expression[%d] for %s is %s\n", dim, BE_get_name(param->name),
           z_exp));

    ASSERTION((exp_size > strlen(z_exp)));
    return z_exp;
}

/*
 * BE_declare_surrogates
 *
 * Declares surrogates for the decorated parameter passed
 */
void BE_declare_surrogates
#ifdef PROTO
(
    AST_operation_n_t *oper,
    AST_parameter_n_t *param
)
#else
(oper, param)
    AST_operation_n_t *oper;
    AST_parameter_n_t *param;
#endif
{
    NAMETABLE_id_t name;
    int count;

    name = param->be_info.param->name;

    /*
     * [ptr] arrays don't exist on the ssr stack; just a pointer
     * to such an array is used
     */
    if ((param->type->kind == AST_array_k) && AST_PTR_SET(param)) return;

    /*
     * Conformant and [heap] arrays: put a pointer to the slice-type on the
     * stack
     */
    else if (param->type->kind == AST_array_k &&
             (AST_CONFORMANT_SET(param->type) || AST_HEAP_SET(param)))
        BE_declare_local_var(name,
                BE_Array_Info(param->be_info.param->flat)->alloc_type,
                oper, NULL, 0);

    else
        BE_declare_local_var(name, param->type, oper, NULL, 0);
}

/*
 * BE_declare_server_surrogates
 *
 * Calls BE_declare_surrogates for each parameter of an operation
 */
void BE_declare_server_surrogates
#ifdef PROTO
(
    AST_operation_n_t *oper
)
#else
(oper)
    AST_operation_n_t *oper;
#endif
{
    AST_parameter_n_t *param;

    for (param = oper->parameters; param; param = param->next)
    {
        if (param->type->kind == AST_handle_k)
            continue;
        else if ( (param->type->kind == AST_pointer_k)
                     && (param->type->type_structure.pointer->pointee_type->kind
                                                     == AST_handle_k) )
            continue;
        else
            BE_declare_surrogates(oper, param);
    }
}

/*
 * BE_create_recs
 *
 * Copies into a new list the receive parameters in the list passed and
 * returns it.  Allocates new BE_param_i_t structs for the copies.
 */
AST_parameter_n_t *BE_create_recs
#ifdef PROTO
(
    AST_parameter_n_t *params,
    BE_side_t side
)
#else
(params, side)
    AST_parameter_n_t *params;
    BE_side_t side;
#endif
{
    AST_parameter_n_t *param, *this_param;
    AST_parameter_n_t *recs = NULL;


    for (param = params; param; param = param->next)
    {
        if (param->type->kind == AST_pipe_k) continue;

        if ((side == BE_client_side && !AST_OUT_SET(param)) ||
            (side == BE_server_side && !AST_IN_SET(param))) continue;

        this_param = (AST_parameter_n_t *)BE_ctx_malloc(sizeof(AST_parameter_n_t));
        *this_param = *param;
        this_param->be_info.param =
            (BE_param_i_t *)BE_ctx_malloc(sizeof(BE_param_i_t));
        *this_param->be_info.param = *param->be_info.param;
        this_param->next = NULL;
        this_param->last = this_param;

        Append(recs, this_param);
    }

    return recs;
}

#ifdef DEBUG_VERBOSE
void traverse
#ifdef PROTO
(
    AST_parameter_n_t *list,
    int indent
)
#else
(list, indent)
    AST_parameter_n_t *list;
    int indent;
#endif
{
    AST_parameter_n_t *param;
    BE_call_rec_t *call;
    BE_arm_t *arm;
    int i;

    for (param = list; param; param = param->next)
    {
        for (i = 0; i < indent; i++) printf(" ");
        printf("Name: %s [%lx]:", BE_get_name(param->name), param);

        if (AST_IGNORE_SET(param)) printf(" ignore");
        if (AST_REF_SET(param)) printf(" ref");

        if (BE_PI_Flags(param) & BE_OPEN_RECORD) printf(" open_record(hdr)");
        if (BE_PI_Flags(param) & BE_ARRAY_HEADER) printf(" array_header");

        if (BE_PI_Flags(param) & BE_ALIGN_MP) printf(" align_mp");
        if (BE_PI_Flags(param) & BE_SYNC_MP) printf(" sync_mp");
        if (BE_PI_Flags(param) & BE_NEW_SLOT) printf(" new_slot");
        if (BE_PI_Flags(param) & BE_FIELD) printf(" field");
        if (BE_PI_Flags(param) & BE_CHECK_BUFFER) printf(" check_buffer");
        if (BE_PI_Flags(param) & BE_ALLOCATE) printf(" allocate");
        if (BE_PI_Flags(param) & BE_IN_ORECORD) printf(" in_orecord");
        if (BE_PI_Flags(param) & BE_ADVANCE_MP) printf(" advance_mp");
        if (BE_PI_Flags(param) & BE_NEW_BLOCK) printf(" new_block");
        if (BE_PI_Flags(param) & BE_DEFER) printf(" defer");
        if (BE_PI_Flags(param) & BE_STRING0) printf(" string0");
        if (BE_PI_Flags(param) & BE_FATTRS_FLAT) printf(" fattrs_flat");
        if (BE_PI_Flags(param) & BE_PARRAY) printf(" parray");
        if (BE_PI_Flags(param) & BE_PTR_ARRAY) printf(" ptr_array");
        if (BE_PI_Flags(param) & BE_ARRAYIFIED) printf(" arrayified");
        if (BE_PI_Flags(param) & BE_PTR2STR) printf(" ptr2str");
        if (BE_PI_Flags(param) & BE_OOL_HEADER) printf(" ool_header");
        if (BE_PI_Flags(param) & BE_SKIP) printf(" skip");
        if (BE_PI_Flags(param) & BE_OOL) printf(" ool");
        if (BE_PI_Flags(param) & BE_HDRLESS) printf(" ool_hdrless");
        if (BE_PI_Flags(param) & BE_OOL_YANK_ME) printf(" ool_yank_me");
        if (BE_PI_Flags(param) & BE_ARR_OF_STR) printf(" arr_of_str");
        if (BE_PI_Flags(param) & BE_REF_PASS)
        {
            printf(" ref_pass");
            if (param->type->kind == AST_array_k)
                printf("[%x]", BE_Array_Info(param)->original);
        }
        if (BE_PI_Flags(param) & BE_LAST_FIELD) printf(" last_field");

        if (param->type->kind == AST_null_k)
            printf(" align(%d)", param->type->alignment_size);

        printf("\n");

        if (BE_PI_Flags(param) & BE_OOL)
        {
            for (i = 0; i < indent; i++) printf(" ");
            printf("OOL Details\n");
            for (i = 0; i < indent+2; i++) printf(" ");
            printf("[type: %s, name: %s",
                BE_get_name(BE_Ool_Info(param)->type->name),
                BE_get_name(BE_Ool_Info(param)->name));
            if (BE_Ool_Info(param)->any_pointers)
                printf(", any_ptrs");
            if (BE_Ool_Info(param)->use_P_rtn)
                printf(", use_P_rtn");
            printf("]\n");
        }

        if (param->type->kind == AST_array_k &&
            !(BE_PI_Flags(param) & BE_ARRAY_HEADER))
        {
            for (i = 0; i < indent; i++) printf(" ");
            printf("Elements:\n");
            traverse(BE_Array_Info(param)->flat_elt, indent+2);
        }

        if (param->type->kind == AST_disc_union_k)
        {
            for (i = 0; i < indent; i++) printf(" ");
            printf("Arms: (%d total slot%s)\n", BE_Du_Info(param)->vec_size,
                   BE_Du_Info(param)->vec_size != 1 ? "s" : "");

            for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
                traverse(arm->flat, indent+2);
        }

        if (BE_Calls_Before(param))
        {
            for (i = 0; i < indent; i++) printf(" ");
            printf("Calls before:\n");
            for (call = BE_Calls_Before(param); call; call = call->next)
                switch (call->type)
                {
                    case BE_call_xmit_as:
                        for (i = 0; i < indent+2; i++) printf(" ");
                        printf("%s_to_xmit_rep()\n",
                            BE_get_name(call->call.xmit_as.native_type->name));
                    break;
                    case BE_call_ctx_handle:
                        for (i = 0; i < indent+2; i++) printf(" ");
                        printf("ctx_to_wire(%s, %srundown)\n",
                           BE_get_name(call->call.ctx_handle.native_name),
                            call->call.ctx_handle.rundown ? "" : "no ");
                    break;
                }
        }

        if (BE_Calls_After(param))
        {
            for (i = 0; i < indent; i++) printf(" ");
            printf("Calls after:\n");
            for (call = BE_Calls_After(param); call; call = call->next)
                switch(call->type)
                {
                    case BE_call_xmit_as:
                        for (i = 0; i < indent+2; i++) printf(" ");
                        printf("%s_from_xmit_rep()\n",
                            BE_get_name(call->call.xmit_as.native_type->name));
                    break;
                    case BE_call_ctx_handle:
                        for (i = 0; i < indent+2; i++) printf(" ");
                        printf("ctx_from_wire(%s, %srundown)\n",
                           BE_get_name(call->call.ctx_handle.native_name),
                            call->call.ctx_handle.rundown ? "" : "no ");
                }
        }
    }
}

void
traverse_blocks
#ifdef PROTO
(
    BE_param_blk_t *block
)
#else
(block)
    BE_param_blk_t *block;
#endif
{
    int block_count = 0;
    for (; block != NULL; block = block->next)
    {
        printf ("  block %d:", block_count++);
        if (block->flags & BE_SP_BLOCK) printf(" BE_SP_BLOCK");
        if (block->flags & BE_PTR_BLOCK) printf(" BE_PTR_BLOCK");
        if (block->flags & BE_FIRST_BLOCK) printf(" BE_FIRST_BLOCK");
        if (block->flags & BE_PRUNED_BLOCK) printf(" BE_PRUNED_BLOCK");
        printf("\n");
        traverse(block->params, 4);
    }
}

#endif
