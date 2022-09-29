/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: outpipes.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.4  1993/02/05  14:49:59  sanders
 * 	Added code to CSPELL_unmarshall_out_pipe to correctly
 * 	generate code for string output pipes - OT 4678
 * 	[1993/02/05  14:46:55  sanders]
 *
 * Revision 1.1.2.3  1993/01/03  21:41:21  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:38:02  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  18:50:58  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:22  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:48  devrcs
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
**      outpipes.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Code generation for [out] pipe parameters
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <cstubgen.h>
#include <nametbl.h>
#include <flatten.h>
#include <decorate.h>
#include <dutils.h>
#include <marshall.h>
#include <genpipes.h>
#include <outpipes.h>
#include <backend.h>
#include <oolrtns.h>
#include <localvar.h>
#include <mmarsh.h>
#include <munmarsh.h>
#include <mutils.h>

static BE_mn_t BE_unmarshall_c_pipe_names =
{
    "mp",
    "op",
    "drep",
    "elt",
    "st",
    "&mem_handle",
    "call_h",
    "goto closedown;"
};

/******************************************************************************/
/*                                                                            */
/*    Generate declarations needed for caller's [out] pipes                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_caller_out_pipe_decls
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *p_operation
)
#else
( fid, p_operation )
    FILE *fid;
    AST_operation_n_t *p_operation;
#endif
{
    AST_parameter_n_t *p_parameter;
    int out_pipe_count;

    if ( ! AST_HAS_OUT_PIPES_SET(p_operation) ) return;

    fprintf ( fid, "unsigned long left_in_wire_array=0;\n" );
    fprintf ( fid, "unsigned long supplied_size;\n" );
    out_pipe_count = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        /* For each [in] pipe parameter we need a chunk pointer */
        if (AST_OUT_SET(p_parameter)
             && ( (p_parameter->type->kind == AST_pipe_k)
                 || ((p_parameter->type->kind == AST_pointer_k)
                    && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) ) )
        {
            out_pipe_count++;
            CSPELL_pipe_base_type_exp( fid, p_parameter->type );
            fprintf( fid, " *out_pipe_chunk_ptr_%d;\n", out_pipe_count );
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*    Generate code to unmarshall [out] pipe parameters                       */
/*                                                                            */
/******************************************************************************/
void CSPELL_unmarshall_out_pipe
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,
    char *pipe_struct_name,
    int pipe_num,
    char *mp_name,
    char *op_name,
    char *drep_name,
    char *elt_name,
    char *call_h_name,
    char *st_name,
    boolean do_ool,
    boolean *p_routine_mode
)
#else
( fid, p_type, pipe_struct_name, pipe_num,  mp_name, op_name, drep_name,
  elt_name, call_h_name, st_name, do_ool, p_routine_mode )
    FILE *fid;
    AST_type_n_t *p_type;
    char *pipe_struct_name;
    int pipe_num;
    char *mp_name;
    char *op_name;
    char *drep_name;
    char *elt_name;
    char *call_h_name;
    char *st_name;
    boolean do_ool;
    boolean *p_routine_mode;
#endif
{
    AST_parameter_n_t *p_parameter;
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type;
    AST_operation_n_t dummy_oper;
    AST_parameter_n_t *elt;
    char chunk_ptr_string[MAX_ID+1];
    BE_mn_t pipe_marshalling_names;
    int index_count, slice;

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = 0;
    dummy_param.field_attrs = NULL;
    dummy_param.type = p_type;

    /* Generate the local name for the pointer to the pipe data. */
    sprintf(chunk_ptr_string, "out_pipe_chunk_ptr_%d", pipe_num);

    /*
     * If we are generating the client ool unmarshalling routine for the pipe,
     * we need to first decorate the parameter as a pipe.
     */
    if (do_ool)
    {
        /*
         * Flatten the pipe and decorate it which causes the internal conformant
         * array chunk to be decorated and unmarshalling vars to be allocated.
         */
        flattened_type = BE_flatten_parameter(&dummy_param,
            NAMETABLE_add_id(chunk_ptr_string), p_type, BE_client_side, true,
            &open_array, 0);
        BE_undec_piped_arrays(p_type->type_structure.pipe->be_info.pipe->flat);
        BE_decorate_parameter(&dummy_oper, flattened_type, BE_client_side, 0);
    }

    /*
     * Flatten the pipe which will give us a conformant array for a pipe chunk.
     */
    flattened_type = BE_flatten_parameter(&dummy_param,
        NAMETABLE_add_id(chunk_ptr_string), p_type, BE_client_side, true,
        &open_array, BE_F_PA_RTN | BE_F_OOL_RTN | BE_F_PIPE_ROUTINE);
    BE_Array_Info(flattened_type)->first_element_exp =
        BE_first_element_expression(flattened_type);

    if (do_ool)
    {
        /* Routine declaration. */
        CSPELL_unmarshall_ool_type_decl(fid, p_type, BE_caller, false,
            flattened_type, false, false, false, false);
        fprintf(fid, "{\n");

        /* Declare local variables for pipe. */
        fprintf(fid, "unsigned long left_in_wire_array=0;\n");
        CSPELL_pipe_base_type_exp(fid, p_type);
        fprintf(fid, " *%s;\n", chunk_ptr_string);
        fprintf(fid, "unsigned long supplied_size;\n");
        fprintf(fid, "ndr_boolean buffer_changed;\n");

        /* Declare local variables gen'd by decoration. */
        CSPELL_local_var_decls(fid, &dummy_oper);
    }

    /* Loop while pipe data is available and push into user buffer. */
    fprintf( fid, "\nwhile(ndr_true)\n" );
    fprintf( fid, "{\n" );

    /* Do we have any data pending to hand off to the user? */
    fprintf(fid, "if(left_in_wire_array==0)\n");
    fprintf(fid, "{\n");

    /* If not, get the next chunk */

    /* Unmarshall pipe_chunk_size.  Do alignment first.  Since buffers
     * are multiples of 8-bytes, this align can never go past the end of 
     * the buffer.  After the alignment, we may need to get a new buffer,
     * but it will be 8-byte aligned already.
     */
    fprintf(fid, "rpc_align_mop(%s,%s,4);\n", mp_name, op_name);
    fprintf(fid, "if((byte_p_t)%s - %s->data_addr >= %s->data_len)\n",
        mp_name, elt_name, elt_name);
    fprintf(fid, "{\n");
    fprintf(fid, "rpc_ss_new_recv_buff(%s,(rpc_call_handle_t)%s,&%s,&%s);\n",
        elt_name, call_h_name, mp_name, st_name);
#ifdef NO_EXCEPTIONS
    if (do_ool)
        fprintf(fid, "if (%s != error_status_ok) RAISE(rpc_x_ss_pipe_comm_error);\n", st_name);
    else
        CSPELL_test_status(fid);
#endif
    fprintf(fid, "}\n");
    /* Alignment is checked before buffer exhaustion is checked */
    fprintf(fid,
        "rpc_convert_ulong_int(%s,ndr_g_local_drep,%s,left_in_wire_array);\n",
        drep_name, mp_name);
    fprintf(fid, "rpc_advance_mop(%s,%s,4);\n", mp_name, op_name);

    fprintf(fid, "if(left_in_wire_array==0)\n");
    fprintf(fid, "{\n");
    /* End of pipe */
    fprintf(fid, "(*%s.push)(%s.state,NULL,0);\n",
        pipe_struct_name, pipe_struct_name);
    fprintf(fid, "break;\n");
    fprintf(fid, "}\n");
    fprintf(fid, "}\n");

    /* If not end of pipe, ask user for some space. */
    fprintf(fid, "(*%s.alloc)(%s.state,left_in_wire_array*sizeof",
        pipe_struct_name, pipe_struct_name);
    CSPELL_pipe_base_cast_exp(fid, p_type);
    fprintf(fid, ",\n    &%s,&supplied_size);\n", chunk_ptr_string);

    /* Convert buffer size to element count. */
    fprintf(fid, "supplied_size /= sizeof");
    CSPELL_pipe_base_cast_exp(fid, p_type);
    fprintf(fid, ";\n");
    fprintf(fid,"if(supplied_size==0)\n");
    fprintf(fid, "    RAISE(rpc_x_ss_pipe_memory);\n");
    fprintf(fid, "if(supplied_size>left_in_wire_array)\n");
    fprintf(fid, "    supplied_size=left_in_wire_array;\n");

    /* Unmarshall array elements. */
    spell_name(fid, BE_Array_Info(flattened_type)->count_var);
    fprintf(fid, " = ");
    spell_name(fid, BE_Array_Info(flattened_type)->size_var);
    fprintf(fid, " = supplied_size");

    if ( flattened_type->type->kind == AST_array_k )
    {
        /* The base type of the pipe is an array */
        index_count = BE_Array_Info(flattened_type)->loop_nest_count;
	{
	    boolean array_of_string = false;
	    AST_type_n_t *elt_type;
	    for (elt_type = p_type->type_structure.pipe->base_type;
		 elt_type->kind == AST_array_k;
		 elt_type = elt_type->type_structure.array->element_type) {
		if (AST_STRING_SET(elt_type)) {
		    array_of_string = true;
		}
	    }
	    if (array_of_string) {
		index_count--;
	    }
	}
        for (slice = 1; slice < index_count; slice++)
        {
            fprintf( fid, "*(%d-0+1)",
                     flattened_type->type->type_structure.array
                                ->index_vec[slice].upper_bound->value.int_val );
        }
    }

    fprintf( fid, ";\n");

    pipe_marshalling_names.mp = mp_name;
    pipe_marshalling_names.op = op_name;
    pipe_marshalling_names.drep = drep_name;
    pipe_marshalling_names.elt = elt_name;
    pipe_marshalling_names.st = st_name;
    pipe_marshalling_names.pmem_h = NULL;
    pipe_marshalling_names.call_h = call_h_name;
    pipe_marshalling_names.bad_st = (do_ool) ? "return;" : "goto closedown;";
    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;

    if (!BE_space_opt)
    {
        /* Determine if conversion between dreps is needed */
        fprintf(fid, "if (rpc_equal_drep(%s, ndr_g_local_drep))\n{\n", drep_name);

        /* Unmarshall with matching dreps */
        BE_unmarshall_param(fid, flattened_type,
            ( do_ool  ? (BE_M_NODE | BE_M_BUFF_EXISTS) : 0 ) | BE_M_CALLER |
            BE_M_CHECK_BUFFER | BE_M_ALIGN_MP | BE_M_PIPE,
            &pipe_marshalling_names, p_routine_mode);
        fprintf(fid, "}\nelse\n");
    }

    /* Unmarshall with differing dreps */
    fprintf(fid, "{\n");
    BE_unmarshall_param(fid, flattened_type,
        ( do_ool  ? (BE_M_NODE | BE_M_BUFF_EXISTS) : 0 ) | BE_M_CALLER |
        BE_M_CHECK_BUFFER | BE_M_CONVERT | BE_M_ALIGN_MP | BE_M_PIPE,
        &pipe_marshalling_names, p_routine_mode);
    fprintf(fid, "}\n");

    /* Record how much of current array user didn't take. */
    fprintf(fid, "left_in_wire_array-=supplied_size;\n");
    /* Pass the data to the user */
    fprintf(fid, "(*%s.push)(%s.state,\n", pipe_struct_name, pipe_struct_name);
    fprintf(fid, "    %s,supplied_size);\n", chunk_ptr_string);

    fprintf( fid, "}\n" );

    if (do_ool)
        fprintf(fid, "}\n\n");
}

/******************************************************************************/
/*                                                                            */
/*    Generate code to unmarshall [out] pipe parameters                       */
/*                                                                            */
/******************************************************************************/
void CSPELL_unmarshall_out_pipes
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *p_operation
)
#else
( fid, p_operation )
    FILE *fid;
    AST_operation_n_t *p_operation;
#endif
{
    AST_parameter_n_t *p_parameter;
    AST_type_n_t *p_type;
    int out_pipe_count;
    char pipe_struct_name[MAX_ID+4];
    boolean routine_mode = false;

    if ( ! AST_HAS_OUT_PIPES_SET(p_operation) ) return;

    out_pipe_count = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( AST_OUT_SET(p_parameter) && !AST_OUT_OF_LINE_SET(p_parameter)
             && ( (p_parameter->type->kind == AST_pipe_k)
                 || ((p_parameter->type->kind == AST_pointer_k)
                    && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) ) )
        {
            /* find the base type */
            if (p_parameter->type->kind == AST_pointer_k)
                p_type = p_parameter->type->type_structure.pointer->pointee_type;
            else
                p_type = p_parameter->type;

            /*
             *  Generate code to unmarshall the pipe parameter or call the ool
             *  routine to unmarshall the pipe.
             */
            out_pipe_count++;
            if (!AST_OUT_OF_LINE_SET(p_type))
            {
                BE_spell_pipe_struct_name(p_parameter, pipe_struct_name);
                CSPELL_unmarshall_out_pipe(fid, p_type, pipe_struct_name,
                    out_pipe_count, "mp", "op", "drep", "elt", "call_h", "st",
                    false, &routine_mode);
                if (routine_mode)
                {
                    BE_u_end_buff( fid, 0 );
                    routine_mode = false;
                }
            }
            else
            {
                fprintf(fid, "unmar_params.p_rcvd_data = elt;\n");
                fprintf(fid, "unmar_params.src_drep = drep;\n");
                fprintf(fid, "unmar_params.p_mem_h = &mem_handle;\n");
                fprintf(fid, "unmar_params.call_h = (rpc_call_handle_t)call_h;\n");
                BE_u_start_buff( fid, 0 );

                BE_spell_ool_unmarshall(fid, p_parameter->be_info.param->flat,
                    BE_M_CALLER, &BE_unmarshall_c_pipe_names);

                BE_u_end_buff( fid, 0 );
            }
        }
    }
}

/******************************************************************************/
/*                                                                            */
/*    Generate a pipe push routine                                            */
/*                                                                            */
/******************************************************************************/
void CSPELL_pipe_push_routine
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_pipe_type
)
#else
( fid, p_pipe_type )
    FILE *fid;
    AST_type_n_t *p_pipe_type;
#endif
{
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type;
    AST_operation_n_t dummy_oper;
    int dummy_slots_used;
    boolean routine_mode = false;

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = 0;
    flattened_type = BE_flatten_parameter( &dummy_param,
                        NAMETABLE_add_id("chunk_array"),
                        p_pipe_type, BE_server_side,
                        true, &open_array,
                        BE_F_PA_RTN | BE_F_OOL_RTN | BE_F_PIPE_ROUTINE);
    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;
    BE_undec_piped_arrays(flattened_type);
    BE_decorate_parameter( &dummy_oper, flattened_type, BE_server_side, 0 );

    /* Now start generating code */
    CSPELL_pipe_support_header( fid, p_pipe_type, BE_pipe_push_k, false );
    fprintf( fid, "{\n" );
    CSPELL_local_var_decls( fid, &dummy_oper );
    fprintf( fid, "IoVec_t(2) iovec;\n" );
    fprintf( fid, "ndr_byte packet[15];\n");
    fprintf( fid, "rpc_mp_t pmp = (rpc_mp_t)&packet[0];\n");
    fprintf( fid, "rpc_mp_t mp;\n" );
    fprintf( fid, "rpc_op_t op;\n" );
    if ( BE_ool_in_flattened_list(BE_Array_Info(flattened_type)->flat_elt) )
    {
        fprintf( fid, "rpc_ss_marsh_state_t unmar_params;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp = &unmar_params;\n");
        fprintf(fid, "unsigned long space_in_buffer;\n");
    }
    fprintf( fid, "error_status_t *p_st = rpc_p_pipe_state->p_st;\n\n");
#ifdef DUMPERS
    /* Declare variable to enable debugging of pipe state */
    fprintf( fid, "rpc_ss_ee_pipe_state_t *pst=(rpc_ss_ee_pipe_state_t *)state;\n");
#endif

    fprintf( fid, "op= *rpc_p_pipe_state->p_op;\n" );
    fprintf( fid, "rpc_init_mp(pmp,&packet[0]);\n" );
    fprintf( fid, "iovec.num_elt = 2;\n" );

    fprintf( fid, "if(rpc_p_pipe_state->pipe_filled)" );
    fprintf( fid, "{\n" );
    fprintf( fid, "RAISE(rpc_x_ss_pipe_closed);\n" );
    fprintf( fid, "}\n" );

    fprintf( fid,
    "if(rpc_p_pipe_state->pipe_number!=-*rpc_p_pipe_state->p_current_pipe)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "RAISE(rpc_x_ss_pipe_order);\n" );
    fprintf( fid, "}\n" );

    /* Marshall conformant array */
    BE_PI_Flags(flattened_type) |= BE_ARRAY_HEADER;
    BE_marshall_param(fid, flattened_type, BE_M_ALIGN_MP | BE_M_NEW_SLOT
                              | BE_M_PIPE | BE_M_SYNC_MP | BE_M_MAINTAIN_OP, 0,
                      &dummy_slots_used, &routine_mode, 0);

    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;
    BE_marshall_param(fid, flattened_type, BE_M_PIPE |BE_M_ALIGN_MP |
                         BE_M_CALLEE | BE_M_MAINTAIN_OP, 1, &dummy_slots_used,
      &routine_mode, 0);

    /* End routine mode and mp & op in in pipe state */
    if (routine_mode)
    {
        BE_u_end_buff( fid, BE_M_CALLEE | BE_M_PIPE);
        routine_mode = false;
    }

    fprintf( fid,
 "rpc_call_transmit(rpc_p_pipe_state->call_h,(rpc_iovector_p_t)&iovec,(unsigned32*)p_st);\n");
    fprintf( fid, "if (*p_st != error_status_ok)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid,
         "if (*p_st==rpc_s_call_cancelled) RAISE(RPC_SS_THREADS_X_CANCELLED);\n" );
    fprintf( fid, "else RAISE(rpc_x_ss_pipe_comm_error);\n" );
    fprintf( fid, "}\n" );

    /* Have we received a pipe terminator? */
    fprintf( fid, "if(pipe_chunk_size==0)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "rpc_p_pipe_state->pipe_filled=ndr_true;\n" );
    fprintf( fid,
      "*rpc_p_pipe_state->p_current_pipe=rpc_p_pipe_state->next_out_pipe;\n" );
    fprintf( fid, "}\n" );

    fprintf( fid, "*rpc_p_pipe_state->p_op=op;\n" );
    fprintf( fid, "}\n" );
}
