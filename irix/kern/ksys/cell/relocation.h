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
#ident "$Id: relocation.h,v 1.3 1997/10/02 20:34:45 cp Exp $"

#ifndef	_KSYS_RELOCATION_H_
#define	_KSYS_RELOCATION_H_	1

#ifndef	CELL
#error included by non-CELL configuration
#endif

#include <ksys/cell.h>
#include <ksys/cell/object.h>
#include <ksys/cell/service.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>
#include <sys/sema.h>
#include <sys/errno.h>

/*
 * Interface to generate a list of objects to be relocated to
 * another cell.
 */ 
typedef	obj_bag_t obj_list_t;
extern obj_list_t obj_list_create(	/* Create an empty list */
		uint_t);			/* IN expected num entries */
extern void obj_list_put( 		/* Add an object to a list */
		obj_list_t,			/* IN manifest */
		obj_handle_t *);		/* IN object handle */
extern int  obj_list_get(		/* Read an object entry from list */
		obj_list_t,			/* IN manifest */
		obj_handle_t *);		/* OUT object handle */
/* Other list manipulations map directly to bag operations...  */
#define obj_list_destroy(list)  obj_bag_destroy(list)
#define obj_list_rewind(list)	obj_bag_rewind(list)
#define obj_list_skip(list)	obj_bag_skip(list, OBJ_TAG_HANDLE)
#define obj_list_seek(list,m,p)	obj_bag_seek(list,m,p)
#define obj_list_tell(list,m,p)	obj_bag_tell(list,m,p)
#define obj_list_is_empty(list)	obj_bag_is_empty(list)

/*
 * Object Manifest Interface
 * An Object Manifest is a data structure managed by the Kernel Object
 * Relocation Engine (KORE). It contains the description of a set of
 * objects undergoing relocation and this is used during the "prepare"
 * phase to replicate objects on the target cell - either through
 * export/import or as the precursory step in object server relocation
 */
struct obj_manifest;
typedef struct obj_manifest	obj_manifest_t;

typedef int			obj_rmode_t;
#define	OBJ_RMODE_REFERENCE	0x1		/* RR */
#define	OBJ_RMODE_SERVER	0x2		/* SR */
#define	OBJ_RMODE_SERVER_CLIENT	0x4		/* target object not quiesced */

extern obj_manifest_t *obj_mft_create(  /* Create object manifest */
		cell_t);			/* IN target cell */
extern int  obj_mft_put(		/* Add object to manifest */
		obj_manifest_t *,		/* IN manifest */
		void *,				/* IN virtual object */
		int,				/* IN service number */
		obj_rmode_t);			/* IN relocation mode */
extern int  obj_mft_ship(		/* Relocate all objects in manifest */
		obj_manifest_t *);		/* IN manifest */
extern int  obj_mft_get(		/* Get object named in manifest */
		obj_manifest_t *,		/* IN manifest */
		void **);			/* OUT virtual object */
extern void obj_mft_destroy(		/* Done with manifest */
		obj_manifest_t *);		/* IN manifest */

extern obj_bag_t	obj_mft_src_info(obj_manifest_t *);
extern obj_bag_t	obj_mft_tgt_info(obj_manifest_t *);

/*
 * Object Manifest Information structures
 */
typedef struct {
	obj_handle_t	hndl;			/* object handle */
	obj_tag_t	tag;			/* object info tag */
	obj_bag_size_t	info_size;		/* object info size */
	void		*infop;			/* object info ptr */
} obj_info_t;

typedef struct {
	cell_t		source_cell;		/* source cell */
	cell_t		target_cell;		/* target cell */
	obj_rmode_t	rmode;			/* relocation mode: RR|SR etc */
	obj_info_t	source;			/* source object info */
	obj_info_t	target;			/* target object info */
} obj_mft_info_t;

extern void obj_mft_info_get(
		obj_manifest_t *,		/* IN manifest */
		obj_mft_info_t *);		/* IN object info structure */

extern void obj_mft_info_put(
		obj_manifest_t *,		/* IN manifest */
		obj_mft_info_t *);		/* IN object info structure */


/*
 * Object Relocation Interface.
 *
 * Object managers must provide callout routines according to the
 * following interfaces so that object servers can be relocated
 * by the Kernel Object Relocation Engine, KORE.
 */
typedef struct {
	/*
	 * Prepare an object for relocation to 'target' cell.
	 * This involves verifying that the object can be moved
	 * and setting its state as OBJ_STATE_SR_OUTBOUND. The identity
	 * the object is added (as a bag) to the object manifest.
	 * This bag may carry, in particular, an existence token.
	 * Called on the source cell of an object.
	 */
	int (*obj_source_prepare)(
		obj_manifest_t *,	/* IN object manifest */
		void *);		/* IN virtual object */
	/*
	 * Prepare for object relocation from 'source' cell.
	 * This involves verifying that the object can be moved
	 * to the local cell. The virtual object, which may need to be
	 * created, is quiesced and its state is set to OBJ_STATE_SR_INBOUND.
	 * An embryonic ds is interposed and its objid will
	 * be returned to the source cell which will retarget
	 * all clients to the new target. A target info bag may
	 * also be added to the manifest containing recalled
	 * token state and data; this is returned to the source cell.
	 */
	int (*obj_target_prepare)(
		obj_manifest_t *,	/* IN object manifest */
		void **);		/* OUT virtual object prepared */
	/*
	 * Quiesce the source server. When this has been done
	 * new lookups are blocked and hence the (returned) token
	 * server state is stable for existence tokens.
	 * KORE retargets all clients follwing this step.
	 */
	int (*obj_source_retarget)(
		obj_manifest_t *,	/* IN object manifest */
		tks_state_t **);	/* OUT token server state */
	/*
	 * Quiesce and retarget a client to the embryonic server.
	 * The object manifest is read to obtain lookup information.
	 */
	int (*obj_client_retarget)(
		obj_manifest_t *);	/* IN object manifest */
	/*
	 * Package the state of object into a bag.
	 * The local virtual object has a dc behavior installed.
	 */
	int (*obj_source_bag)(
		obj_manifest_t *,	/* IN object manifest */
		obj_bag_t);		/* IN object state bag */
	/*
	 * Unpackage the state of object from a bag.
	 * The physical object and the token server state
	 * is recreated on the local cell and the virtual object
	 * is unquiesced.
	 */
	int (*obj_target_unbag)(
		obj_manifest_t *,	/* IN object manifest */
		obj_bag_t);		/* IN object state bag */
	/*
	 * Relocation complete.
	 * The source object is to be torn-down.
	 */
	int (*obj_source_end)(
		obj_manifest_t *);	/* IN object manifest */
	/*
	 * Abort relocation on source cell.
	 */
	int (*obj_source_abort)(
		obj_manifest_t *);	/* IN object manifest */
	/*
	 * Abort relocation on target cell.
	 */
	int (*obj_target_abort)(
		obj_manifest_t *);	/* IN object manifest */
} obj_relocation_if_t;

/*
 * The following table defines the interface to a set of
 * object management functions for an object type.  
 */
typedef struct {
	/*
	 * Lookup server cell for object identified by id.
	 */
	int (*obj_lookup)(
		void *,			/* IN object id */
		service_t *);		/* OUT server's service */
	/*
	 * Locate local object identified by id and relocate
	 * its server to another cell.
	 * A NULL handle pointer requests eviction of all objects.
	 */
	int (*obj_evict)(
		void *,			/* IN object id */
		cell_t);		/* IN target cell */
	/*
	 * Shutdown object subsystem.
	 * This involves purging caches and ensuring no dependency
	 * remaining on the local cell and handing-off the namespace.
	 */
	int (*obj_shutdown)(
		cell_t);		/* IN surrogate cell */
} obj_service_if_t;

/*
 * External interface to object manger functions.
 */
extern int	obj_svr_lookup(service_t, void *, service_t *);
extern int	obj_svr_evict(service_t, void *, cell_t);
extern int	obj_svr_shutdown(service_t, cell_t);

/*
 * Macro determining whether a handle represents a local physical object.
 */
#define OBJ_HANDLE_IS_LOCAL(hp)	 ((hp)->h_service.s_cell == cellid())	

/*
 * Register the object manager for a given service as being
 * capable of performing object relocation using the supplied
 * callout vector.
 */
extern void obj_service_register(
		service_t,		/* IN (cell, svcnum) */	
		obj_service_if_t *,	/* IN object manager interface */
		obj_relocation_if_t *);	/* IN object relocation callout tbl */

/*
 * The state of an object appropo relocation is recorded in the
 * object's state structure.
 */
typedef struct {
	mutex_t		ost_lock;	/* lock */
	bhv_head_t	*ost_bhp;	/* behavior head */
	short		ost_state;	/* state */
	short		ost_nrefs;	/* num references intransit */
} obj_state_t;

#define	OBJ_STATE_LOCAL		0	/* immobile */
#define	OBJ_STATE_SR_OUTBOUND	1	/* relocating server from local cell */
#define	OBJ_STATE_SR_INBOUND	2	/* relocating server to local cell */
#define	OBJ_STATE_SR_ORPHANED	3	/* relocation reqd, no local client */

#define OBJ_STATE_INIT(osp, bhp)					\
	mutex_init(&(osp)->ost_lock, MUTEX_DEFAULT, "obj_state");	\
	(osp)->ost_bhp = bhp;						\
	(osp)->ost_state = 0
#define OBJ_STATE_DESTROY(osp)		mutex_destroy(&(osp)->ost_lock)
#define OBJ_STATE_LOCK(osp)		mutex_lock(&(osp)->ost_lock, PZERO)
#define OBJ_STATE_UNLOCK(osp)		mutex_unlock(&(osp)->ost_lock)

#define OBJ_STATE_IS_RR(osp) 				\
	((osp)->ost_state == OBJ_STATE_RR_OUTBOUND)
#define OBJ_STATE_IS_INBOUND(osp) 			\
	((osp)->ost_state == OBJ_STATE_SR_INBOUND)
#define OBJ_STATE_IS_OUTBOUND(osp) 			\
	((osp)->ost_state == OBJ_STATE_SR_OUTBOUND)
#define OBJ_STATE_IS_INTRANSIT(osp) 			\
	(OBJ_STATE_IS_OUTBOUND(osp) || OBJ_STATE_IS_INBOUND(osp))

/*
 * Macro to be called by server routines to check that object
 * relocation is not underway and to wait on the behavior chain
 * lock if so. If a client has been retargetted to a new cell,
 * we may be racing with the object being relocated there and
 * so we wait for an access lock on the behavior chain before
 * proceeding.
 * 
 */
#define OBJ_SVR_RETARGET_CHECK(osp)			\
	if (OBJ_STATE_IS_INBOUND(osp)) {		\
		BHV_READ_LOCK((osp)->ost_bhp);		\
		BHV_READ_UNLOCK((osp)->ost_bhp);	\
	}

/*
 * Macros recording Reference Relocation (RR) state changes:
 */
static __inline int
obj_RR_source_begin(obj_state_t *osp)
{
        int     nrefs;

	OBJ_STATE_LOCK(osp);
	nrefs = osp->ost_nrefs++;
	OBJ_STATE_UNLOCK(osp);
	return nrefs;
}

#define obj_RR_source_end(osp)				\
        OBJ_STATE_LOCK(osp);				\
	osp->ost_nrefs--;				\
	OBJ_STATE_UNLOCK(osp)

#define obj_RR_source_is_outbound(osp)		OBJ_STATE_IS_RR(osp)


/*
 * Macros recording Server Relocation (SR) state changes:
 */
static __inline int
obj_SR_source_begin(obj_state_t *osp)
{
        int     error = OBJ_SUCCESS;

	OBJ_STATE_LOCK(osp);
	if (OBJ_STATE_IS_INTRANSIT(osp)) {
		error = EMIGRATING;
	} else {
		osp->ost_state = OBJ_STATE_SR_OUTBOUND;
	}
	OBJ_STATE_UNLOCK(osp);
	return error;
}
#define obj_SR_source_end(osp)				\
	(osp)->ost_state = OBJ_STATE_LOCAL

#define obj_SR_target_prepared(osp)			\
	ASSERT(BHV_IS_WRITE_LOCKED((osp)->ost_bhp));	\
	(osp)->ost_state = OBJ_STATE_SR_INBOUND

#define obj_SR_target_end(osp)				\
	(osp)->ost_state = OBJ_STATE_LOCAL

#define obj_SR_source_abort(osp)			\
	(osp)->ost_state = OBJ_STATE_LOCAL

#define obj_SR_target_abort(osp)			\
	(osp)->ost_state = OBJ_STATE_LOCAL

#define obj_SR_source_is_outbound(osp)	OBJ_STATE_IS_OUTBOUND(osp)
#define obj_SR_target_is_inbound(osp)	OBJ_STATE_IS_INBOUND(osp)


typedef void * obj_retarget_t;

/*
 * Object retarget support routines:
 */
extern void obj_retarget_begin(		/* Retargetting starts */
		obj_retarget_t *);		/* IN retarget state */
extern void obj_retarget_end(		/* Retargetting done */
		obj_retarget_t *);		/* IN retarget state */
extern void obj_retarget_register(	/* Register client cell */
		obj_retarget_t *,		/* IN retarget state */
		cell_t);			/* IN client cell */
extern void obj_retarget_deregister(	/* Client has retargetted */
		obj_retarget_t *,		/* IN retarget state */
		cell_t);			/* IN client cell */
extern void obj_retarget_wait(		/* Wait for all clients */
		obj_retarget_t *);		/* IN retarget state */

/* Initialize the kernel object relocation engine for a cell. */
extern void obj_kore_init(void);

#endif	/* _KSYS_RELOCATION_H_ */
