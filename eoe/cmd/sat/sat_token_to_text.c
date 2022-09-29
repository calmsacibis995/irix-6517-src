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

#ident "$Revision: 1.2 $"

#include <sys/sat.h>
#include <sys/file.h>
#include <sys/extacct.h>
#include "sat_token.h"
#include <pwd.h>
#include <grp.h>
#include <sys/mkdev.h>
#include <stdlib.h>
#include <time.h>

extern char *sys_call(int, int);
extern char *sat_eventtostr(int );


static char *
itoa(int i)
{
	char buffer[80];

	sprintf(buffer, "%d", i);
	return strdup(buffer);
}

static char *
itox(int i)
{
	char buffer[80];

	sprintf(buffer, "0x%x", i);
	return strdup(buffer);
}

static char *
combine(char *front, char *back)
{
	front = realloc(front, strlen(front) + strlen(back) + 2);
	strcat(front, ",");
	strcat(front, back);
	free(back);

	return front;
}

static char *
username(uid_t uid)
{
	struct passwd_list {
		struct passwd_list	*l_next;
		char			*l_name;
		uid_t			l_uid;
	};
	static struct passwd_list *list; 
	struct passwd_list *lp;
	struct passwd *pwp;

	for (lp = list ; lp ; lp = lp->l_next)
		if (lp->l_uid == uid)
			return strdup(lp->l_name);
	
	if ((pwp = getpwuid(uid)) == NULL)
		return itoa(uid);

	lp = malloc(sizeof(struct passwd_list));
	lp->l_uid = uid;
	lp->l_name = strdup(pwp->pw_name);
	lp->l_next = list;
	list = lp;

	return strdup(pwp->pw_name);
}

static char *
groupname(gid_t gid)
{
	struct group_list {
		struct group_list	*l_next;
		char			*l_name;
		gid_t			l_gid;
	};
	static struct group_list *list; 
	struct group_list *lp;
	struct group *gp;

	for (lp = list ; lp ; lp = lp->l_next)
		if (lp->l_gid == gid)
			return strdup(lp->l_name);
	
	if ((gp = getgrgid(gid)) == NULL)
		return itoa(gid);

	lp = malloc(sizeof(struct group_list));
	lp->l_gid = gid;
	lp->l_name = strdup(gp->gr_name);
	lp->l_next = list;
	list = lp;

	return strdup(gp->gr_name);
}

static char digits[] = "0123456789abcdef";

static char *
hex(char *src, int count)
{
	char *result;
	char *cursor;
	int i;

	result = malloc((count * 3) + 3);

	*result = '(';
	for (i = 0, cursor = result + 1; i < count; i++) {
		*cursor++ = digits[(src[i] >> 4) & 0xf];
		*cursor++ = digits[src[i] & 0xf];
		*cursor++ = ' ';
	}
	*cursor++ = ')';
	*cursor++ = '\0';

	return result;
}

char *
sat_errno_token_to_text(sat_token_t token)
{
	uint8_t error_number;

	sat_fetch_one_token(token, &error_number, sizeof(error_number));

	return strdup(strerror(error_number));
}

char *
sat_audit_id_token_to_text(sat_token_t token)
{
	uid_t auid;

	sat_fetch_one_token(token, &auid, sizeof(auid));

	return username(auid);
}

char *
int32_to_a(sat_token_t token)
{
	int32_t i;

	sat_fetch_one_token(token, &i, sizeof(i));

	return itoa(i);
}

char *
sat_pid_token_to_text(sat_token_t token)
{
	return int32_to_a(token);
}

char *
sat_parent_pid_token_to_text(sat_token_t token)
{
	return int32_to_a(token);
}

char *
sat_binary_token_to_text(sat_token_t token)
{
	int8_t bit;

	sat_fetch_one_token(token, &bit, sizeof(bit));

	return strdup(bit ? "true" : "false");
}

char *
sat_hostid_token_to_text(sat_token_t token)
{
	uint32_t hid;
	char buffer[80];

	sat_fetch_one_token(token, &hid, sizeof(hid));

	sprintf(buffer, "%d.%d.%d.%d",
	    (hid >> 24) & 0xff, (hid >> 16) & 0xff,
	    (hid >> 8) & 0xff, hid & 0xff);
	return strdup(buffer);
}

char *
sat_port_token_to_text(sat_token_t token)
{
	int32_t port;

	sat_fetch_one_token(token, &port, sizeof(port));

	return itoa(port);
}

char *
sat_mode_token_to_text(sat_token_t token)
{
	mode_t mode;
	char buffer[10];

	sat_fetch_one_token(token, &mode, sizeof(mode));

	buffer[0] = (mode & 0400) ? 'r' : '-';
	buffer[1] = (mode & 0200) ? 'w' : '-';
	buffer[2] = (mode & 0100) ? 'x' : '-';
	buffer[3] = (mode & 0040) ? 'r' : '-';
	buffer[4] = (mode & 0020) ? 'w' : '-';
	buffer[5] = (mode & 0010) ? 'x' : '-';
	buffer[6] = (mode & 0004) ? 'r' : '-';
	buffer[7] = (mode & 0002) ? 'w' : '-';
	buffer[8] = (mode & 0001) ? 'x' : '-';
	buffer[9] = '\0';
	return strdup(buffer);
}

char *
sat_svipc_id_token_to_text(sat_token_t token)
{
	int32_t svipc_id;

	sat_fetch_one_token(token, &svipc_id, sizeof(svipc_id));

	return itox(svipc_id);
}

char *
sat_svipc_key_token_to_text(sat_token_t token)
{
	int32_t svipc_key;

	sat_fetch_one_token(token, &svipc_key, sizeof(svipc_key));

	return itox(svipc_key);
}

char *
sat_titled_text_token_to_text(sat_token_t token)
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
	strcat(title, ":");
	
	title = realloc(title, strlen(title) + strlen(text) + 1);
	strcat(title, text);
	free(text);

	return title;
}

char *
sat_cap_set_token_to_text(sat_token_t token)
{
	cap_set_t cap_set;
	char *cap_text;
	char *result;

	sat_fetch_one_token(token, &cap_set, sizeof(cap_set));
	cap_text = cap_to_text(&cap_set, NULL);
	result = malloc(strlen(cap_text) + 3);
	sprintf(result, "(%s)", cap_text);
	cap_free(cap_text);

	return result;
}

char *
sat_cap_value_token_to_text(sat_token_t token)
{
	cap_value_t cap;
	char *cap_text;
	char *result;

	sat_fetch_one_token(token, &cap, sizeof(cap));
	cap_text = cap_value_to_text(cap);
	result = malloc(strlen(cap_text) + 3);
	sprintf(result, "(%s)", cap_text);
	cap_free(cap_text);

	return result;
}

char *
sat_privilege_token_to_text(sat_token_t token)
{
	int cursor;
	cap_value_t cap;
	uint8_t how;
	char *how_text;
	char *cap_text;
	char *result;

	cursor = sat_fetch_token(token, &cap, sizeof(cap), 0);
	cursor = sat_fetch_token(token, &how, sizeof(how), cursor);

	switch (how) {
	case 0:
		how_text = "-/capability=%s";
		break;
	case SAT_SUSERPOSS:
		how_text = "*/capability=%s";
		break;
	case SAT_CAPPOSS:
		how_text = "+/capability=%s";
		break;
	case SAT_SUSERPOSS | SAT_CAPPOSS:
		how_text = "+/capability=%s";
		break;
	default:
		how_text = "?/capability=%s";
		break;
	}
	cap_text = cap_value_to_text(cap);
	result = malloc(strlen(cap_text) + strlen(how_text) + 2);
	sprintf(result, how_text, cap_text);
	cap_free(cap_text);
	return result;
}

char *
sat_acl_token_to_text(sat_token_t token)
{
	struct acl acl;

	sat_fetch_one_token(token, &acl, sizeof(acl));

	return acl_to_short_text(&acl, NULL);
}

char *
sat_mac_label_token_to_text(sat_token_t token)
{
	mac_t mac = (mac_t)malloc(SAT_TOKEN_DATA_SIZE(token));
	char *text;

	sat_fetch_one_token(token, mac, SAT_TOKEN_DATA_SIZE(token));

	text = mac_to_text(mac, NULL);
	mac_free(mac);

	return text;
}

char *
sat_socket_token_to_text(sat_token_t token)
{
	sat_socket_id_t socket;

	sat_fetch_one_token(token, &socket, sizeof(socket));

	return itox(socket);
}

/*
 * XXX:casey - hackish
 */
char *
sat_ifname_token_to_text(sat_token_t token)
{
	sat_ifname_t ifname = (sat_ifname_t)malloc(SAT_IFNAME_SIZE);

	sat_fetch_one_token(token, ifname, SAT_IFNAME_SIZE);

	return ifname;
}

char *
sat_descriptor_list_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	int count;
	int i;
	sat_descriptor_t *fds;
	char *text;

	fds = (sat_descriptor_t *)malloc(data_size);
	count = data_size / sizeof(sat_descriptor_t);

	if (count == 0)
		return strdup("BAD-FD");

	sat_fetch_one_token(token, fds, data_size);

	for (i = 1, text = itoa(fds[0]); i < count; i++)
		text = combine(text, itoa(fds[i]));

	free(fds);

	return text;
}

char *
sat_sysarg_list_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	int count;
	int i;
	sat_sysarg_t *args;
	char *text;

	args = (sat_sysarg_t *)malloc(data_size);
	count = data_size / sizeof(sat_sysarg_t);

	if (count == 0)
		return strdup("BAD-SYSARGS");

	sat_fetch_one_token(token, args, data_size);

	for (i = 1, text = itox(args[0]); i < count; i++)
		text = combine(text, itox(args[i]));

	free(args);

	return text;
}

char *
sat_uid_list_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	int count;
	int i;
	uid_t *uidp;
	char *text;

	uidp = (uid_t *)malloc(data_size);
	count = data_size / sizeof(uid_t);

	if (count == 0)
		return strdup("BAD-UID");

	sat_fetch_one_token(token, uidp, data_size);

	for (i = 1, text = username(uidp[0]); i < count; i++)
		text = combine(text, username(uidp[i]));

	free(uidp);

	return text;
}

char *
sat_gid_list_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	int count;
	int i;
	gid_t *gidp;
	char *text;

	gidp = (gid_t *)malloc(data_size);
	count = data_size / sizeof(gid_t);

	if (count == 0)
		return strdup("BAD-GID");

	sat_fetch_one_token(token, gidp, data_size);

	for (i = 1, text = groupname(gidp[0]); i < count; i++)
		text = combine(text, groupname(gidp[i]));

	free(gidp);

	return text;
}

char *
sat_ip_header_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *header;
	char *text;

	if (data_size == 0)
		return strdup("BAD-IP-HEADER");

	header = malloc(data_size);

	sat_fetch_one_token(token, header, data_size);

	text = hex(header, data_size);
	free(header);

	return text;
}

char *
sat_sockadder_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *sockadder;
	char *text;

	if (data_size == 0)
		return strdup("BAD-SOCKADDER");

	sockadder = malloc(data_size);

	sat_fetch_one_token(token, sockadder, data_size);

	text = hex(sockadder, data_size);
	free(sockadder);

	return text;
}

char *
sat_unknown_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *data;
	char *text;

	if (data_size == 0)
		return strdup("EMPTY-UNKNOWN");

	data = malloc(data_size);

	sat_fetch_one_token(token, data, data_size);

	text = hex(data, data_size);
	free(data);

	return text;
}

char *
sat_file_token_to_text(sat_token_t token)
{
	int cursor;
	ino64_t inode;
	dev_t device;
	char buffer[80];

	cursor = sat_fetch_token(token, &inode, sizeof(ino64_t), 0);
	cursor = sat_fetch_token(token, &device, sizeof(dev_t), cursor);

	sprintf(buffer, "%lld,%d,%d", inode, major(device), minor(device));

	return strdup(buffer);
}

char *
sat_device_token_to_text(sat_token_t token)
{
	dev_t device;
	char buffer[80];

	sat_fetch_one_token(token, &device, sizeof(dev_t));

	sprintf(buffer, "%d,%d", major(device), minor(device));

	return strdup(buffer);
}

char *
sat_ugid_token_to_text(sat_token_t token)
{
	int cursor;
	uid_t uid;
	gid_t gid;

	cursor = sat_fetch_token(token, &uid, sizeof(uid_t), 0);
	cursor = sat_fetch_token(token, &gid, sizeof(gid_t), cursor);

	return combine(username(uid), groupname(gid));
}

char *
sat_openmode_token_to_text(sat_token_t token)
{
	uint32_t flags;
	char buf[256];

	sat_fetch_one_token(token, &flags, sizeof(flags));

	/*
	 * reverse the -= FOPEN done in the kernel
	 */
	flags += FOPEN;

	if ((flags & O_ACCMODE) == O_RDONLY)
		strcpy(buf, "(O_RDONLY");
	if ((flags & O_ACCMODE) == O_WRONLY)
		strcpy(buf, "(O_WRONLY");
	if ((flags & O_ACCMODE) == O_RDWR)
		strcpy(buf, "(O_RDWR");

	if (flags & O_APPEND)
		strcat(buf, " | O_APPEND");
	if (flags & O_CREAT)
		strcat(buf, " | O_CREAT");
	if (flags & O_EXCL)
		strcat(buf, " | O_EXCL");
	if (flags & O_NDELAY)
		strcat(buf, " | O_NDELAY");
	if (flags & O_NOCTTY)
		strcat(buf, " | O_NOCTTY");
	if (flags & O_NONBLOCK)
		strcat(buf, " | O_NONBLOCK");
	if (flags & O_SYNC)
		strcat(buf, " | O_SYNC");
	if (flags & O_TRUNC)
		strcat(buf, " | O_TRUNC");

	strcat(buf, ")");
	return strdup(buf);
}

char *
sat_signal_token_to_text(sat_token_t token)
{
	uint32_t sig;

	sat_fetch_one_token(token, &sig, sizeof(sig));

	return itoa(sig);
}

char *
sat_status_token_to_text(sat_token_t token)
{
	uint32_t status;

	sat_fetch_one_token(token, &status, sizeof(status));

	return itoa(status);
}

char *
sat_satid_token_to_text(sat_token_t token)
{
	uid_t satid;

	sat_fetch_one_token(token, &satid, sizeof(satid));

	return username(satid);
}

char *
sat_syscall_token_to_text(sat_token_t token)
{
	int cursor;
	uint8_t major;
	uint16_t minor;

	cursor = sat_fetch_token(token, &major, sizeof(major), 0);
	cursor = sat_fetch_token(token, &minor, sizeof(minor), cursor);

	return strdup(sys_call(major, minor));
}

char *
sat_time_token_to_text(sat_token_t token)
{
	int cursor;
	time_t clock;
	uint8_t ticks;
	char buffer[80];

	cursor = sat_fetch_token(token, &clock, sizeof(clock), 0);
	cursor = sat_fetch_token(token, &ticks, sizeof(ticks), cursor);

	/*
	 * XXX:casey - Ignore ticks for now.
	 */
	cursor = cftime(buffer, "(%x,%X)", &clock);

	return strdup(buffer);
}

char *
sat_protocol_token_to_text(sat_token_t token)
{
	int cursor;
	uint16_t domain;
	uint16_t protocol;

	cursor = sat_fetch_token(token, &domain, sizeof(domain), 0);
	cursor = sat_fetch_token(token, &protocol, sizeof(protocol), cursor);

	/*
	 * XXX:casey - sat_interpret did better than this.
	 */
	return combine(itox(domain), itox(protocol));
}

char *
sat_ifreq_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *ifreq;
	char *text;

	ifreq = malloc(data_size);
	sat_fetch_one_token(token, ifreq, data_size);

	text = hex(ifreq, data_size);
	free(ifreq);
	return text;
}

char *
sat_opaque_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *data;
	char *text;

	data = malloc(data_size);
	sat_fetch_one_token(token, data, data_size);

	text = hex(data, data_size);
	free(data);
	return text;
}

static char *
sat_string_token_to_text(sat_token_t token)
{
	int data_size = SAT_TOKEN_DATA_SIZE(token);
	char *text;

	text = malloc(data_size + 1);
	text[data_size] = '\0';
	sat_fetch_one_token(token, text, data_size);

	return text;
}

char *
sat_text_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_command_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_pathname_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_lookup_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_cwd_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_root_token_to_text(sat_token_t token)
{
	return sat_string_token_to_text(token);
}

char *
sat_record_header_token_to_text(sat_token_t token)
{
	int cursor;
	int32_t magic;
	sat_token_size_t size;
	int8_t rectype;
	int8_t outcome;
	int8_t sequence;
	int8_t error;
	char buffer[80];

	cursor = sat_fetch_token(token, &magic, sizeof(magic), 0);
	cursor = sat_fetch_token(token, &size, sizeof(size), cursor);
	cursor = sat_fetch_token(token, &rectype, sizeof(rectype), cursor);
	cursor = sat_fetch_token(token, &outcome, sizeof(outcome), cursor);
	cursor = sat_fetch_token(token, &sequence, sizeof(sequence), cursor);
	cursor = sat_fetch_token(token, &error, sizeof(error), cursor);

	if (error)
		sprintf(buffer, "%s,%s,(%s)", sat_eventtostr(rectype),
		    outcome ? "Success" : "Failure", strerror(error));
	else
		sprintf(buffer, "%s,%s", sat_eventtostr(rectype),
		    outcome ? "Success" : "Failure");

	return strdup(buffer);
}

char *
sat_acct_counts_token_to_text(sat_token_t token)
{
	struct acct_counts counts;
	char buffer[256];

	sat_fetch_one_token(token, &counts, sizeof(counts));

	sprintf(buffer,
	    "(mem:%lld swaps:%lld chr:%lld chw:%lld br:%lld bw:%lld syscr:%lld syscw:%lld)",
	    counts.ac_mem, counts.ac_swaps, counts.ac_chr, counts.ac_chw,
	    counts.ac_br, counts.ac_bw, counts.ac_syscr, counts.ac_syscw);

	return strdup(buffer);
}

char *
sat_acct_timers_token_to_text(sat_token_t token)
{
	struct acct_timers timers;
	char buffer[256];

	sat_fetch_one_token(token, &timers, sizeof(timers));

	sprintf(buffer,
	    "(user:%lld system:%lld blocki/o:%lld rawi/o:%lld runqwait:%lld)",
	    timers.ac_utime, timers.ac_stime, timers.ac_bwtime,
	    timers.ac_rwtime, timers.ac_qwtime);
	return strdup(buffer);
}

char *
sat_acct_proc_token_to_text(sat_token_t token)
{
	struct sat_proc_acct pa;
	char buffer[256];
	char timebuf[26];

	sat_fetch_one_token(token, &pa, sizeof(pa));

	ctime_r(&pa.sat_btime, timebuf);
	timebuf[24] = '\0';

	sprintf(buffer,
	    "(acct vers:%d flags:%x nice:%d sched:%d ash:%lld prid:%lld start:%s elapsed:%d)",
	    pa.sat_version, pa.sat_flag, pa.sat_nice, pa.sat_sched,
	    pa.sat_ash, pa.sat_prid, timebuf, pa.sat_etime);
	    
	return strdup(buffer);
}

char *
sat_acct_session_token_to_text(sat_token_t token)
{
	struct sat_session_acct sa;
	char buffer[256];
	char timebuf[26];

	sat_fetch_one_token(token, &sa, sizeof(sa));

	ctime_r(&sa.sat_btime, timebuf);
	timebuf[24] = '\0';

	sprintf(buffer,
	   "(acct vers:%d flags:%d nice:%d ash:0x%016llx prid:%lld start:%s elapsed:%d)",
	    sa.sat_version, sa.sat_flag, sa.sat_nice, sa.sat_ash, sa.sat_prid,
	    timebuf, sa.sat_etime);
	    
	return strdup(buffer);
}

char *
sat_acct_spi_token_to_text(sat_token_t token)
{
	char buffer[256];
	acct_spi_t spi;

	sat_fetch_one_token(token, &spi, sizeof(spi));

	sprintf(buffer, "(SPI:%.8s,%.8s,%.16s,%.16s)",
	    spi.spi_company, spi.spi_initiator, spi.spi_origin, spi.spi_spi);
	    
	return strdup(buffer);
}

char *
sat_acct_spi2_token_to_text(sat_token_t token)
{
	char buffer[256];
	acct_spi_2_t spi;
	sat_token_size_t size;
	int cursor;

	cursor = sat_fetch_token(token, &size, sizeof(size), 0);
	cursor = sat_fetch_token(token, &spi, size, cursor);

	sprintf(buffer, "(SPI:%.8s,%.8s,%.16s,%.16s,%.32s,%lld,%lld,%lld)",
	    spi.spi_company, spi.spi_initiator, spi.spi_origin,
	    spi.spi_spi, spi.spi_jobname, spi.spi_subtime,
	    spi.spi_exectime, spi.spi_waittime);
	    
	return strdup(buffer);
}

char *
sat_token_to_text(sat_token_t token)
{

	switch (token->token_header.sat_token_id) {
	case SAT_TOKEN_BASE:
		return sat_unknown_token_to_text(token);
	case SAT_ACL_TOKEN:
		return sat_acl_token_to_text(token);
	case SAT_BINARY_TOKEN:
		return sat_binary_token_to_text(token);
	case SAT_CAP_SET_TOKEN:
		return sat_cap_set_token_to_text(token);
	case SAT_CAP_VALUE_TOKEN:
		return sat_cap_value_token_to_text(token);
	case SAT_DESCRIPTOR_LIST_TOKEN:
		return sat_descriptor_list_token_to_text(token);
	case SAT_ERRNO_TOKEN:
		return sat_errno_token_to_text(token);
	case SAT_FILE_TOKEN:
		return sat_file_token_to_text(token);
	case SAT_GID_LIST_TOKEN:
		return sat_gid_list_token_to_text(token);
	case SAT_HOSTID_TOKEN:
		return sat_hostid_token_to_text(token);
	case SAT_IFNAME_TOKEN:
		return sat_ifname_token_to_text(token);
	case SAT_IFREQ_TOKEN:
		return sat_ifreq_token_to_text(token);
	case SAT_IP_HEADER_TOKEN:
		return sat_ip_header_token_to_text(token);
	case SAT_MAC_LABEL_TOKEN:
		return sat_mac_label_token_to_text(token);
	case SAT_MODE_TOKEN:
		return sat_mode_token_to_text(token);
	case SAT_PID_TOKEN:
		return sat_pid_token_to_text(token);
	case SAT_PORT_TOKEN:
		return sat_port_token_to_text(token);
	case SAT_PROTOCOL_TOKEN:
		return sat_protocol_token_to_text(token);
	case SAT_RECORD_HEADER_TOKEN:
		return sat_record_header_token_to_text(token);
	case SAT_SOCKADDER_TOKEN:
		return sat_sockadder_token_to_text(token);
	case SAT_SOCKET_TOKEN:
		return sat_socket_token_to_text(token);
	case SAT_SVIPC_ID_TOKEN:
		return sat_svipc_id_token_to_text(token);
	case SAT_SVIPC_KEY_TOKEN:
		return sat_svipc_key_token_to_text(token);
	case SAT_SYSARG_LIST_TOKEN:
		return sat_sysarg_list_token_to_text(token);
	case SAT_SYSCALL_TOKEN:
		return sat_syscall_token_to_text(token);
	case SAT_TEXT_TOKEN:
		return sat_text_token_to_text(token);
	case SAT_TIME_TOKEN:
		return sat_time_token_to_text(token);
	case SAT_UGID_TOKEN:
		return sat_ugid_token_to_text(token);
	case SAT_UID_LIST_TOKEN:
		return sat_uid_list_token_to_text(token);
	case SAT_SATID_TOKEN:
		return sat_satid_token_to_text(token);
	case SAT_DEVICE_TOKEN:
		return sat_device_token_to_text(token);
	case SAT_TITLED_TEXT_TOKEN:
		return sat_titled_text_token_to_text(token);
	case SAT_PATHNAME_TOKEN:
		return sat_pathname_token_to_text(token);
	case SAT_OPENMODE_TOKEN:
		return sat_openmode_token_to_text(token);
	case SAT_SIGNAL_TOKEN:
		return sat_signal_token_to_text(token);
	case SAT_STATUS_TOKEN:
		return sat_signal_token_to_text(token);
	case SAT_LOOKUP_TOKEN:
		return sat_lookup_token_to_text(token);
	case SAT_OPAQUE_TOKEN:
		return sat_opaque_token_to_text(token);
	case SAT_CWD_TOKEN:
		return sat_cwd_token_to_text(token);
	case SAT_ROOT_TOKEN:
		return sat_root_token_to_text(token);
	case SAT_PRIVILEGE_TOKEN:
		return sat_privilege_token_to_text(token);
	case SAT_PARENT_PID_TOKEN:
		return sat_pid_token_to_text(token);
	case SAT_COMMAND_TOKEN:
		return sat_command_token_to_text(token);
	case SAT_ACCT_COUNTS_TOKEN:
		return sat_acct_counts_token_to_text(token);
	case SAT_ACCT_TIMERS_TOKEN:
		return sat_acct_timers_token_to_text(token);
	case SAT_ACCT_PROC_TOKEN:
		return sat_acct_proc_token_to_text(token);
	case SAT_ACCT_SESSION_TOKEN:
		return sat_acct_session_token_to_text(token);
	case SAT_ACCT_SPI_TOKEN:
		return sat_acct_spi_token_to_text(token);
	case SAT_ACCT_SPI2_TOKEN:
		return sat_acct_spi2_token_to_text(token);
	}
	return sat_unknown_token_to_text(token);
}

char *
sat_token_name(sat_token_id_t tid)
{
	int i;
	static struct map_s {
		sat_token_id_t	id;
		char *name;
	} map[] = {
	SAT_RECORD_HEADER_TOKEN,	"RECORD_HEADER",
	SAT_IFREQ_TOKEN,		"IFREQ",
	SAT_PROTOCOL_TOKEN,		"PROTOCOL",
	SAT_TIME_TOKEN,			"TIME",
	SAT_SYSCALL_TOKEN,		"SYSCALL",
	SAT_UGID_TOKEN,			"UGID",
	SAT_FILE_TOKEN,			"FILE",
	SAT_SOCKADDER_TOKEN,		"SOCKADDER",
	SAT_IP_HEADER_TOKEN,		"IP_HEADER",
	SAT_GID_LIST_TOKEN,		"GID_LIST",
	SAT_UID_LIST_TOKEN,		"UID_LIST",
	SAT_SYSARG_LIST_TOKEN,		"SYSARG_LIST",
	SAT_DESCRIPTOR_LIST_TOKEN,	"DESCRIPTOR_LIST",
	SAT_IFNAME_TOKEN,		"IFNAME",
	SAT_SOCKET_TOKEN,		"SOCKET",
	SAT_MAC_LABEL_TOKEN,		"MAC_LABEL",
	SAT_ACL_TOKEN,			"ACL",
	SAT_CAP_VALUE_TOKEN,		"CAP_VALUE",
	SAT_CAP_SET_TOKEN,		"CAP_SET",
	SAT_TEXT_TOKEN,			"TEXT",
	SAT_SVIPC_KEY_TOKEN,		"SVIPC_KEY",
	SAT_SVIPC_ID_TOKEN,		"SVIPC_ID",
	SAT_MODE_TOKEN,			"MODE",
	SAT_PORT_TOKEN,			"PORT",
	SAT_HOSTID_TOKEN,		"HOSTID",
	SAT_BINARY_TOKEN,		"BINARY",
	SAT_PID_TOKEN,			"PID",
	SAT_ERRNO_TOKEN,		"ERRNO",
	SAT_SATID_TOKEN,		"SATID",
	SAT_DEVICE_TOKEN,		"DEVICE",
	SAT_TITLED_TEXT_TOKEN,		"TITLED_TEXT",
	SAT_PATHNAME_TOKEN,		"PATHNAME",
	SAT_OPENMODE_TOKEN,		"OPENMODE",
	SAT_SIGNAL_TOKEN,		"SIGNAL",
	SAT_STATUS_TOKEN,		"STATUS",
	SAT_LOOKUP_TOKEN,		"LOOKUP",
	SAT_OPAQUE_TOKEN,		"OPAQUE",
	SAT_CWD_TOKEN,			"CWD",
	SAT_ROOT_TOKEN,			"ROOT",
	SAT_PRIVILEGE_TOKEN,		"PRIVILEGE",
	SAT_PARENT_PID_TOKEN,		"PARENT_PID",
	SAT_COMMAND_TOKEN,		"COMMAND",
	SAT_ACCT_COUNTS_TOKEN,		"EXTACCT_COUNTS",
	SAT_ACCT_TIMERS_TOKEN,		"EXTACCT_TIMERS",
	SAT_ACCT_PROC_TOKEN,		"EXTACCT_PROC",
	SAT_ACCT_SESSION_TOKEN,		"EXTACCT_SESSION",
	SAT_ACCT_SPI_TOKEN,		"EXTACCT_SPI",
	};

	for (i = 0 ; i < sizeof(map)/sizeof(struct map_s); i++)
		if (tid == map[i].id)
			return (strdup(map[i].name));
	return (strdup("UNKNOWN"));
}
