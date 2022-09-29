/*
 * Fast memory pool allocator for fixed-size blocks
 * Pools can only grow in size (no free or coallesing) ... fall-through
 * to malloc/free if not one of the sizes we support.
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

#ident "$Id: pool.c,v 2.5 1999/08/17 04:13:41 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"

typedef struct nurd {
    struct nurd	*next;
} nurd_t;

typedef struct {
    size_t	pc_size;
    void	*pc_head;
    int		alloc;
} poolctl_t;

/*
 * pool types, and their common occurrences ... see pmFreeResultValues()
 * for code that calls __pmPoolFree()
 *
 * [0] 20 bytes, pmValueSet when numval == 1
 * [1] 12 bytes, pmValueBlock for an 8 byte value
 */

static poolctl_t	poolctl[] = {
    { sizeof(pmValueSet),		NULL,	0 },
    { sizeof(int)+sizeof(__int64_t),	NULL,	0 }
};

static int	npool = sizeof(poolctl) / sizeof(poolctl[0]);

void *
__pmPoolAlloc(size_t size)
{
    int		i;
    void	*p;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    if (poolctl[i].pc_head == NULL) {
		poolctl[i].alloc++;
		p = malloc(size);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDUBUF)
		    fprintf(stderr, "__pmPoolAlloc(%d) -> 0x%p [malloc, pool empty]\n",
			(int)size, p);
#endif
		return p;
	    }
	    else {
		nurd_t	*np;
		np = (nurd_t *)poolctl[i].pc_head;
		poolctl[i].pc_head = np->next;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDUBUF)
		    fprintf(stderr, "__pmPoolAlloc(%d) -> 0x%p\n",
			(int)size, np);
#endif
		return (void *)np;
	    }
	}
	i++;
    } while (i < npool);

    p = malloc(size);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPoolAlloc(%d) -> 0x%p [malloc, not pool size]\n",
	    (int)size, p);
#endif

    return p;
}

/*
 * Note: size must be known so that block is returned to the correct pool.
 */
void
__pmPoolFree(void *ptr, size_t size)
{
    int		i;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    nurd_t	*np;
	    np = (nurd_t *)ptr;
	    np->next = poolctl[i].pc_head;
	    poolctl[i].pc_head = np;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF)
		fprintf(stderr, "__pmPoolFree(0x%p, %d)\n", ptr, (int)size);
#endif
	    return;
	}
	i++;
    } while (i < npool);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPoolFree(0x%p, %d) [free, not pool size]\n",
	    ptr, (int)size);
#endif
    free(ptr);
    return;
}

void
__pmPoolCount(size_t size, int *alloc, int *free)
{
    int		i;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    nurd_t	*np;
	    int		count = 0;
	    np = (nurd_t *)poolctl[i].pc_head;
	    while (np != NULL) {
		count++;
		np = np->next;
	    }
	    *alloc = poolctl[i].alloc;
	    *free = count;
	    return;
	}
	i++;
    } while (i < npool);

    /* not one of the sizes in the pool! */
    *alloc = *free = 0;
    return;
}
#if defined(IRIX6_5) 
#pragma optional __pmPoolCount
#endif
