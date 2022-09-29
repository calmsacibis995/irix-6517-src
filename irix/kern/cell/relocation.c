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
#ident "$Id: relocation.c,v 1.36 1997/10/02 20:32:39 cp Exp $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <ksys/cell/object.h>
#include <ksys/cell/relocation.h>

#include "relocation_private.h"

#include "invk_kore_stubs.h"
#include <ksys/cell/subsysid.h>
#include "I_kore_stubs.h"

#include <sys/ktrace.h>
#if DEBUG && OBJ_TRACE
struct ktrace	*obj_trace = NULL;
cell_t		obj_trace_id = -1;
static void idbg_obj_trace(__psint_t);
static void idbg_manifest(__psint_t);
#endif

struct obj_celldata *obj_celldatap;

/*
 * Initialize a cell's object relocation engine - a service
 * which directs the relocation of object servers between cells.
 */
void
obj_kore_init(void)
{
	obj_celldatap = kern_calloc(1, sizeof(struct obj_celldata));
#if DEBUG && OBJ_TRACE
	if (obj_trace == NULL) {
		obj_trace = ktrace_alloc(10000, 0);
		idbg_addfunc("objtrace", idbg_obj_trace);
		idbg_addfunc("manifest", idbg_manifest);
	}
#endif
	mesg_handler_register(kore_msg_dispatcher, OBJECT_SUBSYSID);
}

/*
 * Register an object manager for a given service as capable of
 * performing object relocation using the supplied callout vector.
 */
void
obj_service_register(
	service_t		svc,
	obj_service_if_t	*obj_service_vector,
	obj_relocation_if_t	*obj_relocation_vector)
{
	ASSERT(SERVICE_TO_CELL(svc) == cellid());
	ASSERT(SERVICE_TO_SVCNUM(svc) < NUMSVCS);
	obj_celldatap->obj_service_table[SERVICE_TO_SVCNUM(svc)] =
		obj_service_vector;
	obj_celldatap->obj_callout_table[SERVICE_TO_SVCNUM(svc)] =
		obj_relocation_vector;
	OBJ_TRACE6("obj_service_register", svc.s_intval,
		   "\n  obj_service_vector", obj_service_vector,
		   "obj_relocation_vector", obj_relocation_vector);
}


/************************************************************************
 * Object Management Functions                                          *
 * The following routines vector into the respective object manager     *
 * to request an object relocation function.                            * 
 ************************************************************************/

int
obj_svr_lookup(
	service_t	svc,
	void		*id,
	service_t	*svr)
{
	int			svc_num = SERVICE_TO_SVCNUM(svc);
	/* REFERENCED */
	cell_t			svc_cell = SERVICE_TO_CELL(svc);
	obj_service_if_t	*svcif;

	ASSERT(svc_cell == cellid());

	OBJ_TRACE4("obj_svr_lookup", svc.s_intval, "id", id);

	svcif = obj_celldatap->obj_service_table[svc_num];
	if (!svcif)
		return EINVAL;
	if (!svcif->obj_lookup)
		return EPERM;

	return (*(svcif->obj_lookup))(id, svr);
}
 
void
I_kore_svr_evict(
	int		svc_num,
	void		*id,
	cell_t		target,
	int		*error)
{
	service_t	obj_svc;

	SERVICE_MAKE(obj_svc, cellid(), svc_num);
	*error = obj_svr_evict(obj_svc, id, target);
}

int
obj_svr_evict(
	service_t	svc,
	void		*id,
	cell_t		target)
{
	int			svc_num = SERVICE_TO_SVCNUM(svc);
	cell_t			svc_cell = SERVICE_TO_CELL(svc);
	obj_service_if_t	*svcif;
	int			error;

	OBJ_TRACE6("obj_svr_evict", svc.s_intval, "id", id, "target", target);

	svcif = obj_celldatap->obj_service_table[svc_num];
	if (!svcif)
		return EINVAL;
	if (!svcif->obj_evict)
		return EPERM;

	/*
	 * If necessary, function-ship to the server cell.
	 */
	if (svc_cell == cellid()) {
		error = (*(svcif->obj_evict))(id, target);
	} else {
		/* REFERENCED */
		int		msgerr;
		service_t	kore_svc;
		SERVICE_MAKE(kore_svc, svc_cell, SVC_OBJECT);
	
		msgerr = invk_kore_svr_evict(kore_svc, svc_num, id, target,
					     &error);
		ASSERT(!msgerr);
	}
	return error;
}

void
I_kore_svr_shutdown(
	int		svc_num,
	cell_t		surrogate,
	int		*error)
{
	service_t	obj_svc;

	SERVICE_MAKE(obj_svc, cellid(), svc_num);
	*error = obj_svr_shutdown(obj_svc, surrogate);
}

int
obj_svr_shutdown(
	service_t	svc,
	cell_t		surrogate)
{
	int			svc_num = SERVICE_TO_SVCNUM(svc);
	cell_t			svc_cell = SERVICE_TO_CELL(svc);
	obj_service_if_t	*svcif;
	int			error;

	ASSERT(svc_cell == cellid());

	OBJ_TRACE4("obj_svr_shutdown", svc.s_intval, "surrogate", surrogate);

	svcif = obj_celldatap->obj_service_table[svc_num];
	if (!svcif)
		return EINVAL;
	if (!svcif->obj_shutdown)
		return EPERM;

	/*
	 * If necessary, function-ship to the server cell.
	 */
	if (svc_cell == cellid()) {
		error = (*(svcif->obj_shutdown))(surrogate);
	} else {
		/* REFERENCED */
		int		msgerr;
		service_t	kore_svc;
		SERVICE_MAKE(kore_svc, svc_cell, SVC_OBJECT);
	
		msgerr = invk_kore_svr_shutdown(kore_svc, svc_num,
						surrogate, &error);
		ASSERT(!msgerr);
	}
	return error;
}


/************************************************************************
 * Object Lists								*
 * Object lists are object handle lists in the form of object bags.	*
 ************************************************************************/

/*
 * Create a list. The expected length dictates the initial block
 * allocation for the underlying bag.
 */
obj_list_t
obj_list_create(
	uint_t		expected_length)
{
#ifdef DEBUG
	uint_t	entry_size = sizeof(obj_handle_t) + 2*sizeof(obj_tag_t);
#else
	uint_t	entry_size = sizeof(obj_handle_t) + sizeof(obj_tag_t);
#endif
	return obj_bag_create(expected_length * entry_size);
}

/*
 * Put a handle on (the end of) a list.
 */
void
obj_list_put(
	obj_list_t	list,
	obj_handle_t	*object)
{
	/* REFERENCED */
	int	error;

	error = obj_bag_put(list, OBJ_TAG_HANDLE,
			    (void *) object, sizeof(obj_handle_t));
	ASSERT(!error);
}

/*
 * Read next handle from a list.
 */
int
obj_list_get(
	obj_list_t	list,
	obj_handle_t	*object)
{
	int	error;
	obj_bag_get_here(list, OBJ_TAG_HANDLE,
			 (void *) object, sizeof(obj_handle_t), error);
	return error;
}



/************************************************************************
 * Kernel Object Relocation Engine					*
 * Object manifests are employed to manage the relocation of a list of	*
 * objects from the local source cell to a target destination cell. A	*
 * series of call-outs are performed to the associated object managers	*
 * to load and unload object data into object bags to effect the 	*
 * relocation of the set of objects from source to target cells.	*
 ************************************************************************/

obj_bag_t
obj_mft_src_info(
	obj_manifest_t	*mftp)
{
	ASSERT(mftp->sinfo);
	return mftp->sinfo;
}

obj_bag_t
obj_mft_tgt_info(
	obj_manifest_t	*mftp)
{
	ASSERT(mftp->tinfo);
	return mftp->tinfo;
}


static void
obj_info_put(
	obj_bag_t	bagp,
	obj_info_t	*ip)
{
	obj_tag_t	tag = ip->tag;
	obj_bag_size_t	size = ip->info_size;
	void		*infop = ip->infop;

	obj_bag_put(bagp, OBJ_TAG_HANDLE, &ip->hndl, sizeof(obj_handle_t));
	if (tag != OBJ_TAG_NONE && infop != NULL && size != 0)
		obj_bag_put(bagp, tag, infop, size);
	else
		obj_bag_put(bagp, OBJ_TAG_NONE, NULL, 0);
}

void
obj_mft_info_put(
	obj_manifest_t	*mftp,
	obj_mft_info_t	*mip)
{
	ASSERT(mip);
	ASSERT(mftp);
	ASSERT(mftp->step == OBJ_SOURCE_PREPARE ||
	       mftp->step == OBJ_TARGET_PREPARE);


	switch (mftp->step) {
	case OBJ_SOURCE_PREPARE:
		ASSERT(mftp->sinfo);
		obj_info_put(mftp->sinfo, &mip->source);
		obj_bag_put(mftp->sinfo, OBJ_TAG_RMODE,
			    &mip->rmode, sizeof(obj_rmode_t));
		break;
	case OBJ_TARGET_PREPARE:
		ASSERT(mftp->tinfo);
		obj_info_put(mftp->tinfo, &mip->target);
		obj_bag_put(mftp->tinfo, OBJ_TAG_RMODE,
			    &mip->rmode, sizeof(obj_rmode_t));
		break;
	default:
		panic("obj_mft_info_put: invalid call");
	}
}

static void
obj_info_get(
	obj_bag_t	bagp,
	obj_info_t	*ip)
{
	obj_tag_t	tag = ip->tag;
	obj_bag_size_t	size = ip->info_size;
	void		*infop = ip->infop;
	/* REFERENCED */
	int		error;

	obj_bag_get_here(bagp, OBJ_TAG_HANDLE,
			 &ip->hndl, sizeof(obj_handle_t), error);
	ASSERT(!error);
	if (tag != OBJ_TAG_NONE && infop != NULL && size != 0) {
		obj_bag_get_here(bagp, tag, infop, size, error);
		if (error) {
			/*
			 * If the expected tag wasn't gotten,
			 * continue but flag the info as null.
			 */
			ip->info_size = 0;
			obj_bag_skip(bagp, OBJ_TAG_ANY);
		}
	} else
		obj_bag_skip(bagp, OBJ_TAG_ANY);
}
	
void
obj_mft_info_get(
	obj_manifest_t	*mftp,
	obj_mft_info_t	*mip)
{
	/* REFERENCED */
	int		error;

	ASSERT(mip);
	ASSERT(mftp);
	ASSERT(mftp->step >= OBJ_SOURCE_PREPARE);

	mip->source_cell = mftp->src;
	mip->target_cell = mftp->tgt;

	/*
	 * For the source prepare step, rmode is what
	 * obj_mft_put placed in the manifest structure.
	 */
	if (mftp->step == OBJ_SOURCE_PREPARE)
		mip->rmode = mftp->rmode;

	/*
	 * Return source info only after source prepare step.
	 */
	if (mftp->step > OBJ_SOURCE_PREPARE) {
		ASSERT(mftp->sinfo);
		obj_info_get(mftp->sinfo, &mip->source);
		obj_bag_get_here(mftp->sinfo, OBJ_TAG_RMODE,
				 &mip->rmode, sizeof(obj_rmode_t), error);
		ASSERT(!error);
	}

	/*
	 * Return target info only after target prepare step.
	 * Note: rmode flags updates that from source mft.
	 */ 
	if (mftp->step > OBJ_TARGET_PREPARE) {
		ASSERT(mftp->tinfo);
		obj_info_get(mftp->tinfo, &mip->target);
		obj_bag_get_here(mftp->tinfo, OBJ_TAG_RMODE,
				 &mip->rmode, sizeof(obj_rmode_t), error);
		ASSERT(!error);
	}
}

void
obj_mft_src_info_add(
	obj_manifest_t	*mftp,
	obj_tag_t	tag,
	void		*info,
	obj_bag_size_t	info_size)
{
	ASSERT(mftp->step == OBJ_SOURCE_PREPARE);
	ASSERT(mftp->sinfo);
	obj_bag_put(mftp->sinfo, tag, info, info_size);	
} 

static void
skip_to_tag(
	obj_bag_t  *bagp,
	obj_tag_t  tag_sought)
{
	obj_tag_t	tag;
	obj_bag_size_t  data_size;
	while (obj_bag_peek(bagp, &tag, &data_size),
	       (tag  != OBJ_TAG_EMPTY && tag != tag_sought))
		obj_bag_skip(bagp, OBJ_TAG_ANY);
}

/*
 * This routine discards the next object info for a given bag.
 * The bag is expected to contain a handle followed additional
 * info items.
 */
static void
skip_bag_info(
	obj_bag_t  *bagp)
{
	/* REFERENCED */
	int	error;

	error = obj_bag_skip(bagp, OBJ_TAG_HANDLE);
	ASSERT(!error);
	skip_to_tag(bagp, OBJ_TAG_HANDLE);
}

static void
obj_mft_sync(
	obj_manifest_t	*mftp)
{
	if (mftp->sinfo)
		skip_to_tag(mftp->sinfo, OBJ_TAG_HANDLE);
	if (mftp->tinfo)
		skip_to_tag(mftp->tinfo, OBJ_TAG_HANDLE);
}

static void
obj_mft_skip(
	obj_manifest_t	*mftp)
{
	if (mftp->sinfo)
		skip_bag_info(mftp->sinfo);
	if (mftp->tinfo)
		skip_bag_info(mftp->tinfo);
}

static void
obj_mft_rewind(
	obj_manifest_t	*mftp)
{
	if (mftp->sinfo)
		obj_bag_rewind(mftp->sinfo);
	if (mftp->tinfo)
		obj_bag_rewind(mftp->tinfo);
}
	
/*
 * Create an object manifest for relocation to a given cell
 * (from the local cell).
 */
obj_manifest_t *
obj_mft_create(
	cell_t		target)
{
	obj_manifest_t	*mftp;

	mftp = kmem_alloc(sizeof(obj_manifest_t), KM_SLEEP);
	ASSERT(mftp);

	mftp->nobj = 0;
	mftp->step = OBJ_SOURCE_PRELUDE;
	mftp->src = cellid();
	mftp->tgt = target;
	mftp->obj_depth = 0;
	mftp->obj_breadth = 0;
	mftp->sinfo = mftp->tinfo = NULL;

	OBJ_TRACE6("obj_mft_create", cellid(),
		   "target", target, "manifest", mftp);
	return mftp;
}

/*
 * Destroy a manifest after relocation.
 */
void
obj_mft_destroy(
	obj_manifest_t	*mftp)
{
	OBJ_TRACE4("obj_mft_destroy", cellid(), "manifest", mftp);
	if (mftp->sinfo)
		obj_bag_destroy(mftp->sinfo);
	if (mftp->tinfo)
		obj_bag_destroy(mftp->tinfo);
	kmem_free((void *) mftp, sizeof(obj_manifest_t));
}

/*
 * Add an object (or object tree) to a manifest.
 * This makes a callout to the associated subsystem (service)
 * to prepare this object for relocation or export/import.
 * This call indicates by relocation mode (rmode) whether there
 * is a referencing object relocating to the target cell.
 * The referenced virtual object may elect to perform a reference
 * relocation (by export/import/unref) or to relocate the server if
 * appropriate.
 */
int
obj_mft_put(
	obj_manifest_t	*src_manifest,
	void		*vobj,
	int		svc_num,
	obj_rmode_t	rmode)
{
	int			error = OBJ_SUCCESS;
	obj_relocation_if_t	*objif;
	obj_kore_step_t		last_step;
	obj_tree_pos_t		last_posn;
	obj_rmode_t		last_mode;
	obj_bag_posn_t		bag_posn;
	
	ASSERT(src_manifest);
	ASSERT(cellid() == src_manifest->src);
	ASSERT(src_manifest->step == OBJ_SOURCE_PRELUDE ||
	       src_manifest->step == OBJ_SOURCE_PREPARE);

	OBJ_TRACE10("obj_mft_put", cellid(), "manifest", src_manifest,
		    "\n  vobj", vobj, "svc_num", svc_num, "rmode", rmode);

	/*
	 * Allocate list/bags now if necessary.
	 */
	if (src_manifest->sinfo == NULL) {
		src_manifest->sinfo = obj_bag_create(0);
	}

	/*
	 * Note the current position in the info bag first.
	 */
	(void) obj_bag_tell(src_manifest->sinfo, OBJ_BAG_MODE_PUT, &bag_posn);

	/*
	 * Call into the subsystem to prepare the object for outbound
	 * relocation. Source information required by the target cell
	 * is added into the info bag attached to the manifest.
	 * The subsytem returns the object id of the prepared object
	 * - although this will be null if the object is being exported
	 * rather than relocated.
	 */
	objif = obj_celldatap->obj_callout_table[svc_num];
	ASSERT(objif);
	ASSERT(objif->obj_source_prepare);
	OBJ_TRACE8("obj_mft_put", cellid(),
		   "\n  vobj", vobj, "depth", src_manifest->obj_depth,
		   "breadth", src_manifest->obj_breadth);

	/*
	 * Save current tree coordinates.
	 * Bump the depth coordinate as we descend into a nested object
	 * and reset the breadth count.
	 */
	last_posn = src_manifest->posn;
	src_manifest->obj_depth++;
	src_manifest->obj_breadth = 0;
	last_mode = src_manifest->rmode;
	src_manifest->rmode = rmode;
	last_step = src_manifest->step;
	src_manifest->step = OBJ_SOURCE_PREPARE;

	/*
	 * Call into the relevant subsystem to have the object
	 * prepared for relocation (or to be exported).
	 */
	error = (*(objif->obj_source_prepare))(src_manifest,
					       vobj);
	src_manifest->posn = last_posn;
	src_manifest->step = last_step;
	src_manifest->rmode = last_mode;
	if (error) {
		/* Truncate the info bag */
		(void) obj_bag_seek(src_manifest->sinfo,
				    OBJ_BAG_MODE_PUT, &bag_posn);
		OBJ_TRACE8("obj_mft_put", cellid(),
			   "manifest", src_manifest,
			   "\n  vobj", vobj, "error", error);
		return error;
	}

	
	/*
	 * Increment count and the breath coord to register this call.
	 */
	src_manifest->nobj++;
	src_manifest->obj_breadth++;

	OBJ_TRACE8("obj_mft_put", cellid(), "manifest", src_manifest,
		   "\n  *(obj_source_prepare) vobj", vobj,
		   "nobj", src_manifest->nobj);
	return error;
}

void kore_retarget_clients(obj_manifest_t *, bitmap_t *);	/* forward */

void
kore_target_prepare(
	cell_t		target_cell,		/* IN target cell */
	int		nobj,			/* IN num objects to relocate */
	obj_bag_t	src_info,	 	/* IN source info bag */
	obj_bag_t	*tgt_infop,		/* OUT target info bag */
	void		**tgt_id,		/* OUT target info id */
	int		*errorp)		/* OUT return value */
{
	service_t		target_obj_svc;
	obj_bag_header_t	src_info_hdr;
	obj_bag_vector_t	*src_info_vecp;
	size_t			src_info_count;
	obj_bag_header_t	tgt_info_hdr;
	obj_bag_vector_t	*tgt_info_vecp = NULL;
	size_t			tgt_info_count = 0;
	/* REFERENCED */
	int			msgerr;

	/*
	 * For messaging we convert object bags to/from header&vector.
	 */
	obj_bag_to_message(src_info, &src_info_hdr,
				&src_info_vecp, &src_info_count);

	SERVICE_MAKE(target_obj_svc, target_cell, SVC_OBJECT);
	msgerr = invk_kore_target_prepare(target_obj_svc, cellid(),
				 nobj,
				 &src_info_hdr,
				 (src_prep_invector_t *) src_info_vecp,
				 src_info_count,
				 &tgt_info_hdr,
				 (tgt_prep_outvector_t **) &tgt_info_vecp,
				 &tgt_info_count,
				 tgt_id,
				 errorp); 
	ASSERT(!msgerr);

	if (*errorp == 0) {
		*tgt_infop = obj_bag_from_message(
				&tgt_info_hdr, tgt_info_vecp, tgt_info_count);
		obj_bag_free_message(tgt_info_vecp, tgt_info_count);
	}
	obj_bag_free_message(src_info_vecp, src_info_count);
}

/*
 * Ship the objects listed by a manifest (and now prepared for
 * migration). This call results in callouts to have the (tree of)
 * objects prepared on the target cell and subsequently bagged on
 * the source cell and unbagged at the target.
 *
 * Note that an object tree is prepared top-down on source and
 * target cells. That is, source information is gathered from the
 * top of the tree down to leaf objects and embronic objects 
 * are created on the target cell in the same sequence. However,
 * once prepared, bagging, transfer and unbagging is carried out
 * bottom-up. That is, leaf objects are fully instantiated first.
 */
int
obj_mft_ship(
	obj_manifest_t	*src_manifest)
{
	int			error = OBJ_SUCCESS;
	cell_t			destination;
	obj_bag_t		src_info;
	obj_handle_t		src_hndl;
	obj_tag_t		hndl_tag = OBJ_TAG_HANDLE;
	obj_bag_size_t		hndl_size = sizeof(obj_handle_t);
	service_t		target_obj_svc;
	obj_relocation_if_t	*objif;
	int			svc_num;
	obj_bag_t		object_bag;
	void			*tgt_id;
	int			obj;
	int			nobj;
	bitmap_t		abt_bmp = NULL;
	obj_bag_header_t	bag_hdr;
	obj_bag_vector_t	*bag_vecp;
	size_t			bag_vec_count;
	/* REFERENCED */
	int			msgerr;

	ASSERT(src_manifest);
	ASSERT(cellid() == src_manifest->src);
	ASSERT(src_manifest->step == OBJ_SOURCE_PRELUDE);
	src_info = src_manifest->sinfo;
	ASSERT(src_info);
	destination = src_manifest->tgt;
	nobj = src_manifest->nobj;
	SERVICE_MAKE(target_obj_svc, destination, SVC_OBJECT);
	
	OBJ_TRACE8("obj_mft_ship", cellid(), "manifest", src_manifest,
		   "\n  destination", destination, "src_info", src_info);

	/*
	 * Called with a manifest of objects whose servers are
	 * local and whose behavior chain is already marked for
	 * outbound relocation.
	 */ 

	if (src_manifest->nobj == 0)
		return OBJ_SUCCESS;

	/*
	 * Call the target to prepare all objects.
	 * It returns its completed list containing target handles
	 * and target object info.
	 */
	kore_target_prepare(destination,
			    src_manifest->nobj,
			    src_manifest->sinfo,
			    &src_manifest->tinfo,
			    &tgt_id,
			    &error); 
	OBJ_TRACE6("obj_mft_ship", cellid(),
		   "tinfo", src_manifest->tinfo, "error", error);
	ASSERT(!error);

	/*
	 * Perform the client retargetting.
	 */
	src_manifest->step = OBJ_SOURCE_RETARGET;
	kore_retarget_clients(src_manifest, &abt_bmp);

	object_bag = obj_bag_create(0);

	obj_mft_rewind(src_manifest);
	src_manifest->step = OBJ_SOURCE_BAG;
	for (obj = 0; obj < nobj; obj++) {
		error = obj_bag_look(src_info, &hndl_tag, &src_hndl, &hndl_size);
		ASSERT(!error);
		svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(src_hndl));
		objif = obj_celldatap->obj_callout_table[svc_num];
		if (abt_bmp && btst(abt_bmp, obj)) {
			obj_bag_put(object_bag, OBJ_TAG_NONE, NULL, 0);
			/* Skip over unused info */
			obj_mft_skip(src_manifest);
			continue;
		}
		ASSERT(objif->obj_source_bag);
		error = (*(objif->obj_source_bag))(src_manifest,
						   object_bag);
		OBJ_TRACE10("obj_mft_ship", cellid(), "bag", object_bag,
			    "\n  *(obj_source_bag) svc_num", svc_num,
			    "objid", HANDLE_TO_OBJID(src_hndl),
			    "error", error);
		if (error) {
			/*
			 * Add a null entry to the object bag and
			 * remember the object id to be aborted.
			 * XXX - for compatibilty.
			 */
			obj_bag_put(object_bag, OBJ_TAG_NONE, NULL, 0);
			if (!abt_bmp)
				abt_bmp = bitmap_zalloc(nobj);
			bset(abt_bmp, obj);
		}
		/* Discard any unread manifest info for this object */
		obj_mft_sync(src_manifest);
	}

	/*
	 * Call the target to unbag all objects.
	 */
	obj_bag_to_message(object_bag, &bag_hdr, &bag_vecp, &bag_vec_count);
	msgerr = invk_kore_target_unbag(target_obj_svc, cellid(),
			       tgt_id,
			       &bag_hdr,
			       (object_state_vec_invector_t *) bag_vecp,
			       bag_vec_count,
			       &error); 
	ASSERT(!msgerr);
	OBJ_TRACE8("obj_mft_ship", cellid(), "manifest", src_manifest,
		   "\n  invk_kore_target_unbag object_bag", object_bag,
		   "error", error);
	ASSERT(!error);
	obj_bag_free_message(bag_vecp, bag_vec_count);

	obj_bag_destroy(object_bag);

	/*
	 * Finally, clean up each source object.
	 * If the object's relocation was aborted, it will be
	 * flagged in the abort bitmap.
	 */
	obj_mft_rewind(src_manifest);
	for (obj = 0; obj < nobj; obj++) {
		/* REFERENCED */
		objid_t	src_objid;
		error = obj_bag_look(src_info, &hndl_tag, &src_hndl, &hndl_size);
		ASSERT(!error);
		src_objid = HANDLE_TO_OBJID(src_hndl);
		svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(src_hndl));
		objif = obj_celldatap->obj_callout_table[svc_num];
		if (abt_bmp && btst(abt_bmp, obj)) {
			ASSERT(objif->obj_source_abort);
			src_manifest->step = OBJ_SOURCE_ABORT;
			error = (*(objif->obj_source_abort))(src_manifest);
			OBJ_TRACE8("obj_mft_ship", cellid(),
				   "\n  *(obj_source_abort) svc_num", svc_num,
				   "objid", src_objid,
				   "error", error);
		} else {
			ASSERT(objif->obj_source_end);
			src_manifest->step = OBJ_SOURCE_END;
			error = (*(objif->obj_source_end))(src_manifest);
			OBJ_TRACE10("obj_mft_ship", cellid(),
				    "manifest", src_manifest,
				    "\n  *(obj_source_end) svc_num", svc_num,
				    "objid", src_objid,
				    "error", error);
		}
		ASSERT(!error);
		/* Discard any unread manifest info for this object */
		obj_mft_sync(src_manifest);
	}
	if (abt_bmp)
		bitmap_free(abt_bmp, nobj);
	return error;
}


/*
 * Here on the target cell to prepare a set of objects
 * to receive inbound relocation. A manifest is provided from the
 * source cell to identify each object in an object-type specific
 * manner. A bag is returned containing object-type specific data
 * which may contain returning tokens and associated state.
 */
/* ARGSUSED */
void
I_kore_target_prepare(
	cell_t			source_cell,		/* IN source cell */
	int			nobj,			/* IN num objects */
	obj_bag_header_t	*src_info_hdrp,	 	/* IN info header */
	src_prep_invector_t	*src_info_vecp,	 	/* IN info vector */
	size_t			src_info_vec_count, 	/* IN info size */
	obj_bag_header_t	*tgt_info_hdrp,		/* OUT header */
	tgt_prep_outvector_t	**tgt_infopp,		/* OUT vector */
	size_t			*tgt_info_countp,	/* OUT vector size */
	void			**tgt_id,		/* OUT target info id */
	int			*errorp,		/* OUT return value */
	void			**desc)			/* OUT unused */
{
	int		error;
	obj_manifest_t	*tgt_manifest;
	obj_tag_t	tag;
	obj_bag_size_t	size;
	void		*v;

	/*
	 * Create the manifest data structure to be passed down
	 * the object tree here on the target cell.
	 */
	tgt_manifest = obj_mft_create(cellid());
	*tgt_id = (void *) tgt_manifest;
	tgt_manifest->src = source_cell;
	tgt_manifest->step = OBJ_TARGET_PREPARE;
	tgt_manifest->tinfo = obj_bag_create(0);
	tgt_manifest->sinfo = obj_bag_from_message(
					src_info_hdrp,
					(obj_bag_vector_t *) src_info_vecp,
					src_info_vec_count);

	OBJ_TRACE6("I_kore_target_prepare", cellid(),
		   "source_cell", source_cell,
		   "\n  tgt_manifest", tgt_manifest);

	/*
	 * We loop through each top-level object making a call to the
	 * relevant object subsystem to do the object-type-specific work.
	 */
	*errorp = 0;
	while (obj_bag_peek(tgt_manifest->sinfo, &tag, &size),
	       tag == OBJ_TAG_HANDLE) {
		error = obj_mft_get(tgt_manifest, &v);
		if (error)
			(*errorp)++;
	}
	ASSERT(tag == OBJ_TAG_EMPTY);

	OBJ_TRACE6("I_kore_target_prepare", cellid(),
		  "\n  tgt_manifest", tgt_manifest, "error", error);
	ASSERT(tgt_manifest->nobj == nobj);

	obj_bag_to_message(tgt_manifest->tinfo,
				tgt_info_hdrp,
				(obj_bag_vector_t **) tgt_infopp,
				tgt_info_countp);
}

/* ARGSUSED */
void
I_kore_target_prepare_done(
	tgt_prep_outvector_t	*tgt_infop,		/* IN vector */
	size_t			tgt_info_count,		/* IN vector size */
	void			*desc)			/* IN descriptor */
{
	/* The vector (but not the bag) must be freed */
	obj_bag_free_message((obj_bag_vector_t *) tgt_infop, tgt_info_count);
}

/*
 * This routine is called on the target cell to prepare a tree of objects
 * to receive inbound relocation. A manifest is provided from the
 * source cell identifying each object in an object-type specific
 * manner. This function defers the lookup and/or creation of
 * objects by callout to the relevant system which is expected to
 * returns a pointer the corresponding virtual object. This may
 * in turn result in this function being called recursively to
 * receive lower-level objects described in the manifest.
 */
int
obj_mft_get(
	obj_manifest_t	*tgt_manifest,	/* IN target's object manifest */
	void		**vobj)		/* OUT virtual object */
{
	int			error = OBJ_SUCCESS;
	obj_relocation_if_t	*objif;
	int			svc_num;
	obj_tree_pos_t		posn;
	obj_tag_t		hndl_tag = OBJ_TAG_HANDLE;
	obj_handle_t		hndl;
	obj_bag_size_t		hndl_size = sizeof(obj_handle_t);
	
	ASSERT(tgt_manifest);
	ASSERT(cellid() == tgt_manifest->tgt);
	ASSERT(tgt_manifest->step == OBJ_TARGET_PREPARE);

	/*
	 * The relevant service number is obtained from the source
	 * info in the manifest.
	 */
	error = obj_bag_look(tgt_manifest->sinfo, &hndl_tag, &hndl, &hndl_size);
	ASSERT(!error);
	svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(hndl));
	OBJ_TRACE10("obj_mft_get", cellid(),
		    "depth", tgt_manifest->obj_depth,
		    "breadth", tgt_manifest->obj_breadth,
		    "\n  svc_num", svc_num,
		    "objid", HANDLE_TO_OBJID(hndl));

	/*
	 * Save current tree coordinates.
	 * Bump the depth coordinate as we descend into a nested object
	 * and reset the breadth count.
	 */
	posn = tgt_manifest->posn;
	tgt_manifest->obj_depth++;
	tgt_manifest->obj_breadth = 0;

	objif = obj_celldatap->obj_callout_table[svc_num];
	ASSERT(objif);
	ASSERT(objif->obj_target_prepare);
	error = (*(objif->obj_target_prepare))(tgt_manifest,
					       vobj);
	tgt_manifest->posn = posn;
	OBJ_TRACE10("obj_mft_get", cellid(),
		   "depth", tgt_manifest->obj_depth,
		   "breadth", tgt_manifest->obj_breadth,
		   "\n  *(obj_target_prepare) *vobj", *vobj, "error", error);
	ASSERT(!error);		/* XXX - what if not? */

	tgt_manifest->nobj++;

	/*
	 * Increment the breath component to register this call.
	 */
	tgt_manifest->obj_breadth++;

	return error;
}

/*
 * This server routine is invoked on the target cell to unbag object
 * data transferred from the source cell. This bagged data
 * corresponds with a manifest of previously prepared objects.
 * Unbagging consists of looping through the manifest and
 * calling-out to the relevant subsystem to do the dirty work.
 */
/* ARGSUSED */
void
I_kore_target_unbag(
	cell_t			source_cell,	/* IN source cell */
	void			*tgt_id,	/* IN target manifest id */
	obj_bag_header_t	*object_bag_hdrp,
	object_state_vec_invector_t *object_bag_vecp,
	size_t			object_bag_vec_count,
	int			*errorp)	/* OUT return value */
{
	obj_manifest_t		*tgt_manifest;
	obj_handle_t		tgt_hndl;
	obj_relocation_if_t	*objif;
	obj_bag_t		tgt_info;
	obj_tag_t		hndl_tag = OBJ_TAG_HANDLE;
	obj_bag_size_t		hndl_size = sizeof(obj_handle_t);
	obj_bag_t		object_bag;
	int			svc_num;
	int			obj;
	int			nobj;
	/* REFERENCED */
	int			error;

	tgt_manifest = (obj_manifest_t *) tgt_id;
	ASSERT(tgt_manifest->step == OBJ_TARGET_PREPARE);
	nobj = tgt_manifest->nobj;
	tgt_info = tgt_manifest->tinfo;

	object_bag = obj_bag_from_message(object_bag_hdrp,
					  (obj_bag_vector_t *) object_bag_vecp,
					  object_bag_vec_count);

	OBJ_TRACE8("I_kore_target_unbag", cellid(), "source_cell", source_cell,
		   "\n  bag", object_bag, "tinfo", tgt_info);

	/*
	 * Here on the target cell to unbag the set of objects.
	 * 
	 * We loop through each object making a call to the unbag object
	 * specific routine to do the object-type-specific work.
	 * Note that a null item (OBJ_TAG_NONE) in the bag results in a
	 * call to the abort callout.
	 */
	obj_mft_rewind(tgt_manifest);
	for (obj = 0; obj < nobj; obj++) {
		error = obj_bag_look(tgt_info, &hndl_tag, &tgt_hndl, &hndl_size);
		ASSERT(!error);
		svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(tgt_hndl));
		objif = obj_celldatap->obj_callout_table[svc_num];
		ASSERT(objif);
		if (obj_bag_skip(object_bag, OBJ_TAG_NONE) == OBJ_BAG_ERR_TAG) {
			ASSERT(objif->obj_target_unbag);
			tgt_manifest->step = OBJ_TARGET_UNBAG;
			error = (*(objif->obj_target_unbag))(
					tgt_manifest,
					object_bag);
			OBJ_TRACE10("I_kore_target_unbag", cellid(),
				    "bag", object_bag,
				    "\n  *(obj_target_unbag) svc_num", svc_num,
				    "objid", HANDLE_TO_OBJID(tgt_hndl),
				    "error", error);
			ASSERT(!error);
		} else {
			/* REFERENCED */
			ASSERT(objif->obj_target_abort);
			tgt_manifest->step = OBJ_TARGET_ABORT;
			(void) (*(objif->obj_target_abort))(tgt_manifest);
			OBJ_TRACE8("I_kore_target_unbag", cellid(),
				    "bag", object_bag,
				    "\n  *(obj_target_abort) svc_num", svc_num,
				    "objid", HANDLE_TO_OBJID(tgt_hndl));
		}
		/* Discard any unread manifest info for this object */
		obj_mft_sync(tgt_manifest);
	}
	*errorp = 0;

	obj_mft_destroy(tgt_manifest);
	obj_bag_destroy(object_bag);

	OBJ_TRACE4("I_kore_target_unbag", cellid(),
		   "*errorp", *errorp);

}


/************************************************************************
 * Object Retargetting Support						*
 * The following routines are used to track the retargetting of client	*
 * cells as the source server redirects all its clients to the parpared *
 * target cell.								*
 ************************************************************************/

/*
 * Request commencement of object migration. Permission is
 * Initialize a synchronizing control structure.
 */
void
obj_retarget_begin(
	obj_retarget_t	*rt)
{
	obj_retarget_state_t	*rtp;

	rtp = (obj_retarget_state_t *) kmem_alloc(sizeof(obj_retarget_state_t),
						  KM_SLEEP);
	ASSERT(rtp);

	spinlock_init(&rtp->rt_lock, "rt_sync");
	sv_init(&rtp->rt_sync, SV_DEFAULT, "rt_sync");

	/* One for the pot to avoid premature triggering */
	rtp->rt_count = 1;

	*rt = (obj_retarget_t) rtp;
}

/*
 * Marks the end. All clients are have retargetted and we deallocate
 * the control structure.
 */
void
obj_retarget_end(
	obj_retarget_t	*rt)
{
	obj_retarget_state_t	*rtp;

	ASSERT(rt);
	rtp = (obj_retarget_state_t *) *rt;
	ASSERT(rtp);

	ASSERT(rtp->rt_count == 0);
	spinlock_destroy(&rtp->rt_lock);
	sv_destroy(&rtp->rt_sync);
	kmem_free((void *) rtp, sizeof(obj_retarget_t));

	*rt = NULL;
}

/*
 * Increment a count of the number of client cells being retargetted.
 */
/* ARGSUSED */
void
obj_retarget_register(
	obj_retarget_t	*rt,
	cell_t		client)
{
	obj_retarget_state_t	*rtp;
	int	s;

	ASSERT(rt);
	rtp = (obj_retarget_state_t *) *rt;
	ASSERT(rtp);

	s = mutex_spinlock(&rtp->rt_lock);
	rtp->rt_count++;
	mutex_spinunlock(&rtp->rt_lock, s);
}

/*
 * Decrement the count of the number of client cells being retargetted.
 * When there are none, signal this to anyone waiting.
 */
/* ARGSUSED */
void
obj_retarget_deregister(
	obj_retarget_t	*rt,
	cell_t		client)
{
	obj_retarget_state_t	*rtp;
	int	s;

	ASSERT(rt);
	rtp = (obj_retarget_state_t *) *rt;
	ASSERT(rtp);

	ASSERT(rtp->rt_count > 0);
	s = mutex_spinlock(&rtp->rt_lock);
	if (--(rtp->rt_count) == 0)
		sv_signal(&rtp->rt_sync);
	mutex_spinunlock(&rtp->rt_lock, s);
}

/*
 * Wait for all registered clients to deregister.
 */
void
obj_retarget_wait(
	obj_retarget_t	*rt)
{
	obj_retarget_state_t	*rtp;
	int	s;

	ASSERT(rt);
	rtp = (obj_retarget_state_t *) *rt;
	ASSERT(rtp);

	ASSERT(rtp->rt_count > 0);
	s = mutex_spinlock(&rtp->rt_lock);
	rtp->rt_count--;			/* Remove the one for the pot */
	while (rtp->rt_count > 0) {
		sv_wait(&rtp->rt_sync, PZERO, &rtp->rt_lock, s);
		s = mutex_spinlock(&rtp->rt_lock);
	}
	mutex_spinunlock(&rtp->rt_lock, s);
}

void
I_kore_client_retargetted(
	cell_t		client,		/* IN who's retargetted */
	int		nobj,		/* IN number of objects */
	bitmap_t	errobj_bmp,	/* IN objects not found */
	size_t		errobj_bmp_sz,	/* IN object bitmap size */
	obj_retarget_t	rt)		/* IN synchronization object */
{
	int			obj;
	obj_retarget_state_t	*rtsp;
	obj_manifest_t		*src_manifest;
	tks_state_t		**tkstate;
	obj_bag_header_t	src_hdr;
	obj_bag_vector_t	*src_vecp;
	size_t			src_vec_count;
	obj_bag_header_t	tgt_hdr;
	obj_bag_vector_t	*tgt_vecp;
	size_t			tgt_vec_count;

	rtsp = (obj_retarget_state_t *) rt;
	src_manifest = (obj_manifest_t *) rtsp->rt_data1;
	tkstate = (tks_state_t **) rtsp->rt_data2;

	OBJ_TRACE10("I_kore_client_retargetted", cellid(), "client", client,
		    "\n  nobj", nobj, "errobj_bmp", errobj_bmp,
		    "src_manifest", src_manifest);

	/*
	 * The job's done if there were no errore.
	 */
	if (errobj_bmp == NULL) {
		obj_retarget_deregister(&rt, client);
		return;
	}

	ASSERT(src_manifest);
	ASSERT(tkstate);
	/*
	 * Otherwise we have to re-check whether the erring objects
	 * still exist on this cell.  We try again if any remains
	 * - we presume there was a race in which the retarget
	 * overtook a grant and the new client hadn't been created.
	 */
	for (obj = 0; obj < nobj; obj++) {
		if (btst(errobj_bmp, obj)) {
			ASSERT(tkstate[obj]);
			if (!tks_isclient(tkstate[obj], client))
				bclr(errobj_bmp, obj);
		}
	}
	if (bitmap_isnull(errobj_bmp, nobj)) {
		/* No unfound object still exists - we're done */
		obj_retarget_deregister(&rt, client);
	} else {
		service_t	client_obj_svc;
		/* REFERENCED */
		int		msgerr;
		/*
		 * Try again...
		 */
		OBJ_TRACE10("I_kore_client_retargetted", CELL_LOCAL,
				"invk_kore_client_retarget cell", client,
				"\n  sinfo", src_manifest->sinfo,
				"tinfo", src_manifest->tinfo,
				"errobj_bmp", errobj_bmp);
		obj_bag_to_message(src_manifest->sinfo,
					&src_hdr, &src_vecp, &src_vec_count);
		obj_bag_to_message(src_manifest->tinfo,
					&tgt_hdr, &tgt_vecp, &tgt_vec_count);
		SERVICE_MAKE(client_obj_svc, client, SVC_OBJECT);
		msgerr = invk_kore_client_retarget(
				client_obj_svc, CELL_LOCAL,
				src_manifest->tgt, src_manifest->nobj,
				&src_hdr,
				(src_rt_invector_t *) src_vecp, src_vec_count,
				&tgt_hdr,
				(tgt_rt_invector_t *) tgt_vecp, tgt_vec_count,
				errobj_bmp, errobj_bmp_sz, rt);
		ASSERT(!msgerr);
		obj_bag_free_message(src_vecp, src_vec_count);
		obj_bag_free_message(tgt_vecp, tgt_vec_count);
	}
}

void
I_kore_client_retarget(
	cell_t			source_cell,	/* IN source server cell */
	cell_t			target_cell,	/* IN target server cell */
	int			nobj,		/* IN num objects to retarget */
	obj_bag_header_t	*src_hdrp, 	/* IN tgt info header */
	src_rt_invector_t	*src_vecp, 	/* IN tgt info vector */
	size_t			src_vec_count, 	/* IN tgt info size */
	obj_bag_header_t	*tgt_hdrp, 	/* IN tgt info header */
	tgt_rt_invector_t	*tgt_vecp, 	/* IN tgt info vector */
	size_t			tgt_vec_count, 	/* IN tgt info size */
	bitmap_t		object_bmp,	/* IN which objects */
	size_t			object_bmp_sz,	/* IN object bitmap size */
	obj_retarget_t		rt)		/* IN synchronization object */
{
	obj_manifest_t		*mftp;
	service_t		source_obj_svc;
	obj_relocation_if_t	*objif;
	bitmap_t		errobj_bmp = NULL;
	obj_tag_t		hndl_tag = OBJ_TAG_HANDLE;
	obj_bag_size_t		hndl_size = sizeof(obj_handle_t);
	obj_handle_t		hndl;
	int			svc_num;
	int			obj;
	int			error;
	/* REFERENCED */
	int			msgerr;

	mftp = obj_mft_create(cellid());
	mftp->src = source_cell;
	mftp->tgt = target_cell;
	mftp->step = OBJ_CLIENT_RETARGET;
	mftp->sinfo = obj_bag_from_message(src_hdrp,
						(obj_bag_vector_t *) src_vecp,
						src_vec_count);
	mftp->tinfo = obj_bag_from_message(tgt_hdrp,
						(obj_bag_vector_t *) tgt_vecp,
						tgt_vec_count);

	OBJ_TRACE10("I_kore_client_retarget", cellid(),
		    "source", source_cell, "sinfo bag", mftp->sinfo,
		    "\n  tinfo bag", mftp->tinfo, "object_bmp", object_bmp);

	obj_mft_rewind(mftp);
	for (obj = 0; obj < nobj; obj++) {
		error = obj_bag_look(mftp->tinfo, &hndl_tag, &hndl, &hndl_size);
		ASSERT(!error);
		svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(hndl));
		if (btst(object_bmp, obj)) {
			objif = obj_celldatap->obj_callout_table[svc_num];
			ASSERT(objif);
			ASSERT(objif->obj_client_retarget);
			error = (*(objif->obj_client_retarget))(mftp);
			OBJ_TRACE10("I_kore_client_retarget", cellid(),
				    "obj", obj,
				    " \n  *(obj_client_retarget) svc_num",
					svc_num,
				    "objid", HANDLE_TO_OBJID(hndl),
				    "error", error);
			if (error) {
				if (!errobj_bmp)
					errobj_bmp = bitmap_zalloc(nobj);
				bset(errobj_bmp, obj);
			}
		} else {
			/* Skip over unused info */
			obj_mft_skip(mftp);
		}
	}

	SERVICE_MAKE(source_obj_svc, source_cell, SVC_OBJECT);
	ASSERT(bitmap_size(nobj) == object_bmp_sz);
	msgerr = invk_kore_client_retargetted(source_obj_svc, CELL_LOCAL, nobj,
					errobj_bmp, object_bmp_sz, rt);
	ASSERT(!msgerr);
	if (errobj_bmp)
		bitmap_free(errobj_bmp, nobj);
	obj_mft_destroy(mftp);
}

void
kore_retarget_clients(
	obj_manifest_t	*src_manifest,		/* IN manifest */
        bitmap_t	*abt_bmpp)		/* OUT abort bitmap */
{
	int			error = OBJ_SUCCESS;
	cell_t			source;
	cell_t			target;
	obj_handle_t		src_hndl;
	obj_tag_t		hndl_tag = OBJ_TAG_HANDLE;
	obj_bag_size_t		hndl_size = sizeof(obj_handle_t);
	obj_relocation_if_t	*objif;
	int			svc_num;
	obj_bag_t		tgt_info;
	obj_bag_t		src_info;
	int			nobj;
	int			obj;
	int			cell;
	tks_state_t		**tkstate;
	obj_retarget_t		rt;
	obj_bag_header_t	src_hdr;
	obj_bag_vector_t	*src_vecp;
	size_t			src_vec_count;
	obj_bag_header_t	tgt_hdr;
	obj_bag_vector_t	*tgt_vecp;
	size_t			tgt_vec_count;

	ASSERT(src_manifest);
	ASSERT(cellid() == src_manifest->src);
	tgt_info = src_manifest->tinfo;
	src_info = src_manifest->sinfo;
	ASSERT(src_info);
	ASSERT(tgt_info);
	source = src_manifest->src;
	target = src_manifest->tgt;
	nobj = src_manifest->nobj;
	
	OBJ_TRACE8("kore_retarget_clients", cellid(), "manifest", src_manifest,
		   "\n  target", target, "src_info", src_info);

	/*
	 * Allocate token state pointers for each object.
	 */ 
	tkstate = (tks_state_t **) kmem_zalloc(nobj*sizeof(tks_state_t *),
					       KM_SLEEP);

	obj_retarget_begin(&rt);
	((obj_retarget_state_t *) rt)->rt_data1 = (void *) src_manifest;
	((obj_retarget_state_t *) rt)->rt_data2 = (void *) tkstate;

	obj_mft_rewind(src_manifest);
	ASSERT(*abt_bmpp == NULL);
	for (obj = 0; obj < nobj; obj++) {
		error = obj_bag_look(src_info, &hndl_tag, &src_hndl, &hndl_size);
		ASSERT(!error);
		svc_num = SERVICE_TO_SVCNUM(HANDLE_TO_SERVICE(src_hndl));
		objif = obj_celldatap->obj_callout_table[svc_num];
		if (objif->obj_source_retarget != NULL) {
			error = (*(objif->obj_source_retarget))(
					src_manifest,
					&tkstate[obj]);
			OBJ_TRACE10("kore_retarget_clients", cellid(),
				    "tks_state", tkstate[obj],
				    "\n  *(obj_source_retarget) svc_num", svc_num,
				    "objid", HANDLE_TO_OBJID(src_hndl),
				    "error", error);
			if (error) {
				if (*abt_bmpp == NULL)
					*abt_bmpp = bitmap_zalloc(nobj);
				bset(*abt_bmpp, obj);
			}
			/*
			 * Toss (any further) details for this object 
			 * to get to next handle in manifest.
			 */
			obj_mft_sync(src_manifest);
		} else {
			/*
			 * Toss all details for this object 
			 * to get to next handle in manifest.
			 */
			obj_mft_skip(src_manifest);
		}
	}

	/*
	 * All source servers are now quiesced and we have the token state
	 * for each. For each remote cell with any client object,
	 * request the retarget.
	 * What we need to intuit first is which cells we need to visit.
	 * And for each cell we do visit, which client objects are there.
	 */
	obj_bag_to_message(src_info, &src_hdr, &src_vecp, &src_vec_count);
	obj_bag_to_message(tgt_info, &tgt_hdr, &tgt_vecp, &tgt_vec_count);
	for (cell = 0; cell < MAX_CELLS; cell++) {
		bitmap_t	object_bmp;
		service_t	client_obj_svc;
 
		if (cell == source || cell == target)
			continue;

		object_bmp = bitmap_zalloc(nobj);
		for (obj = 0; obj < nobj; obj++) {
			if (tkstate[obj] && tks_isclient(tkstate[obj], cell)) {
				bset(object_bmp, obj);
			}
		}
		if (!bitmap_isnull(object_bmp, nobj)) {
			/* REFERENCED */
			int	msgerr;

			OBJ_TRACE10("kore_retarget_clients", CELL_LOCAL,
				"invk_kore_client_retarget cell", cell,
				"\n  sinfo", src_info,
				"tinfo", tgt_info,
				"object_bmp", object_bmp);
			obj_retarget_register(&rt, cell);
			SERVICE_MAKE(client_obj_svc, cell, SVC_OBJECT);
			msgerr = invk_kore_client_retarget(
				client_obj_svc, CELL_LOCAL,
				target, nobj,
				&src_hdr,
				(src_rt_invector_t *) src_vecp, src_vec_count,
				&tgt_hdr,
				(tgt_rt_invector_t *) tgt_vecp, tgt_vec_count,
				object_bmp, bitmap_size(nobj),
				rt);
			ASSERT(!msgerr);
		}
		bitmap_free(object_bmp, nobj);
	}
	obj_bag_free_message(src_vecp, src_vec_count);
	obj_bag_free_message(tgt_vecp, tgt_vec_count);

	OBJ_TRACE4("kore_retarget_clients", cellid(), "waiting, rt", rt);
	obj_retarget_wait(&rt);

	/*
	 * Deallocate stuff.
	 */
	obj_retarget_end(&rt);
	kmem_free((void *) tkstate, nobj*sizeof(char *));
}


#if DEBUG && OBJ_TRACE
static void
idbg_obj_trace(__psint_t x)
{
	__psint_t	id;
	int		idx;
	int		count;

	if (x == -1) {
		qprintf("Displaying all entries\n");
		idx = -1;
		count = 0;
	} else if (x < 0) {
		idx = -1;
		count = (int)-x;
		qprintf("Displaying last %d entries\n", count);
	} else {
		qprintf("Displaying entries for cell %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(obj_trace, id, idx, count);
}

void
idbg_manifest(__psint_t x)
{
	obj_manifest_t	*mftp;
	uint_t		step;
	char		*step_name[] = {
				"OBJ_SOURCE_PRELUDE",
				"OBJ_SOURCE_PREPARE",
				"OBJ_TARGET_PREPARE",
				"OBJ_SOURCE_RETARGET",
				"OBJ_CLIENT_RETARGET",
				"OBJ_SOURCE_BAG",
				"OBJ_TARGET_UNBAG",
				"OBJ_SOURCE_END",
				"OBJ_SOURCE_ABORT",
				"OBJ_TARGET_ABORT"
			};
	mftp = (obj_manifest_t *) x;
	step = mftp->step;
	qprintf("Manifest @0x%x: source %d target %d step %s\n",
		mftp, mftp->src, mftp->tgt,
		step <= OBJ_TARGET_ABORT ? step_name[mftp->step] : "??");
	qprintf("   nobj %d, depth %d breadth %d\n",
		mftp->nobj, mftp->posn.depth,  mftp->posn.breadth);
	qprintf("   sinfo bag 0x%x; tinfo bag 0x%x\n",
		 mftp->sinfo, mftp->tinfo);
}
#endif
