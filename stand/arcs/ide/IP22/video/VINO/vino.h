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

#ifndef __SYS_VINO_H__
#define __SYS_VINO_H__

/*
 *  vino.h - interface file for vino video driver
 *
 * $Revision: 1.2 $
 */

typedef struct {
        int reg;
        int val;
} RegVal;

extern int vino_probe(void);
int vino_initmc(void);

int vinoI2cReadReg(VinoBoard *, int, int);
int vinoI2cWriteBlock (VinoBoard *, int , int , int *);
extern int vino_7191_inputs(uchar);
extern int vino_7191_init(void);
int vinoI2cWriteReg (VinoBoard *, int, int, int);
int i2cForceBusIdle (VinoRegs);
int vinoI2cReadStatus (VinoBoard *, int);

/*
 *  Vino source video node definitions
 */

#define VINO_SRC_VIDEO_0	0
#define VINO_SRC_VIDEO_1	1

/**************************************************************************
 *
 *  Video format parameter definitions
 *
 **************************************************************************/

/*
 *  NTSC field format definitions
 */

#define NTSC_ACTIVE_FULLSCREEN_X		640
#define NTSC_ACTIVE_FULLSCREEN_Y_FIELD		240

#define NTSC_ACTIVE_XSTART			 0
#define NTSC_ACTIVE_YODD_START			20
#define NTSC_ACTIVE_YEVEN_START			21

#define NTSC_ACTIVE_FULLSCREEN_CLIP_START	\
	NTSC_ACTIVE_XSTART | 			\
	NTSC_ACTIVE_YODD_START << 10 | 		\
	NTSC_ACTIVE_YEVEN_START << 19

#define NTSC_ACTIVE_XEND			\
	NTSC_ACTIVE_XSTART + NTSC_ACTIVE_FULLSCREEN_X

#define NTSC_ACTIVE_YODD_END			\
	NTSC_ACTIVE_YODD_START + NTSC_ACTIVE_FULLSCREEN_Y_FIELD

#define NTSC_ACTIVE_YEVEN_END			\
	NTSC_ACTIVE_YEVEN_START + NTSC_ACTIVE_FULLSCREEN_Y_FIELD

#define NTSC_ACTIVE_FULLSCREEN_CLIP_END		\
	NTSC_ACTIVE_XEND | 			\
	NTSC_ACTIVE_YODD_END << 10 | 		\
	NTSC_ACTIVE_YEVEN_END << 19

#define NTSC_MAX_FRAME_RATE		0x3FF << 1 | 0
#define PAL_MAX_FRAME_RATE		0x3FF << 1 | 1

/**************************************************************************
 *
 *  Vino register definitions used with V_GET_REG and V_SET_REG ioctls.
 *
 **************************************************************************/

/*
 * Vino ASIC registers.
 */

#define V_VINO_REG_BASE			0

#define V_VINO_REG_REV_ID		V_VINO_REG_BASE + 0
#define V_VINO_REG_CONTROL		V_VINO_REG_BASE + 1
#define V_VINO_REG_INTR_STATUS		V_VINO_REG_BASE + 2
#define V_VINO_REG_I2C_CONTROL		V_VINO_REG_BASE + 3
#define V_VINO_REG_I2C_DATA		V_VINO_REG_BASE + 4

#define V_VINO_REG_A_ALPHA		V_VINO_REG_BASE + 5
#define V_VINO_REG_A_CLIP_START		V_VINO_REG_BASE + 6
#define V_VINO_REG_A_CLIP_END		V_VINO_REG_BASE + 7
#define V_VINO_REG_A_FRAME_RATE		V_VINO_REG_BASE + 8
#define V_VINO_REG_A_FIELD_CNTR		V_VINO_REG_BASE + 9
#define V_VINO_REG_A_LINE_SIZE		V_VINO_REG_BASE + 10
#define V_VINO_REG_A_LINE_COUNT		V_VINO_REG_BASE + 11
#define V_VINO_REG_A_PAGE_INDEX		V_VINO_REG_BASE + 12
#define V_VINO_REG_A_NEXT_4_DESC	V_VINO_REG_BASE + 13
#define V_VINO_REG_A_DESC_TBL_PTR	V_VINO_REG_BASE + 14
#define V_VINO_REG_A_DESC_0		V_VINO_REG_BASE + 15
#define V_VINO_REG_A_DESC_1		V_VINO_REG_BASE + 16
#define V_VINO_REG_A_DESC_2		V_VINO_REG_BASE + 17
#define V_VINO_REG_A_DESC_3		V_VINO_REG_BASE + 18
#define V_VINO_REG_A_FIFO_THRSHLD	V_VINO_REG_BASE + 19
#define V_VINO_REG_A_FIFO_READ		V_VINO_REG_BASE + 20
#define V_VINO_REG_A_FIFO_WRITE		V_VINO_REG_BASE + 21

#define V_VINO_REG_B_ALPHA		V_VINO_REG_BASE + 22
#define V_VINO_REG_B_CLIP_START		V_VINO_REG_BASE + 23
#define V_VINO_REG_B_CLIP_END		V_VINO_REG_BASE + 24
#define V_VINO_REG_B_FRAME_RATE		V_VINO_REG_BASE + 25
#define V_VINO_REG_B_FIELD_CNTR		V_VINO_REG_BASE + 26
#define V_VINO_REG_B_LINE_SIZE		V_VINO_REG_BASE + 27
#define V_VINO_REG_B_LINE_COUNT		V_VINO_REG_BASE + 28
#define V_VINO_REG_B_PAGE_INDEX		V_VINO_REG_BASE + 29
#define V_VINO_REG_B_NEXT_4_DESC	V_VINO_REG_BASE + 30
#define V_VINO_REG_B_DESC_TBL_PTR	V_VINO_REG_BASE + 31
#define V_VINO_REG_B_DESC_0		V_VINO_REG_BASE + 32
#define V_VINO_REG_B_DESC_1		V_VINO_REG_BASE + 33
#define V_VINO_REG_B_DESC_2		V_VINO_REG_BASE + 34
#define V_VINO_REG_B_DESC_3		V_VINO_REG_BASE + 35
#define V_VINO_REG_B_FIFO_THRSHLD	V_VINO_REG_BASE + 36
#define V_VINO_REG_B_FIFO_READ		V_VINO_REG_BASE + 37
#define V_VINO_REG_B_FIFO_WRITE		V_VINO_REG_BASE + 38

/*
 *  7191 DMSD registers.
 */

#define V_DMSD_REG_BASE			V_VINO_REG_BASE + 39

#define V_DMSD_REG_IDEL                 V_DMSD_REG_BASE + 0
#define V_DMSD_REG_HSYB			V_DMSD_REG_BASE + 1
#define V_DMSD_REG_HSYS			V_DMSD_REG_BASE + 2
#define V_DMSD_REG_HCLB			V_DMSD_REG_BASE + 3
#define V_DMSD_REG_HCLS			V_DMSD_REG_BASE + 4
#define V_DMSD_REG_HPHI			V_DMSD_REG_BASE + 5
#define V_DMSD_REG_LUMA                 V_DMSD_REG_BASE + 6
#define V_DMSD_REG_HUEC			V_DMSD_REG_BASE + 7
#define V_DMSD_REG_CKTQ			V_DMSD_REG_BASE + 8
#define V_DMSD_REG_CKTS			V_DMSD_REG_BASE + 9
#define V_DMSD_REG_PLSE			V_DMSD_REG_BASE + 10
#define V_DMSD_REG_SESE			V_DMSD_REG_BASE + 11
#define V_DMSD_REG_GAIN			V_DMSD_REG_BASE + 12
#define V_DMSD_REG_STDC			V_DMSD_REG_BASE + 13
#define V_DMSD_REG_IOCK			V_DMSD_REG_BASE + 14
#define V_DMSD_REG_CTL3			V_DMSD_REG_BASE + 15
#define V_DMSD_REG_CTL4			V_DMSD_REG_BASE + 16
#define V_DMSD_REG_CHCV                 V_DMSD_REG_BASE + 17
#define V_DMSD_REG_HS6B			V_DMSD_REG_BASE + 18
#define V_DMSD_REG_HS6S			V_DMSD_REG_BASE + 19
#define V_DMSD_REG_HC6B			V_DMSD_REG_BASE + 20
#define V_DMSD_REG_HC6S			V_DMSD_REG_BASE + 21
#define V_DMSD_REG_HP6I			V_DMSD_REG_BASE + 22

/*
 *  Camera controller registers.
 */

#define V_CDMC_REG_BASE			V_DMSD_REG_BASE + 23

#define V_CDMC_REG_CTRL_STATUS		V_CDMC_REG_BASE + 0
#define V_CDMC_REG_SHUTTER_SPEED	V_CDMC_REG_BASE + 1
#define V_CDMC_REG_GAIN			V_CDMC_REG_BASE + 2
#define V_CDMC_REG_BRIGHTNESS		V_CDMC_REG_BASE + 3
#define V_CDMC_REG_RED_BAL		V_CDMC_REG_BASE + 4
#define V_CDMC_REG_BLUE_BAL		V_CDMC_REG_BASE + 5
#define V_CDMC_REG_RED_SAT		V_CDMC_REG_BASE + 6
#define V_CDMC_REG_BLUE_SAT		V_CDMC_REG_BASE + 7
#define V_CDMC_REG_MASTER_RESET		V_CDMC_REG_BASE + 8

/*
 *  The V_MAX_REG must have a value equal to the highest 
 *  numbered register.
 */

#define V_MAX_REG			V_CDMC_REG_BASE + 8

#endif /* __SYS_VINO_H__ */

