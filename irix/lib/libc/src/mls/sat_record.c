/*
 * Copyright 1990, 1991, 1992 Silicon Graphics, Inc. 
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
	#pragma weak sat_read_header_info = _sat_read_header_info
	#pragma weak sat_free_header_info = _sat_free_header_info
	#pragma weak sat_intrp_pathname = _sat_intrp_pathname
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sat.h>
#include <string.h>
#include <limits.h>
#include <sys/mac.h>
#include <sys/sat_compat.h>

/* word alignment stuff */
#define ALIGN(x) ((x) + align[(ptrdiff_t)(x) % 4])
static const char align[4] = { 0, 3, 2, 1 };

/*
 * Read sat tokens
 * for irix6.5 and later SAT records
 */
sat_token_t
_fread_sat_token(FILE *fp)

{
	sat_token_header_t header;
	sat_token_t token;
	int data_size;

	if (fread(&header, 1, sizeof(header), fp) != sizeof(header))
		return NULL;

	token = (sat_token_t)malloc(header.sat_token_size);
	token->token_header = header;
	data_size = (int)header.sat_token_size - (int)sizeof(header);
	
	if (fread(&token->token_data, 1, data_size, fp) != data_size) {
		free(token);
		return NULL;
	}

	return token;
}

/*
 * Read a record header.
 */
int
sat_read_header_info(FILE *in, struct sat_hdr_info *hinfo,
		     int mask, int file_major, int file_minor)
{
	int	struct_size;
	int	lblsize;
	int	capsize = 0;
	int	cwd_len;
	int	root_len;
	int	pname_len;
	char	*origbuf, *buf;

	hinfo->sat_groups = NULL;
	hinfo->sat_plabel = NULL;
	hinfo->sat_pcap = NULL;
	hinfo->sat_pname = NULL;
	hinfo->sat_cwd = NULL;
	hinfo->sat_rootdir = NULL;
	hinfo->sat_buffer = NULL;

	if (file_major == 1 && file_minor == 0) {
		struct sat_hdr_1_0 head_1_0;

		memset((void *) &head_1_0, '\0', sizeof(head_1_0));

		fread(&head_1_0, sizeof(head_1_0), 1, in);
		if (ferror(in) || feof(in))
			return SFI_ERROR;
		struct_size = sizeof(head_1_0);

		hinfo->sat_magic = head_1_0.sat_magic;
		hinfo->sat_rectype = head_1_0.sat_rectype;
		hinfo->sat_outcome = head_1_0.sat_outcome;
		hinfo->sat_sequence = head_1_0.sat_sequence;
		hinfo->sat_errno = head_1_0.sat_errno;
		hinfo->sat_time = head_1_0.sat_time;
		hinfo->sat_ticks = head_1_0.sat_ticks;
		hinfo->sat_syscall = head_1_0.sat_syscall;
		hinfo->sat_subsyscall = head_1_0.sat_subsyscall;
		hinfo->sat_host_id = 0L;
		hinfo->sat_id = head_1_0.sat_id;
		hinfo->sat_tty = head_1_0.sat_tty;
		hinfo->sat_ppid = head_1_0.sat_ppid;
		hinfo->sat_pid = head_1_0.sat_pid;
		hinfo->sat_euid = head_1_0.sat_euid;
		hinfo->sat_ruid = head_1_0.sat_ruid;
		hinfo->sat_egid = head_1_0.sat_egid;
		hinfo->sat_rgid = head_1_0.sat_rgid;
		hinfo->sat_recsize = head_1_0.sat_recsize;
		hinfo->sat_ngroups = head_1_0.sat_glist_len;
		hinfo->sat_hdrsize = head_1_0.sat_hdrsize;
		lblsize = head_1_0.sat_label_size;
		cwd_len = head_1_0.sat_cwd_len;
		root_len = head_1_0.sat_root_len;
		pname_len = head_1_0.sat_pname_len;

		origbuf = buf = malloc(head_1_0.sat_hdrsize);
		if (! origbuf)
			return SFI_ERROR;
		if (mask & SHI_BUFFER) {
			hinfo->sat_buffer = origbuf;
			memcpy(buf, &head_1_0, struct_size);
			buf += struct_size;
		}
	} else if (file_major == 1 && file_minor > 0 && file_minor <= 2) {
		struct sat_hdr_1_1 head_1_1;

		memset((void *) &head_1_1, '\0', sizeof(head_1_1));

		fread(&head_1_1, sizeof(head_1_1), 1, in);
		if (ferror(in) || feof(in))
			return SFI_ERROR;
		struct_size = sizeof(head_1_1);

		hinfo->sat_magic = head_1_1.sat_magic;
		hinfo->sat_rectype = head_1_1.sat_rectype;
		hinfo->sat_outcome = head_1_1.sat_outcome;
		hinfo->sat_sequence = head_1_1.sat_sequence;
		hinfo->sat_errno = head_1_1.sat_errno;
		hinfo->sat_time = head_1_1.sat_time;
		hinfo->sat_ticks = head_1_1.sat_ticks;
		hinfo->sat_syscall = head_1_1.sat_syscall;
		hinfo->sat_subsyscall = head_1_1.sat_subsyscall;
		hinfo->sat_host_id = head_1_1.sat_host_id;
		hinfo->sat_id = head_1_1.sat_id;
		hinfo->sat_tty = head_1_1.sat_tty;
		hinfo->sat_ppid = head_1_1.sat_ppid;
		hinfo->sat_pid = head_1_1.sat_pid;
		hinfo->sat_euid = head_1_1.sat_euid;
		hinfo->sat_ruid = head_1_1.sat_ruid;
		hinfo->sat_egid = head_1_1.sat_egid;
		hinfo->sat_rgid = head_1_1.sat_rgid;
		hinfo->sat_recsize = head_1_1.sat_recsize;
		hinfo->sat_ngroups = head_1_1.sat_glist_len;
		hinfo->sat_hdrsize = head_1_1.sat_hdrsize;
		lblsize = head_1_1.sat_label_size;
		cwd_len = head_1_1.sat_cwd_len;
		root_len = head_1_1.sat_root_len;
		pname_len = head_1_1.sat_pname_len;

		origbuf = buf = malloc(head_1_1.sat_hdrsize);
		if (! origbuf)
			return SFI_ERROR;
		if (mask & SHI_BUFFER) {
			hinfo->sat_buffer = origbuf;
			memcpy(buf, &head_1_1, struct_size);
			buf += struct_size;
		}
        } else if (file_major >= 2 && file_major < 4) {
		struct sat_hdr head;

		memset((void *) &head, '\0', sizeof(head));

		fread(&head, sizeof(head), 1, in);
		if (ferror(in) || feof(in))
			return SFI_ERROR;
		struct_size = sizeof(head);

		hinfo->sat_magic = head.sat_magic;
		hinfo->sat_rectype = head.sat_rectype;
		hinfo->sat_outcome = head.sat_outcome;
		hinfo->sat_cap = head.sat_cap;
		hinfo->sat_sequence = head.sat_sequence;
		hinfo->sat_errno = head.sat_errno;
		hinfo->sat_time = head.sat_time;
		hinfo->sat_ticks = head.sat_ticks;
		hinfo->sat_syscall = head.sat_syscall;
		hinfo->sat_subsyscall = head.sat_subsyscall;
		hinfo->sat_host_id = head.sat_host_id;
		hinfo->sat_id = head.sat_id;
		hinfo->sat_tty = head.sat_tty;
		hinfo->sat_ppid = head.sat_ppid;
		hinfo->sat_pid = head.sat_pid;
		hinfo->sat_euid = head.sat_euid;
		hinfo->sat_ruid = head.sat_ruid;
		hinfo->sat_egid = head.sat_egid;
		hinfo->sat_rgid = head.sat_rgid;
		hinfo->sat_recsize = head.sat_recsize;
		hinfo->sat_ngroups = head.sat_glist_len;
		hinfo->sat_hdrsize = head.sat_hdrsize;
		lblsize = head.sat_label_size;
		cwd_len = head.sat_cwd_len;
		root_len = head.sat_root_len;
		pname_len = head.sat_pname_len;
		capsize = head.sat_cap_size;

		origbuf = buf = malloc(head.sat_hdrsize);
		if (! origbuf)
			return SFI_ERROR;
		if (mask & SHI_BUFFER) {
			hinfo->sat_buffer = origbuf;
			memcpy(buf, &head, struct_size);
			buf += struct_size;
		}
        } else if (file_major == 4 && file_minor == 0) {
		sat_token_t token;
		int	cursor;
		int	goteids;
		int	gotrids;
		int	total_recsize = 0;
		int	accum_recsize = 0;
		char	tokbuf[4];
		void*	p;

		while (token = _fread_sat_token(in)) {
		    if ((total_recsize != 0)
			 && (accum_recsize >= total_recsize)) {
			break;
		    }
		    switch (token->token_header.sat_token_id) {
		    case SAT_RECORD_HEADER_TOKEN:
			goteids = 0;	/* reset uid/gid counters */
			gotrids = 0;

			/* Copy record header fields */
			accum_recsize = token->token_header.sat_token_size;
			/* record magic number */
			cursor = 0;
			p = &token->token_data[cursor];
			hinfo->sat_magic = *(int32_t*)p;
			/* total record size */
			cursor += sizeof(int32_t);
			p = &token->token_data[cursor];
			hinfo->sat_recsize = total_recsize
					   = *(sat_token_size_t*)p;
			/* record type */
			cursor += sizeof(sat_token_size_t);
			p = &token->token_data[cursor];
			hinfo->sat_rectype = *(uint8_t*)p;
			/* event outcome */
			cursor += sizeof(uint8_t);
			p = &token->token_data[cursor];
			hinfo->sat_outcome = *(uint8_t*)p;
			/* sequence number */
			cursor += sizeof(uint8_t);
			p = &token->token_data[cursor];
			hinfo->sat_sequence = *(uint8_t*)p;
			/* record errno number */
			cursor += sizeof(uint8_t);
			p = &token->token_data[cursor];
			hinfo->sat_errno = *(uint8_t*)p;
			/* record header size */
			cursor += sizeof(uint8_t);
			hinfo->sat_hdrsize = cursor;
			/* copy original record to buffer if flag is set */
			if (mask & SHI_BUFFER) {
			    origbuf = malloc(cursor);
			    if (! origbuf) {
				return SFI_ERROR;
			    }
			    hinfo->sat_buffer = origbuf;
			    memcpy(origbuf, &token->token_header, cursor);
			}
			break;

		    case SAT_COMMAND_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_PNAME) != 0) {
			    hinfo->sat_pname = strdup(&token->token_data[0]);
			}
			break;

		    case SAT_ERRNO_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_errno = *(uint8_t*)p;
			break;

		    case SAT_TIME_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			cursor = 0;
			p = &token->token_data[cursor];
			hinfo->sat_time = *(time_t*)p;
			cursor += sizeof(time_t);
			p = &token->token_data[cursor];
			hinfo->sat_ticks = *(uint8_t*)p;
			break;

		    case SAT_SYSCALL_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			cursor = 0;
			p = &token->token_data[cursor];
			hinfo->sat_syscall = *(uint8_t*)p;
			cursor += sizeof(uint8_t);
			/* must copy subsyscall data into properly aligned
			 * buffer before doing type conversion	*/
			memcpy(&tokbuf[0], &token->token_data[cursor],
				sizeof(uint16_t));
			p = &tokbuf;
			hinfo->sat_subsyscall = *(uint16_t*)p;
			break;

		    case SAT_HOSTID_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_host_id = *(uint32_t*)p;
			break;

		    case SAT_SATID_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_id = *(uid_t*)p;
			break;

		    case SAT_PORT_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_tty = *(int32_t*)p;
			break;

		    case SAT_PARENT_PID_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_ppid = *(pid_t*)p;
			break;

		    case SAT_PID_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_pid = *(pid_t*)p;
			break;

		    case SAT_UGID_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if (gotrids) {
			    /* Third or later SAT_UGID_TOKEN
			     * - not in struct sat_hdr_info */
			} else if (goteids) {
			    /* Second SAT_UGID_TOKEN */
			    gotrids = 1;
			    cursor = 0;
			    p = &token->token_data[cursor];
			    hinfo->sat_ruid = *(uid_t*)p;
			    cursor += sizeof(uid_t);
			    p = &token->token_data[cursor];
			    hinfo->sat_rgid = *(gid_t*)p;
			} else {
			    /* First SAT_UGID_TOKEN */
			    goteids = 1;
			    cursor = 0;
			    p = &token->token_data[cursor];
			    hinfo->sat_euid = *(uid_t*)p;
			    cursor += sizeof(uid_t);
			    p = &token->token_data[cursor];
			    hinfo->sat_egid = *(gid_t*)p;
			}
			break;

		    case SAT_UID_LIST_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			cursor = 0;
			p = &token->token_data[cursor];
			hinfo->sat_euid = *(uid_t*)p;
			cursor += sizeof(uid_t);
			p = &token->token_data[cursor];
			hinfo->sat_ruid = *(uid_t*)p;
			break;

		    case SAT_GID_LIST_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_GROUPS) != 0) {
			    int i;
			    p = &token->token_header.sat_token_size;
			    hinfo->sat_ngroups = (int)((*(sat_token_size_t*)p
						    - sizeof(sat_token_header_t))
						 / sizeof(gid_t));
			    hinfo->sat_groups = (gid_t *)malloc(sizeof(gid_t)
							 * hinfo->sat_ngroups);
			    if (hinfo->sat_groups) {
				cursor = 0;
				for (i=0; i < hinfo->sat_ngroups; i++) {
				    p = &token->token_data[cursor];
				    hinfo->sat_groups[i] = *(gid_t*)p;
				    cursor += sizeof(gid_t);
				}
			    }
			}
			break;

		    case SAT_CWD_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_CWD) != 0) {
			    hinfo->sat_cwd = strdup(&token->token_data[0]);
			}
			break;

		    case SAT_ROOT_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_PNAME) != 0) {
			    hinfo->sat_rootdir = strdup(&token->token_data[0]);
			}
			break;

		    case SAT_MAC_LABEL_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_PLABEL) != 0) {
			    hinfo->sat_plabel = malloc(lblsize);
			    if (hinfo->sat_plabel) {
				cursor = 0;
				p = &token->token_data[cursor];
				lblsize = *(uint16_t*)p;
				cursor += sizeof(uint16_t);
				memcpy(hinfo->sat_plabel,
					&token->token_data[cursor],lblsize);
			    }
			}
			break;

		    case SAT_CAP_SET_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			if ((mask & SHI_CAP) != 0) {
			    hinfo->sat_pcap = malloc(sizeof(cap_set_t));
			    if (hinfo->sat_pcap) {
				memcpy(hinfo->sat_pcap, &token->token_data[0],
					sizeof(cap_set_t));
			    }
			}
			break;

		    case SAT_CAP_VALUE_TOKEN:
			accum_recsize += token->token_header.sat_token_size;
			p = &token->token_data[0];
			hinfo->sat_cap = *(cap_value_t*)p;
			break;

		    default:
			accum_recsize += token->token_header.sat_token_size;
			break;
		    }
		}	/* end of while */
		free (token);
		return SFI_OKAY;
	} else {
                fprintf(stderr,
                    "Error: Don't know how to decode version %d.%d!\n",
                    file_major, file_minor);
                return SFI_ERROR;
	}

	/* read the variable-length portion of the header */

	fread(buf, hinfo->sat_hdrsize - struct_size, 1, in);
	if (ferror(in) || feof(in))
		return SFI_ERROR;

	if ((mask & SHI_GROUPS) && hinfo->sat_ngroups) {
		hinfo->sat_groups =
			(gid_t *) malloc(sizeof(gid_t) * hinfo->sat_ngroups);
		if (hinfo->sat_groups) {
			int i;
			if (file_major == 1) {
				for (i=0; i < hinfo->sat_ngroups; i++) {
					hinfo->sat_groups[i] =
						((o_gid_t *)buf)[i];
				}
			} else {
				for (i=0; i < hinfo->sat_ngroups; i++) {
					hinfo->sat_groups[i] =
						((gid_t *)buf)[i];
				}
			}
		}
	}
	if (file_major == 1)
		buf += sizeof(o_gid_t) * hinfo->sat_ngroups;
	else
		buf += sizeof(gid_t) * hinfo->sat_ngroups;

	if ((mask & SHI_PLABEL) && lblsize) {
		hinfo->sat_plabel = malloc(lblsize);
		if (hinfo->sat_plabel)
			memcpy(hinfo->sat_plabel, buf, lblsize);
	}
	buf += lblsize;

	if ((mask & SHI_CWD) && cwd_len) {
		hinfo->sat_cwd = strdup(buf);
	}
	buf += cwd_len;

	if ((mask & SHI_ROOTDIR) && root_len) {
		hinfo->sat_rootdir = strdup(buf);
	}
	buf += root_len;

	if ((mask & SHI_PNAME) && pname_len) {
		hinfo->sat_pname = strdup(buf);
	}
	buf += pname_len;

	if ((mask & SHI_CAP) && capsize) {
		hinfo->sat_pcap = malloc(capsize);
		if (hinfo->sat_pcap)
			memcpy(hinfo->sat_pcap, buf, capsize);
	}
	buf += capsize;

	if (! (mask & SHI_BUFFER))
		free(origbuf);

	return SFI_OKAY;
}


/*
 * Free a record header.
 */
void
sat_free_header_info(struct sat_hdr_info *hinfo)
{
	if (hinfo->sat_groups != NULL) {
		free(hinfo->sat_groups);
	}
	if (hinfo->sat_plabel != NULL) {
		free(hinfo->sat_plabel);
	}
	if (hinfo->sat_pcap != NULL) {
		free(hinfo->sat_pcap);
	}
	if (hinfo->sat_pname != NULL) {
		free(hinfo->sat_pname);
	}
	if (hinfo->sat_cwd != NULL) {
		free(hinfo->sat_cwd);
	}
	if (hinfo->sat_rootdir != NULL) {
		free(hinfo->sat_rootdir);
	}
	if (hinfo->sat_buffer != NULL) {
		free(hinfo->sat_buffer);
	}
	memset((void *) hinfo, '\0', sizeof(*hinfo));
}


/*
 * Read a pathname struct.
 */
/* ARGSUSED */
int
sat_intrp_pathname(char **buf, struct sat_pathname *pn, char **reqname,
		   char **actname, mac_t *label, int file_major,
		   int file_minor)
{   
	size_t size;
	size_t lblsize;
	char *lbl;

	if (file_major == 1) {
		struct sat_pathname_1_0 *pn_1_0;
		pn_1_0 = (struct sat_pathname_1_0 *)(*buf);
		size = sizeof(struct sat_pathname_1_0);

		pn->sat_inode = pn_1_0->sat_inode;
		pn->sat_device = pn_1_0->sat_device;
		pn->sat_fileown = pn_1_0->sat_fileown;
		pn->sat_filegrp = pn_1_0->sat_filegrp;
		pn->sat_filemode = pn_1_0->sat_filemode;
		pn->sat_reqname_len = pn_1_0->sat_reqname_len;
		pn->sat_actname_len = pn_1_0->sat_actname_len;
	} else if (file_major == 2) {
			struct sat_pathname_2_1 *pn_2_1;
			pn_2_1 = (struct sat_pathname_2_1 *)(*buf);
			size = sizeof(struct sat_pathname_2_1);
	
			pn->sat_inode = pn_2_1->sat_inode;
			pn->sat_device = pn_2_1->sat_device;
			pn->sat_fileown = pn_2_1->sat_fileown;
			pn->sat_filegrp = pn_2_1->sat_filegrp;
			pn->sat_filemode = pn_2_1->sat_filemode;
			pn->sat_reqname_len = pn_2_1->sat_reqname_len;
			pn->sat_actname_len = pn_2_1->sat_actname_len;
		}
		else {
			size = sizeof(struct sat_pathname);
			memcpy(pn, *buf, size);
		}

	if (pn->sat_reqname_len > PATH_MAX ||
	    pn->sat_actname_len > 2*PATH_MAX) {
		fprintf(stderr, "Error!  bad pathname length!\n");
		if (reqname) (*reqname) = NULL;
		if (actname) (*actname) = NULL;
		if (label) (*label) = NULL;
		return -1;
	}

	if (reqname) {
		char *name = (*buf) + size;
		(*reqname) = malloc(pn->sat_reqname_len + 1);
		if (*reqname) {
			strncpy((*reqname), name, pn->sat_reqname_len);
			(*reqname)[pn->sat_reqname_len] = '\0';
		}
	}

	if (actname) {
		char *name = (*buf) + size + pn->sat_reqname_len;
		(*actname) = malloc(pn->sat_actname_len + 1);
		if (*actname) {
			strncpy((*actname), name, pn->sat_actname_len);
			(*actname)[pn->sat_actname_len] = '\0';
		}
	}

	lbl = (*buf) + size + pn->sat_reqname_len + pn->sat_actname_len;
	lblsize = mac_size((mac_t) ALIGN(lbl));
	if (label) {
		if (lblsize) {
			(*label) = (mac_t) malloc(lblsize);
			if (*label)
				memcpy(*label, ALIGN(lbl), lblsize);
		} else
			(*label) = NULL;
	}

	(*buf) += ALIGN(size + pn->sat_reqname_len + pn->sat_actname_len
		+ lblsize);
	return 0;
}
