/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nodunmar.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:41:11  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:45  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:50:34  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:03  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:44  devrcs
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
**
**  NAME:
**
**      nodunmar.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Generation of node unmarshalling code
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <be_pvt.h>
#include <bedeck.h>
#include <flatten.h>
#include <bounds.h>
#include <dflags.h>
#include <marshall.h>
#include <decorate.h>
#include <munmarsh.h>
#include <mutils.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <nodesupp.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <backend.h>
#include <dutils.h>
#include <localvar.h>

BE_mn_t BE_unmarshall_node_names =
{
    "p_unmar_params->mp",
    "p_unmar_params->op",
    "p_unmar_params->src_drep",
    "p_unmar_params->p_rcvd_data",
    "(*p_unmar_params->p_st)",
    "p_unmar_params->p_mem_h",
    "p_unmar_params->call_h",
    "RAISE(rpc_x_ss_pipe_comm_error);"
};

/******************************************************************************/
/*                                                                            */
/*    Output the declaration part of a node unmarshalling routine             */
/*                                                                            */
/******************************************************************************/
void CSPELL_unmarshall_node_decl
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be marshalled */
    BE_call_side_t call_side,
    boolean in_header,        /* TRUE if decl to be emitted to header file */
    boolean varying           /* TRUE for rtn is for fixed|varying array */
)
#else
( fid, p_type, call_side, in_header, varying )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be marshalled */
    BE_call_side_t call_side;
    boolean in_header;        /* TRUE if decl to be emitted to header file */
    boolean varying;
#endif
{
    fprintf( fid, "\n");
    CSPELL_pa_routine_name( fid, p_type, call_side, BE_unmarshalling_k,
                            varying );
    if ( in_header )
    {
        fprintf( fid, "()\n");
    }
    else
    {
        /* Spell parameter list. */
        fprintf( fid, "( p_referred_to_by, NIDL_node_type, p_unmar_params");
        if ( varying )
            fprintf( fid, ", NIDL_varying" );
        fprintf( fid, " )\n" );

        /* Spell parameter declarations. */
        CSPELL_typed_name( fid, p_type, NAMETABLE_NIL_ID, NULL, false, true );
        fprintf( fid, " **p_referred_to_by;\n");
        fprintf( fid, "rpc_ss_node_type_k_t NIDL_node_type;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *p_unmar_params;\n");
        if ( varying )
            fprintf( fid, "idl_boolean NIDL_varying;\n" );
    }
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
void CSPELL_unmarshall_node
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,     /* Type of node to be unmarshalled */
    BE_call_side_t call_side,
    boolean declare_static,
    boolean in_aux
)
#else
( fid, p_type, call_side, declare_static, in_aux )
    FILE *fid;
    AST_type_n_t *p_type;     /* Type of node to be unmarshalled */
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
    boolean varying;
    boolean routine_mode = false;   /* Only applies at top-level in
                                        unmarshalling */

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
                || (call_side == BE_callee && AST_IN_VARYING_SET(p_type))
                || (call_side == BE_caller && AST_OUT_VARYING_SET(p_type)))));

    /* Prepare to emit code */
    pointee_standin.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    (dummy_oper.be_info.oper)->local_vars = NULL;
    (dummy_oper.be_info.oper)->next_local_var = 0;
    dummy_oper.be_info.oper->flags = 0;
    pointee_standin.uplink = &dummy_oper;
    pointee_standin.type = p_type;
    pointee_standin.flags = varying ? AST_VARYING : 0;
    pointee_standin.flags |= (p_type->flags
        & (AST_REF | AST_PTR | AST_UNIQUE | AST_STRING | AST_STRING0));
    pointee_standin.field_attrs = NULL;

    flattened_type = BE_flatten_parameter( &pointee_standin,
                        NAMETABLE_add_id("(*p_node)"), p_type, BE_server_side,
                        true, &open_array, BE_F_PA_RTN );
    BE_propagate_orecord_info( flattened_type );
    pointees = BE_yank_pointers( flattened_type );

    BE_flatten_field_attrs( flattened_type );
    for ( tp = flattened_type; tp != NULL; tp = tp->next)
        BE_decorate_parameter( &dummy_oper, tp, BE_client_side,
            p_type->kind == AST_array_k ? BE_D_PARRAY : 0 );
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
    if (BE_dump_unode)
    {
        printf("\nunode dump : CSPELL_unmarshall_node\n");
        printf(  "-----------------------------------\n");
        traverse(flattened_type, 2);
        traverse(pointees, 2);
    }
#endif

    /* Routine declaration */
    if ( declare_static ) fprintf( fid, "\nstatic void " );
    CSPELL_unmarshall_node_decl( fid, p_type, call_side, false, varying );
    fprintf( fid, "{\n");

    /* Declare local variables */
    CSPELL_local_var_decls( fid, &dummy_oper );
    CSPELL_typed_name( fid, p_type, NAMETABLE_NIL_ID, NULL, false, true );
    fprintf( fid, " *p_node = NULL;\n");
    fprintf( fid, "long NIDL_already_unmarshalled = 0;\n");
    if ( call_side == BE_callee )
    {
        fprintf( fid, "unsigned long *p_stored_node_number;\n");
    }
    else
    {
        if ( BE_any_pointers( flattened_type ) )
        {
            fprintf( fid, "long NIDL_new_node = (long)idl_false;\n");
        }
    }
    fprintf( fid, "unsigned long node_size;\n");
    fprintf( fid, "unsigned long node_number;\n");
    fprintf( fid, "ndr_boolean buffer_changed;\n");

    /* Start of executable code */

    /* For a mutable node get the node number */
    fprintf( fid, "if ( NIDL_node_type == rpc_ss_mutable_node_k )\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "node_number = (unsigned long)*p_referred_to_by;\n" );
    fprintf( fid, "if(node_number==0)return;\n" );
    /*
     * A separate lookup is required for conformant objects because we don't
     * know the size of the object until we unmarshall the Z value but the Z
     * value won't be there if the node has already been unmarshalled.
     */
    if ( AST_CONFORMANT_SET(p_type) )
    {
        fprintf( fid, "p_node = ");
        CSPELL_cast_exp( fid, BE_pointer_type_node(p_type));
        fprintf( fid, "  rpc_ss_lookup_pointer_to_node(\n");
        fprintf( fid, "  p_unmar_params->node_table, node_number, &NIDL_already_unmarshalled);\n");
        fprintf( fid, "*p_referred_to_by = p_node;\n");
        fprintf( fid, "if (NIDL_already_unmarshalled) return;\n");
    }
    fprintf( fid, "}\n" );

    /* Get the address the node will occupy */
    if ( call_side == BE_callee )
    {
        fprintf( fid, "if ( NIDL_node_type == rpc_ss_old_ref_node_k )\n" );
        /* Existing [ref] node. Set pointer to storage */
        fprintf( fid, " p_node = *p_referred_to_by;\n" );

        /* If node doesn't exist, then allocate */
        fprintf( fid, "else if ( p_node == NULL )\n" );
        fprintf( fid, "{\n" );

        /* Get storage for the node */
        if ( p_type->kind == AST_array_k )
        {
            /*
             * Conformant array, just need array size
             */
            if ( AST_CONFORMANT_SET(p_type) )
            {
                BE_spell_conformant_unmarshall( fid,
                   flattened_type, BE_M_ALIGN_MP | BE_M_MAINTAIN_OP |
                                   BE_M_CONVERT | BE_M_CHECK_BUFFER,
                   &BE_unmarshall_node_names );
                fprintf( fid, "node_size = %s * sizeof",
                         BE_get_name(BE_Array_Info(flattened_type)->size_var) );
                CSPELL_cast_exp(fid,
                    flattened_type->type->type_structure.array->element_type);
            }
            else
            {
                fprintf( fid, "node_size = sizeof" );
                CSPELL_cast_exp( fid, p_type );
            }
            fprintf(fid, ";\n");
        }
        else
        {
            /*
             * Conformant structure, need array size plus structure size.
             */
            fprintf( fid, "node_size = sizeof" );
            CSPELL_cast_exp( fid, p_type );
            fprintf( fid, ";\n" );
            if ( AST_CONFORMANT_SET(p_type) )
            {
                /* Unmarshall the conformance information */
                BE_spell_conformant_unmarshall( fid,
                   BE_Open_Array(flattened_type), BE_M_ALIGN_MP |
                       BE_M_MAINTAIN_OP | BE_M_CONVERT | BE_M_CHECK_BUFFER,
                   &BE_unmarshall_node_names );

                /*
                 *   Add in the size of the array part (Z * sizeof(array
                 *   element)) - sizeof(array element)
                 */
                fprintf(fid, "node_size+=((");
                if ( AST_SMALL_SET(BE_Open_Array(flattened_type)) )
                {
                    spell_name(fid, BE_Array_Info(BE_Open_Array(flattened_type))
                                    ->size_var);
                }
                else
                {
                    fprintf(fid, "%s",
                             BE_Array_Info(BE_Open_Array(flattened_type))
                                ->size_exp);
                }
                fprintf( fid,")*sizeof" );
                CSPELL_cast_exp( fid, BE_Open_Array(flattened_type)->type
                                        ->type_structure.array->element_type );
                /* Subtract out the one array element instance in the struct */
                fprintf(fid, ")-sizeof");
                CSPELL_cast_exp( fid, BE_Open_Array(flattened_type)->type
                                        ->type_structure.array->element_type );
                fprintf(fid, " ;\n");
            }
        }

        /* Allocate storage for the node ([ptr] or new ref pointer). */
        fprintf( fid, "if (NIDL_node_type == rpc_ss_mutable_node_k)\n");
        fprintf( fid, "p_node = ");
        CSPELL_cast_exp( fid, BE_pointer_type_node(p_type));
        fprintf( fid, "rpc_ss_return_pointer_to_node(\n");
        fprintf( fid, "    p_unmar_params->node_table, node_number, node_size,\n");
        fprintf( fid, "    NULL, &NIDL_already_unmarshalled, (long *)NULL);\n");
        fprintf( fid, "else\n");
        fprintf( fid, "p_node = " );
        CSPELL_cast_exp( fid, BE_pointer_type_node(p_type));
        fprintf( fid, "rpc_ss_mem_alloc(\n" );
        fprintf( fid, "    p_unmar_params->p_mem_h, node_size );\n" );

        /* Make pointer point at pointee */
        fprintf( fid, "*p_referred_to_by = p_node;\n");
        fprintf( fid, "if (NIDL_already_unmarshalled) return;\n");

        fprintf( fid, "}\n" );
    }
    else
    {   /* Caller side processing. */

        /* Is storage for this node already allocated? */
        fprintf( fid, "if ( NIDL_node_type == rpc_ss_old_ref_node_k )" );
        fprintf( fid, " p_node = *p_referred_to_by;\n" );

        if ( p_type->kind == AST_array_k )
        {
            if ( AST_CONFORMANT_SET(p_type) )
            {
                BE_spell_conformant_unmarshall( fid,
                   flattened_type,
                   BE_M_ALIGN_MP | BE_M_MAINTAIN_OP | BE_M_CONVERT |
                   BE_M_ADVANCE_MP | BE_M_CHECK_BUFFER,
                   &BE_unmarshall_node_names );
            }
        }
        else
        {
            if ( AST_CONFORMANT_SET(p_type) )
            {
                BE_spell_conformant_unmarshall( fid,
                   BE_Open_Array(flattened_type),
                   BE_M_ALIGN_MP | BE_M_MAINTAIN_OP | BE_M_CONVERT |
                   BE_M_ADVANCE_MP | BE_M_CHECK_BUFFER,
                   &BE_unmarshall_node_names );
            }
        }

        fprintf( fid, "if (p_node == NULL)\n");
        fprintf( fid, "{\n" );

        /* Node size calculation */
        fprintf( fid, "node_size =" );
        if ( p_type->kind == AST_array_k )
        {
            /*
             * Conformant array, just need the array size
             */
            if ( AST_CONFORMANT_SET(p_type) )
                fprintf( fid, " %s",
                         BE_get_name(BE_Array_Info(flattened_type)->size_var) );
            else
                fprintf( fid, " %d", BE_Array_Info(flattened_type)->num_elts );
            fprintf( fid, "*sizeof" );
            CSPELL_cast_exp( fid,
                    flattened_type->type->type_structure.array->element_type);
        }
        else
        {
            /*
             * Conformant structure, need size of structure and array together.
             *   The expression is sizeof(struct) - sizeof(array element) +
             *   (Z * sizeof(array element))
             */
            fprintf( fid, "    sizeof" );
            CSPELL_cast_exp( fid, p_type );
            /* This is a node that was constructed by the callee */
            if (  AST_CONFORMANT_SET(p_type) )
            {
                /* Subtract out the one array element instance in the struct */
                fprintf(fid, "-sizeof");
                CSPELL_cast_exp( fid, BE_Open_Array(flattened_type)->type
                                        ->type_structure.array->element_type );

                /* Add in the actual array size */
                fprintf(fid, "+(");
                if ( AST_SMALL_SET(BE_Open_Array(flattened_type) ) )
                {
                    spell_name(fid, BE_Array_Info(BE_Open_Array(flattened_type))
                                ->size_var);
                }
                else
                {
                    fprintf(fid, "%s",
                         BE_Array_Info(BE_Open_Array(flattened_type))
                                ->size_exp);
                }
                /* * sizeof (array element) */
                fprintf( fid,")*sizeof" );
                CSPELL_cast_exp( fid, BE_Open_Array(flattened_type)->type
                                        ->type_structure.array->element_type );
            }
        }
        fprintf( fid, ";\n");
        /* End of node size calcuation */

        /* Allocate storage for the node. */
        fprintf( fid, "if (NIDL_node_type == rpc_ss_mutable_node_k)\n");
        fprintf( fid, "  p_node = ");
        CSPELL_cast_exp( fid, BE_pointer_type_node(p_type));
        fprintf( fid, "rpc_ss_return_pointer_to_node(\n");
        fprintf( fid, "      p_unmar_params->node_table, node_number, node_size,\n");
        fprintf( fid, "      p_unmar_params->p_allocate, &NIDL_already_unmarshalled, ");
        if ( BE_any_pointers( flattened_type ) )
            fprintf( fid, "&NIDL_new_node);\n" );
        else
            fprintf( fid, "(long *)NULL);\n" );
        fprintf( fid, "else\n");
        fprintf( fid, "{\n" );
        if ( BE_any_pointers( flattened_type ) )
            fprintf( fid, "NIDL_new_node = (long)idl_true;\n" );
        fprintf( fid, "  p_node = " );
        CSPELL_cast_exp( fid, BE_pointer_type_node(p_type));
        fprintf( fid, "(*(p_unmar_params->p_allocate))(node_size);\n" );
        fprintf( fid, "}\n" );
        fprintf( fid, "if (p_node == NULL) RAISE(rpc_x_no_memory);\n");
        fprintf( fid, "}\n");

        /* Make pointer point at pointee */
        fprintf( fid, "*p_referred_to_by = p_node;\n");
        fprintf( fid, "if (NIDL_already_unmarshalled) return;\n");

    }

    /* On callee side, may be doing allocation only */
    if ( ( call_side == BE_callee ) && ( ! AST_CONFORMANT_SET(p_type) ) )
    {
        fprintf( fid, "if ( NIDL_node_type == rpc_ss_alloc_ref_node_k )\n" );
        fprintf( fid, "{\n" );
        BE_spell_alloc_ref_pointers( fid, pointees, BE_arp_before, true );
        fprintf( fid, "return;\n");
        fprintf( fid, "}\n" );
    }

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
            if ( ! (BE_PI_Flags(elt) & BE_SKIP) )
                BE_unmarshall_param(fid, elt,
                    BE_M_NODE | BE_M_CHECK_BUFFER | BE_M_CONVERT
                    | BE_M_ADVANCE_MP | BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS |
                    ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE),
                    &BE_unmarshall_node_names, &routine_mode );
        }
    }

    /* Now for each pointer, unmarshall the pointee */
    for (elt = pointees; elt; elt = elt->next)
    {
        if ( ! (BE_PI_Flags(elt) & BE_SKIP) )
            BE_unmarshall_param(fid, elt, BE_M_NODE | BE_M_CHECK_BUFFER |
                BE_M_CONVERT
                | BE_M_ADVANCE_MP | BE_M_MAINTAIN_OP | BE_M_BUFF_EXISTS |
                ((call_side==BE_caller) ? BE_M_CALLER : BE_M_CALLEE),
                &BE_unmarshall_node_names, &routine_mode );
    }

    fprintf( fid, "}\n");
}

/******************************************************************************/
/*                                                                            */
/*    Control of generation of node unmarshalling routines                    */
/*                                                                            */
/******************************************************************************/
void BE_gen_node_unmarshalling
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
     * Generate mini-prototypes into the aux file for those routines that
     * are static to the aux to prevent errors if they are forward referenced.
     */
    for( curr_sp_type_link = p_interface->sp_types;
         curr_sp_type_link != NULL;
         curr_sp_type_link = curr_sp_type_link->next )
    {
        if (! AST_SELF_POINTER_SET(curr_sp_type_link->type))
        {
            fprintf(fid,"static void ");
            CSPELL_pa_routine_name( fid, curr_sp_type_link->type, call_side,
                BE_unmarshalling_k, true );
            fprintf(fid,"();\n");
        }
    }


    /*
     * Generate unmarshalling routines into the aux file.
     */
    for( curr_sp_type_link = p_interface->sp_types;
         curr_sp_type_link != NULL;
         curr_sp_type_link = curr_sp_type_link->next )
    {
        if (curr_sp_type_link->type->name == NAMETABLE_NIL_ID)
        {
            /* The type of the clone is spelt by BE_gen_node_marshalling */
            CSPELL_unmarshall_node( fid,
                        curr_sp_type_link->type->be_info.type->clone,
                        call_side, true, true );
        }
        else
        {
            CSPELL_unmarshall_node( fid, curr_sp_type_link->type, call_side,
                          ( ! AST_SELF_POINTER_SET(curr_sp_type_link->type) ),
                          true );
        }
    }
}
