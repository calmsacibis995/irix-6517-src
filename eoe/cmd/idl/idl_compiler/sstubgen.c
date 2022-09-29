/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sstubgen.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.4  1993/01/03  22:11:44  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:38:20  bbelch]
 *
 * Revision 1.1.4.3  1992/12/23  19:08:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:05:42  zeliff]
 * 
 * Revision 1.1.4.2  1992/11/05  19:29:44  hinxman
 * 	Fix OT5600 - infinite loop in server stub if comms failure with context handle
 * 	[1992/11/05  16:11:58  hinxman]
 * 
 * Revision 1.1.2.4  1992/05/13  19:09:47  harrow
 * 	Generate frees at end of server stub only for [in]-only parameters
 * 	because if they are [out] the routines will be called during
 * 	marshalling.
 * 	[1992/05/13  11:44:54  harrow]
 * 
 * Revision 1.1.2.3  1992/05/05  15:19:50  harrow
 * 	     Initialize the node table only if alias detection is required.
 * 	     [1992/05/04  20:03:19  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:13:46  harrow
 * 	          Add calls to server stub to manage reference counts which
 * 	          prevent context rundown while the context handle is in
 * 	          use by a manager.
 * 
 * 	          Correct signiture of server stub to utilzie the correct
 * 	          date type for transfer syntax.
 * 	          [1992/03/19  18:14:31  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:52  devrcs
 * 	          Initial revision
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
**      sstubgen.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Generate a server stub, including the calls to the manager routines
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <ifspec.h>
#include <cspeldcl.h>
#include <cspell.h>
#include <localvar.h>
#include <marshall.h>
#include <mutils.h>
#include <decorate.h>
#include <nodmarsh.h>
#include <nodunmar.h>
#include <nodesupp.h>
#include <genpipes.h>
#include <backend.h>
#include <dutils.h>

typedef enum {
                                /* handle_t parameter with [represent_as] */
    BE_no_rep_as_handle_t_k,  /* not present */
    BE_val_rep_as_handle_t_k, /* passed by value */
    BE_ref_rep_as_handle_t_k  /* passed by reference */
} BE_rep_as_handle_t_k_t;

static char rep_as_handle_name[] = "IDL_handle_rep_as";

/*
 * BE_server_binding_analyze
 *
 * Find whether operation can be called by [auto_handle] client, and
 * whether there is a first handle_t parameter with a [represent_as] attribute
 *
 */
static void BE_server_binding_analyze
#ifdef PROTO
(
    AST_operation_n_t *p_operation,
    boolean *server_binding_explicit, /* TRUE if no client can use [auto_handle]
                                         binding with this operation */
    BE_rep_as_handle_t_k_t *p_rep_as_handle_param,
    NAMETABLE_id_t *p_rep_as_type_name  /* type of handle param */
)
#else
( p_operation, server_binding_explicit, p_rep_as_handle_param,
  p_rep_as_type_name )
    AST_operation_n_t *p_operation;
    boolean *server_binding_explicit;
    BE_rep_as_handle_t_k_t *p_rep_as_handle_param;
    NAMETABLE_id_t *p_rep_as_type_name;
#endif
{
    AST_parameter_n_t *p_first_parameter;
    AST_type_n_t *p_type, *p_pointee_type;

    *p_rep_as_handle_param = BE_no_rep_as_handle_t_k;
    if ( AST_EXPLICIT_HANDLE_SET(p_operation) )
    {
        *server_binding_explicit = false;
        return;
    }
    if ( AST_HAS_IN_CTX_SET(p_operation) )
    {
        *server_binding_explicit = true;
        return;
    }
    p_first_parameter = p_operation->parameters;
    if ( p_first_parameter == NULL )
    {
        *server_binding_explicit = false;
        return;
    }
    p_type = p_first_parameter->type;
    if ( p_type->kind == AST_handle_k )
    {
        *server_binding_explicit = true;
        if ( p_type->rep_as_type != NULL )
        {
            *p_rep_as_handle_param = BE_val_rep_as_handle_t_k;
            *p_rep_as_type_name = p_type->name;
        }
        return;
    }
    if ( AST_HANDLE_SET(p_type) )
    {
        *server_binding_explicit = true;
        return;
    }
    if ( p_type->kind == AST_pointer_k )
    {
        p_pointee_type = p_type->type_structure.pointer->pointee_type;
        if ( p_pointee_type->kind == AST_handle_k )
        {
            *server_binding_explicit = true;
            if ( p_pointee_type->rep_as_type != NULL )
            {
                *p_rep_as_handle_param = BE_ref_rep_as_handle_t_k;
                *p_rep_as_type_name = p_pointee_type->name;
            }
            return;
        }
        if ( AST_HANDLE_SET(p_pointee_type) )
        {
            *server_binding_explicit = true;
            return;
        }
    }
    *server_binding_explicit = false;
    return;
}

/*
 * CSPELL_manager_call
 *
 * Emit a call to a manager operation
 */
static void CSPELL_manager_call
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation,
    BE_rep_as_handle_t_k_t rep_as_handle_param,
    NAMETABLE_id_t rep_as_type_name
)
#else
(fid, p_interface, p_operation, rep_as_handle_param, rep_as_type_name)
    FILE *fid;
    AST_interface_n_t *p_interface;
    AST_operation_n_t *p_operation;
    BE_rep_as_handle_t_k_t rep_as_handle_param;
    NAMETABLE_id_t rep_as_type_name;
#endif
{
    boolean first;          /* true for first parameter in parameter list */
    AST_parameter_n_t *pp;  /* Pointer down list of parameters */

    if ( rep_as_handle_param != BE_no_rep_as_handle_t_k )
    {
        fprintf( fid, "%s_to_local(&h,&%s);\n",
                BE_get_name(rep_as_type_name),
                rep_as_handle_name );
    }

    fprintf (fid, "\n/* manager call */\n");
    fprintf( fid, "NIDL_manager_entered = ndr_true;\n" );
    fprintf( fid,
              "RPC_SS_THREADS_ENABLE_GENERAL(NIDL_general_cancel_state);\n" );

    if (p_operation->result->type->kind != AST_void_k)
        fprintf(fid, "%s = ",
            BE_get_name(p_operation->result->be_info.param->name));

    fprintf(fid, "(*((%s_v%d_%d_epv_t *)mgr_epv)->%s)",
            BE_get_name(p_interface->name), (p_interface->version%65536),
            (p_interface->version/65536), BE_get_name(p_operation->name));

    fprintf(fid,"(");
    first = true;
    for (pp = p_operation->parameters; pp != NULL; pp = pp->next) {
        if (first)
        {
            first = false;
            if ( rep_as_handle_param != BE_no_rep_as_handle_t_k )
            {
                fprintf( fid, "%c%s",
                  (rep_as_handle_param == BE_ref_rep_as_handle_t_k) ? '&' : ' ',
                  rep_as_handle_name );
                continue;
            }
            else if (pp->type->kind == AST_handle_k)
            {
                fprintf(fid, "h");
                continue;
            }
            else if ( (pp->type->kind == AST_pointer_k)
                      && (pp->type->type_structure.pointer->pointee_type->kind
                                                     == AST_handle_k) )
            {
                fprintf(fid, "&h");
                continue;
            }
        }
        else
            fprintf (fid, ",\n ");
        if (pp->type->kind == AST_array_k && AST_PTR_SET(pp)) {
            spell_name(fid, pp->be_info.param->flat->name);
            fprintf (fid, " == NULL ? NULL : *");
            spell_name(fid, pp->be_info.param->flat->name);
        }
        else spell_name(fid, pp->be_info.param->name);
    }
    fprintf(fid, ");\n");
    fprintf( fid,
              "RPC_SS_THREADS_RESTORE_GENERAL(NIDL_general_cancel_state);\n" );

    if ( rep_as_handle_param != BE_no_rep_as_handle_t_k )
    {
        fprintf( fid, "%s_free_local(&%s);\n",
                BE_get_name(rep_as_type_name),
                rep_as_handle_name );
    }
}

/*
 *  CSPELL_server_stub_routine
 *
 *  Generate a server stub routine for an operation
 */
static void CSPELL_server_stub_routine
#ifdef PROTO
(
    FILE *fid,
    language_k_t language,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation
)
#else
(fid, language, p_interface, p_operation)
    FILE *fid;
    language_k_t language;
    AST_interface_n_t *p_interface;
    AST_operation_n_t *p_operation;
#endif
{
    long first_pipe;        /* Index of first pipe to be processed */
    boolean uses_packet;
    boolean explicit_binding;
    BE_rep_as_handle_t_k_t rep_as_handle_param;
    NAMETABLE_id_t rep_as_type_name;

    BE_decorate_operation(p_operation, BE_server_side);

    fprintf (fid, "\nstatic void op%ld_ssr", p_operation->op_number);

    fprintf (fid, "\n#ifdef IDL_PROTOTYPES\n");

    fprintf (fid, "(\n");
    fprintf (fid, " handle_t h,\n");
    fprintf (fid, " rpc_call_handle_t call_h,\n");
    fprintf (fid, " rpc_iovector_elt_p_t elt,\n");
    fprintf (fid, " ndr_format_p_t drep,\n");
    fprintf (fid, " rpc_transfer_syntax_p_t transfer_syntax,\n");
    fprintf (fid, " rpc_mgr_epv_t mgr_epv,\n");
    fprintf (fid, " error_status_t *st_p\n)\n");

    fprintf (fid, "#else\n");

    fprintf (fid,
         "(h, call_h, elt, drep, transfer_syntax, mgr_epv, st_p)\n");
    fprintf (fid, " handle_t h;\n");
    fprintf (fid, " rpc_call_handle_t call_h;\n");
    fprintf (fid, " rpc_iovector_elt_p_t elt;\n");
    fprintf (fid, " ndr_format_p_t drep;\n");
    fprintf (fid, " rpc_transfer_syntax_p_t transfer_syntax;\n");
    fprintf (fid, " rpc_mgr_epv_t mgr_epv;\n");
    fprintf (fid, " error_status_t *st_p;\n");

    fprintf (fid, "#endif\n");

    fprintf (fid, "{\n");
    fprintf(fid, "rpc_op_t op;\n");
    fprintf(fid, "rpc_mp_t mp;\n");
    fprintf(fid, "volatile error_status_t st;\n");
    fprintf(fid, "rpc_ss_mem_handle mem_handle;\n");
    fprintf(fid, "ndr_boolean buffer_changed;\n");
    fprintf(fid, "volatile ndr_boolean NIDL_manager_entered = ndr_false;\n");
    fprintf(fid,
         "volatile RPC_SS_THREADS_CANCEL_STATE_T NIDL_general_cancel_state;\n");

    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_ENABLE_ALLOCATE_SET(p_operation))
    {
        fprintf(fid, "rpc_ss_thread_support_ptrs_t NIDL_support_ptrs;\n");
    }

    if (AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "IoVec_t(1) sp_iovec;\n");
        fprintf(fid, "unsigned long space_in_buffer;\n");
    }

    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation)
    || AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_ENABLE_ALLOCATE_SET(p_operation))
    {
        fprintf(fid, "rpc_ss_marsh_state_t unmar_params;\n");
        fprintf(fid, "rpc_ss_marsh_state_t *NIDL_msp = &unmar_params;\n");
    }

    if (AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation))
    {
        fprintf(fid, "long current_pipe = 0;\n");
    }

    BE_declare_stack_vars(fid, p_operation, &uses_packet);
    CSPELL_local_var_decls(fid, p_operation);
    BE_server_binding_analyze(p_operation, &explicit_binding,
                                 &rep_as_handle_param, &rep_as_type_name);
    if ( rep_as_handle_param != BE_no_rep_as_handle_t_k )
    {
        fprintf( fid, "%s %s;\n",
                      BE_get_name(rep_as_type_name), rep_as_handle_name );
    }

    /*
     *  Start of excutable code
     */
    fprintf(fid, "RPC_SS_INIT_SERVER\n");
    fprintf(fid, "st = error_status_ok;\n");
    fprintf(fid, "mem_handle.memory=(rpc_void_p_t)NULL;\n");
    fprintf(fid, "mem_handle.node_table=(rpc_ss_node_table_t)NULL;\n");
    fprintf(fid, "TRY\n");

    BE_spell_pointer_init(fid, p_operation);

    /*
     * Initialize the assorted node and pipe related locals
     */
    if (AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation))
    {
        fprintf(fid, "sp_iovec.num_elt = 1;\n");
        fprintf(fid, "NIDL_msp->p_iovec = (rpc_iovector_t*)&sp_iovec;\n");
    }
    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_HAS_IN_OOLS_SET(p_operation)
    || AST_HAS_OUT_OOLS_SET(p_operation)
    || AST_HAS_IN_PIPES_SET(p_operation)
    || AST_HAS_OUT_PIPES_SET(p_operation)
    || AST_HAS_IN_CTX_SET(p_operation)
    || AST_HAS_OUT_CTX_SET(p_operation)
    || AST_ENABLE_ALLOCATE_SET(p_operation))
    {
        fprintf(fid, "NIDL_msp->p_st = (error_status_t*)&st;\n");
        fprintf(fid, "unmar_params.p_mem_h = &mem_handle;\n");
        fprintf(fid, "unmar_params.p_rcvd_data = elt;\n");
        fprintf(fid, "unmar_params.src_drep = *drep;\n");
        fprintf(fid, "unmar_params.call_h = call_h;\n");
        fprintf(fid, "unmar_params.version = 0;\n");
    }

    /*
     * transmit_as/represent_as surrogate initialization.  This needs to be
     * done before ref pointer allocation in case there are any ref pointers
     * in the net rep of an xmit/repas type.
     */
    {
        AST_parameter_n_t *param;
        for (param = BE_Flat_Params(p_operation); param; param = param->next)
        {
            if (BE_Calls_Before(param))
            {
                BE_spell_assign_xmits(fid, param);
                BE_spell_assign_nets(fid, param);
            }
        }
    }

    /*
     * Node initializations
     */
    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_ENABLE_ALLOCATE_SET(p_operation))
    {
        fprintf( fid,"rpc_ss_create_support_ptrs( &NIDL_support_ptrs,&mem_handle);\n" );
        if (AST_HAS_FULL_PTRS_SET(p_operation))
            fprintf(fid,"rpc_ss_init_node_table(&NIDL_msp->node_table,&mem_handle);\n");
    }

    BE_spell_alloc_ref_pointers(fid, BE_Flat_Params(p_operation), BE_arp_before,
                                false);

    /*
     * Pipe initializations
     */
    if (AST_HAS_IN_PIPES_SET(p_operation) || AST_HAS_OUT_PIPES_SET(p_operation))
        CSPELL_init_server_pipes( fid, p_operation, &first_pipe );

    if (AST_HAS_INS_SET(p_operation) || AST_HAS_IN_PIPES_SET(p_operation))
        fprintf(fid, "rpc_init_mp(mp, elt->data_addr);\n");

    if (AST_HAS_INS_SET(p_operation)) BE_unmarshall(fid, p_operation, BE_in);
    else if (AST_HAS_IN_PIPES_SET(p_operation))
        fprintf(fid, "rpc_init_op(op);\n");

    BE_spell_alloc_open_outs(fid, p_operation);

    if (AST_HAS_IN_PIPES_SET(p_operation) || AST_HAS_OUT_PIPES_SET(p_operation))
        fprintf( fid, "current_pipe=0x%x;\n", first_pipe );

    if ( ( ! AST_HAS_IN_PIPES_SET(p_operation) )
            && AST_HAS_OUT_PIPES_SET(p_operation) )
        fprintf(fid, "rpc_init_op(op);\n");

    if (AST_HAS_IN_CTX_SET(p_operation)
        || AST_HAS_OUT_CTX_SET(p_operation))
    {
        fprintf(fid,
             "rpc_ss_ctx_client_ref_count_inc(h, (error_status_t *)&st);\n");
        CSPELL_test_status(fid);
    }

    CSPELL_manager_call(fid, p_interface, p_operation,
                                 rep_as_handle_param, rep_as_type_name);

    if (AST_HAS_IN_PIPES_SET(p_operation) || AST_HAS_OUT_PIPES_SET(p_operation))
    {
        fprintf( fid, "if (current_pipe != %d)\n", BE_FINISHED_WITH_PIPES );
        fprintf( fid, "{\n" );
        fprintf( fid, "RAISE(rpc_x_ss_pipe_discipline_error);\n" );
        fprintf( fid, "}\n" );
    }

    if (AST_HAS_OUTS_SET(p_operation)) BE_marshall(fid, p_operation, BE_out);

    /*
     * Emit TYPE_free() calls for any [in] transmit_as types and
     * free_local_rep() calls for any [in] represent_as types
     */
    BE_spell_type_frees(fid, BE_Recs(p_operation), TRUE);
    BE_spell_free_locals(fid, BE_Recs(p_operation), TRUE);

    fprintf(fid, "closedown: ;\n");
    fprintf(fid, "CATCH_ALL\n");

    /*
     * For all exceptions other than report status, send the exception to the
     * client.  For the report status exception, just fall through and
     * perform the normal failing status reporting.w
     */
    fprintf(fid,
         "if (!RPC_SS_EXC_MATCHES(THIS_CATCH,&rpc_x_ss_pipe_comm_error))\n{\n");
    fprintf(fid, "if ( ! NIDL_manager_entered )\n{\n");
    BE_spell_end_unmarshall( fid, &BE_def_mn );
    if ( ! explicit_binding )
    {
        fprintf(fid, "st = rpc_s_manager_not_entered;\n");
    }
    fprintf(fid, "}\n");
    if ( ! explicit_binding )
    {
        fprintf(fid, "else\n");
    }
    fprintf(fid, "{\nrpc_ss_send_server_exception(call_h,THIS_CATCH);\n");
    fprintf(fid, "st = error_status_ok;\n}\n");
    fprintf(fid, "}\n");
    fprintf(fid, "ENDTRY\n");

    if (AST_HAS_IN_CTX_SET(p_operation)
        || AST_HAS_OUT_CTX_SET(p_operation))
    {
        fprintf(fid,
             "rpc_ss_ctx_client_ref_count_dec(h, (error_status_t *)&st);\n");
    }

    if (AST_HAS_IN_PTRS_SET(p_operation)
    || AST_HAS_OUT_PTRS_SET(p_operation)
    || AST_ENABLE_ALLOCATE_SET(p_operation))
    {
        fprintf( fid, "rpc_ss_destroy_support_ptrs();\n" );
    }
    fprintf(fid, "if (mem_handle.memory) rpc_ss_mem_free(&mem_handle);\n");
    fprintf(fid, "if (st != error_status_ok)\n{\n");
    fprintf(fid, "if (st == rpc_s_call_cancelled)\n{\n");
    fprintf(fid,   "rpc_ss_send_server_exception(");
    fprintf(fid,      "call_h,&RPC_SS_THREADS_X_CANCELLED);\n");
    fprintf(fid,   "st = error_status_ok;\n");
    fprintf(fid,  "}\nelse\n{\n");
    if ( ! explicit_binding )
    {
        fprintf(fid, "if (NIDL_manager_entered)\n");
    }
    fprintf(fid,   "{\nrpc_ss_send_server_exception(");
    fprintf(fid,      "call_h,&rpc_x_ss_remote_comm_failure);\n");
    fprintf(fid,   "st = error_status_ok;\n");
    fprintf(fid, "}\n}\n}\n");

    /* When reached here st is either error_status_ok
     * or rpc_s_manager_not_entered
     */
    fprintf(fid, "*st_p = st;\n");

    fprintf(fid, "}\n");
}

/*
 * BE_gen_sstub
 *
 * Public entry point for server stub file generation
 */
void BE_gen_sstub
#ifdef PROTO
(
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *p_interface,     /* Ptr to AST interface node */
    language_k_t language,  /* Language stub is to interface to */
    char header_name[],     /* Name of header file to be included in stub */
    boolean bugs[],         /* List of backward compatibility "bugs" */
    boolean generate_mepv   /* TRUE if a manager epv is to be generated */
)
#else
(fid, p_interface, language, header_name, bugs, generate_mepv)
    FILE *fid;
    AST_interface_n_t *p_interface;
    language_k_t language;
    char header_name[];
    boolean bugs[];
    boolean generate_mepv;
#endif
{
    AST_export_n_t *p_export;
    AST_operation_n_t *p_operation;
    boolean first;

    /*
     * Emit #defines and #includes
     */
    CSPELL_std_include(fid, header_name, BE_server_stub_k, p_interface->op_count);

    BE_pointed_at_routines( fid, p_interface, BE_callee );

    /*
     * Emit operation definitions
     */
    for (p_export = p_interface->exports; p_export; p_export = p_export->next)
        if (p_export->kind == AST_operation_k)
        {
            BE_push_malloc_ctx();
            NAMETABLE_set_temp_name_mode();
            p_operation = p_export->thing_p.exported_operation;
            CSPELL_server_stub_routine(fid, language, p_interface, p_operation);
            NAMETABLE_clear_temp_name_mode();
            BE_pop_malloc_ctx();
        }

    /*
     * Emit server epv
     */
    fprintf (fid, "\nstatic rpc_v2_server_stub_proc_t NIDL_epva[] = \n{\n");
    first = true;
    for (p_export = p_interface->exports; p_export; p_export = p_export->next)
        if (p_export->kind == AST_operation_k)
        {
            if (first)
                first = false;
            else
                fprintf(fid, ",\n");
            fprintf(
                fid, " (rpc_v2_server_stub_proc_t)op%ld_ssr",
                p_export->thing_p.exported_operation->op_number
            );
        }
    fprintf (fid, "\n};\n");

    /*
     * Emit static if_spec definition and global exported pointer
     */
    CSPELL_interface_def(fid, p_interface, BE_server_stub_k, generate_mepv);
}
