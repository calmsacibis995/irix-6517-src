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
#ifndef	_CFS_RELOCATE_H_
#define	_CFS_RELOCATE_H_
#ident	"$Id: cfs_relocation.h,v 1.2 1997/06/25 21:17:58 cp Exp $"

#include <ksys/cell/relocation.h>
#include "dvn.h"

/*
 * CFS object bag tags.
 */
#define OBJ_TAG_CFS_HANDLE	OBJ_SVC_TAG(SVC_CFS, 0)
#define OBJ_TAG_CFS_BHV_ID	OBJ_SVC_TAG(SVC_CFS, 1)
#define OBJ_TAG_CFS_BHV_END	OBJ_SVC_TAG(SVC_CFS, 2)
#define OBJ_TAG_CFS_DSVN	OBJ_SVC_TAG(SVC_CFS, 3)
#define OBJ_TAG_CFS_MAX		3

/* Data associated with OBJ_TAG_CFS_DSVN: */
typedef struct {
	dvn_flags_t	flags;
	char		vrgen_flags;
} dsvn_bag_t;

/*
 * Vnode behavior relocation interface.
 *
 * This table is registered for each type of vnode behavior supporting
 * object server relocation. CFS uses these callouts to accomplish
 * the bagging and unbagging of filesystem-specific behaviors to 
 * relocate a vnode's behavior chain via KORE.
 */
typedef struct {
	int (*cfs_vn_bhv_source_bag) (
		bhv_desc_t *,		/* IN bhv descriptor */
		obj_bag_t);		/* IN object bag */
	int (*cfs_vn_bhv_target_unbag) (
		vnode_t *,		/* IN vnode */
		obj_bag_t);		/* IN object bag */
	int (*cfs_vn_bhv_source_end) (
		bhv_desc_t *);		/* IN bhv descriptor */
	int (*cfs_vn_bhv_source_abort) (
		bhv_desc_t *);		/* IN bhv descriptor */
	int (*cfs_vn_bhv_target_abort) (
		bhv_desc_t *);		/* IN bhv descriptor */
} cfs_vn_kore_if_t;

/*
 * Types of vnode behaviors that CFS recognizes for relocation.
 */
typedef enum {  VN_BHV_UNKNOWN,		/* not specified */
		VN_BHV_CFS,		/* CFS itself */
		VN_BHV_PR,		/* procfs */
		VN_BHV_END		/* housekeeping end-of-range */
} cfs_bhv_t;

/*
 * Behavior relocation interfaces.
 */
extern cfs_vn_kore_if_t prnode_obj_bhv_iface;
extern cfs_vn_kore_if_t cfs_obj_bhv_iface;

/*
 * Registration routine for relocated vnode behaviors.
 */
extern void	cfs_vn_bhv_register(cfs_bhv_t, cfs_vn_kore_if_t *);

/*
 * CFS object (aka vnode) relocation initialization.
 */
extern void	cfs_obj_init(void);

#endif	/* _CFS_RELOCATE_H_ */
