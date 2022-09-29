/*
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

#include <sgidefs.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/acl.h>


#define SAT_TITLE_SIZE	8

struct ifreq;

#ifdef	_KERNEL
extern void sat_append_token(sat_token_t, void *, int);
#else	/* _KERNEL */
extern sat_token_t sat_append_token(sat_token_t, void *, int);
#endif	/* _KERNEL */
extern void sat_free_token(sat_token_t);

extern sat_token_t sat_new_token(int);
extern sat_token_t sat_new_errno_token(uint8_t);
extern sat_token_t sat_new_audit_id_token(uid_t);
extern sat_token_t sat_new_pid_token(int);
extern sat_token_t sat_new_binary_token(int8_t);
extern sat_token_t sat_new_hostid_token(uint32_t);
extern sat_token_t sat_new_port_token(int32_t);
extern sat_token_t sat_new_mode_token(mode_t);
extern sat_token_t sat_new_svipc_id_token(int32_t);
extern sat_token_t sat_new_svipc_key_token(int32_t);
extern sat_token_t sat_new_text_token(char *);
extern sat_token_t sat_new_cap_set_token(cap_t);
extern sat_token_t sat_new_cap_value_token(cap_value_t);
extern sat_token_t sat_new_acl_token(acl_t);
extern sat_token_t sat_new_mac_label_token(mac_t);
extern sat_token_t sat_new_socket_token(sat_socket_id_t);
extern sat_token_t sat_new_ifname_token(sat_ifname_t);
extern sat_token_t sat_new_descriptor_list_token(sat_descriptor_t *, int);
extern sat_token_t sat_new_sysarg_list_token(sat_sysarg_t *, int);
extern sat_token_t sat_new_uid_list_token(uid_t *, int);
extern sat_token_t sat_new_gid_list_token(gid_t *, int);
extern sat_token_t sat_new_ip_header_token(char *, int);
extern sat_token_t sat_new_sockadder_token(char *, int);
extern sat_token_t sat_new_file_token(ino64_t, dev_t);
extern sat_token_t sat_new_ugid_token(uid_t, gid_t);
extern sat_token_t sat_new_syscall_token(uint8_t, uint16_t);
extern sat_token_t sat_new_time_token(time_t, uint8_t);
extern sat_token_t sat_new_protocol_token(uint16_t, uint16_t);
extern sat_token_t sat_new_ifreq_token(struct ifreq *);
extern sat_token_t sat_new_record_header_token(int32_t, sat_token_size_t,
						int8_t, int8_t, int8_t,int8_t);
extern sat_token_t sat_new_satid_token(uid_t);
extern sat_token_t sat_new_openmode_token(uint32_t);
extern sat_token_t sat_new_signal_token(uint32_t);
extern sat_token_t sat_new_status_token(uint32_t);
extern sat_token_t sat_new_device_token(dev_t);
extern sat_token_t sat_new_titled_text_token(char *, char *);
extern sat_token_t sat_new_pathname_token(char *, sat_token_size_t);
extern sat_token_t sat_new_lookup_token(char *, sat_token_size_t);
extern sat_token_t sat_new_opaque_token(char *, sat_token_size_t);
extern sat_token_t sat_new_privilege_token(cap_value_t, uint8_t);
extern sat_token_t sat_read_token(int);
extern sat_token_t sat_fread_token(FILE *);

extern int sat_fetch_token(sat_token_t, void *, int, int);
extern void sat_fetch_one_token(sat_token_t, void *, int);

extern char *sat_errno_token_to_text(sat_token_t);
extern char *sat_audit_id_token_to_text(sat_token_t);
extern char *sat_pid_token_to_text(sat_token_t);
extern char *sat_binary_token_to_text(sat_token_t);
extern char *sat_hostid_token_to_text(sat_token_t);
extern char *sat_port_token_to_text(sat_token_t);
extern char *sat_mode_token_to_text(sat_token_t);
extern char *sat_svipc_id_token_to_text(sat_token_t);
extern char *sat_svipc_key_token_to_text(sat_token_t);
extern char *sat_text_token_to_text(sat_token_t);
extern char *sat_cap_set_token_to_text(sat_token_t);
extern char *sat_cap_value_token_to_text(sat_token_t);
extern char *sat_acl_token_to_text(sat_token_t);
extern char *sat_mac_label_token_to_text(sat_token_t);
extern char *sat_socket_token_to_text(sat_token_t);
extern char *sat_ifname_token_to_text(sat_token_t);
extern char *sat_descriptor_list_token_to_text(sat_token_t);
extern char *sat_sysarg_list_token_to_text(sat_token_t);
extern char *sat_uid_list_token_to_text(sat_token_t);
extern char *sat_gid_list_token_to_text(sat_token_t);
extern char *sat_ip_header_token_to_text(sat_token_t);
extern char *sat_sockadder_token_to_text(sat_token_t);
extern char *sat_unknown_token_to_text(sat_token_t);
extern char *sat_file_token_to_text(sat_token_t);
extern char *sat_ugid_token_to_text(sat_token_t);
extern char *sat_syscall_token_to_text(sat_token_t);
extern char *sat_time_token_to_text(sat_token_t);
extern char *sat_protocol_token_to_text(sat_token_t);
extern char *sat_ifreq_token_to_text(sat_token_t);
extern char *sat_record_header_token_to_text(sat_token_t);
extern char *sat_satid_token_to_text(sat_token_t);
extern char *sat_device_token_to_text(sat_token_t);
extern char *sat_titled_text_token_to_text(sat_token_t);
extern char *sat_pathname_token_to_text(sat_token_t);
extern char *sat_lookup_token_to_text(sat_token_t);
extern char *sat_openmode_token_to_text(sat_token_t);
extern char *sat_signal_token_to_text(sat_token_t);
extern char *sat_status_token_to_text(sat_token_t);
extern char *sat_opaque_token_to_text(sat_token_t);
extern char *sat_cwd_token_to_text(sat_token_t);
extern char *sat_root_token_to_text(sat_token_t);
extern char *sat_privilege_token_to_text(sat_token_t);

extern char *sat_token_to_text(sat_token_t);
extern char *sat_token_name(sat_token_id_t);

extern void sat_fetch_errno_token(sat_token_t, uint8_t *);
extern void sat_fetch_pid_token(sat_token_t, pid_t *);
extern void sat_fetch_parent_pid_token(sat_token_t, pid_t *);
extern void sat_fetch_binary_token(sat_token_t, int8_t *);
extern void sat_fetch_hostid_token(sat_token_t, uint32_t *);
extern void sat_fetch_port_token(sat_token_t, int32_t *);
extern void sat_fetch_mode_token(sat_token_t, mode_t *);
extern void sat_fetch_svipc_id_token(sat_token_t, int32_t *);
extern void sat_fetch_svipc_key_token(sat_token_t, int32_t *);
extern void sat_fetch_titled_text_token(sat_token_t, char **, char **);
extern void sat_fetch_cap_set_token(sat_token_t, cap_t *);
extern void sat_fetch_cap_value_token(sat_token_t, cap_value_t *);
extern void sat_fetch_privilege_token(sat_token_t, cap_value_t *, uint8_t *);
extern void sat_fetch_acl_token(sat_token_t, struct acl *);
extern void sat_fetch_mac_label_token(sat_token_t, mac_t *);
extern void sat_fetch_socket_token(sat_token_t, sat_socket_id_t *);
extern void sat_fetch_ifname_token(sat_token_t, sat_ifname_t *);
extern void sat_fetch_descriptor_list_token(sat_token_t, sat_descriptor_t **);
extern void sat_fetch_sysarg_list_token(sat_token_t, sat_sysarg_t **);
extern void sat_fetch_uid_list_token(sat_token_t, uid_t **);
extern void sat_fetch_gid_list_token(sat_token_t, gid_t **);
extern void sat_fetch_ip_header_token(sat_token_t, char **);
extern void sat_fetch_sockadder_token(sat_token_t, char **);
extern void sat_fetch_unknown_token(sat_token_t, char **);
extern void sat_fetch_file_token(sat_token_t, ino64_t *, dev_t *);
extern void sat_fetch_device_token(sat_token_t, dev_t *);
extern void sat_fetch_ugid_token(sat_token_t, uid_t *, gid_t *);
extern void sat_fetch_openmode_token(sat_token_t, uint32_t *);
extern void sat_fetch_signal_token(sat_token_t, uint32_t *);
extern void sat_fetch_status_token(sat_token_t, uint32_t *);
extern void sat_fetch_satid_token(sat_token_t, uid_t *);
extern void sat_fetch_syscall_token(sat_token_t, uint8_t *, uint16_t *);
extern void sat_fetch_time_token(sat_token_t, time_t *, uint8_t *);
extern void sat_fetch_protocol_token(sat_token_t, uint16_t *, uint16_t *);
extern void sat_fetch_ifreq_token(sat_token_t, char **);
extern void sat_fetch_opaque_token(sat_token_t, char **);
extern void sat_fetch_text_token(sat_token_t, char **);
extern void sat_fetch_command_token(sat_token_t, char **);
extern void sat_fetch_pathname_token(sat_token_t, char **);
extern void sat_fetch_lookup_token(sat_token_t, char **);
extern void sat_fetch_cwd_token(sat_token_t, char **);
extern void sat_fetch_root_token(sat_token_t, char **);
extern void sat_fetch_record_header_token(sat_token_t, int32_t *,
		sat_token_size_t *, int8_t *, int8_t *, int8_t *, int8_t *);
