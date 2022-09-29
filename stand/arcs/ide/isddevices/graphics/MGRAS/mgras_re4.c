/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sgidefs.h>
#include <libsc.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

__uint32_t rssstart = 0;
__uint32_t rssend = 1;
__uint32_t numTRAMs;

__uint32_t numRE4s;

__uint32_t check_re_revision(__uint32_t);

#define IS_RE 1
#define IS_TE 0
#define IS_PP 2

RSS_Reg_Info_t re4_rwreg[] = {
        {"RE TRI_X0",           TRI_X0,  TWENTYONEBIT_MASK, 1},
        {"RE TRI_X1",           TRI_X1,  TWENTYONEBIT_MASK, 1},
        {"RE TRI_X2",           TRI_X2,  TWENTYONEBIT_MASK, 1},
        {"RE TRI_YMAX",         TRI_YMAX,       TWENTYONEBIT_MASK, 1},
        {"RE TRI_YMID",         TRI_YMID,       TWENTYONEBIT_MASK, 1},
        {"RE TRI_YMIN",         TRI_YMIN,       TWENTYONEBIT_MASK, 1},
        {"RE TRI_DXDY0_HI",     TRI_DXDY0_HI,   ELEVENBIT_MASK, 1},
        {"RE TRI_DXDY0_LOW",    TRI_DXDY0_LOW,  TWENTYSIXBIT_MASK, 1},
        {"RE TRI_DXDY1_HI",     TRI_DXDY1_HI,   ELEVENBIT_MASK, 1},
        {"RE TRI_DXDY1_LOW",    TRI_DXDY1_LOW,  TWENTYSIXBIT_MASK, 1},
        {"RE TRI_DXDY2_LOW",    TRI_DXDY2_LOW,          TWENTYSIXBIT_MASK, 1},
        {"RE GLINE_XSTARTF",    GLINE_XSTARTF,  TWENTYONEBIT_MASK, 1},
        {"RE GLINE_YSTARTF",    GLINE_YSTARTF,  TWENTYONEBIT_MASK, 1},
        {"RE GLINE_XENDF",      GLINE_XENDF,    TWENTYONEBIT_MASK, 1},
        {"RE GLINE_YENDF",      GLINE_YENDF,    TWENTYONEBIT_MASK, 1},
        {"RE GLINE_DX",         GLINE_DX,       TWENTYSIXBIT_MASK, 1},
        {"RE GLINE_DY",         GLINE_DY,       TWENTYSIXBIT_MASK, 1},
        {"RE GLINE_ADJUST",     GLINE_ADJUST,   TWENTYFIVEBIT_MASK, 1},
	{"RE4 PACKED_COLOR",	PACKED_COLOR,	  	THIRTYTWOBIT_MASK, 1},
	{"RE4 RE_RED",	 	RE_RED,	  		TWENTYFIVEBIT_MASK, 1},
	{"RE4 RE_GREEN",	RE_GREEN,	  	TWENTYFIVEBIT_MASK, 1},
	{"RE4 RE_BLUE",		RE_BLUE,	  	TWENTYFIVEBIT_MASK, 1},
	{"RE4 RE_ALPHA",	RE_ALPHA,	  	TWENTYFIVEBIT_MASK, 1},
	{"RE4 XFRCOUNTERS",	XFRCOUNTERS,	 	THIRTYTWOBIT_MASK, 1},
	{"RE4 XFRMODE",	 	XFRMODE,	 	TWENTYONEBIT_MASK, 1},
	{"RE4 LSPAT",	 	LSPAT,	 		THIRTYTWOBIT_MASK, 1},
	{"RE4 LSCRL",	 	LSCRL,	 		TWENTYBIT_MASK, 1},
        {"RE4 UNDEFINED",       0x999,           	0},
/* registers after here are not tested */
	{"RE4 CHAR_H",	 	CHAR_H,	  		THIRTYTWOBIT_MASK, 1},
	{"RE4 CHAR_L",	 	CHAR_L,	  		THIRTYTWOBIT_MASK, 1},
        {"RE TRI_DXDY2_HI_IR",  TRI_DXDY2_HI_IR,        EIGHTEENBIT_MASK, 1},
	{"RE4 XLINE_XYSTARTI",	XLINE_XYSTARTI,		THIRTYTWOBIT_MASK, 1},
	{"RE4 XLINE_XYENDI",	XLINE_XYENDI,		THIRTYTWOBIT_MASK, 1},
	{"RE4 XLINE_INC1",	XLINE_INC1,  		SEVENTEENBIT_MASK, 1},
	{"RE4 XLINE_INC2",	XLINE_INC2,  		SEVENTEENBIT_MASK, 1},
	{"RE4 XLINE_ERROR_OCT",	XLINE_ERROR_OCT, 	TWENTYONEBIT_MASK, 1},
	{"RE4 BLOCK_XYSTARTI",	BLOCK_XYSTARTI,  	THIRTYTWOBIT_MASK, 1},
	{"RE4 BLOCK_XYENDI",	BLOCK_XYENDI,	  	THIRTYTWOBIT_MASK, 1},
	{"RE4 BLOCK_XSAVEOCT",	BLOCK_XSAVEOCT,  	SIXTEENBIT_MASK, 0},
	{"RE4 DRX",	 	DRX,			TWENTYFIVEBIT_MASK, 0},
	{"RE4 DRE",	 	DRE,			TWENTYFIVEBIT_MASK, 0},
	{"RE4 DGE",	 	DGE,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DGX",	 	DGX,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DBE",	 	RE_DBE,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DBX",	 	DBX,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DAE",	 	DAE,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DAX",	 	DAX,	 		TWENTYFIVEBIT_MASK, 0},
	{"RE4 Z_HI",	 	Z_HI,	 		ELEVENBIT_MASK, 0},
	{"RE4 Z_LOW",	 	Z_LOW,	 		TWENTYSIXBIT_MASK, 0},
	{"RE4 DZX_HI",	 	DZX_HI,	 		ELEVENBIT_MASK, 0},
	{"RE4 DZX_LOW",	 	DZX_LOW,	 	TWENTYSIXBIT_MASK, 0},
	{"RE4 DZE_HI",	 	DZE_HI,	 		ELEVENBIT_MASK, 0},
	{"RE4 DZE_LOW",	 	DZE_LOW,	 	TWENTYSIXBIT_MASK, 0},
        {"RE FILLMODE",         FILLMODE,       THIRTYONEBIT_MASK, 0},
        {"RE TEXMODE1",         TEXMODE1,       NINETEENBIT_MASK, 0},
        {"RE SCISSORX",         SCISSORX,       THIRTYTWOBIT_MASK, 0},
        {"RE SCISSORY",         SCISSORY,       THIRTYTWOBIT_MASK, 0},
        {"RE XYWIN",            XYWIN,          THIRTYTWOBIT_MASK, 0},
	{"RE4 BKGND_RG",	BKGND_RG,	 	TWENTYFOURBIT_MASK, 0},
	{"RE4 BKGND_BA",	BKGND_BA,	 	TWENTYFOURBIT_MASK, 0},
	{"RE4 TXENV_RG",	TXENV_RG,	 	TWENTYFOURBIT_MASK, 0},
	{"RE4 TXENV_B",	 	TXENV_B,	 	TWELVEBIT_MASK, 0},
	{"RE4 FOG_RG",	 	FOG_RG,	 		TWENTYFOURBIT_MASK, 0},
	{"RE4 FOG_B",	 	FOG_B,	 		TWELVEBIT_MASK, 0},
	{"RE4 GLINECONFIG",	GLINECONFIG,	 	TWENTYNINEBIT_MASK, 0},
	{"RE4 SCRMSK1X",	SCRMSK1X,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK1Y",	SCRMSK1Y,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK2X",	SCRMSK2X,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK2Y",	SCRMSK2Y,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK3X",	SCRMSK3X,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK3Y",	SCRMSK3Y,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK4X",	SCRMSK4X,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 SCRMSK4Y",	SCRMSK4Y,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 WINMODE",	 	WINMODE,	 	EIGHTBIT_MASK, 0},
	{"RE4 XFRSIZE",	 	XFRSIZE,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 XFRINITFACTOR",	XFRINITFACTOR,	 	SEVENTEENBIT_MASK, 0},
	{"RE4 XFRFACTOR",	XFRFACTOR,	 	SEVENTEENBIT_MASK, 0},
	{"RE4 XFRMASKLOW",	XFRMASKLOW,	 	THIRTYTWOBIT_MASK, 0},
	{"RE4 XFRMASKHIGH",	XFRMASKHIGH,	 	THIRTYTWOBIT_MASK, 0},
        {"RE XFRABORT",         XFRABORT,       0, 0},
	{"RE4 RE_TOGGLECNTX",	RE_TOGGLECNTX,	 	0, 0},
/* The user should not test these regs under normal circumstances */
	{"RE4 IR_ALIAS",	IR_ALIAS,  		SEVENBIT_MASK, 0},
        {"RE IR",               IR,             SEVENBIT_MASK, 0},
        {"RE XFRCONTROL",       XFRCONTROL,     SIXBIT_MASK, 1},
/* test DEVICE_ADDR, DEVICE_DATA by writing to PP RDRAM/RAC registers */
	{"RE4 DEVICE_ADDR",	DEVICE_ADDR,	 	THIRTYTWOBIT_MASK, 1},
	{"RE4 DEVICE_DATA",	DEVICE_DATA,	 	THIRTYTWOBIT_MASK, 1},
        {"RE CONFIG",           CONFIG,         TWELVEBIT_MASK, 0},
	{"RE4 STATUS",	 	STATUS,	 		ELEVENBIT_MASK, 1},
	{"RE4 GLINECONFIG_ALIAS", 	GLINECONFIG_ALIAS, 	TWENTYNINEBIT_MASK, 1},
	{"RE4 CSIM TEXSYNC",	 TEXSYNC, 	FOURBIT_MASK, 1},
/* double load addresses */
        {"RE TRI_X0_X1",        TRI_X0 + DBLBIT,  TWENTYONEBIT_MASK, 1},
        {"RE TRI_X2_YMAX",      TRI_X2 + DBLBIT,  TWENTYONEBIT_MASK, 1},
        {"RE TRI_YMID_YMIN",    TRI_YMID + DBLBIT,       TWENTYONEBIT_MASK, 1},
        {"RE TRI_DXDY0_HI_LOW",     TRI_DXDY0_HI + DBLBIT,   ELEVENBIT_MASK, 1},
        {"RE TRI_DXDY1_HI_LOW",     TRI_DXDY1_HI + DBLBIT,   ELEVENBIT_MASK, 1},
        {"RE TRI_DXDY2_HI_IR_LOW", TRI_DXDY2_HI_IR + DBLBIT,EIGHTEENBIT_MASK,1},
        {"RE GLINE_XSTARTF_Y",    GLINE_XSTARTF + DBLBIT, TWENTYONEBIT_MASK, 1},
        {"RE GLINE_XENDF_Y",      GLINE_XENDF + DBLBIT,   TWENTYONEBIT_MASK, 1},
        {"RE GLINE_DX_DY",         GLINE_DX + DBLBIT,    TWENTYSIXBIT_MASK, 1},
	{"RE4 RE_RED_GREEN",	 	RE_RED + DBLBIT, TWENTYFIVEBIT_MASK, 1},
	{"RE4 RE_BLUE_ALPHA",	RE_BLUE + DBLBIT, TWENTYFIVEBIT_MASK, 1},
	{"RE4 XLINE_INC1_INC2",	XLINE_INC1 + DBLBIT,  	SEVENTEENBIT_MASK, 1},
	{"RE4 XLINE_ERROR_OCT_IR",XLINE_ERROR_OCT +DBLBIT,TWENTYONEBIT_MASK, 1},
	{"RE4 DRE_DRX",	 	DRE + DBLBIT,		TWENTYFIVEBIT_MASK, 0},
	{"RE4 DGE_DGX",	 	DGE + DBLBIT,	 	TWENTYFIVEBIT_MASK, 0},
	{"RE4 DBE_DBX",	 	RE_DBE + DBLBIT,	TWENTYFIVEBIT_MASK,0},
	{"RE4 DAE_DAX",	 	DAE + DBLBIT,	 	TWENTYFIVEBIT_MASK, 0},
	{"RE4 Z_HI_LOW",	Z_HI + DBLBIT, 		ELEVENBIT_MASK, 0},
	{"RE4 DZX_HI_LOW",	DZX_HI + DBLBIT,	ELEVENBIT_MASK, 0},
	{"RE4 DZE_HI_LOW",	DZE_HI + DBLBIT,	ELEVENBIT_MASK, 0}
};

RSS_Reg_Info_t te_rwreg[] = {
/* WARP_ENABLE = 0  */
        {"TE SW_U",             SW_U,   TWENTYFIVEBIT_MASK, 1},
        {"TE SW_L",             SW_L,   TWENTYSIXBIT_MASK, 1},
        {"TE TW_U",             TW_U,   TWENTYFIVEBIT_MASK, 1},
        {"TE TW_L",             TW_L,   TWENTYSIXBIT_MASK, 1},
        {"TE WI_U",             WI_U,   FIVEBIT_MASK, 1},
        {"TE WI_L",             WI_L,   TWENTYSIXBIT_MASK, 1},
        {"TE DWIE_U",   DWIE_U,         SEVENTEENBIT_MASK, 1},
        {"TE DWIE_L",   DWIE_L,         TWENTYSIXBIT_MASK, 1},
        {"TE DWIX_U",   DWIX_U,         SEVENTEENBIT_MASK, 1},
        {"TE DWIX_L",   DWIX_L,         TWENTYSIXBIT_MASK, 1},
        {"TE DWIY_U",   DWIY_U,         FIVEBIT_MASK, 1},
        {"TE DWIY_L",   DWIY_L,         TWENTYSIXBIT_MASK, 1},
        {"TE UNDEFINED",        0x999,          0},
/* regs after here will not be tested in the write/read test */
        {"TE DSWX_U",   DSWX_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DSWX_L",   DSWX_L,  TWENTYSIXBIT_MASK, 0},
        {"TE DTWX_U",   DTWX_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DTWX_L",   DTWX_L,  TWENTYSIXBIT_MASK, 0},
        {"TE DSWY_U",   DSWY_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DSWY_L",   DSWY_L,  TWENTYSIXBIT_MASK, 0},
        {"TE DTWY_U",   DTWY_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DTWY_L",   DTWY_L,  TWENTYSIXBIT_MASK, 0},
        {"TE DTWE_U",   DTWE_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DTWE_L",   DTWE_L,  TWENTYSIXBIT_MASK, 0},
        {"TE DSWE_U",   DSWE_U,  TWENTYFIVEBIT_MASK, 0},
        {"TE DSWE_L",   DSWE_L,  TWENTYSIXBIT_MASK, 0},
        {"TE TXSCALE",  TXSCALE,        SIXBIT_MASK, 0},
        {"TE TXDETAIL",         TXDETAIL,       NINEBIT_MASK, 0},
        {"TE TXADDR",           TXADDR,         FIVEBIT_MASK, 0},
/* TXMIPMAP, TXBORDER, and DETAILSCALE must be tested after writing to
 * TXADDR since TXADDR is a pointer into these 3 regs
 */
        {"TE TXMIPMAP",         TXMIPMAP,       NINEBIT_MASK, 0},
        {"TE TXBORDER",         TXBORDER,       NINEBIT_MASK, 0},
        {"TE DETAILSCALE",      DETAILSCALE,    SIXBIT_MASK, 0},
/* single buffered registers */
        {"TE TXTILE",   TXTILE,  TWENTYSIXBIT_MASK, 0},
        {"TE TXLOD",    TXLOD,   TWENTYONEBIT_MASK, 0},
        {"TE TXBASE",   TXBASE,  EIGHTBIT_MASK, 0},
        {"TE TRAM_CNTRL",       TRAM_CNTRL,     TWENTYBIT_MASK, 0},
        {"TE MDDMA_CNTRL",      MDDMA_CNTRL,    TWENTYNINEBIT_MASK, 0},
        {"TE TEXRBUFFER",       TEXRBUFFER,     ONEBIT_MASK, 0},
        {"TE TXCLAMPSIZE",      TXCLAMPSIZE,    TWENTYSIXBIT_MASK, 0},
/* shared with TRAM */
        {"TE TEXMODE2",         TEXMODE2,       TWENTYFIVEBIT_MASK, 0},
        {"TE TXSIZE",           TXSIZE,         TWENTYTHREEBIT_MASK, 0},
        {"TE TXBCOLOR_RG",      TXBCOLOR_RG,    TWENTYFOURBIT_MASK, 0},
        {"TE TXBCOLOR_BA",      TXBCOLOR_BA,    TWENTYFOURBIT_MASK, 0},
        {"TE MAX_PIXEL_RG",     MAX_PIXEL_RG,   TWENTYFOURBIT_MASK, 0},
        {"TE MAX_PIXEL_BA",     MAX_PIXEL_BA,   TWENTYFOURBIT_MASK, 0},
/* regs after here will not be tested in the write/read test */
/* TRAM regs */
        {"TR TL_WBUFFER",       TL_WBUFFER,     0x1, 0},
        {"TR TL_MODE",          TL_MODE,        0x7fff, 0},
        {"TR TL_SPEC",          TL_SPEC,        TWELVEBIT_MASK, 0},
        {"TR TL_ADDR",          TL_ADDR,        FOURBIT_MASK, 0},
        {"TR TL_MIPMAP",        TL_MIPMAP,      NINEBIT_MASK, 0},
        {"TR TL_BORDER",        TL_BORDER,      NINEBIT_MASK, 0},
        {"TR TL_VIDCNTRL",      TL_VIDCNTRL,    0x1, 0},
        {"TR TL_BASE",          TL_BASE,        EIGHTBIT_MASK, 0},
        {"TR TL_VIDABORT",      TL_VIDABORT,    0, 0},
        {"TR TL_S_SIZE",        TL_S_SIZE,      FOURTEENBIT_MASK, 0},
        {"TR TL_T_SIZE",        TL_T_SIZE,      FOURTEENBIT_MASK, 0},
        {"TR TL_S_LEFT",        TL_S_LEFT,      FOURTEENBIT_MASK, 0},
        {"TR TL_T_BOTTOM",      TL_T_BOTTOM,    FOURTEENBIT_MASK, 0},
        {"TE TE_TOGGLECNTX",    TE_TOGGLECNTX,  0, 0},
        {"TE TEVERSION",        TEVERSION,      TWENTYNINEBIT_MASK, 1},
	{"TE TESTMODE",		0x18f,		0x7, 1},
	{"TE SOFT RESET",	0x18e,		0x7, 1},
	{"TR SOFT RESET",	0x1af,		0x7, 1},
	{"TE TEST_TCROM_U",	0x198,		0x7, 1},
	{"TE TEST_TCROM_L",	0x199,		0x7, 1},
	{"TE TEST_TCRAM_U",	0x19a,		0x7, 1},
	{"TE TEST_TCRAM_L",	0x19b,		0x7, 1},
	{"TE TEST_CFIFO_U",	0x19e,		0x7, 1},
	{"TE TEST_CFIFO_L",	0x19f,		0x7, 1},
/* shared with RE4, so not readable. Written to by the RE4. */
        {"TE TRI_X0",           TRI_X0,  TWENTYONEBIT_MASK, 0},
        {"TE TRI_X1",           TRI_X1,  TWENTYONEBIT_MASK, 0},
        {"TE TRI_X2",           TRI_X2,  TWENTYONEBIT_MASK, 0},
        {"TE TRI_YMAX",         TRI_YMAX,       TWENTYONEBIT_MASK, 0},
        {"TE TRI_YMID",         TRI_YMID,       TWENTYONEBIT_MASK, 0},
        {"TE TRI_YMIN",         TRI_YMIN,       TWENTYONEBIT_MASK, 0},
        {"TE TRI_DXDY0_HI",     TRI_DXDY0_HI,   ELEVENBIT_MASK, 0},
        {"TE TRI_DXDY0_LOW",    TRI_DXDY0_LOW,  TWENTYSIXBIT_MASK, 0},
        {"TE TRI_DXDY1_HI",     TRI_DXDY1_HI,   ELEVENBIT_MASK, 0},
        {"TE TRI_DXDY1_LOW",    TRI_DXDY1_LOW,  TWENTYSIXBIT_MASK, 0},
        {"TE TRI_DXDY2_HI_IR",  TRI_DXDY2_HI_IR,        SIXTEENBIT_MASK, 0},
        {"TE TRI_DXDY2_LOW",    TRI_DXDY2_LOW,          TWENTYSIXBIT_MASK, 0},
        {"TE GLINE_XSTARTF",    GLINE_XSTARTF,  TWENTYONEBIT_MASK, 0},
        {"TE GLINE_YSTARTF",    GLINE_YSTARTF,  TWENTYONEBIT_MASK, 0},
        {"TE GLINE_XENDF",      GLINE_XENDF,    TWENTYONEBIT_MASK, 0},
        {"TE GLINE_YENDF",      GLINE_YENDF,    TWENTYONEBIT_MASK, 0},
        {"TE GLINE_DX",         GLINE_DX,       TWENTYSIXBIT_MASK, 0},
        {"TE GLINE_DY",         GLINE_DY,       TWENTYSIXBIT_MASK, 0},
        {"TE GLINE_ADJUST",     GLINE_ADJUST,   TWENTYFIVEBIT_MASK, 0},
        {"TE IR",               IR,             FIVEBIT_MASK, 0},
        {"TE XFRABORT",         XFRABORT,       0},
        {"TE XFRCONTROL",       XFRCONTROL,     SIXBIT_MASK, 0},
        {"TE FILLMODE",         FILLMODE,       TWENTYNINEBIT_MASK, 0},
        {"TE TEXMODE1",         TEXMODE1,       EIGHTEENBIT_MASK, 0},
        {"TE CONFIG",           CONFIG,         TWELVEBIT_MASK, 0},
        {"TE SCISSORX",         SCISSORX,       THIRTYTWOBIT_MASK, 0},
        {"TE SCISSORY",         SCISSORY,       THIRTYTWOBIT_MASK, 0},
        {"TE XYWIN",            XYWIN,          THIRTYTWOBIT_MASK, 0},
/* double load addresses */
        {"TE SW_U_L",             SW_U + DBLBIT,   TWENTYFIVEBIT_MASK, 1},
        {"TE TW_U_L",             TW_U + DBLBIT,   TWENTYFIVEBIT_MASK, 1},
        {"TE WI_U_L",             WI_U + DBLBIT,   FIVEBIT_MASK, 1},
        {"TE DWIE_U_L",   DWIE_U + DBLBIT,         SEVENTEENBIT_MASK, 1},
        {"TE DWIX_U_L",   DWIX_U + DBLBIT,         SEVENTEENBIT_MASK, 1},
        {"TE DWIY_U_L",   DWIY_U + DBLBIT,         FIVEBIT_MASK, 1},
        {"TE DSWE_U_L",   DSWE_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1},
        {"TE DTWE_U_L",   DTWE_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1},
        {"TE DSWX_U_L",   DSWX_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1},
        {"TE DTWX_U_L",   DTWX_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1},
        {"TE DSWY_U_L",   DSWY_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1},
        {"TE DTWY_U_L",   DTWY_U + DBLBIT,  TWENTYFIVEBIT_MASK, 1}
};

RSS_Reg_Info_t pp_rwreg[] = {
	{"PP_PIXCMD"            , PP_PIXCMD & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_FILLMODE"          , PP_FILLMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_COLORMASKMSB"      , PP_COLORMASKMSB & 0x1ff, THIRTYTWOBIT_MASK,0},
	{"PP_COLORMASKLSBA"    , PP_COLORMASKLSBA & 0x1ff, THIRTYTWOBIT_MASK,0},
	{"PP_COLORMASKLSBB"   , PP_COLORMASKLSBB & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_BLENDFACTOR"       , PP_BLENDFACTOR & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_STENCILMODE"       , PP_STENCILMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_STENCILMASK"       , PP_STENCILMASK & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_ZMODE"             , PP_ZMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_AFUNCMODE"         , PP_AFUNCMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_ACCMODE"           , PP_ACCMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_BLENDCOLORRG"     , PP_BLENDCOLORRG & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_BLENDCOLORBA"     , PP_BLENDCOLORBA & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_DRBPOINTERS"       , PP_DRBPOINTERS & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_DRBSIZE"           , PP_DRBSIZE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGMODE"           , PP_TAGMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGDATA_R"         , PP_TAGDATA_R & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGDATA_G"         , PP_TAGDATA_G & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGDATA_B"         , PP_TAGDATA_B & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGDATA_A"         , PP_TAGDATA_A & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_TAGDATA_Z"         , PP_TAGDATA_Z & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOORDINATE"   , PP_PIXCOORDINATE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOLORR"         , PP_PIXCOLORR & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOLORG"         , PP_PIXCOLORG & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOLORB"         , PP_PIXCOLORB & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOLORA"         , PP_PIXCOLORA & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_PIXCOLORACC"       , PP_PIXCOLORACC & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_WINMODE"           , PP_WINMODE & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_MAINBUFSEL"        , PP_MAINBUFSEL & 0x1ff, THIRTYTWOBIT_MASK, 0},
	{"PP_OLAYBUFSEL"        , PP_OLAYBUFSEL & 0x1ff, THIRTYTWOBIT_MASK, 0}
};

/**************************************************************************
 *                                                                        *
 *  read_REStatusReg                                                      *
 *                                                                        *
 *  	Read the RE4 status register of 1 RE4                             *
 *                                                                        *
 **************************************************************************/

__uint32_t
read_REStatusReg(__uint32_t rssnum)
{
	__uint32_t errors = 0, status = 0xdead, version_num = 2;

	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
		rssnum, TWELVEBIT_MASK);

	HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(STATUS))), TWELVEBIT_MASK, 
		status, CSIM_RSS_DEFAULT);

	if (((status & RE4NUMBER_BITS) >> 2) != rssnum) {
		msg_printf(ERR, "RE4 ID is wrong. RE4's Status register got=0x%x, expected = 0x%x, diff = 0x%x\n",
		status, (status & 0x7f3) | (rssnum << 2), 
		status ^ ((status & 0x7f3) | (rssnum << 2)));
		errors++;
	}

	/* allow version to be 0 (rev A) or 2 (rev C) */
	if ((((status & 0xf0)>> 4) != 0) && (((status & 0xf0)>> 4) != 2)) {
		msg_printf(ERR, "RE4's version is wrong. RE4's Status register got = 0x%x, expected = 0x%x or 0x%x\n",
		status, (status & 0x70f) | (2 <<4), (status & 0x70f) );
		errors++;
	}

	if (((status & 0x3) + 1) != numRE4s) {
		msg_printf(ERR, "The RE4 count is wrong. RE4's Status register got = 0x%x, expected = 0x%x, diff = 0x%x\n",
		status, (status & 0x7fc) | (numRE4s >> 1), 
		status ^ ((status & 0x7fc) | (numRE4s >> 1))); 
		errors++;
	}

	return (errors);
}	


__uint32_t 
check_re_revision(__uint32_t rssnum)
{
	__uint32_t revision=0, status;
	__int32_t i;

	/* need to check that all rss have rev C before returning non-zero */
	for (i = 0; i < numRE4s; i++) {
	   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),i, TWELVEBIT_MASK);

	   HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(STATUS))), TWELVEBIT_MASK, 
			status, CSIM_RSS_DEFAULT);

	   revision += ((status & 0xf0)>> 4);
	}

	if ((revision/numRE4s) >= 2)
		return ((revision/numRE4s));
	else
		return (0);
}

/* User callable RE revision check */

__int32_t
mg_re_rev(int argc, char **argv)
{
	__uint32_t rssnum = 0, bad_arg = 0;

        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,"Usage: -r[RSS number] \n");
		return (0);
        }

	return (check_re_revision(rssnum));
}

/**************************************************************************
 *                                                                        *
 *  re_checkargs							  *
 *                                                                        *
 *  	Check the arguments for re routines				  *
 *                                                                        *
 **************************************************************************/

__uint32_t
re_checkargs(
	__uint32_t rssnum, 
	__uint32_t regnum, 
	__uint32_t rflag, 
	__uint32_t gflag)
{
        __uint32_t errors = 0, foo;

        if (rflag) {
           if ((rssnum == 0) || ((rssnum >= 1) && (rssnum < numRE4s)))
           {
                rssstart = rssnum;
                rssend = rssstart+1;
           }
           else {
                msg_printf(SUM, 
			"The RSS number must be >= 0 and < %d\n", numRE4s);
                errors++;
           }
        }

        if (gflag) {
		re_or_te_or_pp(regnum, &foo);
	}

        return (errors);
}

/**************************************************************************
 *                                                                        *
 *  re_or_te_or_pp()							  *
 *                                                                        *
 *  	from the regnum, tell me if I'm re (=1) or te/tram (=0) or pp =2  *
 *                                                                        *
 **************************************************************************/

__uint32_t
re_or_te_or_pp(__uint32_t regnum, __uint32_t *real_mask)
{
	__uint32_t i, is_re4, mask=0, is_readable;
	char regname[30];

	is_re4 = IS_RE;
        GET_RE_REG_NAME(regnum);
        if (strcmp(regname, "RE UNDEFINED") == 0) {
        	GET_TE_REG_NAME(regnum);
                if (strcmp(regname, "TE UNDEFINED") == 0) {
                   	GET_PP_REG_NAME(regnum);
                      	if (strcmp(regname, "PP UNDEFINED") == 0) {
                     		msg_printf(SUM, 
				   "Register # 0x%x is undefined.\n", regnum);
                        	is_re4 = 3;
		   	}
			else
				is_re4 = IS_PP;
	   	}
		else
			is_re4 = IS_TE;
        }

	*real_mask = mask;
	return (is_re4);
}

/**************************************************************************
 *                                                                        *
 *  mgras_REStatusReg -r[optional RE4_Number] -l[optional loopcount]      *
 *                                                                        *
 *  	Read the RE4 status register of 1 or all RE4s                     *
 *                                                                        *
 *      RE4_Number is optional and if specified, only the RE4 in that     *
 *      RSS will be tested. If no RE4_Number is specified, all REs will   *
 *      be checked.                            				  *
 **************************************************************************/

int
mgras_REStatusReg(int argc, char ** argv)
{
	__uint32_t errors=0, rflag = 0, regnum = 0, gflag = 0, bad_arg = 0;
        __uint32_t rssnum, tot_errs=0;
	__int32_t j, loopcount = 1;

	/* initialize to test everything */
	rssstart = 0; rssend = numRE4s;

	msg_printf(VRB,"RE4 Status Reg Test\n");
	
        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                rflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM,
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,"Usage: -r[optional RSS number] -l[optional loopcount]\n");
		return (0);
        }

        if (re_checkargs(rssnum, regnum, rflag, gflag))
                return (0);

 	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	  for (j = (rssend-1); j >= 0; j--) {
	    errors = read_REStatusReg(j);
	    tot_errs += errors;
	    REPORT_PASS_OR_FAIL_NR(
		  (&RE_err[RE4_STATUS_REG_TEST + j*LAST_RE_ERRCODE]), 
			errors);
	    errors = 0;
	    msg_printf(DBG, "j=%d\n", j);
	  }
	}

	/* reset config register */
        HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);

	RETURN_STATUS(tot_errs);
}

/**************************************************************************
 *                                                                        *
 *  rdwr_RSSReg 							  *
 *                                                                        *
 *      read or write the specified reg. No error checking 		  *
 *                                                                        *
 **************************************************************************/

__uint32_t
rdwr_RSSReg(
	__uint32_t rssnum, 
	__uint32_t regnum, 
	__uint32_t data, 
	__uint32_t mask, 
	__uint32_t access, 
	__uint32_t is_re4,
	__uint32_t dbl,
	__uint32_t data2,
	__uint32_t mode,
	__uint32_t ld_ex)
{
	__uint32_t i, actual, actual_lo, is_readable = 0;
	__uint32_t  toggle_reg;
	char regname[30];

	actual = actual_lo = 0xdead;

	is_readable = 1; /* hack for now */

	switch (is_re4) {
		case IS_RE:
			toggle_reg = RE_TOGGLECNTX;
			GET_RE_REG_NAME(regnum)
			break;
		case IS_TE:
			toggle_reg = TE_TOGGLECNTX;
			GET_TE_REG_NAME(regnum);
			break;
		case IS_PP:
			toggle_reg = RE_TOGGLECNTX;
			GET_PP_REG_NAME(regnum);
			break;
		default:
			strcpy(regname, "Undefined");
			break;	
	}

   	if (access == WRITE) {
		if ((regnum == STATUS) || (regnum == TEVERSION)) {
			msg_printf(VRB, "The %s register is not writable\n",
				regname);
		}
		else {
		   if (dbl) {
		      msg_printf(VRB, 
			"Double Write RSS-%d, %s reg (0x%x), data_hi:0x%x, data_lo: 0x%x\n",
			rssnum, regname, regnum, data, data2);


		      if (ld_ex == 0) {
		      	WRITE_RSS_REG_DBL(rssnum, regnum, data, data2,mask);
		      }
		      else {
			HQ3_PIO_WR64_RE_EX(RSS_BASE + HQ_RSS_SPACE(regnum), data, data2, mask);
		      }
#ifdef MGRAS_DIAG_SIM
		      WRITE_RSS_REG(rssnum, toggle_reg, 0xdeadbeef, 0xffffffff);
		      if (is_readable) {
			READ_RSS_REG(rssnum, (regnum & 0x1ff), data, mask);
			msg_printf(VRB, 
				"RSS-%d %s register (0x%x) contents: 0x%x\n",
				rssnum, regname, regnum, actual & mask);
			READ_RSS_REG(rssnum, ((regnum & 0x1ff)+1), data, mask);
			msg_printf(VRB, 
				"RSS-%d %s register (0x%x) contents: 0x%x\n",
				rssnum, regname, regnum+1, actual & mask);
		      }
#endif
		   }
		   else { /* single */
		      msg_printf(VRB, 
			"Writing RSS-%d, %s reg (0x%x), data:0x%x\n",
			rssnum, regname, regnum, data);
		
			/* accept all data for now */
			mask = ~0x0;

		      if (ld_ex == 1) {
		      	HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(regnum), data, mask);
		      }
		      else {
		      	WRITE_RSS_REG(rssnum, regnum, data, mask);
		      }
#ifdef MGRAS_DIAG_SIM
		      WRITE_RSS_REG(rssnum, toggle_reg, 0xdeadbeef, 0xffffffff);
		      if (is_readable) {
		         READ_RSS_REG(rssnum, regnum, data, mask);
		         msg_printf(VRB,"RSS-%d %s register (0x%x) read:0x%x\n",
				rssnum, regname, regnum, actual & mask);
		      }
#endif
		   } /* single */
		}
	}
	else if (access == READ) {
	   if (is_readable) {	
	      if (dbl) {
		READ_RSS_REG_DBL(rssnum, regnum, mask);
		msg_printf(VRB,
		   "Double read RSS-%d %s register (0x%x) hi:0x%x, lo:0x%x\n",
				rssnum, regname, regnum, actual, actual_lo);
	      }
	      else {
		actual = 0xdead;
		if (ld_ex) {
		   HQ3_PIO_RD_RE(((RSS_BASE + (HQ_RSS_SPACE(regnum))) | 0x1000),
				mask, actual, 0x0)
		}
		else {
			READ_RSS_REG(rssnum, regnum, data, mask);
		}
		msg_printf(VRB, "RSS-%d %s register (0x%x) contents: 0x%x\n",
				rssnum, regname, regnum, actual);
	      }
	   }
	   else
		msg_printf(VRB, "The %s register is not readable\n", regname);
	}	

	return(0);
}

/**************************************************************************
 *                                                                        *
 *  mgras_WriteRSSReg -r[RSS #] -g[regnum] -d[data] -m[mask] -p -e[data2]  *
 *			-o[mode #] -x -l[loopcount, 0 = infinite]  	  *
 *                                                                        *
 *      Write the specified reg 					  *
 *                                                                        *
 *	-p = do a double register write. -p is optional			  *
 *	-e = the upper 32 bits of data in a 64-bit (double) reg write. It *
 *		only is used if the -p switch is used.			  *
 *	-x = load and execute bit set					  *
 *                                                                        *
 **************************************************************************/

int
mgras_WriteRSSReg(int argc, char ** argv)
{
	__uint32_t data, rssnum, regnum, ld_ex = 0, is_re4;
	__uint32_t rflag = 0, gflag = 0, dflag = 0, bad_arg = 0;
	__uint32_t dbl = 0, eflag = 0, data2 = 0xdeadcafe, mode = 1, cfifo_status;
	__int32_t loopcount = 1;
	__uint32_t mask = 0xffffffff;

	/* initialize */
	rssnum = 0;

        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                rflag++; break;
                        case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
                        case 'd':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&data);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&data);
                                dflag++; break;
                        case 'e':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&data2);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&data2);
                                eflag++; break;
#if 0
                        case 'm':
				if (argv[0][2]=='\0') {
                                	atob_L(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob_L(&argv[0][2], (int*)&mask);
                                mflag++; break;
#endif
                        case 'x':
                                ld_ex++; break;
                        case 'o':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&mode);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&mode);
                                break;
                        case 'p':
                                dbl = 1; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM,
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
	rflag = 1; gflag = 1, mflag = 1; dflag = 1;
	rssnum = 0; regnum = CHAR_L; /* TRI_X0 */

	/* also check a double buffered reg */
	
	mask = THIRTYTWOBIT_MASK; data = 0x5555;
	dbl = 1; eflag =1 ;regnum = 0x280;data2 = 0xbaadcafe; data = 0x11223344;
#endif
	msg_printf(DBG, 
	 "RSS #:%d, regnum:%x, data:%x, mask:%x dbl:%d, data2:%x, ld_ex:%d, loopcount:%d\n", 
		rssnum, regnum, data, mask, dbl, data2, ld_ex, loopcount);

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
            "Usage: -r[optional RSS #, default=0] -g[regnum] -d[data1] -p -e[data2] -l[optional loopcount, 0=infinite] -x[(optional)load and execute, default is not to execute]\n");
                return (0);
        }

	if ((gflag == 0) || (dflag == 0)) {
		msg_printf(SUM,"You must specify RSS#, reg#, data\n");
           	msg_printf(SUM,
            "Usage: -r[optional RSS #, default=0] -g[regnum] -d[data] -p -e[data2] -l[optional loopcount, 0=infinite] -x[(optional)1=load and execute]\n");
		return (0);
	}

	if ((dbl) && (eflag == 0)) {
		msg_printf(SUM,"You must specify the 2nd data with the -e flag since you specified -p\n");
           	msg_printf(SUM,
            "Usage: -r[RSS #] -g[regnum] -d[data] -p -e[data2] -l[optional loopcount, 0=infinite] -x(optional load and execute)\n");
		return (0);
	}

        if (re_checkargs(rssnum, regnum, rflag, gflag))
                return (0);

 	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

#if 1
	is_re4 = re_or_te_or_pp(regnum, &mask);
#endif
	if (rflag) {
		WRITE_RSS_REG(rssnum, CONFIG, rssnum, ~0x0);
	}

	while (--loopcount) {
		WAIT_FOR_CFIFO(cfifo_status, 8, CFIFO_TIMEOUT_VAL);
                if (cfifo_status == CFIFO_TIME_OUT) {
                       msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
	               return (-1);
                }
		rdwr_RSSReg(rssnum,regnum,data,mask,WRITE,is_re4,dbl,data2,
			mode,ld_ex);
	}

	return 0;
}

/**************************************************************************
 *                                                                        *
 *  mgras_ReadRSSReg -r[RSS #] -g[regnum] -m[mask] -l[loopcount]          *
 *                                                                        *
 *      Read the specified reg                      			  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_ReadRSSReg(int argc, char ** argv)
{
	__uint32_t rssnum, regnum, dbl = 0, is_re4;
	__uint32_t rflag = 0, gflag = 0, bad_arg = 0;
	__int32_t loopcount = 1;
	__uint32_t mask = 0xffffffff, ld_ex = 0;

	/* initialize */
	rssnum = 0;

        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                rflag++; break;
                        case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
#if 0
                        case 'm':
				if (argv[0][2]=='\0') {
                                	atob_L(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob_L(&argv[0][2], (int*)&mask);
                                mflag++; break;
#endif
                        case 'x':
				ld_ex++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM,
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: 
				bad_arg++; break;
                }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
	rflag = 1; gflag = 1, mflag = 1; 
	rssnum = 0; regnum = XFRCOUNTERS; /* 0x158 */
	mask = THIRTYTWOBIT_MASK;
	dbl = 0; regnum = 0x80; 
#endif

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
            "Usage: -r[optional RSS #, default=0] -g[regnum] -l[optional loopcount] -x(optional read and execute)\n");
                return (0);
        }

	if ((gflag == 0)) {
		msg_printf(SUM,"You must specify reg#\n");
           	msg_printf(SUM,
            "Usage: -r[optional RSS #, default=0] -g[regnum] -l[optional loopcount]\n");
		return (0);
	}

        if (re_checkargs(rssnum, regnum, rflag, gflag))
                return (0);

 	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	is_re4 = re_or_te_or_pp(regnum, &mask);

	if ((check_re_revision(rssnum) == 0) && (is_re4 == IS_TE)) {
		msg_printf(SUM, "This revision of the RE cannot read TE registers. Skipping this test\n");
		return (0);
	}
	if (rflag) {
		WRITE_RSS_REG(rssnum, CONFIG, rssnum, ~0x0);
	}

	while (--loopcount) {
		rdwr_RSSReg(rssnum, regnum, CSIM_RSS_DEFAULT, mask, READ, 
			is_re4, dbl, 0, 1, ld_ex);
	}

	return(0);
}

/**************************************************************************
 *                                                                        *
 *  test_RSSRdWrReg                                                       *
 *                                                                        *
 *      Write/Read marching 1/0 to the specified reg                      *
 *                                                                        *
 **************************************************************************/

__uint32_t
test_RSSRdWrReg(__uint32_t loc_rssnum,char * str,__uint32_t regnum, __uint32_t mask, __uint32_t stop_on_err, __uint32_t is_re4)
{
        __uint32_t i, data,actual, status = 0, errors = 0, reg_size, toggle_reg;
	__uint32_t cfifo_status;

	msg_printf(DBG,"Walking 1/0 to RSS-%d, %s register (0x%x)\n",
				loc_rssnum, str, regnum);

	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),
		loc_rssnum, TWELVEBIT_MASK);

	/* to get around a bug in TRI-X2, YMAX */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(IR))),
		0, TWELVEBIT_MASK);

	switch (is_re4) {
		case IS_RE:
			toggle_reg = RE_TOGGLECNTX;
			break;
		case IS_TE:
			toggle_reg = TE_TOGGLECNTX;
			break;
		default:
			toggle_reg = RE_TOGGLECNTX;
			break;	
	}

	/* for TXMIPMAP, TXBORDER, DETAILSCALE, write TXADDR between writes */
	if ((regnum == TXMIPMAP) || (regnum == TXBORDER) || 
		(regnum == DETAILSCALE)) 
	{
		toggle_reg = TXADDR;
		data = 0; /* level 0 for TXADDR */
	}		

	/* figure out how big the register is */
	status = mask;
	reg_size = 0;
	while (status) {
		status >>= 1;
		reg_size++;
	}

	WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    	if (cfifo_status == CFIFO_TIME_OUT) {
        	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
        	return (-1);
    	}

	msg_printf(DBG,"Walking 1\n");
        /* Walk 1 */
        for (i = 0; i < reg_size; i++) {
           HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(regnum))),walk_1_0_32_rss[i],mask);
	   if (toggle_reg == TXADDR)
			data = ~walk_1_0_32_rss[i];
           HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(toggle_reg))), 
		data, TWENTYFIVEBIT_MASK);
           HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), mask, actual, 
				walk_1_0_32_rss[i]);
           COMPARE(str, (RSS_BASE + (HQ_RSS_SPACE(regnum))),
				walk_1_0_32_rss[i], actual, mask, errors);
	   if (errors && stop_on_err) {
		return (errors);
	   }
        }

	msg_printf(DBG,"Walking 0\n");

        /* Walk 0 */
        for (i = 0; i < reg_size; i++) {
           HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(regnum))),~walk_1_0_32_rss[i],
			mask);
	   if (toggle_reg == TXADDR)
		data = walk_1_0_32_rss[i];
           HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(toggle_reg))), 
		data, TWENTYFIVEBIT_MASK);
           HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), mask, actual, 
			~walk_1_0_32_rss[i]);
           COMPARE(str, RSS_BASE +  HQ_RSS_SPACE(regnum), 
                        ~walk_1_0_32_rss[i], actual, mask, errors);
	   if (errors && stop_on_err) {
		return (errors);
	   }
        }
	return (errors);
}
	
/**************************************************************************
 *                                                                        *
 *  mgras_RERdWrRegs -r[optional RSS_Number] -g[optional regnum] 	  *
 *		-m[optional mask] -x [1|0] -l[optional loopcount]	  *
 *                                                                        *
 *  	Write/Read marching 1/0 to readable regs                 	  *
 *                                                                        *
 *      RSS_Number is optional and if specified, only the RE4 in that     *
 *      RSS will be tested. If no RSS_Number is specified, all REs will   *
 *      be checked.                                                       *
 *                                                                        *
 *      regnum is also optional and specifies which register is to be     *
 *      tested. regnum must appear with a RSS_Number, but RSS_Number can  *
 *      be specified without regnum - in that case, all RE registers      *
 *      associated with the specified RSS_Number RSS will be tested.      *
 *                                                                        *
 *      mask specifies the bit mask for the register                      *
 *	-x controls stop_on_err						  *
 **************************************************************************/

int
mgras_RERdWrRegs(int argc, char ** argv)
{
	__uint32_t errors=0, mask, rssnum, regnum, status, is_readable;
	__uint32_t i, j, rssloop, stop_on_err = 1, is_re4 = 1;
	__uint32_t rflag = 0, gflag = 0, mflag = 0, bad_arg = 0;
	__int32_t loopcount = 1, test_TETR_regs = 0;
	char regname[30];

	NUMBER_OF_RE4S();

	/* initialize to test everything */

        rssstart = 0; rssend = numRE4s;

        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                rflag++; break;
                        case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
                        case 'm':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&mask);
                                mflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM,
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        case 'x':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&stop_on_err);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&stop_on_err);
                                break;
                        case 't':
				test_TETR_regs = 1;
				is_re4 = IS_TE;
                                break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
             "Usage: -r[optional RSS #] -g[optional reg #] -m[optional mask] -t (test TE registers) -l[optional loopcount] -x[1|0] (optional stop-on-error, default =1)\n");
		return (0);
        }

        if (re_checkargs(rssnum, regnum, rflag, gflag))
                return (0);

	
	if (gflag ^ mflag) {
          msg_printf(SUM, "Register and mask go together.\n");
          msg_printf(SUM, "If you specify one, you must specify the other.\n");
          return (0);
        }

 	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

#ifdef MGRAS_DIAG_SIM
	stop_on_err = 0;
#endif

	if ((check_re_revision(rssnum) == 0) && (test_TETR_regs)) {
		msg_printf(SUM, "This revision of the RE cannot read TE registers. Skipping this test\n");
		return (0);
	}

	while (--loopcount) {
          for (rssloop = rssstart; rssloop < rssend; rssloop++) {
	
		msg_printf(DBG, "rssloop: %d, rssstart: %d, rssend: %d\n",
			rssloop, rssstart, rssend);

             if (gflag) { /* test only 1 reg per rss */
		   is_re4 = re_or_te_or_pp(regnum, &mask);
		   switch (is_re4) {
			case IS_RE:
				GET_RE_REG_NAME(regnum)
				break;
			case IS_TE:
				GET_TE_REG_NAME(regnum);
				break;
			default:
				strcpy(regname, "Undefined");
				break;	
		   }

		   if ((check_re_revision(rssnum) == 0) && (is_re4 == IS_TE)) {
			msg_printf(SUM, "This revision of the RE cannot read TE registers. Skipping this test\n");
			return (0);
		   }

			
                   errors += test_RSSRdWrReg(rssloop,regname, regnum, mask,
					stop_on_err, is_re4);
		   if (is_re4 == IS_RE) {
		   		REPORT_PASS_OR_FAIL(
		      			(&RE_err[RE4_RDWR_REGS_TEST + 
					rssloop*LAST_RE_ERRCODE]), 
	  				errors);
			} else {
				if ( is_re4 == IS_TE) {
		   			REPORT_PASS_OR_FAIL(
		      			(&TE_err[TE1_RDWRREGS_TEST+ 
					rssloop*LAST_TE_ERRCODE]), 
	  				errors);
				}
			}
		   }
             
             else { /* test all regs per rss */
                j = 0;
		if (test_TETR_regs) {
                   while (te_rwreg[j].regNum != 0x999) {
                   	errors += test_RSSRdWrReg(rssloop, te_rwreg[j].str, 
		     		te_rwreg[j].regNum,te_rwreg[j].mask,
				stop_on_err, is_re4);
		        if (errors && stop_on_err) {
	  			REPORT_PASS_OR_FAIL(
	  			   (&TE_err[TE1_RDWRREGS_TEST+ 
					rssloop*LAST_TE_ERRCODE]), errors);
		        }	
			j++;
		   }
		}
		else {
                   while (re4_rwreg[j].regNum != 0x999) {
                   	errors += test_RSSRdWrReg(rssloop, re4_rwreg[j].str, 
		     		re4_rwreg[j].regNum,re4_rwreg[j].mask,
				stop_on_err, is_re4);
		        if (errors && stop_on_err) {
	  			REPORT_PASS_OR_FAIL(
	  			   (&RE_err[RE4_RDWR_REGS_TEST+ 
					rssloop*LAST_RE_ERRCODE]), errors);
		       }	
                       j++;
                   }
		}
             }
	     if (test_TETR_regs) {
	        REPORT_PASS_OR_FAIL_NR(
	  	(&TE_err[TE1_RDWRREGS_TEST+ rssloop*LAST_TE_ERRCODE]),errors);
	     }
	     else {
	        REPORT_PASS_OR_FAIL_NR(
	  	(&RE_err[RE4_RDWR_REGS_TEST+ rssloop*LAST_RE_ERRCODE]),errors);
	     }
          }
	}
	RETURN_STATUS(errors);
}

/**************************************************************************
 *                                                                        *
 *  _mgras_rss_init 							  *
 *									  *
 * Sets register values to 0						  *
 *                                                                        *
 **************************************************************************/
__uint32_t
_mgras_rss_init(__uint32_t rssnum)
{

#if 0
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
	   rssnum, TWELVEBIT_MASK);

	/* re4 regs */
	i = 0;
	while (re4_rwreg[i].regNum != XFRABORT) {
	   	WRITE_RSS_REG(rssnum, re4_rwreg[i].regNum, data, 
			re4_rwreg[i].mask);
		i++;
		if ((i & 0xff) == 0x1f) {
			WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			if (cfifo_status == CFIFO_TIME_OUT) {
			   msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
			   return (-1);
			}
		}
        }

	/* te, tram regs */
	i = 0;
	while (te_rwreg[i].regNum != TEVERSION) {
	   	WRITE_RSS_REG(rssnum, te_rwreg[i].regNum, data, 
			te_rwreg[i].mask);
		i++;
		if ((i & 0xff) == 0x1f) {
			WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			if (cfifo_status == CFIFO_TIME_OUT) {
			   msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
			   return (-1);
			}
		}
        }

#if 0
	/* pp regs */
	for (j = 0x160; j <= 0x17b; j++) {
	   	WRITE_RSS_REG(rssnum, j, data, THIRTYTWOBIT_MASK);
        }
   	WRITE_RSS_REG(rssnum, 0x17e, data, THIRTYTWOBIT_MASK);
   	WRITE_RSS_REG(rssnum, 0x17f, data, THIRTYTWOBIT_MASK);
#endif
#endif

	return 0;
}

/**************************************************************************
 *                                                                        *
 *  mgras_rss_init -r[optional RSS_Number]				  *
 *                                                                        *
 **************************************************************************/
mgras_rss_init(int argc, char ** argv)
{
	__uint32_t i, rssnum, rflag = 0, errors = 0, bad_arg = 0;
	__uint32_t status;

	NUMBER_OF_RE4S();
        rssstart = 0; rssend = numRE4s;

        /* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
                switch (argv[0][1]) {
                        case 'r':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&rssnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&rssnum);
                                rflag++; break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
             "Usage: -r[optional RSS #] \n\n");
		return (0);
        }

        if (re_checkargs(rssnum, 0, rflag, 0))
                return (0);

	for (i = rssstart; i < rssend; i++) {
		_mgras_rss_init(i);
	}

	return(errors);
}
