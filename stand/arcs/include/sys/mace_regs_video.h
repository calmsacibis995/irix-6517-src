/*****************************************************************************
 * $Id: mace_regs_video.h,v 1.1 1996/01/18 17:29:10 montep Exp $
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
 *****************************************************************************/

/*
 * Moosehead Video Registers
 *
 */

#ifdef __cplusplus
/*extern "C" {*/
#endif

#ifndef	__MACE_REG_T
#define	__MACE_REG_T
typedef unsigned long long reg_t;
#endif	/* __MACE_REG_T */

typedef struct videohwregs_s {
    reg_t	vhw_cfg;
} videohwregs_s;

typedef union videohwregs_u {
    videohwregs_s	reg;
    reg_t		pad[0x8000];
} vidhwregs_t;

typedef struct controlreg_s {
    reg_t	pad:56;				/* 63:8 */
    reg_t	enable_fifo_ovflw:1;		/* 7 */
    reg_t	enable_vertical_ovrflw:1;	/* 6 */
    reg_t	enable_horizontal_ovrflw:1;	/* 5 */
    reg_t	enable_buffer_ovrflw:1;		/* 4 */
    reg_t	enable_lostsync:1;		/* 3 */
    reg_t	enable_dmacomplete:1;		/* 2 */
    reg_t	enable_verticalsync:1;		/* 1 */
    reg_t	enable_dma:1;			/* 0 */
} controlreg_s;

typedef union controlreg_u {
    reg_t		word;
    controlreg_s	bits;
} controlreg_t;

typedef struct statusreg_s {
    reg_t	pad1:54;			/* 63:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} statusreg_s;

typedef union statusreg_u {
    reg_t		word;
    statusreg_s		bits;
} statusreg_t;

typedef struct configinreg_s {
    reg_t	pad1:53;			/* 63:11 */
    reg_t	vin_source:2;			/* 10:9 */
    reg_t	mode:2;				/* 8:7 */
    reg_t	interleaved:1;			/* 6 */
    reg_t	ecc:1;				/* 5 */
    reg_t	precision:1;			/* 4 */
    reg_t	format:3;			/* 3:1 */
    reg_t	reset:1;			/* 0 */
} configinreg_s;

typedef union configinreg_u {
    reg_t		word;
    configinreg_s	bits;
} configinreg_t;

typedef struct configoutreg_s {
    reg_t	pad1:35;			/* 63:19 */
    reg_t	audio_genlock:2;		/* 18:17 */
    reg_t	gbe_genlock:2;			/* 16:15 */
    reg_t	out_genlock:2;			/* 14:13 */
    reg_t	svout_source:2;			/* 12:11 */
    reg_t	pvout_source:2;			/* 10:9 */
    reg_t	mode:2;				/* 8:7 */
    reg_t	interleaved:1;			/* 6 */
    reg_t	alpha_out:1;			/* 5 */
    reg_t	precision:1;			/* 4 */
    reg_t	format:3;			/* 3:1 */
    reg_t	reset:1;			/* 0 */
} configoutreg_s;

/* reset field */
#define	CONFIG_ENABLE			0
#define	CONFIG_RESET			1

/* format */
#define	CONFIG_FORMAT_RGBA8		0
#define	CONFIG_FORMAT_ABGR8		1
#define	CONFIG_FORMAT_RGBA5_DITHERED	2
#define	CONFIG_FORMAT_RGBA5		3
#define	CONFIG_FORMAT_YUV8		4
#define	CONFIG_FORMAT_YUV10		5

/* precision */
#define	CONFIG_PRECISION_8		0
#define	CONFIG_PRECISION_10		1

/* ecc */
#define	CONFIG_ECC_OFF			0
#define	CONFIG_ECC_ON			1

/* alpha_out */
#define	CONFIG_YUVONLY			0
#define	CONFIG_YUVA			1

/* interleaved */
#define	CONFIG_FIELDS			0
#define	CONFIG_INTERLEAVED		1

/* mode */
#define	CONFIG_LINEAR			0
#define	CONFIG_TILED			2
#define	CONFIG_MIPMAPPED		3

/* pvout_source, svout_source */
#define	CONFIG_YUVOUT			0
#define	CONFIG_ALPHAOUT			1
#define	CONFIG_PRIMD1OUT		2
#define	CONFIG_SECD1OUT			3

/* out_genlock, gbe_genlock, audio_genlock */
#define	CONFIG_INTERNALTIMING		0
#define	CONFIG_PRIMD1TIMING		2
#define	CONFIG_SECD1TIMING		3

typedef union configoutreg_u {
    reg_t		word;
    configoutreg_s	bits;
} configoutreg_t;

typedef struct clipreg_s {
    reg_t	scaling_on:1;			/* 63 */
    reg_t	pad:7;				/* 62:56 */
    reg_t	scaling_ratio:8;		/* 55:48 */
    reg_t	count:16;			/* 47:32 */
    reg_t	end:16;				/* 31:16 */
    reg_t	start:16;			/* 15:0 */
} clipreg_s;

typedef union clipreg_u {
    reg_t		word;
    clipreg_s		bits;
} clipreg_t;

typedef struct padreg_s {
    reg_t	pad1:16;			/* 63:48 */
    reg_t	count:16;			/* 47:32 */
    reg_t	pad2:16;			/* 31:16 */
    reg_t	start:16;			/* 15:0 */
} padreg_s;

typedef union padreg_u {
    reg_t		word;
    padreg_s		bits;
} padreg_t;

typedef struct filtinreg_s {
    reg_t	pad1:22;			/* 31:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} filtinreg_s;

typedef union filtinreg_u {
    reg_t		word;
    filtinreg_s		bits;
} filtinreg_t;

typedef struct filtoutreg_s {
    reg_t	pad1:22;			/* 31:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} filtoutreg_s;

typedef union filtoutreg_u {
    reg_t		word;
    filtoutreg_s	bits;
} filtoutreg_t;

typedef struct vtimoutreg_s {
    reg_t	pad1:22;			/* 31:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} vtimoutreg_s;

typedef union vtimoutreg_u {
    reg_t		word;
    vtimoutreg_s	bits;
} vtimoutreg_t;

typedef struct intvtimreg_s {
    reg_t	pad1:22;			/* 31:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} intvtimreg_s;

typedef union intvtimreg_u {
    reg_t		word;
    intvtimreg_s	bits;
} intvtimreg_t;

typedef struct syncdelayreg_s {
    reg_t	pad1:22;			/* 31:10 */
    reg_t	dma_complete_status:1;		/* 9 */
    reg_t	sync_present_status:1;		/* 8 */
    reg_t	fifo_ovrflw:1;			/* 7 */
    reg_t	vertical_ovrflw:1;		/* 6 */
    reg_t	horizontal_ovrflw:1;		/* 5 */
    reg_t	buffer_ovrflw:1;		/* 4 */
    reg_t	lost_sync:1;			/* 3 */
    reg_t	dma_complete:1;			/* 2 */
    reg_t	vertical_sync:1;		/* 1 */
    reg_t	pad2:1;				/* 0 */
} syncdelayreg_s;

typedef union syncdelayreg_u {
    reg_t		word;
    syncdelayreg_s	bits;
} syncdelayreg_t;

typedef struct vidinchregs_s {
    controlreg_t	control;
    statusreg_t		status;
    configinreg_t	config;
    reg_t		next_desc;
    reg_t		fld_offset;
    reg_t		line_width;
    clipreg_t		hclip_odd;
    clipreg_t		vclip_odd;
    clipreg_t		hclip_even;
    clipreg_t		vclip_even;
    filtinreg_t		filt_in;
    reg_t		alpha_in;
} vidinchregs_s;

typedef union vidinchregs_u {
    vidinchregs_s	reg;
    reg_t		pad[0x8000];
} vidinchregs_t;

typedef struct vidoutchregs_s {
    controlreg_t	control;
    statusreg_t		status;
    configoutreg_t	config;
    reg_t		next_desc;
    reg_t		fld_offset;
    reg_t		line_size;
    padreg_t		hpad_odd;
    padreg_t		vpad_odd;
    padreg_t		hpad_even;
    padreg_t		vpad_even;
    filtoutreg_t	filt_out;
    vtimoutreg_t	vtim_out;
    intvtimreg_t	int_vtim;
    syncdelayreg_t	sync_delay;
} vidoutchregs_s;

typedef union vidoutchregs_u {
    vidoutchregs_s	reg;
    reg_t		pad[0x8000];
} vidoutchregs_t;

typedef struct vidregs {
    vidhwregs_t		hw;
    vidinchregs_t	vidin[2];
    vidoutchregs_t	vidout;
} vidregs_t;

#define	CONTROL		0x0000		/* I/O */
#define	STATUS		0x0008		/* I/O */
#define	CONFIG		0x0010		/* I/O */
#define	NEXT_DESC	0x0018		/* I/O */
#define	FLD_OFFSET	0x0020		/* I/O */
#define	LINE_WIDTH	0x0028		/* I/O */

#define	HCLIP_ODD	0x0030		/* I   */
#define	VCLIP_ODD	0x0038		/* I   */
#define	ALPHAIN_ODD	0x0040		/* I   */

#define	HCLIP_EVEN	0x0048		/* I   */
#define	VCLIP_EVEN	0x0050		/* I   */
#define	ALPHAIN_EVEN	0x0058		/* I   */

#define	HPAD_ODD	0x0030		/*   O */
#define	VPAD_ODD	0x0038		/*   O */
#define	HPAD_EVEN	0x0040		/*   O */
#define	VPAD_EVEN	0x0048		/*   O */

#define	GEN_DELAY	0x0050		/*   O */
#define	VHW_CFG		0x0058		/*   O */

#define	VID_DESC_CACHE	0x0080		/* I/O */
#define	VID_DESC_CACHELEN 0x0038	/* I/O */

#define	MV_IN_REG_MIN		CONTROL
#define	MV_IN_REG_MAX		ALPHAIN_EVEN

#define	MV_OUT_REG_MIN		CONTROL
#define	MV_OUT_REG_MAX		VHW_CFG

#ifdef __cplusplus
/*}*/
#endif

/* === */
