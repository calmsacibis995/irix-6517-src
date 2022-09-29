/*
** Copyright 1997,1998 Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
** $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/graphics/ODYSSEY/RCS/odsy_sdram.c,v 1.1 1999/12/01 21:12:59 mnicholson Exp $
*/


#include "sys/types.h"

#include "odsy_internals.h"




/*
** If you need to be able to "see" something in the policy code
** you have to define/prototype it here.  I'd rather not have
** to name everything odsyMMthisAndThat but sometimes we have
** to.  Probably should just do this everywhere :(
*/


#define odsyMMcoresidRange_HwWrSlotId(cores)        (cores)->dep_slot.hw_wr_slot_id
#define odsyMMcoresidRange_HwWrSlot(cores)          (cores)->dep_slot.kv_hw_wr_slot


#define odsyMMcoresidRange_CoresSlotId(cores)    (cores)->gctx_side_id
#define odsyMMcoresidRange_CoresSlot(cores)       (cores)->dep_slot.kv_cores_slot


#define odsyMMobject_AddrCommSlot(mmobj)         (mmobj)->mmgrp->obj_addr_group->addr[(mmobj)->obj_level]



int odsyMMxfrBackingStore(odsy_mm_obj *mmobj,int *async, odsy_dma_marker *last_op_marker, int to_host);

int odsyMMcoresidRangeExistsForObjectContext(odsy_mm_obj *mmobj, odsy_gctx *gctx);
int odsyMMobjectIsInCoresidRange(odsy_mm_obj *mmobj, odsy_mm_coresid_range *cores);



#ifndef ODSY_SDRAM_MLOAD_DEVICE
/*---------------------------------------------------------------------------*/
/*                          PRODUCTION CODE ONLY                             */
/*---------------------------------------------------------------------------*/
#if !defined(_STANDALONE)



static int NewSharegroup(struct gfx_gfx *gfxp, odsy_gctx *gctx,odsy_mm_sg **sgp, odsy_mm *mm);
void DeleteSharegroup(struct gfx_gfx *gfxp, odsy_mm_sg *sg);

void DetachGctxFromSharegroup(odsy_mm_sg *sg, odsy_gctx *gctx);

static odsy_mm_obj *NewObj(odsy_mm_sg *sg, odsy_mm_obj_grp *mmgrp, int level);
static void DeleteObj(odsy_mm_obj *mmobj);

static odsy_mm_obj_grp *NewGroup(odsy_gctx *gctx, odsy_mm_sg *sg, odsy_mm_alloc_arg *);
void DeleteGroup(odsy_mm_obj_grp *mmgrp);

static odsy_mm_coresid_range *NewCoresidRange(odsy_gctx             *gctx,
					      odsy_mm_obj_grp       *mmgrp,
					      int                    high,
					      int                    low,
					      odsy_chunked_set *     grp_side_set,
					      odsy_chunked_set *     gctx_side_set,
					      odsy_mm_query_arg      *k_qryarg);

static void DeleteCoresidRange( odsy_mm_coresid_range *cores);


static int CoresidRangeFinder(void *_cores, void *_cmp_with);
static int CoresidSubRangeFinder(void *_cores, void *_cmp_with);

void ReleaseBackingStore(odsy_mm_obj *mmobj);



static int odsyMMlock(struct gfx_gfx *gfxp,odsy_mm *mm);
static int odsyMMunlock(struct gfx_gfx *gfxp,odsy_mm *mm);


static int SpewObjectToUser(odsy_mm_obj *,
			    int sg_hdl, int grp_hdl, int obj_level,
			    __uint64_t *tbl_addr, 
			    __uint32_t *entry_nr);



#ifdef ODSY_SDRAM_TRACE_KMEM
void *sdram_kmem_alloc (size_t size, int flag, char *f, int l)
{
	void * ret;
	ret=kmem_alloc(size,flag);
	cmn_err(CE_DEBUG,"odsy pmm: ka 0x%x %d line %d\n",ret, size,l); 
	return ret;
}


void sdram_kmem_free(void *x, size_t size, char *f, int l)
{
	cmn_err(CE_DEBUG,"odsy pmm: kf 0x%x %d line %d\n",x, size, l); 
	kmem_free(x, size);

}

#ifdef ODSY_DALLOC
#undef kmem_alloc
#undef kmem_free
#endif

#define kmem_alloc(a,b) sdram_kmem_alloc(a, b, __FILE__, __LINE__)
#define kmem_free(a,b) sdram_kmem_free(a, b, __FILE__, __LINE__)
#endif



/*
** This interface is used by user-land code to query 
** the pipe memory manager system on various things.
** It isn't intended for use by main-line OpenGL
** code.  We'll use it in the lab during bringup
** and perhaps to guide an sdram dumping mechanism.
** If at some point we want to make a gui thingy
** that tells users how sdram is configured we could
** use this ioctl.
*/
int gfP_OdsyMMdebug(struct gfx_gfx   *gfxp,
		    struct rrm_rnode *rnp,
		    void             *u_dbgarg,
		    int              *rvalp)
{
	odsy_data           *bdata;
	odsy_mm_debug_arg    k_dbgarg;
	odsy_mm     *mm;
	odsy_mm_obj *mmobj;
	odsy_mm_sg  *sg;

	*rvalp = 0;

        GFXLOGT(ODSY_gfP_OdsyMMdebug);
	ASSERT(gfxp);
	bdata = ODSY_BDATA_FROM_GFXP(gfxp); 
	ASSERT(bdata); 

	mm = &bdata->mm;

	ODSY_COPYIN_SIMPLE_ARG(u_dbgarg,&k_dbgarg,odsy_mm_debug_arg,
			       "ODSY_gfP_OdsyMMdbg: bad ptr to debug arg");


	switch(k_dbgarg.flag) {
		case ODSY_MM_DEBUG_OBJECT_SNAPSHOT: {
			int        obj_level;
			__uint32_t entry_nr    = 0;
			int        max_entries = k_dbgarg.arg.obj_snapshot.tbl_entries;
		        __uint64_t tbl_addr    = k_dbgarg.arg.obj_snapshot.tbl_ptr + sizeof(__uint32_t);

			/* we skip the first word so when we're done we can say how many records we wrote */

			if (max_entries < ODSY_MM_OBJS_PER_GROUP + 1)  {
				*rvalp = EFAULT;
				return EFAULT;
			}



			/* crawl all known objects and spew them out */
			ODSY_MM_LOCK(gfxp,mm);
			
			/* first the cfifo */
			if (*rvalp = SpewObjectToUser(&mm->cfifo,
						      ODSY_MM_NULLHDL,ODSY_MM_NULLHDL,0,
						      &tbl_addr,&entry_nr)) {
				ODSY_MM_UNLOCK(gfxp,mm);
				return *rvalp;
			}
						      

			/* objects in the screen group */
			for (obj_level=0; obj_level<ODSY_MM_OBJS_PER_GROUP; obj_level++) {
				mmobj = mm->screen_grp.objs[obj_level];
				if (mmobj && mmobj->resid) {
					if (*rvalp = SpewObjectToUser(mmobj,
								      0,ODSY_MM_SCREEN_GROUP,obj_level,
								      &tbl_addr,&entry_nr)){
						ODSY_MM_UNLOCK(gfxp,mm);
						return *rvalp;
					}
				}
			}

			
			/* now walk all the objects */

			OdsyInitCSetCursor(&mm->sg_set);
			while ( (sg = (odsy_mm_sg *)OdsyNextCSetElem(&mm->sg_set)) != 0 ) {
				OdsyInitCSetCursor(&sg->obj_set);
				while ( (mmobj = (odsy_mm_obj *)OdsyNextCSetElem(&sg->obj_set)) != 0 ) {
					if (entry_nr >= max_entries) {
						ODSY_MM_UNLOCK(gfxp,mm);
						cmn_err(CE_WARN,"odsy pmm: not enough debug entries (%d)\n",max_entries);
						*rvalp = EAGAIN;
						return EAGAIN;
					}
					if (*rvalp = SpewObjectToUser(mmobj,
								      mmobj->mmgrp->sg->sg_hdl,
								      mmobj->mmgrp->grp_hdl,
								      mmobj->obj_level,
								      &tbl_addr,&entry_nr)){
						ODSY_MM_UNLOCK(gfxp,mm);
						return *rvalp;
					}
				}
			}

			if (copyout((void*)&entry_nr,(void*)k_dbgarg.arg.obj_snapshot.tbl_ptr,sizeof(entry_nr))) {
				*rvalp = EFAULT;
				cmn_err(CE_WARN,"odsy pmm: debug ioctl copyout fault at entry_nr");
			}

			ODSY_MM_UNLOCK(gfxp,mm);
		}
		
		break;

		default:
			cmn_err(CE_WARN,"odsy pmm: unknown debug ioctl flag\n");
			*rvalp = EINVAL;
			return EINVAL;
	}

	return *rvalp;
}


static int SpewObjectToUser(odsy_mm_obj *mmobj, 
			    int sg_hdl, int grp_hdl, int obj_level,
			    __uint64_t *tbl_addr, 
			    __uint32_t *entry_nr)
{
	odsy_mm_debug_mm_obj k_obj;

	k_obj.base_addr  = mmobj->base_addr;
	k_obj.attr       = mmobj->attr;
	k_obj.sg_hdl     = sg_hdl;
	k_obj.grp_hdl    = grp_hdl;
	k_obj.obj_level  = obj_level;
	k_obj.resid      = mmobj->resid;
	k_obj.pinned     = mmobj->pinned;

	if (copyout((void*)&k_obj,(void*)*tbl_addr,sizeof(k_obj))){
		cmn_err(CE_WARN,"odsy pmm: debug copyout fault in SpewObjectToUser\n");
		return EFAULT;
	}

	(*entry_nr)++;
	(*tbl_addr) += sizeof(k_obj);

	return 0;
}


/* 
** user calls with ODSY_MM_ATTACH
**
** 
** the ctxsw and mm code depend upon this user-
** initiated call to perform the following:
**
**    . signify end of context creation sequence
**      for the given rnid.
**
**    . as such, notice that the context is allowed
**      to switch onto the pipe (w/o hinderance).  
**      we're given a user-land pointer to the valid 
**      sw shadow rgn.
**      we take that versus the default shadow 
**      in use for the ctx during, if any, ctxsw.
**
**    . attach the gfx context to its sharegroup 
**      by mapping in various things to its address
**      space, and/or forwarding along addresses of
**      already mapped regions.
** 
*/

int gfP_OdsyMMattach(struct gfx_gfx   *gfxp,
                     struct rrm_rnode *rnp,
                     void             *u_attarg,
		     int              *rvalp)
{
	odsy_data           *bdata;
	odsy_mm_attach_arg   k_attarg;
	odsy_mm             *mm;
	odsy_chunked_set    *sg_set;
	odsy_ddrnode        *gctx,*share_gctx;
	odsy_mm_sg          *sg;
	int                  sg_id,error,i;


        GFXLOGT(ODSY_gfP_OdsyMMattach);

	ASSERT(gfxp);

	bdata = ODSY_BDATA_FROM_GFXP(gfxp); 
	ASSERT(bdata); 

	ODSY_COPYIN_SIMPLE_ARG(u_attarg,&k_attarg,odsy_mm_attach_arg,
			       "ODSY_gfP_OdsyMMattach: bad ptr to attach arg");


	if (! (gctx  = odsyGctxFromRnid(k_attarg.my_rnid))){
		cmn_err(CE_WARN,"odsy: you don't exist");
		*rvalp = EINVAL;
		return EINVAL;
	}
	/*
	** why not " gctx = ODSY_GCTX_FROM_RNP(rnp); " above?  
        ** because that method implies we have a bound rn.  but _this_ 
	** context has never been bound, because we're still inside 
	** CreateContext.  BINDPROCTORN  doesn't happen until MakeCurrent.
	*/

	ASSERT(gctx);

	/*
	** allow the board manager to double trip mm attach.  
	** we don't have to do anything really because it
	** is already attached.  if we return anything to 
	** user-land (inout args) later we need to go through
	** the motions.
	*/
	if(gctx->uv_sw_shadow){
		if (IS_BRDMGR_GFXP(gfxp)) {
			*rvalp = 0;
			return 0;
		} 
		else {
			cmn_err(CE_WARN,"odsy: you've double tripped MM_ATTACH");
			*rvalp = EINVAL;
			return EINVAL;
		}
	}



	if ( error = odsyPinAndMapGctxSwShadowRgn(gctx,rvalp,(void *)k_attarg.sw_shadow_rgn)){
		*rvalp = error;
		return error;
	}

	/*
	**  attach this guy to the sharegroup.
	*/

	mm     = &(bdata->mm);

	/*
	** initialize this gctx's known group and scratch (i)buffer chunked sets
	*/
	OdsyInitCSet(&gctx->coresid_range_set,4); /* 2^4 chunksize */
	OdsyInitCSet(&gctx->ibfr_set,4); /* 2^4 chunksize */
	OdsyInitCSet(&gctx->coresid_rgn_set,4);
	
	// Gross hack.  We use a few records at the front of the SwWrRgn to communicate
	// the context's view of the active textures at any given point in time.  The 
	// only other place to put this would have been the sw attribute shadow.  But
	// it isn't really hw state so that is the wrong place for it.  Instead of doing
	// pointer arithmetic all the time to jump past this region we just allocate the
	// same number of slots as are consumed by these...

#define HDLS_PER_SW_SLOT (sizeof(odsy_mm_cores_slot)/sizeof(odsy_mm_hdl))

	ASSERT(!(sizeof(odsy_mm_cores_slot) % sizeof(odsy_mm_hdl)));
	//	ASSERT(!(ODSY_MM_MAX_ACTIVE_TEXTURE_GROUPS % HDLS_PER_SW_SLOT ));

	for ( i=0; i <= (ODSY_MM_MAX_ACTIVE_TEXTURE_GROUPS / HDLS_PER_SW_SLOT); i++ ) {
		OdsyAllocSpecificCSetId(&gctx->coresid_range_set, 
					i, 
					(void*)(__psint_t)(i+1) /*dummy, but can't be zero!*/);
	}
	

	ODSY_MM_LOCK(gfxp,mm);

	if(k_attarg.sg_rnid == 0 || k_attarg.sg_rnid == ODSY_MM_NO_SHARE){ 

#ifdef ODSY_DEBUG_SDRAM
		if (IS_BRDMGR_GCTX(gctx)) {
			cmn_err(CE_DEBUG,"odsy: board manager sharegroup being created\n");
		}
#endif

		/* create a new sharegroup and attach the gctx */
		if (*rvalp = NewSharegroup(gfxp,gctx,&sg,mm)){
			ODSY_MM_UNLOCK(gfxp,mm);
			k_attarg.hw_wr_rgn=0; /*didn't get it*/
			ODSY_COPYOUT_SIMPLE_ARG(u_attarg,&k_attarg,odsy_mm_attach_arg,
			       "ODSY_gfP_OdsyMMattach: bad ptr to attach arg");
			return *rvalp;
		}
#ifdef ODSY_DEBUG_SG
printf("Create context no share gctx=%x sg=%x\n",gctx, sg);
#endif
		k_attarg.hw_wr_rgn=(odsy_uaddr)sg->as->uv_hw_wr_rgn;
	} 
	else {

		/* attach the gctx to an already existing sharegroup. */
		if (! (share_gctx=odsyGctxFromRnid(k_attarg.sg_rnid))){
			ODSY_MM_UNLOCK(gfxp,mm);
			cmn_err(CE_WARN,"odsy: invalid share context (rnid) given");
			*rvalp = EINVAL;
			return EINVAL;	
		}

		sg = share_gctx->sg;

		if ( (gctx->ctx_hdl = OdsyAllocCSetId(&sg->ctx_set,(void*)gctx)) == ODSY_MM_NULLHDL ){
			ODSY_MM_UNLOCK(gfxp,mm);
			cmn_err(CE_WARN,"odsy: couldn't allocate space for a new sharegroup context member");
			*rvalp = ENOMEM;
			return ENOMEM;
		}

		sg->ref_cnt++;
		gctx->sg = sg; /*attach context to share group*/
		k_attarg.hw_wr_rgn=(odsy_uaddr)sg->as->uv_hw_wr_rgn;

	}
#ifdef ODSY_DEBUG_SG
	cmn_err(CE_DEBUG,"odsy pmm: Create context gctx=%x sg=%x rc=%d\n",gctx, gctx->sg, gctx->sg->ref_cnt);
#endif

	ODSY_MM_UNLOCK(gfxp,mm);

	/* read decouple swapbuf flag */
	gctx->decouple_swapbuf_from_vr = ((k_attarg.flag & ODSY_MM_ATTACH_DECOUPLE_SWAPBUF) != 0);

	k_attarg.sw_syncid         = gctx->sw_syncid;
	k_attarg.ctx_hdl           = gctx->ctx_hdl;
	k_attarg.glget_rgn         = (odsy_uaddr)(((char *)gctx->uv_hw_shadow) 
						  + (18 * ODSY_CACHELINESZ));
	k_attarg.global_rgn        = (odsy_uaddr)sg->as->uv_global_rgn;
	k_attarg.ucode_rgn         = (odsy_uaddr)((char*) sg->as->uv_global_rgn
						  + sizeof(odsy_mm_global_rgn));
	k_attarg.num_ucode_lines   = bdata->num_ucode_lines;

	ODSY_COPYOUT_SIMPLE_ARG(u_attarg,&k_attarg,odsy_mm_attach_arg,
			       "ODSY_gfP_OdsyMMattach: bad ptr to attach arg");

	*rvalp=0;
	return 0;

}


/*
** this is called once during the death path for the
** context. (should be robust over multiple calls though).
** use this opportunity to unmap/free the sdram mgr specific
** stuff associated with this context.
*/
void odsyMMdetach(struct gfx_gfx *gfxp, odsy_gctx *gctx)
{
	odsy_mm_sg  *sg;
	odsy_mm     *mm;
	int          destroy_sg;

        GFXLOGT(ODSY_MMdetach);

	ASSERT(gctx);
	ASSERT(gfxp); 


	if (sg = gctx->sg) {
		mm = sg->mm;

		ODSY_MM_LOCK(gfxp,mm);
		{
			DetachGctxFromSharegroup(sg,gctx);
			destroy_sg = (--sg->ref_cnt == 0);
#ifdef ODSY_DEBUG_SG
			cmn_err(CE_DEBUG,"odsy sdram: MMDetach gctx=%x sg=%x rc=%d\n",
				gctx, gctx->sg, gctx->sg->ref_cnt);
#endif
			
			if (destroy_sg){
				DeleteSharegroup(gfxp,sg);
			}
		}

		ODSY_MM_UNLOCK(gfxp,mm);
	}
}

/* 
** Create a new object (and group if necessary).  Or realloc.
*/
int gfP_OdsyMMalloc(struct gfx_gfx   *gfxp,
                    struct rrm_rnode *rnp,
                    void             *u_aarg,
                    int              *rvalp)
{

	odsy_mm_alloc_arg  k_aarg;

	odsy_mm_sg        *sg;
	odsy_gctx         *gctx;

	int                rv,is_new_group;
	odsy_mm_obj       *mmobj;
	odsy_mm_obj_grp   *mmgrp;
	int                need_alloc = 1;

        GFXLOGT(ODSY_gfP_OdsyMMalloc);

	ASSERT(gfxp);

	gctx = ODSY_BOUND_GCTX_FROM_GFXP(gfxp);
	ODSY_GFP_NOT_BOUND_CHECK(gctx);

	sg = gctx->sg;
	ODSY_GFP_NO_SG_CHECK(sg);


	ODSY_COPYIN_SIMPLE_ARG(u_aarg,&k_aarg,odsy_mm_alloc_arg,
			       "ODSY_gfP_OdsyMMalloc: bad arg usr ptr");


	mmobj = 0;
	mmgrp = 0;


	if ( k_aarg.obj_level >= ODSY_MM_OBJS_PER_GROUP ) {
		cmn_err(CE_WARN,"odsy: invalid obj level %d given to alloc",k_aarg.obj_level);
		*rvalp = EINVAL;
		return EINVAL;
		}

	ODSY_MM_LOCK(gfxp,sg->mm);
	{
		is_new_group = k_aarg.grp_hdl == ODSY_MM_NULLHDL;

		if (!is_new_group && !(mmgrp = OdsyLookupCSetId(&sg->grp_set,k_aarg.grp_hdl))){
			/* the group the user wants to add to doesn't exist. */
			cmn_err(CE_WARN,"odsy: invalid object group in mm alloc\n");
			*rvalp = EINVAL;
			goto bail_out;

		}

		if (is_new_group  && !(mmgrp=NewGroup(gctx,sg,&k_aarg))){
			cmn_err(CE_WARN,"odsy: could not allocate space for a new mm group (ENOMEM)");
			*rvalp = ENOMEM;
			goto bail_out;
		}


		if (mmobj = mmgrp->objs[k_aarg.obj_level]) {
			/* realloc */
			need_alloc = rv = 0;

			/* delete existing object if it isn't reusable */
			if (mmobj->attr.dx   != k_aarg.attr.dx  ||
			    mmobj->attr.dy   != k_aarg.attr.dy  ||
			    mmobj->attr.dz   != k_aarg.attr.dz  ||
			    mmobj->attr.ox   != k_aarg.attr.ox  ||
			    mmobj->attr.oy   != k_aarg.attr.oy  ||
			    mmobj->attr.oz   != k_aarg.attr.oz  ||
			    mmobj->attr.stride_s   != k_aarg.attr.stride_s  ||
			    mmobj->attr.stride_t   != k_aarg.attr.stride_t  ||
			    mmobj->attr.bpp  != k_aarg.attr.bpp ||
			    mmobj->attr.type != k_aarg.attr.type) {
				need_alloc  = 1;

				/* walk out of the policy/strategy code if we need to. */
				if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_COMMITTED)
					(*mmobj->mmgrp->sg->mm->fncs->decommit)(mmobj);	
				
				if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_RESERVED)
					(*mmobj->mmgrp->sg->mm->fncs->release)(mmobj);	

				DeleteObj(mmobj);
				mmobj = 0;
			} 
		}

		if (need_alloc) {
			/* create a new object */
			if (!(mmobj = NewObj(sg,mmgrp,k_aarg.obj_level))){
				if (is_new_group) {
					odsy_mm_hdl grp_hdl = mmgrp->grp_hdl;
					DeleteGroup(mmgrp);
					OdsyDeallocCSetId(&sg->grp_set,grp_hdl);
				}
				cmn_err(CE_WARN,"odsy: could not allocate a new mm object (ENOMEM)");
				*rvalp = ENOMEM;
				goto bail_out;
				
			}
			rv = *rvalp = odsyMMalloc(gfxp,sg,&k_aarg,mmgrp,mmobj);
		}


		if (rv){
			DeleteObj(mmobj);
			if (is_new_group) {
				odsy_mm_hdl grp_hdl = mmgrp->grp_hdl;
				DeleteGroup(mmgrp);
				OdsyDeallocCSetId(&sg->grp_set,grp_hdl);
			}
		} else {
			k_aarg.grp_hdl=mmobj->grp_hdl;
			k_aarg.obj_hdl=mmobj->obj_hdl;	
		}
	}
	ODSY_MM_UNLOCK(gfxp,sg->mm);

	ODSY_COPYOUT_SIMPLE_ARG(u_aarg,&k_aarg,odsy_mm_alloc_arg,
				"ODSY_gfP_OdsyMMalloc: bad alloc arg usr ptr");
	
        return rv;

bail_out:
	ODSY_MM_UNLOCK(gfxp,sg->mm);
	return *rvalp;
}




int gfP_OdsyMMdealloc(struct gfx_gfx   *gfxp,
                      struct rrm_rnode *rnp,
                      void             *u_arg,
                      int              *rvalp)
{
	odsy_mm_dealloc_arg  k_arg;
	odsy_gctx           *gctx;
	odsy_mm_sg          *sg;
	odsy_mm_obj         *mmobj;
	odsy_mm_obj_grp     *mmgrp;

        GFXLOGT(ODSY_gfP_OdsyMMdealloc);

	ASSERT(gfxp);

	gctx = ODSY_BOUND_GCTX_FROM_GFXP(gfxp);
	ODSY_GFP_NOT_BOUND_CHECK(gctx);

	sg = gctx->sg;
	ODSY_GFP_NO_SG_CHECK(sg);


	ODSY_COPYIN_SIMPLE_ARG(u_arg,&k_arg,odsy_mm_dealloc_arg,
			       "ODSY_gfP_OdsyMMdealloc: bad arg usr ptr");


	mmobj = 0;
	mmgrp = 0;

	ODSY_MM_LOCK(gfxp,sg->mm);
	{
		if (!(mmgrp = (odsy_mm_obj_grp *)OdsyLookupCSetId(&sg->grp_set,k_arg.grp_hdl))) {
			ODSY_MM_UNLOCK(gfxp,sg->mm);
			*rvalp = EINVAL;
			return EINVAL;
		}

		if ( k_arg.flag == ODSY_MM_DEALLOC_GROUP ) {
			odsy_mm_hdl grp_hdl = mmgrp->grp_hdl;
			if ( mmgrp->is_scratch ) 
				OdsyDeallocCSetId(&gctx->ibfr_set,mmgrp->ibfr_set_hdl);
			DeleteGroup(mmgrp);
			OdsyDeallocCSetId(&sg->grp_set,grp_hdl);
		}
		else {
			if ( k_arg.obj_level >= ODSY_MM_OBJS_PER_GROUP ) {
				cmn_err(CE_WARN,"odsy: invalid obj level %d given to dealloc",k_arg.obj_level);
				*rvalp = EINVAL;
				return EINVAL;
			}
			
			if(mmobj = mmgrp->objs[k_arg.obj_level]) {
				if (mmgrp->nr_objs == 1) {
					odsy_mm_hdl grp_hdl = mmgrp->grp_hdl;					
					k_arg.flag = ODSY_MM_DEALLOC_GROUP;
					/* remove the entire group if this is the last obj */
					if ( mmgrp->is_scratch ) 
						OdsyDeallocCSetId(&gctx->ibfr_set,mmgrp->ibfr_set_hdl);
					DeleteGroup(mmgrp);
					OdsyDeallocCSetId(&sg->grp_set,grp_hdl);
				}
				else {
					
					/* walk out of the policy/strategy code if we need to. */
					if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_COMMITTED)
						(*mmobj->mmgrp->sg->mm->fncs->decommit)(mmobj);	
					
					
					if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_RESERVED)
						(*mmobj->mmgrp->sg->mm->fncs->release)(mmobj);	
					
					DeleteObj(mmobj);
				}
			}
		}
	}
	ODSY_MM_UNLOCK(gfxp,sg->mm);

	if (k_arg.flag == ODSY_MM_DEALLOC_GROUP) {
		/* send out the flag so the client knows the group is toast */
		ODSY_COPYOUT_SIMPLE_ARG(u_arg,&k_arg,odsy_mm_dealloc_arg,"odsy: bad mm dealloc arg usr ptr");
	}

        return 0;
}




/*
** force residency for the coresid range defined by grp_hdl[lo,hi] given.
*/
int gfP_OdsyMMmakeResident(struct gfx_gfx   *gfxp,
			   struct rrm_rnode *rnp,
			   void             *u_arg,
			   int              *rvalp)
{
	odsy_gctx                 *gctx;
	odsy_mm_sg                *sg;
	odsy_mm_obj_grp           *mmgrp;
	odsy_mm_make_resident_arg  k_mkresarg;
	int got_sg = 0;

        GFXLOGT(ODSY_gfP_OdsyMMmakeResident);

	gctx = ODSY_BOUND_GCTX_FROM_GFXP(gfxp);
	ODSY_GFP_NOT_BOUND_CHECK(gctx);

	sg = gctx->sg;
	ODSY_GFP_NO_SG_CHECK(sg);


	ODSY_COPYIN_SIMPLE_ARG(u_arg,&k_mkresarg,odsy_mm_make_resident_arg,
			       "ODSY_gfP_OdsyMMmakeResident: bad arg ptr");

	/*
	** instead of creating yet another hdl space for coresid ranges, 
	** just look it up.  this means we have code that looks very much
	** like that in MM_QUERY, but without the "add if it doesn't exist" 
	** part.  if the range is invalid or the group is bogus, just bail.
	*/

	if ((k_mkresarg.low >= ODSY_MM_OBJS_PER_GROUP)  ||
	    (k_mkresarg.high >= ODSY_MM_OBJS_PER_GROUP) ||
	    (k_mkresarg.high < k_mkresarg.low)) {
		cmn_err(CE_WARN,"odsy: invalid mm low and/or high given to make resident");
		*rvalp = EINVAL;
		return EINVAL;
	}

	*rvalp = 0;

	ODSY_MM_LOCK(gfxp,sg->mm); /* should probaby do a CheckValidFault here... */
	{
		odsy_chunked_set      *setset;
		odsy_mm_coresid_range *cores;
		int                    cmpwith[3]; 
		
		if (!(mmgrp=OdsyLookupCSetId(&sg->grp_set,k_mkresarg.grp_hdl))) {
			cmn_err(CE_WARN,"odsy: invalid mm grp_hdl given to make resident");
			*rvalp = EINVAL;
			goto bail_out;
		}

		ASSERT(mmgrp->grp_hdl == k_mkresarg.grp_hdl);

		if (!(setset = (odsy_chunked_set*) OdsyLookupCSetId(&mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl))) {
			cmn_err(CE_WARN,"odsy: failed lookup of setset in make resident: ctx_hdl: %d ",gctx->ctx_hdl);
			*rvalp = EINVAL;
			goto bail_out;
		}


		cmpwith[0]=k_mkresarg.high; cmpwith[1]=k_mkresarg.low; cmpwith[2] = k_mkresarg.grp_hdl;

		if (! (cores=(odsy_mm_coresid_range*)OdsyFindMatchingCSetElem(setset,(void*)cmpwith,CoresidRangeFinder))) {
			cmn_err(CE_WARN,"odsy pmm: failed lookup of coresid range in make resident");
			*rvalp = EINVAL;
			goto bail_out;
		}

		/*
		** sanity check.  the user-mode code is supposed to have set sw_val != hw_val before calling...
		*/
		if (odsyMMcoresidRange_HwWrSlot(cores)->val == odsyMMcoresidRange_CoresSlot(cores)->val) {
			cmn_err(CE_WARN,"odsy pmm: sw_val == hw_val in make resident");
			*rvalp = EINVAL;
			goto bail_out;
		}



		if ((*sg->mm->fncs->commit)(cores)) {
			cmn_err(CE_WARN,"odsy pmm: commit failure");
			*rvalp = EINVAL;
			goto bail_out;
		}


		ASSERT(odsyMMcoresidRange_CoresSlot(cores)->resid == 1);
		ASSERT(odsyMMcoresidRange_CoresSlot(cores)->busy  == 0);



	}
bail_out:
	ODSY_MM_UNLOCK(gfxp,sg->mm);
	return *rvalp;

}



static int MapKernelRegionIntoUserSpace(odsy_addr_rgn *rgn,int perms) 
{
	
	v_mapattr_t vmap_attr = 
	{
		(uvaddr_t)(ulong)0,
		(uvaddr_t)(ulong)0,
		VDD_COHRNT_EXLWR,
		VDD_UNCACHED_DEFAULT, /* we're cached, this doesn't matter*/
		0
	};

	vmap_attr.v_end = (uvaddr_t)rgn->size;
	vmap_attr.v_prot = perms;

	ASSERT(rgn->uv_ptr == 0);

	return odsyMapPages (0/*vm system determines address*/,
			     &rgn->uv_ptr,
			     rgn->kv_ptr,
			     (int)rgn->size,
			     &vmap_attr, 1,
			     &rgn->ddv);


}



static int PinAndMapUserRegionIntoKernelSpace(odsy_addr_rgn *rgn)
{
	unsigned int  mask_perm = GOOD_PGBITS | pte_cachebits();
	int error;
	
	if (error = useracc(rgn->uv_ptr, rgn->size, B_READ|B_PHYS|B_ASYNC,0)){
		cmn_err(CE_WARN,"odsy: failed to lock down user memory");
		return error;
	}

	if (!(rgn->kv_ptr = maputokv((caddr_t)rgn->uv_ptr,rgn->size, mask_perm))) {
		unuseracc(rgn->uv_ptr, rgn->size, B_READ | B_PHYS | B_ASYNC);
		cmn_err(CE_WARN,"odsy: failed to map user memory into kernel");
		return EINVAL;
	}

	return 0;

}




//
// We no longer need this ioctl.
//
int gfP_OdsyMMnotify(struct gfx_gfx *gfxp,
		    struct rrm_rnode     *rnp,
		    void                 *u_arg,
		    int                  *rvalp)
{

	*rvalp = EINVAL;
	return *rvalp;
}



static int CoresidRangeFinder(void *_cores, void *_cmp_with) 
{
	int *cmp_with = (int*)_cmp_with;
	odsy_mm_coresid_range *cores = (odsy_mm_coresid_range*)_cores;
	if ((cores->mmgrp->grp_hdl == cmp_with[2]) && 
	    (cores->high == cmp_with[0] && cores->low == cmp_with[1])) {
		return 0;
	}
	else return 1;

}

/* if the given _cores brackets the comparator then match found */
static int CoresidSubRangeFinder(void *_cores, void *_cmp_with) 
{
	int *cmp_with = (int*)_cmp_with;
	odsy_mm_coresid_range *cores = (odsy_mm_coresid_range*)_cores;
	if ((cores->mmgrp->grp_hdl == cmp_with[2]) && 
	    (cores->high >= cmp_with[0] && cores->low <= cmp_with[1])) {
		return 0;
	}
	else return 1;

}


/*
** For the mmobj,gctx pair is there a coresid range that covers it?
*/
int odsyMMcoresidRangeExistsForObjectContext(odsy_mm_obj *mmobj, odsy_gctx *gctx)
{
	odsy_mm_obj_grp * mmgrp = mmobj->mmgrp;
	odsy_chunked_set *setset = (odsy_chunked_set*) 
		OdsyLookupCSetId(&mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl);
	int cmpwith[3]; 

	if (!setset) return 0; /* none whatsoever */

	cmpwith[0]=mmobj->obj_level; cmpwith[1]=mmobj->obj_level; cmpwith[2] = mmgrp->grp_hdl;

	if (OdsyFindMatchingCSetElem(setset,(void*)cmpwith,CoresidSubRangeFinder)){
		return 1;
	}

	return 0;
}


/*
** Is this object a member of the group implied by this coresid range?
*/
int odsyMMobjectIsInCoresidRange(odsy_mm_obj *mmobj, odsy_mm_coresid_range *cores)
{
	int obj_level;
	for(obj_level=0; obj_level<ODSY_MM_OBJS_PER_GROUP; obj_level++){
		if (mmobj == cores->mmgrp->objs[obj_level]) return 1;
	}
	return 0;
}


//
// Users call this ioctl to query status of object groups and
// lock ranges.  If necessary we allocate coresid range tracking
// memory here and map it into the client's address space.
//
int gfP_OdsyMMquery(struct gfx_gfx   *gfxp,
		    struct rrm_rnode *rnp,
		    void             *u_qryarg,
		    int              *rvalp)
{

	odsy_gctx               *gctx;
	odsy_mm_sg              *sg;
	odsy_mm_hdl              grp_hdl;
	int                      low;
	int                      high;
	odsy_mm_query_arg        k_qryarg;
	int                      error;
	odsy_mm_obj_grp         *mmgrp;
	odsy_mm_obj             *mmobj;
	odsy_mm_coresid_range   *cores;


        GFXLOGT(ODSY_gfP_OdsyMMqueryNotify);
	
	gctx = ODSY_BOUND_GCTX_FROM_GFXP(gfxp);
	ODSY_GFP_NOT_BOUND_CHECK(gctx);

	sg=gctx->sg;
	ODSY_GFP_NO_SG_CHECK(sg);

	*rvalp = 0;

	ODSY_COPYIN_SIMPLE_ARG(u_qryarg,&k_qryarg,odsy_mm_query_arg,
			       "odsy: bad mm query/notify arg usr ptr");


	grp_hdl = k_qryarg.grp_hdl;
	low     = k_qryarg.low;
	high    = k_qryarg.high;
	
	// This ioctl might change later, but for now...

	ASSERT(k_qryarg.flag == ODSY_MM_QUERY_LOCKRANGE);

#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: MMquery CoresidRange 0x%x,0x%x[%d,%d]\n",
		sg->sg_hdl,grp_hdl,k_qryarg.low, k_qryarg.high);
#endif 

	if ((low  < 0) || (low >= ODSY_MM_OBJS_PER_GROUP)  ||
	    (high < 0) || (high >= ODSY_MM_OBJS_PER_GROUP) ||
	    (high < low)) {
		cmn_err(CE_WARN,"odsy: invalid mm low and/or high given to query");
		*rvalp = EINVAL;
		return EINVAL;
	}

	ODSY_MM_LOCK(gfxp,sg->mm);
	{

		if (!(mmgrp=OdsyLookupCSetId(&sg->grp_set,grp_hdl))) {
			ODSY_MM_UNLOCK(gfxp,sg->mm);
			cmn_err(CE_WARN,"odsy: invalid mm grp_hdl given to query");
			*rvalp = EINVAL;
			return EINVAL;
		}

		ASSERT(mmgrp->grp_hdl == grp_hdl);

		k_qryarg.obj_addr_slot = mmgrp->obj_addr_slot_id;
		k_qryarg.is_scratch    = mmgrp->is_scratch;
		

		/* 
		** see if we already have a coresid range (for this gctx,mmgrp pair) 
		** that covers the range given.  if we don't find one, create it and attach it.
		*/
		{
			odsy_chunked_set *setset = (odsy_chunked_set*) 
				OdsyLookupCSetId(&mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl);
			int cmpwith[3]; 
			
			if (!setset) {
				if (!(setset = (odsy_chunked_set*)kmem_alloc(sizeof(odsy_chunked_set),KM_SLEEP))) {
					ODSY_MM_UNLOCK(gfxp,sg->mm);
					*rvalp = ENOMEM;
					return ENOMEM;
				}

				OdsyInitCSet(setset,2/*^2*/);

				if (OdsyAllocSpecificCSetId(&mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl,setset)!=gctx->ctx_hdl){
					cmn_err(CE_DEBUG,"odsy pmm: failed to allocate specific id for coresid ranges by gctx\n");
					ODSY_MM_UNLOCK(gfxp,sg->mm);
					*rvalp = ENOMEM;
					return ENOMEM;
				}
			}

			cmpwith[0]=high; cmpwith[1]=low; cmpwith[2] = grp_hdl;
			
			if (! (cores=(odsy_mm_coresid_range*)OdsyFindMatchingCSetElem(setset,
										      (void*)cmpwith,
										      CoresidRangeFinder))) {
#ifdef ODSY_DEBUG_SDRAM
				cmn_err(CE_DEBUG,"odsy pmm: creating new CoresidRange...\n");
#endif
				cores = NewCoresidRange(gctx,
							mmgrp,
							high,low,
							setset,&gctx->coresid_range_set,
							&k_qryarg);
				
			}
#ifdef ODSY_DEBUG_SDRAM
			else {
				cmn_err(CE_DEBUG,"odsy pmm: found existing CoresidRange 0x%x\n",cores);
			}
#endif
		}
		
		if (cores == 0) {
			ODSY_MM_UNLOCK(gfxp,sg->mm);
			cmn_err(CE_WARN,"odsy: no mem ODSY_MM_QUERY_LOCKRANGE ");
			*rvalp = ENOMEM;
			return ENOMEM;
		}
		
		
		
		k_qryarg.cores_slot = odsyMMcoresidRange_CoresSlotId(cores);
		k_qryarg.hw_wr_slot = odsyMMcoresidRange_HwWrSlotId(cores);
				
	}
	ODSY_MM_UNLOCK(gfxp,sg->mm);


	ODSY_COPYOUT_SIMPLE_ARG(u_qryarg,&k_qryarg,odsy_mm_query_arg,
				"ODSY_gfP_OdsyMMquery: bad pointer to query arg");

	*rvalp = 0;
        return 0;

}


/*
** works relative to the currently bound context.
**
*/
int gfP_OdsyMMxfr(struct gfx_gfx   *gfxp,
                  struct rrm_rnode *rnp,
                  void             *u_xfrarg,
                  int              *rvalp)
{
	odsy_data       *bdata;
	odsy_mm_xfr_arg  k_xfrarg;
	odsy_mm_hdl      grp_hdl;
	unsigned short   obj_level;
	odsy_gctx       *gctx;
	odsy_mm_sg      *sg;
	odsy_mm_obj     *mmobj;
	odsy_mm_obj_grp *mmgrp;
	odsy_dma_rgn    *dmargn;
	odsy_draw_hdl   *hdl;
	odsy_drawable   *draw;
    	unsigned char    srcBt;
    	unsigned char    dstIsPixPipe;
	unsigned int     minimum_host_bfr_len;
	int              invalidate_bs = 0;
	__uint64_t       xfr_num;
	int              dma_DX,dma_DY;
	int              rc = 0;

        GFXLOGT(ODSY_gfP_OdsyMMxfr);

	gctx = ODSY_BOUND_GCTX_FROM_GFXP(gfxp);
	ODSY_GFP_NOT_BOUND_CHECK(gctx);

	sg  = gctx->sg;
	ODSY_GFP_NO_SG_CHECK(sg);

	ODSY_COPYIN_SIMPLE_ARG(u_xfrarg,&k_xfrarg,odsy_mm_xfr_arg,
			       "ODSY_gfP_OdsyMMxfr: bad ptr to xfr arg");

	mmobj = 0;

	grp_hdl   = k_xfrarg.grp_hdl;
	obj_level = k_xfrarg.obj_level;  

	if (grp_hdl == ODSY_MM_NULLHDL){
		cmn_err(CE_NOTE,"odsy: null group hdl given to odsy mm xfr\n");
		*rvalp = 0;
		return 0;
	}

	bdata = ODSY_BDATA_FROM_GCTX(gctx);

	// Make sure we ctxsw off any other gctx!
	if (RRM_CheckValidFault(gfxp) < 0 ) {
		*rvalp = EFAULT;
		return EFAULT;
	}
	ODSY_MM_LOCK(gfxp,sg->mm);

	switch(grp_hdl) {
		case ODSY_MM_SCREEN_GROUP: /* used only by Xserver so far. */
			ASSERT(IS_BRDMGR_GCTX(gctx)); /* just a consistency check */
			ASSERT(IS_BRDMGR_GFXP(gfxp)); /* ditto ^ 2                */
			mmgrp = &(bdata->mm.screen_grp);
			mmobj = mmgrp->objs[obj_level];
			mmobj->resid = 1; /* just force it */
			break;

		case ODSY_MM_CURRENT_DRAWABLE_GROUP: /* used by OpenGL clients */
		case ODSY_MM_CURRENT_READABLE_GROUP:
			hdl = (grp_hdl == ODSY_MM_CURRENT_DRAWABLE_GROUP) ? &gctx->cur_draw : &gctx->cur_read;
			if (!(hdl->flags & ODSY_DRAW_HDL_BOUND)) {
				cmn_err(CE_NOTE,"odsy: context attempted to xfr to non-existent current read/draw");
				rc = EINVAL;
				goto bail_out_unlock;
			}
			
			if (hdl->flags & ODSY_DRAW_HDL_HASHED) { /* pbuffer */
				mraccess(&bdata->drawable_lock);
				draw = (odsy_drawable *)odsyHashLookup(bdata->drawables, 
								       ODSY_DRAW_HASH(hdl->id), 
								       hdl->id);
				mrunlock(&bdata->drawable_lock);
				/* what keeps this thing from disappearing now? */

				if (!draw || !(draw->mmgrp)) {
					cmn_err(CE_WARN,"odsy: no pbuffer draw,readable/mmgrp bound to gctx in mm xfr");
					rc = EINVAL;
					goto bail_out_unlock;
				}
				mmgrp = draw->mmgrp;
				mmobj = mmgrp->objs[obj_level];
			}
			else { /* onscreen */
				mmgrp = &(ODSY_BDATA_FROM_GCTX(gctx)->mm.screen_grp);
				mmobj = mmgrp->objs[obj_level];
			}

			break;

		default: /* lookup. not the current read/draw.  used by OpenGL clients */

			if ( obj_level >= ODSY_MM_OBJS_PER_GROUP ){
				cmn_err(CE_WARN,"odsy: bad group,level:(%d,%d) given to MM_XFR",grp_hdl,obj_level);
				rc = EINVAL;
				goto bail_out_unlock;
			}

			if (!(mmgrp=OdsyLookupCSetId(&sg->grp_set,grp_hdl))){
				cmn_err(CE_WARN,"odsy: invalid mm grp_hdl given to MM_XFR");
				rc = EINVAL;
				goto bail_out_unlock;
			}
			if ((mmobj=mmgrp->objs[obj_level]) == NULL) {
				cmn_err(CE_WARN,"odsy: unused level given to MM_XFR");
				rc = EINVAL;
				goto bail_out_unlock;
			}


	}


	if (!mmobj) {
		cmn_err(CE_WARN,"odsy pmm (mmxfr): no mm object associated with transfer");
		rc = EINVAL;
		goto bail_out_unlock;
			
	}

	/*
	** sanity check the dma parameters
	*/
	if ((grp_hdl != ODSY_MM_CURRENT_DRAWABLE_GROUP) && (grp_hdl != ODSY_MM_CURRENT_READABLE_GROUP)) {
		/* make sure the object is committed (resident) */
		if (! mmobj->resid ) {
			cmn_err(CE_WARN,"odsy pmm (mmxfr): object not resident 0x%x,0x%x[%d] before dma",
				mmobj->mmgrp->sg->sg_hdl,mmobj->mmgrp->grp_hdl,mmobj->obj_level);
			rc = EINVAL;
			goto bail_out_unlock;
		}
		if (k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin == 0) 
			k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin = mmobj->base_addr;

		k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin=
			k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin + k_xfrarg.sdram_offset;

		/*should check for 3D texture being in bounds*/
#ifdef ODSY_DEBUG_SDRAM
		cmn_err(CE_DEBUG,"odsy pmm: MMxfr grp:%d level:%d mmobj:0x%x origin:0x%x use:0x%x\n", 
			grp_hdl,obj_level, mmobj, mmobj->base_addr, k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin);
#endif
	}
	else {
		/* user wants to dma to current readable or drawable */
		int rel_origin, rel_offset, check_origin,check_offset, rel_cfg, check_cfg;
		rel_origin = k_xfrarg.dma_param.gfx_origin.gfxRelAddr;
		rel_offset = EXTRACT_BIT(k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin,28); /* is offset relative */
		if (!rel_origin || !rel_offset){
			cmn_err(CE_NOTE,"odsy: dma to/from current read/draw doesn't have rel:rel setup properly (%d:%d)\n",
				rel_origin,rel_offset);
			rc = EINVAL;
			goto bail_out_unlock;
		}
		
		/* make sure they used the right dma registers for what they're after */
		rel_origin = k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin & 0x7;      /* dma register for origin     */
		rel_offset = (k_xfrarg.dma_param.gfx_origin.gfxBufferOrigin>>3) & 0x7; /* dma register for offset     */
		rel_cfg    = k_xfrarg.dma_param.gfx_field_format.gfxFieldSelect;       /* dma register for front/back */

		if (grp_hdl == ODSY_MM_CURRENT_DRAWABLE_GROUP) {
			if (obj_level < ODSY_MM_ACCUM_BASE_LEVEL) {
				check_origin = ODSY_MM_XFR_CURRENT_DRAWABLE_ORIGIN;
			}
			else {
				check_origin = ODSY_MM_XFR_CURRENT_DRAWABLE_ACCUM_B0_ORIGIN + (obj_level - ODSY_MM_ACCUM_BASE_LEVEL);
			}

			check_offset = ODSY_MM_XFR_CURRENT_DRAWABLE_OFFSET;
			check_cfg    = ODSY_MM_XFR_CURRENT_DRAWABLE_CONFIG;
		}
		else {
			check_origin = ODSY_MM_XFR_CURRENT_READABLE_ORIGIN; /* can't refer to accum in readable */
			check_offset = ODSY_MM_XFR_CURRENT_READABLE_OFFSET;
			check_cfg    = ODSY_MM_XFR_CURRENT_READABLE_CONFIG;
		}
		if ((rel_offset != check_offset) || (rel_cfg != check_cfg) || (rel_origin != check_origin) ){
			cmn_err(CE_NOTE,"odsy: dma to/from current read/draw doesn't use the right "
			"dma registers %d:%d vs %d:%d (correct)\n",rel_origin,rel_offset,check_origin,check_offset);
			rc = EINVAL;
			goto bail_out_unlock;
		}
		
		/* okay, looks good... */
	}
		
    	dstIsPixPipe = (ODSY_DMA_XFR_MODE_TOPIPE(k_xfrarg.dma_param.mode_host_format.transferMode)) ? 1 : 0;
    	srcBt = (ODSY_DMA_XFR_MODE_BT(k_xfrarg.dma_param.mode_host_format.transferMode)) ? 1 : 0;


	// gfxWinYSize or gfxWinXSize == 0 maps to 64K.
	if ( k_xfrarg.dma_param.gfx_win_size.gfxWinYSize == 0 ) 
		dma_DY = 64*1024;
	else 
		dma_DY = k_xfrarg.dma_param.gfx_win_size.gfxWinYSize;
#if 0
	if ( k_xfrarg.dma_param.gfx_win_size.gfxWinXSize == 0 ) 
		dma_DX = 64*1024;
	else 
		dma_DX = k_xfrarg.dma_param.gfx_win_size.gfxWinXSize;
#endif
	

	minimum_host_bfr_len = 
		 dma_DY * k_xfrarg.dma_param.host_scan_line.hostScanLine +
		(dma_DY -1) * k_xfrarg.dma_param.host_stride.hostStride;
#if defined(ODSY_DEBUG_DMA)

		cmn_err(CE_DEBUG,"odsy: dma: host_bfr_len=%d  minimum_len=%d for ysize=%d scanline=%d skip=%d\n",
			k_xfrarg.host_bfr_len,
			minimum_host_bfr_len,
			dma_DY,
			k_xfrarg.dma_param.host_scan_line.hostScanLine,
			k_xfrarg.dma_param.host_stride.hostStride);
#endif

	if ( k_xfrarg.host_bfr_len < minimum_host_bfr_len) {
		cmn_err(CE_NOTE,"odsy: dma invalid parameters: host_bfr_len=%d < minimum_len=%d for ysize=%d scanline=%d skip=%d\n",
			k_xfrarg.host_bfr_len,
			minimum_host_bfr_len,
			k_xfrarg.dma_param.gfx_win_size.gfxWinYSize,
			k_xfrarg.dma_param.host_scan_line.hostScanLine,
			k_xfrarg.dma_param.host_stride.hostStride);
		rc = EINVAL;
		goto bail_out_unlock;
	}

	k_xfrarg.dma_param.mode_host_format.transferMode  
		= ODSY_DMA_XFR_MODE(0,/*generate intr*/
				    srcBt,/* src is bottom to top*/
				    1,/*data is dup'd, or single buzz (always dup) ?*/
				    dstIsPixPipe,/*destination is pxpipe ?*/
				    0,/*is this gbr ?*/
				    1,/*is coherent ?*/
				    !(k_xfrarg.flag & ODSY_MM_XFR_TO_GFX));/* to host? */


	// Actually start the DMA.  If the user has asked for a blocking dma we will
	// wait for it to complete here.  Otherwise we go right back to user-land.
	{
		int             async;
		odsy_dma_marker op_marker;

		async = k_xfrarg.flag & ODSY_MM_XFR_ASYNCHRONOUS;

		if ( rc = odsyDMAOp(gfxp,gctx,bdata,
				    (void*)k_xfrarg.host_bfrp,    /* user virt base ptr          */
				    (size_t)k_xfrarg.host_bfr_len,/* length in bytes             */				    
				    &async,
				    &k_xfrarg.dma_param,
				    &op_marker) ) {
			cmn_err(CE_WARN,"odsy pmm: mm xfr dma failed");
			goto bail_out_unlock;
		}

		// Allow ourselves to lose graphics...  otherwise we'd
		// hold off ctxsw until this (potentially blocking) dma finishes!

		ODSY_MM_UNLOCK(gfxp,sg->mm);

		if ( k_xfrarg.flag & ODSY_MM_XFR_BLOCKING ) {
			if (rc = odsyDMAWait(bdata,gfxp,gctx,ODSY_DMA_WAIT_OP,&op_marker)) {
				cmn_err(CE_WARN,"odsy pmm: failed blocking wait on marker 0x%x",op_marker);
				// rc will be returned after clean-up below.
				// System hangs if we don't return xfrarg here?
			}
		}

		// If we blocked above the marker is turned into the invalid number.

		k_xfrarg.num = op_marker;
	}

	ODSY_COPYOUT_SIMPLE_ARG(u_xfrarg,&k_xfrarg,odsy_mm_xfr_arg,
				"ODSY_gfP_OdsyMMxfr: bad ptr to xfr arg");

	*rvalp = rc;
        return rc;
	
bail_out_unlock:
	ODSY_MM_UNLOCK(gfxp,sg->mm);
	*rvalp = rc;
	return rc;
}



/*------------------------------------------------------------ ioctl entry / internal entry demarcation */


/*
** LockMM and UnlockMM
**   Basically wrappers around grabbing/releasing gfxlock.
**   Left as actual procedures so we can perform instrumenting, later.
**   That's also why the mm pointer is there...
*/
/*ARGSUSED*/
int odsyMMlock(struct gfx_gfx *gfxp, odsy_mm *mm)
{

	if (!OWNS_GFXSEMA(gfxp)) {
		GET_GFXSEMA(gfxp);
	} 

	ASSERT(OWNS_GFXSEMA(gfxp));

	INCR_GFXLOCK(gfxp);

	return 0;
}
/*ARGSUSED*/
int odsyMMunlock(struct gfx_gfx *gfxp, odsy_mm *mm)
{
	ASSERT(OWNS_GFXSEMA(gfxp));

	DECR_GFXLOCK(gfxp);

	return 0;
}
#endif  /* !_STANDALONE */



/* 
** odsyMMinit 
**
** called _before_ we register with the device independent layer.
** setup enough so that writing to the cfifo makes sense.  also
** initialize all the structs for the sdram mgr.
**
**
** we implement a static allocation ordering
** scheme for the cmd fifo, frame and associated wid 
** bfrs:  the cfifo base is always zero and we add
** the frame and wid buffers when X asks for (and sets) 
** them...  the code to allocate and setup the frame 
** buffer and wid/ovly buffer is gfP_OdsyBRDMGRconfigFb.
**

        0                               end of sdram
        | cbfr | fbfr | wbfr | czbfr | abfr(s) |  nbfr   |
                                                   /
                                                  /
                                                 /
                                                /
                                               /
                                        pix/tex/pbuffers

*/
void odsyMMinit(odsy_hw *hw, odsy_mm *mm)
{
	int i;

        GFXLOGT(ODSY_odsyMMinit);

	// Assume mm is zeroed at boot.  On re-init we want to hold on
	// to some pointers (global region).  Previously we bzero()ed.

	mm->bdata = ODSY_BDATA_FROM_BASE(hw);

	/*
	** default policy/strategy functions
	*/
#if !defined( _STANDALONE )
	mm->fncs = &mm->_fncs;
	mm->_fncs.reserve         = odsyMMlameReserve;
	mm->_fncs.commit          = odsyMMlameCommit;
	mm->_fncs.validate_commit = odsyMMlameValidateCommit;
	mm->_fncs.decommit        = odsyMMlameDecommit;
	mm->_fncs.release         = odsyMMlameRelease;
	mm->hook           = 0;
#else
	mm->fncs = &mm->_fncs;
	mm->_fncs.reserve         = 0;
	mm->_fncs.commit          = 0;
	mm->_fncs.validate_commit = 0;
	mm->_fncs.decommit        = 0;
	mm->_fncs.release         = 0;
	mm->hook           = 0;
#endif

	/*
	** setup the screen mmgrp 
	*/
	mm->screen_grp.sg = 0; /* hack */
	mm->screen_grp.grp_hdl = ODSY_MM_SCREEN_GROUP;
	for (i=0;i<ODSY_MM_OBJS_PER_GROUP;i++) mm->screen_grp.objs[i] = 0;
	mm->screen_grp.objs[ODSY_MM_MAIN_LEVEL]    = &mm->fbfr;
	mm->screen_grp.objs[ODSY_MM_OVLYWID_LEVEL] = &mm->wbfr;
	mm->screen_grp.objs[ODSY_MM_COARSEZ_LEVEL] = &mm->czbfr;
	for (i=0;i<ODSY_MM_ACCUM_MAX_LEVELS;i++) {
		mm->screen_grp.objs[ODSY_MM_ACCUM_BASE_LEVEL+i] = &mm->abfr[i];
	}
	mm->screen_grp.nr_objs = 2; /* don't know if this matters*/
	mm->screen_grp.obj_addr_slot_id = -1; /* n/a */
	mm->screen_grp.obj_addr_group = 0;    /* ditto */
	for (i=0;i<ODSY_MM_OBJS_PER_GROUP;i++) {
		odsy_mm_obj *mmobj;
		if (mmobj = mm->screen_grp.objs[i]) {
			mmobj->sg        = 0; /* hack, should re-route to board manager's later */
			mmobj->mmgrp     = &mm->screen_grp;
			mmobj->grp_hdl   = ODSY_MM_SCREEN_GROUP;
			mmobj->obj_level = i;
			mmobj->base_addr = 0;
			mmobj->resid     = 1;
			mmobj->bs.valid  = 0;
			mmobj->bs.mem    = 0;
			mmobj->bs.last_dma_marker = ODSY_XFR_NUM_INVAL;
			mmobj->fncs_state = ODSY_MM_FNCS_OBJ_STATE_NONE; /* not tracked by policy code */
			mmobj->hook       = 0;/* not tracked by policy code */

		}
	}
	/* we don't know much about these yet, other than their type */
	mm->fbfr.attr.type = ODSY_MM_FBFR;
	mm->wbfr.attr.type = ODSY_MM_WBFR;
	mm->czbfr.attr.type = ODSY_MM_ZBFR;
	for (i=0;i<ODSY_MM_ACCUM_MAX_LEVELS;i++) {
		mm->abfr[i].attr.type = ODSY_MM_ABFR;
	}

	/* this means that the mm_objs for each of:
	** cfifo,fbfr,wbfr and root allocable space (nbfr)
	** are all zeroed out.
	*/
	mm->cfifo.grp_hdl        = ODSY_MM_NULLHDL;
	mm->cfifo.obj_level      = 0;
	mm->cfifo.obj_hdl        = ODSY_MM_NULLHDL;
	mm->cfifo.base_addr      = 0;
	mm->cfifo.sg             = 0;
	mm->cfifo.resid          = 1;
	mm->cfifo.pinned         = 1;

	mm->cfifo.attr.type      = ODSY_MM_CBFR;
	mm->cfifo.attr.bpp       = 2;
	mm->cfifo.attr.dx        = 16 * ODSY_CFIFO_NR_2BPP_TILES;
	mm->cfifo.attr.dy        = 16;
	mm->cfifo.attr.dz        = 1;
	mm->cfifo.attr.ox        = 0;
	mm->cfifo.attr.oy        = 0;
	mm->cfifo.attr.oz        = 0;
	mm->cfifo.attr.stride_s        = ODSY_CFIFO_NR_2BPP_TILES;
	mm->cfifo.attr.stride_t        = ODSY_CFIFO_NR_2BPP_TILES;

	/* the free zone */
	mm->nbfr.attr.type = ODSY_MM_NBFR;

	/* 
	** start frame buffer after the cfifo at the next available 32bpp tile.
	** gfP_OdsyBRDMGRconfigFb will take up where we leave off and fix
	** up any alignment problems. that routine will also set the base_addr
	** for the  wbfr and nbfr(free space).  but we set 'em here just for kicks.
	*/
        mm->fbfr.base_addr = ODSY_MM_32BPP_ADDR_FROM_TILENR(ODSY_CFIFO_NR_32BPP_TILES);
        mm->nbfr.base_addr = mm->wbfr.base_addr = mm->fbfr.base_addr;


	OdsyInitCSet(&mm->sg_set,4); /* chunk size of 2^4 */
	OdsyInitCSet(&mm->as_set,4); /* chunk size of 2^4 */
}

#if !defined(_STANDALONE)

/*
** allocate a region of sdram described by the given attrs
** dimensions will be rounded up to the next integral tile.
**
** for now, we translate the attributes to the 32 bpp tile-space.
** this gives us an allocation "granularity" of 16*14*32=8k 
** (which sucks, really).
**
** the "right way" to do this is as a carving up of 
** the tile-space hierarchy.  
**
** just for kicks: here's what fits nicely into an 8k "page"
**    rgba8 (4bpp) * 64 * 64 -> 16k (or 2 "pages")
**    rgba8 (4bpp) * 32 * 32 ->  4k (or 1/2 "pages")
**
** if we _really_ wanted to keep this simplistic "pagesize",
** we _could_ allow the lib to carve up the region on its
** own in order to pack the smaller levels.  this would
** (in effect) coalesce them wrt placement and swapping
** (possibly a decent trade-off vs lib complexity).  
** 
**
*/
int odsyMMalloc(struct gfx_gfx    *gfxp, 
		odsy_mm_sg        *sg, 
		odsy_mm_alloc_arg *aarg,
		odsy_mm_obj_grp   *mmgrp, 
		odsy_mm_obj       *mmobj)
{
	odsy_chunked_set *setset;
	odsy_mm_obj_attr *attr = &aarg->attr;

	if (mmobj->grp_hdl == ODSY_MM_NULLHDL) {
#ifdef ODSY_DEBUG_SDRAM
		cmn_err(CE_DEBUG,"mmobj 0x%x ->grp_hdl == NULL\n", mmobj);
#endif
	}

	
	if(attr->bpp!=32 && attr->bpp!=16 && attr->bpp!=8 && attr->bpp!=4 && attr->bpp!=2){
		cmn_err(CE_WARN,"odsy: invalid object format given in MMalloc request");
		return EINVAL;
	}		

	/*y has no limit because of 3d textures.  XXX sure there's a limit. */
	if((attr->dx==0) || (attr->dx>ODSY_MM_MAX_DIM) ||  (attr->dy==0)
	                 ||(attr->dy == 0)||(attr->dz==0)) { 
		cmn_err(CE_WARN,"odsy: invalid object dimensions given in MMalloc request");
		return EINVAL;
	}

	mmobj->attr.dx     = attr->dx;
	mmobj->attr.dy     = attr->dy;
	mmobj->attr.dz     = attr->dz;
	mmobj->attr.ox     = attr->ox;
	mmobj->attr.oy     = attr->oy;
	mmobj->attr.oz     = attr->oz;
	mmobj->attr.stride_s     = attr->stride_s;
	mmobj->attr.stride_t     = attr->stride_t;

#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy mmalloc: (%dx%dx%d) (%d,%d,%d) (%d,%d)\n",
		attr->dx,attr->dy,attr->dz,
		attr->ox,attr->oy,attr->oz,
		attr->stride_s,attr->stride_t);

#endif
	
	// A little sanity checking on the attr setup...

	if (mmobj->attr.stride_s == 0) {
		mmobj->attr.stride_s =
			 ODSY_MM_NR_TILES_FROM_PIXELS(attr->dx+attr->ox); 
				/* whole tiles */
	} else {
		if (mmobj->attr.stride_s < ODSY_MM_NR_TILES_FROM_PIXELS(attr->dx+attr->ox)) {

		cmn_err(CE_WARN,"odsy: invalid stride_s (too small) in MMalloc request");
		return EINVAL;
		}	

	}

        if (mmobj->attr.stride_t == 0) {
		/*if texture with borders they should have gave us a value*/
		mmobj->attr.stride_t = 
			ODSY_MM_NR_TILES_FROM_PIXELS(attr->dy+attr->oy) *
					mmobj->attr.stride_s;

			
	} else {
		if ( mmobj->attr.stride_t <
			(ODSY_MM_NR_TILES_FROM_PIXELS(attr->dy+attr->oy) *
				 mmobj->attr.stride_s)) { 
		cmn_err(CE_WARN,"odsy: invalid stride_t (too small) in MMalloc request"); 
		return EINVAL;
		 }	
	}

	mmobj->attr.nrtiles = mmobj->attr.stride_t * (mmobj->attr.dz + mmobj->attr.oz);
	mmobj->attr.type    = attr->type;
	mmobj->attr.res_fmt = attr->type; 
	mmobj->attr.bpp     = attr->bpp;


	// Reserve anything necessary within the system so that we
	// can guarantee to be able to make the object resident later.

	ODSY_MM_LOCK(gfxp,sg->mm);
	{
		int ctx_hdl;

		if ((*sg->mm->fncs->reserve)(mmobj)) {
			cmn_err(CE_WARN,"odsy: mm unresolveable oversubscription (object "
				"deletion/redefinition within the sharegroup necessary)");
			ODSY_MM_UNLOCK(gfxp,sg->mm);
			return EAGAIN;
		}


		/* we could be adding a new level to a sparse level set. mark any corresponding      *\
		\* coresid range as non-resident, so the user comes back and gets updated addresses  */

		/*
		for each gctx element of sg 
				 for each cores in mmgrp->coresid_ranges_by_gctx[ctx_hdl]
						  
						  cores->resid_busy.busy = 1;
		                                  cores->resid_busy.resid = 0;
	        */

		OdsyInitCSetCursor(&mmgrp->coresid_ranges_by_gctx);

		while ( setset = (odsy_chunked_set*) OdsyNextCSetElem(&mmgrp->coresid_ranges_by_gctx) ) {

			int cores_hdl;
			odsy_mm_coresid_range *cores;

			OdsyInitCSetCursor(setset);

			while ( cores = (odsy_mm_coresid_range*)OdsyNextCSetElem(setset) ) {

				if (cores->low <= mmobj->obj_level && 
				    cores->high >= mmobj->obj_level) {
					/* this isn't true victimization...  we just added a new reserved level */
					__synchronize();
					odsyMMcoresidRange_CoresSlot(cores)->busy  = 1;
					__synchronize();
					odsyMMcoresidRange_CoresSlot(cores)->resid = 0;
					__synchronize();
					odsyMMcoresidRange_CoresSlot(cores)->busy  = 0;
					__synchronize();
				}

			}
			
		}

	}

	ODSY_MM_UNLOCK(gfxp,sg->mm);

	return 0;

}





/*ARGSUSED*/
int odsyMMvalidatePcxSwap(odsy_data *bdata, odsy_gctx *igctx, odsy_gctx *ogctx)
{
	odsy_mm *mm = &bdata->mm;

	/* assume gfxsema and gfxlock held */
	ASSERT(bdata->gfx_data.gfxsema_owner);
	ASSERT(bdata->gfx_data.gfxlock);

	// Short circuit because any items created by the 
	// board manager (pbuffers) are never swapped out.
	if (IS_BRDMGR_GCTX(igctx)) return 0;

	// Make sure any objects we need to be resident, are.
	return (*mm->fncs->validate_commit)(mm,igctx,ogctx);
}





/*
** create a new address space node if we need.  either way attach
** this sharegroup to the one we create or find...  assume we are
** mm locked elsewhere.
*/
int SGAddressSpace(struct gfx_gfx *gfxp, odsy_mm_sg *insg,  odsy_mm *mm)
{
	odsy_mm_as  *as;
	int          elem_nr;
        vasid_t      vasid;
	int          i;

        as_lookup_current(&vasid);

	// See if we can find vasid in the set of already known address spaces.

	OdsyInitCSetCursor(&mm->as_set);

	while ( as = (odsy_mm_as *)OdsyNextCSetElem(&mm->as_set) ) {
		
		if ((as->vasid.vas_pasid == vasid.vas_pasid) && (as->vasid.vas_obj == vasid.vas_obj)) {

			as->ref_cnt++; /* added me to the address space*/
#ifdef ODSY_DEBUG_SDRAM
			cmn_err(CE_DEBUG,"odsy pmm: Added sg 0x%x to as 0x%x\n",insg, as);
#endif
			insg->as = as;   
			return 0;
		}
	}
	
	

	/*XXX cache me */
	if (! (as = (odsy_mm_as *) kmem_alloc(sizeof(odsy_mm_as),KM_SLEEP))) goto jettison_kmem;
	as->vasid = vasid;
#ifdef ODSY_DEBUG_SG
	cmn_err(CE_DEBUG,"odsy pmm: Create address space for sg 0x%x as 0x%x\n",insg, as);
#endif
	/* insert the new as record into the display's set of address spaces */
	{
		if((as->as_hdl = OdsyAllocCSetId(&mm->as_set,(void*)as)) == ODSY_MM_NULLHDL ){
			cmn_err(CE_WARN,"odsy: couldn't allocate an as id for a new as");
			goto jettison_kmem;
		}
	}



	as->mm = mm;
	as->ref_cnt = 1;

	as->uv_hw_wr_rgn      = 0; 
	

	// Map the swsync write-back region into the address space.

	if (odsyPinAndMapASRegions(as) != 0){
		cmn_err(CE_NOTE,"odsy: could not pin and map as regions\n");
		OdsyDeallocCSetId(&mm->as_set,as->as_hdl);
		if (as) {
			KMEM_FREE(as,sizeof(odsy_mm_as),0xfe01);
		}
		return ENOMEM;
	}


	insg->as = as;   /* now the sharegroup can find its address space */

	return 0;

jettison_kmem:
	/*XXX cache me */
	if (as) {
		KMEM_FREE(as,sizeof(odsy_mm_as),0xfe03);
	}

	return ENOMEM;
}




void DeleteSGFromAddressSpace(struct gfx_gfx *gfxp, odsy_mm_sg *sg)
{
	odsy_mm_as *as;
	int destroy_as;
	as= sg->as;

	ODSY_MM_LOCK(gfxp,as->mm);

	destroy_as = (--as->ref_cnt == 0);

	if (destroy_as == 0) {
		ODSY_MM_UNLOCK(gfxp,as->mm);
		return;
        }

	/*delete the address space*/
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: Delete Address Space 0x%x\n", as);
#endif

	GFXLOGT(ODSY_DeleteAddressSpace);
	GFXLOGV(as->as_hdl);

	OdsyDeallocCSetId(&as->mm->as_set,as->as_hdl);

	ODSY_MM_UNLOCK(gfxp,as->mm);


        odsyUnpinAndUnmapASRegions(as);


	KMEM_FREE(as,sizeof(odsy_mm_as),0xfe05);

}




/*
** create a new sharegroup and make the creating gctx the first member
*/
int NewSharegroup(struct gfx_gfx *gfxp,
		  odsy_gctx      *gctx,
		  odsy_mm_sg    **sgp, 
		  odsy_mm        *mm )
{
	odsy_mm_sg               *sg;
	int elem_nr;
	
	*sgp = 0;

	/*XXX cache me */
	if (! (sg = (odsy_mm_sg *) kmem_alloc(sizeof(odsy_mm_sg),KM_SLEEP))) goto jettison_kmem;

	/* insert the new sg record into the display's set of sharegroups */
	ODSY_MM_LOCK(gfxp,mm);

	if((sg->sg_hdl = OdsyAllocCSetId(&mm->sg_set,(void*)sg)) == ODSY_MM_NULLHDL ){
		cmn_err(CE_WARN,"odsy: couldn't allocate a sharegroup id for a new sharegroup");
		goto jettison_kmem;
	}


	sg->mm = mm;
	sg->ref_cnt = 1;

	// We don't allocate/map the object address communiction region(s) until gfP_OdsyMMnotify.

	OdsyInitCSet(&sg->obj_addr_rgns,4/* 16 chunks O(1) */);

	/* check to see if we've already attached to this address space */
	if (SGAddressSpace(gfxp,sg,mm) != 0){
		cmn_err(CE_NOTE,"odsy: SGAddressSpace failed");
			goto jettison_kmem;
	}


	OdsyInitCSet(&sg->ctx_set,4);     /* chunk size of 2^4 for the context set */
	OdsyInitCSet(&sg->obj_set,4);     /* ditto for the set of mm objects       */
	OdsyInitCSet(&sg->grp_set,4);     /* and for object groups                 */
	OdsyInitCSet(&sg->dma_rgn_set,4); /* and for backing store dma regions     */
	OdsyInitCSet(&sg->obj_addr_slot_set,4); /* and for sdram communication slot rgns */


	if ( (gctx->ctx_hdl = OdsyAllocCSetId(&sg->ctx_set,(void*)gctx)) == ODSY_MM_NULLHDL){
		cmn_err(CE_WARN,"odsy: could not allocate a new context sharegroup set element");
		OdsyDeallocCSetId(&mm->sg_set,sg->sg_hdl);
		goto jettison_kmem;
	}

	gctx->sg       = sg;   /* the gctx can find its sharegroup */

	*sgp = sg;

	ODSY_MM_UNLOCK(gfxp,mm);

	return 0;

jettison_kmem:
	/*XXX cache me */
	if (sg) {
		KMEM_FREE(sg,sizeof(odsy_mm_sg),0xfe07);
	}
	ODSY_MM_UNLOCK(gfxp,mm);
	return ENOMEM;
}




void DeleteSharegroup(struct gfx_gfx *gfxp, odsy_mm_sg *sg)
{
	odsy_mm_obj_grp *mmgrp;

	GFXLOGT(ODSY_DeleteSharegroup);
	GFXLOGV(sg->sg_hdl);

	/* 
	** release all the groups/objects left hanging around.
	** there are no contexts attached at this point, so we
	** can free up the kv heap memory and be done with it.
	** note: we only free the groups explicitly.  as each
	** group frees itself, it frees the objects it knows
	** about.  because there are no contexts left to refer
	** to these objects, we don't have to worry about 
	** dangling references in any "known" group or object
	** structs at this point...
	*/
	OdsyInitCSetCursor(&sg->grp_set);

	while ( mmgrp = OdsyNextCSetElem(&sg->grp_set) ) {
		if ( mmgrp->is_scratch ) {
			cmn_err(CE_WARN,"odsy pmm: DelSG: sees scratch!\n");
		}
		DeleteGroup(mmgrp);
	}
	OdsyDestroyCSet(&sg->grp_set,0);



	
	OdsyDeallocCSetId(&sg->mm->sg_set,sg->sg_hdl);

#ifdef ODSY_DEBUG_SG
	cmn_err(CE_DEBUG,"odsy pmm: Delete Sharegroup 0x%x\n", sg);
#endif
        odsyUnpinAndUnmapSGRegions(sg);
	DeleteSGFromAddressSpace(gfxp,sg);

	KMEM_FREE(sg,sizeof(odsy_mm_sg),0xfe09);


}





void DetachGctxFromSharegroup(odsy_mm_sg *sg, odsy_gctx *gctx)
{
	odsy_mm_obj_grp       *mmgrp;
	odsy_mm_coresid_range *cores;

	ASSERT(sg);
	ASSERT(gctx);
	ASSERT(sg == gctx->sg);


#ifdef ODSY_DEBUG_SG
	cmn_err(CE_DEBUG,"odsy sdram: DetachGctxFromSharegroup %x %x\n", sg, gctx);
#endif

	// Destroy all coresid ranges this gctx has outstanding.

	OdsyInitCSetCursor(&gctx->coresid_range_set);

	while ( cores = OdsyNextCSetElem(&gctx->coresid_range_set) ) {


#ifdef ODSY_DEBUG_SG
		cmn_err(CE_DEBUG,"odsy sdram : detachGctx cores 0x%x\n",cores);
#endif
		// !!!Gross hack!!!  We have a few dummy records in the cset for the
		// coresid ranges.  Hopefully all pointers are > ACTIVE_TEXTURE_GROUPS :(
		if ( (__psunsigned_t)cores <= ODSY_MM_MAX_ACTIVE_TEXTURE_GROUPS ) continue;


		// Remove all coresid ranges this gctx has wrt the mmgrp
		// mmgrp_side_set can be duplicated in some coresid ranges. (multiple ranges by gctx on same grp)
		// So don't destroy it multiple times !
		cores->mmgrp_side_set = (odsy_chunked_set*)OdsyLookupCSetId(&cores->mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl);
		if ( cores->mmgrp_side_set ) {
#ifdef ODSY_DEBUG_SG
			cmn_err(CE_DEBUG,"odsy sdram : detachGctx remove set_set 0x%x\n",cores->mmgrp_side_set);
#endif
			OdsyDeallocCSetId(&cores->mmgrp->coresid_ranges_by_gctx,gctx->ctx_hdl);
			OdsyDestroyCSet(cores->mmgrp_side_set,0);

			KMEM_FREE(cores->mmgrp_side_set,sizeof(odsy_chunked_set),0xfe0c);
			
		}
		
		DeleteCoresidRange(cores);
	}




	// Release the groups which are only ever referenced by this gctx.

	OdsyInitCSetCursor(&gctx->ibfr_set);
	while ( mmgrp = OdsyNextCSetElem(&gctx->ibfr_set) ) {
		odsy_mm_hdl grp_hdl = mmgrp->grp_hdl;
		if ( mmgrp->is_scratch ) {
			DeleteGroup(mmgrp);
			OdsyDeallocCSetId(&sg->grp_set,grp_hdl);
		} else {
			cmn_err(CE_WARN,"found non scratch in ibfr_set\n");
		}
	}


	OdsyDestroyCSet(&gctx->coresid_range_set,0);
	OdsyDestroyCSet(&gctx->ibfr_set,0);

	// Take ourselves out of the sharegroup's set of contexts.

	OdsyDeallocCSetId(&sg->ctx_set,gctx->ctx_hdl);
	gctx->ctx_hdl = -1;

}



odsy_mm_obj *NewObj(odsy_mm_sg *sg, odsy_mm_obj_grp *mmgrp, int level)
{


	odsy_mm_obj *mmobj;
	
	if (level<0 || level>= ODSY_MM_OBJS_PER_GROUP) {
		cmn_err(CE_WARN,"odsy: bad level for new mm object");
		return 0;
	}

  	                           /*XXX cache me */
	if (mmobj  = (odsy_mm_obj*)kmem_alloc(sizeof(odsy_mm_obj),KM_SLEEP)){
		mmobj->sg        = sg;
		mmobj->mmgrp     = mmgrp;
		mmobj->grp_hdl   = mmgrp->grp_hdl;
		mmobj->obj_level = level;
		mmobj->obj_hdl   = ODSY_MM_NULLHDL; /* still not _really_ viable */

		mmobj->resid  = 0; /* !resid and !bs.valid is okay.  just garbage. */
		mmobj->pinned = 0;

		mmobj->bs.slice_bytes= mmobj->attr.dx * mmobj->attr.dy * mmobj->attr.bpp;
		mmobj->bs.bytes      = mmobj->attr.dx * mmobj->attr.dy * mmobj->attr.dz * mmobj->attr.bpp;
		mmobj->bs.valid      = 0;
		mmobj->bs.mem        = 0;
		mmobj->bs.last_dma_marker = ODSY_XFR_NUM_INVAL;


		mmobj->hook       = 0;  /* opaque to us */
		mmobj->fncs_state = ODSY_MM_FNCS_OBJ_STATE_NONE;

                mmobj->obj_hdl=OdsyAllocCSetId(&sg->obj_set, (void *) mmobj);
                if((mmobj->obj_hdl) == ODSY_MM_NULLHDL) {
			KMEM_FREE(mmobj,sizeof(odsy_mm_obj),0xfe0f);
                        return 0;
                }
		mmgrp->objs[level]= mmobj; /*attach to group by level*/
		mmgrp->nr_objs ++;
	}

#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: NewObj 0x%x,0x%x[%d]\n", sg->sg_hdl, mmobj->mmgrp->grp_hdl,level);
#endif

	return mmobj;
}


void DeleteObj(odsy_mm_obj *mmobj)
{
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: DeleteObj 0x%x,0x%x[%d]\n", 
		mmobj->mmgrp->sg->sg_hdl, mmobj->mmgrp->grp_hdl, mmobj->obj_level);
#endif

	if (mmobj->fncs_state != ODSY_MM_FNCS_OBJ_STATE_NONE) {
		cmn_err(CE_WARN,"odsy pmm: object state %d in DeleteObj");
	}

	/* take self out of sharegroup's object set */
	OdsyDeallocCSetId(&mmobj->mmgrp->sg->obj_set,mmobj->obj_hdl);
	ReleaseBackingStore(mmobj);

	mmobj->mmgrp->objs[mmobj->obj_level] = 0; 
	mmobj->mmgrp->nr_objs--;

	/*XXX cache me */
	KMEM_FREE(mmobj,sizeof(odsy_mm_obj),0xfe11);
}


/*
** ReleaseBackingStore
**     Jettison any backing memory associated with this object.
*/
void ReleaseBackingStore(odsy_mm_obj *mmobj)
{
	int size = mmobj->attr.dz * mmobj->attr.dx * mmobj->attr.dy * mmobj->attr.bpp;

	if ( mmobj->bs.last_dma_marker != ODSY_XFR_NUM_INVAL ) {
		odsyDMAWait( mmobj->mmgrp->sg->mm->bdata,0,0,ODSY_DMA_WAIT_OP,&mmobj->bs.last_dma_marker );
	}

	if ( mmobj->bs.mem ) {
		KMEM_FREE((void*)mmobj->bs.mem,size,0xfe13);
	}
	mmobj->bs.mem = 0;
}


// ---------------------------------------------------------------------------
// Create a new object group.  A little extra complexity added when we note
// that we don't have a place to put the sdram addresses so the user can
// see them.  Instead of allocating a honking big (as we'd ever need) space
// for that we "fault-in" chunks here.  
// ---------------------------------------------------------------------------
odsy_mm_obj_grp *NewGroup(odsy_gctx *gctx, odsy_mm_sg *sg, odsy_mm_alloc_arg *karg)
{

	int got_grp_hdl      = 0;
	int got_obj_addr_hdl = 0;
	int got_addr_rgn     = 0;
	int got_addr_rgn_chunk=0;
	int got_addr_rgn_id   =0;
	int rgn_chunk;

	odsy_mm_obj_grp *mmgrp = (odsy_mm_obj_grp*)kmem_alloc(sizeof(odsy_mm_obj_grp),KM_SLEEP);
	odsy_addr_rgn *addr_rgn;

	if (mmgrp) {
		mmgrp->creator    = gctx; // creator only makes sense for ibfrs which aren't shared
		mmgrp->sg         = sg;
		mmgrp->is_scratch = karg->attr.type == ODSY_MM_IBFR;

		if ( mmgrp->is_scratch ) {
			if ((mmgrp->ibfr_set_hdl 
			     = OdsyAllocCSetId(&gctx->ibfr_set,(void *)mmgrp)) == ODSY_MM_NULLHDL) {
				KMEM_FREE(mmgrp,sizeof(odsy_mm_obj_grp),0xfe15);
				return 0;			
			}
		} 
		else {
			mmgrp->ibfr_set_hdl = ODSY_MM_NULLHDL;
		}

		// Allocate an id within the sharegroup.

		if((mmgrp->grp_hdl  = OdsyAllocCSetId(&sg->grp_set, (void *) mmgrp)) == ODSY_MM_NULLHDL) {
			if ( mmgrp->is_scratch ) {
				OdsyDeallocCSetId(&gctx->ibfr_set,mmgrp->ibfr_set_hdl);
			}
			KMEM_FREE(mmgrp,sizeof(odsy_mm_obj_grp),0xfe17);
			return 0;
		}

		got_grp_hdl = 1;


		// Allocate an object pipe address communication slot.

		if( (mmgrp->obj_addr_slot_id = OdsyAllocCSetId(&sg->obj_addr_slot_set,(void*)mmgrp)) == ODSY_MM_NULLHDL ) {
			goto bail_out;
		}
		got_obj_addr_hdl = 1;
		
		if ( ! (addr_rgn = (odsy_addr_rgn*)OdsyLookupCSetId(&sg->obj_addr_rgns,
								    rgn_chunk = (int)ODSY_MM_OAC_SLOT_CHUNK(mmgrp->obj_addr_slot_id))) ) {


			// If we're over the limit on the regrowth of the object address region chunks just fail, hard.
			if ( rgn_chunk >= ODSY_MM_OAC_MAX_SLOT_CHUNKS ) {
				cmn_err(CE_WARN,"odsy pmm: reached slot chunks hard limit in NewGroup");
				goto bail_out;
			}

			// We need to grow the object pipe memory address communication region.
			// Allocate a rgn struct, then the actual memory, then map it.  
			// If that all works out then put it into the CSet.

			if ( ! (addr_rgn = (odsy_addr_rgn*)kmem_alloc(sizeof(odsy_addr_rgn),KM_SLEEP)) ) {
				cmn_err(CE_WARN,"odsy pmm: can't allocate addr region struct in NewGroup");
				goto bail_out;
			}
			got_addr_rgn = 1;

			if ( ! (addr_rgn->kv_ptr = (caddr_t)kmem_alloc(ODSY_MM_OAC_RGN_CHUNK_SIZE,KM_SLEEP)) ) {
				cmn_err(CE_WARN,"odsy pmm: can't allocate addr region chunk in NewGroup");
				goto bail_out;
			}
			got_addr_rgn_chunk = 1;

			if ( OdsyAllocSpecificCSetId(&sg->obj_addr_rgns,
						     (int)ODSY_MM_OAC_SLOT_CHUNK(mmgrp->obj_addr_slot_id),
						     (void*)addr_rgn) 
			     == ODSY_MM_NULLHDL ) {
				cmn_err(CE_WARN,"odsy pmm: can't allocate obj pipe memory address "
					"communication slot id in NewGroup");
				goto bail_out;
			}
			got_addr_rgn_id = 1;

			addr_rgn->size = ODSY_MM_OAC_RGN_CHUNK_SIZE;
			addr_rgn->uv_ptr = 0;
			if ( MapKernelRegionIntoUserSpace(addr_rgn,VDD_READ) ) {
				cmn_err(CE_WARN,"odsy pmm: can't map kernel mem into user in NewGroup");
				goto bail_out;
			}

			// This goes back to the user to tell them what just happened.
			karg->flag |= ODSY_MM_ALLOC_GREW_OAC_RGN;
			karg->obj_addr_comm_rgn = (odsy_uaddr)addr_rgn->uv_ptr;
			karg->obj_addr_comm_rgn_chunk = (unsigned short)rgn_chunk;

			
		} // Whew.  What a mess... glad that is over.  


		mmgrp->obj_addr_group = ((odsy_obj_addr_group*)addr_rgn->kv_ptr)
			+ ODSY_MM_OAC_SLOT_CHUNK_OFFSET(mmgrp->obj_addr_slot_id);

		bzero((caddr_t)mmgrp->objs,sizeof(odsy_mm_obj*)*ODSY_MM_OBJS_PER_GROUP);

		OdsyInitCSet(&mmgrp->coresid_ranges_by_gctx,2); /* 2^2 elem to start with (idx by gctx id) */
	}

#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: NewGroup 0x%x,0x%x\n",sg->sg_hdl,mmgrp->grp_hdl);
#endif

	mmgrp->nr_objs = 0;

	return mmgrp;


bail_out:


	if ( got_addr_rgn_id ) {
		OdsyDeallocCSetId(&sg->obj_addr_rgns,
				  (int)ODSY_MM_OAC_SLOT_CHUNK(mmgrp->obj_addr_slot_id));
	}
	if ( got_addr_rgn_chunk ) {
		KMEM_FREE(addr_rgn->kv_ptr,ODSY_MM_OAC_RGN_CHUNK_SIZE,0xfe19);
	}
	if ( got_addr_rgn ) {
		KMEM_FREE(addr_rgn,sizeof(odsy_addr_rgn),0xfe1b);
	}
	if ( got_obj_addr_hdl ) {
		OdsyDeallocCSetId(&sg->obj_addr_slot_set,mmgrp->obj_addr_slot_id);
	}
	if ( got_grp_hdl ) {
		OdsyDeallocCSetId(&sg->grp_set,mmgrp->grp_hdl);
	}
	KMEM_FREE(mmgrp,sizeof(odsy_mm_obj_grp),0xfe1d);
	return 0;
	
}





static void DeleteCoresidRange(odsy_mm_coresid_range *cores)
{
	odsy_data *bdata;


#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: DeleteCoresidRange 0x%x,0x%x[%d,%d] \n",
		cores->mmgrp->sg->sg_hdl,cores->mmgrp->grp_hdl,cores->low,cores->high);
#endif


	bdata = cores->mmgrp->sg->mm->bdata;
	/* 
	** XXX we can't unconditionally return this to the freelist because there may
	** be an outstanding sw sync token which hasn't written back yet... 
	** we should at least check to see that this isn't the case.
	*/
	if (odsyMMcoresidRange_HwWrSlotId(cores) != ODSY_MM_NULLHDL) { 
		OdsyFree32kSlot(&bdata->hw_wr_slots,odsyMMcoresidRange_HwWrSlotId(cores));
	}



	KMEM_FREE(cores,sizeof(odsy_mm_coresid_range),0xfe1f);
	
}





void DeleteGroup(odsy_mm_obj_grp *mmgrp) {

	int                    obj_nr;
	odsy_mm_obj           *mmobj;
	odsy_chunked_set      *cores_by_gctx = 0;
	odsy_mm_coresid_range *cores;
	
	GFXLOGT(ODSY_DeleteGroup);
	GFXLOGV(mmgrp->grp_hdl);
#ifdef ODSY_DEBUG_SG
	cmn_err(CE_DEBUG,"odsy pmm: DeleteGroup 0x%x,0x%x\n", mmgrp->sg->sg_hdl, mmgrp->grp_hdl);
#endif

	if (mmgrp->grp_hdl != ODSY_MM_NULLHDL) { /* sometimes we need to just reclaim the memory */

		/* release all coresid ranges wrt this group in any gctx */
		OdsyInitCSetCursor(&mmgrp->coresid_ranges_by_gctx);

		while ( cores_by_gctx = OdsyNextCSetElem(&mmgrp->coresid_ranges_by_gctx) ) {
			// Each coresid range is in the gctx's side set.
#ifdef ODSY_DEBUG_SG
			cmn_err(CE_DEBUG,"odsy pmm: deleteGroup set_set 0x%x\n",cores_by_gctx);
#endif
			OdsyInitCSetCursor(cores_by_gctx);

			while ( cores = OdsyNextCSetElem(cores_by_gctx) ) {
#ifdef ODSY_DEBUG_SG
				cmn_err(CE_DEBUG,"odsy pmm: deleteGroup cores 0x%x\n",cores);
#endif
				if ( cores->gctx->coresid_range_set.c_size ) {
					OdsyDeallocCSetId(&cores->gctx->coresid_range_set,cores->gctx_side_id);
					DeleteCoresidRange(cores);
				} else {
					cmn_err(CE_WARN,"odsy pmm: just leaked a cores...\n");
				}
			}
			OdsyDestroyCSet(cores_by_gctx,0);
			KMEM_FREE(cores_by_gctx,sizeof(odsy_chunked_set),0xfe21);
		}

		OdsyDestroyCSet(&mmgrp->coresid_ranges_by_gctx,0);


		// Destroy each object in turn after removing them from the policy system.

		for (obj_nr=0; obj_nr<ODSY_MM_OBJS_PER_GROUP; obj_nr++) {
			if (mmobj = mmgrp->objs[obj_nr]) {

				/* walk out of the policy/strategy code if we need to. */
				if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_COMMITTED)
					(*mmobj->mmgrp->sg->mm->fncs->decommit)(mmobj);	

				
				if (mmobj->fncs_state == ODSY_MM_FNCS_OBJ_STATE_RESERVED)
					(*mmobj->mmgrp->sg->mm->fncs->release)(mmobj);	


				DeleteObj(mmobj);
			}
		}


		
		/* give up sdram address communication slot id */
		OdsyDeallocCSetId(&mmgrp->sg->obj_addr_slot_set,mmgrp->obj_addr_slot_id);

		// caller removes the group from the sg's group cset



	}


	/*XXX cache me */
	KMEM_FREE(mmgrp,sizeof(odsy_mm_obj_grp),0xfe23);
}



//
// Create tracking state for a new coresidency range.
// If we need more cores slot space we allocate it here.
//
static odsy_mm_coresid_range *NewCoresidRange(odsy_gctx             *gctx,
					      odsy_mm_obj_grp       *mmgrp,
					      int                    high,
					      int                    low,
					      odsy_chunked_set *     grp_side_set,
					      odsy_chunked_set *     gctx_side_set,
					      odsy_mm_query_arg      *k_qryarg)

{
	int got_hw_slot_id = 0;
	int got_rb_slot_id = 0;
	int got_gctx_side_id = 0;
	int got_grp_side_id  = 0;

	odsy_mm_sg            *sg     = mmgrp->sg;
	odsy_mm               *mm     = mmgrp->sg->mm;
	odsy_data             *bdata  = mm->bdata;
	odsy_mm_coresid_range *cores = (odsy_mm_coresid_range*)kmem_alloc(sizeof(odsy_mm_coresid_range),KM_SLEEP);
	odsy_addr_rgn         *rgn;
	int                    rgn_chunk;


	if (!cores) return 0;

	cores->gctx           = gctx;
	cores->mmgrp          = mmgrp;
	cores->low            = low;
	cores->high           = high;
	cores->mmgrp_side_set = grp_side_set;

	// Get the ids we need for tracking these guys from both sides.

	if ((cores->gctx_side_id
	     = odsyMMcoresidRange_CoresSlotId(cores) 
	     = OdsyAllocCSetId(gctx_side_set,(void*)cores)) == ODSY_MM_NULLHDL) {
		goto bail_out;
	}
	got_gctx_side_id = 1;

	// This id we just got is the the coresidency slot within the gctx.
	// If we (for some bad reason) don't have the cores rgn chunk necessary
	// to track this one, then that means the user has ignored our suggestion
	// to allocate more... we have to fail.
	
	
	if ( !( rgn = (odsy_addr_rgn *)
		OdsyLookupCSetId(&gctx->coresid_rgn_set,
				 rgn_chunk = (int)ODSY_MM_CORES_SLOT_CHUNK(odsyMMcoresidRange_CoresSlotId(cores)))) ) {

		// If we aren't over the limits on coresid region chunks, try to allocate and map a new one.
		
		if ( rgn_chunk >= ODSY_MM_CORES_MAX_SLOT_CHUNKS ) {
			cmn_err(CE_WARN,"odsy pmm: out of cores region chunks");
			goto bail_out;
		}

		

		// Allocate the rgn thingy.
		if ( !(rgn = (odsy_addr_rgn*)kmem_alloc(sizeof(odsy_addr_rgn),KM_SLEEP)) ) {
			cmn_err(CE_WARN,"odsy pmm: cannot allocate memory for cores rgns struct");
			goto bail_out;
		}
		bzero(rgn,sizeof(odsy_addr_rgn));


		// Allocate the memory we need.

		if ( !(rgn->kv_ptr = (caddr_t)kmem_alloc(ODSY_MM_CORES_RGN_CHUNK_SIZE, KM_SLEEP)) ) {
			cmn_err(CE_WARN,"odsy pmm: cannot allocate cores region chunk");
			KMEM_FREE(rgn,sizeof(odsy_addr_rgn),0xfe25);
			goto bail_out;
		}

		rgn->size = ODSY_MM_CORES_RGN_CHUNK_SIZE;

		// Perform the mapping.

		if ( MapKernelRegionIntoUserSpace(rgn, VDD_READ|VDD_WRITE) ) {
			cmn_err(CE_WARN,"odsy pmm: cannot map cores region into user space");
			KMEM_FREE((void*)rgn->kv_ptr,ODSY_MM_CORES_RGN_CHUNK_SIZE,0xfe27);
			KMEM_FREE(rgn,sizeof(odsy_addr_rgn),0xfe29);
			goto bail_out;
		}

		
		// Put the region pointer in the set.

		OdsyAllocSpecificCSetId(&gctx->coresid_rgn_set,rgn_chunk,rgn);

		// The first few words of the zeroth chunk are used to 
		// communicate live object handles in the direction
		// of context->kernel for quick lookup at ctxsw time.
		// We need to set these so we don't think they are active.

		if ( rgn_chunk == 0 ) {
			int i;
			for ( i=0; i < ODSY_MM_MAX_ACTIVE_TEXTURE_GROUPS; i++) {
				*(((volatile odsy_mm_hdl*)rgn->kv_ptr) + i ) = ODSY_MM_NULLHDL;
			}
		}
		


		// Communicate what just happened to the user.
		k_qryarg->flag |= ODSY_MM_QUERY_GREW_CORESID_RGNS;
		k_qryarg->cores_rgn_chunk = (unsigned short)rgn_chunk;
		k_qryarg->cores_rgn       = (odsy_uaddr)rgn->uv_ptr;
		
	}


	if ((cores->mmgrp_side_id=OdsyAllocCSetId(grp_side_set,(void*)cores)) == ODSY_MM_NULLHDL) {
		goto bail_out;
	}
	got_grp_side_id = 1;


	// Allocate a hw write-back dependency tracking slot for this gctx,coresid range pair.

	if((odsyMMcoresidRange_HwWrSlotId(cores) = OdsyAlloc32kSlot(&bdata->hw_wr_slots)) == ODSY_MM_NULLHDL){
		goto bail_out;
	}
	got_hw_slot_id = 1;


	// Initialize the depdendency tracking pointers for this coresidency
	// range so that we do not have to compute them every time we use them.

	ASSERT(mm->kv_hw_wr_rgn);

	odsyMMcoresidRange_HwWrSlot(cores)      =   mm->kv_hw_wr_rgn + odsyMMcoresidRange_HwWrSlotId(cores);
	odsyMMcoresidRange_HwWrSlot(cores)->val = 0;


	// Assert that the coresidency slot rgn chunk mappings have taken place...

	ASSERT(rgn->uv_ptr);

	odsyMMcoresidRange_CoresSlot(cores) = ((odsy_mm_cores_slot*)rgn->kv_ptr)
		+ ODSY_MM_CORES_SLOT_CHUNK_OFFSET(odsyMMcoresidRange_CoresSlotId(cores));
	bzero((void*)odsyMMcoresidRange_CoresSlot(cores),sizeof(odsy_mm_cores_slot));



#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: new CoresidRange 0x%x,0x%x[%d,%d]  slot ids hw:0x%x cores:0x%x\n",
		sg->sg_hdl,cores->mmgrp->grp_hdl,cores->low,cores->high,
		odsyMMcoresidRange_HwWrSlotId(cores),odsyMMcoresidRange_CoresSlotId(cores));
#endif	

	return cores;


bail_out:
	if (got_grp_side_id)
		OdsyDeallocCSetId(grp_side_set,cores->mmgrp_side_id);

	if (got_gctx_side_id)
		OdsyDeallocCSetId(gctx_side_set,cores->gctx_side_id);

	if (got_hw_slot_id)
		OdsyFree32kSlot(&bdata->hw_wr_slots,odsyMMcoresidRange_HwWrSlotId(cores));
	
	KMEM_FREE(cores,sizeof(odsy_mm_coresid_range),0xfe2b);
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: failed creation of new CoresidRange\n");
#endif
	return 0;
}


/**** A FEW CRAPPY DMA DEFINES PROBABLY DUPLICATED ELSEWHERE ****/
#define PIX_SZ_2B   0
#define PIX_SZ_4B   1
#define PIX_SZ_8B   2
#define PIX_SZ_16B  3

#define GFX_IO_IN_FMT_1B   0
#define GFX_IO_IN_FMT_2B   1
#define GFX_IO_IN_FMT_4B   2
#define GFX_IO_IN_FMT_8B   4
#define GFX_IO_IN_FMT_8B2  3  /* weird overlap */
#define GFX_IO_IN_FMT_16B  5
#define GFX_IO_IN_FMT_16B2 4  /* weird overlap */
#define GFX_IO_IN_FMT_32B  6

#define GFX_IO_OUT_FMT_PASS 0


static unsigned GFX_IO_IN_FMT_[3/*idx on fldsz encoding*/] = {
	GFX_IO_IN_FMT_2B,
	GFX_IO_IN_FMT_4B,
	GFX_IO_IN_FMT_8B
}; 
/**** A FEW CRAPPY DMA DEFINES PROBABLY DUPLICATED ELSEWHERE ****/



static void SetupDMAParams(odsy_mm_obj *mmobj, odsy_dma_param *dma_param) 
{
	int pxsz;

	bzero((void*)&dma_param,sizeof(odsy_dma_param));

	dma_param->gfx_origin.gfxRelAddr          = 0;
	dma_param->gfx_origin.gfxBufferOrigin     = mmobj->base_addr;
	dma_param->gfx_stride.gfxBufferStride     = mmobj->attr.stride_s;
	dma_param->gfx_win_size.gfxWinXSize       = mmobj->attr.dx;
	dma_param->gfx_win_size.gfxWinYSize       = mmobj->attr.dy;
	dma_param->gfx_win_offset.gfxWinXOffset   = mmobj->attr.ox;
	dma_param->gfx_win_offset.gfxWinYOffset   = mmobj->attr.oy;
	/* FieldOffsetA,B,FieldSelect,UseFront = 0 */
	switch(mmobj->attr.bpp){
		case 8: pxsz = PIX_SZ_8B; break;
		case 4: pxsz = PIX_SZ_4B; break;
		case 2: pxsz = PIX_SZ_2B; break;
		default:
			ASSERT(0);
	}
	dma_param->gfx_field_format.XXXNewFormat = 1;/* meaningless */
	dma_param->gfx_field_format.gfxPixelSize = dma_param->gfx_field_format.gfxFieldSize = pxsz;
	dma_param->gfx_field_format.gfxOutFormat   =  GFX_IO_OUT_FMT_PASS;
	dma_param->gfx_field_format.gfxInFormat    =  GFX_IO_IN_FMT_[pxsz];
	
	dma_param->host_stride.hostStride = 0;
	dma_param->host_scan_line.hostScanLine = mmobj->attr.bpp * mmobj->attr.dx;
	
	dma_param->mode_host_format.transferMode = 
		ODSY_DMA_XFR_MODE(0,/*!gen intr*/
				  0,/* bottom to top */
				  1,/* dup data */
				  0,/* dst is pixpipe*/
				  0,/* is ! gbr */
				  1,/* is coherent */
				  1/* is to host */);

	dma_param->mode_host_format.hostPixelSize = pxsz;

	dma_param->mode_host_format.hostInNoOvwrtAlpha = 0;
	dma_param->mode_host_format.hostInLsbFirst     = 0;
	dma_param->mode_host_format.hostInSwapBytes    = 0;
	dma_param->mode_host_format.hostInExapansion   = 0;
	dma_param->mode_host_format.hostInData         = B_I_INPUT_PIXEL_DATA_DEFAULT;
	dma_param->mode_host_format.hostInFormat       = B_I_INPUT_PIXEL_FORMAT_DEFAULT;


}


//
// DMA an object to/from host memory backing store.  Use async if possible.
//
int odsyMMxfrBackingStore(odsy_mm_obj *mmobj,int *async, odsy_dma_marker *last_op_marker, int to_host)
{

	int            backed_bytes, bytes_per_slice, i;
	odsy_dma_param dma_param;
	



#ifdef  ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy pmm: %s 0x%x,0x%x[%d] (%dx%dx%dx%dBpp @ 0x%x)\n",
		to_host?"backing":"restoring",
		mmobj->mmgrp->sg->sg_hdl,
		mmobj->mmgrp->grp_hdl,
		mmobj->obj_level,
		mmobj->attr.dx, mmobj->attr.dy, mmobj->attr.dz, mmobj->attr.bpp,mmobj->base_addr);
#endif

	SetupDMAParams(mmobj,&dma_param);

	bytes_per_slice = mmobj->attr.dx * mmobj->attr.dy * mmobj->attr.bpp;
	backed_bytes    = mmobj->attr.dz *  bytes_per_slice;

	ASSERT( mmobj->bs.mem );


	//
	// 3D texture levels (dz >= 1) are backed off as a set of slices.
	// Hence this loop...
	// 
        for (i=0; i < mmobj->attr.dz; i++ ) {


		int rc;
		int sdram_offset;
		int stride_t_bytes;
		int used_async;

		stride_t_bytes = 16 *16 * mmobj->attr.bpp * mmobj->attr.stride_t;
		sdram_offset   = (i + mmobj->attr.oz) * stride_t_bytes;
                sdram_offset   = (sdram_offset >> 4);  /* encode */

		dma_param.gfx_origin.gfxBufferOrigin = mmobj->base_addr + sdram_offset;

#ifdef  ODSY_DEBUG_SDRAM_SLICES
		cmn_err(CE_DEBUG, "odsy pmm: backing slice %d base %x orig %x\n", i, mmobj->base_addr, dma_param.gfx_origin.gfxBufferOrigin);
#endif

		if ( rc = odsyDMAOp(0,0, // gfxp, gctx
				    mmobj->mmgrp->sg->mm->bdata,
				    (unsigned char *)mmobj->bs.mem + (i* bytes_per_slice), // start ptr
				    bytes_per_slice,
				    async,
				    &dma_param,
				    last_op_marker) ) {
			cmn_err(CE_WARN," odsy pmm: backing store transfer failed");
			return EINVAL;
		}

	}


	return 0;
	
}





#endif  /* !_STANDALONE */
#endif	/* ODSY_SDRAM_MLOAD_DEVICE */
/*if ! ODSY_SDRAM_MLOAD_DEVICE*/

#if !defined(_STANDALONE)
/*---------------------------------------------------------------------------*/
/*    CODE WHICH EXISTS IN BOTH PRODUCTION AND MLOADABLE DRIVER HACKERY      */
/*---------------------------------------------------------------------------*/

#ifdef ODSY_SDRAM_MLOAD_DEVICE
#define MLMANGLE(symb) odsy_mload_##symb
#define MLMAGIC 1
#else
#define MLMANGLE(symb) symb
#define MLMAGIC 0
#endif


#include "odsy_sdram_lame_mm.c"



/*---------------------------------------------------------------------------*/
/*      MLOAD BOOTSTRAP/DEVICE CODE  NO PRODUCTION CODE BELOW HERE!!!        */
/*---------------------------------------------------------------------------*/
#ifdef ODSY_SDRAM_MLOAD_DEVICE

static odsy_mm_fncs MLMANGLE(sdram_fncs) = {
	MLMANGLE(odsyMMlameReserve),
	MLMANGLE(odsyMMlameCommit),
	MLMANGLE(odsyMMlameValidateCommit),
	MLMANGLE(odsyMMlameDecommit),
	MLMANGLE(odsyMMlameRelease)
};


#include "sys/mload.h"

char *odsy_sdrammversion= M_VERSION;
int odsy_sdramdevflag = D_MP;

void odsy_sdramstart(void)
{
	odsy_data *bdata;
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdramstart\n");
#endif

	bdata = ODSY_BDATA_FROM_PIPENR(0); /*XXX all pipe #s? */
	
	/* attempt to keep someone from hosing their system.  not bulletproof of course */
	if(bdata->nropen_ddrn) {
		cmn_err(CE_WARN,"odsy pmm dynamic reload failed!  there are still ddrn around.\n");
		return;
	} 

	/* wire our fn table into the mm struct's fncs pointer */
	bdata->mm.fncs = &MLMANGLE(sdram_fncs);



}

int odsy_sdramunload(void)
{
	odsy_data *bdata;
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdramunload\n");
#endif

	/* rewire the original fn table */
	bdata = ODSY_BDATA_FROM_PIPENR(0);
	bdata->mm.fncs = &bdata->mm._fncs;

	return 0;
}


void odsy_sdraminit (void)
{
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdraminit: &OdsyBoards[] 0x%x\n",OdsyBoards);
#endif
}



/*ARGSUSED*/
int  odsy_sdramopen(dev_t *devp)
{
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdramopen\n");
#endif

	return 0;
}

/*ARGSUSED*/
int  odsy_sdramclose(dev_t dev)
{
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdramclose\n");
#endif

	return 0;
}

/*ARGSUSED*/
int odsy_sdramioctl(dev_t        dev, 
		  unsigned     cmd, 
		  caddr_t      arg, 
		  int          flag, 
		  struct cred *cr,
		  int         *rvalp)
{
#ifdef ODSY_DEBUG_SDRAM
	cmn_err(CE_DEBUG,"odsy_sdramioctl\n");
#endif

	*rvalp  = 0;
	return 0;
}


int odsy_sdramread()
{
	return EBADRQC;
}

int odsy_sdramwrite()
{
	return EBADRQC;
}


/*ARGSUSED*/
odsy_sdrammap(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
	return EBADRQC;
}

/*ARGSUSED*/
odsy_sdramunmap(dev_t dev, vhandl_t *vt)
{
	return EBADRQC;
}


#endif /* ODSY_SDRAM_MLOAD_DEVICE */

#endif /* !_STANDALONE */
