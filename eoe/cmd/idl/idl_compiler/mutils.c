/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: mutils.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:40:31  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:43  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:49:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:00  zeliff]
 * 
 * Revision 1.1.2.5  1992/05/13  19:09:05  harrow
 * 	Add option to enable generation of repas/xmit as free calls for
 * 	[in]-only parameters in addtion to [out] or [in,out] types.
 * 	[1992/05/13  11:38:36  harrow]
 * 
 * Revision 1.1.2.4  1992/05/08  20:04:09  harrow
 * 	     Accomodate the special case of an array parameter being
 * 	     passed to a transmit_as routine.  Normally, an & is always
 * 	     added but for an array parameters this must be suppressed
 * 	     to obtain the correct signiture for the transmit_as routine.
 * 	     [1992/05/05  14:04:07  harrow]
 * 
 * Revision 1.1.2.3  1992/04/06  19:49:31  harrow
 * 	          Fix to context handle rundown race condition, no longer requires
 * 	          use of locking version of context_to_wire routine.
 * 	          [1992/04/01  18:40:12  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:12:16  harrow
 * 	               Utilize the new routine that maintains reference counts on
 * 	               active context handles.
 * 	               [1992/03/19  16:45:20  harrow]
 * 
 * 	               Modify BE_scalar_type_suffix returns "v1_enum" when [v1_enum]
 * 	               this corrects the NDR of a [v1_enum] to be 4 bytes instead of 2.
 * 	               [1992/03/19  15:36:46  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:37  devrcs
 * 	               Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989,1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      mutils.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Utilities for the marshalling modules
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dflags.h>
#include <dutils.h>
#include <clihandl.h>
#include <cstubgen.h>
#include <mutils.h>
#include <cspell.h>
#include <nodesupp.h>

/*
 * BE_scalar_type_suffix
 */
char *BE_scalar_type_suffix
#ifdef PROTO
(
    AST_type_n_t *type
)
#else
(type)
    AST_type_n_t *type;
#endif
{
    switch(type->kind)
    {
        case AST_boolean_k:
            return "boolean";
        case AST_byte_k:
            return "byte";
        case AST_character_k:
            return "char";
        case AST_enum_k:
            return AST_V1_ENUM_SET(type) ? "v1_enum" : "enum";
        case AST_small_integer_k:
            return "small_int";
        case AST_short_integer_k:
            return "short_int";
        case AST_long_integer_k:
            return "long_int";
        case AST_hyper_integer_k:
            return "hyper_int";
        case AST_small_unsigned_k:
            return "usmall_int";
        case AST_short_unsigned_k:
            return "ushort_int";
        case AST_long_unsigned_k:
            return "ulong_int";
        case AST_hyper_unsigned_k:
            return "uhyper_int";
        case AST_short_float_k:
            return "short_float";
        case AST_long_float_k:
            return "long_float";
        default:
            INTERNAL_ERROR("Unknown type kind");
            return "?";
    }
}

/*
 * BE_spell_pointer_init
 *
 * Initialize the server-stub surrogates for [ref] pointer parameters
 */
void BE_spell_pointer_init
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper
)
#else
(fid, oper)
    FILE *fid;
    AST_operation_n_t *oper;
#endif
{
    BE_ptr_init_t *ptr_init;

    if (BE_Pointers(oper))
    {
        fprintf(fid, "/* initialize pointer surrogates */\n");

        for (ptr_init = BE_Pointers(oper); ptr_init; ptr_init = ptr_init->next)
            if (ptr_init->heap)
            {
                fprintf(fid, "%s = ", BE_get_name(ptr_init->pointer_name));
                CSPELL_cast_exp(fid, ptr_init->pointer_type);
                fprintf(fid, "rpc_ss_mem_alloc(&mem_handle, sizeof");
                CSPELL_cast_exp(fid, ptr_init->pointee_type);
                fprintf(fid, ");\n");
            }
            else
            {
                fprintf(fid, "%s = &%s;\n",
                    BE_get_name(ptr_init->pointer_name),
                    BE_get_name(ptr_init->pointee_name));
            }

        fprintf(fid, "\n");
    }
}

/*
 * BE_spell_to_xmits
 *
 * Emit all the _to_xmit calls associated with a particular parameter
 */
void BE_spell_to_xmits
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if (call->type == BE_call_xmit_as)
        {
            /* 
             * Suppress the & for arrays.  & on an array parameter causes
             * an extra level of indirection.
             */
            char address_of = '&';
            if (call->call.xmit_as.native_type->kind == AST_array_k)
                address_of = ' ';

           spell_name(fid, call->call.xmit_as.native_type->name);
                
            fprintf(fid, "_to_xmit(%c%s, &%s);\n",
                address_of,
                BE_get_name(call->call.xmit_as.native_name),
                BE_get_name(call->call.xmit_as.pxmit_name));
        }
}

/*
 * BE_spell_free_xmits
 *
 * Emit all the _free_xmit calls associated with a particular parameter
 */
void BE_spell_free_xmits
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_call_xmit_as)
        {
            spell_name(fid, call->call.xmit_as.native_type->name);
            fprintf(fid, "_free_xmit(%s);\n",
                    BE_get_name(call->call.xmit_as.pxmit_name));
        }
}

/*
 * BE_spell_assign_xmits
 *
 *  Before unmarshalling, make the pxmit_name locals point at the xmit_name
 *  locals.  If the xmit_name is NULL, then there is no stack surrogate to
 *  point at because the transmitted type is conformant and is dynamically
 *  allocated so don't do the initialization.
 */
void BE_spell_assign_xmits
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if ((call->type == BE_call_xmit_as) &&
            (call->call.xmit_as.xmit_name != NAMETABLE_NIL_ID))
        {
            fprintf(fid, "%s = &%s; %s\n",
                    BE_get_name(call->call.xmit_as.pxmit_name),
                    BE_get_name(call->call.xmit_as.xmit_name),
                    "/* transmit_as unmarshalling surrogate */");
        }
}

/*
 * BE_spell_from_xmits
 *
 * Emit all the _from_xmit calls associated with a particular parameter
 */
void BE_spell_from_xmits
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_call_xmit_as)
        {
            /* 
             * Suppress the & for arrays.  & on an array parameter causes
             * an extra level of indirection.
             */
            char address_of = '&';
            if (call->call.xmit_as.native_type->kind == AST_array_k)
                address_of = ' ';

            spell_name(fid, call->call.xmit_as.native_type->name);
            fprintf(fid, "_from_xmit(%s, %c%s);\n",
                BE_get_name(call->call.xmit_as.pxmit_name),
                address_of,
                BE_get_name(call->call.xmit_as.native_name));
        }
}

/*
 * BE_spell_type_frees
 *
 * Server side only: emit all the type_free calls associated with the
 * parameter list passed
 */
void BE_spell_type_frees
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *params,
    boolean in_only
)
#else
(fid, params, in_only)
    FILE *fid;
    AST_parameter_n_t *params;
    boolean in_only;
#endif
{
    AST_parameter_n_t *param;
    BE_call_rec_t *call;

    for (param = params; param; param = param->next)
        for (call = BE_Calls_After(param); call; call = call->next)
            if (call->type == BE_call_xmit_as)
            {
                if (in_only)
                {
                  /* 
                   * If processing [in]-onlys then don't call free routine
                   * if [out] is present.
                   */
                  if (AST_OUT_SET(param)) continue;
                }
                spell_name(fid, call->call.xmit_as.native_type->name);
                fprintf(fid, "_free_inst(&%s);\n",
                        BE_get_name(call->call.xmit_as.native_name));
            }
}

/*
 * BE_spell_from_locals
 *
 * Emit all the _from_local calls associated with a particular parameter
 */
void BE_spell_from_locals
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if (call->type == BE_call_rep_as)
        {
            spell_name(fid, call->call.rep_as.net_type->name);
            fprintf(fid, "_from_local(&%s, &%s);\n",
                BE_get_name(call->call.rep_as.local_name),
                BE_get_name(call->call.rep_as.pnet_name));
        }
}

/*
 * BE_spell_free_nets
 *
 * Emit all the _free calls associated with a particular parameter
 */
void BE_spell_free_nets
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_call_rep_as)
        {
            spell_name(fid, call->call.rep_as.net_type->name);
            fprintf(fid, "_free_inst(%s);\n",
                    BE_get_name(call->call.rep_as.pnet_name));
        }
}

/*
 * BE_spell_assign_nets
 *
 *  Before unmarshalling, make the pnet_name locals point at the net_name
 *  locals. If the net_name is NULL, then there is no stack surrogate to point
 *  at because the transmitted type is conformant and is dynamically allocated
 *  so don't do the initialization.
 */
void BE_spell_assign_nets

#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if ((call->type == BE_call_rep_as) &&
            (call->call.rep_as.net_name != NAMETABLE_NIL_ID))
        {
            fprintf(fid, "%s = &%s; %s\n",
                    BE_get_name(call->call.rep_as.pnet_name),
                    BE_get_name(call->call.rep_as.net_name),
                    "/* represent_as unmarshalling surrogate */");
       }
}

/*
 * BE_spell_to_locals
 *
 * Emit all the _to_local calls associated with a particular parameter
 */
void BE_spell_to_locals
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_call_rep_as)
        {
            spell_name(fid, call->call.rep_as.net_type->name);
            fprintf(fid, "_to_local(%s, &%s);\n",
                BE_get_name(call->call.rep_as.pnet_name),
                BE_get_name(call->call.rep_as.local_name));
        }
}

/*
 * BE_spell_free_locals
 *
 * Server side only: emit all the _free_local calls associated with the
 * parameter list passed
 */
void BE_spell_free_locals
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *params,
    boolean in_only
)
#else
(fid, params, in_only)
    FILE *fid;
    AST_parameter_n_t *params;
    boolean in_only;
#endif
{
    AST_parameter_n_t *param;
    BE_call_rec_t *call;

    for (param = params; param; param = param->next)
        for (call = BE_Calls_After(param); call; call = call->next)
            if (call->type == BE_call_rep_as)
            {
                if (in_only)
                {
                  /* 
                   * If processing [in]-onlys then don't do parameters with
                   * [out] specified.
                   */
                  if (AST_OUT_SET(param)) continue;
                }
                spell_name(fid, call->call.rep_as.net_type->name);
                fprintf(fid, "_free_local(&%s);\n",
                        BE_get_name(call->call.rep_as.local_name));
            }
}

/*
 * BE_spell_ctx_to_wires
 *
 * Emit calls to convert void *s into ndr_context_handles for a parameter
 */
void BE_spell_ctx_to_wires
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_call_side_t call_side
)
#else
(fid, param, call_side)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_call_side_t call_side;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if (call->type == BE_call_ctx_handle)
        {
            if (call_side == BE_caller)
                fprintf(fid, "rpc_ss_er_ctx_to_wire(%s, &%s, (handle_t)%c%s, %s, &st);\n",
                    BE_get_name(call->call.ctx_handle.native_name),
                    BE_get_name(call->call.ctx_handle.wire_name),
                    BE_handle_info.deref_assoc,
                    BE_handle_info.assoc_name,
                    AST_OUT_SET(param) ? "ndr_true" : "ndr_false");
            else
            {
                fprintf(fid,
                         "rpc_ss_ee_ctx_to_wire(%s, &%s, (handle_t)h, ",
                    BE_get_name(call->call.ctx_handle.native_name),
                    BE_get_name(call->call.ctx_handle.wire_name));
                if (call->call.ctx_handle.rundown)
                {
                    /*
                     * Emit the name of the rundown routine for this type, or
                     * for the pointee if the parameter was passed by reference
                     */
                    spell_name(fid, call->call.ctx_handle.native_type->name);
                    fprintf(fid, "_rundown, ");
                }
                else fprintf(fid, "(void (*)())NULL, ");
                fprintf(fid, "%s, &st);\n",
                        AST_IN_SET(param) ? "ndr_true" : "ndr_false");
            }
            fprintf(fid, "if (st != error_status_ok) goto closedown;\n");
        }
}

/*
 * BE_spell_ctx_from_wires
 *
 * Emit calls to convert ndr_context_handles into void *s for a parameter
 */
void BE_spell_ctx_from_wires
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_call_side_t call_side
)
#else
(fid, param, call_side)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_call_side_t call_side;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_call_ctx_handle)
        {
            if (call_side == BE_caller)
                fprintf(fid,
                    "rpc_ss_er_ctx_from_wire(&%s, &%s, (handle_t)%c%s, %s, &st);\n",
                    BE_get_name(call->call.ctx_handle.wire_name),
                    BE_get_name(call->call.ctx_handle.native_name),
                    BE_handle_info.deref_assoc,
                    BE_handle_info.assoc_name,
                    AST_IN_SET(param) ? "ndr_true" : "ndr_false");
            else
                fprintf(fid, "rpc_ss_ee_ctx_from_wire(&%s, &%s, &st);\n",
                    BE_get_name(call->call.ctx_handle.wire_name),
                    BE_get_name(call->call.ctx_handle.native_name));
            fprintf(fid, "if (st != error_status_ok) goto closedown;\n");
        }
}

/*
 * BE_spell_ptrs_to_array_init
 *
 * Initialize the ptr to array local/surrogate for [ptr] array parameters
 */
void BE_spell_ptrs_to_array_init
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_Before(param); call; call = call->next)
        if (call->type == BE_init_ptr_to_array)
        {
            fprintf(fid, "%s = ",
                BE_get_name(call->call.ptr_to_array.ptr_name));
            CSPELL_cast_exp (fid, call->call.ptr_to_array.ptr_type);
            fprintf(fid, "%s;\n",
                BE_get_name(call->call.ptr_to_array.param_name));
        }
}

/*
 * BE_spell_ptrs_to_conf_init
 *
 * Initialize the ptr to array local/surrogate for [ptr] array parameters
 */
void BE_spell_ptrs_to_conf_init
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    BE_call_rec_t *call;

    for (call = BE_Calls_After(param); call; call = call->next)
        if (call->type == BE_init_ptr_to_conf)
        {
            fprintf(fid, "%s = ",
                BE_get_name(call->call.ptr_to_conf.param_name));
            CSPELL_cast_exp (fid, call->call.ptr_to_conf.param_type);
            fprintf(fid, "%s;\n",
                BE_get_name(call->call.ptr_to_conf.pointee_name));
        }
}

/*
 * BE_spell_alloc_open_outs
 *
 * Emit code to allocate [out]-only conformant arrays on the server side
 * before the call to the manager routine.  This routine will have to be
 * updated if any other [out]-only conformant types come into existence.
 * (Such as [out]-only arrayified ref pointers.)
 */
void BE_spell_alloc_open_outs
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *oper
)
#else
(fid, oper)
    FILE *fid;
    AST_operation_n_t *oper;
#endif
{
    AST_parameter_n_t *param;
    boolean first_param = true, first_index;
    int i;

    for (param = oper->parameters; param; param = param->next)
        if (AST_OUT_SET(param) && !AST_IN_SET(param)
        && ((param->type->kind == AST_array_k
            && AST_CONFORMANT_SET(param->type))
        || (BE_Is_Arrayified(param,param->type)
            && AST_REF_SET(param))))
        {
            if (first_param)
            {
                fprintf(fid,
                    "/* allocate [out]-only conformant parameters */\n");
                first_param = false;
            }
            fprintf(fid, "%s = ",
                    BE_get_name(param->be_info.param->flat->name));
            CSPELL_cast_exp(fid,
                BE_Array_Info(param->be_info.param->flat)->alloc_type);
            fprintf(fid, "rpc_ss_mem_alloc(&mem_handle, ");

            /*
             * Emit an expression for the number of elements in the array
             */
            if (!AST_SMALL_SET(param))
            {
                /*
                 * 2.0 NDR: form the product of the Z expressions
                 */
                first_index = true;
                for (i = 0; i < param->be_info.param->flat->type->type_structure.array->index_count;
                     i++)
                {
                    if (!first_index)
                    {
                        fprintf(fid, "*");
                    }
                    fprintf(fid, "%s",
                        BE_Array_Info(param->be_info.param->flat)->Z_exps[i]);
                    first_index = false;
                }
            }
            else fprintf(fid, "%s",
                BE_Array_Info(param->be_info.param->flat)->size_exp);

            fprintf(fid, "*sizeof");
            CSPELL_cast_exp(fid,
                param->be_info.param->flat->type->type_structure.array->element_type);
            fprintf(fid, ");\n");

            if (BE_any_ref_pointers(
                BE_Array_Info(param->be_info.param->flat)->flat_elt))
            {
                /*
                 * Allocation is done with the ref_pass flattened parameter
                 */
                if (param->type->type_structure.array->element_type->kind ==
                    AST_pointer_k)
                    BE_spell_alloc_ref_pointer(fid,
                       param->be_info.param->flat->next, BE_arp_after, false);
                else
                    BE_spell_alloc_ref_pointer(fid,
                       param->be_info.param->flat->next->next, BE_arp_after,
                       false);
            }
        }
}

/*
 * BE_spell_alloc_ref_pointer
 *
 * Emit code to pre-allocate a ref pointer on the server side.  Ref pointers
 * contained in fixed parameters are allocated before unmarshalling, those
 * contained in [in...] conformant parameters are allocated during
 * unmarshalling, and those contained in [out] only conformant parameters,
 * i.e. arrays, are allocated after unmarshalling.
 */
void BE_spell_alloc_ref_pointer
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_arp_flags_t when,       /* BE_arp_before, BE_arp_during, BE_arp_after */
    boolean in_node            /* Called in the context of a node routine */
)
#else
(fid, param, when, in_node)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_arp_flags_t when;
    boolean in_node;
#endif
{
    boolean first_index;
    int i, index_count;

    /*
     * Allocate pointers when the non-defer flattened parameter is encountered.
     * Conformant pointers are always allocated in node marshalling code.
     */
    if (param->type->kind == AST_pointer_k && AST_REF_SET(param) &&
        !(BE_PI_Flags(param) & BE_DEFER) &&
        !AST_CONFORMANT_SET(param->type->type_structure.pointer->pointee_type))
    {
        CSPELL_pa_routine_name(fid,
            param->type->type_structure.pointer->pointee_type,
            BE_callee, BE_unmarshalling_k, false);
        fprintf(fid, "(&%s, rpc_ss_alloc_ref_node_k, ",
                BE_get_name(param->name));
        if (in_node) fprintf(fid, "p_unmar_params);\n");
        else fprintf(fid, "&unmar_params);\n");
    }

    /*
     * Allocate arrays when the "ref pass" part of the flattened array is
     * encountered.
     */
    else if (param->type->kind == AST_array_k &&
             (BE_PI_Flags(param) & BE_REF_PASS) &&
             BE_any_ref_pointers(BE_Array_Info(param)->flat_elt))
    {
        /*
         * Conformant arrays are allocated during or after unmarshalling
         */
        if (when == BE_arp_before && AST_CONFORMANT_SET(param->type)) return;

        index_count = param->type->type_structure.array->index_count;
        /*
         * Don't use the first_element_exp here because it may be based
         * on variant A values
         */
        fprintf(fid, "%s = &%s",
                BE_get_name(BE_Array_Info(param)->element_ptr_var),
                BE_get_name(param->name));
        for (i = 0; i < index_count; i++) fprintf(fid, "[0]");
        fprintf(fid, ";\n");

        fprintf(fid, "for (%s = ",
            BE_get_name(BE_Array_Info(param)->index_var));

        if (when == BE_arp_before)
            fprintf(fid, "%d", BE_Array_Info(param)->num_elts);
        else
        {
            /*
             * BE_arp_during or BE_arp_after--conformant arrays
             */
            if (!AST_SMALL_SET(param))
            {
                /*
                 * 2.0 NDR: form the product of the Z variables or
                 *          expressions
                 */
                first_index = true;
                for (i = 0; i < index_count; i++)
                {
                    if (!first_index)
                    {
                        fprintf(fid, "*");
                    }
                    if (when == BE_arp_during)
                        fprintf(fid, "%s",
                            BE_get_name(BE_Array_Info(param)->Z[i]));
                    else /* when == BE_arp_after */
                        fprintf(fid, "%s",
                            BE_Array_Info(param)->Z_exps[i]);
                    first_index = false;
                }
            }
            else fprintf(fid, "%s",
                BE_Array_Info(param->be_info.param->flat)->size_exp);
        }

        fprintf(fid, "; %s; %s--)\n{\n",
            BE_get_name(BE_Array_Info(param)->index_var),
            BE_get_name(BE_Array_Info(param)->index_var));

        BE_spell_alloc_ref_pointers(fid, BE_Array_Info(param)->flat_elt,
                                    when, in_node);

        fprintf(fid, "%s++;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var));
        fprintf(fid, "}\n");
    }
}

/*
 * BE_spell_alloc_ref_pointers
 *
 * Server side: Iterate over a list of parameters, allocating [ref] pointers
 */
void BE_spell_alloc_ref_pointers
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *params,
    BE_arp_flags_t when,       /* BE_arp_before, BE_arp_during, BE_arp_after */
    boolean in_node            /* Called in the context of a node routine */
)
#else
(fid, params, when, in_node)
    FILE *fid;
    AST_parameter_n_t *params;
    BE_arp_flags_t when;
    boolean in_node;
#endif
{
    AST_parameter_n_t *param = params;

    while (param)
    {
        /*
         * Skip conformant structures, whose ref pointers must be allocated
         * during unmarshalling
         */
        if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        {
            do param = param->next;
            while (!(BE_PI_Flags(param) & BE_LAST_FIELD));
        }
        else BE_spell_alloc_ref_pointer(fid, param, when, in_node);

        param = param->next;
    }
}

/*
 * BE_ool_in_flattened_list
 *
 * Returns TRUE if a flattened list includes any [out_of_line] parameters
 */
boolean BE_ool_in_flattened_list
#ifdef PROTO
(
    AST_parameter_n_t *p_flat
)
#else
( p_flat )
    AST_parameter_n_t *p_flat;
#endif
{
    AST_parameter_n_t *pp;
    BE_arm_t *arm;

    for (pp = p_flat; pp != NULL; pp = pp->next)
    {
        if ( BE_PI_Flags(pp) & BE_OOL )
        {
            return true;
        }
        if ( pp->type->kind == AST_disc_union_k )
        {
            for (arm = BE_Du_Info(pp)->arms; arm != NULL; arm = arm->next)
            {
                if ( BE_ool_in_flattened_list(arm->flat) )
                    return true;
            }
        }
    }
    return false;
}
