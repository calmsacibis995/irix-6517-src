/*
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

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <ksys/as.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/mmci.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_ns.h"
#include "mld.h"
#include "pm.h"
#include "aspm.h"


void
kthread_afflink_new(kthread_t* ckt, kthread_t* pkt, pm_t* pm)
{

        ASSERT(ckt);
        ASSERT(pkt);
        ASSERT(pm);
        
        /*
         * To set the affinity link for the process we
         * invoke the affinity method on the current
         * default pm.
         */
        (*pm->afflink)(pm,  AFFLINK_NEWPROC_SHARED, ckt, pkt);
        
        return;
}

/*
 * This method informs a policy module that the current affinity
 * link will no longer be used.
 */
void
kthread_afflink_decusg(kthread_t* kt, asid_t *asid)
{
        pmo_ns_t        *pmo_ns;
        pmo_ns_it_t     pm_it;
        pmo_ns_entry_t  *cpmo;
        int             ns_ospl;
	mld_t		*mldlink;

        ASSERT(kt);
        ASSERT(asid);

        if (AS_ISNULL(asid)) {
                return;
        }

	/*
	 * Capture k_mldlink
	 */
	mldlink = kt->k_mldlink;
	if (mldlink == 0)
		return;

        /*
         * Scan the specified AS for policy modules that reference
         * the current affinity link.
         */
        pmo_ns = getpmo_ns(*asid);
        ASSERT(pmo_ns);
        ns_ospl = pmo_ns_lock(pmo_ns);

	pmo_ns_it_init(&pm_it, pmo_ns);
	while ((cpmo = pmo_ns_it_getnext(&pm_it)) != NULL) {
		if (pmo_gettype(cpmo->pmo) == __PMO_PM &&
		    ((pm_t*)cpmo->pmo)->pmo == mldlink) {

			/*
			 * upcall to the policy module
			 */
			pm_decusg((pm_t*)cpmo->pmo);
		}
	}
        pmo_ns_unlock(pmo_ns, ns_ospl);
        relpmo_ns(*asid);
}

/*
 * This method removes a process's previous affinity link and
 * sets up new affinity links
 */
void
kthread_afflink_exec(kthread_t* kt, pm_t* pm)
{
	int s;

        ASSERT(kt);
        ASSERT(pm);
        
        /*
         * Unset the old affinity links
         */
	s = kt_lock(kt);

        if (kt->k_mldlink) {
                pmo_decref(kt->k_mldlink, kt);
        }

        kt->k_mldlink = 0;
	kt->k_affnode = CNODEID_NONE;
        CNODEMASK_CLRALL(kt->k_maffbv);
	kt_unlock(kt, s);
        
        /*
         * Now set the new affinity links
         */
        (*pm->afflink)(pm, AFFLINK_EXEC, kt, NULL);
}

/*
 * This method removes the affinity links for an exiting process
 */
void
kthread_afflink_unlink(kthread_t* kt, asid_t *asid)
{
        int s;

        ASSERT(kt);
        /*
         * Unset affinity links
         */

        if (asid) {

                /*
                 * Decrement the usage of the policy module
                 */
                kthread_afflink_decusg(kt, asid);

        } else {

                /*
                 * Remove existing affinity link
                 */
        	s = kt_lock(kt);
                if (kt->k_mldlink) {
                        pmo_decref(kt->k_mldlink, kt);
                }

                kt->k_mldlink = 0;
                kt->k_affnode = CNODEID_NONE;
                CNODEMASK_CLRALL(kt->k_maffbv);
        	kt_unlock(kt, s);
        }
}

