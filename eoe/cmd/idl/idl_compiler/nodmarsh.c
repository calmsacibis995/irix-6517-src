/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodmarsh.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:06  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:38  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:55  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:43  devrcs
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
**      nodmarsh.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Generation of node marshalling code
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <be_pvt.h>
#include <bedeck.h>
#include <flatten.h>
#include <bounds.h>
#include <dutils.h>
#include <marshall.h>
#include <decorate.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <cstubgen.h>
#include <nodesupp.h>
#include <nodmarsh.h>
#include <backend.h>
#include <dflags.h>
#include <hdgen.h>
#include <localvar.h>
#include <mutils.h>

/******************************************************************************/
/*                                                                            */
/*    Output the declaration part of a node marshalling routine               */
/*                                                                            */
/******************************************************************************/
void CSPELL_marshall_node_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *array_desc,  /* not used unless rtn is for array type */
    boolean varying           /* TRUE if rtn is for fixed|varying array */
)
#else
( fid, p_type, call_side, in_header, array_desc, varying )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
    boolean in_header;        /* TRUE if decl to be emitted to header file */
    AST_parameter_n_t *array_desc;
    boolean varying;
#endif
{
    int i;
    int num_array_dimensions;

    fprintf( fid, "\n");
    CSPELL_pa_routine_name( fid, p_type, call_side, BE_marshalling_k, varying );
    if ( p_type->kind == AST_array_k )
    {
        num_array_dimensions = p_type->type_structure.array->index_count;
    }
    if ( in_header )
    {
        fprintf( fid, "()\n");
    }
    else
    {
        /* Spell parameter list. */
        fprintf( fid, "( p_node, NIDL_node_type, NIDL_msp" );
        if ( varying )
            fprintf( fid, ", NIDL_varying" );
        if ( p_type->kind == AST_array_k )
        {
            fprintf( fid, "\n" );
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
        fprintf( fid, ")\n");

        /* Spell parameter declarations. */
        CSPELL_typed_name( fid, p_type, NAMETABLE_NIL_ID, NULL, false, true );
        fprintf( fid, " *p_node;\n");
        fprintf( fid, "rpc_ss_node_type_k_t NIDL_node_type;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp;\n");
        if ( varying )
            fprintf( fid, "idl_boolean NIDL_varying;\n" );
        if ( p_type->kind == AST_array_k )
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
    if ( in_header )
    {
        fprintf( fid, ";\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Spell return if error after runtime call                                */
/*                                                                            */
/******************************************************************************/
#ifdef PROTO
void CSPELL_return_on_status (
    FILE *fid
)
#else
void CSPELL_return_on_status (fid)
    FILE *fid;
#endif
{
#ifdef NO_EXCEPTIONS
    fprintf(fid, "if (*st != error_status_ok) return;\n");
#endif
}

/****************************************************************************/
/*                                                                          */
/*    Output code to find the node number that corresponds to a pointer     */
/*    field and place the node number in local variable IDL_node_number     */
/*                                                                          */
/* **************************************************************************/
void CSPELL_get_node_number
#ifdef PROTO
(
    FILE *fid,
    char *field_name,             /* The name of the pointer field */
    char *node_number_access,     /* Text to be used to access
                                      "last node number used" */
    boolean in_operation,         /* FALSE if in marshalling routine */
                                  /* TRUE if in operation stub */
    BE_call_side_t call_side
)
#else
( fid, field_name, node_number_access, in_operation, call_side )
    FILE *fid;
    char *field_name;
    char *node_number_access;     /* Text to be used to access
                                      "last node number used" */
    boolean in_operation;
    BE_call_side_t call_side;
#endif
{
    fprintf( fid, "IDL_node_number = rpc_ss_register_node (\n");
    fprintf( fid, "  NIDL_msp->node_table, (byte_p_t)%s, ndr_false, NULL);\n",
             field_name);
}

/******************************************************************************/
/*                                                                            */
/*    Output the code to marshall a node of a specific type                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_marshall_node
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean declare_static,
    boolean in_aux
)
#else
( fid, p_type, call_side, declare_static, in_aux )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
    boolean declare_static;
    boolean in_aux;
#endif
{
    AST_parameter_n_t pointee_standin;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type, *pointees;
    AST_parameter_n_t *tp;
    AST_operation_n_t dummy_oper;
    AST_parameter_n_t *elt;
    BE_mflags_t flags;
    int dummy_slots_used;
    boolean varying;
    boolean routine_mode;   /* FALSE if mp and op have been set up for
                                in-line marshalling */

    /*
     * For array types, the emitted routine either handles only a fixed instance
     * of the array, or it handles a fixed|varying instance of an array, with
     * a parameter to determine whether the instance is fixed or varying.
     * If we are emitting code into an _aux file we can make no assumptions
     * about usage, and must emit the more general latter case.  If we are
     * emitting code into a _stub file we may emit the former case.
     */
    varying = (AST_STRING_SET(p_type) || AST_STRING0_SET(p_type) ||
               (p_type->kind == AST_array_k &&
                (  (in_aux && !declare_static)
                || (call_side == BE_caller && AST_IN_VARYING_SET(p_type))
                || (call_side == BE_callee && AST_OUT_VARYING_SET(p_type)))));

    /* Prepare to emit code */
    pointee_standin.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    pointee_standin.uplink = &dummy_oper;
    pointee_standin.type = p_type;
    pointee_standin.flags = varying ? AST_VARYING : 0;
    pointee_standin.flags |=
        (p_type->flags & (AST_REF | AST_PTR | AST_UNIQUE | AST_STRING | AST_STRING0));
    pointee_standin.field_attrs = NULL;

    flattened_type = BE_flatten_parameter( &pointee_standin,
                        NAMETABLE_add_id("(*p_node)"), p_type, BE_client_side,
                        true, &open_array, BE_F_PA_RTN );
    BE_propagate_orecord_info( flattened_type );
    pointees = BE_yank_pointers( flattened_type );

    BE_flatten_field_attrs( flattened_type );
    for ( tp = flattened_type; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side,
            p_type->kind == AST_array_k ? BE_D_PARRAY | BE_D_PARRAYM : 0 );
    BE_flag_alignment( flattened_type, 1 );

    BE_flatten_field_attrs( pointees );
    for ( tp = pointees; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side, 0 );
    BE_flag_alignment( pointees, 1 );
    if ( (pointees != NULL) && (AST_OUT_OF_LINE_SET(p_type)) )
    {
        /* BE_OOL and BE_SKIP are inherited from flattened_type to pointees */
        BE_PI_Flags(pointees) &= (~BE_SKIP);
        BE_PI_Flags(pointees) |= BE_OOL;
        if (BE_PI_Flags(flattened_type) & BE_OPEN_RECORD)
                BE_Orecord_Info(pointees) = BE_Orecord_Info(flattened_type);
    }

#ifdef DEBUG_VERBOSE
    if (BE_dump_mnode)
    {
        printf("\nmnode dump : CSPELL_marshall_node\n");
        printf(  "---------------------------------\n");
        traverse(flattened_type, 2);
        traverse(pointees, 2);
    }
#endif

    /* Routine declaration */
    if ( declare_static ) fprintf( fid, "\nstatic void ");
    CSPELL_marshall_node_decl( fid, p_type, call_side, false,
                               flattened_type, varying );
    fprintf( fid, "{\n");

    /* Declare local variables */
    CSPELL_local_var_decls( fid, &dummy_oper );
    fprintf( fid, "long NIDL_already_marshalled;\n");
    fprintf( fid, "unsigned long space_for_node;\n");
    fprintf( fid, "rpc_mp_t mp;\n");
    fprintf( fid, "rpc_op_t op;\n\n");

    /* Don't marshall anything for NULL */
    fprintf( fid, "if(p_node==NULL)return;\n" );

    /*
     *  For mutable nodes Lookup the node in the node table.  If we have
     *  already marshalled this node then don't do it again, just return.
     */
    fprintf( fid, "if (NIDL_node_type == rpc_ss_mutable_node_k) {\n");
    fprintf( fid,
"  rpc_ss_register_node(NIDL_msp->node_table,(byte_p_t)p_node,idl_true,&NIDL_already_marshalled);\n" );
    fprintf( fid, "  if(NIDL_already_marshalled)return;\n" );
    fprintf( fid, "}\n");

    /*
     * Do to_xmit/from_local calls so that the space for node can be
     * calculated properly for conformant and varying type structures which
     * contain the size based on the values in the net rep.
     */
    flags = 0;
    for (elt = flattened_type; elt; elt = elt->next)
    {
        if (BE_Calls_Before(elt))
        {
            BE_spell_from_locals(fid, elt);
            BE_spell_to_xmits(fid, elt);
            flags |= BE_M_XMIT_CVT_DONE;
        }
    }

    /* Do we need to change buffers before marshalling the fields? */
    fprintf( fid, "space_for_node=%s+7;\n", BE_list_size_exp(flattened_type) );
    fprintf( fid, "if (space_for_node > NIDL_msp->space_in_buff)\n");
    fprintf( fid, "{\n");
    fprintf( fid, "rpc_ss_marsh_change_buff(NIDL_msp,space_for_node);\n");
    CSPELL_return_on_status( fid );
    fprintf( fid, "}\n");

    routine_mode = true;
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
        for (elt = flattened_type; elt; elt = elt->next)
        {
            if ( ! (BE_PI_Flags(elt) & BE_SKIP) )
            {
                BE_marshall_param( fid, elt, BE_M_NODE | BE_M_ADVANCE_MP |
                     BE_M_SLOTLESS | BE_M_MAINTAIN_OP | flags |
                     BE_M_BUFF_EXISTS |
                     ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE), 0,
                                   &dummy_slots_used, &routine_mode, 0 );
            }
        }
        if ( ! routine_mode )
        {
            fprintf( fid, "NIDL_msp->space_in_buff -= (op - NIDL_msp->op);\n");
            fprintf( fid, "NIDL_msp->mp = mp;\n");
            fprintf( fid, "NIDL_msp->op = op;\n");
            routine_mode = true;
        }
    }

    for (elt = pointees; elt; elt = elt->next)
    {
        if ( ! (BE_PI_Flags(elt) & BE_SKIP) )
            BE_marshall_param( fid, elt, BE_M_NODE | BE_M_ADVANCE_MP |
                 BE_M_SLOTLESS | BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS |
                 ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE), 0,
                               &dummy_slots_used, &routine_mode, 0 );
    }

    fprintf( fid, "}\n");
}

/******************************************************************************/
/*                                                                            */
/*    Control of generation of node marshalling routines                      */
/*                                                                            */
/******************************************************************************/
void BE_gen_node_marshalling
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
    AST_type_p_n_t *curr_sp_type_link;  /* Position in list of node types */

    /*
     *  Some of the routines generated into the aux file will be static, those
     *  should have a mini-prototype emitted declaring them as static void
     *  to prevent errors if they are forward referenced.
     */
    for( curr_sp_type_link = p_interface->sp_types;
         curr_sp_type_link != NULL;
         curr_sp_type_link = curr_sp_type_link->next )
    {
        if (! AST_SELF_POINTER_SET(curr_sp_type_link->type))
        {
            fprintf(fid,"static void ");
            CSPELL_pa_routine_name( fid, curr_sp_type_link->type, call_side,
                BE_marshalling_k, true );
            fprintf(fid,"();\n");
        }
    }

    /*
     *  Generate marshalling routines for self-pointing nodes and types
     *  pointed-at by self-pointing types.
     */
    for( curr_sp_type_link = p_interface->sp_types;
         curr_sp_type_link != NULL;
         curr_sp_type_link = curr_sp_type_link->next )
    {
        BE_push_malloc_ctx();
        NAMETABLE_set_temp_name_mode();
        if (curr_sp_type_link->type->name == NAMETABLE_NIL_ID)
        {
            CSPELL_type_def( fid,
                    curr_sp_type_link->type->be_info.type->clone, false );
            CSPELL_marshall_node( fid,
                        curr_sp_type_link->type->be_info.type->clone,
                        call_side, true, true );
        }
        else
        {
            CSPELL_marshall_node( fid, curr_sp_type_link->type, call_side,
                          ( ! AST_SELF_POINTER_SET(curr_sp_type_link->type) ),
                          true );
        }
        NAMETABLE_clear_temp_name_mode();
        BE_pop_malloc_ctx();
    }
}
