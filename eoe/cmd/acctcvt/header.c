/*
 * acctcvt/header.c
 *	acctcvt functions for dealing with file headers
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

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <stdlib.h>
#include <strings.h>

#include "common.h"


/*
 * BuildFakeHeader
 *	Synthesizes a fake canonical header and stores it in the
 *	specified header_t. Returns 0 if successful, -1 if not.
 */
int
BuildFakeHeader(header_t *Header)
{
	char fullhostname[128];
	char *tz;
	int  varlen = 0;
	struct sat_list_ent *lep;

	/* Start with a clean slate */
	bzero(Header, sizeof(header_t));

	/* Fill in the easy parts */
	strncpy(Header->fh.sat_magic,
		SAT_FILE_MAGIC,
		sizeof(Header->fh.sat_magic));
	Header->fh.sat_major = 0;	/* "Version 0.0" means it's fake */
	Header->fh.sat_minor = 0;
	Header->fh.sat_start_time = time(0);
	Header->fh.sat_stop_time = 0L;
	Header->fh.sat_host_id = (sat_host_t) gethostid();
	Header->fh.sat_mac_enabled = 0;
	Header->fh.sat_cap_enabled = 0;
	Header->fh.sat_cipso_enabled = 0;
	Header->fh.sat_user_entries = 1;
	Header->fh.sat_group_entries = 1;
	Header->fh.sat_host_entries = 1;

	/* Set up the timezone */
	Header->tz = Header->vardata + varlen;
	tz = getenv("TZ");
	if (tz != NULL) {
		strncpy(Header->tz, tz, 128);
		Header->tz[127] = '\0';
	}
	else {
		Header->tz[0] = '\0';
	}
	Header->fh.sat_timezone_len = (int) strlen(Header->tz) + 1;
	varlen += Header->fh.sat_timezone_len;

	/* Hostname of the current machine */
	Header->hostname = Header->vardata + varlen;
	if (gethostname(Header->hostname, 128) != 0) {
		Header->hostname[0] = '\0';
	}
	Header->fh.sat_hostname_len = strlen(Header->hostname) + 1;
	varlen += Header->fh.sat_hostname_len;

	/* Domain name of the current machine */
	Header->domainname = Header->vardata + varlen;
	if (getdomainname(Header->domainname, 128) != 0) {
		Header->domainname[0] = '\0';
	}
	Header->fh.sat_domainname_len = strlen(Header->domainname) + 1;
	Header->fh.sat_domainname_len +=
	    align[(varlen + Header->fh.sat_domainname_len) % 4];
	varlen += Header->fh.sat_domainname_len;

	/* A single user entry */
	Header->users = malloc(sizeof(char *));
	if (Header->users == NULL) {
		InternalError();
		abort();
	}

	lep = (struct sat_list_ent *) (Header->vardata + varlen);
	varlen += sizeof(struct sat_list_ent);

	Header->users[0] = Header->vardata + varlen;
	strcpy(Header->users[0], "root");

	lep->sat_id = 0;
	lep->sat_name.len = (int) strlen(Header->users[0]) + 1;
	lep->sat_name.len += align[lep->sat_name.len % 4];

	varlen += lep->sat_name.len;

	/* A single group entry */
	Header->groups = malloc(sizeof(char *));
	if (Header->groups == NULL) {
		InternalError();
		abort();
	}

	lep = (struct sat_list_ent *) (Header->vardata + varlen);
	varlen += sizeof(struct sat_list_ent);

	Header->groups[0] = Header->vardata + varlen;
	strcpy(Header->groups[0], "sys");

	lep->sat_id = 0;
	lep->sat_name.len = (int) strlen(Header->groups[0]) + 1;
	lep->sat_name.len += align[lep->sat_name.len % 4];

	varlen += lep->sat_name.len;

	/* A single host entry */
	Header->hosts = malloc(sizeof(char *));
	if (Header->hosts == NULL) {
		InternalError();
		abort();
	}

	Header->hostids = malloc(sizeof(int));
	if (Header->hostids == NULL) {
		InternalError();
		abort();
	}

	strncpy(fullhostname, Header->hostname, 126);
	fullhostname[127] = '\0';
	if (strchr(fullhostname, '.') == NULL) {
		strcat(fullhostname, ".");
		strncat(fullhostname, Header->domainname, 127);
	}

	lep = (struct sat_list_ent *) (Header->vardata + varlen);
	varlen += sizeof(struct sat_list_ent);

	Header->hosts[0] = Header->vardata + varlen;
	strcpy(Header->hosts[0], fullhostname);
	Header->hostids[0] = Header->fh.sat_host_id;

	lep->sat_id = (uid_t) gethostid();
	lep->sat_name.len = (int) strlen(fullhostname) + 1;
	lep->sat_name.len += align[lep->sat_name.len % 4];

	varlen += lep->sat_name.len;

	/* Determine the final size of the header */
	Header->fh.sat_total_bytes = varlen;
	Header->fh.sat_total_bytes += align[Header->fh.sat_total_bytes % 4];

	return 0;
}


/*
 * FreeHeader
 *	Releases the resources used by a header_t
 */
void
FreeHeader(header_t *Header)
{
	if (Header == NULL) {
		return;
	}

	if (Header->users != NULL) {
		free(Header->users);
	}

	if (Header->groups != NULL) {
		free(Header->groups);
	}

	if (Header->hosts != NULL) {
		free(Header->hosts);
	}

	if (Header->hostids != NULL) {
		free(Header->hostids);
	}
}


/*
 * ParseHeader
 *	Parses the data in the specified image_t and converts it to a
 *	canonical file header. Returns 0 if successful, -1 if not.
 */
int
ParseHeader(image_t *Image, header_t *Header)
{
	/* Start off with a clean slate */
	bzero(Header, sizeof(header_t));

	/* Proceed according to the input format */
	switch (Image->format) {

	    case fmt_ea62:
	    case fmt_ea64:
	    case fmt_ea65:
		if (ParseHeader_SAT(Image, Header) != 0) {
			FreeHeader(Header);
			return -1;
		}
		break;

	    default:
		InternalError();
		FreeHeader(Header);
		return -1;
	}

	/* We are finished */
	return 0;
}


/*
 * ReadHeader
 *	Reads a file header from the specified input_t and stores it
 *	in the specified image_t. Returns 0 if successful, -1 if not.
 */
int
ReadHeader(input_t *Input, image_t *Image)
{
	/* Start off with a clean slate */
	Image->len = 0;

	/* Proceed according to the input format */
	switch (Input->format) {

	    case fmt_ea62:
	    case fmt_ea64:
	    case fmt_ea65:
		if (ReadHeader_SAT(Input, Image) != 0) {
			return -1;
		}
		break;

	    default:
		InternalError();
		return -1;
	}

	/* We are finished */
	return 0;
}


/*
 * WriteHeader
 *	Writes the specified canonical header to the specified output
 *	source.	Returns 0 if successful, -1 if not.
 */
int
WriteHeader(output_t *Output, header_t *Header)
{
	int result;

	/* Proceed according to the output format type */
	switch (Output->format) {

	    case fmt_ea62:
	    case fmt_ea64:
	    case fmt_ea65:
		result = WriteHeader_SAT(Output, Header);
		break;

	    case fmt_text:
	    case fmt_acctcom:
		result = WriteHeader_text(Output, Header);
		break;

	    default:
		InternalError();
		abort();
	}

	/* All done */
	return result;
}
