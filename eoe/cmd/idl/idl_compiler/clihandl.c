/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: clihandl.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:38:28  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:19  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:44:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:30  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/24  14:09:14  harrow
 * 	Add cast on result of bind routine to prevent ANSI C warnings.
 * 	[1992/03/19  15:16:27  harrow]
 * 
 * Revision 1.1  1992/01/19  03:00:49  devrcs
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
**      clihandl.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Client binding handle stuff
**
**  VERSION: DCE 1.0
**
*/
#include <nidl.h>
#include <ast.h>
#include <bedeck.h>

#include <commstat.h>
#include <ifspec.h>
#include <clihandl.h>
#include <dutils.h>
#include <cspell.h>
#include <cstubgen.h>
#include <marshall.h>

static char assoc_handle_name[] = "assoc_handle";
char assoc_handle_ptr[] = "IDL_assoc_handle_p";

/******************************************************************************/
/*                                                                            */
/*    Spell declaration of assoc_handle                                       */
/*                                                                            */
/******************************************************************************/
static void CSPELL_decl_assoc_handle_vars
#ifdef PROTO
(
    FILE *fid,
    BE_handle_info_t *p_handle_info
)
#else
(fid, p_handle_info)
    FILE *fid;
    BE_handle_info_t *p_handle_info;
#endif
{
    if ( (p_handle_info->handle_type == BE_impl_handle_t_k)
         || (p_handle_info->handle_type == BE_context_handle_k)
         || (p_handle_info->handle_type == BE_auto_handle_k) )
    {
        fprintf( fid, "error_status_t NIDL_st2;\n" );
    }
    if ( (p_handle_info->handle_type == BE_rep_as_handle_t_k)
         || (p_handle_info->handle_type == BE_rep_as_handle_t_p_k) )
    {
        fprintf( fid, "volatile handle_t *%s;\n", assoc_handle_ptr );
    }
    else
    {
        fprintf (fid, "volatile handle_t %s;\n", assoc_handle_name);
    }
}

/******************************************************************************/
/*                                                                            */
/*    Analyze the type of handle to be used in the call, and emit any         */
/*    necessary declarations                                                  */
/*                                                                            */
/******************************************************************************/
void BE_setup_client_handle
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation,
    BE_handle_info_t *p_handle_info
)
#else
(fid, p_interface, p_operation, p_handle_info)
    FILE *fid;
    AST_interface_n_t *p_interface;
    AST_operation_n_t *p_operation;
    BE_handle_info_t *p_handle_info;
#endif
{
    AST_parameter_n_t *p_first_parameter;
    AST_type_n_t *p_type;

    p_handle_info->deref_assoc = ' ';
    p_handle_info->deref_generic = ' ';
    p_first_parameter = p_operation->parameters;
    if (p_first_parameter != NULL)
    {
        p_type = p_first_parameter->type;
        if (p_type->kind == AST_handle_k)
        {
            if (p_type->rep_as_type != NULL)
            {
                p_handle_info->handle_type = BE_rep_as_handle_t_k;
                p_handle_info->rep_as_name =
                                 p_first_parameter->be_info.param->name;
                p_handle_info->rep_as_type = p_type->name;
                p_handle_info->assoc_name = assoc_handle_ptr;
                p_handle_info->deref_assoc = '*';
                CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
                return;
            }
            p_handle_info->handle_type = BE_parm_handle_t_k;
            NAMETABLE_id_to_string(
                    p_first_parameter->be_info.param->name,
                    &(p_handle_info->assoc_name) );
            return;
        }
        if ( (p_type->kind == AST_pointer_k)
              && (p_type->type_structure.pointer->pointee_type->kind
                    == AST_handle_k) )
        {
            if (p_type->type_structure.pointer->pointee_type->rep_as_type
                                                                != NULL)
            {
                p_handle_info->handle_type = BE_rep_as_handle_t_p_k;
                p_handle_info->rep_as_name =
                                 p_first_parameter->be_info.param->name;
                p_handle_info->rep_as_type = p_type->type_structure.pointer
                                    ->pointee_type->name;
                p_handle_info->assoc_name = assoc_handle_ptr;
                p_handle_info->deref_assoc = '*';
                CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
                return;
            }
            p_handle_info->handle_type = BE_parm_handle_t_k;
            NAMETABLE_id_to_string(
                    p_first_parameter->be_info.param->name,
                    &(p_handle_info->assoc_name) );
            p_handle_info->deref_assoc = '*';
            return;
        }
        if ( AST_HANDLE_SET(p_type) )
        {
            p_handle_info->handle_type = BE_parm_user_handle_k;
            p_handle_info->assoc_name = assoc_handle_name;
            CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
            NAMETABLE_id_to_string( p_type->name, &(p_handle_info->type_name) );
            NAMETABLE_id_to_string(
                    p_first_parameter->be_info.param->name,
                    &(p_handle_info->user_handle_name) );
            return;
        }
        if ( (p_type->kind == AST_pointer_k)
             && AST_HANDLE_SET(p_type->type_structure.pointer->pointee_type) )
        {
            p_handle_info->handle_type = BE_parm_user_handle_k;
            p_handle_info->assoc_name = assoc_handle_name;
            CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
            NAMETABLE_id_to_string(
                          p_type->type_structure.pointer->pointee_type->name,
                          &(p_handle_info->type_name) );
            NAMETABLE_id_to_string(
                    p_first_parameter->be_info.param->name,
                    &(p_handle_info->user_handle_name) );
            p_handle_info->deref_generic = '*';
            return;
        }
        if ( AST_HAS_IN_CTX_SET(p_operation) )
        {
            p_handle_info->handle_type = BE_context_handle_k;
            p_handle_info->assoc_name = assoc_handle_name;
            CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
            /* We only bind this way if the first parameter is not handle_t or
               [handle], so there is a non-parametric binding to fall back to */
            if (p_interface->implicit_handle_name != NAMETABLE_NIL_ID)
            {
                if ( ! AST_IMPLICIT_HANDLE_G_SET(p_interface) )
                {
                    return;
                }
                NAMETABLE_id_to_string( p_interface->implicit_handle_type_name,
                                                  &(p_handle_info->type_name) );
                NAMETABLE_id_to_string( p_interface->implicit_handle_name,
                                           &(p_handle_info->user_handle_name) );
                return;
            }
            /* If we get here, must be [auto_handle] */
            fprintf( fid, "volatile idl_boolean NIDL_restartable=idl_true;\n" );
            fprintf( fid, "idl_boolean NIDL_fallen_back_to_auto=idl_false;\n" );
            return;
        }
    }
    if (p_interface->implicit_handle_name != NAMETABLE_NIL_ID)
    {
        p_handle_info->assoc_name = assoc_handle_name;
        if ( ! AST_IMPLICIT_HANDLE_G_SET(p_interface) )
        {
            p_handle_info->handle_type = BE_impl_handle_t_k;
            CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
            return;
        }
        CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
        /* Checker ensures we must have a [handle] type here */
        p_handle_info->handle_type = BE_impl_user_handle_k;
        NAMETABLE_id_to_string( p_interface->implicit_handle_type_name,
                               &(p_handle_info->type_name) );
        NAMETABLE_id_to_string( p_interface->implicit_handle_name,
                               &(p_handle_info->user_handle_name) );
        return;
    }
    /* If we get here, must be [auto_handle] */
    p_handle_info->handle_type = BE_auto_handle_k;
    p_handle_info->assoc_name = assoc_handle_name;
    p_handle_info->auto_handle_idempotent_op =
                    ( AST_IDEMPOTENT_SET(p_operation) ) ? true : false;
    CSPELL_decl_assoc_handle_vars(fid,p_handle_info);
    fprintf( fid, "volatile idl_boolean NIDL_restartable=idl_true;\n" );
    fprintf( fid, "idl_boolean NIDL_timeout_was_set_low=idl_false;\n" );
    return;
}

/******************************************************************************/
/*                                                                            */
/*    Spell "duplicate implicit handle_t handle"                              */
/*                                                                            */
/******************************************************************************/
static void CSPELL_dup_implicit_handle_t
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface
)
#else
( fid, p_interface )
    FILE *fid;
    AST_interface_n_t *p_interface;
#endif
{
    fprintf(fid,"rpc_binding_handle_copy(");
    spell_name(fid, p_interface->implicit_handle_name);
    fprintf(fid, ",(rpc_binding_handle_t*)&assoc_handle");
    fprintf(fid, ",(error_status_t*)&st);\n" );
    CSPELL_test_status(fid);
}

/******************************************************************************/
/*                                                                            */
/*    Spell "use a generic handle to get a binding"                           */
/*                                                                            */
/******************************************************************************/
static void CSPELL_bind_generic_handle
#ifdef PROTO
(
    FILE *fid,
    BE_handle_info_t *p_handle_info
)
#else
( fid, p_handle_info )
    FILE *fid;
    BE_handle_info_t *p_handle_info;
#endif
{
        fprintf (fid, "assoc_handle = (volatile handle_t)%s_bind(%c%s);\n",
                 p_handle_info->type_name, p_handle_info->deref_generic,
                 p_handle_info->user_handle_name);
}

/******************************************************************************/
/*                                                                            */
/*    Spell "use the [auto_handle] mechanism to get a binding"                */
/*                                                                            */
/******************************************************************************/
static void CSPELL_bind_auto_handle
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *p_operation,
    BE_stat_info_t *p_comm_stat_info,
    BE_stat_info_t *p_fault_stat_info
)
#else
( fid, p_operation, p_comm_stat_info, p_fault_stat_info )
    FILE *fid;
    AST_operation_n_t *p_operation;
    BE_stat_info_t *p_comm_stat_info;
    BE_stat_info_t *p_fault_stat_info;
#endif
{
        fprintf (fid,"rpc_ss_make_import_cursor_valid(&NIDL_auto_handle_mutex,\n");
        fprintf (fid,    "&NIDL_import_cursor,\n" );
        fprintf (fid,
          "   (rpc_if_handle_t)&NIDL_ifspec,&NIDL_auto_handle_status);\n" );
        fprintf( fid, "if(NIDL_auto_handle_status!=error_status_ok)\n" );
        fprintf( fid, "{\n" );
        fprintf( fid, "st=NIDL_auto_handle_status;\n");
        fprintf( fid, "goto IDL_auto_binding_failure;\n");
        fprintf( fid, "}\n" );
        fprintf (fid, "find_server:\nTRY\n");
        fprintf (fid,"rpc_ss_import_cursor_advance(&NIDL_auto_handle_mutex,\n");
        fprintf (fid,  "&NIDL_timeout_was_set_low,&NIDL_import_cursor,\n" );
        fprintf (fid,  "(rpc_if_handle_t)&NIDL_ifspec,&NIDL_error_using_binding,\n" );
        fprintf (fid,  "&NIDL_interface_binding,(rpc_binding_handle_t*)&assoc_handle,\n");
        fprintf (fid,  "(error_status_t*)&NIDL_auto_handle_status,(error_status_t*)&st);\n");
        fprintf (fid, "if(st!=error_status_ok)\n");
        fprintf (fid, "{\n");
        fprintf (fid, "NIDL_restartable=idl_false;\n");
        fprintf (fid, "goto closedown;\n");
        fprintf (fid, "}\n");
}

/******************************************************************************/
/*                                                                            */
/*    Spell "prepare for and make call to rpc_call_start"                     */
/*                                                                            */
/******************************************************************************/
void CSPELL_call_start
#ifdef PROTO
(
    FILE *fid,
    BE_handle_info_t *p_handle_info,
    AST_interface_n_t *p_interface,
    AST_operation_n_t *p_operation,
    unsigned long op_num,            /* Number of current operation */
    BE_stat_info_t *p_comm_stat_info,
    BE_stat_info_t *p_fault_stat_info
)
#else
(fid, p_handle_info, p_interface, p_operation, op_num, p_comm_stat_info,
 p_fault_stat_info)
    FILE *fid;
    BE_handle_info_t *p_handle_info;
    AST_interface_n_t *p_interface;
    AST_operation_n_t *p_operation;
    unsigned long op_num;            /* Number of current operation */
    BE_stat_info_t *p_comm_stat_info;
    BE_stat_info_t *p_fault_stat_info;
#endif
{
    AST_parameter_n_t *p_parameter;
    char *parameter_name;
    char deref_context;
    boolean found_in_context_handle;

    if (p_handle_info->handle_type == BE_impl_handle_t_k)
    {
        CSPELL_dup_implicit_handle_t( fid, p_interface );
    }
    else if ((p_handle_info->handle_type == BE_parm_user_handle_k)
        || (p_handle_info->handle_type == BE_impl_user_handle_k))
    {
        CSPELL_bind_generic_handle( fid, p_handle_info );
    }
    else if (p_handle_info->handle_type == BE_context_handle_k)
    {
        fprintf (fid, "assoc_handle = NULL;\n");
        found_in_context_handle = false;
        for (p_parameter = p_operation->parameters;
             p_parameter != NULL;
             p_parameter = p_parameter->next)
        {
            if ( AST_CONTEXT_SET(p_parameter) && AST_IN_SET(p_parameter)
                    && !AST_OUT_SET(p_parameter) )
            {
                if ( p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_void_k ) deref_context = ' ';
                else deref_context = '*';  /* Dereference ptr to context */
                NAMETABLE_id_to_string (
                   p_parameter->be_info.param->name,
                            &parameter_name);
                fprintf (fid, "if ( %c%s != NULL )\n",
                                    deref_context, parameter_name);
                fprintf (fid, "{\n");
                fprintf(fid, "rpc_binding_handle_copy(");
                fprintf(fid, "((rpc_ss_caller_context_element_t *)");
                fprintf(fid, "%c%s)->using_handle,\n", deref_context, parameter_name);
                fprintf(fid, "(rpc_binding_handle_t*)&assoc_handle,");
                fprintf(fid, "(error_status_t*)&st);\n");
                CSPELL_test_status (fid);
                fprintf (fid, "}\n");
                found_in_context_handle = true;
                break;
            }
        }
        if ( ! found_in_context_handle )
        {
            for (p_parameter = p_operation->parameters;
                 p_parameter != NULL;
                 p_parameter = p_parameter->next)
            {
                if ( AST_CONTEXT_SET(p_parameter) && AST_IN_SET(p_parameter)
                    && AST_OUT_SET(p_parameter) )
                {
                    if ( p_parameter->type->type_structure.pointer->pointee_type
                         ->kind == AST_void_k ) deref_context = ' ';
                    else deref_context = '*';  /* Dereference ptr to context */
                    NAMETABLE_id_to_string (p_parameter->be_info.param->name,
                            &parameter_name);
                    fprintf (fid, "if ( %c%s != NULL )\n",
                                        deref_context, parameter_name);
                    fprintf (fid, "{\n");
                    fprintf(fid, "rpc_binding_handle_copy(");
                    fprintf(fid, "((rpc_ss_caller_context_element_t *)");
                    fprintf(fid, "%c%s)->using_handle,\n", deref_context, parameter_name);
                    fprintf(fid, "(rpc_binding_handle_t*)&assoc_handle, ");
                    fprintf(fid, "(error_status_t*)&st);\n");
                    CSPELL_test_status (fid);
                    fprintf (fid, "goto make_assoc;\n");
                    fprintf (fid, "}\n");
                }
            }
            fprintf (fid, "make_assoc:\n  ");
        }
        /* If assoc_handle remains null, raise an exception */
        fprintf (fid, "if(assoc_handle == NULL)\n");
        fprintf (fid, "{\n");
        fprintf (fid, "RAISE(rpc_x_ss_in_null_context);\n");
        fprintf (fid, "}\n");
    }
    else if (p_handle_info->handle_type == BE_auto_handle_k)
    {
        fprintf (fid, "assoc_handle = NULL;\n");
        CSPELL_bind_auto_handle( fid, p_operation, p_comm_stat_info,
                                      p_fault_stat_info );
    }
    else if (p_handle_info->handle_type == BE_rep_as_handle_t_k)
    {
        fprintf( fid, "%s_from_local(&%s,(handle_t **)&%s);\n",
                    BE_get_name(p_handle_info->rep_as_type),
                    BE_get_name(p_handle_info->rep_as_name),
                    assoc_handle_ptr );
    }
    else if (p_handle_info->handle_type == BE_rep_as_handle_t_p_k)
    {
        fprintf( fid, "%s_from_local(%s,(handle_t **)&%s);\n",
                    BE_get_name(p_handle_info->rep_as_type),
                    BE_get_name(p_handle_info->rep_as_name),
                    assoc_handle_ptr );
    }

    fprintf(fid, "rpc_call_start((rpc_binding_handle_t)%c%s, 0",
                    p_handle_info->deref_assoc, p_handle_info->assoc_name);
    if ( AST_BROADCAST_SET(p_operation) )
    {
        fprintf( fid, "|rpc_c_call_brdcst" );
    }
    if ( AST_MAYBE_SET(p_operation) )
    {
        fprintf( fid, "|rpc_c_call_maybe" );
    }
    if ( AST_IDEMPOTENT_SET(p_operation) )
    {
        fprintf( fid, "|rpc_c_call_idempotent" );
    }
    fprintf(fid,",\n (rpc_if_handle_t)&NIDL_ifspec,%ld,", op_num);
    fprintf(fid,"(rpc_call_handle_t*)&call_h,&transfer_syntax,(unsigned32*)&st);\n");
    CSPELL_test_status (fid);
}

/******************************************************************************/
/*                                                                            */
/*    Spell interface-wide declarations needed for [auto_handle]              */
/*                                                                            */
/******************************************************************************/
void CSPELL_auto_handle_statics
#ifdef PROTO
(
    FILE * fid
)
#else
(fid)
    FILE *fid;
#endif
{
    /* Declare the variables */
    fprintf( fid,
"static RPC_SS_THREADS_ONCE_T NIDL_interface_client_once = RPC_SS_THREADS_ONCE_INIT;\n" );
    fprintf( fid, "static RPC_SS_THREADS_MUTEX_T NIDL_auto_handle_mutex;\n" );
    fprintf( fid, "static rpc_ns_handle_t NIDL_import_cursor;\n" );
    fprintf( fid,
              "static rpc_binding_handle_t NIDL_interface_binding = NULL;\n" );
    fprintf( fid, "static idl_boolean NIDL_error_using_binding = idl_false;\n");
    fprintf( fid,
  "static error_status_t NIDL_auto_handle_status=rpc_s_ss_no_import_cursor;\n" );

    /* And a procedure that will be called once to initialize them */
    fprintf( fid, "static void NIDL_auto_handle_init()\n" );
    fprintf( fid, "{\n" );
    fprintf( fid,
            "RPC_SS_THREADS_MUTEX_CREATE( &NIDL_auto_handle_mutex );\n" );
    fprintf( fid, "}\n" );
}

/******************************************************************************/
/*                                                                            */
/*    Spell logic for whether to restart an [auto_handle] operation           */
/*                                                                            */
/******************************************************************************/
void CSPELL_restart_logic
#ifdef PROTO
(
    FILE * fid,
    AST_operation_n_t *p_operation,
    boolean uses_packet
)
#else
(fid,p_operation,uses_packet)
    FILE *fid;
    AST_operation_n_t *p_operation;
    boolean uses_packet;
#endif
{
    fprintf( fid, "ENDTRY\n" );
    fprintf( fid,
 "if ((st != error_status_ok) && (st != rpc_s_no_more_bindings))\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "if (NIDL_restartable)\n" );
    fprintf( fid, "{\n" );
    fprintf( fid, "if (call_h!=NULL) rpc_call_end((rpc_call_handle_t*)&call_h");
    fprintf( fid,     ",(unsigned32*)&st);\n" );
    fprintf( fid, "call_h=NULL;\n" );
    fprintf( fid, "if( elt != NULL )\n{\n" );
    BE_spell_end_unmarshall( fid, &BE_def_mn );
    fprintf( fid, "elt=NULL;\n}\n" );
    if (uses_packet)
        fprintf(fid, "pmp=(rpc_mp_t)&packet[0];\n");
    fprintf( fid, "goto find_server;\n" );
    fprintf( fid, "}\n" );
    fprintf( fid,
"else rpc_ss_flag_error_on_binding(&NIDL_auto_handle_mutex,\n");
    fprintf (fid, "   &NIDL_error_using_binding,\n" );
    fprintf (fid, "   &NIDL_interface_binding,(rpc_binding_handle_t*)&assoc_handle);\n");

    fprintf( fid, "}\n" );
}


/******************************************************************************/
/*                                                                            */
/*  If the call was made on a binding handle made by rpc_binding_handle_copy, */
/*    emit a call to rpc_binding_free                                         */
/*                                                                            */
/******************************************************************************/
void CSPELL_binding_free_if_needed
#ifdef PROTO
(
    FILE *fid,
    BE_handle_info_t *p_handle_info
)
#else
(fid, p_handle_info)
    FILE *fid;
    BE_handle_info_t *p_handle_info;
#endif
{
    if (p_handle_info->handle_type == BE_impl_handle_t_k)
    {
        fprintf( fid, "rpc_binding_free((handle_t*)&assoc_handle,&NIDL_st2);\n" );
    }
    else if ( (p_handle_info->handle_type == BE_auto_handle_k)
               || (p_handle_info->handle_type == BE_context_handle_k) )
    {
        fprintf( fid,
         "if(assoc_handle!=NULL)rpc_binding_free((rpc_binding_handle_t*)&assoc_handle,&NIDL_st2);\n" );
    }
}
