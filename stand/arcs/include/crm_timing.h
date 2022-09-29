#ifndef __CRM_TIMING_H__
#define __CRM_TIMING_H__

/*
 * crm_timing.h
 *
 *	Brief description of purpose.
 *
 */

/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.3 $"

struct crime_timing_info crimeVTimings[ CRM_PROM_VT_SIZE ] = {
    {
	CRM_PROM_VT_NULL,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    },
    {
	CRM_PROM_VT_640_480_60,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	640,		480,		59940,		25175,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	800,	640,		800,		656,		752,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	525,	480,		525,		490,		492,
     /*	pll_m,	pll_n,		pll_p */
	5,	1,		2
    },

    {
	CRM_PROM_VT_800_600_60,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	800,		600,		60317,		40000,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1056,	800,		1056,		840,		968,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	628,	600,		628,		601,		605,
     /*	pll_m,	pll_n,		pll_p */
	8,	1,		2
    },

    {
	CRM_PROM_VT_800_600_72,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	800,		600,		72188,		50000,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1040,	800,		1040,		856,		976,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	666,	600,		666,		637,		643,
     /*	pll_m,	pll_n,		pll_p */
	5,	1,		1
    },

    {
	CRM_PROM_VT_1024_768_60,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1024,		768,		60004,		63546,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1344,	1024,		1344,		1048,		1184,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	806,	768,		806,		771,		777,
     /*	pll_m,	pll_n,		pll_p */
	13,	2,		1
    },

    {
	CRM_PROM_VT_1024_768_70,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1024,		768,		70069,		75000,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1328,	1024,		1328,		1048,		1184,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	806,	768,		806,		771,		777,
     /*	pll_m,	pll_n,		pll_p */
	15,	2,		1
    },

    {
	CRM_PROM_VT_1024_768_75,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1024,		768,		75029,		78750,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1312,	1024,		1312,		1040,		1136,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	800,	768,		800,		769,		772,
     /*	pll_m,	pll_n,		pll_p */
	39,	5,		1
    },


    {
	CRM_PROM_VT_1280_1024_48,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1280,		1024,		48000,		85882,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1680,	1280,		1680,		1360,		1480,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	1065,	1024,		1065,		1028,		1031,
     /*	pll_m,	pll_n,		pll_p */
	30,	7,		0
    },

    {
	CRM_PROM_VT_1280_1024_50,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1280,		1024,		50000,		89544,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1680,	1280,		1680,		1360,		1480,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	1065,	1024,		1065,		1027,		1030,
     /*	pll_m,	pll_n,		pll_p */
	9,	1,		1
    },

    {
	CRM_PROM_VT_1280_1024_60,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1280,		1024,		59940,		107245,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1680,	1280,		1680,		1310,		1430,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	1065,	1024,		1065,		1027,		1030,
     /*	pll_m,	pll_n,		pll_p */
	16,	3,		0
    },

    {
	CRM_PROM_VT_1280_1024_72,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1280,		1024,		72000,		128701,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1690,	1280,		1690,		1310,		1450,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	1064,	1024,		1064,		1027,		1030,
     /*	pll_m,	pll_n,		pll_p */
	13,	2,		0
    },

    {
	CRM_PROM_VT_1280_1024_75,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	0,	1280,		1024,		75025,		135000,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1688,	1280,		1688,		1296,		1440,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	1066,	1024,		1066,		1025,		1028,
     /*	pll_m,	pll_n,		pll_p */
	27,	4,		0
    },


    {
	CRM_PROM_VT_EXTRA_1,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    },


    {
     CRM_PROM_VT_1280_492_120,
     /*	flags,	width,		height,		fields_sec,	cfreq */
	CRIME_VOF_FS_STEREO,	
		1280,		492,		119880,		107072,
     /*	htotal,	hblank_start,	hblank_end,	hsync_start,	hsync_end */
	1680,	1280,		1680,		1310,		1430,
     /*	vtotal,	vblank_start,	vblank_end,	vsync_start,	vsync_end */
	532,	492,		532,		495,		498,
     /*	pll_m,	pll_n,		pll_p */
	16,	3,		0
    },


    {
	CRM_PROM_VT_EXTRA_2,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    },

    {
        CRM_PROM_VT_1600_1024_50,
     /* flags,  width,          height,         fields_sec,     cfreq */
        0,      1600,           1024,           50000,          101270,
     /* htotal, hblank_start,   hblank_end,     hsync_start,    hsync_end */
        1900,   1600,           1900,           1630,           1730,
     /* vtotal, vblank_start,   vblank_end,     vsync_start,    vsync_end */
        1066,   1024,           1066,           1027,           1030,
     /* pll_m,  pll_n,          pll_p */
        5,      1,              0
    }
};

#endif /* !__CRM_TIMING_H__ */
