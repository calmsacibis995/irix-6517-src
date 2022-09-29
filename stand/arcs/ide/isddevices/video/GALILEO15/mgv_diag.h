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

#ifndef _MGV_DIAG_H_

#define _MGV_DIAG_H_

#if (defined(IP28) || defined(IP30))
#define __paddr_t               __uint64_t
#else
#define __paddr_t               __uint32_t
#endif


/*
 * The order in which to add stuff in this header file is:
 * o include file declaration
 * o constants
 * o type definitions and prototypes
 * o external variable declaration
 * o external function declaration
 * o macros
 */

/* INCLUDE FILE DECLARATION */
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/i8254clock.h"
#include "sys/time.h"
#include "sys/ktime.h"
#include "sys/ksynch.h"
#include "sys/sema.h"
#include "sys/cpu.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "sys/mgv_extern.h"
#include "sys/mgv_vgi1_hw.h"
#include "ide_xbow.h"
#include "sys/errno.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"		/* for ASSERT() */
#ifdef IP30
#include "sys/GIO/giobr.h"     /* for all the structure defs */
#include "sys/PCI/bridge.h"
#endif

/* CONSTANTS */
#define	MGV_GAL15_INIT_NUM	 1
#define	MGV_EDH_INIT_NUM	 2
#define	MGV_XPOINT_INIT_NUM	 3
#define	MGV_INPUT_INIT_NUM	 4
#define	MGV_VIDEO_IN_INIT_NUM	 5
#define	MGV_VIDEO_OUT_INIT_NUM	 6
#define	MGV_GENLOCK_INIT_NUM	 7
#define	MGV_CC1_INIT_NUM	 8
#define	MGV_AB1_INIT_NUM	 9
#define	MGV_CSC_INIT_NUM	10
#define	MGV_TMI_INIT_NUM	11
#define	MGV_CC1_INT_INIT_NUM	12
#define	MGV_VGI1_1_INIT_NUM	13
#define	MGV_VGI1_2_INIT_NUM	14
#define	MGV_I2C_INIT_NUM	15
#define	MGV_LAST_DEVICE_NUM	15

#define	MAX_XTALK_PORT_INFO_INDEX 	7

/*
 * Board Constants 
 */
#define	MGV_BASE0_PHYS       0x1f062800L   /* Gfx base */
#define AB_OFFSET       0x0
#define CC_OFFSET       0x800
#define GAL_OFFSET      0xc00
#define CSC_TMI_OFFSET  0x1000

/*
 * Utility Constants
 */
#define EIGHTBIT                8
#define TENBIT                  10
#define WALK1                   0x01
#define WALK0                   0xFE

/*
 * GALILEO 1.5 Direct register numbers
 */
#define GAL_DIR0              0  /* Gal1.5 indirect address register */
#define GAL_DIR1              1  /* Gal1.5 indirect data register */
#define GAL_DIR2              2  /* Gal1.5 indirect for Gal/TMI/CSC rev # */
#define GAL_DIR3              3  /* S/W reset register */
#define GAL_DIR4              4  /* Unused */
#define GAL_DIR5              5  /* Scaler Initialization */
#define GAL_DIR6              6  /* TMI/Scaler address register */
#define GAL_DIR7              7  /* TMI/Scaler data register */

/*
 * GALILEO 1.5 Indirect register numbers
 */
#define GAL_TV_STD                      0x0
#define GAL_FIRMWARE                    0x1 /* Gal 1.5 firmware rev only */
#define GAL_VIN_CTRL                    0x2
#define GAL_VIN_CH1_STATUS              0x3
#define GAL_VIN_CH2_STATUS              0x4
#define GAL_CC1_IO_CTRL5                0x5
#define GAL_CC1_IO_CTRL6                0x6
#define GAL_CC1_GFX_TOP_BLANK           0x7
#define GAL_CH1_HPHASE_LO               0x8
#define GAL_CH1_HPHASE_HI               0x9
#define GAL_CH2_HPHASE_LO               0xa
#define GAL_CH2_HPHASE_HI               0xb
#define GAL_CH1_CTRL                    0xc
#define GAL_CH2_CTRL                    0xd
#define GAL_VCXO_LO                     0xe
#define GAL_VCXO_HI                     0xf
#define GAL_REF_CTRL                    0x10
#define GAL_GENLOCK_STATUS              0x11
#define GAL_MUX_ADDR                    0x12 /* Galmux indirect real addr */
#define GAL_TRIG_CTRL                   0x13
#define GAL_REF_OFFSET_LO               0x14
#define GAL_REF_OFFSET_HI               0x15
#define GAL_ADC_MODE                    0x16
#define GAL_ADC_LO                      0x17
#define GAL_ADC_HI                      0x18
#define GAL_TRIG_STATUS                 0x19
#define GAL_VGI1_RESET                  0x1e

/*** Additional indirect register for IP30 ***/
#ifdef IP30
#define GAL_GFX_FRAME_RESET             0x1a
#define GAL_EDH_FLAGS1                  0x1b
#define GAL_EDH_FLAGS2                  0x1c
#define GAL_EDH_FLAGS3                  0x1d
#define GAL_EDH_FLAGS4                  0x1e
#define GAL_EDH_INTR                    0x1f
#define GAL_EDH_DEVADDR                 0x20
#define GAL_KSYNC_CTRL                  0x21
#define GAL_KSYNC_OFFSET_LO             0x22
#define GAL_KSYNC_OFFSET_HI             0x23
#define GAL_IIC_DATA                    0x24
#define GAL_IIC_CTL                     0x25
#define GAL_IIC_RESET                   0x26
#define GAL_30_60_REFRESH               0x27


/*
 * Gennum GS9001 EDH Coprocessor I2C registers
 */
#define NUM_EDH_PARTS 4
#define MGV_EDH_PART_INDEX(channel, part)  (channel + 2*part)


/*
 * Data format for read words
 */
#define EDH_ERROR0_FLAGS_READ           0x0
#define EDH_ERROR1_FLAGS_READ           0x1
#define EDH_STD_ERR2_CNT_READ           0x2
#define EDH_ERROR1_CNT_READ             0x3
#define EDH_ERROR0_CNT_READ             0x4
#define EDH_ACTIVE1_CNT_READ            0x5
#define EDH_ACTIVE0_CNT_READ            0x6
#define EDH_FIELD1_CRC_READ             0x7
#define EDH_FIELD0_CRC_READ             0x8
#define EDH_RESERVED_1_2_READ           0x9
#define EDH_RESERVED_2_3_READ           0xa
#define EDH_RESERVED_3_4_READ           0xb
#define EDH_RESERVED_5_6_READ           0xc
#define EDH_RESERVED_6_7_READ           0xd
#define EDH_RESERVED_7_READ             0xe


/*
 * Data format for write words only
 */
#define EDH_ERROR0_FLAGS_WRITE          0x0
#define EDH_ERROR1_FLAGS_WRITE          0x1
#define EDH_MASK0_STATUS_WRITE          0x2
#define EDH_MASK1_STATUS_WRITE          0x3
#define EDH_SEN0_STATUS_WRITE           0x4
#define EDH_SEN1_STATUS_WRITE           0x5
#define EDH_STD_SELECT_WRITE            0x6
#define EDH_ERROR0_CNT_WRITE            0x7
#define EDH_ACTIVE1_CNT_WRITE           0x8
#define EDH_ACTIVE0_CNT_WRITE           0x9
#define EDH_RESERVED_1_2_WRITE          0xa
#define EDH_RESERVED_2_3_WRITE          0xb
#define EDH_RESERVED_4_5_WRITE          0xc
#define EDH_RESERVED_5_6_WRITE          0xd
#define EDH_RESERVED_6_7_WRITE          0xe

#endif
/*
 * AB1 Direct register numbers 
 */
#define AB_EXT_MEM              0
#define AB_INT_REG              1
#define AB_TEST_REGS            2
#define AB_LOW_ADDR             3
#define AB_HIGH_ADDR            4
#define AB_SYSCON               5
#define AB_CS_ID                6

/*
 *	AB1 Indirect registers
 */
#define AB_A_X_OFFSET_HI		0x0
#define AB_A_X_OFFSET_LO		0x1
#define AB_A_Y_OFFSET_HI		0x2
#define AB_A_Y_OFFSET_LO		0x3
#define AB_A_XMOD			0x4
#define AB_A_WIN_STATUS			0x5
#define	AB_A_VIDEO_INFO			0x6
#define AB_A_DECIMATION			0x7

#define AB_B_X_OFFSET_HI		0x10
#define AB_B_X_OFFSET_LO		0x11
#define AB_B_Y_OFFSET_HI		0x12
#define AB_B_Y_OFFSET_LO		0x13
#define AB_B_XMOD			0x14
#define AB_B_WIN_STATUS			0x15
#define	AB_B_VIDEO_INFO			0x16
#define AB_B_DECIMATION			0x17

#define AB_C_X_OFFSET_HI		0x20
#define AB_C_X_OFFSET_LO		0x21
#define AB_C_Y_OFFSET_HI		0x22
#define AB_C_Y_OFFSET_LO		0x23
#define AB_C_XMOD			0x24
#define AB_C_WIN_STATUS			0x25
#define	AB_C_VIDEO_INFO			0x26

#define AB_TEST_RAMTEST			0x0
#define AB_TEST_A_CYCLE			0x1
#define AB_TEST_B_CYCLE			0x2
#define AB_TEST_CHIPREV			0x3

/*
 * AB1 Memory test constants
 */
#define	AB1_MAX_TRYCNT		10
#define	AB1_RED_CHAN		0x0
#define	AB1_GRN_CHAN		0x8
#define	AB1_BLU_CHAN		0x10
#define	AB1_ALP_CHAN		0x18
#define	AB1_ADDR_SIZE		0xffff

/*
 * CC1 Direct register numbers
 */
#define CC_SYSCON               0
#define CC_INDIR1               1
#define CC_INDIR2               2
#define CC_FRAME_BUF            3
#define CC_FLD_FRM_SEL          4
#define CC_FRM_BUF_DATA         5
#define CC_IIC_CNTL             6
#define CC_IIC_DATA             7

/*
 *	CC1 Indirect Registers
 */
#define CC_G_CHAN_A_SETUP	0	/* Graphics channel A setup */
#define CC_G_CHAN_A_LINE_1	1	/* Graphics A first line */
#define CC_G_CHAN_B_SETUP	2	/* Graphics B setup */
#define CC_G_CHAN_BC_LINE_1	3	/* B & C first line */
#define CC_G_CHAN_C_SETUP	4	/* C setup */

#define CC_A_LEFT		8	/* Window A left edge */
#define CC_A_RIGHT		9
#define CC_A_BLACK		10	/* Window A blackout */
#define CC_A_TOP		11	/* top line number */
#define CC_A_BOTTOM		12
#define CC_A_PHS_DET_HI		13	/* phase detector thresh, high bits */
#define CC_A_PHS_DET_LO		14

#define CC_B_LEFT		16
#define CC_B_RIGHT		17
#define CC_B_BLACK		18
#define CC_B_TOP		19
#define CC_B_BOTTOM		20
#define CC_B_PHS_DET_HI		21
#define CC_B_PHS_DET_LO		22

#define CC_C_LEFT		24
#define CC_C_RIGHT		25
#define CC_C_BLACK		26
#define CC_C_TOP		27
#define CC_C_BOTTOM		28
#define CC_C_PHS_DET_HI		29
#define CC_C_PHS_DET_LO		30

#define CC_MISC			32	/* Various setups */

#define CC_V_GEN_UP		33	/* Video genlock timing params */
#define CC_V_GEN_HOR		34
#define CC_V_GEN_VERT		35

#define CC_V_NONBLANK		36	/* first unblanked video output line */
#define CC_GENERAL_OUT		37	/* General output bits */

#define CC_INT_CTRL		38	/* interrupt control and status */

#define CC_CUR_VIDEO_LINE	39	/* current video line num */

#define CC_IN_INT_HI		40	/* input interupt line num, high bits */
#define CC_IN_INT_LO		41
#define CC_OUT_INT_HI		42	/* output int line num, high bits */
#define CC_OUT_INT_LO		43
#define CC_IN_LINE_HI		44	/* input line number, high bits */
#define CC_IN_LINE_LO		45
#define CC_OUT_LINE_HI		46 	/* output line number, hi bits */
#define CC_OUT_LINE_LO		47

#define CC_FRM_BUF_SETUP	48
#define CC_FRM_BUF_PTR		49

#define CC_ALPHA_BKGRD		50
#define CC_ALPHA_Y		51
#define CC_ALPHA_U		52
#define CC_ALPHA_V		53

#define CC_CC1_1		54
#define CC_CC1_2		55
#define CC_CC1_3		56
#define CC_CC1_4		57
#define CC_CC1_5		58


#define CC_ALPHA_COMPOS		59	/* Alpha blender compositing mode */

#define CC_KEY_GEN_1		64
#define CC_KEY_GEN_2		65
#define CC_KEY_GEN_3		66
#define CC_KEY_GEN_4		67
#define CC_KEY_GEN_5		68
#define CC_KEY_GEN_6		69
#define CC_KEY_GEN_7		70
#define CC_KEY_GEN_8		71
#define CC_KEY_GEN_9		72
#define CC_KEY_GEN_10		73
#define CC_KEY_GEN_11		74
#define CC_KEY_GEN_12		75
#define CC_KEY_GEN_13		76
#define CC_KEY_GEN_14		77
#define CC_KEY_GEN_15		78

#define CC_ALPHA_SHADOW_1	79 	/* Alpha blender shadow scaling */
#define CC_ALPHA_SHADOW_2	80

#define CC_VERS_ID		128	/* ID number */
/*
 *  CSC Registers
 */
#define CSC_ADDR              0
#define CSC_DATA              1
#define CSC_RESET             2


/*
 *  CSC Indirect Registers
 */

#define CSC_SELECT_CTRL         1
#define CSC_INDIR_LUTWRITE    	2
 

/*
 *      Modes used for CSC tests
 */

#define CSC_LUT_PASSTHRU                30
#define CSC_LUT_WALKING_POSITIVE        31
#define CSC_LUT_WALKING_NEGATIVE        32
#define CSC_LUT_ALL1                    33
#define CSC_LUT_YUV2RGB                 34
#define CSC_LUT_RGB2YUV                 35
#define CSC_LUT_CCIR2FULLSCALE          36
#define CSC_X2KLUT_PASSTHRU             37
#define CSC_X2KLUT_YUV2RGB              38
#define CSC_X2KLUT_RGB2YUV              39
#define CSC_X2KLUT_CCIR2FULLSCALE       40
#define CSC_COEF_YG_PASSTHRU            41
#define CSC_COEF_UB_PASSTHRU            42
#define CSC_COEF_VR_PASSTHRU            43
#define CSC_COEF_ALL_PASSTHRU           44
#define CSC_COEF_YUV2RGB                45
#define CSC_COEF_RGB2YUV                46
#define CSC_COEF_CCIR2FULLSCALE         47
#define CSC_LUT_WALKING10bit            48
#define CSC_LUT_WALKING_ALL             49

#define CSC_LUT_MAXLOGIC_SPECIAL        50
#define CSC_COEF_MAXLOGIC_SPECIAL       51
#define CSC_X2KLUT_MAXLOGIC_SPECIAL     52

/* bit field 0 & 1 of csc ind addr 1 */

#define	CSC_NORMAL_MODE		0 
#define	CSC_LOAD_COEF_MODE	1 
#define	CSC_LOAD_LUT_MODE	2
#define	CSC_READ_LUT_MODE	3

/* bit field 2 of csc ind addr 1 */

#define	CSC_RESET_NORMAL		0 
#define	CSC_RESET_AUTO_INCREMENT	1

/* IP30 Specific     */
#ifdef IP30

/*
 * TMI Registers
 */
#define TMI_ADDR              GAL_DIR6
#define TMI_DATA              GAL_DIR7
#else
#define TMI_ADDR              0
#define TMI_DATA              1
#define TMI_RESET             2
#endif

/*
 *  Scaler Registers
 */

#define SCL_ADDR		   GAL_DIR6
#define SCL_DATA		   GAL_DIR7

#define SCALE1_MAX_REG_ADDR        0x11  /*Address of last indir scaler_1 registers*/
#define SCALE2_MIN_REG_ADDR	   0x20  /*Address of first indir scaler_1 registers*/
#define SCALE2_MAX_REG_ADDR	   0x31  /*Address of last indir scaler_1 registers*/


/*
 *      Indirect TMI ASIC registers
 */
#define TMI_MODE_STAT                   0x80    /* ASIC registers */
#define TMI_STATUS                      0x81
#define TMI_LEFT_START_MS               0x82
#define TMI_LEFT_START_LS               0x83
#define TMI_HIN_LENGTH_MS               0x84
#define TMI_HIN_LENGTH_LS               0x85
#define TMI_VIN_DELAY_MS                0x86
#define TMI_VIN_DELAY_LS                0x87
#define TMI_VOUT_DELAY_MS               0x88
#define TMI_VOUT_DELAY_LS               0x89
#define TMI_PIX_COUNT_HIGH              0x8a
#define TMI_PIX_COUNT_MID               0x8b
#define TMI_PIX_COUNT_LOW               0x8c
#define TMI_HOUT_LENGTH_MS              0x8d
#define TMI_HOUT_LENGTH_LS              0x8e
#define TMI_TEST_ADDR_MS                0x8f
#define TMI_TEST_ADDR_LS                0x90
#define TMI_TEST_DATA                   0x91
#define TMI_TEST_MISC                   0x92
#define TMI_CHIP_ID                     0x93
#define TMI_HBLNK_DLY_COUNT_MS          0x94
#define TMI_HBLNK_DLY_COUNT_LS          0x95

/*
 *      Indirect Scaler registers
 *      Scaler 1 & 2 write only (duplicate of 0x40 - 0x51 entries)
 *      Used by TMI software
 */
#define TMI_Y_VSRC_CNT_LSB              0x60    /* scaler registers */
#define TMI_Y_VSRC_CNT_MSB              0x61
#define TMI_Y_HSRC_CNT_LSB              0x62
#define TMI_Y_HSRC_CNT_MSB              0x63
#define TMI_Y_VTRG_CNT_LSB              0x64
#define TMI_Y_VTRG_CNT_MSB              0x65
#define TMI_Y_HTRG_CNT_LSB              0x66
#define TMI_Y_HTRG_CNT_MSB              0x67
#define TMI_UV_VSRC_CNT_LSB             0x68
#define TMI_UV_VSRC_CNT_MSB             0x69
#define TMI_UV_HSRC_CNT_LSB             0x6a
#define TMI_UV_HSRC_CNT_MSB             0x6b
#define TMI_UV_VTRG_CNT_LSB             0x6c
#define TMI_UV_VTRG_CNT_MSB             0x6d
#define TMI_UV_HTRG_CNT_LSB             0x6e
#define TMI_UV_HTRG_CNT_MSB             0x6f
#define TMI_VIN_DELAY_ODD               0x70
#define TMI_VIN_DELAY_EVEN              0x71


/*
 * Constants used in TMI tests
 */
#define TMI_MAX_SRAM_SIZE		0x8000 /* 32 K */
#define	TMI_TEST_MODE_BIT		(1 << 0)
#define	TMI_TEST_WRITE_BIT		(1 << 1)
#define	TMI_TEST_READ_BIT		(1 << 2)
#define	TMI_TEST_TE_BIT			(1 << 3)
#define	TMI_ADDR0_TYPE			0
#define	TMI_ADDR1_TYPE			1
#define	TMI_PATRN_TYPE			2

/*
 * VGI1 Register definitions
 */
#define VGI1_GIO_ID          0x00000000
#define VGI1_SYS_MODE        0x00000010
#define VGI1_CHIP_ID         0x00000020
#define VGI1_CH1_SETUP       0x00001000
#define VGI1_CH1_TRIG        0x00001200
#define VGI1_CH1_STATUS      0x00001300
#define VGI1_CH1_INT_ENAB    0x00001010
#define VGI1_CH1_H_PARAM     0x00001020
#define VGI1_CH1_V_PARAM     0x00001030
#define VGI1_CH1_DATA_ADR    0x00001210
#define VGI1_CH1_DESC_ADR    0x00001220
#define VGI1_CH1_DMA_STRIDE  0x00001040
#define VGI1_CH1_DMA_DEBUG   0x00001100
#define VGI1_CH2_SETUP       0x00002000
#define VGI1_CH2_TRIG        0x00002200
#define VGI1_CH2_STATUS      0x00002300
#define VGI1_CH2_INT_ENAB    0x00002010
#define VGI1_CH2_H_PARAM     0x00002020
#define VGI1_CH2_V_PARAM     0x00002030
#define VGI1_CH2_DATA_ADR    0x00002210
#define VGI1_CH2_DESC_ADR    0x00002220
#define VGI1_CH2_DMA_STRIDE  0x00002040
#define VGI1_CH2_DMA_DEBUG   0x00002100

/*
 * MUX (VBAR) Register Definitions
 */
#define MUX_CC1_CH1_IN              0xf0
#define MUX_CC1_CH2_IN              0xf1
#define MUX_CSC_A                   0xf2
#define MUX_CSC_B                   0xf3
#define MUX_EXP_D_OUT               0xf4
#define MUX_VGI1_CH1_IN             0xf5
#define MUX_VGI1_CH2_IN             0xf6
#define MUX_D1_CH2_OUT              0xf7
#define MUX_D1_CH1_OUT              0xf8
#define MUX_TMI_YUV_IN              0xf9
#define MUX_TMI_ALPHA_IN            0xfa
#define MUX_EXP_SYNC_OUT            0xfb
#define MUX_EXP_A_OUT               0xfe
#define MUX_EXP_B_OUT               0xff

/*
 * Constants used in VGI1 tests
 */
#define	VGI1_CHIP_ID_MASK		0x1
#define	VGI1_CHIP_ID_VAL		0x1
#define	VGI1_POLL_TIME_OUT		0x100
#define	VGI1_DMA_DONE_DELAY		31
#define	VGI1_CH_FLD_CNT_MASK		0x7fff
#define	VGI1_CHANNEL_1			0x0
#define	VGI1_CHANNEL_2			0x1
#define	VGI1_CC1_DATA_PATH		0x2
#define	VGI1_NTSC_CCIR601		0x0
#define	VGI1_NTSC_SQUARE		0x1
#define	VGI1_PAL_CCIR601		0x2
#define	VGI1_PAL_SQUARE			0x3
#define	VGI1_POLL_SMALL_TIME_OUT	VGI1_POLL_TIME_OUT
#define	VGI1_POLL_LARGE_TIME_OUT	0x300000
#define	VGI1_PAGE_SIZE_4K_BITS		12
#define	VGI1_PAGE_SIZE_16K_BITS		14
#define	VGI1_DMA_READ			0
#define	VGI1_DMA_WRITE			1
#define	VGI1_NO_VIDEO_INPUT		0xfff

/*
 * VGI1 - GIO Device Addr
 * XXX: Please verify the addresses for both VGI1s
 */
#define	MGV_VGI1_1_DEV_ADDR	BRIDGE_DEVIO0
#define	MGV_VGI1_2_DEV_ADDR	BRIDGE_DEVIO1

/*
 * Crosspoint output switch choices for Galileo 1.5.
 */
#define GAL_OUT_CH2_CC1         0
#define GAL_OUT_CH1_CC1         1
#define GAL_OUT_CH1_CSC         2
#define GAL_OUT_CH2_CSC         3
#define GAL_OUT_EXP_D           4
#define GAL_OUT_CH1_GIO         5
#define GAL_OUT_CH2_GIO         6
#define GAL_OUT_CH2             7
#define GAL_OUT_CH1             8
#define GAL_OUT_TMI_YUV         9
#define GAL_OUT_TMI_ALPHA       10
#define GAL_OUT_EXP_SYNC        11
#define GAL_OUT_EXP_A           14
#define GAL_OUT_EXP_B           15

/*
 * Select any of these as an input to Galileo 1.5
 * crosspoint, (VBAR1), output switches.
 */
#define	GAL_XPOINT_IN_SHIFT		4
#define GAL_CSC_CH1_OUT_SHIFT		(CSC_CH1_OUT 	<< GAL_XPOINT_IN_SHIFT)
#define GAL_DIGITAL_CH1_IN_SHIFT	(DIGITAL_CH1_IN	<< GAL_XPOINT_IN_SHIFT)
#define GAL_CSC_CH2_OUT_SHIFT		(CSC_CH2_OUT	<< GAL_XPOINT_IN_SHIFT)
#define GAL_DIGITAL_CH2_IN_SHIFT	(DIGITAL_CH2_IN	<< GAL_XPOINT_IN_SHIFT)
#define GAL_EXP_C_IN_SHIFT		(EXP_C_IN	<< GAL_XPOINT_IN_SHIFT)
#define GAL_VGI1_CH1_OUT_SHIFT		(VGI1_CH1_OUT	<< GAL_XPOINT_IN_SHIFT)
#define GAL_VGI1_CH2_OUT_SHIFT		(VGI1_CH2_OUT	<< GAL_XPOINT_IN_SHIFT)
#define GAL_BLACK_REF_IN_SHIFT		(BLACK_REF_IN	<< GAL_XPOINT_IN_SHIFT)
#define GAL_CC1_CH1_OUT_SHIFT		(CC1_CH1_OUT	<< GAL_XPOINT_IN_SHIFT)
#define GAL_CC1_CH2_OUT_SHIFT		(CC1_CH2_OUT	<< GAL_XPOINT_IN_SHIFT)
#define GAL_EXP_A_IN_SHIFT		(EXP_A_IN	<< GAL_XPOINT_IN_SHIFT)
#define GAL_EXP_B_IN_SHIFT		(EXP_B_IN	<< GAL_XPOINT_IN_SHIFT)

/*
 * Constants used in CC1 tests
 */
#define	CC1_PIXELS_PER_LINE	1280/4
#define	CC1_LINE_COUNT		320

/* Center value for Horizontal Phase. */
#define HPHASE_CENTER   0xc00
#define DAC_CENTER      2048
#define BOFFSET_CENTER  0x200

/*
 * EDH read-write constants
 */
#define	MGV_EDH_WRITE	0
#define	MGV_EDH_READ	1


/*
 * defines for the 8584 i2c "driver"
 */
#define I2C_WRITE              0x4
#define I2C_SLAVE_ADDRESS      0x55
#define I2C_BUSY(srv_bd)       (_srv_inb_I2C(srv_bd, GAL_IIC_CTL) & 0x80)
#define I2C_LRB(srv_bd)        (_srv_inb_I2C(srv_bd, GAL_IIC_CTL) & 0x08)
#define DELAY_CHECK            0x5 /* usec to wait for i2c bus to clear */
#define I2C_START              0xC5
#define I2C_STOP               0xc3
#define I2C_STOP_NOACK         0xc2
#define I2C_NOACK              0x40

#define GAL_IIC_CLOCK_REG      0xA0
#define GAL_SCL_45KHZ          0x01
#define GAL_SCL_90KHZ          0x00
#define GAL_SCLK_8MHZ          0x18

/*
 *      Addresses of supported I2C devices for SpeedRacer Galileo 1.5.
 */

#define BUS_CONV_ADDR   0x40
#define EDH_ADDR        0x1A
#define GAL_ADDR_8584   0xAA

/*
 * Bridge constants
 */ 
#define GIOBR_DEV_CONFIG(flgs, dev)     ( (flgs) | (dev) )
#define LSBIT(word)             ((word) &~ ((word)-1))

/* 
 *Break down all bits in all registers in all chips, on the Galileo 1.5
 *Video card.
 */
/* register group constants */
#define	BT_CC_INDIR_GROUP		0
#define	BT_CC_DIR_GROUP			1

/*	CC1 Indirect Regs */
#define BT_A_DIRECT			1
#define BT_A_24_32			2
#define BT_A_SKIP			3
#define BT_A_DOIT			4
#define BT_A_ACTIVE			5
#define BT_B_DIRECT			6
#define BT_B_24_32			7
#define BT_B_DITHER			8
#define BT_B_FULL			9
#define BT_B_SKIP			10
#define BT_B_DOIT			11
#define BT_B_ACTIVE			12
#define BT_C_DITHER			13
#define BT_C_SKIP			14
#define BT_C_DOIT			15
#define BT_A_LEFT			16
#define BT_A_RIGHT			17
#define BT_A_BLACK_TOP_HI		18
#define BT_A_BLACK_BOT_HI		19
#define BT_A_BACKOUT			20
#define BT_A_TOP_LINE			21
#define BT_A_BOTTOM_LINE		22
#define BT_A_PHASE_HI			23
#define BT_A_PHASE_LO			24
#define BT_B_LEFT			25
#define BT_B_RIGHT			26
#define BT_B_BLACK_TOP_HI		27
#define BT_B_BLACK_BOT_HI		28
#define BT_B_BACKOUT			29
#define BT_B_TOP_LINE			30
#define BT_B_BOTTOM_LINE		31
#define BT_B_PHASE_HI			32
#define BT_B_PHASE_LO			33
#define BT_C_LEFT			34
#define BT_C_RIGHT			35
#define BT_C_BLACK_TOP_HI		36
#define BT_C_BLACK_BOT_HI		37
#define BT_C_BACKOUT			38
#define BT_C_TOP_LINE			39
#define BT_C_BOTTOM_LINE		40
#define BT_C_PHASE_HI			41
#define BT_C_PHASE_LO			42
#define BT_OVERLAP			43
#define BT_FIELD_ID			44
#define BT_PIPE				45
#define BT_DITHER			46
#define BT_REFRESH			47
#define BT_HALF_HOR_HI			48
#define BT_HALF_VERT_HI			49
#define BT_HALF_HOR_LO			50
#define BT_HALF_VERT_LO			51
#define BT_NONBLANK_VIDEO		52
#define BT_GEN_OUT			53
#define BT_CLEAR_INT			54
#define BT_ENABLE_INT			55
#define BT_INT_SOURCE			56
#define BT_VID_PORT0_ENAB		57
#define BT_EXP_PORT0_ENAB		58
#define BT_EXP_PORT1_ENAB		59
#define BT_LATCH_INP_LINE		60
#define BT_LATCH_OUT_LINE		61
#define BT_INT_IN_LINE_HI		62
#define BT_INT_IN_LINE_LO		63
#define BT_INT_OUT_LINE_HI		64
#define BT_INT_OUT_LINE_LO		65
#define BT_INP_LINE_LATCH_HI		66
#define BT_INP_LINE_LATCH_FIELD		67
#define BT_INP_LINE_LATCH_LO		68
#define BT_OUT_LINE_LATCH_HI		69
#define BT_OUT_LINE_LATCH_FIELD		70
#define BT_OUT_LINE_LATCH_LO		71
#define BT_FRM_BUF_DIRECT		72
#define BT_FRM_BUF_IN			73
#define BT_FRM_BUF_OUT			74
#define BT_FILT_FOR_DEC			75
#define BT_ADDRESSING_MODE		76
#define BT_HIDDEN			77
#define BT_DECIM_FACTOR			78
#define BT_HOST_LINE_NUM_HI		79
#define BT_ALPHA_BKGRND			80
#define BT_FLAT_Y			81
#define BT_FLAT_U			82
#define BT_FLAT_V			83
#define BT_XP_GR_A			84
#define BT_XP_GR_B			85
#define BT_XP_GR_C			86
#define BT_XP_EXP_0			87
#define BT_XP_FRM_BUF_IN		88
#define BT_XP_VID_OUT			89
#define BT_XP_FOR_IN			90
#define BT_XP_BACK_IN			91
#define BT_XP_ALPHA_FOR			92
#define BT_XP_ALPHA_BACK		93
#define BT_ALPHA_COMPOS_BACK		94
#define BT_ALPHA_COMPOS_FOR		95

/* CC1 Direct Registers */
#define BT_NTSC_PAL			96
#define BT_PIXEL_SHAPE			97
#define BT_EXP0_DIRECT			98
#define BT_MASTER_TIME			99
#define BT_DMA_CLOCK			100
#define BT_TBC				101
#define BT_DO_IT			102
#define BT_INDIRECT_ADDR		103
#define BT_INDIRECT_DATA		104
#define BT_FRM_BUF_HOR			105
#define BT_FRM_BUF_LINE_LO		106
#define BT_LOAD_ADDR			107
#define BT_RESET_FIFO			108
#define BT_XP_OUT			109
#define BT_XP_INP			110
#define BT_HOST_RW			111
#define BT_FRM_BUF_SEQ			112
#define BT_FRM_BUF_DATA			113
#define BT_I2C_IDLE			114
#define BT_I2C_RW			115
#define BT_I2C_HOLD_BUS			116
#define BT_I2C_BUS_SPEED		117
#define BT_I2C_BUSY			118
#define BT_I2C_ACK			119
#define BT_I2C_BUS_ERROR		120
#define BT_I2C_DATA			121



/* AB Directs - System Control Bits */
#define BT_AB_NTSC			122
#define BT_EXPRESS_MODE			123
#define BT_ENABLE_VIDEO			124
#define BT_CHIP_ID			125


/* AB Indirects - Internal Registers */
#define BT_A_PAN_X_LO			126
#define BT_A_PAN_X_HI			127
#define BT_A_PAN_Y_LO			128
#define BT_A_PAN_Y_HI			129
#define BT_A_WIN_X_LEFT			130
#define BT_A_WIN_X_RIGHT		131
#define BT_A_ENABLE			132
#define BT_A_FLICKER			133
#define BT_A_PAN			134
#define BT_A_ZOOM			135
#define BT_A_ALPHA_ENAB			136
#define BT_A_FULL_SCREEN		137
#define BT_A_FREEZE			138
#define BT_A_DIRECTION			139
#define BT_A_DECIMATION			140
#define BT_B_PAN_X_LO			141
#define BT_B_PAN_X_HI			142
#define BT_B_PAN_Y_LO			143
#define BT_B_PAN_Y_HI			144
#define BT_B_WIN_X_LEFT			145
#define BT_B_WIN_X_RIGHT		146
#define BT_B_ENABLE			147
#define BT_B_FLICKER			148
#define BT_B_PAN			149
#define BT_B_ZOOM			150
#define BT_B_ALPHA_ENAB			151
#define BT_B_FREEZE			152
#define BT_B_DIRECTION			153
#define BT_B_RESOLUTION			154
#define BT_B_DECIMATION			155
#define BT_C_PAN_X_LO			156
#define BT_C_PAN_X_HI			157
#define BT_C_PAN_Y_LO			158
#define BT_C_PAN_Y_HI			159
#define BT_C_WIN_X_LEFT			160
#define BT_C_WIN_X_RIGHT		161
#define BT_C_ENABLE			162
#define BT_C_FLICKER			163
#define BT_C_PAN			164
#define BT_C_ZOOM			165
#define BT_C_FREEZE			166

/* AB Indirects - Test Registers */
#define BT_DRAM_REFRESH			167
#define BT_DRAM_TEST			168
#define BT_A_DEFAULT_CYCLE		169
#define BT_B_DEFAULT_CYCLE		170
#define BT_CHIP_REV			171

/* AB direct register to enable D1 */
#define BT_D1_ENABLE			172

/* GAL 1.5 Direct Registers */
#define BT_GAL_GAL_REVISION		173
#define BT_GAL_TMI_REVISION		174

/* GAL 1.5 Indirect Registers */
#define BT_GAL_PIXEL_MODE		175
#define BT_GAL_TIMING_MODE		176
#define BT_GAL_AUTO_PHASE		177
#define BT_GAL_CH1_IN_STD		178
#define BT_GAL_CH1_IN_PRESENT		179
#define BT_GAL_CH1_IN_AUTO		180
#define BT_GAL_CH1_IN_2HREF		181
#define BT_GAL_CH2_IN_STD		182
#define BT_GAL_CH2_IN_PRESENT		183
#define BT_GAL_CH2_IN_AUTO		184
#define BT_GAL_CH2_IN_2HREF		185
#define BT_GAL_INPUT_ALPHA		186
#define BT_GAL_CH1_IN_ROUND		187
#define BT_GAL_CH2_IN_ROUND		188
#define BT_GAL_CH12_IN_DITHER		189
#define BT_GAL_OUTPUT_ALPHA		190
#define BT_GAL_CH1_OUT_VB		191
#define BT_GAL_CH2_OUT_VB		192
#define BT_GAL_CH2_CC1_CHROMA		193
#define BT_GAL_CH1_OUT_HPHASE_LO	194
#define BT_GAL_CH1_OUT_HPHASE_HI	195
#define BT_GAL_CH2_OUT_HPHASE_LO	196
#define BT_GAL_CH2_OUT_HPHASE_HI	197
#define BT_GAL_CH1_FRAME_SYNC		198
#define BT_GAL_CH1_OUT_BLANK		199
#define BT_GAL_CH1_OUT_FREEZE		200
#define BT_GAL_CH2_FRAME_SYNC		201
#define BT_GAL_CH2_OUT_BLANK		202
#define BT_GAL_CH2_OUT_FREEZE		203
#define BT_GAL_VCXO_LO			204
#define BT_GAL_VCXO_HI			205
#define BT_GAL_GENLOCK_REF		206
#define BT_GAL_REF_LOCK			207
#define BT_GAL_INOUT_1			208
#define BT_GAL_INOUT_2			209
#define BT_GAL_GFXA_TOP_BLANK		210
#define BT_GAL_GFXB_TOP_BLANK		211
#define BT_GAL_VCXO_DAC_BUSY		212
#define BT_GAL_VCXO_ADC_BUSY		213
#define BT_GAL_REF_OFFSET_LO		214
#define BT_GAL_REF_OFFSET_HI		215
#define BT_GAL_VCXO_ADC_MODE		216
#define BT_GAL_VCXO_ADC_HI		217
#define BT_GAL_VCXO_ADC_LO		218
#define BT_GAL_FIRMWARE_REVISION	219
#define BT_GAL_CH1_IN_8BIT		220
#define BT_GAL_CH2_IN_8BIT		221

#define BT_GAL_CH1_CC1			222
#define BT_GAL_CH2_CC1			223
#define BT_GAL_CSC_A			224
#define BT_GAL_CSC_B			225
#define BT_GAL_D_EXP			226
#define BT_GAL_CH1_GIO			227
#define BT_GAL_CH2_GIO			228
#define BT_GAL_CH2_VOUT			229
#define BT_GAL_CH1_VOUT			230
#define BT_GAL_TMI_A			231
#define BT_GAL_TMI_B			232
#define BT_GAL_SYNC_PORT		233
#define BT_GAL_A_EXP			234
#define BT_GAL_B_EXP			235
#define BT_CC1_PIXEL_TIMING		236
#define BT_GAL_CH1_OUT_CHROMA		237
#define BT_GAL_CH2_OUT_CHROMA		238
#define BT_GAL_VGI1_SW_RESET		239
#define BT_GAL_GFX_FRAME_RESET		240

#define BT_CSC_YUV422_IN		250
#define BT_CSC_FILTER_IN		251
#define BT_CSC_TRC_IN			252
#define BT_CSC_LOAD_MODE		253
#define BT_CSC_AUTO_CNT			254
#define BT_CSC_LUT_SELECT		255
#define BT_CSC_LUT_READ 		256
#define BT_CSC_RW_REGISTER		257
#define BT_CSC_PAGE_ADDR		258
#define BT_CSC_YUV422_OUT		259
#define BT_CSC_FILTER_OUT		260
#define BT_CSC_RGB_BLANK		261
#define BT_CSC_FULLSCALE		262
#define BT_CSC_MAX_ALPHA_OUT		263
#define BT_CSC_ALPHA_OUT		264
#define BT_CSC_BLACK_OUT		265

/*	TMI Indirect Regs */
#define	BT_TMI_M_ROUND			270
#define	BT_TMI_M_MIPMAP_ENABLE		271
#define	BT_TMI_M_SCALER_YUVOUT		272
#define	BT_TMI_M_INTRPT_ENABLE		273
#define	BT_TMI_M_ABORT			274
#define	BT_TMI_M_SWAP			275
#define	BT_TMI_S_XFER_FIELD		276
#define	BT_TMI_S_XFER_DONE		277
#define	BT_TMI_S_INTRPT_ENABLE		278
#define	BT_TMI_S_SWAP_STAT		279
#define	BT_TMI_S_VID_IN_FIELD		280


/*
 * Remember after changing the above define values or
 * ranges to adjust the following two table boundary
 * ranges. Which is referenced in mgv_singlebit().
 */
#define BT_BEGIN		BT_A_DIRECT
#define BT_END			BT_GAL_VGI1_SW_RESET

#define BT_CC_INDIRECT_MIN	BT_A_DIRECT
#define BT_CC_INDIRECT_MAX	BT_ALPHA_COMPOS_FOR

#define BT_CC_DIRECT_MIN	BT_NTSC_PAL
#define BT_CC_DIRECT_MAX	BT_I2C_DATA

#define BT_AB_DIRECT_MIN	BT_AB_NTSC
#define BT_AB_DIRECT_MAX	BT_CHIP_ID

#define BT_AB_INTERNEL_MIN	BT_A_PAN_X_LO
#define BT_AB_INTERNEL_MAX	BT_C_FREEZE

#define BT_AB_TEST_MIN		BT_DRAM_REFRESH
#define BT_AB_TEST_MAX		BT_CHIP_REV

#define BT_GAL_DIRECT_MIN	BT_GAL_GAL_REVISION
#define BT_GAL_DIRECT_MAX	BT_GAL_TMI_REVISION

#define BT_GAL_INDIRECT_MIN	BT_GAL_PIXEL_MODE
#define BT_GAL_INDIRECT_MAX	BT_GAL_GFX_FRAME_RESET

#define BT_CSC_INDIRECT_MIN	BT_CSC_YUV422_IN
#define BT_CSC_INDIRECT_MAX	BT_CSC_BLACK_OUT

#define BT_TMI_INDIRECT_MIN	BT_TMI_M_ROUND
#define BT_TMI_INDIRECT_MAX	BT_TMI_S_VID_IN_FIELD

#define LOBYTE(x)    ((x) & 0xff)
#define HIBYTE(x)    ((((x) >> 8) & 0xff))

/* logic definitions - basic numbers */
#define OK			0
#define PASS			OK
#define FAIL			!PASS
#define ODD			0
#define EVEN			1
#define ODD_FIELD		ODD
#define EVEN_FIELD		EVEN
#define	ZERO			0
#ifndef NULL
#define	NULL			0
#endif
#define ONE			1
#define TWO			2
#define EIGHTBIT		8
#define TENBIT			10
#define TENBIT_MASK		0x03

#define BYTE_ZERO		0x00

/* Test results macros */
#define	RESET_TSTBUF		0
#define DSPY_TSTBUF		1

/* Default image location on the screen */
#define VO_H_OFF		0
#define VO_H_LEN		0
#define VO_V_OFF		0
#define VO_V_LEN		0
/* XDIV was changed from 4 to 1 to assure a complete raster line is written */
#define XDIV  			1
#define YDIV  			4
#define MAX_HOR 		768

/* video display variables */
#define FRAMES_PER_SEC			30
#define TICKS_PER_SEC			5
#define MSEC_PER_PATTERN_DEFAULT	20000
#define SEC_PER_LOCATION_DEFAULT	15
#define SEC_PER_MOTION_DEFAULT		0.5
#define VID_DIAG_PTRN_DEFAULT		0
#define MAX_SEC_PER_PATTERN		60
#define MAX_SEC_PER_MOTION		5
#define CHANNEL_DEFAULT			ONE
#define MAX_CHANNELS			TWO
#define MAX_REGMEMS			30
#define MAX_ERRORS_DISPLAYED		10
#define MAX_TESTS_PER_REGMEM		10

#define MAX_POS_INT			0xEFFF

/* vidout_L0 & vid_diags test parameter argument sequencing */
#define CHAN_ARG		1
#define CHAN_IN_ARG		1
#define CHAN_OUT_ARG		2
#define STD_ARG			3
#define SEC_ARG			4
#define LOC_ARG			3
#define MOT_ARG			4
#define VID_ARG			5

/* Bit test patterns for memory and register tests */
#define ALLONES			0xFF
#define ALLONES_W             	0xFFFF
#define ALLZEROES		0x00
#define ALLZEROES_W           	0x0000
#define ALT10			0xAA
#define ALT10_W                 0xAAAA
#define ALT01			0x55
#define ALT01_W                 0x5555
#define WALK1			0x01 /* starting value - moves left */
#define WALK0			0xFE /* starting value - moves left */
#define NUM_BIT_PATTERNS	6
/*#define WALK1			(1 << (i%8)) */
/*#define WALK0			(~(WALK1))   */

#define	VID_WALK1		0
#define	VID_WALK0		1
#define	VID_VLINE		2
#define	VID_HLINE		3
#define	VID_BOX			4
#define	VID_XHBOX		5
#define NUM_VID_PATTERNS 6


/* Register test request bit locations for batch processing */
#define TEST_A1			(1 << 0)
#define TEST_A0			(1 << 1)
#define TEST_10			(1 << 2)
#define TEST_01			(1 << 3)
#define TEST_W1			(1 << 4)
#define TEST_W0			(1 << 5)
#define INVALID_TESTS	(0xC0)
/* save bits 6 & 7 for defining two more blocks of 6 each tests tbd */

/* Bit locations for Video Output register (Table 16) */
#define VO_PASSTHRU             (1 << 0)
#define VO_BLANK_PICT           (1 << 1)
#define VO_FREEZE               (1 << 2)
#define VO_BLNK_CHRM            (1 << 3)

/* Bit locations for Gen Lock GLCK_VCXOCONN register (Table 17) */
#define GL_REFSRC_FRERUN        0
#define GL_REFSRC_LOOP          1
#define GL_REFSRC_DIN2          2
#define GL_REFSRC_DIN1          3

#define GL_CH01                 0
#define GL_CH10                 1
#define GL_CH0                  2
#define GL_CH1                  3

#define GL_VCXOCONN_DFLT        ( 0x0 )

/* Bit locations for Gen Lock GLCK_STATUS register (Table 18) */
#define GL_REF_UNLCK            (0 << 1)
#define GL_VCXO_WR_BUSY         (4 << 1)

/*
 * DCBCTRL access protocol defines for Galileo 1.5 using HQ3
 * NOTE: assuming synchronous handshake protocol for CC1
 */

#ifdef IP30

#define HQ3_GAL_PROTOCOL_NOACK \
  (MGRAS_DCB_CSSETUP(4) | MGRAS_DCB_CSHOLD(4) | \
   MGRAS_DCB_CSWIDTH(4))

#define HQ3_TMI_PROTOCOL_SYNC \
  (MGRAS_DCB_CSSETUP(0) | MGRAS_DCB_CSHOLD(0) | \
   MGRAS_DCB_CSWIDTH(0) | MGRAS_DCB_ENSYNCACK)

#else

#define HQ3_GAL_PROTOCOL_NOACK \
  (MGRAS_DCB_CSSETUP(3) | MGRAS_DCB_CSHOLD(3) | \
   MGRAS_DCB_CSWIDTH(3))

#define HQ3_TMI_PROTOCOL_SYNC \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) | \
   MGRAS_DCB_CSWIDTH(2) | MGRAS_DCB_ENASYNCACK)

#endif

#define HQ3_CC1_PROTOCOL_NOACK \
  (MGRAS_DCB_CSSETUP(1) | MGRAS_DCB_CSHOLD(0) |  \
   MGRAS_DCB_CSWIDTH(1))

#define HQ3_CC1_PROTOCOL_SYNC_FRAME \
  (MGRAS_DCB_CSSETUP(0) | MGRAS_DCB_CSHOLD(0) |  \
   MGRAS_DCB_CSWIDTH(0) | MGRAS_DCB_ENSYNCACK)

#define HQ3_CC1_PROTOCOL_SYNC \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) |  \
   MGRAS_DCB_CSWIDTH(1))

#define HQ3_CC1_PROTOCOL_ASYNC_FRAME \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) |  \
   MGRAS_DCB_CSWIDTH(2) | MGRAS_DCB_ENASYNCACK)

#define HQ3_CC1_PROTOCOL_SYNC_FRAME \
  (MGRAS_DCB_CSSETUP(0) | MGRAS_DCB_CSHOLD(0) |  \
   MGRAS_DCB_CSWIDTH(0) | MGRAS_DCB_ENSYNCACK)

#define HQ3_AB1_PROTOCOL_ASYNC \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) |  \
   MGRAS_DCB_CSWIDTH(2) | MGRAS_DCB_ENASYNCACK)

#define HQ3_GAL_PROTOCOL_ASYNC \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(3) | \
   MGRAS_DCB_CSWIDTH(3) | MGRAS_DCB_ENASYNCACK)


/*** used previously ***/
#if 0
#define HQ3_TMI_PROTOCOL_NOACK \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) | \
   MGRAS_DCB_CSWIDTH(2))
#endif

#define HQ3_TMI_PROTOCOL_NOACK \
  (MGRAS_DCB_CSSETUP(3) | MGRAS_DCB_CSHOLD(3) | \
   MGRAS_DCB_CSWIDTH(4))

/*
 * ERROR CODES
 * These constants should be in sync with mgv_err[] defined in mgv_globals.c
 */
/* Board Specific ASIC independent Tests 0 - 4 */
#define	MGV_INIT_TEST				  0
#define	MGV_GPI_TRIGGER_TEST			  1

/* AB1 TESTS  5 - 29 */
#define	AB1_DRAM_ADDR_BUS_RED_CHAN_TEST		  5	
#define	AB1_DRAM_ADDR_BUS_GRN_CHAN_TEST		  6
#define	AB1_DRAM_ADDR_BUS_BLU_CHAN_TEST		  7
#define	AB1_DRAM_ADDR_BUS_ALP_CHAN_TEST		  8
#define	AB1_DRAM_DATA_BUS_RED_CHAN_TEST		  9
#define	AB1_DRAM_DATA_BUS_GRN_CHAN_TEST		 10
#define	AB1_DRAM_DATA_BUS_BLU_CHAN_TEST		 11
#define	AB1_DRAM_DATA_BUS_ALP_CHAN_TEST		 12
#define	AB1_DRAM_PATRN_RED_CHAN_TEST		 13
#define	AB1_DRAM_PATRN_GRN_CHAN_TEST		 14
#define	AB1_DRAM_PATRN_BLU_CHAN_TEST		 15
#define	AB1_DRAM_PATRN_ALP_CHAN_TEST		 16
#define	AB1_DRAM_PATRN_SLOW_RED_CHAN_TEST	 17
#define	AB1_DRAM_PATRN_SLOW_GRN_CHAN_TEST	 18
#define	AB1_DRAM_PATRN_SLOW_BLU_CHAN_TEST	 19
#define	AB1_DRAM_PATRN_SLOW_ALP_CHAN_TEST	 20
#define	AB1_DCB_TEST				 21

/* CC1 TESTS 30 - 39 */
#define	CC1_DRAM_ADDR_BUS_TEST			 30
#define	CC1_DRAM_DATA_BUS_TEST			 31
#define	CC1_DRAM_PATRN_TEST			 32
#define	CC1_DCB_TEST			 	 33

/* VGI1 TESTS 40 - 49 */
#define	VGI1_REG_PATTERN_TEST			 40
#define	VGI1_REG_WALK_BIT_TEST		 	 41
#define	VGI1_INTERRUPT_TEST		 	 42
#define	VGI1_DMA_TEST				 43

/* CSC specific error codes 50 - 59 */
#define CSC_ADDR_BUS_TEST                       50
#define CSC_DATA_BUS_TEST                       51
#define CSC_INPUT_LUT_TEST                      52
#define CSC_OUTPUT_LUT_TEST                     53
#define CSC_COEF_TEST                           54
#define CSC_X2K_TEST                            55

/* TMI TESTS 60 - 69 */
#define	TMI_REG_PATTERN_TEST			 60
#define	TMI_REG_DATA_BUS_TEST		 	 61
#define	TMI_BYE_PASS_TEST		 	 62
#define	TMI_SRAM_ADDR_UNIQ_TEST		 	 63
#define	TMI_SRAM_PATRN_TEST		 	 64

/* SCALER TESTS 70 - 71 */
#define SCL_ADDR_BUS_TEST			70
#define SCL_DATA_BUS_TEST			71

/* TYPE DEFINITIONS AND PROTOTYPES */
typedef	struct _mgv_info {
        int                  swrev;        /* driver/daemon interface version */
        int                  bdrev;        /* Galileo board rev. */
        int                  CCrev;        /* CC1 rev.  */
        int                  ABrev;        /* AB1 rev.  */
        int                  VGIrev;       /* VGI1 rev.  */
        int                  CSC_TMIrev;       /* TMI rev.  */
        int                  board_type;   /* see above */
        mgv_vgi1_gio_id_t    vgi1_gio_id;  /* VGI1 info */
} mgv_info_t;

typedef struct _mgv_vgi1_info {
        __uint32_t packing;            /* VL packing from user args */

        __uint32_t unit_rate;          /* Unit (field/frame) rate (per sec) */
        __uint32_t ust_adjust;         /* UST delta to jack */

	/* These are used in DMA functons */
	ulonglong_t hparam;		/* Horizontal parameter */
	ulonglong_t vparam;		/* Vertical parameter */
	ulonglong_t chSet;		/* Channel Setup Register Value */
	ulonglong_t trigOn;		/* Trigger On */
	ulonglong_t trigOff;		/* Trigger Off */
	ulonglong_t abortDMA;		/* Abort DMA */
	ulonglong_t stride;		/* DMA stride */

	mgv_dma_unit_t dma_unit;	/* Transfer unit */
	mgv_vgi1_ch_setup_t ch_setup;	/* Channel Setup Register defs */
	__uint32_t ch_setup_reg_offset;	/* Channel Setup Register offset */
	__uint32_t ch_trig_reg_offset;	/* Channel Trigger Register offset */
	__uint32_t ch_dma_stride_reg_offset;	/* Channel Trigger Register offset */
	__uint32_t ch_status_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_int_enab_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_hparam_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_vparam_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_data_adr_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_desc_adr_reg_offset;/* Channel Trigger Register offset */
	__uint32_t ch_vout_ctrl_reg_offset;
	__uint32_t ch_vid_src;
	unsigned char	*pFieldDescPhys;
	unsigned char	*pFieldDescKseg;
	unsigned char	*pDMADataPhys;
	unsigned char	*pDMADataKseg;
} mgv_vgi1_info_t;

typedef struct _mgv_cc1_info {
        __uint32_t	field_count;    /* CC1 interrupt field counter */
        __uint32_t	num_fields;     /* number of fields in frame buffer */
        __uint32_t	cur_frame_delay;
        stamp_t		frontier;       /* frontier MSC */
        stamp_t		prev_ust;       /* Latency corrected previous UST */
        stamp_t		ust;            /* Latency corrected UST for current field */
        stamp_t		msc;            /* MSC for current field */
} mgv_cc1_info_t;

typedef struct  _mgv_csc_sel_ctrl { /* CSC indirect addr register 1 */
        uchar_t sel_output_lut : 2; /* select output LUT for Read Back mode */
        uchar_t sel_lut_addr   : 3; /* select LUT for address application */
        uchar_t auto_inc_normal: 1; /* 1 reset auto inc to 0; 0 normal mode */
        uchar_t luts_mode      : 2; /* 0 normal; 1 load coef; 2 load LUT; 3 read LUT */

} mgv_csc_sel_ctrl_t ;

typedef struct	_mgvdiag {
	void		*giobase;	/* base addr of VGI1 ASIC */
	void		*vgi1_1_base;	/* base addr of VGI1 ASIC 1 */
	void		*vgi1_2_base;	/* base addr of VGI1 ASIC 2 */
	void		*brbase;	/* base addr of BRIDGE ASIC */
	mgras_hw	*mgbase;	/* base addr of MGRAS hardware */
	__paddr_t	cc;		/* Address to CC1 */
	__paddr_t	ab;		/* Address to AB1 */
	__paddr_t	gal_15;		/* Address to GAL_1.5  chips */
	__paddr_t	csc_tmi;	/* Address to TMI/CSC chips */
	__paddr_t	vgi1addr;	/* Address to VGI1 PIO registers */
	uchar_t		videoMode;	/* video timing NTSC/PAL */
	uchar_t		pixelMode;	/* current pixel mode SQR/MHZ13.5 */
	mgv_info_t	info;		/* Board info */
	mgv_vgi1_info_t	vgi1[MGV_VGI1_NUM_CHAN];
	mgv_cc1_info_t	cc1;
	__uint32_t	transferSize;	/* DMA transfer size in bytes */
	__uint32_t	pageBits;	/* number of bits to represent a page */
	__uint32_t	pageSize;	/* page size in bytes */
	__paddr_t	pageMask;	/* page mask */
	__uint32_t	chan;
	__uint32_t	tvStd;
	__uint32_t	vidFmt;
	__uint32_t  	nFields;	/* Number of Fields */
	uchar_t		*pVGIDMABuf;	/* pointer to the DMA buffer */
	uchar_t		*pVGIDMABufAl;	/* pointer to the DMA buffer Aligned */
	__uint32_t	VGIDMAOp;	/* DMA Read or Write */
	__uint32_t	vout_freeze;	/* freeze or unfreeze video output */
	__uint32_t	vin_round;	/* Video Input Round or Trunc */
	__uint32_t	vout_expd;	/* Video Output Expansion or Not */
	__uint32_t	dualLink;
	ulonglong_t	VGI1DMAOpErrStatus;
	ulonglong_t	VGI1DMAClrErrStatus;
	__uint32_t	burst_len;
	uchar_t		force_unused;
	uchar_t		force_insuf;
	ulonglong_t	intEnableVal;
} mgvdiag;

typedef struct _mgv_test_info {
	char	*TestStr;
	char	*ErrStr;
} mgv_test_info;

typedef	struct _mgv_ind_regs {
	__uint32_t	reg;
	__uint32_t	val;
} mgv_ind_regs;

typedef struct _mgv_vgi1_rwregs {
	char		*str;
	__uint32_t	offset;
	ulonglong_t	mask;
} mgv_vgi1_rwregs;


typedef struct _mgv_tmi_rwregs {
	char		*str;
	__uint32_t	offset;
} mgv_tmi_rwregs;

typedef struct _exp_bits_t {
	__uint32_t name;	/* See above */
	__uint32_t reg;		/* Subaddress */
	unsigned char mask;	/* Value to use as mask, ie 2^num_of_bits */
	unsigned char shift;	/* How many bits to shift over */
} mgv_exp_bits;

/* test_d525 (also test_allstds.c) test pattern generator sequence */
typedef enum {  
		COLOR_BARS,	/* case 0 */
		PULSE_BAR,	/* case 1 */
		MOD_RAMP,	/* case 2 */
		MULTIBURST,	/* case 3 */
		STEP5,		/* case 4 */
		YZONE,		/* case 5 */
		YCZONE		/* case 6 */ } gen_t_patterns_t;

/* Video Out Level 0 (vidout_L0.c) test pattern sequence */
/* NOTE: NULL must always be the end of the sequence */


/* EDH structures */

/*
 * Following structure and constants are used in mgv_edh_mem.c
 */

/* EDH Word 1 for WRITE - Error flags*/
typedef union {
	struct _edh_w1 {
	        unsigned ap_idh         : 1;
		unsigned ap_eda         : 1;
		unsigned ap_edh         : 1;
		unsigned anc_ues        : 1;
		unsigned anc_ida        : 1;
		unsigned anc_idh        : 1;
		unsigned anc_eda        : 1;
		unsigned anc_edh        : 1;
	} bits;
	uchar_t val;
} edh_write_word1;

/* EDH Word 2 for WRITE - Error Flags*/
typedef union {
	struct _edh_w2 {
		unsigned sticky_flags	: 1;
		unsigned ff_ues 	: 1;
		unsigned ff_ida 	: 1;
		unsigned ff_idh 	: 1;
		unsigned ff_eda 	: 1;
		unsigned ff_edh 	: 1;
		unsigned ap_ues 	: 1;
		unsigned ap_ida         : 1;
	} bits;
	uchar_t val;
}  edh_write_word2;


/* EDH Word 3 for WRITE */
typedef union {
	struct _edh_w3 {
		unsigned map_idh 	: 1;
		unsigned map_eda 	: 1;
		unsigned map_edh 	: 1;
		unsigned manc_ues 	: 1;
		unsigned manc_ida 	: 1;
		unsigned manc_idh 	: 1;
		unsigned manc_eda 	: 1;
		unsigned manc_edh       : 1;
	} bits;
	uchar_t val;
}  edh_write_word3;

/* EDH Word 4 for WRITE */
typedef union {
	struct _edh_w4 {
		unsigned mask_rw	: 1;
		unsigned mff_ues 	: 1;
		unsigned mff_ida 	: 1;
		unsigned mf_idh 	: 1;
		unsigned mff_eda 	: 1;
		unsigned mff_edh 	: 1;
		unsigned map_ues 	: 1;
		unsigned map_ida        : 1;
	} bits;
	uchar_t val;
}  edh_write_word4;

/* EDH Word 5 for WRITE */
typedef union {
	struct _edh_w5 {
		unsigned sap_idh	: 1;
		unsigned sap_eda 	: 1;
		unsigned sap_edh 	: 1;
		unsigned sall_ues 	: 1;
		unsigned sanc_ida 	: 1;
		unsigned sanc_idh 	: 1;
		unsigned sanc_eda 	: 1;
		unsigned sanc_edh       : 1;
	} bits;
	uchar_t val;
}  edh_write_word5;


/* EDH Word 6 for WRITE */
typedef union {
	struct _edh_w6 {
		unsigned auto_clr	: 1;
		unsigned clr_cnt 	: 1;
		unsigned trs_sel 	: 1;
		unsigned sff_ida 	: 1;
		unsigned sff_idh 	: 1;
		unsigned sff_eda 	: 1;
		unsigned sff_edh 	: 1;
		unsigned sap_ida        : 1;
	} bits;
	uchar_t val;
}  edh_write_word6;

/* EDH Word 7 for WRITE */
typedef union {
	struct _edh_w7 {
		unsigned rw1_b3 	: 1;
		unsigned rw1_b2 	: 1;
		unsigned unused1 	: 1;
		unsigned unused2 	: 1;
		unsigned sel_std 	: 1;
		unsigned ntsc_pal 	: 1;
		unsigned hd1_d1 	: 1;
		unsigned d1_d2  	: 1;
	} bits;
	uchar_t val;
} edh_write_word7;

/* EDH Word 8 for WRITE */
typedef union {
	struct _edh_w8 {
		unsigned rw2_b5 	: 1;
		unsigned rw2_b4 	: 1;
		unsigned rw2_b3 	: 1;
		unsigned rw2_b2 	: 1;
		unsigned rw1_b7 	: 1;
		unsigned rw1_b6 	: 1;
		unsigned rw1_b5 	: 1;
		unsigned rw1_b4         : 1;
	} bits;
	uchar_t val;
}  edh_write_word8;

/* EDH Word 9 for WRITE */
typedef union {
	struct _edh_w9 {
		unsigned rw3_b7 	: 1;
		unsigned rw3_b6 	: 1;
		unsigned rw3_b5 	: 1;
		unsigned rw3_b4 	: 1;
		unsigned rw3_b3 	: 1;
		unsigned rw3_b2 	: 1;
		unsigned rw2_b7 	: 1;
		unsigned rw2_b6         : 1;
	} bits;
	uchar_t val;
}  edh_write_word9;

/* EDH Word 10 for WRITE */
typedef union {
	struct _edh_w10 {
		unsigned rw5_b3 	: 1;
		unsigned rw5_b2 	: 1;
		unsigned rw4_b7 	: 1;
		unsigned rw4_b6 	: 1;
		unsigned rw4_b5 	: 1;
		unsigned rw4_b4 	: 1;
		unsigned rw4_b3 	: 1;
		unsigned rw4_b2         : 1;
	} bits;
	uchar_t val;
}  edh_write_word10;


/* EDH Word 11 for WRITE */
typedef union {
	struct _edh_w11 {
		unsigned rw6_b5	        : 1;
		unsigned rw6_b4 	: 1;
		unsigned rw6_b3 	: 1;
		unsigned rw6_b2 	: 1;
		unsigned rw5_b7 	: 1;
		unsigned rw5_b6 	: 1;
		unsigned rw5_b5 	: 1;
		unsigned rw5_b4         : 1;
	} bits;
	uchar_t val;
}  edh_write_word11;

/* EDH Word 12 for WRITE */
typedef union {
	struct _edh_w12 {
		unsigned rw7_b7  	: 1;
		unsigned rw7_b6 	: 1;
		unsigned rw7_b5 	: 1;
		unsigned rw7_b4 	: 1;
		unsigned rw7_b3 	: 1;
		unsigned rw7_b2 	: 1;
		unsigned rw6_b7 	: 1;
		unsigned rw6_b6         : 1;
	} bits;
	uchar_t val;
}  edh_write_word12;


/*
 * Following structures are used for EDH READ Memory map 
 */

/* EDH Word 1 for READ - Error flags*/
typedef union {
	struct _edh_r1 {
	        unsigned ap_idh         : 1;
		unsigned ap_eda         : 1;
		unsigned ap_edh         : 1;
		unsigned anc_ues        : 1;
		unsigned anc_ida        : 1;
		unsigned anc_idh        : 1;
		unsigned anc_eda        : 1;
		unsigned anc_edh        : 1;
	} bits;
	uchar_t val;
}  edh_read_word1;

/* EDH Word 2 for READ - Error Flags*/
typedef union {
	struct _edh_r2 {
		unsigned anc_ext	: 1;
		unsigned ff_ues 	: 1;
		unsigned ff_ida 	: 1;
		unsigned ff_idh 	: 1;
		unsigned ff_eda 	: 1;
		unsigned ff_edh 	: 1;
		unsigned ap_ues 	: 1;
		unsigned ap_ida         : 1;
	} bits;
	uchar_t val;
} edh_read_word2;

/* EDH Word 3 for READ */
typedef union {
	struct _edh_r3 {
		unsigned b20    	: 1;
		unsigned b19     	: 1;
		unsigned b18     	: 1;
		unsigned b17      	: 1;
		unsigned b16     	: 1;
		unsigned ntsc_pal 	: 1;
		unsigned hd1_d1 	: 1;
		unsigned d1_d2          : 1;
	} bits;
	uchar_t val;
} edh_read_word3;

/* EDH Word 4 for READ */
typedef union {
	struct _edh_r4 {
		unsigned b15    	: 1;
		unsigned b14    	: 1;
		unsigned b13    	: 1;
		unsigned b12    	: 1;
		unsigned b11    	: 1;
		unsigned b10    	: 1;
		unsigned b9     	: 1;
		unsigned b8             : 1;
	} bits;
	uchar_t val;
} edh_read_word4;

/* EDH Word 5 for READ */
typedef union {
	struct _edh_r5 {
		unsigned b7     	: 1;
		unsigned b6     	: 1;
		unsigned b5     	: 1;
		unsigned b4      	: 1;
		unsigned b3     	: 1;
		unsigned b2      	: 1;
		unsigned b1     	: 1;
		unsigned b0             : 1;
	} bits;
	uchar_t val;
} edh_read_word5;


/* EDH Word 6 for READ */
typedef union {
	struct _ap_crc {
		unsigned b15    	: 1;
		unsigned b14    	: 1;
		unsigned b13    	: 1;
		unsigned b12    	: 1;
		unsigned b11    	: 1;
		unsigned b10    	: 1;
		unsigned b9     	: 1;
		unsigned b8             : 1;
	} bits;
	uchar_t val;
} edh_read_word6;

/* EDH Word 7 for READ */
typedef union {
	struct _ap_crc2 {
		unsigned b7     	: 1;
		unsigned b6     	: 1;
		unsigned b5     	: 1;
		unsigned b4      	: 1;
		unsigned b3     	: 1;
		unsigned b2      	: 1;
		unsigned b1     	: 1;
		unsigned b0             : 1;
	} bits;
	uchar_t val;
} edh_read_word7;

/* EDH Word 8 for READ */
typedef union {
	struct _ff_crc {
		unsigned b15    	: 1;
		unsigned b14    	: 1;
		unsigned b13    	: 1;
		unsigned b12    	: 1;
		unsigned b11    	: 1;
		unsigned b10    	: 1;
		unsigned b9     	: 1;
		unsigned b8             : 1;
	} bits;
	uchar_t val;
} edh_read_word8;

/* EDH Word 9 for READ */
typedef union {
	struct _ff_crc2 {
		unsigned b7     	: 1;
		unsigned b6     	: 1;
		unsigned b5     	: 1;
		unsigned b4      	: 1;
		unsigned b3     	: 1;
		unsigned b2      	: 1;
		unsigned b1     	: 1;
		unsigned b0             : 1;
	} bits;
	uchar_t val;
} edh_read_word9;

/* EDH Word 10 for READ */
typedef union {
	struct _edh_r10 {
		unsigned rw2_b3 	: 1;
		unsigned rw2_b2 	: 1;
		unsigned rw1_b7 	: 1;
		unsigned rw1_b6 	: 1;
		unsigned rw1_b5 	: 1;
		unsigned rw1_b4 	: 1;
		unsigned rw1_b3 	: 1;
		unsigned rw1_b2         : 1;
	} bits;
	uchar_t val;
} edh_read_word10;


/* EDH Word 11 for READ */
typedef union {
	struct _edh_r11 {
		unsigned rw3_b5	        : 1;
		unsigned rw3_b4 	: 1;
		unsigned rw3_b3 	: 1;
		unsigned rw3_b2 	: 1;
		unsigned rw2_b7 	: 1;
		unsigned rw2_b6 	: 1;
		unsigned rw2_b5 	: 1;
		unsigned rw2_b4         : 1;
	} bits;
	uchar_t val;
} edh_read_word11;

/* EDH Word 12 for READ */
typedef union {
	struct _edh_r12 {
		unsigned rw4_b7  	: 1;
		unsigned rw4_b6 	: 1;
		unsigned rw4_b5 	: 1;
		unsigned rw4_b4 	: 1;
		unsigned rw4_b3 	: 1;
		unsigned rw4_b2 	: 1;
		unsigned rw3_b7 	: 1;
		unsigned rw3_b6         : 1;
	} bits;
	uchar_t val;
} edh_read_word12;

/* EDH Word 13 for READ */
typedef union {
	struct _edh_r13 {
		unsigned rw6_b3 	: 1;
		unsigned rw6_b2 	: 1;
		unsigned rw5_b7 	: 1;
		unsigned rw5_b6 	: 1;
		unsigned rw5_b5 	: 1;
		unsigned rw5_b4 	: 1;
		unsigned rw5_b3 	: 1;
		unsigned rw5_b2         : 1;
	} bits;
	uchar_t val;
} edh_read_word13;


/* EDH Word 14 for READ */
typedef union {
	struct _edh_r14 {
		unsigned rw7_b5	        : 1;
		unsigned rw7_b4 	: 1;
		unsigned rw7_b3 	: 1;
		unsigned rw7_b2 	: 1;
		unsigned rw6_b7 	: 1;
		unsigned rw6_b6 	: 1;
		unsigned rw6_b5 	: 1;
		unsigned rw6_b4         : 1;
	} bits;
	uchar_t val;
} edh_read_word14;

/* EDH Word 15 for READ */
typedef union {
	struct _edh_r15 {
		unsigned b7	  	: 1;
		unsigned b6 		: 1;
		unsigned b5 		: 1;
		unsigned b4 		: 1;
		unsigned b3 		: 1;
		unsigned b2 		: 1;
		unsigned rw7_b7 	: 1;
		unsigned rw7_b6         : 1;
	} bits;
	uchar_t val;
} edh_read_word15;



/* Galileo 1.5 */

typedef enum {  CCIR_525, SQ_525, CCIR_625, SQ_625, ALL_STD } tv_std_t;

typedef enum { 
			ALLONES_PTRN,
			ALLZEROES_PTRN,
			ALT10_PTRN,
			ALT01_PTRN,
			WALK1_PTRN,
			WALK0_PTRN,
			HEX80_PTRN,
			HEX40_PTRN,
			VLINE_PTRN, /* not yet functional */
			HLINE_PTRN, /* not yet functional */
			BOX_PTRN,   /* not yet functional */
			XHBOX_PTRN, /* not yet functional */
			ALL_PTRN } vout_patterns_t;

/********************************************************************/
/* THIS IS THE MAIN INDIVIDUAL TEST DATA STRUCTURE */
typedef struct _test_data {
        char*   test_name_p;    /* ascii test name  */
        int     test_value;     /* test value */
        int     mask;           /* mask to test only active bits */
        int     result;         /* value ^ expected */
        } test_data;


/* THIS IS THE MAIN REGISTER/MEMORY TEST DATA STRUCTURE */
/* IT INCLUDES TEST DATA STORAGE FOR 10 TESTS ON 1 REG./MEMORY LOC. */
typedef struct _reg_test  {
        char			*reg_name_p;    /* ascii register name */
        int				addr;           /* indirect addr */
						/* ptr to all reg. test results */
        test_data		*tests_p[MAX_TESTS_PER_REGMEM];
        unsigned char	pass;	/* if any result !=0 pass = FALSE */
        } reg_test;


/* EXTERNAL VARIABLE DECLARATION */
extern 	mgvdiag		*mgvbase;
extern	mgras_hw	*mgbase;
extern	__uint32_t      mgv_ab1_basebuf[16];
extern	__uint32_t 	mgv_walk[24];
extern	__uint32_t 	mgv_walk_one[16];
extern	__uint32_t 	mgv_patrn[6];
extern	__uint32_t 	mgv_patrn32[6];
extern	uchar_t 	mgv_patrn8[5];
extern	uchar_t 	tmi_bypass_patrn[16];
extern	_mg_xbow_portinfo_t	mg_xbow_portinfo_t[6];
extern	mgv_test_info	mgv_err[];
extern	mgv_vgi1_ch_trig_t	mgv_abort_dma;
extern	mgv_vgi1_ch_trig_t	mgv_trig_on;
extern	mgv_vgi1_ch_trig_t	mgv_trig_off;
extern	unsigned char   *mgv_pVGIDMADesc ;
#ifdef IP30
extern	unsigned char   mgv_VGIDMADesc[4096 * 2 * 4];  /** 16 K page **/
#else
extern	unsigned char   mgv_VGIDMADesc[4096 * 2];
#endif
extern	mgv_exp_bits 	mgv_bit_desc[];
extern	int test_values[];
extern	char* asc_bit_pat_p[];
extern	char* asc_vid_pat_p[];
extern	char* asc_tv_std_p[];
extern	unsigned char register_mask[];
extern	char *csc_luts[];
#if 1
extern	float   yfgm[1][1];
extern	float   ufgm[1][1];
extern	float   vfgm[1][1];
extern	short   afgm[1][1];
#else
extern	float   yfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
extern	float   ufgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
extern	float   vfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
extern	short   afgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
#endif

extern	gen_t_patterns_t vo_L0_ptrns[];
extern	int tv_std_loc[][16][2];
extern	int tv_std_length[][2];
extern	reg_test batch_test[MAX_REGMEMS];
extern	mgv_vgi1_h_param_t	hpNTSC, hpNTSCSQ, hpPAL, hpPALSQ;
extern	mgv_vgi1_v_param_t	vpNTSC, vpNTSCSQ, vpPAL, vpPALSQ;

/* CSC Specific values */
extern int csc_inlut, csc_alphalut, csc_coef, csc_x2k, csc_outlut, csc_inlut_value, csc_tolerance;
extern int csc_clipping, csc_constant_hue, csc_each_read ;

/*** IIC variables ****/
extern int mgv_iic_war;

/* EDH Specific Read and Write Memory words */
/* Variables for EDH Read and Write words */
extern	edh_read_word1 edh_r1;
extern	edh_read_word2 edh_r2;
extern	edh_read_word3 edh_r3;
extern	edh_read_word4 edh_r4;
extern	edh_read_word5 edh_r5;
extern	edh_read_word6 edh_r6;
extern	edh_read_word7 edh_r7;
extern	edh_read_word8 edh_r8;
extern	edh_read_word9 edh_r9;
extern	edh_read_word10 edh_r10;
extern	edh_read_word11 edh_r11;
extern	edh_read_word12 edh_r12;
extern	edh_read_word13 edh_r13;
extern	edh_read_word14 edh_r14;
extern	edh_read_word15 edh_r15;

extern	edh_write_word1 edh_w1;
extern	edh_write_word2 edh_w2;
extern	edh_write_word3 edh_w3;
extern	edh_write_word4 edh_w4;
extern	edh_write_word5 edh_w5;
extern	edh_write_word6 edh_w6;
extern	edh_write_word7 edh_w7;
extern	edh_write_word8 edh_w8;
extern	edh_write_word9 edh_w9;
extern	edh_write_word10 edh_w10;
extern	edh_write_word11 edh_w11;
extern	edh_write_word12 edh_w12;

extern uchar_t shadow_edh_iic_read[16];
extern uchar_t shadow_edh_iic_write[12];

/*** Bridge structure , initialized in mgv_globals.c ***/
extern struct giobr_bridge_config_s srv_bridge_config;
extern __uint32_t bridge_port;

/* EXTERNAL FUNCTION DECLARATION */
extern	__uint32_t	_mgv_reportPassOrFail(mgv_test_info *, __uint32_t);
extern	__uint32_t      _mvg_VGI1PollReg(__uint32_t reg, ulonglong_t val, 
	ulonglong_t mask, __uint32_t timeOut, ulonglong_t *regVal);
#if (defined(IP28) || defined(IP30))
#define mgv_vgi1_load_ll(A)	*(volatile ulonglong_t *)(A)
#define mgv_vgi1_store_ll(D,A)	*(volatile ulonglong_t *)(A) = (D);
#else
extern ulonglong_t mgv_vgi1_store_ll(ulonglong_t, void *);
extern ulonglong_t mgv_vgi1_load_ll(void *);
#endif
extern	__uint32_t	_mgv_VGI1DMA(void);
extern	__uint32_t      _mgv_reset(__uint32_t device_num);
extern	uchar_t* _mgv_PageAlign(uchar_t *pBuf);
extern void _mgv_csc_ReadLut(__uint32_t, __uint32_t, u_int16_t *);
extern void _mgv_csc_LoadLut(__uint32_t, __uint32_t, u_int16_t *);
extern void _mgv_csc_LoadBuf(__uint32_t, u_int16_t *, __uint32_t, __uint32_t);
extern __uint32_t _mgv_csc_VerifyBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t*);
extern void _mgv_csc_setupDefault(void);
extern __uint32_t _mgv_csc_tmi_probe(void);
extern void    msg_printf(int, char *, ...);
extern int pause(int, char *, char *);
extern __uint32_t _mgv_singlebit(__uint32_t, __uint32_t, __uint32_t);
extern int sprintf(char *buf, const char *fmt, ...);
extern __uint32_t  _mgv_outb(__paddr_t, unsigned char);
extern __uint32_t        _mgv_outw(__paddr_t, __uint32_t);
extern void busy(int);
extern __uint32_t        _mgv_inw(__paddr_t addr);
extern uchar_t _mgv_inb(__paddr_t);
extern void _mgv_CC1TestSetup(void);
extern __uint32_t  _mgv_VGI1DMAGetBufSize(void);
extern __uint32_t  _mgv_VGI1DMADispErr(void);
extern void  _mgv_VGI1InitDMAParam(void);
extern __uint32_t  _mgv_VGI1StartAndProcessDMA(void);
extern void flush_cache(void);
extern int test_gal15_rw1(void);
extern __uint32_t        _mgv_CC1PollFBRdy(__uint32_t);
extern void _mgv_delay_dots( unsigned char, unsigned char);
extern int _mgv_check_vin_status(unsigned char, unsigned char);
extern void _mgv_setup_VGI1DMA(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char*, unsigned char, unsigned int);

extern __uint32_t      _mgv_CC1PollReg(unsigned char, unsigned char, unsigned char, __uint32_t ,unsigned char*);

extern void _mgv_make_vid_array( unsigned int *, unsigned char, unsigned int, unsigned int, unsigned int, unsigned int, int, int, int, int);

extern int write_CC1_field(uint,  uint, uint, uint, uint, unsigned char *);
extern void test_allstds(unsigned long *, int, unsigned char, int, int);
extern int _mgv_compare_byte_buffers(long, long, unsigned char *, unsigned char *, unsigned char, unsigned char);

extern void _mgv_setup_cc1_out(unsigned char, unsigned char*, unsigned char*);
extern int rd_CC1_field(uint,  uint, uint, uint, uint, unsigned char *);

#ifdef IP30
extern void mgv_reset_i2c(void);
extern uchar_t _mgv_inb_I2C(int);
extern void _mgv_outb_I2C(int, int);
extern __uint32_t mgv_initI2C(void);
extern uchar_t mgv_I2C_edh_seq(int, int, int, int);
extern void mgv_select_edh_part(int,int,int,int);
extern int _mgv_edh_rw(int addr, uchar_t reg, uchar_t *val, int rw);
extern int _mgv_edh_w1(uchar_t reg, uchar_t val);
extern int _mgv_edh_r1(uchar_t reg, uchar_t *);
extern void mgras_probe_all_widgets();
extern __uint32_t  _mgv_probe_bridge(void);
extern __uint32_t  _mgv_initBridge(giobr_bridge_config_t);
extern __uint32_t  mgv_br_rrb_alloc(gioio_slot_t, int);
#endif

/*
 * Forward Function References
 */
__uint32_t      mgv_CC1DramAddrBusTest(void);
__uint32_t      mgv_CC1DramDataBusTest(void);
__uint32_t      mgv_CC1DramPatrnTest(void);
__uint32_t      _mgv_CC1WaitForTransferReady (mgvdiag *mgvbase);
void            _mgv_CC1WriteFrameBufferSetup (mgvdiag *mgvbase,
                __uint32_t fld_frm_sel, __uint32_t startline);
void            _mgv_CC1ReadFrameBufferSetup (mgvdiag *mgvbase,
                __uint32_t fld_frm_sel, __uint32_t startline);

__uint32_t      mgv_VGI1IntTest (__uint32_t argc, char **argv);


/* MACROS */
#define	AB_W1(mgvbase, reg, _val) {					\
  msg_printf(DBG,"Entering AB_W1 macro\n");                             \
  _mgv_outb(mgvbase->ab|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1,_val);	\
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	AB_W4(mgvbase, reg, _val) {					\
  _mgv_outw(mgvbase->ab|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_4,_val); \
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	AB_R1(mgvbase, reg, _val) {					\
  _val=_mgv_inb(mgvbase->ab|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1);	\
}

#define	AB_R4(mgvbase, reg, _val) {					\
  _val=_mgv_inw(mgvbase->ab|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_4);	\
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	CC_W1(mgvbase, reg, _val) {					\
  _mgv_outb(mgvbase->cc|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1,_val);	\
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	CC_W4(mgvbase, reg, _val) {					\
  msg_printf(DBG, "before CC_W4...\n");					\
  _mgv_outw(mgvbase->cc|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_4,_val); \
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	CC_R1(mgvbase, reg, _val) {					\
  _val=_mgv_inb(mgvbase->cc|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1);	\
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	CC_R4(mgvbase, reg, _val) {					\
  msg_printf(DBG, "before CC_R4...\n");					\
  _val=_mgv_inw(mgvbase->cc|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_4);	\
  msg_printf(DBG, "mgvbase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase, reg, _val);						\
}

#define	CC_IND_W1(mgvbase, reg, _val) {				\
  _mgv_outb(mgvbase->cc|MGRAS_DCB_CRS(CC_INDIR1)|MGRAS_DCB_DATAWIDTH_1,reg);	\
  _mgv_outb(mgvbase->cc|MGRAS_DCB_CRS(CC_INDIR2)|MGRAS_DCB_DATAWIDTH_1,_val);	\
  msg_printf(DBG, "CC_IND_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

#define	CC_IND_R1(mgvbase, reg, _val) {				\
  _mgv_outb(mgvbase->cc|MGRAS_DCB_CRS(CC_INDIR1)|MGRAS_DCB_DATAWIDTH_1,reg);	\
  _val=_mgv_inb(mgvbase->cc|MGRAS_DCB_CRS(CC_INDIR2)|MGRAS_DCB_DATAWIDTH_1);	\
  msg_printf(DBG, "CC_IND_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

/*  msg_printf(DBG, "mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1 0x%x; val 0x%x\n", mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1, _val);	\*/
#define CSC_W1(mgvbase, reg, _val) {					\
  _mgv_outb(mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1,_val);\
}

/*   msg_printf(DBG, "mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1 0x%x; val 0x%x\n", mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1, _val);	\ */

#define	CSC_R1(mgvbase, reg, _val) {					\
  _val=_mgv_inb(mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1);	\
}

#if 0
#define CSC_IND_W1(mgvbase, reg, _val) {					\
  msg_printf(DBG, "mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_ADDR)|MGRAS_DCB_DATAWIDTH_1 0x%x; reg 0x%x val 0x%x\n", mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_ADDR)|MGRAS_DCB_DATAWIDTH_1, reg, _val);	\
  _mgv_outb(mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_ADDR)|MGRAS_DCB_DATAWIDTH_1,reg);\
  _mgv_outb(mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_DATA)|MGRAS_DCB_DATAWIDTH_1,_val);	\
}
#endif

#define CSC_IND_W1(mgvbase, reg, _val) {					\
	CSC_W1(mgvbase,CSC_ADDR,reg);	\
	CSC_W1(mgvbase,CSC_DATA,_val);	\
}

#if 0
#define	CSC_IND_R1(mgvbase, reg, _val) {					\
  _mgv_outb(mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_ADDR)|MGRAS_DCB_DATAWIDTH_1,reg);	\
  _val=_mgv_inb(mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_DATA)|MGRAS_DCB_DATAWIDTH_1);	\
  msg_printf(DBG, "mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_DATA)|MGRAS_DCB_DATAWIDTH_1 0x%x; val 0x%x\n", mgvbase->csc_tmi|MGRAS_DCB_CRS(CSC_DATA)|MGRAS_DCB_DATAWIDTH_1, _val);	\
}
#endif

#define CSC_IND_R1(mgvbase, reg, _val) {					\
	CSC_W1(mgvbase,CSC_ADDR,reg);	\
	CSC_R1(mgvbase,CSC_DATA,_val);	\
}

#define TMI_W1(mgvbase, reg, _val) {					\
  _mgv_outb(mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1,_val);\
}

#define	TMI_R1(mgvbase, reg, _val) {					\
  _val=_mgv_inb(mgvbase->csc_tmi|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1);	\
}

#ifdef IP30

#define TMI_IND_W1(mgvbase, reg, _val) {					\
	GAL_W1(mgvbase,TMI_ADDR,reg);	\
	GAL_W1(mgvbase,TMI_DATA,_val);	\
}

#define TMI_IND_R1(mgvbase, reg, _val) {					\
	GAL_W1(mgvbase,TMI_ADDR,reg);	\
	GAL_R1(mgvbase,TMI_DATA,_val);	\
}

#define SCL_IND_W1(mgvbase, reg, _val) {					\
	GAL_W1(mgvbase,SCL_ADDR,reg);	\
	GAL_W1(mgvbase,SCL_DATA,_val);	\
}

#define SCL_IND_R1(mgvbase, reg, _val) {					\
	GAL_W1(mgvbase,SCL_ADDR,reg);	\
	GAL_R1(mgvbase,SCL_DATA,_val);	\
}

#define MGV_I2C_BUSY()    (_mgv_inb_I2C(GAL_IIC_CTL) & 0x80)

#endif

#define	GAL_IND_W1(mgvbase, reg, _val) {				\
  _mgv_outb(mgvbase->gal_15|MGRAS_DCB_CRS(GAL_DIR0)|MGRAS_DCB_DATAWIDTH_1,reg);	\
  _mgv_outb(mgvbase->gal_15|MGRAS_DCB_CRS(GAL_DIR1)|MGRAS_DCB_DATAWIDTH_1,_val);	\
  msg_printf(DBG, "GAL_IND_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

#define	GAL_IND_R1(mgvbase, reg, _val) {				\
  _mgv_outb(mgvbase->gal_15|MGRAS_DCB_CRS(GAL_DIR0)|MGRAS_DCB_DATAWIDTH_1,reg);	\
  _val=_mgv_inb(mgvbase->gal_15|MGRAS_DCB_CRS(GAL_DIR1)|MGRAS_DCB_DATAWIDTH_1);	\
  msg_printf(DBG, "GAL_IND_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

#define	GAL_W1(mgvbase, reg, _val) {				\
  msg_printf(DBG, "before GAL_W1...\n");				\
  _mgv_outb(mgvbase->gal_15|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1,_val);	\
  msg_printf(DBG, "mgvbase->gal_15 = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase->gal_15, reg, _val);						\
}

#define	GAL_R1(mgvbase, reg, _val) {				\
  msg_printf(DBG, "before GAL_R1...\n");				\
  _val=_mgv_inb(mgvbase->gal_15|MGRAS_DCB_CRS(reg)|MGRAS_DCB_DATAWIDTH_1);	\
  msg_printf(DBG, "mgvbase->gal_15 = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase->gal_15, reg, _val);						\
}

#define	MGV_SET_VGI1_1_BASE {						\
	mgvbase->giobase = mgvbase->vgi1_1_base;			\
}

#define	MGV_SET_VGI1_2_BASE {						\
	mgvbase->giobase = mgvbase->vgi1_2_base;			\
}


#define	VGI1_W32(mgvbase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_W32...\n");				\
  *(__uint32_t *)((__paddr_t) mgvbase->giobase + reg) = val & mask;	\
  msg_printf(DBG, "mgvbase->giobase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase->giobase, reg, val);						\
}

#define	VGI1_R32(mgvbase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_R32...\n");				\
  val = *(__uint32_t *)((__paddr_t) mgvbase->giobase + reg) & mask;	\
  msg_printf(DBG, "mgvbase->giobase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	mgvbase->giobase, reg, val);						\
}

#define	VGI1_W64(mgvbase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_W64...\n");				\
  mgv_vgi1_store_ll((((ulonglong_t)val) & mask),(void *)((__paddr_t) mgvbase->giobase + reg)); \
  msg_printf(DBG,"mgvbase->giobase = 0x%x; reg = 0x%x; value = 0x%llx;\n",	\
	mgvbase->giobase, reg, ((ulonglong_t)val));						\
  msg_printf(DBG, "after VGI1_W64...\n");				\
}

#define	VGI1_R64(mgvbase, reg, val, mask) {				\
  ulonglong_t __val;							\
  msg_printf(DBG, "before VGI1_R64...\n");				\
  __val = (ulonglong_t) mgv_vgi1_load_ll((void *)((__paddr_t)mgvbase->giobase + reg)); 	\
  val = __val & mask;							\
  msg_printf(DBG,"mgvbase->giobase = 0x%x; reg = 0x%x; value = 0x%llx;\n",	\
	mgvbase->giobase, reg, __val);						\
  msg_printf(DBG, "after VGI1_R64...\n");				\
}

/*
* Given a 10-bit or 12-bit value, the following two macros parse it into
* two bytes. Low byte contains 8 LSBits of the original value, and
* high byte contains either 2 or 4 MSBits, depending on it's 10 or 12
* bit.
*/
#define getHiByte(val) \
    ((val & ~0xff)>>8)

#define getLowByte(val) \
	(val & 0xff)

/*
 * Set default high and low bytes
 */
#define setDefaultVoutTiming(low_reg, hi_reg) \
  	GAL_IND_W1(mgvbase, low_reg, getLowByte(0x800)); \
	GAL_IND_W1(mgvbase, hi_reg, getHiByte(0x800));

#define setDefaultFreeRun \
	GAL_IND_W1(mgvbase, GAL_VCXO_LO, getLowByte(0x800)); \
	GAL_IND_W1(mgvbase, GAL_VCXO_HI, getHiByte(0x800));

#if 0
/* Check this XXX */

#define setDefaultBlackTiming \
	GAL_IND_W1(mgvbase, GAL_FLORA_LO, getLowByte(0x200)); \
	GAL_IND_W1(mgvbase, GAL_FLORA_HI, getHiByte(0x200));

#endif
#define setDefaultBlackTiming \
        GAL_IND_W1(mgvbase, GAL_REF_OFFSET_LO, getLowByte(0x200)); \
        GAL_IND_W1(mgvbase, GAL_REF_OFFSET_HI, getHiByte(0x200));


/*
 * Read high or low bytes
 */
#define readHiByte(val) \
	(val & 0xf)

#define readLowByte(val) \
	(val & 0xff)

#endif /* _MGV_DIAG_H_ */
