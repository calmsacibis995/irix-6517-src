/*
 * os/numa/pmo_ns.c
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


#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pmo.h>
#include <sys/mmci.h>
#include "pmo_error.h"
#include "pmo_base.h"
#include "pmo_ns.h"

#define	MAX_PMO_NS	((_PAGESZ*32)/sizeof(pmo_ns_entry_t))

static void pmo_ns_expand(pmo_ns_t *, int, int);

/**************************************************************************
 *                    Naming Table for PM's and MLD's                     *
 **************************************************************************/

/*
 * This is a simple naming service for all the memory management policy
 * objects. We create a policy name space for each address space. If a process
 * has both a private space (p_region) and a shared space (s_region from shaddr),
 * then we use the link to the naming table from the shaddr.
 */


pmo_ns_t*
pmo_ns_create(int maxentries)
{
        pmo_ns_t* pmo_ns;
        pmo_ns_entry_t* entry;

        pmo_ns = kmem_zalloc(sizeof(pmo_ns_t), KM_SLEEP);
        ASSERT(pmo_ns);
        
        entry = kmem_zalloc(sizeof(pmo_ns_entry_t) * maxentries, KM_SLEEP);
        ASSERT(entry);

        pmo_ns->pmo_ns_entry = entry;
        pmo_ns->maxentries = maxentries;
        pmo_ns->nentries = 0;
        pmo_ns->index = 0;
        pmo_ns->flags = 0;

        spinlock_init(&pmo_ns->lock, "pmo_ns_lock");
        sv_init(&pmo_ns->wait, SV_DEFAULT, "pmo_ns_wait");

	pmo_base_init(pmo_ns, __PMO_PMO_NS, (pmo_method_t)pmo_ns_destroy);
        return (pmo_ns);
}

void
pmo_ns_destroy(pmo_ns_t* pmo_ns)
{
        pmo_ns_it_t it;
        pmo_ns_entry_t* entry;
        int ns_ospl;
        
        ASSERT (pmo_ns);

        /*
         * We need to decref all the pmo objects
         * currently in this ns.
         */
        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_EXPANDING) {
                panic("[pmo_ns_destroy]: pmo_ns destruction while expanding");
        }
        
        pmo_ns->flags |= PMO_NS_DESTROYING;

   
        pmo_ns_it_init(&it, pmo_ns);
        while ((entry = pmo_ns_it_getnext(&it)) != NULL) {
                pmo_decref(entry->pmo, pmo_ns);
        }
   
        pmo_ns_unlock(pmo_ns, ns_ospl);

        if (pmo_ns->flags & PMO_NS_EXPANDING) {
                panic("[pmo_ns_destroy]: pmo_ns destruction while expanding");
        }        

        sv_destroy(&pmo_ns->wait);
        spinlock_destroy(&pmo_ns->lock);
        kmem_free(pmo_ns->pmo_ns_entry, sizeof(pmo_ns_entry_t) * pmo_ns->maxentries);
        kmem_free(pmo_ns, sizeof(pmo_ns_t));
}

pmo_handle_t
pmo_ns_insert(pmo_ns_t* pmo_ns, pmo_type_t pmo_type, void* pmo)
{
        int ns_ospl;
        int i;
        int count;
        pmo_ns_entry_t* entry;
        pmo_handle_t handle;

        ASSERT (pmo_ns);
        ASSERT (pmo);
        
        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_insert]: pmo_ns insertion while destroying");
        }
        
        if (pmo_ns->nentries >= pmo_ns->maxentries) {
                /*
                 * We ran out of space.
                 * We check whether somebody is already
                 * expanding the directory. If so, we wait;
                 * otherwise we expand it right here.
                 */
		if (pmo_ns->maxentries >= MAX_PMO_NS) {
			pmo_ns_unlock(pmo_ns, ns_ospl);
			return (PMOERR_NOMEM);
		}
                if (pmo_ns->flags & PMO_NS_EXPANDING) {
                      
                        while (pmo_ns->flags & PMO_NS_EXPANDING) {
                                pmo_ns->flags |= PMO_NS_WAITING;
                                sv_wait(&pmo_ns->wait, PZERO, &pmo_ns->lock, ns_ospl);
                                ns_ospl = pmo_ns_lock(pmo_ns);
                        }
                } else {
			int new_maxentries;

			new_maxentries = min(pmo_ns->maxentries * 2, MAX_PMO_NS);
			pmo_ns_expand(pmo_ns, ns_ospl, new_maxentries);
		}
                ASSERT(pmo_ns->flags == 0);
        }

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_insert]: pmo_ns insertion while destroying");
        }
        
        ASSERT (pmo_ns->index < pmo_ns->maxentries);

        for (i = pmo_ns->index, count = 0;
             count < pmo_ns->maxentries;
             i = (i + 1) % pmo_ns->maxentries, count++) {
                entry = &pmo_ns->pmo_ns_entry[i];
                if (entry->pmo == NULL) {
                        /*
                         * If the pmo entry is NULL, then the
                         * entry is available.
                         */
                        handle = PMO_NS_BUILDHANDLE(i);
                        entry->pmo_handle = handle;
                        entry->pmo_type = pmo_type;
                        entry->pmo = pmo;
                        pmo_incref(pmo, pmo_ns);
                        break;
                }
        }

        ASSERT (count < pmo_ns->maxentries);
        pmo_ns->index = (i + 1) % pmo_ns->maxentries;
        pmo_ns->nentries++;

        pmo_ns_unlock(pmo_ns, ns_ospl);
        return (handle);
}

/*
 * Interface to allocate a specific entry in the name space so that
 * restart can restore handles used when checkpointed
 */
pmo_handle_t
pmo_ns_insert_handle(pmo_ns_t* pmo_ns, pmo_type_t pmo_type, void* pmo,
		     pmo_handle_t pmo_handle)
{
	int index;
        int ns_ospl;
        pmo_ns_entry_t* entry;

        ASSERT (pmo_ns);
        ASSERT (pmo);
        
	index = PMO_NS_GETINDEX(pmo_handle);

	if ((index < 0)||(index >= MAX_PMO_NS))
		return(PMOERR_EINVAL);

        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_insert]: pmo_ns insertion while destroying");
        }
        
        if (index >= pmo_ns->maxentries) {
                /*
                 * We need more space.
                 * We check whether somebody is already
                 * expanding the directory. If so, we wait;
                 * otherwise we expand it right here.
                 */
                if (pmo_ns->flags & PMO_NS_EXPANDING) {
                      
                        while (pmo_ns->flags & PMO_NS_EXPANDING) {
                                pmo_ns->flags |= PMO_NS_WAITING;
                                sv_wait(&pmo_ns->wait, PZERO, &pmo_ns->lock, ns_ospl);
                                ns_ospl = pmo_ns_lock(pmo_ns);
                        }
                } else {
			int new_maxentries;
			/*
			 * Since we limit hw big the table can get, this
			 * loop has a small, bounded max iteration
			 */
			for (new_maxentries = 2 * pmo_ns->maxentries;
				new_maxentries <= index;
					new_maxentries *= 2);

			new_maxentries = min(new_maxentries, MAX_PMO_NS);

			ASSERT(new_maxentries > index);

			pmo_ns_expand(pmo_ns, ns_ospl, new_maxentries);
                }

                ASSERT(pmo_ns->flags == 0);
        }

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_insert]: pmo_ns insertion while destroying");
        }
        
        ASSERT (pmo_ns->index < pmo_ns->maxentries);

	/*
	 * If the pmo entry is NULL, then the
	 * entry is available.
	 */
        entry = &pmo_ns->pmo_ns_entry[index];
        if (entry->pmo != NULL) {
        	pmo_ns_unlock(pmo_ns, ns_ospl);
		return (PMOERR_EBUSY);
	}
	entry->pmo_handle = pmo_handle;
	entry->pmo_type = pmo_type;
	entry->pmo = pmo;
	pmo_incref(pmo, pmo_ns);

        ASSERT (index < pmo_ns->maxentries);

        pmo_ns->nentries++;

        pmo_ns_unlock(pmo_ns, ns_ospl);
        return (pmo_handle);
}

void*
pmo_ns_find(pmo_ns_t* pmo_ns, pmo_handle_t pmo_handle, pmo_type_t pmo_type)
{
        int ns_ospl;
        int index;
        void* pmo;
        pmo_ns_entry_t* entry;
        
        ASSERT (pmo_ns);

        index = PMO_NS_GETINDEX(pmo_handle);

        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_find]: pmo_ns find while destroying");
        }


        if (index < 0 || index >= pmo_ns->maxentries) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        }
        entry = &pmo_ns->pmo_ns_entry[index];
        if (entry->pmo == NULL) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        }

        ASSERT (entry->pmo_handle == pmo_handle);
        ASSERT (entry->pmo_type == pmo_gettype(entry->pmo));

        if (pmo_type != __PMO_ANY && pmo_type != pmo_gettype(entry->pmo)) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        } 

        pmo = entry->pmo;
        pmo_incref(pmo, pmo_ref_find);

        pmo_ns_unlock(pmo_ns, ns_ospl);

        return (pmo);
}

pmo_handle_t
pmo_ns_find_handle(pmo_ns_t* pmo_ns, pmo_handle_t pmo_handle, pmo_type_t pmo_type)
{
        int ns_ospl;
        int index;
        pmo_ns_entry_t* entry;
	pmo_handle_t handle;
        
        ASSERT (pmo_ns);

        index = PMO_NS_GETINDEX(pmo_handle);

        if (index < 0)
                return (PMOERR_EINVAL);

        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_find_handle]: pmo_ns find handle while destroying");
        }
	for (; index < pmo_ns->maxentries; index++) {
        	entry = &pmo_ns->pmo_ns_entry[index];
	        if ((entry->pmo != NULL) &&
                    (pmo_type == __PMO_ANY || pmo_type == entry->pmo_type)) {
			handle = entry->pmo_handle;
	                pmo_ns_unlock(pmo_ns, ns_ospl);
	                return (handle);
	        }
	}
	pmo_ns_unlock(pmo_ns, ns_ospl);
	return (PMOERR_ESRCH);
}

pmo_handle_t
pmo_ns_pmo_lookup(pmo_ns_t* pmo_ns, pmo_type_t pmo_type, void* pmo)
{
        int ns_ospl;
        pmo_ns_entry_t* entry;
        int i;
        pmo_handle_t handle;

        ASSERT(pmo_ns);
        ASSERT(pmo);
        
        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_lookup]:  while destroying");
        }

        
        for (i = 0; i < pmo_ns->maxentries; i++) {
                entry = &pmo_ns->pmo_ns_entry[i];
                if ((entry->pmo == pmo) &&
                    (pmo_type == __PMO_ANY || pmo_type == entry->pmo_type)) {
                        handle = entry->pmo_handle;
                        pmo_ns_unlock(pmo_ns, ns_ospl);
                        return (handle);
                }
        }
        pmo_ns_unlock(pmo_ns, ns_ospl);
        return (PMOERR_ESRCH);
}


void*
pmo_ns_extract(pmo_ns_t* pmo_ns, pmo_handle_t pmo_handle, pmo_type_t pmo_type)
{
        int ns_ospl;
        int index;
        void* pmo;
        pmo_ns_entry_t* entry;
         
        ASSERT (pmo_ns);

        index = PMO_NS_GETINDEX(pmo_handle);

        ns_ospl = pmo_ns_lock(pmo_ns);

        if (pmo_ns->flags & PMO_NS_DESTROYING) {
                panic("[pmo_ns_extract]:  while destroying");
        }
        
        if (index < 0 || index >= pmo_ns->maxentries) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        }
        entry = &pmo_ns->pmo_ns_entry[index];
        if (entry->pmo == NULL) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        }

        ASSERT (entry->pmo_handle == pmo_handle);
        
        if (pmo_type != __PMO_ANY && pmo_type != pmo_gettype(entry->pmo)) {
                pmo_ns_unlock(pmo_ns, ns_ospl);
                return (NULL);
        } 

        pmo = entry->pmo;

        /*
         * delete entry
         */
        entry->pmo = NULL;
        pmo_ns->nentries--;
        pmo_decref(pmo, pmo_ns);

        pmo_ns_unlock(pmo_ns, ns_ospl);

        return (pmo);
}

char*
pmo_ns_entry_format(pmo_ns_entry_t* entry, char* str, int len)
{
        ASSERT (entry);

        if (len < 64) {
                return (NULL);
        }

        sprintf(str, "Handle: 0x%x, Type: 0x%x, pmo: 0x%x",
                entry->pmo_handle, entry->pmo_type, entry->pmo);

        return (str);
}


/**************************************************************************
 *                PM  Factory Naming Table Iterator                       *
 **************************************************************************/

void
pmo_ns_it_init(pmo_ns_it_t* pmo_ns_it, pmo_ns_t* pmo_ns)
{
        ASSERT(pmo_ns_it);

        pmo_ns_it->pmo_ns = pmo_ns;
        pmo_ns_it->it_index = 0;
}

pmo_ns_entry_t*
pmo_ns_it_getnext(pmo_ns_it_t* pmo_ns_it)
{
        int i;
        pmo_ns_entry_t* entry;

        ASSERT (pmo_ns_it);
        ASSERT (pmo_ns_it->pmo_ns);

        for (i = pmo_ns_it->it_index; i < pmo_ns_it->pmo_ns->maxentries; i++) {
                entry = &pmo_ns_it->pmo_ns->pmo_ns_entry[i];
                if (entry->pmo != NULL) {
/**************************
                        if (!((ulong)entry->pmo & 0x80000000)) {
                                printf("GETNEXT PROBLEM index: %d\n", pmo_ns_it->it_index);
                                printf("pmo_ns:0x%llx\n", pmo_ns_it->pmo_ns);
                                printf("maxe %d nent %d index %d flags 0x%x\n",
                                       pmo_ns_it->pmo_ns->maxentries,
                                       pmo_ns_it->pmo_ns->nentries,
                                       pmo_ns_it->pmo_ns->index,
                                       pmo_ns_it->pmo_ns->flags);
                                for (i = 0; i < 40; i++) {
                                        printf("E[%d] h %d t %d pmo 0x%llx\n",
                                               i,
                                               pmo_ns_it->pmo_ns->pmo_ns_entry[i].pmo_handle,
                                               pmo_ns_it->pmo_ns->pmo_ns_entry[i].pmo_type,
                                               pmo_ns_it->pmo_ns->pmo_ns_entry[i].pmo);
                                }
                        }
                                        
*************/
                        
                        pmo_ns_it->it_index = i + 1;
                        return (entry);
                }
        }

        return (NULL);
}
                 
                
static void
pmo_ns_expand(pmo_ns_t *pmo_ns, int ns_ospl, int new_maxentries)
{
	pmo_ns_entry_t* new_entry;
			
	pmo_ns->flags |= PMO_NS_EXPANDING;
	pmo_ns_unlock(pmo_ns, ns_ospl);

	new_entry = kmem_zalloc(sizeof(pmo_ns_entry_t)*new_maxentries, KM_SLEEP);
	ASSERT(new_entry);

	ns_ospl = pmo_ns_lock(pmo_ns);

	if (pmo_ns->flags & PMO_NS_DESTROYING) {
		panic("[pmo_ns_insert]: pmo_ns insertion while destroying");
	}
	
	bcopy((void*)pmo_ns->pmo_ns_entry,
	      (void*)new_entry,
	      sizeof(pmo_ns_entry_t) * pmo_ns->maxentries);
	kmem_free(pmo_ns->pmo_ns_entry,
		  sizeof(pmo_ns_entry_t) * pmo_ns->maxentries);
	
	pmo_ns->pmo_ns_entry = new_entry;
	pmo_ns->index = pmo_ns->maxentries; /* set to old maxentries */
	pmo_ns->maxentries = new_maxentries;

	pmo_ns->flags &= ~PMO_NS_EXPANDING;
	if (pmo_ns->flags & PMO_NS_WAITING) {
		pmo_ns->flags &= ~PMO_NS_WAITING;
		sv_broadcast(&pmo_ns->wait);
	}
}
