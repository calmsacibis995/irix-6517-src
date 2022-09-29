/*
 * acctcvt/extacct62.c
 *	acctcvt functions for dealing with IRIX 6.2 extended acct records
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/extacct.h>
#include <sys/sat.h>
#include <sys/sat_compat.h>

#include "common.h"



/*
 * ParseRecord_EA62
 *	Parses the specified image_t as an IRIX 6.2 extended accounting
 *	record and converts it into a canonical data record, storing the
 *	result in the specified record_t. Returns 0 if successful, -1 if
 *	not.  
*/
int
ParseRecord_EA62(const image_t *Image, record_t *Record)
{
	const char *recbufptr;
	int  recsize;
	const struct sat_hdr_2_0 *rechdr = (struct sat_hdr_2_0 *) Image->data;

	/* Validate the image */
	if (Image->format != fmt_ea62) {
		InternalError();
		return -1;
	}

	if (rechdr->sat_magic != 0x12345678) {
		if (Debug & DEBUG_INVALID) {
			fprintf(stderr,
				"%s: invalid magic number (%x)\n",
				MyName,
				rechdr->sat_magic);
		}
		return -1;
	}

	recsize = rechdr->sat_hdrsize + rechdr->sat_recsize;
	if (recsize != Image->len) {
		if (Debug & DEBUG_INVALID) {
			fprintf(stderr,
				"%s: invalid record length %d vs. %d\n",
				MyName,
				Image->len,
				recsize);
		}
		return -1;
	}

	/* Extract common values from the static part of the record header */
	Record->size		= recsize;
	Record->rectype		= rechdr->sat_rectype;
	Record->outcome		= rechdr->sat_outcome;
	Record->sequence	= rechdr->sat_sequence;
	Record->error		= rechdr->sat_errno;

	Record->clock		= rechdr->sat_time;
	Record->ticks		= rechdr->sat_ticks;

	Record->syscall		= rechdr->sat_syscall;
	Record->subsyscall	= rechdr->sat_subsyscall;

	Record->satuid		= rechdr->sat_id;
	Record->tty		= rechdr->sat_tty;
	Record->ppid		= rechdr->sat_ppid;
	Record->pid		= rechdr->sat_pid;
	Record->euid		= rechdr->sat_euid;
	Record->ruid		= rechdr->sat_ruid;
	Record->egid		= rechdr->sat_egid;
	Record->rgid		= rechdr->sat_rgid;

	recbufptr = Image->data + sizeof(struct sat_hdr_2_0);

	/* Extract the group list from the header */
	Record->numgroups = rechdr->sat_glist_len;
	if (Record->numgroups > 0) {
		Record->groups = malloc(Record->numgroups * sizeof(gid_t));
		if (Record->groups == NULL) {
			InternalError();
			abort();
		}

		bcopy(recbufptr,
		      Record->groups,
		      Record->numgroups * sizeof(gid_t));
		recbufptr += Record->numgroups * sizeof(gid_t);
	}

	/* Skip the label completely */
	recbufptr += rechdr->sat_label_size;

	/* Current working directory */
	if (rechdr->sat_cwd_len > 0) {
		Record->cwd = strdup(recbufptr);
		if (Record->cwd == NULL) {
			InternalError();
			abort();
		}
		recbufptr += rechdr->sat_cwd_len;
	}

	/* Skip the root directory */
	recbufptr += rechdr->sat_root_len;

	/* Command name */
	if (rechdr->sat_pname_len > 0) {
		Record->command = strdup(recbufptr);
		if (Record->command == NULL) {
			InternalError();
			abort();
		}
		recbufptr += rechdr->sat_pname_len;
	}

	/* Skip over any padding */
	recbufptr = Image->data + rechdr->sat_hdrsize;

	/* The rest of the record depends on the record type */
	switch (Record->rectype) {

	    case SAT_PROC_ACCT:
		bcopy(recbufptr,
		      &Record->data.p.pa,
		      sizeof(struct sat_proc_acct));
		recbufptr += sizeof(struct sat_proc_acct);

		bcopy(recbufptr,
		      &Record->timers,
		      sizeof(struct acct_timers));
		recbufptr += sizeof(struct acct_timers);

		bcopy(recbufptr,
		      &Record->counts,
		      sizeof(struct acct_counts));
		recbufptr += sizeof(struct acct_counts);

		break;

	    case SAT_SESSION_ACCT:
		bcopy(recbufptr,
		      &Record->data.s.sa,
		      sizeof(struct sat_session_acct));
		recbufptr += sizeof(struct sat_session_acct);

		Record->data.s.spifmt = 1;
		Record->data.s.spilen = sizeof(struct acct_spi);
		Record->data.s.spi = malloc(Record->data.s.spilen);
		if (Record->data.s.spi == NULL) {
			InternalError();
			abort();
		}

		bcopy(recbufptr,
		      Record->data.s.spi,
		      Record->data.s.spilen);
		recbufptr += Record->data.s.spilen;

		bcopy(recbufptr,
		      &Record->timers,
		      sizeof(struct acct_timers));
		recbufptr += sizeof(struct acct_timers);

		bcopy(recbufptr,
		      &Record->counts,
		      sizeof(struct acct_counts));
		recbufptr += sizeof(struct acct_counts);

		break;

	    case SAT_AE_CUSTOM:
		if (rechdr->sat_recsize > 0) {
			Record->custom = strdup(recbufptr);
			if (Record->custom == NULL) {
				InternalError();
				abort();
			}
			recbufptr += strlen(Record->custom) + 1;
		}
		break;

	    default:
		InternalError();
		return -1;
	}

	/* Finish with a couple of static fields */
	Record->gotcaps = 0;
	Record->gotstat = 0;
	Record->gotext  = 1;

	return 0;
}



/*
 * ReadRecord_EA62
 *	Reads an IRIX 6.2 extended accounting record from the specified
 *	input_t into the specified image_t. Returns 0 if successful, -1
 *	if not.  
 */
int
ReadRecord_EA62(input_t *Input, image_t *Image)
{
	char *recbufptr;
	int  recsize;
	struct sat_hdr_2_0 *rechdr;

	/* Read the record header */
	if (Read(Input, Image->data, sizeof(struct sat_hdr_2_0)) != 0) {
		if (!InputEOF(Input)) {
			fprintf(stderr,
				"%s: unable to read record header\n",
				MyName);
		}
		return -1;
	}
	rechdr = (struct sat_hdr_2_0 *) Image->data;

	/* Validate the record */
	if (rechdr->sat_magic != 0x12345678) {
		fprintf(stderr,
			"%s: invalid magic number (%x) - unable to continue\n",
			MyName,
			rechdr->sat_magic);
		exit(3);
	}

	/* Calculate the size of the entire record and ensure */
	/* we have room for it				      */
	Image->len = rechdr->sat_hdrsize + rechdr->sat_recsize;
	if (Image->len > MAX_IMAGE_LEN) {
		InternalError();
		abort();
	}

	/* Read the rest of the record */
	if (Read(Input,
		 Image->data + sizeof(struct sat_hdr_2_0),
		 Image->len - sizeof(struct sat_hdr_2_0)) != 0)
	{
		if (!InputEOF(Input)) {
			fprintf(stderr,
				"%s: unable to read record body\n",
				MyName);
		}
		return -1;
	}

	/* Save the record type in a convenient location */
	Image->type = rechdr->sat_rectype;

	return 0;
}


/*
 * WriteRecord_EA62
 *	Writes the specified record_t to the specified output_t in
 *	IRIX 6.2 extended accounting format. Returns 0 if successful,
 *	-1 if not.
 */
int
WriteRecord_EA62(output_t *Output, record_t *Record)
{
	char recbuf[4096];
	char *recbufptr;
	int  recsize;
	struct sat_hdr_2_0 *rechdr;

	/* Set up the record buffer */
	bzero(recbuf, 4096);
	rechdr = (struct sat_hdr_2_0 *) recbuf;
	recsize = sizeof(struct sat_hdr_2_0);
	recbufptr = recbuf + recsize;

	/* Fill in the easy parts of the record header */
	rechdr->sat_magic	= 0x12345678;
	rechdr->sat_rectype	= Record->rectype;
	rechdr->sat_outcome	= Record->outcome;
	rechdr->sat_sequence	= Record->sequence;
	rechdr->sat_errno	= Record->error;
	rechdr->sat_time	= Record->clock;
	rechdr->sat_ticks	= Record->ticks;
	rechdr->sat_syscall	= Record->syscall;
	rechdr->sat_subsyscall	= Record->subsyscall;
	rechdr->sat_host_id	= 0;	/* "only set in post-processing" */
	rechdr->sat_id		= Record->satuid;
	rechdr->sat_tty		= Record->tty;
	rechdr->sat_ppid	= Record->ppid;
	rechdr->sat_pid		= Record->pid;

	rechdr->sat_label_size	= 0;
	rechdr->sat_cwd_len	= 0;	/* not present in 3.1? */
	rechdr->sat_root_len	= 0;

	/* If owner IDs are present, use those in place of real & effective */
	if (Record->gotowner) {
		rechdr->sat_euid = Record->ouid;
		rechdr->sat_egid = Record->ogid;
		rechdr->sat_ruid = Record->ouid;
		rechdr->sat_rgid = Record->ogid;
	}
	else {
		rechdr->sat_euid = Record->euid;
		rechdr->sat_ruid = Record->ruid;
		rechdr->sat_egid = Record->egid;
		rechdr->sat_rgid = Record->rgid;
	}

	/* Set up the group list if present */
	rechdr->sat_glist_len	= Record->numgroups;
	if (rechdr->sat_glist_len > 0) {
		int listsize = rechdr->sat_glist_len * sizeof(gid_t);

		bcopy(Record->groups, recbufptr, listsize);
		recbufptr += listsize;
		recsize   += listsize;
	}

	/* Set up the process name */
	rechdr->sat_pname_len = strlen(Record->command) + 1;
	bcopy(Record->command, recbufptr, rechdr->sat_pname_len);

	recbufptr += rechdr->sat_pname_len;
	recsize   += rechdr->sat_pname_len;

	/* Set up the header size, aligning as needed */
	recsize += align[recsize % 4];
	rechdr->sat_hdrsize = recsize;

	/* Now proceed according to the record type */
	recbufptr = recbuf + recsize;
	recsize   = 0;
	switch (rechdr->sat_rectype) {

	    case SAT_PROC_ACCT:
		bcopy(&Record->data.p.pa,
		      recbufptr,
		      sizeof(struct sat_proc_acct));
		recbufptr += sizeof(struct sat_proc_acct);
		recsize   += sizeof(struct sat_proc_acct);

		bcopy(&Record->timers,
		      recbufptr,
		      sizeof(struct acct_timers));
		recbufptr += sizeof(struct acct_timers);
		recsize   += sizeof(struct acct_timers);

		bcopy(&Record->counts,
		      recbufptr,
		      sizeof(struct acct_counts));
		recbufptr += sizeof(struct acct_counts);
		recsize   += sizeof(struct acct_counts);

		break;

	    case SAT_SESSION_ACCT:
	    {
		    struct sat_session_acct sa;

		    sa = Record->data.s.sa;
		    sa.sat_version = 1;	/* tweak canonical sat_session_acct */
		    sa.sat_spilen  = 0;

		    bcopy(&sa, recbufptr, sizeof(struct sat_session_acct));

		    recbufptr += sizeof(struct sat_session_acct);
		    recsize   += sizeof(struct sat_session_acct);

		    bcopy(Record->data.s.spi,
			  recbufptr,
			  sizeof(struct acct_spi));
		    recbufptr += sizeof(struct acct_spi);
		    recsize   += sizeof(struct acct_spi);

		    bcopy(&Record->timers,
			  recbufptr,
			  sizeof(struct acct_timers));
		    recbufptr += sizeof(struct acct_timers);
		    recsize   += sizeof(struct acct_timers);

		    bcopy(&Record->counts,
			  recbufptr,
			  sizeof(struct acct_counts));
		    recbufptr += sizeof(struct acct_counts);
		    recsize   += sizeof(struct acct_counts);

		    break;
	    }

	    case SAT_AE_CUSTOM:
		rechdr->sat_recsize = strlen(Record->custom) + 1;
		bcopy(Record->custom, recbufptr, rechdr->sat_recsize);

		recbufptr += rechdr->sat_recsize;
		recsize   += rechdr->sat_recsize;

		break;

	    default:
		InternalError();
		return -1;
	}

	/* Set up the length of the data portion of the record */
	rechdr->sat_recsize = recsize + align[recsize % 4];

	/* Write the record out */
	if (Write(Output, recbuf, rechdr->sat_hdrsize + rechdr->sat_recsize)
	    != 0)
	{
		return -1;
	}

	return 0;
}
