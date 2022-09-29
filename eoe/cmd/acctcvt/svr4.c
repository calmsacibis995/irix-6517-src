/*
 * acctcvt/svr4.c
 *	acctcvt functions for dealing with SVR4 accounting records
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/acct.h>
#include <sys/extacct.h>
#include <sys/param.h>


#include "common.h"


/* Internal function prototypes */
static comp_t  Compress(accum_t);
static accum_t Expand(comp_t);

#define MAX_SVR4_VALUE 17177772032L



/*
 * ParseRecord_SVR4
 *	Parses the specified image_t as an SVR4 record and converts it
 *	into a canonical data record, storing the result in the
 *	specified record_t. Returns 0 if successful, -1 if not.
 */
int
ParseRecord_SVR4(const image_t *Image, record_t *Record)
{
	char buf[10];
	struct acct *acctrec = (struct acct *) Image->data;

	/* Unfortunately, there is very little we can do in the way */
	/* of sanity-checking the image, other than ensuring that   */
	/* it is for SVR4 (as opposed to SVR3.X)		    */
	if (!(acctrec->ac_flag & AEXPND)) {
		fprintf(stderr,
			"%s: invalid accounting record - AEXPND not set\n",
			MyName);
		return -1;
	}

	/* Build a record_t as best we can. We assume the record_t */
	/* has already been bzero'ed.				   */
	Record->size	= sizeof(struct acct);
	Record->rectype	= Image->type;

	strncpy(buf, acctrec->ac_comm, 8);
	buf[8] = '\0';
	Record->command = strdup(buf);
	if (Record->command == NULL) {
		InternalError();
		abort();
	}

	Record->tty	= acctrec->ac_tty;
	Record->ruid	= acctrec->ac_uid;
	Record->rgid	= acctrec->ac_gid;
	Record->status	= acctrec->ac_stat;

	Record->gotcaps = 0;
	Record->gotstat = 1;
	Record->gotext  = 0;

	Record->timers.ac_utime = Expand(acctrec->ac_utime) * TICK;
	Record->timers.ac_stime = Expand(acctrec->ac_stime) * TICK;

	Record->counts.ac_mem   = Expand(acctrec->ac_mem);
	Record->counts.ac_chr	= Expand(acctrec->ac_io);
	Record->counts.ac_br	= Expand(acctrec->ac_rw);

	if (acctrec->ac_flag & AFORK) {
		Record->data.p.pa.sat_flag |= SPASF_FORK;
	}
	if (acctrec->ac_flag & ASU) {
		Record->data.p.pa.sat_flag |= SPASF_SU;
	}

	Record->data.p.pa.sat_btime = acctrec->ac_btime;
	Record->data.p.pa.sat_etime = acctrec->ac_etime;

	return 0;
}


/*
 * ReadRecord_SVR4
 *	Reads an SVR4 record from the specified input_t into the
 *	specified image_t. Returns 0 if successful, -1 if not.
 */
int
ReadRecord_SVR4(input_t *Input, image_t *Image)
{
	Image->len  = sizeof(struct acct);
	Image->type = SAT_PROC_ACCT;

	return Read(Input, Image->data, Image->len);
}


/*
 * WriteRecord_SVR4
 *	Writes the specified record_t to the specified output_t
 *	in SVR4 format. Returns 0 if successful, -1 if not.
 */
int
WriteRecord_SVR4(output_t *Output, record_t *Record)
{
	struct acct acctrec;

	/* Start off with a clean slate */
	bzero(&acctrec, sizeof(acctrec));

	/* Copy in relevant data from the canonical record */
	acctrec.ac_flag = AEXPND;
	if (Record->data.p.pa.sat_flag & SPASF_FORK) {
		acctrec.ac_flag |= AFORK;
	}
	if (Record->data.p.pa.sat_flag & SPASF_SU) {
		acctrec.ac_flag |= ASU;
	}

	if (Record->gotstat) {
		acctrec.ac_stat = Record->status;
	}

	acctrec.ac_tty	 = Record->tty;
	acctrec.ac_btime = Record->data.p.pa.sat_btime;
	acctrec.ac_utime = Compress(Record->timers.ac_utime / TICK);
	acctrec.ac_stime = Compress(Record->timers.ac_stime / TICK);
	acctrec.ac_etime = Record->data.p.pa.sat_etime;
	acctrec.ac_mem	 = Compress(Record->counts.ac_mem);
	acctrec.ac_io	 = Compress(Record->counts.ac_chr +
				    Record->counts.ac_chw);
	acctrec.ac_rw	 = Compress(Record->counts.ac_br +
				    Record->counts.ac_bw);

	strncpy(acctrec.ac_comm, Record->command, 8);

	/* If the owner UID/GID are available, use those. Otherwise */
	/* use the real UID/GID.				    */
	if (Record->gotowner) {
		acctrec.ac_uid = Record->ouid;
		acctrec.ac_gid = Record->ogid;
	}
	else {
		acctrec.ac_uid = Record->ruid;
		acctrec.ac_gid = Record->rgid;
	}

	/* Write the record out and return */
	return Write(Output, &acctrec, sizeof(acctrec));
}


/****************************************************************/
/*			PRIVATE FUNCTIONS			*/
/****************************************************************/


/*
 * Compress
 *	"Compresses" an accum_t into a comp_t, the compressed,
 *	quasi-floating-point data type used in SVR4 accounting
 *	that consists of a 13-bit mantissa and a 3-bit base-8
 *	exponent.
 */
static comp_t
Compress(accum_t Value)
{
	short   exponent = 0;
	int64_t mantissa = Value;
	int     round = 0;

	/* Clamp the value at the maximum that can be described */
	/* in SVR4 format if necessary				*/
	if (mantissa > MAX_SVR4_VALUE) {
		mantissa = MAX_SVR4_VALUE;
	}

	/* Calculate the exponent */
        while (mantissa >= 8192) {
                ++exponent;
                round = mantissa & 04;
                mantissa >>= 3;
        }

	/* Round up if necessary */
        if (round) {
                ++mantissa;
                if (mantissa >= 8192) {
                        mantissa >>= 3;
                        ++exponent;
                }
        }

	/* Build and return the result */
        return (comp_t) ((exponent << 13) + mantissa);
}


/*
 * Expand
 *	Converts a comp_t (see "Compress") into an accum_t
 */
accum_t
Expand(comp_t Value)
{
	int64_t exponent;
	int64_t mantissa;
	
	/* Isolate the exponent and mantissa */
	exponent = (Value >> 13) & 0x0007;
	mantissa = Value & 0x1fff;

	/* Put the values back together */
	return mantissa << (exponent * 3);
}
