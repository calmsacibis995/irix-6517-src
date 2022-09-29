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

#ifndef _EVO_DIAG_H_

#define _EVO_DIAG_H_

#if (defined(IP28) || defined(IP30))
#define __paddr_t               __uint64_t
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
#include "sys/errno.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"          /* for ASSERT() */
#include "sys/GIO/giobr.h"     /* for all the structure defs */
#include "sys/PCI/bridge.h"
#include "sys/cpu.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "sys/evo_extern.h" 
#include "sys/evo_vgi1_hw.h"
#include "ide_xbow.h"


/* CONSTANTS */
#define EVO_BOARD_INIT_NUM	 1
#define	EVO_BRIDGE_INIT_NUM	 2
#define	EVO_INPUT_INIT_NUM	 3  /*Moosecam*/
#define	EVO_XPOINT_1_INIT_NUM	 4
#define	EVO_XPOINT_2_INIT_NUM	 5
#define	EVO_VGI1_1_INIT_NUM	 6
#define	EVO_VGI1_2_INIT_NUM	 7 
#define	EVO_CSC_1_INIT_NUM	 8	
#define	EVO_CSC_2_INIT_NUM	 9 
#define	EVO_CSC_3_INIT_NUM	10 
#define	EVO_SCL_1_INIT_NUM	11 
#define EVO_SCL_2_INIT_NUM	12
#define	EVO_VOC_INIT_NUM	13
#define	EVO_VOC_INT_INIT_NUM	14
#define	EVO_ENC_INIT_NUM	15
#define	EVO_DEC_INIT_NUM	16
#define	EVO_CFPGA_INIT_NUM	17 
#define	EVO_GENLOCK_INIT_NUM	18
#define	EVO_VGIALL_INIT_NUM	19
#define	EVO_CSCALL_INIT_NUM	20
#define	EVO_LAST_DEVICE_NUM	20

#define	MAX_XTALK_PORT_INFO_INDEX 	7

/*
 * Board Constants 
 */
#define	EVO_BASE0_PHYS       0x1f000000L   /*XXX Probably not needed Evo base */
#define VGI1_1_BASE          0x1f000000L   /*XXX Probably not needed VGI1 base*/
#define VGI1_2_BASE          0x1f400000L   /*XXX Probably not needed VGI2 base*/
#define GIO_PIO_BASE         0x1f500000L   /*XXX Probably not needed GIO - PIO base*/

#define I2C_OFFSET        0x1004
#define FPG_OFFSET        0x2004
#define SCL_1_OFFSET      0x3004
#define SCL_2_OFFSET      0x3804 
#define DCB_CRS_OFFSET 	  0x4004
#define CSC_1_OFFSET      0x5004
#define CSC_2_OFFSET      0x5804 
#define CSC_3_OFFSET      0x5c04 
#define VBAR_1_OFFSET     0x6004
#define VBAR_2_OFFSET     0x6804 
#define MISC_OFFSET	  0xb004
#define BUF_OFFSET        0x7404  /*XXX not yet defined*/
#define ENC_OFFSET        0x8004  /*XXX not yet defined*/
#define DEC_OFFSET        0x9004  /*XXX not yet defined*/
#define VOC_OFFSET        0xf004  /*XXX not yet defined*/


/*
 * Utility Constants
 */
#define EIGHTBIT                8
#define TENBIT                  10
#define WALK1                   0x01
#define WALK0                   0xFE
#define EVO_NO_ACCESS		0
#define EVO_READ_WRITE		1
#define EVO_READ_ONLY		2
#define EVO_WRITE_ONLY		3
#define EVO_DBL_BUFFER		4
#define EVO_VE			5
#define EVO_VSDT		6
#define EVO_GXL_COUNT		7
#define EVO_VOF_CONTROL		8
#define EVO_VDRC_CONTROL	9	
#define EVO_PLL_CONTROL		10
#define EVO_GEN_CONTROL		11
#define EVO_FIFO_CONTROL	12
#define EVO_FIFO_PTR		13
#define EVO_SILT		14

/*
*   VOC Constants
*/
#define VOF_PIXFIFO_SIZE		512


/*
 *      VOC registers   
 */
  #define VOC_STATUS              0x0     /* VOC status register */
  #define VOC_CONTROL             0x1     /* VOC control register */
  #define VOC_DBUFFER             0x2     /* double buffer register */
  #define VOC_IWSC_CONTROL        0x3     /* IWSC control:screen# & ip screen hor. line length in groupxels */
  #define VOC_IWSC_OFFSETS        0x4     /* IWSC vertical/horizontal offsets */
  #define VOC_IWSC_SIZES          0x5     /* IWSC vertical/horizontal size */
  #define VOC_VOF_CONTROL         0x6     /* VOF control register */
  #define VOC_VOF_SIGNAL_INIT     0x7     /* signal initial level table */
  #define VOC_VPHASE              0x8     /* vertical phase register */
  #define VOC_VSTATE_DUR_TBL      0x9     /* vertical state duration table */
  #define VOC_E0_GRP_CNT          0xa     /* edge 0 group pixel count */
  #define VOC_E1_GRP_CNT          0xb     /* edge 1 group pixel count */
  #define VOC_E2_GRP_CNT          0xc     /* edge 2 group pixel count */
  #define VOC_E3_GRP_CNT          0xd     /* edge 3 group pixel count */
  #define VOC_E4_GRP_CNT          0xe     /* edge 4 group pixel count */
  #define VOC_E5_GRP_CNT          0xf     /* edge 5 group pixel count */
  #define VOC_E6_GRP_CNT          0x10    /* edge 6 group pixel count */
  #define VOC_E7_GRP_CNT          0x11    /* edge 7 group pixel count */
  #define VOC_VALID_EDGES_LO      0x12    /* valid edge value low word */
  #define VOC_VALID_EDGES_HI      0x13    /* valid edge value high word */
  #define VOC_VOF_PIXEL_BLANK     0x14    /* pixel blanking value */
  #define VOC_PFD_CONTROL         0x30    /* frame detector control register */
  #define VOC_PFD_FRM_SEQ_TBL_LO  0x31    /* frame sequence table hi word */
  #define VOC_PFD_FRM_SEQ_TBL_HI  0x32    /* frame sequence table lo word */
  #define VOC_PFD_DUR_CTR_OLD     0x33    /* duration counter PFD previous count: read only */
  #define VOC_PSD_DUR_CTR         0x34    /* independent duration counter for PSD*/
  #define VOC_PSD_CONTROL         0x43    /* sync detector control register */
  #define VOC_VDRC_CONTROL        0x44    /* VDRC control register */
  #define VOC_VDRC_MAW            0x45    /* VDRC Managed Area Width register */
  #define VOC_VDRC_BEG_LINE       0x47    /* VDRC start line parameters */
  #define VOC_VDRC_MID_LINE       0x48    /* VDRC mid line parameters */
  #define VOC_VDRC_END_LINE       0x49    /* VDRC end line parameters */
  #define VOC_VDRC_GXEL_OFFSET    0x4a    /* VDRC groupxel offset (goupxels skipped) */
  #define VOC_VDRC_REQZ		  0x4b    /* Request Z or not per field*/
  #define VOC_VDRC_REQ0           0x50    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ1           0x51    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ2           0x52    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ3           0x53    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ4           0x54    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ5           0x55    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ6           0x56    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ7           0x57    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ8           0x58    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ9           0x59    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQa           0x5a    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQb           0x5b    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQc           0x5c    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQd           0x5d    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQe           0x5e    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQf           0x5f    /* VDRC field request table: addresses 0x50 through 0x5f */
  #define VOC_VDRC_REQ_PATTERN0   0x60    /* VDRC field req. pattern tbl: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN1   0x61    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN2   0x62    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN3   0x63    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN4   0x64    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN5   0x65    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN6   0x66    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN7   0x67    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN8   0x68    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERN9   0x69    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNa   0x6a    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNb   0x6b    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNc   0x6c    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNd   0x6d    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNe   0x6e    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_VDRC_REQ_PATTERNf   0x6f    /* VDRC field request table: addresses 0x60 through 0x6f */
  #define VOC_PLL_CONTROL         0x70    /* phase lock loop control register */
  #define VOC_PLL_ADDRESS         0x71    /* phase lock loop address register */
  #define VOC_PLL_READBACK     	  0x72    /* phase lock loop readback register */
  #define VOC_PLL_DATA            0x73    /* phase lock loop data register */
  #define VOC_GEN_CONTROL         0x81    /* genlock control register */
  #define VOC_GEN_HMASK_1         0x82    /* genlock HMASK control register */
  #define VOC_GEN_HMASK_2         0x83    /* genlock HMASK window start/width register */
  #define VOC_GEN_BPCLAMP         0x84    /* genlock BP_CLAMP control register */
  #define VOC_FIFO_CONTROL        0x8a    /* line fifo control regsiter */
  #define VOC_FIFO_RW_POINTER     0x8b    /* line fifo write/read pointer */
  #define VOC_SAC_CONTROL         0x8d    /* signature control register */
  #define VOC_SIZ_CONTROL         0x8f    /* resizing control register */
  #define VOC_SIZ_X_COEF_MAX      0x90    /* resizing control register */
  #define VOC_SIZ_X_FLTR_SLP      0x91    /* resizing control register */
  #define VOC_SIZ_X_DELTA         0x92    /* resizing control register */
  #define VOC_SIZ_X_C0_BEG        0x93    /* resizing control register */
  #define VOC_SIZ_X_C1_BEG        0x94    /* resizing control register */
  #define VOC_SIZ_X_C2_BEG        0x95    /* resizing control register */
  #define VOC_SIZ_X_C3_BEG        0x96    /* resizing control register */
  #define VOC_SIZ_Y_COEF_MAX      0x97    /* resizing control register */
  #define VOC_SIZ_Y_FLTR_SLP      0x98    /* resizing control register */
  #define VOC_SIZ_Y_DELTA         0x99    /* resizing control register */
  #define VOC_SIZ_Y_S0_F0_BEG     0x9a    /* resizing control register */
  #define VOC_SIZ_Y_S1_FO_BEG     0x9b    /* resizing control register */
  #define VOC_SIZ_Y_S0_F1_BEG     0x9c    /* resizing control register */
  #define VOC_SIZ_Y_S1_F1_BEG     0x9d    /* resizing control register */

  #define VOC_MAX_REGS		  157	  /*Max. VOC Register Number*/

/*
 *      Board registers
 */
  #define EVO_READBACK            0x0

/*
 *      IIC registers
 */
  #define EVO_IIC_CONTROL         0x0
  #define EVO_IIC_STATUS          0x10

/*
 *      FPGA registers for device setup/control
 */
  #define PIO_VOC_PIXELS         0x2004 /*voc pixels per line*/
  #define PIO_VOC_LINES          0x2014 /*voc lines per field*/
  #define PIO_SCL1_PIXELS        0x2204
  #define PIO_SCL1_LINES         0x2214
  #define PIO_SCL2_PIXELS        0x2404
  #define PIO_SCL2_LINES         0x2414


/*
 *      FPGA register addresses during FPGA configuration mode
 */
  #define CONF_VOC_FPGA           0x2004 /*voc pixels per line*/
  #define CONF_SCL1_IP_FPGA   	  0x2104
  #define CONF_SCL1_OP_FPGA       0x2204
  #define CONF_SCL2_IP_FPGA   	  0x2304
  #define CONF_SCL2_OP_FPGA       0x2404
  #define CONF_BPICKER_FPGA       0x2504
  #define CONF_REFGEN_FPGA        0x2604
  #define CONF_NSQ_FIL_FPGA       0x2704
  #define CONF_FM_RST_FPGA        0x2804


/*
 *   Misc Registers (addresses from 0x7004+) 
 */
    #define MISC_STATUS_CONF	    0x0 /*9 bits*/
    #define MISC_FPGA_DONE	    0x100 /*9 bits*/
    #define MISC_SW_RESET           0x100
    #define MISC_PLL_CONTROL	    0x200
    #define MISC_FILT_TRIG_CTL      0x300
    #define MISC_FMBUF_DATA    	    0x400
    #define MISC_FMBUF_CONTROL 	    0x500

    #define MAX_FMBUF_ADDRESS  	    0x1ffff /*frame buffer depth*/


/*
 *   MISC Register Values for different cases
 */

     #define VAL_EVO_BOARD_RESET	0xe
     #define VAL_EVO_BOARD_NORM		0xf
     #define VAL_EVO_VGIPLL_RESET	0xd
     #define VAL_EVO_VGIPLL_NORM	0xb /*Needed if both VGIPLL & VGI are reset, VGIPLL reset must be
					      removed before the end of VGI reset*/
     #define VAL_EVO_VGI_RESET		0xb
     #define VAL_EVO_CSC_RESET		0x7

/*
 *XXX   Misc Registers defined from Galileo code...will need changing
 */

    #define MISC_TV_STD             0x0
    #define MISC_VIN_CTRL           0x01
    #define MISC_VIN_CH1_STATUS     0x02
    #define MISC_VIN_CH2_STATUS     0x03
    #define MISC_CH1_CTRL           0x04 /*MISC_VOUT1_CTRL in driver*/
    #define MISC_CH2_CTRL           0x05 /*MISC_VOUT2_CTRL in driver*/
    #define MISC_REF_CTRL           0x06
    #define MISC_BDREV              0x08
    #define MISC_TRIG_STATUS        0x09
    #define MISC_FIRMWARE           0x0a
    #define MISC_GENLOCK_STATUS     0x0b
    #define MISC_ADC_MODE           0x0c
    #define MISC_ADC_LO             0x0d
    #define MISC_ADC_HI             0x0e
    #define MISC_VCXO_LO            0x0f
    #define MISC_VCXO_HI            0x10
    #define MISC_TRIG_CTRL          0x11
    #define MISC_CH1_HPHASE_LO	    0x14
    #define MISC_CH1_HPHASE_HI	    0x15
    #define MISC_CH2_HPHASE_LO	    0x16
    #define MISC_CH2_HPHASE_HI	    0x17
    #define MISC_REF_OFFSET_LO	    0x18
    #define MISC_REF_OFFSET_HI	    0x19
    #define MISC_MUX_ADDR	    0x19 /*VBAR controller*/

/*
 *  Scaler Registers
 */
  #define Y_SCALE_VSRC_CNT_LSB       0x00
  #define Y_SCALE_VSRC_CNT_MSB       0x01
  #define Y_SCALE_HSRC_CNT_LSB       0x02
  #define Y_SCALE_HSRC_CNT_MSB       0x03
  #define Y_SCALE_VTRG_CNT_LSB       0x04
  #define Y_SCALE_VTRG_CNT_MSB       0x05
  #define Y_SCALE_HTRG_CNT_LSB       0x06
  #define Y_SCALE_HTRG_CNT_MSB       0x07
  #define UV_SCALE_VSRC_CNT_LSB      0x08
  #define UV_SCALE_VSRC_CNT_MSB      0x09
  #define UV_SCALE_HSRC_CNT_LSB      0x0a
  #define UV_SCALE_HSRC_CNT_MSB      0x0b
  #define UV_SCALE_VTRG_CNT_LSB      0x0c
  #define UV_SCALE_VTRG_CNT_MSB      0x0d
  #define UV_SCALE_HTRG_CNT_LSB      0x0e
  #define UV_SCALE_HTRG_CNT_MSB      0x0f
  #define SCALE_VIN_DELAY_ODD        0x10 
  #define SCALE_VIN_DELAY_EVEN       0x11 
  #define SCALE_MAX_REGS             17  /*total number of scaler registers*/


/*
 *  CSC Registers
 */
#define CSC_ADDR              0 /*CSC Indirect Address Register*/
#define CSC_DATA              1 /*CSC Indirect Data Register*/
#define CSC_MODE              2 /*CSC_MODE is #2 in kernel code*/
#define CSC_ID                3 /*ID register, Read Only*/ 
#define CSC_DIR4              4 /*Unused*/
#define CSC_DIR5              5 /*Unused*/
#define CSC_DIR6              6 /*Programmable Parts*/
#define CSC_DIR7              7 /*Programmable Parts*/


/*CSC Mode Register Values*/
/*Put LUT in disabled state prior to switching between Read/Normal modes*/

#define CSC_RESET			(1 << 0)
#define CSC_BYPASS			(1 << 1)
#define CSC_DISABLED			0
#define CSC_LUT_LOAD_READ		(1 << 2)	
#define CSC_NORMAL_MODE			(1 << 3)	


/*
 *  CSC Indirect Registers
 *  CSC indirect addresses from 6 - FF are not used
 */

#define CSC_IN_CTRL             0x0 /*#0x1 in kernel code*/
#define CSC_OUT_CTRL            0x1 /*#0x0 in kernel code*/
#define CSC_OFFSET_LS           0x2 /*#0x2 in kernel header*/
#define CSC_OFFSET_MS           0x3 /*#0x3 in kernel header*/
#define CSC_RW_LUTS             0x4 /*from kernel header*/
#define CSC_RW_COEF             0x5 /*from data sheet*/

/*
 * CSC CONSTANTS
 */

#define EVO_CSC_MAX_LOAD	1024
#define EVO_CSC_MAX_COEF	9


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

/* XXX bit field 0,1 of csc ind addr 0 Check these definitions*/
/* XXX bit fields 3-7 of csc ind addr 0 are not used*/

/* #define	CSC_YUV422_IN			0  
#define	CSC_INPUT_DIF_ENABLE		1
#define	CSC_TRC_INPUT_CHECK	        2 
*/

/* XXX bit field 0 - 7 of csc ind addr 1 Check these definitions*/
/*
#define	CSC_YUV422_OUT			0  
#define	CSC_OUTPUT_DIF_ENABLE		1
#define	CSC_OUT_RGB_BLK_LEVEL	        2 
#define CSC_OUT_FULL_SCL		3
#define CSC_K_MUX_SEL			4
#define CSC_ALPHA_MUX_SEL		5
#define CSC_BLACK_OUT			6
*/

/*XXX CSC COEF Offsets, COEF are 15 bit wide, with the LSB written first*/
#define CSC_YG_C0_COEF			0x0
#define CSC_YG_C1_COEF			0x1
#define CSC_YG_C2_COEF			0x2
#define CSC_UB_C0_COEF			0x3  
#define CSC_UB_C1_COEF			0x4
#define CSC_UB_C2_COEF			0x5
#define CSC_VR_C0_COEF			0x6
#define CSC_VR_C1_COEF			0x7
#define CSC_VR_C2_COEF			0x8

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
#define	VGI1_DMA_READ			0
#define	VGI1_DMA_WRITE			1
#define	VGI1_NO_VIDEO_INPUT		0xfff

/*
 * VGI1 - GIO Device Addr
 * XXX: Please verify the addresses for both VGI1s
 */
#define	EVO_VGI1_1_DEV_ADDR	0x1F000000
#define	EVO_VGI1_2_DEV_ADDR	0x1F400000
#define	EVO_GIO_PIO_DEV_ADDR	0x1F500000

/*
 * XXX Crosspoint input switch choices for VBAR_1-EVO. Require updating.
 * Output registers are loaded with input number to establish connections
 */

#define MUX_VOC_C1	           0x0
#define MUX_VOC_C2	     	   0x1
#define MUX_CSC_1_A  		   0x2
#define MUX_CSC_1_B         	   0x3
#define MUX_SCALER1_IN             0x4
#define MUX_UNUSED1                0x5
#define MUX_VGI1_CH1_IN            0x6
#define MUX_VGI1_CH2_IN            0x7
#define MUX_TO_OTHER_VBAR_CH1      0x8
#define MUX_TO_OTHER_VBAR_CH2      0x9
#define MUX_ANALOG_IN	   	   0xa /*encoder*/
#define MUX_CAM_IN          	   0xb /*moosecam*/
#define MUX_UNUSED2                0xc
#define MUX_UNUSED3                0xd
#define MUX_UNUSED4                0xe
#define MUX_UNUSED5           	   0xf

/*
 * XXX MUX (VBAR_1) Output Register Definitions ...These need to be updated 
 * Output registers are loaded with input number to establish connections
 */
  #define EVOA_MUX_UNUSED1         0xf0
  #define EVOA_MUX_UNUSED2         0xf1
  #define EVOA_CSC_1_A             0xf2
  #define EVOA_CSC_1_B             0xf3
  #define EVOA_SCALER1_IN          0xf4
  #define EVOA_MUX_UNUSED3         0xf5
  #define EVOA_VGI1_CH1_IN         0xf6
  #define EVOA_VGI1_CH2_IN         0xf7
  #define EVOA_TO_OTHER_VBAR_CH1   0xf8
  #define EVOA_TO_OTHER_VBAR_CH2   0xf9
  #define EVOA_VIDEO_CH1_OUT       0xfa 
  #define EVOA_VIDEO_CH2_OUT       0xfb 
  #define EVOA_MUX_UNUSED4         0xfc
  #define EVOA_MUX_UNUSED5         0xfd
  #define EVOA_MUX_UNUSED6         0xfe
  #define EVOA_MUX_UNUSED7         0xff

/*
 * XXX Crosspoint input switch choices for VBAR_2-EVO. Require updating.
 * Output registers are loaded with input number to establish connections
 */
#define MUXB_UNUSED1	             0x0
#define MUXB_UNUSED2     	     0x1
#define MUXB_CSC_2_A  		     0x2
#define MUXB_CSC_2_B         	     0x3
#define MUXB_SCALER2_IN              0x4
#define MUXB_BLACK_REF_IN            0x5
#define MUXB_VGI1B_CH1_IN            0x6
#define MUXB_VGI1B_CH2_IN            0x7
#define MUXB_TO_OTHER_VBAR_CH1       0x8
#define MUXB_TO_OTHER_VBAR_CH2       0x9
#define MUXB_UNUSED3	   	     0xa /*encoder*/
#define MUXB_UNUSED4          	     0xb /*moosecam*/
#define MUXB_UNUSED5                 0xc
#define MUXB_UNUSED6                 0xd
#define MUXB_UNUSED7                 0xe
#define MUXB_UNUSED8           	     0xf

/*
 * XXX MUX (VBAR_2) Output Register Definitions ...These need to be updated 
 * Output registers are loaded with input number to establish connections
 */
  #define EVOB_MUXB_UNUSED1          0xf0
  #define EVOB_MUXB_UNUSED2          0xf1
  #define EVOB_CSC_2_A               0xf2
  #define EVOB_CSC_2_B               0xf3
  #define EVOB_SCALER2_IN            0xf4
  #define EVOB_MUXB_UNUSED3          0xf5
  #define EVOB_VGI1B_CH1_IN          0xf6
  #define EVOB_VGI1B_CH2_IN          0xf7
  #define EVOB_TO_OTHER_VBAR_CH1     0xf8
  #define EVOB_TO_OTHER_VBAR_CH2     0xf9
  #define EVOB_MUX_UNUSED4           0xfa
  #define EVOB_MUX_UNUSED5           0xfb
  #define EVOB_MUX_UNUSED6           0xfc
  #define EVOB_MUX_UNUSED7           0xfd
  #define EVOB_MUX_UNUSED8           0xfe
  #define EVOB_MUX_UNUSED9           0xff

/*
 * Don't forget to reset these if the above MUX_ definitions change
 */
  #define MUX_MIN                 MUX_UNUSED1
  #define MUX_MAX                 MUX_UNUSED9

/*
Select any of these as an input to EVO 
crosspoint, (VBAR1), output switches.
*/

#define EVO_XPOINT_IN_SHIFT		4
#define VIDEO_IN_SHIFT                  (VIDEO_IN		<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR1_CH1_OUT_SHIFT        (VGI1_VBAR1_CH1_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR1_CH2_OUT_SHIFT        (VGI1_VBAR1_CH2_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR1_SCALER_OUT_SHIFT     (VGI1_VBAR1_SCALER_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR1_CH1_CSC_OUT_SHIFT    (VGI1_VBAR1_CH1_CSC_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR1_CH2_CSC_OUT_SHIFT    (VGI1_VBAR1_CH2_CSC_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR2_CH1_OUT_SHIFT        (VGI1_VBAR2_CH1_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR2_CH2_OUT_SHIFT        (VGI1_VBAR2_CH2_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR2_SCALER_OUT_SHIFT     (VGI1_VBAR2_SCALER_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR2_CH1_CSC_OUT_SHIFT    (VGI1_VBAR2_CH1_CSC_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VGI1_VBAR2_CH2_CSC_OUT_SHIFT    (VGI1_VBAR2_CH2_CSC_OUT	<< EVO_XPOINT_IN_SHIFT)
#define VOC_CH1_OUT_SHIFT               (VOC_CH1_OUT		<< EVO_XPOINT_IN_SHIFT)
#define VOC_CH2_OUT_SHIFT               (VOC_CH2_OUT		<< EVO_XPOINT_IN_SHIFT)
#define BLACK_REF_OUT_SHIFT         	(BLACK_REF_OUT		<< EVO_XPOINT_IN_SHIFT)
#define VBAR1_FROM_VBAR2_CH1_SHIFT      (VBAR1_FROM_VBAR2_CH1	<< EVO_XPOINT_IN_SHIFT)
#define VBAR1_FROM_VBAR2_CH2_SHIFT      (BAR1_FROM_VBAR2_CH2	<< EVO_XPOINT_IN_SHIFT)
#define VBAR2_FROM_VBAR1_CH1_SHIFT      (BAR2_FROM_VBAR1_CH1	<< EVO_XPOINT_IN_SHIFT)
#define VBAR2_FROM_VBAR1_CH2_SHIFT      (BAR2_FROM_VBAR1_CH2	<< EVO_XPOINT_IN_SHIFT)



/* Center value for Horizontal Phase. */
#define HPHASE_CENTER   0xc00
#define DAC_CENTER      2048
#define BOFFSET_CENTER  0x200

/*
 *Scaler Register Definitions 
*/ 

#define SCALE_VSRC_CNT_LSB 		0x00
#define SCALE_VSRC_CNT_MSB      	0x01
#define SCALE_HSRC_CNT_LSB      	0x02
#define SCALE_HSRC_CNT_MSB      	0x03
#define SCALE_VTRG_CNT_LSB      	0x04
#define SCALE_VTRG_CNT_MSB      	0x05
#define SCALE_HTRG_CNT_LSB      	0x06
#define SCALE_HTRG_CNT_MSB      	0x07
#define UV_SCALE_VSRC_CNT_LSB      	0x08
#define UV_SCALE_VSRC_CNT_MSB      	0x09
#define UV_SCALE_HSRC_CNT_LSB      	0x0a
#define UV_SCALE_HSRC_CNT_MSB      	0x0b
#define UV_SCALE_VTRG_CNT_LSB      	0x0c
#define UV_SCALE_VTRG_CNT_MSB      	0x0d
#define UV_SCALE_HTRG_CNT_LSB      	0x0e
#define UV_SCALE_HTRG_CNT_MSB      	0x0f
#define SCALE_VIN_DELAY_ODD     	0x10
#define SCALE_VIN_DELAY_EVEN    	0x11


#define I2C_MAX_RETRY_COUNT 3

#define I2C_FORCE_IDLE  (0 << 0)

#define I2C_IS_IDLE     (0 << 0)
#define I2C_NOT_IDLE    (1 << 0)

#define I2C_WRITE       (0 << 1)
#define I2C_READ        (1 << 1)

#define I2C_RELEASE     (0 << 2)
#define I2C_HOLD        (1 << 2)

#define I2C_64          (0 << 3)
#define I2C_128         (1 << 3)

#define I2C_XFER_DONE   (0 << 4)
#define I2C_XFER_BUSY   (1 << 4)

#define I2C_ACK         (0 << 5)
#define I2C_NACK        (1 << 5)

#define I2C_OK          (0 << 7)
#define I2C_ERR         (1 << 7)

/*
 * Bridge constants
 */
#define GIOBR_DEV_CONFIG(flgs, dev)     ( (flgs) | (dev) )
#define LSBIT(word)             ((word) &~ ((word)-1))


/*
 *	XXX Addresses of supported I2C devices for EVO. Needs updating.
 */

#define BUS_CONV_ADDR   0x40

/* 
 *Break down all bits in all registers in all chips, on the EVO 
 *Video card.
 */


/* VOC registers */
#define	BT_VOC_NTSC			1
#define	BT_VOC_D1_ENABLE		2
#define	BT_VOC_B_ENABLE			3
#define	BT_VOC_B_BACKOUT		4
#define	BT_VOC_C_ENABLE			5
#define	BT_VOC_C_BACKOUT		6
#define	BT_VOC_A_ENABLE			7
#define	BT_VOC_A_FULL_SCREEN		8
#define	BT_VOC_A_DIRECT			9
#define	BT_VOC_A_DIRECTION		10
#define	BT_VOC_OVERLAP			11
#define	BT_VOC_EXPRESS_MODE		12
#define	BT_VOC_ENABLE_INT		13
#define	BT_VOC_INT_IN_LINE_HI		14
#define	BT_VOC_INT_IN_LINE_LO		15
#define	BT_VOC_DBL_BUF_EN		16
#define	BT_VOC_PIXEL_BLANK_ENABLE	17
#define	BT_VOC_HIGH_WATER_MARK		18
#define	BT_VOC_FIFO_FIELD_RESET		19

/* Misc registers */
#define BT_MISC_PIXEL_MODE		20
#define BT_MISC_TIMING_MODE		21
#define BT_MISC_AUTO_PHASE		22
#define BT_MISC_CH1_IN_STD		23
#define BT_MISC_CH1_IN_PRESENT		24
#define BT_MISC_CH1_IN_AUTO		25
#define BT_MISC_CH1_IN_2HREF		26
#define BT_MISC_CH2_IN_STD		27
#define BT_MISC_CH2_IN_PRESENT		28
#define BT_MISC_CH2_IN_AUTO		29
#define BT_MISC_CH2_IN_2HREF		30
#define BT_MISC_INPUT_ALPHA		31
#define BT_MISC_CH1_IN_ROUND		32
#define BT_MISC_CH2_IN_ROUND		33
#define BT_MISC_CH12_IN_DITHER		34
#define BT_MISC_OUTPUT_ALPHA		35
#define BT_MISC_CH1_OUT_VB		36
#define BT_MISC_CH2_OUT_VB		37
#define BT_MISC_CH2_CC1_CHROMA		38
#define BT_MISC_CH1_OUT_HPHASE_LO	39
#define BT_MISC_CH1_OUT_HPHASE_HI	40
#define BT_MISC_CH2_OUT_HPHASE_LO	41
#define BT_MISC_CH2_OUT_HPHASE_HI	42
#define BT_MISC_CH1_FRAME_SYNC		43
#define BT_MISC_CH1_OUT_BLANK		44
#define BT_MISC_CH1_OUT_FREEZE		45
#define BT_MISC_CH2_FRAME_SYNC		46
#define BT_MISC_CH2_OUT_BLANK		47
#define BT_MISC_CH2_OUT_FREEZE		48
#define BT_MISC_VCXO_LO			49
#define BT_MISC_VCXO_HI			50
#define BT_MISC_GENLOCK_REF		51
#define BT_MISC_REF_LOCK		52
#define BT_MISC_INOUT_1			53
#define BT_MISC_INOUT_2			54
#define BT_MISC_GFXA_TOP_BLANK		55
#define BT_MISC_GFXB_TOP_BLANK		56
#define BT_MISC_VCXO_DAC_BUSY		57
#define BT_MISC_VCXO_ADC_BUSY		58
#define BT_MISC_REF_OFFSET_LO		59
#define BT_MISC_REF_OFFSET_HI		60
#define BT_MISC_VCXO_ADC_MODE		61
#define BT_MISC_VCXO_ADC_HI		62
#define BT_MISC_VCXO_ADC_LO		63
#define BT_MISC_FIRMWARE_REVISION	64
#define BT_MISC_CH1_IN_8BIT		65
#define BT_MISC_CH2_IN_8BIT		66
#define BT_MISC_PIXEL_TIMING		67
#define BT_MISC_CH1_OUT_CHROMA		68
#define BT_MISC_CH2_OUT_CHROMA		69
#define BT_MISC_VGI1_SW_RESET		70
#define BT_MISC_GFX_FRAME_RESET		71

/* VBAR 1 registers */
#define BT_VBAR1_UNUSED1		72
#define BT_VBAR1_UNUSED2		73
#define BT_VBAR1_CSC_A			74
#define BT_VBAR1_CSC_B			75
#define BT_VBAR1_CH1_GIO		76
#define BT_VBAR1_CH2_GIO		77
#define BT_VBAR1_UNUSED3		78
#define BT_VBAR1_UNUSED4		79
#define BT_VBAR1_SCALER_IN		80
#define	BT_VBAR1_TO_VBAR2_CH1		81
#define	BT_VBAR1_TO_VBAR2_CH2		82
#define BT_VBAR1_UNUSED5		83
#define BT_VBAR1_UNUSED6		84
#define BT_VBAR1_UNUSED7		85
#define BT_VBAR1_UNUSED8		86
#define BT_VBAR1_UNUSED9		87

/* VBAR 2 registers */
#define BT_VBAR2_UNUSED1		88
#define BT_VBAR2_UNUSED2		89
#define BT_VBAR2_CSC_A			90
#define BT_VBAR2_CSC_B			91
#define BT_VBAR2_CH1_GIO		92
#define BT_VBAR2_CH2_GIO		93
#define BT_VBAR2_VOUT_CH1		94
#define BT_VBAR2_VOUT_CH2		95
#define BT_VBAR2_SCALER_IN		96
#define	BT_VBAR2_TO_VBAR1_CH1		97
#define	BT_VBAR2_TO_VBAR1_CH2		98
#define BT_VBAR2_UNUSED3		99
#define BT_VBAR2_UNUSED4		100
#define BT_VBAR2_UNUSED5		101	
#define BT_VBAR2_UNUSED6		102
#define BT_VBAR2_UNUSED7		103

/* 	CSC #1 Indirect Regs */
#define BT_CSC1_YUV422_IN               104
#define BT_CSC1_FILTER_IN               105
#define BT_CSC1_TRC_IN                  106
#define BT_CSC1_YUV422_OUT              107
#define BT_CSC1_FILTER_OUT              108
#define BT_CSC1_RGB_BLANK               109
#define BT_CSC1_FULLSCALE               110
#define BT_CSC1_MAX_ALPHA_OUT           111
#define BT_CSC1_ALPHA_OUT               112
#define BT_CSC1_BLACK_OUT               113
#define BT_CSC1_OFFSET_LS 		114
#define BT_CSC1_OFFSET_MS		115

/* 	CSC #2 Indirect Regs */
#define BT_CSC2_YUV422_IN               116
#define BT_CSC2_FILTER_IN               117
#define BT_CSC2_TRC_IN                  118
#define BT_CSC2_YUV422_OUT              119
#define BT_CSC2_FILTER_OUT              120
#define BT_CSC2_RGB_BLANK               121
#define BT_CSC2_FULLSCALE               122
#define BT_CSC2_MAX_ALPHA_OUT           123
#define BT_CSC2_ALPHA_OUT               124
#define BT_CSC2_BLACK_OUT               125
#define BT_CSC2_OFFSET_LS 		126
#define BT_CSC2_OFFSET_MS		127

/* 	CSC #3 Indirect Regs */
#define BT_CSC3_YUV422_IN               128
#define BT_CSC3_FILTER_IN               129
#define BT_CSC3_TRC_IN                  130
#define BT_CSC3_YUV422_OUT              131
#define BT_CSC3_FILTER_OUT              132
#define BT_CSC3_RGB_BLANK               133
#define BT_CSC3_FULLSCALE               134
#define BT_CSC3_MAX_ALPHA_OUT           135
#define BT_CSC3_ALPHA_OUT               136
#define BT_CSC3_BLACK_OUT               137
#define BT_CSC3_OFFSET_LS 		138
#define BT_CSC3_OFFSET_MS		139


/*
 * Remember after changing the above define values or
 * ranges to adjust the following two table boundary
 * ranges. Which is referenced in evo_singlebit().
 */
#define BT_BEGIN		BT_VOC_NTSC
#define BT_END			BT_CSC3_OFFSET_MS

#define	BT_VOC_MIN		BT_VOC_NTSC
#define	BT_VOC_MAX		BT_VOC_FIFO_FIELD_RESET

#define BT_MISC_MIN		BT_MISC_PIXEL_MODE
#define BT_MISC_MAX		BT_MISC_GFX_FRAME_RESET

#define BT_VBAR1_MIN		BT_VBAR1_UNUSED1
#define BT_VBAR1_MAX		BT_VBAR1_UNUSED9

#define BT_VBAR2_MIN		BT_VBAR2_UNUSED1
#define BT_VBAR2_MAX		BT_VBAR2_UNUSED7

#define BT_CSC1_INDIRECT_MIN	BT_CSC1_YUV422_IN
#define BT_CSC1_INDIRECT_MAX	BT_CSC1_OFFSET_MS

#define BT_CSC2_INDIRECT_MIN	BT_CSC2_YUV422_IN
#define BT_CSC2_INDIRECT_MAX	BT_CSC2_OFFSET_MS

#define BT_CSC3_INDIRECT_MIN	BT_CSC3_YUV422_IN
#define BT_CSC3_INDIRECT_MAX	BT_CSC3_OFFSET_MS

/*
 * WARNING: above "BT_XXX" defines are direct indexed into exp_bits.
 */

struct exp_bits_t {
	int name;		/* See above */
	int reg;		/* Subaddress */
	unsigned char mask;	/* Value to use as mask, ie 2^num_of_bits */
	unsigned char shift;	/* How many bits to shift over */
};



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


#define L_CH01                 0
#define L_CH10                 1
#define L_CH0                  2
#define L_CH1                  3


/*
 * XXX ERROR CODES  ***probably need updating***
 * XXX These constants should be in sync with evo_err[] defined in evo_globals.c
 */
/* Board Specific ASIC independent Tests 0 - 4 */
#define	EVO_INIT_TEST				  0
#define	EVO_GPI_TRIGGER_TEST			  1

/* VGI1 TESTS 10 - 19 */
#define	VGI1_REG_PATTERN_TEST			 10
#define	VGI1_REG_WALK_BIT_TEST		 	 11
#define	VGI1_INTERRUPT_TEST		 	 12
#define	VGI1_DMA_TEST				 13

/* CSC specific error codes 20 - 29 */
#define CSC_ADDR_BUS_TEST                       20
#define CSC_DATA_BUS_TEST                       21
#define CSC_INPUT_LUT_TEST                      22
#define CSC_OUTPUT_LUT_TEST                     23
#define CSC_COEF_TEST                           24
#define CSC_X2K_TEST                            25

/* SCALER specific error codes 30 - 39 */
#define SCL_ADDR_TEST                       	30
#define SCL_DATA_TEST                       	31

/* VOC specific error codes 40 - 49 */
#define VOC_ADDR_TEST                       	40
#define VOC_DATA_TEST                       	41

/* FMBUF specific error codes 50 - 59 */
#define FMBUF_ADDR_TEST                         50
#define FMBUF_DATA_TEST                         51

/* TYPE DEFINITIONS AND PROTOTYPES */
typedef	struct _evo_info {
        int                  swrev;        /* driver/daemon interface version */
        int                  bdrev;        /* EVO board rev. */
        int                  BRGrev;       /*  Bridge rev.  */
        int                  VGIrev;       /* VGI1 rev.  */
        int                  VOCrev;        /* VOC rev.  */
        int                  CSCrev;       /* CSC rev.  */
        int                  SCLrev;        /* Scaler rev.  */
        int                  board_type;   /* see above */
        evo_vgi1_gio_id_t    vgi1_gio_id;  /* VGI1 info */
} evo_info_t;

typedef struct _evo_vgi1_info {
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

	evo_dma_unit_t dma_unit;		/* Transfer unit */
	evo_vgi1_ch_setup_t ch_setup;	/* Channel Setup Register defs */
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
} evo_vgi1_info_t;

typedef struct  _evo_csc_sel_ctrl { /* CSC indirect addr register 1 */
        uchar_t sel_output_lut : 2; /* select output LUT for Read Back mode */
        uchar_t sel_lut_addr   : 3; /* select LUT for address application */
        uchar_t auto_inc_normal: 1; /* 1 reset auto inc to 0; 0 normal mode */
        uchar_t luts_mode      : 2; /* 0 normal; 1 load coef; 2 load LUT; 3 read LUT */

} evo_csc_sel_ctrl_t ;

typedef struct  _evo_csc_op_ctrl { /* CSC indirect addr register 1 */
    uchar_t sel_ofmt             : 1; /* 1 = YUV422/4, 0 = Other */
    uchar_t sel_odif             : 1; /* 1 = Decimation, 0 = Passthrough */
    uchar_t sel_op_black_lvl     : 1; /* sets appropriate black level 1 = RGB, 0 = YUV */
    uchar_t sel_op_full_scl      : 1; /* 1 = Full Scale, 0 = CCIR */
    uchar_t sel_a_kin            : 1; /* 0 = Max X, 1 = Alpha for KLUT input*/
    uchar_t sel_a_kop            : 1; /* 0 = Normal Alpha, 1 = Max X for Alpha LUT input */
    uchar_t op_frc_black         : 1; /* 1 = Force Op to Black Immediately 0 = Normal Op Imm. */
    uchar_t reserved             : 1; /* not used*/
} evo_csc_op_ctrl_t ;


/*XXX this structure will need updating for the EVO board*/
typedef struct	_evodiag {
	void		*vgi1_1_base;	/* base addr of VGI1 ASIC 1 */
	void		*vgi1_2_base;	/* base addr of VGI1 ASIC 2 */
	void		*vgibase;	/* base addr of selected VGI*/
	void		*gio_pio_base;	/* base addr of gio-pio interface */
	void		*brbase;	/* base addr of BRIDGE ASIC */
	__paddr_t	voc;		/* Address to VOC */
	__paddr_t	scl1;		/* Address to Scaler 1 */
	__paddr_t	scl2;		/* Address to Scaler 2 */
	__paddr_t	scl;		/* Address to selected Scaler*/
	__paddr_t	evo_misc;	/* Address to EVO Misc. chips */
	__paddr_t	fpga_dwl;	/* Address to FPGA being downloaded */
	__paddr_t	csc1;		/* Address to CSC chips */
	__paddr_t	csc2;		/* Address to CSC chips */
	__paddr_t	csc3;		/* Address to CSC chips */
	__paddr_t	dcb_crs;	/* Address to DCB-CRS register */
	__paddr_t	csc;		/* Address to selected CSC chip */
	__paddr_t	vbar1;		/* Address to VBAR1 chips */
	__paddr_t	vbar2;		/* Address to VBAR2 chips */
	__paddr_t	vbar;		/* Address to selected VBAR chip */
	__paddr_t	enc;		/* Address to encoder chips */
	__paddr_t	dec;		/* Address to decoder chips */
	__paddr_t	vgi1addr;	/* Address to VGI1 PIO registers */
	uchar_t		videoMode;	/* video timing NTSC/PAL */
	uchar_t		pixelMode;	/* current pixel mode SQR/MHZ13.5 */
	evo_info_t	info;		/* Board info */
	evo_vgi1_info_t	vgi1[EVO_VGI1_NUM_CHAN];
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
  	u_int16_t	*lutbuf;	/* pointer to CSC LUT buffer*/	
  	u_int16_t	*coefbuf;	/* pointer to CSC coefficient buffer*/	
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
} evodiag;


typedef struct _evo_test_info {
	char	*TestStr;
	char	*ErrStr;
} evo_test_info;

typedef	struct _evo_ind_regs {
	__uint32_t	reg;
	__uint32_t	val;
} evo_ind_regs;

typedef struct _evo_vgi1_rwregs {
	char		*str;
	__uint32_t	offset;
	ulonglong_t	mask;
} evo_vgi1_rwregs;

typedef struct _evo_csc_dcbregs {
	char		*str;
	__uint32_t	offset;
	ulonglong_t	mask;
} evo_csc_dcbregs;

typedef struct _evo_csc_indregs {
	char		*str;
	__uint32_t	offset;
	ulonglong_t	mask;
} evo_csc_indregs;

typedef struct  _voc_Regs {
        char            *name;          /* name of the register */
        __uint32_t 	address;        /* register offset */
        __uint32_t      mask;           /* read-write mask */
        int             mode;           /* read / write only or read & write */
} voc_Regs;

typedef struct _exp_bits_t {
	__uint32_t name;	/* See above */
	__uint32_t reg;		/* Subaddress */
	unsigned char mask;	/* Value to use as mask, ie 2^num_of_bits */
	unsigned char shift;	/* How many bits to shift over */
} evo_exp_bits;

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


/* EVO */

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
extern 	evodiag		*evobase;
extern	__uint32_t 	evo_walk[24];
extern	__uint32_t 	evo_walk_one[16];
extern	__uint32_t 	evo_patrn[6];
extern	__uint32_t 	evo_patrn32[6];
extern	uchar_t 	evo_patrn8[5];
extern	uchar_t 	csc_bypass_patrn[16]; /*updated from mgv specs*/
extern	_mg_xbow_portinfo_t	mg_xbow_portinfo_t[6];
extern	evo_test_info	evo_err[];
extern	evo_vgi1_ch_trig_t	evo_abort_dma;
extern	evo_vgi1_ch_trig_t	evo_trig_on;
extern	evo_vgi1_ch_trig_t	evo_trig_off;
extern	unsigned char   *evo_pVGIDMADesc, evo_VGIDMADesc[4096 * 2];
extern	evo_exp_bits 	evo_bit_desc[];
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
extern	evo_vgi1_h_param_t	hpNTSC, hpNTSCSQ, hpPAL, hpPALSQ;
extern	evo_vgi1_v_param_t	vpNTSC, vpNTSCSQ, vpPAL, vpPALSQ;

/* CSC Specific values */
extern int csc_inlut, csc_alphalut, csc_coef, csc_x2k, csc_outlut, csc_inlut_value, csc_tolerance;
extern int csc_clipping, csc_constant_hue, csc_each_read ;

/*** Bridge structure , initialized in mgv_globals.c ***/
extern struct giobr_bridge_config_s srv_bridge_config;


/* EXTERNAL FUNCTION DECLARATION */
extern void mgras_probe_all_widgets(void);
extern	__uint32_t	_evo_reportPassOrFail(evo_test_info *, __uint32_t);
extern	__uint32_t      _evo_VGI1PollReg(__uint32_t reg, ulonglong_t val, 
	ulonglong_t mask, __uint32_t timeOut, ulonglong_t *regVal);
#define evo_vgi1_load_ll(A)     *(volatile ulonglong_t *)(A)
#define evo_vgi1_store_ll(D,A)  *(volatile ulonglong_t *)(A) = (D);
extern	__uint32_t	_evo_VGI1DMA(void);
extern	__uint32_t      _evo_reset(__uint32_t device_num);
extern	uchar_t* _evo_PageAlign(uchar_t *pBuf);
extern void _evo_csc_ReadLut(__uint32_t, __uint32_t, u_int16_t *);
extern void _evo_csc_LoadLut(__uint32_t, __uint32_t, u_int16_t *);
extern void _evo_csc_LoadBuf(__uint32_t, u_int16_t *, __uint32_t, __uint32_t);
extern __uint32_t _evo_csc_VerifyBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t*);
extern void _evo_csc_setupDefault(void);
extern __uint32_t _evo_csc_tmi_probe(void);
extern void    msg_printf(int, char *, ...);
extern int pause(int, char *, char *);
extern __uint32_t _evo_singlebit(__uint32_t, __uint32_t, __uint32_t);
extern int sprintf(char *buf, const char *fmt, ...);
extern __uint32_t  _evo_outb(__paddr_t, unsigned char);
extern __uint32_t  _evo_outhw(__paddr_t, unsigned short);
extern __uint32_t        _evo_outw(__paddr_t, __uint32_t);
extern void busy(int);
extern __uint32_t        _evo_inw(__paddr_t addr);
extern unsigned short _evo_inhw(__paddr_t);
extern uchar_t _evo_inb(__paddr_t);
extern __uint32_t  _evo_VGI1DMAGetBufSize(void);
extern __uint32_t  _evo_VGI1DMADispErr(void);
extern void  _evo_VGI1InitDMAParam(void);
extern __uint32_t  _evo_VGI1StartAndProcessDMA(void);
extern	__uint32_t evo_VGI1DMATest (__uint32_t , __uint32_t , \
  __uint32_t , __uint32_t , __uint32_t ,  __uint32_t , \
  ulonglong_t , __uint32_t , __uint32_t , __uint32_t , __uint32_t  , \
  ulonglong_t, __uint32_t , __uint32_t ); 
extern	__uint32_t  evo_cscTest (__uint32_t , __uint32_t , \
	__uint32_t , __uint32_t , __uint32_t ,  __uint32_t,  \
	__uint32_t , __uint32_t );
extern void flush_cache(void);
extern int test_evo_rw1(void);
extern void _evo_delay_dots( unsigned char, unsigned char);
extern int _evo_check_vin_status(unsigned char, unsigned char);
extern void _evo_setup_VGI1DMA(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char*, unsigned char, unsigned int);


extern void _evo_make_vid_array( unsigned int *, unsigned char, unsigned int, unsigned int, unsigned int, unsigned int, int, int, int, int);

extern void test_allstds(unsigned long *, int, unsigned char, int, int);
extern int _evo_compare_byte_buffers(long, long, unsigned char *, unsigned char *, unsigned char, unsigned char);

extern int _evo_I2C_wait_for_ack(void);
extern int _evo_I2C_wait_for_xfer(void);
extern int _evo_I2C_wait_for_long_xfer(void);
extern void _evo_I2C_force_idle(void);
extern int _evo_I2C_wait_for_idle(void);
extern uchar_t _evo_I2C_seq_retry(int addr, int subaddr, int reg, int val, \
	int rw, int vlut, int datawrite);
extern void mgras_probe_all_widgets();
extern __uint32_t  _evo_probe_bridge(void);
extern __uint32_t  _evo_initBridge(giobr_bridge_config_t);
extern __uint32_t  evo_br_rrb_alloc(gioio_slot_t, int);


/*
 * Forward Function References
 */

__uint32_t      evo_VGI1IntTest (__uint32_t argc, char **argv);


/* MACROS */


#define	EVO_SET_CSC_1_BASE {						\
	msg_printf(DBG, "CSC Base is now CSC 1\n");			\
	evobase->csc = evobase->csc1;					\
}

#define	EVO_SET_CSC_2_BASE {						\
	msg_printf(DBG, "CSC Base is now CSC 2\n");			\
	evobase->csc = evobase->csc2;					\
}

#define	EVO_SET_CSC_3_BASE {						\
	msg_printf(DBG, "CSC Base is now CSC 3\n");			\
	evobase->csc = evobase->csc3;					\
}

/*CSC Direct register writes*/
#define CSC_W1(evobase, reg, _val) {					\
  _evo_outb(evobase->dcb_crs, reg);\
  _evo_outb(evobase->csc, _val);\
  msg_printf(DBG, "evobase->dcb_crs 0x%x; evobase->csc 0x%x; val 0x%x\n", evobase->dcb_crs, evobase->csc, _val);	\
}


#define CSC_IND_W1(evobase, reg, _val) {					\
  	msg_printf(DBG, "At CSC_IND_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
	CSC_W1(evobase,CSC_ADDR,reg);	\
	CSC_W1(evobase,CSC_DATA,_val);	\
}

/*CSC Direct register reads*/
#define	CSC_R1(evobase, reg, _val) {					\
  _evo_outb(evobase->dcb_crs, reg);\
  _val=_evo_inb(evobase->csc);	\
  msg_printf(DBG, "evobase->dcb_crs 0x%x; evobase->csc 0x%x; val 0x%x\n", evobase->dcb_crs, evobase->csc, _val);	\
}

#define CSC_IND_R1(evobase, reg, _val) {					\
	CSC_W1(evobase,CSC_ADDR,reg);	\
	CSC_R1(evobase,CSC_DATA,_val);	\
  	msg_printf(DBG, "After CSC_IND_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}


/*Begin Scaler Macros*/

#define	EVO_SET_SCL1_BASE {						\
	msg_printf(DBG, "SCL Base is now SCL 1\n");			\
	evobase->scl = evobase->scl1;					\
}

#define	EVO_SET_SCL2_BASE {						\
	msg_printf(DBG, "SCL Base is now SCL 2\n");			\
	evobase->scl = evobase->scl2;					\
}

#define EVO_SCL_R1(evobase, reg, _val) {				\
	_evo_outb(evobase->scl, reg);					\
	_val = _evo_inb(evobase->scl+(__paddr_t)0x10);			\
}

#define EVO_SCL_W1(evobase, reg, _val) {				\
	_evo_outb(evobase->scl, reg);					\
	_evo_outb(evobase->scl+(__paddr_t)0x10, _val);			\
}

/*Begin VBAR Macros*/

#define	EVO_VBAR_W1(evobase, reg, _val) {				\
  msg_printf(DBG, "EVO_VBAR_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
  _evo_outb((evobase->vbar+(__paddr_t)reg), _val);		\
}

#define	EVO_VBAR_R1(evobase, reg, _val) {				\
  _val=_evo_inb((evobase->vbar+(__paddr_t)reg));	\
  msg_printf(DBG, "EVO_VBAR_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

#define	EVO_SET_VBAR1_BASE {						\
	msg_printf(DBG, "VBAR Base is now VBAR 1\n");     \
	evobase->vbar = evobase->vbar1;			\
}

#define	EVO_SET_VBAR2_BASE {						\
	msg_printf(DBG, "VBAR Base is now VBAR 2\n");     \
	evobase->vbar = evobase->vbar2;			\
}


/*End VBAR Macros*/

#define EVO_VOC_R1(evobase, reg, _val) {				\
	_val = _evo_inw(evobase->voc+(__paddr_t)(8*reg));			\
  	msg_printf(DBG, "EVO_VOC_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

#define EVO_VOC_W1(evobase, reg, _val) {				\
  	msg_printf(DBG, "EVO_VOC_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
	_evo_outw(evobase->voc+(__paddr_t)(8*reg), _val);			\
}

/*End VOC Macros*/

/*XXX No EVO frame buffer read macros ...because data is available only through VBAR */ 
/*XXX Framestore data register is 2 8-bit pixels (16 bits) wide. Control information */
/*XXX is 1 byte wide. The frame store itself is an 8 bit wide X 3 (RGB) deep FIFO*/ 
#define EVO_FMB_W1(evobase, reg, _val) {				\
  	msg_printf(DBG, "EVO_FMB_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
	_evo_outhw(evobase->evo_misc+(__paddr_t)reg, _val);			\
  	msg_printf(DBG, "After writing data (16 bits, low pix bits 0-7) into the pixel register\n"); \
	_evo_outb(evobase->evo_misc+(__paddr_t)reg+(__paddr_t)0x100, 0x40);	\
  	msg_printf(DBG, "After selecting clock for software load\n"); \
	_evo_outb(evobase->evo_misc+(__paddr_t)reg+(__paddr_t)0x100, 0x78);	\
  	msg_printf(DBG, "After strobing clocks high\n"); \
	_evo_outb(evobase->evo_misc+(__paddr_t)reg+(__paddr_t)0x100, 0x40);	\
  	msg_printf(DBG, "After strobing clocks low, data should now have been clocked in\n"); \
}

/*End Frame Buffer Macros*/

/*XXX Writes to EVO Misc. Registers...these may not be directly accessible
If so these macros should be replaced with EVO_IND_W1 and EVO_IND_R1
along the lines of CSC Macros */

#define	EVO_W1(evobase, reg, _val) {				\
  msg_printf(DBG, "EVO_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
  _evo_outb((evobase->evo_misc+(__paddr_t)reg), _val);		\
}

#define	EVO_R1(evobase, reg, _val) {				\
  _val=_evo_inb((evobase->evo_misc+(__paddr_t)reg));	\
  msg_printf(DBG, "EVO_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}


/*EVO MACROS to write to GIO-PIO registers*/ 

#define	EVO_PIO_W1(evobase, reg, _val) {				\
  msg_printf(DBG, "EVO_PIO_W1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
  _evo_outhw(((__paddr_t)evobase->gio_pio_base+(__paddr_t)reg), _val);		\
}

#define	EVO_PIO_R1(evobase, reg, _val) {				\
  _val=_evo_inhw(((__paddr_t)evobase->gio_pio_base+(__paddr_t)reg));	\
  msg_printf(DBG, "EVO_PIO_R1 ::: reg 0x%x; val 0x%x\n", reg, _val); \
}

/*End EVO_PIO Macros*/


#define	EVO_SET_VGI1_1_BASE {						\
	msg_printf(DBG, "VGI Base is now VGI 1\n");			\
	evobase->vgibase = evobase->vgi1_1_base;			\
}

#define	EVO_SET_VGI1_2_BASE {						\
	msg_printf(DBG, "VGI Base is now VGI 2\n");			\
	evobase->vgibase = evobase->vgi1_2_base;			\
}


#define	VGI1_W32(evobase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_W32...\n");				\
  msg_printf(DBG, "evobase->vgibase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	evobase->vgibase, reg, val);						\
  *(__uint32_t *)((__paddr_t) evobase->vgibase + reg) = val & mask;	\
}

#define	VGI1_R32(evobase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_R32...\n");				\
  val = *(__uint32_t *)((__paddr_t) evobase->vgibase + reg) & mask;	\
  msg_printf(DBG, "evobase->vgibase = 0x%x; reg = 0x%x; value = 0x%x;\n",	\
	evobase->vgibase, reg, val);						\
}

#define	VGI1_W64(evobase, reg, val, mask) {				\
  msg_printf(DBG, "before VGI1_W64...\n");				\
  msg_printf(DBG,"evobase->vgibase = 0x%x; reg = 0x%x; value = 0x%llx;\n",	\
	evobase->vgibase, reg, ((ulonglong_t)val));						\
  *(__uint64_t *)((__paddr_t) evobase->vgibase + reg) = val & mask;	\
  msg_printf(DBG, "after VGI1_W64...\n");				\
}

#define	VGI1_R64(evobase, reg, val, mask) {				\
  ulonglong_t __val;							\
  msg_printf(DBG, "before VGI1_R64...\n");				\
  __val = *(__uint64_t *)((__paddr_t) evobase->vgibase + reg) & mask;	\
  val = __val & mask;							\
  msg_printf(DBG,"evobase->vgibase = 0x%x; reg = 0x%x; value = 0x%llx;\n",	\
	evobase->vgibase, reg, __val);						\
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
  	EVO_W1(evobase, low_reg, getLowByte(0x800)); \
	EVO_W1(evobase, hi_reg, getHiByte(0x800));

#define setDefaultFreeRun \
	EVO_W1(evobase, GAL_VCXO_LO, getLowByte(0x800)); \
	EVO_W1(evobase, GAL_VCXO_HI, getHiByte(0x800));

#if 0
/* Check this XXX */

#define setDefaultBlackTiming \
	EVO_W1(evobase, GAL_FLORA_LO, getLowByte(0x200)); \
	EVO_W1(evobase, GAL_FLORA_HI, getHiByte(0x200));

#endif
#define setDefaultBlackTiming \
        EVO_W1(evobase, MISC_REF_OFFSET_LO, getLowByte(0x200)); \
        EVO_W1(evobase, MISC_REF_OFFSET_HI, getHiByte(0x200));


/*
 * Read high or low bytes
 */
#define readHiByte(val) \
	(val & 0xf)

#define readLowByte(val) \
	(val & 0xff)

#endif /* _EVO_DIAG_H_ */
