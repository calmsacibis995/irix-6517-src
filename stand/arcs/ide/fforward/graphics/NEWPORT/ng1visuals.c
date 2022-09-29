/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Newport visuals routines
 *  $Revision: 1.10 $
 */

#include <sys/param.h>
#include <sys/sbd.h>
#include "sys/ng1hw.h"
#include "ide_msg.h"
#include "gl/rex3macros.h"
#include "ng1visuals.h"
#include "sys/xmap9.h"
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/ng1.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern Rex3chip *REX;
extern int REXindex;

/* Cached drawmode registers. */ 

extern int dm1;
extern struct ng1_info ng1_ginfo[];
extern struct ng1_timing_info *ng1_timing[];
int currentvisual = V_CI8;

#define _setbits(d, mask, bits) d &= ~mask; d |= bits

/*
 * All the Newport visuals / frame buffer formats.
 * XXX We don't have support for underlay or
 * double-buffered aux planes.
 */

#define X9_CMAP(num)  ( num << XM9_MSB_CMAP_SHIFT )
#define X9_AMAP(num)  ( num << XM9_AUX_MSB_CMAP_SHIFT )
#define X9_RGB0       ( 0x01 << XM9_PIX_MODE_SHIFT )
#define X9_RGB1       ( 0x02 << XM9_PIX_MODE_SHIFT )
#define X9_RGB2       ( 0x03 << XM9_PIX_MODE_SHIFT )
#define X9_4BIT       ( 0x00 << XM9_PIX_SIZE_SHIFT )
#define X9_8BIT       ( 0x01 << XM9_PIX_SIZE_SHIFT )
#define X9_12BIT      ( 0x02 << XM9_PIX_SIZE_SHIFT )
#define X9_24BIT      ( 0x03 << XM9_PIX_SIZE_SHIFT )
#define X9_OLAY       ( XM9_AUX_MODE_OLAY )
#define X9_BB         ( XM9_PIX_BUF1)

#define X9_CIMODE	( XM9_PIXSIZE_8 & ~XM9_PIX_MODE_MASK )
#define X9_RGBMODE	( XM9_PIXSIZE_8 & ~XM9_PIX_MODE_MASK ) | \
			( 1 << XM9_PIX_MODE_SHIFT )

ng1_visual ng1visual[] =  {

				/* 0, ci8 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8,
		0xff, 0xff << 24,
		V_DB_OK,
		0, X9_8BIT | X9_CMAP(0)
	},

				/* 1, ci12 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12,
		0xfff, 0xfff << 16,
		V_24_ONLY | V_DB_OK,
		1, X9_12BIT | X9_CMAP(0)
	},
				/* 2, rgba20 */
	{
		DM1_LO_SRC | DM1_RGBAPLANES| DM1_DRAWDEPTH12 | DM1_HOSTDEPTH32 | DM1_RGBMODE,
		0xfff, 0xfff,
		V_IS_RGB | V_24_ONLY | V_DB_OK,
		2, X9_12BIT | X9_RGB1
	},
				/* 3, ovl8 */
	{
		DM1_LO_SRC | DM1_OLAYPLANES| DM1_DRAWDEPTH8  | DM1_HOSTDEPTH8,
		0xffff00, 0xffff00, /* mask is bogus, bits aren't contiguous */
		V_24_ONLY,
		3, X9_8BIT | X9_OLAY | X9_AMAP(16)
	},
				/* 4, rgb8 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RGBMODE,
		0xff, 0xff << 24,
		V_IS_RGB | V_DB_OK,
		4, X9_8BIT | X9_RGB1
	},

				/* 5, rgb24 */
	{	
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH24 | DM1_HOSTDEPTH32 | DM1_RGBMODE,
		0xffffff, 0xffffff,
		V_IS_RGB | V_24_ONLY | V_DB_OK,
		5, X9_24BIT | X9_RGB0
	},

				/* 6, rgbfb12 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 | DM1_RGBMODE,
		0xfff, 0xfff << 16,
		V_IS_RGB | V_24_ONLY | V_IS_FRONTBUF,
		6, X9_12BIT | X9_RGB1
	},
				/* 7, rgbbb12 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 | 
				DM1_DBLSRC | DM1_RGBMODE,
		0xfff000, 0xfff << 16,
		V_IS_RGB | V_24_ONLY | V_IS_BACKBUF,
		6, /* same DID as front buf */ X9_12BIT | X9_RGB1 | X9_BB
	},
				/* 8, cifb12 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12,
		0xfff, 0xfff << 16,
		V_24_ONLY | V_IS_FRONTBUF,
		7, X9_12BIT | X9_CMAP(0)
	},
				/* 9, cibb12 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 | DM1_DBLSRC,
		0xfff000, 0xfff << 16,
		V_24_ONLY | V_IS_BACKBUF,
		7, /* same DID as front buf */ X9_12BIT | X9_CMAP(0) | X9_BB
	},
				/* 10, rgbafb12 */
	{
		DM1_LO_SRC | DM1_RGBAPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 |DM1_RGBMODE,
		0xfff, 0xfff << 16,
		V_IS_RGB | V_24_ONLY | V_IS_FRONTBUF,
		8, X9_12BIT | X9_RGB1
	},
				/* 11, rgbabb12 */
	{
		DM1_LO_SRC | DM1_RGBAPLANES | DM1_DRAWDEPTH12 | DM1_HOSTDEPTH12 |
				DM1_DBLSRC | DM1_RGBMODE,
		0xfff000, 0xfff << 16,
		V_IS_RGB | V_24_ONLY | V_IS_BACKBUF,
		8, /* same DID as front buf */ X9_12BIT | X9_RGB1 | X9_BB
	},
				/* 12, cifb4 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4,
		0xf, 0xf << 24,
		V_IS_FRONTBUF,
		9, X9_4BIT | X9_CMAP(0)
	},
				/* 13, cibb4 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4 | DM1_DBLSRC,
		0xf0, 0xf << 24,
		V_IS_BACKBUF,
		9, /* same DID as front buf */ X9_4BIT | X9_CMAP(0) | X9_BB
	},
				/* 14, rgbfb4 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4 | DM1_RGBMODE,
		0xf, 0xf << 24,
		V_IS_RGB | V_IS_FRONTBUF,
		10, X9_4BIT | X9_RGB1
	},
				/* 15, rgbbb4 */
	{
		DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4 | 
				DM1_DBLSRC | DM1_RGBMODE,
		0xf0, 0xf << 24,
		V_IS_RGB | V_IS_BACKBUF,
		10, X9_4BIT | X9_RGB1 | X9_BB
	},
				/* 16, pup2 */
	{
		DM1_LO_SRC | DM1_PUPPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4,
		0xcc, 0x3 << 24,
		0,
		-1, -1	/* There is no DID for pup mode */
	},
				/* 17, cid2 */
	{
		DM1_LO_SRC | DM1_CIDPLANES | DM1_DRAWDEPTH4 | DM1_HOSTDEPTH4,
		0x33, 0x3 << 24,
		0,
		-1, -1	/* No DID for CIDs */
	}

};
int nvisuals = sizeof (ng1visual) / sizeof(ng1visual[0]);

	 /*****************************************
	 * Routines to set current visual,
	 * swap buffer, set buffer. 
	 *****************************************/

int
ng1_setvisual (uint v)
{
	ng1_visual *vp;

	if (v >= NVISUALS)  {
		msg_printf (ERR,"bad visual id in ng1_setvisual\n");
		return -1;
	}
	vp = &ng1visual [v];
	
 	if ((vp->flags & V_24_ONLY) && (ng1_ginfo[REXindex].bitplanes != 24)) { 
		msg_printf (ERR,"visual id %d invalid on 8 plane fb\n", v);
		return -1;
	}

 	_setbits (dm1, VISUAL_MASK, vp->dm1bits); 

	REX->set.wrmask = vp->wrmask;

	/*
	 * Do xmap setup ...
	 */
	if (vp->modereg >= 0)   /* No DID for PUP, CID */
                xmap9SetModeReg(REX, vp->modereg, vp->mode,
				ng1_timing[REXindex]->cfreq);

        currentvisual = v;
/* 	msg_printf (VRB, "v %d did %d mode %x wmask %x dm1bits %x flags %x \n", v, vp->modereg, vp->mode, vp->wrmask, vp->dm1bits, vp->flags); */

	return 0;
}


/*
 * XXX This is rather clumsy; need a better way to do swapbuf 
 */

int
ng1setvisual(int argc, char **argv)
{
	uint i;
	if(!ng1checkboard())
		return -1;

        if (argc < 2) {
		printf("usage: %s visual_id\n", argv[0]);

		printf("	0:      ci8      1:     ci12     2:     rgba20\n");
		printf("	3:      ovl8     4:     rgb8     5:     rgb24\n");
		printf("	6:      rgbfb12  7:     rgbbb12  8:     cifb12\n");
		printf("	9:      cibb12   10:    rgbafb12 11:    rgbabb12\n");
		printf("	12:     cifb4    13:    cibb4    14:    rgbfb4\n");
		printf("	15:     rgbbb4   16:    pup2     17:    cid2\n");

                return -1;
        }
        atob(argv[1], (int*)&i);

	if (i >= NVISUALS) {
		msg_printf (ERR,"invalid visual id %d, use [0 .. %d]\n",
						i, NVISUALS);
		return -1;
	}
	ng1_setvisual (i);
	return 0;

}
