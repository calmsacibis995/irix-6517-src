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

#ident "$Id: sortinst.c,v 2.1 1997/05/02 04:07:37 kenmcd Exp $"

#include <stdlib.h>
#include "pmapi.h"

static int
comp(const void *a, const void *b)
{
    pmValue	*ap = (pmValue *)a;
    pmValue	*bp = (pmValue *)b;

    return ap->inst - bp->inst;
}

void
pmSortInstances(pmResult *rp)
{
    int		i;

    for (i = 0; i < rp->numpmid; i++) {
	if (rp->vset[i]->numval > 1) {
	    qsort(rp->vset[i]->vlist, rp->vset[i]->numval, sizeof(pmValue), comp);
	}
    }
}
