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
 ***************************************************************************/

/*
 *  vino_diags.h - interface file for vino ide diagnostics
 *
 * $Revision: 1.4 $
 */

/*
 * VINO ASIC Register offsets for testing only
 * 
 */
#define TEST_REG_BASE			0x0

#define TEST_REG_REV_ID			(TEST_REG_BASE + 0x0)
#define TEST_REG_CONTROL		(TEST_REG_BASE + 0x8)
#define TEST_REG_INTR_STATUS		(TEST_REG_BASE + 0x10)
#define TEST_REG_I2C_CONTROL		(TEST_REG_BASE + 0x18)
#define TEST_REG_I2C_DATA		(TEST_REG_BASE + 0x20)

#define TEST_REG_CH_A_ALPHA		(TEST_REG_BASE + 0x28)
#define TEST_REG_CH_A_CLIP_START	(TEST_REG_BASE + 0x30)
#define TEST_REG_CH_A_CLIP_END		(TEST_REG_BASE + 0x38)
#define TEST_REG_CH_A_FRAME_RATE	(TEST_REG_BASE + 0x40)
#define TEST_REG_CH_A_FIELD_CNTR	(TEST_REG_BASE + 0x48)
#define TEST_REG_CH_A_LINE_SIZE		(TEST_REG_BASE + 0x50)
#define TEST_REG_CH_A_LINE_COUNT	(TEST_REG_BASE + 0x58)
#define TEST_REG_CH_A_PAGE_INDEX	(TEST_REG_BASE + 0x60)
#define TEST_REG_CH_A_NEXT_4_DESC	(TEST_REG_BASE + 0x68)
#define TEST_REG_CH_A_DESC_TBL_PTR	(TEST_REG_BASE + 0x70)
#define TEST_REG_CH_A_DESC_0		(TEST_REG_BASE + 0x78)
#define TEST_REG_CH_A_DESC_1		(TEST_REG_BASE + 0x80)
#define TEST_REG_CH_A_DESC_2		(TEST_REG_BASE + 0x88)
#define TEST_REG_CH_A_DESC_3		(TEST_REG_BASE + 0x90)
#define TEST_REG_CH_A_FIFO_THRSHLD	(TEST_REG_BASE + 0x98)
#define TEST_REG_CH_A_FIFO_READ		(TEST_REG_BASE + 0xA0)
#define TEST_REG_CH_A_FIFO_WRITE	(TEST_REG_BASE + 0xA8)

#define TEST_REG_CH_B_ALPHA		(TEST_REG_BASE + 0xB0)
#define TEST_REG_CH_B_CLIP_START	(TEST_REG_BASE + 0xB8)
#define TEST_REG_CH_B_CLIP_END		(TEST_REG_BASE + 0xC0)
#define TEST_REG_CH_B_FRAME_RATE	(TEST_REG_BASE + 0xC8)
#define TEST_REG_CH_B_FIELD_CNTR	(TEST_REG_BASE + 0xD0)
#define TEST_REG_CH_B_LINE_SIZE		(TEST_REG_BASE + 0xD8)
#define TEST_REG_CH_B_LINE_COUNT	(TEST_REG_BASE + 0xE0)
#define TEST_REG_CH_B_PAGE_INDEX	(TEST_REG_BASE + 0xE8)
#define TEST_REG_CH_B_NEXT_4_DESC	(TEST_REG_BASE + 0xF0)
#define TEST_REG_CH_B_DESC_TBL_PTR	(TEST_REG_BASE + 0xF8)
#define TEST_REG_CH_B_DESC_0		(TEST_REG_BASE + 0x100)
#define TEST_REG_CH_B_DESC_1		(TEST_REG_BASE + 0x108)
#define TEST_REG_CH_B_DESC_2		(TEST_REG_BASE + 0x110)
#define TEST_REG_CH_B_DESC_3		(TEST_REG_BASE + 0x118)
#define TEST_REG_CH_B_FIFO_THRSHLD	(TEST_REG_BASE + 0x120)
#define TEST_REG_CH_B_FIFO_READ		(TEST_REG_BASE + 0x128)
#define TEST_REG_CH_B_FIFO_WRITE	(TEST_REG_BASE + 0x130)

#define TRUE 1
#define FALSE 0

/* added from cv.h */
#define SUCCESS	0
#define FAILURE	-1

/* this gives approximately a 1/30 sec delay when used with DELAY(1) */
#define WAIT_TIMEOUT	33333

/* time in micro-seconds for DELAY() */
#define TIME_1_SECOND 1000000
#define MAX_DELAY_LOOP 5

/* inputs on the DMSD */
#define COMPOSITE (0)
#define SVIDEO (1<<7)
#define NO_SOURCE (255)

/* constants for DAM tests */
#define NTSC_CONST 0
#define PAL_CONST 1
#define TEST_CONST 2

#define INTR_EOFLD (1<<0)
#define INTR_FOF (1<<1)
#define INTR_EODT (1<<2)
#define INTR_NO_EOFLD (~((INTR_EOFLD) | (INTR_EOFLD<<4)))

#define STOP_BIT (1<<31)
#define JUMP_BIT (1<<30)
#define CLEAR_STP_JMP 0x3fffffff

#define ENABLE_INTERLEAVE ((1<<20) | (1<<8))

/* X=0 Ye=0 Yo=0 */
#define START_CLIP_0 0x0

/* X=640 Ye=240 Yo=240 */
/* #define END_CLIP_NTSC 0x783c280 */
/* X=639 Ye=239 Yo=239 */
#define END_CLIP_NTSC 0x77bbe7f

/* X=768 Ye=288 Yo=288 */
/* #define END_CLIP_PAL 0x9048300 */
/* X=767 Ye=287 Yo=287 */
#define END_CLIP_PAL 0x8fc7eff

/* X=100 Ye=100 Yo=100 */
#define END_CLIP_TINY 0x3219064

/* set threshold to half way point (128/2 -1)<<3 */
#define STAND_THRESHOLD 0x1f8

/* set threshold to generate a fifo overflow */
#define MAX_THRESHOLD 0x3f8

#define FRAMERATE_FULL 0x3ff

/* MC register stuff */
#define MC_BASE 0x1fa00000
#define GIO64_ARB_ADDR 0x84

#define LOCK_MEMORY 0x1fa0010c
#define EISA_LOCK 0x1fa00114
#define MC_CPUCTRL0 0x4
#define MC_CPUCTRL0_DATA 0x00842501
#define MC_SYSID 0x1c

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
 *  Vino ASIC - ID and Revision Register
 */

#define VINO_ID_VALUE(reg)	(((reg) & 0xF0) >> 4)
#define VINO_CHIP_ID		0xB
#define REV_NUM_MASK		0x0F

#define BIT(n)	(0x1 << n)

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
#define CDMC_ADDR_ASIC	0x56
#define CDMC_ADDR_FPGA	0xAE

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
