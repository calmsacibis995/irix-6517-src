/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _DS_VSOCK_H_
#define _DS_VSOCK_H_
#ident "$Revision: 1.11 $"

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>

#define VSOCK_TOKEN_EXIST       0
#define VSOCK_NTOKENS           1

#define VSOCK_EXISTENCE_TOKEN           TK_MAKE_SINGLETON(VSOCK_TOKEN_EXIST, TK_READ)
#define VSOCK_EXISTENCE_TOKENSET        TK_MAKE(VSOCK_TOKEN_EXIST, TK_READ)

/*
 * Vsock handle.   This contains two kinds of identifier.   The
 * obj_handle_t contains an identifier which is actually a pointer
 * to the object in server space, and which can be used in calls
 * only if we hold an existence token, and can be sure that the
 * object exists.   The abstract ident is an identifier that a
 * server can use to look up the object to retrieve its current
 * address in server space if it could have changed - for example
 * after migration.
 */
typedef	__uint32_t	vsock_gen_t;
typedef struct vsock_handle {
	obj_handle_t	vs_objhand;             /* generic object handle */
	vsock_gen_t	vs_ident;		/* abstract ident */
} vsock_handle_t;

typedef struct dcvsock {
	bhv_desc_t      dcvs_bhv;               /* behaviour */
	TKC_DECL(dcvs_tclient, VSOCK_NTOKENS);
	vsock_handle_t	dcvs_handle;
	unsigned long   dcvs_flags;
} dcvsock_t;

typedef struct dsvsock {
	TKS_DECL(dsvsock_tclient, VSOCK_NTOKENS);
	TKS_DECL(dsvsock_tserver, VSOCK_NTOKENS);
	bhv_desc_t      dsvs_bhv;
	vsock_gen_t	dsvs_id;
	u_int           dsvs_flags;
	vsock_handle_t	dsvs_handle;
} dsvsock_t;
#define	DSVS_TO_BHV(dsp)	(&((dsp)->dsvs_bhv))
#define	DSVS_TO_ID(dsp)	((dsp)->dsvs_id)
#define VSOCK_IS_DCVS(vs)						\
	(BHV_OPS(VSOCK_TO_FIRST_BHV(vs)) == &dcsock_vector)
#define	BHV_TO_DCVS(bdp)						\
	(ASSERT(BHV_OPS(bdp) == &dcsock_vector), (dcvsock_t *)BHV_PDATA(bdp))
#define	VSOCK_TO_DCVS(vs) 	BHV_TO_DCVS(VSOCK_TO_FIRST_BHV(vs))
#define	DCVS_TO_HANDLE(dcp)	((dcp)->dcvs_handle)

#define DCVS_TO_SERVICE(dcp)	HANDLE_TO_SERVICE((dcp)->dcvs_handle.vs_objhand)
#define DCVS_TO_OBJID(dcp)	HANDLE_TO_OBJID((dcp)->dcvs_handle.vs_objhand)

#define VS_HANDLE_TO_SERVICE(handle)   HANDLE_TO_SERVICE((handle).vs_objhand)
#define VS_HANDLE_IS_NULL(handle)      HANDLE_IS_NULL((handle).vs_objhand)

#define	VSOCK_HANDLE_MAKE(handle, objid, ident)				\
{									\
	HANDLE_MAKE((handle).vs_objhand, vsock_service_id, objid);	\
	(handle).vs_ident = ident;					\
}
#define	VSOCK_HANDLE_EQU(h1, h2)					\
	(HANDLE_EQU((h1).vs_objhand, (h2).vs_objhand) &&		\
	(h1).vs_ident == (h2).vs_ident)
#define	DSVS_HANDLE_MAKE(handle, dsp)					\
{									\
	VSOCK_HANDLE_MAKE(handle, DSVS_TO_BHV(dsp), DSVS_TO_ID(dsp));	\
}

#define BHV_TO_DSVSOCK(bdp) \
	(ASSERT(BHV_POSITION(bdp) == VSOCK_POSITION_DS), \
	(dsvsock_t *)(BHV_PDATA(bdp)))

extern struct vsocket_ops       dssock_vector;
extern struct vsocket_ops       dcsock_vector;

extern  void    dssock_alloc (dsvsock_t **);
extern	void	dcsock_initialize(void);
extern	void	dssock_initialize(void);
extern	void	vsock_export(vsock_t *, vsock_handle_t  *);
extern	void	vsock_handle(vsock_t *, vsock_handle_t *);
extern	void	vsock_import(vsock_handle_t *, vsock_t **);
extern	int	dcsock_existing(vsock_t *, vsock_handle_t *);
extern	int	dcsock_create(struct vsocket	*, int, 
	struct vsocket	**, int, int);

extern  void    idbg_dsvsock1(char *, dsvsock_t *);
extern  void    idbg_dcvsock1(char *, dcvsock_t *);
extern	void	idbg_vsockhandle(char *, vsock_handle_t *);

#endif  /* _DS_VSOCK_H_ */
