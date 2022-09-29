/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.2 $"

#include "cxfs.h"
#include "dcxvn.h"
#include "dsxvn.h"
#include "sys/idbgentry.h"

void cxfs_prdcxvn(dcxvn_t *dcp)
{
	qprintf("dcxvn_t %x%s\n", dcp, dcp ? " with:" : "");
	if (dcp)
		qprintf("	dcvnp %x vattrp %x dcxvfs %x tokensp %x extentsp %x size %d\n",
			dcp->dcx_dcvn, dcp->dcx_vap, dcp->dcx_vfs,
				dcp->dcx_tokens, dcp->dcx_extents,
				dcp->dcx_ext_count);
}

void cxfs_prdsxvn(dsxvn_t *dsp)
{
	qprintf("dsxvn_t %x%s\n", dsp, dsp ? " with:" : "");
	if (dsp)
		qprintf("	dsvnp %x bdp %x tkclient %x tserver %x\n",
			dsp->dsx_dsvn, dsp->dsx_bdp, dsp->dsx_tclient,
				dsp->dsx_tserver);
}
