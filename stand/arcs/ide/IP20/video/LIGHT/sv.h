
/*****************************************************************

	sv.h	Low level header file for starter video SRV1

******************************************************************/

#define	FALSE			0
#define	TRUE			1

#define NTSC			0
#define PAL			1

#define RATE_60HZ		0
#define RATE_50HZ		1

		/* input window constants */

#define WINDOW_OFFSET_X		60
#define WINDOW_OFFSET_Y		38

		/* input window constants NTSC */

#define VOPOS_XINIT_NTSC	340
#define VOPOS_YINIT_NTSC	60
#define VOSIZE_XINIT_NTSC	636
#define VOSIZE_YINIT_NTSC	460

#define VOSIZE_XENG_NTSC	640	/* engineering sizes */
#define VOSIZE_YENG_NTSC	480	/* engineering sizes */

		/* input window constants PAL */
#define VOPOS_XINIT_PAL		265
#define VOPOS_YINIT_PAL		55
#define VOSIZE_XINIT_PAL	764
#define VOSIZE_YINIT_PAL	536

#define VOSIZE_XENG_PAL		768	/* engineering sizes */
#define VOSIZE_YENG_PAL		576	/* engineering sizes */
/****************************************************************************/

/*
 * RAW BOARD ADDRESS/LEN
 */
#define	SVB_PHYS_ADDR		0x1F3E0000
#define	SVB_ADDR		PHYS_TO_K1(SVB_PHYS_ADDR) /* SV base */
#define	SVB_LEN			0x10000
/****************************************************************************/

/*
 * BOARD SUB-ADDRESSES 
 */
		/* SVC1 Registers */
#define SV_SVCID		0	       /* ID register */
#define	SV_CONFIG		(0x04	>> 2)  /* SVB_CTRL_REG */
#define	SV_DMACTL		(0x08	>> 2)  /* SVB_DMA_CTRL */
#define	SV_DMADAT		(0x0C	>> 2)  /* SVB_DMA_DATA */
#define	SV_IN_X_START		(0x10	>> 2)  /* SVB_XIN_START */
#define	SV_IN_X_END		(0x14	>> 2)  /* SVB_XIN_STOP */
#define	SV_IN_Y_START		(0x18	>> 2)  /* SVB_YIN_START */
#define	SV_IN_Y_END		(0x1C	>> 2)  /* SVB_YIN_STOP */
#define	SV_DECIM		(0x20	>> 2)  /* SVB_DECIMATION */
#define	SV_BUS_CTL		(0x2C	>> 2)  /* SVB_BUS_CTRL */
#define	SV_FUNC_CTL		(0x30	>> 2)  /* SVB_FUNC_CTL_REG */
#define	SV_CLOCK_MODE		(0x34	>> 2)  /* SVB_CLK_MODE */


		/* SVCID register values */
#define SV_SVC1_CHIP_ID		0x60
#define SV_SVC1_REV_1		0x01
#define SV_REV_P1		(SV_SVC1_CHIP_ID | SV_SVC1_REV_1)

		/* CONFIG register values */
#define SV_CONFIG_RESET		0x00
#define SV_CONFIG_CLEAR		0xFF    /* XXX seems like should be 0x01 */
#define SV_VSYNC_INT_ENAB	0x02	/* enable video vert sync interrupt */
#define SV_DMA_INT_ENAB		0x20	/* enable DMA vert position intrpt */

		/* DMACTL register values */
#define	SV_DMA_INIT		0xC0	/* init val */
#define	SV_DECIMATE_SETUP1	0xEC	/* fifo1 -> GIO (w/decimate by 4) */
#define	SV_DECIMATE_SETUP2	0xE8	/* fifo1 -> GIO (w/decimate by 4) */
#define	SV_FIFO2GIO_SETUP1	0xCC	/* YUV fifo1 -> GIO */
#define	SV_FIFO2GIO_SETUP2	0xC8	/* YUV fifo1 -> GIO */
#define SV_SF_FIFO1_SETUP1	0xD4	/* fifo1 still-frame out */
#define SV_SF_FIFO1_SETUP2	0xD0	/* fifo1 still-frame out */
#define SV_SF_FIFO3_SETUP1	0xDC	/* fifo3 still-frame out */
#define SV_SF_FIFO3_SETUP2	0xD8	/* fifo3 still-frame out */

		/* BUS_CTL register values */
#define	SV_BUS_READ_FIFO1	0x6A	/* fifo1 -> GIO */
#define	SV_BUS_WRITE_FIFOS	0x9D	/* GIO -> fifo1 & fifo3 */
#define	SV_BUS_INIT		0x7A	/* vid -> screen, screen -> out */

		/* FUNC_CTL register values */
#define SV_COLOR_SET		0x80
#define SV_MONO_SET		0x7F

#define SV_NTSC_SET		0xEF
#define SV_PAL_SET		0x10

#define SV_BYPASS_VLUT_SET	0xBF
#define SV_USE_VLUT_SET		0x40

#define	SV_FRAMEGRAB_SET	0x20
#define	SV_FRAMEGRAB_OFF	0xDF

#define	SV_FIELD_DROP_SET	0xF7
#define	SV_FIELD_DROP_OFF	0x08

#define	SV_DITHER_SET		0xFB
#define	SV_DITHER_OFF		0x04

#define	SV_FILTER_SET		0xFD
#define	SV_FILTER_OFF		0x02

#define	SV_SLAVEOI_SET		0x01
#define	SV_SLAVEOI_OFF		0xFE

#define SV_FUNC_CTL_INIT	(SV_FIELD_DROP_OFF | SV_FILTER_OFF | \
				 SV_SLAVEOI_SET)

#define SV_FIFO3_WRITE		(SV_FRAMEGRAB_SET | SV_FIELD_DROP_OFF | \
				 SV_FILTER_OFF | SV_SLAVEOI_SET)
#if 0
#define SV_FIFO3_WRITE		(SV_COLOR_SET | SV_USE_VLUT_SET | \
				 SV_FRAMEGRAB_SET | SV_FIELD_DROP_OFF | \
				 SV_FILTER_OFF | SV_SLAVEOI_SET)
#endif

		/* CLOCK_MODE register values */
#define SV_CLOCK_INIT_RESET	0x80	/* clock mode reset */
#define SV_CLOCK_INIT_GO	0x00	/* default (init) clock mode */
#define SV_FIFO1_READ_RESET	0xa0	/* reset fifos for reading from host */
#define SV_FIFO1_READ_GO	0x20	/* allow fifo1 to be read from host */
#define SV_FIFOS_WRITE_RESET	0xf0	/* reset fifos for writing from host */
#define SV_FIFOS_WRITE_GO	0x70	/* allow writing to fifos 1 & 3 */
/****************************************************************************/

/* I2C BUS DEFINITIONS */

	/* I2C Bus addresses */
#define	SV_I2C_CS_CTRL		(0x804	>> 2)  /* SVB_I2C_CTRL_REG */
#define	SV_I2C_CS_DATA		(0x808	>> 2)  /* SVB_I2C_DATA_REG */

	/* 8584 I2C Controller self address */
#define	SV_I2C_8584_ADDR	0x55

	/* pointer data for registers (serial i/o off) */
#define	SV_I2C_OWNADDR_REG	0x00
#define	SV_I2C_CLK_REG		0x20

	/* pointer data for virtual registers (serial i/o on) */
#define	SV_I2C_SLV_ADDR_REG	0x40
#define	SV_I2C_SUB_ADDR_REG	0x45 /* sets PIN 0x80 status */
#define	SV_I2C_STOP		0xC3

	/* init data */
#define	SV_I2C_CLK_8MHZ		0x18

	/* status masks */
#define	SV_I2C_SERIAL_BUSY	0x80

#define	SV_I2C_TIMEOUT		0x10
#define	SV_I2C_BER		0x10
#define SV_I2C_LRB		0x08
#define	SV_I2C_TIMEOUT_DELAY	0x100

#define	I2C_BUSY(pSV)		(theSVboard[pSV] & SV_I2C_SERIAL_BUSY)
#define	I2C_BERR(pSV)		(theSVboard[pSV] & SV_I2C_BER)
/****************************************************************************/


	/* DENC addresses */
#define	SV_DENC_ADDR_CLUT	(0x810	>> 2)  /* SVB_DENC_ADDR_CLUT */
#define	SV_DENC_DATA_CLUT	(0x814	>> 2)  /* SVB_DENC_DATA_CLUT */
#define	SV_DENC_ADDR_CTL	(0x818	>> 2)  /* SVB_DENC_ADDR_CTL */
#define	SV_DENC_DATA_CTL	(0x81C	>> 2)  /* SVB_DENC_DATA_CTL */

	/* Pixkey */
#define	SV_PIXKEY_BASE		(0xC00	>> 2)  /* SVB_PIXKEY_BASE */

	/* Color Look-Up Table */
#define	SV_CLUT_BASE		(0x1000	>> 2)  /* SVB_CLUT_BASE */

	/* SV CLUT offsets */
#define	SV_LUT00		0x00  /* SVBLUT00 */
#define	SV_LUT01		0x01  /* SVBLUT01 */
#define	SV_LUT02		0x02  /* SVBLUT02 */
#define	SV_LUT03		0x03  /* SVBLUT03 */

#define	SV_LUT_INC		0x04  /* SVBLUT_INC */
/****************************************************************************/

/* CSC DEFINITIONS */
#define	CSC_ADDR		0xE0
#define	CSC_CONTROL_REG		0x00
#define	CSC_LUT_REG		0x01
#define CSCA_RLUT_REG		0x01
#define CSCA_GLUT_REG		0x02
#define CSCA_BLUT_REG		0x03
#define CSCA_LUTS_REG		0x04

#define CSC_WRITE_LUTS		0xBF
#define CSC_READ_LUTS		0x40

#define CSC_MONO_VLUT_LD	0x34
#define CSC_MONO_VLUT_RUN	0x74

#define CSC_COLOR8_VLUT_LD	0x38
#define CSC_COLOR8_VLUT_RUN	0x78

#define CSC_COLOR9_VLUT_LD	0x39
#define CSC_COLOR9_VLUT_RUN	0x79
/****************************************************************************/

/* DMSD DEFINITIONS */
#define	DMSD_ADDR		0x8A  /* alternative (unused) addr: 0x8e */
#define DMSD_PAGE1		0x00
#define DMSD_PAGE2		0x14

#define	DMSD_REG_IDEL		0x00
#define	DMSD_REG_HSYB50		0x01
#define	DMSD_REG_HSYE50		0x02
#define	DMSD_REG_HCLB50		0x03
#define	DMSD_REG_HCLE50		0x04
#define	DMSD_REG_HSP50		0x05
#define	DMSD_REG_LUMA		0x06
#define	DMSD_REG_HUE		0x07
#define	DMSD_REG_CKQAM		0x08
#define	DMSD_REG_CKSECAM	0x09
#define	DMSD_REG_SENPAL		0x0A
#define	DMSD_REG_SENSECAM	0x0B
#define	DMSD_REG_GC0		0x0C
#define	DMSD_REG_STDMODE	0x0D
#define	DMSD_REG_IOCLK		0x0E
#define	DMSD_REG_CTRL3		0x0F
#define	DMSD_REG_CTRL4		0x10
#define	DMSD_REG_CHCV		0x11
#define	DMSD_REG_HSYB60		0x14
#define	DMSD_REG_HSYE60		0x15
#define	DMSD_REG_HCLB60		0x16
#define	DMSD_REG_HCLE60		0x17
#define	DMSD_REG_HSP60		0x18

		/* theDMSD.dmsdRegLUMA */
#define	DMSD_PREF_SEL_ON	0x40
#define	DMSD_PREF_SEL_OFF	0xFFFFFFBF
#define	DMSD_CTBS_SEL_ON	0x80
#define	DMSD_CTBS_SEL_OFF	0xFFFFFF7F

#define	DMSD_APER_MASK		0xFFFFFFFC
#define	DMSD_APER_BITS		0x03
#define	DMSD_CORI_MASK		0xFFFFFFF3
#define	DMSD_CORI_BITS		0x0C
#define	DMSD_BPSS_MASK		0xFFFFFFCF
#define	DMSD_BPSS_BITS		0x30

		/* theDMSD.dmsdRegGC0 */
#define	DMSD_COL_SEL_ON		0x80
#define	DMSD_COL_SEL_OFF	0xFFFFFF7F

#define	DMSD_LFIS_MASK		0xFFFFFF9F
#define	DMSD_LFIS_BITS		0x60

		/* theDMSD.dmsdRegSTDmode */
#define	DMSD_TV_MODE_SEL	0xFFFFFF7F
#define	DMSD_VTR_MODE_SEL	0x80

		/* theDMSD.dmsdRegIOclk */
#define	DMSD_INPUT_SEL_MASK	0xFFFFFFFC
#define	DMSD_INPUT_SEL_0	0x00	
#define	DMSD_INPUT_SEL_1	0x01	
#define	DMSD_INPUT_SEL_2	0x02	
#define	DMSD_COMP_MODE_SEL	0xFFFFFFFB
#define	DMSD_SVID_MODE_SEL	0x04
#define	DMSD_HPLL_SEL_ON	0xFFFFFF7F
#define	DMSD_HPLL_SEL_OFF	0x80

		/* theDMSD.dmsdRegCTRL3 */
#define	DMSD_YDEL_MASK		0xFFFFFFF8
#define	DMSD_YDEL_BITS		0x07

#define	DMSD_OFTS_SEL411_ON	0xFFFFFFF7
#define	DMSD_OFTS_SEL422_ON	0x08

#define	DMSD_FSEL_SEL50_ON	0xFFFFFFBF
#define	DMSD_FSEL_SEL60_ON	0x40

#define	DMSD_AUFD_SEL_ON	0x80
#define	DMSD_AUFD_SEL_OFF	0xFFFFFF7F


		/* theDMSD.dmsdRegCTRL4 */
#define	DMSD_VNOI_MASK		0xFFFFFFFC
#define	DMSD_VNOI_BITS		0x03

#define	DMSD_HRFS_SEL_PHIL	0xFFFFFFFB
#define	DMSD_HRFS_SEL_CCIR	0x04

		/* theDMSD.dmsdRegCHCV */
#define	DMSD_CHCV_NTSC		0x2C
#define	DMSD_CHCV_PAL		0x59
/****************************************************************************/

/* DENC DEFINITIONS */
#define DENC_ADDR		0xB0
	/* DENC internal register indices */
#define	DENC_REG_IN00		0x00
#define	DENC_REG_IN01		0x01
#define	DENC_REG_IN02		0x02
#define	DENC_REG_IN03		0x03
#define	DENC_REG_SYNC00		0x04
#define	DENC_REG_SYNC01		0x05
#define	DENC_REG_SYNC02		0x06
#define	DENC_REG_SYNC03		0x07
#define	DENC_REG_OUT00		0x08
#define	DENC_REG_OUT01		0x09
#define	DENC_REG_OUT02		0x0A
#define	DENC_REG_OUT03		0x0B
#define	DENC_REG_ENCODE00	0x0C
#define	DENC_REG_ENCODE01	0x0D
#define	DENC_REG_ENCODE02	0x0E

		/* DENC_REG_IN00: 0x00 */

	/* MOD 0-1 - (0,1) */
#define	DENC_MOD_MASK		0xFC
#define	DENC_GENLOCK_ON		0x00
#define	DENC_STANDALONE_ON	0x01
#define	DENC_MASTER_ON		0x02
#define	DENC_TEST_ON		0x03

	/* CCIR - (2) */
#define	DENC_CCIR_MASK		0xFB
#define	DENC_CCIR_ON		0x04
#define	DENC_CCIR_OFF		0x00

	/* SCBW - (3) */
#define	DENC_SCBW_MASK		0xF7
#define	DENC_SCBW_NORM		0x00
#define	DENC_SCBW_HI		0x08

	/* FMT - (4,5,6) */
#define	DENC_FMT_MASK		0x8F
#define	DENC_FMT_411DMSD2	0x00
#define	DENC_FMT_411CUSTM	0x10
#define	DENC_FMT_422DMSD2	0x20
#define	DENC_FMT_422CUSTM	0x30
#define	DENC_FMT_444YUV		0x40
#define	DENC_FMT_444RGB		0x50
#define	DENC_FMT_RESERVED	0x60
#define	DENC_FMT_8BIT_INDEX	0x70

	/* VTBY - (7) */
#define	DENC_VTBY_MASK		0x7F
#define	DENC_VTBY_ON		0x00
#define	DENC_VTBY_OFF		0x80

		/* DENC_REG_IN01: 0x01 */

	/* TRER - (0-7) */
#define	DENC_TRER_MIN		0x00
#define	DENC_TRER_MAX		0xFF

		/* DENC_REG_IN02: 0x02 */

	/* TREG - (0-7) */
#define	DENC_TREG_MIN		0x00
#define	DENC_TREG_MAX		0xFF

		/* DENC_REG_IN03: 0x03 */

	/* TREB - (0-7) */
#define	DENC_TREB_MIN		0x00
#define	DENC_TREB_MAX		0xFF

		/* DENC_REG_SYNC00: 0x04 */

	/* HLCK - (0) READ ONLY */
#define	DENC_HLCK_MASK		0x01
#define	DENC_HLCK_LOCKED	0x00
#define	DENC_HLCK_UNLOCKED	0x01

	/* OEF - (1) READ ONLY */
#define	DENC_OEF_MASK		0x02
#define	DENC_OEF_EVEN		0x00
#define	DENC_OEF_ODD		0x02

	/* HPLL - (2) */
#define	DENC_HPLL_MASK		0xFB
#define	DENC_HPLL_ON		0x00
#define	DENC_HPLL_OFF		0x04

	/* NINT - (3) */
#define	DENC_NINT_MASK		0xF7
#define	DENC_NINT_INT_ON	0x00
#define	DENC_NINT_INT_OFF	0x08

	/* VTCR - (4) */
#define	DENC_VTCR_MASK		0xEF
#define	DENC_VTCR_TV		0x00
#define	DENC_VTCR_VCR		0x10

	/* SCEN - (5) */
#define	DENC_SCEN_MASK		0xCF
#define	DENC_SCEN_ON		0x20
#define	DENC_SCEN_OFF		0x00

	/* SYSEL - (6,7) */
#define	DENC_SYSEL_MASK		0x3F
#define	DENC_SYSEL_CSYNC_LO	0x00
#define	DENC_SYSEL_HSVS_LO	0x40
#define	DENC_SYSEL_CSYNC_HI	0x80
#define	DENC_SYSEL_HSVS_HI	0xC0

		/* DENC_REG_SYNC01: 0x05 */

	/* GDC 0-5 - (0,1,2,3,4,5) */
#define	DENC_GDC_MIN		0x00
#define	DENC_GDC_MAX		0x6F

		/* DENC_REG_SYNC02: 0x06 */

	/* IDEL 0-7 - (0-7) */
#define	DENC_IDEL_MIN		0x00
#define	DENC_IDEL_MAX		0xFF

		/* DENC_REG_SYNC03: 0x07 */

	/* PSO 0-5 - (0,1,2,3,4,5) */
#define	DENC_PSO_MIN		0x00
#define	DENC_PSO_MAX		0x6F

		/* DENC_REG_OUT00: 0x08 */

	/* SRSN - (0) */
#define	DENC_SRSN_MASK		0xFE
#define	DENC_SRSN_RESET_ON	0x01
#define	DENC_SRSN_RESET_OFF	0x00

	/* GPSW - (1) */
#define	DENC_GPSW_MASK		0xFD
#define	DENC_GPSW_ON		0x02
#define	DENC_GPSW_OFF		0x00

	/* IM - (3) */
#define	DENC_IM_MASK		0xFB
#define	DENC_IM_ON			0x00
#define	DENC_IM_OFF			0x04

	/* COKI - (3) */
#define	DENC_COKI_MASK		0xF7
#define	DENC_COKI_COLOR_ON	0x00
#define	DENC_COKI_COLOR_OFF	0x08

	/* CPR - (4) */
#define	DENC_CPR_MASK		0xEF
#define	DENC_CPR_LDVREF_ON	0x00
#define	DENC_CPR_LDVREF_OFF	0x10

	/* SRC - (5) */
#define	DENC_SRC_MASK		0xDF
#define	DENC_SRC_EXT_ON		0x00
#define	DENC_SRC_DTV2_ON	0x20

	/* KEYE - (6) */
#define	DENC_KEYE_MASK		0xBF
#define	DENC_KEYE_OFF		0x00
#define	DENC_KEYE_ON		0x40

	/* DD - (7) */
#define	DENC_DD_MASK		0x7F
#define	DENC_DD_OFF		0x80
#define	DENC_DD_ON		0x00

		/* RESERVED: 0x09 */

		/* RESERVED: 0x0A */

		/* RESERVED: 0x0B */

		/* DENC_REG_ENCODE00: 0x0C */

	/* CHPS - (0-7) */
#define	DENC_CHPS_MIN		0x00
#define	DENC_CHPS_MAX		0xFF

		/* DENC_REG_ENCODE01: 0x0D */

	/* FSCO - (0-7) */
#define	DENC_FSCO_MIN		0x00
#define	DENC_FSCO_MAX		0xFF

		/* DENC_REG_ENCODE02: 0x0E */

	/* STD - (0-3) */
#define	DENC_STD_MASK		0xF0
#define	DENC_STD_NTSC4460_SQP	0x00
#define	DENC_STD_NTSC4450_SQP	0x01
#define	DENC_STD_PALBG50_SQP	0x02

#define	DENC_STD_NTSC4460_CCIR	0x03
#define	DENC_STD_NTSC4450_CCIR	0x04
#define	DENC_STD_PALBG50_CCIR	0x05

#define	DENC_STD_RES_1		0x06
#define	DENC_STD_RES_2		0x07

#define	DENC_STD_PALM60_SQP	0x08
#define	DENC_STD_PALM60_CCIR	0x09
#define	DENC_STD_PALM50_CCIR	0x0A
#define	DENC_STD_PALM50_SQP	0x0B

#define	DENC_STD_NTSCM60_SQP	0x0C
#define	DENC_STD_NTSCM60_CCIR	0x0D


	/* CLK - (4) READ ONLY */
#define	DENC_CLK_MASK		0x10
#define	DENC_CLK_LOCKED		0x00
#define	DENC_CLK_UNLOCKED	0x10


	/* function error status returns */
#define	NO_ERROR		0x00		/* ERR_OK */
#define ERROR			-1		/* general error */

#define	ERR_I2C_TIMEOUT		0x01

#define	ERR_DEV_MEM		0x02
#define	ERR_MEM_MAP		0x03

#define	ERR_GFX_OPEN		0x04
#define	ERR_GFX_FSTAT		0x05
#define	ERR_GFX_ATTACH		0x06
#define	ERR_GFX_MAPALL		0x07

#define	ERR_FILE_ERROR		0x08
#define	ERR_NULL_FILENAME	0x09

#define	ERR_MALLOC		0x0A

		/* LG1 definitions */

#define LG1_MAPPED_ADDRESS      0x1000

#define VIDEO_OPTION_MASK	0x7F
#define VIDEO_OPTION_ON		0x80

#define LIVEVID_COLOR_MAP_MASK  0x3F
#define	LIVEVID_COLOR_MAP_SEL03	0xC0
#define	LIVEVID_COLOR_MAP_SEL02	0x80
#define	LIVEVID_COLOR_MAP_SEL01	0x40
#define	LIVEVID_COLOR_MAP_SEL00	0x00

#define VIDEO_REPLACE_MASK      0x0000
#define VIDEO_REPLACE_MODE      0x6000

		/* VC1 command bus registers */

#define	TEMP_REG		0x00
#define	XMAP_CONTROL_REG	0x00
#define	XMAP_MODE_REG		0x01
#define	EXTERN_MEM_REG		0x02
#define	TEST_REG		0x03
#define	ADDR_LO_REG		0x04
#define	ADDR_HI_REG		0x05
#define	SYS_CTRL_REG		0x06

		/* VC1 sub-addresses */

#define	AUXVIDEO_MAP_REG	0x62

		/* BT479 definitions */

#define	BTPAL00			0x00
#define	BTPAL01			0x10
#define	BTPAL02			0x20
#define	BTPAL03			0x30

#define	BT479_COMMAND_REG00	0x82
#define	BT479_PAL_RAM_START	0x00
#define	BT479_PAL_RAM_LEN	0x100

		/* LUT data types */

#define	MONO_LUT		0x00
#define	RGB332V1_LUT		0x01
#define	RGB332GLRGB_LUT		0x02

		/* Read YUV, Write RGB defs */

typedef struct {
	unsigned char y[4];
	unsigned char u,v;
} SVBLOCK;

/*	brooktree numbers for YUV -> RGB	*/
/*
#define A11_ (1.0)
#define A12_ (0.0)
#define A13_ (1.14)
#define A21_ (1)
#define A22_ (-0.395)
#define A23_ (-0.581)
#define A31_ (1.0)
#define A32_ (2.032)
#define A33_ (0.0)
*/

/*	chucks' numbers for YUV -> RGB		*/

#define A11_ (1.0)
#define A12_ (0.0)
#define A13_ (1.0)
#define A21_ (1)
#define A22_ (-0.186)
#define A23_ (-0.508)
#define A31_ (1.0)
#define A32_ (1.0)
#define A33_ (0.0)

/*	chucks' numbers for RGB -> YUV		*/

#define B11_ (0.3)
#define B12_ (0.59)
#define B13_ (0.11)
#define B21_ (-0.3)
#define B22_ (-0.59)
#define B23_ (0.89)
#define B31_ (0.7)
#define B32_ (-0.59)
#define B33_ (-0.11)


#define YMASK			0xff0
#define UMASK			0xc
#define VMASK			0x3
#define SVBLKSIZE		4
#define ODD			0
#define EVN			1

#define YUV_BLANKING_PAL	30
#define YUV_BLANKING_NTSC	30

#define RGB_BLANKING_PAL	30
#define RGB_BLANKING_NTSC	30

#define XSIZE_NTSC		640
#define YSIZE_NTSC		480
#define XSIZE_PAL		768
#define YSIZE_PAL		576

#define	YUV_DATA_SIZE_NTSC	\
    ((YUV_BLANKING_NTSC * XSIZE_NTSC) + (XSIZE_NTSC * (YSIZE_NTSC / 2)))
#define	RGB_DATA_SIZE_NTSC	\
    ((RGB_BLANKING_NTSC * XSIZE_NTSC) + (XSIZE_NTSC * YSIZE_NTSC))

#define	YUV_DATA_SIZE_PAL	\
    ((YUV_BLANKING_PAL * XSIZE_PAL) + (XSIZE_PAL * (YSIZE_PAL / 2)))
#define	RGB_DATA_SIZE_PAL	\
    ((RGB_BLANKING_PAL * XSIZE_PAL) + (XSIZE_PAL * YSIZE_PAL))

/*************************************************************
 * misc definitions
 *************************************************************/
#define GIO_BUS_DELAY	100  /* bus delay needed (approx. in microseconds) */

/*************************************************************
 * global data
 *************************************************************/
extern volatile long *theSVboard; /* global pointer to SV1 Board */
extern unsigned char svc1_revid;  /* SVC1 chip ID and revision number */
extern int *Reportlevel;	  /* diagnostics message report level */


/*************************************************************
 * Debugging stuff
 *************************************************************/

#ifdef I2C_DEBUG
long status_poll[1000];
int poll_num;
#endif

int writeI2Cdata(long);
int writeI2Cctrl(long);
int writeI2CdataNP(long);
int pollI2C(void);
int writeParallelData(long, long);
int delayWritedata(long);

int initsv(void);
int vfifo1test(void);
int voclut_test(void);
