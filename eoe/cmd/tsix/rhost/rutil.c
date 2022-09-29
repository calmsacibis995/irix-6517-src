/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#include <stdlib.h>
#include <sys/types.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stropts.h>

#include <sys/mac_label.h>
#include <sys/capability.h>
#include <mls.h>

#include "rutil.h"

/*
 * Log routines.
 */

extern char * remote;
extern int debug;

void
log_errno(char *s1, char *s2)
{
	int serrno = errno;

	/*kludge_stderr();*/
	(void)fprintf(stderr, "%s: %s%s: %s\n",
		      remote, s1,s2, strerror(serrno));
	(void)fflush(stderr);
	errno = serrno;
}


void
bad_errno(char *s1, char*s2)
{
	log_errno(s1,s2);
	/*cleanup(1); */
}


void
log_debug(int lvl, char* name, char *p, char *s1, char *s2, char *s3)
{
	char pbuf[200+MAXHOSTNAMELEN+30];

	if (debug < lvl)
	    return;

	/*kludge_stderr();*/
	(void)sprintf(pbuf, "%s%s: %.200s\n", remote,name, p);
	(void)fprintf(stderr, pbuf, s1,s2,s3);
	(void)fflush(stderr);
}

/*
 *  Capability set utilities.
 */

#define MAX_CAP_BUF_SIZE	1024

/*
 * capabilities name (case insensitive) and value
 */
struct {
	char *uname, *lname;
	cap_value_t val;
} capability[] = {
{"CAP_CHOWN", "cap_chown", CAP_CHOWN},
{"CAP_DAC_WRITE", "cap_dac_write", CAP_DAC_WRITE},
{"CAP_DAC_READ_SEARCH", "cap_dac_read_search", CAP_DAC_READ_SEARCH},
{"CAP_FOWNER", "cap_fowner", CAP_FOWNER},
{"CAP_FSETID", "cap_fsetid", CAP_FSETID},
{"CAP_KILL", "cap_kill", CAP_KILL}, 
{"CAP_LINK_DIR", "cap_link_dir", CAP_LINK_DIR}, 
{"CAP_SETFPRIV", "cap_setfpriv", CAP_SETFPRIV}, 
{"CAP_SETPPRIV", "cap_setppriv", CAP_SETPPRIV},
{"CAP_SETGID", "cap_setgid", CAP_SETGID},
{"CAP_SETUID", "cap_setuid", CAP_SETUID},
{"CAP_MAC_DOWNGRADE", "cap_mac_downgrade", CAP_MAC_DOWNGRADE},
{"CAP_MAC_READ", "cap_mac_read", CAP_MAC_READ}, 
{"CAP_MAC_RELABEL_SUBJ", "cap_mac_relabel", CAP_MAC_RELABEL_SUBJ},
{"CAP_MAC_WRITE", "cap_mac_write", CAP_MAC_WRITE}, 
{"CAP_MAC_UPGRADE", "cap_mac_upgrade", CAP_MAC_UPGRADE}, 
{"CAP_INF_NOFLOAT_OBJ", "cap_inf_nofloat_obj", CAP_INF_NOFLOAT_OBJ}, 
{"CAP_INF_NOFLOAT_SUBJ", "cap_inf_nofloat_subj", CAP_INF_NOFLOAT_SUBJ},
{"CAP_INF_DOWNGRADE", "cap_inf_downgrade", CAP_INF_DOWNGRADE},
{"CAP_INF_UPGRADE", "cap_inf_upgrade", CAP_INF_UPGRADE},
{"CAP_INF_RELABEL_SUBJ", "cap_inf_relabel_subj", CAP_INF_RELABEL_SUBJ}, 
{"CAP_AUDIT_CONTROL", "cap_audit_control", CAP_AUDIT_CONTROL},
{"CAP_AUDIT_WRITE", "cap_audit_write", CAP_AUDIT_WRITE},
{"CAP_MAC_MLD", "cap_mac_mld", CAP_MAC_MLD},
{"CAP_MEMORY_MGT", "cap_memory_mgt", CAP_MEMORY_MGT},
{"CAP_SWAP_MGT", "cap_swap_mgt", CAP_SWAP_MGT},
{"CAP_TIME_MGT", "cap_time_mgt", CAP_TIME_MGT},
{"CAP_SYSINFO_MGT", "cap_sysinfo_mgt", CAP_SYSINFO_MGT}, 
{"CAP_MOUNT_MGT", "cap_mount_mgt", CAP_MOUNT_MGT},
{"CAP_QUOTA_MGT", "cap_quota_mgt", CAP_QUOTA_MGT},
{"CAP_PRIV_PORT", "cap_priv_port", CAP_PRIV_PORT},
{"CAP_STREAMS_MGT", "cap_streams_mgt", CAP_STREAMS_MGT},
{"CAP_SCHED_MGT", "cap_sched_mgt", CAP_SCHED_MGT},
{"CAP_PROC_MGT", "cap_proc_mgt", CAP_PROC_MGT},
{"CAP_SVIPC_MGT", "cap_svipc_mgt", CAP_SVIPC_MGT},
{"CAP_NETWORK_MGT", "cap_network_mgt", CAP_NETWORK_MGT},
{"CAP_DEVICE_MGT", "cap_device_mgt", CAP_DEVICE_MGT},
{"CAP_ACCT_MGT", "cap_acct_mgt", CAP_ACCT_MGT},
{"CAP_SHUTDOWN", "cap_shutdown", CAP_SHUTDOWN},
{"CAP_CHROOT", "cap_chroot", CAP_CHROOT},
{"CAP_DAC_EXECUTE", "cap_dac_execute", CAP_DAC_EXECUTE},
{"ALL", "all", CAP_ALL_ON},
{"CAP_NVRAM_MGT", "cap_nvram_mgt", CAP_SYSINFO_MGT},
{"CAP_MKNOD", "cap_mknod", CAP_DEVICE_MGT},
};

/*
 * states for text processing 
 */
#define	NAME	0
#define OP	1
#define FLAG	2

static int state = NAME;

static void
cap_error(void *b0, void *b1, int e, int l)
{

    if (b0)
        free(b0);
    if (b1)
        free(b1);
    /* printf("Error %d at %d\n",e,l); */
    setoserror(e);
}


/*
 * Translate a text form of a capability state to the internal form of a
 * capability state.
 *		text input form is 
 *			clause[ clause]...
 *		where clause is 
 * 			[cap_name[,cap_name...]]op[flags][op[flags]]... 
 */
cap_t
parse_cap(const char *buf_p)
{
	cap_t cap;
	char op, flag;
	cap_value_t	tval, val = 0;
	int i, name_expected = 0;
	char *s, *f, *bp;
	cap_value_t *flag_t;

	state=NAME;
	if (!buf_p) {
		cap_error((void *)0, (void *)0, EINVAL,1);
		return (cap_t)0;
	}

	if (!(cap = (cap_t)malloc(sizeof(struct cap_set)))) {
		cap_error((void *)0, (void *)0, ENOMEM,2);
		return (cap_t)0;
	}
	cap->cap_effective = 0;
        cap->cap_permitted = 0;
        cap->cap_inheritable = 0;
	if (!(f = bp = (char *)malloc(strlen(buf_p)))) {
		cap_error((void *)cap, (void *)0, ENOMEM,3);
		return (cap_t)0;
	}

	strcpy(bp, buf_p);

	for (;;) {
		switch(state) {
		case NAME:
			s = f;
			while (*f != '+' && *f != '-' && *f != '=' && *f != ',' && *f)
				++f;
			switch(*f) {
			case '+':
			case '-':
			case '=':
				state = OP;
				op = *f;
			case ',':
				name_expected = (name_expected || *f == ',');
				if (name_expected && s == f) {
					cap_error((void *)cap, (void *)bp, EINVAL,4);
					return (cap_t)0;
				}
				*f++ = '\0';
				for (i=0; i<=CAP_MAX_ID+2; i++) {
					if (!strcmp(s, capability[i].uname) || !strcmp(s, capability[i].lname)) {
						val += capability[i].val;
						break;
					}
				}
				if (i > CAP_MAX_ID+2) {
					cap_error((void *)cap, (void *)bp, EINVAL,5);
					return (cap_t)0;
				}
				continue;
			default:
				cap_error((void *)cap, (void *)bp, EINVAL,6);
				return (cap_t)0;
			}
		case OP:
			switch(*f) {
			case ' ':
				state = NAME;
				val = 0;
				name_expected = 0;
			case '+':
			case '-':
			case '=':
			case '\0':
				switch(op) {
				case '+':
				case '-':
					if (!val) {
						cap_error((void *)cap, (void *)bp, EINVAL,7);
						return (cap_t)0;
					}
				case '=':
					if (!(tval = val))
						tval = CAP_ALL_ON;
					cap->cap_effective &= ~tval;
					cap->cap_permitted &= ~tval;
					cap->cap_inheritable &= ~tval;
				}
				if (*f == '\0')
					goto ret;
				else {
					op = *f++;
					continue;
				}
			case 'e':
			case 'i':
			case 'p':
				state = FLAG;
				flag = *f++;
				continue;
			default:
				cap_error((void *)cap, (void *)bp, EINVAL,9);
				return (cap_t)0;
			}
		case FLAG:
			switch(flag) {
			case 'e':
				flag_t = &cap->cap_effective;
				break;
			case 'p':
				flag_t = &cap->cap_permitted;
				break;
			case 'i':
				flag_t = &cap->cap_inheritable;
			}
			switch(op) {
			case '=':
				if (!(tval = val))
					tval = CAP_ALL_ON;
				cap->cap_effective &= ~tval;
				cap->cap_permitted &= ~tval;
				cap->cap_inheritable &= ~tval;
				op = '+';
			case '+':
				*flag_t |= val;
				break;
			case '-':
				*flag_t &= ~val;
			}
			switch(*f) {
			case '+':
			case '-':
			case '=':
				state = OP;
				op = *f++;
				continue;
			case 'e':
			case 'i':
			case 'p':
				flag = *f++;
				continue;
			case ' ':
				state = NAME;
				val = 0;
				name_expected = 0;
				++f;
				continue;
			case '\0':
				goto ret;
			default:
				cap_error((void *)cap, (void *)bp, EINVAL,10);
				return (cap_t)0;
			}
		}
	}
ret:
	if (bp)
		free((void *)bp);
	return cap;
}

