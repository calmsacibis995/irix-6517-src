#ifndef __GL_CRIMEDEF_H__
#define __GL_CRIMEDEF_H__
/*
**
** crimedef.h - CRIME chip interface defines 
**
*/

#ifdef BIT
#undef BIT
#endif
#define BIT(n) 	(1 << n)
/*
** Chip register settings
*/
/* Buffer types */
#define CRM_BUF_FB_A			0
#define CRM_BUF_FB_B			1
#define CRM_BUF_FB_C			2
#define CRM_BUF_TEX 			3
#define CRM_BUF_LINEAR_A		4
#define CRM_BUF_LINEAR_B		5
#define CRM_BUF_CID			6

/* Buffer depth values */
#define CRM_BUF_DEPTH_8                 0
#define CRM_BUF_DEPTH_16                1
#define CRM_BUF_DEPTH_32                2

/* Pixel types */
#define CRM_PIXEL_COLOR_INDEX           0
#define CRM_PIXEL_RGB                   1
#define CRM_PIXEL_RGBA                  2
#define CRM_PIXEL_ABGR                  3
#define CRM_PIXEL_YCRCB                15 

/* Pixel depth values */
#define CRM_PIXEL_DEPTH_8		0
#define CRM_PIXEL_DEPTH_16		1
#define CRM_PIXEL_DEPTH_32		2

/* doublePix selection */	
#define CRM_PIXSEL_LOWER		0
#define CRM_PIXSEL_UPPER		1

/* Texel types */
#define CRM_TEXEL_RGB                   1
#define CRM_TEXEL_RGBA                  2
#define CRM_TEXEL_RGBA4                 4
#define CRM_TEXEL_ALPHA			5
#define CRM_TEXEL_INTENSITY             6
#define CRM_TEXEL_LUMINANCE             7
#define CRM_TEXEL_LUMINANCE_ALPHA       8

/* Texel depth values */
#define CRM_TEXEL_DEPTH_8		0
#define CRM_TEXEL_DEPTH_16		1
#define CRM_TEXEL_DEPTH_32		2

/* Texel 16-bit formats */	
#define CRM_TEXEL_4444			0
#define CRM_TEXEL_1555			1

/* Drawing op codes */
#define CRM_OPCODE_POINT		0
#define CRM_OPCODE_LINE			1
#define CRM_OPCODE_TRI			2
#define CRM_OPCODE_RECT			3
#define CRM_OPCODE_FLUSH		4

/* MTE op codes */
#define CRM_MTE_CLEAR			0
#define CRM_MTE_COPY			1

/* Edge type */
#define	CRM_EDGE_LR_BT			0
#define	CRM_EDGE_RL_BT			1
#define	CRM_EDGE_LR_TB			2
#define	CRM_EDGE_RL_TB			3

/* Texel generation constants */
#define CRM_TEXTURE_CLAMP 		0
#define CRM_TEXTURE_REPEAT 		1
#define CRM_TEXTURE_CLAMP_TO_EDGE	2
#define CRM_TEXTURE_CLAMP_TO_BORDER	3

#define CRM_TEXTURE_NEAREST			0
#define CRM_TEXTURE_LINEAR			1
#define CRM_TEXTURE_NEAREST_MIPMAP_NEAREST	2
#define CRM_TEXTURE_LINEAR_MIPMAP_NEAREST	3
#define CRM_TEXTURE_NEAREST_MIPMAP_LINEAR	4
#define CRM_TEXTURE_LINEAR_MIPMAP_LINEAR	5

#define CRM_TEXTURE_MAX_LOD		10

#define CRM_TEXTURE_1D			0
#define CRM_TEXTURE_2D			1

/* Texture blend constants */
#define CRM_TEXTURE_FUNC_MODULATE 	0
#define CRM_TEXTURE_FUNC_DECAL		1
#define CRM_TEXTURE_FUNC_BLEND		2
#define CRM_TEXTURE_FUNC_REPLACE	3

/* Blend src functions */
#define CRM_BFS_ZERO				0x0
#define CRM_BFS_ONE				0x1
#define CRM_BFS_DST_COLOR			0x2
#define CRM_BFS_ONE_MINUS_DST_COLOR		0x3
#define CRM_BFS_SRC_ALPHA			0x4
#define CRM_BFS_ONE_MINUS_SRC_ALPHA		0x5
#define CRM_BFS_DST_ALPHA			0x6
#define CRM_BFS_ONE_MINUS_DST_ALPHA		0x7
#define CRM_BFS_CONSTANT_COLOR_EXT              0x8
#define CRM_BFS_ONE_MINUS_CONSTANT_COLOR_EXT    0x9
#define CRM_BFS_CONSTANT_ALPHA_EXT              0xa
#define CRM_BFS_ONE_MINUS_CONSTANT_ALPHA_EXT    0xb
#define CRM_BFS_SRC_ALPHA_SATURATE              0xc

/* Blend dst functions */
#define CRM_BFD_ZERO				0x0
#define CRM_BFD_ONE				0x1
#define CRM_BFD_SRC_COLOR			0x2
#define CRM_BFD_ONE_MINUS_SRC_COLOR		0x3
#define CRM_BFD_SRC_ALPHA			0x4
#define CRM_BFD_ONE_MINUS_SRC_ALPHA		0x5
#define CRM_BFD_DST_ALPHA			0x6
#define CRM_BFD_ONE_MINUS_DST_ALPHA		0x7
#define CRM_BFD_CONSTANT_COLOR_EXT              0x8
#define CRM_BFD_ONE_MINUS_CONSTANT_COLOR_EXT    0x9
#define CRM_BFD_CONSTANT_ALPHA_EXT              0xa
#define CRM_BFD_ONE_MINUS_CONSTANT_ALPHA_EXT    0xb

/* Blending operators */
#define CRM_BOP_ADD			0
#define CRM_BOP_MIN_EXT			1
#define CRM_BOP_MAX_EXT			2
#define CRM_BOP_SUBTRACT_EXT		3
#define CRM_BOP_REVERSE_SUBTRACT_EXT	4

/* Logic ops */
#define CRM_LOGICOP_ZERO		0
#define CRM_LOGICOP_AND			1
#define CRM_LOGICOP_AND_REVERSE		2
#define CRM_LOGICOP_COPY		3
#define CRM_LOGICOP_AND_INVERTED	4
#define CRM_LOGICOP_NOOP		5
#define CRM_LOGICOP_XOR			6
#define CRM_LOGICOP_OR			7
#define CRM_LOGICOP_NOR			8
#define CRM_LOGICOP_EQUIV		9
#define CRM_LOGICOP_INVERT		10
#define CRM_LOGICOP_OR_REVERSE		11
#define CRM_LOGICOP_COPY_INVERTED	12
#define CRM_LOGICOP_OR_INVERTED		13
#define CRM_LOGICOP_NAND		14
#define CRM_LOGICOP_SET			15

/* Stencil ops */
#define CRM_STENCIL_OP_KEEP		0
#define CRM_STENCIL_OP_ZERO		1
#define CRM_STENCIL_OP_REPLACE		2
#define CRM_STENCIL_OP_INCR		3
#define CRM_STENCIL_OP_DECR		4
#define CRM_STENCIL_OP_INVERT		5

#define NUM_SCREEN_MASKS                5

/* BufMode bits shift */
#define BM_DOUBLE_PIX_SEL       BIT(0)
#define BM_DOUBLE_PIX           BIT(1)
#define BM_PIX_DEPTH_SHIFT      2
#define BM_PIX_DEPTH_MASK       (3 << BM_PIX_DEPTH_SHIFT)
#define BM_PIX_DEPTH_8          (CRM_PIXEL_DEPTH_8 << BM_PIX_DEPTH_SHIFT)
#define BM_PIX_DEPTH_16         (CRM_PIXEL_DEPTH_16 << BM_PIX_DEPTH_SHIFT)
#define BM_PIX_DEPTH_32         (CRM_PIXEL_DEPTH_32 << BM_PIX_DEPTH_SHIFT)
#define BM_PIX_TYPE_SHIFT       4
#define BM_PIX_TYPE_MASK        (3 << BM_PIX_TYPE_SHIFT)
#define BM_COLOR_INDEX          (CRM_PIXEL_COLOR_INDEX << BM_PIX_TYPE_SHIFT)
#define BM_RGB                  (CRM_PIXEL_RGB << BM_PIX_TYPE_SHIFT)
#define BM_RGBA                 (CRM_PIXEL_RGBA << BM_PIX_TYPE_SHIFT)
#define BM_ABGR                 (CRM_PIXEL_ABGR << BM_PIX_TYPE_SHIFT)
#define BM_BUF_DEPTH_SHIFT      8
#define BM_BUF_DEPTH_MASK       (3 << BM_BUF_DEPTH_SHIFT)
#define BM_BUF_DEPTH_8          (CRM_PIXEL_DEPTH_8 << BM_BUF_DEPTH_SHIFT)
#define BM_BUF_DEPTH_16         (CRM_PIXEL_DEPTH_16 << BM_BUF_DEPTH_SHIFT)
#define BM_BUF_DEPTH_32         (CRM_PIXEL_DEPTH_32 << BM_BUF_DEPTH_SHIFT)
#define BM_BUF_TYPE_SHIFT       10
#define BM_BUF_TYPE_MASK        (7 << BM_BUF_TYPE_SHIFT)
#define BM_FB_A                 (CRM_BUF_FB_A << BM_BUF_TYPE_SHIFT)
#define BM_FB_B                 (CRM_BUF_FB_B << BM_BUF_TYPE_SHIFT)
#define BM_FB_C                 (CRM_BUF_FB_C << BM_BUF_TYPE_SHIFT)
#define BM_TEX                  (CRM_BUF_TEX << BM_BUF_TYPE_SHIFT)
#define BM_LINEAR_A             (CRM_BUF_LINEAR_A << BM_BUF_TYPE_SHIFT)
#define BM_LINEAR_B             (CRM_BUF_LINEAR_B << BM_BUF_TYPE_SHIFT)
#define BM_CID                  (CRM_BUF_CID << BM_BUF_TYPE_SHIFT)

#ifdef CRM_PIXEL_BYTES
#if CRM_PIXEL_BYTES == 4
#define X_VERTEX_SHIFT                  18
#define CRM_PIXEL_DEPTH                 2
#define LAST_BYTE_OFFSET                0x30000
#elif CRM_PIXEL_BYTES == 2
#define X_VERTEX_SHIFT                  17
#define CRM_PIXEL_DEPTH                 1
#define LAST_BYTE_OFFSET                0x10000
#else
#define X_VERTEX_SHIFT                  16
#define CRM_PIXEL_DEPTH                 0
#define LAST_BYTE_OFFSET                0
#endif
#endif


/* ClipMode */
#define CM_ENCID                BIT(11)
#define CM_CID_MAP_1            BIT(10)
#define CM_ENSCRMASK_SHIFT      5
#define CM_ENSCRMASK_MASK       (31 << CM_ENSCRMASK_SHIFT)
#define CM_ENSCRMASK5           (BIT(5) | BIT(6) | BIT(7) | BIT(8) | BIT(9))
#define CM_ENSCRMASK4           (BIT(5) | BIT(6) | BIT(7) | BIT(8))
#define CM_ENSCRMASK3           (BIT(5) | BIT(6) | BIT(7))
#define CM_ENSCRMASK2           (BIT(5) | BIT(6))
#define CM_ENSCRMASK1           BIT(5)
#define CM_SCRMASK_SHIFT        0
#define CM_SCRMASK_MASK         (31 << CM_SCRMASK_SHIFT)
#define CM_SCRMASK5_IN          (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4))
#define CM_SCRMASK4_IN          (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define CM_SCRMASK3_IN          (BIT(0) | BIT(1) | BIT(2))
#define CM_SCRMASK2_IN          (BIT(0) | BIT(1))
#define CM_SCRMASK1_IN          BIT(0)

/* DrawMode */
#define DM_ENNOCONFLICT         BIT(23)
#define DM_ENGL                 BIT(22)
#define DM_ENPIXXFER            BIT(21)
#define DM_ENSCISSORTEST        BIT(20)
#define DM_ENLINESTIPPLE        BIT(19)
#define DM_ENPOLYSTIPPLE        BIT(18)
#define DM_ENOPAQSTIPPLE        BIT(17)
#define DM_ENSMOOTHSHADE        BIT(16)
#define DM_ENTEXTURE            BIT(15)
#define DM_ENFOG                BIT(14)
#define DM_ENCOVERAGE           BIT(13)
#define DM_ENANTILINE           BIT(12)
#define DM_ENALPHATEST          BIT(11)
#define DM_ENBLEND              BIT(10)
#define DM_ENLOGICOP            BIT(9)
#define DM_ENDITHER             BIT(8)
#define DM_ENCOLORMASK          BIT(7)
#define DM_ENCOLORBYTEMASK      BIT(3) | BIT(4) | BIT(5) | BIT(6)
#define DM_ENDEPTHTEST          BIT(2)
#define DM_ENDEPTHMASK          BIT(1)
#define DM_ENSTENCILTEST        BIT(0)

#define DM_ENCBM_LO8            BIT(3)
#define DM_ENCBM_LO16           BIT(3) | BIT(4)
#define DM_ENCBM_LO24           BIT(3) | BIT(4) | BIT(5)

/* Primitive */
#define PRIM_WIDTH_SHIFT        0
#define PRIM_WIDTH_MASK         (0xFFFF << PRIM_WIDTH_SHIFT)
#define PRIM_ZERO_WIDTH         0x20
#define PRIM_EDGE_SHIFT         16
#define PRIM_EDGE_MASK          (3 << PRIM_EDGE_SHIFT)
#define PRIM_EDGE_LR_BT         (CRM_EDGE_LR_BT << PRIM_EDGE_SHIFT)
#define PRIM_EDGE_RL_BT         (CRM_EDGE_RL_BT << PRIM_EDGE_SHIFT)
#define PRIM_EDGE_LR_TB         (CRM_EDGE_LR_TB << PRIM_EDGE_SHIFT)
#define PRIM_EDGE_RL_TB         (CRM_EDGE_RL_TB << PRIM_EDGE_SHIFT)
#define PRIM_SKIPLAST           BIT(18)
#define PRIM_RESERVED_SHIFT     19
#define PRIM_RESERVED_MASK      (0x1F << PRIM_RESERVED_SHIFT)
#define PRIM_OPCODE_SHIFT       24
#define PRIM_OPCODE_MASK        (0xFF << PRIM_OPCODE_SHIFT)
#define PRIM_OPCODE_POINT       (CRM_OPCODE_POINT << PRIM_OPCODE_SHIFT)
#define PRIM_OPCODE_LINE        (CRM_OPCODE_LINE << PRIM_OPCODE_SHIFT)
#define PRIM_OPCODE_TRI         (CRM_OPCODE_TRI << PRIM_OPCODE_SHIFT)
#define PRIM_OPCODE_RECT        (CRM_OPCODE_RECT << PRIM_OPCODE_SHIFT)
#define PRIM_OPCODE_FLUSH       (CRM_OPCODE_FLUSH << PRIM_OPCODE_SHIFT)

/* copy mode */
#define MTE_CLEAR               (0 << 11)
#define MTE_COPY                (1 << 11)
#define MTE_EN_STIPPLE          (1 << 10)
#define MTE_PIX_DEPTH_SHFT      8
#define SRC_TLB_SHIFT           5
#define DST_TLB_SHIFT           2

/* Line Stipple */
#define MAX_REPEAT              0
#define REPEAT_COUNT            8
#define MAX_INDEX               16
#define STIPPLE_INDEX           24


/*
** Chip register offsets 
*/
  
/* Interface buffer space base address */
#define CRM_INTFBUF_BASE                0x0

/* TLB base address */
#define CRM_TLB_BASE			0x1000

/* Offsets into framebuffer, texture, cid, and linear TLB's */ 
#define CRM_TLB_FB_A_OFFSET		0x0
#define CRM_TLB_FB_B_OFFSET		0x200
#define CRM_TLB_FB_C_OFFSET		0x400
#define CRM_TLB_TEX_OFFSET		0x600
#define CRM_TLB_CID_OFFSET		0x6e0
#define CRM_TLB_LINEAR_A_OFFSET		0x700
#define CRM_TLB_LINEAR_B_OFFSET		0x780

/* Pixel pipe base address */
#define CRM_PIXPIPE_BASE		0x2000

/* Start space is offset 2kb from the base */
#define CRM_START_OFFSET		0x800

/* Pixel pipe register offsets from base */
#define	CRM_BUF_MODE_SRC_REG		0x000
#define	CRM_BUF_MODE_DST_REG		0x008

#define	CRM_CLIP_MODE_REG		0x010

#define	CRM_DRAW_MODE_REG		0x018

#define	CRM_SCRMASK0_REG		0x020
#define	CRM_SCRMASK1_REG		0x028
#define	CRM_SCRMASK2_REG		0x030
#define	CRM_SCRMASK3_REG		0x038
#define	CRM_SCRMASK4_REG		0x040

#define	CRM_SCISSOR_REG			0x048

#define	CRM_WINOFFSET_SRC_REG		0x050
#define	CRM_WINOFFSET_DST_REG		0x058

#define	CRM_PRIMITIVE_REG		0x060

#define	CRM_VERTEX_X_XY0_REG		0x070
#define	CRM_VERTEX_X_XY1_REG		0x074
#define	CRM_VERTEX_X_XY2_REG		0x078

#define	CRM_VERTEX_GL_X0_REG		0x080
#define	CRM_VERTEX_GL_Y0_REG		0x084
#define	CRM_VERTEX_GL_X1_REG		0x088
#define	CRM_VERTEX_GL_Y1_REG		0x08c
#define	CRM_VERTEX_GL_X2_REG		0x090
#define	CRM_VERTEX_GL_Y2_REG		0x094

#define CRM_START_SETUP_REG		0x098

#define	CRM_PIXELXFER_SRC_ADDR_REG	0x0a0
#define	CRM_PIXELXFER_SRC_XSTEP_REG	0x0a8
#define	CRM_PIXELXFER_SRC_YSTEP_REG	0x0ac
#define	CRM_PIXELXFER_DST_LINADDR_REG	0x0b0
#define	CRM_PIXELXFER_DST_LINSTRIDE_REG	0x0b4

#define	CRM_STIPPLE_MODE_REG		0x0c0
#define	CRM_STIPPLE_PATT_REG		0x0c4

#define	CRM_SHADE_FGCOLOR_REG		0x0d0
#define	CRM_SHADE_BGCOLOR_REG		0x0d8
#define	CRM_SHADE_R0_REG		0x0e0
#define	CRM_SHADE_G0_REG		0x0e4
#define	CRM_SHADE_B0_REG		0x0e8
#define	CRM_SHADE_A0_REG		0x0ec
#define	CRM_SHADE_DRDX_REG		0x0f0
#define	CRM_SHADE_DGDX_REG		0x0f4
#define	CRM_SHADE_DRDY_REG		0x0f8
#define	CRM_SHADE_DGDY_REG		0x0fc
#define	CRM_SHADE_DBDX_REG		0x100
#define	CRM_SHADE_DADX_REG		0x104
#define	CRM_SHADE_DBDY_REG		0x108
#define	CRM_SHADE_DADY_REG		0x10c

#define	CRM_TEX_MODE_REG		0x110
#define	CRM_TEX_FORMAT_REG		0x118
#define	CRM_TEX_SQ0_REG			0x120
#define	CRM_TEX_TQ0_REG			0x128
#define	CRM_TEX_Q0_REG			0x130
#define CRM_TEX_STSHIFT_REG		0x134
#define	CRM_TEX_DSQDX_REG		0x138
#define	CRM_TEX_DSQDY_REG		0x140
#define	CRM_TEX_DTQDX_REG		0x148
#define	CRM_TEX_DTQDY_REG		0x150
#define	CRM_TEX_DQDX_REG		0x158
#define	CRM_TEX_DQDY_REG		0x15c
#define	CRM_TEX_BORDERCOLOR_REG		0x160
#define	CRM_TEX_ENVCOLOR_REG		0x168

#define	CRM_FOG_COLOR_REG		0x170
#define	CRM_FOG_F0_REG			0x178
#define	CRM_FOG_DFDX_REG		0x180
#define	CRM_FOG_DFDY_REG		0x188

#define	CRM_ANTIALIAS_LINE_REG		0x190
#define	CRM_ANTIALIAS_COV_REG		0x194

#define	CRM_ALPHATEST_REG		0x198

#define	CRM_BLEND_CONSTCOLOR_REG	0x1a0
#define	CRM_BLEND_FUNC_REG		0x1a8

#define	CRM_LOGICOP_REG			0x1b0

#define	CRM_COLORMASK_REG		0x1b8

#define	CRM_DEPTH_FUNC_REG		0x1c0
#define	CRM_DEPTH_Z0_REG		0x1c8
#define	CRM_DEPTH_DZDX_REG		0x1d0
#define	CRM_DEPTH_DZDY_REG		0x1d8

#define	CRM_STENCIL_MODE_REG		0x1e0
#define	CRM_STENCIL_MASK_REG		0x1e8

#define	CRM_PIXPIPE_NULL_REG		0x1f0
#define	CRM_PIXPIPE_FLUSH_REG		0x1f8

/* MTE base address */
#define CRM_MTE_BASE			0x3000

/* MTE register offsets from base */
#define	CRM_MTE_MODE_REG		0x0
#define	CRM_MTE_BYTEMASK_REG		0x8
#define	CRM_MTE_STIPPLEMASK_REG		0x10
#define	CRM_MTE_FGVALUE_REG		0x18
#define	CRM_MTE_SRC0_REG		0x20
#define	CRM_MTE_SRC1_REG		0x28
#define	CRM_MTE_DST0_REG		0x30
#define	CRM_MTE_DST1_REG		0x38
#define	CRM_MTE_SRCYSTEP_REG		0x40
#define	CRM_MTE_DSTYSTEP_REG		0x48

#define	CRM_MTE_NULL_REG		0x70
#define	CRM_MTE_FLUSH_REG		0x78

/* Status register offsets from base */
#define CRM_STATUS_BASE             	0x4000
#define CRM_STATUS_REG              	0x0
#define SET_STARTPTR_REG        	0x8

#define CRM_PAGE_SIZE			0x1000

/*****************************************************************************/
/*
** These are the shadow register mask defines.  The mask is in
** hwcx->shadow.PixPipeNull.
**
** XXX: Warning:  Those defines have to be kept in sync with the
**      kernel defines that are used for the context switching.
**
** XXX: A simple context switching code (that would be used in the kernel
**      driver) used in the opengl cxsw simulator is in CRIME/crm_sim.c
**
** XXX: A one means copy, therefore if copymask is 0 we  copy nothing.
*/
/* mask defines used for the copymask */
/*
** Line stipple regs (2):
**      Stipple.mode
**      Stipple.pattern
*/
#define CRM_CXSW_LSTIPPLE       0x01

#define CRM_CXSW_MTE            0x04    /* mte registers (9 regs) */

/* opengl-only defines */
/*
** Texture regs (23):
**      Texture.mode
**      Texture.format
**      Texture.sq0 tq0 q0      (2)
**      Texture.dsqdx dtqdx dqdx        (2)
**      Texture.dsqdy dtqdy dqdy        (2)
**      Texture.envColor
*/
#define CRM_CXSW_TEXTURE        0x08

/*
** Shade regs (12):
**      Shade.r0 drdx drdy
**      Shade.g0 dgdx dgdy
**      Shade.b0 dbdx dbdy
**      Shade.a0 dadx dady
*/
#define CRM_CXSW_SHADE          0x10

/*
** Depth/Stencil (9):
**      Depth.func
**      Depth.z0        (2)
**      Depth.dzdx      (2)
**      Depth.dzdy      (2)
**      Stencil.mode
**      Stencil.mask
*/
#define CRM_CXSW_SZ             0x20

/*
** Complex rendering ops (8):
**      Fog.color
**      Fog.f0
**      Fog.dfdx
**      Fog.dfdy
**      Antialias
**      AlphaTest
**      Blend.constColor
**      Blend.func
*/
#define CRM_CXSW_MISC           0x40

#define CRM_CXSW_ALL         	(CRM_CXSW_LSTIPPLE | CRM_CXSW_MTE | \
				  CRM_CXSW_TEXTURE | CRM_CXSW_SHADE | \
				  CRM_CXSW_SZ | CRM_CXSW_MISC)
/*
 * This bit distinguishes X and GL shadow areas.
 * If set, this is an X context, and the kernel
 * will not load the Vertex.GL, Shade, Scissor, Fog
 * Antialias, Blend, Depth, Alphatest, Stencil or
 * Texture registers when switching this process in.
 * It will switch the Vertex.X
 * and Shade.[fg]Color registers.
 *
 * XXX Probably could avoid reloading the above
 * when resuming a GL process that was preempted by
 * the server.
 */
#define     CRM_CXSW_NOT_GL         0x80

#endif /* !__GL_CRIMEDEF_H__ */
