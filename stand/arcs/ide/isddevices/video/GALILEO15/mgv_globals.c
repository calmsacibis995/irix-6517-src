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
#include "mgv_diag.h"
#ifdef IP30
#include "../../../godzilla/include/d_bridge.h"
#include "../../../godzilla/include/d_godzilla.h"
#endif

/*
 * Forward References
 */
__uint32_t 	_mgv_reportPassOrFail(mgv_test_info *tPtr, __uint32_t errors);
__uint32_t	_mgv_outb(__paddr_t addr, unsigned char val);
uchar_t		_mgv_inb(__paddr_t addr);
__uint32_t	_mgv_outw(__paddr_t addr, __uint32_t val);
__uint32_t	_mgv_inw(__paddr_t addr);
__uint32_t 	_mgv_singlebit(__uint32_t reg, __uint32_t val, __uint32_t regGroup);
__uint32_t	_mgv_CC1PollReg(unsigned char reg, unsigned char exp, 
		unsigned char mask, __uint32_t timeOut,unsigned char* regVal);
__uint32_t	_mgv_CC1PollFBRdy(__uint32_t timeOut);
__uint32_t	mgv_waitForSpace(__uint32_t argc, char **argv);

unsigned char* 	_mgv_PageAlign(unsigned char *pBuf);
int 		check_reference(void);
int 		_mgv_i2c_seq_retry_count = 0;

/*
 * Global Variables
 */

/*** For Speed Racer Bridge port ***/
__uint32_t bridge_port;

/*** For EDH read and write ***/
/*   Allocate one word extra for read, for mgv_i2c_rcv */
uchar_t shadow_edh_iic_read[16];
uchar_t shadow_edh_iic_write[12];

mgv_vgi1_ch_trig_t mgv_abort_dma = {
        0LL,                            /* unused0 */
        MGV_VGI1_DMA_ABORT_ON,          /* abort */
        MGV_VGI1_TRIG_DISABLE           /* int_trig */
};
mgv_vgi1_ch_trig_t mgv_trig_on = {
        0LL,                            /* unused0 */
        MGV_VGI1_DMA_ABORT_OFF,         /* abort */
        MGV_VGI1_TRIG_ENABLE            /* int_trig */
};
mgv_vgi1_ch_trig_t mgv_trig_off = {
        0LL,                            /* unused0 */
        MGV_VGI1_DMA_ABORT_OFF,         /* abort */
        MGV_VGI1_TRIG_DISABLE           /* int_trig */
};

__uint32_t	mgv_ab1_basebuf[16] = {
	0x00010001, 0x00020002, 0x00030003, 0x00040004,
	0x00050005, 0x00060006, 0x00070007, 0x00080008,
	0x00090009, 0x000A000A, 0x000B000B, 0x000C000C,
	0x000D000D, 0x000D000E, 0x000F000F, 0x00100010
};

__uint32_t mgv_walk[24] = {
	0x11111111,  0x22222222,  0x33333333,  0x44444444,
	0x55555555,  0x66666666,  0x77777777,  0x88888888,
	0x99999999,  0xaaaaaaaa,  0xbbbbbbbb,  0xcccccccc,
	~0x11111111, ~0x22222222, ~0x33333333, ~0x44444444,
	~0x55555555, ~0x66666666, ~0x77777777, ~0x88888888,
	~0x99999999, ~0xaaaaaaaa, ~0xbbbbbbbb, ~0xcccccccc

};

__uint32_t mgv_walk_one[16] = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000
};

__uint32_t mgv_patrn[6] = {
	0x55550000, 0xAAAA5555, 0x0000AAAA,
	0x55550000, 0xAAAA5555, 0x0000AAAA
};

__uint32_t mgv_patrn32[6] = {
	((__uint32_t)0x55550000),
	((__uint32_t)0xAAAA5555),
	((__uint32_t)0x0000AAAA),
	((__uint32_t)0x5555AAAA),
	((__uint32_t)0x55555555),
	((__uint32_t)0xAAAAAAAA),
};

uchar_t tmi_bypass_patrn[16] = {
	0x01, 0x02, 0x04, 0x08,
	0x10, 0x20, 0x40, 0x80,
	0xFE, 0xFD, 0xFB, 0xF7,
	0xEF, 0xDF, 0xBF, 0x7F
};

uchar_t mgv_patrn8[5] = {
	0x55, 0xAA, 0xCC, 0x33, 0xFF
};

/*
 * This structure matches with the constants defined in mgv_diag.h
 * for error codes 
 */
#if 0  
/*** moved to video_errcodes.c ***/
mgv_test_info	mgv_err[] = {
	/* Impact Video Board Error Strings and Codes */
	/* Board Specific ASIC independent Tests 0 - 4 */
	{"Impact Video Initialization Test",		"XXXXX"},  /* 00 */
	{"GPI Trigger Test",				"XXXXX"},  /* 01 */
	{"",						"XXXXX"},  /* 02 */
	{"",						"XXXXX"},  /* 03 */
	{"",						"XXXXX"},  /* 04 */

	/* AB1 Error Strings and Codes */
	{"AB1 Dram Address Bus Red Channel Test",	"XXXXX"},  /* 05 */
	{"AB1 Dram Address Bus Green Channel Test",	"XXXXX"},  /* 06 */
	{"AB1 Dram Address Bus Blue Channel Test",	"XXXXX"},  /* 07 */
	{"AB1 Dram Address Bus Alpha Channel Test",	"XXXXX"},  /* 08 */
	{"AB1 Dram Data Bus Red Channel Test",		"XXXXX"},  /* 09 */
	{"AB1 Dram Data Bus Green Channel Test",	"XXXXX"},  /* 10 */
	{"AB1 Dram Data Bus Blue Channel Test",		"XXXXX"},  /* 11 */
	{"AB1 Dram Data Bus Alpha Channel Test",	"XXXXX"},  /* 12 */
	{"AB1 Dram Pattern Red Channel Test",		"XXXXX"},  /* 13 */
	{"AB1 Dram Pattern Green Channel Test",		"XXXXX"},  /* 14 */
	{"AB1 Dram Pattern Blue Channel Test",		"XXXXX"},  /* 15 */
	{"AB1 Dram Pattern Alpha Channel Test",		"XXXXX"},  /* 16 */
	{"AB1 Dram Pattern Slow Red Channel Test",	"XXXXX"},  /* 17 */
	{"AB1 Dram Pattern Slow Green Channel Test",	"XXXXX"},  /* 18 */
	{"AB1 Dram Pattern Slow Blue Channel Test",	"XXXXX"},  /* 19 */
	{"AB1 Dram Pattern Slow Alpha Channel Test",	"XXXXX"},  /* 20 */
	{"AB1 DCB Test",				"XXXXX"},  /* 21 */
	{"",						"XXXXX"},  /* 22 */
	{"",						"XXXXX"},  /* 23 */
	{"",						"XXXXX"},  /* 24 */
	{"",						"XXXXX"},  /* 25 */
	{"",						"XXXXX"},  /* 26 */
	{"",						"XXXXX"},  /* 27 */
	{"",						"XXXXX"},  /* 28 */
	{"",						"XXXXX"},  /* 29 */

	/* CC1 Error Strings and Codes */
	{"CC1 Dram Address Bus Test",			"XXXXX"},  /* 30 */
	{"CC1 Dram Data Bus Test",			"XXXXX"},  /* 31 */
	{"CC1 Dram Pattern Test",			"XXXXX"},  /* 32 */
	{"CC1 DCB Test",				"XXXXX"},  /* 33 */
	{"",						"XXXXX"},  /* 34 */
	{"",						"XXXXX"},  /* 35 */
	{"",						"XXXXX"},  /* 36 */
	{"",						"XXXXX"},  /* 37 */
	{"",						"XXXXX"},  /* 38 */
	{"",						"XXXXX"},  /* 39 */

	/* VGI1 Error Strings and Codes */
	{"VGI1 Register Pattern Test",			"XXXXX"},  /* 40 */
	{"VGI1 Register Walking Bit Test",		"XXXXX"},  /* 41 */
	{"VGI1 Interrupt Test",				"XXXXX"},  /* 42 */
	{"VGI1 DMA Test",				"XXXXX"},  /* 43 */
	{"",						"XXXXX"},  /* 44 */
	{"",						"XXXXX"},  /* 45 */
	{"",						"XXXXX"},  /* 46 */
	{"",						"XXXXX"},  /* 47 */
	{"",						"XXXXX"},  /* 48 */
	{"",						"XXXXX"},  /* 49 */

        /* CSC Error Strings and Codes */
        {"CSC  Address Bus Test",                       "XXXXX"},  /* 50 */
        {"CSC  Data Bus Test",                          "XXXXX"},  /* 51 */
        {"CSC  Input LUTs Test",                        "XXXXX"},  /* 52 */
        {"CSC  Output LUTs Test",                       "XXXXX"},  /* 53 */
        {"CSC  Coefficient Test",                       "XXXXX"},  /* 54 */
        {"CSC  X2K Lut Test",                           "XXXXX"},  /* 55 */
	{"",						"XXXXX"},  /* 56 */
	{"",						"XXXXX"},  /* 57 */
	{"",						"XXXXX"},  /* 58 */
	{"",						"XXXXX"},  /* 59 */

        /* TMI Error Strings and Codes */
	{"TMI Register Pattern Test",			"XXXXX"},  /* 60 */
	{"TMI Register Data Bus Test",			"XXXXX"},  /* 61 */
	{"TMI Bye Pass Test",				"XXXXX"},  /* 62 */
	{"TMI SRAM Address Uniq Test",			"XXXXX"},  /* 63 */
	{"TMI SRAM Pattern Test",			"XXXXX"},  /* 64 */
	{"",						"XXXXX"},  /* 65 */
	{"",						"XXXXX"},  /* 66 */
	{"",						"XXXXX"},  /* 67 */
	{"",						"XXXXX"},  /* 68 */
	{"",						"XXXXX"},  /* 69 */

        /* SCALER Error Strings and Codes */
        {"SCL  Address Bus Test",                       "XXXXX"},  /* 70 */
        {"SCL  Data Bus Test",                          "XXXXX"},  /* 71 */

};
#endif

/* The following LUT's are defined in mgv_extern.h.
   Change the following if those LUTs value change.
*/
char * csc_luts[] = {
	"YG INPUT LUT",		/* CSC_YG_IN */
	"UB INPUT LUT",		/* CSC_UB_IN */
	"VR INPUT LUT",		/* CSC_VR_IN */
	"XK INPUT LUT",		/* CSC_XK_IN */
	"YG OUTPUT LUT",	/* CSC_YG_OUT */	
	"UB OUTPUT LUT",	/* CSC_UB_OUT */
	"VR OUTPUT LUT",	/* CSC_VR_OUT */
	"ALPHA OUTPUT LUT"	/* CSC_ALPHA_LUT */
	};

mgv_exp_bits mgv_bit_desc[] = {

/* CC INDIRECT */
0,			0,	0,	0, /* empty slot */
BT_A_DIRECT,		0,	1,	0,
BT_A_24_32,		0,	1,	1,
BT_A_SKIP,		0,	1,	6,
BT_A_DOIT,		0,	1,	7,
BT_A_ACTIVE,		1,	0xf,	0,

BT_B_DIRECT,		2,	1,	0,
BT_B_24_32,		2,	1,	1,
BT_B_DITHER,		2,	1,	3,
BT_B_FULL,		2,	1,	4,
BT_B_SKIP,		2,	1,	6,
BT_B_DOIT,		2,	1,	7,
BT_B_ACTIVE, 		3,	0xff,	0,

BT_C_DITHER,		4,	1,	3,
BT_C_SKIP,		4,	1,	6,
BT_C_DOIT,		4,	1,	7,

BT_A_LEFT,		8,	0xff,	0,
BT_A_RIGHT,		9,	0xff,	0,
BT_A_BLACK_TOP_HI,	10,	3,	0,
BT_A_BLACK_BOT_HI,	10,	3,	2,
BT_A_BACKOUT,		10,	1,	4,
BT_A_TOP_LINE,		11,	0xff,	0,
BT_A_BOTTOM_LINE,	12,	0xff,	0,
BT_A_PHASE_HI,		13,	1,	0,
BT_A_PHASE_LO,		14,	0xff,	0,

BT_B_LEFT, 		16,	0xff,	0,
BT_B_RIGHT, 		17,	0xff,	0,
BT_B_BLACK_TOP_HI,	18,	3,	0,
BT_B_BLACK_BOT_HI,	18,	3,	2,
BT_B_BACKOUT, 		18,	1,	4,
BT_B_TOP_LINE, 		19,	0xff,	0,
BT_B_BOTTOM_LINE, 	20,	0xff,	0,
BT_B_PHASE_HI, 		21,	1,	0,
BT_B_PHASE_LO, 		22,	0xff,	0,

BT_C_LEFT, 		24,	0xff, 	0,
BT_C_RIGHT, 		25,	0xff, 	0,
BT_C_BLACK_TOP_HI,	26,	3,	0,
BT_C_BLACK_BOT_HI,	26,	3,	2,
BT_C_BACKOUT, 		26,	1,	4,
BT_C_TOP_LINE, 		27,	0xff,	0,
BT_C_BOTTOM_LINE, 	28,	0xff,	0,
BT_C_PHASE_HI, 		29,	1,	0,
BT_C_PHASE_LO, 		30,	0xff,	0,

BT_OVERLAP, 		32,	3,	0,
BT_FIELD_ID, 		32,	1,	2,
BT_PIPE,		32,	1,	3,
BT_DITHER, 		32,	1,	4,
BT_REFRESH, 		32,	1,	5,

BT_HALF_HOR_HI, 	33,	1,	0,
BT_HALF_VERT_HI, 	33,	7,	4,
BT_HALF_HOR_LO, 	34,	0xff,	0,
BT_HALF_VERT_LO, 	35,	0xff,	0,

BT_NONBLANK_VIDEO,	36,	0x1f,	0,
BT_GEN_OUT,		37,	0x1f,	0,
BT_CLEAR_INT,		38,	1,	0,
BT_ENABLE_INT,		38,	1,	1,
BT_INT_SOURCE,		38,	1,	2,
BT_VID_PORT0_ENAB,	38,	1,	5,
BT_EXP_PORT0_ENAB,	38,	1,	6,
BT_EXP_PORT1_ENAB,	38,	1,	7,

BT_LATCH_INP_LINE,	39,	1,	0,
BT_LATCH_OUT_LINE,	39,	1,	1,
BT_INT_IN_LINE_HI,	40,	1,	0,
BT_INT_IN_LINE_LO,	41,	0xff,	0,
BT_INT_OUT_LINE_HI,	42,	1,	0,
BT_INT_OUT_LINE_LO,	43,	0xff,	0,

BT_INP_LINE_LATCH_HI, 	44,	1,	0,
BT_INP_LINE_LATCH_FIELD,44,	1,	1,
BT_INP_LINE_LATCH_LO, 	45,	0xff,	0,
BT_OUT_LINE_LATCH_HI, 	46,	1,	0,
BT_OUT_LINE_LATCH_FIELD,46,	1,	1,
BT_OUT_LINE_LATCH_LO, 	47,	0xff,	0,

BT_FRM_BUF_DIRECT,	48,	1,	0,
BT_FRM_BUF_IN,		48,	1,	1,
BT_FRM_BUF_OUT,		48,	1,	2,
BT_FILT_FOR_DEC,	48,	1,	3,
BT_ADDRESSING_MODE,	48,	1,	4,
BT_HIDDEN,		48,	1,	5,
BT_DECIM_FACTOR,	48,	3,	6,

BT_HOST_LINE_NUM_HI,	49,	0xff,0,
BT_ALPHA_BKGRND,	50,	1,	0,
BT_FLAT_Y,		51,	0xff,0,
BT_FLAT_U,		52,	0xff,0,
BT_FLAT_V,		53,	0xff,0,

BT_XP_GR_A,		54,	7,	0,
BT_XP_GR_B,		54,	7,	4,
BT_XP_GR_C,		55,	7,	0,
BT_XP_EXP_0,		55,	7,	4,
BT_XP_FRM_BUF_IN,	56,	7,	0,
BT_XP_VID_OUT,		56,	7,	4,
BT_XP_FOR_IN,		57,	7,	0,
BT_XP_BACK_IN,		57,	7,	4,

BT_XP_ALPHA_FOR, 	58,	7,	0,
BT_XP_ALPHA_BACK, 	58,	7,	4,
BT_ALPHA_COMPOS_BACK,	59,	3,	0,
BT_ALPHA_COMPOS_FOR,	59,	3,	2,

/* CC_DIRECTS */

BT_NTSC_PAL,		0,	1,	0,
BT_PIXEL_SHAPE,		0,	1,	1,
BT_EXP0_DIRECT,		0,	1,	2,
BT_MASTER_TIME,		0,	1,	3,
BT_DMA_CLOCK,		0,	1,	4,
BT_TBC,			0,	1,	5,
BT_DO_IT,		0,	1,	6,

BT_INDIRECT_ADDR,	1,	0xff,	0,
BT_INDIRECT_DATA,	2,	0xff,	0,
BT_FRM_BUF_HOR,		3,	0x1f,	0,
BT_FRM_BUF_LINE_LO,	3,	1,	5,
BT_LOAD_ADDR,		3,	1,	6,
BT_RESET_FIFO,		3,	1,	7,

BT_XP_OUT,		4,	3,	0,
BT_XP_INP,		4,	3,	2,
BT_HOST_RW,		4,	7,	4,
BT_FRM_BUF_SEQ,		4,	1,	7,

BT_FRM_BUF_DATA,	5,	0xff,	0,

BT_I2C_IDLE,		6,	1,	0,
BT_I2C_RW,		6,	1,	1,
BT_I2C_HOLD_BUS,	6,	1,	2,
BT_I2C_BUS_SPEED,	6,	1,	3,
BT_I2C_BUSY,		6,	1,	4,
BT_I2C_ACK,		6,	1,	5,
BT_I2C_BUS_ERROR,	6,	1,	7,

BT_I2C_DATA,		7,	0xff,	0,

/* AB */

BT_AB_NTSC,		5,	1,	0,
BT_EXPRESS_MODE,	5,	1,	1,
BT_ENABLE_VIDEO,	5,	1,	2,
BT_CHIP_ID,		5,	3,	3,

BT_A_PAN_X_LO,		1,	0xff,	0,
BT_A_PAN_X_HI,		0,	3,	0,
BT_A_PAN_Y_LO,		3,	0xff,	0,
BT_A_PAN_Y_HI,		2,	3,	0,
BT_A_WIN_X_LEFT,	4,	7,	0,
BT_A_WIN_X_RIGHT,	4,	7,	3,
BT_A_ENABLE,		5,	1,	0,
BT_A_FLICKER,		5,	1,	1,
BT_A_PAN,		5,	1,	2,
BT_A_ZOOM,		5,	7,	3,
BT_A_ALPHA_ENAB,	5,	1,	6,
BT_A_FULL_SCREEN,	5,	1,	7,

BT_A_FREEZE,		6,	1,	0,
BT_A_DIRECTION,		6,	1,	1,
BT_A_DECIMATION,	7,	7,	0,

BT_B_PAN_X_LO,		0x11,	0xff,	0,
BT_B_PAN_X_HI,		0x10,	0x3,	0,
BT_B_PAN_Y_LO,		0x13,	0xff,	0,
BT_B_PAN_Y_HI,		0x12,	0x3,	0,
BT_B_WIN_X_LEFT,	0x14,	7,	0,
BT_B_WIN_X_RIGHT,	0x14,	7,	3,
BT_B_ENABLE,		0x15,	1,	0,
BT_B_FLICKER,		0x15,	1,	1,
BT_B_PAN,		0x15,	1,	2,
BT_B_ZOOM,		0x15,	7,	3,
BT_B_ALPHA_ENAB,	0x15,	1,	6,

BT_B_FREEZE,		0x16,	1,	0,
BT_B_DIRECTION,		0x16,	1,	1,
BT_B_RESOLUTION,	0x16,	1,	2,
BT_B_DECIMATION,	0x17,	7,	0,

BT_C_PAN_X_LO,		0x21,	0xff,	0,
BT_C_PAN_X_HI,		0x20,	0x3,	0,
BT_C_PAN_Y_LO,		0x23,	0xff,	0,
BT_C_PAN_Y_HI,		0x22,	0x3,	0,
BT_C_WIN_X_LEFT,	0x24,	0x7,	0,
BT_C_WIN_X_RIGHT,	0x24,	0x7,	3,
BT_C_ENABLE,		0x25,	1,	0,
BT_C_FLICKER,		0x25,	1,	1,
BT_C_PAN,		0x25,	1,	2,
BT_C_ZOOM,		0x25,	7,	3,
BT_C_FREEZE,		0x26,	1,	0,

BT_DRAM_REFRESH,	0,	7,	0,
BT_DRAM_TEST, 		0,	1,	7,
BT_A_DEFAULT_CYCLE,	1,	0xff, 	0,
BT_B_DEFAULT_CYCLE,	2,	0xff, 	0,
BT_CHIP_REV,		3,	7,	0,

/* AB direct register 5 */
BT_D1_ENABLE,		5,      1,      5,

/* GAL15 direct register bits  */
BT_GAL_GAL_REVISION,	0x2,	0xf,	0,
BT_GAL_TMI_REVISION,	0x2,	0xf,	4,

/* GAL15 indirect register bits  */
BT_GAL_PIXEL_MODE,	0x0,	0x1,	0,
BT_GAL_TIMING_MODE,	0x0,	0x1,	1,
BT_GAL_AUTO_PHASE,	0x2,	0x3,	0,
BT_GAL_CH1_IN_STD,	0x3,	0x3,	0,
BT_GAL_CH1_IN_PRESENT,	0x3,	0x1,	4,
BT_GAL_CH1_IN_AUTO,	0x3,	0x1,	5,
BT_GAL_CH1_IN_2HREF,	0x3,	0x1,	6,
BT_GAL_CH2_IN_STD,	0x4,	0x3,	0,
BT_GAL_CH2_IN_PRESENT,	0x4,	0x1,	4,
BT_GAL_CH2_IN_AUTO,	0x4,	0x1,	5,
BT_GAL_CH2_IN_2HREF,	0x4,	0x1,	6,
BT_GAL_INPUT_ALPHA,	0x5,	0x3,	0,
BT_GAL_CH1_IN_ROUND,	0x5,	0x1,	4,
BT_GAL_CH2_IN_ROUND,	0x5,	0x1,	5,
BT_GAL_CH12_IN_DITHER,	0x5,	0x1,	6,
BT_GAL_OUTPUT_ALPHA,	0x6,	0x3,	0,
BT_GAL_CH1_OUT_VB,	0x6,	0x1,	4,
BT_GAL_CH2_OUT_VB,	0x6,	0x1,	5,
BT_GAL_CH2_CC1_CHROMA,	0x6,	0x1,	7,
BT_GAL_CH1_OUT_HPHASE_LO,0x8,	0xff,	0,
BT_GAL_CH1_OUT_HPHASE_HI,0x9,	0xf,	0,
BT_GAL_CH2_OUT_HPHASE_LO,0xa,	0xff,	0,
BT_GAL_CH2_OUT_HPHASE_HI,0xb,	0xf,	0,
BT_GAL_CH1_FRAME_SYNC,	0xc,	0x1,	0,
BT_GAL_CH1_OUT_BLANK,	0xc,	0x1,	1,
BT_GAL_CH1_OUT_FREEZE,	0xc,	0x1,	2,
BT_GAL_CH2_FRAME_SYNC,	0xd,	0x1,	0,
BT_GAL_CH2_OUT_BLANK,	0xd,	0x1,	1,
BT_GAL_CH2_OUT_FREEZE,	0xd,	0x1,	2,
BT_GAL_VCXO_LO,		0xe,	0xff,	0,
BT_GAL_VCXO_HI,		0xf,	0x3,	0,
BT_GAL_GENLOCK_REF,	0x10,	0x3,	0,
BT_GAL_REF_LOCK,	0x11,	0x1,	0,
BT_GAL_INOUT_1,		0x13,	0x1,	0, /* not used */
BT_GAL_INOUT_2,		0x13,	0x2,	1, /* not used */
BT_GAL_GFXA_TOP_BLANK,	0x7,	0x1,	0,
BT_GAL_GFXB_TOP_BLANK,	0x7,	0x1,	4,
BT_GAL_VCXO_DAC_BUSY,	0x11,	0x1,	4,
BT_GAL_VCXO_ADC_BUSY,	0x11,	0x1,	6,
BT_GAL_REF_OFFSET_LO,	0x14,	0xff,	0,
BT_GAL_REF_OFFSET_HI,	0x15,	0xf,	0,
BT_GAL_VCXO_ADC_MODE,	0x16,	0x3,	0,
BT_GAL_VCXO_ADC_HI,	0x18,	0xf,	0,
BT_GAL_VCXO_ADC_LO,	0x17,	0xff,	0,
BT_GAL_FIRMWARE_REVISION,0x1,	0xff,	0,
BT_GAL_CH1_IN_8BIT,	0x2,	0x1,	4,
BT_GAL_CH2_IN_8BIT,	0x2,	0x1,	5,

/* VBAR1 register bit definitions */
BT_GAL_CH1_CC1,		0xf0,	0xf,	0, /* alpha or pixels */
BT_GAL_CH2_CC1,		0xf1,	0xf,	0, /* pixels */
BT_GAL_CSC_A,		0xf2,	0xf,	0,
BT_GAL_CSC_B,		0xf3,	0xf,	0,
BT_GAL_D_EXP,		0xf4,	0xf,	0,
BT_GAL_CH1_GIO,		0xf5,	0xf,	0, /* vgi1-A */
BT_GAL_CH2_GIO,		0xf6,	0xf,	0, /* vgi1-B */
BT_GAL_CH2_VOUT,	0xf7,	0xf,	0, /* pixels */
BT_GAL_CH1_VOUT,	0xf8,	0xf,	0, /* alpha or pixels */
BT_GAL_TMI_A,		0xf9,	0xf,	0,
BT_GAL_TMI_B,		0xfa,	0xf,	0,
BT_GAL_SYNC_PORT,	0xfb,	0xf,	0,
BT_GAL_A_EXP,		0xfe,	0xf,	0,
BT_GAL_B_EXP,		0xff,	0xf,	0,
BT_CC1_PIXEL_TIMING,	0x7,	0x3,	6,
BT_GAL_CH1_OUT_CHROMA,	0xc,	0x1,	3,
BT_GAL_CH2_OUT_CHROMA,	0xd,	0x1,	3,
BT_GAL_VGI1_SW_RESET,	0x1e,	0x1,	0,
BT_GAL_GFX_FRAME_RESET,	0x1a,	0x1,	0,

/* CSC register bit definitions */
BT_CSC_YUV422_IN,	0x0,	0x1,	0,
BT_CSC_FILTER_IN,	0x0,	0x1,	1,
BT_CSC_TRC_IN,		0x0,	0x1,	2,
BT_CSC_LOAD_MODE,	0x1,	0x2,	0,
BT_CSC_AUTO_CNT,	0x1,	0x1,	2,
BT_CSC_LUT_SELECT,	0x1,	0x7,	3,
BT_CSC_LUT_READ,	0x1,	0x2,	6,
BT_CSC_RW_REGISTER,	0x2,	0xff,	0,
BT_CSC_PAGE_ADDR,	0x3,	0x3f,	0,
BT_CSC_YUV422_OUT,	0x4,	0x1,	0,
BT_CSC_FILTER_OUT,	0x4,	0x1,	1,
BT_CSC_RGB_BLANK,	0x4,	0x1,	2,
BT_CSC_FULLSCALE,	0x4,	0x1,	3,
BT_CSC_MAX_ALPHA_OUT,	0x4,	0x1,	4,
BT_CSC_ALPHA_OUT,	0x4,	0x1,	5,
BT_CSC_BLACK_OUT,	0x4,	0x1,	6,

/* TMI register bit definitions */
BT_TMI_M_ROUND,		0x80,	0x3,	0,
BT_TMI_M_MIPMAP_ENABLE,	0x80,	0x1,	2,
BT_TMI_M_SCALER_YUVOUT,	0x80,	0x1,	3,
BT_TMI_M_INTRPT_ENABLE,	0x80,	0x1,	5,
BT_TMI_M_ABORT,		0x80,	0x1,	6,
BT_TMI_M_SWAP,		0x80,	0x1,	7,
BT_TMI_S_XFER_FIELD,	0x80,	0x1,	0,
BT_TMI_S_XFER_DONE,	0x80,	0x1,	1,
BT_TMI_S_INTRPT_ENABLE,	0x80,	0x1,	2,
BT_TMI_S_SWAP_STAT,	0x80,	0x1,	3,
BT_TMI_S_VID_IN_FIELD,	0x80,	0x1,	4,
};

mgvdiag	mgvbase_data;
mgvdiag	*mgvbase = (mgvdiag *) &mgvbase_data;
unsigned char   *mgv_pVGIDMADesc;
#ifdef IP30
int mgv_iic_war = 1;
unsigned char    mgv_VGIDMADesc[4096 * 2 * 4];
#else
unsigned char    mgv_VGIDMADesc[4096 * 2];
#endif

mgv_vgi1_h_param_t	hpNTSC = {0LL};
mgv_vgi1_h_param_t	hpNTSCSQ = {0LL};
mgv_vgi1_h_param_t	hpPAL = {0LL};
mgv_vgi1_h_param_t	hpPALSQ = {0LL};

mgv_vgi1_v_param_t	vpNTSC = {0LL};
mgv_vgi1_v_param_t	vpNTSCSQ = {0LL};
mgv_vgi1_v_param_t	vpPAL = {0LL};
mgv_vgi1_v_param_t	vpPALSQ = {0LL};

/*                  Parameters for each tv standard          */
/*                CCIR_525, SQ_525,   CCIR_625, SQ_625   */
int      xsize[]     = { 720, 640, 720, 768 };
int      ysize[]     = { 486, 486, 576, 768 };

/*			    	allones,allzeroes,alt10, alt01,walk1,walk0 */
/* DO NOT CHANGE INITIAL WALK VALUES: walk pattern generators will limit values */
int test_values[] = { 0xFF,    0x00,    0xAA,  0x55, 0x01, 0xFE, 0x80, 0x40 };

char* asc_bit_pat_p[] = { 
				"ALLONES", "ALLZEROES", "ALT1&0",
				"ALT0&1",  "WALK1",     "WALK0",
				"HEX80",   "HEX40"};

char* asc_vid_pat_p[] = {
				"ALLONES", "ALLZEROES", "ALT1&0",
				"ALT0&1",  "WALK1",     "WALK0",
				"VLINE",   "HLINE",     "BOX",
				"XHBOX" };

char* asc_tv_std_p[] =  {  "CCIR_525", "SQ_525", "CCIR_625", "SQ_625" };


char* asc_primary_num[] = {
				"FIRST", "SECOND", "THIRD", "FOURTH",
				"FIFTH", "SIXTH",  "SEVENTH", "EIGHTH",
				"NINTH", "TENTH" };

/********************************************************************/
/* Galileo 1.5 register mask for active bits - index is reg.indir. addr. */
/* does not apply to DCB bus */

unsigned char register_mask[] = {	
					0x03, 0xFF, /* addr 0-1   */
					0x33, 0x73, /* addr 2-3   */
					0x73, 0x73, /* addr 4-5   */
					0xB3, 0xD1, /* addr 6-7   */
					0xFF, 0x0F, /* addr 8-9   */
					0xFF, 0x0F, /* addr 10-11 */
					0x0F, 0x0F, /* addr 12-13 */
					0xFF, 0x0F, /* addr 14-15 */
					0x03, 0x51, /* addr 16-17 */
					0xFF, 0x01, /* addr 18-19 */
					0xFF, 0x03, /* addr 20-21 */
					0x03, 0xFF, /* addr 22-23 */
					0x0F, 0x01	/* addr 24-25 */ }; 
				
#if 1
/* YUVA arrays for test_allstds patterns */
float   yfgm[1][1];
float   ufgm[1][1];
float   vfgm[1][1];
short   afgm[1][1];
#else
/* YUVA arrays for test_allstds patterns */
float   yfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
float   ufgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
float   vfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
short   afgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
#endif


/* Galileo 1.5 */

int tv_std_loc[][16][2] = { 
			{ /* CCIR525 */
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{122,0},{122,180}, {122,360}, {122,540}, 
			{243,0},{243,180}, {243,360}, {243,540}, 
			{365,0},{365,180}, {365,360}, {365,540}},
			{ /* SQ525 */ 
			{  0,0},{  0,160}, {  0,320}, {  0,480},
			{122,0},{122,160}, {122,320}, {122,480}, 
			{243,0},{243,160}, {243,320}, {243,480}, 
			{365,0},{365,160}, {365,320}, {365,480}},
			{ /* CCIR625 */
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{144,0},{144,180}, {144,360}, {144,540}, 
			{288,0},{288,180}, {288,360}, {288,540}, 
			{432,0},{432,180}, {432,360}, {432,540}},
			{ /* SQ625 */ 
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{192,0},{192,180}, {192,360}, {192,540}, 
			{384,0},{384,180}, {384,360}, {384,540}, 
			{576,0},{576,180}, {576,360}, {576,540}} 
			};

int tv_std_length[][2] = {
					/* YLENGTH/YDIV, XLENGTH */
			 	/* CCIR525 */  121, 720,
				/* SQ525   */  121, 640,
				/* CCIR625 */  144, 720,
				/* SQ625   */  144, 768  };

reg_test	batch_test[MAX_REGMEMS];

/* EDH Specific Read and Write Memory words */
/* Variables for EDH Read and Write words */
edh_read_word1 edh_r1;
edh_read_word2 edh_r2;
edh_read_word3 edh_r3;
edh_read_word4 edh_r4;
edh_read_word5 edh_r5;
edh_read_word6 edh_r6;
edh_read_word7 edh_r7;
edh_read_word8 edh_r8;
edh_read_word9 edh_r9;
edh_read_word10 edh_r10;
edh_read_word11 edh_r11;
edh_read_word12 edh_r12;
edh_read_word13 edh_r13;
edh_read_word14 edh_r14;
edh_read_word15 edh_r15;

edh_write_word1 edh_w1;
edh_write_word2 edh_w2;
edh_write_word3 edh_w3;
edh_write_word4 edh_w4;
edh_write_word5 edh_w5;
edh_write_word6 edh_w6;
edh_write_word7 edh_w7;
edh_write_word8 edh_w8;
edh_write_word9 edh_w9;
edh_write_word10 edh_w10;
edh_write_word11 edh_w11;
edh_write_word12 edh_w12;

/* CSC Specific variables */
int csc_inlut, csc_alphalut, csc_coef, csc_x2k, csc_outlut, csc_inlut_value, csc_tolerance;
int csc_clipping, csc_constant_hue, csc_each_read ;

/*
 * Global functions
 */
__uint32_t
_mgv_reportPassOrFail(mgv_test_info *tPtr, __uint32_t errors)
{
    if (!errors) {
	msg_printf(SUM, "IMPV:- %s passed. ---- \n" , tPtr->TestStr);
	return 0;
    } else {
	msg_printf(SUM, "IMPV:- %s failed. ErrCode %s Errors %d\n",
		tPtr->TestStr, tPtr->ErrStr, errors);
	return -1;
    }
}

__uint32_t
_mgv_outb(__paddr_t addr, unsigned char val)
{
    register volatile uchar_t *ioaddr;

    ioaddr  = (volatile uchar_t *)addr;
    *ioaddr = val;
    return 0;
}

uchar_t
_mgv_inb(__paddr_t addr)
{
    register uchar_t val;
    register volatile uchar_t *ioaddr;

    ioaddr = (volatile uchar_t *) addr;
    val  = *ioaddr;
    return (val & 0xff);
}

__uint32_t
_mgv_outw(__paddr_t addr, __uint32_t val)
{
    register volatile unsigned int *ioaddr;

    ioaddr  = (volatile unsigned int *)addr;
    *ioaddr = val;
    return 0;
}

__uint32_t
_mgv_inw(__paddr_t addr)
{
    register __uint32_t val;
    register volatile __uint32_t *ioaddr;

    ioaddr = (volatile unsigned int *) addr;
    val  = *ioaddr;
    return (val);
}

__uint32_t
_mgv_singlebit(__uint32_t reg, __uint32_t val, __uint32_t regGroup)
{
    __uint32_t offset;
    unsigned char mask, shift;
    uchar_t cur_val;
    mgv_exp_bits	*pBitDesc = &mgv_bit_desc[reg];

    offset = pBitDesc->reg;
    offset = pBitDesc->reg;
    mask = pBitDesc->mask;
    shift = pBitDesc->shift;

    val &= mask;	/* validate data */

#if THESE_ARE_UDED
    __uint32_t name;
    name = pBitDesc->name;
	/*
	 * GAL15 Indirect registers
	 */
	if ((name >= BT_GAL_INDIRECT_MIN) && (name <= BT_GAL_INDIRECT_MAX)) {
	   cur_val = mgv_galind(mgv_bd, offset, 0, VID_READ);

	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_galind(mgv_bd, offset, cur_val, VID_WRITE);
	      return(val);
	}

	/*
	 * GAL direct registers
	 */
	if ((name >= BT_GAL_DIRECT_MIN) && (name <= BT_GAL_DIRECT_MAX)) {
	   cur_val = mgv_ccdir(mgv_bd, offset, 0, VID_READ);
	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_ccdir(mgv_bd, offset, cur_val, VID_WRITE);
	      return(val);
	}
#endif

    switch (regGroup) {

    case BT_CC_INDIR_GROUP:
    		/*
     		 * CC Indirect registers
     		 */
    		CC_W1(mgvbase, CC_INDIR1, offset);
    		CC_R1(mgvbase, CC_INDIR2, cur_val);
    		cur_val &= (0xff & ~(mask << shift));
    		cur_val |= (val << shift);
    		CC_W1(mgvbase, CC_INDIR1, offset);
    		CC_W1(mgvbase, CC_INDIR2, cur_val);
		break;

    case BT_CC_DIR_GROUP:
		/*
		 * CC direct registers
		 */
		CC_R1(mgvbase, offset, cur_val);
		cur_val &= (0xff & ~(mask << shift));
		cur_val |= (val << shift);
		CC_W1(mgvbase, offset, cur_val);
		break;
    default:
		msg_printf(ERR, "This reg group is not implemented\n");
		break;
    }
#if THESE_ARE_UDED
	/*
	 * CSC Indirect registers
	 */
	if ((name >= BT_CSC_INDIRECT_MIN) && (name <= BT_CSC_INDIRECT_MAX)) {
	   cur_val = mgv_cscind(mgv_bd, offset, 0, VID_READ);

	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_cscind(mgv_bd, offset, cur_val, VID_WRITE);
	      return(val);
	}

	/*
	 * TMI Indirect registers
	 */
	if ((name >= BT_TMI_INDIRECT_MIN) && (name <= BT_TMI_INDIRECT_MAX)) {
	   /* status register bits require a real read */
	   if ((name == BT_TMI_S_XFER_FIELD)    || 
	       (name == BT_TMI_S_XFER_DONE)     ||
	       (name == BT_TMI_S_INTRPT_ENABLE) ||
	       (name == BT_TMI_S_SWAP_STAT)     ||
	       (name == BT_TMI_S_VID_IN_FIELD)) {
	      cur_val = mgv_tmiind_hwread(mgv_bd,offset);
	      rw = VID_READ;
	      }
	   else cur_val = mgv_tmiind(mgv_bd, offset, 0, VID_READ);

	   if (rw == VID_WRITE) {
	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_tmiind(mgv_bd, offset, cur_val, VID_WRITE);
	      return(val);
	   } 
 	   else {
	      cur_val &= (mask << shift);
	      return(cur_val >> shift);
	   }
	}
	/*
	 * AB direct registers
	 */
	if (((name >= BT_AB_DIRECT_MIN) && (name <= BT_AB_DIRECT_MAX)) ||
	     (name == BT_D1_ENABLE)) {
	   cur_val = mgv_abdir(mgv_bd, offset, 0, VID_READ);
	   if (rw == VID_WRITE) {
	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_abdir(mgv_bd, offset, cur_val, VID_WRITE);
	      return(val);
	   } 
	   else {
	      cur_val &= (mask << shift);
	      return(cur_val >> shift);
	   }
	}

	/* 
	 * AB internal registers
	 */
	if ((name >= BT_AB_INTERNEL_MIN) && (name <= BT_AB_INTERNEL_MAX)) {
	   cur_val = mgv_abind(mgv_bd, AB_INT_REG, offset, 0, VID_READ);
	   if (rw == VID_WRITE) {
	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_abind(mgv_bd, AB_INT_REG, offset, cur_val, VID_WRITE);
	      return(val);
	   }
	   else {
	      cur_val &= (mask << shift);
	      return(cur_val >> shift);
	   }
	}

	/*
	 * AB test registers 
	 */
	if ((name >= BT_AB_TEST_MIN) && (name <= BT_AB_TEST_MAX)) {
	   cur_val = mgv_abind(mgv_bd, AB_TEST_REGS, offset, 0, VID_READ);
	   if (rw == VID_WRITE) {
	      cur_val &= (0xff & ~(mask << shift));
	      cur_val |= (val << shift);
	      mgv_abind(mgv_bd, AB_TEST_REGS, offset, cur_val, VID_WRITE);
	      return(val);
	   } 
	   else {
	      cur_val &= (mask << shift);
	      return(cur_val >> shift);
	   }
	}

#endif
	return(0);
}

int check_reference(void)
{
	int source, status, low_byte, hi_byte, tmp;

	msg_printf(DBG,"Checking gen-lock reference:\n");

	GAL_IND_R1(mgvbase, GAL_REF_CTRL, source);
	msg_printf(DBG,"after GAL_IND_R1 ");

	GAL_IND_R1(mgvbase, GAL_GENLOCK_STATUS, status);

	/* Source is loop-thru, input #2 or input #1, and it's unlocked */
	if (source && (status&0x1))
		return(1);

	/* Read VCXO Free run frequency (DAC) high and low bytes */
	GAL_IND_R1(mgvbase, GAL_VCXO_LO, low_byte);
	GAL_IND_R1(mgvbase, GAL_VCXO_HI, hi_byte);

	/* Read VCXO READ (ADC) connection mode */
	GAL_IND_R1(mgvbase, GAL_ADC_MODE, tmp);

	/*  Read VCXO low and hi bytes from status registers */
	GAL_IND_R1(mgvbase, GAL_ADC_LO, low_byte);
	GAL_IND_R1(mgvbase, GAL_ADC_HI, hi_byte);

	return(0);
}

unsigned char*
_mgv_PageAlign(unsigned char *pBuf)
{
    __paddr_t		tmp;
    unsigned char 	*retPtr = pBuf;

    if (((__paddr_t)pBuf) & (mgvbase->pageSize - 1)) {
    	tmp = ((__paddr_t)pBuf) + mgvbase->pageSize;
    	tmp &= mgvbase->pageMask;
	retPtr = (unsigned char *) tmp;
    } 
    return (retPtr);
}

__uint32_t
_mgv_CC1PollReg(unsigned char reg, unsigned char exp, unsigned char mask,
	__uint32_t timeOut, unsigned char* regVal)
{
    unsigned char val, val_masked;

	do 
	{
    		CC_W1(mgvbase, CC_INDIR1, reg);
    		CC_W1(mgvbase, CC_INDIR2, exp);
		delay( 250 );
    		CC_W1(mgvbase, CC_INDIR1, reg);
    		CC_R1(mgvbase, CC_INDIR2, val);
		val_masked = (val & mask);
		timeOut--;
    	} while (timeOut && (val_masked != exp));

	regVal = &val_masked;

	if (timeOut) 
	{
	    return(0x0);
	} 
	else 
	{
	    return(0x1);
#if 0
			msg_printf(ERR, "\n_mgv_CC1PollReg:- reg %2d; exp 0x%2x; mask 0x%2x; val 0x%2x; masked_val 0x%2x\n", reg, exp, mask, val, val_masked);
#endif
	}
}


__uint32_t
_mgv_CC1PollFBRdy(__uint32_t timeOut )
{
    unsigned char val; 

	do 
	{
    		CC_R1(mgvbase, CC_FRAME_BUF, val);
		timeOut--;
   	} while (timeOut && !(val & 0x80) );

	if (timeOut) 
	{
		return(0x0);
	} 
	else 
	{
		msg_printf(ERR, "\nCC1 FB not ready");
		return(0x1);
	}
}


void _mgv_delay_dots( unsigned char seconds, unsigned char newline )
{
unsigned char i;
	if( newline )
		msg_printf(JUST_DOIT,"\n");
	for(i=0; i < seconds; i++)
	{
		delay( 1000 );
		msg_printf(JUST_DOIT,".");
	}
}

void _mgv_delay_countdown( unsigned char seconds, unsigned char newline, unsigned char overlay )
{
unsigned char i;
	if( newline )
		msg_printf(JUST_DOIT,"\n");
	for(i= seconds; i > 0; i--)
	{
		delay( 1000 );
		msg_printf(JUST_DOIT,"%s%3d", overlay ? "\b\b\b": 
				(((seconds-i)% 25) ? "  ": " \n"), i );
	}
}

/**********************************************************************
*
*       Function: _mgv_check_vin_status(unsigned char ch_sel, tv_std)
*
*
*       Return: 0 - channel input active & genlocked for tv_std
*                  -1 - error from no input, no genlock, etc.
*
*
***********************************************************************/
int _mgv_check_vin_status(unsigned char ch_sel, unsigned char tv_std)
{
unsigned char reg_val, i, first_time = TRUE;

        msg_printf(JUST_DOIT,"\nCHECKING FOR VIDEO INPUT STATUS");
        /* check for video input status */
        for(i = 0; i < 10; i++)
        {
                if(ch_sel==ONE)
		{
                        GAL_IND_R1(mgvbase, GAL_VIN_CH1_STATUS, reg_val);
		}
                else
                        GAL_IND_R1(mgvbase, GAL_VIN_CH2_STATUS, reg_val);

                if( (reg_val & 0x10) == 0)
                                break;
		else if (first_time == TRUE)
		{
			msg_printf(JUST_DOIT,"...PLEASE CONNECT CABLE(S)");
			first_time = FALSE;
		}
		delay( 1000 );
		msg_printf(JUST_DOIT,".");
        }

        /* display failed result messages */
        if(reg_val & 0x10)
        {
               	msg_printf(SUM,"\nCONNECT THE VIDEO SOURCE TO CHANNEL %s AND REPEAT THIS TEST\n", (ch_sel==ONE) ? "ONE":"TWO");
               	return(-1);
        }
        else if( (reg_val & 0x03) != tv_std )
        {
                msg_printf(SUM,"\nSET THE VIDEO STANDARD TO %s AND REPEAT THIS TEST\n", asc_tv_std_p[ tv_std ]);
                return(-1);
        }
        else if(reg_val & 0x10)
        {
                msg_printf(SUM,"\nAUTOPHASE NOT LOCKED,CHECK GENLOCK STATUS, AND REPEAT THIS TEST\n");
                return(-1);
        }
#if 0
        else if(reg_val & 0x20)
        {
                msg_printf(JUST_DOIT,"\nCHANNEL %s INPUT OUT OF RANGE.\
                        CHECK SETTINGS AND REPEAT THIS TEST\n",\
                        (ch_sel==ONE) ? "ONE":"TWO");
                return(-1);
        }
#endif

return(0);
}
/**************************************************************************
*
*	Function: _mgv_setup_VGIDMA(unsigned char channel, unsigned char tv_std,
*				unsigned char vidFmt, unsigned char* pBuf,
*				unsigned char op, unsigned int vout_freeze)
*
*	Returns: None.
*
***************************************************************************/
void _mgv_setup_VGI1DMA(unsigned char channel, unsigned char tv_std,
			unsigned char vidFmt, unsigned char fields,
			unsigned char* pBuf, 
			unsigned char op, unsigned int vout_freeze)
{
	mgvbase->chan = channel;
	mgvbase->tvStd = tv_std;
	mgvbase->vidFmt = vidFmt;
	mgvbase->nFields = fields;
	mgvbase->pVGIDMABuf = (uchar_t *)pBuf;
	mgvbase->VGIDMAOp = op;
	mgvbase->vout_freeze = vout_freeze;
}


__uint32_t
mgv_waitForSpace(__uint32_t argc, char **argv)
{
    __uint32_t secs, i;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 's':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &secs);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &secs);
			}
                        break;
        }
        argc--; argv++;
    }

    msg_printf(SUM, "\n");
    for (i = 0; i < secs; i++) {
	msg_printf(SUM, ".");
	if ((pause(2, " ", " ")) != 2) return (0);
	delay (1000);
    }
    msg_printf(SUM, "\n");
    return(-1);

}


#ifdef IP30
/*
 * The following set of functions deals with reading and writing
 * data to EDH thru I2C bus (used only for speed racer)
 */


int
_mgv_edh_w1(uchar_t reg, uchar_t val)
{
	int error = 0;

	msg_printf(DBG, "Entering _mgv_edh_w1...\n");

	/* First set value needed */
	shadow_edh_iic_write[reg-1] = val;
	error = mgv_I2C_edh_seq(EDH_ADDR, (reg-1), val, MGV_EDH_WRITE);

	msg_printf(DBG, "Leaving _mgv_edh_w1...\n");

    return (error);
}


int
_mgv_edh_r1(uchar_t reg, uchar_t *addr)
{
	int val = 0, t_val = 0;

	msg_printf(DBG, "Entering _mgv_edh_r1...\n");

	val = mgv_I2C_edh_seq(EDH_ADDR, (reg-1), t_val, MGV_EDH_READ);
	*addr = shadow_edh_iic_read[reg-1];
	msg_printf(DBG, "Leaving _mgv_edh_r1...\n");

        return (val);
}

/*** Following structures are for bridge initialization ***/

/*
 * DMA Dirmap attributes for Command (desc) channel
 */
#define DEVICE_DMA_ATTRS_CMD                    ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        BRIDGE_DEV_RT                     | \
        /* BRIDGE_DEV_PREF */ 0                           | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

/*
 * DMA Dirmap attributes for Data channel
 */
#define DEVICE_DMA_ATTRS_DATA                   ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        BRIDGE_DEV_RT                     | \
        BRIDGE_DEV_PREF                            | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

/* 3 bridge slots in hipri-ring for Max PIO priority over DMAs */
#define GIOBR_ARB_RING_BRIDGE   (BRIDGE_ARB_REQ_WAIT_TICK(2) | \
				 BRIDGE_ARB_REQ_WAIT_EN(0xFF) | \
                                 BRIDGE_ARB_HPRI_RING_B0 )

struct giobr_bridge_config_s srv_bridge_config =  
{
    GIOBR_ARB_FLAGS | GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV,
    GIOBR_ARB_RING_BRIDGE,
    /* even devices, 0,2,4,6        odd devices 1,3,5,7 */
     {  DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
	DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD,
	0,                          0,
	0,                          0},
     {  7,                              7,
        1,                              1,
        0,                              0,
        0,                              0},
    /* {0,0,0,0,4,0,6,0} */  /* for interrupts from cc1 & vgi1  */
    {0,0,0,0,4,0,0,0} /* no interrupts from cc1 */
};

/*
 * BRIDGE ASIC configuration for two VGI1 case 
 */

struct giobr_bridge_config_s srv_bridge_config_2vgi1 =  
{
    GIOBR_ARB_FLAGS | GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV,
    GIOBR_ARB_RING_BRIDGE,
    /* even devices, 0,2,4,6        odd devices 1,3,5,7 */
     {  DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
	DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD,
	DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
	DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD},
     {  3,                              3,
        1,                              1,
        3,                              3,
        1,                              1},
    {7,7,7,7,2,3,6,7}
};

/*
 *    mgv_br_rrb_alloc: allocate some additional RRBs
 *      for the specified slot. Returns 1 if there were
 *      insufficient free RRBs to satisfy the request,
 *      or 0 if the request was fulfilled.
 *
 *      Note that if a request can be partially filled,
 *      it will be, even if we return failure.
 *
 *      IMPL NOTE: again we avoid iterating across all
 *      the RRBs; instead, we form up a word containing
 *      one bit for each free RRB, then peel the bits
 *      off from the low end.
 */


 __uint32_t
mgv_br_rrb_alloc(gioio_slot_t slot, int more)
{
    int                     oops = 0;
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp, bit;

    if (slot & 1) {
      BR_REG_RD_32(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      BR_REG_RD_32(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    tmp = (0x88888888 & ~reg) >> 3;
    while (more-- > 0) {
        bit = LSBIT(tmp);
        if (!bit) {
            oops = 1;
            break;
        }
        tmp &= ~bit;
        reg = ((reg & ~(bit * 15)) | (bit * (8 + slot / 2)));
    }
    if (slot & 1) {
      BR_REG_WR_32(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      BR_REG_WR_32(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    return oops;
}

#endif /* IP30 */

