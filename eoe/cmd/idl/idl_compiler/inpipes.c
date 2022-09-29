/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: inpipes.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.5  1993/01/21  16:23:52  weisman
 * 	Fix for OT 3309.  If the data to marshall into a pipe were greater than
 * 	NIDL_PIPE_BUFF_SIZE (currently 2048 bytes in stubbase.h), the user-supplied
 * 	alloc routine would called with only NIDL_PIPE_BUFF_SIZE.
 *
 * 	Now, any request larger than NIDL_PIPE_BUFF_SIZE will be correctly be
 * 	passed into the user-supplied alloc routine.
 * 	[1993/01/19  22:45:54  weisman]
 *
 * Revision 1.1.2.4  1993/01/03  21:39:58  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:49  bbelch]
 * 
 * Revision 1.1.2.3  1992/12/23  18:47:54  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:03  zeliff]
 * 
 * Revision 1.1.2.2  1992/11/16  19:41:49  hinxman
 * 	Fix OT5214 Datagram packet pool is corrupted by stub freeing non-existent
 * 	                buffer
 * 	[1992/11/11  21:37:03  hinxman]
 * 
 * Revision 1.1  1992/01/19  03:01:21  devrcs
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
**      inpipes.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Code generation for [in] pipe parameters
**
**  VERSION: DCE 1.0

**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <nametbl.h>
#include <flatten.h>
#include <marshall.h>
#include <mmarsh.h>
#include <mutils.h>
#include <dutils.h>
#include <bounds.h>
#include <decorate.h>
#include <localvar.h>
#include <cstubgen.h>
#include <nodunmar.h>
#include <genpipes.h>
#include <inpipes.h>
#include <oolrtns.h>
#include <backend.h>

static BE_mn_t BE_unmarshall_s_pipe_names =
{
    "*rpc_p_pipe_state->p_mp",
    "*rpc_p_pipe_state->p_op",
    "rpc_p_pipe_state->src_drep",
    "rpc_p_pipe_state->p_rcvd_data",
    "(*st)",
    "rpc_p_pipe_state->p_mem_h",
    "rpc_p_pipe_state->call_h",
    "\n{\nif(*st==rpc_s_call_cancelled)RAISE(RPC_SS_THREADS_X_CANCELLED);\nelse RAISE(rpc_x_ss_pipe_comm_error);\n}"
};

static BE_mn_t BE_unmarshall_pipe_of_ool_names =
{
    "unmar_params.mp",
    "unmar_params.op",
    "unmar_params.src_drep",
    "unmar_params.p_rcvd_data",
    "(*unmar_params.p_st)",
    "unmar_params.p_mem_h",
    "unmar_params.call_h",
    "\n{\nif(*unmar_params.p_st==rpc_s_call_cancelled)RAISE(RPC_SS_THREADS_X_CANCELLED);\nelse RAISE(rpc_x_ss_pipe_comm_error);\n}"
};

/******************************************************************************/
/*                                                                            */
/*    Output "if buffer has a release routine, execute it"                    */
/*                                                                            */
/******************************************************************************/
static void CSPELL_release_buffer
#ifdef PROTO
(
    FILE *fid,
    char *pointer_name
)
#else
( fid, pointer_name )
    FILE *fid;
    char *pointer_name;
#endif
{
    fprintf(fid, 
"if(%s->p_rcvd_data->buff_dealloc != NULL && %s->p_rcvd_data->data_len != 0)\n",
            pointer_name, pointer_name);
    fprintf(fid, "    (*(%s->p_rcvd_data->buff_dealloc))\n", pointer_name );
    fprintf(fid, "       (%s->p_rcvd_data->buff_addr);\n", pointer_name );
}

/******************************************************************************/
/*                                                                            */
/*    Generate declarations needed for caller's [in] pipes                    */
/*                                                                            */
/******************************************************************************/
void CSPELL_caller_in_pipe_decls
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
    int in_pipe_count;

    if ( ! AST_HAS_IN_PIPES_SET(p_operation) ) return;

    in_pipe_count = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        /* For each [in] pipe parameter we need a chunk pointer */
        if ( AST_IN_SET(p_parameter) && ! AST_OUT_OF_LINE_SET(p_parameter) &&
              ( (p_parameter->type->kind == AST_pipe_k)
                 || ((p_parameter->type->kind == AST_pointer_k)
                    && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) ) )
        {
            in_pipe_count++;
            CSPELL_pipe_base_type_exp( fid, p_parameter->type );
            fprintf( fid, " *in_pipe_chunk_ptr_%d;\n", in_pipe_count );
        }
    }

    /* Only output if there are in-line pipes */
    if (in_pipe_count) fprintf ( fid, "unsigned long in_pipe_buff_size;\n" );
}


/******************************************************************************/
/*                                                                            */
/*  Support routine to emit code that marshalls a single [in] pipe.  Can be   */
/*  used for either ool or in-line.                                           */
/*                                                                            */
/******************************************************************************/
void CSPELL_marshall_in_pipe
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type,
    char *pipe_struct_name,
    int pipe_num,
    char *iovec_name,
    boolean do_ool
)
#else
( fid, p_type, pipe_struct_name, pipe_num, iovec_name, do_ool)
    FILE *fid;
    AST_type_n_t *p_type;
    char *pipe_struct_name;
    int pipe_num;
    char *iovec_name;
    boolean do_ool;
#endif
{
    AST_parameter_n_t dummy_param;
    AST_parameter_n_t *open_array = NULL;
    AST_parameter_n_t *flattened_type;
    AST_operation_n_t dummy_oper;
    AST_parameter_n_t *elt;
    int dummy_slots_used = 0;
    char chunk_ptr_string[MAX_ID+1];
    boolean routine_mode = false;

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = 0;
    dummy_param.field_attrs = NULL;
    dummy_param.type = p_type;

    /* Generate the local name for the pointer to the pipe data */
    sprintf(chunk_ptr_string, "in_pipe_chunk_ptr_%d", pipe_num);

    /*
     * If we are generating the client ool routine for the pipe, we need to
     * first decorate the parameter as a pipe.
     */
    if (do_ool)
    {
        /*
         *  Flatten the pipe and decorate it which causes the internal
         *  conformant array chunk to be decorated and marshalling variables to
         *  be allocated.
         */
        flattened_type = BE_flatten_parameter( &dummy_param,
            NAMETABLE_add_id(chunk_ptr_string), p_type, BE_client_side, true,
            &open_array, 0);
        BE_undec_piped_arrays(p_type->type_structure.pipe->be_info.pipe->flat);
        BE_decorate_parameter( &dummy_oper, flattened_type, BE_client_side, 0);
    }


    /*
     *  Flatten the pipe which will give us a conformant array for a pipe chunk
     */
    flattened_type = BE_flatten_parameter( &dummy_param,
        NAMETABLE_add_id(chunk_ptr_string), p_type, BE_client_side, true,
        &open_array, BE_F_PA_RTN | BE_F_OOL_RTN | BE_F_PIPE_ROUTINE);
    BE_Array_Info(flattened_type)->first_element_exp =
        BE_first_element_expression(flattened_type);

    if (do_ool)
    {
        /* Routine declaration */
        CSPELL_marshall_ool_type_decl( fid, p_type, BE_caller, false,
                flattened_type, false, false, false, false );
        fprintf(fid, "{\n");

        /* Declare pipe vars */
        fprintf(fid, "unsigned long pipe_chunk_size;\n");
        fprintf(fid, "unsigned long in_pipe_buff_size;\n");
        CSPELL_pipe_base_type_exp( fid, p_type );
        fprintf(fid, " *%s;\n", chunk_ptr_string);

        fprintf(fid, "rpc_op_t op = NIDL_msp->op;\n");
        fprintf(fid, "rpc_mp_t mp;\n");

        /* Declare local variables */
        CSPELL_local_var_decls(fid, &dummy_oper );

        /*
         * Make sure that an iovector and packet of sufficient size are declared
         * (7 bytes for alignment to 8, 7 bytes for synchronization to 8,
         * 3 bytes for alignment to 4, 4 bytes for count value
         */
        fprintf(fid, "IoVec_t(2) iovec;\n");
        fprintf(fid, "ndr_byte packet[21];\n");
        fprintf(fid, "rpc_mp_t pmp = (rpc_mp_t)&packet[0];\n");
        if ( BE_ool_in_flattened_list(BE_Array_Info(flattened_type)->flat_elt) )
        {
            fprintf(fid, "unsigned long space_in_buffer;\n");
        }
    }


    /* Loop while pipe data is available & transmit to server */
    fprintf(fid, "\ndo {\n" );

    /* Get a buffer from the user */
    fprintf(fid, "(*%s.alloc)(%s.state,\n",
          pipe_struct_name, pipe_struct_name );

    fprintf (fid, "((NIDL_PIPE_BUFF_SIZE > sizeof (");
    CSPELL_pipe_base_type_exp (fid, p_type);
    fprintf (fid, ")) ? NIDL_PIPE_BUFF_SIZE : sizeof (");
    CSPELL_pipe_base_type_exp (fid, p_type);
    fprintf (fid, ")),\n");


    fprintf(fid, "&in_pipe_chunk_ptr_%d,&in_pipe_buff_size);\n", pipe_num);

    /*
     *  If the provided buffer is not large enough to contain a single element
     *  then raise the pipe memory exception.
     */
    fprintf(fid,"if(in_pipe_buff_size<sizeof" );
    CSPELL_pipe_base_cast_exp( fid, p_type );
    fprintf(fid,") RAISE(rpc_x_ss_pipe_memory);\n" );

    /* Get the next chunk of data from the user */
    fprintf(fid, "(*%s.pull)(%s.state,\n", pipe_struct_name, pipe_struct_name);
    fprintf(fid, "in_pipe_chunk_ptr_%d,in_pipe_buff_size/sizeof",pipe_num);
    CSPELL_pipe_base_cast_exp( fid, p_type );
    fprintf(fid, ",&pipe_chunk_size);\n" );

    /* Initialize the iovector and packet */
    fprintf(fid, "%s.num_elt = 2;\n", iovec_name);
    fprintf(fid, "rpc_init_mp(pmp, packet);\n");
    fprintf(fid, "rpc_align_mp(pmp, 8);\n" );

    BE_PI_Flags(flattened_type) |= BE_ARRAY_HEADER;
    BE_marshall_param(fid, flattened_type, BE_M_SYNC_MP | BE_M_PIPE |
        BE_M_ALIGN_MP | BE_M_NEW_SLOT | BE_M_MAINTAIN_OP, 0, &dummy_slots_used,
        &routine_mode, 0);

    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;
    BE_marshall_param(fid, flattened_type, BE_M_PIPE | BE_M_CALLER |
         BE_M_ALIGN_MP | BE_M_MAINTAIN_OP,
          1, &dummy_slots_used, &routine_mode, 0);

    fprintf( fid,
 "rpc_call_transmit(NIDL_msp->call_h,(rpc_iovector_p_t)&%s,(unsigned32*)%s);\n",
            iovec_name, (do_ool ? "NIDL_msp->p_st" : "&st"));

    if (do_ool)
        fprintf(fid,
  "if (*NIDL_msp->p_st != error_status_ok) RAISE(rpc_x_ss_pipe_comm_error);\n");
    else
        CSPELL_test_status(fid);

    fprintf(fid, "} while (pipe_chunk_size != 0);\n");

    if (do_ool)
    {
        fprintf(fid, "NIDL_msp->op = op;\n");
        fprintf(fid, "}\n\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Generate code to marshal [in] pipe parameters                           */
/*                                                                            */
/******************************************************************************/
void CSPELL_marshall_in_pipes
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
    int in_pipe_count;
    char pipe_struct_name[MAX_ID+4];

    if ( ! AST_HAS_IN_PIPES_SET(p_operation) ) return;

    in_pipe_count = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        /* For each [in] pipe parameter, passed by value or reference */
        if ( AST_IN_SET(p_parameter) && !AST_OUT_OF_LINE_SET(p_parameter) &&
              ( (p_parameter->type->kind == AST_pipe_k)
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
             *  Generate code to marshall the pipe parameter or call the ool
             *  routine to marshall the pipe.
             */
            in_pipe_count++;
            if (!AST_OUT_OF_LINE_SET(p_type))
            {
                BE_spell_pipe_struct_name( p_parameter, pipe_struct_name );
                CSPELL_marshall_in_pipe( fid, p_type, pipe_struct_name,
                    in_pipe_count, "iovec", false);
            }
            else
            {
                BE_spell_ool_marshall(fid, p_parameter->be_info.param->flat,
                    BE_M_CALLER, 0);
                CSPELL_test_status(fid);
            }
        }
    }
}


/******************************************************************************/
/*                                                                            */
/*    Output "check whether buffer is empty and get next data item"           */
/*                                                                            */
/******************************************************************************/
static void CSPELL_check_and_get
#ifdef PROTO
(
    FILE *fid,
    char *type_s,
    char *name_s,
    unsigned long size
)
#else
( fid, type_s, name_s, size )
    FILE *fid;
    char *type_s;
    char *name_s;
    unsigned long size;
#endif
{
    char *pointer_name;
    char *mp_name;
    char *op_name;

    pointer_name = "rpc_p_pipe_state";
    mp_name = "(*rpc_p_pipe_state->p_mp)";
    op_name = "(*rpc_p_pipe_state->p_op)";

    /* 
     * If there is any need to align, do it first.  Since buffers
     * are multiples of 8-bytes, this align can never go past the end of 
     * the buffer.  After the alignment, we may need to get a new buffer,
     * but it will be 8-byte aligned already.
     */
    if ( size > 1 )
    {
        fprintf(fid,
            "rpc_align_mop(%s, %s, %d);\n", mp_name, op_name, size );
    }
    fprintf(fid, "if ((byte_p_t)%s\n", mp_name );
    fprintf(fid, "     - %s->p_rcvd_data->data_addr\n", pointer_name );
    fprintf(fid, "     >= %s->p_rcvd_data->data_len)\n", pointer_name );
    fprintf(fid, "{\n" );
    fprintf(fid,
"rpc_ss_new_recv_buff(%s->p_rcvd_data,%s->call_h,rpc_p_pipe_state->p_mp,st);\n",
                    pointer_name, pointer_name);

#ifdef NO_EXCEPTIONS
    fprintf( fid, "if (*st != error_status_ok)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid,
         "if (*st==rpc_s_call_cancelled) RAISE(RPC_SS_THREADS_X_CANCELLED);\n" );
    fprintf( fid, "else RAISE(rpc_x_ss_pipe_comm_error);\n" );
    fprintf( fid, "}\n" );
#endif
    fprintf(fid, "}\n" );
    /* Alignment is checked before buffer exhaustion is checked */
    fprintf(fid,
     "rpc_convert_%s(%s->src_drep,ndr_g_local_drep,\n",type_s, pointer_name );
    fprintf(fid, "    %s, %s);\n", mp_name, name_s );
    fprintf(fid,
        "rpc_advance_mop(%s, %s, %d);\n", mp_name, op_name, size );
}

/******************************************************************************/
/*                                                                            */
/*    Generate a pipe pull routine                                            */
/*                                                                            */
/******************************************************************************/
void CSPELL_pipe_pull_routine
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
    char *pointer_name;
    int index_count, slice;
    boolean routine_mode = false;
    boolean ool_in_base_type;

    pointer_name = "rpc_p_pipe_state";

    /* Prepare to emit code */
    dummy_param.be_info.param = NULL;
    dummy_oper.be_info.oper = (BE_oper_i_t *)BE_ctx_malloc(sizeof(BE_oper_i_t));
    dummy_oper.be_info.oper->local_vars = NULL;
    dummy_oper.be_info.oper->flags = 0;
    dummy_param.uplink = &dummy_oper;
    dummy_param.flags = 0;
    flattened_type = BE_flatten_parameter( &dummy_param,
                        NAMETABLE_add_id("chunk_array"),
                        p_pipe_type, BE_client_side,
                        true, &open_array,
                        BE_F_PA_RTN | BE_F_OOL_RTN | BE_F_PIPE_ROUTINE);

    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;
    BE_undec_piped_arrays(flattened_type);
    BE_decorate_parameter( &dummy_oper, flattened_type,
                                   BE_client_side, 0 );

    /* Now start generating code */
    CSPELL_pipe_support_header( fid, p_pipe_type, BE_pipe_pull_k, false );
    fprintf( fid, "{\n" );
    CSPELL_local_var_decls( fid, &dummy_oper );
    fprintf( fid, "ndr_boolean buffer_changed;\n");
    ool_in_base_type =
              BE_ool_in_flattened_list(BE_Array_Info(flattened_type)->flat_elt);
    if ( ool_in_base_type )
    {
        fprintf( fid, "rpc_ss_marsh_state_t unmar_params;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp = &unmar_params;\n");
    }
    fprintf( fid, "error_status_t *st = rpc_p_pipe_state->p_st;\n\n");
#ifdef DUMPERS
    /* Declare variable to enable debugging of pipe state */
    fprintf( fid, "rpc_ss_ee_pipe_state_t *pst=(rpc_ss_ee_pipe_state_t *)state;\n");
#endif

    fprintf( fid, "if(rpc_p_pipe_state->pipe_drained)" );
    fprintf( fid, "{\n" );
    fprintf( fid,"if(rpc_p_pipe_state->next_in_pipe>0) {\n");
    /* If we are expecting another in pipe we have not yet relesed the buffer */
    CSPELL_release_buffer( fid, pointer_name );
    fprintf( fid, "}\nRAISE(rpc_x_ss_pipe_empty);\n" );
    fprintf( fid, "}\n" );

    fprintf( fid,
    "if(rpc_p_pipe_state->pipe_number!=*rpc_p_pipe_state->p_current_pipe)\n" );
    fprintf( fid, "{\n" );
    CSPELL_release_buffer( fid, pointer_name );
    fprintf( fid, "RAISE(rpc_x_ss_pipe_order);\n" );
    fprintf( fid, "}\n" );

    /* Is there part of a received array that can be given to the user? */
    fprintf( fid, "if(rpc_p_pipe_state->left_in_wire_array==0)\n" );
    fprintf( fid, "{\n" );
    /* If not, we need to get another array */
    /* Get array element count */
    CSPELL_check_and_get( fid, "ulong_int",
                          "rpc_p_pipe_state->left_in_wire_array", 4 );

    /* Have we received a pipe terminator? */
    fprintf( fid, "if(rpc_p_pipe_state->left_in_wire_array==0)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "rpc_p_pipe_state->pipe_drained=ndr_true;\n" );
    fprintf( fid,
      "*rpc_p_pipe_state->p_current_pipe=rpc_p_pipe_state->next_in_pipe;\n" );

    /*
     *  If last [in] pipe, reset op for new data stream, and free data buffer
     *  if necessary
     */
    fprintf( fid,"if(rpc_p_pipe_state->next_in_pipe<0) {\n");
    CSPELL_release_buffer( fid, pointer_name );
    fprintf( fid,  "*rpc_p_pipe_state->p_op=0;\n" );
    fprintf( fid,  "}\n" );
    fprintf( fid, "*ecount=0;\n" );
    fprintf( fid, "return;\n" );
    fprintf( fid, "}\n" );


    fprintf( fid, "}\n" );
    /* How much of the data does the user want? */
    fprintf( fid, "if(esize == 0)\n" );
    fprintf( fid, "{\n" );
    CSPELL_release_buffer( fid, pointer_name );
    fprintf( fid, "RAISE(rpc_x_ss_pipe_memory);\n" );
    fprintf( fid, "}\n" );

    fprintf( fid, "if(esize>rpc_p_pipe_state->left_in_wire_array)\n" );
    fprintf( fid, "   *ecount=rpc_p_pipe_state->left_in_wire_array;\n" );
    fprintf( fid, "else *ecount=esize;\n" );

    /* Unmarshal array elements */
    spell_name( fid, BE_Array_Info(flattened_type)->count_var);
    fprintf( fid, " = ");
    spell_name( fid, BE_Array_Info(flattened_type)->size_var);
    fprintf( fid, " = *ecount" );

    if ( flattened_type->type->kind == AST_array_k )
    {
        /* The base type of the pipe is an array */
        index_count = BE_Array_Info(flattened_type)->loop_nest_count;
        for (slice = 1; slice < index_count; slice++)
        {
            fprintf( fid, "*(%d-0+1)",
                     flattened_type->type->type_structure.array
                                ->index_vec[slice].upper_bound->value.int_val );
        }
    }

    fprintf( fid, ";\n");

    /* Prepare to unmarshall array elements */
    if ( ool_in_base_type )
    {
        fprintf( fid, "unmar_params.src_drep = rpc_p_pipe_state->src_drep;\n");
        fprintf( fid,
                "unmar_params.p_rcvd_data = rpc_p_pipe_state->p_rcvd_data;\n");
        fprintf( fid, "unmar_params.call_h = rpc_p_pipe_state->call_h;\n");
        fprintf( fid, "unmar_params.p_mem_h = rpc_p_pipe_state->p_mem_h;\n");
        fprintf( fid, "unmar_params.p_st = st;\n");
        fprintf( fid, "unmar_params.version = 0;\n");
    }

    BE_PI_Flags(flattened_type) &= ~BE_ARRAY_HEADER;

    if (!BE_space_opt)
    {
        /* Determine if conversion between dreps is needed */
        fprintf(fid, "if (rpc_equal_drep(%s, ndr_g_local_drep))\n{\n",
            BE_unmarshall_s_pipe_names.drep);

        /* Unmarshall with matching dreps */
        BE_unmarshall_param(fid, flattened_type,
            BE_M_CHECK_BUFFER | BE_M_BUFF_EXISTS | BE_M_CALLEE |
            BE_M_ALIGN_MP | BE_M_PIPE,
                          &BE_unmarshall_s_pipe_names, &routine_mode );

        /* End routine mode and mp & op in in pipe state */
        if (routine_mode)
        {
            BE_u_end_buff( fid, BE_M_CALLEE | BE_M_PIPE );
            routine_mode = false;
        }
        fprintf(fid, "}\nelse\n");
    }

    /* Unmarshall with differing dreps */
    fprintf(fid, "{\n");
    BE_unmarshall_param(fid, flattened_type,
        BE_M_CHECK_BUFFER | BE_M_BUFF_EXISTS | BE_M_CALLEE |
        BE_M_CONVERT | BE_M_ALIGN_MP | BE_M_PIPE,
                      &BE_unmarshall_s_pipe_names, &routine_mode );
    /* End routine mode and mp & op in in pipe state */
    if (routine_mode)
    {
        BE_u_end_buff( fid, BE_M_CALLEE | BE_M_PIPE );
        routine_mode = false;
    }
    fprintf(fid, "}\n");

    fprintf( fid, "rpc_p_pipe_state->left_in_wire_array-=(*ecount);\n" );
    fprintf( fid, "}\n" );
}
