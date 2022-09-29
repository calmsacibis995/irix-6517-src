/**************************************************************************
 *									  *
 *	 	Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Id: dfile_subr.c,v 1.5 1997/11/10 23:42:05 ethan Exp $"

#include <ksys/vfile.h>
#include "dfile_private.h"
#include "invk_dsfile_stubs.h"

void
vfile_export(
	vfile_t		*vf,
	vfile_export_t	*vfe)
{
	service_t	svc;
	bhv_desc_t	*bdp;

	BHV_READ_LOCK(&vf->vf_bh);
retry:
	bdp = BHV_HEAD_FIRST(&vf->vf_bh);
	if (BHV_OPS(bdp) == &dcfile_ops) {
		dcfile_t	*dcp;

		dcp = BHV_TO_DCFILE(bdp);
		vfe->vfe_handle = dcp->dcf_handle;
		SERVICE_MAKE(svc, cellid(), SVC_VFILE);
		HANDLE_MAKE(vfe->vfe_exporter, svc, vf);
	} else {
		dsfile_t	*dsp;

		/*
		 * vfile server
		 */
		dsp = vfile_to_dsfile(vf);
		if (dsp == NULL)
			goto retry;
		vfe->vfe_handle = dsp->dsf_handle;
		vfe->vfe_exporter = vfe->vfe_handle;
	}
	/*
	 * take an extra local ref until the import is done
	 */
	VFILE_REF_HOLD(vf);
	/*
	 * Now export the underlying objects
	 */
	if (VF_IS_VNODE(vf)) {
		vfe->vfe_flag = 0;
		cfs_vnexport(VF_TO_VNODE(vf), &vfe->vfe_vnexport);
	} else {
		ASSERT(VF_IS_VSOCK(vf));
		vfe->vfe_flag = FSOCKET;
		vsock_export(VF_TO_VSOCK(vf), &vfe->vfe_vsock_export);
	}
	BHV_READ_UNLOCK(&vf->vf_bh);
}

void
vfile_export_end(
	vfile_t		*vf)
{

	VFILE_REF_RELEASE(vf);
}

void
vfile_import_node2(
	vfile_export_t	*vfe,
	vfile_t		*vf)
{
	vnode_t		*vp;
	vsock_t		*vs;

	if (vfe->vfe_flag == FSOCKET) {
		vsock_import(&vfe->vfe_vsock_export, &vs);
		ASSERT(vs != NULL);
		if (VF_TO_VSOCK(vf) == NULL)
			VF_SET_DATA(vf, vs);
		else {
			ASSERT(VF_TO_VSOCK(vf) == vs);
			(void)vsock_drop_ref(vs);
		}
	} else {
		cfs_vnimport(&vfe->vfe_vnexport, &vp);
		ASSERT(vp != NULL);
		if (VF_TO_VNODE(vf) == NULL)
			VF_SET_DATA (vf, vp);
		else {
			ASSERT(VF_TO_VNODE(vf) == vp);
			VN_RELE(vp);
		}
	}
}
	
void
vfile_import(
	vfile_export_t	*vfe,
	vfile_t		**vfp)
{
	dcfile_t	*dcp;
	vfile_t		*vf;
	tk_set_t	already;
	tk_set_t	granted;
	tk_set_t	refused;
	int		error;
	off_t		offset;
	int		flags;
	/* REFERENCED */
	int		msgerr;
	extern tkc_ifstate_t dcfile_tclient_iface;

	do {
		if (dcfile_lookup_handle(&vfe->vfe_handle, &dcp) == 0) {
			vf = DCFILE_TO_VFILE(dcp);
			vfile_import_node2(vfe, vf);
			*vfp = vf;
			return;
		}

		/*
		 * get the token from the DS
		 */
		msgerr = invk_dsfile_obtain(HANDLE_TO_SERVICE(vfe->vfe_handle),
				  cellid(), DFILE_EXISTENCE_TOKENSET,
				  TK_NULLSET, 0, &already, &granted, &refused,
				  &vfe->vfe_handle, &offset, &flags,
				  &error);
		ASSERT(!msgerr);
		if (error == EMIGRATED) {
			/*
			 * DS has moved - try in the new place
			 */
			error = 0;
			continue;
		} else if (error) {
			panic("vfile_import");
		}
		if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, granted)) {
			dcfile_create(&dcp, NULL, &vfe->vfe_handle);
			vf = DCFILE_TO_VFILE(dcp);
			vfile_import_node2(vfe, vf);
			vf->vf_flag = flags;
			*vfp = vf;
			return;
		}
	} while (1);
}
