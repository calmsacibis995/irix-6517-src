/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_FS_CELL_CFS_INTFC_H_
#define	_FS_CELL_CFS_INTFC_H_

#ident	"$Id: cfs_intfc.h,v 1.3 1997/08/26 17:57:55 dnoveck Exp $"

/*
 * fs/cell/cfs_intfc.h -- Special interface routines for cfs
 *
 * This header defines the special interface provided by cfs to cell-
 * specific filesystem code, including cxfs-v1.
 *
 * It should only be included by cfs, cxfs-v1, and cell-specific files
 * within other file systems (e.g. prvfsops_cell.c)
 */

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <ksys/cell/tkm.h>
#include <fs/cell/fsc_types.h>
#include <fs/cfs/cfs.h>

extern void 		cfs_register_cxfs(int fstype, 
					  tk_set_t tokens_of_interest);
extern void 		cfs_mountpoint_setup(vfs_t *vfsp, 
					     vnode_t **rdirp, 
					     mount_import_t *mip,
					     cxfs_sinfo_t *cxsp);
extern int              cfs_mountpoint_export(vfs_t *vfsp, 
					      vnode_t *rvp, 
					      cell_t client, 
					      mount_import_t *mnti);
extern bhv_desc_t *	cfs_dcvfs_behavior(vfs_t *vfsp,
					   int fstype);
extern void  		cfs_dcvfs_behavior_free(bhv_desc_t *bdp);
extern void             cfs_vfsimport(cfs_handle_t *vfsh, 
				      vfs_t **vfsp_ret);
extern void             cfs_vfsexport(vfs_t *vfsp, 
				      cfs_handle_t *vfsh); 

/*
 * The following is called to atomically update the time in the
 * vattr within dcvn.  The attr_flag argument should be one of 
 * AT_ATIME, AT_CTIME, or AT_MTIME.
 */
extern void 		cfs_dcvn_set_times(dcvn_t *dcp, 
                                           timespec_t *tsp, 
                                           int attr_flag);

#endif /* _FS_CELL_CFS_INTFC_H_ */
