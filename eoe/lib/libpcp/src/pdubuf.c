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

#ident "$Id: pdubuf.c,v 2.7 1999/08/17 04:13:41 kenmcd Exp $"

#include <stdio.h>
#include <malloc.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

#define PDU_CHUNK	1024	/* unit of space allocation for PDU buffer */

typedef struct bufctl {
    struct bufctl	*bc_next;
    int			bc_size;
    int			bc_pincnt;
    char		*bc_buf;
    char		*bc_bufend;
} bufctl_t;

static bufctl_t	*buf_free;
static bufctl_t	*buf_pin;
static bufctl_t	*buf_pin_tail;

#ifdef PCP_DEBUG
static void
pdubufdump(void)
{
    bufctl_t	*pcp;

    if (buf_free != NULL) {
	fprintf(stderr, "   free pdubuf[size]:");
	for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next)
	    fprintf(stderr, " 0x%p[%d]", pcp->bc_buf, pcp->bc_size);
	fputc('\n', stderr);
    }

    if (buf_pin != NULL) {
	fprintf(stderr, "   pinned pdubuf[pincnt]:");
	for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next)
	    fprintf(stderr, " 0x%p[%d]", pcp->bc_buf, pcp->bc_pincnt);
	fputc('\n', stderr);
    }
}
#endif

__pmPDU *
__pmFindPDUBuf(int need)
{
    bufctl_t	*pcp;

    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    break;
    }
    if (pcp == NULL) {
	if ((pcp = (bufctl_t *)malloc(sizeof(*pcp))) == NULL)
	    return NULL;
	pcp->bc_pincnt = 0;
	pcp->bc_size = PDU_CHUNK * (1 + need/PDU_CHUNK);
	if ((pcp->bc_buf = (char *)valloc(pcp->bc_size)) == NULL) {
	    free(pcp);
	    return NULL;
	}
	pcp->bc_next = buf_free;
	pcp->bc_bufend = &pcp->bc_buf[pcp->bc_size];
	buf_free = pcp;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "__pmFindPDUBuf(%d) -> 0x%p\n", need, pcp->bc_buf);
	pdubufdump();
    }
#endif

    return (__pmPDU *)pcp->bc_buf;
}

void
__pmPinPDUBuf(void *handle)
{
    bufctl_t	*pcp;
    bufctl_t	*prior = NULL;

    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_buf <= (char *)handle && (char *)handle < pcp->bc_bufend)
	    break;
	prior = pcp;
    }

    if (pcp != NULL) {
	/* first pin for this buffer, move between lists */
	if (prior == NULL)
	    buf_free = pcp->bc_next;
	else
	    prior->bc_next = pcp->bc_next;
	pcp->bc_next = NULL;
	if (buf_pin_tail != NULL)
	    buf_pin_tail->bc_next = pcp;
	buf_pin_tail = pcp;
	if (buf_pin == NULL)
	    buf_pin = pcp;
	pcp->bc_pincnt = 1;
    }
    else {
	for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	    if (pcp->bc_buf <= (char *)handle && (char *)handle < pcp->bc_bufend)
		break;
	}
	if (pcp != NULL)
	    pcp->bc_pincnt++;
	else {
	    __pmNotifyErr(LOG_WARNING, "__pmPinPDUBuf: 0x%x not in pool!", handle);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF)
		pdubufdump();
#endif
	    return;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPinPDUBuf(0x%p) -> pdubuf=0x%p, cnt=%d\n",
	    handle, pcp->bc_buf, pcp->bc_pincnt);
#endif
    return;
}

int
__pmUnpinPDUBuf(void *handle)
{
    bufctl_t	*pcp;
    bufctl_t	*prior = NULL;

    for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_buf <= (char *)handle && (char *)handle < &pcp->bc_buf[pcp->bc_size])
	    break;
	prior = pcp;
    }
    if (pcp == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDUBUF) {
	    fprintf(stderr, "__pmUnpinPDUBuf(0x%p) -> fails\n", handle);
	    pdubufdump();
	}
#endif
	return 0;
    }

    if (--pcp->bc_pincnt == 0) {
	if (prior == NULL)
	    buf_pin = pcp->bc_next;
	else
	    prior->bc_next = pcp->bc_next;
	if (buf_pin_tail == pcp)
	    buf_pin_tail = prior;
	
	pcp->bc_next = buf_free;
	buf_free = pcp;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmUnpinPDUBuf(0x%p) -> pdubuf=0x%p, pincnt=%d\n",
		handle, pcp->bc_buf, pcp->bc_pincnt);
#endif

    return 1;
}

void
__pmCountPDUBuf(int need, int *alloc, int *free)
{
    bufctl_t	*pcp;
    int		count;

    count = 0;
    for (pcp = buf_pin; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    count++;
    }
    *alloc = count;

    count = 0;
    for (pcp = buf_free; pcp != NULL; pcp = pcp->bc_next) {
	if (pcp->bc_size >= need)
	    count++;
    }
    *free = count;
    *alloc += count;

    return;
}
#if defined(IRIX6_5) 
#pragma optional __pmCountPDUBuf
#endif
