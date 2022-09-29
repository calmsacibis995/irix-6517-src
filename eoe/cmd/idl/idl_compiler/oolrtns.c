/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: oolrtns.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:41:16  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:54  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:50:46  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:12  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/13  19:04:19  harrow
 * 	Propagat a flag for use in correcting  [transmit_as] on a field of an [out_of_line] structure
 * 	[1992/05/13  11:42:00  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:46  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      oolrtns.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Routines supporting out_of_line marshalling
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <decorate.h>
#include <dflags.h>
#include <dutils.h>
#include <flatten.h>
#include <hdgen.h>
#include <marshall.h>
#include <mmarsh.h>
#include <nametbl.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <oolrtns.h>
#include <backend.h>
#include <inpipes.h>
#include <outpipes.h>
#include <bounds.h>
#include <localvar.h>

/******************************************************************************/
/*                                                                            */
/*    Does a type have any pointer fields?                                    */
/*                                                                            */
/******************************************************************************/
boolean BE_type_has_pointers
#ifdef PROTO
(
    AST_type_n_t *p_type
)
#else
( p_type )
    AST_type_n_t *p_type;
#endif
{
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_operation_n_t dummy_oper;

    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = 0;
    dummy_param.field_attrs = NULL;

    return( BE_any_pointers ( BE_flatten_parameter( &dummy_param,
                        NAMETABLE_add_id("(*p_node)"), p_type, BE_client_side,
                        true, &open_array, BE_F_PA_RTN | BE_F_OOL_RTN ) ) );
}

/******************************************************************************/
/*                                                                            */
/*    Spell the name of a routine for un/marshalling an out_of_line type      */
/*                                                                            */
/******************************************************************************/
void CSPELL_ool_routine_name
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,
    BE_call_side_t call_side,
    BE_marshalling_k_t to_or_from_wire,
    boolean varying,
    boolean pointees
)
#else
(fid, p_type, call_side, to_or_from_wire, varying, pointees)
    FILE *fid;
    AST_type_n_t *p_type;
    BE_call_side_t call_side;
    BE_marshalling_k_t to_or_from_wire;
    boolean varying;
    boolean pointees;
#endif
{
        spell_name( fid, p_type->name );
        fprintf( fid, "%c%c%c",
                      (pointees) ? 'P' : 'O',
                      (to_or_from_wire == BE_marshalling_k) ? 'm' : 'u',
                      (call_side == BE_caller) ? 'r' : 'e' );
}

/******************************************************************************/
/*                                                                            */
/*    Spell the setting up of variables used in un/marshalling varying arrays */
/*                                                                            */
/******************************************************************************/
void BE_ool_spell_varying_info
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *flattened_type
)
#else
(fid, flattened_type)
    FILE *fid;
    AST_parameter_n_t *flattened_type;
#endif
{
    AST_parameter_n_t *elt;
    int i;

    for (elt = flattened_type; elt; elt = elt->next)
    {
        if( (BE_PI_Flags(elt) & BE_ARRAY_HEADER) && AST_VARYING_SET(elt) )
        {
            /*
             * ool routines for arrays must be conditionalized to handle
             * varying or non-varying array instance.
             */
            if (!(BE_PI_Flags(elt) & BE_FIELD))
                fprintf(fid, "if (NIDL_varying)\n{\n");
            /* Varying case. */
            if (!AST_SMALL_SET(elt))
            {
                for (i = 0;
                     i < elt->type->type_structure.array->index_count;
                     i++)
                {
                    fprintf(fid, "%s = %s;\n%s = %s;\n",
                            BE_get_name(BE_Array_Info(elt)->A[i]),
                            BE_Array_Info(elt)->A_exps[i],
                            BE_get_name(BE_Array_Info(elt)->B[i]),
                            BE_Array_Info(elt)->B_exps[i]);
                }
                fprintf(fid, "%s = %s;\n",
                        BE_get_name(BE_Array_Info(elt)->count_var),
                        BE_Array_Info(elt)->count_exp);
            }
            else
            {
                fprintf(fid, "%s = %s;\n",
                        BE_get_name(BE_Array_Info(elt)->count_var),
                        BE_Array_Info(elt)->count_exp);
            }

            /* Non-varying case. */
            if (!(BE_PI_Flags(elt) & BE_FIELD))
            {
                fprintf(fid, "}\nelse\n{\n");
                if (!AST_SMALL_SET(elt))
                {
                    AST_array_index_n_t *index_p =
                            elt->type->type_structure.array->index_vec;

                    for (i = 0;
                         i < elt->type->type_structure.array->index_count;
                         i++)
                    {
                        if (AST_FIXED_LOWER_SET(index_p)
                            &&  index_p->lower_bound->value.int_val == 0)
                        {
                            fprintf(fid, "%s = %ld;\n",
                                    BE_get_name(BE_Array_Info(elt)->A[i]),
                                    index_p->lower_bound->value.int_val);
                            if (AST_FIXED_UPPER_SET(index_p))
                                fprintf(fid, "%s = %ld;\n",
                                        BE_get_name(BE_Array_Info(elt)->B[i]),
                                        index_p->upper_bound->value.int_val-
                                        index_p->lower_bound->value.int_val+1);
                            else
                                fprintf(fid, "%s = %s;\n",
                                        BE_get_name(BE_Array_Info(elt)->B[i]),
                                        BE_get_name(BE_Array_Info(elt)->Z[i]));
                        }
                        else
                            INTERNAL_ERROR("Array with nonzero lower bound not supported");

                        index_p++;
                    }

                    fprintf(fid, "%s = %s;\n",
                            BE_get_name(BE_Array_Info(elt)->count_var),
                            BE_Array_Info(elt)->count_exp);
                }
                else
                {
                    fprintf(fid, "%s = %s;\n",
                            BE_get_name(BE_Array_Info(elt)->count_var),
                            BE_Array_Info(elt)->size_exp);
                }
                fprintf(fid, "}\n");
            }
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*    Output the declaration part of a out_of_line type marshalling routine   */
/*                                                                            */
/******************************************************************************/
void CSPELL_marshall_ool_type_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type,
    boolean varying,          /* TRUE if rtn is for [non]varying array */
    boolean has_pointers,     /* TRUE if the type includes any pointers */
    boolean pointee_routine,  /* TRUE for routines that marshall pointees */
    boolean v1_stringifiable  /* TRUE if v1_array && !v1_string */
)
#else
( fid, p_type, call_side, in_header, flattened_type, varying, has_pointers,
  pointee_routine, v1_stringifiable )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
    boolean in_header;        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type;
    boolean varying;
    boolean has_pointers;
    boolean pointee_routine;
    boolean v1_stringifiable;
#endif
{
    int i;
    int num_array_dimensions;
    AST_parameter_n_t *array_desc;

    fprintf( fid, "\n");
    CSPELL_ool_routine_name( fid, p_type, call_side, BE_marshalling_k, varying,
                             pointee_routine );
    if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
    {
        array_desc = (p_type->kind == AST_structure_k)
                       ? BE_Open_Array(flattened_type) : flattened_type;
        if ( ! AST_SMALL_SET(array_desc) )
            num_array_dimensions =
                                array_desc->type->type_structure.array->index_count;
    }
#if 0
    fprintf( fid, "\n#ifdef IDL_PROTOTYPES\n");
    fprintf( fid, "(\n");
    spell_name( fid, p_type->name );
    fprintf( fid, " *p_node,\n");
    if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
    {
        array_desc = (p_type->kind == AST_structure_k)
                       ? BE_Open_Array(flattened_type) : flattened_type;
        if ( AST_SMALL_SET(array_desc) )
        {
            if ( AST_CONFORMANT_SET(p_type) )
            {
                fprintf( fid, "ndr_ulong_int " );
                spell_name( fid, BE_Array_Info(array_desc)->size_var );
                fprintf( fid, ",\n" );
            }
            if (varying)
            {
                fprintf( fid, "ndr_ulong_int " );
                spell_name( fid, BE_Array_Info(array_desc)->count_var );
                fprintf( fid, ",\n" );
            }
        }
        else
        {
            num_array_dimensions =
                                array_desc->type->type_structure.array->index_count;
            for (i=0; i<num_array_dimensions; i++)
            {
                if ( AST_CONFORMANT_SET(p_type) )
                {
                    fprintf( fid, "ndr_ulong_int " );
                    spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                    fprintf( fid, ",\n" );
                }
                if ( varying )
                {
                    fprintf( fid, "ndr_ulong_int " );
                    spell_name( fid, BE_Array_Info(array_desc)->A[i] );
                    fprintf( fid, ",\n" );
                    fprintf( fid, "ndr_ulong_int " );
                    spell_name( fid, BE_Array_Info(array_desc)->B[i] );
                    fprintf( fid, ",\n" );
                }
            }
        }
    }
    if ( (call_side == BE_caller)  )
    {
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp\n");
    }
    fprintf( fid, ")\n" );
    fprintf( fid, "#else\n");
#endif

    if ( in_header )
    {
        fprintf( fid, "()\n");
    }
    else
    {
        /* Spell parameter list. */
        fprintf( fid, "( p_node, NIDL_msp" );
        if ( varying )
            fprintf( fid, ", NIDL_varying" );
        if ( v1_stringifiable )
            fprintf( fid, ", v1_string_flag" );
        if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
        {
            if ( AST_SMALL_SET(array_desc) )
            {
                if ( AST_CONFORMANT_SET(p_type) )
                {
                    fprintf( fid, ", " );
                    spell_name( fid, BE_Array_Info(array_desc)->size_var );
                }
                if (varying)
                {
                    fprintf( fid, ", " );
                    spell_name( fid, BE_Array_Info(array_desc)->count_var );
                }
            }
            else
            {
                for (i=0; i<num_array_dimensions; i++)
                {
                    if ( AST_CONFORMANT_SET(p_type) )
                    {
                        fprintf( fid, ", " );
                        spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                    }
                    if ( varying )
                    {
                        fprintf( fid, ", " );
                        spell_name( fid, BE_Array_Info(array_desc)->A[i] );
                        fprintf( fid, ", " );
                        spell_name( fid, BE_Array_Info(array_desc)->B[i] );
                    }
                }
            }
        }
        fprintf( fid, " )\n");

        /* Spell parameter declarations. */
        spell_name( fid, p_type->name );
        fprintf( fid, " *p_node;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp;\n" );
        if ( varying )
            fprintf( fid, "idl_boolean NIDL_varying;\n" );
        /*
         * Add a parameter to OOL marshalling routines for types that may
         * be V1_stringified but are not themselves marked with a V1_string
         * attribute.  [OOL routines for stringified types will marshall
         * their data as per string NDR without having to be so told by
         * the in-line call.]
         */
        if ( v1_stringifiable )
        {
            fprintf( fid, "idl_boolean v1_string_flag;\n" );
        }
        if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
        {
            if ( AST_SMALL_SET(array_desc) )
            {
                if ( AST_CONFORMANT_SET(p_type) )
                {
                    fprintf( fid, "ndr_ulong_int " );
                    spell_name( fid, BE_Array_Info(array_desc)->size_var );
                    fprintf( fid, ";\n" );
                }
                if (varying)
                {
                    fprintf( fid, "ndr_ulong_int " );
                    spell_name( fid, BE_Array_Info(array_desc)->count_var );
                    fprintf( fid, ";\n" );
                }
            }
            else
            {
                for (i=0; i<num_array_dimensions; i++)
                {
                    if ( AST_CONFORMANT_SET(p_type) )
                    {
                        fprintf( fid, "ndr_ulong_int " );
                        spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                        fprintf( fid, ";\n" );
                    }
                    if ( varying )
                    {
                        fprintf( fid, "ndr_ulong_int " );
                        spell_name( fid, BE_Array_Info(array_desc)->A[i] );
                        fprintf( fid, ";\n" );
                        fprintf( fid, "ndr_ulong_int " );
                        spell_name( fid, BE_Array_Info(array_desc)->B[i] );
                        fprintf( fid, ";\n" );
                    }
                }
            }
        }
    }
#if 0
    fprintf( fid, "#endif\n");
#endif
    if ( in_header )
    {
        fprintf( fid, ";\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Output the code to marshall an out_of_line type                         */
/*                                                                            */
/******************************************************************************/
static void CSPELL_marshall_ool_type
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side
)
#else
( fid, p_type, call_side )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
#endif
{
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type, *pointees;
    AST_parameter_n_t *tp;
    AST_operation_n_t dummy_oper;
    AST_parameter_n_t *elt;
    AST_parameter_n_t *array_desc;
    BE_dflags_t flags;
    int dummy_slots_used;
    boolean varying;
    boolean has_pointers;
    boolean v1_stringifiable;
    boolean routine_mode;   /* FALSE if mp and op have been set up for
                                in-line marshalling */
    int i;

    /*
     * ool types must be named types.  Even if the type has no varying instances
     * in this interface, it could have varying instances in another interface
     * that imports this one.  Thus, ool routines for arrays are always generic
     * so that they can handle a varying or non-varying instance; below we
     * always flatten an array as varying.
     */
    varying = (p_type->kind == AST_array_k
               || AST_STRING_SET(p_type) || AST_STRING0_SET(p_type));

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = varying ? AST_VARYING : 0;
    dummy_param.field_attrs = NULL;

    flattened_type = BE_flatten_parameter( &dummy_param,
                        NAMETABLE_add_id("(*p_node)"), p_type, BE_client_side,
                        true, &open_array, BE_F_OOL_RTN );
    BE_propagate_orecord_info( flattened_type );
    pointees = BE_yank_pointers( flattened_type );

    BE_flatten_field_attrs( flattened_type );
    for ( tp = flattened_type; tp != NULL; tp = tp->next)
    {
        flags = 0;
        if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
        {
            flags |= (BE_D_PARRAY | BE_D_OARRAYM);
        }
        if (p_type->kind == AST_array_k)
        {
            flags |= BE_D_PARRAYM ;
        }
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side,flags );
    }
    BE_flag_alignment( flattened_type, 1 );

    BE_flatten_field_attrs( pointees );
    for ( tp = pointees; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side, 0 );
    BE_flag_alignment( pointees, 1 );
    has_pointers = BE_any_pointers( flattened_type );
    v1_stringifiable = (varying
                  && AST_SMALL_SET(p_type)
                  && !AST_STRING0_SET(p_type)
                  && !has_pointers);

#ifdef DEBUG_VERBOSE
    if (BE_dump_mool)
    {
        printf("\nmool dump : CSPELL_marshall_ool_type\n");
        printf(  "------------------------------------\n");
        traverse(flattened_type, 2);
        traverse(pointees, 2);
    }
#endif

    /* Routine declaration */
    CSPELL_marshall_ool_type_decl( fid, p_type, call_side, false,
            flattened_type, varying, has_pointers, false, v1_stringifiable );
    fprintf( fid, "{\n");

    /* Declare local variables */
    CSPELL_local_var_decls( fid, &dummy_oper );
    fprintf( fid, "unsigned long space_for_node;\n");
    fprintf( fid, "rpc_mp_t mp;\n");
    fprintf( fid, "rpc_op_t op;\n\n");

    /* Do we need to change buffers before marshalling the fields? */
    fprintf( fid, "space_for_node=%s+7;\n", BE_list_size_exp(flattened_type) );
    fprintf( fid, "if (space_for_node > NIDL_msp->space_in_buff)\n");
    fprintf( fid, "{\n");
    fprintf( fid, "rpc_ss_marsh_change_buff(NIDL_msp,space_for_node);\n");
    CSPELL_return_on_status( fid );
    fprintf( fid, "}\n");

    /*
     * Omit the gap for singleton [ref] pointers
     */
    if (p_type->kind == AST_pointer_k && AST_REF_SET(p_type))
        ;
    else
    {
        /*
         * Marshall the fields of the structure
         */

        if ( AST_CONFORMANT_SET(p_type) )
        {
            /* Special processing of open record header */
            if (p_type->kind == AST_structure_k)
                array_desc = BE_Open_Array(flattened_type);
            else array_desc = flattened_type;
            fprintf(fid, "%s = %s;\n",
                              BE_get_name(BE_Array_Info(array_desc)->size_var),
                              BE_Array_Info(array_desc)->size_exp);
            if (varying)
            {
                fprintf( fid, "mp = NIDL_msp->mp;\n");
                fprintf( fid, "op = NIDL_msp->op;\n");
                routine_mode = false;
                BE_spell_varying_marshall( fid, array_desc, BE_M_NODE |
                        BE_M_OOL_RTN | BE_M_MAINTAIN_OP | BE_M_SLOTLESS );
            }
            else
            {
                fprintf(fid, "%s = %s;\n",
                              BE_get_name(BE_Array_Info(array_desc)->count_var),
                              BE_get_name(BE_Array_Info(array_desc)->size_var));
                routine_mode = true;
            }
            elt = flattened_type->next;
        }
        else
        {
            elt = flattened_type;
            routine_mode = true;
        }

        for ( ; elt; elt = elt->next)
        {
            if (BE_PI_Flags(elt) & BE_SKIP)
                continue;

            BE_marshall_param( fid, elt, BE_M_NODE | BE_M_ADVANCE_MP |
                 BE_M_ALIGN_MP | BE_M_OOL_RTN | BE_M_BUFF_EXISTS |
                 BE_M_SLOTLESS | BE_M_MAINTAIN_OP |
                 ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE), 0,
                               &dummy_slots_used, &routine_mode, 0 );
        }

        if (!routine_mode)
        {
            fprintf( fid, "NIDL_msp->space_in_buff -= (op - NIDL_msp->op);\n");
            fprintf( fid, "NIDL_msp->mp = mp;\n");
            fprintf( fid, "NIDL_msp->op = op;\n");
            routine_mode = true;
        }
    }
    fprintf( fid, "}\n");

    if (has_pointers)
    {
        CSPELL_marshall_ool_type_decl( fid, p_type, call_side, false,
                flattened_type, varying, true, true, false );
        fprintf( fid, "{\n");
        CSPELL_local_var_decls( fid, &dummy_oper );
        if ( AST_CONFORMANT_SET(p_type) )
        {
            /* Special processing of open record header */
            if (p_type->kind == AST_structure_k)
                array_desc = BE_Open_Array(flattened_type);
            else array_desc = flattened_type;
            fprintf(fid, "%s = %s;\n",
                              BE_get_name(BE_Array_Info(array_desc)->size_var),
                              BE_Array_Info(array_desc)->size_exp);
            if ( ! varying)
            {
                fprintf(fid, "%s = %s;\n",
                              BE_get_name(BE_Array_Info(array_desc)->count_var),
                              BE_get_name(BE_Array_Info(array_desc)->size_var));
            }
        }
        BE_ool_spell_varying_info( fid, flattened_type );
        for (elt = pointees; elt; elt = elt->next)
        {
            if (BE_PI_Flags(elt) & BE_SKIP)
                continue;

            BE_marshall_param( fid, elt, BE_M_NODE | BE_M_ADVANCE_MP |
                 BE_M_SLOTLESS | BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS |
                      BE_M_OOL_RTN |
                 ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE), 0,
                               &dummy_slots_used, &routine_mode, 0 );
        }
        fprintf( fid, "}\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Output the declaration part of a node unmarshalling routine             */
/*                                                                            */
/******************************************************************************/
void CSPELL_unmarshall_ool_type_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type,
    boolean varying,          /* TRUE if rtn is for [non]varying array */
    boolean has_pointers,     /* TRUE if the type includes any pointers */
    boolean pointee_routine,  /* TRUE for routines that unmarshall pointees */
    boolean v1_stringifiable  /* TRUE if v1_array && !v1_string */
)
#else
( fid, p_type, call_side, in_header, flattened_type, varying, has_pointers,
  pointee_routine, v1_stringifiable )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
    boolean in_header;        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *flattened_type;
    boolean varying;
    boolean has_pointers;
    boolean pointee_routine;
    boolean v1_stringifiable;
#endif
{
    int i;
    int num_array_dimensions;
    AST_parameter_n_t *array_desc;

    fprintf( fid, "\n");
    CSPELL_ool_routine_name( fid, p_type, call_side, BE_unmarshalling_k,
                            varying, pointee_routine );
    if ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
    {
        array_desc = (p_type->kind == AST_structure_k)
                       ? BE_Open_Array(flattened_type) : flattened_type;
        if ( ! AST_SMALL_SET(array_desc) )
            num_array_dimensions =
                                array_desc->type->type_structure.array->index_count;
    }
#if 0
    fprintf( fid, "\n#ifdef IDL_PROTOTYPES\n");
    fprintf( fid, "(\n");
    spell_name( fid, p_type->name );
    fprintf( fid, " *p_node,\n");
    if ( AST_CONFORMANT_SET(p_type) )
    {
        array_desc = (p_type->kind == AST_structure_k)
                       ? BE_Open_Array(flattened_type) : flattened_type;
        if ( AST_SMALL_SET(array_desc) )
        {
            fprintf( fid, "ndr_ulong_int " );
            spell_name( fid, BE_Array_Info(array_desc)->size_var );
            fprintf( fid, ",\n" );
        }
        else
        {
            num_array_dimensions =
                            array_desc->type->type_structure.array->index_count;
            for (i=0; i<num_array_dimensions; i++)
            {
                fprintf( fid, "ndr_ulong_int " );
                spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                fprintf( fid, ",\n" );
            }
        }
    }
    if ( (call_side == BE_caller) && (has_pointers) )
    {
        if ( pointee_routine )
        {
            fprintf( fid, "idl_boolean NIDL_new_node,\n" );
        }
    }
    if ( (call_side == BE_callee) && (pointee_routine) )
    {
        fprintf( fid, "rpc_ss_node_type_k_t NIDL_node_type,\n" );
    }
    fprintf( fid, "rpc_ss_marsh_state_t *p_unmar_params\n");
    fprintf( fid, ")\n" );
    fprintf( fid, "#else\n");
#endif

    if ( in_header )
    {
        fprintf( fid, "()\n");
    }
    else
    {
        /* Spell parameter list. */
        fprintf( fid, "( p_node" );
        if ( (call_side == BE_caller) && (has_pointers) )
        {
            if ( pointee_routine )
            {
                fprintf( fid, ", NIDL_new_node" );
            }
        }
        if ( (call_side == BE_callee) && (pointee_routine) )
        {
            fprintf( fid, ", NIDL_node_type" );
        }
        fprintf( fid, ", p_unmar_params" );
        if ( varying )
            fprintf( fid, ", NIDL_varying" );
        if ( v1_stringifiable )
            fprintf( fid, ", v1_string_flag" );
        if ( AST_CONFORMANT_SET(p_type) )
        {
            if ( AST_SMALL_SET(array_desc) )
            {
                fprintf( fid, ", " );
                spell_name( fid, BE_Array_Info(array_desc)->size_var );
            }
            else
            {
                for (i=0; i<num_array_dimensions; i++)
                {
                    fprintf( fid, ", " );
                    spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                }
            }
        }
        fprintf( fid, " )\n");

        /* Spell parameter declarations. */
        spell_name( fid, p_type->name );
        fprintf( fid, " *p_node;\n" );
        if ( (call_side == BE_caller) && (has_pointers) )
        {
            if ( pointee_routine )
            {
                fprintf( fid, "idl_boolean NIDL_new_node;\n" );
            }
        }
        if ( (call_side == BE_callee) && (pointee_routine) )
        {
            fprintf( fid, "rpc_ss_node_type_k_t NIDL_node_type;\n" );
        }
        fprintf( fid, "rpc_ss_marsh_state_t *p_unmar_params;\n" );
        if ( varying )
            fprintf( fid, "idl_boolean NIDL_varying;\n" );
        /*
         * Add a parameter to OOL unmarshalling routines for types that may
         * be V1_stringified but are not themselves marked with a V1_string
         * attribute.  [OOL routines for stringified types will unmarshall
         * their data as per string NDR without having to be so told by
         * the in-line call.]
         */
        if ( v1_stringifiable )
        {
            fprintf( fid, "idl_boolean v1_string_flag;\n" );
        }
        if ( AST_CONFORMANT_SET(p_type) )
        {
            if ( AST_SMALL_SET(array_desc) )
            {
                fprintf( fid, "ndr_ulong_int " );
                spell_name( fid, BE_Array_Info(array_desc)->size_var );
                fprintf( fid, ";\n" );
            }
            else
            {
                for (i=0; i<num_array_dimensions; i++)
                {
                   fprintf( fid, "ndr_ulong_int " );
                   spell_name( fid, BE_Array_Info(array_desc)->Z[i] );
                   fprintf( fid, ";\n" );
                }
            }
        }
    }
#if 0
    fprintf( fid, "#endif\n");
#endif
    if ( in_header )
    {
        fprintf( fid, ";\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Output the code to unmarshal a node of a specific type                  */
/*                                                                            */
/******************************************************************************/
static void CSPELL_unmarshall_ool_type
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be unmarshalled */
    BE_call_side_t call_side
)
#else
( fid, p_type, call_side )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be unmarshalled */
    BE_call_side_t call_side;
#endif
{
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type, *pointees;
    AST_parameter_n_t *tp;
    AST_operation_n_t dummy_oper;
    AST_parameter_n_t *elt;
    AST_parameter_n_t *array_desc;
    boolean varying;
    boolean has_pointers;
    boolean v1_stringifiable;
    boolean routine_mode = false;   /* Only used at top level */

    /*
     * ool types must be named types.  Even if the type has no varying instances
     * in this interface, it could have varying instances in another interface
     * that imports this one.  Thus, ool routines for arrays are always generic
     * so that they can handle a varying or non-varying instance; below we
     * always flatten an array as varying.
     */
    varying = (p_type->kind == AST_array_k
               || AST_STRING_SET(p_type) || AST_STRING0_SET(p_type));

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    (dummy_oper.be_info.oper)->local_vars = NULL;
    (dummy_oper.be_info.oper)->next_local_var = 0;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = varying ? AST_VARYING : 0;
    dummy_param.field_attrs = NULL;

    flattened_type = BE_flatten_parameter( &dummy_param,
                        NAMETABLE_add_id("(*p_node)"), p_type, BE_server_side,
                        true, &open_array, BE_F_OOL_RTN );
    BE_propagate_orecord_info( flattened_type );
    pointees = BE_yank_pointers( flattened_type );

    BE_flatten_field_attrs( flattened_type );
    for ( tp = flattened_type; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side,
            ( (p_type->kind == AST_array_k) || AST_CONFORMANT_SET(p_type) )
                         ? BE_D_PARRAY | BE_D_OARRAYU : 0 );
    BE_flag_alignment( flattened_type, 1 );

    BE_flatten_field_attrs( pointees );
    for ( tp = pointees; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side, 0 );
    BE_flag_alignment( pointees, 1 );
    has_pointers = BE_any_pointers( flattened_type );
    v1_stringifiable = (varying
                  && AST_SMALL_SET(p_type)
                  && !AST_STRING0_SET(p_type)
                  && !has_pointers);

#ifdef DEBUG_VERBOSE
    if (BE_dump_uool)
    {
        printf("\nuool dump : CSPELL_unmarshall_ool_type\n");
        printf(  "--------------------------------------\n");
        traverse(flattened_type, 2);
        traverse(pointees, 2);
    }
#endif

    /* Routine declaration */
    CSPELL_unmarshall_ool_type_decl( fid, p_type, call_side, false,
        flattened_type, varying, has_pointers, false, v1_stringifiable);
    fprintf( fid, "{\n");

    /* Declare local variables */
    CSPELL_local_var_decls( fid, &dummy_oper );
    if ( BE_any_pointers(flattened_type) )
    {
        fprintf( fid, "unsigned long node_number;\n");
    }
    fprintf( fid, "ndr_boolean buffer_changed;\n");

    /* Start of executable code */
    if ( p_type->kind == AST_structure_k)
    {
        if ( AST_CONFORMANT_SET(p_type) &&
            !AST_SMALL_SET(BE_Open_Array(flattened_type)))
        {
            spell_name(fid,
                       BE_Array_Info(BE_Open_Array(flattened_type))->size_var);
            fprintf(fid, "= %s;\n",
                BE_Array_Info(BE_Open_Array(flattened_type))->size_exp);
        }
        if ( AST_CONFORMANT_SET(p_type) )
        {
            fprintf(fid, "%s = %s;\n",
            BE_get_name(BE_Array_Info(BE_Open_Array(flattened_type))->count_var),
            BE_get_name(BE_Array_Info(BE_Open_Array(flattened_type))->size_var));
        }
    }

    if ( p_type->kind == AST_array_k)
    {
        if ( AST_CONFORMANT_SET(p_type) )
        {
            spell_name(fid, BE_Array_Info(flattened_type)->size_var);
            fprintf(fid, "= %s;\n", BE_Array_Info(flattened_type)->size_exp);
            if ( ! varying )
            {
                fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(flattened_type)->count_var),
                    BE_get_name(BE_Array_Info(flattened_type)->size_var));
            }
        }
    }

    /*
     * Omit the garbage part of singleton [ref] pointers
     */
    if (p_type->kind == AST_pointer_k && AST_REF_SET(p_type))
        ;
    else
    {
        /* Unmarshall the fields of the structure */
        for (elt = flattened_type; elt; elt = elt->next)
        {
            if (BE_PI_Flags(elt) & BE_SKIP)
                continue;

            BE_unmarshall_param(fid, elt,
                    BE_M_NODE | BE_M_CHECK_BUFFER | BE_M_CONVERT |
                    BE_M_ADVANCE_MP | BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS |
                    BE_M_ALIGN_MP | BE_M_OOL_RTN |
                    ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE),
                    &BE_unmarshall_node_names, &routine_mode );
        }
    }
    fprintf( fid, "}\n");

    /* Now for each pointer, unmarshall the pointee */
    if (has_pointers)
    {
        CSPELL_unmarshall_ool_type_decl( fid, p_type, call_side, false,
                                 flattened_type, varying, true, true, false );
        fprintf( fid, "{\n");
        CSPELL_local_var_decls( fid, &dummy_oper );
        if ( p_type->kind == AST_structure_k)
        {
            if ( AST_CONFORMANT_SET(p_type) &&
                !AST_SMALL_SET(BE_Open_Array(flattened_type)))
            {
                spell_name(fid,
                       BE_Array_Info(BE_Open_Array(flattened_type))->size_var);
                fprintf(fid, "= %s;\n",
                    BE_Array_Info(BE_Open_Array(flattened_type))->size_exp);
            }
            if ( AST_CONFORMANT_SET(p_type) )
            {
                fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info
                                  (BE_Open_Array(flattened_type))->count_var),
                    BE_get_name(BE_Array_Info
                                  (BE_Open_Array(flattened_type))->size_var));
            }
        }

        if ( p_type->kind == AST_array_k)
        {
            if ( AST_CONFORMANT_SET(p_type) )
            {
                spell_name(fid, BE_Array_Info(flattened_type)->size_var);
                fprintf(fid, "= %s;\n", BE_Array_Info(flattened_type)->size_exp);
                if ( ! varying )
                {
                    fprintf(fid, "%s = %s;\n",
                        BE_get_name(BE_Array_Info(flattened_type)->count_var),
                        BE_get_name(BE_Array_Info(flattened_type)->size_var));
                }
            }
        }
        BE_ool_spell_varying_info( fid, flattened_type );
        for (elt = pointees; elt; elt = elt->next)
        {
            if (BE_PI_Flags(elt) & BE_SKIP)
                continue;

            BE_unmarshall_param(fid, elt, BE_M_NODE | BE_M_CHECK_BUFFER |
                BE_M_CONVERT | BE_M_ADVANCE_MP |
                BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS | BE_M_OOL_RTN |
                ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE),
                &BE_unmarshall_node_names, &routine_mode );
        }
        fprintf( fid, "}\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Spell the routines for un/marshalling  out_of_line types                */
/*                                                                            */
/******************************************************************************/
void BE_out_of_line_routines
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface,
    BE_call_side_t call_side
)
#else
( fid, p_interface, call_side )
    FILE *fid;
    AST_interface_n_t *p_interface;
    BE_call_side_t call_side;
#endif
{
    AST_type_p_n_t *curr_ool_type_link;
                                    /* Position in list of out_of_line types */
    boolean routine_mode = false;   /* Only used at top level */

    for( curr_ool_type_link = p_interface->ool_types;
         curr_ool_type_link != NULL;
         curr_ool_type_link = curr_ool_type_link->next )
    {
        if (curr_ool_type_link->type->kind == AST_handle_k)
            continue;   /* Don't out of line types that resolve to handle_t */
        BE_push_malloc_ctx();
        NAMETABLE_set_temp_name_mode();
        /* Generate ool routines for non-pipes */
        if (curr_ool_type_link->type->kind != AST_pipe_k)
        {
             CSPELL_marshall_ool_type( fid, curr_ool_type_link->type, call_side );
             CSPELL_unmarshall_ool_type( fid, curr_ool_type_link->type, call_side );
        }
        /* Generate pipe ool routines on client side */
        else if (call_side == BE_caller)
        {
            CSPELL_marshall_in_pipe(fid, curr_ool_type_link->type, "(*p_node)",
                0, "iovec", true);
            CSPELL_unmarshall_out_pipe(fid, curr_ool_type_link->type,
                "(*p_node)", 0,
                "(p_unmar_params->mp)", "(p_unmar_params->op)",
                "(p_unmar_params->src_drep)", "(p_unmar_params->p_rcvd_data)",
                "(p_unmar_params->call_h)", "(*p_unmar_params->p_st)", true,
                &routine_mode);
        }
        NAMETABLE_clear_temp_name_mode();
        BE_pop_malloc_ctx();
    }
}
