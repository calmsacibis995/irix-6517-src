/*
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

#ident "$Id: xpc.c,v 1.10 1997/07/23 04:23:34 chatz Exp $"

#include <stdio.h>
#include <sgidefs.h>
#include <syslog.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "cluster.h"
#include "xpc.h"

extern int	errno;

typedef union {
    __uint32_t	half[2];	/* two of these per __uint64_t */
    __uint64_t	full;
} ctr_t;

/*
 * Warning - ENDIAN dependency ...
 * half[XPC_BOTTOM] is the least significant part
 */
#define XPC_TOP		0
#define XPC_BOTTOM	1

typedef struct hashnode {
    struct hashnode	*next;
    pmID		pmid;
    int			inst;
    ctr_t		ctr;
} HashNode;

typedef struct {
    int		nodes;
    int		hsize;
    HashNode	**hash;
} HashCtl;

static HashCtl	xpc_hash;

/*
 * The Hash functions are based on similar ones from PCP (libpcp/src/logmeta.c)
 *
 * These ones use a 32-bit key == pmid ^ inst to hash, and then scan for
 * equality of both pmid and inst.
 */

static HashNode *
HashSearch(pmID pmid, int inst, HashCtl *hcp)
{
    HashNode		*hp;
    unsigned int	key = pmid ^ inst;

    if (hcp->hsize == 0)
	return (HashNode *)0;

    for (hp = hcp->hash[key % hcp->hsize]; hp != (HashNode *)0; hp = hp->next) {
	if (hp->pmid == pmid && hp->inst == inst)
	    return hp;
    }
    return (HashNode *)0;
}

static HashNode *
HashAdd(pmID pmid, int inst, HashCtl *hcp)
{
    HashNode    	*hp;
    unsigned int	key = pmid ^ inst;
    unsigned int	k;

    hcp->nodes++;

    if (hcp->hsize == 0) {
	hcp->hsize = 1;	/* arbitrary number */
	if ((hcp->hash = (HashNode **)calloc(hcp->hsize, sizeof(HashNode *))) == (HashNode **)0) {
	    hcp->hsize = 0;
	    return (HashNode *)0;
	}
    }
    else if (hcp->nodes / 4 > hcp->hsize) {
	HashNode	*tp;
	HashNode	**old = hcp->hash;
	int		oldsize = hcp->hsize;

	hcp->hsize *= 2;
	if (hcp->hsize % 2) hcp->hsize++;
	if (hcp->hsize % 3) hcp->hsize += 2;
	if (hcp->hsize % 5) hcp->hsize += 2;
	if ((hcp->hash = (HashNode **)calloc(hcp->hsize, sizeof(HashNode *))) == (HashNode **)0) {
	    hcp->hsize = oldsize;
	    hcp->hash = old;
	    return (HashNode *)0;
	}
	/*
	 * re-link chains
	 */
	while (oldsize) {
	    for (hp = old[--oldsize]; hp != (HashNode *)0; ) {
		tp = hp;
		hp = hp->next;
		k = (tp->pmid ^ tp->inst);
		k = k % hcp->hsize;
		tp->next = hcp->hash[k];
		hcp->hash[k] = tp;
	    }
	}
	free(old);
    }

    if ((hp = (HashNode *)malloc(sizeof(HashNode))) == (HashNode *)0)
	return (HashNode *)0;

    k = key % hcp->hsize;
    hp->pmid = pmid;
    hp->inst = inst;
    hp->next = hcp->hash[k];
    hcp->hash[k] = hp;

    return hp;
}

__uint64_t *
XPCincr(pmID pmid, int inst, __uint32_t val)
{
    HashNode		*hp;
    ctr_t		*cp;
    static ctr_t	fake_ctr;
    extern int		errno;

    hp = HashSearch(pmid, inst, &xpc_hash);

    if (hp == (HashNode *)0) {
	/* first time for this pmid and inst */
	hp = HashAdd(pmid, inst, &xpc_hash);
	if (hp == (HashNode *)0) {
	    __pmNotifyErr(LOG_WARNING, "XPCincr: HashAdd(%s, %d, ...) failed: %s\n",
		    pmIDStr(pmid), inst, pmErrStr(-errno));
	    cp = &fake_ctr;
	}
	else
	    cp = &hp->ctr;
	cp->full = 0;
    }
    else
	cp = &hp->ctr;

    if (val < cp->half[XPC_BOTTOM]) {
	/* assume overflow */
#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_XPC)
	    __pmNotifyErr(LOG_DEBUG, "XPCincr: o'flow: pmid: %s inst: %d this: %x prior: %x\n",
		pmIDStr(pmid), inst, val, cp->half[XPC_BOTTOM]);
#endif
	cp->half[XPC_TOP]++;
    }
    cp->half[XPC_BOTTOM] = val;

    return &cp->full;
}

#ifdef PCP_DEBUG
void
XPCdump(void)
{
    HashNode		*hp;
    int			i;

    if (xpc_hash.hsize == 0) {
	fprintf(stderr, "\nHash[] empty\n");
	return;
    }

    fprintf(stderr, "\nHash[] %d entries\n", xpc_hash.nodes);
    for (i = 0; i < xpc_hash.hsize; i++) {
	fprintf(stderr, "\nHash[%d] ...\n", i);
	for (hp = xpc_hash.hash[i]; hp != (HashNode *)0; hp = hp->next) {
	    fprintf(stderr, "  %s", pmIDStr(hp->pmid));
	    if (hp->inst != PM_IN_NULL)
		fprintf(stderr, "[%d]", hp->inst);
	    fprintf(stderr, " %d|%d\n", hp->ctr.half[XPC_TOP], hp->ctr.half[XPC_BOTTOM]);

	}
    }
}
#endif
