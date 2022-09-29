/*
 * os/numa/pmo_list.c
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
#include <sys/pmo.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include "pmo_list.h"
#include "pmo_base.h"
#include "numa_init.h"

/*
 * The dynamic memory allocation zone for pmo lists
 */
static zone_t* pmolist_zone = 0;

/*
 * The pmolist initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
pmolist_init()
{
        ASSERT(pmolist_zone == 0);
        pmolist_zone = kmem_zone_init(sizeof(pmolist_t), "pmolist");
        ASSERT(pmolist_zone);

        return (0);
}

pmolist_t*
pmolist_create(int len)
{
        pmolist_t* pmolist;

        ASSERT(len >= 1);
        
        pmolist = kmem_zone_zalloc(pmolist_zone, KM_SLEEP);

        if (pmolist == NULL) {
                return (NULL);
        }

        if (len == 1) {
                pmolist->arr = NULL;
        } else {
                pmolist->arr = kmem_zalloc(len * sizeof(void*), KM_SLEEP);
                if (pmolist->arr == NULL) {
                        kmem_zone_free(pmolist_zone, pmolist);
                        return (NULL);
                }
        }
        
        pmolist->mlen = len;
        pmolist->clen = 0;

        return (pmolist);
}

/*ARGSUSED*/
void
pmolist_destroy(pmolist_t* pmolist, void* rop)
{
        int i;

        ASSERT(pmolist);
        
        for (i = 0; i < pmolist->clen; i++) {
                pmo_decref(pmolist_getelem(pmolist, i), rop);
        }
        if (pmolist->mlen > 1) {
                ASSERT(pmolist->arr);
                kmem_free(pmolist->arr, pmolist->mlen * sizeof(void*));
        }
        kmem_zone_free(pmolist_zone, pmolist);
}

/*
 * Setup a pmolist allocated on the stack with just one element
 * The destructor musn't be called on this kind of pmolist!
 */
void
pmolist_setup(pmolist_t* pmolist, void* pmo)
{
        ASSERT(pmolist);
        ASSERT(pmo);
        
        pmolist->mlen = 1;
        pmolist->clen = 1;        
        pmolist->arr = (void**)pmo;
}

/*ARGSUSED*/
void
pmolist_insert(pmolist_t* pmolist, void* pmo, void* rop)
{
        ASSERT (pmolist);
        ASSERT (pmo);
        ASSERT (pmolist->clen < pmolist->mlen);

        if (pmolist->mlen == 1) {
                pmolist->arr = (void**)pmo;
        } else {
                pmolist->arr[pmolist->clen] = pmo;
        }
        pmo_incref(pmo, rop);
        pmolist->clen++;
}

void*
pmolist_getelem(pmolist_t* pmolist, int i)
{
        void* pmo;
        
        ASSERT(pmolist);
        ASSERT(i < pmolist->clen);
        ASSERT(pmolist->clen <= pmolist->mlen);
        
        if (pmolist->mlen == 1) {
                pmo = (void*)pmolist->arr;
        } else {
                pmo = pmolist->arr[i];
        }

        return (pmo);
}
        
pmolist_t*
pmolist_createfrom(pmolist_t* srclist, int newlen, void* rop)
{
        pmolist_t* newlist;
        int i;

        ASSERT(srclist);
        ASSERT(newlen >= pmolist_getmlen(srclist));

        if ((newlist = pmolist_create(newlen)) == NULL) {
                return (NULL);
        }

        for (i = 0; i < pmolist_getclen(srclist); i++) {
                pmolist_insert(newlist, pmolist_getelem(srclist, i), rop);
        }

        return (newlist);
}

void
pmolist_print(pmolist_t* pmolist)
{
        int i;
        
        ASSERT(pmolist);

        printf("[pmolist]: 0x%x\n", pmolist);
        
        for (i = 0; i < pmolist_getclen(pmolist); i++) {
                printf("Elem[%d]: 0x%x\n", i, pmolist_getelem(pmolist, i));
        }
}

/*ARGSUSED*/
void
pmolist_refupdate(pmolist_t* pmolist, void* pmo)
{
        int i;
        
        ASSERT(pmolist);

        for (i = 0; i < pmolist_getclen(pmolist); i++) {
                pmo_incref(pmolist_getelem(pmolist, i), pmo);
                pmo_decref(pmolist_getelem(pmolist, i), pmo_ref_list);
        }
}        

    
