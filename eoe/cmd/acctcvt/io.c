/*
 * acctcvt/io.c
 *	I/O functions for acctcvt
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
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>

#include "common.h"



/*
 * CloseInput
 *	Close an input_t and release any resources it may be holding
 */
void
CloseInput(input_t *Input)
{
	if (Input == NULL) {
		InternalError();
		return;
	}

	if (Input->unread != NULL) {
		free(Input->unread);
	}

	fclose(Input->file);

	bzero(Input, sizeof(input_t));
}


/*
 * CloseOutput
 *	Close an output_t and release any resources it may be holding
 */
void
CloseOutput(output_t *Output)
{
	if (Output == NULL) {
		InternalError();
		return;
	}

	fclose(Output->file);

	bzero(Output, sizeof(output_t));
}


/*
 * InputEOF
 *	Returns non-zero if the specified input_t is at the end-of-file,
 *	or 0 if not.
 */
int
InputEOF(input_t *Input)
{
	if (Input == NULL) {
		return 1;
	}

	if (Input->unreadlen > 0) {
		return 0;
	}

	return feof(Input->file);
}


/*
 * InputError
 *	Returns non-zero if the specified input_t is in an error state,
 *	0 if not.
 */
int
InputError(input_t *Input)
{
	if (Input == NULL) {
		return 1;
	}

	return ferror(Input->file);
}


/*
 * OpenInput
 *	Does whatever is necessary to make the specified input_t ready
 *	for reading. Returns 0 if successful, -1 if not.
 */
int
OpenInput(input_t *Input)
{
	/* Proceed according to the type of input source we are using */
	switch (Input->source) {

	    case in_file:
		/* Reading from a file */
		Input->file = fopen(Input->name, "r");
		if (Input->file == NULL) {
			fprintf(stderr,
				"%s: unable to open input file %s - %s\n",
				MyName,
				Input->name,
				strerror(errno));
			return -1;
		}
		break;

	    case in_stdin:
		/* Reading from stdin */
		Input->file = stdin;
		break;

	    default:
		/* Unknown input source */
		InternalError();
		return -1;
	}

	return 0;
}


/*
 * OpenOutput
 *	Does whatever is necessary to make the specified output_t ready
 *	for writing. Returns 0 if successful, -1 if not.
 */
int
OpenOutput(output_t *Output)
{
	/* Proceed according to the destination type */
	switch (Output->dest) {

	    case out_file:
		/* Writing to a file */
		Output->file = fopen(Output->name, "w");
		if (Output->file == NULL) {
			fprintf(stderr,
				"%s: unable to open output file %s - %s\n",
				MyName,
				Output->name,
				strerror(errno));
			exit(2);
		}
		break;

	    case out_cmd:
		/* Writing to a child process */
		Output->file = StartChildProcess(Output->name);
		if (Output->file == NULL) {
			return -1;
		}
		break;

	    case out_stdout:
		/* Writing to stdout */
		Output->file = stdout;
		break;

	    default:
		/* Unknown output destination */
		InternalError();
		return -1;
	}			    

	return 0;
}


/*
 * Print
 *	Similar to fprintf, but writes to the specified output_t.
 *	Returns 0 if successful, -1 if not.
 */
int
Print(output_t *Output, const char *Format, ...)
{
	va_list Args;

	if (Output == NULL) {
		InternalError();
		return -1;
	}

	va_start(Args, Format);
	vfprintf(Output->file, Format, Args);
	va_end(Args);

	return 0;
}


/*
 * Read
 *	Reads the specified number of bytes of data from the specified
 *	input_t into the specified buffer, honoring any previously
 *	unread data. Always reads exactly the number of bytes that
 *	is specified. Returns 0 if successful, -1 if not.
 */
int
Read(input_t *Input, void *Buffer, int TotalLength)
{
	char *CurrBuf = (char *) Buffer;
	int Len = TotalLength;
	int NumBytes;

	/* Watch for trivial cases */
	if (Input == NULL  ||  TotalLength < 1) {
		return -1;
	}

	/* If an unread buffer is present, use data from there first */
	if (Input->unreadlen > 0) {

		/* Calculate the # of bytes we can unread */
		NumBytes = TotalLength;
		if (TotalLength > Input->unreadlen) {
			NumBytes = Input->unreadlen;
		}

		/* Copy the unread bytes into the user's buffer */
		bcopy(Input->unread, Buffer, NumBytes);
		Len -= NumBytes;
		CurrBuf += NumBytes;

		/* Adjust the unread buffer */
		Input->unreadlen -= NumBytes;
		if (Input->unreadlen > 0) {
			memmove(Input->unread,
				Input->unread + NumBytes,
				Input->unreadlen);
		}

		/* If we have completely satisfied the read, we're done */
		if (Len == 0) {
			return 0;
		}
	}

	/* Satisfy the remainder of the read from the actual file */
	if (fread(CurrBuf, Len, 1, Input->file) != 1) {
		if (!feof(Input->file)) {
			fprintf(stderr,
				"%s: error on read - %s\n",
				MyName, strerror(errno));
		}
		return -1;
	}

	return 0;
}


/*
 * Unread
 *	Pushes the specified data back into the specified input_t.
 *	Returns 0 if successful, -1 if not.
 */
int
Unread(input_t *Input, void *Buffer, int Length)
{
	/* Enlarge the current unread buffer if necessary */
	if (Input->unreadlen + Length > Input->unreadbufsize) {
		Input->unreadbufsize += (Length + 100);
		Input->unread = realloc(Input->unread, Input->unreadbufsize);
		if (Input->unread == NULL) {
			InternalError();
			abort();
		}
	}

	/* Copy the new data to the end of the buffer */
	bcopy(Buffer, Input->unread + Input->unreadlen, Length);
	Input->unreadlen += Length;

	return 0;
}


/*
 * Write
 *	Writes the specified number of bytes of data from the specified
 *	buffer to the specified output_t. Returns 0 if successful, -1 if not.
 */
int
Write(output_t *Output, void *Buffer, int Length)
{

	/* Watch for trivial cases */
	if (Output == NULL  ||  Output->file == NULL  ||  Length < 1) {
		return 0;
	}

	/* stdio does most of the hard work */
	if (fwrite(Buffer, Length, 1, Output->file) != 1) {

		/* An EPIPE isn't fatal: we may be writing to two */
		/* output streams, and the second may still be OK */
		if (errno == EPIPE) {
			if (Debug & DEBUG_INVALID) {
				fprintf(stderr,
					"%s: warning - broken pipe detected\n",
					MyName);
			}

			fclose(Output->file);
			Output->file = NULL;

			return 0;
		}

		/* Any other problem *is* Bad */
		fprintf(stderr,
			"%s: error on write - %s\n",
			MyName,
			strerror(errno));

		return -1;
	}

	return 0;
}
