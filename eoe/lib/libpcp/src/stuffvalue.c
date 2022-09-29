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

#ident "$Id: stuffvalue.c,v 1.6 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

int
__pmStuffValue(const pmAtomValue *avp, int aggr_len, pmValue *vp, int type)
{
    int		valfmt;
    size_t	need;

    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vp->value.lval = avp->ul;
	    valfmt = PM_VAL_INSITU;
	    break;

	case PM_TYPE_FLOAT:
	    need = PM_VAL_HDR_SIZE + sizeof(float);
	    if ((vp->value.pval = (pmValueBlock *)malloc(need)) == NULL)
		return -errno;
	    vp->value.pval->vlen = (int)need;
	    vp->value.pval->vtype = PM_TYPE_FLOAT;
	    memcpy((void *)vp->value.pval->vbuf, (void *)&avp->f, sizeof(float));
	    valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_DOUBLE:
	    need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
	    if ((vp->value.pval = (pmValueBlock *)__pmPoolAlloc(need)) == NULL)
		return -errno;
	    vp->value.pval->vlen = (int)need;
	    if (type == PM_TYPE_DOUBLE)
		vp->value.pval->vtype = PM_TYPE_DOUBLE;
	    else if (type == PM_TYPE_64)
		vp->value.pval->vtype = PM_TYPE_64;
	    else
		vp->value.pval->vtype = PM_TYPE_U64;
	    memcpy((void *)vp->value.pval->vbuf, (void *)&avp->ull, sizeof(*avp));
	    valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_AGGREGATE:
	    need = PM_VAL_HDR_SIZE + aggr_len;
	    if (aggr_len == sizeof(__int64_t))
		vp->value.pval = (pmValueBlock *)__pmPoolAlloc(need);
	    else
		vp->value.pval = (pmValueBlock *)malloc(need);
	    if (vp->value.pval == NULL)
		return -errno;
	    vp->value.pval->vlen = (int)need;
	    vp->value.pval->vtype = PM_TYPE_AGGREGATE;
	    memcpy((void *)vp->value.pval->vbuf, avp->vp, aggr_len);
	    valfmt = PM_VAL_DPTR;
	    break;
	    
	case PM_TYPE_STRING:
	    need = PM_VAL_HDR_SIZE + strlen(avp->cp) + 1;
	    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
		vp->value.pval = (pmValueBlock *)__pmPoolAlloc(need);
	    else
		vp->value.pval = (pmValueBlock *)malloc(need);
	    if (vp->value.pval == NULL)
		return -errno;
	    vp->value.pval->vlen = (int)need;
	    vp->value.pval->vtype = PM_TYPE_STRING;
	    strcpy((char *)vp->value.pval->vbuf, avp->cp);
	    valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_AGGREGATE_STATIC:
	    vp->value.pval = (pmValueBlock *)avp->vp;
	    valfmt = PM_VAL_SPTR;
	    break;

	default:
	    valfmt = PM_ERR_GENERIC;
    }
    return valfmt;
}
