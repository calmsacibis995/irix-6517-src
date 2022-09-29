/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_SESSION_DSESSION_H_
#define	_SESSION_DSESSION_H_

#ident "$Id: dsession.h,v 1.4 1997/04/12 18:16:18 emb Exp $"

#include <ksys/behavior.h>
#include <ksys/cell/service.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>
#include <ksys/vsession.h>
#include <ksys/cell/wp.h>
#include <ksys/cell/object.h>
#include "dproc.h"

/*
 * Token class for distributed session synchronization.
 */
#define	VSESSION_TOKEN_EXIST		0
#define	VSESSION_TOKEN_MEMBER		1
#define	VSESSION_NTOKENS		2

/*
 * The EXISTENCE token ensures that a session exists.
 * Although it may be empty.
 */
#define	VSESSION_EXISTENCE_TOKEN		\
	TK_MAKE_SINGLETON(VSESSION_TOKEN_EXIST, TK_READ)
#define	VSESSION_EXISTENCE_TOKENSET	\
	TK_MAKE(VSESSION_TOKEN_EXIST, TK_READ)

/*
 * The MEMBER token asserts that there are members of
 * the session.  Used to trigger session tear-down.
 */
#define	VSESSION_MEMBER_TOKEN		\
	TK_MAKE_SINGLETON(VSESSION_TOKEN_MEMBER, TK_READ)
#define	VSESSION_MEMBER_TOKENSET	\
	TK_MAKE(VSESSION_TOKEN_MEMBER, TK_READ)

typedef struct dcsession {
	TKC_DECL(dcsess_tclient, VSESSION_NTOKENS);
	obj_handle_t	dcsess_handle;
	bhv_desc_t	dcsess_bhv;
} dcsession_t;

typedef struct dssession {
	TKC_DECL(dssess_tclient, VSESSION_NTOKENS);
	TKS_DECL(dssess_tserver, VSESSION_NTOKENS);
	int		dssess_notify_idle;
	bhv_desc_t	dssess_bhv;
} dssession_t;

/*
 * Macros.
 */
#define	BHV_IS_DSSESSION(b)	(BHV_OPS(b) == &dssession_ops)
#define	BHV_IS_DCSESSION(b)	(BHV_OPS(b) == &dcsession_ops)

#define BHV_TO_DCSESSION(bdp) \
	(ASSERT(BHV_IS_DCSESSION(bdp)), \
	(dcsession_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DSSESSION(bdp) \
	(ASSERT(BHV_IS_DSSESSION(bdp)), \
	(dssession_t *)(BHV_PDATA(bdp)))

#define	DCSESSION_TO_SERVICE(dcp)	\
	HANDLE_TO_SERVICE((dcp)->dcsess_handle)
#define	DCSESSION_TO_OBJID(dcp)		\
	HANDLE_TO_OBJID((dcp)->dcsess_handle)

extern session_ops_t dssession_ops;
extern session_ops_t dcsession_ops;

static __inline service_t
sid_to_service(pid_t sid)
{
	service_t	svc = pid_to_service(sid);
	svc.s_svcnum &= ~SVC_PROCESS_MGMT;
	svc.s_svcnum = SVC_SESSION;
	return svc;
}
#define sid_is_local(sid)	pid_is_local(sid)

extern int dcsession_create(int, service_t, vsession_t **);
extern vsession_t *dssession_create(pid_t);
extern void vsession_interpose(vsession_t *vsession);

extern void idbg_dssession_print(struct dssession *);
extern void idbg_dcsession_print(struct dcsession *);
struct psession;
extern void idbg_psession_print(struct psession *);

#endif	/* _SESSION_DSESSION_H_ */
