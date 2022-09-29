/*
 * Copyright 1999, Silicon Graphics, Inc.
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

#ident "$Id: match_inst.c,v 1.1 1999/04/28 10:06:17 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "pmapi.h"	/* _pmCtime only */
#include "impl.h"	/* _pmCtime only */
#include "dstruct.h"
#include "fun.h"
#include "show.h"

/*
 * x-arg1 is the bexp, x->arg2 is the regex
 */
void
cndMatch_inst(Expr *x)
{
    Expr        *arg1 = x->arg1;
    Expr        *arg2 = x->arg2;
    Truth	*ip1;
    Truth	*op;
    int		n;
    int         i;
    int		sts;
    int		mi;
    Metric	*m;

    EVALARG(arg1)
    ROTATE(x)


#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "match_inst(0x%p): regex handle=0x%p desire %s\n",
	    x, arg2->ring, x->op == CND_MATCH ? "match" : "nomatch");
	dumpExpr(x);
    }
#endif

    if (x->tspan > 0) {

	mi = 0;
	m = &arg1->metrics[mi++];
	i = 0;
	ip1 = (Truth *)(&arg1->smpls[0])->ptr;
	op = (Truth *)(&x->smpls[0])->ptr;

	for (n = 0; n < x->tspan; n++) {

	    if (!arg2->valid || !arg1->valid) {
		*op++ = DUNNO;
	    }
	    else if (x->e_idom <= 0) {
		*op++ = FALSE;
	    }
	    else {
		while (i >= m->m_idom) {
		    /*
		     * no more values, next metric
		     */
		    m = &arg1->metrics[mi++];
		    i = 0;
		}

		if (m->inames == NULL) {
		    *op++ = FALSE;
		}
		else {
		    sts = regexec((regex_t *)arg2->ring, m->inames[i], 0, NULL, 0);
#if PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL2) {
			if (x->op == CND_MATCH && sts != REG_NOMATCH) {
			    fprintf(stderr, "match_inst: inst=\"%s\" match && %s\n",
				m->inames[i],
				*ip1 == TRUE ? "true" :
				    (*ip1 == FALSE ? "false" :
					(*ip1 == DUNNO ? "unknown" : "bogus" )));

			}
			else if (x->op == CND_NOMATCH && sts == REG_NOMATCH) {
			    fprintf(stderr, "match_inst: inst=\"%s\" nomatch && %s\n",
				m->inames[i],
				*ip1 == TRUE ? "true" :
				    (*ip1 == FALSE ? "false" :
					(*ip1 == DUNNO ? "unknown" : "bogus" )));
			}
		    }
#endif
		    if ((x->op == CND_MATCH && sts != REG_NOMATCH) ||
			(x->op == CND_NOMATCH && sts == REG_NOMATCH))
			*op++ = *ip1 && TRUE;
		    else
			*op++ = *ip1 && FALSE;
		}
		i++;
	    }
	    ip1++;
	}
	x->valid++;
    }
    else
	x->valid = 0;

    x->smpls[0].stamp = arg1->smpls[0].stamp;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cndMatch_inst(0x%p) ...\n", x);
	dumpExpr(x);
    }
#endif
}

