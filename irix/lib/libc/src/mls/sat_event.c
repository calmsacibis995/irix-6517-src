/*
 * Copyright 1990, Silicon Graphics, Inc. 
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

#ifdef __STDC__
	#pragma weak sat_eventtostr = _sat_eventtostr
	#pragma weak sat_strtoevent = _sat_strtoevent
#endif

#include "synonyms.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <string.h>

#define MAXEVENTMSGLEN	32
static const struct {
	short	type;
	char	name[MAXEVENTMSGLEN - sizeof(short)];
} events[] = {
	SAT_ACCESS_DENIED,	"sat_access_denied",
	SAT_ACCESS_FAILED,	"sat_access_failed",
	SAT_CHDIR,		"sat_chdir",
	SAT_CHROOT,		"sat_chroot",
	SAT_OPEN,		"sat_open",
	SAT_OPEN_RO,		"sat_open_ro",
	SAT_READ_SYMLINK,	"sat_read_symlink",
	SAT_FILE_CRT_DEL,	"sat_file_crt_del",
	SAT_FILE_CRT_DEL2,	"sat_file_crt_del2",
	SAT_FILE_WRITE,		"sat_file_write",
	SAT_MOUNT,		"sat_mount",
	SAT_FILE_ATTR_READ,	"sat_file_attr_read",
	SAT_FILE_ATTR_WRITE,	"sat_file_attr_write",
	SAT_EXEC,		"sat_exec",
	SAT_SYSACCT,		"sat_sysacct",

	SAT_FCHDIR,		"sat_fchdir",
	SAT_FD_READ,		"sat_fd_read",
	SAT_FD_READ2,		"sat_fd_read2",
	SAT_TTY_SETLABEL,	"sat_tty_setlabel",
	SAT_FD_WRITE,		"sat_fd_write",
	SAT_FD_ATTR_WRITE,	"sat_fd_attr_write",
	SAT_PIPE,		"sat_pipe",
	SAT_DUP,		"sat_dup",
	SAT_CLOSE,		"sat_close",

	SAT_FORK,		"sat_fork",
	SAT_EXIT,		"sat_exit",
	SAT_PROC_READ,		"sat_proc_read",
	SAT_PROC_WRITE,		"sat_proc_write",
	SAT_PROC_ATTR_READ,	"sat_proc_attr_read",
	SAT_PROC_ATTR_WRITE,	"sat_proc_attr_write",
	SAT_PROC_OWN_ATTR_WRITE, "sat_proc_own_attr_write",
	SAT_PROC_ACCT,		"sat_proc_acct",
	SAT_SESSION_ACCT,	"sat_session_acct",

	SAT_SVIPC_ACCESS,	"sat_svipc_access",
	SAT_SVIPC_CREATE,	"sat_svipc_create",
	SAT_SVIPC_REMOVE,	"sat_svipc_remove",
	SAT_SVIPC_CHANGE,	"sat_svipc_change",

	SAT_BSDIPC_CREATE,	"sat_bsdipc_create",
	SAT_BSDIPC_CREATE_PAIR,	"sat_bsdipc_create_pair",
	SAT_BSDIPC_SHUTDOWN,	"sat_bsdipc_shutdown",
	SAT_BSDIPC_MAC_CHANGE,	"sat_bsdipc_mac_change",
	SAT_BSDIPC_ADDRESS,	"sat_bsdipc_address",
	SAT_BSDIPC_RESVPORT,	"sat_bsdipc_resvport",
	SAT_BSDIPC_DELIVER,	"sat_bsdipc_deliver",
	SAT_BSDIPC_CANTFIND,	"sat_bsdipc_cantfind",
	SAT_BSDIPC_SNOOP_OK,	"sat_bsdipc_snoop_ok",
	SAT_BSDIPC_SNOOP_FAIL,	"sat_bsdipc_snoop_fail",

	SAT_CLOCK_SET,		"sat_clock_set",
	SAT_HOSTNAME_SET,	"sat_hostname_set",
	SAT_DOMAINNAME_SET,	"sat_domainname_set",
	SAT_HOSTID_SET,		"sat_hostid_set",

	SAT_CHECK_PRIV,		"sat_check_priv",
	SAT_CONTROL,		"sat_control",
	SAT_SYS_NOTE,		"sat_sys_note",

	SAT_BSDIPC_DAC_CHANGE,	"sat_bsdipc_dac_change",
	SAT_BSDIPC_DAC_DENIED,	"sat_bsdipc_dac_denied",
	SAT_BSDIPC_IF_SETUID,	"sat_bsdipc_if_setuid",
	SAT_BSDIPC_RX_OK,	"sat_bsdipc_rx_ok",
	SAT_BSDIPC_RX_RANGE,	"sat_bsdipc_rx_range",
	SAT_BSDIPC_RX_MISSING,	"sat_bsdipc_rx_missing",
	SAT_BSDIPC_TX_OK,	"sat_bsdipc_tx_ok",
	SAT_BSDIPC_TX_RANGE,	"sat_bsdipc_tx_range",
	SAT_BSDIPC_TX_TOOBIG,	"sat_bsdipc_tx_toobig",
	SAT_BSDIPC_IF_CONFIG,	"sat_bsdipc_if_config",
	SAT_BSDIPC_IF_INVALID,	"sat_bsdipc_if_invalid",
	SAT_BSDIPC_IF_SETLABEL,	"sat_bsdipc_if_setlabel",

	SAT_AE_AUDIT,		"sat_ae_audit",
	SAT_AE_IDENTITY,	"sat_ae_identity",
	SAT_AE_DBEDIT,		"sat_ae_dbedit",
	SAT_AE_MOUNT,		"sat_ae_mount",
	SAT_AE_CUSTOM,		"sat_ae_custom",
	SAT_AE_LP,		"sat_ae_lp",
	SAT_AE_X_ALLOWED,	"sat_ae_x_allowed",
	SAT_AE_X_DENIED,	"sat_ae_x_denied",

	SAT_SVR4NET_CREATE,	"sat_svr4net_create",
	SAT_SVR4NET_ADDRESS,	"sat_svr4net_address",
	SAT_SVR4NET_SHUTDOWN,	"sat_svr4net_shutdown",

	SAT_PROC_OWN_EXT_ATTR_WRITE, "sat_proc_own_ext_attr_write",

	NULL,			NULL,
};


/*
 * sat_eventtostr - convert an event index to an event string
 */

char *
sat_eventtostr(int event_type)
{
	int i;

	if (event_type <= 0 || event_type >= SAT_NTYPES)
		return NULL;

	for (i = 0; events[i].name[0] != NULL; i++)
		if (events[i].type == event_type)
			return ((char *)events[i].name);

	return NULL;
}

/*
 * event_strtoevent - convert a string format event name into an event index
 */
int
sat_strtoevent(const char *event_name)
{
	int	i;

	if (event_name == NULL)
		return (-1);

	for (i=0; events[i].name[0] != NULL; i++)
		if (! strcmp( events[i].name, event_name ))
			return events[i].type;

	return (-1);
}
