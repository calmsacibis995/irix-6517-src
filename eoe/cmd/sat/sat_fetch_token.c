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

#include <sys/sat.h>
#include <stdlib.h>
#include <net/if.h>
#include "sat_token.h"

static void
fetch_data_token(sat_token_t token, char **data)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);

	*data = malloc(data_size);
	sat_fetch_one_token(token, *data, data_size);
}

static void
fetch_int32_token(sat_token_t token, int32_t *ip)
{
	sat_fetch_one_token(token, ip, sizeof(*ip));
}

void
sat_fetch_errno_token(sat_token_t token, uint8_t *error_number_p)
{
	sat_fetch_one_token(token, error_number_p, sizeof(*error_number_p));
}

void
sat_fetch_pid_token(sat_token_t token, pid_t *pp)
{
	fetch_int32_token(token, (int32_t *)pp);
}

void
sat_fetch_parent_pid_token(sat_token_t token, pid_t *pp)
{
	fetch_int32_token(token, (int32_t *)pp);
}

void
sat_fetch_binary_token(sat_token_t token, int8_t *bit_p)
{
	sat_fetch_one_token(token, bit_p, sizeof(*bit_p));
}

void
sat_fetch_hostid_token(sat_token_t token, uint32_t *hid_p)
{
	fetch_int32_token(token, (int32_t *)hid_p);
}

void
sat_fetch_port_token(sat_token_t token, int32_t *port_p)
{
	fetch_int32_token(token, (int32_t *)port_p);
}

void
sat_fetch_mode_token(sat_token_t token, mode_t *mode_p)
{
	sat_fetch_one_token(token, mode_p, sizeof(*mode_p));
}

void
sat_fetch_svipc_id_token(sat_token_t token, int32_t *svipc_id_p)
{
	fetch_int32_token(token, svipc_id_p);
}

void
sat_fetch_svipc_key_token(sat_token_t token, int32_t *svipc_key_p)
{
	fetch_int32_token(token, svipc_key_p);
}

void
sat_fetch_titled_text_token(sat_token_t token, char **title_p, char **text_p)
{
	int text_size;
	int cursor;
	char *title = malloc(SAT_TITLE_SIZE + 2);
	char *text;

	text_size = SAT_TOKEN_DATA_SIZE(token) - SAT_TITLE_SIZE;
	text = malloc(text_size);

	cursor = sat_fetch_token(token, title, SAT_TITLE_SIZE, 0);
	cursor = sat_fetch_token(token, text, text_size, cursor);

	title[SAT_TITLE_SIZE] = '\0';
	
	*title_p = title;
	*text_p = text;
}

void
sat_fetch_cap_set_token(sat_token_t token, cap_t *cap_p)
{
	fetch_data_token(token, (char **)cap_p);
}

void
sat_fetch_cap_value_token(sat_token_t token, cap_value_t *cvp)
{
	sat_fetch_one_token(token, cvp, sizeof(*cvp));
}

void
sat_fetch_privilege_token(sat_token_t token, cap_value_t *cvp, uint8_t *hp)
{
	int cursor;

	cursor = sat_fetch_token(token, cvp, sizeof(*cvp), 0);
	cursor = sat_fetch_token(token, hp, sizeof(*hp), cursor);
}

void
sat_fetch_acl_token(sat_token_t token, struct acl *acl_p)
{
	sat_fetch_one_token(token, acl_p, sizeof(*acl_p));
}

void
sat_fetch_mac_label_token(sat_token_t token, mac_t *mac_p)
{
	fetch_data_token(token, (char **)mac_p);
}

void
sat_fetch_socket_token(sat_token_t token, sat_socket_id_t *socket_p)
{
	sat_fetch_one_token(token, socket_p, sizeof(*socket_p));
}

void
sat_fetch_ifname_token(sat_token_t token, sat_ifname_t *ifname_p)
{
	sat_fetch_one_token(token, ifname_p, SAT_IFNAME_SIZE);
}

void
sat_fetch_descriptor_list_token(sat_token_t token, sat_descriptor_t **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_sysarg_list_token(sat_token_t token, sat_sysarg_t **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_uid_list_token(sat_token_t token, uid_t **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_gid_list_token(sat_token_t token, gid_t **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_ip_header_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_sockadder_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, (char **)data_p);
}

void
sat_fetch_unknown_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_file_token(sat_token_t token, ino64_t *inode_p, dev_t *device_p)
{
	int cursor;

	cursor = sat_fetch_token(token, inode_p, sizeof(*inode_p), 0);
	cursor = sat_fetch_token(token, device_p, sizeof(*device_p), cursor);
}

void
sat_fetch_device_token(sat_token_t token, dev_t *device_p)
{
	sat_fetch_one_token(token, device_p, sizeof(*device_p));
}

void
sat_fetch_ugid_token(sat_token_t token, uid_t *uid_p, gid_t *gid_p)
{
	int cursor;

	cursor = sat_fetch_token(token, uid_p, sizeof(*uid_p), 0);
	cursor = sat_fetch_token(token, gid_p, sizeof(*gid_p), cursor);
}

void
sat_fetch_openmode_token(sat_token_t token, uint32_t *mode_p)
{
	fetch_int32_token(token, (int32_t *)mode_p);
}

void
sat_fetch_signal_token(sat_token_t token, uint32_t *sig_p)
{
	fetch_int32_token(token, (int32_t *)sig_p);
}

void
sat_fetch_status_token(sat_token_t token, uint32_t *status_p)
{
	fetch_int32_token(token, (int32_t *)status_p);
}

void
sat_fetch_satid_token(sat_token_t token, uid_t *auid_p)
{
	sat_fetch_one_token(token, auid_p, sizeof(*auid_p));
}

void
sat_fetch_syscall_token(sat_token_t token, uint8_t *major_p, uint16_t *minor_p)
{
	int cursor;

	cursor = sat_fetch_token(token, major_p, sizeof(*major_p), 0);
	cursor = sat_fetch_token(token, minor_p, sizeof(*minor_p), cursor);
}

void
sat_fetch_time_token(sat_token_t token, time_t *clock_p, uint8_t *ticks_p)
{
	int cursor;

	cursor = sat_fetch_token(token, clock_p, sizeof(*clock_p), 0);
	cursor = sat_fetch_token(token, ticks_p, sizeof(*ticks_p), cursor);
}

void
sat_fetch_protocol_token(sat_token_t token, uint16_t *dom_p, uint16_t *proto_p)
{
	int cursor;

	cursor = sat_fetch_token(token, dom_p, sizeof(*dom_p), 0);
	cursor = sat_fetch_token(token, proto_p, sizeof(*proto_p), cursor);
}

void
sat_fetch_ifreq_token(sat_token_t token, char **ifreq_p)
{
	fetch_data_token(token, ifreq_p);
}

void
sat_fetch_opaque_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_text_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_command_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_pathname_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_lookup_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_cwd_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_root_token(sat_token_t token, char **data_p)
{
	fetch_data_token(token, data_p);
}

void
sat_fetch_record_header_token(
	sat_token_t token,
	int32_t *magic_p,
	sat_token_size_t *size_p,
	int8_t *rectype_p,
	int8_t *outcome_p,
	int8_t *sequence_p,
	int8_t *error_p)
{
	int cursor;

	cursor = sat_fetch_token(token, magic_p, sizeof(*magic_p), 0);
	cursor = sat_fetch_token(token, size_p, sizeof(*size_p), cursor);
	cursor = sat_fetch_token(token, rectype_p, sizeof(*rectype_p), cursor);
	cursor = sat_fetch_token(token, outcome_p, sizeof(*outcome_p), cursor);
	cursor = sat_fetch_token(token, sequence_p, sizeof(*sequence_p),cursor);
	cursor = sat_fetch_token(token, error_p, sizeof(*error_p), cursor);
}
