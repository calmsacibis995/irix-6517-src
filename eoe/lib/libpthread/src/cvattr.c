/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "common.h"
#include "cvattr.h"

#include <errno.h>
#include <pthread.h>


/* Default attributes
 */
cvattr_t cvattr_default = {
	0	/* ca_flags */
};


int
pthread_condattr_init(pthread_condattr_t *attr)
{
	*attr = cvattr_default;
	return (0);
}


/* ARGSUSED */
int
pthread_condattr_destroy(pthread_condattr_t *attr)
{
	return (0);
}


int
pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
	*pshared = attr->ca_pshared
			? PTHREAD_PROCESS_SHARED
			: PTHREAD_PROCESS_PRIVATE;
	return (0);
}


int
pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
	if (pshared == PTHREAD_PROCESS_SHARED) {
		attr->ca_pshared = 1;
	} else if (pshared == PTHREAD_PROCESS_PRIVATE) {
		attr->ca_pshared = 0;
	} else {
		return (EINVAL);
	}
	return (0);
}
