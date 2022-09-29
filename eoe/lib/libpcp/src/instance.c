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

#ident "$Id: instance.c,v 2.18 1997/10/24 06:29:29 markgw Exp $"

#include <stdio.h>
#include <malloc.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

int
pmLookupInDom(pmInDom indom, const char *name)
{
    int		n;
    __pmPDU	*pb;
    __pmInResult	*result;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, &ctxp->c_origin, indom, PM_IN_NULL, name);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (n == PDU_INSTANCE) {
		    if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) >= 0) {
			n = result->instlist[0];
			__pmFreeInResult(result);
		    }
		}
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		/* We can safely cast away const here */
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, PM_IN_NULL, 
							  (char *)name, 
							  &result);
		else
		    n = dp->dispatch.version.two.instance(indom, PM_IN_NULL, 
							  (char *)name, 
							  &result, 
							  dp->dispatch.version.two.ext);
		if (n < 0 && dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		    n = XLATE_ERR_1TO2(n);
	    }
	    if (n >= 0) {
		n = result->instlist[0];
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogLookupInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, name);
	}
    }

    return n;
}

int
pmNameInDom(pmInDom indom, int inst, char **name)
{
    int		n;
    __pmPDU	*pb;
    __pmInResult	*result;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, &ctxp->c_origin, indom, inst, NULL);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (n == PDU_INSTANCE) {
		    if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) >= 0) {
			if ((*name = strdup(result->namelist[0]))== NULL)
			    n = -errno;
			__pmFreeInResult(result);
		    }
		}
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, inst, NULL, &result);
		else
		    n = dp->dispatch.version.two.instance(indom, inst, NULL, &result, dp->dispatch.version.two.ext);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	    if (n >= 0) {
		if ((*name = strdup(result->namelist[0])) == NULL)
		    n = -errno;
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    char	*tmp;
	    if ((n = __pmLogNameInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, inst, &tmp)) >= 0) {
		if ((*name = strdup(tmp)) == NULL)
		    n = -errno;
	    }
	}
    }

    return n;
}

int
pmGetInDom(pmInDom indom, int **instlist, char ***namelist)
{
    int			n;
    int			i;
    __pmPDU		*pb;
    __pmInResult		*result;
    int			need;
    __pmContext		*ctxp;
    char		*p;
    int			*ilist;
    char		**nlist;
    __pmDSO		*dp;

    /* avoid ambiguity when no instances or errors */
    *instlist = NULL;
    *namelist = NULL;
    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {

	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, &ctxp->c_origin, indom, PM_IN_NULL, NULL);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
		if (n == PDU_INSTANCE) {
		    if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) < 0)
			return n;
		    if (result->numinst == 0) {
			__pmFreeInResult(result);
			return 0;
		    }
		    need = 0;
		    for (i = 0; i < result->numinst; i++) {
			need += sizeof(**namelist) + strlen(result->namelist[i]) + 1;
		    }
		    if ((ilist = (int *)malloc(result->numinst * sizeof(result->instlist[0]))) == NULL) {
			__pmFreeInResult(result);
			return -errno;
		    }
		    if ((nlist = (char **)malloc(need)) == NULL) {
			free(ilist);
			__pmFreeInResult(result);
			return -errno;
		    }
		    *instlist = ilist;
		    *namelist = nlist;
		    p = (char *)&nlist[result->numinst];
		    for (i = 0; i < result->numinst; i++) {
			ilist[i] = result->instlist[i];
			strcpy(p, result->namelist[i]);
			nlist[i] = p;
			p += strlen(result->namelist[i]) + 1;
		    }
		    n = result->numinst;
		    __pmFreeInResult(result);
		}
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, PDU_BINARY, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, PM_IN_NULL, NULL,
					      &result);
		else
		    n = dp->dispatch.version.two.instance(indom, PM_IN_NULL, NULL,
					       &result,
					       dp->dispatch.version.two.ext);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	    if (n >= 0) {
		if (result->numinst == 0) {
		    __pmFreeInResult(result);
		    return 0;
		}
		need = 0;
		for (i = 0; i < result->numinst; i++) {
		    need += sizeof(**namelist) + strlen(result->namelist[i]) + 1;
		}
		if ((ilist = (int *)malloc(result->numinst * sizeof(result->instlist[0]))) == NULL) {
		    __pmFreeInResult(result);
		    return -errno;
		}
		if ((nlist = (char **)malloc(need))== NULL) {
		    free(ilist);
		    __pmFreeInResult(result);
		    return -errno;
		}
		*instlist = ilist;
		*namelist = nlist;
		p = (char *)&nlist[result->numinst];
		for (i = 0; i < result->numinst; i++) {
		    ilist[i] = result->instlist[i];
		    strcpy(p, result->namelist[i]);
		    nlist[i] = p;
		    p += strlen(result->namelist[i]) + 1;
		}
		n = result->numinst;
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    int		*insttmp;
	    char	**nametmp;
	    if ((n = __pmLogGetInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, &insttmp, &nametmp)) >= 0) {
		need = 0;
		for (i = 0; i < n; i++) {
		    need += sizeof(char *) + strlen(nametmp[i]) + 1;
		}
		if ((ilist = (int *)malloc(n * sizeof(insttmp[0]))) == NULL) {
		    return -errno;
		}
		if ((nlist = (char **)malloc(need)) == NULL) {
		    free(ilist);
		    return -errno;
		}
		*instlist = ilist;
		*namelist = nlist;
		p = (char *)&nlist[n];
		for (i = 0; i < n; i++) {
		    ilist[i] = insttmp[i];
		    strcpy(p, nametmp[i]);
		    nlist[i] = p;
		    p += strlen(nametmp[i]) + 1;
		}
	    }
	}
    }

    return n;
}

#ifdef PCP_DEBUG
void
__pmDumpInResult(FILE *f, const __pmInResult *irp)
{
    int		i;
    fprintf(f,"pmInResult dump from 0x%p for InDom %s (0x%x), numinst=%d\n",
		irp, pmInDomStr(irp->indom), irp->indom, irp->numinst);
    for (i = 0; i < irp->numinst; i++) {
	fprintf(f, "  [%d]", i);
	if (irp->instlist != NULL)
	    fprintf(f, " inst=%d", irp->instlist[i]);
	if (irp->namelist != NULL)
	    fprintf(f, " name=\"%s\"", irp->namelist[i]);
	fputc('\n', f);
    }
    return;
}
#endif

void
__pmFreeInResult(__pmInResult *res)
{
    int		i;

    if (res->namelist != NULL) {
	for (i = 0; i < res->numinst; i++) {
	    if (res->namelist[i] != NULL) {
		free(res->namelist[i]);
	    }
	}
	free(res->namelist);
    }
    if (res->instlist != NULL)
	free(res->instlist);
    free(res);
}
