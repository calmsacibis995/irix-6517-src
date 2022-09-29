/*
 * acctcvt/sathdr.c
 *	acctcvt functions for dealing with SAT file headers
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

#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sat.h>

#include "common.h"


/*
 * ParseHeader_SAT
 *	Parses the specified image_t for a SAT header (versions 1.2
 *	and up) and stores the result in the specified header_t.
 *	Returns 0 if successful, -1 if not.
 */
int
ParseHeader_SAT(image_t *Image, header_t *Header)
{
	int  i;
	int  major, minor;
	int  varlen;
	struct sat_list_ent *le;

	/* The static portion of the header can be used verbatim */
	bcopy(Image->data, &Header->fh, sizeof(Header->fh));

	/* Perform some sanity checks on the header data */
	if (strncmp(Header->fh.sat_magic,
		    SAT_FILE_MAGIC,
		    sizeof(SAT_FILE_MAGIC) - 1) != 0)
	{
		fprintf(stderr,
			"%s: missing or invalid file header\n",
			MyName);
		return -1;
	}

	switch (Image->format) {

	    case fmt_ea62:
	    case fmt_ea64:
		major = 3;
		minor = 1;
		break;

	    case fmt_ea65:
		major = 4;
		minor = 0;
		break;

	    default:
		InternalError();
		return -1;
	}
	if (Header->fh.sat_major != major  ||  Header->fh.sat_minor != minor) {
		fprintf(stderr,
			"%s: input file is SAT version %d.%d, version %d.%d "
			    "was expected\n",
			MyName,
			Header->fh.sat_major,
			Header->fh.sat_minor,
			major,
			minor);
		return -1;
	}

	/* Make sure there is enough space for the variable length data */
	varlen = Header->fh.sat_total_bytes - sizeof(Header->fh);
	if (varlen > MAX_DATA_LEN) {
		InternalError();
		return -1;
	}

	/* Copy the remainder of the image into the variable data area */
	bcopy(Image->data + sizeof(Header->fh), Header->vardata, varlen);

	/* Set up the easy internal pointers in the header_t */
	varlen = 0;

	Header->tz = Header->vardata + varlen;
	varlen += Header->fh.sat_timezone_len;

	Header->hostname = Header->vardata + varlen;
	varlen += Header->fh.sat_hostname_len;

	Header->domainname = Header->vardata + varlen;
	varlen += Header->fh.sat_domainname_len;

	/* Churn through the lists, saving the first entry in each */
	Header->users = malloc(Header->fh.sat_user_entries * sizeof(char *));
	if (Header->users == NULL) {
		InternalError();
		abort();
	}

	for (i = 0;  i < Header->fh.sat_user_entries;  ++i) {
		le = (struct sat_list_ent *) (Header->vardata + varlen);
		varlen += sizeof(struct sat_list_ent);

		Header->users[i] = Header->vardata + varlen;
		varlen += le->sat_name.len;
	}

	Header->groups = malloc(Header->fh.sat_group_entries * sizeof(char *));
	if (Header->groups == NULL) {
		InternalError();
		abort();
	}

	for (i = 0;  i < Header->fh.sat_group_entries;  ++i) {
		le = (struct sat_list_ent *) (Header->vardata + varlen);
		varlen += sizeof(struct sat_list_ent);

		Header->groups[i] = Header->vardata + varlen;
		varlen += le->sat_name.len;
	}

	Header->hosts = malloc(Header->fh.sat_host_entries * sizeof(char *));
	if (Header->hosts == NULL) {
		InternalError();
		abort();
	}

	Header->hostids = malloc(Header->fh.sat_host_entries * sizeof(int));
	if (Header->hostids == NULL) {
		InternalError();
		abort();
	}

	for (i = 0;  i < Header->fh.sat_host_entries;  ++i) {
		le = (struct sat_list_ent *) (Header->vardata + varlen);
		varlen += sizeof(struct sat_list_ent);

		Header->hostids[i] = le->sat_id;
		Header->hosts[i] = Header->vardata + varlen;
		varlen += le->sat_name.len;
	}

	return 0;
}


/*
 * ReadHeader_SAT
 *	Reads a SAT header (versions 1.2 and up) from the specified
 *	input source and stores it in the specified image_t. Returns
 *	0 if successful, -1 if not.
 */
int
ReadHeader_SAT(input_t *Input, image_t *Image)
{
	int varlen;
	struct sat_filehdr *fh;

	/* Read the static portion of the file header */
	if (Read(Input, Image->data, sizeof(struct sat_filehdr)) != 0) {
		fprintf(stderr,
			"%s: unable to read input file header\n",
			MyName);
		return -1;
	}

	/* Perform a brief sanity check on the header data */
	fh = (struct sat_filehdr *) Image->data;
	if (strncmp(fh->sat_magic,
		    SAT_FILE_MAGIC,
		    sizeof(SAT_FILE_MAGIC) - 1) != 0)
	{
		fprintf(stderr,
			"%s: missing or invalid file header\n",
			MyName);
		return -1;
	}

	/* Make sure there is enough space for the variable length data */
	varlen = fh->sat_total_bytes - sizeof(struct sat_filehdr);
	if (varlen > MAX_IMAGE_LEN) {
		InternalError();
		return -1;
	}

	/* Read in the remainder of the file header data */
	if (Read(Input, Image->data + sizeof(struct sat_filehdr), varlen)
	    != 0)
	{
		fprintf(stderr,
			"%s: unable to read remainder of input file header\n",
			MyName);
		return -1;
	}

	/* Save some additional info about the image */
	Image->format = Input->format;
	Image->len    = sizeof(struct sat_filehdr) + varlen;

	return 0;
}


/*
 * WriteHeader_SAT
 *	Write out a file header in SAT format. Fortunately, the only
 *	substantial difference between SAT file headers starting with
 *	version 1.2 is the version number itself, so this works for
 *	almost all versions of SAT. Returns 0 if successful, -1 if not.
 */
int
WriteHeader_SAT(output_t *Output, header_t *Header)
{
	char buf[MAX_IMAGE_LEN];
	int  buflen;
	int  i;
	int  major, minor;
	struct sat_filehdr *fh;
	struct sat_list_ent *lep;

	/* Make sure the buffer is big enough */
	if (Header->fh.sat_total_bytes > sizeof(buf)) {
		InternalError();
		abort();
	}

	/* Set up the buffer with the static portion of the header */
	bzero(buf, sizeof(buf));

	bcopy(&Header->fh, buf, sizeof(struct sat_filehdr));
	buflen = sizeof(struct sat_filehdr);

	fh = (struct sat_filehdr *) buf;

	/* Set up the details that are specific to the output format */
	switch (Output->format) {

	    case fmt_ea62:
		fh->sat_major = 3;
		fh->sat_minor = 1;

		fh->sat_cap_enabled = 0;
		fh->sat_cipso_enabled = 0;

		break;

	    case fmt_ea64:
		fh->sat_major = 3;
		fh->sat_minor = 1;

		fh->sat_cap_enabled = 1;
		fh->sat_cipso_enabled = 0;

		break;

	    case fmt_ea65:
		fh->sat_major = 4;
		fh->sat_minor = 0;

		if (fh->sat_user_entries > 1) {	/* Clamp lists to one entry? */
			fh->sat_user_entries = 1;
		}
		if (fh->sat_group_entries > 1) {
			fh->sat_group_entries = 1;
		}
		if (fh->sat_host_entries > 1) {
			fh->sat_host_entries = 1;
		}

		break;

	    default:
		InternalError();
		abort();
	}

	/* Set up the simple strings */
	if (fh->sat_timezone_len > 0) {
		bcopy(Header->tz, buf + buflen, fh->sat_timezone_len);
		buflen += fh->sat_timezone_len;
	}

	if (fh->sat_hostname_len > 0) {
		bcopy(Header->hostname, buf + buflen, fh->sat_hostname_len);
		buflen += fh->sat_hostname_len;
	}

	if (fh->sat_domainname_len > 0) {
		bcopy(Header->domainname,
		      buf + buflen,
		      fh->sat_domainname_len);
		buflen += fh->sat_domainname_len;
	}

	/* Set up the lists */
	for (i = 0;  i < fh->sat_user_entries;  ++i) {
		lep = (struct sat_list_ent *) (buf + buflen);
		buflen += sizeof(struct sat_list_ent);

		strcpy(buf + buflen, Header->users[i]);

		lep->sat_id = 0;
		lep->sat_name.len = (int) strlen(Header->users[i]) + 1;
		lep->sat_name.len += align[lep->sat_name.len % 4];

		buflen += lep->sat_name.len;
	}

	for (i = 0;  i < fh->sat_group_entries;  ++i) {
		lep = (struct sat_list_ent *) (buf + buflen);
		buflen += sizeof(struct sat_list_ent);

		strcpy(buf + buflen, Header->groups[i]);

		lep->sat_id = 0;
		lep->sat_name.len = (int) strlen(Header->groups[i]) + 1;
		lep->sat_name.len += align[lep->sat_name.len % 4];

		buflen += lep->sat_name.len;
	}

	for (i = 0;  i < fh->sat_host_entries;  ++i) {
		lep = (struct sat_list_ent *) (buf + buflen);
		buflen += sizeof(struct sat_list_ent);

		strcpy(buf + buflen, Header->hosts[i]);

		lep->sat_id = Header->hostids[i];
		lep->sat_name.len = (int) strlen(Header->hosts[i]) + 1;
		lep->sat_name.len += align[lep->sat_name.len % 4];

		buflen += lep->sat_name.len;
	}

	/* Determine the final size of the header */
	fh->sat_total_bytes = buflen;
	fh->sat_total_bytes += align[fh->sat_total_bytes % 4];

	/* Write it out to the output file */
	if (Write(Output, buf, buflen) != 0) {
		return -1;
	}

	return 0;
}
