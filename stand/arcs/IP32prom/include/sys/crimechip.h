#ifndef __GL_CRIMECHIP_H__
#define __GL_CRIMECHIP_H__
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
*/

/*
** This is the Crime Rendering Engine interface
**
** $Revision: 1.1 $
*/

#include "crimereg.h"

#ifdef EASY_TO_READ
typedef struct crimeregs {
    CrmIntfBufReg       Ib;		/*page0*/
    CrmTlbReg           Tlb;            /*page1*/
    CrmBufModeReg       BufMode;	/*page2*/
    CrmClipModeReg      ClipMode;
    CrmDrawModeReg      DrawMode;
    CrmClipRectReg      ScrMask[5];
    CrmClipRectReg      Scissor;
    CrmWinOffsetReg     WinOffset;
    CrmPrimitiveReg     Primitive;
    CrmVertexReg        Vertex;
    CrmStartSetupReg	StartSetup;
    CrmPixelXferReg     PixelXfer;
    CrmStippleReg       Stipple;
    CrmShadeReg         Shade;
    CrmTextureReg       Texture;
    CrmFogReg           Fog;
    CrmAntialiasReg     Antialias;
    CrmAlphaTestReg     AlphaTest;
    CrmBlendReg         Blend;
    CrmLogicOpReg       LogicOp;
    CrmColorMaskReg     ColorMask;
    CrmDepthReg         Depth;
    CrmStencilReg       Stencil;
    CrmPixPipeNullReg   PixPipeNull;
    CrmPixPipeFlushReg  PixPipeFlush;
    CrmMteReg           Mte;            /*page3*/
    CrmStatusReg	Status;		/*page4*/
    CrmSetStartPtrReg	SetStartPtr;
} Crimeregs;
#endif 

/*
**  register aligned Crime chip.
*/
#define align(regName,r0,r1,regType) \
	unsigned char __pad##regName [(r1) - (r0) - sizeof(regType)]


#define CRMDRAWREGS							\
    CrmBufModeReg       BufMode;					\
    CrmClipModeReg      ClipMode;	align(ClipMode,			\
					      CRM_CLIP_MODE_REG,	\
					      CRM_DRAW_MODE_REG,	\
					      CrmClipModeReg);		\
    CrmDrawModeReg      DrawMode;	align(DrawMode,			\
					      CRM_DRAW_MODE_REG,	\
					      CRM_SCRMASK0_REG,		\
					      CrmDrawModeReg);		\
    CrmClipRectReg      ScrMask[5];					\
    CrmClipRectReg      Scissor;					\
    CrmWinOffsetReg     WinOffset;					\
    CrmPrimitiveReg     Primitive;	align(Primitive,		\
					      CRM_PRIMITIVE_REG,	\
					      CRM_VERTEX_X_XY0_REG,	\
					      CrmPrimitiveReg);		\
    CrmVertexReg        Vertex;						\
    CrmStartSetupReg	StartSetup;	align(StartSetup,		\
					      CRM_START_SETUP_REG,	\
					      CRM_PIXELXFER_SRC_ADDR_REG,\
					      CrmStartSetupReg);	\
									\
    CrmPixelXferReg     PixelXfer;	align(PixelXfer,		\
                                              CRM_PIXELXFER_SRC_ADDR_REG,\
                                              CRM_STIPPLE_MODE_REG,	\
                                              CrmPixelXferReg);		\
    CrmStippleReg       Stipple;	align(Stipple,			\
					      CRM_STIPPLE_MODE_REG,	\
					      CRM_SHADE_FGCOLOR_REG,	\
					      CrmStippleReg);		\
    CrmShadeReg         Shade;						\
    CrmTextureReg       Texture;					\
    CrmFogReg           Fog;						\
    CrmAntialiasReg     Antialias;					\
    CrmAlphaTestReg     AlphaTest;	align(AlphaTest,		\
					      CRM_ALPHATEST_REG,	\
					      CRM_BLEND_CONSTCOLOR_REG,	\
					      CrmAlphaTestReg);		\
    CrmBlendReg         Blend;						\
    CrmLogicOpReg       LogicOp;	align(LogicOp,			\
					      CRM_LOGICOP_REG,		\
					      CRM_COLORMASK_REG,	\
					      CrmLogicOpReg);		\
    CrmColorMaskReg     ColorMask;	align(ColorMask,		\
					      CRM_COLORMASK_REG,	\
					      CRM_DEPTH_FUNC_REG,	\
					      CrmColorMaskReg);		\
    CrmDepthReg         Depth;						\
    CrmStencilReg       Stencil;	align(Stencil,			\
					      CRM_STENCIL_MODE_REG,	\
					      CRM_PIXPIPE_NULL_REG,	\
					      CrmStencilReg);		\
    CrmPixPipeNullReg	PixPipeNull;    align(PixPipeNull,		\
					      CRM_PIXPIPE_NULL_REG,	\
					      CRM_PIXPIPE_FLUSH_REG,	\
					      CrmPixPipeNullReg);	\
    CrmPixPipeFlushReg	PixPipeFlush

typedef struct crimechip {
  					/*page0*/

    CrmIntfBufReg	Ib;		align(Ib,
                                              CRM_INTFBUF_BASE,
                                              CRM_TLB_BASE,
                                              CrmIntfBufReg);

  					/*page1*/

    CrmTlbReg		Tlb;		align(Tlb, 
					      CRM_TLB_BASE,
					      CRM_PIXPIPE_BASE, 
					      CrmTlbReg);

  					/*page2*/
    CRMDRAWREGS;
					align(PixPipeFlush,
					      CRM_PIXPIPE_FLUSH_REG,
					      CRM_PAGE_SIZE,
					      CrmPixPipeFlushReg);

					/*page3*/
	
    CrmMteReg		Mte;		align(Mte, 0, 
					      CRM_PAGE_SIZE,
				 	      CrmMteReg);
					/*page4*/

					#undef Status
    CrmStatusReg	Status;		align(Status, 0,
                                              SET_STARTPTR_REG,
                                              CrmStatusReg);
    CrmSetStartPtrReg	SetStartPtr;

} Crimechip;



typedef struct crmshadowregs {
    CRMDRAWREGS;
					align(PixPipeFlush,
					      CRM_PIXPIPE_FLUSH_REG,
					      CRM_PIXPIPE_FLUSH_REG + 8,
					      CrmPixPipeFlushReg);
    CrmMteReg		Mte;

} CrmShadowRegs;
typedef CrmShadowRegs CrimeShadow;

#undef align
typedef int Status;

#endif /* __GL_CRIMECHIP_H__ */
