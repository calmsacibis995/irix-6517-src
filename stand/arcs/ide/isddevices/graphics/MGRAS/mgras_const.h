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
#ifndef __MGRAS_CONST_H__

#define __MGRAS_CONST_H__

/*
 * XXX: These definitions do not belong here.
 *      If kernel header files have them , please remove them from here
 */
#ifndef KERNEL_HAS_DEFINED 

#define TLB_MODE_GTLB_ADD(x)  (((x) << 24) & 0xff)
#define TLB_MODE_COMPAT 0x400000  /* 22 bit */
#define TLB_MODE_GTLB_ADD_COMPAT(x) (TLB_MODE_GTLB_ADD((x)) | (TLB_MODE_COMPAT))

#define TLB_TAG_MASK 0x7ffff
#define TLB_PHYS_MASK 0x3fffff
#define TLB_TEST_WR_MASK 0x1f
#define TLB_TEST_RD_MASK 0x3fffff

#define HQ4_PHYS_ADDR_DATA      0
#define HQ4_TAG_DATA            1

#define HQ4_MFG_NUM					0x2aa
#define HQ4_PART_NUM				0xc003
#define HQ4_REV_NUM					1

#define LLP_MAXBURST				16
#define LLP_NULLTIMEOUT				(6 << 10)
#define LLP_MAXRETRY				(0x3ff << 16)

#define WIDGET_INDENT_DEF  (1 << 27) | (0xc003 << 11) | (0x2aa << 1) | 0x1
#define WIDGET_STAT_DEF 					0 
#define WIDGET_ERRHI_DEF 					0 
#define WIDGET_ERRLO_DEF 					0 
#define WIDGET_CNTL_DEF  					0x32f
#define WIDGET_REQ_TIMEOUT_DEF 				0xfffff
#define WIDGET_INTR_DST_HI_DEF 				0x0
#define WIDGET_INTR_DST_LO_DEF 				0x0
#define WIDGET_ERR_CMD_DEF 					0x0
#define WIDGET_LLP_CONFIG_DEF				(LLP_MAXBURST | LLP_NULLTIMEOUT | LLP_MAXRETRY)	
#define WIDGET_TARGET_FLUSH_DEF				0
#endif    /* KERNEL_HAD_THESE_DEFINED */

#ifndef PASSED
#define PASSED 1
#endif

#ifndef FAILED
#define FAILED 0
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define SIZE_DMA_WR_DATA 8*4 
#define SIZE_RGBA_DATA 8*4 

/* Texture output formats */
#define TEX_UNDEF_OUT   3
#define TEX_SHORT_OUT   2
#define TEX_RGBA8_OUT   1
#define TEX_RGBA12_OUT  0

/* Texture input formats */
#define TEX_DCB_IN      6
#define TEX_UNDEF_IN    5
#define TEX_RGBA16_IN   4
#define TEX_SHORT_IN    3
#define TEX_RGBA10_IN   2
#define TEX_ABGR8_IN    1
#define TEX_RGBA8_IN    0


#define TDMA_FIFO_SIZE 32
#define VDMA_FIFO_SIZE 32 

#define MAX_RTDMA_SIZE 0x1000



#ifndef _RDONLY_ 
#define _RDONLY_ 0
#endif

#ifndef _WRONLY_ 
#define _WRONLY_ 1
#endif

#ifndef _RDWR_ 
#define _RDWR_ 2
#endif

#define WALK_1_0_32_RSS 64

#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define	__paddr_t		__uint64_t
#else
#define	__paddr_t		__uint32_t
#endif

#define MGRAS_BASE0_PHYS        0x1F000000      /* HQ3 base addr */
#define MGRAS_PX_RE4_PHYS       0x1F040000
#define	MGRAS_MAXBOARDS		2    /* max. # MGRAS boards in system */

#define	MGRAS_DAC_CMD5_ADDR		0xd
#define         BD_VC3_RESET    	(1 << 5)
#define			BD_VC3_RESET_BIT	5
#define         DGTL_SYNC_OUT_BIT   3
#define         PIX_CLK_OSC_SEL_BIT 0

#ifdef DELAY			/* if sys/param.h is included first */
#undef DELAY
#endif
#define		DELAY 	us_delay

#undef BIT
#define BIT(n)  (0x1 << n)

#define RSS_BASE                ((__paddr_t)&(mgbase->rss) - (__paddr_t)mgbase)

#define CLOCKS  (1280 * 1024)

#define DL_POOL         4
#define GL_POOL         3
#define ERR_INDEX_ILLEGAL       ~0x0
#define DMABUFSIZE      517120
#define XEND    1344
#define YEND    1024


#define DFIFO_HW_BIT    0x20
#define DFIFO_LW_BIT    0x40


/* HQ Formatter/Converter Mode Register Values */
#define HQ_FC_NIBBLE                    (0 << 0)
#define HQ_FC_BYTE                      (1 << 0)
#define HQ_FC_SHORT                     (2 << 0)
#define HQ_FC_WORD                      (3 << 0)
#define HQ_FC_1_COMP_PER_PIXEL          (0 << 2)
#define HQ_FC_2_COMP_PER_PIXEL          (1 << 2)
#define HQ_FC_3_COMP_PER_PIXEL          (2 << 2)
#define HQ_FC_4_COMP_PER_PIXEL          (3 << 2)
#define HQ_FC_NIBBLE_EXPANSION_NONE     (2 << 8)
#define HQ_FC_ZERO_ALIGN_MASK           (1 << 10)
#define HQ_FC_INVALIDATE_TRAILING       (1 << 11)
#define HQ_FC_CANONICAL                 (HQ_FC_NIBBLE_EXPANSION_NONE | \
                                         HQ_FC_ZERO_ALIGN_MASK | \
                                         HQ_FC_INVALIDATE_TRAILING)


#define HQ3WRREGS_SIZE  31
#define HQ3RDREGS_SIZE  17
#define HQ3RWREGS_SIZE  14
#define HQ3_REIF_REGS_SIZE      4
#define HQ3_HAG_REGS_SIZE         20
#define HQ3_HAG_DWREGS_SIZE     5
#define HQ3UCODE_PAT_SIZE       26
#define         gio_be          0x1
#define         dcb_be          (1 << 1)
#define         ext_pipe        (1 << 2)
#define         hq_runs_tlb     (1 << 3)
#define RSS_DATA_MASK           0xffffffff
#define TLB_TEST_BIT    0x4000  /* bit 14 */
#define TLB_MASK        0x3fffff

/* CONFIG Reset conditions */
#define NUMGES  0x1
#define UCODE_STALL     0x8     /* bit 3 */
#define DIAG_GIO_CP     0x80    /* bit 7 */

/* GIO_CONFIG Reset conditions */
#define GIO_BIGENDIAN_BIT       0x1     /* bit 0 */
#define DCB_BIGENDIAN   0x2     /* bit 1 */
#define EXT_PIPEREG     0x4     /* bit 2 */
#define TLB_UPDATE      0x8     /* bit 3 */
#define HQFIFO_HW       0x1000
#define MST_W_BC_DAT    0x30000
#define MST_GNT_AS      0x180000
#define MST_R_DAT_AS    0x400000
#define MST_W_DAT_AS    0x6000000
#define MST_FREE_REQ    0x70000000

#define GIO_ID_ADDR     0x0
#define GIO_ID          0x00050190

#define   RSS_XFRCOUNTERS         (0x158 << 2)
#define   RSS_CHAR_HI             (0x70 << 2)
#define   RSS_CHAR_LO             (0x71 << 2)

#define   GE_DIAG_PASSED        0xcafe
#define   GE_DIAG_FAILED        0xbaadcafe
#define	  GE_READ_VAL		0x20000 /* bit 17 in FLAG register */

#define TIMEOUT 20000

#undef		HQ3_UCODERAM_LEN
#undef		MGRAS_UCODERAM_INITVAL
#define         HQ3_UCODERAM_LEN        (0x6000/4)
#define         MGRAS_UCODERAM_INITVAL  0x0

#define CFIFO_HW_BIT    0x8
#define CFIFO_LW_BIT    0x10
#define HOST_CP_BIT0    0x800000
#define CP_FLAG0        0x400

#define GE11_DIAG_REG_BASE      0x40000
#define RAMSCAN_BASE            0x41000
#define OFIFO_0                 0x42000
#define OFIFO_1                 0x42800
#define OFIFO_2                 0x43000
#define NUM_OF_DIAG_REG         15

#define GE11_REGS_SIZE 4

#define GE_UCODE_64RAM          GE11_UCODE_SIZE
#define GE_UCODE_32RAM          (GE11_UCODE_SIZE >> 2)
#define HQBUS_BYPASS_BIT 0x40
#define RAMSCAN_CNTROL_MASK     0x7ffff

#define HQ_GIOCFG_HQBUS_BYPASS_BIT      (1 <<  6)

#define EXECUTION_CONTROL_REGISTER      0x40000
#define OFIFO_WM        0x68
#define GO_BIT  BIT(0)
#define UCODE_LAT       BIT(4)
#define GE11_DIAG_READ  BIT(18)


#define         CMAP_RAM_MASK   0x1fff

#define         MGRAS_TIME_OUT          10000
#define         HQ_STATUS_VBLANK        (1 << 2)

#define         HQ_MOD                  0x0
#define         RE_MOD                  0x1
#define         BE_MOD                  0x2

#define         READ_MODE               0x0
#define         READ_VFY_MODE           0x1


#define		LINEAR_RAMP		0x0
#define		RAMP_FLAT_0		0x1
#define		RAMP_FLAT_5		0x2
#define		RAMP_FLAT_A		0x3
#define		RAMP_FLAT_F		0x4
#define		RAMP_TYPES		0x4

#define		RGB_MODE		0x0
#define		COLOR_INDEX_MODE	0x1
#define		PIXEL_MODE_TYPES	0x1


#define DIBPOINTER_REG                  0x0
#define DIBTOPSCAN_REG                  0x4
#define DIBSKIP_REG                     0x8
#define DIBCTL0_REG                     0xC
#define DIBCTL1_REG                     0x10
#define DIBCTL2_REG                     0x14
#define DIBCTL3_REG                     0x18
#define RETRY_REG                       0x1C
#define REFCTL_REG                      0x20

#define REF_RAC_CTL                     0x0
#define RE_RAC_DATA                     0x4

#define         VC3_VT_FRAME_TBL_PTR                    8
#define         VC3_VT_LINE_SEQ_PTR                     9
#define         VC3_VT_LINES_RUN                        0xa
#define         VC3_CURS_TBL_PTR                        0xc
#define         VC3_WORKING_CURS_Y                      0xd
#define         VC3_UNUSED                              0xf
#define VC3_RAM_MASK    0x7ffe          /* Last 16 Bit Entry in the VC3 Sram */

#define DATA16          0

#define DIBPOINTER_REG                  0x0
#define DIBTOPSCAN_REG                  0x4
#define DIBSKIP_REG                     0x8
#define DIBCTL0_REG                     0xC
#define DIBCTL1_REG                     0x10
#define DIBCTL2_REG                     0x14
#define DIBCTL3_REG                     0x18
#define RETRY_REG                       0x1C
#define REFCTL_REG                      0x20

#define REF_RAC_CTL                     0x0
#define RE_RAC_DATA                     0x4

#define         VC3_VT_FRAME_TBL_PTR                    8
#define         VC3_VT_LINE_SEQ_PTR                     9
#define         VC3_VT_LINES_RUN                        0xa
#define         VC3_CURS_TBL_PTR                        0xc
#define         VC3_WORKING_CURS_Y                      0xd
#define         VC3_UNUSED                              0xf

/*	CRS	0	*/
#define		XMAP_PP1_SELECT				0x00

/*	CRS	1	*/
#define		XMAP_INDEX				0x10

/*	CRS	2	*/
#define		XMAP_CONFIG				0x20
#define		XMAP_REV				0x24
#define		XMAP_FIFO_CAP				0x28

/*	CRS	3	*/
#define		XMAP_BUF_SELECT				0x30
#define		XMAP_OVL_BUF_SELECT			0x34

/*	CRS	4	*/
#define		XMAP_MAIN_MODE				0x40

/*	CRS	5	*/
#define		XMAP_OVERLAY_MODE			0x50

/*	CRS	6	*/
#define		XMAP_DIB_PTR				0x60
#define		XMAP_DIB_TOPSCAN			0x64
#define		XMAP_DIB_SKIP				0x68
#define		XMAP_DIB_CTRL0				0x6C
#define		XMAP_DIB_CTRL1				0x610
#define		XMAP_DIB_CTRL2				0x614
#define		XMAP_DIB_CTRL3				0x618


/*	CRS	7	*/
#define		XMAP_RE_RAC_CTRL			0x70
#define		XMAP_RE_RAC_DATA			0x74


#define		READ_ONLY		1
#define		WRITE_ONLY		2
#define		READ_WRITE		3

#define         CMAP_REV_F              0x4
#define		CMAP_REV_D		0x2
#define		CMAP_REV_E		0x3

#define         CMAP0_TEST                              0
#define         CMAP0_ADDR_BUS_TEST                     1
#define         CMAP0_DATA_BUS_TEST                     2
#define         CMAP0_ADDR_UNIQ_TEST                    3
#define         CMAP0_PATRN_TEST                        4

#define         CMAP1_TEST                              5
#define         CMAP1_ADDR_BUS_TEST                     6
#define         CMAP1_DATA_BUS_TEST                     7
#define         CMAP1_ADDR_UNIQ_TEST                    8
#define         CMAP1_PATRN_TEST                        9

#define         CMAP_UNIQ_TEST                          10

#define         VC3_TEST                                11
#define         VC3_ADDR_BUS_TEST                       12
#define         VC3_DATA_BUS_TEST                       13
#define         VC3_ADDR_UNIQ_TEST                      14
#define         VC3_PATRN_TEST                          15
#define         VC3_REG_TEST                            16
#define		VC3_SRAM_64				17

#define         DAC_CLR_PLLT_WALK_BIT_TEST              18
#define         DAC_CLR_PLLT_ADDR_UNIQ_TEST             19
#define         DAC_CLR_PLLT_PATRN_TEST                 20
#define         DAC_CONTROL_REG_TEST                    21
#define         DAC_MODE_REG_TEST                       22
#define         DAC_ADDR_REG_TEST                       23

#define         MGRAS_DAC_RED_SIG       0x10
#define         MGRAS_DAC_GRN_SIG       0x11
#define         MGRAS_DAC_BLU_SIG       0x12
#define         MGRAS_DAC_MISC_SIG      0x13

#define         XMAP0_DCB_REG_TEST                      24
#define         XMAP1_DCB_REG_TEST                      25
#define         XMAP2_DCB_REG_TEST                      26
#define         XMAP3_DCB_REG_TEST                      27
#define         XMAP4_DCB_REG_TEST                      28
#define         XMAP5_DCB_REG_TEST                      29
#define         XMAP6_DCB_REG_TEST                      30
#define         XMAP7_DCB_REG_TEST                      31

#define		PIXEL_PATH_PATH_TEST			32

#define		RE4_STATUS_REG_TEST			0
#define		RE4_RDWR_REGS_TEST			1
#define		RE4_RAC_REG_TEST			2	
#define		RE4_FIFOS_TEST				3
#define		RE4_INTERNALRAM_TEST			4
#define		RE4_COLOR_TRI_TEST			5
#define		RE4_NOTEX_TEST				6
#define		RE4_PPMODES_TEST			7
#define		RE4_TEX_TEST 				8
#define		RE4_FOG_TEST				9
#define		RE4_SCRNMSk_TEST			10
#define		RE4_AA_TEST				11
#define		RE4_CHARS_TEST				12
#define		RE4_POLYSTIP_TEST			13
#define		RE4_LINES_TEST				14
#define		RE4_SCISSOR_TEST			15
#define		RE4_POINTS_TEST				16
#define		RE4_Z_TRI_TEST				17
#define		RE4_XPOINT_TEST				18
#define		RE4_XBLK_TEST				19
#define		RE4_WRONLYREGS_TEST			20
#define		RE4_ALPHA_BLEND_TEST			21
#define		LAST_RE_ERRCODE				22

#define		TE1_REV_TEST				0
#define		TE1_RDWRREGS_TEST			1
#define		TE1_VIDEO_TEST				2
#define		TE1_CNTXSW_TEST				3
#define		TE1_LOD_TEST				4
#define		TE1_LDSMPL_TEST				5
#define		TE1_DETAIL_TEX_TEST			6
#define		TE1_LUT_TEST				7
#define		TE1_WRONLYREGS_TEST			8
#define		TE1_TEX_POLY_TEST			9	
#define		TE1_NO_TEX_POLY_TEST			10	
#define		TEX_8KHIGH_TEST				11	
#define		TEX_8KWIDE_TEST				12	
#define		TEX_DETAIL_TEST				13	
#define		TEX_MAG_TEST				14	
#define		TEX_PERSP_TEST				15	
#define		TEX_LINEGL_TEST				16	
#define		TEX_SCISTRI_TEST			17	
#define		TEX_LOAD_TEST				18	
#define		TEX_LUT4D_TEST				19	
#define		TEX_MDDMA_TEST				20	
#define		TEX_TEX1D_TEST				21	
#define		TEX_TEX3D_TEST				22	
#define		TEX_TEREV_TEST				23
#define		LAST_TE_ERRCODE				23	

#define		TRAM_REV_TEST				0
#define		TRAM_DMA_TEST				1
#define		LAST_TRAM_ERRCODE			2

#define		RDRAM_READ_REG_TEST			0
#define		RDRAM_ADDRBUS_TEST   			1
#define		RDRAM_DATABUS_TEST   			2
#define		RDRAM_UNIQUE_TEST			3
#define		RDRAM_PIO_MEM_TEST			4
#define		RDRAM_DMA_MEM_TEST			5
#define		LAST_RDRAM_ERRCODE			6

#define		PP1_RACREG_TEST				0
#define		PP1_OVERLAY_TEST			1
#define		PP1_CLIPID_TEST				2
#define		PP1_STENCIL_TEST			3
#define		PP1_PP_DAC_TEST				4
#define		PP1_LOGICOP_TEST			5
#define		PP1_DITHER_TEST				6
#define		LAST_PP_ERRCODE				7


#define         BLANK   {"", ""}

#define HQ3_GIOBUS_TEST			0
#define HQ3_HQBUS_TEST			1
#define HQ3_REGISTERS_TEST		2	
#define HQ3_TLB_TEST			3	
#define HQ3_REIF_CTX_TEST		4	
#define HQ3_HAG_CTX_TEST		5	
#define HQ3_UCODE_DATA_BUS_TEST		6	
#define HQ3_UCODE_ADDR_BUS_TEST		7	
#define HQ3_UCODE_PATTERN_TEST		8	
#define HQ3_UCODE_ADDRUNIQ_TEST		9
#define	HQ3_RSS_DATABUS_TEST		10
#define	HQ3_CP_TEST			11
#define	HQ3_CFIFO_TEST			12
#define	HQ3_CONV_32BITS_TEST		13
#define	HQ3_CONV_16BITS_TEST		14
#define	HQ3_CONV_8BITS_TEST		15
#define HQ3_CONV_STUFF_TEST		16
#define HQCP_GE_REBUS_HQ_TEST		17
#define HQCP_GE_REBUS_RE_TEST		18
#define HOST_HQ_DMA_TEST		19
#define HOST_HQ_CP_DMA_TEST		20

#define GE11_UCODE_DATA_BUS_TEST		0	
#define GE11_UCODE_ADDR_BUS_TEST		1	
#define GE11_UCODE_PATTERN_TEST			2	
#define GE11_UCODE_ADDRUNIQ_TEST		3	
#define GE11_UCODE_WALKING_BIT_TEST		4	
#define GE11_CRAM_TEST				5
#define GE11_ERAM_TEST				6
#define GE11_WRAM_TEST				7
#define GE11_ALU_TEST				8
#define GE11_DREG_TEST				9
#define GE11_INST_TEST				10
#define GE11_DMA_TEST				11
#define GE11_AALU3_TEST				12
#define GE11_MUL_TEST				13
#define GE11_AREG_TEST				14
#define GE11_UCODE_SERVER			15
#define GE11_UCODE_DNLOAD			16


#define	DMA_HQPIO_REDMA_PIO_BURST_TEST		 0
#define	DMA_HQPIO_REDMA_IND_BURST_TEST		 1
#define	DMA_HQPIO_REDMA_PIO_PIO_TEST		 2
#define	DMA_HQPIO_REDMA_IND_PIO_TEST		 3
#define	DMA_HQPIO_REDMA_PIO_IND_TEST		 4

#define	DMA_HQ_RE_PP1_DMA_BURST_TEST		 5
#define	DMA_HQ_RE_PP1_PIO_PIO_TEST		 6
#define	DMA_HQ_RE_PP1_PIO_IND_TEST		 7
#define	DMA_HQ_RE_PP1_BURST_BURST_TEST		 8
#define	DMA_HQ_RE_PP1_PIO_BURST_TEST		 9
#define	DMA_HQ_RE_PP1_IND_BURST_TEST		10

#define	DMA_HQ_RE_TRAM_DMA_TEST			11
#define	DMA_HQ_RE_TRAM_PIO_TEST			12

#define DMA_HQPIO_REDMA_IND_IND_TEST		13

#define DMA_HQ_RE_TRAM_BURSTWR_DMAPIORD_TEST	14	

#define	HQ4_REG_TEST				0

#define FRAME_BOTTOM    0x0
#define         VC3_VT_FRAME_TBL_PTR                    8
#define         VC3_VT_LINE_SEQ_PTR                     9


#define MGRAS_CMAP_NCOLMAPENT   8192

#define VC3_RAMSIZE  	8192

#define 	CURS_TIMING_TABLE	3
#define 	MDID_DIDs_TIMING       0x4
#define 	VIDEO_DIDs_TIMING       0x5

#define		_1600_1200_60		0x159
#define		_1600_1200_60_SI	0x156	/* For Solid Impact */
#define		_1280_1024_76		0x139
#define		_1280_1024_72		0x130
#define		_1280_1024_60		0x107
#define		_1280_1024_50		0x88
#define		_1280_492_120ST		0x1074
#define		_1280_1024_30I		0x54
#define		_1024_768_60		0x64

#define		_1024_768_60p		0x58
#define		_1024_768_100ST		0x108
#define		_1920_1035_60FI		0x149
#define		_1280_959_30RS343	0x19
#define		_CMAP_SPECIAL		0x555

#define		NTSC_VCO_VAL		0x195
#define		PAL_VCO_VAL			0x236

#define MDID_DIDs       0x1
#define ODID_DIDs       0x2
#define REGULAR_DIDs    0x3

#define VIDEO1_DIDs    	0x4
#define VIDEO2_DIDs    	0x5
#define VIDEO3_DIDs    	0x6
#define VIDEO4_DIDs    	0x7


#define		DAC_ID_OLD		0x76
#define		DAC_REVISION_OLD	0x0 

#define		DAC_ID			0x79
#define		DAC_REVISION		0x1 
#define		DAC_STATUS		

#define         Cmap0           0x0
#define         Cmap1           0x1
#define         CmapAll         0x2

#define         Linear0         0xaad
#define         Linear1         0xbad
#define         LinearAll       0xdad
#define         DFB_COLORBARS   0xdfb

/* defines for Gold Crc Indices */
#define	RED_TRI_CRC_INDEX			0
#define	GRN_TRI_CRC_INDEX			1
#define	BLU_TRI_CRC_INDEX			2
#define	ALP_TRI_CRC_INDEX			3
#define	ZTRI_S1T0_CRC_INDEX			4
#define	ZTRI_S1T1_CRC_INDEX			5
#define	ZTRI_S1T2_CRC_INDEX			6
#define	ZTRI_S1T3_CRC_INDEX			7
#define	ZTRI_S1T4_CRC_INDEX			8
#define	ZTRI_S1T5_CRC_INDEX			9
#define	ZTRI_S1T6_CRC_INDEX			10
#define	ZTRI_S1T7_CRC_INDEX			11
#define	ZTRI_S2T0_CRC_INDEX			12
#define	ZTRI_S2T1_CRC_INDEX			13
#define	ZTRI_S2T2_CRC_INDEX			14
#define	ZTRI_S2T3_CRC_INDEX			15
#define	ZTRI_S2T4_CRC_INDEX			16
#define	ZTRI_S2T5_CRC_INDEX			17
#define	ZTRI_S2T6_CRC_INDEX			18
#define	ZTRI_S2T7_CRC_INDEX			19
#define	POINTS_CRC_INDEX			20
#define	LINES_CRC_INDEX				21
#define	POLYSTIP_CRC_INDEX			22
#define	XBLK_CRC_INDEX				23
#define	CHARS_CRC_INDEX				24
#define	TE1_TEX_POLY_CRC_INDEX			25
#define	TE1_NO_TEX_POLY_CRC_INDEX		26
#define	TEX_TEX3D_CRC_INDEX			27
#define	TEX_TEX1D_CRC_INDEX			28
#define	TEX_LOAD_CRC_INDEX			29
#define	TEX_SCISTRI_CRC_INDEX			30
#define	TEX_LINEGL_CRC_INDEX			31
#define	TEX_PERSP_CRC_INDEX			32
#define	TEX_MAG_CRC_INDEX			33
#define	TEX_BORDERTALL_CRC_INDEX		34
#define	TEX_BORDERWIDE_CRC_INDEX		35
#define	TEX_LUT4D_CRC_INDEX			36
#define	TEX_MDDMA_CRC_INDEX			37
#define	LOGICOP_CRC_INDEX			38
#define	DITHER_CRC_INDEX			39
#define	NOTEX_LINE_CRC_INDEX			40	
#define	TEX_DETAIL_CRC_INDEX			41

#define	LAST_CRC_INDEX				41
#define	ILLEGAL_CRC_INDEX			-1


#define		CROSS_HAIR	2
#define		GLYPH_64	1
#define		GLYPH_32	0
#define		DIAG_CURS	3

#define CSIM_RSS_DEFAULT    0x0

/* RE4 Status register bits */
#define RE4COUNT_BITS   0x3
#define RE4NUMBER_BITS  0xc
#define RE4VERSION_BITS 0xf0
#define RE4CMDFIFOEMPTY_BITS 0x100
#define RE4BUSY_BITS 0x200
#define RE4PP1BUSY_BITS 0x400

#define MAXRSSNUM	4
#define PP_PER_RSS	2
#define RDRAM_PER_PP	3

/* MSByte definitions */
#define PP_RAC_CONTROL0	8
#define PP_RAC_CONTROL2	0xa	
#define RE_RAC_CONTROL0	4	
#define RE_RAC_CONTROL1	5
#define RDRAM2_REG	2
#define RDRAM0_MEM	4	
#define RDRAM2_MEM	6

/* defines for read/write functions */
#define WRITE 1
#define READ  0

/* texture defines */
#define NUMTRAM_BITS 0x600000

/* min and max memory addresses */
#define RDRAM_MEM_MIN 	0x4000000
#define RDRAM_MEM_MAX	0x61ffffe
#define MEM_HIGH	0x1ffffe

/* defines for rdram_checkargs */
#define MEM 0
#define REG 1

/* defines for rss functionality */
#define PIX_COLOR_SCALE 0xfff

/* defines for RDRAM framebuffer tests */
#define HALFWORD		18 /* 18 bits, for 9-bit bytes */
#define RD_MEM_BROADCAST_WR	0x7000000
#define PIXEL_SUBTILE_SIZE      12

/* additional device address defines */
#define TEX_LUT		0x90000000
#define POLYSTIP_RAM	0xa0000000
#define AA_TABLE	0xb0000000
#define ADDRESS_RAM_HI	0xc0000000
#define ADDRESS_RAM_LO 	0xd0000000
#define ZFIFO_A		0xe0000000
#define ZFIFO_B		0xf0000000

/* temp RDRAM/RAC defines until we get them into a header file */

/* RDRAM registers */
#define RD_DEVICE_TYPE  0x000000
#define RD_DEVICE_ID    0x000004
#define RD_DELAY        0x000008
#define RD_MODE         0x00000C
#define RD_REF_INTERVAL 0x000010
#define RD_REF_ROW      0x000014
#define RD_RAS_INTERVAL 0x000018
#define RD_MIN_INTERVAL 0x00001C
#define RD_ADDR_SELECT  0x000020
#define RD_DEVICE_MANUF 0x000024
#define RD_DUMP         0x0000d0
#define RD_ROW          0x000200

/* RAC Registers */
#define RAC_CONTROL     0x8000000

/* RAC_CONTROL register bits with reset values */
#define STOPT           0x1             /* 0            */
#define STOPR           0x2             /* 0            */
#define PWRUP           0x4             /* 1            */
#define EXTBE           0x8             /* 0            */
#define SOUTI           0x10            /* 0            */
#define RESET           0x20            /* 1            */
#define CCT1LD          0x40            /* 0 ? or load at reset */
#define CCT1EN          0x80            /* 1            */
#define CCTLL           0x3f00          /* 000000       */
#define SCANIN          0x4000          /* 0            */
#define SCANEN          0x8000          /* 0            */
#define SCANMODE        0x10000         /* 0            */
#define SCANCLK         0x20000         /* 0            */
#define BISTMODE        0x40000         /* 0            */
#define BYPASS          0x80000         /* 0            */
#define NACKErr         0x100000        /* 0            */
#define PREINIT         0x200000        /* 1            */
#define FORCEBE         0x400000        /* 0            */
#define RXSEL           0x3c00000       /* 0100         */
#define TXSEL           0x3c000000      /* 0100         */


/* Bit mask defs */
#define SIXTYFOURBIT_MASK       0xffffffffffffffff
#define THIRTYTWOBIT_MASK       0xffffffff
#define THIRTYONEBIT_MASK       0x7fffffff
#define THIRTYBIT_MASK          0x3fffffff
#define TWENTYNINEBIT_MASK      0x1fffffff
#define TWENTYEIGHTBIT_MASK     0xfffffff
#define TWENTYSIXBIT_MASK       0x3ffffff
#define TWENTYFIVEBIT_MASK      0x1ffffff
#define TWENTYFOURBIT_MASK      0xffffff
#define TWENTYTHREEBIT_MASK     0x7fffff
#define TWENTYTWOBIT_MASK     	0x3fffff
#define TWENTYONEBIT_MASK       0x1fffff
#define TWENTYBIT_MASK          0xfffff
#define NINETEENBIT_MASK        0x7ffff
#define EIGHTEENBIT_MASK        0x3ffff
#define SEVENTEENBIT_MASK       0x1ffff
#define SIXTEENBIT_MASK         0xffff
#define FOURTEENBIT_MASK        0x3fff
#define THIRTEENBIT_MASK        0x1fff
#define TWELVEBIT_MASK          0xfff
#define ELEVENBIT_MASK          0x7ff
#define TENBIT_MASK             0x3ff
#define NINEBIT_MASK            0x1ff
#define EIGHTBIT_MASK           0xff
#define SEVENBIT_MASK           0x7f
#define SIXBIT_MASK             0x3f
#define FIVEBIT_MASK            0x1f
#define FOURBIT_MASK            0xf
#define ONEBIT_MASK             0x1

#define NO_PRINT_OUT    0x98765432


/* from gfx/lib/opengl/MG0/render.h */

/*
** The distinction between __GL_SHADE_SMOOTH and __GL_SHADE_SMOOTH_LIGHT is
** simple.  __GL_SHADE_SMOOTH indicates if the polygon will be smoothly
** shaded, and __GL_SHADE_SMOOTH_LIGHT indicates if the polygon will be
** lit at each vertex.  Note that __GL_SHADE_SMOOTH might be set while
** __GL_SHADE_SMOOTH_LIGHT is not set if the lighting model is GL_FLAT, but
** the polygons are fogged.
*/
#define __GL_SHADE_RGB          0x0001
#define __GL_SHADE_SMOOTH       0x0002 /* smooth shaded polygons */
#define __GL_SHADE_DEPTH_TEST   0x0004
#define __GL_SHADE_TEXTURE      0x0008
#define __GL_SHADE_STIPPLE      0x0010 /* polygon stipple */
#define __GL_SHADE_STENCIL_TEST 0x0020
#define __GL_SHADE_DITHER       0x0040
#define __GL_SHADE_LOGICOP      0x0080
#define __GL_SHADE_BLEND        0x0100
#define __GL_SHADE_ALPHA_TEST   0x0200
#define __GL_SHADE_TWOSIDED     0x0400
#define __GL_SHADE_MASK         0x0800

/* Two kinds of fog... */
#define __GL_SHADE_SLOW_FOG     0x1000
#define __GL_SHADE_CHEAP_FOG    0x2000

/* do we iterate depth values in software */
#define __GL_SHADE_DEPTH_ITER   0x4000

#define __GL_SHADE_LINE_STIPPLE 0x8000

#define __GL_SHADE_CULL_FACE    0x00010000
#define __GL_SHADE_SMOOTH_LIGHT 0x00020000 /* smoothly lit polygons */

/* from vertex.h */
#define __GL_FRONTFACE         0


/* defines for TEXMODE2 reg */
#define TEXMODE2_3DITER_EN	1
#define TEXMODE2_WARP_ENABLE	1
#define TEXMODE2_RACP		1
#define TEXMODE2_SACP		1
#define TEXMODE2_TACP		1

/* defines for TXSIZE reg */
#define TXSIZE_US_SSIZE		1
#define TXSIZE_US_TSIZE		1
#define TXSIZE_US_RSIZE		1

 /* from attrib.h */
#define __GL_FOG_ENABLE 	(1 << 5)

#define DBLBIT 0x200

#define	CFIFO_TIME_OUT			1
#define	CFIFO_MAX_LEVEL			40
#define	CFIFO_MIN_LEVEL			8
#define	CFIFO_DELAY_VAL			64
#define CFIFO_TIMEOUT_VAL		0xff0000
#define CFIFO_COUNT			32


/* defines for verif porting -- taken from gfx/include/opengl/glconsts.h */
#define GL_RGB                              0x1907
#define GL_RGBA                             0x1908
#define GL_LUMINANCE                        0x1909
#define GL_LUMINANCE_ALPHA                  0x190A
#define GL_UNSIGNED_SHORT                   0x1403
#define GL_UNSIGNED_BYTE                    0x1401
#define GL_UNSIGNED_SHORT_4_4_4_4_EXT       0x8033

/* defines for texture TE revision test */
#define TE_REV_D_CHECKSUM	0x2aa70d210ull
#define TE_REV_B_1TRAM_CHECKSUM	0x0bdfc48fcull
#define TE_REV_B_4TRAM_CHECKSUM	0x236379000ull
#define TE_REV_UNDETERMINED    	0
#define TE_REV_B        	1
#define TE_REV_D        	2


/* defines to convert this to compile in ide */
#define GLushort        ushort_t
#define GLubyte         char
#define GLint           __int32_t
#define GLenum          __uint32_t
#define GLuint          __uint32_t
#if defined(_STANDALONE)
#define stderr          VRB
#define fprintf         msg_printf
#define exit            return
#endif /* _STANDALONE */

#define HQ_FC_ADDR_PIXEL_FORMAT_MODE            0xc00
#define page_4k         0
#define MGRAS_DMA_TIMEOUT_COUNT         0xf000
/*
 * Register Addresses
 */
#define REIF_SCAN_WIDTH                 0xa02
#define REIF_LINE_COUNT                 0xa03
#define REIF_DMA_TYPE                   0xa06
#define REIF_DMA_CTRL                   0xa05

#define RSS_PIXCMD_PP1                  0x160
#define RSS_FILLCMD_PP1                 0x161
#define RSS_COLORMASK_MSB_PP1           0x162
#define RSS_COLORMASK_LSBA_PP1          0x163
#define RSS_COLORMASK_LSBB_PP1          0x164
#define RSS_DRBPTRS_PP1                 0x16d
#define RSS_DRBSIZE_PP1                 0x16e
#define RSS_MBUFSEL_PP1                 0x17e
#define RSS_MMODE_PP1                   0x16e
#define RSS_DIBCTRL0_PP1                0x16e
#define RSS_DIBCTRL1_PP1                0x16e
#define RSS_DIBCTRL2_PP1                0x16e
#define RSS_DIBCTRL3_PP1                0x16e

#define HQ_RSS_ADDR_CSIM_SCAN_WIDTH     0x30
#define HQ_RSS_ADDR_CSIM_FB_XY          0x33
#define HQ_RSS_ADDR_CSIM_FB_DEPTH       0x31
#define HQ_RSS_ADDR_CSIM_FB_SIZE        0x32

/*
 * HQ constants
 */
#define HQ_CD_ADDR_CD_FLAG_SET          0xe04
#define HQ_FLAG_CD_FLAG_BIT             (1 << 16)
#define HQ_GIOCFG_HQ_UPDATES_TLB_BIT    (1 << 3)

/*
 * HQ busy register
 */
#define HQ_SYNC_BUSY			1	
#define HIF_BUSY			(1 << 1)
#define CFIFO_BUSY			(1 << 2)
#define CD_BUSY				(1 << 3)
#define REIF_BUSY			(1 << 4)
#define	HQ_BUSY_MSTR_DMA_BIT		(1 << 9)
#define HQ_BUSY_REIF_DMA_ACTV_BIT	(1 << 11)
#define	HQ_BUSY_HAG_DMA_BIT		(1 << 12)

#define	DMA_TIMED_OUT			0x1

/*
 * Kind of DMA for HQ3
 */
#define	DMA_OP				0x0
#define	PIO_OP				0x1

/*
 * DMA Transfer Device Info
 */
#define	DMA_PP1				0x0
#define	DMA_TRAM			0x1
#define	DMA_TRAM_REV			0x2

/*
 * Kind of DMA (Burst/PIO) Info
 */
#define	DMA_BURST			0x0
#define	DMA_PIO				0x1
#define	INDIRECT_ADDR			0x2

/*
 * DMA Operational Type (Read/Write)
 */
#define	DMA_WRITE			0x0
#define	DMA_READ			0x1

/*
 * PP1 Related Constants
 */
#define	DMA_PP1_A			0
#define	DMA_PP1_B			1
#define	DMA_PP1_AB			2

/* 
 * HQ3 DMA Constants
 */
#define DMA_IM1_POOL                    0
#define DMA_IM2_POOL                    1
#define DMA_IM3_POOL                    2
#define DMA_GL_POOL                     3
#define DMA_DL_POOL                     4

/*
 * Host <-> HQ/CP DL DMA Constants
 */
#define	Y_DEC_FACTOR_MASK	((1 << 17) - 1)
#define DLIST0_ADDR     	0x100
#define DLIST0_SIZE     	3

#define DLIST1_ADDR     	0x1060
#define	DLIST1_SIZE		128

#define	DFIFO_WR	0xe6
#define	DFIFO_RD	0xe5

#define PAGE_SIZE       	page_4k
#define BYTESPERPAGE(x)    	(1 << ((x) + 12))
#define	PAGE_BIT_SIZE		12

#define PAT_NORM	0
#define PAT_INV		1

/* TRAM memory patterns */
#define TRAM_WALK_0_1_TEST	0
#define TRAM_PAGE_ADDR_TEST	1
#define TRAM_FF00_TEST		2
#define TRAM_PATTERN_TEST	3
#define TRAM_PATTERN_TEST_0	30
#define TRAM_PATTERN_TEST_1	31
#define TRAM_PATTERN_TEST_2	32
#define TRAM_PATTERN_TEST_3	33
#define TRAM_PATTERN_TEST_4	34
#define TRAM_PATTERN_TEST_5	35
#define TRAM_PATTERN_TEST_6	36
#define TRAM_PATTERN_TEST_7	37
#define TRAM_PATTERN_TEST_8	38
#define TRAM_PATTERN_TEST_9	39
#define TRAM_A5A5_TEST		4
#define TRAM_TEX_LINE_PAT	5
#define TRAM_ALL_PATTERNS	100
#define TRAM_VERIF_8KHIGH	6
#define TRAM_VERIF_8KWIDE	7	
#define TRAM_VERIF_DETAIL	8	
#define TRAM_VERIF_MAG		9	
#define TRAM_VERIF_PRSPCTV	10
#define TRAM_VERIF_LINE_GL_SC	11
#define TRAM_VERIF_SCIS_TRI	12
#define TRAM_VERIF_LOAD		13
#define TRAM_VERIF_LUT4D	14
#define TRAM_VERIF_MDDMA	15
#define TRAM_VERIF_TEX1D	16
#define TRAM_VERIF_TEX3D	17

/* PP DMA memory patterns */
#define PP_WALK_0_1_TEST	0
#define PP_PAGE_ADDR_TEST	1
#define PP_FF00_TEST		2
#define PP_PATTERN_TEST		3
#define PP_PATTERN_TEST_0	30
#define PP_PATTERN_TEST_1	31
#define PP_PATTERN_TEST_2	32
#define PP_PATTERN_TEST_3	33
#define PP_PATTERN_TEST_4	34
#define PP_PATTERN_TEST_5	35
#define PP_PATTERN_TEST_6	36
#define PP_PATTERN_TEST_7	37
#define PP_PATTERN_TEST_8	38
#define PP_PATTERN_TEST_9	39
#define PP_A5A5_TEST		4
#define PP_ALL_PATTERNS		100

#define PP_RGBA8888		0x88888888
#define PP_RGB12		0xcccccccc

#define	WALKTIME		0x4000
#define RGBA8888        	1
#define RGBA4444        	2
#define RGBA5551        	3
#define RGB121212       	4
#define RGBA9999        	5
#define CI12            	6
#define CI8             	7
#define ZST32           	8

#define points_array_checksum		0x002fd4fbe7cull
#define block_array_checksum		0x17f3fffff40ull
#define notex_line_checksum		0x03fffffffc0ull
#define tex_line_checksum		0x01a21247dbcull
#define telod_128_checksum		0x034979f2eccull
#define telod_128_4tram_checksum	0x034424b0d7full
#define telod_32_checksum		0x00d1111107cull
#define logicop_array_checksum		0x188eecb0
#define dither_array_checksum		0x00d06000d06ull
#define notex_poly_array_checksum	0x639c0
#define tex_poly_array_checksum		0x63af6afae79ull

#define __GL_X_MAJOR    0 /* from MG0/include/render.h */
#define __GL_LINE_SMOOTH_ENABLE  (1 <<  9)

#define TE_ID_BITS      0x6000000
#define TE_COUNT_BITS   0x1800000
#define TE_VERSION_BITS 0xf

#define MGRAS_BC1_FORCE_DCB_STAT_BIT        BIT(1)
#define MGRAS_BC1_VC3_RESET_BIT             BIT(5)
/* #define MGRAS_BC1_DEFAULT                   (MGRAS_BC1_VC3_RESET_BIT) */

#define GE11_UCODE_MASK         0xffffffff
#define GE11_UCODE2_MASK        0xff
#define GE11_UCODE_SIZE         64*1024 /* should be 256 * 1024 */
#define GE11_DIAG_READ          BIT(18)
#define GE11_DIAGSEL_BPSTALL    BIT(5)
#define GE11_DIAGSEL_INPUT      BIT(8)
#define GE!!_REG_READ           BIT(31)

#define OFIFO_WM        0x68
#define DIAG_MODE       BIT(0)
#define ROUTE_RE_2_HQ	BIT(8)
#define	GE_READ		BIT(17)

#define RAMSCAN_FASTMODE	BIT(17)
#define	RAMSCAN_SCANMODE	BIT(18)

#define WSYNC_FIFO_DISABLE	BIT(0)
#define WSYNC_FIFO_WM		0x10

#define GE11_RAMSCAN_BASE	0x41000

#define EXECUTION_CONTROL       0x40000
#define BREAKPT_ADDR            0x40004
#define RAMSCAN_CONTROL         0x40008
#define WRT_SYNC_FIFO_CONTROL   0x4000c
#define DIAG_SEL                0x40010
#define ENABLE_MASK             0x40014
#define CM_OFIFO_TAG_CONFIG     0x40018
#define MICROCODE_ERROR         0x4001c
#define OFIFO_0_TH_A_STATUS     0x40020
#define OFIFO_0_TH_B_STATUS     0x40024
#define OFIFO_1_TH_A_STATUS     0x40028
#define OFIFO_1_TH_B_STATUS     0x4002c
#define OFIFO_2_TH_A_STATUS     0x40030
#define OFIFO_2_TH_B_STATUS     0x40034
#define OUTPUT_FIFO_CNTL        0x40038


#define HQ3_VERSION	 	0x10
#define	HQ3_VERSION_MASK	0xf0
#define	HQ3_ID_MASK		0xffffff00
#define	HQ3_ID			0x48513300
#define GIO_CONFIG_MASK		0xffffffff
#define HQ3_UCODE_MASK		0x00ffffff

#if HQ4
#define HQ3_CONFIG_MASK		0x3ffff
#else
#define HQ3_CONFIG_MASK		0x7fff
#endif

#define HQ3_FLAGS_MASK		0x1ffffff
#define HQ3_INTR_MASK		0xffffffff
#define HQ3_HQCP_MASK           0x1fff
#define HQ3_CPDATA_MASK         0xffffffff
#define HQ3_CFIFO_MASK          0xffffffff
#define HQ3_HQCP_MASK           0x1fff
#define HQ3_CPDATA_MASK         0xffffffff
#define HQ3_CFIFO_MASK          0xffffffff
#define	HQ3_PIX_CMD_WORD		0x80000000

#define GE_DIAG_WR		BIT(31)

/* HQ3 writable registers index */

#define HQ3_CONFIG		1
#define FLAG_CNTXT		2
#define FLAG_SET_PRIV		3
#define	FLAG_CLR_PRIV 		4
#define FLAG_ENAB_SET		5
#define FLAG_ENAB_CLR		6
#define INTR_ENAB_SET		7	
#define INTR_ENAB_CLR		8	
#define CFIFO_HW		9
#define CFIFO_LW		10
#define CFIFO_DELAY		11
#define DFIFO_HW		12
#define DFIFO_LW		13
#define DFIFO_DELAY		14
#define GE0_DIAG_D		15
#define GE0_DIAG_A		16
#define GE1_DIAG_D		17
#define GE1_DIAG_A		18
#define CTXSW			19	
#define CFIFO_PRIV1		20	
#define CFIFO_PRIV2		21	
#define BFIFO_HW		22
#define BFIFO_LW		23
#define BFIFO_DELAY		24
#define GIO_CONFIG		25
#define GIO_BIGENDIAN		26	
#define GIO_LITENDIAN		27	
#define FLAG_SET		28
#define FLAG_CLR		29
#define CFIFO1			30
#define CFIFO2			31

#define MGRAS_XMAP_PP1_SELECT_OFF     ((__paddr_t)&(mgbase->xmap.pp1_sel) - (__paddr_t)mgbase)
#define MGRAS_XMAP_INDEX_OFF          ((__paddr_t)&(mgbase->xmap.index) - (__paddr_t)mgbase)
#define MGRAS_XMAP_CONFIG_OFF    ((__paddr_t)&(mgbase->xmap.config) - (__paddr_t)mgbase)
#define MGRAS_XMAP_CONFIG_BYTE_OFF    ((__paddr_t)&(mgbase->xmap.config_byte) - (__paddr_t)mgbase)
#define MGRAS_XMAP_BUF_SELECT_OFF    ((__paddr_t)&(mgbase->xmap.buf_sel) - (__paddr_t)mgbase)
#define MGRAS_XMAP_MAIN_MODE_OFF    ((__paddr_t)&(mgbase->xmap.main_mode) - (__paddr_t)mgbase)
#define MGRAS_XMAP_OVERLAY_MODE_OFF    ((__paddr_t)&(mgbase->xmap.ovl_mode) - (__paddr_t)mgbase)
#define MGRAS_XMAP_DIB_OFF    ((__paddr_t)&(mgbase->xmap.dib) - (__paddr_t)mgbase)
#define MGRAS_XMAP_DIB_DW_OFF    ((__paddr_t)&(mgbase->xmap.dib_dw) - (__paddr_t)mgbase)
#define MGRAS_XMAP_RE_RAC_OFF    ((__paddr_t)&(mgbase->xmap.re_rac) - (__paddr_t)mgbase)

/* HQ3 writable registers address offset definition */


#define HQ3_CONFIG_ADDR		((__paddr_t)&(mgbase->hq_config) - (__paddr_t)mgbase)
#define GIO_CONFIG_ADDR		((__paddr_t)&(mgbase->gio_config) - (__paddr_t)mgbase)
#define FLAG_SET_PRIV_ADDR	((__paddr_t)&(mgbase->flag_set_priv) - (__paddr_t)mgbase)
#define	FLAG_CLR_PRIV_ADDR 	((__paddr_t)&(mgbase->flag_clr_priv) - (__paddr_t)mgbase)

/*
 * XXX: Complete the address calculations for flag and intr registers
 */
#ifdef HQ4
#define FLAG_ENAB_SET_ADDR      0
#define FLAG_ENAB_CLR_ADDR      0
#define INTR_ENAB_SET_ADDR      0
#define INTR_ENAB_CLR_ADDR      0
#else
#define FLAG_ENAB_SET_ADDR	((__paddr_t)&(mgbase->flag_enab_set) - (__paddr_t)mgbase)
#define FLAG_ENAB_CLR_ADDR	((__paddr_t)&(mgbase->flag_enab_clr) - (__paddr_t)mgbase)
#define INTR_ENAB_SET_ADDR	((__paddr_t)&(mgbase->intr_enab_set) - (__paddr_t)mgbase)
#define INTR_ENAB_CLR_ADDR	((__paddr_t)&(mgbase->intr_enab_clr) - (__paddr_t)mgbase)
#endif

#define CFIFO_HW_ADDR		((__paddr_t)&(mgbase->cfifo_hw) - (__paddr_t)mgbase)
#define CFIFO_LW_ADDR		((__paddr_t)&(mgbase->cfifo_lw) - (__paddr_t)mgbase)
#define CFIFO_DELAY_ADDR	((__paddr_t)&(mgbase->cfifo_delay) - (__paddr_t)mgbase)
#define DFIFO_HW_ADDR		((__paddr_t)&(mgbase->dfifo_hw) - (__paddr_t)mgbase)
#define DFIFO_LW_ADDR		((__paddr_t)&(mgbase->dfifo_lw) - (__paddr_t)mgbase)
#define DFIFO_DELAY_ADDR	((__paddr_t)&(mgbase->dfifo_delay) - (__paddr_t)mgbase)
#define GE11_0_DIAG_D_ADDR		((__paddr_t)&(mgbase->ge0_diag_d) - (__paddr_t)mgbase)
#define GE11_0_DIAG_A_ADDR		((__paddr_t)&(mgbase->ge0_diag_a) - (__paddr_t)mgbase)
#define GE11_1_DIAG_D_ADDR		((__paddr_t)&(mgbase->ge1_diag_d) - (__paddr_t)mgbase)
#define GE11_1_DIAG_A_ADDR		((__paddr_t)&(mgbase->ge1_diag_a) - (__paddr_t)mgbase)
#define BFIFO_HW_ADDR		((__paddr_t)&(mgbase->bfifo_lw) - (__paddr_t)mgbase)
#define BFIFO_LW_ADDR		((__paddr_t)&(mgbase->bfifo_lw) - (__paddr_t)mgbase)
#define BFIFO_DELAY_ADDR	((__paddr_t)&(mgbase->bfifo_lw) - (__paddr_t)mgbase)
#define FLAG_SET_ADDR		((__paddr_t)&(mgbase->flag_set) - (__paddr_t)mgbase)
#define FLAG_CLR_ADDR		((__paddr_t)&(mgbase->flag_clr) - (__paddr_t)mgbase)
#define CFIFO1_ADDR		((__paddr_t)&(mgbase->cfifo_priv) - (__paddr_t)mgbase)
#define	TLB_VALIDS_ADDR		((__paddr_t)&(mgbase->tlb_valids) - (__paddr_t)mgbase)
/* HQ3 readable registers index */

#define HQPC			1	
#define CP_DATA			2
#define HQ3_CONFIG_RD		3
#define FLAG_SET_PRIV_RD	4
#define FLAG_ENAB_SET_RD	5
#define	CTX_DATA		6	
#define GIO_CONFIG_RD		7
#define	TLB_CURR_ADDR		8	
#define TLB_VALIDS_RD		9
#define	HQ3_STATUS		10	
#define	FIFO_STATUS		11	
#define	REBUS_READH		12	
#define	REBUS_READL		13	
#define	GIO_STATUS		14
#define	BUSY_DMA		15

/* convert hardcode values to #defines */
#define SCAN_WIDTH_OFF			((__paddr_t)&(mgbase->reif_ctx.scan_width) - (__paddr_t)mgbase)
#define RE_DMA_MODES_OFF		((__paddr_t)&(mgbase->reif_ctx.re_dma_modes) - (__paddr_t)mgbase)
#define RE_DMA_CNTRL_OFF		((__paddr_t)&(mgbase->reif_ctx.re_dma_cntrl) - (__paddr_t)mgbase)
#define RE_DMA_TYPE_OFF		((__paddr_t)&(mgbase->reif_ctx.re_dma_type) - (__paddr_t)mgbase)
#define LINE_COUNT_OFF		((__paddr_t)&(mgbase->reif_ctx.line_count) - (__paddr_t)mgbase)
#define IM_PG_WIDTH_OFF		((__paddr_t)&(mgbase->hag_ctx.im_pg_width) - (__paddr_t)mgbase)
#define IM_ROW_OFFSET_OFF		((__paddr_t)&(mgbase->hag_ctx.im_row_offset) - (__paddr_t)mgbase)
#define IM_ROW_START_ADDR_OFF		((__paddr_t)&(mgbase->hag_ctx.im_row_start_addr) - (__paddr_t)mgbase)
#define IM_LINE_CNT_OFF		((__paddr_t)&(mgbase->hag_ctx.im_line_cnt) - (__paddr_t)mgbase)
#define IM_FIRST_SPAN_WIDTH_OFF		((__paddr_t)&(mgbase->hag_ctx.im_first_span_width) - (__paddr_t)mgbase)
#define IM_LAST_SPAN_WIDTH_OFF		((__paddr_t)&(mgbase->hag_ctx.im_last_span_width) - (__paddr_t)mgbase)
#define IM_Y_DEC_FACTOR_OFF		((__paddr_t)&(mgbase->hag_ctx.im_y_dec_factor) - (__paddr_t)mgbase)
#define IM_DMA_CTRL_OFF		((__paddr_t)&(mgbase->hag_ctx.im_dma_ctrl) - (__paddr_t)mgbase)

/* HQ3 readable registers address offset definition */

#define HQPC_ADDR			((__paddr_t)&(mgbase->hqpc) - (__paddr_t)mgbase)
#define CP_DATA_ADDR		((__paddr_t)&(mgbase->cp_data) - (__paddr_t)mgbase)
#define	CTX_DATA_ADDR		((__paddr_t)&(mgbase->ctx_data) - (__paddr_t)mgbase)
#define	TLB_CURR_ADDR_ADDR	((__paddr_t)&(mgbase->tlb_curr_addr) - (__paddr_t)mgbase)
#define	STATUS_ADDR			((__paddr_t)&(mgbase->status) - (__paddr_t)mgbase)
#define	FIFO_STATUS_ADDR	((__paddr_t)&(mgbase->fifo_status) - (__paddr_t)mgbase)
#define	REBUS_READH_ADDR	((__paddr_t)&(mgbase->rebus_read_hi) - (__paddr_t)mgbase)
#define	REBUS_READL_ADDR	((__paddr_t)&(mgbase->rebus_read_lo) - (__paddr_t)mgbase)
#define	GIO_STATUS_ADDR		((__paddr_t)&(mgbase->gio_status) - (__paddr_t)mgbase)
#define	BUSY_DMA_ADDR		((__paddr_t)&(mgbase->busy_dma) - (__paddr_t)mgbase)

/* HQ3 ucode address */

#define HQUC_ADDR		((__paddr_t)&(mgbase->hquc) - (__paddr_t)mgbase)

#define REIF_CTX		0
#define	HAG_CTX			1
#define HIF_CTX			2
#define GIO_CTX			3

#define	REIF_CTX_BASE		((__paddr_t)&(mgbase->reif_ctx) - (__paddr_t)mgbase)
#define	HAG_CTX_BASE		((__paddr_t)&(mgbase->hag_ctx) - (__paddr_t)mgbase)
#define	HIF_CTX_BASE		((__paddr_t)&(mgbase->hif_ctx) - (__paddr_t)mgbase)
#define	GIO_CTX_BASE		((__paddr_t)&(mgbase->gio_ctx) - (__paddr_t)mgbase)
#define	TLB_ADDR			((__paddr_t)&(mgbase->tlb[0]) - (__paddr_t)mgbase)

/*
 * XXX: These hard-wired addresses have to be resolved soon...
 */
#ifdef HQ4
#define TLB_TEST_ADDR   ((__paddr_t)&(mgbase->tlb_test) - (__paddr_t)mgbase)
#define	TLB_SIZE		(((__paddr_t)&(mgbase->tlb_test) - (__paddr_t)&(mgbase->tlb[0])) / 16)
#define HQUC_SIZE		(__paddr_t)&(mgbase->pad63) - (__paddr_t)&(mgbase->hquc[0]) - 1
#define GRAPHICS_TLB_MODE ((__paddr_t)&(mgbase->graphics_tlb_mode) - (__paddr_t)mgbase)
#define GRAPHICS_DMA_MODE_B ((__paddr_t)&(mgbase->graphics_dma_mode_b) - (__paddr_t)mgbase)
#define GRAPHICS_DMA_MODE_A ((__paddr_t)&(mgbase->graphics_dma_mode_a) - (__paddr_t)mgbase)
#else /* HQ3 */
#define TLB_TEST_ADDR   0x50124 /* TLB_TEST ADDRESS */
#define	TLB_SIZE		(((__paddr_t)&(mgbase->tlb_curr_addr) - (__paddr_t)&(mgbase->tlb[0]))/8)
#define HQUC_SIZE		(__paddr_t)&(mgbase->window) - (__paddr_t)&(mgbase->hquc[0]) - 1
#endif

/**** REIF_CTX register map ******/

#define RSS_CTX_CMD_BASE	0x1000	

#define REIF_CTX_CMD_BASE	0xA00	

#define GE_RD_WD0	0x00
#define GE_RD_WD1	0x01
#define SCAN_WIDTH	0x02
#define LINE_COUNT	0x03
#define	RE_DMA_MODES	0x04
#define	RE_DMA_CNTRL	0x05
#define	RE_DMA_TYPE	0x06	
#define	REBUS_SYNC	0x07
#define	REIF_FLAGS	0x08
#define	REBUS_OUT1	0x09	
#define	REBUS_OUT2	0x0a	
#define DIAG_RD_WD0	0x0c
#define DIAG_RD_WD1	0x0b

/**** HAG_CTX register map ******/

#define HAG_CTX_CMD_BASE	0x800	

#define	IM_PG_LIST1		0x00
#define	IM_PG_LIST2		0x01
#define	IM_PG_LIST3		0x02
#define	IM_PG_LIST4		0x03
#define	IM_PG_WIDTH		0x04
#define	IM_ROW_OFFSET		0x05
#define	IM_ROW_START_ADDR	0x06
#define	IM_LINE_CNT		0x07
#define	IM_FIRST_SP_WIDTH	0x08
#define	IM_LAST_SP_WIDTH	0x09
#define	IM_Y_DEC_FACTOR		0x0a	
#define	IM_DMA_CNTL_STAT	0x0b	/* not rw */
#define	DL_JUMP_NO_WAIT		0x0c	/* not rw */
#define	DL_JUMP_WAIT		0x0e	/* not rw */
#define STACK_RAM_WD0_SAVE	0x10	/* not rw */
#define STACK_RAM_WD1_SAVE	0x18	/* not rw */
#define	IM_TABLE_BASE_ADDR1	0x20	/* double word */
#define	IM_TABLE_BASE_ADDR2	0x22	/* double word */
#define	IM_TABLE_BASE_ADDR3	0x24	/* double word */
#define	GL_TABLE_BASE_ADDR	0x26	/* double word */
#define	DL_TABLE_BASE_ADDR	0x28	/* double word */
#define	DMA_PG_SIZE		0x2a
#define	DMA_PG_NO_RANGE_IM1	0x2b
#define	DMA_PG_NO_RANGE_IM2	0x2c
#define	DMA_PG_NO_RANGE_IM3	0x2d
#define	DMA_PG_NO_RANGE_GL	0x2e
#define	DMA_PG_NO_RANGE_DL	0x2f
#define	MAX_IM1_TBL_PTR_OFFSET	0x30
#define	MAX_IM2_TBL_PTR_OFFSET	0x31
#define	MAX_IM3_TBL_PTR_OFFSET	0x32
#define	MAX_GL_TBL_PTR_OFFSET	0x33
#define	MAX_DL_TBL_PTR_OFFSET	0x34
#define	HAG_STATE_FLAGS		0x35
#define	HAG_STATE_REMAINDER	0x36	
#define DL_PUSH			0x37	/* not rw */	
#define DL_RETURN		0x39	/* not rw */	
#define	RESTORE_CFIFO		0x40	/* not rw */
#define	GIO_RESTORE_ADDRS	0x41	/* not rw */

/*
 * bits in the formatter mode register
 */
#define HQFORM_COMPONENT_SIZE_MASK      3
#define HQFORM_NIBBLE   0
#define HQFORM_BYTE     1
#define HQFORM_SHORT    2
#define HQFORM_WORD     3
#define HQFORM_COMP_PER_PIXEL_MASK      (3 << 2)
#define HQFORM_1_COMP_PER_PIXEL         (0 << 2)
#define HQFORM_2_COMP_PER_PIXEL         (1 << 2)
#define HQFORM_3_COMP_PER_PIXEL         (2 << 2)
#define HQFORM_4_COMP_PER_PIXEL         (3 << 2)
#define HQFORM_SRC_DST_ENDIAN_MASK      (3 << 4)
#define HQFORM_BE_TO_BE                 (0 << 4)
#define HQFORM_LE_TO_BE                 (1 << 4)
#define HQFORM_BE_TO_LE                 (2 << 4)
#define HQFORM_SWAP_BYTE_MASK           (1 << 6)
#define HQFORM_SWIZZLE_MASK             (1 << 7)
#define HQFORM_NIBBLE_EXPANSION_MASK    (3 << 8)
#define HQFORM_NIBBLE_EXPANSION_ZERO    (0 << 8)
#define HQFORM_NIBBLE_EXPANSION_REPL    (1 << 8)
#define HQFORM_NIBBLE_EXPANSION_NONE    (2 << 8)
#define HQFORM_ZERO_ALIGN_MASK          (1 << 10)
#define HQFORM_INVALIDATE_TRAILING      (1 << 11)
#define HQFORM_NIBBLE_SWAP_MASK         (1 << 12)

#define HQFORM_CANONICAL (HQFORM_BE_TO_BE | HQFORM_NIBBLE_EXPANSION_NONE | HQFORM_ZERO_ALIGN_MASK | HQFORM_INVALIDATE_TRAILING)

#endif /* __MGRAS_CONST_H__ */
