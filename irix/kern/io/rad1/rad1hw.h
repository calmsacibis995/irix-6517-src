/* rad1hw.h */

/****
 *
 *	Copyright 1998 - PsiTech Inc.
 * THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION OF PSITECH.
 * Its receipt or possession does not create any right to reproduce,
 * disclose its contents, or to manufacture, use or sell anything it
 * may describe.  Reproduction, disclosure, or use without specific
 * written authorization of PsiTech is strictly prohibited.
 *
 ****
 *
 * Revision history:
 *	3/16/98	PAG	more created
 *
 ****/

#ifndef _RAD1HW_DEFS
#define _RAD1HW_DEFS

#define RAD1_2V_VENDOR_ID	0x3D3D
#define RAD1_2V_DEVICE_ID	0x0009
#define RAD1_VENDOR_ID	0x104C
#define RAD1_DEVICE_ID	0x3D07

#define RAD1_CFG_SIZE	256
#define RAD1_REGION_0_SIZE	0x20000
#define RAD1_REGION_1_SIZE	0x800000
#define RAD1_REGION_2_SIZE	0x800000
#define RAD1_EPROM_SIZE	0x10000
#define RAD1_SGRAM_SIZE	 RAD1_REGION_1_SIZE


/************************************************
*
* 	Region 0 Register Offsets
*
* NOTE: The offsets defined here map the registers
*       into the big-endian address region
*
 ************************************************/

/* Control Status Registers */
#define RESETSTATUS_REG		0x00000000
#define SOFT_RESET_FLAG	(1 << 31)

#define INTENABLE_REG		0x00000008
typedef union {
	struct {
		unsigned int rsvd2 : 19;
		unsigned int video_strm_ext_int_en : 1;
		unsigned int video_ddc_int_en : 1;
		unsigned int video_strm_ser_int_en : 1;
		unsigned int video_strm_a_int_en : 1;
		unsigned int video_strm_b_int_en : 1;
		unsigned int bypass_dma_int_en : 1;
		unsigned int texture_inv_int_en : 1;
		unsigned int scanline_int_en : 1;
		unsigned int vert_retrace_int_en : 1;
		unsigned int error_int_en : 1;
		unsigned int rsvd : 1;
		unsigned int sync_int_en : 1;
		unsigned int dma_int_en : 1;
	} bits;
	int all;
} p2_intenable_reg;

#define INTFLAGS_REG		0x00000010
#define DMA_INT_FLAG			(1 << 0)
#define SYNC_INT_FLAG			(1 << 1)
#define ERROR_INT_FLAG			(1 << 3)
#define VERT_RETRACE_INT_FLAG	(1 << 4)
#define SCANLINE_INT_FLAG		(1 << 5)
#define TEXTURE_INV_INT_FLAG	(1 << 6)
#define BYPASS_DMA_INT_FLAG		(1 << 7)
#define VIDEO_STRM_B_INT_FLAG	(1 << 8)
#define VIDEO_STRM_A_INT_FLAG	(1 << 9)
#define VIDEO_STRM_SER_INT_FLAG	(1 << 10)
#define VIDEO_DDC_INT_FLAG		(1 << 11)
#define VIDEO_STRM_EXT_INT_FLAG	(1 << 12)
#define SVGA_INT_FLAG			(1 << 31)

#define INFIFOSPACE_REG		0x00000018

#define OUTFIFOWORDS_REG	0x00000020

#define INDMAADDRESS_REG	0x00000028

#define INDMACOUNT_REG		0x00000030

#define ERRORFLAGS_REG		0x00000038
#define IN_FIFO_ERR_FLAG			(1 << 0)
#define OUT_FIFO_ERR_FLAG			(1 << 1)
#define MSG_ERR_FLAG				(1 << 2)
#define DMA_ERR_FLAG				(1 << 3)
#define VIDEO_FIFO_UFLO_ERR_FLAG	(1 << 4)
#define VIDEO_STRM_B_FIFO_UFLO_FLAG	(1 << 5)
#define VIDEO_STRM_A_FIFO_UFLO_FLAG	(1 << 6)
#define MASTER_ERR_FLAG				(1 << 7)
#define OUT_DMA_ERR_FLAG			(1 << 8)
#define IN_DMA_OWRITE_ERR_FLAG		(1 << 9)
#define OUT_DMA_OWRITE_ERR_FLAG		(1 << 10)
#define VIDEO_STRM_A_INV_ILACE_FLAG	(1 << 11)
#define VIDEO_STRM_B_INV_ILACE_FLAG	(1 << 12)

#define VCLKCTL_REG			0x00000040
typedef union {
	struct {
		unsigned int rsvd : 22;
		unsigned int recoveryTime : 8;
		unsigned int vClkCtl : 2;
	} bits;
	int all;
} p2_vclkctl_reg;

#define APERTUREONE_REG			0x00000050
#define APERTURETWO_REG			0x00000058
typedef union {
	struct {
		unsigned int rsvd2 : 22;
		unsigned int ROMAccess : 1;
		unsigned int SVGAAccess : 1;
		unsigned int packed16bitReadMode : 1;
		unsigned int packed16bitWriteMode : 1;
		unsigned int packed16bitWriteBufferSelect : 1;
/* see packed16bitReadBufferSelect */
		unsigned int packed16bitReadBufferSelect : 1;
#define PACKED16BITBUFFERSEL_A		0
#define PACKED16BITBUFFERSEL_B		1
		unsigned int packed16bitMemoryEnable : 1;
		unsigned int rsvd : 1;
		unsigned int memoryByteControl : 2;
#define MEMORYBYTECONTROL_STD		0
#define MEMORYBYTECONTROL_SWAP		1
#define MEMORYBYTECONTROL_HW_SWAP	2
	} bits;
	int all;
} p2_aperture_reg;

#define DMACONTROL_REG			0x00000060
typedef union {
	struct {
		unsigned int rsvd : 23;
		unsigned int textureExecuteByteSwap : 2;
#define TEXTUREEXECUTE_BYTE_SWAP_STD	0
#define TEXTUREEXECUTE_RD_BIT31			1
#define TEXTUREEXECUTE_HW_SWAP			2
		unsigned int AGPHighPriority : 1;
		unsigned int AGPDataThrottle : 1;
		unsigned int outDMAByteSwap : 1;
/* see inDMAByteSwap */
		unsigned int longReadDisable : 1;
		unsigned int inDMADataThrottle : 1;
		unsigned int inDMAUsingAGP : 1;
		unsigned int inDMAByteSwap : 1;
#define DMA_LITTLEENDIAN	0
#define DMA_BIGENDIAN	1
	} bits;
	int all;
} p2_dmacontrol_reg;

#define FIFODISCON_REG			0x00000068
typedef union {
	struct {
		unsigned int graphicsProcessorActive : 1;
		unsigned int rsvd : 29;
		unsigned int outputFIFODisconnectEnable : 1;
		unsigned int inputFIFODisconnectEnable : 1;
	} bits;
	int all;
} p2_fifodiscon_reg;
#define GRAPH_PROC_ACTIVE_BIT	(1 << 31)

#define CHIPCONFIG_REG			0x00000070
typedef union {
	struct {
		unsigned int rsvd2 : 23;
		unsigned int subSystemFromROM : 1;
		unsigned int SClkSel : 1;
#define SCLKSEL_PCLK		0
#define SCLKSEL_HALF_PCLK	1
#define SCLKSEL_MCLK		2
#define SCLKSEL_HALF_MCLK	3
		unsigned int AGPCapable : 1;
		unsigned int SBACapable : 1;
		unsigned int shortReset : 1;
		unsigned int rsvd1 : 1;
		unsigned int AGPDataThrottle : 1;
		unsigned int retryDisable : 1;
		unsigned int rsvd : 2;
		unsigned int VGAFixed : 1;
		unsigned int VGAEnable : 1;
		unsigned int baseClassZero : 1;
	} bits;
	int all;
} p2_chipconfig_reg;

#define OUTDMAADDRESS_REG		0x00000080

#define OUTDMACOUNT_REG			0x00000088
#define MAX_OUTDMA_SIZE			0x0000FFFF

#define AGPTEXADDRESS_REG		0x00000090

#define BYDMAADDRESS_REG		0x000000A0

#define BYDMASTRIDE_REG			0x000000B8

#define BYDMAMEMADDR_REG		0x000000C0

#define BYDMASIZE_REG			0x000000C8
typedef union {
	struct {
		unsigned int rsvd1 : 4;
		unsigned int height : 12;
		unsigned int rsvd : 4;
		unsigned int width : 12;
	} bits;
	int all;
} p2_bydmasize_reg;
#define BYDMASIZE_HW(h, w) (((h) << 16) | ((w) & 0x0FFF))

#define BYDMABYTEMASK_REG		0x000000D0
typedef union {
	struct {
		unsigned int rsvd1 : 11;
		unsigned int rightEdgeMask : 4;
		unsigned int rsvd : 11;
		unsigned int leftEdgeMask : 4;
	} bits;
	int all;
} p2_bydmabytemask_reg;

#define BYDMACONTROL_REG		0x000000D8
typedef union {
	struct {
		unsigned int rsvd : 2;
		unsigned int useAGP : 1;
		unsigned int byteSwapControl : 2;
#define BYDMA_BYTE_SWAP_STD	0 
#define BYDMA_BYTE_SWAP		1 
#define BYDMA_HW_SWAP		2
		unsigned int offsetY : 5;
		unsigned int offsetX : 5;
		unsigned int pp2 : 3;
		unsigned int pp1 : 3;
		unsigned int pp0 : 3;
		unsigned int patchType : 2;
#define BYDMA_PATCH_DIS		0
#define BYDMA_PATCH_8X8		1
#define BYDMA_PATCH_32X32	2
		unsigned int dataFormat : 3;
#define BYDMA_FMT_8BITS		0
#define BYDMA_FMT_16BITS	1
#define BYDMA_FMT_32BITS	2
#define BYDMA_FMT_4BITS		3
#define BYDMA_FMT_YUV_Y		4
#define BYDMA_FMT_YUV_U		5
#define BYDMA_FMT_YUV_V		6
		unsigned int restartGP : 1;
		unsigned int mode : 2;
#define BYDMA_MODE_OFF		0
#define BYDMA_MODE_IMPLICIT	1
#define BYDMA_MODE_AP1		2
#define BYDMA_MODE_AP2		3
	} bits;
	int all;
} p2_bydmacontrol_reg;

#define BYDMACOMPLETE_REG		0x000000E8
#define BYDMA_COMPLETE_BIT		(1 << 0)


/* Memory Control Registers */
#define REBOOT_REG				0x00001000

#define MEMCONTROL_REG			0x00001040
#define MEMCONTROL_SGRAM		(0 << 4)
#define MEMCONTROL_SDRAM		(1 << 4)

#define BOOTADDRESS_REG			0x00001080

#define MEMCONFIG_REG			0x000010C0
typedef union {
	struct {
		unsigned int burst1Cycle : 1;
		unsigned int numberBanks : 2;
#define NUMBER_OF_BANKS_1	0
#define NUMBER_OF_BANKS_2	1
#define NUMBER_OF_BANKS_3	2
#define NUMBER_OF_BANKS_4	3
		unsigned int refreshCount : 7;
		unsigned int blockWrite1 : 1;
		unsigned int bankDelay : 3;
		unsigned int deadCycleEnable : 1;
		unsigned int CASLatency : 1;
#define CAS_LATENCY_2	0
#define CAS_LATENCY_3	1
		unsigned int timeRASMin : 3;
		unsigned int rsvd : 2;
		unsigned int rowCharge : 1;
		unsigned int timeRCD : 3;
		unsigned int timeRC : 4;
		unsigned int timeRP : 3;
	} bits;
	int all;
} p2_memconfig_reg;

#define BYPASSWRITEMASK_REG		0x00001100

#define FRAMEBUFFERWRITEMASK_REG		0x00001140

#define COUNT_REG				0x00001180


/* GP FIFOs */
#define OUT_FIFO			0x00002000

#define IN_FIFO				0x00002000


/* Video Control Registers */
#define SCREENBASE_REG			0x00003000

#define SCREENSTRIDE_REG		0x00003008

#define HTOTAL_REG				0x00003010

#define HGEND_REG				0x00003018

#define HBEND_REG				0x00003020

#define HSSTART_REG				0x00003028

#define HSEND_REG				0x00003030

#define VTOTAL_REG				0x00003038

#define VBEND_REG				0x00003040

#define VSSTART_REG				0x00003048

#define VSEND_REG				0x00003050

#define VIDEOCONTROL_REG		0x00003058
typedef union {
	struct {
		unsigned int rsvd : 15;
		unsigned int data64Enable : 1;
		unsigned int GPPendingRight : 1;
		unsigned int bypassPendingRight : 1;
		unsigned int rightFrame : 1;
		unsigned int rightEyeControl : 1;
#define RIGHT_EYE_HI	0
#define RIGHT_EYE_LO	1
		unsigned int stereoEnable : 1;
		unsigned int bufferSwapCtl : 2;
#define BUFFERSWAP_SYNCONFRAME	0
#define BUFFERSWAP_FREERUN		1
#define BUFFERSWAP_LIMITTOFRAME	2
		unsigned int GPPending : 1;
		unsigned int bypassPending : 1;
		unsigned int vSyncCtl : 2;
/* see hSyncCtl */
		unsigned int hSyncCtl : 2;
#define SYNC_FORCE_HI	0 
#define SYNC_ACTIVE_HI	1 
#define SYNC_FORCE_LO	2
#define SYNC_ACTIVE_LO	3
		unsigned int lineDouble : 1;
		unsigned int blankCtl : 1;
#define BLANKCTL_HI	0 
#define BLANKCTL_LO	1
		unsigned int enable : 1;
	} bits;
	int all;
} p2_videocontrol_reg;
#define BYPASS_PENDING_FLAG		(1 << 7)
#define GP_PENDING_FLAG			(1 << 8)
#define RIGHT_FRAME_FLAG		(1 << 13)
#define BYPASS_PENDING_RIGHT_FLAG	(1 << 14)
#define GP_PENDING_RIGHT_FLAG	(1 << 15)

#define INTERRUPTLINE_REG		0x00003060

#define DISPLAYDATA_REG			0x00003068
typedef union {
	struct {
		unsigned int rsvd : 16;
		unsigned int monitorIDOut : 3;
		unsigned int monitorIDIn : 3;
		unsigned int useMonitorID : 1;
		unsigned int wait : 1;
		unsigned int stop : 1;
		unsigned int start : 1;
		unsigned int dataValid : 1;
		unsigned int latchedData : 1;
		unsigned int clkOut : 1;
		unsigned int dataOut : 1;
		unsigned int clkIn : 1;
		unsigned int dataIn : 1;
	} bits;
	int all;
} p2_displaydata_reg;

#define LINECOUNT_REG			0x00003070

#define FIFOCONTROL_REG			0x00003078
typedef union {
	struct {
		unsigned int rsvd2 : 15;
		unsigned int underflow : 1;
		unsigned int rsvd1 : 3;
		unsigned int highThreshold : 5;
		unsigned int rsvd : 3;
		unsigned int lowThreshold : 5;
	} bits;
	int all;
} p2_fifocontrol_reg;

#define SCREENBASERIGHT_REG		0x00003080


/* RAMDAC Registers */
#define RDPALETTEWRITEADDRESS_REG	0x00004000

#define RDPALETTEDATA_REG		0x00004008

#define RDPIXELMASK_REG			0x00004010

#define RDPALETTEREADADDRESS_REG	0x00004018

#define RDCURSORCOLORADDRESS_REG	0x00004020

#define RDINDEXLOW_2V_REG	0x00004020

#define RDCURSORCOLORDATA_REG	0x00004028

#define RDINDEXHIGH_2V_REG	0x00004028

#define RDINDEXEDDATA_2V_REG	0x00004030

#define RDINDEXCONTROL_2V_REG	0x00004038

#define RDINDEXEDDATA_REG		0x00004050

#define RDCURSORRAMDATA_REG		0x00004058

#define RDCURSORXLOW_REG		0x00004060

#define RDCURSORXHIGH_REG		0x00004068

#define RDCURSORYLOW_REG		0x00004070

#define RDCURSORYHIGH_REG		0x00004078

/* RAMDAC Indirect Registers */
#define RDCURSORCONTROL_REG		0x06
typedef union {
	struct {
		unsigned int rsvd : 24;
		unsigned int cursorSize : 2;
#define CURSORSIZE_32X32	0
#define CURSORSIZE_64X64	1
		unsigned int cursorSelect : 2;
		unsigned int RAMAddr : 2;
		unsigned int cursorMode : 2;
#define CURSORMODE_DIS		0
#define CURSORMODE_3COLOR	1
#define CURSORMODE_XGA		2
#define CURSORMODE_XWINDOW	3
	} bits;
	struct {
		unsigned int rsvd : 29;
		unsigned int readbackPosition : 1;
		unsigned int doubleY : 1;
		unsigned int doubleX : 1;
	} bits_2v;
	int all;
} p2_rdcursorcontrol_reg;

#define RDCOLORMODE_REG			0x18
typedef union {
	struct {
		unsigned int rsvd1 : 24;
		unsigned int trueColor : 1;
		unsigned int rsvd : 1;
		unsigned int RGB : 1;
		unsigned int gui : 1;
		unsigned int pixelFormat : 4;
#define PIXFMT_SVGA				0
#define PIXFMT_RGB332			1
#define PIXFMT_RGB232_OFFSET	2
#define PIXFMT_RGBA2321			3
#define PIXFMT_RGBA5551			4
#define PIXFMT_RGBA4444			5
#define PIXFMT_RGB565			6
#define PIXFMT_RGBA8888			8
#define PIXFMT_RGB888			9
	} bits;
	int all;
} p2_rdcolormode_reg;

#define RDMODECONTROL_REG		0x19
typedef union {
	struct {
		unsigned int rsvd : 29;
		unsigned int dynamicDoubleBuffer : 1;
		unsigned int staticDoubleBuffer : 1;
		unsigned int bufferSelect : 1;
#define BUFFER_SEL_FRONT	0
#define BUFFER_SEL_BACK		1
	} bits;
	int all;
} p2_rdmodecontrol_reg;

#define RDPALETTEPAGE_REG		0x1C

#define RDMISCCONTROL_REG		0x1E
typedef union {
	struct {
		unsigned int rsvd : 26;
		unsigned int syncOnGreen : 1;
		unsigned int blankPedestal : 1;
		unsigned int vSyncPolarity : 1;
/* see hSyncPolarity */
		unsigned int hSyncPolarity : 1;
#define SYNC_NONINVERTED	0
#define SYNC_INVERTED	1
		unsigned int paletteWidth : 1;
#define PALETTEWIDTH_6BITS	0
#define PALETTEWIDTH_8BITS	1
		unsigned int DACPowerDown : 1;
	} bits;
	int all;
} p2_rdmisccontrol_reg;

#define RDPIXELCLOCKA1_REG		0x20

#define RDPIXELCLOCKA2_REG		0x21

#define RDPIXELCLOCKA3_REG		0x22
/* see PIXELCLOCK_EN_BIT */

#define RDPIXELCLOCKB1_REG		0x23

#define RDPIXELCLOCKB2_REG		0x24

#define RDPIXELCLOCKB3_REG		0x25
/* see PIXELCLOCK_EN_BIT */

#define RDPIXELCLOCKC1_REG		0x26

#define RDPIXELCLOCKC2_REG		0x27

#define RDPIXELCLOCKC3_REG		0x28
#define PIXELCLOCK_EN_BIT	(1 << 3)

#define RDPIXELCLOCKSTATUS_REG	0x29
/*see PLL_LOCKED_FLAG */

#define RDMEMORYCLOCK1_REG		0x30

#define RDMEMORYCLOCK2_REG		0x31

#define RDMEMORYCLOCK3_REG		0x32

#define RDMEMORYCLOCKSTATUS_REG	0x33
#define PLL_LOCKED_FLAG	(1 << 4)

#define RDCOLORKEYCONTROL_REG	0x40
typedef union {
	struct {
		unsigned int rsvd1 : 27;
		unsigned int testPolarity : 1;
#define TESTPOLARITY_TRUE	0
#define TESTPOLARITY_FALSE	1
		unsigned int blueEnable : 1;
		unsigned int greenEnable : 1;
		unsigned int redEnable : 1;
		unsigned int overlayEnable : 1;
	} bits;
	int all;
} p2_rdcolorkeycontrol_reg;

#define RDOVERLAYKEY_REG	0x41

#define RDREDKEY_REG		0x42

#define RDGREENKEY_REG		0x43

#define RDBLUEKEY_REG		0x44

/* 2V RAMDAC Indirect Registers */
#define RDMISCCONTROL_2V_REG	0x000
typedef union {
	struct {
		unsigned int rsvd1 : 25;
		unsigned int stereoDoubleBuffer : 1;
		unsigned int pixelDoubleBuffer : 1;
		unsigned int overlay : 1;
		unsigned int directColor : 1;
		unsigned int lastReadAddress : 1;
		unsigned int pixelDouble : 1;
		unsigned int highColorResolution : 1;
	} bits;
	int all;
} p2_rdmisccontrol_2v_reg;

#define RDSYNCCONTROL_2V_REG	0x001
typedef union {
	struct {
		unsigned int rsvd1 : 26;
		unsigned int vSyncCtl : 3;
/* see hSyncCtl */
		unsigned int hSyncCtl : 3;
#define ACTIVE_LOW_AT_PIN_2V	0
#define ACTIVE_HIGH_AT_PIN_2V	1
#define TRISTATE_AT_PIN_2V	2
#define FORCE_ACTIVE_2V	3
#define FORCE_INACTIVE_2V	4
	} bits;
	int all;
} p2_rdsynccontrol_2v_reg;

#define RDDACCONTROL_2V_REG	0x002
typedef union {
	struct {
		unsigned int rsvd1 : 24;
		unsigned int blankPedestal : 1;
		unsigned int blankBlueDAC : 1;
		unsigned int blankGreenDAC : 1;
		unsigned int blankRedDAC : 1;
		unsigned int rsvd : 1;
		unsigned int DACPowerCtl : 3;
#define NORMAL_OPERATION_2V	0
#define LOWPOWER_2V	1
	} bits;
	int all;
} p2_rddaccontrol_2v_reg;

#define RDPIXELSIZE_2V_REG	0x003
#define PIXELSIZE_8BITS_2V	0
#define PIXELSIZE_16BITS_2V	1
#define PIXELSIZE_32BITS_2V	2
#define PIXELSIZE_24BITS_2V	4

#define RDCOLORFORMAT_2V_REG	0x004
typedef union {
	struct {
		unsigned int rsvd1 : 25;
		unsigned int linearColorExtension : 1;
		unsigned int RGB : 1;
		unsigned int colorFormat : 5;
#define COLORFORMAT_8888_2V	0
#define COLORFORMAT_5551FRONT_2V	1
#define COLORFORMAT_4444_2V	2
#define COLORFORMAT_332FRONT_2V	5
#define COLORFORMAT_332BACK_2V	6
#define COLORFORMAT_2321FRONT_2V	9
#define COLORFORMAT_2321BACK_2V	10
#define COLORFORMAT_232FRONTOFF_2V	11
#define COLORFORMAT_232BACKOFF_2V	12
#define COLORFORMAT_5551BACK_2V	13
#define COLORFORMAT_CI8_2V	14
#define COLORFORMAT_565FRONT_2V	16
#define COLORFORMAT_565BACK_2V	17
	} bits;
	int all;
} p2_rdcolorformat_2v_reg;

#define RDCURSORMODE_2V_REG	0x005
typedef union {
	struct {
		unsigned int rsvd1 : 25;
		unsigned int reversePixelOrder : 1;
		unsigned int type : 2;
#define TYPE_WINDOWS_2V	0
#define TYPE_X_2V	1
#define TYPE_3COLOR_2V	2
#define TYPE_15COLOR_2V	3
		unsigned int format : 3;
#define FORMAT_64X64_2V	0
#define FORMAT_32X32_0_2BITS_2V	1
#define FORMAT_32X32_1_2BITS_2V	2
#define FORMAT_32X32_2_2BITS_2V	3
#define FORMAT_32X32_3_2BITS_2V	4
#define FORMAT_32X32_01_4BITS_2V	5
#define FORMAT_32X32_23_4BITS_2V	6
		unsigned int cursorEnable : 1;
	} bits;
	int all;
} p2_rdcursormode_2v_reg;

#define RDCURSORCONTROL_2V_REG	0x006

#define RDCURSORXLOW_2V_REG		0x007

#define RDCURSORXHIGH_2V_REG	0x008

#define RDCURSORYLOW_2V_REG		0x009

#define RDCURSORYHIGH_2V_REG	0x00A

#define RDCURSORHOTSPOTX_2V_REG	0x00B

#define RDCURSORHOTSPOTY_2V_REG	0x00C

#define RDOVERLAYKEY_2V_REG	0x00D

#define RDPAN_2V_REG	0x00E

#define RDSENSE_2V_REG	0x00F
#define RDSENSE_2V_RED_BIT	0
#define RDSENSE_2V_GREEN_BIT	1
#define RDSENSE_2V_BLUE_BIT	2

#define RDCHECKCONTROL_2V_REG	0x018
typedef union {
	struct {
		unsigned int rsvd1 : 30;
		unsigned int LUT : 1;
		unsigned int pixel : 1;
	} bits;
	int all;
} p2_rdcheckcontrol_2v_reg;

#define RDCHECKPIXELRED_2V_REG	0x019
#define RDCHECKPIXELGREEN_2V_REG	0x01A
#define RDCHECKPIXELBLUE_2V_REG	0x01B

#define RDCHECKLUTLRED_2V_REG	0x01C
#define RDCHECKLUTGREEN_2V_REG	0x01D
#define RDCHECKLUTBLUE_2V_REG	0x01E

#define RDDCLKCONTROL_2V_REG	0x200
#define PLL_ENABLED_2V_BIT	(1 << 0)
#define PLL_LOCKED_2V_FLAG	(1 << 1)

#define RDDCLK0PRESCALE_2V_REG	0x201
#define RDDCLK0FEEDBACKSCALE_2V_REG	0x202
#define RDDCLK0POSTSCALE_2V_REG	0x203

#define RDDCLK1PRESCALE_2V_REG	0x204
#define RDDCLK1FEEDBACKSCALE_2V_REG	0x205
#define RDDCLK1POSTSCALE_2V_REG	0x206

#define RDDCLK2PRESCALE_2V_REG	0x207
#define RDDCLK2FEEDBACKSCALE_2V_REG	0x208
#define RDDCLK2POSTSCALE_2V_REG	0x209

#define RDDCLK3PRESCALE_2V_REG	0x20A
#define RDDCLK3FEEDBACKSCALE_2V_REG	0x20B
#define RDDCLK3POSTSCALE_2V_REG	0x20C

#define RDMCLKCONTROL_2V_REG	0x20D
/* see PLL_ENABLED_2V_BIT and PLL_LOCKED_2V_FLAG */

#define RDMCLKPRESCALE_2V_REG	0x20E
#define RDMCLKFEEDBACKSCALE_2V_REG	0x20F
#define RDMCLKPOSTSCALE_2V_REG	0x210

#define RDCURSORPALETTE_2V_REG	0x303

#define RDCURSORPATTERN_2V_REG	0x400


/* SVGA Registers */
#define VGACONTROL_REG		0x000063C4
typedef union {
	struct {
		unsigned int rsvd2 : 25;
		unsigned int enableVTG : 1;
		unsigned int dacAddr3 : 1;
		unsigned int dacAddr2 : 1;
		unsigned int enableVGADisplay : 1;
		unsigned int enableInterrupts : 1;
		unsigned int enableHostDACAccess : 1;
		unsigned int enableHostMemoryAccess : 1;
	} bits;
	int all;
} p2_vgacontrol_reg;


/* GP Registers */
/* NOTE: incomplete as of 7/10/98 */
#define STARTXDOM_REG		0x00008000

#define DXDOM_REG		0x00008008

#define STARTXSUB_REG		0x00008010

#define DXSUB_REG		0x00008018

#define STARTY_REG		0x00008020

#define DY_REG		0x00008028

#define GP_COUNT_REG		0x00008030

#define RENDER_REG		0x00008038
#define RENDER_AREASTIPPLEENABLE	(1 << 0)
#define RENDER_FASTFILLENABLE		(1 << 3)
#define RENDER_PRIMITIVE_LINE		(0 << 6)
#define RENDER_PRIMITIVE_TRAPEZOID	(1 << 6)
#define RENDER_PRIMITIVE_POINT		(2 << 6)
#define RENDER_PRIMITIVE_RECTANGLE	(3 << 6)
#define RENDER_SYNCONBITMASK		(1 << 11)
#define RENDER_SYNCONHOSTDATA		(1 << 12)
#define RENDER_TEXTUREENABLE		(1 << 13)
#define RENDER_FOGENABLE			(1 << 14)
#define RENDER_SUBPIXELCORRECTIONENABLE		(1 << 16)
#define RENDER_REUSEBITMASK			(1 << 17)
#define RENDER_INCREASEX			(1 << 21)
#define RENDER_INCREASEY			(1 << 22)

#define CONTINUENEWLINE_REG		0x00008040

#define CONTINUENEWDOM_REG		0x00008048

#define CONTINUENEWSUB_REG		0x00008050

#define CONTINUE_REG		0x00008058

#define BITMASKPATTERN_REG		0x00008068

#define WAITFORCOMPLETION_REG		0x00008088

#define RASTERIZERMODE_REG	0x000080A0

#define YLIMITS_REG	0x000080A8
/* see XLIMITS_REG */

#define XLIMITS_REG	0x000080C8
#define LIMITS_MINMAX(min, max) (((max) << 16) | ((min) & 0x0FFF))

#define RECTANGLEORIGIN_REG	0x000080D0
#define RECTORIGIN_YX(y, x) (((y) << 16) | ((x) & 0x0FFF))

#define RECTANGLESIZE_REG	0x000080D8
#define RECTSIZE_HW(h, w) (((h) << 16) | ((w) & 0x0FFF))

#define PACKEDDATALIMITS_REG	0x00008150
typedef union {
	struct {
		unsigned int relativeOffset : 3;
		unsigned int rsvd : 1;
		unsigned int xStart : 12;
		unsigned int rsvd1 : 4;
		unsigned int xEnd : 12;
	} bits;
	int all;
} p2_packeddatalimits_reg;

#define SCISSORMODE_REG	0x00008180
typedef union {
	struct {
		unsigned int rsvd1 : 30;
		unsigned int screenScissorEnable : 1;
		unsigned int userScissorEnable : 1;
	} bits;
	int all;
} p2_scissormode_reg;

#define SCISSORMINXY_REG	0x00008188

#define SCISSORMAXXY_REG	0x00008190

#define SCREENSIZE_REG	0x00008198
#define SCREENSIZE_HW(h, w) ((((h) & 0x07FF) << 16) | ((w) & 0x07FF))

#define AREASTIPPLEMODE_REG		0x000081A0
typedef union {
	struct {
		unsigned int rsvd3 : 11;
		unsigned int forceBackgroundColor : 1;
		unsigned int mirrorY : 1;
		unsigned int mirrorX : 1;
		unsigned int invertStipplePattern : 1;
		unsigned int rsvd2 : 2;
		unsigned int yOffset : 3;
		unsigned int rsvd1 : 2;
		unsigned int xOffset : 3;
		unsigned int rsvd : 6;
		unsigned int unitEnable : 1;
	} bits;
	int all;
} p2_areastipplemode_reg;

#define WINDOWORIGIN_REG	0x000081C8
typedef union {
	struct {
		unsigned int rsvd : 4;
		unsigned int y : 12;
		unsigned int rsvd1 : 4;
		unsigned int x : 12;
	} bits;
	int all;
} p2_windoworigin_reg;

#define AREASTIPPLEPATTERN0_REG		0x00008200

#define AREASTIPPLEPATTERN1_REG		0x00008208

#define AREASTIPPLEPATTERN2_REG		0x00008210

#define AREASTIPPLEPATTERN3_REG		0x00008218

#define AREASTIPPLEPATTERN4_REG		0x00008220

#define AREASTIPPLEPATTERN5_REG		0x00008228

#define AREASTIPPLEPATTERN6_REG		0x00008230

#define AREASTIPPLEPATTERN7_REG		0x00008238

#define TEXTUREADDRESSMODE_REG		0x00008380
#define TEXTUREADDRESSMODE_TEXTURE_ADDRESS_ENABLE (1 << 0)
#define TEXTUREADDRESSMODE_PERSPECTIVE_CORRECTION_ENABLE (1 << 1)

#define SSTART_REG		0x00008388

#define DSDX_REG		0x00008390

#define DSDYDOM_REG		0x00008398

#define TSTART_REG		0x000083A0

#define DTDX_REG		0x000083A8

#define DTDYDOM_REG		0x000083B0

#define DQDX_REG		0x000083C0

#define DQDYDOM_REG		0x000083C8

#define TEXELLUTINDEX_REG		0x000084C0

#define TEXELLUTDATA_REG		0x000084C8

#define TEXELLUTADDRESS_REG		0x000084D0
#define TEXELLUTADDRESS_SYSTEM_MEMORY		(1 << 30)
#define TEXELLUTADDRESS_INVALID_ADDRESS		(1 << 31)

#define TEXELLUTTRANSFER_REG	0x000084D8

#define TEXTUREBASEADDRESS_REG	0x00008580

#define TEXTUREMAPFORMAT_REG	0x00008588
typedef union {
	struct {
		unsigned int rsvd2 : 10;
		unsigned int texelSize : 3;
#define TEXELSIZE_8BITS		0
#define TEXELSIZE_16BITS	1
#define TEXELSIZE_32BITS	2
#define TEXELSIZE_4BITS		3
#define TEXELSIZE_24BITS	4
		unsigned int rsvd1 : 1;
		unsigned int subpatchMode : 1;
		unsigned int windowOrigin : 1;
/* see FBREADMODE_REG windowOrigin */
		unsigned int rsvd : 7;
		unsigned int pp2 : 3;
		unsigned int pp1 : 3;
		unsigned int pp0 : 3;
	} bits;
	int all;
} p2_texturemapformat_reg;

#define TEXTUREDATAFORMAT_REG	0x00008590
typedef union {
	struct {
		unsigned int rsvd : 22;
		unsigned int spanFormat : 1;
		unsigned int alphaMap : 2;
		unsigned int textureFormatExtension : 1;
/* see RDCOLORFORMAT_2V_REG colorFormat bit 4, plus below */
		unsigned int colorOrder : 1;
/* see ALPHABLENDMODE_REG colorOrder */
		unsigned int noAlphaBuffer : 1;
		unsigned int textureFormat : 4;
/* see RDCOLORFORMAT_2V_REG colorFormat bits 0-3, plus: */
#define COLORFORMAT_CI4_2V	15
#define COLORFORMAT_YUV444_2V	18
#define COLORFORMAT_YUV422_2V	19
	} bits;
	int all;
} p2_texturedataformat_reg;

#define TEXEL0_REG	0x00008600

#define TEXTUREREADMODE_REG	0x00008670
typedef union {
	struct {
		unsigned int rsvd2 : 7;
		unsigned int packedData : 1;
		unsigned int rsvd1 : 6;
		unsigned int filterMode : 1;
		unsigned int height : 4;
		unsigned int width : 4;
		unsigned int rsvd : 4;
		unsigned int tWrapMode : 2;
#define WRAPMODE_CLAMP		0
#define WRAPMODE_REPEAT		1
#define WRAPMODE_MIRROR		2
		unsigned int sWrapMode : 2;
/* see tWrapMode */
		unsigned int enable : 1;
	} bits;
	int all;
} p2_texturereadmode_reg;

#define TEXELLUTMODE_REG	0x00008678
typedef union {
	struct {
		unsigned int rsvd : 20;
		unsigned int pixelsPerEntry : 2;
#define PIXELSPERENTRY_1		0
#define PIXELSPERENTRY_2		1
#define PIXELSPERENTRY_4		2
		unsigned int offset : 8;
		unsigned int directIndex : 1;
#define DIRECTINDEX_TEXTUREDATA		0
#define DIRECTINDEX_FRAGXY		1
		unsigned int enable : 1;
	} bits;
	int all;
} p2_texellutmode_reg;

#define TEXTURECOLORMODE_REG		0x00008680
typedef union {
	struct {
		unsigned int rsvd : 25;
		unsigned int KsDDA : 1;
		unsigned int KdDDA : 1;
		unsigned int textureType : 1;
#define TEXTURETYPE_RGB		0
#define TEXTURETYPE_RAMP	1
		unsigned int applicationMode : 3;
#define APPLICATIONMODE_RGB_MODULATE	0
#define APPLICATIONMODE_RGB_DECAL		1
#define APPLICATIONMODE_RGB_COPY		3
#define APPLICATIONMODE_RGB_MODULATEHIGHLIGHT		4
#define APPLICATIONMODE_RGB_DECALHIGHLIGHT		5
#define APPLICATIONMODE_RGB_COPYHIGHLIGHT		7
#define APPLICATIONMODE_RAMP_DECAL		1
#define APPLICATIONMODE_RAMP_MODULATE	2
#define APPLICATIONMODE_RAMP_HIGHLIGHT	4
		unsigned int textureEnable : 1;
	} bits;
	int all;
} p2_texturecolormode_reg;

#define FOGMODE_REG		0x00008690
typedef union {
	struct {
		unsigned int rsvd1 : 29;
		unsigned int fogTest : 1;
		unsigned int rsvd : 1;
		unsigned int fogEnable : 1;
	} bits;
	int all;
} p2_fogmode_reg;

#define DFDX_REG		0x000086A8

#define DFDYDOM_REG		0x000086B0

#define DKSDX_REG		0x000086D0

#define DKSDYDOM_REG		0x000086D8

#define DKDDX_REG		0x000086E8

#define DKDDYDOM_REG		0x000086F0

#define RSTART_REG		0x00008780

#define DRDX_REG		0x00008788

#define DRDYDOM_REG		0x00008790

#define GSTART_REG		0x00008798

#define DGDX_REG		0x000087A0

#define DGDYDOM_REG		0x000087A8

#define BSTART_REG		0x000087B0

#define DBDX_REG		0x000087B8

#define DBDYDOM_REG		0x000087C0

#define ASTART_REG		0x000087C8

#define COLORDDAMODE_REG		0x000087E0
typedef union {
	struct {
		unsigned int rsvd : 30;
		unsigned int shadingMode : 1;
#define SHADINGMODE_FLAT	0
#define SHADINGMODE_GOURAUD	1
		unsigned int unitEnable : 1;
	} bits;
	int all;
} p2_colorddamode_reg;

#define CONSTANTCOLOR_REG		0x000087E8

#define COLOR_REG		0x000087F0

#define ALPHABLENDMODE_REG		0x00008810
typedef union {
	struct {
		unsigned int rsvd1 : 13;
		unsigned int alphaConversion : 1;
#define ALPHACONVERSION_SCALE	0
#define ALPHACONVERSION_SHIFT	1
		unsigned int colorConversion : 1;
#define COLORCONVERSION_SCALE	0
#define COLORCONVERSION_SHIFT	1
		unsigned int colorFormatExtension : 1;
/* see RDCOLORFORMAT_2V_REG colorFormat bit 4 */
		unsigned int rsvd : 1;
		unsigned int blendType : 1;
#define BLENDTYPE_RGB	0
#define BLENDTYPE_RAMP	1
		unsigned int colorOrder : 1;
#define COLORORDER_BGR	0
#define COLORORDER_RGB	1
		unsigned int noAlphaBuffer : 1;
		unsigned int colorFormat : 4;
/* see RDCOLORFORMAT_2V_REG colorFormat bits 0-3 */
		unsigned int operation : 7;
#define OPERATION_FORMAT 16
#define OPERATION_PREMULT 81
#define OPERATION_BLEND 84
		unsigned int alphaBlendEnable : 1;
	} bits;
	int all;
} p2_alphablendmode_reg;

#define DITHERMODE_REG		0x00008818
typedef union {
	struct {
		unsigned int rsvd1 : 15;
		unsigned int colorFormatExtension : 1;
/* see RDCOLORFORMAT_2V_REG colorFormat bit 4 */
		unsigned int rsvd : 2;
		unsigned int forceAlpha : 2;
#define FORCEALPHA_DISABLE	0
#define FORCEALPHA_FORCETO0	1
#define FORCEALPHA_FORCETO0XF8	2
		unsigned int ditherMethod : 1;
#define DITHERMETHOD_ORDERED	0
#define DITHERMETHOD_LINE		1
		unsigned int colorOrder : 1;
/* see ALPHABLENDMODE_REG colorOrder */
		unsigned int yOffset : 2;
		unsigned int xOffset : 2;
		unsigned int colorFormat : 4;
/* see RDCOLORFORMAT_2V_REG colorFormat bits 0-3 */
		unsigned int ditherEnable : 1;
		unsigned int unitEnable : 1;
	} bits;
	int all;
} p2_dithermode_reg;

#define FBSOFTWAREWRITEMASK_REG		0x00008820

#define LOGICALOPMODE_REG		0x00008828
typedef union {
	struct {
		unsigned int rsvd1 : 26;
		unsigned int useConstantFBWriteData : 1;
		unsigned int logicOp : 4;
#define LOGICOP_CLEAR	0
#define LOGICOP_AND		1
#define LOGICOP_ANDREVERSE	2
#define LOGICOP_COPY	3
#define LOGICOP_ANDINVERTED	4
#define LOGICOP_NOOP	5
#define LOGICOP_XOR		6
#define LOGICOP_OR		7
#define LOGICOP_NOR		8
#define LOGICOP_EQUIV	9
#define LOGICOP_INVERT	10
#define LOGICOP_ORREVERSE	11
#define LOGICOP_COPYINVERT	12
#define LOGICOP_ORINVERT	13
#define LOGICOP_NAND	14
#define LOGICOP_SET	15
		unsigned int logicalOpEnable : 1;
	} bits;
	int all;
} p2_logicalopmode_reg;

#define FBWRITEDATA_REG		0x00008830

#define LBREADMODE_REG		0x00008880
typedef union {
	struct {
		unsigned int rsvd1 : 12;
		unsigned int patchEnable : 1;
		unsigned int windowOrigin : 1;
/* see FBREADMODE_REG windowOrigin */
		unsigned int dataType : 2;
#define DATATYPE_LBDEFAULT	0
#define DATATYPE_LBSTENCIL 	1
#define DATATYPE_LBDEPTH 	2
		unsigned int rsvd : 5;
		unsigned int readDestinationEnable : 1;
		unsigned int readSourceEnable : 1;
		unsigned int pp2 : 3;
		unsigned int pp1 : 3;
		unsigned int pp0 : 3;
	} bits;
	int all;
} p2_lbreadmode_reg;

#define LBREADFORMAT_REG		0x00008888
typedef union {
	struct {
		unsigned int rsvd1 : 28;
		unsigned int stencilWidth : 2;
#define STENCILWIDTH_0	0
#define STENCILWIDTH_1 	3
		unsigned int depthWidth : 2;
#define DEPTHWIDTH_16	0
#define DEPTHWIDTH_15 	3
	} bits;
	int all;
} p2_lbreadformat_reg;

#define LBSOURCEOFFSET_REG		0x00008890

#define LBDATA_REG		0x00008898

#define LBSTENCIL_REG		0x000088A8

#define LBDEPTH_REG		0x000088B0

#define LBWINDOWBASE_REG		0x000088B8

#define LBWRITEMODE_REG		0x000088C0

#define LBWRITEFORMAT_REG		0x000088C8
/* see p2_lbreadformat_reg */

#define TEXTUREDATA_REG		0x000088E8

#define TEXTUREDOWNLOADOFFSET_REG		0x000088F0

#define WINDOW_REG		0x00008980
typedef union {
	struct {
		unsigned int rsvd2 : 13;
		unsigned int disableLBUpdate : 1;
		unsigned int rsvd1 : 13;
		unsigned int LBUpdateSource : 1;
#define LBUPDATESOURCE_LBSOURCEDATA	0
#define LBUPDATESOURCE_REGISTERS 	1
		unsigned int forceLBUpdate : 1;
		unsigned int rsvd : 3;
	} bits;
	int all;
} p2_window_reg;

#define STENCILMODE_REG		0x00008988
typedef union {
	struct {
		unsigned int rsvd : 17;
		unsigned int stencilSource : 2;
#define STENCILSOURCE_TEST	0
#define STENCILSOURCE_STENCILREG	1
#define STENCILSOURCE_LBDATA	2
#define STENCILSOURCE_LBSOURCEDATA	3
		unsigned int func : 3;
/* see p2_depthmode_reg compareMode */
		unsigned int sfail : 3;
#define UPDATEMETHOD_KEEP	0
#define UPDATEMETHOD_ZERO	1
#define UPDATEMETHOD_REPLACE	2
#define UPDATEMETHOD_INCREMENT	3
#define UPDATEMETHOD_DECREMENT	4
#define UPDATEMETHOD_INVERT	5
		unsigned int dpfail : 3;
/* see sfail */
		unsigned int dppass : 3;
/* see sfail */
		unsigned int unitEnable : 1;
	} bits;
	int all;
} p2_stencilmode_reg;

#define STENCILDATA_REG		0x00008990

#define STENCIL_REG		0x00008998

#define DEPTHMODE_REG		0x000089A0
typedef union {
	struct {
		unsigned int rsvd : 25;
		unsigned int compareMode : 3;
#define COMPAREMODE_NEVER	0
#define COMPAREMODE_LESS	1
#define COMPAREMODE_EQUAL	2
#define COMPAREMODE_LESSOREQUAL	3
#define COMPAREMODE_GREATER	4
#define COMPAREMODE_NOTEQUAL	5
#define COMPAREMODE_GREATEROREQUAL	6
#define COMPAREMODE_ALWAYS	7
		unsigned int newDepthSource : 2;
#define NEWDEPTHSOURCE_FRAGDEPTHVAL	0
#define NEWDEPTHSOURCE_LBDATA	1
#define NEWDEPTHSOURCE_DEPTHREG	2
#define NEWDEPTHSOURCE_LBSOURCEDATA	3
		unsigned int writeMask : 1;
		unsigned int unitEnable : 1;
	} bits;
	int all;
} p2_depthmode_reg;

#define DEPTH_REG		0x000089A8

#define DZDXU_REG		0x000089C0

#define DZDXL_REG		0x000089C8

#define DZDYDOMU_REG		0x000089D0

#define DZDYDOML_REG		0x000089D8

#define FBREADMODE_REG		0x00008A80
typedef union {
	struct {
		unsigned int rsvd3 : 5;
		unsigned int patchMode : 2;
#define PATCHMODE_PATCH	0
#define PATCHMODE_SUBPATCH	1
#define PATCHMODE_SUBPATCHPACK	2
		unsigned int rsvd2 : 2;
		unsigned int relativeOffset : 3;
		unsigned int packedData : 1;
		unsigned int patchEnable : 1;
		unsigned int rsvd1 : 1;
		unsigned int windowOrigin : 1;
#define WINDOWORIGIN_TOPLEFT	0
#define WINDOWORIGIN_BOTTOMLEFT	1
		unsigned int dataType : 1;
#define DATATYPE_FBDEFAULT	0
#define DATATYPE_FBCOLOR 	1
		unsigned int rsvd : 4;
		unsigned int readDestinationEnable : 1;
		unsigned int readSourceEnable : 1;
		unsigned int pp2 : 3;
		unsigned int pp1 : 3;
		unsigned int pp0 : 3;
	} bits;
	int all;
} p2_fbreadmode_reg;

#define FBSOURCEOFFSET_REG	0x00008A88

#define FBPIXELOFFSET_REG	0x00008A90

#define FBDATA_REG	0x00008AA0
#define FBDATA_TAG	0x0154

#define FBWINDOWBASE_REG	0x00008AB0

#define FBWRITEMODE_REG		0x00008AB8
typedef union {
	struct {
		unsigned int rsvd1 : 28;
		unsigned int upLoadData : 1;
#define NOUPLOAD	0
#define UPLOADCOLORTOHOST	1
		unsigned int rsvd : 2;
		unsigned int writeEnable : 1;
	} bits;
	int all;
} p2_fbwritemode_reg;

#define FBHARDWAREWRITEMASK_REG		0x00008AC0

#define FBBLOCKCOLOR_REG		0x00008AC8

#define FBREADPIXEL_REG		0x00008AD0
#define FBREADPIXEL_8BITS	0
#define FBREADPIXEL_16BITS	1
#define FBREADPIXEL_32BITS	2
#define FBREADPIXEL_24BITS	4

#define FILTERMODE_REG		0x00008C00
typedef union {
	struct {
		unsigned int rsvd1 : 18;
		unsigned int statisticsData : 1;
		unsigned int statisticsTag : 1;
		unsigned int synchronizationData : 1;
		unsigned int synchronizationTag : 1;
		unsigned int colorData : 1;
		unsigned int colorTag : 1;
		unsigned int stencilData : 1;
		unsigned int stencilTag : 1;
		unsigned int depthData : 1;
		unsigned int depthTag : 1;
		unsigned int rsvd : 4;
	} bits;
	int all;
} p2_filtermode_reg;

#define STATISTICMODE_REG	0x00008C08
typedef union {
	struct {
		unsigned int rsvd : 26;
		unsigned int spans : 1;
#define SPANS_EXCLUDE	0
#define SPANS_INCLUDE	1
		unsigned int compareFunction : 1;
#define COMPAREFUNCTION_INSIDE	0
#define COMPAREFUNCTION_OUTSIDE	1
		unsigned int passiveSteps : 1;
#define PASSIVESTEPS_EXCLUDE	0
#define PASSIVESTEPS_INCLUDE	1
		unsigned int activeSteps : 1;
#define ACTIVESTEPS_EXCLUDE	0
#define ACTIVESTEPS_INCLUDE	1
		unsigned int statsType : 1;
#define STATSTYPE_PICKING	0
#define STATSTYPE_EXTENT	1
		unsigned int enableStats : 1;
	} bits;
	int all;
} p2_statisticmode_reg;

#define SYNC_REG		0x00008C40
#define SYNC_INT_EN	(1 << 31)

#define SUSPENDUNTILFRAMEBLANK_REG	0x00008C78

#define FBSOURCEBASE_REG	0x00008D80

#define FBSOURCEDELTA_REG	0x00008D88
#define SOURCEDELTA_YX(y, x) (((y) << 16) | ((x) & 0x0FFF))

#define CONFIG_REG		0x00008D90

#define YUVMODE_REG	0x00008F00
typedef union {
	struct {
		unsigned int rsvd : 26;
		unsigned int texelDisableUpdate : 1;
		unsigned int rejectTexel : 1;
#define PIXELSPERENTRY_1		0
#define PIXELSPERENTRY_2		1
#define PIXELSPERENTRY_4		2
		unsigned int testData : 1;
#define YUVTESTDATA_INPUT	0
#define YUVTESTDATA_OUTPUT	1
		unsigned int testMode : 2;
#define YUVTESTMODE_NO		0
#define YUVTESTMODE_PASS	1
#define YUVTESTMODE_FAIL	2
		unsigned int enable : 1;
	} bits;
	int all;
} p2_yuvmode_reg;

#define CHROMAMAPUPPERBOUND_REG		0x00008F08

#define CHROMAMAPLOWERBOUND_REG		0x00008F10

#define ALPHAMAPUPPERBOUND_REG		0x00008F18

#define ALPHAMAPLOWERBOUND_REG		0x00008F20

#define DELTAMODE_REG		0x00009300

#define DRAWTRIANGLE_REG		0x00009308

#define DRAWLINE01_REG		0x00009318

#define DRAWLINE10_REG		0x00009320


/************************************************
*
* 	Miscellaneous
*
 ************************************************/

#define RAD1_SECTOR_SIZE 128
#define SAVED_INFO_OFFSET (RAD1_EPROM_SIZE - (2 * RAD1_SECTOR_SIZE))
#define SAVED_INFO_MAX_INDEX ((RAD1_SECTOR_SIZE / sizeof(int)) - 1)
typedef struct
{
	int item[SAVED_INFO_MAX_INDEX + 1];
} per2_eprom_info;
#define RESOLUTION_INDEX 0


#define INTtoFIXED16_16(i) ((i) << 16)	/* int to 16.16 fixed format */
#define INTtoFIXED12_18(i) ((i) << 20)	/* int to 12.18 fixed format */
#define INTtoFIXED9_11(i) ((i) << 15)	/* int to 9.11 fixed format */

/* DMA Tag */
typedef struct {
	unsigned int address_tag : 9;
	unsigned int reserved : 5;
	unsigned int mode : 2;
	unsigned int data : 16;
} DMA_tag;
#define DMA_TAG_MODE_HOLD		0
#define DMA_TAG_MODE_INCREMENT	1
#define DMA_TAG_MODE_INDEXED	2

#define RAD1_DISABLE	0
#define RAD1_ENABLE		1
#define RAD1_FALSE		0
#define RAD1_TRUE		1

#define RAD1_WRITEMASK_ALL		0xFFFFFFFF
#define RAD1_WRITEMASK_RGB		0x00FFFFFF
#define RAD1_WRITEMASK_A		0xFF000000
#define RAD1_WRITEMASK_R		0x00FF0000
#define RAD1_WRITEMASK_G		0x0000FF00
#define RAD1_WRITEMASK_B		0x000000FF

#define RAD1_REFERENCE_CLOCK	14318180
#define RAD1_ACTUAL_PLL_FREQ(m,n,p)	((RAD1_REFERENCE_CLOCK * m)/(n * (1 << p)))
#define DEFAULT_83MHZ_MEMCLK /* */
#ifdef DEFAULT_83MHZ_MEMCLK
#define RAD1_DEFAULT_MEM_M		128
#define RAD1_DEFAULT_MEM_N		11
#define RAD1_DEFAULT_MEM_P		1
#else /* DEFAULT_83MHZ_MEMCLK */
/* 63MHz */
#define RAD1_DEFAULT_MEM_M		123
#define RAD1_DEFAULT_MEM_N		14
#define RAD1_DEFAULT_MEM_P		1
#endif /* DEFAULT_83MHZ_MEMCLK */
#define RAD1_DEFAULT_MEM_FREQ \
	RAD1_ACTUAL_PLL_FREQ(RAD1_DEFAULT_MEM_M,RAD1_DEFAULT_MEM_N,RAD1_DEFAULT_MEM_P)

#define WR_INDEXED_REGISTER(register_base, register, data) \
	*((register_base) + RDPALETTEWRITEADDRESS_REG/4) = register; \
	*((register_base) + RDINDEXEDDATA_REG/4) = data
#define WR_INDEXED_REGISTER_2V(register_base, register, data) \
	*((register_base) + RDINDEXLOW_2V_REG/4) = register; \
	*((register_base) + RDINDEXHIGH_2V_REG/4) = (register) >> 8; \
	*((register_base) + RDINDEXEDDATA_2V_REG/4) = data

#define WAIT_IN_FIFO(n) while (*(registers + INFIFOSPACE_REG/4) < n) delay(0)
#endif /* _RAD1HW_DEFS */

