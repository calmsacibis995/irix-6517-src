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
#include "vp.h"
#include "mtxattr.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>


/* Default attributes
 */
mtxattr_t mtxattr_default = {
	DEFAULT_MUTEX_PROTOCOL,	/* ma_protocol */
	PX_PRIO_MIN,		/* ma_priority */
	PTHREAD_MUTEX_DEFAULT,	/* ma_type */
	0			/* ma_flags */
};

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	*attr = mtxattr_default;
	return (0);
}


/* ARGSUSED */
int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return (0);
}


int
pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
	*pshared = attr->ma_pshared
				? PTHREAD_PROCESS_SHARED
				: PTHREAD_PROCESS_PRIVATE;
	return (0);
}


int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
	if (pshared == PTHREAD_PROCESS_SHARED) {
		if (attr->ma_protocol == PTHREAD_PRIO_INHERIT)
			return (EINVAL);
		attr->ma_pshared = 1;
	} else if (pshared == PTHREAD_PROCESS_PRIVATE) {
		attr->ma_pshared = 0;
	} else {
		return (EINVAL);
	}
	return (0);
}


#if defined(_POSIX_THREAD_PRIO_INHERIT) && defined(_POSIX_THREAD_PRIO_PROTECT)
int
pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
	if (protocol == PTHREAD_PRIO_INHERIT && attr->ma_pshared)
		return (EINVAL);

	if (protocol == PTHREAD_PRIO_NONE ||
	    protocol == PTHREAD_PRIO_INHERIT ||
	    protocol == PTHREAD_PRIO_PROTECT) {
		attr->ma_protocol = protocol;
		return (0);
	}

	return (EINVAL);
}


int
pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol)
{
	*protocol = attr->ma_protocol;
	return (0);
}


int
pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int pri)
{
	if (pri >= PX_PRIO_MIN && pri <= PX_PRIO_MAX) {
		attr->ma_priority = pri;
		return (0);
	}
	return (EINVAL);
}


int
pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *pri)
{
	*pri = attr->ma_priority;
	return (0);
}
#endif /* _POSIX_THREAD_PRIO_INHERIT && _POSIX_THREAD_PRIO_PROTECT */


int
pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	if (type == PTHREAD_MUTEX_NORMAL ||
	    type == PTHREAD_MUTEX_ERRORCHECK ||
	    type == PTHREAD_MUTEX_RECURSIVE ||
	    type == PTHREAD_MUTEX_DEFAULT) {
		attr->ma_type = type;
		return (0);
	}

	return (EINVAL);
}


int
pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
	*type = attr->ma_type;
	return (0);
}
