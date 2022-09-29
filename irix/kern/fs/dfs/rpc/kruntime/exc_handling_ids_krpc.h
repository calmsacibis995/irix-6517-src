/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: exc_handling_ids_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:26  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.102.2  1996/02/18  23:46:39  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:20  marty]
 *
 * Revision 1.1.102.1  1995/12/08  00:15:07  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:09 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  23:56:19  root]
 * 
 * Revision 1.1.99.2  1994/02/22  22:47:59  cbrooks
 * 	CR9987
 * 	[1994/02/22  22:04:00  cbrooks]
 * 
 * Revision 1.1.99.1  1994/01/21  22:32:03  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:24  cbrooks]
 * 
 * Revision 1.1.8.4  1993/07/14  21:54:56  pwang
 * 	Bug OT #8246, #8270, Added definitions for the new error codes
 * 	introduced by the new idl compiler
 * 	[1993/07/14  21:44:55  pwang]
 * 
 * Revision 1.1.8.2  1993/07/12  22:55:54  pwang
 * 	Replaced the definition "#define rpc_x_no_ns_privilege_id
 * 	rpc_s_no_ns_privilege" with "#define rpc_x_no_ns_permission_id
 * 	rpc_s_no_ns_permission" to match the new idl compiler
 * 	[1993/07/12  22:54:38  pwang]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:22  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:38  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:39:13  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:21  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:55:55  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:50:43  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:16:42  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**  NAME:
**
**      exc_ids.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**    Exception definitions for the DCE exceptions (see osc/exc.h)
**
*/

#ifndef _EXC_HANDLING_IDS_KRPC_H
#define _EXC_HANDLING_IDS_KRPC_H	1

/* DON'T USE (not a legal arg to longjmp)       0 */

/*
 * Exception package defined exceptions
 */

#define exc_e_alerted_id                        1

#define unused_exception                        0xffff0000U

/*
 * Mappings for things in sys_idl/rpcsts.h
 * Since this is an exported file, these defines remain in lower case
 */

/* DG and common errors */

#define rpc_x_assoc_grp_not_found_id            rpc_s_assoc_grp_not_found
#define rpc_x_call_failed_id                    rpc_s_call_failed
#define rpc_x_call_timeout_id			rpc_s_call_timeout
#define rpc_x_cancel_timeout_id                 rpc_s_cancel_timeout
#define rpc_x_coding_error_id                   rpc_s_coding_error
#define rpc_x_comm_failure_id                   rpc_s_comm_failure
#define rpc_x_context_id_not_found_id           rpc_s_context_id_not_found
#define rpc_x_endpoint_not_found_id             rpc_s_endpoint_not_found
#define rpc_x_in_args_too_big_id                rpc_s_in_args_too_big
#define rpc_x_invalid_binding_id                rpc_s_invalid_binding
#define rpc_x_invalid_bound_id                  rpc_s_fault_invalid_bound
#define rpc_x_invalid_call_opt_id               rpc_s_invalid_call_opt
#define rpc_x_invalid_naf_id_id                 rpc_s_invalid_naf_id
#define rpc_x_invalid_rpc_protseq_id            rpc_s_invalid_rpc_protseq
#define rpc_x_invalid_tag_id                    rpc_s_fault_invalid_tag
#define rpc_x_invalid_timeout_id                rpc_s_invalid_timeout
#define rpc_x_manager_not_entered_id            rpc_s_manager_not_entered
#define rpc_x_max_descs_exceeded_id             rpc_s_max_descs_exceeded
#define rpc_x_no_fault_id                       rpc_s_no_fault
#define rpc_x_no_memory_id                      rpc_s_no_memory
#define rpc_x_not_rpc_tower_id                  rpc_s_not_rpc_tower
#define rpc_x_object_not_found_id               rpc_s_object_not_found
#define rpc_x_op_rng_error_id                   rpc_s_op_rng_error
#define rpc_x_protocol_error_id                 rpc_s_protocol_error
#define rpc_x_protseq_not_supported_id          rpc_s_protseq_not_supported
#define rpc_x_rpcd_comm_failure_id              rpc_s_rpcd_comm_failure
#define rpc_x_server_too_busy_id                rpc_s_server_too_busy
#define rpc_x_unknown_if_id                     rpc_s_unknown_if
#define rpc_x_unknown_error_id                  rpc_s_fault_unspec
#define rpc_x_unknown_mgr_type_id               rpc_s_unknown_mgr_type
#define rpc_x_unknown_reject_id                 rpc_s_unknown_reject
#define rpc_x_unknown_remote_fault_id           rpc_s_fault_unspec
#define rpc_x_unsupported_type_id               rpc_s_unsupported_type
#define rpc_x_who_are_you_failed_id             rpc_s_who_are_you_failed
#define rpc_x_wrong_boot_time_id                rpc_s_wrong_boot_time
#define rpc_x_wrong_kind_of_binding_id          rpc_s_wrong_kind_of_binding
#define uuid_x_getconf_failure_id               uuid_s_getconf_failure
#define uuid_x_internal_error_id                uuid_s_internal_error
#define uuid_x_no_address_id                    uuid_s_no_address
#define uuid_x_socket_failure_id                uuid_s_socket_failure
#define rpc_x_cannot_set_nodelay_id             rpc_s_cannot_set_nodelay

/* knck shouldn't need these, but libnidl defines them :-( */
/* CN errors */

#define rpc_x_access_control_info_inv_id        rpc_s_access_control_info_inv
#define rpc_x_assoc_grp_max_exceeded_id         rpc_s_assoc_grp_max_exceeded
#define rpc_x_assoc_req_rejected_id             rpc_s_assoc_req_rejected
#define rpc_x_assoc_shutdown_id                 rpc_s_assoc_shutdown
#define rpc_x_cannot_accept_id                  rpc_s_cannot_accept
#define rpc_x_cannot_connect_id                 rpc_s_cannot_connect
#define rpc_x_cant_bind_sock_id                 rpc_s_cant_bind_sock
#define rpc_x_cant_create_sock_id               rpc_s_cant_create_sock
#define rpc_x_cant_create_socket_id		rpc_s_cant_create_socket
#define rpc_x_cant_inq_socket_id                rpc_s_cant_inq_socket
#define rpc_x_cant_listen_sock_id               rpc_s_cant_listen_sock
#define rpc_x_connect_closed_by_rem_id          rpc_s_connect_closed_by_rem
#define rpc_x_connect_no_resources_id           rpc_s_connect_no_resources
#define rpc_x_connect_rejected_id               rpc_s_connect_rejected
#define rpc_x_connect_timed_out_id              rpc_s_connect_timed_out
#define rpc_x_connection_aborted_id             rpc_s_connection_aborted
#define rpc_x_connection_closed_id              rpc_s_connection_closed
#define rpc_x_host_unreachable_id               rpc_s_host_unreachable
#define rpc_x_invalid_endpoint_format_id        rpc_s_invalid_endpoint_format
#define rpc_x_loc_connect_aborted_id            rpc_s_loc_connect_aborted
#define rpc_x_network_unreachable_id            rpc_s_network_unreachable
#define rpc_x_no_rem_endpoint_id                rpc_s_no_rem_endpoint
#define rpc_x_rem_host_crashed_id               rpc_s_rem_host_crashed
#define rpc_x_rem_host_down_id                  rpc_s_rem_host_down
#define rpc_x_rem_network_shutdown_id           rpc_s_rem_network_shutdown
#define rpc_x_rpc_prot_version_mismatch_id      rpc_s_rpc_prot_version_mismatch
#define rpc_x_string_too_long_id                rpc_s_string_too_long
#define rpc_x_too_many_rem_connects_id          rpc_s_too_many_rem_connects
#define rpc_x_tsyntaxes_unsupported_id          rpc_s_tsyntaxes_unsupported
                                                
/* knck shouldn't need these, but libnidl defines them :-( */
/* NS import routine errors */

#define rpc_x_binding_vector_full_id            rpc_s_binding_vector_full
#define rpc_x_cycle_detected_id                 rpc_s_cycle_detected
#define rpc_x_database_busy_id                  rpc_s_database_busy
#define rpc_x_entry_already_exists_id           rpc_s_entry_already_exists
#define rpc_x_entry_not_found_id                rpc_s_entry_not_found
#define rpc_x_group_not_found_id                rpc_s_group_not_found
#define rpc_x_incomplete_name_id                rpc_s_incomplete_name
#define rpc_x_invalid_arg_id                    rpc_s_invalid_arg
#define rpc_x_invalid_import_context_id         rpc_s_invalid_import_context
#define rpc_x_invalid_inquiry_context_id        rpc_s_invalid_inquiry_context
#define rpc_x_invalid_inquiry_type_id           rpc_s_invalid_inquiry_type
#define rpc_x_invalid_lookup_context_id         rpc_s_invalid_lookup_context
#define rpc_x_invalid_name_syntax_id            rpc_s_invalid_name_syntax
#define rpc_x_invalid_object_id                 rpc_s_invalid_object
#define rpc_x_invalid_string_binding_id         rpc_s_invalid_string_binding
#define rpc_x_invalid_vers_option_id            rpc_s_invalid_vers_option
#define rpc_x_name_service_unavailable_id       rpc_s_name_service_unavailable
#define rpc_x_no_env_setup_id                   rpc_s_no_env_setup
#define rpc_x_no_more_bindings_id               rpc_s_no_more_bindings
#define rpc_x_no_more_elements_id               rpc_s_no_more_elements
#define rpc_x_no_more_members_id                rpc_s_no_more_members
#define rpc_x_no_ns_permission_id               rpc_s_no_ns_permission
#define rpc_x_not_found_id                      rpc_s_not_found
#define rpc_x_not_rpc_entry_id                  rpc_s_not_rpc_entry
#define rpc_x_obj_uuid_not_found_id             rpc_s_obj_uuid_not_found
#define rpc_x_profile_not_found_id              rpc_s_profile_not_found
#define rpc_x_underspecified_name_id            rpc_s_underspecified_name
#define rpc_x_unknown_ns_error_id               rpc_s_unknown_ns_error
#define rpc_x_unsupported_name_syntax_id        rpc_s_unsupported_name_syntax
#define uuid_x_invalid_string_uuid_id           uuid_s_invalid_string_uuid
#define twr_x_not_implemented_id                twr_s_not_implemented
#define twr_x_unknown_tower_id                  twr_s_unknown_tower


/* Stub support errors */
/* for exceptions that don't have corresponding rpc_s_ status values */

#define rpc_ss_exception_id                     0xfffe0000U

#define rpc_x_ss_char_trans_open_fail_id        (01 + rpc_ss_exception_id) /* ??? rpc_s_fault_context_damaged */
#define rpc_x_ss_char_trans_short_file_id       (02 + rpc_ss_exception_id) /* ??? rpc_s_fault_in_null_context */
#define rpc_x_ss_context_damaged_id             (03 + rpc_ss_exception_id) /* ??? rpc_s_fault_context_damaged */
#define rpc_x_ss_in_null_context_id             (04 + rpc_ss_exception_id) /* ??? rpc_s_fault_in_null_context */
#define rpc_x_ss_report_status_id               (06 + rpc_ss_exception_id) /* ??? rpc_x_ss_report_status */
#define unknown_status_exception_id             (07 + rpc_ss_exception_id) /* ??? unknown_status_exception */
#define rpc_x_ss_pipe_empty_id                  rpc_s_fault_pipe_empty
#define rpc_x_ss_pipe_closed_id                 rpc_s_fault_pipe_closed
#define rpc_x_ss_pipe_order_id                  rpc_s_fault_pipe_order
#define rpc_x_ss_pipe_discipline_error_id       rpc_s_fault_pipe_discipline
#define rpc_x_ss_pipe_comm_error_id             rpc_s_fault_pipe_comm_error
#define rpc_x_ss_pipe_memory_id                 rpc_s_fault_pipe_memory
#define rpc_x_ss_context_mismatch_id            rpc_s_fault_context_mismatch
#define rpc_x_ss_remote_comm_failure_id         rpc_s_fault_remote_comm_failure
#define rpc_x_ss_remote_no_memory_id            rpc_s_fault_remote_no_memory

/*
 * interface's stub/runtime version is unknown
 */
#define	rpc_x_unknown_stub_rtl_if_vers_id	rpc_s_unknown_stub_rtl_if_vers

#define rpc_x_auth_bad_integrity_id		rpc_s_auth_bad_integrity
#define rpc_x_auth_badaddr_id			rpc_s_auth_badaddr
#define rpc_x_auth_baddirection_id		rpc_s_auth_baddirection
#define rpc_x_auth_badkeyver_id 		rpc_s_auth_badkeyver
#define rpc_x_auth_badmatch_id	 		rpc_s_auth_badmatch
#define rpc_x_auth_badorder_id	 		rpc_s_auth_badorder
#define rpc_x_auth_badseq_id	 		rpc_s_auth_badseq
#define rpc_x_auth_badversion_id 		rpc_s_auth_badversion
#define rpc_x_auth_field_toolong_id 		rpc_s_auth_field_toolong
#define rpc_x_auth_inapp_cksum_id		rpc_s_auth_inapp_cksum
#define rpc_x_auth_method_id			rpc_s_auth_method
#define rpc_x_auth_msg_type_id			rpc_s_auth_msg_type
#define rpc_x_auth_modified_id			rpc_s_auth_modified
#define rpc_x_auth_mut_fail_id			rpc_s_auth_mut_fail
#define rpc_x_auth_nokey_id			rpc_s_auth_nokey
#define rpc_x_auth_not_us_id			rpc_s_auth_not_us
#define rpc_x_auth_repeat_id			rpc_s_auth_repeat
#define rpc_x_auth_skew_id			rpc_s_auth_skew
#define rpc_x_auth_tkt_expired_id		rpc_s_auth_tkt_expired
#define rpc_x_auth_tkt_nyv_id			rpc_s_auth_tkt_nyv
#define rpc_x_call_id_not_found_id		rpc_s_call_id_not_found
#define rpc_x_credentials_too_large_id		rpc_s_credentials_too_large
#define rpc_x_invalid_checksum_id		rpc_s_invalid_checksum
#define rpc_x_invalid_crc_id			rpc_s_invalid_crc
#define rpc_x_invalid_credentials_id		rpc_s_invalid_credentials
#define rpc_x_key_id_not_found_id		rpc_s_key_id_not_found
#define rpc_x_stub_protocol_error_id  		rpc_s_stub_protocol_error

/*
 * IDL Encoding Services related errors
 */

#define rpc_x_ss_bad_buffer_id			rpc_s_ss_bad_buffer
#define	rpc_x_ss_bad_es_action_id		rpc_s_ss_bad_es_action
#define	rpc_x_ss_wrong_es_version_id		rpc_s_ss_wrong_es_version

/*
 * Extended IDL exceptions
 */

#define rpc_x_no_client_stub_id			rpc_s_fault_no_client_stub
#define rpc_x_no_server_stub_id			rpc_s_no_server_stub

/*
 * I18n related errors
 */

#define rpc_x_ss_incompatible_codesets_id	rpc_s_ss_incompatible_codesets
#define rpc_x_ss_codeset_conv_error_id		rpc_s_ss_codeset_conv_error

#endif /* _EXC_HANDLING_IDS_KRPC_H */
