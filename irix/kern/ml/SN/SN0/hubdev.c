/*
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

/*
 * ml/SN0/hubdev.c
 * Basic device management for devices in the hub.
 * A hub contains several "devices" we want to dynamically
 * export to user land via a vertex in the system hardware
 * graph and a corresponding device driver. These drivers
 * provide an attach method that needs to be called only
 * when they register themselves. To allow for this operation,
 * this module implements a callout list containing pointers
 * to the methods that need to be called whenever a hub is
 * discovered during the hub discovery/initialization process.
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/iograph.h>
#include <sys/SN/SN0/hubdev.h>
#include "sn0_private.h"

struct hubdev_callout {
        int (*attach_method)(dev_t);
        struct hubdev_callout *fp;
};

typedef struct hubdev_callout hubdev_callout_t;

mutex_t hubdev_callout_mutex;
hubdev_callout_t *hubdev_callout_list = NULL;

void
hubdev_init(void)
{
	mutex_init(&hubdev_callout_mutex, MUTEX_DEFAULT, "hubdev");
        hubdev_callout_list = NULL;
}
        
void
hubdev_register(int (*attach_method)(dev_t))
{
        hubdev_callout_t *callout;
        
        ASSERT(attach_method);

        callout =  kmem_zalloc(sizeof(hubdev_callout_t), KM_SLEEP);
        ASSERT(callout);
        
	mutex_lock(&hubdev_callout_mutex, PZERO);
        /*
         * Insert at the end of the list
         */
        callout->fp = hubdev_callout_list;
        hubdev_callout_list = callout;
        callout->attach_method = attach_method;

	mutex_unlock(&hubdev_callout_mutex);
}

int
hubdev_unregister(int (*attach_method)(dev_t))
{
        hubdev_callout_t **p;
        
        ASSERT(attach_method);
   
	mutex_lock(&hubdev_callout_mutex, PZERO);
        /*
         * Remove registry element containing attach_method
         */
        for (p = &hubdev_callout_list; *p != NULL; p = &(*p)->fp) {
                if ((*p)->attach_method == attach_method) {
                        hubdev_callout_t* victim = *p;
                        *p = (*p)->fp;
                        kmem_free(victim, sizeof(hubdev_callout_t));
                        mutex_unlock(&hubdev_callout_mutex);
                        return (0);
                }
        }
        mutex_unlock(&hubdev_callout_mutex);
        return (ENOENT);
}


int
hubdev_docallouts(dev_t hub)
{
        hubdev_callout_t *p;
        int errcode;

	mutex_lock(&hubdev_callout_mutex, PZERO);
        
        for (p = hubdev_callout_list; p != NULL; p = p->fp) {
                ASSERT(p->attach_method);
                errcode = (*p->attach_method)(hub);
                if (errcode != 0) {
			mutex_unlock(&hubdev_callout_mutex);
                        return (errcode);
                }
        }
        
        mutex_unlock(&hubdev_callout_mutex);
        return (0);
}

/*
 * Given a hub vertex, return the base address of the Hspec space
 * for that hub.
 */
caddr_t
hubdev_prombase_get(dev_t hub)
{
	hubinfo_t	hinfo = NULL;

	hubinfo_get(hub, &hinfo);
	ASSERT(hinfo);

	return ((caddr_t)NODE_RBOOT_BASE(hinfo->h_nasid));
}

cnodeid_t
hubdev_cnodeid_get(dev_t hub)
{
	hubinfo_t	hinfo = NULL;
	hubinfo_get(hub, &hinfo);
	ASSERT(hinfo);

	return hinfo->h_cnodeid;
}
