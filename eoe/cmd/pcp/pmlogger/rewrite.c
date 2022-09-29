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

#ident "$Id: rewrite.c,v 1.4 1997/07/25 07:00:02 markgw Exp $"

#include <stdio.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"

/*
 * PDU for pmResult (PDU_RESULT) -- from libpcp/src/p_fetch.c
 */

typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;

typedef struct {
    __pmPDUHdr		hdr;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

__pmPDU *
rewrite_pdu(__pmPDU *pb, int version)
{
    result_t		*pp;
    int			numpmid;
    int			valfmt;
    int			numval;
    int			vsize;
    vlist_t		*vlp;
    pmValueSet		*vsp;
    pmValueBlock	*vbp;
    int			i;
    int			j;

    if (version == PM_LOG_VERS02)
	return pb;

    if (version == PM_LOG_VERS01) {
	pp = (result_t *)pb;
	numpmid = ntohl(pp->numpmid);
	vsize = 0;
	for (i = 0; i < numpmid; i++) {
	    vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	    numval = ntohl(vlp->numval);
	    valfmt = ntohl(vlp->valfmt);
	    for (j = 0; j < numval; j++) {
		vsp = (pmValueSet *)vlp;
		if (valfmt == PM_VAL_INSITU)
		    continue;
		vbp = (pmValueBlock *)&pb[ntohl(vsp->vlist[j].value.lval)];
		if (vbp->vtype == PM_TYPE_FLOAT) {
		    /* suck FLOAT back from pmValueBlock and make INSITU */
		    vlp->valfmt = htonl(PM_VAL_INSITU);
		    memcpy(&vsp->vlist[j].value.lval, &vbp->vbuf, sizeof(float));
		}
		vbp->vtype = 0;
	    }
	    vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	    if (numval > 0)
		vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
	    if (numval < 0)
		vlp->numval = htonl(XLATE_ERR_2TO1(numval));
	}

	return pb;
    }

    fprintf(stderr, "Errors: do not know how to re-write the PDU buffer for a version %d archive\n", version);
    exit(1);
    /*NOTREACHED*/
}
