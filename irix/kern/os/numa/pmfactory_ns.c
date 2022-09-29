/*
 * os/numa/pmfactory_ns.c
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
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

/**************************************************************************
 *                   Policy Module  Factory Naming                        *
 **************************************************************************/

#include <sys/pmo.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/mmci.h>
#include "pmo_error.h"
#include "pmo_base.h"
#include "pmfactory_ns.h"
#include "numa_init.h"

#define PM_NULL_TYPE 0

/*
 * This method creates a system-wide Policy Module directory
 * visible to users. This directory is created only once
 * at boot time.
 */
pmfactory_ns_t*
pmfactory_ns_create(void)
{
        pmfactory_ns_t* pmfactory_ns;

        pmfactory_ns = kmem_zalloc(sizeof(pmfactory_ns_t), KM_SLEEP);
        ASSERT(pmfactory_ns);

        pmfactory_ns->index = 0;
        pmfactory_ns->nentries = 0;
        spinlock_init(&pmfactory_ns->lock, "pmfactory_ns_lock");

        return (pmfactory_ns);
}

pmo_handle_t
pmfactory_ns_insert(pmfactory_ns_t* pmfactory_ns,
                    char* pm_name,
                    pmfactory_t pmfactory)
{
        int ns_ospl;
        int i;
        int count;
        pmfactory_ns_entry_t* entry;

        ASSERT (pmfactory_ns);
        ASSERT (pm_name);
        ASSERT (pmfactory);

        ns_ospl = pmfactory_ns_lock(pmfactory_ns);
        if (pmfactory_ns->nentries >= PMFACTORY_NS_NENT) {
                /*
                 * We ran out of free entries
                 */
                pmfactory_ns_unlock(pmfactory_ns, ns_ospl);
                return (PMOERR_NS_NOSPACE);
        }

        ASSERT (pmfactory_ns->index < PMFACTORY_NS_NENT);

        for (i = pmfactory_ns->index, count = 0;
             count < PMFACTORY_NS_NENT;
             i = (i + 1) % PMFACTORY_NS_NENT, count++) {
                entry = &pmfactory_ns->pmfactory_ns_entry[i];
                if (entry->pmfactory == NULL) {
                        entry->pmfactory = pmfactory;
                        entry->pm_type = PM_NULL_TYPE;
                        strncpy(entry->pm_name, pm_name, PM_NAME_SIZE - 1);
                        break;
                }
        }

        ASSERT (count < PMFACTORY_NS_NENT);
        pmfactory_ns->index = i + 1;
        pmfactory_ns->nentries++;

        pmfactory_ns_unlock(pmfactory_ns, ns_ospl);
        return (0);
}

pmfactory_t
pmfactory_ns_find(pmfactory_ns_t* pmfactory_ns, char* pm_name)
{
        int ns_ospl;
        int i;
        pmfactory_ns_entry_t* entry;
        pmfactory_t pmfactory;

        ASSERT (pmfactory_ns);
        ASSERT (pm_name);

        ns_ospl = pmfactory_ns_lock(pmfactory_ns);

        for (i = 0; i <  PMFACTORY_NS_NENT; i++) {
                entry = &pmfactory_ns->pmfactory_ns_entry[i];
                if (entry->pmfactory != NULL) {
                        if ((strcmp(entry->pm_name, pm_name) == 0) &&
                            (entry->pm_type == PM_NULL_TYPE)) {
                                pmfactory = entry->pmfactory;
                                pmfactory_ns_unlock(pmfactory_ns, ns_ospl);
                                return (pmfactory);
                        }
                }
        }

        pmfactory_ns_unlock(pmfactory_ns, ns_ospl);

        printf("Failed Searching for %s\n", pm_name);
        printf("Current entries: \n");

        for (i = 0; i <  PMFACTORY_NS_NENT; i++) {
                entry = &pmfactory_ns->pmfactory_ns_entry[i];
                if (entry->pmfactory != NULL) {
                        printf("NAME: [%s], type: %d\n", entry->pm_name, entry->pm_type);
                }
        }        
        
        return (NULL);
}

pmfactory_t
pmfactory_ns_extract(pmfactory_ns_t* pmfactory_ns, char* pm_name)
{
        int ns_ospl;
        int i;
        pmfactory_ns_entry_t* entry;
        pmfactory_t pmfactory;

        ASSERT (pmfactory_ns);
        ASSERT (pm_name);

        ns_ospl = pmfactory_ns_lock(pmfactory_ns);

        for (i = 0; i <  PMFACTORY_NS_NENT; i++) {
                entry = &pmfactory_ns->pmfactory_ns_entry[i];
                if (entry->pmfactory != NULL) {
                        if ((strcmp(entry->pm_name, pm_name) == 0) &&
                            (entry->pm_type == PM_NULL_TYPE)) {
                                pmfactory = entry->pmfactory;
                                /*
                                 * delete entry
                                 */
                                entry->pmfactory = 0;
                                pmfactory_ns->nentries--;
                                
                                pmfactory_ns_unlock(pmfactory_ns, ns_ospl);
                                return (pmfactory);
                        }
                }
        }

        pmfactory_ns_unlock(pmfactory_ns, ns_ospl);
        return (NULL);
}

char*
pmfactory_ns_entry_format(pmfactory_ns_entry_t* entry, char* str, int len)
{
        ASSERT (entry);

        if (len < 64) {
                return (NULL);
        }
        sprintf(str, "Name: %s, Type: %d, Factory: 0x%x",
                entry->pm_name, entry->pm_type, entry->pmfactory);

        return (str);
}

/**************************************************************************
 *                PM  Factory Naming Table Iterator                       *
 **************************************************************************/


pmfactory_ns_it_t*
pmfactory_ns_it_create(pmfactory_ns_t* pmfactory_ns)
{
        pmfactory_ns_it_t* pmfactory_ns_it;
        
        ASSERT (pmfactory_ns);

        pmfactory_ns_it = kmem_zalloc(sizeof(pmfactory_ns_it_t), KM_SLEEP);
        ASSERT(pmfactory_ns_it);

        pmfactory_ns_it->pmfactory_ns = pmfactory_ns;
        pmfactory_ns_it->it_index = 0;

        return (pmfactory_ns_it);
}

void
pmfactory_ns_it_destroy(pmfactory_ns_it_t* pmfactory_ns_it)
{
        ASSERT (pmfactory_ns_it);

        kmem_free(pmfactory_ns_it, sizeof(pmfactory_ns_it_t));
}


pmfactory_ns_entry_t*
pmfactory_ns_it_getnext(pmfactory_ns_it_t* pmfactory_ns_it)
{
        int ns_ospl;
        int i;
        pmfactory_ns_entry_t* entry;

        ASSERT (pmfactory_ns_it);

         ns_ospl = pmfactory_ns_lock(pmfactory_ns_it->pmfactory_ns);
         for (i = pmfactory_ns_it->it_index; i < PMFACTORY_NS_NENT; i++) {
                 entry = &pmfactory_ns_it->pmfactory_ns->pmfactory_ns_entry[i];
                 if (entry->pmfactory != NULL) {
                         pmfactory_ns_it->it_index = i + 1;
                         pmfactory_ns_unlock(pmfactory_ns_it->pmfactory_ns, ns_ospl);
                         return (entry);
                 }
         }

         pmfactory_ns_unlock(pmfactory_ns_it->pmfactory_ns, ns_ospl);
         return (NULL);
}


/**************************************************************************
 *           Policy Module Factory NameSpace Initialization               *
 **************************************************************************/

static pmfactory_ns_t* pmfactory_ns_default;

/*
 * Initializer called from main to setup the
 * Policy Module Factory Table.
 */
int
pmfactory_ns_init(void)
{
        /*
         * Create the Pmfactory_Ns
         */
        pmfactory_ns_default = pmfactory_ns_create();
        ASSERT (pmfactory_ns_default);

        return (0);
}

/*
 * Method to export PM CLASS FACTORIES
 */
void
pmfactory_export(pmfactory_t pmfactory, char* name)
{
	/*REFERENCED*/
        pmo_handle_t errcode;
        
        ASSERT(pmfactory);
        ASSERT(name);
        ASSERT(pmfactory_ns_default);
#ifndef SABLE
#if DEBUG1
        cmn_err(CE_NOTE, "[pmfactory_export]: Exporting %s", name);
#endif /* DEBUG1 */
#endif

        errcode = pmfactory_ns_insert(pmfactory_ns_default,
                                      name,
                                      pmfactory);
        ASSERT (!pmo_iserrorhandle(errcode));
}
        

pmfactory_ns_t*
pmfactory_ns_getdefault()
{
        return (pmfactory_ns_default);
}
