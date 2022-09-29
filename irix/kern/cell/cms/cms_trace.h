/*
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

#ifndef	_CMS_TRACE_H
#define	_CMS_TRACE_H

/*
 * Trace events for membership tracing.
 */
#define CMS_TR_ACCEPT	1
#define CMS_TR_PROPOSAL	2
#define CMS_TR_REQ_PROPOSAL	3
#define CMS_TR_MEMB_QUERY	4
#define CMS_TR_MEMB_REPLY	5
#define CMS_TR_RETRV_MESG	6
#define CMS_TR_RECV_MESG_INV_CELL	7
#define CMS_TR_RECV_MESG_INV_SEQ_NO	8
#define CMS_TR_RECV_MESG	9
#define CMS_TR_FRWD_DIR	10
#define CMS_TR_FRWD_INDIR	11
#define CMS_TR_MEMB_MESG	12
#define CMS_TR_BROAD_MESG	13
#define CMS_TR_PUSHB_MESG	14
#define CMS_TR_FAILURE		15
#define CMS_TR_MEMB_RECV_SET	16
#define CMS_TR_MEMB_FAIL	17
#define CMS_TR_MEMB_SUCC	18
#define CMS_TR_PROPOSAL_IGNORE	19
#define CMS_TR_MEMB_MIN		20
#define CMS_TR_DELAY_FSTOP	21
#define CMS_TR_NEW_STATE	22
#define	CMS_TR_CONN_SET		23
#define	CMS_TR_RESET_CELLS	24
#define	CMS_TR_AGE		25
#define CMS_TR_RECOVERY_COMP	26
#define CMS_TR_REC_COMP_MSG	27
#define	CMS_TR_TIMEOUT		28

#ifdef	_KERNEL
#include "sys/idbgentry.h"
#include "sys/ktrace.h"

extern	ktrace_t	*cms_trace;
#ifdef	DEBUG
#define	cms_trace(x, a1, a2, a3)	KTRACE6(cms_trace, \
			(void *)(__psint_t)x, \
			(void *)(__psint_t)cip->cms_state, \
			(void *)(__psint_t)a1, \
			(void *)(__psint_t)a2, (void *)(__psint_t)a3, 0)

extern void cms_trace_init(void);
#else
#define	cms_trace(x, a1, a2, a3)
#define	cms_trace_init()
#endif /* DEBUG */

#else /* _KERNEL */
#define	qprintf	printf
#define	cms_trace(x, a1, a2, a3) \
	cms_trace_enter(x, (void *)(__psint_t)(a1), (void *)(__psint_t)(a2), \
                        (void *)(__psint_t)(a3))
#define	cms_trace_init()
extern	void cms_trace_enter(int, void *, void *, void *);
#endif	/* KERNEL */

#endif /* _CMS_TRACE_H */
