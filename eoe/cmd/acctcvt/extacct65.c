/*
 * acctcvt/extacct65.c
 *	acctcvt functions for dealing with IRIX 6.5 extended acct records
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

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/extacct.h>
#include <sys/sat.h>

#include "common.h"


/* Internal function prototypes */
static int  ReadToken(input_t *, void *, int);
static void WriteToken(image_t *, void *, int, sat_token_id_t);


/*
 * ParseRecord_EA65
 *	Parses the specified image_t as an IRIX 6.5 extended
 *	accounting record and converts it into a canonical data
 *	record, storing the result in the specified record_t. Returns
 *	0 if successful, -1 if not.  
 */
int
ParseRecord_EA65(const image_t *Image, record_t *Record)
{
	int  cursor;
	int  goteids = 0;
	int  gotrids = 0;
	int  gotoids = 0;
	int32_t magic;
	sat_token_t token;

	/* Make sure a record header is present */
	token = (sat_token_t) Image->data;
	if (token->token_header.sat_token_id != SAT_RECORD_HEADER_TOKEN) {
		fprintf(stderr,
			"%s: record does not start with header (id=%d)\n",
			MyName,
			token->token_header.sat_token_id);
		return -1;
	}

	/* Copy over the interesting parts of the record header */
	cursor = sizeof(sat_token_header_t);

	bcopy(Image->data + cursor, &magic, sizeof(int32_t));
	cursor += sizeof(int32_t);

	bcopy(Image->data + cursor, &Record->size, sizeof(sat_token_size_t));
	cursor += sizeof(sat_token_size_t);

	bcopy(Image->data + cursor, &Record->rectype, sizeof(uint8_t));
	cursor += sizeof(uint8_t);

	bcopy(Image->data + cursor, &Record->outcome, sizeof(uint8_t));
	cursor += sizeof(uint8_t);

	bcopy(Image->data + cursor, &Record->sequence, sizeof(uint8_t));
	cursor += sizeof(uint8_t);

	bcopy(Image->data + cursor, &Record->error, sizeof(uint8_t));
	cursor += sizeof(uint8_t);

	/* Validate the header data */
	if (magic != SAT_RECORD_MAGIC) {
		if (Debug & DEBUG_INVALID) {
			fprintf(stderr,
				"skipping bogus record with magic number "
				    "0x%08x\n",
				magic);
		}
		return -1;
	}

	if (Record->rectype != Image->type) {
		InternalError();
		return -1;
	}

	/* Sift through each of the remaining tokens until we reach */
	/* the end of the data image				    */
	while (cursor < Image->len) {
		const char *tokendata;
		sat_token_header_t tokenhdr;

		/* Copy the next token into properly aligned storage */
		bcopy(Image->data + cursor,
		      &tokenhdr,
		      sizeof(sat_token_header_t));

		/* Update our pointers */
		tokendata = Image->data + cursor + sizeof(sat_token_header_t);
		cursor += tokenhdr.sat_token_size;

		/* Proceed according to the token type */
		switch (tokenhdr.sat_token_id) {

		    case SAT_TIME_TOKEN:
			bcopy(tokendata,
			      &Record->clock,
			      sizeof(time_t));
			bcopy(tokendata + sizeof(time_t),
			      &Record->ticks,
			      sizeof(uint8_t));
			break;

		    case SAT_SYSCALL_TOKEN:
			bcopy(tokendata,
			      &Record->syscall,
			      sizeof(uint8_t));
			bcopy(tokendata + sizeof(uint8_t),
			      &Record->subsyscall,
			      sizeof(uint16_t));
			break;

		    case SAT_SATID_TOKEN:
			bcopy(tokendata, &Record->satuid, sizeof(uid_t));
			break;

		    case SAT_COMMAND_TOKEN:
			if (tokendata[0] != '\0') {
				Record->command = strdup(tokendata);
				if (Record->command == NULL) {
					InternalError();
					abort();
				}
			}
			break;

		    case SAT_CWD_TOKEN:
			if (tokendata[0] != '\0') {
				Record->cwd = strdup(tokendata);
				if (Record->cwd == NULL) {
					InternalError();
					abort();
				}
			}
			break;

		    case SAT_DEVICE_TOKEN:
			bcopy(tokendata, &Record->tty, sizeof(dev_t));
			break;

		    case SAT_PARENT_PID_TOKEN:
			bcopy(tokendata, &Record->ppid, sizeof(pid_t));
			break;

		    case SAT_PID_TOKEN:
			bcopy(tokendata, &Record->pid, sizeof(pid_t));
			break;

		    case SAT_UGID_TOKEN:
			if (gotoids) {
				/* Fourth or later SAT_UGID_TOKEN */
				if (Debug & DEBUG_INVALID) {
					fprintf(stderr,
						"%s: ignoring extra UGID "
						    "token in record type "
						    "%d\n",
						MyName,
						Record->rectype);
				}
			}
			else if (gotrids) {
				/* Third SAT_UGID_TOKEN */
				gotoids = 1;
				Record->gotowner = 1;
				bcopy(tokendata,
				      &Record->ouid,
				      sizeof(uid_t));
				bcopy(tokendata + sizeof(uid_t),
				      &Record->ogid,
				      sizeof(gid_t));
			}
			else if (goteids) {
				/* Second SAT_UGID_TOKEN */
				gotrids = 1;
				bcopy(tokendata,
				      &Record->ruid,
				      sizeof(uid_t));
				bcopy(tokendata + sizeof(uid_t),
				      &Record->rgid,
				      sizeof(gid_t));
			}
			else {
				/* First SAT_UGID_TOKEN */
				goteids = 1;
				bcopy(tokendata,
				      &Record->euid,
				      sizeof(uid_t));
				bcopy(tokendata + sizeof(uid_t),
				      &Record->egid,
				      sizeof(gid_t));
			}
			break;

		    case SAT_GID_LIST_TOKEN:
		    {
			    int listlen;

			    listlen = (tokenhdr.sat_token_size
				       - sizeof(sat_token_header_t));

			    Record->numgroups = listlen / sizeof(gid_t);
			    Record->groups = malloc(listlen);
			    if (Record->groups == NULL) {
				    InternalError();
				    abort();
			    }

			    bcopy(tokendata, Record->groups, listlen);

			    break;
		    }

		    case SAT_CAP_SET_TOKEN:
			bcopy(tokendata, &Record->caps, sizeof(cap_set_t));
			Record->gotcaps = 1;
			break;

		    case SAT_ACCT_COUNTS_TOKEN:
			bcopy(tokendata,
			      &Record->counts,
			      sizeof(struct acct_counts));
			break;

		    case SAT_ACCT_TIMERS_TOKEN:
			bcopy(tokendata,
			      &Record->timers,
			      sizeof(struct acct_timers));
			break;

		    case SAT_ACCT_PROC_TOKEN:
			if (Record->rectype == SAT_PROC_ACCT) {
				bcopy(tokendata,
				      &Record->data.p.pa,
				      sizeof(struct sat_proc_acct));
			}
			else {
				if (Debug & DEBUG_INVALID) {
					fprintf(stderr,
						"%s: ignoring PROC_ACCT token "
						    "in record type %d\n",
						MyName,
						Record->rectype);
				}
			}
			break;

		    case SAT_ACCT_SESSION_TOKEN:
			if (Record->rectype == SAT_SESSION_ACCT) {
				bcopy(tokendata,
				      &Record->data.s.sa,
				      sizeof(struct sat_session_acct));
			}
			else {
				if (Debug & DEBUG_INVALID) {
					fprintf(stderr,
						"%s: ignoring SESSION_ACCT "
						    "token in record type "
						    "%d\n",
						MyName,
						Record->rectype);
				}
			}
			break;

		    case SAT_ACCT_SPI_TOKEN:
			if (Record->rectype == SAT_SESSION_ACCT) {
				Record->data.s.spilen =
				    sizeof(struct acct_spi);
				Record->data.s.spifmt = 1;

				Record->data.s.spi =
				    malloc(Record->data.s.spilen);
				if (Record->data.s.spi == NULL) {
					InternalError();
					abort();
				}

				bcopy(tokendata,
				      Record->data.s.spi,
				      Record->data.s.spilen);
			}
			else {
				if (Debug & DEBUG_INVALID) {
					fprintf(stderr,
						"%s: ignoring ACCT_SPI "
						    "token in record type "
						    "%d\n",
						MyName,
						Record->rectype);
				}
			}
			break;

		    case SAT_ACCT_SPI2_TOKEN:
			if (Record->rectype == SAT_SESSION_ACCT) {
				bcopy(tokendata,
				      &Record->data.s.spilen,
				      sizeof(sat_token_size_t));

				Record->data.s.spifmt = 2;

				Record->data.s.spi =
				    malloc(Record->data.s.spilen);
				if (Record->data.s.spi == NULL) {
					InternalError();
					abort();
				}

				bcopy(tokendata + sizeof(sat_token_size_t),
				      Record->data.s.spi,
				      Record->data.s.spilen);
			}
			else {
				if (Debug & DEBUG_INVALID) {
					fprintf(stderr,
						"%s: ignoring ACCT_SPI2 "
						    "token in record type "
						    "%d\n",
						MyName,
						Record->rectype);
				}
			}
			break;

		    case SAT_TEXT_TOKEN:
			if (Debug & DEBUG_RECORD) {
				fprintf(stderr, "%s: SAT_TEXT_TOKEN token "
					"type %d for record type %d = '%s'\n",
					MyName, tokenhdr.sat_token_id,
					Record->rectype, tokendata);
			}
			if (tokendata[0] != '\0') {
				Record->custom = strdup(tokendata);
				if (Record->custom == NULL) {
					InternalError();
					abort();
				}
			}
			break;

		    case SAT_PRIVILEGE_TOKEN:
			bcopy(tokendata, &Record->priv, sizeof(cap_value_t));
			bcopy(tokendata + sizeof(cap_value_t),
			      &Record->privhow,
			      sizeof(uint8_t));
			Record->gotpriv = 1;
			break;

		    case SAT_ROOT_TOKEN:
			/* Gracefully ignore this token. */
			break;
			
		    default:
			if (Debug & DEBUG_UNKNOWN) {
				fprintf(stderr,
					"%s: unrecognized token type (%d, %02x)"
						" for record type %d\n",
					MyName,
					tokenhdr.sat_token_id,
					tokenhdr.sat_token_id,
					Record->rectype);
			}
			break;
		}
	}

	/* Finish with a couple of static fields */
	Record->gotstat = 0;
	Record->gotext  = 1;

	/* All done with this record */
	return 0;
}


/*
 * ReadRecord_EA65
 *	Reads an IRIX 6.5 extended accounting record from the
 *	specified input_t into the specified image_t. Returns 0 if
 *	successful, -1 if not.
 */
int
ReadRecord_EA65(input_t *Input, image_t *Image)
{
	char tokenbuf[MAX_IMAGE_LEN];
	sat_token_header_t *tokenhdr = (sat_token_header_t *) tokenbuf;
	sat_token_size_t   size;
	sat_token_t token = (sat_token_t) tokenbuf;
	uint8_t rectype;

	/* The first token we read should be a record header */
	if (ReadToken(Input, tokenbuf, sizeof(tokenbuf)) != 0) {
		return -1;
	}
	if (tokenhdr->sat_token_id != SAT_RECORD_HEADER_TOKEN) {
		fprintf(stderr,
			"%s: next record has no header (id=%d) - unable to "
			    "continue\n",
			MyName,
			tokenhdr->sat_token_id);
		return -1;
	}

	/* Save the record header token in the record buffer */
	bcopy(tokenbuf, Image->data, tokenhdr->sat_token_size);
	Image->len = tokenhdr->sat_token_size;

	/* Extract the record size and type for future reference */
	bcopy(token->token_data + sizeof(int32_t),
	      &size,
	      sizeof(sat_token_size_t));
	bcopy(token->token_data + sizeof(int32_t) + sizeof(sat_token_size_t),
	      &rectype,
	      sizeof(rectype));
	Image->type = rectype;

	/* Be verbose if desired */
	if (Debug & DEBUG_TOKEN) {
		fprintf(stderr,
			"\nHEADER Type %d   Length %d  (Token %d)\n",
			Image->type,
			size,
			tokenhdr->sat_token_size);
	}

	/* Read and process additional tokens until we come across */
	/* another record header				   */
	while (ReadToken(Input, tokenbuf, sizeof(tokenbuf)) == 0) {

		/* If we have encountered another record header, */
		/* unread it and quit the loop			 */
		if (tokenhdr->sat_token_id == SAT_RECORD_HEADER_TOKEN) {
			if (Unread(Input, tokenbuf, tokenhdr->sat_token_size)
			    != 0)
			{
				InternalError();
				abort();
			}
			break;
		}

		/* Make sure we haven't grown too large */
		if (Image->len + tokenhdr->sat_token_size > MAX_IMAGE_LEN) {
			InternalError();
			abort();
		}

		/* Copy the token to the record buffer */
		bcopy(tokenbuf,
		      Image->data + Image->len,
		      tokenhdr->sat_token_size);

		/* Be verbose if desired */
		if (Debug & DEBUG_TOKEN) {
			fprintf(stderr,
				"    TOKEN %02x  Length %-3d  Offset %d\n",
				tokenhdr->sat_token_id,
				tokenhdr->sat_token_size,
				Image->len);
		}

		/* Increment end of image counter */
		Image->len += tokenhdr->sat_token_size;
	}

	/* If the input_t is in an error state, we aborted due to trouble */
	if (InputError(Input)) {
		return -1;
	}

	/* All done with this record */
	return 0;
}


/*
 * WriteRecord_EA65
 *	Writes the specified record_t to the specified output_t as an
 *	IRIX 6.5 extended accounting record.  Returns 0 if successful,
 *	-1 if not.
 */
int
WriteRecord_EA65(output_t *Output, record_t *Record)
{
	char tokenbuf[MAX_IMAGE_LEN];
	image_t image;
	int sizeoffset;
	int tokenlen;
	int32_t magic;
	sat_token_size_t size = 0;

	/* Initialize the image_t */
	image.len = 0;

	/* Slog through all of the standard tokens */

	/* SAT_RECORD_HEADER_TOKEN */
	magic = SAT_RECORD_MAGIC;
	bcopy(&magic, tokenbuf, sizeof(int32_t));
	tokenlen = sizeof(int32_t);

	sizeoffset = tokenlen;		/* Save size for very last */
	bcopy(&size,			/* Just write a dummy for now */
	      tokenbuf + tokenlen,	/* (to make purify happy)     */
	      sizeof(size));
	tokenlen += sizeof(Record->size);

	bcopy(&Record->rectype,
	      tokenbuf + tokenlen,
	      sizeof(Record->rectype));
	tokenlen += sizeof(Record->rectype);

	bcopy(&Record->outcome,
	      tokenbuf + tokenlen,
	      sizeof(Record->outcome));
	tokenlen += sizeof(Record->outcome);

	bcopy(&Record->sequence,
	      tokenbuf + tokenlen,
	      sizeof(Record->sequence));
	tokenlen += sizeof(Record->sequence);

	bcopy(&Record->error,
	      tokenbuf + tokenlen,
	      sizeof(Record->error));
	tokenlen += sizeof(Record->error);

	WriteToken(&image, tokenbuf, tokenlen, SAT_RECORD_HEADER_TOKEN);

	/* SAT_TIME_TOKEN */
	bcopy(&Record->clock, tokenbuf, sizeof(Record->clock));
	tokenlen = sizeof(Record->clock);

	bcopy(&Record->ticks, tokenbuf + tokenlen, sizeof(Record->ticks));
	tokenlen += sizeof(Record->ticks);

	WriteToken(&image, tokenbuf, tokenlen, SAT_TIME_TOKEN);

	/* SAT_SYSCALL_TOKEN */
	bcopy(&Record->syscall, tokenbuf, sizeof(Record->syscall));
	tokenlen = sizeof(Record->syscall);

	bcopy(&Record->subsyscall,
	      tokenbuf + tokenlen,
	      sizeof(Record->subsyscall));
	tokenlen += sizeof(Record->subsyscall);

	WriteToken(&image, tokenbuf, tokenlen, SAT_SYSCALL_TOKEN);

	/* SAT_SATID_TOKEN */
	WriteToken(&image,
		   &Record->satuid,
		   sizeof(Record->satuid),
		   SAT_SATID_TOKEN);

	/* SAT_COMMAND_TOKEN */
	if (Record->command != NULL) {
		WriteToken(&image,
			   Record->command,
			   strlen(Record->command) + 1,
			   SAT_COMMAND_TOKEN);
	}

	/* SAT_CWD_TOKEN */
	if (Record->cwd != NULL) {
		WriteToken(&image,
			   Record->cwd,
			   strlen(Record->cwd) + 1,
			   SAT_CWD_TOKEN);
	}

	/* SAT_DEVICE_TOKEN */
	WriteToken(&image,
		   &Record->tty,
		   sizeof(Record->tty),
		   SAT_DEVICE_TOKEN);

	/* SAT_PARENT_PID_TOKEN */
	WriteToken(&image,
		   &Record->ppid,
		   sizeof(Record->ppid),
		   SAT_PARENT_PID_TOKEN);

	/* SAT_PID_TOKEN */
	WriteToken(&image,
		   &Record->pid,
		   sizeof(Record->pid),
		   SAT_PID_TOKEN);

	/* SAT_UGID_TOKEN (effective IDs) */
	bcopy(&Record->euid, tokenbuf, sizeof(uid_t));
	tokenlen = sizeof(uid_t);

	bcopy(&Record->egid, tokenbuf + tokenlen, sizeof(gid_t));
	tokenlen += sizeof(gid_t);

	WriteToken(&image, tokenbuf, tokenlen, SAT_UGID_TOKEN);

	/* SAT_UGID_TOKEN (real IDs) */
	bcopy(&Record->ruid, tokenbuf, sizeof(uid_t));
	tokenlen = sizeof(uid_t);

	bcopy(&Record->rgid, tokenbuf + tokenlen, sizeof(gid_t));
	tokenlen += sizeof(gid_t);

	WriteToken(&image, tokenbuf, tokenlen, SAT_UGID_TOKEN);

	/* SAT_GID_LIST_TOKEN */
	if (Record->numgroups > 0) {
		WriteToken(&image,
			   Record->groups,
			   Record->numgroups * sizeof(gid_t),
			   SAT_GID_LIST_TOKEN);
	}

	/* SAT_CAP_SET_TOKEN */
	if (Record->gotcaps) {
		WriteToken(&image,
			   &Record->caps,
			   sizeof(Record->caps),
			   SAT_CAP_SET_TOKEN);
	}

	/* Now handle tokens that are specific to the record type */
	switch (Record->rectype) {

	    case SAT_PROC_ACCT:

		/* SAT_ACCT_PROC_TOKEN */
		WriteToken(&image,
			   &Record->data.p.pa,
			   sizeof(struct sat_proc_acct),
			   SAT_ACCT_PROC_TOKEN);
		break;

	    case SAT_SESSION_ACCT:

		/* SAT_ACCT_SESSION_TOKEN */
		WriteToken(&image,
			   &Record->data.s.sa,
			   sizeof(struct sat_session_acct),
			   SAT_ACCT_SESSION_TOKEN);

		if (Record->data.s.spi == NULL) {
			break;
		}

		/* SAT_UGID_TOKEN */
		if (Record->gotowner) {
			bcopy(&Record->ouid, tokenbuf, sizeof(uid_t));
			tokenlen = sizeof(uid_t);

			bcopy(&Record->ogid,
			      tokenbuf + tokenlen,
			      sizeof(gid_t));
			tokenlen += sizeof(gid_t);

			WriteToken(&image, tokenbuf, tokenlen, SAT_UGID_TOKEN);
		}

		switch (Record->data.s.spifmt) {

		    case 1:

			/* SAT_ACCT_SPI_TOKEN */
			WriteToken(&image,
				   Record->data.s.spi,
				   sizeof(struct acct_spi),
				   SAT_ACCT_SPI_TOKEN);
			break;

		    case 2:

			/* SAT_ACCT_SPI2_TOKEN */
			bcopy(&Record->data.s.spilen,
			      tokenbuf,
			      sizeof(sat_token_size_t));
			tokenlen = sizeof(sat_token_size_t);

			bcopy(Record->data.s.spi,
			      tokenbuf + tokenlen,
			      Record->data.s.spilen);
			tokenlen += Record->data.s.spilen;

			WriteToken(&image,
				   tokenbuf,
				   tokenlen,
				   SAT_ACCT_SPI2_TOKEN);

			break;

		    default:
			InternalError();
			return -1;
		}

		break;

	    case SAT_AE_CUSTOM:
		if (Record->custom != NULL) {
			WriteToken(&image,
				   Record->custom,
				   strlen(Record->custom) + 1,
				   SAT_TEXT_TOKEN);
		}

		break;

	    default:
		InternalError();
		return -1;
	}

	/* Remaining tokens are common to both accounting record types */
	if (Record->rectype != SAT_AE_CUSTOM) {

		/* SAT_ACCT_TIMERS_TOKEN */
		WriteToken(&image,
		   	&Record->timers,
		   	sizeof(Record->timers),
		   	SAT_ACCT_TIMERS_TOKEN);

		/* SAT_ACCT_COUNTS_TOKEN */
		WriteToken(&image,
		   	&Record->counts,
		   	sizeof(Record->counts),
		   	SAT_ACCT_COUNTS_TOKEN);
	}

	/* SAT_PRIVILEGE_TOKEN */
	if (Record->gotpriv) {
		bcopy(&Record->priv, tokenbuf, sizeof(cap_value_t));
		tokenlen = sizeof(cap_value_t);

		bcopy(&Record->privhow, tokenbuf + tokenlen, sizeof(uint8_t));
		tokenlen += sizeof(uint8_t);

		WriteToken(&image, tokenbuf, tokenlen, SAT_PRIVILEGE_TOKEN);
	}

	/* Now that we know the length of the entire record, */
	/* go back and add it to the record header	     */
	size = (sat_token_size_t) image.len;
	bcopy(&size,
	      image.data + sizeof(sat_token_header_t) + sizeoffset,
	      sizeof(sat_token_size_t));

	/* Finally, we can write the record to the output_t */
	if (Write(Output, image.data, image.len) != 0) {
		return -1;
	}

	return 0;
}


/****************************************************************/
/*			PRIVATE FUNCTIONS			*/
/****************************************************************/


/*
 * ReadToken
 *	Reads a SAT 4.0 token from the specified input_t into the
 *	specified buffer, up to some maximum number of bytes. 0 is
 *	returned if successful, -1 if not.
 */
static int
ReadToken(input_t *Input, void *Buffer, int MaxLen)
{
	int data_size;
	sat_token_t token = (sat_token_t) Buffer;

	/* Make sure there is enough room for at least a header */
	if (MaxLen < sizeof(sat_token_header_t)) {
		InternalError();
		abort();
	}

	/* Read in the token header first */
	if (Read(Input, &token->token_header, sizeof(sat_token_header_t)) != 0)
	{
		if (!InputEOF(Input)) {
			fprintf(stderr,
				"%s: unable to read token header\n",
				MyName);
		}
		return -1;
	}

	/* Make sure there is enough room for the rest of the token */
	if (token->token_header.sat_token_size > MaxLen) {
		fprintf(stderr,
			"%s: token too large - size=%d max=%d\n",
			MyName,
			token->token_header.sat_token_size,
			MaxLen);
		InternalError();
		abort();
	}

	/* Read the rest of the raw data */
	data_size = (token->token_header.sat_token_size
		     - sizeof(sat_token_header_t));
	/* data_size may be 0 because it is possible that a token doesn't have
	   any data; a token may only have the token header.  An example is
	   SAT_LOOKUP_TOKEN - the Lookup Name field may be null.
	*/
	if ((data_size > 0) &&
	    Read(Input, &token->token_data, data_size) != 0) {
		fprintf(stderr,
			"%s: unable to read token body\n",
			MyName);
		return -1;
	}

	return 0;
}


/*
 * WriteToken
 *	Writes the specified data out as a SAT 4.0 token to
 *	the specified image_t, using the specified token ID.
 */
static void
WriteToken(image_t *Image, void *Buffer, int Length, sat_token_id_t ID)
{
	sat_token_header_t hdr;

	hdr.sat_token_id = ID;
	hdr.sat_token_size = Length + sizeof(sat_token_header_t);

	bcopy(&hdr, Image->data + Image->len, sizeof(sat_token_header_t));
	Image->len += sizeof(sat_token_header_t);

	bcopy(Buffer, Image->data + Image->len, Length);
	Image->len += Length;
}
