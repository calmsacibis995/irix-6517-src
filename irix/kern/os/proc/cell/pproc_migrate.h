/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_OS_PROC_CELL_PPROC_MIGRATE_H_
#define	_OS_PROC_CELL_PPROC_MIGRATE_H_ 1

#ident "$Id: pproc_migrate.h,v 1.3 1997/04/15 20:32:04 cp Exp $"

extern int pproc_source_prepare(bhv_desc_t *, cell_t, obj_manifest_t *);
extern void pproc_obj_target_prepare(vproc_t *, obj_manifest_t *);
extern void pproc_source_bag(bhv_desc_t *, cell_t, obj_bag_t);
extern void pproc_target_unbag(bhv_desc_t *, obj_bag_t);
extern void pproc_source_end(bhv_desc_t *);

#endif	/* _OS_PROC_CELL_PPROC_MIGRATE_H_ */
