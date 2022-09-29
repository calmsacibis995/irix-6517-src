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

#ident "$Id: freeresult.c,v 2.6 1997/08/21 04:46:59 markgw Exp $"

#include "pmapi.h"
#include "impl.h"

#define MAGIC PM_VAL_HDR_SIZE + sizeof(__int64_t)

/* Free result buffer routines */

void
__pmFreeResultValues(pmResult *result)
{
    register pmValueSet *pvs;
    register pmValueSet **ppvs;
    register pmValueSet **ppvsend;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "__pmFreeResultValues(0x%p) numpmid=%d\n",
	    result, result->numpmid);
    }
#endif

    if (result->numpmid == 0)
	return;

    ppvsend = &result->vset[result->numpmid];

    /* if _any_ vset[] -> an address within a pdubuf, we are done */
    for (ppvs = result->vset; ppvs < ppvsend; ppvs++) {
	if (__pmUnpinPDUBuf((void *)*ppvs))
	    return;
    }

    /* not created from a pdubuf, really free the memory */
    for (ppvs = result->vset; ppvs < ppvsend; ppvs++) {
	pvs = *ppvs;
	if (pvs->numval > 0 && pvs->valfmt == PM_VAL_DPTR) {
	    /* pmValueBlocks may be malloc'd as well */
	    int		j;
	    for (j = 0; j < pvs->numval; j++) {
		if (pvs->vlist[j].value.pval->vlen == MAGIC) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_PDUBUF) {
			fprintf(stderr, "__pmPoolFree(0x%p) pmValueBlock pmid=%s inst=%d\n",
			    pvs->vlist[j].value.pval, pmIDStr(pvs->pmid),
			    pvs->vlist[j].inst);
		    }
#endif
		    __pmPoolFree(pvs->vlist[j].value.pval, MAGIC);
		}
		else {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_PDUBUF) {
			fprintf(stderr, "free(0x%p) pmValueBlock pmid=%s inst=%d\n",
			    pvs->vlist[j].value.pval, pmIDStr(pvs->pmid),
			    pvs->vlist[j].inst);
		    }
#endif
		    free(pvs->vlist[j].value.pval);
		}
	    }
	}
	if (pvs->numval == 1) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF) {
		fprintf(stderr, "__pmPoolFree(0x%p) vset pmid=%s\n",
		    pvs, pmIDStr(pvs->pmid));
	    }
#endif
	    __pmPoolFree(pvs, sizeof(pmValueSet));
	}
	else {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF) {
		fprintf(stderr, "free(0x%p) vset pmid=%s\n",
		    pvs, pmIDStr(pvs->pmid));
	    }
#endif
	    free(pvs);
	}
    }
}

void
pmFreeResult(pmResult *result)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "pmFreeResult(0x%p)\n", result);
    }
#endif
    __pmFreeResultValues(result);
    free(result);
}
