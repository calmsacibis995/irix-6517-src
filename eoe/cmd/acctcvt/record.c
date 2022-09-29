/*
 * acctcvt/record.c
 *	acctcvt functions for dealing with canonical data records
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

#include <stdlib.h>
#include <time.h>

#include "common.h"


/*
 * FreeRecord
 *	Releases the resources used by a record_t
 */
void
FreeRecord(record_t *Record)
{
	if (Record == NULL) {
		return;
	}

	if (Record->command != NULL) {
		free(Record->command);
	}

	if (Record->custom != NULL) {
		free(Record->custom);
	}

	if (Record->cwd != NULL) {
		free(Record->cwd);
	}

	if (Record->groups != NULL) {
		free(Record->groups);
	}

	if (Record->rectype == SAT_SESSION_ACCT  &&
	    Record->data.s.spi != NULL)
	{
		free(Record->data.s.spi);
	}
}


/*
 * ParseRecord
 *	Parses the data in the specified buffer and converts it
 *	to a canonical data record, storing the results in the
 *	specified record_t. Returns 0 if succesful, -1 if not.
 */
int
ParseRecord(const image_t *Image, record_t *Record)
{
	int	 result;

	/* Start with a clean slate */
	bzero(Record, sizeof(record_t));

	/* Proceed according to the input format */
	switch (Image->format) {

	    case fmt_ea62:
		result = ParseRecord_EA62(Image, Record);
		break;

	    case fmt_ea64:
		result = ParseRecord_EA64(Image, Record);
		break;

	    case fmt_ea65:
		result = ParseRecord_EA65(Image, Record);
		break;

	    case fmt_svr4:
		result = ParseRecord_SVR4(Image, Record);
		break;

	    default:
		InternalError();
		return -1;
	}

	/* If something went wrong, give up */
	if (result != 0) {
		FreeRecord(Record);
		return -1;
	}

	return 0;
}


/*
 * ReadRecord
 *	Reads a single record from the specified input source into
 *	the specified image_t. Returns 0 if successful, -1 if not.
 */
int
ReadRecord(input_t *Input, image_t *Image)
{
	int	 result;

	/* Clean up the image_t */
	Image->len = 0;
	Image->format = Input->format;

	/* Proceed according to the input format */
	switch (Input->format) {

	    case fmt_ea62:
		result = ReadRecord_EA62(Input, Image);
		break;

	    case fmt_ea64:
		result = ReadRecord_EA64(Input, Image);
		break;

	    case fmt_ea65:
		result = ReadRecord_EA65(Input, Image);
		break;

	    case fmt_svr4:
		result = ReadRecord_SVR4(Input, Image);
		break;

	    default:
		InternalError();
		return -1;
	}

	return result;
}


/*
 * WriteRecord
 *	Writes a canonical data record to the specified output_t.
 *	Returns 0 if successful, -1 if not.
 */
int
WriteRecord(output_t *Output, record_t *Record)
{
	double  cpu;
	double  mem;
	int     result;

	cpu = ((double) Record->timers.ac_utime + Record->timers.ac_stime) /
		NSEC_PER_SEC;
	mem = KCORE(Record->counts.ac_mem / (cpu * HZ));

	if (cname && Cmatch(Record->command, cname) == NULL)
		return 0;
	if (ashcut && ashcut != Record->data.p.pa.sat_ash)
		return 0;
	if (cpucut && cpucut >= cpu)
		return 0;
	if (syscut && syscut >=
	    ((double) Record->timers.ac_stime / NSEC_PER_SEC))
		return 0;
	if (iocut && iocut >= (Record->counts.ac_chr + Record->counts.ac_chw))
		return 0;
	if (memcut && memcut >= mem)
		return 0;
	
	/* Proceed according to the output format type */
	switch (Output->format) {

	    case fmt_ea62:
		result = WriteRecord_EA62(Output, Record);
		break;

	    case fmt_ea64:
		result = WriteRecord_EA64(Output, Record);
		break;

	    case fmt_ea65:
		result = WriteRecord_EA65(Output, Record);
		break;

	    case fmt_svr4:
		result = WriteRecord_SVR4(Output, Record);
		break;

	    case fmt_text:
		result = WriteRecord_text(Output, Record);
		break;

	    case fmt_acctcom:
		result = WriteRecord_acctcom(Output, Record);
		break;
		
	    case fmt_awk:
		result = WriteRecord_awk(Output, Record);
		break;

	    default:
		InternalError();
		abort();
	}

	/* All done */
	return result;
}
