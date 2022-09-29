/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.9 $"

/*
 * Vsock object relocation management.
 */

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <ksys/cell.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/relocation.h>
#include "./dsvsock.h"
#include "./vsock.h"

#include "invk_dsvsock_stubs.h"
#include "invk_dcvsock_stubs.h"

/*
 * Quiesce local client operations-in-progress.
 */
void
vsock_client_quiesce(
	vsock_t  *vso)
{
	bhv_head_t      *bhp = VS_BHV_HEAD(vso);

	/*
	 * Quiesce this client.
	 * Note: other object types will need to wake-up interruptible ops;
	 * vshm has none though.
	 */
	BHV_WRITE_LOCK(bhp);
}

/*
 * Unquiesce local client operations-in-progress.
 */
void
vsock_client_unquiesce(
	vsock_t  *vso)
{
	bhv_head_t	*bhp = VS_BHV_HEAD(vso);

	BHV_WRITE_UNLOCK(bhp);
}

/*
 * Map from a vsock to a vsock_handle.  The returned handle does
 * not have an associated existence token.
 */
void
vsock_export(
	vsock_t		*vso,
	vsock_handle_t	*handlep)	/* out */
{

	BHV_READ_LOCK(VS_BHV_HEAD(vso));
	if (VSOCK_IS_DCVS(vso)) {
		*handlep = DCVS_TO_HANDLE(VSOCK_TO_DCVS(vso));
		BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
	} else {
		vsock_handle(vso, handlep);
		BHV_READ_UNLOCK(VS_BHV_HEAD(vso));
	}
}

/*
 * Map from a vsock_handle to a vsocket, allocating one if
 * necessary.
 */
void
vsock_import(
	vsock_handle_t	*handlep,
	vsock_t		**vsop)	/* out */
{
	vsock_t		*vso = NULL;
	int		dom, type, proto;
	int		granted;
	int		try_cnt = 0;
	int		msgerr;

	if (VS_HANDLE_IS_NULL(*handlep)) {
		if (vsop) {
			*vsop = vso;
		}
		return;
	}
	while (1) {
		/*
		 * Search locally
		 */
		vsock_lookup_id (handlep, vsop);
		if (*vsop != NULL) {
			return;
		}
	
		msgerr = invk_dsvsock_obtain_exist(VS_HANDLE_TO_SERVICE(*handlep),
			cellid(), handlep, &dom, &type, &proto, &granted);
		ASSERT(!msgerr);
		if (granted) {
			vso = vsocket_alloc();
			vso->vs_type = type;
			vso->vs_protocol = proto;
			vso->vs_domain = dom;
	
			dcsock_existing(vso, handlep);
			vsock_enter_id (handlep, vso);
			if (vsop) {
				*vsop = vso;
			}
			return;
		}

		cell_backoff(++try_cnt);
	}
}

#define OBJ_TAG_VSOCK OBJ_SVC_TAG(SVC_VSOCK, 0)

int
vsock_obj_source_prepare(
	obj_manifest_t	*mftp,		/* IN/OUT object manifest */
	void		*v)		/* IN vsock */
{
	service_t	svc;
	vsock_t		*vp = (vsock_t *)v;
        vsock_handle_t	handle;
	obj_mft_info_t	minfo;

	vsock_export (vp, &handle);

	SERVICE_MAKE(svc, cellid(), SVC_VSOCK);
	HANDLE_MAKE(minfo.source.hndl, svc, vp);
	minfo.source.tag = OBJ_TAG_VSOCK;
	minfo.source.infop = &handle;
	minfo.source.info_size = sizeof(vsock_handle_t);
	obj_mft_info_put(mftp, &minfo);

	return 0;
}

int
vsock_obj_target_prepare(
	obj_manifest_t	*mftp,			/* IN object manifest */
	void		**v)			/* OUT virtual object */
{
	vsock_t		*vp;
	service_t	svc;
        vsock_handle_t	handle;
	obj_mft_info_t	minfo;
	/*REFERENCED*/
	int		error;

	minfo.source.tag = OBJ_TAG_VSOCK;
	minfo.source.infop = &handle;
	minfo.source.info_size = sizeof(vsock_handle_t);
	obj_mft_info_get(mftp, &minfo);

	vsock_import (&handle, &vp);
	ASSERT(vp);
	*v = vp;
	SERVICE_MAKE(svc, cellid(), SVC_VSOCK);
	HANDLE_MAKE(minfo.target.hndl, svc, vp);
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_put(mftp, &minfo);

	return 0;
}

/* ARGSUSED */
int
vsock_obj_source_bag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	source_bag)		/* IN/OUT object state */
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	return 0;
}

/* ARGSUSED */
int
vsock_obj_target_unbag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	return 0;
}

/* ARGSUSED */
int
vsock_obj_source_end(
	obj_manifest_t	*mftp)			/* IN object manifest */
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	return 0;
}

/* ARGSUSED */
int
vsock_obj_source_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	return 0;
}

/* ARGSUSED */
int
vsock_obj_target_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	return 0;
}

obj_relocation_if_t vsock_obj_iface = {
	vsock_obj_source_prepare,
	vsock_obj_target_prepare,
	NULL,
	NULL,
	vsock_obj_source_bag,
	vsock_obj_target_unbag,
	vsock_obj_source_end,
	vsock_obj_source_abort,
	vsock_obj_target_abort
};
