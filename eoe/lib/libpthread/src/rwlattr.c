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
#include "rwlattr.h"

#include <errno.h>
#include <pthread.h>


/* Default attributes
 */
rwlattr_t rwlattr_default = {
	0	/* ra_flags */
};


int
pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	*attr = rwlattr_default;
	return (0);
}


/* ARGSUSED */
int
pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	return (0);
}


int
pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared)
{
	*pshared = attr->ra_pshared
			? PTHREAD_PROCESS_SHARED
			: PTHREAD_PROCESS_PRIVATE;
	return (0);
}


int
pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared)
{
	if (pshared == PTHREAD_PROCESS_SHARED) {
		attr->ra_pshared = 1;
	} else if (pshared == PTHREAD_PROCESS_PRIVATE) {
		attr->ra_pshared = 0;
	} else {
		return (EINVAL);
	}
	return (0);
}
