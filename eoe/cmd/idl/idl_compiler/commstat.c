/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: commstat.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:38  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:34  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:44:45  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:46  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:52  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      commstat.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Deliver comm_status or raise exception in client stub
**      Deliver fault_status or raise exception in client stub
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <commstat.h>
#include <cspell.h>
#include <nametbl.h>

/*******************************************************************************/
/*                                                                             */
/*  Determine how comm_status is to be returned                                */
/*                                                                             */
/*******************************************************************************/
void BE_get_comm_stat_info
#ifdef PROTO
(
    AST_operation_n_t *p_operation,
    BE_stat_info_t *p_comm_stat_info
)
#else
( p_operation, p_comm_stat_info )
    AST_operation_n_t *p_operation;
    BE_stat_info_t *p_comm_stat_info;
#endif
{
    AST_parameter_n_t *p_parameter;

    if ( AST_COMM_STATUS_SET(p_operation->result) )
    {
        p_comm_stat_info->type = BE_stat_result_k;
        return;
    }
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( AST_COMM_STATUS_SET(p_parameter) )
        {
            p_comm_stat_info->type = BE_stat_param_k;
            p_comm_stat_info->name = p_parameter->be_info.param->name;
            return;
        }
        else if ( AST_ADD_COMM_STATUS_SET(p_parameter) )
        {
            p_comm_stat_info->type = BE_stat_addl_k;
            p_comm_stat_info->name = p_parameter->be_info.param->name;
            return;
        }
    }
    p_comm_stat_info->type = BE_stat_except_k;
}

/*******************************************************************************/
/*                                                                             */
/*  Determine how fault_status is to be returned                               */
/*                                                                             */
/*******************************************************************************/
void BE_get_fault_stat_info
#ifdef PROTO
(
    AST_operation_n_t *p_operation,
    BE_stat_info_t *p_fault_stat_info
)
#else
( p_operation, p_fault_stat_info )
    AST_operation_n_t *p_operation;
    BE_stat_info_t *p_fault_stat_info;
#endif
{
    AST_parameter_n_t *p_parameter;

    if ( AST_FAULT_STATUS_SET(p_operation->result) )
    {
        p_fault_stat_info->type = BE_stat_result_k;
        return;
    }
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( AST_FAULT_STATUS_SET(p_parameter) )
        {
            p_fault_stat_info->type = BE_stat_param_k;
            p_fault_stat_info->name = p_parameter->be_info.param->name;
            return;
        }
        else if ( AST_ADD_FAULT_STATUS_SET(p_parameter) )
        {
            p_fault_stat_info->type = BE_stat_addl_k;
            p_fault_stat_info->name = p_parameter->be_info.param->name;
            return;
        }
    }
    p_fault_stat_info->type = BE_stat_except_k;
}

/*******************************************************************************/
/*                                                                             */
/*    Spell code that returns status to client                                 */
/*                                                                             */
/*******************************************************************************/
void CSPELL_return_status
#ifdef PROTO
(
    FILE *fid,
    BE_stat_info_t *p_comm_stat_info,
    BE_stat_info_t *p_fault_stat_info,
    char *status_var_name,
    char *result_param_name
)
#else
( fid, p_comm_stat_info, p_fault_stat_info, status_var_name, result_param_name )
    FILE *fid;
    BE_stat_info_t *p_comm_stat_info;
    BE_stat_info_t *p_fault_stat_info;
    char *status_var_name;
    char *result_param_name;
#endif
{
#define MAX_STATUS_STRING 72+MAX_ID+MAX_ID
    char *str_p_comm_status;
    char *str_p_fault_status;
    char *name_work;
    char comm_status_work[MAX_STATUS_STRING];
    char fault_status_work[MAX_STATUS_STRING];

    /* Handle [comm_status] parameters */
    switch( p_comm_stat_info->type )
    {
        case BE_stat_addl_k:
            /* 
             *  If an added comm_status parameter, always pass it to
             *  rpc_ss_report_error so that it will be set to either
             *  error_status_ok, or the correct status value.
             */
            NAMETABLE_id_to_string( p_comm_stat_info->name,
                                                       &str_p_comm_status );
            break;
        case BE_stat_result_k:
            /*
             * For a function result, pass the address of the surrogate, if
             * we had a comm_status-related error.  Otherwise pass NULL so
             * that rpc_ss_report_error will not overwrite the user's value
             */
            sprintf( comm_status_work,"(%s!=error_status_ok) ? &%s : NULL",
                        status_var_name, result_param_name );
            str_p_comm_status = comm_status_work;
            break;
        case BE_stat_param_k:
            /*
             *  For a user parameter, pass the parameter, if we had a
             *  comm_status-related error.  Otherwise pass NULL so that
             *  rpc_ss_report_error will not overwrite the user's value
             */
            NAMETABLE_id_to_string( p_comm_stat_info->name, &name_work );
            sprintf( comm_status_work,"(%s!=error_status_ok) ? %s : NULL",
                        status_var_name, name_work );
            str_p_comm_status = comm_status_work;
            break;
        default:
            str_p_comm_status = "NULL";
            break;
    }
    ASSERTION(strlen(str_p_comm_status) < MAX_STATUS_STRING);

    /* Handle [fault_status] parameters */
    switch( p_fault_stat_info->type )
    {
        case BE_stat_addl_k:
            /* 
             *  If an added fault_status parameter, always pass it to
             *  rpc_ss_report_error so that it will be set to either
             *  error_status_ok, or the correct status value.
             */
            NAMETABLE_id_to_string( p_fault_stat_info->name,
                                                       &str_p_fault_status );
            break;
        case BE_stat_result_k:
            /*
             *  For a function result, pass the address of the surrogate, if we
             *  had an error.  Otherwise pass NULL so that rpc_ss_report_error
             *  will not overwrite the user's value.  We need not distiguish
             *  between comm/fault errors because either will mean that the
             *  user's values has not been unmarshalled anyway and thus we
             *  should set it to error_status_ok.
             */
            sprintf( fault_status_work,"(%s!=error_status_ok) ? &%s : NULL",
                        status_var_name, result_param_name );
            str_p_fault_status = fault_status_work;
            break;
        case BE_stat_param_k:
            /*
             *  For a function result, pass the address of the surrogate, if we
             *  had an error.  Otherwise pass NULL so that rpc_ss_report_error
             *  will not overwrite the user's value.  We need not distiguish
             *  between comm/fault errors because either will mean that the
             *  user's values has not been unmarshalled anyway and thus we
             *  should set it to error_status_ok.
             */
            NAMETABLE_id_to_string( p_fault_stat_info->name, &name_work );
            sprintf( fault_status_work,"(%s!=error_status_ok) ? %s : NULL",
                        status_var_name, name_work );
            str_p_fault_status = fault_status_work;
            break;
        default:
            str_p_fault_status = "NULL";
            break;
    }
    ASSERTION(strlen(str_p_fault_status) < MAX_STATUS_STRING);

    fprintf( fid,
"rpc_ss_report_error(NIDL_fault_code,%s,NIDL_async_cancel_state,\n %s,\n %s);\n",
                      status_var_name, str_p_comm_status, str_p_fault_status );

}
