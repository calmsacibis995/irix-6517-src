/****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 ****************************************************************************/

#ifndef __SYS_VINOHW_H__
#define __SYS_VINOHW_H__

/*
 * Vino Video driver
 *
 * vinohw.h - contains the vino hardware specific definitions.
 *
 * $Revision: 1.1 $
 *
 */

/*
 *  Miscellaneous type definitions
 */

typedef volatile unsigned long vol_ulong;
typedef unsigned char uchar;

/*
 *  Vino ASIC channel register definition
 */

typedef struct {
	vol_ulong	alpha;
	vol_ulong	alpha_l;
	vol_ulong	clip_start;
	vol_ulong	clip_start_l;
	vol_ulong	clip_end;
	vol_ulong	clip_end_l;
	vol_ulong	frame_rate;
	vol_ulong	frame_rate_l;
	vol_ulong	field_counter;
	vol_ulong	field_counter_l;
	vol_ulong	line_size;
	vol_ulong	line_size_l;
	vol_ulong	line_count;
	vol_ulong	line_count_l;
	vol_ulong	page_index;
	vol_ulong	page_index_l;
	vol_ulong	next_4_desc;
	vol_ulong	next_4_desc_l;
	vol_ulong	desc_table_ptr;
	vol_ulong	desc_table_ptr_l;
	vol_ulong	desc_0;
	vol_ulong	desc_0_l;
	vol_ulong	desc_1;
	vol_ulong	desc_1_l;
	vol_ulong	desc_2;
	vol_ulong	desc_2_l;
	vol_ulong	desc_3;
	vol_ulong	desc_3_l;
	vol_ulong	fifo_threshold;
	vol_ulong	fifo_threshold_l;
	vol_ulong	fifo_read;
	vol_ulong	fifo_read_l;
	vol_ulong	fifo_write;
	vol_ulong	fifo_write_l;
} VinoChannel;

/*
 *  Vino ASIC register definitions
 */

typedef struct {
	vol_ulong	rev_id;
	vol_ulong	rev_id_l;
	vol_ulong	control;
	vol_ulong	control_l;
	vol_ulong	intr_status;
	vol_ulong	intr_status_l;
	vol_ulong	i2c_control;
	vol_ulong	i2c_control_l;
	vol_ulong	i2c_data;
	vol_ulong	i2c_data_l;
	VinoChannel	a;
	VinoChannel	b;
} *VinoRegs;

/*
 *  Vino board state and driver state definition
 */

#define SHADOW_7191_REG_SIZE	0x19

typedef struct {
	VinoRegs        hw_addr;
	uchar   	shadow_7191_regs [SHADOW_7191_REG_SIZE];
} VinoBoard;

/*
 *  Register description definition used in the master reg table
 *  used by vinoGetReg and vinoSetReg.  The register description
 *  is used to translate between the external ioctl register id 
 *  and the chip and register offset in the vino hw.
 */

typedef struct {
    int reg_id;		/* ioctl register identifier	*/
    int chip_type;	/* chip type identifier		*/
    int offset;		/* reg offset or subaddr	*/
} RegDesc;

/*
 *  Device Dependent Control Descripiton
 */

typedef struct {
    int ctl_id;		/* ioctl control identifier */
    int chip_type;	/* chip type id */
    int reg_off;	/* register offset */
    int max_val;	/* maximum control value */
    int bit_mask;	/* mask to clear the bit(s) */
    int bit_shift;	/* bit shift count */
} VinoDDControlDesc;

/*
 *  Node description - describes the state of the vino nodes.
 *
 *  Vino has two sources video nodes which can be connected to 
 *  two drain memory nodes.
 */

typedef struct {
    int type;
    int kind;
    int	channel;
    int format;
    int timing;
} NodeDesc;

/*
 *  Register types for vino video asics.
 */

#define VINO_ASIC	0		/* vino asic */
#define VINO_DMSD	1		/* phillips 7191 DMSD asic */
#define VINO_CDMC	2		/* camera controller asic */

/*************************************************************************
 *
 *  Vino ASIC register definitions
 *
 *************************************************************************/

/*
 *  Macros to read and write the Vino ASIC regs.
 *
 *  These macros are used to isolate the hw reg accesses so that
 *  if necessary they can be changed to calls to assembly functions
 *  which will do double word loads and stores.
 */

/*
 *  Vino reg read/write macros that are specified by 
 *  vino base address and register name from the VinoRegs
 *  struct.  The reg name points to a 64 bit offset.
 *  This is done to allow VINO_REG_WRITE_PHYS
 *  and VINO_REG_READ_PHYS to always get 64 bit addresses in case
 *  we have to later use assembly code to do double word accesses.
 */

#define VINO_REG_READ(vinohw, reg) \
	    VINO_REG_READ_PHYS(&(vinohw->reg))
	    
#define VINO_REG_WRITE(vinohw, reg, val) \
	    VINO_REG_WRITE_PHYS(&(vinohw->reg), (val))

/*
 *  Macros that take a register address that is on 64 bit offsets.
 *  To access the low word of the 64 bit double word we add 4 to the
 *  address and access the register.  If we have to do double word
 *  accesses then an assembly function will be called with the 64 bit
 *  address directly.
 */

#define VINO_REG_READ_PHYS(addr)	\
	    (*((ulong *)((char *)addr + 4)))

#define VINO_REG_WRITE_PHYS(addr, val)	\
	    (*((ulong *)((char *)addr + 4)) = (val))

/*
 *  Vino ASIC register numbers used by reg get/set.
 */

#define	VINO_REG_REV_ID			0
#define	VINO_REG_CONTROL		1
#define	VINO_REG_INTR_STATUS		2
#define	VINO_REG_I2C_CONTROL		3
#define	VINO_REG_I2C_DATA		4

#define	VINO_REG_A_ALPHA		5
#define	VINO_REG_A_CLIP_START		6
#define	VINO_REG_A_CLIP_END		7
#define	VINO_REG_A_FRAME_RATE		8
#define	VINO_REG_A_FIELD_CNTR		9
#define	VINO_REG_A_LINE_SIZE		10
#define	VINO_REG_A_LINE_COUNT		11
#define	VINO_REG_A_PAGE_INDEX		12
#define	VINO_REG_A_NEXT_4_DESC		13
#define	VINO_REG_A_DESC_TBL_PTR		14
#define	VINO_REG_A_DESC_0		15
#define	VINO_REG_A_DESC_1		16
#define	VINO_REG_A_DESC_2		17
#define	VINO_REG_A_DESC_3		18
#define	VINO_REG_A_FIFO_THRSHLD		19
#define	VINO_REG_A_FIFO_READ		20
#define	VINO_REG_A_FIFO_WRITE		21

#define	VINO_REG_B_ALPHA		22
#define	VINO_REG_B_CLIP_START		23
#define	VINO_REG_B_CLIP_END		24
#define	VINO_REG_B_FRAME_RATE		25
#define	VINO_REG_B_FIELD_CNTR		26
#define	VINO_REG_B_LINE_SIZE		27
#define	VINO_REG_B_LINE_COUNT		28
#define	VINO_REG_B_PAGE_INDEX		29
#define	VINO_REG_B_NEXT_4_DESC		30
#define	VINO_REG_B_DESC_TBL_PTR		31
#define	VINO_REG_B_DESC_0		32
#define	VINO_REG_B_DESC_1		33
#define	VINO_REG_B_DESC_2		34
#define	VINO_REG_B_DESC_3		35
#define	VINO_REG_B_FIFO_THRSHLD		36
#define	VINO_REG_B_FIFO_READ		37
#define	VINO_REG_B_FIFO_WRITE		38

/*
 *  Vino ASIC - ID and Revision Register
 */

#define VINO_ID_VALUE(reg)	(((reg) & 0xF0) >> 4)
#define VINO_CHIP_ID		0xB
#define REV_NUM_MASK		0x0F

/*
 *  Vino ASIC - Control Register definitions
 */

#define BIT(n)	(0x1 << n)

#define CR_LITTLE_ENDIAN		BIT(0)
#define CR_A_EN_FIELD_TRANS_INTR	BIT(1)
#define CR_A_EN_FIFO_OVRFL_INTR		BIT(2)
#define CR_A_EN_END_OF_DESC_INTR	BIT(3)
#define CR_B_EN_FIELD_TRANS_INTR	BIT(4)
#define CR_B_EN_FIFO_OVRFL_INTR		BIT(5)
#define CR_B_EN_END_OF_DESC_INTR	BIT(6)
#define CR_A_ENABLE			BIT(7)
#define CR_A_EN_INTERLEAVE		BIT(8)
#define CR_A_EN_SYNC			BIT(9)
#define CR_A_SELECT_D1			BIT(10)
#define CR_A_COLOR_SPC_CONV_RGB		BIT(11)
#define CR_A_EN_LUMMA_ONLY		BIT(12)
#define CR_A_EN_DECIMATION		BIT(13)
#define CR_A_DEC_SCALE_FACTOR(sf)	((sf & 7) << 14)
#define CR_A_EN_DEC_HORIZ_ONLY		BIT(17)
#define CR_A_EN_DITHER_24_TO_8		BIT(18)
#define CR_B_ENABLE			BIT(19)
#define CR_B_EN_INTERLEAVE		BIT(20)
#define CR_B_EN_SYNC			BIT(21)
#define CR_B_SELECT_D1			BIT(22)
#define CR_B_COLOR_SPC_CONV_RGB		BIT(23)
#define CR_B_EN_LUMMA_ONLY		BIT(24)
#define CR_B_EN_DECIMATION		BIT(25)
#define CR_B_DEC_SCALE_FACTOR(sf)	((sf & 7) << 26)
#define CR_B_EN_DEC_HORIZ_ONLY		BIT(29)
#define CR_B_EN_DITHER_24_TO_8		BIT(30)

/*
 *  Vino ASIC - Interrupt and Status Register
 */

#define ISR_A_END_OF_FIELD_INTR		BIT(0)
#define ISR_A_FIFO_OVRFL_INTR		BIT(1)
#define ISR_A_END_OF_DESC_INTR		BIT(2)
#define ISR_B_END_OF_FIELD_INTR		BIT(3)
#define ISR_B_FIFO_OVRFL_INTR		BIT(4)
#define ISR_B_END_OF_DESC_INTR		BIT(5)

/*
 *  Vino ASIC - I2C Control and Status Register
 */

#define I2C_FORCE_IDLE  (0 << 0)

#define I2C_IS_IDLE(status)     (((status) & I2C_NOT_IDLE) == 0)
#define I2C_NOT_IDLE    	(1 << 0)
#define I2C_WRITE       	(0 << 1)
#define I2C_READ        	(1 << 1)
#define I2C_RELEASE_BUS 	(0 << 2)
#define I2C_HOLD_BUS    	(1 << 2)
#define I2C_XFER_DONE   	(0 << 4)
#define I2C_XFER_BUSY   	(1 << 4)
#define I2C_ACK         	(0 << 5)
#define I2C_NACK        	(1 << 5)
#define I2C_BUS_OK      	(0 << 7)
#define I2C_BUS_ERR     	(1 << 7)

/*
 * I2C bus slave addresses
 */

#define DMSD_ADDR	0x8A
/* was 0x57 */
#define CDMC_ADDR	0xAE

#define I2C_WRITE_ADDR	0x0
#define I2C_READ_ADDR	0x1

#define I2C_OK			0
#define I2C_ERROR		-1

/*
 *  vinoI2cWriteBlock() definitions
 */

#define	END_BLOCK		-1	/* flag to mark the end of reg list */
#define SKIP_RO_REG		-2	/* flag to mark a read only reg */

/*************************************************************************
 *
 *  Phillips 7191 DMSD register definitions
 *
 *************************************************************************/

/*
 *  Phillips 7191 DMSD register subaddresses.
 */

#define DMSD_REG_IDEL           0x00
#define DMSD_REG_HSYB		0x01
#define DMSD_REG_HSYS		0x02
#define DMSD_REG_HCLB		0x03
#define DMSD_REG_HCLS		0x04
#define DMSD_REG_HPHI		0x05
#define DMSD_REG_LUMA           0x06
#define DMSD_REG_HUEC		0x07
#define DMSD_REG_CKTQ		0x08
#define DMSD_REG_CKTS		0x09
#define DMSD_REG_PLSE		0x0A
#define DMSD_REG_SESE		0x0B
#define DMSD_REG_GAIN		0x0C
#define DMSD_REG_STDC		0x0D
#define DMSD_REG_IOCK		0x0E
#define DMSD_REG_CTL3		0x0F
#define DMSD_REG_CTL4		0x10
#define DMSD_REG_CHCV           0x11
#define DMSD_REG_HS6B		0x14
#define DMSD_REG_HS6S		0x15
#define DMSD_REG_HC6B		0x16
#define DMSD_REG_HC6S		0x17
#define DMSD_REG_HP6I		0x18

#define DMSD_REG_STATUS         0xFF	/* not really a subaddress */

/*
 *  Phillips 7191 DMSD - Luminance Control Register definitions
 */

#define DMSD_BYPS_MASK		0x7F		/* bit 7 */
#define DMSD_BYPS_SHIFT		7
#define	DMSD_BYPS_SVID		0x80
#define DMSD_PREF_MASK		0xBF		/* bit 6 */
#define DMSD_PREF_SHIFT		6
#define DMSD_BPSS_MASK		0xCF		/* bits 5 - 4 */
#define DMSD_BPSS_SHIFT		4
#define DMSD_CORI_MASK		0xF3		/* bits 3 - 2 */
#define DMSD_CORI_SHIFT		2
#define DMSD_APER_MASK		0xFC		/* bits 1 - 0 */
#define DMSD_APER_SHIFT		0

/*
 *  Phillips 7191 DMSD - Colour-killer threshold QAM reg definitions
 */

#define DMSD_CKTQ_MASK		0x07		/* bits 7 - 3 */
#define DMSD_CKTQ_SHIFT		3

/*
 *  Phillips 7191 DMSD - Colour-killer threshold SECAM reg definitions
 */

#define DMSD_CKTS_MASK		0x07		/* bits 7 - 3 */
#define DMSD_CKTS_SHIFT		3

/*
 *  Phillips 7191 DMSD - Gain control register definitions
 */

#define DMSD_COLO_MASK		0x7F		/* bit 7 */
#define DMSD_COLO_SHIFT		7
#define DMSD_GAIN_MASK		0x9F		/* bits 6 - 5 */
#define DMSD_GAIN_SHIFT		5

/*
 *  Phillips 7191 DMSD - Standard/Mode control register definitions
 */

#define DMSD_VTRC_MASK		0x7F		/* bit 7 */
#define DMSD_VTRC_SHIFT		7
#define DMSD_NFEN_MASK		0xF7		/* bit 3 */
#define DMSD_NFEN_SHIFT		3
#define DMSD_HRMV_MASK		0xFB		/* bit 2 */
#define DMSD_HRMV_SHIFT		2
#define DMSD_SECS_MASK		0xFE		/* bit 0 */
#define DMSD_SECS_SHIFT		0

/*
 *  Phillips 7191 DMSD - I/O and Clock Control Register definitions
 */

#define DMSD_HPLL_MASK		0x7F		/* bit 7 */
#define DMSD_HPLL_SHIFT		7
#define DMSD_CHRS_MASK		0xFB		/* bit 2 */
#define DMSD_CHRS_SHIFT		2
#define DMSD_CHRS_SVID		0x04
#define DMSD_GPSW_MASK		0xFC		/* bits 1 - 0 */
#define DMSD_GPSW_SHIFT		0
#define DMSD_GPSW_CVBS		0x00
#define DMSD_GPSW_SVID		0x02

/*
 *  Phillips 7191 DMSD - Control #3 Register definitions
 */

#define DMSD_AUFD_MASK		0x7F		/* bit 7 */
#define DMSD_AUFD_SHIFT		7
#define DMSD_FSEL_MASK		0xBF		/* bit 6 */
#define DMSD_FSEL_SHIFT		6
#define DMSD_SXCR_MASK		0xDF		/* bit 5 */
#define DMSD_SXCR_SHIFT		5
#define DMSD_SCEN_MASK		0xEF		/* bit 4 */
#define DMSD_SCEN_SHIFT		4
#define DMSD_YDEL_MASK		0xF8		/* bits 2 - 0 */
#define DMSD_YDEL_SHIFT		0

/*
 *  Phillips 7191 DMSD - Control #4 Register definitions
 */

#define DMSD_VNOI_MASK		0xFC		/* bits 1 - 0 */
#define DMSD_VNOI_SHIFT		0

/*************************************************************************
 *
 *  Camera Controller (CDMC) register definitions
 *
 *************************************************************************/

/*
 * Camera CDMC register subaddresses.
 */

#define CDMC_REG_CTRL_STATUS	0x00
#define CDMC_REG_SHUTTER_SPEED	0x01
#define CDMC_REG_GAIN		0x02
#define CDMC_REG_BRIGHTNESS	0x03
#define CDMC_REG_RED_BAL	0x04
#define CDMC_REG_BLUE_BAL	0x05
#define CDMC_REG_RED_SAT	0x06
#define CDMC_REG_BLUE_SAT	0x07
#define CDMC_REG_VERSION	0x0E
#define CDMC_REG_MASTER_RESET	0x0F

#define CDMC_REG_READ_ADDR_MAX 	CDMC_REG_BLUE_SAT
#define CDMC_REG_WRITE_ADDR_MAX	CDMC_REG_BLUE_SAT

#endif /* __SYS_VINOHW_H__ */

