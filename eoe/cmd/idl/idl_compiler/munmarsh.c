/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: munmarsh.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.5  1993/03/03  23:07:40  weisman
 * 	Install Al Sanders' and Carl Burnette's fixes for OT 3783 and 7412
 * 	[1993/03/03  22:54:05  weisman]
 *
 * Revision 1.1.5.2  1993/03/02  18:20:11  sanders
 * 	In BE_spell_varying_unmarshall, don't rely on BE_ARR_OF_STR
 * 	flag to determine if parameter is array of strings.  Flag
 * 	won't be set if out-of-line
 * 
 * Revision 1.1.4.4  1993/01/03  21:40:26  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:35  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  18:49:04  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:51  zeliff]
 * 
 * Revision 1.1.4.2  1992/11/16  19:42:21  hinxman
 * 	Fix OT5214 Datagram packet pool is corrupted by stub freeing non-existent
 * 	                buffer
 * 	[1992/11/11  21:37:53  hinxman]
 * 
 * Revision 1.1.2.2  1992/03/24  14:11:52  harrow
 * 	Add support to enable status code mapping. This
 * 	is selectable per-platform via stubbase.h.
 * 	[1992/03/19  14:45:34  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:35  devrcs
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
**  NAME:
**
**      munmarsh.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Emission routines for parameter unmarshalling
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <dflags.h>
#include <cspell.h>
#include <mutils.h>
#include <marshall.h>
#include <nodesupp.h>
#include <oolrtns.h>
#include <munmarsh.h>
#include <cspeldcl.h>

static void align_mp
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    char *op,
    AST_type_n_t *type,
    BE_mflags_t maintain_op
)
#else
(fid, mp, op, type, maintain_op)
    FILE *fid;
    char *mp;
    char *op;
    AST_type_n_t *type;
    BE_mflags_t maintain_op;
#endif
{
    if ((type)->alignment_size > 1)
    {
        if (maintain_op) fprintf(fid, "rpc_align_mop(%s, %s, %d);\n",
                                               mp, op, (type)->alignment_size);
        else fprintf(fid, "rpc_align_mp(%s, %d);\n", mp, (type)->alignment_size);
    }
}

static void unmarshall_scalar
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t convert,
    char *mp,
    char *drep,
    AST_type_n_t *type,
    char *name
)
#else
(fid, convert, mp, drep, type, name)
    FILE *fid;
    BE_mflags_t convert;
    char *mp;
    char *drep;
    AST_type_n_t *type;
    char *name;
#endif
{
    if (convert) fprintf(fid, "rpc_convert_%s(%s, ndr_g_local_drep, %s, %s);\n",
                               BE_scalar_type_suffix(type), drep, mp, name);
    else fprintf(fid, "rpc_unmarshall_%s(%s, %s);\n",
                               BE_scalar_type_suffix(type), mp, name);

    while (type->defined_as != NULL) type = type->defined_as;
    if (type->name == NAMETABLE_add_id("error_status_t"))
    {
        fprintf(fid, "#ifdef IDL_ENABLE_STATUS_MAPPING\n");
        fprintf(fid, "rpc_ss_map_dce_to_local_status(&(%s));\n#endif\n",name);
    }
}

static void advance_mp
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    char *op,
    AST_type_n_t *type,
    BE_mflags_t maintain_op
)
#else
(fid, mp, op, type, maintain_op)
    FILE *fid;
    char *mp;
    char *op;
    AST_type_n_t *type;
    BE_mflags_t maintain_op;
#endif
{
    if (maintain_op) fprintf(fid, "rpc_advance_mop(%s, %s, %d);\n", mp, op,
                                   (type)->ndr_size);
    else fprintf(fid, "rpc_advance_mp(%s, %d);\n", mp, (type)->ndr_size);
}

#ifdef NO_EXCEPTIONS
static void check_status
#ifdef PROTO
(
    FILE *fid,
    char *st,
    char *bad_st
)
#else
(fid, st, bad_st)
    FILE *fid;
    char *st;
    char *bad_st;
#endif
{
    fprintf(fid, "if (%s != error_status_ok) %s\n", st, bad_st);
}
#endif

static void check_buffer
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    char *elt,
    char *st,
    char *call_h,
    char *bad_st,
    BE_mflags_t buff_bool
)
#else
(fid, mp, elt, st, call_h, bad_st, buff_bool)
    FILE *fid;
    char *mp;
    char *elt;
    char *st;
    char *call_h;
    char *bad_st;
    BE_mflags_t buff_bool;
#endif
{
    fprintf(fid, "if ((byte_p_t)%s - %s->data_addr >= %s->data_len)\n{\n", mp,
            elt,  elt);
    fprintf(fid, "rpc_ss_new_recv_buff(%s, (rpc_call_handle_t)%s, &(%s), &%s);\n",
                    elt, call_h, mp, st);
#ifdef NO_EXCEPTIONS
    check_status(fid, st, bad_st);
#endif
    if (buff_bool) fprintf(fid, "buffer_changed = ndr_true;\n");
    fprintf(fid, "}\n");
}

void BE_u_start_buff
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags
)
#else
(fid, flags)
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if ( (flags & BE_M_PIPE) && (flags & BE_M_CALLEE) )
    {
        fprintf( fid, "unmar_params.mp = *(rpc_p_pipe_state->p_mp);\n");
        fprintf( fid, "unmar_params.op = *(rpc_p_pipe_state->p_op);\n");
    }
    else
    {
        fprintf(fid, "unmar_params.mp = mp;\n");
        fprintf(fid, "unmar_params.op = op;\n");
    }
}

void BE_u_end_buff
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags
)
#else
(fid, flags)
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if ( (flags & BE_M_PIPE) && (flags & BE_M_CALLEE) )
    {
        fprintf( fid, "*(rpc_p_pipe_state->p_mp) = unmar_params.mp;\n");
        fprintf( fid, "*(rpc_p_pipe_state->p_op) = unmar_params.op;\n");
    }
    else
    {
        fprintf(fid, "mp = unmar_params.mp;\n");
        fprintf(fid, "op = unmar_params.op;\n");
    }
}

/*
 * BE_spell_start_unmarshall
 */
void BE_spell_start_unmarshall
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags,   /*  BE_M_MAINTAIN_OP */
    BE_mn_t *mn
)
#else
(fid, flags, mn)
    FILE *fid;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    if (flags & BE_M_MAINTAIN_OP)
        fprintf(fid, "rpc_init_op(%s);\n", mn->op);
}

/*
 * BE_spell_end_unmarshall
 */
void BE_spell_end_unmarshall
#ifdef PROTO
(
    FILE *fid,
    BE_mn_t *mn
)
#else
(fid, mn)
    FILE *fid;
    BE_mn_t *mn;
#endif
{
    fprintf(fid,
"if (%s->buff_dealloc && %s->data_len!=0)(*%s->buff_dealloc)(%s->buff_addr);\n",
                 mn->elt, mn->elt, mn->elt, mn->elt);
}

/*
 * BE_u_match_routine_mode
 *
 * Switch routine mode on or off, as required
 */
void BE_u_match_routine_mode
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags,
    boolean routine_mode_required,
    boolean *p_routine_mode
)
#else
(fid, flags, routine_mode_required, p_routine_mode)
    FILE *fid;
    BE_mflags_t flags;
    boolean routine_mode_required;
    boolean *p_routine_mode;
#endif
{
    if ( (*p_routine_mode != routine_mode_required) && !(flags & BE_M_NODE) )
    {
        if (routine_mode_required)
            BE_u_start_buff( fid, flags );
        else
            BE_u_end_buff( fid, flags );
        *p_routine_mode = routine_mode_required;
    }
}

/*
 * BE_spell_scalar_unmarshall
 */
void BE_spell_scalar_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,          /*  BE_M_ALIGN_MP
                                 * |BE_M_MAINTAIN_OP
                                 * |BE_M_CHECK_BUFFER
                                 * |BE_M_CONVERT
                                 * |BE_M_BUFF_BOOL
                                 */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    if (flags & BE_M_ALIGN_MP)
        align_mp(fid, mn->mp, mn->op, param->type, (flags & BE_M_MAINTAIN_OP));
    if (flags & BE_M_CHECK_BUFFER)
        check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                     flags & BE_M_BUFF_BOOL);
    unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep, param->type,
                         BE_get_name(param->name));
    if (flags & BE_M_ADVANCE_MP)
        advance_mp(fid, mn->mp, mn->op, param->type, flags & BE_M_MAINTAIN_OP);
}

/*
 * BE_spell_null_unmarshall
 */
void BE_spell_null_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,          /*  BE_M_ALIGN_MP
                                 * |BE_M_MAINTAIN_OP
                                 * |BE_M_CHECK_BUFFER
                                 * |BE_M_CONVERT
                                 * |BE_M_BUFF_BOOL
                                 */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    if (flags & BE_M_ALIGN_MP)
    {
        align_mp(fid, mn->mp, mn->op, param->type, (flags & BE_M_MAINTAIN_OP));
        if (flags & BE_M_CHECK_BUFFER)
            check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                     flags & BE_M_BUFF_BOOL);
    }
}

/*
 * BE_spell_conformant_unmarshall
 *
 * Unmarshall the conformant part of the header for an open array or open
 * record parameter
 */
void BE_spell_conformant_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags, /*
                        *  BE_M_ALIGN_MP
                        * |BE_M_MAINTAIN_OP
                        * |BE_M_CHECK_BUFFER
                        * |BE_M_CONVERT
                        */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    int i;

    if (flags & BE_M_ALIGN_MP)
        align_mp(fid, mn->mp, mn->op,
                 AST_SMALL_SET(param) ? BE_ushort_int_p : BE_ulong_int_p,
                 flags & BE_M_MAINTAIN_OP);

    if (!AST_SMALL_SET(param))
    {
        for (i = 0; i < param->type->type_structure.array->index_count; i++)
        {
            if (flags & BE_M_CHECK_BUFFER)
                check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h,
                             mn->bad_st, flags & BE_M_BUFF_BOOL);

            unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                              BE_ulong_int_p,
                              BE_get_name(BE_Array_Info(param)->Z[i]));
            advance_mp(fid, mn->mp, mn->op, BE_ulong_int_p,
                       flags & BE_M_MAINTAIN_OP);

        }
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->size_var),
                BE_Array_Info(param)->size_exp);
    }
    else
    {
        if (flags & BE_M_CHECK_BUFFER)
            check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                         flags & BE_M_BUFF_BOOL);

        unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                          BE_ushort_int_p,
                          BE_get_name(BE_Array_Info(param)->size_var));
        advance_mp(fid, mn->mp, mn->op, BE_ushort_int_p,
                   flags & BE_M_MAINTAIN_OP);
    }

    /*
     * The count for a conformant, non-varying array must be stuffed in
     * a loop controlling count variable here rather than later
     * in BE_spell_varying_unmarshall
     */
    if (!AST_VARYING_SET(param))
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);
}

/*
 * spell_varying_unmarshall
 *
 * Unmarshall the varying part of the header for an open array or open record
 * parameter
 */
void spell_varying_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags, /*
                        *  BE_M_ALIGN_MP
                        * |BE_M_MAINTAIN_OP
                        * |BE_M_CHECK_BUFFER
                        * |BE_M_CONVERT
                        */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    int i, index_count;
    AST_type_n_t *elt_type;

    /*
     * if this is the header for an object to be unmarshalled by a
     * call to an out_of_line routine defer unmarshalling the varying
     * info to the body of that routine
     */
    if (BE_PI_Flags(param) & BE_OOL_HEADER)
        return;

    /*
     * If we are generating an unmarshalling routine for a pointed-at or
     * out_of_line array, the unmarshalling of the A,B values needs to be
     * conditional since the routine can also be called to unmarshall a
     * non-varying instance of the array type.
     */
    if ((flags & BE_M_NODE) && !(BE_PI_Flags(param) & BE_FIELD))
        fprintf(fid, "if (NIDL_varying)\n{\n");

    /*
     * if this was also conformant then we've already aligned the header
     */
    if ((flags & BE_M_ALIGN_MP)
    && (!AST_CONFORMANT_SET(param->type) || (BE_PI_Flags(param)& BE_IN_ORECORD)))
        align_mp(fid, mn->mp, mn->op,
                 AST_SMALL_SET(param) ? BE_ushort_int_p : BE_ulong_int_p,
                 flags & BE_M_MAINTAIN_OP);

    if (!AST_SMALL_SET(param))
    {
        index_count = param->type->type_structure.array->index_count;
	/*
	 * If parameter represents an array of strings, decrement the index
	 * count, so we don't wind up with an extra set of loop variables.
	 * We can't rely on the BE_ARR_OF_STR flag bit here, since it may
	 * not have been set if this is an out of line element
	 */
	for (elt_type = param->type->type_structure.array->element_type;
		elt_type->kind == AST_array_k;
		elt_type = elt_type->type_structure.array->element_type) {
		    if (AST_STRING_SET(elt_type)) {
			index_count--;
		    }
	}


        for (i = 0; i < index_count; i++)
        {
            if (flags & BE_M_CHECK_BUFFER)
                check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h,
                             mn->bad_st, flags & BE_M_BUFF_BOOL);

            unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                              BE_ulong_int_p,
                              BE_get_name(BE_Array_Info(param)->A[i]));
            advance_mp(fid, mn->mp, mn->op, BE_ulong_int_p,
                       flags & BE_M_MAINTAIN_OP);

            if (flags & BE_M_CHECK_BUFFER)
                check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h,
                             mn->bad_st, flags & BE_M_BUFF_BOOL);

            unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                              BE_ulong_int_p,
                              BE_get_name(BE_Array_Info(param)->B[i]));
            advance_mp(fid, mn->mp, mn->op,
                       BE_ulong_int_p, flags & BE_M_MAINTAIN_OP);
        }

        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);
    }
    else
    {
        if (flags & BE_M_CHECK_BUFFER)
            check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                         flags & BE_M_BUFF_BOOL);

        unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                          BE_ushort_int_p,
                          BE_get_name(BE_Array_Info(param)->count_var));
        advance_mp(fid, mn->mp, mn->op, BE_ushort_int_p,
                   flags & BE_M_MAINTAIN_OP);
    }

    /*
     * Raise an exception if the number of elements to be unmarshalled is
     * greater than the amount of room in the array
     */
    if (!(BE_PI_Flags(param) & BE_STRING0))
        fprintf(fid, "if (%s > %s) RAISE(rpc_x_invalid_bound);\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                AST_CONFORMANT_SET(param->type) ?
                    BE_get_name(BE_Array_Info(param)->size_var) :
                    BE_Array_Info(param)->size_exp);

    /*
     * If we're emitting an ool unmarshalling routine for a v1_stringifiable
     * array, emit conditional code to add one to the count variable.
     */
    if ((flags & BE_M_NODE) && (flags & BE_M_OOL_RTN)
        &&  !(BE_PI_Flags(param) & BE_FIELD))
    {
        boolean has_pointers;
        boolean v1_stringifiable;

        has_pointers = BE_any_pointers(param);
        v1_stringifiable = AST_SMALL_SET(param->type)
                            && !AST_STRING0_SET(param->type)
                            && !has_pointers;
        if (v1_stringifiable)
            fprintf(fid, "if (v1_string_flag) %s++;\n",
                          BE_get_name(BE_Array_Info(param)->count_var));
    }

    if ((flags & BE_M_NODE) && !(BE_PI_Flags(param) & BE_FIELD))
    {
        /* Initialize the local A,B,count vars for the fixed array case. */
        fprintf(fid, "}\nelse\n{\n");

        if (!AST_SMALL_SET(param))
        {
            AST_array_index_n_t *index_p =
                    param->type->type_structure.array->index_vec;

            for (i = 0; i < param->type->type_structure.array->index_count; i++)
            {
                if (AST_FIXED_LOWER_SET(index_p)
                    &&  index_p->lower_bound->value.int_val == 0)
                {
                    fprintf(fid, "%s = %ld;\n",
                            BE_get_name(BE_Array_Info(param)->A[i]),
                            index_p->lower_bound->value.int_val);
                    if (AST_FIXED_UPPER_SET(index_p))
                        fprintf(fid, "%s = %ld;\n",
                                BE_get_name(BE_Array_Info(param)->B[i]),
                                index_p->upper_bound->value.int_val-
                                index_p->lower_bound->value.int_val+1);
                    else
                        fprintf(fid, "%s = %s;\n",
                                BE_get_name(BE_Array_Info(param)->B[i]),
                                BE_get_name(BE_Array_Info(param)->Z[i]));
                }
                else
                    INTERNAL_ERROR("Array with nonzero lower bound not supported");

                index_p++;
            }

            fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->count_exp);
        }
        else
        {
            fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->size_exp);
        }
        fprintf(fid, "}\n");
    }
}


/*
 * spell_ool_count_adjust
 */
static void spell_ool_count_adjust
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
    /*
     * if element is a non-string0 ool array then the loop count need
     * to be adjusted by dividing it by the the count exp for the element
     */

    AST_type_n_t *elt_type = param->type->type_structure.array->element_type;
    char *var = BE_get_name(BE_Array_Info(param)->count_var);

    if (elt_type->kind == AST_array_k
    && AST_OUT_OF_LINE_SET(elt_type)
    && !AST_STRING_SET(elt_type)
    && !AST_STRING0_SET(elt_type))
        fprintf(fid, "%s = %s/(%s);\n", var, var,
            BE_Array_Info(BE_Array_Info(param)->flat_elt)->count_exp);
}

/*
 * spell_array_pointees_unmarshall
 *
 * Second pass of array unmarshalling to unmarshall element pointees
 */
static void spell_array_pointees_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*  BE_M_ALIGN_MP
                         * |BE_M_MAINTAIN_OP
                         * |BE_M_CHECK_BUFFER
                         * |BE_M_BUFF_BOOL
                         * |BE_M_PIPE
                         */
    BE_mn_t *mn,
    boolean *p_routine_mode
)
#else
(fid, param, flags, mn, p_routine_mode)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
    boolean *p_routine_mode;
#endif
{
    AST_parameter_n_t *elt;
    BE_mflags_t elt_flags;

    fprintf(fid, "%s = %s;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var),
            BE_Array_Info(param)->first_element_exp);

    /*
     * Try to set up the count variable again.  If the array is varying
     * or fixed, use the count expression.  Otherwise, use the size variable.
     */
    if (AST_VARYING_SET(param) || !(AST_CONFORMANT_SET(param->type)))
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);
    else fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_get_name(BE_Array_Info(param)->size_var));


    spell_ool_count_adjust(fid, param);

    /*
     * Iterate over the number of elements in the array
     */
    fprintf(fid, "while (%s)\n{\n",
                  BE_get_name(BE_Array_Info(param)->count_var));

    elt_flags = BE_M_ENOUGH_ROOM | BE_M_ADVANCE_MP | BE_M_BUFF_EXISTS |
        (flags &
            (BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_DEFER | BE_M_NODE |
             BE_M_CALLER | BE_M_CALLEE));
    /*
     * Unmarshall an array element
     */
    for (elt = BE_Array_Info(param)->flat_elt; elt; elt = elt->next)
        if (!(BE_PI_Flags(elt) & BE_SKIP))
            BE_unmarshall_param(fid, elt, elt_flags, mn, p_routine_mode);

    fprintf(fid, "%s++;\n%s--;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var),
            BE_get_name(BE_Array_Info(param)->count_var));

    fprintf(fid, "}\n");
}

/*
 * BE_spell_array_hdr_unmarshall
 *
 * Align to and unmarshall conformance or variance bytes, if any
 */
void BE_spell_array_hdr_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*  BE_M_ALIGN_MP
                         * |BE_M_MAINTAIN_OP
                         * |BE_M_CHECK_BUFFER
                         */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    /*
     * Unmarshall the conformance bytes for arrays not contained in open
     * records
     */
    if (AST_CONFORMANT_SET(param->type) && !(BE_PI_Flags(param)& BE_IN_ORECORD)
        && !(flags & BE_M_PIPE) && !(flags & BE_M_NODE))
    {
        BE_spell_conformant_unmarshall(fid, param, flags, mn);

        /*
         * allocate the array if necessary
         */
        if (BE_PI_Flags(param) & BE_ALLOCATE)
        {
            fprintf(fid, "%s = ", BE_get_name(param->name));
            CSPELL_cast_exp(fid, BE_Array_Info(param)->alloc_type);
            fprintf(fid, "rpc_ss_mem_alloc(%s, %s*sizeof", mn->pmem_h,
                BE_get_name(BE_Array_Info(param)->size_var));
            CSPELL_cast_exp(fid, BE_Array_Info(param)->element_ptr_type->
                                    type_structure.pointer->pointee_type);
            fprintf(fid, ");\n");
        }

        if (flags & BE_M_CALLEE &&
            BE_any_ref_pointers(BE_Array_Info(param)->flat_elt))
        {
            /*
             * Allocation is done with the ref_pass flattened parameter
             */
            if (param->type->type_structure.array->element_type->kind ==
                AST_pointer_k)
                BE_spell_alloc_ref_pointer(fid, param->next, BE_arp_during,
                                           flags & BE_M_NODE);
            else
                BE_spell_alloc_ref_pointer(fid, param->next->next,
                                           BE_arp_during, flags & BE_M_NODE);
        }
    }

    if (AST_VARYING_SET(param))
        spell_varying_unmarshall(fid, param, flags, mn);
    else
    {
        /*
         * The array is not varying.  If it is fixed, set the count variable
         * equal to the number of elements in the array.  Otherwise, the
         * count variable was set up during conformance/variance unmarshalling.
         * For an [out_of_line] array this will happen in the ool routine.
         */
        if ( !AST_CONFORMANT_SET(param->type)
             && !(BE_PI_Flags(param) & BE_OOL_HEADER) )
            fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->count_exp);
    }
}

/*
 * BE_spell_array_unmarshall
 */
void BE_spell_array_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*  BE_M_ALIGN_MP
                         * |BE_M_MAINTAIN_OP
                         * |BE_M_CHECK_BUFFER
                         * |BE_M_BUFF_BOOL
                         * |BE_M_PIPE
                         */
    BE_mn_t *mn,
    boolean *p_routine_mode
)
#else
(fid, param, flags, mn, p_routine_mode)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
    boolean *p_routine_mode;
#endif
{
    AST_parameter_n_t *elt;
    BE_mflags_t elt_flags;
    boolean do_bug2;
    char drep_field[10];
    boolean ool_elts = false;
    boolean routine_mode_at_entry;  /* Is routine mode used for the first
                                        field of an element */
    boolean ifdef_emitted = false;

    if (BE_PI_Flags(param) & BE_REF_PASS)
    {
        /*
         * Second array pass for pointee unmarshalling
         */
        spell_array_pointees_unmarshall(fid, param, flags, mn, p_routine_mode);
        return;
    }

    if (BE_PI_Flags(BE_Array_Info(param)->flat_elt) & (BE_OOL | BE_SKIP) )
    {
        ool_elts = true;
    }

    /*
     * Kick up the count variable for strings0
     */
    if (BE_PI_Flags(param) & BE_STRING0)
        fprintf(fid, "%s++; /* Unmarshall trailing NULL */\n",
            BE_get_name(BE_Array_Info(param)->count_var));

    if ((flags & BE_M_ALIGN_MP)
        && BE_Array_Info(param)->flat_elt->type->alignment_size > 1)
    {
        /*
         * Don't align to the elements of empty arrays unless -bug 2 is
         * set and we're marshalling an aligned array of scalars.
         */
        do_bug2 = (BE_bug_array_align2 && AST_SMALL_SET(param) &&
                   (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY) &&
                   (param->type->type_structure.array->element_type->kind !=
                       AST_structure_k));

        if ((AST_CONFORMANT_SET(param->type) || AST_VARYING_SET(param)) &&
            !do_bug2)
            fprintf(fid, "if (%s > 0)\n{\n    ",
                BE_get_name(BE_Array_Info(param)->count_var));

        align_mp(fid, mn->mp, mn->op, BE_Array_Info(param)->flat_elt->type,
                 flags & BE_M_MAINTAIN_OP);

        if ((AST_CONFORMANT_SET(param->type) || AST_VARYING_SET(param)) &&
            !do_bug2)
            fprintf(fid, "}\n");
    }

    if (flags & BE_M_CHECK_BUFFER)
        check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                     flags & BE_M_BUFF_BOOL);

    fprintf(fid, "%s = ",
            BE_get_name(BE_Array_Info(param)->element_ptr_var));
#if 0 /* I don't think this is needed */
    CSPELL_cast_exp(fid,
            BE_Array_Info(param)->element_ptr_type);
#endif
    fprintf(fid, "%s;\n",
            BE_Array_Info(param)->first_element_exp);

    spell_ool_count_adjust (fid, param);

    if ( BE_PI_Flags(BE_Array_Info(param)->flat_elt) & BE_OOL )
        routine_mode_at_entry = true;
    else
        routine_mode_at_entry = false;
    BE_u_match_routine_mode(fid, flags, routine_mode_at_entry, p_routine_mode);

    /*
     * Iterate over the number of elements in the array
     */
    fprintf(fid, "while (%s)\n{\n",
                  BE_get_name(BE_Array_Info(param)->count_var));

    if (!(flags & BE_M_ENOUGH_ROOM))
    {
        fprintf(fid,
            "/* compute a lower bound on the number of elts in this buffer */\n");
        fprintf(fid,
                "%s = (%s->data_len - ((byte_p_t)%s - %s->data_addr))/%d;\n",
                BE_get_name(BE_Array_Info(param)->frag_var),
                mn->elt, mn->mp, mn->elt,
                BE_Array_Info(param)->ndr_elt_size);
        fprintf(fid, "if (%s > %s) %s = %s;\n",
                BE_get_name(BE_Array_Info(param)->frag_var),
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_get_name(BE_Array_Info(param)->frag_var),
                BE_get_name(BE_Array_Info(param)->count_var));
    }

    if ( (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY)
            && ( ! (flags & BE_M_CONVERT)
                 || (BE_Array_Info(param)->flat_elt->type->kind == AST_byte_k)
                 || (BE_Array_Info(param)->flat_elt->type->kind
                                                          == AST_boolean_k) ) )
    {
        if (BE_Array_Info(param)->flat_elt->type->kind == AST_character_k)
            fprintf(fid, "#ifdef PACKED_CHAR_ARRAYS\n");
        else if (BE_Array_Info(param)->flat_elt->type->kind == AST_byte_k)
            fprintf(fid, "#ifdef PACKED_BYTE_ARRAYS\n");
        else fprintf(fid, "#ifdef PACKED_SCALAR_ARRAYS\n");
        ifdef_emitted = true;
        /*
         * The block copy path for scalars, bytes, and characters on the
         * unmarshall path
         */

        switch(BE_Array_Info(param)->flat_elt->type->kind)
        {
            case AST_character_k:
                strcpy( drep_field, "char_rep" );
                break;
            case AST_small_integer_k:
            case AST_short_integer_k:
            case AST_long_integer_k:
            case AST_hyper_integer_k:
            case AST_small_unsigned_k:
            case AST_short_unsigned_k:
            case AST_long_unsigned_k:
            case AST_hyper_unsigned_k:
            case AST_enum_k:
                strcpy( drep_field, "int_rep" );
                break;
            case AST_short_float_k:
            case AST_long_float_k:
                strcpy( drep_field, "float_rep" );
                break;
        }

        if (!(flags & BE_M_ENOUGH_ROOM))
        {
            fprintf(fid, "%s = %s*%d;\n",
                    BE_get_name(BE_Array_Info(param)->block_var),
                    BE_get_name(BE_Array_Info(param)->frag_var),
                    BE_Array_Info(param)->ndr_elt_size);
            fprintf(fid, "memcpy((char *)%s, (char *)%s, %s);\n",
                    BE_get_name(BE_Array_Info(param)->element_ptr_var),
                    mn->mp,
                    BE_get_name(BE_Array_Info(param)->block_var));
            fprintf(fid, "rpc_advance_mop(%s, %s, %s);\n", mn->mp, mn->op,
                    BE_get_name(BE_Array_Info(param)->block_var));
            fprintf(fid, "%s += %s;\n",
                    BE_get_name(BE_Array_Info(param)->element_ptr_var),
                    BE_get_name(BE_Array_Info(param)->frag_var));
            fprintf(fid, "%s -= %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_get_name(BE_Array_Info(param)->frag_var));
        }
        else
        /*
         * If we know there's enough room then just copy the entire array out
         * of the buffer
         */
        {
            if (!(BE_PI_Flags(param) & BE_STRING0))
            {
                fprintf(fid,
                    "memcpy((char *)%s, (char *)%s, %s*%d);\n",
                    BE_get_name(BE_Array_Info(param)->element_ptr_var),
                    mn->mp,
                    BE_Array_Info(param)->count_exp,
                    BE_Array_Info(param)->ndr_elt_size);
                fprintf(fid, "%s -= %s; /* %s = 0 */\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->count_exp,
                    BE_get_name(BE_Array_Info(param)->count_var));
                fprintf(fid, "rpc_advance_mop(%s, %s, %s*%d);\n", mn->mp, mn->op,
                    BE_Array_Info(param)->count_exp,
                    BE_Array_Info(param)->ndr_elt_size);
            }
            else
            {
                fprintf(fid,
                    "memcpy((char *)%s, (char *)%s, %s*%d);\n",
                    BE_get_name(BE_Array_Info(param)->element_ptr_var),
                    mn->mp,
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->ndr_elt_size);
                fprintf(fid, "rpc_advance_mop(%s, %s, %s*%d);\n", mn->mp, mn->op,
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->ndr_elt_size);
                fprintf(fid, "%s = 0;\n",
                    BE_get_name(BE_Array_Info(param)->count_var));
            }
        }
        fprintf(fid, "#else\n");

    }

    /*
     * The non-block copy path:
     *
     * Iterate over the number of elements in this packet
     */
    fprintf( fid, "{\n" );
    if (!(flags & BE_M_ENOUGH_ROOM))
        fprintf(fid, "while (%s--)\n{\n",
                BE_get_name(BE_Array_Info(param)->frag_var));

    elt_flags = BE_M_ENOUGH_ROOM | BE_M_ADVANCE_MP |
        (flags &
            (BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_DEFER | BE_M_NODE |
             BE_M_PIPE | BE_M_BUFF_EXISTS | BE_M_CALLER | BE_M_CALLEE));
    /*
     * Unmarshall an array element
     */
    for (elt = BE_Array_Info(param)->flat_elt; elt; elt = elt->next)
    {
        if ( !(BE_PI_Flags(elt) & BE_SKIP) )
            BE_unmarshall_param(fid, elt, elt_flags, mn, p_routine_mode);
    }
    BE_u_match_routine_mode(fid, flags, routine_mode_at_entry, p_routine_mode);

    fprintf(fid, "%s++;\n%s--;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var),
            BE_get_name(BE_Array_Info(param)->count_var));

    if (!(flags & BE_M_ENOUGH_ROOM)) fprintf(fid, "}\n");
    fprintf(fid, "}\n");

    if ( ifdef_emitted )
    {
        fprintf(fid, "#endif\n");
    }

    if (!(flags & BE_M_ENOUGH_ROOM))
    {
        /*
         * If this is not an array of scalars then our estimate may have been
         * low; unmarshall array elements, checking after each scalar, until
         * the buffer changes
         */
        if (!(BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY))
        {
            fprintf(fid, "/* pick up any remaining elements in the buffer */\n");
            fprintf(fid, "buffer_changed = ndr_false;\n");

            fprintf(fid, "while (%s && !buffer_changed)\n{\n",
                BE_get_name(BE_Array_Info(param)->count_var));
            /*
             * Unmarshall an array element, checking the buffer after each
             * scalar
             */

            elt_flags = BE_M_CHECK_BUFFER | BE_M_ADVANCE_MP | BE_M_BUFF_BOOL |
                (flags &
                    (BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_DEFER | BE_M_NODE |
                     BE_M_PIPE | BE_M_BUFF_EXISTS | BE_M_CALLER | BE_M_CALLEE));

            for (elt = BE_Array_Info(param)->flat_elt; elt; elt = elt->next)
            {
                if ( !(BE_PI_Flags(elt) & BE_SKIP) )
                    BE_unmarshall_param(fid, elt, elt_flags, mn, p_routine_mode);
            }
            BE_u_match_routine_mode(fid, flags, routine_mode_at_entry,
                                     p_routine_mode);

            fprintf(fid, "%s++;\n%s--;\n",
                    BE_get_name(BE_Array_Info(param)->element_ptr_var),
                    BE_get_name(BE_Array_Info(param)->count_var));

            fprintf(fid, "}\n");
        }
        /*
         * Otherwise just get a new buffer here if there are any elements left
         * to unmarshall
         */
        else
        {
            fprintf(fid, "if (%s)\n{\n",
            BE_get_name(BE_Array_Info(param)->count_var));
            check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                              (flags & BE_M_BUFF_BOOL));
            fprintf(fid, "}\n");
        }
    }
    fprintf(fid, "}\n");
}

/*
 * BE_spell_open_record_unmarshall
 *
 * Unmarshall the conformance bits for an open record and allocate it if
 * necessary
 */
void BE_spell_open_record_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_MAINTAIN_OP
                         */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    /*
     * Allocation for conformant nodes is done in the node marshalling code
     */
    if (flags & BE_M_NODE) return;

    BE_spell_conformant_unmarshall(fid, BE_Open_Array(param), flags, mn);

    /*
     * Allocate the open record if necessary
     */
    if (BE_PI_Flags(param) & BE_ALLOCATE)
    {
        fprintf(fid, "%s = ", BE_get_name(BE_Orecord_Info(param)->alloc_var));
        CSPELL_cast_exp(fid, BE_Orecord_Info(param)->alloc_typep);
        fprintf(fid, "rpc_ss_mem_alloc(%s, sizeof", mn->pmem_h);
        CSPELL_cast_exp(fid, BE_Orecord_Info(param)->alloc_type);
        fprintf(fid, "+(%s)*sizeof",
            BE_get_name(BE_Array_Info(BE_Open_Array(param))->size_var));
        CSPELL_cast_exp(fid,
            BE_Array_Info(BE_Open_Array(param))->element_ptr_type->
                                    type_structure.pointer->pointee_type);
        fprintf(fid, "-sizeof");
        CSPELL_cast_exp(fid,
            BE_Array_Info(BE_Open_Array(param))->element_ptr_type->
                                    type_structure.pointer->pointee_type);
        fprintf(fid, ");\n");
    }

    if (flags & BE_M_CALLEE)
    {
        do
        {
            param = param->next;
            BE_spell_alloc_ref_pointer(fid, param, BE_arp_during,
                                       flags & BE_M_NODE);
        } while (!(BE_PI_Flags(param) & BE_LAST_FIELD));
    }
}

/*
 * spell_union_pointees_unmarshall
 *
 * Unmarshall the pointees of a discriminated union
 */
static void spell_union_pointees_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                          *  BE_M_ALIGN_MP
                          * |BE_M_MAINTAIN_OP
                          * |BE_M_CHECK_BUFFER
                          * |BE_M_CONVERT
                          */
    BE_mn_t *mn,
    boolean *p_routine_mode
)
#else
(fid, param, flags, mn, p_routine_mode)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
    boolean *p_routine_mode;
#endif
{
    AST_case_label_n_t *label;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    BE_mflags_t new_flags;

    fprintf(fid, "switch (%s)\n{\n",
            BE_get_name(BE_Du_Info(param)->discrim->name));

    for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
    {
         for (label = arm->labels; label; label = label->next)
            if (label->default_label)
            {
                fprintf(fid, "default:\n");
            }
            else
            {
                fprintf(fid, "case ");
                CSPELL_constant_val(fid, label->value);
                fprintf(fid, ":\n");
            }

        if (flags & BE_M_OOL_RTN)
        {
            BE_ool_spell_varying_info( fid, arm->referred_to_by->flat );
        }

        new_flags = BE_M_ADVANCE_MP | BE_M_BUFF_EXISTS |
            (flags &
                (BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_DEFER |
                 BE_M_CHECK_BUFFER | BE_M_ENOUGH_ROOM | BE_M_NODE |
                 BE_M_CALLER | BE_M_CALLEE));

        for (pp = arm->flat; pp; pp = pp->next)
            BE_unmarshall_param(fid, pp, new_flags, mn, p_routine_mode);

        fprintf(fid, "break;\n");
    }

    fprintf(fid, "}\n");

}

/*
 * BE_spell_union_unmarshall
 *
 * Unmarshall the switch part of a discriminated union
 */
void BE_spell_union_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                          *  BE_M_ALIGN_MP
                          * |BE_M_MAINTAIN_OP
                          * |BE_M_CHECK_BUFFER
                          * |BE_M_CONVERT
                          */
    BE_mn_t *mn,
    boolean *p_routine_mode
)
#else
(fid, param, flags, mn, p_routine_mode)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
    boolean *p_routine_mode;
#endif
{
    AST_case_label_n_t *label;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    boolean defaulted = 0;
    BE_mflags_t new_flags;
    boolean arm_routine_mode;  /* TRUE if arm element unmarshalled by routine */

    if (BE_PI_Flags(param) & BE_REF_PASS)
    {
        /*
         * Second union pass for pointee unmarshalling
         */
        spell_union_pointees_unmarshall(fid, param, flags, mn, p_routine_mode);
        return;
    }

    fprintf(fid, "switch (%s)\n{\n",
            BE_get_name(BE_Du_Info(param)->discrim->name));

    for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
    {
        arm_routine_mode = *p_routine_mode;
        for (label = arm->labels; label; label = label->next)
            if (label->default_label)
            {
                fprintf(fid, "default:\n");
                defaulted = 1;
            }
            else
            {
                fprintf(fid, "case ");
                CSPELL_constant_val(fid, label->value);
                fprintf(fid, ":\n");
            }

        new_flags = BE_M_ADVANCE_MP | BE_M_BUFF_EXISTS |
            (flags &
                (BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_DEFER | BE_M_NODE |
                 BE_M_CHECK_BUFFER | BE_M_ENOUGH_ROOM | BE_M_CALLER |
                 BE_M_CALLEE));

        for (pp = arm->flat; pp; pp = pp->next)
        {
            if (!(BE_PI_Flags(pp) & BE_SKIP))
                BE_unmarshall_param(fid, pp, new_flags, mn, &arm_routine_mode);
        }

        if (arm_routine_mode != *p_routine_mode)
        {
            /* Make all arms revert to marshalling mode at start of union */
            if (arm_routine_mode)
                BE_u_end_buff( fid, flags );
            else
                BE_u_start_buff( fid, flags );
        }
        fprintf(fid, "break;\n");
    }

    if (!defaulted) fprintf(fid, "default: RAISE(rpc_x_invalid_tag);\n");

    fprintf(fid, "}\n");
}

/*
 * spell_pointee_unmarshall
 *
 * Emit the arcana necessary to unmarshall a pointee
 */
static void spell_pointee_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,        /* BE_M_CALLER|BE_M_CALLEE|BE_M_NODE */
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    char *node_type_flag;
    AST_type_n_t *pointee_type =
        param->type->type_structure.pointer->pointee_type;
    boolean varying, pseudo_varying;

    /*
     * Decide which flag to send to the unmarshalling routine
     */
    if (AST_REF_SET(param))
    {
        /*
         * Conformant ref node on the server side: always allocate on the fly
         */
        if ((flags & BE_M_CALLEE) && AST_CONFORMANT_SET(pointee_type))
            node_type_flag = "rpc_ss_new_ref_node_k";

        /*
         * Emitting code into a node unmarshalling routine--
         *     Caller side: allocate only if under a changed mutable node
         *     Callee side: allocate unless under an existing ref node
         */
        else if (flags & BE_M_NODE)
        {
            if (flags & BE_M_CALLER) node_type_flag =
                "NIDL_new_node?rpc_ss_new_ref_node_k:rpc_ss_old_ref_node_k";
            else node_type_flag =
                "NIDL_node_type==rpc_ss_old_ref_node_k?rpc_ss_old_ref_node_k:rpc_ss_new_ref_node_k";
        }

        /*
         * Emitting code into a {c,s}sr--thing was pre-allocated
         */
        else node_type_flag = "rpc_ss_old_ref_node_k";
    }
    else node_type_flag = "rpc_ss_mutable_node_k";

    varying = (BE_PI_Flags(param) & (BE_PTR_ARRAY | BE_ARRAYIFIED | BE_PTR2STR))
              && (AST_VARYING_SET(param) || AST_STRING_SET(param) ||
                  AST_STRING_SET(pointee_type));

    /*
     * Even if an array is not varying, it might need to be marshalled via a
     * generic routine which expects parameters semantically equivalent to
     * the A,B values for a varying array.
     */
    pseudo_varying =
        (!varying && pointee_type->kind == AST_array_k &&
         ( ((flags & BE_M_CALLEE) && AST_IN_VARYING_SET(pointee_type))
         ||((flags & BE_M_CALLER) && AST_OUT_VARYING_SET(pointee_type))));

    CSPELL_pa_routine_name(fid, pointee_type,
        flags & BE_M_CALLER ? BE_caller : BE_callee,
        BE_unmarshalling_k, varying);

    fprintf(fid, "(");
    /*
     * Add a cast to the correct type.  enums are a special case
     * and must be cast to int* which is expected by the pa routine.
     */
    if (pointee_type->kind == AST_enum_k)
        fprintf(fid, "(int**)");
    else
        CSPELL_cast_exp(fid, BE_pointer_type_node(param->type));
    fprintf(fid, "&%s, %s, ", BE_get_name(param->name), node_type_flag);

    if (flags & BE_M_NODE)
    {
        fprintf(fid, "p_unmar_params");
    }
    else
    {
        fprintf(fid, "&unmar_params");
    }

    if (pointee_type->kind == AST_array_k)
    {
        if (varying)
            fprintf(fid, ", idl_true");
        else if (pseudo_varying)
            fprintf(fid, ", idl_false");
    }

    fprintf(fid, ");\n");

#ifdef NO_EXCEPTIONS
    check_status(fid, mn->st, mn->bad_st);
#endif
}

/*
 * BE_spell_pointer_unmarshall
 *
 * Unmarshall a pointer
 */
void BE_spell_pointer_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    static char *node_number = "IDL_node_number";

    if (flags & BE_M_DEFER)
    {
        if (flags & BE_M_ALIGN_MP)
            align_mp(fid, mn->mp, mn->op, param->type,
                     flags & BE_M_MAINTAIN_OP);

        if (flags & BE_M_CHECK_BUFFER)
            check_buffer(fid, mn->mp, mn->elt, mn->st, mn->call_h, mn->bad_st,
                         flags & BE_M_BUFF_BOOL);

        if (AST_MUTABLE_SET(param) && !AST_IGNORE_SET(param))
        {
            /*
             * Unmarshall node number
             */
            fprintf(fid, "{\nidl_ulong_int %s;\n",node_number);
            unmarshall_scalar(fid, flags & BE_M_CONVERT, mn->mp, mn->drep,
                              BE_ulong_int_p, node_number);

            fprintf(fid, "%s = ", BE_get_name(param->name));

            if (BE_PI_Flags(param) & BE_ARRAYIFIED)
            {
                CSPELL_cast_exp(fid,
                    (BE_pointer_type_node(BE_Ptr_Info(param)->
                        flat_array->type->type_structure.array->element_type)));
            }
            else CSPELL_cast_exp(fid, param->type);

            fprintf(fid, "%s;\n}\n", node_number);
        }

        if (flags & BE_M_ADVANCE_MP)
            advance_mp(fid, mn->mp, mn->op, BE_ulong_int_p,
                       flags & BE_M_MAINTAIN_OP);
    }
    else
    {
        spell_pointee_unmarshall(fid, param, flags, mn);
    }
}

/*
 * BE_spell_ool_unmarshall
 *
 * Unmarshall a param of out-of-line type
 */
void BE_spell_ool_unmarshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,
    BE_mn_t *mn
)
#else
(fid, param, flags, mn)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    BE_mn_t *mn;
#endif
{
    int i, num_array_dimensions;
    boolean varying, v1_string_on_param, v1_string_type;
    AST_parameter_n_t *open_array_param;
    AST_type_n_t *ool_type;
    boolean ool_type_has_pointers;

    ool_type = BE_Ool_Info(param)->type;
    ool_type_has_pointers = BE_Ool_Info(param)->any_pointers;

    if (ool_type->kind == AST_array_k)
    {
        varying = AST_VARYING_SET(param)
                  || ( AST_STRING_SET(param)
                                   &&  !(BE_Ool_Info(param)->has_calls_before) )
                  || AST_STRING0_SET(param)
                  || (BE_PI_Flags(param) & BE_PTR2STR);
        v1_string_type = AST_STRING0_SET(ool_type);
        v1_string_on_param = AST_STRING0_SET(param) && !v1_string_type;
    }
    else
    {
        varying = false;
        v1_string_type = false;
        v1_string_on_param = false;
    }

    CSPELL_ool_routine_name(fid, BE_Ool_Info(param)->call_type,
        flags & BE_M_CALLER ? BE_caller : BE_callee,
        BE_unmarshalling_k, varying,
        ( BE_Ool_Info(param)->use_P_rtn));

    fprintf(fid, "(");

    CSPELL_cast_exp(fid, BE_pointer_type_node(BE_Ool_Info(param)->call_type));

    fprintf(fid, "%s%s",
        ( ool_type->kind == AST_array_k
        || ((flags & BE_M_CALLEE)
            && AST_CONFORMANT_SET(ool_type)
            && (!(flags & BE_M_NODE))
            && (BE_Ool_Info(param)->top_level))
        || BE_Ool_Info(param)->has_calls_before )
            ? "" : "&", BE_get_name(BE_Ool_Info(param)->call_name) );

    if (flags & BE_M_NODE)
    {
        if (BE_Ool_Info(param)->use_P_rtn)
        {
            if (flags & BE_M_CALLER)
            {
                fprintf( fid, ", NIDL_new_node" );
            }
            else
            {
                fprintf( fid, ", NIDL_node_type" );
            }
        }
        fprintf(fid, ", p_unmar_params");
    }
    else
    {
        if (BE_Ool_Info(param)->use_P_rtn)
        {
            if (flags & BE_M_CALLER)
            {
                fprintf( fid, ", idl_false" );
            }
            else
            {
                fprintf( fid, ", rpc_ss_old_ref_node_k" );
            }
        }
        fprintf(fid, ", &unmar_params");
    }

    if (ool_type->kind == AST_array_k)
        fprintf(fid, ", %s", (varying) ? "idl_true" : "idl_false");

    if (BE_Is_Open_Record(BE_Ool_Info(param)->call_type))
    {
        open_array_param = BE_Open_Array(param);
        if (AST_SMALL_SET(open_array_param))
        {
            fprintf(fid, ", %s",
                         BE_get_name(BE_Array_Info(open_array_param)->size_var));
        }
        else
        {
            num_array_dimensions = open_array_param->
                type->type_structure.array->index_count;
            for (i=0; i<num_array_dimensions; i++)
            {
                fprintf(fid, ", %s",
                    BE_get_name(BE_Array_Info(open_array_param)->Z[i]));
            }
        }
    }

    if (BE_Ool_Info(param)->call_type->kind == AST_array_k)
    {
        if (AST_SMALL_SET(param))
        {
            /*
             * Need to tell an ool v1_array unmarshalling routine whether
             * the object being unmarshalled is actually a v1_string.
             */
            if (!ool_type_has_pointers && !v1_string_type)
                fprintf (fid, ", idl_%s", v1_string_on_param ? "true" : "false");
            if (AST_CONFORMANT_SET(BE_Ool_Info(param)->call_type))
               fprintf(fid, ", %s", BE_get_name(BE_Array_Info(param)->size_var));
        }
        else
        {
            if ( AST_STRING_SET(BE_Ool_Info(param)->call_type) && AST_CONFORMANT_SET(param->type) )
            {
                fprintf(fid, ", %s",
                    BE_get_name(BE_Array_Info(param)->Z[0]));
            }
            else
            {
                num_array_dimensions
                    = param->type->type_structure.array->index_count;
                for (i=0; i<num_array_dimensions; i++)
                    if (AST_CONFORMANT_SET(param->type))
                        fprintf(fid, ", %s",
                            BE_get_name(BE_Array_Info(param)->Z[i]));
            }
        }
    }

    fprintf(fid, ");\n");

#ifdef NO_EXCEPTIONS
    check_status(fid, mn->st, mn->bad_st);
#endif

}
