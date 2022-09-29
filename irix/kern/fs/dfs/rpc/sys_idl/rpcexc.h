/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcexc.h,v $
 * Revision 65.1  1997/10/20 19:16:20  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.543.2  1996/02/18  22:57:25  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:18  marty]
 *
 * Revision 1.1.543.1  1995/12/08  00:23:41  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:11 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/08  00:01:18  root]
 * 
 * Revision 1.1.539.3  1994/08/23  20:19:15  tom
 * 	Add exception: rpc_x_ss_codeset_conv_error (OT 10410)
 * 	[1994/08/23  20:17:33  tom]
 * 
 * Revision 1.1.539.2  1994/02/18  13:07:12  rico
 * 	Add new exception rpc_x_stub_protocol_error.
 * 	[1994/02/17  12:48:11  rico]
 * 
 * Revision 1.1.539.1  1994/01/21  22:40:06  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:30:53  cbrooks]
 * 
 * Revision 1.1.4.2  1993/07/07  20:12:49  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:40:44  ganni]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1990, 1991, 1993 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**
**  NAME:
**
**      rpcexc.h
**
**  FACILITY:
**
**      Remote Procedure Call
**
**  ABSTRACT:
**
**      Include file to make NIDL exceptions visible to user code
**
**  %a%private_begin
**
**  MODIFICATION HISTORY:
**
**  07-Jun-93 A.I.Hinxman  Change exception for type vector mismatch
**  17-May-93 A.I.Hinxman  International character support
**  04-Jan-93 A.I.Hinxman  Add exceptions for IDL encoding services
**  12-Nov-91 harrow       Remove rpc_x_call_failed, obsolete.
**  23-Oct-91 harrow       Remove exceptions runtime removed in RPC5c
**  21-aug-91 mishkin      cma_exception.h => exc_handling.h
**  09-Aug-91 harrow       Add macro for referencing global exceptions
**  07-Aug-91 harrow       rpc_x_ss_report_error nolonger exists
**  07-Aug-91 harrow       Specially mark exceptions as globals
**  24-Jul-91 A.I.Hinxman  Add authentication exceptions
**  13-jun-91 labossiere   fix include syntax for krpc
**  13-Jun-91 A.I.Hinxman  Remove obsolete exceptions/error codes
**  04-Jun-91 A.I.Hinxman  Extended error code/exception list
**  16-May-91 mishkin      Tweak #includes
**  15-Feb-91 harrow       Add an exception to report a failing status
**                         and remove unused exception for too many nodes.
**  29-jan-91 nacey        Make inclusion of cma_exception conditional
**  17-Jan-91 A.I.Hinxman  rpc_s_asyntax_unsupported replaced by rpc_s_unknown_if
**  17-Dec-90 A.I.Hinxman  Include cma_exception.h instead of cma.h
**  05-Dec-90 A.I.Hinxman  Complete exception handling
**  29-nov-90   woodbury    change #include paths for IBM
**  26-Nov-90 A.I.Hinxman  call_cancelled and ASCII/EBCDIC translate file errors
**  14-Nov-90 A.I.Hinxman  Changes to CN error codes
**  05-Nov-90 A.I.Hinxman  Exceptions for DG errors
**  31-Oct-90 A.I.Hinxman  rpc_x_no_more_entries -> rpc_x_no_more_bindings
**  29-Oct-90 A.I.Hinxman  rpc_s_proto_error should have been rpc_s_protocol_error.
**  25-Oct-90 A.I.Hinxman  Exceptions for NS import routine errors
**  17-Oct-90 A.I.Hinxman  alert->cancel, add CN errors
**  10-Jul-90 A.I.Hinxman  [auto_handle], alerts
**  04-May-90 A.I.Hinxman  Original version.
**
**  %a%private_end
**
*/

#ifndef RPCEXC_H
#define RPCEXC_H 	1

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(VMS) || defined(__VMS)
#pragma nostandard
#if defined(__DECC) || defined(__cplusplus)
#pragma extern_model __save
#pragma extern_model __strict_refdef
#endif /* DEC C or C++ */
#endif /* VMS  */


#ifndef _KERNEL
#ifdef DOS_FILENAME_RESTRICTIONS 
#  include <dce/exc_hand.h>
#else
#  include <dce/exc_handling.h>
#endif /* MS-DOS */
#else
#  include <dce/ker/exc_handling.h>
#endif /* ! defined(_KERNEL)  */

#ifdef vaxc
#  define HAS_GLOBALDEF
#endif

#ifndef HAS_GLOBALDEF
#  if defined(WIN32) && !defined(DCE_INSIDE_DLL)
#    define RPC_EXTERN_EXCEPTION(name) extern EXCEPTION *name##_dll
#  else
#    define RPC_EXTERN_EXCEPTION extern EXCEPTION
#  endif
#else
#  define RPC_EXTERN_EXCEPTION globalref EXCEPTION
#endif /* HAS_GLOBALDEF */

/* DG and common errors */
RPC_EXTERN_EXCEPTION( rpc_x_assoc_grp_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_call_timeout );
RPC_EXTERN_EXCEPTION( rpc_x_cancel_timeout );
RPC_EXTERN_EXCEPTION( rpc_x_coding_error );
RPC_EXTERN_EXCEPTION( rpc_x_comm_failure );
RPC_EXTERN_EXCEPTION( rpc_x_context_id_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_endpoint_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_in_args_too_big );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_binding );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_bound );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_call_opt );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_naf_id );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_rpc_protseq );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_tag );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_timeout );
RPC_EXTERN_EXCEPTION( rpc_x_manager_not_entered );
RPC_EXTERN_EXCEPTION( rpc_x_max_descs_exceeded );
RPC_EXTERN_EXCEPTION( rpc_x_no_fault );
RPC_EXTERN_EXCEPTION( rpc_x_no_memory );
RPC_EXTERN_EXCEPTION( rpc_x_not_rpc_tower );
RPC_EXTERN_EXCEPTION( rpc_x_object_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_op_rng_error );
RPC_EXTERN_EXCEPTION( rpc_x_protocol_error );
RPC_EXTERN_EXCEPTION( rpc_x_protseq_not_supported );
RPC_EXTERN_EXCEPTION( rpc_x_rpcd_comm_failure );
RPC_EXTERN_EXCEPTION( rpc_x_server_too_busy );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_if );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_error );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_mgr_type );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_reject );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_remote_fault );
RPC_EXTERN_EXCEPTION( rpc_x_unsupported_type );
RPC_EXTERN_EXCEPTION( rpc_x_who_are_you_failed );
RPC_EXTERN_EXCEPTION( rpc_x_wrong_boot_time );
RPC_EXTERN_EXCEPTION( rpc_x_wrong_kind_of_binding );
RPC_EXTERN_EXCEPTION( uuid_x_getconf_failure );
RPC_EXTERN_EXCEPTION( uuid_x_internal_error );
RPC_EXTERN_EXCEPTION( uuid_x_no_address );
RPC_EXTERN_EXCEPTION( uuid_x_socket_failure );

/* CN errors */
RPC_EXTERN_EXCEPTION( rpc_x_access_control_info_inv );
RPC_EXTERN_EXCEPTION( rpc_x_assoc_grp_max_exceeded );
RPC_EXTERN_EXCEPTION( rpc_x_assoc_shutdown );
RPC_EXTERN_EXCEPTION( rpc_x_cannot_accept );
RPC_EXTERN_EXCEPTION( rpc_x_cannot_connect );
RPC_EXTERN_EXCEPTION( rpc_x_cant_inq_socket );
RPC_EXTERN_EXCEPTION( rpc_x_cant_create_socket );
RPC_EXTERN_EXCEPTION( rpc_x_connect_closed_by_rem );
RPC_EXTERN_EXCEPTION( rpc_x_connect_no_resources );
RPC_EXTERN_EXCEPTION( rpc_x_connect_rejected );
RPC_EXTERN_EXCEPTION( rpc_x_connect_timed_out );
RPC_EXTERN_EXCEPTION( rpc_x_connection_aborted );
RPC_EXTERN_EXCEPTION( rpc_x_connection_closed );
RPC_EXTERN_EXCEPTION( rpc_x_host_unreachable );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_endpoint_format );
RPC_EXTERN_EXCEPTION( rpc_x_loc_connect_aborted );
RPC_EXTERN_EXCEPTION( rpc_x_network_unreachable );
RPC_EXTERN_EXCEPTION( rpc_x_no_rem_endpoint );
RPC_EXTERN_EXCEPTION( rpc_x_rem_host_crashed );
RPC_EXTERN_EXCEPTION( rpc_x_rem_host_down );
RPC_EXTERN_EXCEPTION( rpc_x_rem_network_shutdown );
RPC_EXTERN_EXCEPTION( rpc_x_rpc_prot_version_mismatch );
RPC_EXTERN_EXCEPTION( rpc_x_string_too_long );
RPC_EXTERN_EXCEPTION( rpc_x_too_many_rem_connects );
RPC_EXTERN_EXCEPTION( rpc_x_tsyntaxes_unsupported );

/* NS import routine errors */
RPC_EXTERN_EXCEPTION( rpc_x_binding_vector_full );
RPC_EXTERN_EXCEPTION( rpc_x_entry_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_group_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_incomplete_name );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_arg );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_import_context );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_inquiry_context );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_inquiry_type );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_lookup_context );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_name_syntax );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_object );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_vers_option );
RPC_EXTERN_EXCEPTION( rpc_x_name_service_unavailable );
RPC_EXTERN_EXCEPTION( rpc_x_no_env_setup );
RPC_EXTERN_EXCEPTION( rpc_x_no_more_bindings );
RPC_EXTERN_EXCEPTION( rpc_x_no_more_elements );
RPC_EXTERN_EXCEPTION( rpc_x_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_not_rpc_entry );
RPC_EXTERN_EXCEPTION( rpc_x_obj_uuid_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_profile_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_unsupported_name_syntax );

/* Authentication errors */
RPC_EXTERN_EXCEPTION( rpc_x_auth_bad_integrity );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badaddr );
RPC_EXTERN_EXCEPTION( rpc_x_auth_baddirection );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badkeyver );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badmatch );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badorder );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badseq );
RPC_EXTERN_EXCEPTION( rpc_x_auth_badversion );
RPC_EXTERN_EXCEPTION( rpc_x_auth_field_toolong );
RPC_EXTERN_EXCEPTION( rpc_x_auth_inapp_cksum );
RPC_EXTERN_EXCEPTION( rpc_x_auth_method );
RPC_EXTERN_EXCEPTION( rpc_x_auth_msg_type );
RPC_EXTERN_EXCEPTION( rpc_x_auth_modified );
RPC_EXTERN_EXCEPTION( rpc_x_auth_mut_fail );
RPC_EXTERN_EXCEPTION( rpc_x_auth_nokey );
RPC_EXTERN_EXCEPTION( rpc_x_auth_not_us );
RPC_EXTERN_EXCEPTION( rpc_x_auth_repeat );
RPC_EXTERN_EXCEPTION( rpc_x_auth_skew );
RPC_EXTERN_EXCEPTION( rpc_x_auth_tkt_expired );
RPC_EXTERN_EXCEPTION( rpc_x_auth_tkt_nyv );
RPC_EXTERN_EXCEPTION( rpc_x_call_id_not_found );
RPC_EXTERN_EXCEPTION( rpc_x_credentials_too_large );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_checksum );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_crc );
RPC_EXTERN_EXCEPTION( rpc_x_invalid_credentials );
RPC_EXTERN_EXCEPTION( rpc_x_key_id_not_found );

/* Stub support errors */
RPC_EXTERN_EXCEPTION( rpc_x_ss_char_trans_open_fail );
RPC_EXTERN_EXCEPTION( rpc_x_ss_char_trans_short_file );
RPC_EXTERN_EXCEPTION( rpc_x_ss_context_damaged );
RPC_EXTERN_EXCEPTION( rpc_x_ss_context_mismatch );
RPC_EXTERN_EXCEPTION( rpc_x_ss_in_null_context );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_closed );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_comm_error );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_discipline_error );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_empty );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_memory );
RPC_EXTERN_EXCEPTION( rpc_x_ss_pipe_order );
RPC_EXTERN_EXCEPTION( rpc_x_ss_remote_comm_failure );
RPC_EXTERN_EXCEPTION( rpc_x_ss_remote_no_memory );
RPC_EXTERN_EXCEPTION( rpc_x_ss_bad_buffer );
RPC_EXTERN_EXCEPTION( rpc_x_ss_bad_es_action );
RPC_EXTERN_EXCEPTION( rpc_x_ss_wrong_es_version );
RPC_EXTERN_EXCEPTION( rpc_x_ss_incompatible_codesets );
RPC_EXTERN_EXCEPTION( rpc_x_stub_protocol_error );
RPC_EXTERN_EXCEPTION( rpc_x_unknown_stub_rtl_if_vers );
RPC_EXTERN_EXCEPTION( rpc_x_ss_codeset_conv_error );
RPC_EXTERN_EXCEPTION( rpc_x_no_client_stub );
RPC_EXTERN_EXCEPTION( rpc_x_no_server_stub );

#if defined(WIN32) && !defined(DCE_INSIDE_DLL)
/* DG and common errors */
#define rpc_x_assoc_grp_not_found (*rpc_x_assoc_grp_not_found_dll)
#define rpc_x_call_timeout (*rpc_x_call_timeout_dll) 
#define rpc_x_cancel_timeout (*rpc_x_cancel_timeout_dll)
#define rpc_x_coding_error (*rpc_x_coding_error_dll)
#define rpc_x_comm_failure (*rpc_x_comm_failure_dll)
#define rpc_x_context_id_not_found (*rpc_x_context_id_not_found_dll)
#define rpc_x_endpoint_not_found (*rpc_x_endpoint_not_found_dll)
#define rpc_x_in_args_too_big (*rpc_x_in_args_too_big_dll)
#define rpc_x_invalid_binding (*rpc_x_invalid_binding_dll)
#define rpc_x_invalid_bound (*rpc_x_invalid_bound_dll)
#define rpc_x_invalid_call_opt (*rpc_x_invalid_call_opt_dll)
#define rpc_x_invalid_naf_id (*rpc_x_invalid_naf_id_dll)
#define rpc_x_invalid_rpc_protseq (*rpc_x_invalid_rpc_protseq_dll)
#define rpc_x_invalid_tag (*rpc_x_invalid_tag_dll) 
#define rpc_x_invalid_timeout (*rpc_x_invalid_timeout_dll) 
#define rpc_x_manager_not_entered (*rpc_x_manager_not_entered_dll)
#define rpc_x_max_descs_exceeded (*rpc_x_max_descs_exceeded_dll)
#define rpc_x_no_fault (*rpc_x_no_fault_dll)
#define rpc_x_no_memory (*rpc_x_no_memory_dll) 
#define rpc_x_not_rpc_tower (*rpc_x_not_rpc_tower_dll)
#define rpc_x_object_not_found (*rpc_x_object_not_found_dll)
#define rpc_x_op_rng_error (*rpc_x_op_rng_error_dll)
#define rpc_x_protocol_error (*rpc_x_protocol_error_dll)
#define rpc_x_protseq_not_supported (*rpc_x_protseq_not_supported_dll)
#define rpc_x_rpcd_comm_failure (*rpc_x_rpcd_comm_failure_dll)
#define rpc_x_server_too_busy (*rpc_x_server_too_busy_dll) 
#define rpc_x_unknown_if (*rpc_x_unknown_if_dll)
#define rpc_x_unknown_error (*rpc_x_unknown_error_dll) 
#define rpc_x_unknown_mgr_type (*rpc_x_unknown_mgr_type_dll)
#define rpc_x_unknown_reject (*rpc_x_unknown_reject_dll)
#define rpc_x_unknown_remote_fault (*rpc_x_unknown_remote_fault_dll)
#define rpc_x_unsupported_type (*rpc_x_unsupported_type_dll)
#define rpc_x_who_are_you_failed (*rpc_x_who_are_you_failed_dll) 
#define rpc_x_wrong_boot_time (*rpc_x_wrong_boot_time_dll)
#define rpc_x_wrong_kind_of_binding (*rpc_x_wrong_kind_of_binding_dll)
#define uuid_x_getconf_failure (*uuid_x_getconf_failure_dll)
#define uuid_x_internal_error (*uuid_x_internal_error_dll) 
#define uuid_x_no_address (*uuid_x_no_address_dll) 
#define uuid_x_socket_failure (*uuid_x_socket_failure_dll)
/* CN errors */
#define rpc_x_access_control_info_inv (*rpc_x_access_control_info_inv_dll)  
#define rpc_x_assoc_grp_max_exceeded (*rpc_x_assoc_grp_max_exceeded_dll) 
#define rpc_x_assoc_shutdown (*rpc_x_assoc_shutdown_dll)
#define rpc_x_cannot_accept (*rpc_x_cannot_accept_dll)
#define rpc_x_cannot_connect (*rpc_x_cannot_connect_dll)  
#define rpc_x_cant_inq_socket (*rpc_x_cant_inq_socket_dll) 
#define rpc_x_cant_create_socket (*rpc_x_cant_create_socket_dll)
#define rpc_x_connect_closed_by_rem (*rpc_x_connect_closed_by_rem_dll)
#define rpc_x_connect_no_resources (*rpc_x_connect_no_resources_dll)
#define rpc_x_connect_rejected (*rpc_x_connect_rejected_dll)
#define rpc_x_connect_timed_out (*rpc_x_connect_timed_out_dll)
#define rpc_x_connection_aborted (*rpc_x_connection_aborted_dll)
#define rpc_x_connection_closed (*rpc_x_connection_closed_dll)
#define rpc_x_host_unreachable (*rpc_x_host_unreachable_dll)
#define rpc_x_invalid_endpoint_format (*rpc_x_invalid_endpoint_format_dll)
#define rpc_x_loc_connect_aborted (*rpc_x_loc_connect_aborted_dll)
#define rpc_x_network_unreachable (*rpc_x_network_unreachable_dll)
#define rpc_x_no_rem_endpoint (*rpc_x_no_rem_endpoint_dll)
#define rpc_x_rem_host_crashed (*rpc_x_rem_host_crashed_dll)
#define rpc_x_rem_host_down (*rpc_x_rem_host_down_dll)
#define rpc_x_rem_network_shutdown (*rpc_x_rem_network_shutdown_dll)
#define rpc_x_rpc_prot_version_mismatch (*rpc_x_rpc_prot_version_mismatch_dll)
#define rpc_x_string_too_long (*rpc_x_string_too_long_dll)
#define rpc_x_too_many_rem_connects (*rpc_x_too_many_rem_connects_dll)
#define rpc_x_tsyntaxes_unsupported (*rpc_x_tsyntaxes_unsupported_dll)
/* NS import routine errors */
#define rpc_x_binding_vector_full (*rpc_x_binding_vector_full_dll)
#define rpc_x_entry_not_found (*rpc_x_entry_not_found_dll)
#define rpc_x_group_not_found (*rpc_x_group_not_found_dll)
#define rpc_x_incomplete_name (*rpc_x_incomplete_name_dll)
#define rpc_x_invalid_arg (*rpc_x_invalid_arg_dll)
#define rpc_x_invalid_import_context (*rpc_x_invalid_import_context_dll)
#define rpc_x_invalid_inquiry_context (*rpc_x_invalid_inquiry_context_dll)
#define rpc_x_invalid_inquiry_type (*rpc_x_invalid_inquiry_type_dll)
#define rpc_x_invalid_lookup_context (*rpc_x_invalid_lookup_context_dll)
#define rpc_x_invalid_name_syntax (*rpc_x_invalid_name_syntax_dll)
#define rpc_x_invalid_object (*rpc_x_invalid_object_dll)
#define rpc_x_invalid_vers_option (*rpc_x_invalid_vers_option_dll)
#define rpc_x_name_service_unavailable (*rpc_x_name_service_unavailable_dll)
#define rpc_x_no_env_setup (*rpc_x_no_env_setup_dll)
#define rpc_x_no_more_bindings (*rpc_x_no_more_bindings_dll)
#define rpc_x_no_more_elements (*rpc_x_no_more_elements_dll)
#define rpc_x_not_found (*rpc_x_not_found_dll)
#define rpc_x_not_rpc_entry (*rpc_x_not_rpc_entry_dll)
#define rpc_x_obj_uuid_not_found (*rpc_x_obj_uuid_not_found_dll)
#define rpc_x_profile_not_found (*rpc_x_profile_not_found_dll)
#define rpc_x_unsupported_name_syntax (*rpc_x_unsupported_name_syntax_dll)
/* Authentication errors */
#define rpc_x_auth_bad_integrity (*rpc_x_auth_bad_integrity_dll)
#define rpc_x_auth_badaddr (*rpc_x_auth_badaddr_dll)
#define rpc_x_auth_baddirection (*rpc_x_auth_baddirection_dll)
#define rpc_x_auth_badkeyver (*rpc_x_auth_badkeyver_dll)
#define rpc_x_auth_badmatch (*rpc_x_auth_badmatch_dll)
#define rpc_x_auth_badorder (*rpc_x_auth_badorder_dll)
#define rpc_x_auth_badseq (*rpc_x_auth_badseq_dll)
#define rpc_x_auth_badversion (*rpc_x_auth_badversion_dll)
#define rpc_x_auth_field_toolong (*rpc_x_auth_field_toolong_dll)
#define rpc_x_auth_inapp_cksum (*rpc_x_auth_inapp_cksum_dll)
#define rpc_x_auth_method (*rpc_x_auth_method_dll)
#define rpc_x_auth_msg_type (*rpc_x_auth_msg_type_dll)
#define rpc_x_auth_modified (*rpc_x_auth_modified_dll)
#define rpc_x_auth_mut_fail (*rpc_x_auth_mut_fail_dll)
#define rpc_x_auth_nokey (*rpc_x_auth_nokey_dll)
#define rpc_x_auth_not_us (*rpc_x_auth_not_us_dll)
#define rpc_x_auth_repeat (*rpc_x_auth_repeat_dll)
#define rpc_x_auth_skew (*rpc_x_auth_skew_dll)
#define rpc_x_auth_tkt_expired (*rpc_x_auth_tkt_expired_dll)
#define rpc_x_auth_tkt_nyv (*rpc_x_auth_tkt_nyv_dll)
#define rpc_x_call_id_not_found (*rpc_x_call_id_not_found_dll)
#define rpc_x_credentials_too_large (*rpc_x_credentials_too_large_dll)
#define rpc_x_invalid_checksum (*rpc_x_invalid_checksum_dll)
#define rpc_x_invalid_crc (*rpc_x_invalid_crc_dll)
#define rpc_x_invalid_credentials (*rpc_x_invalid_credentials_dll)
#define rpc_x_key_id_not_found (*rpc_x_key_id_not_found_dll)
/* Stub support errors */
#define rpc_x_ss_char_trans_open_fail (*rpc_x_ss_char_trans_open_fail_dll)
#define rpc_x_ss_char_trans_short_file (*rpc_x_ss_char_trans_short_file_dll)
#define rpc_x_ss_context_damaged (*rpc_x_ss_context_damaged_dll)
#define rpc_x_ss_context_mismatch (*rpc_x_ss_context_mismatch_dll)
#define rpc_x_ss_in_null_context (*rpc_x_ss_in_null_context_dll)
#define rpc_x_ss_pipe_closed (*rpc_x_ss_pipe_closed_dll)
#define rpc_x_ss_pipe_comm_error (*rpc_x_ss_pipe_comm_error_dll)
#define rpc_x_ss_pipe_discipline_error (*rpc_x_ss_pipe_discipline_error_dll)
#define rpc_x_ss_pipe_empty (*rpc_x_ss_pipe_empty_dll)
#define rpc_x_ss_pipe_memory (*rpc_x_ss_pipe_memory_dll)
#define rpc_x_ss_pipe_order (*rpc_x_ss_pipe_order_dll)
#define rpc_x_ss_remote_comm_failure (*rpc_x_ss_remote_comm_failure_dll)
#define rpc_x_ss_remote_no_memory (*rpc_x_ss_remote_no_memory_dll)
#define rpc_x_ss_bad_buffer (*rpc_x_ss_bad_buffer_dll)
#define rpc_x_ss_bad_es_action (*rpc_x_ss_bad_es_action_dll)
#define rpc_x_ss_wrong_es_version (*rpc_x_ss_wrong_es_version_dll)
#define rpc_x_ss_incompatible_codesets (*rpc_x_ss_incompatible_codesets_dll)
#define rpc_x_unknown_stub_rtl_if_vers (*rpc_x_unknown_stub_rtl_if_vers_dll)
#define rpc_x_tx_not_in_transaction (*rpc_x_tx_not_in_transaction_dll)
#define rpc_x_tx_open_failed  (*rpc_x_tx_open_failed_dll)
#define rpc_x_ss_codeset_conv_error (*rpc_x_ss_codeset_conv_error_dll)
#define rpc_x_no_client_stub (*rpc_x_no_client_stub_dll)
#define rpc_x_no_server_stub (*rpc_x_no_server_stub_dll)
#define rpc_x_stub_protocol_error (*rpc_x_stub_protocol_error_dll)
#endif

#if defined(VMS) || defined(__VMS)
#if defined(__DECC) || defined(__cplusplus)
#pragma extern_model __restore
#endif /* DEC C or C++ */

#pragma standard

#endif /* VMS */

#ifdef __cplusplus
}
#endif

#endif /* _RPCEXC_H */
