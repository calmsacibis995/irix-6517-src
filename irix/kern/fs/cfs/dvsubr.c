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
#ident	"$Id: dvsubr.c,v 1.15 1997/08/22 18:31:17 mostek Exp $"

/*
 * General routines for the Cell File System.
 */
#include "dv.h"

/*
 * CFS global data.
 */
int		cfs_fstype;			/* type among vfs's */
service_t	cfs_service_id;			/* our CFS service id */
handle_hashtab_t dcvn_hashtab[DCVN_NHASH];	/* dcvn hash table */
handle_hashtab_t dsvn_hashtab[DSVN_NHASH];	/* dsvn hash table */
zone_t		*dcvn_zone;			/* dcvn zone */
zone_t		*dsvn_zone;			/* dsvn zone */
cfs_gen_t	dsvn_gen_num;			/* gen. # for vnode handles */
vnodeops_t	dsvn_ops;			/* server-side vnode ops */
void 		cxfs_init(void);

dsstat_info_t	dsstats;			/* statistics for DS CFS */
dcstat_info_t	dcstats;			/* statistics for DC CFS */


/* 
 * Initialize CFS.  Called through the vfs switch at system initialization.
 *
 * vswp -- pointer to the CFS entry in the vfs switch table
 * fstype -- index of CFS in the vfs switch table used as the CFS fs type.
 */
/* ARGSUSED */
int
cfs_init(
	vfssw_t		*vswp,
	int		fstype)
{
	extern void	dvfs_init(void);
	extern void	dvn_init(void);
	extern void	cfs_obj_init(void);

	cfs_fstype = fstype;	/* our fstype number */

	SERVICE_MAKE(cfs_service_id, cellid(), SVC_CFS);  /* our service id */

	dvfs_init();	/* vfs init */
	dvn_init();	/* vnode init */
	cfs_obj_init();	/* vnode object relocation init */
	cxfs_init();	/* initialize CXFS since it isn't in the vfssw[] */

	return 0;
}
