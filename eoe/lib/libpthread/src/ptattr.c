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
#include "pt.h"
#include "ptattr.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/capability.h>
#include <unistd.h>


/* Compile time check for symbol values.
 */
#if PTHREAD_CREATE_JOINABLE != 0	\
	|| PTHREAD_CREATE_DETACHED != 1	\
	|| PTHREAD_SCOPE_PROCESS != 0	\
	|| PTHREAD_EXPLICIT_SCHED != 0	\
	|| PTHREAD_INHERIT_SCHED != 1
#error Value incorrectly defined, boolean defaults must match fields.
#endif


/* Default attributes
 */
ptattr_t ptattr_default = {
	PTHREAD_CREATE_JOINABLE,/* detached */
	PTHREAD_SCOPE_PROCESS,	/* system */
	PTHREAD_EXPLICIT_SCHED,	/* inherit */
	SCHED_RR,		/* policy */
	PX_PRIO_MIN,		/* priority */

	PT_STACK_DEFAULT,	/* stacksize */
	NULL,			/* stackaddr */
	0			/* guardsize - init'ed in ptattr_bootstrap */
};


void
ptattr_bootstrap()
{
	extern int pt_page_size;
	ptattr_default.pa_guardsize = pt_page_size;
}


int
pthread_attr_init(pthread_attr_t *attr)
{
	*attr = ptattr_default;
	return (0);
}


/* ARGSUSED */
int
pthread_attr_destroy(pthread_attr_t *attr)
{
	return (0);
}


int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if (stacksize < PT_STACK_MIN) {
		return (EINVAL);
	}
	attr->pa_stacksize = stacksize;
	return (0);
}


int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	*stacksize = attr->pa_stacksize;
	return (0);
}


int
pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
/* XXX test for illegal addresses/ STK_INVALIDADDR */
	attr->pa_stackaddr = stackaddr;
	return (0);
}


int
pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	*stackaddr = attr->pa_stackaddr;
	return (0);
}


int
pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	attr->pa_guardsize = guardsize;
	return (0);
}


int
pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	*guardsize = attr->pa_guardsize;
	return (0);
}


int
pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	if (!(detachstate == PTHREAD_CREATE_DETACHED
	      || detachstate == PTHREAD_CREATE_JOINABLE)) {
		return (EINVAL);
	}

	attr->pa_detached = detachstate;
	return (0);
}


int
pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	*detachstate = attr->pa_detached;
	return (0);
}


int
pthread_attr_setscope(pthread_attr_t *attr, int contentionscope)
{
	cap_t		cap;
	cap_value_t	cap_value;

	if (!(contentionscope == PTHREAD_SCOPE_PROCESS
	      || contentionscope == PTHREAD_SCOPE_SYSTEM)) {
		return (EINVAL);
	}

	if (contentionscope != PTHREAD_SCOPE_SYSTEM) {
		goto success;
	}

	/* System scope (RT) is privileged.
	 * We reject the call iff the process does not have and
	 * can never have the required privilege.
	 *
	 * First check for root (effective and real ids).
	 */
	if (geteuid() == 0 || getuid() == 0) {
		goto success;
	}

	/* If we aren't root check for the CAP_SCHED_MGT capability.
	 */
	if ((cap = cap_get_proc()) == 0) {
		if (oserror() == ENOMEM) {	/* cannot tell */
			return (ENOMEM);
		}
		return (EPERM);			/* no capabilities */
	}

	/* Fail if CAP_SCHED_MGT is inaccessible (should never happen)
	 * or is neither effective (current) or permitted (maximium).
	 */
	if (cap_get_flag(cap, CAP_SCHED_MGT, CAP_EFFECTIVE, &cap_value) != 0
	    ||
	    cap_value == CAP_CLEAR
	    && (cap_get_flag(cap, CAP_SCHED_MGT, CAP_PERMITTED, &cap_value) != 0
	        || cap_value == CAP_CLEAR)) {
		return (EPERM);
	}
success:
	attr->pa_system = contentionscope;
	return (0);
}


int
pthread_attr_getscope(const pthread_attr_t *attr, int *contentionscope)
{
	*contentionscope = attr->pa_system;
	return (0);
}


int
pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	if (!(inherit == PTHREAD_EXPLICIT_SCHED
	      || inherit == PTHREAD_INHERIT_SCHED)) {
		return (EINVAL);
	}

	attr->pa_inherit = inherit;
	return (0);
}


int
pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
	*inherit = attr->pa_inherit;
	return (0);
}


int
pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	if (!(policy == SCHED_TS || policy == SCHED_RR
	      || policy == SCHED_FIFO)) {
		return (EINVAL);
	}

	attr->pa_policy = policy;
	return (0);
}


int
pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	*policy = attr->pa_policy;
	return (0);
}


int
pthread_attr_setschedparam(pthread_attr_t *attr,
			   const struct sched_param *param)
{
	if (param->sched_priority < sched_get_priority_min(attr->pa_policy)
	    || param->sched_priority > sched_get_priority_max(attr->pa_policy)){
		return (EINVAL);
	}

	attr->pa_priority = param->sched_priority;
	return (0);
}


int
pthread_attr_getschedparam(const pthread_attr_t *attr,
			   struct sched_param *param)
{
	param->sched_priority = attr->pa_priority;
	return (0);
}
