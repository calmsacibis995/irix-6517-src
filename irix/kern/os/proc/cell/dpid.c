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

#include <sys/types.h>
#include <sys/debug.h>
#include <ksys/pid.h>
#include "pid_private.h"
#include "I_dpid_stubs.h"

void
I_dpid_getlist(
        pidord_t *next,
        size_t *realcount,
        pid_t **pidbufp,
        size_t *pidbuf_count,
        pidord_t **ordbufp,
        size_t *ordbuf_count,
        void **descp)
{
        size_t max_count = *pidbuf_count;
        size_t count = max_count;
	pid_t *pidbuf;
	pidord_t *ordbuf;

        ASSERT(*pidbuf_count == *ordbuf_count);
	pidbuf = (pid_t *) kmem_alloc(max_count * sizeof(pid_t),
				      KM_SLEEP);

	ordbuf = (pid_t *) kmem_alloc(max_count * sizeof(pidord_t),
				      KM_SLEEP);

        if (pid_getlist_local(*next, &count, pidbuf, ordbuf))
		*next = pid_base + pid_wrap;
	else if (count != 0) 
                *next = *(ordbuf + (count - 1));
	*realcount = count;
	if (count == 0)
		count = 1;
	*pidbuf_count = count;
	*ordbuf_count = count;
	*pidbufp = pidbuf;
	*ordbufp = ordbuf;
	*descp = (void *) max_count;
}
        
void
I_dpid_getlist_done(
        pid_t *pidbuf ,
        size_t pidbuf_count,
        pidord_t *ordbuf ,
        size_t ordbuf_count,
        void *desc)
{
        size_t max_count = (size_t) desc;

	ASSERT(max_count >= pidbuf_count &&
	       max_count >= ordbuf_count &&
	       pidbuf_count == ordbuf_count);
	kmem_free(pidbuf, max_count * sizeof(pid_t));
	kmem_free(ordbuf, max_count * sizeof(pidord_t));
}

