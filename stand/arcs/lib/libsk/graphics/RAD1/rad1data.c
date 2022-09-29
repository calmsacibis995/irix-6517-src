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
 *	7/16/98		PAG	for rad1
 *	2/26/98		PAG	added 1024x1024x60
 *	10/10/97	WRW	changed VDATA v1280_1K_60[] timing parameters; reduced vblank
 *								by 1 line so active lines = 1024
 *	10/07/97	WRW	changed VDATA v1280_1K_60[] timing parameters be VESA standard
 *	8/28/97		PAG	corrected 2Kx1K to 2Kx1000
 *	8/21/97		WRW	Added Sony GDM-90W01 (1920 x 1080 x 75Hz)
 *	3/12/97		WRW	Added Hyundai HL 5864E 640x480, 800x600,1024x768x60/70Hz
 *								Reduced height of 2Kx1K from 1024 to 1000 to leave room
 *								in frame buffer for DMA chaining parameters
 *	11/20/96	WRW	Added  1200x1600 @ 76Hz and	1600x1200 @ 79Hz
 *								timing for use with Image Systems monitors
 *	8/13/96		PAG	Initial Design
 *
 ****/

#ifdef PER2_CONSOLE_ROM
#define ROMPRE static
#else /* PER2_CONSOLE_ROM */
#include "rad1.h"
#define ROMPRE
#if defined(ALLOC_PRAGMA)
#pragma data_seg("PAGE")
#endif
#endif /* PER2_CONSOLE_ROM */

/* */
/* PER2 mode information Tables. */
/* */
/* #define SGI_3_ONLY TRUE */


ROMPRE PER2INITDATA PER2Modes[] = {
	{
		/* VESA STANDARD Ver 1.0 Release 0.7 */
		0,				/* Mode index used in setting the mode */
		1280,			/* X Resolution, in pixels */
		1024,			/* Y Resolution, in pixels */
		60,				/* Screen Frequency, in Hertz. */
		1687,	/* horizontal total (number of 32 or 64 bit units -1) */
		48,		/* horizontal sync start (front porch) */
		160,	/* horizontal sync end (front porch + sync length) */
		408,	/* horizontal blanking end (blank length -1) */
		408,	/* horizontal gate end */
		1064,	/* vertical total in lines (number of lines -1) */
		0,	 	/* vertical sync start (front porch -1) */
		3,		/* vertical sync end (front porch + sync length -1) */
		41,		/* vertical blanking end (blank length) */
		640,	/* screen stride in 64bit units */
		181,	/* pixel clock PLL M factor */
		12,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
#ifndef SGI_3_ONLY
	{
		/* 800x600x32BPP@75Hz */
		/* taken from Hardware Reference Manual section 5.2.2 */
		0,				/* Mode index used in setting the mode */
		800,			/* X Resolution, in pixels */
		600,			/* Y Resolution, in pixels */
		75,				/* Screen Frequency, in Hertz. */
		1055,	/* horizontal total */
		16,		/* horizontal sync start (front porch) */
		96,		/* horizontal sync end (front porch + sync length) */
		256,	/* horizontal blanking end (blank length) */
		256,	/* horizontal gate end */
		624,	/* vertical total in lines */
		0,	 	/* vertical sync start (front porch) */
		3,		/* vertical sync end (front porch + sync length) */
		25,		/* vertical blanking end (blank length) */
		400,	/* screen stride in 64bit units */
		242,	/* pixel clock PLL M factor */
		35,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
#endif /* SGI_3_ONLY */
	{
		0,				/* Mode index used in setting the mode */
		1280,			/* X Resolution, in pixels */
		1024,			/* Y Resolution, in pixels */
		72,				/* Screen Frequency, in Hertz. */
		1695,	/* horizontal total (number of 32 or 64 bit units -1) */
		32,		/* horizontal sync start (front porch) */
		192,	/* horizontal sync end (front porch + sync length) */
		416,	/* horizontal blanking end (blank length -1) */
		416,	/* horizontal gate end */
		1062,	/* vertical total in lines (number of lines -1) */
		2,	 	/* vertical sync start (front porch -1) */
		5,		/* vertical sync end (front porch + sync length -1) */
		39,		/* vertical blanking end (blank length) */
		640,	/* screen stride in 64bit units */
		183,	/* pixel clock PLL M factor */
		10,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
#ifndef SGI_3_ONLY
	{
		/* HYUNDAI HL5864E */
		0,														/* Mode index used in setting the mode */
		640,													/* X Resolution, in pixels */
		480,													/* Y Resolution, in pixels */
		72,														/* Screen Frequency, in Hertz. */
		847,	/* horizontal total (number of 32 or 64 bit units -1) */
		28,		/* horizontal sync start (front porch) */
		68,		/* horizontal sync end (front porch + sync length) */
		208,	/* horizontal blanking end (blank length -1) */
		208,	/* horizontal gate end */
		522,	/* vertical total in lines (number of lines -1) */
		8,	 	/* vertical sync start (front porch -1) */
		11,		/* vertical sync end (front porch + sync length -1) */
		43,		/* vertical blanking end (blank length) */
		320,	/* screen stride in 64bit units */
		219,	/* pixel clock PLL M factor */
		49,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
	{
		/* HYUNDAI HL5864E */
		0,														/* Mode index used in setting the mode */
		800,													/* X Resolution, in pixels */
		600,													/* Y Resolution, in pixels */
		56,														/* Screen Frequency, in Hertz. */
		1023,	/* horizontal total (number of 32 or 64 bit units -1) */
		24,		/* horizontal sync start (front porch) */
		96,		/* horizontal sync end (front porch + sync length) */
		224,	/* horizontal blanking end (blank length -1) */
		224,	/* horizontal gate end */
		624,	/* vertical total in lines (number of lines -1) */
		1,	 	/* vertical sync start (front porch -1) */
		3,		/* vertical sync end (front porch + sync length -1) */
		50,		/* vertical blanking end (blank length) */
		400,	/* screen stride in 64bit units */
		176,	/* pixel clock PLL M factor */
		35,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
#endif /* SGI_3_ONLY */
	{
		/* HYUNDAI HL5864E */
		0,														/* Mode index used in setting the mode */
		1024,													/* X Resolution, in pixels */
		768,													/* Y Resolution, in pixels */
		60,													/* Screen Frequency, in Hertz. */
		1327,	/* horizontal total (number of 32 or 64 bit units -1) */
		48,		/* horizontal sync start (front porch) */
		112,	/* horizontal sync end (front porch + sync length) */
		304,	/* horizontal blanking end (blank length -1) */
		304,	/* horizontal gate end */
		798,	/* vertical total in lines (number of lines -1) */
		0,	 	/* vertical sync start (front porch -1) */
		4,		/* vertical sync end (front porch + sync length -1) */
		31,		/* vertical blanking end (blank length) */
		512,	/* screen stride in 64bit units */
		219,	/* pixel clock PLL M factor */
		49,		/* pixel clock PLL N factor */
		0,		/* pixel clock PLL P factor */
	},
#ifndef SGI_3_ONLY
	{
		/* HYUNDAI HL5864E */
		0,														/* Mode index used in setting the mode */
		1024,													/* X Resolution, in pixels */
		768,													/* Y Resolution, in pixels */
		70,														/* Screen Frequency, in Hertz. */
		1327,	/* horizontal total (number of 32 or 64 bit units -1) */
		48,		/* horizontal sync start (front porch) */
		112,	/* horizontal sync end (front porch + sync length) */
		304,	/* horizontal blanking end (blank length -1) */
		304,	/* horizontal gate end */
		805,	/* vertical total in lines (number of lines -1) */
		2,	 	/* vertical sync start (front porch -1) */
		8,		/* vertical sync end (front porch + sync length -1) */
		38,		/* vertical blanking end (blank length) */
		512,	/* screen stride in 64bit units */
		220,	/* pixel clock PLL M factor */
		21,		/* pixel clock PLL N factor */
		1,		/* pixel clock PLL P factor */
	},
	{
		0,														/* Mode index used in setting the mode */
		1024,													/* X Resolution, in pixels */
		1024,													/* Y Resolution, in pixels */
		60,													/* Screen Frequency, in Hertz. */
		1327,	/* horizontal total (number of 32 or 64 bit units -1) */
		36,		/* horizontal sync start (front porch) */
		56,		/* horizontal sync end (front porch + sync length) */
		304,	/* horizontal blanking end (blank length -1) */
		304,	/* horizontal gate end */
		1063,	/* vertical total in lines (number of lines -1) */
		3,	 	/* vertical sync start (front porch -1) */
		6,		/* vertical sync end (front porch + sync length -1) */
		40,		/* vertical blanking end (blank length) */
		512,	/* screen stride in 64bit units */
		95,		/* pixel clock PLL M factor */
		16,		/* pixel clock PLL N factor */
		0,		/* pixel clock PLL P factor */
	},
#endif /* SGI_3_ONLY */
};

#ifndef PER2_CONSOLE_ROM
ROMPRE unsigned int NumPER2Modes = sizeof(PER2Modes) / sizeof (PER2INITDATA);
#endif /* PER2_CONSOLE_ROM */

/* Screen Width Partial Product Table */
ROMPRE PER2PPINFO PER2pps[] = {
	{0,0,0,0},
	{32,1,0,0},
	{64,1,1,0},
	{96,1,1,1},
	{128,2,1,1},
	{160,2,2,1},
	{192,2,2,2},
	{224,3,2,1},
	{256,3,2,2},
	{288,3,3,1},
	{320,3,3,2},
	{384,3,3,3},
	{416,4,3,1},
	{448,4,3,2},
	{512,4,3,3},
	{544,4,4,1},
	{576,4,4,2},
	{640,4,4,3},
	{768,4,4,4},
	{800,5,4,1},
	{832,5,4,2},
	{896,5,4,3},
	{1024,5,4,4},
	{1056,5,5,1},
	{1088,5,5,2},
	{1152,5,5,3},
	{1280,5,5,4},
	{1536,5,5,5},
	{1568,6,5,1},
	{1600,6,5,2},
	{1664,6,5,3},
	{1792,6,5,4},
	{2048,6,5,5}
};

#ifndef PER2_CONSOLE_ROM
ROMPRE unsigned int NumPER2pps = sizeof(PER2pps) / sizeof (PER2PPINFO);

#if defined(ALLOC_PRAGMA)
#pragma data_seg()
#endif
#endif /* PER2_CONSOLE_ROM */





