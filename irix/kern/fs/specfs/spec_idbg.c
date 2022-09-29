/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.24 $"

#include <sys/types.h>
#include <sys/idbgentry.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <fs/specfs/spec_csnode.h>
#include <fs/specfs/spec_lsnode.h>


static void idbg_spec_info(__psint_t, __psint_t, int, char **);
static void idbg_spec_list(__psint_t, __psint_t, int, char **);

static void idbg_spec_ls_dev(__psint_t, __psint_t, int, char **);
static void idbg_spec_cs_dev(__psint_t, __psint_t, int, char **);

static void idbg_spec_ls_list(__psint_t, __psint_t, int, char **);
static void idbg_spec_cs_list(__psint_t, __psint_t, int, char **);

void idbg_prcsnode(struct csnode *);
void idbg_prlsnode(struct lsnode *);

#ifdef	SPECFS_STATS_DEBUG

extern struct spec_stat_info spec_stats;

static void idbg_spec_stats(__psint_t, __psint_t, int, char **);

static void idbg_spec_cs_hash(__psint_t, __psint_t, int, char **);
static void idbg_spec_ls_hash(__psint_t, __psint_t, int, char **);
#endif	/* SPECFS_STATS_DEBUG */



void
spec_idbg_init(void)
{
	idbg_addfunc("spec_cs",		idbg_spec_cs_list);
	idbg_addfunc("spec_ls",		idbg_spec_ls_list);
	idbg_addfunc("spec_info",	idbg_spec_info);
	idbg_addfunc("spec_list",	idbg_spec_list);
	idbg_addfunc("spec_cs_dev",	idbg_spec_cs_dev);
	idbg_addfunc("spec_ls_dev",	idbg_spec_ls_dev);
#ifdef	SPECFS_STATS_DEBUG
	idbg_addfunc("spec_cs_hash",	idbg_spec_cs_hash);
	idbg_addfunc("spec_ls_hash",	idbg_spec_ls_hash);
	idbg_addfunc("spec_stats",	idbg_spec_stats);
#endif	/* SPECFS_STATS_DEBUG */
}


/* ARGSUSED */
static void
idbg_spec_info(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int		i;
	int		hash_entry_in_use;
	int		snode_exists;
	int		snode_is_cached;
	struct csnode	*csp;
	struct lsnode	*lsp;
	struct vnode	*vp;


	qprintf("spec_fstype  = %d\n", spec_fstype);
	qprintf("spec_dev     = 0x%x(%d/%d, %d)\n",
					spec_dev,
					major(spec_dev), emajor(spec_dev),
					minor(spec_dev));
	qprintf("spec_vfsp    = 0x%x \n", spec_vfsp);

	qprintf("Local  snodes:\n");

	hash_entry_in_use = snode_exists = snode_is_cached = 0;

	for (i = 0; i < DLSPECTBSIZE; i++) {

		if ((lsp = spec_lsnode_table[i]) == NULL)
			continue;

		hash_entry_in_use++;

		for (; lsp != NULL; lsp = lsp->ls_next) {

			snode_exists++;

			vp = LSP_TO_VP(lsp);

			if (vp->v_count == 0)
				snode_is_cached++;

		}
	}

	qprintf("\t%5d\thash table size\n",		DLSPECTBSIZE);
	qprintf("\t%5d\thash entries in use\n",		hash_entry_in_use);
	qprintf("\t%5d\tsnodes allocated\n",		snode_exists);
	qprintf("\t%5d\tsnodes with v_count == 0\n",	snode_is_cached);


	qprintf("Common snodes:\n");

	hash_entry_in_use = snode_exists = snode_is_cached = 0;

	for (i = 0; i < DCSPECTBSIZE; i++) {

		if ((csp = spec_csnode_table[i]) == NULL)
			continue;

		hash_entry_in_use++;

		for (; csp != NULL; csp = csp->cs_next) {

			snode_exists++;

			vp = CSP_TO_VP(csp);

			if (vp->v_count == 0)
				snode_is_cached++;

		}
	}

	qprintf("\t%5d\thash table size\n",		DCSPECTBSIZE);
	qprintf("\t%5d\thash entries in use\n",		hash_entry_in_use);
	qprintf("\t%5d\tsnodes allocated\n",		snode_exists);
	qprintf("\t%5d\tsnodes with v_count == 0\n",	snode_is_cached);
}


/* ARGSUSED */
static void
idbg_spec_ls_dev(__psint_t x, __psint_t a2, int argc, char **argv)
{
	vnode_t	*idbg_vnode(__psint_t);


	int		i;
	int		header_done;
	int		maj = -1;
	int		min = -1;
	struct lsnode	*lsp;

	if (argc == 0) {
		qprintf("usage: spec_ls_dev major# [minor#]\n");
		return;
	}

	maj = (int)atoi(argv[0]);

	if (argc == 2)
		min = (int)atoi(argv[1]);

	for (i = 0; i < DLSPECTBSIZE; i++) {

		header_done = 0;

		if ((lsp = spec_lsnode_table[i]) == NULL)
			continue;


		for (; lsp != NULL; lsp = lsp->ls_next) {

			if (   major(lsp->ls_dev)  == maj
			    || emajor(lsp->ls_dev) == maj) {

				if ((min > 0) && (minor(lsp->ls_dev) != min))
					continue;

				if (! header_done) {
					qprintf(
					    "Local Snode Hash list %d:\n", i);
					header_done = 1;
				}

				idbg_prlsnode(lsp);
			}
		}
	}
}


/* ARGSUSED */
static void
idbg_spec_ls_list(__psint_t x, __psint_t a2, int argc, char **argv)
{
	vnode_t	*idbg_vnode(__psint_t);

	int		i;
	struct lsnode	*lsp;

	if (x == -1L) {

		for (i = 0; i < DLSPECTBSIZE; i++) {

			if ((lsp = spec_lsnode_table[i]) == NULL)
				continue;

			qprintf("Local Snode Hash list %d:\n", i);

			for (; lsp != NULL; lsp = lsp->ls_next)
				idbg_prlsnode(lsp);
		}

	} else {
		idbg_prlsnode((struct lsnode *)x);
	}
}


/* ARGSUSED */
static void
idbg_spec_list(__psint_t x, __psint_t a2, int argc, char **argv)
{
	idbg_spec_ls_list(x, a2, argc, argv);
	idbg_spec_cs_list(x, a2, argc, argv);
}


/* ARGSUSED */
static void
idbg_spec_cs_dev(__psint_t x, __psint_t a2, int argc, char **argv)
{
	vnode_t	*idbg_vnode(__psint_t);


	int		i;
	int		header_done;
	int		maj = -1;
	int		min = -1;
	struct csnode	*csp;

	if (argc == 0) {
		qprintf("usage: spec_cs_dev major# [minor#]\n");
		return;
	}

	maj = (int)atoi(argv[0]);

	if (argc == 2)
		min = (int)atoi(argv[1]);

	for (i = 0; i < DCSPECTBSIZE; i++) {

		header_done = 0;

		if ((csp = spec_csnode_table[i]) == NULL)
			continue;


		for (; csp != NULL; csp = csp->cs_next) {

			if (   major(csp->cs_dev)  == maj
			    || emajor(csp->cs_dev) == maj) {

				if ((min > 0) && (minor(csp->cs_dev) != min))
					continue;

				if (! header_done) {
					qprintf(
					    "Common Snode Hash list %d:\n", i);
					header_done = 1;
				}

				idbg_prcsnode(csp);
			}
		}
	}
}


/* ARGSUSED */
static void
idbg_spec_cs_list(__psint_t x, __psint_t a2, int argc, char **argv)
{
	vnode_t	*idbg_vnode(__psint_t);

	int		i;
	struct csnode	*csp;


	if (x == -1L) {

		for (i = 0; i < DCSPECTBSIZE; i++) {

			if ((csp = spec_csnode_table[i]) == NULL)
				continue;

			qprintf("Common Snode Hash list %d:\n", i);

			for (; csp != NULL; csp = csp->cs_next)
				idbg_prcsnode(csp);

		}

	} else {
		idbg_prcsnode((struct csnode *)x);
	}
}


#ifdef	SPECFS_STATS_DEBUG

/* ARGSUSED */
static void
idbg_spec_cs_hash(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int		i;
	int		n;
	struct csnode	*csp;

	qprintf("Common Snode Hash Counts\n");

	for (i = 0; i < DCSPECTBSIZE; i++) {

		if ((csp = spec_csnode_table[i]) == NULL)
			continue;

		for (n = 0; csp != NULL; csp = csp->cs_next)
			n++;
		
		qprintf("%4d: %5d\n", i, n);
	}
}


/* ARGSUSED */
static void
idbg_spec_ls_hash(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int		i;
	int		n;
	struct lsnode	*lsp;

	qprintf("Local Snode Hash Counts\n");

	for (i = 0; i < DLSPECTBSIZE; i++) {

		if ((lsp = spec_lsnode_table[i]) == NULL)
			continue;

		for (n = 0; lsp != NULL; lsp = lsp->ls_next)
			n++;

		qprintf("%4d: %5d\n", i, n);
	}
}


#ifdef	CELL_IRIX
extern int spec_cellid;
#endif	/* CELL_IRIX */

struct spec_stat_list_entry	{
	char *routine;
	int  *counter;
} spec_stat_list[] = {
#ifdef	CELL_IRIX
	"...............Begin CELL", &spec_cellid,
	"spec_dc_addmap_subr......", &spec_stats.spec_dc_addmap_subr,
	"spec_dc_attach...........", &spec_stats.spec_dc_attach,
	"spec_dc_bmap_subr........", &spec_stats.spec_dc_bmap_subr,
	"spec_dc_clone............", &spec_stats.spec_dc_clone,
	"spec_dc_close_hndl.......", &spec_stats.spec_dc_close_hndl,
	"spec_dc_cmp_gen..........", &spec_stats.spec_dc_cmp_gen,
	"spec_dc_delmap_subr......", &spec_stats.spec_dc_delmap_subr,
	"spec_dc_get_size.........", &spec_stats.spec_dc_get_size,
	"spec_dc_get_gen..........", &spec_stats.spec_dc_get_gen,
	"spec_dc_get_opencnt......", &spec_stats.spec_dc_get_opencnt,
	"spec_dc_ioctl_hndl.......", &spec_stats.spec_dc_ioctl_hndl,
	"spec_dc_ismounted........", &spec_stats.spec_dc_ismounted,
	"spec_dc_mountedflag......", &spec_stats.spec_dc_mountedflag,
	"spec_dc_open_hndl........", &spec_stats.spec_dc_open_hndl,
	"spec_dc_poll_hndl........", &spec_stats.spec_dc_poll_hndl,
	"spec_dc_read_hndl........", &spec_stats.spec_dc_read_hndl,
	"spec_dc_teardown_subr....", &spec_stats.spec_dc_teardown_subr,
	"spec_dc_strategy_subr....", &spec_stats.spec_dc_strategy_subr,
	"spec_dc_strgetmsg_hndl...", &spec_stats.spec_dc_strgetmsg_hndl,
	"spec_dc_strputmsg_hndl...", &spec_stats.spec_dc_strputmsg_hndl,
	"spec_dc_write_hndl.......", &spec_stats.spec_dc_write_hndl,
	".........................", NULL,
	"I_spec_dc_strategy.......", &spec_stats.I_spec_dc_strategy,
	"I_spec_ds_addmap.........", &spec_stats.I_spec_ds_addmap,
	"I_spec_ds_attach.........", &spec_stats.I_spec_ds_attach,
	"I_spec_ds_bmap...........", &spec_stats.I_spec_ds_bmap,
	"I_spec_ds_clone..........", &spec_stats.I_spec_ds_clone,
	"I_spec_ds_close..........", &spec_stats.I_spec_ds_close,
	"I_spec_ds_cmp_gen........", &spec_stats.I_spec_ds_cmp_gen,
	"I_spec_ds_delmap.........", &spec_stats.I_spec_ds_delmap,
	"I_spec_ds_get_gen........", &spec_stats.I_spec_ds_get_gen,
	"I_spec_ds_get_opencnt....", &spec_stats.I_spec_ds_get_opencnt,
	"I_spec_ds_get_size.......", &spec_stats.I_spec_ds_get_size,
	"I_spec_ds_ioctl..........", &spec_stats.I_spec_ds_ioctl,
	"I_spec_ds_ismounted......", &spec_stats.I_spec_ds_ismounted,
	"I_spec_ds_list_read......", &spec_stats.I_spec_ds_list_read,
	"I_spec_ds_map............", &spec_stats.I_spec_ds_map,
	"I_spec_ds_mountedflag....", &spec_stats.I_spec_ds_mountedflag,
	"I_spec_ds_open...........", &spec_stats.I_spec_ds_open,
	"I_spec_ds_poll...........", &spec_stats.I_spec_ds_poll,
	"I_spec_ds_read...........", &spec_stats.I_spec_ds_read,
	"I_spec_ds_strategy.......", &spec_stats.I_spec_ds_strategy,
	"I_spec_ds_strgetmsg......", &spec_stats.I_spec_ds_strgetmsg,
	"I_spec_ds_strputmsg......", &spec_stats.I_spec_ds_strputmsg,
	"I_spec_ds_teardown.......", &spec_stats.I_spec_ds_teardown,
	"I_spec_ds_write..........", &spec_stats.I_spec_ds_write,
	".................End CELL", NULL,
#endif	/* CELL_IRIX */
	"make_specvp..............", &spec_stats.make_specvp,
	"make_specvp_locked.......", &spec_stats.make_specvp_locked,
	"spec_access..............", &spec_stats.spec_access,
	"spec_addmap..............", &spec_stats.spec_addmap,
	"spec_allocstore..........", &spec_stats.spec_allocstore,
	"spec_at_free.............", &spec_stats.spec_at_free,
	"spec_at_inactive.........", &spec_stats.spec_at_inactive,
	"spec_at_insert_bhv.......", &spec_stats.spec_at_insert_bhv,
	"spec_at_getattr..........", &spec_stats.spec_at_getattr,
	"spec_at_lock.............", &spec_stats.spec_at_lock,
	"spec_at_setattr..........", &spec_stats.spec_at_setattr,
	"spec_at_unlock...........", &spec_stats.spec_at_unlock,
	"spec_attach..............", &spec_stats.spec_attach,
	"spec_attr_func...........", &spec_stats.spec_attr_func,
	"spec_attr_get............", &spec_stats.spec_attr_get,
	"spec_attr_list...........", &spec_stats.spec_attr_list,
	"spec_attr_remove.........", &spec_stats.spec_attr_remove,
	"spec_attr_set............", &spec_stats.spec_attr_set,
	"spec_bmap................", &spec_stats.spec_bmap,
	"spec_canonical_name_get..", &spec_stats.spec_canonical_name_get,
	"spec_check_vp............", &spec_stats.spec_check_vp,
	"spec_close...............", &spec_stats.spec_close,
	"spec_cmp.................", &spec_stats.spec_cmp,
	"spec_cover...............", &spec_stats.spec_cover,
	"spec_create..............", &spec_stats.spec_create,
	"spec_cs_addmap_subr......", &spec_stats.spec_cs_addmap_subr,
	"spec_cs_attach...........", &spec_stats.spec_cs_attach,
	"spec_cs_attr_func........", &spec_stats.spec_cs_attr_func,
	"spec_cs_attr_get_vop.....", &spec_stats.spec_cs_attr_get_vop,
	"spec_cs_attr_list_vop....", &spec_stats.spec_cs_attr_list_vop,
	"spec_cs_attr_remove_vop..", &spec_stats.spec_cs_attr_remove_vop,
	"spec_cs_attr_set_vop.....", &spec_stats.spec_cs_attr_set_vop,
	"spec_cs_bmap_subr........", &spec_stats.spec_cs_bmap_subr,
	"spec_cs_bmap_vop.........", &spec_stats.spec_cs_bmap_vop,
	"spec_cs_canonical_name_gt", &spec_stats.spec_cs_canonical_name_get,
	"spec_cs_clone............", &spec_stats.spec_cs_clone,
	"spec_cs_close_hndl.......", &spec_stats.spec_cs_close_hndl,
	"spec_cs_close_vop........", &spec_stats.spec_cs_close_vop,
	"spec_cs_cmp_gen..........", &spec_stats.spec_cs_cmp_gen,
	"spec_cs_delete...........", &spec_stats.spec_cs_delete,
	"spec_cs_delmap_subr......", &spec_stats.spec_cs_delmap_subr,
	"spec_cs_device_close.....", &spec_stats.spec_cs_device_close,
	"spec_cs_device_hold......", &spec_stats.spec_cs_device_hold,
	"spec_cs_device_rele......", &spec_stats.spec_cs_device_rele,
	"spec_cs_dflt_poll........", &spec_stats.spec_cs_dflt_poll,
	"spec_cs_free.............", &spec_stats.spec_cs_free,
	"spec_cs_fsync_vop........", &spec_stats.spec_cs_fsync_vop,
	"spec_cs_get..............", &spec_stats.spec_cs_get,
	"spec_cs_get_gen..........", &spec_stats.spec_cs_get_gen,
	"spec_cs_get_opencnt......", &spec_stats.spec_cs_get_opencnt,
	"spec_cs_get_size.........", &spec_stats.spec_cs_get_size,
	"spec_cs_getattr_vop......", &spec_stats.spec_cs_getattr_vop,
	"spec_cs_inactive_vop.....", &spec_stats.spec_cs_inactive_vop,
	"spec_cs_ioctl_hndl.......", &spec_stats.spec_cs_ioctl_hndl,
	"spec_cs_ioctl_vop........", &spec_stats.spec_cs_ioctl_vop,
	"spec_cs_ismounted........", &spec_stats.spec_cs_ismounted,
	"spec_cs_kill_poll........", &spec_stats.spec_cs_kill_poll,
	"spec_cs_lock.............", &spec_stats.spec_cs_lock,
	"spec_cs_mac_label_get....", &spec_stats.spec_cs_mac_label_get,
	"spec_cs_mac_pointer_get..", &spec_stats.spec_cs_mac_pointer_get,
	"spec_cs_makesnode........", &spec_stats.spec_cs_makesnode,
	"spec_cs_mark.............", &spec_stats.spec_cs_mark,
	"spec_cs_mountedflag......", &spec_stats.spec_cs_mountedflag,
	"spec_cs_open_hndl........", &spec_stats.spec_cs_open_hndl,
	"spec_cs_open_vop.........", &spec_stats.spec_cs_open_vop,
	"spec_cs_poll_hndl........", &spec_stats.spec_cs_poll_hndl,
	"spec_cs_poll_vop.........", &spec_stats.spec_cs_poll_vop,
	"spec_cs_read_rsync.......", &spec_stats.spec_cs_read_rsync,
	"spec_cs_read_hndl........", &spec_stats.spec_cs_read_hndl,
	"spec_cs_read_hndl_exit...", &spec_stats.spec_cs_read_hndl_exit,
	"spec_cs_read_vop.........", &spec_stats.spec_cs_read_vop,
	"spec_cs_rwlock...........", &spec_stats.spec_cs_rwlock,
	"spec_cs_rwlock_lcl.......", &spec_stats.spec_cs_rwlock_lcl,
	"spec_cs_rwlock_vop.......", &spec_stats.spec_cs_rwlock_vop,
	"spec_cs_rwunlock.........", &spec_stats.spec_cs_rwunlock,
	"spec_cs_rwunlock_lcl.....", &spec_stats.spec_cs_rwunlock_lcl,
	"spec_cs_rwunlock_vop.....", &spec_stats.spec_cs_rwunlock_vop,
	"spec_cs_setattr_vop......", &spec_stats.spec_cs_setattr_vop,
	"spec_cs_strgetmsg_hndl...", &spec_stats.spec_cs_strgetmsg_hndl,
	"spec_cs_strgetmsg_vop....", &spec_stats.spec_cs_strgetmsg_vop,
	"spec_cs_strputmsg_hndl...", &spec_stats.spec_cs_strputmsg_hndl,
	"spec_cs_strputmsg_vop....", &spec_stats.spec_cs_strputmsg_vop,
	"spec_cs_strategy_subr....", &spec_stats.spec_cs_strategy_subr,
	"spec_cs_teardown_subr....", &spec_stats.spec_cs_teardown_subr,
	"spec_cs_unlock...........", &spec_stats.spec_cs_unlock,
	"spec_cs_write_hndl.......", &spec_stats.spec_cs_write_hndl,
	"spec_cs_write_hndl_exit..", &spec_stats.spec_cs_write_hndl_exit,
	"spec_cs_write_vop........", &spec_stats.spec_cs_write_vop,
	"spec_delete..............", &spec_stats.spec_delete,
	"spec_delmap..............", &spec_stats.spec_delmap,
	"spec_devsize.............", &spec_stats.spec_devsize,
	"spec_dump................", &spec_stats.spec_dump,
	"spec_fcntl...............", &spec_stats.spec_fcntl,
	"spec_fid.................", &spec_stats.spec_fid,
	"spec_fid2................", &spec_stats.spec_fid2,
	"spec_find................", &spec_stats.spec_find,
	"spec_fixgen..............", &spec_stats.spec_fixgen,
	"spec_flush...............", &spec_stats.spec_flush,
	"spec_free................", &spec_stats.spec_free,
	"spec_frlock..............", &spec_stats.spec_frlock,
	"spec_fsync...............", &spec_stats.spec_fsync,
	"spec_get.................", &spec_stats.spec_get,
	"spec_getattr.............", &spec_stats.spec_getattr,
	"spec_inactive............", &spec_stats.spec_inactive,
	"spec_insert..............", &spec_stats.spec_insert,
	"spec_dev_is_inuse........", &spec_stats.spec_dev_is_inuse,
	"spec_ioctl...............", &spec_stats.spec_ioctl,
	"spec_ismounted...........", &spec_stats.spec_ismounted,
	"spec_link................", &spec_stats.spec_link,
	"spec_link_removed........", &spec_stats.spec_link_removed,
	"spec_lock................", &spec_stats.spec_lock,
	"spec_lookup..............", &spec_stats.spec_lookup,
	"spec_mac_label_get.......", &spec_stats.spec_mac_label_get,
	"spec_mac_pointer_get.....", &spec_stats.spec_mac_pointer_get,
	"spec_make_clone..........", &spec_stats.spec_make_clone,
	"spec_makesnode...........", &spec_stats.spec_makesnode,
	"spec_map.................", &spec_stats.spec_map,
	"spec_mark................", &spec_stats.spec_mark,
	"spec_mkdir...............", &spec_stats.spec_mkdir,
	"spec_mountedflag.........", &spec_stats.spec_mountedflag,
	"spec_open................", &spec_stats.spec_open,
	"spec_pathconf............", &spec_stats.spec_pathconf,
	"spec_poll_cell...........", &spec_stats.spec_poll_cell,
	"spec_poll_noncell........", &spec_stats.spec_poll_noncell,
	"spec_read................", &spec_stats.spec_read,
	"spec_read_rsync..........", &spec_stats.spec_read_rsync,
	"spec_readdir.............", &spec_stats.spec_readdir,
	"spec_readlink............", &spec_stats.spec_readlink,
	"spec_realvp..............", &spec_stats.spec_realvp,
	"spec_reclaim.............", &spec_stats.spec_reclaim,
	"spec_remove..............", &spec_stats.spec_remove,
	"spec_rename..............", &spec_stats.spec_rename,
	"spec_rmdir...............", &spec_stats.spec_rmdir,
	"spec_rwlock..............", &spec_stats.spec_rwlock,
	"spec_rwunlock............", &spec_stats.spec_rwunlock,
	"spec_seek................", &spec_stats.spec_seek,
	"spec_setattr.............", &spec_stats.spec_setattr,
	"spec_setfl...............", &spec_stats.spec_setfl,
	"spec_strategy............", &spec_stats.spec_strategy,
	"spec_strgetmsg...........", &spec_stats.spec_strgetmsg,
	"spec_strputmsg...........", &spec_stats.spec_strputmsg,
	"spec_symlink.............", &spec_stats.spec_symlink,
	"spec_teardown............", &spec_stats.spec_teardown,
	"spec_teardown_fsync......", &spec_stats.spec_teardown_fsync,
	"spec_unlock..............", &spec_stats.spec_unlock,
#ifdef	CELL_IRIX
	"spec_vnimport............", &spec_stats.spec_vnimport,
#endif	/* CELL_IRIX */
	"spec_vnode_change........", &spec_stats.spec_vnode_change,
	"spec_vp..................", &spec_stats.spec_vp,
#ifdef	JTK_DEBUG
	"spec_vp_is_common........", &spec_stats.spec_vp_is_common,
#endif	/* JTK_DEBUG */
	"spec_write...............", &spec_stats.spec_write,
};

int	spec_stat_list_cnt = sizeof(spec_stat_list)
					/ sizeof(struct spec_stat_list_entry);

/* ARGSUSED */
static void
idbg_spec_stats(__psint_t x, __psint_t a2, int argc, char **argv)
{
	int	i;
	int	num_left;
	int	num_right;
	int	num_rows;

	num_right = spec_stat_list_cnt / 2;
	num_left  = spec_stat_list_cnt - num_right;

	num_rows  = (num_left > num_right) ? num_left : num_right;

	qprintf(
"idbg_spec_stats: spec_stat_list_cnt/%d num_left/%d num_right/%d num_rows/%d\n",
						spec_stat_list_cnt,
						num_left, num_right, num_rows);

	for (i = 0; i < num_rows; i++) {
		/*
		 * Print an entry from the left
		 */
		if (i < num_left) {

			if (spec_stat_list[i].counter)
				qprintf("%s %9d\t", spec_stat_list[i].routine,
					   *spec_stat_list[i].counter);
			else
				qprintf("%s          \t",
					    spec_stat_list[i].routine);
		}

		/*
		 * Print an entry from the right
		 */
		if (i < num_right) {
			if (spec_stat_list[num_left+i].counter)
				qprintf("%s %9d\n",
					    spec_stat_list[num_left+i].routine,
					   *spec_stat_list[num_left+i].counter);
			else
				qprintf("%s          \n",
					    spec_stat_list[num_left+i].routine);
		} else {
			qprintf("\n");
		}
	}

	qprintf("\n");
}
#endif	/* SPECFS_STATS_DEBUG */
