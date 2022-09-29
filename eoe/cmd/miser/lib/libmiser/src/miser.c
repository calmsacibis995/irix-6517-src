/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/miser.c
 *
 * DESCRIPTION:
 *	Micelleneous miser support functions.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/sysmp.h>
#include <unistd.h>
#include "libmiser.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static quanta_t	minfo_quantum;


int
miser_init()
/*
 * Checks if the miser daemon is running, returns 1 if true else 0.
 */
{
	if (sysmp(MP_MISER_GETRESOURCE, MPTS_MISER_QUANTUM, &minfo_quantum) 
			|| minfo_quantum == 0) {
		merror("\nmiser: miser is not configured.\n\n");
		return 1;	/* error */
	}

	return 0;	/* success */

} /* miser_init */


quanta_t
miser_quantum()
/*
 * Returns quantum size configured during miser initialization. 
 */
{
	return minfo_quantum ? minfo_quantum : 1;

} /* miser_quantum */


id_type_t
miser_qid(const char *str)
/*
 * Convert queue name string to numeric queue id value.
 */
{
	id_type_t val  = 0;
	int       size = 0;
	char*     tmp  = (char *) &val;

	size = MIN(strlen(str), sizeof(id_type_t));

	while (size--) {
		*(tmp++) = tolower(*(str++));
	}

	return val;

} /* miser_qid */


const char*
miser_qstr(id_type_t qid)
/*
 * Convert numeric queue id to queue name string.	
 */
{
	static char qstr[sizeof(id_type_t)+1];

	memcpy(qstr, (char *) &qid, sizeof(id_type_t));
	qstr[sizeof(id_type_t)] = '\0';

	return qstr;

} /* miser_qstr */


int
miser_checkaccess(FILE *fp, int atype)
/*
 * Check for file access and return 1 for success and 0 for fail.
 */
{
	return atype == F_OK ||
	       sysmp(MP_MISER_CHECKACCESS, fileno(fp), atype) != -1;

} /* miser_checkaccess */ 


char* 
miser_error(int16_t error)
/*
 * Return error string based on the symbol defined in miser_public.h. 
 */
{
        switch(error)  {

        case MISER_ERR_OK:
                return "MISER_ERR_OK";

        case MISER_ERR_POLICY_ID_BAD:
                return "Policy ID invalid.";

        case MISER_ERR_QUEUE_ID_BAD:
                return "Queue ID invalid.";

        case MISER_ERR_RESOURCES_OUT:
                return "Resources are exhausted.";

        case  MISER_ERR_RESOURCES_INSUFF:
                return "There are insufficient resources.";

        case MISER_ERR_INTERVAL_BAD:
                return "The interval is incorrect.";

        case MISER_ERR_TIME_QUANTUM_INVALID:
                return "The time quantum specified is invalid.";

        case MISER_ERR_QUEUE_JOBS_ACTIVE:
                return "There are active jobs on the queue.";

        case MISER_ERR_INVALID_FILE:
                return "The file specified could not be accessed.";

        case MISER_ERR_INVALID_FNAME:
                return "The file name specified is invalid.";

        case MISER_ERR_EFAULT:
                return "MISER_ERR_EFAULT";

        case MISER_ERR_PERM:
                return "Queue access permission denied.";

        case MISER_ERR_BAD_TYPE:
                return "MISER_ERR_BAD_TYPE";

        case MISER_ERROR:
                return "MISER_ERROR";

        case MISER_ERR_JOB_ID_NOT_FOUND:
                return "Job ID not found.";

        case MISER_ERR_DURATION_TOO_LONG:
                return "queue duration too long";

        default:
                return "UNKNOWN MISER ERROR";
        }

} /* miser_error */
