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
 *	3/18/98	PAG	created from rad4.h
 *
 ****/
#define TRUE 1
#define FALSE 0

#define SEPARATE		0		/* separate HSync and VSync */
#define COMPOSITE		1		/* HSync and VSync combined on HSync */
#define SYNC_ON_GREEN	2		/* HSync and VSync combined on Green */

#define SYNC_NEGATIVE	0	/* for negative going sync */
#define SYNC_POSITIVE	1	/* for positive going sync */

typedef struct _VIDEO_MODE_INFORMATION
{
	unsigned int ModeIndex;
	unsigned int VisScreenWidth;
	unsigned int VisScreenHeight;
	unsigned int Frequency;
} VIDEO_MODE_INFORMATION, *PVIDEO_MODE_INFORMATION;

/* Structure containing the mode set data */
typedef struct tagVDATA {
	unsigned int htotal;
	unsigned int hsstart;
	unsigned int hsend;
	unsigned int hbend;
	unsigned int hgend;
	unsigned int vtotal;
	unsigned int vsstart;
	unsigned int vsend;
	unsigned int vbend;
	unsigned int screenstride;
	unsigned int pllm;
	unsigned int plln;
	unsigned int pllp;
} VDATA, *PVDATA;

/* Structure containing the mode information */
typedef struct tagPER2INITDATA {
	VIDEO_MODE_INFORMATION modeInformation;
	VDATA VData;
} PER2INITDATA, *PPER2INITDATA;

/* Structure containing the partial product info */
typedef struct tagPER2PPINFO {
	unsigned int width;
	unsigned int pp0;
	unsigned int pp1;
	unsigned int pp2;
} PER2PPINFO;

/* external variables in per2data.c */

extern PER2INITDATA PER2Modes[];
extern unsigned int NumPER2Modes;
extern PER2PPINFO PER2pps[];
extern unsigned int NumPER2pps;
