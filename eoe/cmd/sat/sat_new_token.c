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
#include <strings.h>
#include <net/if.h>
#include "sat_token.h"

#ifdef _KERNEL

sat_token_t
sat_new_token(int tid)
{
	struct mbuf *token;
	sat_token_t tp;
	sat_token_size_t size = sizeof(sat_token_header_t);

	token = m_get(M_WAIT, MT_SAT);
	tp = (sat_token_t)token->m_data;
	token->m_len = sizeof(tp->sat_token_id) + sizeof(tp->sat_token_size);
	bcopy(&tid, &tp->sat_token_id, sizeof(tp->sat_token_id));
	bcopy(&size, &tp->sat_token_size, sizeof(tp->sat_token_size));

	return (sat_token_t)token;
}

void
sat_free_token(sat_token_t token)
{
	if (token == NULL)
		return;

	m_freem(token);
}

void
sat_append_token(sat_token_t token, void *data, int size)
{
	struct mbuf *new;
	int this_size;
	sat_token_size_t token_size;
	char *tsp;

	/*
	 * tsp is the place to put the token size.
	 */
	tsp = mtod( token, char *);
	tsp += sizeof(sat_token_id_t);
	bcopy(tsp, &token_size, sizeof(token_size));

	token_size += size;

	while (size > 0) {
		new = m_get(M_WAIT, MT_SAT);
		this_size = size > MLEN ? MLEN : size;
		bcopy(data, new->m_data, this_size);
		new->m_len = this_size;
		m_cat((struct mbuf *)token, new);
		data = (char *)data + this_size;
		size -= this_size;
	}

	bcopy(&token_size, tsp, sizeof(token_size));
}

#else	/* _KERNEL */

sat_token_t
sat_new_token(int tid)
{
	sat_token_t token;

	token = (sat_token_t) malloc(sizeof(token));
	token->token_header.sat_token_id = tid;
	token->token_header.sat_token_size = sizeof(sat_token_header_t);

	return token;
}

void
sat_free_token(sat_token_t token)
{
	if (token == NULL)
		return;

	free(token);
}

sat_token_t
sat_append_token(sat_token_t token, void *data, int size)
{
	token = (sat_token_t)realloc(token,
	    token->token_header.sat_token_size + size);
	bcopy(data,
	    &token->token_data[token->token_header.sat_token_size -
	    sizeof(sat_token_header_t)], size);
	token->token_header.sat_token_size += size;

	return token;
}

#endif	/* _KERNEL */

sat_token_t
sat_new_errno_token(uint8_t error_number)
{
	sat_token_t token;

	token = sat_new_token(SAT_ERRNO_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &error_number, sizeof(error_number));
#else
	token = sat_append_token(token, &error_number, sizeof(error_number));
#endif

	return token;
}

sat_token_t
sat_new_pid_token(int pid)
{
	sat_token_t token;

	token = sat_new_token(SAT_PID_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &pid, sizeof(pid));
#else
	token = sat_append_token(token, &pid, sizeof(pid));
#endif

	return token;
}

sat_token_t
sat_new_binary_token(int8_t bit)
{
	sat_token_t token;

	token = sat_new_token(SAT_BINARY_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &bit, sizeof(bit));
#else
	token = sat_append_token(token, &bit, sizeof(bit));
#endif

	return token;
}

sat_token_t
sat_new_mode_token(mode_t mode)
{
	sat_token_t token;

	token = sat_new_token(SAT_MODE_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &mode, sizeof(mode));
#else
	token = sat_append_token(token, &mode, sizeof(mode));
#endif

	return token;
}

sat_token_t
sat_new_cap_set_token(cap_t cap_set)
{
	sat_token_t token;

	token = sat_new_token(SAT_CAP_SET_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, cap_set, sizeof(cap_set_t));
#else
	token = sat_append_token(token, cap_set, sizeof(cap_set_t));
#endif

	return token;
}

sat_token_t
sat_new_cap_value_token(cap_value_t cap)
{
	sat_token_t token;

	token = sat_new_token(SAT_CAP_VALUE_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &cap, sizeof(cap));
#else
	token = sat_append_token(token, &cap, sizeof(cap));
#endif

	return token;
}

sat_token_t
sat_new_privilege_token(cap_value_t cap, uint8_t how)
{
	sat_token_t token;

	token = sat_new_token(SAT_PRIVILEGE_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &cap, sizeof(cap));
	sat_append_token(token, &how, sizeof(how));
#else
	token = sat_append_token(token, &cap, sizeof(cap));
	token = sat_append_token(token, &how, sizeof(how));
#endif

	return token;
}

sat_token_t
sat_new_acl_token(acl_t acl)
{
	sat_token_t token;

	token = sat_new_token(SAT_ACL_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, acl, sizeof(struct acl));
#else
	token = sat_append_token(token, acl, sizeof(struct acl));
#endif

	return token;
}

sat_token_t
sat_new_mac_label_token(mac_t mac)
{
	sat_token_t token;
#ifdef _KERNEL
	extern int mac_enabled;

	if (!mac_enabled)
		return NULL;
#endif

	token = sat_new_token(SAT_MAC_LABEL_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, mac, _MAC_SIZE(mac));
#else
	token = sat_append_token(token, mac, mac_size(mac));
#endif

	return token;
}

sat_token_t
sat_new_socket_token(sat_socket_id_t socket)
{
	sat_token_t token;

	token = sat_new_token(SAT_SOCKET_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &socket, sizeof(socket));
#else
	token = sat_append_token(token, &socket, sizeof(socket));
#endif

	return token;
}

sat_token_t
sat_new_ifname_token(sat_ifname_t ifname)
{
	sat_token_t token;

	token = sat_new_token(SAT_IFNAME_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, ifname, SAT_IFNAME_SIZE);
#else
	token = sat_append_token(token, ifname, SAT_IFNAME_SIZE);
#endif

	return token;
}

sat_token_t
sat_new_descriptor_list_token(sat_descriptor_t *fds, int count)
{
	sat_token_t token;
	int size = count * sizeof(sat_descriptor_t);

	token = sat_new_token(SAT_DESCRIPTOR_LIST_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, fds, size);
#else
	token = sat_append_token(token, fds, size);
#endif

	return token;
}

sat_token_t
sat_new_sysarg_list_token(sat_sysarg_t *args, int count)
{
	sat_token_t token;
	int size = count * sizeof(sat_sysarg_t);

	token = sat_new_token(SAT_SYSARG_LIST_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, args, size);
#else
	token = sat_append_token(token, args, size);
#endif

	return token;
}

sat_token_t
sat_new_uid_list_token(uid_t *uidp, int count)
{
	sat_token_t token;
	int size = count * sizeof(uid_t);

	token = sat_new_token(SAT_UID_LIST_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, uidp, size);
#else
	token = sat_append_token(token, uidp, size);
#endif

	return token;
}

sat_token_t
sat_new_gid_list_token(gid_t *gidp, int count)
{
	sat_token_t token;
	int size = count * sizeof(gid_t);

	token = sat_new_token(SAT_GID_LIST_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, gidp, size);
#else
	token = sat_append_token(token, gidp, size);
#endif

	return token;
}


sat_token_t
sat_new_file_token(ino64_t inode, dev_t device)
{
	sat_token_t token;

	token = sat_new_token(SAT_FILE_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &inode, sizeof(ino64_t));
	sat_append_token(token, &device, sizeof(dev_t));
#else
	token = sat_append_token(token, &inode, sizeof(ino64_t));
	token = sat_append_token(token, &device, sizeof(dev_t));
#endif

	return token;
}

sat_token_t
sat_new_ugid_token(uid_t uid, gid_t gid)
{
	sat_token_t token;

	token = sat_new_token(SAT_UGID_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &uid, sizeof(uid_t));
	sat_append_token(token, &gid, sizeof(gid_t));
#else
	token = sat_append_token(token, &uid, sizeof(uid_t));
	token = sat_append_token(token, &gid, sizeof(gid_t));
#endif

	return token;
}

sat_token_t
sat_new_satid_token(uid_t satid)
{
	sat_token_t token;

	token = sat_new_token(SAT_SATID_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &satid, sizeof(satid));
#else
	token = sat_append_token(token, &satid, sizeof(satid));
#endif

	return token;
}

sat_token_t
sat_new_syscall_token(uint8_t major, uint16_t minor)
{
	sat_token_t token;

	token = sat_new_token(SAT_SYSCALL_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &major, sizeof(major));
	sat_append_token(token, &minor, sizeof(minor));
#else
	token = sat_append_token(token, &major, sizeof(major));
	token = sat_append_token(token, &minor, sizeof(minor));
#endif

	return token;
}


sat_token_t
sat_new_time_token(time_t clock, uint8_t ticks)
{
	sat_token_t token;

	token = sat_new_token(SAT_TIME_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &clock, sizeof(clock));
	sat_append_token(token, &ticks, sizeof(ticks));
#else
	token = sat_append_token(token, &clock, sizeof(clock));
	token = sat_append_token(token, &ticks, sizeof(ticks));
#endif

	return token;
}

sat_token_t
sat_new_protocol_token(uint16_t domain, uint16_t protocol)
{
	sat_token_t token;

	token = sat_new_token(SAT_PROTOCOL_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &domain, sizeof(domain));
	sat_append_token(token, &protocol, sizeof(protocol));
#else
	token = sat_append_token(token, &domain, sizeof(domain));
	token = sat_append_token(token, &protocol, sizeof(protocol));
#endif

	return token;
}

sat_token_t
sat_new_ifreq_token(struct ifreq *ifreq)
{
	sat_token_t token;

	token = sat_new_token(SAT_IFREQ_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, ifreq, sizeof(struct ifreq));
#else
	token = sat_append_token(token, ifreq, sizeof(struct ifreq));
#endif

	return token;
}


sat_token_t
sat_new_record_header_token(
	int32_t magic,
	sat_token_size_t size,
	int8_t rectype,
	int8_t outcome,
	int8_t sequence,
	int8_t errno)
{
	sat_token_t token;

	token = sat_new_token(SAT_RECORD_HEADER_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &magic, sizeof(magic));
	sat_append_token(token, &size, sizeof(size));
	sat_append_token(token, &rectype, sizeof(rectype));
	sat_append_token(token, &outcome, sizeof(outcome));
	sat_append_token(token, &sequence, sizeof(sequence));
	sat_append_token(token, &errno, sizeof(errno));
#else
	token = sat_append_token(token, &magic, sizeof(magic));
	token = sat_append_token(token, &size, sizeof(size));
	token = sat_append_token(token, &rectype, sizeof(rectype));
	token = sat_append_token(token, &outcome, sizeof(outcome));
	token = sat_append_token(token, &sequence, sizeof(sequence));
	token = sat_append_token(token, &errno, sizeof(errno));
#endif

	return token;
}

sat_token_t
sat_new_device_token(dev_t device)
{
	sat_token_t token;

	token = sat_new_token(SAT_DEVICE_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, &device, sizeof(dev_t));
#else
	token = sat_append_token(token, &device, sizeof(dev_t));
#endif

	return token;
}

sat_token_t
sat_new_titled_text_token(char *title, char *text)
{
	sat_token_t token;

	token = sat_new_token(SAT_TITLED_TEXT_TOKEN);
#ifdef _KERNEL
	sat_append_token(token, title, SAT_TITLE_SIZE);
	sat_append_token(token, text, strlen(text) + 1);
#else
	token = sat_append_token(token, title, SAT_TITLE_SIZE);
	token = sat_append_token(token, text, strlen(text) + 1);
#endif
	return token;
}

sat_token_t
sat_new_uint32_token(sat_token_id_t tid, uint32_t data)
{
	sat_token_t token;

	token = sat_new_token(tid);
#ifdef _KERNEL
	sat_append_token(token, &data, sizeof(data));
#else
	token = sat_append_token(token, &data, sizeof(data));
#endif

	return token;
}

sat_token_t
sat_new_hostid_token(uint32_t hid)
{
	return sat_new_uint32_token(SAT_HOSTID_TOKEN, hid);
}

sat_token_t
sat_new_signal_token(uint32_t sig)
{
	return sat_new_uint32_token(SAT_SIGNAL_TOKEN, sig);
}

sat_token_t
sat_new_status_token(uint32_t status)
{
	return sat_new_uint32_token(SAT_STATUS_TOKEN, status);
}

sat_token_t
sat_new_port_token(int32_t port)
{
	return sat_new_uint32_token(SAT_PORT_TOKEN, port);
}

sat_token_t
sat_new_svipc_id_token(int32_t svipc_id)
{
	return sat_new_uint32_token(SAT_SVIPC_ID_TOKEN, svipc_id);
}

sat_token_t
sat_new_svipc_key_token(int32_t svipc_key)
{
	return sat_new_uint32_token(SAT_SVIPC_KEY_TOKEN, svipc_key);
}

sat_token_t
sat_new_openmode_token(uint32_t mode)
{
	return sat_new_uint32_token(SAT_OPENMODE_TOKEN, mode);
}

static sat_token_t
sat_new_charp_token(sat_token_id_t tid, char *cp, sat_token_size_t size)
{
	sat_token_t token;

	token = sat_new_token(tid);
#ifdef _KERNEL
	sat_append_token(token, cp, size);
#else
	token = sat_append_token(token, cp, size);
#endif
	return token;
}

sat_token_t
sat_new_pathname_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_PATHNAME_TOKEN, text, size);
}

sat_token_t
sat_new_lookup_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_LOOKUP_TOKEN, text, size);
}

sat_token_t
sat_new_opaque_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_OPAQUE_TOKEN, text, size);
}

sat_token_t
sat_new_ip_header_token(char *header, int count)
{
	return sat_new_charp_token(SAT_IP_HEADER_TOKEN, header, count);
}

sat_token_t
sat_new_sockadder_token(char *sockadder, int count)
{
	return sat_new_charp_token(SAT_SOCKADDER_TOKEN, sockadder, count);
}

sat_token_t
sat_new_text_token(char *text)
{
	return sat_new_charp_token(SAT_TEXT_TOKEN, text, strlen(text) + 1);
}
