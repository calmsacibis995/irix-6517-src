/*
 * Copyright 1993, Silicon Graphics, Inc.
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
	#pragma weak cap_clear = _cap_clear
	#pragma weak cap_init = _cap_init
	#pragma weak cap_from_text = _cap_from_text
	#pragma weak cap_to_text = _cap_to_text
	#pragma weak cap_value_to_text = _cap_value_to_text
	#pragma weak cap_size = _cap_size
	#pragma weak cap_copy_ext = _cap_copy_ext
	#pragma weak cap_copy_int = _cap_copy_int
	#pragma weak cap_free = _cap_free
	#pragma weak cap_get_fd = _cap_get_fd
	#pragma weak cap_get_file = _cap_get_file
	#pragma weak cap_get_proc = _cap_get_proc
	#pragma weak cap_set_fd = _cap_set_fd
	#pragma weak cap_set_file = _cap_set_file
	#pragma weak cap_set_proc = _cap_set_proc
	#pragma weak cap_get_flag = _cap_get_flag
	#pragma weak cap_set_flag = _cap_set_flag
	#pragma weak cap_dup = _cap_dup
	#pragma weak capabilities = _capabilities
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/capability.h>
#include <sys/syssgi.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_CAP_BUF_SIZE	1024

/*
 * capabilities name (case insensitive) and value
 */
struct {
	char *uname, *lname;
	cap_value_t val;
} capabilities[] = {
{"CAP_CHOWN", "cap_chown", CAP_CHOWN},
{"CAP_DAC_WRITE", "cap_dac_write", CAP_DAC_WRITE},
{"CAP_DAC_READ_SEARCH", "cap_dac_read_search", CAP_DAC_READ_SEARCH},
{"CAP_FOWNER", "cap_fowner", CAP_FOWNER},
{"CAP_FSETID", "cap_fsetid", CAP_FSETID},
{"CAP_KILL", "cap_kill", CAP_KILL}, 
{"CAP_LINK_DIR", "cap_link_dir", CAP_LINK_DIR}, 
{"CAP_SETFCAP", "cap_setfcap", CAP_SETFCAP}, 
{"CAP_SETPCAP", "cap_setpcap", CAP_SETPCAP},
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
{"CAP_MAC_RELABEL_OPEN", "cap_mac_relabel_open", CAP_MAC_RELABEL_OPEN},
{"CAP_SIGMASK", "cap_sigmask", CAP_SIGMASK},
{"CAP_XTCB", "cap_xtcb", CAP_XTCB},
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

static void
cap_error(void *b0, void *b1, int e)
{

    if (b0)
        free(b0);
    if (b1)
        free(b1);
    setoserror(e);
}

/*
 * Initialize a capability state in working storage so that all capability
 * flags for all capabilities defined by this implementation are cleared.
 */
int
cap_clear (cap_t cap_p)
{
	if (cap_p == (cap_t) NULL)
	{
		setoserror (EINVAL);
		return (-1);
	}

	cap_p->cap_effective = 0;
	cap_p->cap_permitted = 0;
	cap_p->cap_inheritable = 0;

	return (0);
}

/*
 * Create a capability state in working storage and return a pointer to the
 * capability state. The initial value of all flags for all capabilities
 * defined by this implementation are cleared.
 */
cap_t
cap_init (void)
{
	cap_t cap;

	cap = (cap_t) malloc (sizeof (*cap));
	if (cap == (cap_t) NULL)
	{
		setoserror (ENOMEM);
		return (cap);
	}

	cap_clear (cap);
	return (cap);
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
cap_from_text(const char *buf_p)
{
	cap_t cap;
	char op, flag;
	cap_value_t tval, val = 0;
	int i, name_expected = 0, state = NAME;
	char *s, *f, *bp;
	cap_value_t *flag_t;

	if (!buf_p) {
		cap_error((void *)0, (void *)0, EINVAL);
		return (cap_t)0;
	}

	if (!(cap = (cap_t)malloc(sizeof(*cap)))) {
		cap_error((void *)0, (void *)0, ENOMEM);
		return (cap_t)0;
	}
	cap->cap_effective = 0;
        cap->cap_permitted = 0;
        cap->cap_inheritable = 0;
	if (!(f = bp = (char *)malloc(strlen(buf_p)+1))) {
		cap_error((void *)cap, (void *)0, ENOMEM);
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
					cap_error((void *)cap, (void *)bp, EINVAL);
					return (cap_t)0;
				}
				*f++ = '\0';
				for (i=0; i<=CAP_MAX_ID+2; i++) {
					if (!strcmp(s, capabilities[i].uname) || !strcmp(s, capabilities[i].lname)) {
						val += capabilities[i].val;
						break;
					}
				}
				if (i > CAP_MAX_ID+2) {
					cap_error((void *)cap, (void *)bp, EINVAL);
					return (cap_t)0;
				}
				continue;
			default:
				cap_error((void *)cap, (void *)bp, EINVAL);
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
						cap_error((void *)cap, (void *)bp, EINVAL);
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
				cap_error((void *)cap, (void *)bp, EINVAL);
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
				cap_error((void *)cap, (void *)bp, EINVAL);
				return (cap_t)0;
			}
		}
	}
ret:
	if (bp)
		free((void *)bp);
	return cap;
}

/*
 * Translate an internal form of a capability state to the text form of a
 * capability state.
 *	output text format is
 *  all= capname+[e][i][p] capname+[e][i][p] ...
 */
char *
cap_to_text(cap_t cap_p, size_t *len_p)
{

	char *buf, *c, eop, pop, iop;
	int i, esum, psum, isum;
	cap_value_t etmp, ptmp, itmp;
	

	if (!cap_p) {
		cap_error((void *)0, (void *)0, EINVAL);
		return (char *)0;
	}

	if (!(c = buf = (char *)malloc(MAX_CAP_BUF_SIZE))) {
		cap_error((void *)0, (void *)0, ERANGE);
		return (char *)0;
	}

	strcpy(c, "all=");
	c += strlen("all=");

	esum = psum = isum = 0;
	etmp = cap_p->cap_effective;
	ptmp = cap_p->cap_permitted;
	itmp = cap_p->cap_inheritable;

	for (i=0; i<CAP_MAX_ID; i++) {
		etmp = etmp >> 1;
		ptmp = ptmp >> 1;
		itmp = itmp >> 1;
		if (etmp & 1)
			esum++;
		if (ptmp & 1)
			psum++;
		if (itmp & 1)
			isum++;
	}
	
	if (esum > CAP_MAX_ID/2) {
		etmp = ~cap_p->cap_effective;
		eop = '-';
		*c++ = 'e';
	}
	else {
		etmp = cap_p->cap_effective;
		eop = '+';
	}
	if (psum > CAP_MAX_ID/2) {
		ptmp = ~cap_p->cap_permitted;
		pop = '-';
		*c++ = 'p';
	}
	else {
		ptmp = cap_p->cap_permitted;
		pop = '+';
	}
	if (isum > CAP_MAX_ID/2) {
		itmp = ~cap_p->cap_inheritable;
		iop = '-';
		*c++ = 'i';
	}
	else {
		itmp = cap_p->cap_inheritable;
		iop = '+';
	}
	
	*c++ = ' ';

	for (i=0; i<CAP_MAX_ID; i++) {
		etmp = etmp >> 1;
		ptmp = ptmp >> 1;
		itmp = itmp >> 1;
		if ((etmp & 1) || (ptmp & 1) || (itmp & 1)) {
			char prev_op;

			strcpy(c, capabilities[i].uname);
			c += strlen(capabilities[i].uname);
			prev_op = ' ';
			if (etmp & 1) {
				*c++ = eop;
				*c++ = 'e';
				prev_op = eop;
			}
			if (ptmp & 1) {
				if (prev_op != pop)
					*c++ = pop; 
				*c++ = 'p';
				prev_op = pop;
			}
			if (itmp & 1) {
				if (prev_op != iop)
					*c++ = iop;
				*c++ = 'i';
			}
			*c++ = ' ';
		}
	}
	*--c = '\0';
	if (len_p)
		*len_p = (int)(c-buf); 
	return buf;
}

char *
cap_value_to_text (cap_value_t capv)
{
	int i;
	char *cp;
	char *result = NULL;

	if (capv == CAP_ALL_ON)
		return strdup("all");

	for (i = 0; i < CAP_MAX_ID; i++) {
		if ((capv & capabilities[i].val) == 0)
			continue;
		if (result) {
			cp = malloc(strlen(result) + 
			    strlen(capabilities[i].uname) + 2);
			*cp = '\0';
			strcat(cp, result);
			strcat(cp, ",");
			strcat(cp, capabilities[i].uname);
			free(result);
			result = cp;
		}
		else {
			result = malloc( strlen(capabilities[i].uname) + 1);
			*result = '\0';
			strcat(result, capabilities[i].uname);
		}
	}
	if (result)
		return result;
	return strdup("");
}

ssize_t
cap_size(cap_t cap_p)
{
        if (!cap_p) {
                setoserror(EINVAL);
                return (ssize_t)(-1);
        }
        return ((ssize_t) sizeof (*cap_p));
}

/*
 * For now, the internal and external CAP are the same.
 */
ssize_t
cap_copy_ext(void *buf_p, cap_t cap, ssize_t size)
{

        if (size <= 0 || !cap || !buf_p) {
		cap_error((void *)0, (void *)0, EINVAL);
                return (ssize_t)-1;
        }
        if (size < sizeof(struct cap_set)) {
                cap_error((void *)0, (void *)0, ERANGE);
                return (ssize_t)-1;
        }

        *(cap_t)buf_p = *cap;
        return (sizeof(cap_t));
}

/*
 * For now, the internal and external CAP are the same.
 */
cap_t
cap_copy_int(const void *buf_p)
{
        cap_t cap;

        if (!buf_p) {
                cap_error((void *)0, (void *)0, EINVAL);
                return (cap_t)0;
        }
        if (!(cap = (cap_t)malloc(sizeof(*cap)))) {
                cap_error((void *)0, (void *)0, ENOMEM);
                return (cap_t)0;
        }

        *cap = *(cap_t)buf_p;
        return cap;
}

int
cap_free(void *obj_p)
{
        if (obj_p)
                free(obj_p);
        return 0;
}

/*
 * Get the capability state of an open file.
 */
cap_t
cap_get_fd(int fd)
{
        cap_t cap;

        if (!(cap = malloc(sizeof(*cap)))) {
                setoserror(ENOMEM);
                return (cap_t)0;
        }

        if (syssgi(SGI_CAP_GET, 0, fd, cap) == -1) {
                free((void *)cap);
                return (cap_t)0;
        }
        else
                return cap;
}

/*
 * Get the capability state of a file.
 */
cap_t
cap_get_file(const char *path)
{
        cap_t cap;

        if (!(cap = malloc(sizeof(*cap)))) {
                setoserror(ENOMEM);
                return (cap_t)0;
        }

        if (syssgi(SGI_CAP_GET, path, -1, cap) == -1) {
                free((void *)cap);
                return (cap_t)0;
        }
        else
                return cap;
}

/*
 * Obtain the current process capability state.
 */
cap_t
cap_get_proc(void)
{
	cap_t cap;

        if (!(cap = malloc(sizeof(*cap)))) {
                setoserror(ENOMEM);
                return (cap_t)0;
        }

	if (syssgi(SGI_PROC_ATTR_GET, SGI_CAP_PROCESS, cap) == -1) {
		free((void *)cap);
		return (cap_t)0;
	}
	else 
		return cap;
}

/*
 * Set the capability state of an open file.
 */
int
cap_set_fd(int fd, cap_t cap)
{

	if (!cap) {
		setoserror(EINVAL);
		return (-1);
	}

        if (syssgi(SGI_CAP_SET, 0, fd, cap) == -1)
                return (-1);
        else
                return 0;
}

/*
 * Set the capability state of a file.
 */
int
cap_set_file(const char *path, cap_t cap)
{

	if (!cap) {
		setoserror(EINVAL);
		return (-1);
	}

        if (syssgi(SGI_CAP_SET, path, -1, cap) == -1)
                return (-1);
        else
                return 0;
}

/*
 * Set the process capability state.
 */
int
cap_set_proc(cap_t cap)
{

	if (!cap) {
		setoserror(EINVAL);
		return (-1);
	}

        if (syssgi(SGI_PROC_ATTR_SET, SGI_CAP_PROCESS, cap, sizeof(struct cap_set)) == -1)
		return (-1);
	else
		return 0;
}

#define FLAGBAD(X) \
	(X != CAP_EFFECTIVE && X != CAP_PERMITTED && X != CAP_INHERITABLE)
#define VALUEBAD(X) \
	(X != CAP_CLEAR && X != CAP_SET)

/*
 * Get the value of a capability flag
 */
int
cap_get_flag(cap_t cap_p, cap_value_t cap, cap_flag_t flag, cap_flag_value_t *value_p)
{
	int i;

	if (!cap_p || !value_p || FLAGBAD(flag)) {
		setoserror(EINVAL);
		return (-1);
	}

        for (i=0; i<CAP_MAX_ID; i++) {
		if (cap == capabilities[i].val) {
			switch(flag) {
			case CAP_EFFECTIVE:
				*value_p = (cap_p->cap_effective & cap) ? CAP_SET : CAP_CLEAR;
				return 0;
			case CAP_PERMITTED:
				*value_p = (cap_p->cap_permitted & cap) ? CAP_SET : CAP_CLEAR;
				return 0;
			case CAP_INHERITABLE:
				*value_p = (cap_p->cap_inheritable & cap) ? CAP_SET : CAP_CLEAR;
				return 0;
			}
		}
	}
	setoserror(EINVAL);
	return(-1);
}
		
/*
 * Set the value of a capability flag
 */
int
cap_set_flag(cap_t cap_p, cap_flag_t flag, int ncap, cap_value_t caps[], cap_flag_value_t value)
{
	int i, j;
	cap_set_t cap_tmp;

	if (!cap_p || ncap < 0 || !caps || VALUEBAD(value) || FLAGBAD(flag)) {
		setoserror(EINVAL);
		return (-1);
	}
	cap_tmp = *cap_p;

	for (j = 0; j < ncap; j++) {
	        for (i=0; i<CAP_MAX_ID; i++) {
       	         	if (caps[j] == capabilities[i].val) {
				switch(flag) {
				case CAP_EFFECTIVE:
					if (value == CAP_SET)
						cap_tmp.cap_effective |= capabilities[i].val;
					else
						cap_tmp.cap_effective &= ~capabilities[i].val;
					break;
				case CAP_PERMITTED:
					if (value == CAP_SET)
						cap_tmp.cap_permitted |= capabilities[i].val;
					else
						cap_tmp.cap_permitted &= ~capabilities[i].val;
					break;
				case CAP_INHERITABLE:
					if (value == CAP_SET)
						cap_tmp.cap_inheritable |= capabilities[i].val;
					else
						cap_tmp.cap_inheritable &= ~capabilities[i].val;
					break;
				}
				goto next_set_flag;
			}
		}
		setoserror(EINVAL);
		return (-1);
next_set_flag:
		continue;
	}
	*cap_p = cap_tmp;
	return 0;
}

cap_t
cap_dup (cap_t cap)
{
    cap_t dup = (cap_t) NULL;

    if (cap != (cap_t) NULL)
    {
	dup = (cap_t) malloc (sizeof (*cap));
	if (dup != (cap_t) NULL)
	    *dup = *cap;
	else
	    setoserror (ENOMEM);
    }
    else
	setoserror (EINVAL);
    return (dup);
}
