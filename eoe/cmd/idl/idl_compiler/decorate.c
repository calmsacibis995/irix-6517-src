/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: decorate.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:04  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:45:30  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:59  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      decorate.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Entry points for parameter flattening and decoration live here.
**  Note that array parameters are the only parameters that get decorated.
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <blocks.h>
#include <flatten.h>
#include <dutils.h>
#include <bounds.h>
#include <localvar.h>
#include <dflags.h>
#include <decorate.h>
#include <backend.h>
#include <genpipes.h>

#define Append(list, item)\
    if (list == NULL) list = item;\
    else {list->last->next = item; list->last = list->last->next->last;}

static void decorate_parameters
(
#ifdef PROTO
    AST_operation_n_t *oper,
    AST_parameter_n_t *params,
    BE_side_t side
#endif
);

/*
 * simple_array
 *
 * Returns true if the array parameter passed can be marshalled with the
 * point-at-the-array or block-copy optimizations.  When we figure out
 * how to tell when a given compiler on a given platform lays out structures
 * as per NDR, aligned structs can also be considered "simple".
 */
static boolean simple_array
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    /* For arrayified pointers, optimizations must be in the pointed_at rtn */
    if (BE_PI_Flags(param) & BE_ARRAYIFIED) return(false);

    switch(BE_Array_Info(param)->element_ptr_type->
                                    type_structure.pointer->pointee_type->kind)
    {
        case AST_boolean_k:
        case AST_byte_k:
        case AST_character_k:

        case AST_small_integer_k:
        case AST_short_integer_k:
        case AST_long_integer_k:
        case AST_hyper_integer_k:

        case AST_small_unsigned_k:
        case AST_short_unsigned_k:
        case AST_long_unsigned_k:
        case AST_hyper_unsigned_k:

        case AST_short_float_k:
        case AST_long_float_k:
            /*
             * Array elements requiring any sort of conversion routine can
             * obviously not be block-copied.  Also, arrays with a call-after
             * routine can not use the point-at-the-array optimization because
             * the call-after routine might deallocate the array before the
             * iovec containing its address is processed by the runtime.
             */
            return (BE_Calls_Before(BE_Array_Info(param)->flat_elt) == NULL
                    &&  BE_Calls_After(param) == NULL);

        default:
            return false;
    }
}

/*
 * decorate_array_elements
 *
 * Fills in the stand-in flattened element parameter for an array
 * Fills in the ndr_element_size field for an array
 * Determines whether alignment is needed before any of the flat
 * element fields
 */
static void decorate_array_elements
#ifdef PROTO
(
    AST_operation_n_t *oper,
    AST_parameter_n_t *param,
    BE_side_t side
)
#else
(oper, param, side)
    AST_operation_n_t *oper;
    AST_parameter_n_t *param;
    BE_side_t side;
#endif
{
    AST_parameter_n_t *open_array;
    int last;

    /*
     * Decorate the array elements
     */
    decorate_parameters(oper, BE_Array_Info(param)->flat_elt, side);

    /*
     * *** Eventually get the alignment right for the first element ***
     */
    last = BE_flag_alignment(BE_Array_Info (param)->flat_base_elt,
                             RPC_MAX_ALIGNMENT);

    /*
     * Now, knowing the alignment for the last element, get it right for the
     * first element
     */
    BE_flag_alignment(BE_Array_Info(param)->flat_base_elt, last);

    BE_Array_Info(param)->ndr_elt_size =
        BE_list_size(BE_Array_Info(param)->flat_elt, 1);

    debug(("Element size for %s is %d\n", BE_get_name(param->name),
        BE_Array_Info(param)->ndr_elt_size));
}

/*
 * decorate_array
 *
 * Conjures up allocation and marshalling expressions and declares
 * local variables for the marshalling of arrays
 */
void decorate_array
#ifdef PROTO
(
    AST_operation_n_t *oper,
    AST_parameter_n_t *param,
    BE_side_t side,
    BE_dflags_t flags
)
#else
(oper, param, side, flags)
    AST_operation_n_t *oper;
    AST_parameter_n_t *param;
    BE_side_t side;
    BE_dflags_t flags;
#endif
{
    char *first_elt_exp, *count_exp;
    BE_ptr_init_t *ptr_init;
    int i, index_count;
    boolean helper_variables_needed;
    boolean helper_variables_exist =
                               oper->be_info.oper->flags & BE_HELPERS_EXIST;

    if (BE_Array_Info(param)->decorated) return;

    debug(("Decorating %x(%x)\n", param, BE_Array_Info(param)));
    index_count = param->type->type_structure.array->index_count;

    helper_variables_needed = ! (BE_PI_Flags(param) & (BE_OOL | BE_SKIP));
    if ( BE_Is_Arr_of_Refs(param) )
        helper_variables_needed = true;
    if ( param->type->kind == AST_array_k &&
        BE_any_ref_pointers(BE_Array_Info(param)->flat_elt) )
        helper_variables_needed = true;
    if (helper_variables_needed)
    {
        /*
         * Declare the element pointer variable, which was created in the
         * flattening phase.
         */
        if (!BE_Array_Info(param)->element_ptr_var_declared)
        {
            BE_declare_local_var(BE_Array_Info(param)->element_ptr_var,
                BE_Array_Info(param)->element_ptr_type, oper, NULL, 0);

            BE_Array_Info(param)->element_ptr_var_declared = true;
        }

        /*
         * Set up the index variable for iterating over the elements
         */
        BE_Array_Info(param)->index_var = BE_new_local_var_name("index");
        BE_declare_local_var(BE_Array_Info(param)->index_var,
                         AST_SMALL_SET(param)? BE_ushort_int_p : BE_ulong_int_p,
                         oper, NULL, 0);

        /*
         * Set up the fragment count variable, which holds the number of
         * elements which can be unmarshalled out of a given packet
         */
        BE_Array_Info(param)->frag_var = NAMETABLE_add_id("NIDL_frag");
        if ( ! helper_variables_exist )
            BE_declare_local_var(BE_Array_Info(param)->frag_var,
                         AST_SMALL_SET(param)? BE_ushort_int_p : BE_ulong_int_p,
                         oper, NULL, 0);
    }

    /*
     * Set up the count variable, which holds the number of elements
     * remaining to be unmarshalled
     */
    BE_Array_Info(param)->count_var = BE_new_local_var_name("count");
    /* Declare it unless [out_of_line] marshalling for [v1_array] */
    if ( ! ( AST_SMALL_SET(param) && AST_VARYING_SET(param)
                 && (flags & BE_D_OARRAYM) && (flags & BE_D_PARRAYM) ) )
    {
        BE_declare_local_var(BE_Array_Info(param)->count_var,
                         AST_SMALL_SET(param)? BE_ushort_int_p : BE_ulong_int_p,
                         oper, NULL, 0);
    }

    if ( helper_variables_needed )
    {
        /*
         * Set up the block copy count variable, which holds the number of bytes
         * to be block copied when marshalling
         */
        BE_Array_Info(param)->block_var = NAMETABLE_add_id("NIDL_block");
        if ( ! helper_variables_exist )
        {
            BE_declare_local_var(BE_Array_Info(param)->block_var,BE_ulong_int_p,
                         oper, NULL, 0);
            oper->be_info.oper->flags |= BE_HELPERS_EXIST;
        }
    }

    /*
     * Conformant arrays need a size variable and expression.  A 2.0
     * conformant array also needs a Z variable and expression for each
     * dimension; its size expression is the product of its Z variables.
     */
    if (AST_CONFORMANT_SET(param->type))
    {
        /* set up the size variable */
        BE_Array_Info(param)->size_var = BE_new_local_var_name("size");
        /* Declare it unless generating [out_of_line] rtn for [v1_array] */
        if ( ! ( AST_SMALL_SET(param)
                                   && (flags & (BE_D_OARRAYM | BE_D_OARRAYU)) ) )
        {
            BE_declare_local_var(BE_Array_Info(param)->size_var,
                AST_SMALL_SET(param) ? BE_ushort_int_p : BE_ulong_int_p,
                oper, NULL, 0);
        }

        if (!AST_SMALL_SET(param))
        {
            /*
             * Allocate Z variables and expressions
             */
            BE_Array_Info(param)->Z =
                (NAMETABLE_id_t *)BE_ctx_malloc(index_count*sizeof(NAMETABLE_id_t));
            BE_Array_Info(param)->Z_exps =
                (char **)BE_ctx_malloc(index_count*sizeof(char *));
            /* set up Z variables and expressions */
            for (i = 0; i < index_count; i++)
            {
                BE_Array_Info(param)->Z[i] = BE_new_local_var_name("Z");

                /*
                 * Don't declare Z's when called from pa marshalling routines
                 * because the Z values are in the operation signature
                 */
                if (!(flags & (BE_D_PARRAYM | BE_D_OARRAYM | BE_D_OARRAYU)))
                    BE_declare_local_var(BE_Array_Info(param)->Z[i],
                                         BE_ulong_int_p, oper, NULL, 0);

                BE_Array_Info(param)->Z_exps[i] = BE_Z_expression(param, i);
            }
        }
    }

    /*
     * 2.0 style varying arrays: set up an A and B variable and expression for
     * each dimension of the array
     */
    if (AST_VARYING_SET(param) && !AST_SMALL_SET(param))
    {
        /* allocate A and B variables and expressions */
        BE_Array_Info(param)->A =
            (NAMETABLE_id_t *)BE_ctx_malloc(index_count*sizeof(NAMETABLE_id_t));
        BE_Array_Info(param)->B =
            (NAMETABLE_id_t *)BE_ctx_malloc(index_count*sizeof(NAMETABLE_id_t));
        BE_Array_Info(param)->A_exps =
            (char **)BE_ctx_malloc(index_count*sizeof(char *));
        BE_Array_Info(param)->B_exps =
            (char **)BE_ctx_malloc(index_count*sizeof(char *));
        /* set up A and B variables and expressions */
        for (i = 0; i < index_count; i++)
        {
            BE_Array_Info(param)->A[i] = BE_new_local_var_name("A");
            BE_Array_Info(param)->B[i] = BE_new_local_var_name("B");

            /*
             * Don't declare A's and B's when called from pa marshalling
             * routines because they are in the operation signature
             */
            if (!(flags & BE_D_PARRAYM))
            {
                BE_declare_local_var(BE_Array_Info(param)->A[i],
                                     BE_ulong_int_p, oper, NULL, 0);
                BE_declare_local_var(BE_Array_Info(param)->B[i],
                                     BE_ulong_int_p, oper, NULL, 0);
            }

            BE_Array_Info(param)->A_exps[i] = BE_A_expression(param, i);
            BE_Array_Info(param)->B_exps[i] = BE_B_expression(param, i);
        }
    }

    if (!AST_CONFORMANT_SET(param->type))
        BE_Array_Info(param)->num_elts = BE_num_elts(param);

    /*
     * Set up the allocation type for conformant and [heap] arrays
     */
    if (AST_CONFORMANT_SET(param->type) || AST_HEAP_SET(param))
        BE_Array_Info(param)->alloc_type =
            BE_pointer_type_node(BE_slice_type_node(param->type));

    /*
     * Size and count expressions may depend upon A, B, and Z variables
     */
    BE_Array_Info(param)->count_exp = BE_count_expression(param);
    BE_Array_Info(param)->first_element_exp =
        BE_first_element_expression(param);
    BE_Array_Info(param)->size_exp = BE_size_expression(param);

    if (simple_array(param))
        BE_Array_Info(param)->flags |= BE_SIMPLE_ARRAY;

    /*
     * Arrange for server-side allocation of fixed arrays with [heap] set
     */
    if (side == BE_server_side && !AST_CONFORMANT_SET(param->type) &&
        AST_HEAP_SET(param))
    {
        debug(("Allocating array %s on the heap\n", BE_get_name(param->name)));
        ptr_init = BE_new_ptr_init(param->name,
                                   BE_Array_Info(param)->alloc_type,
                                   NAMETABLE_NIL_ID, param->type, true);
        Append(BE_Pointers(oper), ptr_init);
    }

    BE_Array_Info(param)->decorated = true;
}

/*
 * BE_decorate_parameter
 *
 * Decorate a flattened parameter with allocated BE_param_i_t fields
 */
void BE_decorate_parameter
#ifdef PROTO
(
    AST_operation_n_t *oper,
    AST_parameter_n_t *param,
    BE_side_t side,
    BE_dflags_t flags
)
#else
(oper, param, side, flags)
    AST_operation_n_t *oper;
    AST_parameter_n_t *param;
    BE_side_t side;
    BE_dflags_t flags;
#endif
{
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    AST_parameter_n_t *flat_elt, *original;

    if (param->type->kind == AST_array_k)
    {
        /*
         * [in] conformant arrays must be allocated on the server side
         * unless they are fields of open records
         */
        if (AST_CONFORMANT_SET(param->type) && side == BE_server_side &&
            AST_IN_SET(param) && !(BE_PI_Flags(param) & BE_IN_ORECORD))
            BE_PI_Flags(param) |= BE_ALLOCATE;

        /*
         * Decorate only the actual array, not the ref_pass gizmo, and not
         * the header gizmo, unless there is no actual array, as in the
         * case of arrays of [ref] pointers, in which case the header
         * gizmo should be decorated.
         */
        if (!(BE_PI_Flags(param) & BE_REF_PASS) &&
            (!(BE_PI_Flags(param) & BE_ARRAY_HEADER) ||
             BE_Is_Arr_of_Refs(param)))
            decorate_array(oper, param, side, flags);

        else if (BE_PI_Flags(param) & BE_REF_PASS)
        {
            /*
             * Copy the decorations from the original array to the ref pass
             * gizmo, but save the "flat_elt" chain and the "original" links;
             * the only differences between the two.
             */
            flat_elt = BE_Array_Info(param)->flat_elt;
            original = BE_Array_Info(param)->original;
            *(BE_Array_Info(param)) =
                *(BE_Array_Info(BE_Array_Info(param)->original));
            BE_Array_Info(param)->flat_elt = flat_elt;
            BE_Array_Info(param)->original = original;

            debug(("Copied array_info from %x(%x) to %x(%x)\n",
                  BE_Array_Info(param)->original,
                  BE_Array_Info(BE_Array_Info(param)->original),
                  param, BE_Array_Info(param)));
        }

        decorate_array_elements(oper, param, side);
    }
    else if (param->type->kind == AST_disc_union_k)
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
            decorate_parameters(oper, arm->flat, side);
    else if (param->type->kind == AST_pipe_k)
    {
        decorate_array(oper,
            param->type->type_structure.pipe->be_info.pipe->flat, side, flags);
        decorate_array_elements(oper,
            param->type->type_structure.pipe->be_info.pipe->flat, side);
    }
    else if (param->type->kind == AST_pointer_k &&
             (BE_PI_Flags(param) & (BE_PTR_ARRAY | BE_ARRAYIFIED | BE_PTR2STR)))
    {
        /*
         * Decorate the flat_array field of the pointer-to-array gizmo so that
         * we will have A, B, Z, and allocation expressions later on.  We would
         * eventually like to suppress all local variable declaration during
         * this pass (except for the Z variable during string decoration,
         * which is used during pointed-at string marshalling).  Such
         * suppression would require nothing more than the creation of a new
         * decoration flag, whose presence would suppress the calls to
         * BE_declare_local_var() in decorate_array().
         */
        decorate_array(oper, BE_Ptr_Info(param)->flat_array, side, 0);
    }

#if 0
    else if (param->type->kind == AST_pointer_k && !BE_Is_SP_Pointer(param))
       decorate_parameters(oper, BE_Ptr_Info(param)->pointee, side);
#endif

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
    {
        /*
         * [in] open records must be allocated on the server side
         */
        if (side == BE_server_side && AST_IN_SET(param))
            BE_PI_Flags(param) |= BE_ALLOCATE;

        /*
         * [out] open records must be allocated on the client side
         */
        if (side == BE_client_side && AST_OUT_SET(param) &&
            (BE_PI_Flags(param) & BE_XMITCFMT))
            BE_PI_Flags(param) |= BE_ALLOCATE;
    }
}

/*
 * decorate_parameters
 *
 * Calls decorate_parameter for each parameter in the list passed.
 */
static void decorate_parameters
#ifdef PROTO
(
    AST_operation_n_t *oper,
    AST_parameter_n_t *params,
    BE_side_t side
)
#else
(oper, params, side)
    AST_operation_n_t *oper;
    AST_parameter_n_t *params;
    BE_side_t side;
#endif
{
    AST_parameter_n_t *param;

    for (param = params; param; param = param->next)
            BE_decorate_parameter(oper, param, side, 0);
}

/*
 * BE_decorate_operation
 *
 * Invokes BE_flatten_parameters() to create a flattened parameter list and
 * decorate_parameters() to decorate the list.  Invokes BE_create_blocks() and
 * BE_size_blocks() to group parameters for marshalling.
 */

void BE_decorate_operation
#ifdef PROTO
(
    AST_operation_n_t *oper,
    BE_side_t side
)
#else
(oper, side)
    AST_operation_n_t *oper;
    BE_side_t side;
#endif
{
    BE_param_blk_t *block;
    int alignment = RPC_MAX_ALIGNMENT;
    AST_parameter_n_t *param;
    long size_calc_alignment;

    oper->be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    BE_Flat_Params(oper) = NULL;
    oper->be_info.oper->flags = 0;
    oper->be_info.oper->local_vars = NULL;
    BE_Pointers(oper) = NULL;
    oper->be_info.oper->next_local_var = 0;

    BE_flatten_parameters(oper, side);

    /*
     * On the client side, decorate any pipes for the purposes of picking
     * up local variable declarations
     */
    if (side == BE_client_side)
    {
        /*
         * First clear the decorated and element_ptr_var_declared booleans
         * on all the pipe parameter types
         */
        for (param = BE_Flat_Params(oper); param; param = param->next)
            if (param->type->kind == AST_pipe_k)
            {
                BE_undec_piped_arrays(param->type->type_structure.pipe->be_info.pipe->flat);
            }

        for (param = BE_Flat_Params(oper); param; param = param->next)
            if (param->type->kind == AST_pipe_k)
                BE_decorate_parameter(oper, param, side, 0);
    }

    /* Set the alignment flags before blocking */
    for (param = BE_Flat_Params(oper); param; param = param->next)
    {
        if (param->type->kind == AST_pipe_k) continue;

        if ((side == BE_client_side && !AST_IN_SET(param)) ||
            (side == BE_server_side && !AST_OUT_SET(param))) continue;

        alignment = BE_set_alignment(param, alignment);
    }

    BE_Sends(oper) = BE_create_blocks(BE_Flat_Params(oper), side);
    oper->be_info.oper->vec_size = BE_max_vec_size(BE_Sends(oper));
    size_calc_alignment = 8;
    oper->be_info.oper->pkt_size = BE_sum_pkt_size(BE_Sends(oper),
        &size_calc_alignment);

    if (AST_HAS_IN_PIPES_SET(oper) && side == BE_client_side ||
        AST_HAS_OUT_PIPES_SET(oper) && side == BE_server_side)
    {
        /*
         * Make sure that an iovector and packet of sufficient size are declared
         * (7 bytes for alignment to 8, 7 bytes for synchronization to 8,
         * 3 bytes for alignment to 4, 4 bytes for count value
         */
        if (oper->be_info.oper->vec_size < 2)
            oper->be_info.oper->vec_size = 2;
        if (oper->be_info.oper->pkt_size < 21)
            oper->be_info.oper->pkt_size = 21;
    }

    BE_Recs(oper) = BE_create_recs(BE_Flat_Params(oper), side);

    /*
     * decorate the transmits (which are more complicated because of
     * iovector blocks and slots)
     */
    for (block = BE_Sends(oper); block; block = block->next)
    {
        decorate_parameters(oper, block->params, side);

        /*
         * Synchronize before marshalling the first non-pipe out
         */
        if (side == BE_server_side && AST_HAS_OUT_PIPES_SET(oper))
            BE_flag_sync(block->params, true);
        else BE_flag_sync(block->params, false);

        /*
         * Since the mp is used to compute the data length, force advancement
         * after each parameter is marshalled
         */
        BE_flag_advance_mp(block->params, side,
                           side == BE_client_side ? BE_in : BE_out,
                           true);
    }

#ifdef DEBUG_VERBOSE
    if (BE_dump_sends)
    {
        printf("\nsends dump : BE_decorate_operation\n");
        printf(  "----------------------------------\n");
        for (block = BE_Sends(oper); block; block = block->next)
        {
            printf("Block (%d slot%s):\n", block->vec_size,
                   block->vec_size != 1 ? "s" : "");
            traverse(block->params, 2);
        }
    }
#endif

    /*
     * decorate the receives
     */
    decorate_parameters(oper, BE_Recs(oper), side);

    /*
     * Special care must be taken after unmarshalling out pipes--We
     * don't know anything about alignment or the amount of data that
     * has gone by.
     */
    if (side == BE_client_side && AST_HAS_OUT_PIPES_SET(oper))
    {
        BE_flag_alignment(BE_Recs(oper), 1);
        size_calc_alignment = 1;
        BE_flag_check_buffer(BE_Recs(oper), 1+MIN_BUFF_SIZE,
            &size_calc_alignment);
    }
    else
    {
        BE_flag_alignment(BE_Recs(oper), RPC_MAX_ALIGNMENT);
        size_calc_alignment = 8;
        BE_flag_check_buffer(BE_Recs(oper), 0, &size_calc_alignment);
    }

    /*
     * If there are in pipes to unmarshall, make sure the mp gets
     * advanced after the last fixed-size in
     */
    if (side == BE_server_side && AST_HAS_IN_PIPES_SET(oper))
        BE_flag_advance_mp(BE_Recs(oper), side,
                           side == BE_client_side ? BE_out : BE_in, true);
    else BE_flag_advance_mp(BE_Recs(oper), side,
                           side == BE_client_side ? BE_out : BE_in, false);

#ifdef DEBUG_VERBOSE
    if (BE_dump_recs)
    {
        printf("\nrecs dump : BE_decorate_operation\n");
        printf(  "---------------------------------\n");
        traverse(BE_Recs(oper), 2);
    }
#endif

    if (oper->result->type->kind != AST_void_k)
        BE_declare_surrogates(oper, oper->result);

    if (side == BE_server_side) BE_declare_server_surrogates(oper);

    BE_set_operation_flags(oper);
}
