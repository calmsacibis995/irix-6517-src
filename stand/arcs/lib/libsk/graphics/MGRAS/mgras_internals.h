#ifndef __MGRAS_INTERNALS_H__
#define __MGRAS_INTERNALS_H__

/*
** Copyright 1993, Silicon Graphics, Inc.
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
** $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/graphics/MGRAS/RCS/mgras_internals.h,v 1.10 1996/12/17 08:27:39 spalding Exp $
*/

#include <sys/gfx.h>
#include <sys/mgras.h>
#include <sys/reg.h>

/**************************************************************************
 * Constants
 **************************************************************************/

#define MGRAS_MAPSIZE	        (sizeof(mgras_hw))
                                                   /* 0x100000 */
#define MGRAS_KSPACE_OFFSET     (0x000000)
#define MGRAS_DSPACE_OFFSET     (0x060000)
#define MGRAS_USPACE_OFFSET     (0x070000)
#define MGRAS_TOP_OFFSET        (0x100000)
#define MGRAS_KSPACE_MAPSIZE    (MGRAS_DSPACE_OFFSET - MGRAS_KSPACE_OFFSET)
#define MGRAS_DSPACE_MAPSIZE    (MGRAS_USPACE_OFFSET - MGRAS_DSPACE_OFFSET)
#define MGRAS_USPACE_MAPSIZE    (MGRAS_TOP_OFFSET - MGRAS_USPACE_OFFSET)
#define MGRAS_USER_MAP_BASE     MGRAS_USPACE_OFFSET
#define MGRAS_USER_MAP_SIZE     MGRAS_USPACE_MAPSIZE
#define MGRAS_MGR_MAP_BASE      MGRAS_DSPACE_OFFSET
#define MGRAS_MGR_MAP_SIZE      (MGRAS_DSPACE_MAPSIZE + MGRAS_USPACE_MAPSIZE)

#define MGRAS_MAXBOARDS		2           /* max. # MGRAS boards in system */
#define MGRAS_DCB_SIZE	        0x400


/* XXX usec to wait for some tokens to drain after GFX_DELAY asserted */	
#define HQ3_FIFO_POLL		50	

#define HQ3_CFIFO_VC_POLL	1000
#define HQ3_CFIFO_ER_POLL	1000
#define HQ3_CFIFO_EW_POLL	1000
#define HQ3_CFIFO_EV_POLL	1000

#define HQ3_CFIFO_TIMEOUT		(15*HZ)		/* 15 sec */
#define HQ3_CTXSW_TIMEOUT		(15*HZ)

#define MGRAS_OP_SWAPBUF       BIT(0)
#define MGRAS_OP_MODE          BIT(1)
#define MGRAS_OP_NOTIFY_MGR    BIT(4)
#define MGRAS_OP_ALLOW_OPS     BIT(5)

/* Used to signal MgrasUnSchedSwapTask to search for an RN */
#define MGRAS_SEARCH_FOR_RN	(-17)

#define HQ3_DMAPAGESIZE		(1 << 12)

#ifdef DEBUG
#define	MGRAS_TRACE(a, b, c, d)	mgras_trace(a, b, c, d)
# define TRACE_CREATE	0x100
# define TRACE_DESTROY	0x200
# define TRACE_MAP	0x300
# define TRACE_UNMAP	0x400
# define TRACE_SWAP	0x500	/* near beginning of PcxSwap */
# define TRACE_SWAP_H1	0x501	/* heavy-wt gl read */
# define TRACE_SWAP_H2	0x502	/* heavy-wt gl write */
# define TRACE_SWAP_H3	0x503	/* heavy-wt er read */
# define TRACE_SWAP_H4	0x504	/* heavy-wt er write */
# define TRACE_SWAP_H5	0x505
# define TRACE_SWAP_H6	0x506
# define TRACE_SWAP_H7	0x507
# define TRACE_SWAP_H8	0x508	/* wait for cfifo to drain in irnode */
# define TRACE_CTXSW_0	0x600
# define TRACE_CTXSW_1	0x601
# define TRACE_CTXSW_2	0x602
# define TRACE_FLUSH_START		0x701
# define TRACE_FLUSH_PRE_POLL		0x702
# define TRACE_FLUSH_BLOCK_START	0x703
# define TRACE_FLUSH_BLOCK_DONE		0x704
# define TRACE_FLUSH_POST_POLL		0x705
# define TRACE_FLUSH_FAIL		0x706
# define TRACE_DMA_MAY_BE_IN_PROGRESS	0x800
# define TRACE_DMA_IN_PROGRESS		0x801
# define TRACE_DMA_DONE			0x802
# define TRACE_DMA_NOT_YET_STARTED	0x810
# define TRACE_BUFSWAP_REQUESTED	0x900
# define TRACE_BUFSWAP_BANK_VALID	0x901
# define TRACE_BUFSWAP_FINISH_STARTED	0x902
# define TRACE_BUFSWAP_FINISH_DONE	0x903
# define TRACE_BUFSWAP_SCHEDULED	0x904
# define TRACE_BUFSWAP_RETRACE		0x905
# define TRACE_BUFSWAP_SWAPPED		0x906
#else 
#define MGRAS_TRACE(a, b, c, d)
#endif /* DEBUG */
/**************************************************************************
 * Types
 **************************************************************************/

typedef enum {
  NONE                 = 0,
  SWAPBUF              = MGRAS_OP_SWAPBUF,
  MGR_SWAPBUF          = (MGRAS_OP_SWAPBUF | MGRAS_OP_NOTIFY_MGR),
  MODE                 = MGRAS_OP_MODE,
  MGR_SWAPBUF_AND_MODE = (MGRAS_OP_SWAPBUF | MGRAS_OP_NOTIFY_MGR | MGRAS_OP_MODE)
} mgras_swaptask_t;
/**************************************************************************
 * Global Variables
 **************************************************************************/

extern mgrasboards MgrasBoards[];	    /* array of bds found @ earlyinit */
extern mgras_info MgrasInfos[MGRAS_MAXBOARDS]; /* info about the boards */
extern struct gfx_fncs mgras_gfx_fncs;	    /* dispatch table for gfx fcns */
extern struct mgras_timing_info *mgras_video_timing[]; /* timing tbls */
extern struct htp_fncs mgras_htp_fncs;

/**************************************************************************
 * Prototypes
 **************************************************************************/

extern void mgras_trace(int a, int b, int c, int d);

#ifndef _STANDALONE
extern void MgrasFIFOInterrupt(int index, struct eframe_s *ep);
extern void MgrasGeneralInterrupt(int index, struct eframe_s *ep);
#endif
extern void MgrasBlankScreen(mgras_hw *base,int blank);
void mgrasInitBoard(mgras_hw *, int);
extern void MgrasInitCCR(mgras_hw *base, mgras_info *info);

extern void MgrasLoadVC3SRAM(register mgras_hw *base,
			     unsigned short *data, 
			     unsigned int addr,
			     unsigned int length);
extern void MgrasClearCursor(register mgras_hw *base);
extern void MgrasInitCursor(register mgras_hw *base);
extern void MgrasEnableRetraceInterrupts(mgras_hw *base);
extern void MgrasEnableVideoTiming(mgras_hw *base, int onoff);

extern void MgrasInitIntrVectors(void);

extern int MgrasCtxsw(struct mgras_data *,
		      struct mgras_rnode *,
		      struct mgras_rnode *);
#ifndef _STANDALONE
#if HQ4
extern void mgras_hq4_hiwater_intr(mgras_data *);
extern void mgras_hq4_lowater_intr(mgras_data *);
extern void MgrasGeneralInterrupt(mgras_data *);
extern void MgrasRetraceHandler(register mgras_data *);
#else
extern void MgrasFIFOInterrupt(__psint_t, struct eframe_s *);
extern void MgrasGeneralInterrupt(__psint_t, struct eframe_s *);
extern void MgrasRetraceInterrupt(__psint_t, struct eframe_s *);
#endif
#endif
extern void MgrasRetraceInit(mgras_data *);
extern int MgrasSchedSwapTask(struct gfx_data   *gbd,
			      mgras_swaptask_t   task,
			      int                wid,
			      struct rrm_rnode  *rnp,
			      int                buffer,
			      int                mode,
			      int                interval);
extern int MgrasUnSchedSwapTask(struct gfx_data   *gbd,
				int                wid,
				struct rrm_rnode  *rnp);
extern int MgrasSchedRetraceEvent(struct gfx_data  *gbd,
				  struct rrm_rnode *rnp);
extern int MgrasUnSchedRetraceEvent(struct gfx_data  *gbd, 
				    struct rrm_rnode *rnp);
extern int MgrasSchedMGRSwapValidation(register struct gfx_gfx *,
				       register struct RRM_MGR_SwapBuf *ap);
extern int MgrasValidateMGRSwaps(struct gfx_data *);

extern void MgrasInitGblTexMgr(mgras_data *bdata);
extern int MgrasAddTexSpace(struct gfx_gfx *gfxp, struct mgras_rnode *rnp,
			    mgras_texspace_args *args);
extern void MgrasDeleteTexSpace(struct gfx_gfx *gfxp,
				struct mgras_rnode *rnp,
				mgras_texspace_args *args);
extern int MgrasSaveOverlap(MgrasGMTexDesc_t *desc,
			    mgras_data *bdata,
			    mgras_rnode *rnp);
extern int MgrasRestoreOverlap(mgras_data *bdata, mgras_rnode *in_rnodep,
			       mgras_rnode *intermed_ctx, mgras_rnode *cur_ctx);
extern int MgrasUserSaveTexData(struct gfx_gfx *gfxp,
				struct mgras_texture_read_args *a);
extern int MgrasSaveTexData(mgras_data *bdata,
			    mgras_rnode *savernp,
			    unsigned short startpageidx);
extern int MgrasRestoreTexData(mgras_data *bdata,
			       mgras_rnode *restorernp,
			       unsigned short startpageidx);
extern void DeriveInfo(mgras_hw *base,
		       long code,
		       long vx2,
		       MgrasGMTexDesc_t *desc,
		       int numtrams);
extern void MgrasCleanTexBdata(mgras_data *bdata);
extern void MgrasFinish(mgras_hw *hwcx);
extern void _MgrasWriteTexBase(mgras_hw *base, __uint32_t addr);

extern	int mgras_baseindex(mgras_hw *);
extern int mgras_fp_reinit( mgras_data * );
/**************************************************************************
 * flow-control setup/undo macros
 **************************************************************************/

#if !HQ4
# define SET_FC()
# define UNSET_FC()
#else
# define SET_FC()	gfx_flow_attach_me(bdata->flow_hdl)
# define UNSET_FC()	gfx_flow_detach_me(bdata->flow_hdl)
#endif
/*
 * FC prototypes
 */
void Mgras_FC_Disable(void);

/*
 * XXX Correct values TBD.  Depends on video timing
 */
#define CURSOR_XOFF 29
#define CURSOR_YOFF 31

#endif  /* __MGRAS_INTERNALS_H__ */
