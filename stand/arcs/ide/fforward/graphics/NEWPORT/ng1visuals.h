/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * ng1visuals.h
 * Newport visuals stuff.
 */
 
/* XXX Need a better data structure to simplify double/swap buffer */
typedef struct {
	int dm1bits;	/* Bits to set in drawmode1 for this visual */
	int wrmask;	/* Writemask for this visual */
	int unpkmask;	/* Mask an unpacked pixel in a word (Big Endian), */
			/* used for hostrw0,1 accesses only */
	int flags;	/* Other info about this visual */
	int modereg;    /* xmap mode register index for this visual */
	int mode;       /* xmap mode register value for this visual */
} ng1_visual;


/*
 * ng1_visual.dm1bits mask
 */
#define VISUAL_MASK \
	(DM1_LO_SHIFT | DM1_PLANES | DM1_DRAWDEPTH_MASK | DM1_HOSTDEPTH_MASK | DM1_RGBMODE)

/*
 * ng1_visual.flags bits
 */

#define V_24_ONLY 	1	/* This visual requires 24 bit frame buffer */
#define V_DB_OK 	2	/* Ok to double-buffer from this visual */
#define V_IS_FRONTBUF	4	/* This is a front buffer visual */
#define V_IS_BACKBUF	8	/* This is a back buffer visual */
#define V_IS_RGB        16      /* Pixels are rgb, not CI */


/* 
 * Names for the visuals, used as 
 * indices into the ng1visual array.
 */

#define V_RB24		0
#define V_CI12		1
#define V_RGBA20	2
#define V_OVL8		3
#define V_RGB8		4
#define V_CI8		5
#define V_CID2		6
#define V_PUP2		7
#define V_RGB12_FB	8
#define V_RGB12_BB	9
#define V_CI12_FB	10
#define V_CI12_BB	11
#define V_RGBA12_FB	12
#define V_RGBA12_BB	13
#define V_CI4_FB	14
#define V_CI4_BB	15
#define V_RGB4_FB	16
#define V_RGB4_BB	17

extern int ng1_setvisual();	/* called from minigl etc */
extern int ng1setvisual();	/* called from ide scripts/keyboard */

extern ng1_visual ng1visual[];	/* info about each visual */
extern int nvisuals;		/* number of visuals contained in ng1visual */
extern int currentvisual;	/* index in ng1visual[] of current visual */
#define NVISUALS nvisuals


