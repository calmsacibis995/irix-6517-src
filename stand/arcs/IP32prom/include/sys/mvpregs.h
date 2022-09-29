/*****************************************************************************
 * $Id: mvpregs.h,v 1.1 1997/08/18 20:46:39 philw Exp $
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

#ifndef	__MVPREGS_H
#define	__MVPREGS_H

#ifdef __cplusplus
/*extern "C" {*/
#endif

#ifndef	__MVP_REG_T
#define	__MVP_REG_T
typedef unsigned long long reg_t;
typedef unsigned long gbereg_t;
typedef	unsigned short short_t;
typedef	unsigned char byte_t;
#endif	/* __MVP_REG_T */

typedef struct control_reg_s {
    reg_t	pad:54;				/* 63:10 */
    reg_t	enable_gpi:1;			/* 9 */
    reg_t	enable_crime_memory_error:1;	/* 8 */
    reg_t	enable_fifo_overflow:1;		/* 7 */
    reg_t	enable_vertical_overflow:1;	/* 6 */
    reg_t	enable_horizontal_overflow:1;	/* 5 */
    reg_t	enable_buffer_field_overflow:1;	/* 4 */
    reg_t	enable_lost_sync:1;		/* 3 */
    reg_t	enable_dma_complete:1;		/* 2 */
    reg_t	enable_vertical_sync:1;		/* 1 */
    reg_t	enable_dma:1;			/* 0 */
} control_reg_s;

typedef struct control_word_s {
    unsigned short pad[3];			/* 63:16 */
    unsigned short control;			/* 15:0  */
} control_word_s;

typedef union control_reg_u {
    reg_t		word;
    control_reg_s	bits;
    control_word_s	enable;
} control_reg_t;

#define	MVP_CONTROL_ENABLE_DMA		0x001
#define	MVP_CONTROL_ENABLE_VERTSYNC	0x002
#define	MVP_CONTROL_ENABLE_DMACOMPLETE	0x004
#define	MVP_CONTROL_ENABLE_ERRORS	0x1f8
#define	MVP_CONTROL_ENABLE_FIFO_OVF	0x080
#define	MVP_CONTROL_ENABLE_GPI		0x200
#define	MVP_CONTROL_ENABLE_ALL		0x3ff

#define	MVP_CONTROL_DISABLE_ALL		0x000

typedef struct status_reg_s {
    reg_t	pad1:53;			/* 63:11 */
    reg_t	sync_present_status:1;		/* 10 */
    reg_t	gpi_interrupt:1;		/* 9 */
    reg_t	crime_memory_error_interrupt:1;	/* 8 */
    reg_t	fifo_overflow_interrupt:1;	/* 7 */
    reg_t	vertical_overflow_interrupt:1;	/* 6 */
    reg_t	horizontal_overflow_interrupt:1; /* 5 */
    reg_t	buffer_overflow_interrupt:1;	/* 4 */
    reg_t	lost_sync_interrupt:1;		/* 3 */
    reg_t	dma_complete_interrupt:1;	/* 2 */
    reg_t	vertical_sync_interrupt:1;	/* 1 */
    reg_t	dma_active_status:1;		/* 0 */
} status_reg_s;

typedef struct error_status_s {
    unsigned short pad[3];			/* 63:16 */
    unsigned short status;			/* 15:0  */
} error_status_s;

typedef union status_reg_u {
    reg_t		word;
    status_reg_s	bits;
    error_status_s	error;
} status_reg_t;

#define	MVP_STATUS_ERRORS_INPUT		0x1f8
#define	MVP_STATUS_ERRORS_OUTPUT	0x1f0

typedef struct input_config_reg_s {
    reg_t	pad2:31;			/* 63:33 */
    reg_t	reset_first:1;			/* 32    */
    reg_t	pad1:15;			/* 31:17 */
    reg_t	interleaved:1;			/* 16    */
    reg_t	mem_mode:2;			/* 15:14 */
    reg_t	dither:1;			/* 13    */
    reg_t	format:3;			/* 12:10 */
    reg_t	sof_count:4;			/* 9:6   */
    reg_t	ecc:1;				/* 5     */
    reg_t	precision:1;			/* 4     */
    reg_t	vin_source:2;			/* 3:2   */
    reg_t	d1_reset:1;			/* 1     */
    reg_t	channel_reset:1;		/* 0     */
} input_config_reg_s;

typedef union input_config_reg_u {
    reg_t		word;
    input_config_reg_s	bits;
} input_config_reg_t;

typedef struct output_config_reg_s {
    reg_t	pad1:31;			/* 63:33 */
    reg_t	reset_first:1;			/* 32    */
    reg_t	pad2:6;				/* 31:26 */
    reg_t	genlock_sof_count_even:4;	/* 25:22 */
    reg_t	mem_mode:1;			/* 21    */
    reg_t	interleaved:1;			/* 20    */
    reg_t	clamp:2;			/* 19:18 */
    reg_t	notch_filter:1;			/* 17    */
    reg_t	format:3;			/* 16:14 */
    reg_t	genlock_sof_count_odd:4;	/* 13:10 */
    reg_t	genlock_ecc:1;			/* 9     */
    reg_t	genlock_precision:1;		/* 8     */
    reg_t	genlock_source:2;		/* 7:6   -use SYNC_SOURCE_XXX */
    reg_t	genlock_reset:1;		/* 5     */
    reg_t	port_f_source:2;		/* 4:3   */
    reg_t	port_e_source:2;		/* 2:1   */
    reg_t	reset:1;			/* 0     */
} output_config_reg_s;

typedef union output_config_reg_u {
    reg_t		word;
    output_config_reg_s	bits;
} output_config_reg_t;

/* reset field */
#define	MVP_RESET			0
#define	MVP_ENABLE			1

/* vin_source */
#define	MVP_VIN_SOURCE_AB		0	/* analog/primary d1 */
#define	MVP_VIN_SOURCE_CD		1	/* moosecam/secondary d1 */
#define	MVP_VIN_SOURCE_E		2	/* loopback: output port e */
#define	MVP_VIN_SOURCE_F		3	/* loopback: output port f */

/* precision */
#define	MVP_PRECISION_8		0
#define	MVP_PRECISION_10		1

/* format */
#define	MVP_FORMAT_RGBA32		0	/* 8888 */
#define	MVP_FORMAT_RGBA16		1	/* 1555 */
#define	MVP_FORMAT_YUV422		2	/* YCrCb */
#define	MVP_FORMAT_YUV422_10		3	/* YCrCb: 10 bits/component */
#define	MVP_FORMAT_ABGR32		4	/* vino/cosmo mode */

/* mem_mode */
#define	MVP_MEM_MODE_LINEAR		0
#define	MVP_MEM_MODE_TILED		1
#define	MVP_MEM_MODE_128		2	/* Mipmapped 512x128 */
#define	MVP_MEM_MODE_256		3	/* Mipmapped 512x256 */

/* output source */
#define	MVP_VOUT_SOURCE_NONE		0
#define	MVP_VOUT_SOURCE_PASSTHRU	1
#define	MVP_VOUT_SOURCE_YUV		2
#define	MVP_VOUT_SOURCE_ALPHA	3

/* notch filter */
#define	MVP_NOTCH_FILTER_OFF		0
#define	MVP_NOTCH_FILTER_ON		1

/* clamp */
#define	MVP_CLAMP_NONE			0
#define	MVP_CLAMP_1_254			1
#define	MVP_CLAMP_10_250		2
#define	MVP_CLAMP_SPECIAL		3

/* loop source */
#define	MVP_LOOP_SOURCE_E		0
#define	MVP_LOOP_SOURCE_F		1

/* horizontal filter modes */
#define	MVP_FILTER_MODE_NONE		0	/* none */
#define	MVP_FILTER_MODE_SQ_PAL		1	/* non-square pal <--> square */
#define	MVP_FILTER_MODE_SQ_NTSC		2	/* non-square ntsc<--> square */
#define	MVP_FILTER_MODE_ZOOM_X2		3	/* zoom up/down X by 2 */

typedef struct next_desc_reg_s {
    unsigned int pad;			/* 63:32 */
    unsigned int address;		/* 31:0, 5:0 == 0! */
/*  reg_t	pad2:3;		*/	/* 5:3  */
/*  reg_t	valid:1;	*/	/* 2    */
/*  reg_t	capture:2;	*/	/* 1:0  */
} next_desc_reg_s;

typedef union next_desc_reg_u {
    reg_t		word;
    next_desc_reg_s	bits;
} next_desc_reg_t;

#define	NDA_ADDRESS_SHIFT	6
#define	NDA_VALID		4
#define	NDA_CAPTURE_NEXT_ANY	( 0 | NDA_VALID )
#define	NDA_CAPTURE_NEXT_ODD	( 2 | NDA_VALID )
#define	NDA_CAPTURE_NEXT_EVEN	( 3 | NDA_VALID )
#define	NDA_OUTPUT_NEXT_ODD	( 0 | NDA_VALID )
#define	NDA_OUTPUT_NEXT_EVEN	( 1 | NDA_VALID )

typedef struct field_offset_reg_s {
    short		pad[3];		/* 63:16 */
    unsigned short	offset;		/* 15:0  3:0 == 0! */
} field_offset_reg_s;

typedef union field_offset_reg_u {
    reg_t		word;
    field_offset_reg_s	bits;
} field_offset_reg_t;

typedef struct line_width_reg_s {
    short		pad[3];		/* 63:12 */
    short		width;		/* 11:0, 3:0 == 0! */
} line_width_reg_s;

typedef union line_width_reg_u {
    reg_t		word;
    line_width_reg_s	bits;
} line_width_reg_t;

typedef struct field_size_reg_s {
    reg_t	pad:42;			/* 63:22 */
    reg_t	lines:10;		/* 21:12 */
    reg_t	width:12;		/* 11:0, 3:0 == 0! */
} field_size_reg_s;

typedef union field_size_reg_u {
    reg_t		word;
    field_size_reg_s	bits;
} field_size_reg_t;

typedef struct h_clip_reg_s {
    reg_t	pad:23;				/* 63:41 */
    reg_t	filter_mode:2;			/* 40:39 */
    reg_t	scaling_on:1;			/* 38    */
    reg_t	scaling_ratio:8;		/* 37:30 */
    reg_t	filter_count:10;		/* 29:20 */
    reg_t	end:10;				/* 19:10 */
    reg_t	start:10;			/*  9: 0 */
} h_clip_reg_s;

typedef union h_clip_reg_u {
    reg_t		word;
    h_clip_reg_s	bits;
} h_clip_reg_t;

typedef struct v_clip_reg_s {
    reg_t	pad:25;				/* 63:39 */
    reg_t	scaling_on:1;			/* 38    */
    reg_t	scaling_ratio:8;		/* 37:30 */
    reg_t	end:10;				/* 29:20 */
    reg_t	blkend:10;			/* 19:10 */
    reg_t	start:10;			/*  9: 0 */
} v_clip_reg_s;

typedef union v_clip_reg_u {
    reg_t		word;
    v_clip_reg_s	bits;
} v_clip_reg_t;

typedef struct alpha_reg_s {
    unsigned char	pad[7];			/* 63:8  */
    unsigned char	alpha;			/* 7:0   */
} alpha_reg_s;

typedef union alpha_reg_u {
    reg_t		word;
    alpha_reg_s		bits;
} alpha_reg_t;

typedef struct h_pad_reg_s {
    reg_t	pad:22;				/* 63:42 */
    reg_t	filter_mode:2;			/* 41:40 */
    reg_t	filter_count:10;		/* 39:30 */
    reg_t	eav_image:10;			/* 29:20 */
    reg_t	eav_eav:10;			/* 19:10 */
    reg_t	eav_sav:10;			/* 9:0   */
} h_pad_reg_s;

typedef union h_pad_reg_u {
    reg_t		word;
    h_pad_reg_s		bits;
} h_pad_reg_t;

typedef struct v_pad_reg_s {
    reg_t	pad:24;				/* 63:40 */
    reg_t	sof_start_image:10;		/* 39:30 */
    reg_t	sof_eof:10;			/* 29:20 */
    reg_t	sof_start_vblank:10;		/* 19:10 */
    reg_t	sof_end_vblank:10;		/* 9:0   */
} v_pad_reg_s;

typedef union v_pad_reg_u {
    reg_t		word;
    v_pad_reg_s		bits;
} v_pad_reg_t;

typedef struct genlock_delay_reg_s {
    short		pad[3];			/* 63:11 */
    unsigned short	delay;			/* 10:0  */
} genlock_delay_reg_s;

typedef union genlock_delay_reg_u {
    reg_t		word;
    genlock_delay_reg_s	bits;
} genlock_delay_reg_t;

typedef struct vhw_cfg_reg_s {
    reg_t	pad:28;				/* 63:36 */

    reg_t	mace_revision:4;		/* 35:32 */

    reg_t	pad2:9;				/* 31:23 */

    reg_t	GBE_framelock_sof_count_even:4;	/* 22:19 */

    reg_t	GPI_output:1;			/* 18 */

    reg_t	GBE_framelock_sof_count_odd:4;	/* 17:14 */
    reg_t	GBE_framelock_ecc:1;		/* 13    */
    reg_t	GBE_framelock_precision:1;	/* 12    */
    reg_t	GBE_framelock_source:2;		/* 11:10 */
    reg_t	GBE_framelock_reset:1;		/* 9     */

    reg_t	AUDIO_sync_ecc:1;		/* 8     */
    reg_t	AUDIO_sync_precision:1;		/* 7     */
    reg_t	AUDIO_sync_source:2;		/* 6:5   */
    reg_t	AUDIO_sync_reset:1;		/* 4     */

    reg_t	CD_port:1;			/* 3     */
    reg_t	CD_reset:1;			/* 2     */

    reg_t	AB_port:1;			/* 1     */
    reg_t	AB_reset:1;			/* 0     */
} vhw_cfg_reg_s;

typedef union vhw_cfg_reg_u {
    reg_t		word;
    int			ints[2];
    vhw_cfg_reg_s	bits;
} vhw_cfg_reg_t;

/* AB/CD port selects */
#define	MVP_PORT_ANALOG		0	/* AB port */
#define	MVP_PORT_CAMERA		0	/* CD port */
#define	MVP_PORT_D1		1	/* AB (primary d1),  CD (secondary) */

#define	MVP_MAX_PAGES		32
#define	MVP_PAGE_SHIFT		16
#define	MVP_PAGE_MASK		0xffff
#define	MVP_DMA_PAGE_ALIGN	63

typedef struct dma_desc_reg_s {
    unsigned short	tile_number;		/* 15:0  */
} dma_desc_reg_t;

typedef struct mvp_input_channel_regs_s {
    control_reg_t	control;		/* 0x00 */
    status_reg_t	status;			/* 0x08 */
    input_config_reg_t	iconfig;		/* 0x10 */
    next_desc_reg_t	next_desc;		/* 0x18 */
    field_offset_reg_t	field_offset;		/* 0x20 */
    line_width_reg_t	line_width;		/* 0x28 */
    h_clip_reg_t	h_clip_odd;		/* 0x30 */
    v_clip_reg_t	v_clip_odd;		/* 0x38 */
    alpha_reg_t		alpha_odd;		/* 0x40 */
    h_clip_reg_t	h_clip_even;		/* 0x48 */
    v_clip_reg_t	v_clip_even;		/* 0x50 */
    alpha_reg_t		alpha_even;		/* 0x58 */
    reg_t		pad[4];			/* 0x60 .. 0x78 */
    dma_desc_reg_t	dma_desc[MVP_MAX_PAGES];/* 0x80 .. 0xb8 */
} mvp_input_channel_regs_t;

typedef struct mvp_output_channel_regs_s {
    control_reg_t	control;		/* 0x00 */
    status_reg_t	status;			/* 0x08 */
    output_config_reg_t	oconfig;		/* 0x10 */
    next_desc_reg_t	next_desc;		/* 0x18 */
    field_offset_reg_t	field_offset;		/* 0x20 */
    field_size_reg_t	field_size;		/* 0x28 */
    h_pad_reg_t		h_pad_odd;		/* 0x30 */
    v_pad_reg_t		v_pad_odd;		/* 0x38 */
    h_pad_reg_t		h_pad_even;		/* 0x40 */
    v_pad_reg_t		v_pad_even;		/* 0x48 */
    genlock_delay_reg_t	genlock_delay;		/* 0x50 */
    vhw_cfg_reg_t	vhw_cfg;		/* 0x58 */
    reg_t		pad[4];			/* 0x60 .. 0x78 */
    dma_desc_reg_t	dma_desc[MVP_MAX_PAGES];/* 0x80 .. 0xb8 */
} mvp_output_channel_regs_t;

/*
 * GBE Video Capture Registers
 * (Names are from the "Video Capture" spec. rev 11/17/96)
 */

typedef struct gbe_vc_0_reg_s {			/* 0x080000 */
    gbereg_t		pad:8;			/* 31:24 */
    gbereg_t		right:12;		/* 23:12 */
    gbereg_t		left:12;		/* 11:0  */
} gbe_vc_0_reg_s;

typedef union gbe_vc_0_reg_u {
    gbereg_t		word;
    gbe_vc_0_reg_s	bits;
} gbe_vc_0_reg_t;

typedef struct gbe_vc_1_reg_s {			/* 0x080004 */
    gbereg_t		pad:8;			/* 31:24 */
    gbereg_t		bottom:12;		/* 23:12 */
    gbereg_t		top:12;			/* 11:0  */
} gbe_vc_1_reg_s;

typedef union gbe_vc_1_reg_u {
    gbereg_t		word;
    gbe_vc_1_reg_s	bits;
} gbe_vc_1_reg_t;

typedef struct gbe_vc_2_reg_s {			/* 0x080008 */
    gbereg_t		pad:27;			/* 31:5  */
    gbereg_t		cscbypass:1;		/* 4     */
    gbereg_t		fullscreen:1;		/* 3     */
    gbereg_t		autofield:1;		/* 2     */
    gbereg_t		flicker:1;		/* 1     */
    gbereg_t		dmavideo:1;		/* 0     */
} gbe_vc_2_reg_s;

#define	MVP_VC_DISABLE_DMA	0x00		/* for vc_2 */

typedef union gbe_vc_2_reg_u {
    gbereg_t		word;
    gbe_vc_2_reg_s	bits;
} gbe_vc_2_reg_t;

#define	MVP_GBE_MAX_PAGES	10

typedef struct gbe_vc_3_reg_s {			/* 0x08000c */
    gbereg_t		dmadesc:27;		/* 31:5  */
    gbereg_t		odd_even:1;		/* 4     */
    gbereg_t		end_of_field:1;		/* 3     */
    gbereg_t		field_corrupt:1;	/* 2     */
    gbereg_t		enable_dmadesc:1;	/* 1     */
    gbereg_t		field:1;		/* 0     */
} gbe_vc_3_reg_s;


/* it's easier to OR in the following values to the dma address than it is *
 * to shift the address back and forth into the bits.dmadesc field      */
#define	MVP_VC_FIELD_EVEN	0x00
#define	MVP_VC_FIELD_ODD	0x01
#define	MVP_VC_ENABLE_DMADESC	0x02
#define	MVP_VC_FIELD_CORRUPT	0x04
#define	MVP_VC_END_OF_FIELD	0x08
#define	MVP_VC_ODDEVEN_EVEN	0x00
#define	MVP_VC_ODDEVEN_ODD	0x10

#define	MVP_STATUS_ERRORS_SCREEN	(MVP_VC_FIELD_CORRUPT)

typedef union gbe_vc_3_reg_u {
    gbereg_t		word;
    gbe_vc_3_reg_s	bits;
} gbe_vc_3_reg_t;

typedef struct gbe_dma_desc_reg_s {		/* 0x080010 */
    unsigned short	tile_number;		/* 15:0  */
} gbe_dma_desc_reg_t;

typedef struct gbe_dma_desc_pair_s {
    gbe_dma_desc_reg_t	dma_desc[2];
} gbe_dma_desc_pair_t;

typedef struct gbe_dma_desc_u {
    gbereg_t			word;
    /**** gbe_dma_desc_pair_t		bits;  ****/
    /**** gbe_dma_desc_reg_t	dma_desc[MVP_GBE_MAX_PAGES];  ****/
} gbe_dma_desc_t;


typedef struct gbe_video_capture_regs_s {
    gbe_vc_0_reg_t	vc_0;
    gbe_vc_1_reg_t	vc_1;
    gbe_vc_2_reg_t	vc_2;
    gbe_vc_3_reg_t	vc_3;
    /* gbe_dma_desc_t	vc_dma; */
    gbe_dma_desc_t	vc_4;
    gbe_dma_desc_t	vc_5;
    gbe_dma_desc_t	vc_6;
    gbe_dma_desc_t	vc_7;
    gbe_dma_desc_t	vc_8;
} mvp_screen_channel_regs_t;


/*
 * Moosehead I2c Video Control Registers
 */

typedef unsigned long long i2c_reg_t;

typedef struct i2c_config_s {
    i2c_reg_t	pad:58;				/* 63:6  */
    i2c_reg_t	clock_input:1;			/* 5     */
    i2c_reg_t	data_input:1;			/* 4     */
    i2c_reg_t	clock_override:1;		/* 3     */
    i2c_reg_t	data_override:1;		/* 2     */
    i2c_reg_t	fast_mode:1;			/* 1     */
    i2c_reg_t	reset:1;			/* 0     */
} i2c_config_s;

typedef union i2c_config_u {
    i2c_reg_t		word;
    i2c_config_s	bits;
} i2c_config_t;

typedef struct i2c_data_s {
    i2c_reg_t	pad:56;				/* 63:8 */
    i2c_reg_t	data:8;				/* 7:0  */
} i2c_data_s;

typedef union i2c_data_u {
    i2c_reg_t	word;
    i2c_data_s	bits;
} i2c_data_t;

typedef struct i2c_control_s {
    i2c_reg_t	pad:56;				/* 63:8  */
    i2c_reg_t	bus_error_status:1;		/* 7     */
    i2c_reg_t	pad2:1;				/* 6     */
    i2c_reg_t	acknowledge_status:1;		/* 5     */
    i2c_reg_t	transfer_status:1;		/* 4     */
    i2c_reg_t	pad3:1;				/* 3     */
    i2c_reg_t	last_byte:1;			/* 2     */
    i2c_reg_t	bus_direction:1;		/* 1     */
    i2c_reg_t	force_idle:1;			/* 0     */
} i2c_control_s;

/* status/control bit defines */
#define I2C_FORCE_IDLE  	(0 << 0)
#define I2C_NOT_IDLE    	(1 << 0)	/* 01 */
#define I2C_IDLE_BIT    	(1 << 0)	/* 01 */
#define I2C_WRITE       	(0 << 1)
#define I2C_READ        	(1 << 1)	/* 02 */
#define I2C_RELEASE_BUS 	(0 << 2)
#define I2C_HOLD_BUS    	(1 << 2)	/* 04 */
#define I2C_XFER_DONE   	(0 << 4)
#define I2C_XFER_BUSY   	(1 << 4)	/* 10 */
#define I2C_ACK         	(0 << 5)
#define I2C_NACK        	(1 << 5)	/* 20 */
#define I2C_BUS_OK      	(0 << 7)
#define I2C_BUS_ERR     	(1 << 7)	/* 80 */

#define I2C_IS_IDLE(status)	( ( ( status ) & I2C_NOT_IDLE ) == 0 )
#define I2C_BUS_ERROR(status)	( ( ( status ) & I2C_BUS_ERR ) )

/* config bit defines */
#define I2C_RESET		(1 << 0)	/* 01 */
#define I2C_NOT_RESET		(0 << 0)
#define I2C_FAST_MODE		(1 << 1)	/* 02 */
#define I2C_NOT_FAST_MODE	(0 << 1)

#define I2C_WRITE_ADRS		0
#define I2C_READ_ADRS		1

typedef union i2c_control_u {
    i2c_reg_t		word;
    i2c_control_s	bits;
} i2c_control_t;

typedef struct i2c_regs_s {
    i2c_config_t	i2c_config;
    i2c_reg_t		pad;
    i2c_control_t	i2c_control;
    i2c_data_t		i2c_data;
} i2c_regs_t;

/* either channel, no padding */
typedef union mvp_channel_regs_u {
    mvp_input_channel_regs_t	input;
    mvp_output_channel_regs_t	output;
    mvp_screen_channel_regs_t	screen;
    i2c_regs_t			i2c;
} mvp_channel_regs_t;

/* define the registers within their address spaces */
typedef union mvp_input_regs_range_u {
    mvp_input_channel_regs_t	reg;
    reg_t			pad[0x8000];
} mvp_input_regs_range_t;

typedef union mvp_output_regs_range_u {
    mvp_output_channel_regs_t	reg;
    reg_t			pad[0x8000];
} mvp_output_regs_range_t;

/* all registers, offset from MVP_BASE_ADDRESS */
typedef struct mvp_video_regs_s {
    mvp_input_regs_range_t	input[2];
    mvp_output_regs_range_t	output;
} mvp_video_regs_t;

typedef	struct mvp_interrupt_status_s {
    reg_t	pad:61;
    reg_t	vout_interrupt:1;
    reg_t	vin2_interrupt:1;
    reg_t	vin1_interrupt:1;
} mvp_interrupt_status_s;
#define	MVP_VIDEO_INTERRUPTS	0x7	/* 2:0 of crime interrupt stat regs */
#define	MVP_VIDEO_INPUT1_INTERRUPT	0x1
#define	MVP_VIDEO_INPUT2_INTERRUPT	0x2
#define	MVP_VIDEO_OUTPUT_INTERRUPT	0x4

typedef struct mvp_videoint_status_s {
    unsigned int pad;				/* 63:32 */
    unsigned int status;			/* 31:0  */
} mvp_videoint_status_t;

typedef union mvp_interrupt_status_u {
    mvp_interrupt_status_s	bits;
    mvp_videoint_status_t	intrpt;
    reg_t			word;
} mvp_interrupt_status_t;

typedef	struct mvp_interrupt_status_regs_s {
    reg_t			pad[2];
    mvp_interrupt_status_t	intstat;
    mvp_interrupt_status_t	intmask;
    mvp_interrupt_status_t	swintstat;
    mvp_interrupt_status_t	hwintstat;
} mvp_interrupt_status_regs_t;

/*
 * timer/counters are a somewhat wierd:
 *
 * Each 32 bit value of the ust's and the msc's are in a 64 bit location,
 * except when a 64 bit read of the msc address occurs, the ust is returned
 * in the upper 32 bits.
 * 
 * So reading and writing the values should be done with 32 bit i/o, while
 * atomic reads are done with 64 bit reads.
 */

/* example usage:
 *
 * (ust_msc_regs_t *)
 *	->input[0,1].bits.{msc,ust} = initial_value
 *	->output    .bits.{msc,ust} = initial_value
 *
 * (ust_msc_regs_t) current_value.value =
 *	(ust_msc_regs_t *)->bits.input[0,1].{msc,ust}
 *	(ust_msc_regs_t *)->bits.output    .{msc,ust}
 *
 * (ust_msc_regs_t) atomic_value.word = 
 *		(ust_msc_regset_t *) ->input[0,1].word
 *				     ->output.word
 *
 *	(int)ust = atomic_value.bits.ust
 *	(int)msc = atomic_value.bits.msc
 */

typedef struct ust_compare_regs_s {
    reg_t		word;			/* Actually only 31:0  */
} ust_compare_regs_t;

typedef struct ust_msc_regs_s {
    unsigned int	msc;			/* 63:32 (LSB is field id) */
    unsigned int	ust;			/* 31:0  */
} ust_msc_regs_s;

typedef union ust_msc_regs_u {
    reg_t		word;
    ust_msc_regs_s	bits;
} ust_msc_regs_t;

typedef	struct ust_msc_regset_s {
    ust_compare_regs_t	global_ust;
    ust_compare_regs_t	compare[3];
    ust_msc_regs_t 	audio[3];
    ust_msc_regs_t	video[3];
} ust_msc_regset_t;



#ifndef	K1BASE
#define	K1BASE	0xa0000000
#endif

#define	MVP_BASE_ADDRESS	(0x1f100000|K1BASE)
#define	MVP_VIDEO_IN1_ADDRESS	(mvp_input_channel_regs_t *)(0x1f100000|K1BASE)
#define	MVP_VIDEO_IN2_ADDRESS	(mvp_input_channel_regs_t *)(0x1f180000|K1BASE)
#define	MVP_VIDEO_OUT_ADDRESS	(mvp_output_channel_regs_t *)(0x1f200000|K1BASE)
#define	MVP_GBE_BASE_ADDRESS	(mvp_screen_channel_regs_t *)(0x16080000|K1BASE)
#define	MVP_I2C_BASE_ADDRESS	(i2c_regs_t *)(0x1f330000|K1BASE)
#define	MVP_INTERRUPT_STATUS	(mvp_interrupt_status_regs_t *)(0x14000000|K1BASE)
#define	MVP_UST_MSC_BASE_ADDRESS (ust_msc_regset_t *)(0x1f340000 | K1BASE)

/*
 * assembly routines to read and write 64 bit registers
 */
#if 0
reg_t	mvp_read_reg( reg_t *adrs );
void	mvp_write_reg( reg_t value, reg_t *adrs );
#else

/* use kernel routines because of assembler bug with sign extension */
#ifndef _SYS_CRIME_H__
reg_t	read_reg64( reg_t *adrs );
void	write_reg64( reg_t value, reg_t *adrs );
#endif
#define	mvp_read_reg(a)		read_reg64(a)
#define	mvp_write_reg(a,d)	write_reg64(a,d)
#endif

/* (don't need them for gbe as they are 32 bit regs) */
#define mvp_read_gbereg(adrs)	( *(volatile gbereg_t *)( adrs ) )
#define mvp_write_gbereg(adrs,val) \
    *(volatile gbereg_t *)( adrs ) = (gbereg_t)( val )

#ifdef	DEBUG
/*
 * trace register reads/writes
 */
reg_t	mvp_read_reg_debug( reg_t *adrs, char *name);
void	mvp_write_reg_debug( reg_t *adrs, reg_t value, char *name );
gbereg_t mvp_read_gbereg_debug( gbereg_t *adrs, char *name);
void	mvp_write_gbereg_debug( gbereg_t *adrs, gbereg_t value, char *name );

#define MVP_REG_READ(regbase,reg)	\
    ( (reg_t)mvp_read_reg_debug( &regbase->reg.word, # reg ) );

#define MVP_REG_WRITE(regbase,reg,val)	\
    mvp_write_reg_debug( &regbase->reg.word, (reg_t)(val), # reg );

#define MVP_GBEREG_READ(regbase,reg)	\
    ( (gbereg_t)mvp_read_gbereg_debug( &regbase->reg.word, # reg ) );

#define MVP_GBEREG_WRITE(regbase,reg,val)	\
    mvp_write_gbereg_debug( &regbase->reg.word, (gbereg_t)(val), # reg );

#elif (_MIPS_ISA == 3)
/*
 * 64 bit instructions available in compiler
 */
#define MVP_REG_READ(regbase,reg)	\
    ( *(volatile reg_t *)( regbase->reg.word ) )

#define MVP_REG_WRITE(regbase,reg,val)	\
    *(volatile reg_t *)( regbase->reg.word ) = (reg_t)( val )

#define MVP_GBEREG_READ(regbase,reg)	\
    ( *(volatile gbereg_t *)( regbase->reg.word ) )

#define MVP_GBEREG_WRITE(regbase,reg,val)	\
    *(volatile gbereg_t *)( regbase->reg.word ) = (gbereg_t)( val )

#else	/* _MIPS_ISA != 3 */
/*
 * 64 bit instructions not available in compiler, call assembly routines
 */
#define MVP_REG_READ(regbase,reg)	\
    ( (reg_t)mvp_read_reg( &regbase->reg.word ) );

#define MVP_REG_WRITE(regbase,reg,val)	\
    mvp_write_reg( (reg_t)(val), &regbase->reg.word );

#define MVP_GBEREG_READ(regbase,reg)	\
    ( (gbereg_t)mvp_read_gbereg( &regbase->reg.word ) );

#define MVP_GBEREG_WRITE(regbase,reg,val)	\
    mvp_write_gbereg( &regbase->reg.word, (gbereg_t)(val) );

#endif	/* !DEBUG & _MIPS_ISA != 3 */

/*
 * macro to sync the register with the new value;
 * regbase, shadow point to h/w register structs;
 * reg is register typedef'd name (word selects entire register, ie., reg_t);
 * value is the new register value (also a reg_t).
 */
#define MVP_REG_SYNC(regbase,shadow,reg,value)		\
	if ( shadow->reg.word != value ) {		\
	     shadow->reg.word  = value;			\
	     MVP_REG_WRITE( regbase, reg, value );	\
	}

#define MVP_GBEREG_SYNC(regbase,shadow,reg,value)	\
	if ( shadow->reg.word != value ) {		\
	     shadow->reg.word  = value;			\
	     MVP_GBEREG_WRITE( regbase, reg, value );	\
	}

/* unconditional "sync" */
#define MVP_REG_USYNC(regbase,shadow,reg,value)		\
	{ shadow->reg.word  = value;			\
	  MVP_REG_WRITE( regbase, reg, value );		\
	}

#define MVP_GBEREG_USYNC(regbase,shadow,reg,value)	\
	{ shadow->reg.word  = value;			\
	  MVP_GBEREG_WRITE( regbase, reg, value );	\
	}

/*
 * I2C Register read/write functions
 */
#define	MVP_I2CREG_READ		MVP_REG_READ
#define	MVP_I2CREG_WRITE	MVP_REG_WRITE
#define	MVP_I2CREG_SYNC		MVP_REG_SYNC


/* register address defines */
#if 0	/***  NOT USED !!! ***/
#define	CONTROL		0x0000		/* I/O */
#define	STATUS		0x0008		/* I/O */
#define	CONFIG		0x0010		/* I/O */
#define	NEXT_DESC	0x0018		/* I/O */
#define	FIELD_OFFSET	0x0020		/* I/O */
#define	LINE_WIDTH	0x0028		/* I/O */

#define	H_CLIP_ODD	0x0030		/* I   */
#define	V_CLIP_ODD	0x0038		/* I   */
#define	ALPHA_IN_ODD	0x0040		/* I   */

#define	H_CLIP_EVEN	0x0048		/* I   */
#define	V_CLIP_EVEN	0x0050		/* I   */
#define	ALPHA_IN_EVEN	0x0058		/* I   */

#define	H_PAD_ODD	0x0030		/*   O */
#define	V_PAD_ODD	0x0038		/*   O */
#define	H_PAD_EVEN	0x0040		/*   O */
#define	V_PAD_EVEN	0x0048		/*   O */

#define	GENLOCK_DELAY	0x0050		/*   O */
#define	VHW_CFG		0x0058		/*   O */

#define	DMA_DESC_CACHE	0x0080		/* I/O */
#define	DMA_DESC_CACHELEN 0x0038	/* I/O */

#define	MVP_INPUT_REG_MIN	CONTROL
#define	MVP_INPUT_REG_MAX	ALPHAIN_EVEN

#define	MVP_OUTPUT_REG_MIN	CONTROL
#define	MVP_OUTPUT_REG_MAX	VHW_CFG
#endif

#ifdef __cplusplus
/*}*/
#endif

#endif	/* __MVPREGS_H */

/* === */
