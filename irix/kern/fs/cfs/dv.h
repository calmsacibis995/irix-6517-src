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
#ifndef	_CFS_DV_H_
#define	_CFS_DV_H_
#ident	"$Id: dv.h,v 1.29 1997/08/22 18:31:10 mostek Exp $"

/*
 * Internal CFS header file that is generic to all of the Cell File System.
 */
#include "cfs.h"
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <ksys/behavior.h>
#include <ksys/cell.h>
#include <ksys/cell/service.h>
#include <ksys/cell/tkm.h>
#include <fs/cell/fsc_types.h>

/*
 * The cfs_handle_t is the identifier for remote vnodes.
 * It contains a standard obj_handle_t and a special generation number
 * used as follows:
 * 	Hash table entries containing handle->{dc,ds} mappings must store 
 * 	a handle generation number to account for possibility of handles 
 *	being reused while a "hint" handle is outstanding.  The exposure 
 * 	window during which a hint is outstanding is small, and hence 32 bits 
 *	is of sufficient size.
 *
 * The typedefs for cfs handles are in the public cfs.h header file.
 * Also in this header are CFS_HANDLE_{MAKE_NULL,IS_NULL,EQU}.  However, 
 * all other defines for manipulating the handle are private.
 */

/*
 * Macros.
 */
#define CFS_HANDLE_TO_SERVICE(handle)	HANDLE_TO_SERVICE((handle).ch_objhand)
#define CFS_HANDLE_TO_OBJID(handle)	HANDLE_TO_OBJID((handle).ch_objhand)
#define CFS_HANDLE_TO_GEN(handle)	((handle).ch_gen)
#define CFS_HANDLE_MAKE(handle, objid, gen)				\
{									\
        HANDLE_MAKE((handle).ch_objhand, cfs_service_id, objid); 	\
        (handle).ch_gen = gen;						\
}
#define CFS_HASH_INDEX(objid, numbuckets) HANDLE_HASH_INDEX(objid, numbuckets)

/*
 * Hash table sizes (prime numbers).
 */
#define	DCVN_NHASH	97		
#define	DSVN_NHASH	97		

typedef struct dcstat_info	{
	int cfs_vnexport;
	int cfs_vnimport;
	int dcvn_import_force;
	int dcvn_handle_to_dcvn_NEW;
	int dcvn_handle_to_dcvn_hsh;
	int dcvn_handle_to_dcvn_err;
	int dcvn_newdc_setup;
	int dcvn_newdc_install;
	int dcvn_handle_lookup;
	int dcvn_handle_enter;
	int dcvn_handle_remove;
	int dcvn_destroy;
	int I_dcvn_vnode_change;
	int I_dcvn_page_cache_op;
	int DVN_EXIST_recall;
	int DVN_PCACHE_recall;
	int DVN_PSEARCH_recall;
	int DVN_NAME_recall;
	int DVN_ATTR_recall;
	int DVN_TIMES_recall;
	int DVN_EXTENT_recall;
	int DVN_BIT_recall;
	int DVN_DIT_recall;
	int DVN_EXIST_return;
	int DVN_PCACHE_return;
	int DVN_PSEARCH_return;
	int DVN_NAME_return;
	int DVN_ATTR_return;
	int DVN_TIMES_return;
	int DVN_EXTENT_return;
	int DVN_BIT_return;
	int DVN_DIT_return;
	int DVN_EXIST_obtain;
	int DVN_PCACHE_obtain;
	int DVN_PSEARCH_obtain;
	int DVN_NAME_obtain;
	int DVN_ATTR_obtain;
	int DVN_TIMES_obtain;
	int DVN_EXTENT_obtain;
	int DVN_BIT_obtain;
	int DVN_DIT_obtain;
	int DVN_EXIST_obt_opti;
	int DVN_PCACHE_obt_opti;
	int DVN_PSEARCH_obt_opti;
	int DVN_NAME_obt_opti;
	int DVN_ATTR_obt_opti;
	int DVN_TIMES_obt_opti;
	int DVN_EXTENT_obt_opti;
	int DVN_BIT_obt_opti;
	int DVN_DIT_obt_opti;
	int dcvn_teardown;
	int dcvn_open;
	int dcvn_close;
	int dcvn_read;
	int dcvn_page_read;
	int dcvn_write;
	int dcvn_ioctl;
	int dcvn_getattr;
	int dcvn_setattr;
	int dcvn_access;
	int dcvn_lookup;
	int dcvn_create_int;
	int dcvn_remove;
	int dcvn_link;
	int dcvn_rename;
	int dcvn_rmdir;
	int dcvn_readdir;
	int dcvn_symlink;
	int dcvn_readlink;
	int dcvn_fsync;
	int dcvn_inactive;
	int dcvn_fid;
	int dcvn_fid2;
	int dcvn_rwlock;
	int dcvn_rwunlock;
	int dcvn_seek;
	int dcvn_cmp;
	int dcvn_frlock;
	int dcvn_bmap;
	int dcvn_map;
	int dcvn_addmap;
	int dcvn_delmap;
	int dcvn_allocstore;
	int dcvn_fcntl;
	int dcvn_reclaim;
	int dcvn_attr_get;
	int dcvn_attr_set;
	int dcvn_attr_remove;
	int dcvn_cover;
	int dcvn_vnode_change;
	int dcvn_poll;
	int dcvn_strgetmsg;
	int dcvn_strputmsg;
	int dc_put_times;
	int dc_put_ext;
	int dc_put_gen;
	int cfs_dcvn_set_times;
} dcstat_info_t;

typedef struct dsstat_info	{
	int cfs_vnexport;
	int cfs_vnimport;
	int dsvn_alloc;
	int dsvn_vnode_to_dsvn_nl;
	int dsvn_handle_lookup;
	int dsvn_handle_enter;
	int dsvn_handle_remove;
	int dsvn_destroy;
	int dsvn_teardown;
	int dsvn_read;
	int dsvn_write;
	int dsvn_getattr;
	int dsvn_setattr;
	int dsvn_create;
	int dsvn_remove;
	int dsvn_link;
	int dsvn_rename;
	int dsvn_mkdir;
	int dsvn_rmdir;
	int dsvn_readdir;
	int dsvn_symlink;
	int dsvn_readlink;
	int dsvn_inactive;
	int dsvn_allocstore;
	int dsvn_fcntl;
	int dsvn_reclaim;
	int dsvn_link_removed;
	int dsvn_page_cache_op;
	int dsvn_page_cache_op_it;
	int dsvn_rel_pcache_tok;
	int dsvn_tosspages;
	int dsvn_flushinval_pages;
	int dsvn_flush_pages;
	int dsvn_invalfree_pages;
	int dsvn_pages_sethole;
	int dsvn_vnode_change;
	int vnode_change_iterator;
	int dsvn_strgetmsg;
	int dsvn_strputmsg;
	int dsvn_tsif_recall;
	int dsvn_tsif_recalled;
	int ds_getobjects;
	int ds_update_times;
	int I_dsvn_open;
	int I_dsvn_close;
	int I_dsvn_read;
	int I_dsvn_page_read;
	int I_dsvn_write;
	int I_dsvn_list_read;
	int I_dsvn_list_write;
	int I_dsvn_ioctl;
	int I_dsvn_strgetmsg;
	int I_dsvn_strputmsg;
	int I_dsvn_getattr;
	int I_dsvn_setattr;
	int I_dsvn_access;
	int I_dsvn_lookup;
	int I_dsvn_lookup_done;
	int I_dsvn_create;
	int I_dsvn_create_done;
	int I_dsvn_remove;
	int I_dsvn_link;
	int I_dsvn_rename;
	int I_dsvn_rmdir;
	int I_dsvn_readdir;
	int I_dsvn_symlink;
	int I_dsvn_readlink;
	int I_dsvn_fsync;
	int I_dsvn_fid;
	int I_dsvn_fid2;
	int I_dsvn_rwlock;
	int I_dsvn_rwunlock;
	int I_dsvn_seek;
	int I_dsvn_cmp;
	int I_dsvn_frlock;
	int I_dsvn_bmap;
	int I_dsvn_map;
	int I_dsvn_addmap;
	int I_dsvn_delmap;
	int I_dsvn_allocstore;
	int I_dsvn_fcntl;
	int I_dsvn_attr_get;
	int I_dsvn_attr_get_done;
	int I_dsvn_attr_set;
	int I_dsvn_attr_remove;
	int I_dsvn_cover;
	int I_dsvn_vnode_change;
	int I_dsvn_return;
	int I_dsvn_refuse;
	int I_dsvn_notfound;
	int I_dsvn_obtain;
	int I_dsvn_obtain_done;
	int I_dsvn_obtain_exist;
	int I_dsvn_obtain_exist_done;
	int I_dsvn_vnode_unref;
	int I_dsvn_vnode_reexport;
	int I_dsvn_update_page_flags;
	int plist_to_uio;
	int kvaddr_free;
	int I_dsvn_poll;
} dsstat_info_t;

#define	DSSTAT(xx)						\
	{							\
		extern	dsstat_info_t	dsstats;		\
		dsstats. ## xx += 1;				\
	}

#define	DCSTAT(xx)						\
	{							\
		extern	dcstat_info_t	dcstats;		\
		dcstats. ## xx += 1;				\
	}


/*
 * Per-cell CFS data.
 */
extern int		cfs_fstype;		/* type among vfs's */
extern service_t	cfs_service_id;		/* our CFS service id */
extern handle_hashtab_t dcvn_hashtab[];		/* dcvn hash table */
extern handle_hashtab_t dsvn_hashtab[];		/* dsvn hash table */
extern zone_t		*dcvn_zone;		/* dcvn zone */
extern zone_t		*dsvn_zone;		/* dsvn zone */
extern cfs_gen_t	dsvn_gen_num;		/* gen. # for vnode handles */
extern vnodeops_t 	dsvn_ops;		/* server-side vnode ops */
extern handle_hashtab_t dcvfs_head;		/* dcvfs active list */
extern cfs_gen_t	dsvfs_gen_num;		/* gen. # for vfs handles */
extern	cell_t		rootfs_cell;		/* cell serving root fsys */
extern zone_t		*dcvfs_zone;		/* dcvfs zone */
extern zone_t		*dsvfs_zone;		/* dsvfs zone */
extern vfs_t            **dvfs_dummytab;        /* table of dummy vfs's */
extern dsvfs_t          **dsvfs_dummytab;       /* table of dummy dsvfs's */
extern dcvfs_t          **dcvfs_dummytab;       /* table of dummy dcvfs's */

/*
 * Externs.
 */

#endif	/* _CFS_DV_H_ */

