/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_VSHM_MIGRATE_H
#define	_VSHM_MIGRATE_H 1
#ident "$Id: vshm_migrate.h,v 1.4 1996/08/15 20:09:54 cp Exp $"

extern void vshm_obj_init(void);
extern void vshm_client_quiesce(vshm_t *);
extern void vshm_client_unquiesce(vshm_t *);
extern  int vshm_obj_relocate(vshm_t *, cell_t);
extern  int vshm_obj_evict(cell_t);

/* Object bag tags: */
#define OBJ_TAG_DSSHM		OBJ_SVC_TAG(SVC_SVIPC,0)
#define OBJ_TAG_PSHM		OBJ_SVC_TAG(SVC_SVIPC,1)

#define OBJ_TAG_SRC_PREP	OBJ_SVC_TAG(SVC_SVIPC,2)
typedef struct {
	int		id;
	int		key;
	tk_set_t	granted;
} vshm_src_prep_t; 

/*
 * Object relocation callouts:
 */
extern int vshm_obj_prep_source(objid_t, cell_t, obj_bag_t);
extern int vshm_obj_prep_target(objid_t, cell_t, obj_bag_t,
				objid_t *, obj_bag_t);
extern int vshm_obj_bag_source(objid_t, cell_t, objid_t, obj_bag_t ,obj_bag_t);
extern int vshm_obj_unbag_target(objid_t, cell_t, obj_bag_t);
extern int vshm_obj_end_source(objid_t, cell_t);
extern int vshm_obj_abort_source(objid_t, cell_t);
extern int vshm_obj_abort_target(objid_t, cell_t, objid_t);

#endif	/* _VSHM_MIGRATE_H */
