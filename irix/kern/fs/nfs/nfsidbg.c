/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* 
 * NFS debugging print routines
 */

#include "types.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "export.h"
#include <ksys/behavior.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>

extern void idbg_svdump(sv_t *);
extern int nfs3_fstype, nfs_fstype;

/*
 *	kp nfs addr
 *	addr == : address of mount table entry
 * Behavior pointer is first thing once we get to it.
 */
void
idbg_nfs(struct vfs *vfsp)
{
	struct mntinfo *mi;

	if (vfsp == NULL || (__psint_t)vfsp == -1) {
		return;
	}

	if (vfsp->vfs_fstype != nfs_fstype && 
	    vfsp->vfs_fstype != nfs3_fstype) {
		return;
	}

	mi = vfs_bhvtomi(bhv_base_unlocked(VFS_BHVHEAD(vfsp)));

	qprintf("  host \"%s\" rootvp 0x%x refct %d %s\n",
		mi->mi_hostname, mi->mi_rootvp,
		mi->mi_refct, (mi->mi_flags & MIF_HARD) ? "hard" : "soft");
	qprintf("  tsize %d stsize %d bsize %d timeo %d retrans %d mntno %d\n",
		mi->mi_tsize, mi->mi_stsize, mi->mi_bsize,
		mi->mi_timeo, mi->mi_retrans, mi->mi_mntno);
	qprintf("  rootfsid %d fsidmaps 0x%x rnodes 0x%x lock 0x%x\n",
		mi->mi_rootfsid, mi->mi_fsidmaps, mi->mi_rnodes,
		&mi->mi_lock);
	qprintf("async_reqs 0x%x threads %d\n",
		mi->mi_async_reqs, mi->mi_threads);
}

void
idbg_exportfs(__psint_t x)
{
	extern	struct	exportinfo	*exihashtab[];
	struct exportinfo *exi;
	int	i;

	if (x != -1L)
		return;

	qprintf("exihashtab:\n");
	for (i = 0; i < EXIHASHTABSIZE; i++)
		for (exi = exihashtab[i]; exi; exi = exi->exi_next)
			qprintf("slot %d exi 0x%x fsid[0] 0x%x fsid[1] 0x%x fid 0x%x\n",
				i,
				exi,
				exi->exi_fsid.val[0],
				exi->exi_fsid.val[1],
				exi->exi_fid);
}


/*
 * Print the contents of an rnode (called from idbg_vnbhv)
 */
void
idbg_prrnode(bhv_desc_t *bdp, vtype_t vtype, int header)
{
	struct rnode *rp;

	rp = bhvtor(bdp);
	if (header)
		qprintf("rnode: addr 0x%x\n", rp);
	qprintf(
	"     next 0x%x prevp 0x%x error %d lastr %d &vnode 0x%x\n",
		rp->r_next,
		rp->r_prevp,
		rp->r_error,
		rp->r_lastr,
		rtov(rp));
	qprintf(
	"     iocount %d attrtime 0x%x flags %s%s%s%s%s%s\n",
		rp->r_iocount, rp->r_nfsattrtime.tv_sec,
		rp->r_flags & RLOCKED ? "RLOCKED " : "",
		rp->r_flags & REOF ? "REOF " : "",
		rp->r_flags & RDIRTY ? "RDIRTY " : "",
		rp->r_flags & RVNODEWAIT ? "RVNODEWAIT " : "",
		rp->r_flags & RV3 ? "RV3 " : "",
		rp->r_flags == 0 ? "NONE" : "");
	qprintf(
	"     fhandle: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		rp->r_fh.fh_data[0],
		rp->r_fh.fh_data[1],
		rp->r_fh.fh_data[2],
		rp->r_fh.fh_data[3],
		rp->r_fh.fh_data[4],
		rp->r_fh.fh_data[5],
		rp->r_fh.fh_data[6],
		rp->r_fh.fh_data[7]);
	if (vtype == VLNK) {
		qprintf(
	"     symtime 0x%x symlen %d symval 0x%x",
			rp->r_symtime, rp->r_symlen, rp->r_symval);
	} else {
		qprintf(
	"     unlcred 0x%x unlname 0x%x unldrp 0x%x\n",
			rp->r_unlcred, rp->r_unlname, rp->r_unldrp);
	}
	qprintf("     iowait\n");
	idbg_svdump(&rp->r_iowait);
	qprintf(
	" locktrips %d &lock 0x%x\n", rp->r_locktrips, &rp->r_rwlock);
	qprintf(
	"     cred 0x%x &attr 0x%x localsize %d size %d\n",
		rp->r_cred, &rp->r_nfsattr, rp->r_localsize, rp->r_size);
}

void
nfsidbginit(void)
{
	idbg_addfunc("nfs", idbg_nfs);
	idbg_addfunc("exportfs", idbg_exportfs);
	idbg_addfunc("rnode", idbg_prrnode);
}
