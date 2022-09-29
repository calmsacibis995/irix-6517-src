/*****************************************************************************
 * $Id: mvp7111.h,v 1.2 1997/08/18 20:38:58 philw Exp $
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRIRTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

/*
 * Interface module for 7111 Video Input Processor
 */

CHIPNAME(7111)

static i2c_chipreg_t chipregs_7111[] = {
RT( "chip version",		REV,	0x00, 0x00, 0x00, 0xff, RO ),
RT( "chip version2",		REV2,	0x01, 0x00, 0x00, 0xff, RO ),

 /* book: c0, dos: db */
 /* 0 - comp, 2 - loopback, 5, 7 - svideo */
RT( "analog input control 1",	AIC1,	0x02, 0xd8, 0xd8, 0xff, WO	),

 /* book: 43, dos: 23 */
RT( "analog input control 2",	AIC2,	0x03, 0x23, 0x24, 0x7f, WO	),
    
RT( "analog input control 3",	AIC3,	0x04, 0x00, 0x98, 0xff, WO	),
RT( "analog input control 4",	AIC4,	0x05, 0x00, 0xd4, 0xff, WO	),

 /* book: eb, e0,  dos: 0, 0 */
RT( "horizontal sync start",	HSS,	0x06, 0xeb, 0xeb, 0xff, WO	),
RT( "horizontal sync stop",	HSTP,	0x07, 0xe0, 0xe0, 0xff, WO	),

 /* book: 88, dos: 48 */
RT( "sync control",		SC,	0x08, 0x88, 0x88, 0xef, WO	),

 /* book: 01, dos: 10 */
 /* comp: 00, svideo: 80 */
RT( "luminance control",	LCR,	0x09, 0x02, 0x01, 0xff, WO	),

 /* book, dos: 80 */
RT( "luminance brightness",	LB,	0x0a, 0x7f, 0x84, 0xff, RW	),

 /* book, dos: 47 */
RT( "luminance contrast",	LCST,	0x0b, 0x4b, 0x49, 0xff, RW	),

 /* book, dos: 40 */
RT( "chrominance saturation",	CS,	0x0c, 0x44, 0x38, 0xff, RW	),

 /* book, dos: 0 */
RT( "chroma hue control",	CHC,	0x0d, 0xfe, 0xf8, 0xff, RW	),

 /* book, dos: 1 */
 /* pal/ntsc */
 /* dtco: 0x80 */
RT( "chrominance control",	CC,	0x0e, 0x01, 0x02, 0xff, WO	),

RT( "reserved",			RES,	0x0f, 0x00, 0x00, 0x00, NU ),

 /* book: 40, dos: 48 */
 /* c0: ccir 656-8 bits, 40: yuv422 16 */
RT( "format/delay control",	FDC,	0x10, 0xc7, 0xc7, 0xff, WO	),

 /* book: 1c, dos: 0c */
 /* digital video off = 0x80, digital video on = 0x00 */
RT( "output control 1",		OC1,	0x11, 0x0c, 0x0c, 0xbf, WO	),

 /* book: 01, dos: 80 */
 /* aout: 01:ain1, 02:ain2, 03:internal test pt. */
 /* 80: hlock on pin 39 */
RT( "output control 2",		OC2,	0x12, 0x81, 0x81, 0xdf, WO	),

RT( "reserved",			RES,	0x13, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x14, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x15, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x16, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x17, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x18, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x19, 0x00, 0x00, 0x00, NU ),

RT( "textslicer status",	TSS,	0x1a, 0x00, 0x00, 0xff, RO ),
RT( "textslicer byte 1",	TSB1,	0x1b, 0x00, 0x00, 0xff, RO ),
RT( "textslicer byte 2",	TSB2,	0x1c, 0x00, 0x00, 0xff, RO ),

RT( "reserved",			RES,	0x1d, 0x00, 0x00, 0x00, NU ),
RT( "reserved",			RES,	0x1e, 0x00, 0x00, 0x00, NU ),

RT( "status",			STAT,	0x1f, 0x00, 0x00, 0xff, RO ),
};

static i2c_chip_t chip_7111 = {
CT( "Video Input Processor", VIP, 7111, 0x48, 0x0, 0x1f, 0x1f, I2C_FLAGS_AUTOINCREMENT | I2C_FLAGS_USE_SUBADRS | I2C_FLAGS_PALMODE | I2C_FLAGS_FAST_MODE, chipregs_7111 )
};

#define	MVP_CHIP_7111_STAT_FIDT	0x020	/* field id bit */
#define	MVP_CHIP_7111_STAT_HLCK 0x040	/* horizontal lock */

/* === */
