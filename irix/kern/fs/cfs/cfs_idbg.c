/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: cfs_idbg.c,v 1.14 1997/08/22 18:30:52 mostek Exp $"

#include "sgidefs.h"
#include "sys/types.h"
#include "dvn.h"
#include "dvfs.h"
#include "sys/idbgentry.h"
#include "sys/param.h"
#include <fs/cell/cxfs_intfc.h>

static void idbg_dcvf(__psint_t);
static void idbg_dcvflist(__psint_t);
static void idbg_dcvn(__psint_t);
static void idbg_dcvnlist(__psint_t);
static void idbg_dsvn(__psint_t);
static void idbg_dsvnlist(__psint_t);
#if DVN_TRACING
static void idbg_dvntrace(__psint_t);
#endif
static void idbg_dcstats();
static void idbg_dsstats();

extern handle_hashtab_t dcvn_hashtab[DCVN_NHASH];     /* dcvn hash table */
extern handle_hashtab_t dsvn_hashtab[DSVN_NHASH];     /* dsvn hash table */

extern dsstat_info_t dsstats;		 /* DC CFS statistics */
extern dcstat_info_t dcstats;		 /* DS CFS statistics */

static void prdcvn(dcvn_t *, int);
static void prdsvn(dsvn_t *, int);

void
cfs_idbg_init(void)
{
	idbg_addfunc("dcvn", idbg_dcvn);
	idbg_addfunc("dcvnlist", idbg_dcvnlist);
	idbg_addfunc("dsvn", idbg_dsvn);
	idbg_addfunc("dsvnlist", idbg_dsvnlist);
	idbg_addfunc("dcvf", idbg_dcvf);
	idbg_addfunc("dcvflist", idbg_dcvflist);
#if DVN_TRACING
	idbg_addfunc("dvntrace", idbg_dvntrace);
#endif
	idbg_addfunc("dcstats", idbg_dcstats);
	idbg_addfunc("dsstats", idbg_dsstats);
}

static void
idbg_dcvflist(__psint_t x)
{
	dcvfs_t *curdcp;
        kqueue_t *kq = &(((handle_hashtab_t *)x)->hh_kqueue);

	for (curdcp = (dcvfs_t *)kqueue_first(kq);
             curdcp != (dcvfs_t *)kqueue_end(kq);
             curdcp = (dcvfs_t *)kqueue_next(&curdcp->dcvfs_kqueue)) {
		qprintf("dcvfs @ 0x%x\n",curdcp);
		qprintf("  kq_next 0x%x, kq_prev 0x%x\n",
		curdcp->dcvfs_kqueue.kq_next, curdcp->dcvfs_kqueue.kq_prev);
		qprintf("  &bhv 0x%x\n", &curdcp->dcvfs_bhv);
		idbg_dcvf((__psint_t)curdcp);
        }

}
	
static void
idbg_dcvf(__psint_t x)
{
	dcvfs_t *dcp = (dcvfs_t *)x;
	dcvn_t	*dcvn;

	qprintf("  vfs 0x%x fstype %d tclient 0x%x\n",
		DCVFS_TO_VFS(dcp), dcp->dcvfs_fstype, dcp->dcvfs_tclient);
	qprintf("  handle 0x%x => service.cell %d objid 0x%x gen %d\n",
		dcp->dcvfs_handle,
                CFS_HANDLE_TO_SERVICE(dcp->dcvfs_handle).s_cell,
                CFS_HANDLE_TO_OBJID(dcp->dcvfs_handle),
                CFS_HANDLE_TO_GEN(dcp->dcvfs_handle));
	qprintf("  dcvlist 0x%x, rootvp 0x%x, reclaims 0x%x\n",
		dcp->dcvfs_dcvlist, dcp->dcvfs_rootvp, dcp->dcvfs_reclaims);

	qprintf("  Print dcvnq list\n");
	dcvn = dcp->dcvfs_dcvlist;
	while (dcvn) {
		idbg_dcvn((__psint_t)dcvn);
		dcvn = dcvn->dcv_next;
	}
}

static void
idbg_dcvn(__psint_t x)
{
	dcvn_t	*dcp = (dcvn_t *)x;

	prdcvn(dcp, -1);
}

static void
idbg_dsvn(__psint_t x)
{
	dsvn_t	*dsp = (dsvn_t *)x;

	prdsvn(dsp, -1);
}

static void
idbg_dcvnlist(__psint_t x)
{
	handle_hashtab_t *hh = (handle_hashtab_t *) x;
        kqueue_t 	*kq;
	dcvn_t		*curdcp;
	int 		index;

	if (!hh)
		hh = &dcvn_hashtab[0];

	for (index = 0; index < DCVN_NHASH; index++) {
		kq = &hh->hh_kqueue;
		for (curdcp = (dcvn_t *)kqueue_first(kq);
		     curdcp != (dcvn_t *)kqueue_end(kq);
		     curdcp = (dcvn_t *)kqueue_next(&curdcp->dcv_kqueue)) {
			prdcvn(curdcp, index);
		}
		hh++;
	}
}

static void
idbg_dsvnlist(__psint_t x)
{
	handle_hashtab_t *hh = (handle_hashtab_t *) x;
        kqueue_t 	*kq;
	dsvn_t		*curdsp;
	int 		index;

	if (!hh)
		hh = &dsvn_hashtab[0];

	for (index = 0; index < DSVN_NHASH; index++) {
		kq = &hh->hh_kqueue;
		for (curdsp = (dsvn_t *)kqueue_first(kq);
		     curdsp != (dsvn_t *)kqueue_end(kq);
		     curdsp = (dsvn_t *)kqueue_next(&curdsp->dsv_kqueue)) {
			prdsvn(curdsp, index);
		}
		hh++;
	}
}

void
prdcvn(dcvn_t *dcp, int index)
{
	if (dcp) {
		qprintf("dcvn @ 0x%x: flags 0x%x kq.next 0x%x kq.prev 0x%x\n",
			dcp, dcp->dcv_flags, dcp->dcv_kqueue.kq_next,
				dcp->dcv_kqueue.kq_prev);
		qprintf("  error %d dcvfs 0x%x vrgen %d empty_pcache_cnt %d\n",
			dcp->dcv_error, dcp->dcv_dcvfs,
			dcp->dcv_vrgen, dcp->dcv_empty_pcache_cnt);
		qprintf("  tclient 0x%x next 0x%x prev 0x%x bd 0x%x\n",
			dcp->dcv_tclient, dcp->dcv_next, dcp->dcv_prev, dcp->dcv_bd); 
		qprintf("  vp 0x%x v_count %d &vattr 0x%x\n", DCVN_TO_VNODE(dcp),
			DCVN_TO_VNODE(dcp)->v_count, &dcp->dcv_vattr);
		if (index == -1)
			qprintf("\n");
		else
			qprintf(" index %d\n", index);
		qprintf("  handle 0x%x => service.cell %d objid 0x%x gen %d\n",
			dcp->dcv_handle,
			CFS_HANDLE_TO_SERVICE(dcp->dcv_handle).s_cell,
			CFS_HANDLE_TO_OBJID(dcp->dcv_handle),
			CFS_HANDLE_TO_GEN(dcp->dcv_handle));
		cxfs_prdcxvn(dcp->dcv_dcxvn);
	} else
		qprintf("NULL dcvn for index %d\n", index);
}

void
prdsvn(dsvn_t *dsp, int index)
{
	if (dsp) {
		qprintf("dsvn @ 0x%x: flags 0x%x kq.next 0x%x kq.prev 0x%x\n",
			dsp, dsp->dsv_flags, dsp->dsv_kqueue.kq_next,
				dsp->dsv_kqueue.kq_prev);
		qprintf("  &tclient 0x%x  &tserver 0x%x bd 0x%x\n",
			dsp->dsv_tclient, dsp->dsv_tserver, dsp->dsv_bd);
		if (index == -1)
			qprintf("\n");
		else
			qprintf(" index %d\n", index);
		qprintf("  vp 0x%x v_count %d vrgen_flags 0x%x\n", DSVN_TO_VNODE(dsp),
			DSVN_TO_VNODE(dsp)->v_count, dsp->dsv_vrgen_flags);
		qprintf("  objid 0x%x gen %d\n", DSVN_TO_OBJID(dsp), DSVN_TO_GEN(dsp));
		cxfs_prdsxvn(dsp->dsv_dsxvn);
	} else
		qprintf("NULL dsvn for index %d\n", index);
}

#if DVN_TRACING
/*
 * Display the last 'count' entries.  If no args are given to the 'dvntrace'
 * command, then count will be -1 in which case all the entries are displayed.
 */
void
idbg_dvntrace(__psint_t count)
{
	extern ktrace_t	*dvn_trace_buf;

	if (dvn_trace_buf == NULL) {
		qprintf("The dvn trace buffer is not initialized\n");
		return;
	}

	ktrace_print_buffer(dvn_trace_buf, 0, -1, count);

}

#endif

struct dcstat_list_entry	{
	char *routine;
	int  *counter;
} dcstat_list[] = {
	"cfs_vnexport...........", &dcstats.cfs_vnexport,
	"cfs_vnimport...........", &dcstats.cfs_vnimport,
	"dcvn_import_force......", &dcstats.dcvn_import_force,
	"dcvn_handle_to_dcvn_NEW", &dcstats.dcvn_handle_to_dcvn_NEW,
	"dcvn_handle_to_dcvn_hsh", &dcstats.dcvn_handle_to_dcvn_hsh,
	"dcvn_handle_to_dcvn_err", &dcstats.dcvn_handle_to_dcvn_err,
	"dcvn_newdc_setup.......", &dcstats.dcvn_newdc_setup,
	"dcvn_newdc_install.....", &dcstats.dcvn_newdc_install,
	"dcvn_handle_lookup.....", &dcstats.dcvn_handle_lookup,
	"dcvn_handle_enter......", &dcstats.dcvn_handle_enter,
	"dcvn_handle_remove.....", &dcstats.dcvn_handle_remove,
	"dcvn_destroy...........", &dcstats.dcvn_destroy,
	"I_dcvn_vnode_change....", &dcstats.I_dcvn_vnode_change,
	"I_dcvn_page_cache_op...", &dcstats.I_dcvn_page_cache_op,
	"DVN_EXIST_obtain.......", &dcstats.DVN_EXIST_obtain,
	"DVN_PCACHE_obtain......", &dcstats.DVN_PCACHE_obtain,
	"DVN_PSEARCH_obtain.....", &dcstats.DVN_PSEARCH_obtain,
	"DVN_NAME_obtain........", &dcstats.DVN_NAME_obtain,
	"DVN_ATTR_obtain........", &dcstats.DVN_ATTR_obtain,
	"DVN_TIMES_obtain.......", &dcstats.DVN_TIMES_obtain,
	"DVN_EXTENT_obtain......", &dcstats.DVN_EXTENT_obtain,
	"DVN_BIT_obtain.........", &dcstats.DVN_BIT_obtain,
	"DVN_DIT_obtain.........", &dcstats.DVN_DIT_obtain,
	"DVN_EXIST_obt_opti.....", &dcstats.DVN_EXIST_obt_opti,
	"DVN_PCACHE_obt_opti....", &dcstats.DVN_PCACHE_obt_opti,
	"DVN_PSEARCH_obt_opti...", &dcstats.DVN_PSEARCH_obt_opti,
	"DVN_NAME_obt_opti......", &dcstats.DVN_NAME_obt_opti,
	"DVN_ATTR_obt_opti......", &dcstats.DVN_ATTR_obt_opti,
	"DVN_TIMES_obt_opti.....", &dcstats.DVN_TIMES_obt_opti,
	"DVN_EXTENT_obt_opti....", &dcstats.DVN_EXTENT_obt_opti,
	"DVN_BIT_obt_opti.......", &dcstats.DVN_BIT_obt_opti,
	"DVN_DIT_obt_opti.......", &dcstats.DVN_DIT_obt_opti,
	"DVN_EXIST_recall.......", &dcstats.DVN_EXIST_recall,
	"DVN_PCACHE_recall......", &dcstats.DVN_PCACHE_recall,
	"DVN_PSEARCH_recall.....", &dcstats.DVN_PSEARCH_recall,
	"DVN_NAME_recall........", &dcstats.DVN_NAME_recall,
	"DVN_ATTR_recall........", &dcstats.DVN_ATTR_recall,
	"DVN_TIMES_recall.......", &dcstats.DVN_TIMES_recall,
	"DVN_EXTENT_recall......", &dcstats.DVN_EXTENT_recall,
	"DVN_BIT_recall.........", &dcstats.DVN_BIT_recall,
	"DVN_DIT_recall.........", &dcstats.DVN_DIT_recall,
	"DVN_EXIST_return.......", &dcstats.DVN_EXIST_return,
	"DVN_PCACHE_return......", &dcstats.DVN_PCACHE_return,
	"DVN_PSEARCH_return.....", &dcstats.DVN_PSEARCH_return,
	"DVN_NAME_return........", &dcstats.DVN_NAME_return,
	"DVN_ATTR_return........", &dcstats.DVN_ATTR_return,
	"DVN_TIMES_return.......", &dcstats.DVN_TIMES_return,
	"DVN_EXTENT_return......", &dcstats.DVN_EXTENT_return,
	"DVN_BIT_return.........", &dcstats.DVN_BIT_return,
	"DVN_DIT_return.........", &dcstats.DVN_DIT_return,
	"dc_put_times...........", &dcstats.dc_put_times,
	"dc_put_ext.............", &dcstats.dc_put_ext,
	"dc_put_gen.............", &dcstats.dc_put_gen,
	"cfs_dcvn_set_times.....", &dcstats.cfs_dcvn_set_times,
	"dcvn_teardown..........", &dcstats.dcvn_teardown,
	"dcvn_open..............", &dcstats.dcvn_open,
	"dcvn_close.............", &dcstats.dcvn_close,
	"dcvn_read..............", &dcstats.dcvn_read,
	"dcvn_page_read.........", &dcstats.dcvn_page_read,
	"dcvn_write.............", &dcstats.dcvn_write,
	"dcvn_ioctl.............", &dcstats.dcvn_ioctl,
	"dcvn_getattr...........", &dcstats.dcvn_getattr,
	"dcvn_setattr...........", &dcstats.dcvn_setattr,
	"dcvn_access............", &dcstats.dcvn_access,
	"dcvn_lookup............", &dcstats.dcvn_lookup,
	"dcvn_create_int........", &dcstats.dcvn_create_int,
	"dcvn_remove............", &dcstats.dcvn_remove,
	"dcvn_link..............", &dcstats.dcvn_link,
	"dcvn_rename............", &dcstats.dcvn_rename,
	"dcvn_rmdir.............", &dcstats.dcvn_rmdir,
	"dcvn_readdir...........", &dcstats.dcvn_readdir,
	"dcvn_symlink...........", &dcstats.dcvn_symlink,
	"dcvn_readlink..........", &dcstats.dcvn_readlink,
	"dcvn_fsync.............", &dcstats.dcvn_fsync,
	"dcvn_inactive..........", &dcstats.dcvn_inactive,
	"dcvn_fid...............", &dcstats.dcvn_fid,
	"dcvn_fid2..............", &dcstats.dcvn_fid2,
	"dcvn_rwlock............", &dcstats.dcvn_rwlock,
	"dcvn_rwunlock..........", &dcstats.dcvn_rwunlock,
	"dcvn_seek..............", &dcstats.dcvn_seek,
	"dcvn_cmp...............", &dcstats.dcvn_cmp,
	"dcvn_frlock............", &dcstats.dcvn_frlock,
	"dcvn_bmap..............", &dcstats.dcvn_bmap,
	"dcvn_map...............", &dcstats.dcvn_map,
	"dcvn_addmap............", &dcstats.dcvn_addmap,
	"dcvn_delmap............", &dcstats.dcvn_delmap,
	"dcvn_allocstore........", &dcstats.dcvn_allocstore,
	"dcvn_fcntl.............", &dcstats.dcvn_fcntl,
	"dcvn_reclaim...........", &dcstats.dcvn_reclaim,
	"dcvn_attr_get..........", &dcstats.dcvn_attr_get,
	"dcvn_attr_set..........", &dcstats.dcvn_attr_set,
	"dcvn_attr_remove.......", &dcstats.dcvn_attr_remove,
	"dcvn_cover.............", &dcstats.dcvn_cover,
	"dcvn_vnode_change......", &dcstats.dcvn_vnode_change,
	"dcvn_strgetmsg.........", &dcstats.dcvn_strgetmsg,
	"dcvn_strputmsg.........", &dcstats.dcvn_strputmsg
};

int	dcstat_list_cnt = sizeof(dcstat_list)
					/ sizeof(struct dcstat_list_entry);

struct dsstat_list_entry	{
	char *routine;
	int  *counter;
} dsstat_list[] = {
	"cfs_vnexport...........", &dsstats.cfs_vnexport,
	"cfs_vnimport...........", &dsstats.cfs_vnimport,
	"dsvn_alloc.............", &dsstats.dsvn_alloc,
	"dsvn_vnode_to_dsvn_nl..", &dsstats.dsvn_vnode_to_dsvn_nl,
	"dsvn_handle_lookup.....", &dsstats.dsvn_handle_lookup,
	"dsvn_handle_enter......", &dsstats.dsvn_handle_enter,
	"dsvn_handle_remove.....", &dsstats.dsvn_handle_remove,
	"dsvn_destroy...........", &dsstats.dsvn_destroy,
	"dsvn_teardown..........", &dsstats.dsvn_teardown,
	"dsvn_read..............", &dsstats.dsvn_read,
	"dsvn_write.............", &dsstats.dsvn_write,
	"dsvn_getattr...........", &dsstats.dsvn_getattr,
	"dsvn_setattr...........", &dsstats.dsvn_setattr,
	"dsvn_create............", &dsstats.dsvn_create,
	"dsvn_remove............", &dsstats.dsvn_remove,
	"dsvn_link..............", &dsstats.dsvn_link,
	"dsvn_rename............", &dsstats.dsvn_rename,
	"dsvn_mkdir.............", &dsstats.dsvn_mkdir,
	"dsvn_rmdir.............", &dsstats.dsvn_rmdir,
	"dsvn_readdir...........", &dsstats.dsvn_readdir,
	"dsvn_symlink...........", &dsstats.dsvn_symlink,
	"dsvn_readlink..........", &dsstats.dsvn_readlink,
	"dsvn_inactive..........", &dsstats.dsvn_inactive,
	"dsvn_allocstore........", &dsstats.dsvn_allocstore,
	"dsvn_fcntl.............", &dsstats.dsvn_fcntl,
	"dsvn_reclaim...........", &dsstats.dsvn_reclaim,
	"dsvn_link_removed......", &dsstats.dsvn_link_removed,
	"dsvn_page_cache_op.....", &dsstats.dsvn_page_cache_op,
	"dsvn_page_cache_op_it..", &dsstats.dsvn_page_cache_op_it,
	"dsvn_rel_pcache_tok....", &dsstats.dsvn_rel_pcache_tok,
	"dsvn_tosspages.........", &dsstats.dsvn_tosspages,
	"dsvn_flushinval_pages..", &dsstats.dsvn_flushinval_pages,
	"dsvn_flush_pages.......", &dsstats.dsvn_flush_pages,
	"dsvn_invalfree_pages...", &dsstats.dsvn_invalfree_pages,
	"dsvn_pages_sethole.....", &dsstats.dsvn_pages_sethole,
	"dsvn_vnode_change......", &dsstats.dsvn_vnode_change,
	"vnode_change_iterator..", &dsstats.vnode_change_iterator,
	"dsvn_strgetmsg.........", &dsstats.dsvn_strgetmsg,
	"dsvn_strputmsg.........", &dsstats.dsvn_strputmsg,
	"dsvn_tsif_recall.......", &dsstats.dsvn_tsif_recall,
	"dsvn_tsif_recalled.....", &dsstats.dsvn_tsif_recalled,
	"ds_getobjects..........", &dsstats.ds_getobjects,
	"ds_update_times........", &dsstats.ds_update_times,
	"I_dsvn_return..........", &dsstats.I_dsvn_return,
	"I_dsvn_refuse..........", &dsstats.I_dsvn_refuse,
	"I_dsvn_notfound........", &dsstats.I_dsvn_notfound,
	"I_dsvn_obtain..........", &dsstats.I_dsvn_obtain,
	"I_dsvn_obtain_done.....", &dsstats.I_dsvn_obtain_done,
	"I_dsvn_obtain_exist....", &dsstats.I_dsvn_obtain_exist,
	"I_dsvn_obtain_exist_do.", &dsstats.I_dsvn_obtain_exist_done,
	"I_dsvn_open............", &dsstats.I_dsvn_open,
	"I_dsvn_close...........", &dsstats.I_dsvn_close,
	"I_dsvn_read............", &dsstats.I_dsvn_read,
	"I_dsvn_page_read.......", &dsstats.I_dsvn_page_read,
	"I_dsvn_write...........", &dsstats.I_dsvn_write,
	"I_dsvn_list_read.......", &dsstats.I_dsvn_list_read,
	"I_dsvn_list_write......", &dsstats.I_dsvn_list_write,
	"I_dsvn_ioctl...........", &dsstats.I_dsvn_ioctl,
	"I_dsvn_strgetmsg.......", &dsstats.I_dsvn_strgetmsg,
	"I_dsvn_strputmsg.......", &dsstats.I_dsvn_strputmsg,
	"I_dsvn_getattr.........", &dsstats.I_dsvn_getattr,
	"I_dsvn_setattr.........", &dsstats.I_dsvn_setattr,
	"I_dsvn_access..........", &dsstats.I_dsvn_access,
	"I_dsvn_lookup..........", &dsstats.I_dsvn_lookup,
	"I_dsvn_lookup_done.....", &dsstats.I_dsvn_lookup_done,
	"I_dsvn_create..........", &dsstats.I_dsvn_create,
	"I_dsvn_create_done.....", &dsstats.I_dsvn_create_done,
	"I_dsvn_remove..........", &dsstats.I_dsvn_remove,
	"I_dsvn_link............", &dsstats.I_dsvn_link,
	"I_dsvn_rename..........", &dsstats.I_dsvn_rename,
	"I_dsvn_rmdir...........", &dsstats.I_dsvn_rmdir,
	"I_dsvn_readdir.........", &dsstats.I_dsvn_readdir,
	"I_dsvn_symlink.........", &dsstats.I_dsvn_symlink,
	"I_dsvn_readlink........", &dsstats.I_dsvn_readlink,
	"I_dsvn_fsync...........", &dsstats.I_dsvn_fsync,
	"I_dsvn_fid.............", &dsstats.I_dsvn_fid,
	"I_dsvn_fid2............", &dsstats.I_dsvn_fid2,
	"I_dsvn_rwlock..........", &dsstats.I_dsvn_rwlock,
	"I_dsvn_rwunlock........", &dsstats.I_dsvn_rwunlock,
	"I_dsvn_seek............", &dsstats.I_dsvn_seek,
	"I_dsvn_cmp.............", &dsstats.I_dsvn_cmp,
	"I_dsvn_frlock..........", &dsstats.I_dsvn_frlock,
	"I_dsvn_bmap............", &dsstats.I_dsvn_bmap,
	"I_dsvn_map.............", &dsstats.I_dsvn_map,
	"I_dsvn_addmap..........", &dsstats.I_dsvn_addmap,
	"I_dsvn_delmap..........", &dsstats.I_dsvn_delmap,
	"I_dsvn_allocstore......", &dsstats.I_dsvn_allocstore,
	"I_dsvn_fcntl...........", &dsstats.I_dsvn_fcntl,
	"I_dsvn_attr_get........", &dsstats.I_dsvn_attr_get,
	"I_dsvn_attr_get_done...", &dsstats.I_dsvn_attr_get_done,
	"I_dsvn_attr_set........", &dsstats.I_dsvn_attr_set,
	"I_dsvn_attr_remove.....", &dsstats.I_dsvn_attr_remove,
	"I_dsvn_cover...........", &dsstats.I_dsvn_cover,
	"I_dsvn_vnode_change....", &dsstats.I_dsvn_vnode_change,
	"I_dsvn_vnode_unref.....", &dsstats.I_dsvn_vnode_unref,
	"I_dsvn_vnode_reexport..", &dsstats.I_dsvn_vnode_reexport,
	"I_dsvn_update_page_fla.", &dsstats.I_dsvn_update_page_flags,
	"I_dsvn_poll............", &dsstats.I_dsvn_poll,
	"plist_to_uio...........", &dsstats.plist_to_uio,
	"kvaddr_free............", &dsstats.kvaddr_free
};

int	dsstat_list_cnt = sizeof(dsstat_list)
					/ sizeof(struct dsstat_list_entry);

/* ARGSUSED */
static void
idbg_dcstats()
{
	int	i;
	int	num_left;
	int	num_right;
	int	num_rows;

	num_right = dcstat_list_cnt / 2;
	num_left  = dcstat_list_cnt - num_right;

	num_rows  = (num_left > num_right) ? num_left : num_right;

	qprintf(
"idbg_dcstats: dcstat_list_cnt/%d num_left/%d num_right/%d num_rows/%d\n",
						dcstat_list_cnt,
						num_left, num_right, num_rows);

	for (i = 0; i < num_rows; i++) {
		/*
		 * Print an entry from the left
		 */
		if (i < num_left) {

			if (dcstat_list[i].counter)
				qprintf("%s %9d\t", dcstat_list[i].routine,
					   *dcstat_list[i].counter);
			else
				qprintf("%s          \t",
					    dcstat_list[i].routine);
		}

		/*
		 * Print an entry from the right
		 */
		if (i < num_right) {
			if (dcstat_list[num_left+i].counter)
				qprintf("%s %9d\n",
					    dcstat_list[num_left+i].routine,
					   *dcstat_list[num_left+i].counter);
			else
				qprintf("%s          \n",
					    dcstat_list[num_left+i].routine);
		} else {
			qprintf("\n");
		}
	}

	qprintf("\n");
}
/* ARGSUSED */
static void
idbg_dsstats()
{
	int	i;
	int	num_left;
	int	num_right;
	int	num_rows;

	num_right = dsstat_list_cnt / 2;
	num_left  = dsstat_list_cnt - num_right;

	num_rows  = (num_left > num_right) ? num_left : num_right;

	qprintf(
"idbg_dsstats: dsstat_list_cnt/%d num_left/%d num_right/%d num_rows/%d\n",
						dsstat_list_cnt,
						num_left, num_right, num_rows);

	for (i = 0; i < num_rows; i++) {
		/*
		 * Print an entry from the left
		 */
		if (i < num_left) {

			if (dsstat_list[i].counter)
				qprintf("%s %9d\t", dsstat_list[i].routine,
					   *dsstat_list[i].counter);
			else
				qprintf("%s          \t",
					    dsstat_list[i].routine);
		}

		/*
		 * Print an entry from the right
		 */
		if (i < num_right) {
			if (dsstat_list[num_left+i].counter)
				qprintf("%s %9d\n",
					    dsstat_list[num_left+i].routine,
					   *dsstat_list[num_left+i].counter);
			else
				qprintf("%s          \n",
					    dsstat_list[num_left+i].routine);
		} else {
			qprintf("\n");
		}
	}

	qprintf("\n");
}
