#ifndef __GL_CRIMEREG_H__
#define __GL_CRIMEREG_H__
/*
**
** crimereg.h - CRIME chip interface registers
**
*/

#include "crimedef.h"

#if !defined(_KERNEL) && !defined(X11R6) && !defined(_STANDALONE)
#define CRMREG_BITFIELDS
#endif

/*****************************************/
/* Some useful types */

#ifdef EASY_TO_READ
typedef struct {
    u_long r:8;
    u_long g:8;
    u_long b:8;
    u_long a:8;
} ColorType;
#endif

typedef u_long ColorType;

#ifdef CRMREG_BITFIELDS
typedef struct {
    int x:16;
    int y:16;
} CrmFbAddrType; 
#else
typedef u_long CrmFbAddrType;
#endif

#ifdef CRMREG_BITFIELDS
typedef union {
    CrmFbAddrType fb;
    u_long linear;
} CrmVaddrType;
#else
typedef u_long CrmVaddrType;
#endif


/*****************************************/
/* Programming interface registers */
/*****************************************/


/***********************************************/
/* TLB register typedefs */
/*****************************************/

typedef union {
    u_short	taddr[4];
    u_long	laddr[2];
    long long	dw;
} CrmTlbType;
 
typedef struct { 
    CrmTlbType	fbA[64]; 
    CrmTlbType	fbB[64]; 
    CrmTlbType	fbC[64];
    CrmTlbType	tex[28];
    CrmTlbType	cid[4];
    CrmTlbType	linearA[16];
    CrmTlbType	linearB[16];
} CrmTlbReg;


/***********************************************/
/* Interface buffer register typedefs */
/*****************************************/

typedef struct {
    u_long w[2];
} CrmIntfBufData;

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved1:18;
    u_long start:1;
    u_long wmask:2;
    u_long pageId:3;
    u_long offset:8;
    u_long reserved2;
} CrmIntfBufAddr;

typedef struct {
    u_long reserved:4;
    u_long fullLevel:7;
    u_long emptyLevel:7;
    u_long stallLevel:7;
    u_long stallCount:7;
} CrmIntfBufCtlReg;

#else
typedef unsigned long long CrmIntfBufAddr;

#define CRMIBADDR_OFFSET(s) (((s) >> 32) & 0xff)
#define CRMIBADDR_PAGE(s) (((s) >> 40) & 7)
#define CRMIBADDR_WMASK(s) (((s) >> 43) & 3)
#define CRMIBADDR_START(s) (((s) >> 45) & 1)
#define CRMIBADDR_TO_PHYS(s) \
	(CRMIBADDR_PAGE(s) << 12 | CRMIBADDR_OFFSET(s) << 3)

typedef unsigned long CrmIntfBufCtlReg;

#define CRMIBCTL_FULL_MASK  ((1<<7)-1)
#define CRMIBCTL_FULL_SHIFT 21
#define CRMIBCTL_FULL(s) \
	(((s) >> CRMIBCTL_FULL_SHIFT) & CRMIBCTL_FULL_MASK)

#define CRMIBCTL_EMPTY_MASK  ((1<<7)-1)
#define CRMIBCTL_EMPTY_SHIFT 14
#define CRMIBCTL_EMPTY(s) \
	(((s) >> CRMIBCTL_EMPTY_SHIFT) & CRMIBCTL_EMPTY_MASK)

#define CRMIBCTL_STALL_LEVEL_MASK  ((1<<7)-1)
#define CRMIBCTL_STALL_LEVEL_SHIFT 7
#define CRMIBCTL_STALL_LEVEL(s) \
	(((s) >> CRMIBCTL_STALL_LEVEL_SHIFT) & CRMIBCTL_STALL_LEVEL_MASK)

#define CRMIBCTL_STALL_COUNT_MASK  ((1<<7)-1)
#define CRMIBCTL_STALL_COUNT_SHIFT 0
#define CRMIBCTL_STALL_COUNT(s) \
	(((s) >> CRMIBCTL_STALL_COUNT_SHIFT) & CRMIBCTL_STALL_COUNT_MASK)

#endif

#define CRIME_FIFO_DEPTH 64
typedef struct {
    CrmIntfBufData 	data[CRIME_FIFO_DEPTH];
    CrmIntfBufAddr 	addr[CRIME_FIFO_DEPTH];
    CrmIntfBufCtlReg 	ctl;
    u_long              reserved1;
    u_long              reset;
    u_long              reserved2;
} CrmIntfBufReg;

/***********************************************/
/* Pixel pipe register typedefs */
/*****************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved:19;
    u_long bufType:3;
    u_long bufDepth:2;
    u_long pixType:4;
    u_long pixDepth:2;
    u_long doublePix:1;
    u_long doublePixSel:1;
} CrmBufModeType;
#else
typedef u_long CrmBufModeType;
#endif
typedef struct {
    CrmBufModeType src;
    int align1;
    CrmBufModeType dst;
    int align2;
} CrmBufModeReg;


/*****************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved:20;
    u_long enCid:1;
    u_long cidMapSel:1;
    u_long enScrMask:5;
    u_long scrMaskMode:5;
} CrmClipModeReg;

#else
typedef u_long CrmClipModeReg;
#endif

/*****************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved:8;
    u_long enNoConflict:1;
    u_long enGL:1;
    u_long enPixelXfer:1;
    u_long enScissorTest:1;
    u_long enLineStipple:1;
    u_long enPolyStipple:1;
    u_long enOpaqStipple:1;
    u_long enShade:1;
    u_long enTexture:1;
    u_long enFog:1;
    u_long enCoverage:1;
    u_long enAntialiasLine:1;
    u_long enAlphaTest:1;
    u_long enBlend:1;
    u_long enLogicOp:1;
    u_long enDither:1;
    u_long enColorMask:1;
    u_long enColorByteMask:4;
    u_long enDepthTest:1;
    u_long enDepthMask:1;
    u_long enStencilTest:1;
} CrmDrawModeReg;

#else
typedef u_long CrmDrawModeReg;
#endif

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    CrmFbAddrType min;
    CrmFbAddrType max;
} CrmClipRectReg;
#else
typedef unsigned long long CrmClipRectReg;
#endif
/*****************************************/

typedef struct {
    CrmFbAddrType src;
    int align1;
    CrmFbAddrType dst;
    int align2;
} CrmWinOffsetReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long opCode:8;
    u_long reserved:5;
    u_long lineSkipLastEP:1;
    u_long edgeType:2;
    u_long lineWidth:16;
} CrmPrimitiveReg;

#else
typedef u_long CrmPrimitiveReg;
#endif

/*****************************************/

typedef CrmFbAddrType CrmVertexXReg;

typedef struct {
    u_long x;
    u_long y;
} CrmVertexGLReg;

typedef struct {
    CrmVertexXReg  X[3];
    int	align;
    CrmVertexGLReg GL[3];
} CrmVertexReg;
/*****************************************/

typedef u_long CrmStartSetupReg;

/*****************************************/

typedef struct {
    CrmVaddrType addr;
    int align1;
    int xStep;
    int yStep;
} CrmPixelXferSrcReg;

typedef struct {
    int linAddr;
    int linStride;
} CrmPixelXferDstReg;

typedef struct {
    CrmPixelXferSrcReg src;
    CrmPixelXferDstReg dst;
} CrmPixelXferReg;


/*****************************************/

#ifdef EASY_TO_READ
typedef struct {
    u_long index:8;
    u_long maxIndex:8;
    u_long repeatCnt:8;
    u_long maxRepeat:8;
} CrmStippleModeReg;
#endif
typedef u_long CrmStippleModeReg;

typedef struct {
    CrmStippleModeReg mode;
    u_long pattern;
} CrmStippleReg;

/*****************************************/

typedef struct {
    ColorType fgColor;
    int align1;

    ColorType bgColor;
    int align2;

    int r0;
    int g0;
    int b0;
    int a0;
    int drdx;
    int dgdx;
    int drdy;
    int dgdy;
    int dbdx;
    int dadx;
    int dbdy;
    int dady;
} CrmShadeReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long reserved:7;
    u_long tiled:1;
    u_long texelType:4;
    u_long texelDepth:2;
    u_long mapLevel:4;
    u_long maxLevel:4;
    u_long minFilter:3;
    u_long magFilter:1;
    u_long wrapS:2;
    u_long wrapT:2;
    u_long func:2;
} CrmTextureModeReg;
#else
typedef u_long CrmTextureModeReg;
#endif

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long reserved:16;
    u_long uShift:4;
    u_long vShift:4;
    u_long uWrapShift:4;
    u_long vWrapShift:4;
    u_long uIndexMask:16;
    u_long vIndexMask:16;
} CrmTextureFormatReg;
#else
typedef unsigned long long CrmTextureFormatReg;
#endif

typedef struct {
    CrmTextureModeReg 	mode;
    int align1;

    CrmTextureFormatReg format;

    long long 	sq0;
    long long 	tq0;
    int 	q0;
    int 	stShift;
    long long 	dsqdx;
    long long 	dsqdy;
    long long 	dtqdx;
    long long 	dtqdy;
    int 	dqdx;
    int 	dqdy;

    ColorType borderColor;
    int align2;

    ColorType envColor;
    int align3;
} CrmTextureReg;

/*****************************************/

typedef struct {
    ColorType color;
    int align1;

    int f0;
    int align2;
    int dfdx;
    int align3;
    int dfdy;
    int align4;
} CrmFogReg;

/*****************************************/

typedef struct {
    int slope:16;
    int ideal:16;
} CrmAntialiasLineReg;

typedef struct {
    u_long reserved:16;
    u_long start:8;
    u_long end:8;
} CrmAntialiasCovReg;

typedef struct {
    CrmAntialiasLineReg line;
    CrmAntialiasCovReg  cov;
} CrmAntialiasReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long reserved:20;
    u_long func:4;
    u_long ref:8;
} CrmAlphaTestReg;
#else
typedef u_long CrmAlphaTestReg;
#endif

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long reserved:20;
    u_long op:4;
    u_long src:4;
    u_long dst:4;
} CrmBlendFuncReg;
#else
typedef u_long CrmBlendFuncReg;
#endif

typedef struct {
    ColorType constColor;
    int align1;
    CrmBlendFuncReg func;
    int align2;
} CrmBlendReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long reserved:28;
    u_long op:4;
} CrmLogicOpReg;
#else
typedef u_long CrmLogicOpReg;
#endif

/*****************************************/

typedef u_long CrmColorMaskReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct { 
    u_long reserved:4;
    u_long func:3;
    u_long enTagClear:1;
    u_long clear:24;
} CrmDepthMode;
#else
typedef u_long CrmDepthMode;
#endif

typedef struct {
    CrmDepthMode mode;
    int align1;

    long long z0;
    long long dzdx;
    long long dzdy;
} CrmDepthReg;

/*****************************************/

#ifdef CRMREG_BITFIELDS
typedef struct {
    u_long ref:8;
    u_long mask:8;
    u_long func:4;
    u_long sfail:4;
    u_long dpfail:4;
    u_long dppass:4;
} CrmStencilModeReg;
#else
typedef u_long CrmStencilModeReg;
#endif

typedef struct { 
    CrmStencilModeReg mode;
    int align;
    u_long mask;
} CrmStencilReg;

/*****************************************/

typedef u_long CrmPixPipeNullReg;
typedef u_long CrmPixPipeFlushReg;

/*****************************************/
#ifdef EASY_TO_READ
typedef struct { 
    CrmBufModeReg   	BufMode;
    CrmClipModeReg     	ClipMode;
    CrmDrawModeReg     	DrawMode;
    CrmClipRectReg      ScrMask[5];
    CrmClipRectReg      Scissor;
    CrmWinOffsetReg     WinOffset;
    CrmPrimitiveReg     Primitive;
    CrmVertexReg     	Vertex;
    CrmPixelXferReg     PixelXfer;
    CrmStippleReg      	Stipple;
    CrmShadeReg        	Shade;
    CrmTextureReg      	Texture;
    CrmFogReg          	Fog;
    CrmAntialiasReg     Antialias;
    CrmAlphaTestReg    	AlphaTest;
    CrmBlendReg        	Blend;
    CrmLogicOpReg      	LogicOp;
    CrmColorMaskReg    	ColorMask;
    CrmDepthReg        	Depth;
    CrmStencilReg      	Stencil;
} CrmDrawReg;
#endif

/*****************************************/
/* MTE register typedefs */
/*****************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved:20;
    u_long opCode:1;
    u_long enStipple:1;
    u_long pixDepth:2;
    u_long srcBufType:3;
    u_long dstBufType:3;
    u_long srcECC:1;
    u_long dstECC:1;
} CrmMteModeReg;

#else
typedef u_long CrmMteModeReg;
#endif

typedef struct {
    CrmMteModeReg       mode;
    int align1;
    u_long		byteMask;
    int align2;
    u_long		stippleMask;
    int align3;
    u_long		fgValue;
    int align4;
    CrmVaddrType        src0;
    int align5;
    CrmVaddrType        src1;
    int align6;
    CrmVaddrType        dst0;
    int align7;
    CrmVaddrType        dst1;
    int align8;
    int        		srcYStep;
    int align9;
    int        		dstYStep;
    int align10[9];
    int			null;
    int align11;
    int			flush;
} CrmMteReg;
/***********************************************/
/* Status registers */
/***********************************************/

#ifdef CRMREG_BITFIELDS

typedef struct {
    u_long reserved:3;
    u_long reIdle:1;
    u_long setupIdle:1;
    u_long pixPipeIdle:1;
    u_long mteIdle:1;
    u_long intfBufLevel:7;
    u_long intfBufRdPtr:6;
    u_long intfBufWrPtr:6;
    u_long intfBufStartPtr:6;
} CrmStatusReg;

#else

typedef unsigned long CrmStatusReg;

#define CRMSTAT_RE_IDLE		(1 << 28)

#define CRMSTAT_IB_LEVEL_SHIFT  18
#define CRMSTAT_IB_LEVEL_MASK	0x7f
#define CRMSTAT_IB_LEVEL(s)	(((s) >> CRMSTAT_IB_LEVEL_SHIFT) &\
					 CRMSTAT_IB_RDPTR_MASK)
#define CRMSTAT_IB_RDPTR_SHIFT  12
#define CRMSTAT_IB_RDPTR_MASK	0x3f
#define CRMSTAT_IB_RDPTR(s)	(((s) >> CRMSTAT_IB_RDPTR_SHIFT) &\
					 CRMSTAT_IB_RDPTR_MASK)
#define CRMSTAT_IB_WRPTR_SHIFT  6
#define CRMSTAT_IB_WRPTR_MASK	0x3f
#define CRMSTAT_IB_WRPTR(s)	(((s) >> CRMSTAT_IB_WRPTR_SHIFT) &\
					 CRMSTAT_IB_WRPTR_MASK)
#define CRMSTAT_IB_STPTR_SHIFT  0
#define CRMSTAT_IB_STPTR_MASK	0x3f
#define CRMSTAT_IB_STPTR(s)	(((s) >> CRMSTAT_IB_STPTR_SHIFT) &\
					 CRMSTAT_IB_STPTR_MASK)
#endif

typedef unsigned long CrmSetStartPtrReg;

#endif /* __GL_CRIMEREG_H__ */
