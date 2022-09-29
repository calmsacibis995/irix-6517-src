/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cstubgen.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:38:50  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:56  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:45:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:09  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/05  15:30:52  harrow
 * 	Initialize node table only if needed based upon the operation
 * 	having a [ptr] pointer.
 * 	[1992/05/05  15:29:39  harrow]
 * 
 * Revision 1.1  1992/01/19  03:00:58  devrcs
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
**
**  NAME:
**
**      cstubgen.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Generate a client stub
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <localvar.h>
#include <ifspec.h>
#include <commstat.h>
#include <clihandl.h>
#include <cstubgen.h>
#include <marshall.h>
#include <decorate.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <nodesupp.h>
#include <inpipes.h>
#include <outpipes.h>
#include <dutils.h>
#include <backend.h>

BE_handle_info_t BE_handle_info;


/******************************************************************************/
/*                                                                            */
/*    Spell test of status after runtime call                                 */
/*                                                                            */
/******************************************************************************/
void CSPELL_test_status
#ifdef PROTO
(
    FILE *fid
)
#else
    (fid)
    FILE *fid;
#endif
{
    fprintf(fid, "if (st != error_status_ok) goto closedown;\n");
}

/******************************************************************************/
/*                                                                            */
/*    Spell test of status after rpc_call_transceive                          */
/*                                                                            */
/******************************************************************************/
void CSPELL_test_transceive_status
#ifdef PROTO
(
    FILE *fid
)
#else
    (fid)
    FILE *fid;
#endif
{
    if ( ( BE_handle_info.handle_type == BE_auto_handle_k )
          && ( ! BE_handle_info.auto_handle_idempotent_op ) )
    {
        fprintf(fid, "if (st != error_status_ok)\n{\n");
        fprintf(fid,
         "NIDL_restartable=NIDL_restartable&&(!(rpc_call_did_mgr_execute((rpc_call_handle_t)call_h,&NIDL_st2)));\n");
        fprintf(fid, "goto closedown;\n}\n");
    }
    else CSPELL_test_status( fid );
}

/******************************************************************************/
/*                                                                            */
/*    Spell client stub routine header                                        */
/*                                                                            */
/******************************************************************************/
static void CSPELL_csr_header
#ifdef PROTO
(
    FILE *fid,
    char *p_interface_name,       /* Ptr to name of interface */
    AST_operation_n_t *p_operation, /* Ptr to operation node */
    boolean use_internal_name       /* use internal name if true */
)
#else
(fid, p_interface_name, p_operation, use_internal_name)
    FILE *fid;
    char *p_interface_name;
    AST_operation_n_t *p_operation;
    boolean use_internal_name;
#endif
{
    char *emitted_name;

    if (use_internal_name) {
        char op_internal_name[45];
        sprintf(op_internal_name, "op%ld_csr", p_operation->op_number);
        emitted_name = NAMETABLE_add_id(op_internal_name);
        fprintf(fid, "\nstatic ");
    }
    else
        emitted_name = p_operation->name;

    fprintf (fid, "\n");
    CSPELL_function_def_header (fid, p_operation, emitted_name);
    CSPELL_finish_synopsis (fid, p_operation->parameters);
}

/******************************************************************************/
/*                                                                            */
/*    Generation of a stub for a client operation call                        */
/*                                                                            */
/******************************************************************************/
static void CSPELL_client_stub_routine
#ifdef PROTO
(
    FILE *fid,                      /* Handle for emitted C text */
    AST_interface_n_t *p_interface, /* Ptr to AST interface node */
    language_k_t language,          /* Language stub is to interface to */
    AST_operation_n_t *p_operation, /* Ptr to AST operation node */
    char *p_interface_name,         /* Ptr to name of interface */
    unsigned long op_num,           /* Number of current operation */
    boolean use_internal_name       /* use internal name if true */
)
#else
(fid, p_interface, language, p_operation, p_interface_name,
 op_num, use_internal_name)
    FILE *fid;
    AST_interface_n_t *p_interface;
    language_k_t language;
    AST_operation_n_t *p_operation;
    char *p_interface_name;
    unsigned long op_num;
    boolean use_internal_name;
#endif
{
    BE_stat_info_t comm_stat_info;
    BE_stat_info_t fault_stat_info;
    boolean uses_packet;

    BE_decorate_operation(p_operation, BE_client_side);

    /* What sort of status reporting? */
    BE_get_comm_stat_info( p_operation, &comm_stat_info );
    BE_get_fault_stat_info( p_operation, &fault_stat_info );

    /* Routine header */
    CSPELL_csr_header(fid, p_interface_name, p_operation,
        use_internal_name);

    fprintf (fid, "{\n");

    /*
     * Standard local variables
     */
    fprintf(fid, "rpc_op_t op;\n");
    fprintf(fid, "rpc_mp_t mp;\n");
    fprintf(fid, "ndr_format_t drep;\n");
    fprintf(fid, "rpc_ss_mem_handle mem_handle;\n");
    fprintf(fid, "volatile rpc_call_handle_t call_h = NULL;\n");
    fprintf(fid, "rpc_transfer_syntax_t transfer_syntax;\n");
    fprintf(fid, "volatile error_status_t st;\n");
    fprintf(fid, "rpc_iovector_elt_t outs, *elt = NULL;\n");
    fprintf(fid, "ndr_boolean buffer_changed;\n");
    fprintf(fid, "volatile ndr_ulong_int NIDL_fault_code=error_status_ok;\n");
    fprintf(fid,
           "volatile RPC_SS_THREADS_CANCEL_STATE_T NIDL_async_cancel_state;\n");

    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "unsigned long space_in_buffer;\n");
        fprintf(fid, "IoVec_t(1) sp_iovec;\n");
    }

    if (AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf( fid, "rpc_ss_marsh_state_t unmar_params;\n" );
        fprintf( fid, "rpc_ss_marsh_state_t *NIDL_msp= &unmar_params;\n" );
    }

    if (AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation))
    {
        fprintf(fid, "unsigned long pipe_chunk_size;\n");
    }

    if (AST_HAS_IN_PIPES_SET(p_operation))
    {
        CSPELL_caller_in_pipe_decls(fid, p_operation);
    }

    if (AST_HAS_OUT_PIPES_SET(p_operation))
    {
        CSPELL_caller_out_pipe_decls(fid, p_operation);
    }

    /*
     * Analyze the association handle the call is being made on;
     * declare assoc_handle if necessary
     */
    BE_setup_client_handle (fid, p_interface, p_operation, &BE_handle_info);

    BE_declare_stack_vars(fid, p_operation, &uses_packet);

    CSPELL_local_var_decls(fid, p_operation);

    fprintf(fid, "\n");

    /*
     * Start of executable code
     */
    fprintf(fid, "RPC_SS_INIT_CLIENT\n");
    fprintf(fid, "mem_handle.memory=(rpc_void_p_t)NULL;\n");
    fprintf(fid, "mem_handle.node_table=(rpc_ss_node_table_t)NULL;\n");
    fprintf(fid, "RPC_SS_THREADS_DISABLE_ASYNC(NIDL_async_cancel_state);\n");
    if ( BE_handle_info.handle_type == BE_auto_handle_k )
    {
        fprintf( fid,
   "RPC_SS_THREADS_ONCE(&NIDL_interface_client_once,NIDL_auto_handle_init);\n" );
    }
    fprintf(fid, "TRY\n");
    fprintf(fid, "st = error_status_ok;\n");

    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "sp_iovec.num_elt = 1;\n");
        fprintf(fid, "NIDL_msp->p_iovec = (rpc_iovector_t*)&sp_iovec;\n");
        fprintf(fid, "NIDL_msp->p_st = (error_status_t*)&st;\n");
        fprintf(fid, "NIDL_msp->version = 0;\n");
    }

    if (AST_HAS_FULL_PTRS_SET(p_operation) && 
        (AST_HAS_IN_PTRS_SET(p_operation) || AST_HAS_OUT_PTRS_SET(p_operation)))
    {
        fprintf(fid,"rpc_ss_init_node_table(&unmar_params.node_table,&mem_handle);\n");
    }

    if (AST_HAS_OUT_PTRS_SET(p_operation) )
    {
        /* The following call is done here to enable PRT testing */
        fprintf(fid, "rpc_ss_client_establish_alloc(&unmar_params);\n");
    }

    CSPELL_call_start(fid, &BE_handle_info, p_interface, p_operation, op_num,
                       &comm_stat_info, &fault_stat_info);

    /**** TBS -- CHECK transfer syntax here ****/

    if (AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "NIDL_msp->p_mem_h = &mem_handle;\n");
        fprintf(fid, "NIDL_msp->call_h = (rpc_call_handle_t)call_h;\n");
    }


    BE_marshall(fid, p_operation, BE_in);

    if ( AST_HAS_IN_PIPES_SET(p_operation) )
    {
        if ( BE_handle_info.handle_type == BE_auto_handle_k )
        {
            fprintf( fid, "NIDL_restartable = ndr_false;\n" );
        }
        CSPELL_marshall_in_pipes( fid, p_operation );
    }

    /*
     * If there are no ins, turn the line around
     */
    if ((!AST_HAS_INS_SET(p_operation)) || AST_HAS_IN_PIPES_SET(p_operation))
    {
        fprintf(fid,"{\nrpc_iovector_t niovec; niovec.num_elt = 0;\n");
        fprintf(fid,"elt = &outs;\n");
        fprintf(fid,"rpc_call_transceive((rpc_call_handle_t)call_h,&niovec,elt,&drep,(unsigned32*)&st);\n");
        fprintf(fid,"}\n");
        CSPELL_test_transceive_status(fid);
    }

    if (AST_HAS_OUTS_SET(p_operation) || AST_HAS_OUT_PIPES_SET(p_operation))
        fprintf(fid, "rpc_init_mp(mp, elt->data_addr);\n");

    if (AST_HAS_OUT_PTRS_SET(p_operation)
    ||  AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    ||  AST_HAS_IN_OOLS_SET(p_operation)
    ||  AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "NIDL_msp->p_rcvd_data = elt;\n");
        fprintf(fid, "NIDL_msp->src_drep = drep;\n");
    }

    if ( AST_HAS_OUT_PIPES_SET(p_operation) )
    {
        fprintf(fid, "rpc_init_op(op);\n");
        CSPELL_unmarshall_out_pipes( fid, p_operation );
    }

    if (AST_HAS_OUTS_SET(p_operation))
        BE_unmarshall(fid, p_operation, BE_out);

    /*
     **** TBS -- ELSE CHECK THAT THERE REALLY WERE NO OUTS ****
     */

    fprintf(fid, "\nclosedown: elt = NULL;\n");

    /*
     * Catch the error that indicates that st has be set to a failing status
     * that should be reported, and then do normal cleanup processing.  If
     * for some reason the status is not set, then set it.
     */
    fprintf(fid, "CATCH(rpc_x_ss_pipe_comm_error)\n");
    if ( BE_handle_info.handle_type == BE_auto_handle_k )
    {
        CSPELL_restart_logic( fid, p_operation, uses_packet );

        /*
         * This label is used when no valid auto-handle binding can be
         * found.  The call to this is generated in CSPELL_bind_auto_handle
         * when no valid binding can be found.
         */
        fprintf(fid, "IDL_auto_binding_failure:;\n");
    }

    /*
     * Normal cleanup processing to free up resources and end the call and
     * and report any faults or failing statuses.
     */
    fprintf(fid, "FINALLY\n");

    /*
     *  The only time we need to free the elt is if:
     *
     *      1) status is error_status_ok (runtime doesn't return elt on error)
     *      2) elt is not NULL (set to NULL on normal path)
     *
     *  If we are being terminated via an exception then the status will be
     *  error_status_ok, but the elt will not be NULL thus we need to free the
     *  buffer.
     */
    fprintf(fid, "if((st==error_status_ok) && (elt!=NULL))\n{\n   ");
    BE_spell_end_unmarshall( fid, &BE_def_mn );
    fprintf(fid, "}\n");

    /* End the call, but only if we have one to end for auto handle */
    if ( BE_handle_info.handle_type == BE_auto_handle_k ) fprintf(fid, "if(call_h!=NULL)");
    fprintf(fid, "rpc_ss_call_end(&call_h,&NIDL_fault_code,&st);\n");
    CSPELL_binding_free_if_needed( fid, &BE_handle_info );

    /* Must free user binding after ending the call */
    if ((BE_handle_info.handle_type == BE_parm_user_handle_k)
        || (BE_handle_info.handle_type == BE_impl_user_handle_k))
    {
        /* There is a user handle to unbind. As we are inside an exception
           handler, we don't want any exception the unbind causes */
        fprintf (fid, "TRY\n");
        fprintf (fid, "%s_unbind(%c%s, (handle_t)assoc_handle);\n",
            BE_handle_info.type_name, BE_handle_info.deref_generic,
            BE_handle_info.user_handle_name);
        fprintf (fid, "CATCH_ALL\n");
        fprintf (fid, "ENDTRY\n");
    }

    /* If [represent_as] on handle_t parameter, release the handle_t */
    if ((BE_handle_info.handle_type == BE_rep_as_handle_t_k)
        || (BE_handle_info.handle_type == BE_rep_as_handle_t_p_k))
    {
        fprintf (fid, "TRY\n");
        fprintf( fid, "%s_free_inst((handle_t *)%s);\n",
                    BE_get_name(BE_handle_info.rep_as_type),
                    assoc_handle_ptr );
        fprintf (fid, "CATCH_ALL\n");
        fprintf (fid, "ENDTRY\n");
    }

    /* Release memory allocated by stub code */
    fprintf(fid, "if (mem_handle.memory) rpc_ss_mem_free(&mem_handle);\n");

    /* Give status information to client, or raise the appropriate exception */
    CSPELL_return_status( fid, &comm_stat_info, &fault_stat_info, "st",
        ( (comm_stat_info.type == BE_stat_result_k)
            || (fault_stat_info.type == BE_stat_result_k) )
        ? BE_get_name(p_operation->result->be_info.param->name)
        : (char *)NULL );

    fprintf(fid, "RPC_SS_THREADS_RESTORE_ASYNC(NIDL_async_cancel_state);\n");
    fprintf(fid, "ENDTRY\n");

    /* Set the return value */
    if (p_operation->result->type->kind != AST_void_k)
        fprintf(fid, "return %s;\n",
            BE_get_name(p_operation->result->be_info.param->name));
    fprintf (fid, "}\n");
}

/******************************************************************************/
/*                                                                            */
/*    Main control flow for generating a client stub                          */
/*                                                                            */
/******************************************************************************/
void BE_gen_cstub
#ifdef PROTO
(
    FILE *fid,                      /* Handle for emitted C text */
    AST_interface_n_t *p_interface, /* Ptr to AST interface node */
    language_k_t language,          /* Language stub is to interface to */
    char header_name[],         /* Name of header file to be included in stub */
    boolean bugs[],                 /* List of backward compatibility "bugs" */
    boolean generate_cepv           /* generate cepv if true */
)
#else
(fid, p_interface, language, header_name, bugs, generate_cepv)
    FILE *fid;
    AST_interface_n_t *p_interface;
    language_k_t language;
    char header_name[];
    boolean bugs[];
    boolean generate_cepv;
#endif
{
    AST_export_n_t *p_export;
    AST_operation_n_t *p_operation;
    char *p_interface_name;
    boolean first;

    /*
     * Emit #defines and #includes
     */
    CSPELL_std_include(fid, header_name, BE_client_stub_k, p_interface->op_count);

    /*
     * Emit if_spec definition
     */
    CSPELL_interface_def(fid, p_interface, BE_client_stub_k, false);

    /* If necessary, emit statics needed for [auto_handle] */
    if ( AST_AUTO_HANDLE_SET(p_interface) )
    {
        CSPELL_auto_handle_statics( fid );
    }
    /* If there is an implicit handle, declare it */
    if (p_interface->implicit_handle_name != NAMETABLE_NIL_ID)
    {
        fprintf( fid, "globaldef " );
        if ( ! AST_IMPLICIT_HANDLE_G_SET(p_interface) )
        {
            fprintf(fid, "handle_t ");
        }
        else
        {
            spell_name( fid, p_interface->implicit_handle_type_name);
            fprintf( fid, " " );
        }
        spell_name( fid, p_interface->implicit_handle_name);
        fprintf( fid, ";\n" );
    }

    /*  Emit pointed-at routines only if some operation has the code attribute.
     *  If the [code/nocode] ACF attributes were propagated to type nodes, we
     *  could modify BE_pointed_at_routines() to do something more fine-grained
     *  than this all or nothing decision.
     */
    for (p_export = p_interface->exports; p_export; p_export = p_export->next)
        if (p_export->kind == AST_operation_k)
            if (!AST_NO_CODE_SET(p_export->thing_p.exported_operation))
            {
                BE_pointed_at_routines( fid, p_interface, BE_caller );
                break;  /* Don't spell routines more than once. */
            }

    NAMETABLE_id_to_string(p_interface->name, &p_interface_name);

    /*
     * Emit operation definitions
     */
    for (p_export = p_interface->exports; p_export; p_export = p_export->next)
    {
        if (p_export->kind == AST_operation_k)
        {
            BE_push_malloc_ctx();
            NAMETABLE_set_temp_name_mode();
            p_operation = p_export->thing_p.exported_operation;
            if (!AST_NO_CODE_SET(p_operation))
                CSPELL_client_stub_routine(fid, p_interface, language,
                    p_operation, p_interface_name, p_operation->op_number,
                    generate_cepv);
            NAMETABLE_clear_temp_name_mode();
            BE_pop_malloc_ctx();
        }
    }

    if (generate_cepv) {
        /*
         * Emit EPV declarations
         */
        fprintf(fid, "/* global */ %s_v%d_%d_epv_t %s_v%d_%d_c_epv = {\n",
            p_interface_name,
            (p_interface->version%65536), (p_interface->version/65536),
            p_interface_name,
            (p_interface->version%65536), (p_interface->version/65536));

        first = true;

        for (p_export = p_interface->exports; p_export;
                p_export = p_export->next)
            if (p_export->kind == AST_operation_k)
        {
            if (first) first = false;
            else fprintf (fid, ",\n");
            p_operation = p_export->thing_p.exported_operation;
            if (!AST_NO_CODE_SET(p_operation))
            {
                fprintf(fid, " op%ld_csr", p_operation->op_number);
            }
            else fprintf(fid, " NULL");
        }
        fprintf (fid, "\n};\n");
    }
}
