#ifndef __ODSY_INTERNALS_H__
#error This file only includeable by odsy_internals.h
#endif
/*
** Copyright 1997, Silicon Graphics, Inc.
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
*/



struct odsy_mm;           /* pipe mem mgr anchor   */
struct odsy_mm_sg;        /* share group           */
struct odsy_mm_obj;       /* pipe mem object       */
struct odsy_mm_obj_grp;   /* pipe mem object group */
struct odsy_mm_obj_bs;    /* object backing store  */
struct odsy_data;         /* odsy board data       */
struct odsy_ddrnode;      /* gctx                  */
struct odsy_gctx;         /* ditto                 */
struct odsy_mm_coresid_range; /* range of objects within a group that must be co-resident */
struct odsy_mm_fncs;      /* implementation table  */


typedef struct odsy_mm_obj_bs {
	int                   slice_bytes, bytes;
	int                   valid;
        void                 *mem;      /* kvmem ptr              */
	__uint64_t            last_dma_marker;
} odsy_mm_obj_bs;


typedef struct odsy_mm_obj {
	struct odsy_mm_sg       *sg;
	struct odsy_mm_obj_grp  *mmgrp;

	odsy_mm_hdl              obj_hdl;     /* object handle             */
	odsy_mm_hdl              grp_hdl;     /* group this guy belongs to */
	unsigned short           obj_level;   /* level within the group    */

        odsy_mm_obj_attr         attr;        /* resident img attributes   */
	odsy_sdram_addr          base_addr;   /* base address in sdram     */
	int                      resid;       /* resident, or not?         */
	int                      pinned;      /* non-resident but has to go back where it was */
        struct odsy_mm_obj_bs    bs;          /* backing store             */

	unsigned short           fncs_state;  /* which interfaces the object has gone through */
	void                    *hook;        /* hook for policy/strategy code */

} odsy_mm_obj;


// Associated with every coresidency range are a number of different 
// communication regions.  This pulls them together.

typedef struct odsy_mm_dependency_slot {
	int                      hw_wr_slot_id;      // Slot id within the global hw write-back (swsync) region.
	odsy_mm_hw_wr_slot      *kv_hw_wr_slot;      // Slot pointer into the swsync region for this cores range.

	int                      cores_slot_id;      // Slot id within the gctx for this coresid range.
	odsy_mm_cores_slot      *kv_cores_slot;      // Slot pointer for the coresidency range.
} odsy_mm_dependency_slot;


/* user-mode code refers to ranges of levels within a group which must be all co-resident */
typedef struct odsy_mm_coresid_range {
	int	                      low;   /* low level id  */
	int	                      high;  /* high level id */
	struct odsy_mm_obj_grp       *mmgrp; /* the group the range refers to */
	struct odsy_ddrnode          *gctx;  /* the gctx the range is used by */
	odsy_mm_dependency_slot       dep_slot;
	int                           gctx_side_id;    /* id within the per gctx,mmgrp coresid sets set */
	int                           mmgrp_side_id;    /* id within the per gctx,mmgrp coresid sets set */
	odsy_chunked_set             *mmgrp_side_set;   /* the set to take ourselves out of if we need to */
	void                         *hook;        /* hook for policy/strategy code */
} odsy_mm_coresid_range;


typedef struct odsy_mm_obj_grp {
	struct odsy_mm_sg            *sg;
	odsy_mm_hdl                   grp_hdl;  /* id within the sharegroup */
	odsy_mm_obj                  *objs[ODSY_MM_OBJS_PER_GROUP];
	int                           nr_objs;
	int                           obj_addr_slot_id; 
	odsy_obj_addr_group          *obj_addr_group;
	odsy_chunked_set              coresid_ranges_by_gctx; /* indexed with gctx id/hdl.  hanging off of these are more sets... */
        int                           is_scratch;
	odsy_mm_hdl                   ibfr_set_hdl;           /* set hdl within the owning contexts ibfr set if a scratch buffer set */
	struct odsy_ddrnode          *creator;
} odsy_mm_obj_grp;





/* per 'address space' */
typedef struct odsy_mm_as {
	odsy_mm_hdl         as_hdl;     /* id within the mm */	
	int	            ref_cnt;	/* number of sg that are in this as */
        struct odsy_mm     *mm;    
	vasid_t		    vasid;	/* system id */
	odsy_mm_hw_wr_slot *uv_hw_wr_rgn;
	ddv_handle_t        hw_wr_rgn_ddv;

	odsy_mm_global_rgn *uv_global_rgn;
	ddv_handle_t        global_rgn_ddv;
} odsy_mm_as;


typedef struct odsy_addr_rgn {
	ddv_handle_t ddv;
	caddr_t      kv_ptr;
	uvaddr_t     uv_ptr;
	size_t       size;
} odsy_addr_rgn;


typedef struct odsy_mm_coresid_rgns {
	odsy_addr_rgn resid_busy; 	
	odsy_addr_rgn sw_wr;
} odsy_mm_coresid_rgns;


// Per Sharegroup
typedef struct odsy_mm_sg { 
        struct odsy_mm_as *as;              // Address space the sharegroup is attached to (process address space).
        struct odsy_mm    *mm;              // Pointer back to the main (per display) mm anchor.
	int                ref_cnt;         // When this goes to zero the last context has exitted the sharegroup.
	int                sg_hdl;          // This sg's id in the display's set of sharegroups.

	odsy_chunked_set   obj_addr_slot_set;// We allocate object pipe address communication slots from here.
	odsy_chunked_set   obj_addr_rgns;    // Set of address region chunks for the object address communication
	                                     // region(s).  Each element is * odsy_addr_rgn.

	odsy_chunked_set   grp_set;         // The set of object groups allocated within this sharegroup.
	odsy_chunked_set   obj_set;         // The set of objects allocated within this sharegroup.
	odsy_chunked_set   ctx_set;         // The set of graphics contexts attached to this sharegroup.
	odsy_chunked_set   dma_rgn_set;     // The set of outstanding dma operations wrt this sharegroup (not used yet)
} odsy_mm_sg;



#define ODSY_MM_FNCS_OBJ_STATE_NONE       0
#define ODSY_MM_FNCS_OBJ_STATE_RESERVED   1  
#define ODSY_MM_FNCS_OBJ_STATE_COMMITTED  2
#define ODSY_MM_FNCS_OBJ_STATE_VICTIMIZED 4 


#define ODSY_MM_FNCS_RESERVE_PROTO  (odsy_mm_obj *mmobj)
#define ODSY_MM_FNCS_COMMIT_PROTO   (odsy_mm_coresid_range *cores)
#define ODSY_MM_FNCS_VALIDATE_COMMIT_PROTO (struct odsy_mm *mm, struct odsy_ddrnode *gctx, struct odsy_ddrnode *ogctx)
#define ODSY_MM_FNCS_DECOMMIT_PROTO (odsy_mm_obj *mmobj)
#define ODSY_MM_FNCS_RELEASE_PROTO  (odsy_mm_obj *mmobj)


typedef struct odsy_mm_fncs 
{
	int (*reserve)         ODSY_MM_FNCS_RESERVE_PROTO;
	int (*commit)          ODSY_MM_FNCS_COMMIT_PROTO;
	int (*validate_commit) ODSY_MM_FNCS_VALIDATE_COMMIT_PROTO;
	int (*decommit)        ODSY_MM_FNCS_DECOMMIT_PROTO;
	int (*release)         ODSY_MM_FNCS_RELEASE_PROTO;
} odsy_mm_fncs;



/* per memory management space. */
typedef struct odsy_mm 
{
	struct odsy_data   *bdata;
	odsy_mm_hw_wr_slot *kv_hw_wr_rgn; 
	odsy_mm_global_rgn *kv_global_rgn;
	
	odsy_mm_obj         cfifo;
        odsy_mm_obj         nbfr;

        odsy_mm_obj         fbfr;
        odsy_mm_obj         wbfr;
	odsy_mm_obj         czbfr;  
        odsy_mm_obj         abfr[ODSY_MM_ACCUM_MAX_LEVELS];
	odsy_mm_obj_grp     screen_grp;


	odsy_chunked_set    sg_set; /* set of sharegroups */
	odsy_chunked_set    as_set; /* set of attached address spaces */
	
	int                 min_texmem_space;
	int                 min_pbuffer_space;

	/* virtual fn table and policy/strategy state hook.*/
	odsy_mm_fncs        *fncs;
	odsy_mm_fncs         _fncs;
	void                *hook;

	
} odsy_mm;


/* many ways to say there are 16 pixels in a tile ( in each of x&y ) */
#define ODSY_MM_TILE_DIM_BITS   4
#define ODSY_MM_TILE_DIM       (1<<(ODSY_MM_TILE_DIM_BITS))
#define ODSY_MM_TILE_DIM_MASK  ((ODSY_MM_TILE_DIM)-1)

#define ODSY_MM_MAX_DIM        65536  /*just in x*/

/* 
** number of whole (i.e. rounded up) tiles necessary
** to cover the given number of pixels.
*/
#define ODSY_MM_NR_TILES_FROM_PIXELS(nrpixels) \
       (((nrpixels) >> ODSY_MM_TILE_DIM_BITS) + ((nrpixels) & ODSY_MM_TILE_DIM_MASK ? 1 : 0))

#define ODSY_MM_32BPP_TILE_OFFSET_BITS (9)
#define ODSY_MM_32BPP_TILENR_FROM_ADDR(sdramaddr) ((sdramaddr)>>ODSY_MM_32BPP_TILE_OFFSET_BITS)
#define ODSY_MM_32BPP_ADDR_FROM_TILENR(tilenr)    ((tilenr)<<ODSY_MM_32BPP_TILE_OFFSET_BITS)



     /*
     ** looks silly, but we need to be evil at some point, so we can do it here once and for all
     */
#define ODSY_GFP_NO_SG_CHECK(sg)                                                  \
	if (!sg){                                                                 \
		cmn_err(CE_WARN,"odsy: context not attached to sharegroup\n");    \
		/* XXXkill this thread... */                                      \
		*rvalp = EFAULT;                                                  \
		return EFAULT;                                                    \
	}

#define ODSY_GFP_NOTIFY_CHECK(sg)                                                  \
	if (!sg || !sg->uv_resid_busy_rgn){                                        \
		cmn_err(CE_WARN,"odsy: context has not done NOTIFY\n");            \
		/* XXXkill this thread... */                                       \
		*rvalp = EPERM;                                                    \
		return EPERM;                                                      \
	}

#define ODSY_GFP_NOT_BOUND_CHECK(gctx)                                                        \
        if(!gctx){                                                                            \
		cmn_err(CE_WARN,"odsy: attempted sharegroup operation from unbound gfxp\n");  \
                 /* kill this thread ... */                                                   \
 		*rvalp = EPERM;                                                               \
		return EPERM;                                                                 \
	}



#define ODSY_MM_LOCK(gfxp,mm)  odsyMMlock(gfxp,mm)
#define ODSY_MM_UNLOCK(gfxp,mm) odsyMMunlock(gfxp,mm)



