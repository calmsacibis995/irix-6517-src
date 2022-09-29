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
 *  Mini GL
 *  $Revision: 1.16 $
 */

#include <math.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <assert.h>
#include <sys/vdma.h>

#include "sagfx.h"
#include <sys/xmap9.h>
#include <sys/vc2.h>

#include "ide_msg.h"
#include "gl/rex3macros.h"
#include "local_ng1.h"
#include "ng1visuals.h"
#include <sys/ng1.h>
#include "../dma.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

struct newport_cx context1, context2;
extern Rex3chip *REX;
extern int REXindex;

extern int rex3_config_default;
extern struct ng1_info ng1_ginfo[];
extern struct ng1_timing_info *ng1_timing[];

/* This is not quite correct, for 12bit
 * CI must add 16K.
 */
#define MAGIC_BIAS	4096

#define REX3_NULL_MODE    0
#define REX3_CI_MODE_4    ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | \
                            DM1_HOSTDEPTH4 | DM1_COLORCOMPARE )
#define REX3_RGB_MODE_4   ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH4 | \
                            DM1_HOSTDEPTH4 | DM1_COLORCOMPARE | DM1_RGBMODE )
#define REX3_CI_MODE_8    ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH8 | \
                            DM1_HOSTDEPTH8 | DM1_COLORCOMPARE )
#define REX3_RGB_MODE_8   ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH8 | \
                            DM1_HOSTDEPTH8 | DM1_COLORCOMPARE | DM1_RGBMODE )
#define REX3_CI_MODE_12   ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | \
                            DM1_HOSTDEPTH12 | DM1_COLORCOMPARE )
#define REX3_RGB_MODE_12  ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH12 | \
                            DM1_HOSTDEPTH12 | DM1_COLORCOMPARE | DM1_RGBMODE )
#define REX3_RGB_MODE_24  ( DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH24 | \
                            DM1_HOSTDEPTH32 | DM1_COLORCOMPARE | DM1_RGBMODE )
#define REX3_OLAY_MODE_8  ( DM1_LO_SRC | DM1_OLAYPLANES | DM1_DRAWDEPTH8 | \
                            DM1_HOSTDEPTH8 | DM1_COLORCOMPARE )
#define REX3_PUP_MODE     (DM1_LO_SRC | DM1_PUPPLANES | DM1_DRAWDEPTH8 | \
                            DM1_HOSTDEPTH8 | DM1_COLORCOMPARE)

/* Cached drawmode registers. */ 

int dm0;
int dm1 = REX3_CI_MODE_8;
unsigned int current_clipmode = 0x1e00;
int _line_cmd = DM0_DRAW|DM0_DOSETUP|DM0_STOPONX|DM0_STOPONY|DM0_ILINE;

#define _setbits(d, mask, bits) d &= ~mask; d |= bits

int line_parser(int, char**, int);
int block_parser(int argc, char **argv, int dm0_xtra, int dm1_xtra);
int span_parser(int argc, char **argv, int dm0_xtra);
int newport_dma(char *buf, int x0, int y0, int x1, int y1, int pixsize, int flags, int bytesperrow);


/***********************************************************************
 * 
 * Functions to set gl drawing context
 * and issue rendering primitives.
 *
 * Functions named ng1XXX are called from ide scripts
 * or by finger-pokin' keyboard operators.  Functions
 * named ng1_XXX are called locally.
 *
 ***********************************************************************/


	/*****************************************
	 * Functions to assign values to dm1
	 *****************************************/

int ng1_planes(int b) 	{ _setbits (dm1, DM1_PLANES, b); }

int 
ng1planes(int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s planes\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_planes(i);
	return 0;
}


int ng1_drawdepth (int b) 	{ _setbits (dm1, DM1_DRAWDEPTH_MASK, b); }

int
ng1drawdepth(int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s drawdepth\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_drawdepth((i << DM1_DRAWDEPTH_SHIFT));
	return 0;
}


int ng1_dblsrc (int b) 	{ if (b) dm1 |= DM1_DBLSRC; else dm1 &= ~DM1_DBLSRC; }

int
ng1dblsrc(int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_dblsrc(i);
	return 0;
}


int ng1_yflip (int b) 	{ if (b) dm1 |= DM1_YFLIP; else dm1 &= ~DM1_YFLIP; }

int
ng1yflip(int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_yflip(i);
	return 0;
}


int ng1_rwpacked (int b) { if (b) dm1 |= DM1_RWPACKED; else dm1 &= ~DM1_RWPACKED; }

int
ng1rwpacked (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_rwpacked (i);
	return 0;
}


int ng1_hostdepth (int b) 	{ _setbits (dm1, DM1_HOSTDEPTH_MASK, b); }

int
ng1hostdepth (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s depth\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_hostdepth ((i << DM1_HOSTDEPTH_SHIFT));
	return 0;
}

int ng1_rwdouble (int b) { if (b) dm1 |= DM1_RWDOUBLE; else dm1 &= ~DM1_RWDOUBLE; }

int
ng1rwdouble (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_rwdouble (i);
	return 0;
}

int ng1_swapendian (int b) { if (b) dm1 |= DM1_SWAPENDIAN; else dm1 &= ~DM1_SWAPENDIAN;}

int
ng1swapendian (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_swapendian (i);
	return 0;
}

int ng1_compare (int b) 	{ _setbits (dm1, DM1_COLORCOMPARE_MASK, b); }

int
ng1compare (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s compare\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_compare ((i << DM1_COLORCOMPARE_SHIFT));
	return 0;
}

int ng1_rgbmode (int b) { if (b) dm1 |= DM1_RGBMODE; else dm1 &= ~DM1_RGBMODE; }

int
ng1rgbmode(void)
{
	if(!ng1checkboard())
		return -1;

	ng1_rgbmode (1);
        xmap9SetModeReg (REX, 0, XM9_PIXSIZE_24|(1 << XM9_PIX_MODE_SHIFT),
				 ng1_timing[0]->cfreq);
        SET3(REX->set.drawmode1, dm1);
	return 0;
}    

int
ng1cmode(void)
{
	if(!ng1checkboard())
		return -1;

	ng1_rgbmode (0);
        xmap9SetModeReg (REX, 0, XM9_PIXSIZE_8, ng1_timing[0]->cfreq);
				/* XXX No 12bit CI ? */
        SET3(REX->set.drawmode1, dm1);
	return 0;
}    

	
int ng1_dither (int b) 	{ if (b) dm1 |= DM1_ENDITHER; else dm1 &= ~DM1_ENDITHER; }

int
ng1dither(int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s 0|1\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_dither (i);
	return 0;
}    

int ng1_fastclear (int b) { if (b) dm1 |= DM1_FASTCLEAR; else dm1 &= ~DM1_FASTCLEAR; }

int
ng1fastclear (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_fastclear (i);
	return 0;
}

int ng1_blend (int b) 	{ if (b) dm1 |= DM1_ENBLEND; else dm1 &= ~DM1_ENBLEND; }

int
ng1blend (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_blend (i);
	return 0;
}

int ng1_sfactor (int b) 	{ _setbits (dm1, DM1_SF_MASK, b); }

int
ng1sfactor (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s sfactor\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_sfactor ((i << DM1_SF_SHIFT));
	return 0;
}

int ng1_dfactor (int b) 	{ _setbits (dm1, DM1_DF_MASK, b); }

int
ng1dfactor (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s dfactor\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_dfactor ((i << DM1_DF_SHIFT));
	return 0;
}


int ng1_backblend (int b) { if (b) dm1 |= DM1_ENBACKBLEND; else dm1 &= ~DM1_ENBACKBLEND; }

int
ng1backblend (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_backblend (i);
	return 0;
}


int ng1_prefetch (int b) { if (b) dm1 |= DM1_ENPREFETCH; else dm1 &= ~DM1_ENPREFETCH; }

int
ng1prefetch (int argc, char **argv)
{
	int i;
	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s flag\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	ng1_prefetch (i);
	return 0;
}


int ng1_logicop (int b) 	{ _setbits (dm1, DM1_LO_MASK, b); }

int
ng1logicop(int argc, char **argv)
{
	int i;

	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s mask\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &i);
	ng1_logicop ((i << DM1_LO_SHIFT));
	return 0;
}    
	

/*
 * set the line type for line drawing commands
 */
ng1linetype (int argc, char **argv)
{
	int linetype;

	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf ("usage: %s linetype\n", argv[0]);
		return -1;
	}
	atob (argv[1], &linetype);

	_setbits (_line_cmd, DM0_ADRMODE, linetype);
}


int
ng1ystride (int argc, char **argv)
{
        int i;
	if(!ng1checkboard())
		return -1;

        if (argc < 2) {
                printf("usage: %s flag\n", argv[0]);
                return -1;
        }
        atob(argv[1], &i);
        if(i)
                REX->set.drawmode0 |= DM0_YSTRIDE;
        else
                REX->set.drawmode0 &= ~DM0_YSTRIDE;
        return 0;
}

	
	/*****************************************
	 * Functions to set various rex registers.
	 *****************************************/
void
ng1_writemask (int mask)
{
	 SET3(REX->set.wrmask, mask);
}

int
ng1writemask(int argc, char **argv)
{
	int i;

	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s mask\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &i);
        SET3(REX->set.wrmask, i);
	return 0;
}

int
ng1lslength(int argc, char **argv)
{
	int mode;

	if(!ng1checkboard())
		return -1;

	if (argc != 2) {
		printf("usage: %s mode\n", argv[0]);
		return -1;
	}
	mode = atoi(argv[1]);
	if((mode < 17) || (mode > 32)){
	    mode = 0;
	} else{
	    mode = (mode - 17) << 24;
	}
	REX->set.lsmode = mode;
	return 0;
}

int
ng1lspattern(int argc, char **argv)
{
	int pattern;

	if(!ng1checkboard())
		return -1;

	if (argc != 2) {
		printf("usage: %s pattern\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &pattern);
	REX->set.lspattern = pattern;
	return 0;
}

int
ng1zpattern(int argc, char **argv)
{
	int pattern;

	if(!ng1checkboard())
		return -1;

	if (argc != 2) {
		printf("usage: %s pattern\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &pattern);
	REX->go.zpattern = pattern;
	return 0;
}

int
ng1_colorback(int c)
{
	REX->set.colorback = c;
}

int
ng1colorback(int argc, char **argv)
{
	int color;

	if(!ng1checkboard())
		return -1;

	if (argc != 2) {
		printf("usage: %s index\n", argv[0]);
		return -1;
	}
	color = atoi(argv[1]);
	ng1_colorback(color);
	return 0;
}

int
ng1_color(int c)
{
	REX->set.colorred.word = c << 11;
	return 0;
}

int
ng1_fcolor (float f)
{
	f += MAGIC_BIAS;
        REX->set.colorred.word = (*(int *)(&f)) & 0x7fffff;
}

int
ng1color(int argc, char **argv)
{
	int colord;

	if(!ng1checkboard())
		return -1;

        if (argc != 2) {
                printf("usage: %s index\n", argv[0]);
                return -1;
        }
        colord = atoi(argv[1]);
	ng1_color(colord);

        return 0;
}

int
ng1_rgbcolor(int r, int g, int b)
{
	REX->set.colorred.word = r << 11;
	REX->set.colorgrn.word = g << 11;
	REX->set.colorblue.word = b << 11;
}

int
ng1_argbcolor(int a, int r, int g, int b)
{
	REX->set.coloralpha.word = a;
	REX->set.colorred.word = r;
	REX->set.colorgrn.word = g;
	REX->set.colorblue.word = b;
}

int
ng1rgbcolor(int argc, char **argv)
{
        double alphad, redd, greend, blued;
        float alphaf, redf, greenf, bluef;

	if(!ng1checkboard())
		return -1;

        if (argc != 5) {
                printf("usage: %s a r g b\n", argv[0]);
                return -1;
        }
        alphad = atoi(argv[1]);
        redd = atoi(argv[2]);
        greend = atoi(argv[3]);
        blued = atoi(argv[4]);
        alphaf = MAGIC_BIAS + (float)alphad;
        redf = MAGIC_BIAS + (float)redd;
        greenf = MAGIC_BIAS + (float)greend;
        bluef = MAGIC_BIAS + (float)blued;
        ng1_argbcolor(((*(int *)&alphaf) & 0x7ffff),
                ((*(int *)&redf) & 0x7fffff), ((*(int *)&greenf) & 0x7ffff),
                ((*(int *)&bluef) & 0x7ffff));
        return 0;
}

int
ng1_slope(int a, int r, int g, int b)
{
	REX->set.slopealpha.word = a;
	REX->set.slopered.word = r;
	REX->set.slopegrn.word = g;
	REX->set.slopeblue.word = b;
}

int
ng1slope(int argc, char **argv)
{
	double alphad, redd, greend, blued;
	float alphaf, redf, greenf, bluef;

	if(!ng1checkboard())
		return -1;

	if (argc != 5) {
		printf("usage: %s a r g b\n", argv[0]);
		return -1;
	}
	alphad = atoi(argv[1]);
	redd = atoi(argv[2]);
	greend = atoi(argv[3]);
	blued = atoi(argv[4]);
	alphaf = MAGIC_BIAS + (float)alphad;
	redf = MAGIC_BIAS + (float)redd;
	greenf = MAGIC_BIAS + (float)greend;
	bluef = MAGIC_BIAS + (float)blued;
	ng1_slope(((*(int *)&alphaf) & 0xfffff),
		((*(int *)&redf) & 0x7fffff), ((*(int *)&greenf) & 0xfffff),
		((*(int *)&bluef) & 0xfffff));
	return 0;
}


int ng1_hostrw(int i, unsigned val)
{
	if(i)
	    REX->go.hostrw1 = val;
	else
	    REX->go.hostrw0 = val;
}

int
ng1hostrw(int argc, char **argv, int index)
{
	int value;

	if(!ng1checkboard())
		return -1;

	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &value);;
	ng1_hostrw(index, value);
	return 0;
}

int
ng1xywinorg(int argc, char **argv)
{
	int x, y;

	if(!ng1checkboard())
		return -1;

	if (argc < 3) {
		printf("usage: %s x y\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x);
	atob(argv[2], &y);
	SET3_XY(REX->p1.set.xywin, x, y);
	return 0;
}


	/*****************************************
	 * Rendering routines
	 *****************************************/

/*
 * point() - write a single pixel to VRAM
 *
 *      This function takes x, y coordinates and writes a single pixel into
 *      the framebuffer at that location.
 */
int
ng1point(int argc, char **argv)
{
	int x, y;

	if(!ng1checkboard())
		return -1;

	if (argc < 3) {
		printf("usage: %s x y\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x);
	atob(argv[2], &y);
	REX->set.xystarti = (x << 16 | y);
	REX->set.drawmode1 = dm1;
	REX->go.drawmode0 = DM0_DRAW | DM0_BLOCK;
	REX3WAIT(REX);
	return 0;
}


int
ng1_line(int x0, int y0, int x1, int y1, int dm0_xtra)
{

	REX->set.xystarti = x0 << 16 | y0;
	REX->set.xyendi = x1 << 16 | y1;

	REX->set.drawmode0 = _line_cmd | dm0_xtra;
	REX->go.drawmode1 = dm1;
	REX3WAIT(REX);
}

int
ng1line(int argc, char **argv)
{
	line_parser(argc, argv, 0);
	return 0;
}

int
ng1linesd(int argc, char **argv)
{
	line_parser(argc, argv, DM0_ENLSPATTERN);
	return 0;
}

int
ng1linedd(int argc, char **argv)
{
	line_parser(argc, argv, DM0_ENLSPATTERN | DM0_LSOPAQUE);
	return 0;
}

int
line_parser(int argc, char **argv, int dm0_xtra)
{
	int x0, y0, x1, y1;

	if(!ng1checkboard())
		return -1;

	if (argc < 5) {
		printf("usage: %s x0 y0 x1 y1\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x0);
	atob(argv[2], &y0);
	atob(argv[3], &x1);
	atob(argv[4], &y1);
	ng1_line(x0, y0, x1, y1, dm0_xtra);
	return 0;
}

int
ng1_block(int x0, int y0, int x1, int y1, int dm0_xtra, int dm1_xtra)
{
        REX->set.xyendi = x1 << 16 | y1;
	REX->set.drawmode1 = dm1 | dm1_xtra ;
	REX->set.bresoctinc1 = 0x0;
	if( y1 < y0 )
		REX->set.bresoctinc1 = REX->set.bresoctinc1 | (0x1 << 24);

	switch(dm0_xtra){
	case 0:
	    REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK |
				DM0_STOPONXY ;
            REX->go.xystarti = x0 << 16 | y0;
	    break;
	case DM0_COLORHOST:
	    REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK |
					dm0_xtra;
            REX->set.xystarti = x0 << 16 | y0;
	    break;
	default: /* this include ZPATTERN and OPAQUE_ZPATTERN */
	    REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK | DM0_STOPONX |
					dm0_xtra;
            REX->set.xystarti = x0 << 16 | y0;
	    break;
	}
	REX3WAIT(REX);

}

int
ng1blockimg(int argc, char **argv)
{
	block_parser(argc, argv, DM0_COLORHOST, DM1_RWPACKED);
	return 0;
}

int
ng1blockopstip(int argc, char **argv)
{
	block_parser(argc, argv, DM0_ENZPATTERN | DM0_ZOPAQUE | DM0_LENGTH32, 0);
	return 0;
}

int
ng1blockstip(int argc, char **argv)
{
	block_parser(argc, argv, DM0_ENZPATTERN | DM0_LENGTH32, 0);
	return 0;
}

int
ng1block(int argc, char **argv)
{
	block_parser(argc, argv, 0, 0);
	return 0;
}
int
block_parser(int argc, char **argv, int dm0_xtra, int dm1_xtra)
{
	int x0, y0, x1, y1;

	if(!ng1checkboard())
		return -1;

	if (argc < 5) {
		printf("usage: %s x0 y0 x1 y1\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x0);
	atob(argv[2], &y0);
	atob(argv[3], &x1);
	atob(argv[4], &y1);
	ng1_block(x0, y0, x1, y1, dm0_xtra, dm1_xtra);
	return 0;
}

int
ng1blockystride(int argc, char **argv)
{
        int x0, y0, x1, y1;
	if(!ng1checkboard())
		return -1;

        if (argc < 5) {
                printf("usage: %s x0 y0 x1 y1\n", argv[0]);
                return -1;
        }
        atob(argv[1], &x0);
        atob(argv[2], &y0);
        atob(argv[3], &x1);
        atob(argv[4], &y1);

        REX->set.xyendi = x1 << 16 | y1;
        REX->set.drawmode1 = dm1;
        REX->set.bresoctinc1 = 0x0;

        if( y1 < y0 )
                REX->set.bresoctinc1 = REX->set.bresoctinc1 | (0x1 << 24);

        REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK | DM0_YSTRIDE |
                                DM0_STOPONXY ;

        REX->go.xystarti = x0 << 16 | y0;

        REX3WAIT(REX);
}

int
ng1_drawcid(int n, int x0, int y0, int x1, int y1)
{

	switch(n){
	    case 0:
		current_clipmode |= 0x200;
		break;	
	    case 1:
		current_clipmode |= 0x400;
		break;	
	    case 2:
		current_clipmode |= 0x800;
		break;	
	    case 3:
		current_clipmode |= 0x1000;
		break;	
	}
	REX->p1.set.clipmode = current_clipmode;

        REX->set.xystarti = x0 << 16 | y0;
        REX->set.xyendi = x1 << 16 | y1;
	REX->set.colorvram = n;
	REX->set.wrmask = 0x33;
	REX->set.drawmode1 = DM1_CIDPLANES | DM1_FASTCLEAR | DM1_LO_SRC;
	REX->go.drawmode0 = DM0_DRAW | DM0_BLOCK | DM0_DOSETUP | DM0_STOPONXY;
	REX3WAIT(REX);
	REX->set.drawmode1 = dm1;
	REX->set.wrmask = 0xffffff;
}

int
ng1drawcid(int argc, char **argv)
{
	int n, x0, y0, x1, y1;

	if(!ng1checkboard())
		return -1;

	if((argc == 2) && (argv[1][0] == 'c' )){
	    current_clipmode &= 0x1ff;
	    current_clipmode |= 0x1e00;
	    REX->p1.set.clipmode = current_clipmode;
	    return 0;
	}

	if (argc != 6) {
		printf("usage: %s n x0 y0 x1 y1\n", argv[0]);
		printf("       use \"%s c\" to turn cids checking off\n", argv[0]);
		return -1;
	}
	atob(argv[1], &n);
	if((n > 3) || (n < 0)){
	    printf("drawcid: valid cid is 0 - 3\n");
	    return -1;
	}
	atob(argv[2], &x0);
	atob(argv[3], &y0);
	atob(argv[4], &x1);
	atob(argv[5], &y1);
	ng1_drawcid(n, x0, y0, x1, y1);
	return 0;
}


int
ng1_span(int x0, int y, int x1, int dm0_xtra)
{
        REX->set.xyendi = x1 << 16;
	REX->set.drawmode1 = dm1;
	REX->set.bresoctinc1 = 0x0;
	REX->set.drawmode0 = DM0_DRAW | DM0_SPAN | DM0_STOPONX | dm0_xtra;
        REX->go.xystarti = x0 << 16 | y;
	REX3WAIT(REX);
}

int
ng1span(int argc, char **argv)
{
	span_parser(argc, argv, 0);
	return 0;
}

int
ng1spanstip(int argc, char **argv)
{
	span_parser(argc, argv, DM0_ENLSPATTERN);
	return 0;
}

int
ng1spanopstip(int argc, char **argv)
{
	span_parser(argc, argv, DM0_ENLSPATTERN | DM0_LSOPAQUE);
	return 0;
}

int
span_parser(int argc, char **argv, int dm0_xtra)
{
	int x0, y, x1;

	if(!ng1checkboard())
		return -1;

	if (argc < 4) {
		printf("usage: %s x0 y x1\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x0);
	atob(argv[2], &y);
	atob(argv[3], &x1);
	ng1_span(x0, y, x1, dm0_xtra);
	return 0;
}

static unsigned short ClipMode[] = {
        0x0001, /* enable smask0 inside, not used */
        0x0002, /* enable smask1 inside */
        0x0004, /* enable smask2 inside */
        0x0008, /* enable smask3 inside */
        0x0010  /* enable smask4 inside */
};

void
ng1_setsmask( int n, int x0, int y0, int x1, int y1)
{
	switch(n){
	    case -1:
		current_clipmode &= 0x1fe0;
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    case 0: 
		SET3_XY (REX->set.smask0x, x0, x1);
		SET3_XY (REX->set.smask0y, y0 , y1);
		current_clipmode = current_clipmode | ClipMode[0];
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    case 1: 
		SET3_XY (REX->p1.set.smask1x, (x0 + 0x1000), (x1 + 0x1000));
		SET3_XY (REX->p1.set.smask1y, (y0 + 0x1000) , (y1 + 0x1000));
		current_clipmode = current_clipmode | ClipMode[1];
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    case 2: 
		SET3_XY (REX->p1.set.smask2x, (x0 + 0x1000), (x1 + 0x1000));
		SET3_XY (REX->p1.set.smask2y, (y0 + 0x1000) , (y1 + 0x1000));
		current_clipmode = current_clipmode | ClipMode[2];
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    case 3: 
		SET3_XY (REX->p1.set.smask3x, (x0 + 0x1000), (x1 + 0x1000));
		SET3_XY (REX->p1.set.smask3y, (y0 + 0x1000) , (y1 + 0x1000));
		current_clipmode = current_clipmode | ClipMode[3];
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    case 4: 
		SET3_XY (REX->p1.set.smask4x, (x0 + 0x1000), (x1 + 0x1000));
		SET3_XY (REX->p1.set.smask4y, (y0 + 0x1000) , (y1 + 0x1000));
		current_clipmode = current_clipmode | ClipMode[4];
		SET3 (REX->p1.set.clipmode, current_clipmode);
		break;
	    default:
		printf("setsmask: 0 to clear, 1 - 4 to set\n");
		break;
	}
}

int
ng1setsmask(int argc, char **argv)
{
	int n, x0, y0, x1, y1;

	if(!ng1checkboard())
		return -1;

	if(argc < 6){
	    if((argc != 2) || (argv[1][0] != 'c')){ 
	       printf("usage: %s n x0 y0 x1 y1\n", argv[0]);
                return -1;
	    } else
		n = -1;
        } else {
	    atob(argv[1], &n);
	    atob(argv[2], &x0);
	    atob(argv[3], &y0);
	    atob(argv[4], &x1);
	    atob(argv[5], &y1);
	}
	ng1_setsmask(n, x0, y0, x1, y1);
	return 0;
}

int ng1_scrtoscr (int x0, int y0, int x1, int y1, int dx, int dy) 
{
	uint tmp = 0;
        REX->set.xystarti = x0 << 16 | y0;
        REX->set.xyendi = x1 << 16 | y1;
        SET3_XY (REX->set.xymove, dx, dy);
        REX->set.drawmode1 = dm1;

	/*
	 * Must zero color{red,green,blue} fractions before SCR2SCR
	 * if in RGB mode.  The 'fraction' is the bottom 11 bits.
	 * See errata in rex spec for info.
	 */
	if (dm1 & DM1_RGBMODE) {
		GET3 (REX->set.colorred.word, tmp);
		tmp &= ~0x7ff;
		SET3 (REX->set.colorred.word, tmp);
		GET3 (REX->set.colorgrn.word, tmp);
		tmp &= ~0x7ff;
		SET3 (REX->set.colorgrn.word, tmp);
		GET3 (REX->set.colorblue.word, tmp);
		tmp &= ~0x7ff;
		SET3 (REX->set.colorblue.word, tmp);
	}
		
        REX->go.drawmode0 = DM0_SCR2SCR | DM0_BLOCK | DM0_DOSETUP | 
				DM0_STOPONXY;
        REX3WAIT(REX);
}

int
ng1scrtoscr (int argc, char **argv)
{
	int x0, y0, x1, y1, dx, dy;

	if(!ng1checkboard())
		return -1;

	if (argc < 6) {
		printf ("usage: %s (source block coords) x0 y0 x1 y1 "
				"(offset to dest) dx dy\n", argv[0]);
		return -1;
	}
        atob(argv[1], &x0);
        atob(argv[2], &y0);
        atob(argv[3], &x1);
        atob(argv[4], &y1);
        atob(argv[5], &dx);
        atob(argv[6], &dy);

	ng1_scrtoscr (x0,y0,x1,y1,dx,dy);
	return 0;
}

int ng1_readblock (int x0, int y0, int x1, int y1)
{
        int i, x, y, xinc, k;
        int w, j;
        int tmp_dm1;
        unsigned int quad;
        unsigned char *got;
        int rb24;
        uint tmp;

        tmp_dm1=dm1;
	if(ng1_ginfo[REXindex].bitplanes == 24)
        	rb24 = 1;
	else
		rb24 = 0;

        dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_ENPREFETCH | DM1_LO_SRC;
        if (rb24) {
                dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 |
                    DM1_HOSTDEPTH32 | DM1_RWPACKED;
                ng1_writemask (0xffffff);
		ng1_setvisual(5);
                xinc = 1;
        }
        else {
                dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
                ng1_writemask (0xff);
		ng1_setvisual(0);
                xinc = 4;
        }

        REX->set.drawmode1 = dm1;
        REX->set.drawmode0 = DM0_READ | DM0_BLOCK | DM0_DOSETUP | DM0_COLORHOST;        REX->set.xystarti = x0 << 16 | y0;
        REX->set.xyendi = x1 << 16 | y1;
	REX3WAIT(REX);
        /* Dummy read for Prefetch */
        REX->go.hostrw0; 
        w=x1-x0;
        j= w % 4;
        if((!rb24) && (j))
                x1=x1-j;
        for (y = y0; y < y1; y++) {
                for (x = x0; x < x1; x += xinc) {
                        quad = REX->go.hostrw0;
                        if (rb24) {
                                 msg_printf(VRB, "Pixel=%x at location x=%d y=%d\n", quad,x, y); 
                        } else {
                                for(k=0; k<4; k++){
                                        got = (unsigned char *)(&quad);
					msg_printf(VRB, "Pixel=%x at location x=%d y=%d\n", *got,x+k, y); 
                                        got++;
                                }
                        }


                }
                if((!rb24) && (j)) {
                        quad = REX->go.hostrw0;
                        for(k=0; k<j; k++){
                                got = (unsigned char *)(&quad);
                                msg_printf(VRB, "Pixel=%x at location x=%d y=%d\n", *got,x+k, y); 
                                got++;
                        }
                }
        }

        dm1 = tmp_dm1;
}

int
ng1readblock (int argc, char **argv)
{
        int x0, y0, x1, y1;

	if(!ng1checkboard())
		return -1;

        if (argc < 4) {
                printf ("usage: %s (source block coords) x0 y0 x1 y1\n", argv[0]);
                return -1;
        }
        atob(argv[1], &x0);
        atob(argv[2], &y0);
        atob(argv[3], &x1);
        atob(argv[4], &y1);

        ng1_readblock (x0,y0,x1,y1);
        return 0;
}

int
ng1_polygon(int x0, int y0, int x1, int y1, float color[][3])
{
	int y;
	int i;
	float clr[3];
	float alpha;
	float r, g, b;
	
	for (y = y0; y <= y1; y++) {
		alpha = (y - y0) / (float)(y1 - y0);
		REX->set.colorred.flt = r =
			255.0*((1.0 - alpha)*color[0][0] + alpha*color[2][0]);
		REX->set.colorgrn.flt = g =
			255.0*((1.0 - alpha)*color[0][1] + alpha*color[2][1]);
		REX->set.colorblue.flt = b =
			255.0*((1.0 - alpha)*color[0][2] + alpha*color[2][2]);
		if (r < 0.0 || r >= 256)
			msg_printf(DBG, "r = %f\n", r);
		if (g < 0.0 || g >= 256)
			msg_printf(DBG, "g = %f\n", g);
		if (b < 0.0 || b >= 256)
			msg_printf(DBG, "b = %f\n", b);
		for (i = 0; i < 3; i++) {
			clr[i] = 256*((1.0 - alpha)*(color[1][i] - color[0][i])
				+ alpha*(color[3][i] - color[2][i]))/(x1 - x0);
			if (clr[i] < 0)
				clr[i] -= REX_MCOLOR_FIXIT;
			else
				clr[i] += REX_MCOLOR_FIXIT;
		}
		REX->set.slopered.flt = clr[0];
		REX->set.slopegrn.flt = clr[1];
		REX->set.slopeblue.flt = clr[2];
		ng1_span(x0, y, x1, 0);
	}
}

#ifndef PROM 

struct ng1_pixeldma_args dma;

static int vdma_initted;	/* hopefully == 0 */

static int
vdma (struct ng1_pixeldma_args *a)
{
	unsigned int flags = VDMA_M_INCA | VDMA_M_LBURST;
	unsigned int stride;
	unsigned int nbytes;

	if (vdma_initted == 0) {
		vdma_initted = 1;
		basic_DMA_setup (0);
	}

	if (a->flags & NG1_STRIDE)  {
		stride = a->pmstride - a->xlen;
		nbytes = a->pmstride * a->ylen;
	}
	else {
		stride = 0;
		nbytes = a->xlen * a->ylen;
	}

	if (!(a->flags & NG1_WRITE)) {
		extern void __dcache_inval(void *,int);
		flags |= VDMA_M_GTOH;
		__dcache_inval((void *)KDM_TO_PHYS(a->buf), nbytes);
	}
	else 
		flush_cache();
msg_printf (VRB, "vdma flags %x (%x) dx %x dy %x stride %x buf %x\n",
	flags, a->flags, a->xlen, a->ylen, stride, a->buf);
	
	dma_go ((caddr_t)KDM_TO_PHYS (a->gfxaddr), KDM_TO_PHYS (a->buf), 
		a->ylen, a->xlen, stride, a->yzoom, flags);

	return 0;
}

#define DMAFROMGFX 	1
#define WRITEAFTERREAD 	2
#define DMADEBUG  	16
#define STRIDEDMA 	32
#define SB 		64


int ng1dmatest(int argc, char **argv)
{
	int *lbuf, *rlbuf;
	short *sbuf, *rsbuf;
	char *cbuf, *rcbuf;
	int i, j, k, kk, x0, y0, x1, y1;
	int sb, stride, stridepix, dx, dy, nxpix;
	int inc, pixsize, bytesperpixel, pixperword, packed, depth;
	int flags = 0, errors = 0;
	int buswidth;
	int swapend;

	if(!ng1checkboard())
		return -1;

	if (argc < 5) {
		printf ("usage: %s x0 y0 x1 y1 [flags [stride]]\n", argv[0]);
		return -1;
	}
	atob(argv[1], &x0);
	atob(argv[2], &y0);
	atob(argv[3], &x1);
	atob(argv[4], &y1);

	dy = (y1 - y0 + 1);

	if (argc > 5)
		atob(argv[5], &flags);
	if (flags & STRIDEDMA) {
		if (argc < 7) {
			printf ("usage: %s x0 y0 x1 y1 [flags [stride][sb]]\n",
				argv[0]);
			return -1;
		}
		atob(argv[6], &dx);		/* in pixels for now */
	}
	else
		dx = (x1 - x0 + 1);
	if (flags & SB) {
		i = (flags & STRIDEDMA) ? 8 : 7;
		if (argc < i) {
			printf ("usage: %s x0 y0 x1 y1 [flags [stride][sb]]\n",
				argv[0]);
			return -1;
		}
		atob(argv[i-1], &sb);		/* startbyte */
	}
	else
		sb = 0;

	depth = (dm1 & DM1_HOSTDEPTH_MASK) >> DM1_HOSTDEPTH_SHIFT;
	switch (depth) {
	case 0 : pixsize = 1; break;
	case 1 : pixsize = 1; break;
	case 2 : pixsize = 2; break;
	case 3 : pixsize = 4; break;
	}

	if (sb % pixsize) {
		msg_printf (ERR, "%s pixsize %d doesn't divide start byte %d\n"
				, sb, pixsize);
		return -1;
	}
	swapend = dm1 & DM1_SWAPENDIAN;

	if (dm1 & DM1_RWDOUBLE)
		pixperword = 8 / pixsize;
	else
		pixperword = 4 / pixsize;

	packed = dm1 & DM1_RWPACKED;
	if (packed) 
		inc = 1;
	else  {
		if (sb) {
			msg_printf (ERR,
			"%s start byte only allowed with DM1_RWPACKED\n");
			return -1;
		}
		inc = pixperword; 
	}
	bytesperpixel = inc * pixsize;

	if (REX->p1.set.config & BUSWIDTH) {
		buswidth = 8;
	}
	else
		buswidth = 4;

	stride = dx * bytesperpixel; /* convert stride from pixels to bytes */

	if (sb || (stride % buswidth))
		flags |= STRIDEDMA;

	if ((cbuf = (char *)malloc (dy * stride + sb)) == 0) {
		printf ("Can't get %d bytes in ng1dma\n",(dy * stride));
		return -1;
	}

	/*
	 * Read back an area 1 pixel longer, 1 row deeper
	 * to check for pixels written outside the block.
	 */
	if ((rcbuf = (char *)malloc ((dy+1) * (stride + sb + 16))) == 0) {
		printf ("Can't get %d bytes in ng1dma\n",(dy * stride));
		return -1;
	}

	bzero (rcbuf, (dy+1) * (stride + sb + 16));

	kk = k = sb/pixsize;		/* skip the skipped pixels */

	switch (depth) {
	case 0 :
		for (i = 0; i < dy; i++) {
			if (flags & DMADEBUG)
				printf ("\nRow %d %x\n", i, (cbuf + k));
			for (j = 0; j < dx; j++, k+= inc) {
				cbuf[k] = j & 0xf;
				if (flags & DMADEBUG)
					printf ("%x ", cbuf[k]);
			}
		}
		break;
	case 1 :
		for (i = 0; i < dy; i++) {
			if (flags & DMADEBUG)
				printf ("\nRow %d %x\n", i, (cbuf + k));
			for (j = 0; j < dx; j++, k+= inc) {
				cbuf[k] = j;
				if (flags & DMADEBUG)
					printf ("%x ", cbuf[k]);
			}
		}
		break;
	case 2 :
		sbuf = (short *)cbuf;
		if (swapend && packed)
		for (i = 0; i < dy; i++)
			for (j = 0; j < dx; j++, k+= inc)
				sbuf[k] = ((j & 0xff) << 8) | (i & 0xf);
		else
		for (i = 0; i < dy; i++)
			for (j = 0; j < dx; j++, k+= inc)
				sbuf[k] = ((i & 0xf) << 8) | (j & 0xff);
		break;
	case 3 :
		lbuf = (int *)cbuf;
		
		if (swapend)
		for (i = 0; i < dy; i++) {
			if (flags & DMADEBUG)
				printf ("\nRow %d %x\n", i, (lbuf + k));
			for (j = 0; j < dx; j++, k+= inc) {
				lbuf[k] = ((i & 0x3ff) << 10 | (j & 0x3ff))<<12;
				if (flags & DMADEBUG)
					printf ("%x ", lbuf[k]);
			}
		}
		else
		for (i = 0; i < dy; i++) {
			if (flags & DMADEBUG)
				printf ("\nRow %d %x\n", i, (lbuf + k));
			for (j = 0; j < dx; j++, k+= inc) {
				lbuf[k] = ((i & 0x3ff)<<10 | (j & 0x3ff)) << 4;
				if (flags & DMADEBUG)
					printf ("%x ", lbuf[k]);
			}
		}
		break;
	}

	newport_dma(cbuf + sb, x0, y0, x1, y1, bytesperpixel, 
			flags & ~DMAFROMGFX, stride);
	/*
	 * Read the stuff back and compare.
	 * Read back an extra pixel at the right edge,
	 * and read back an extra row at the bottom.
	 * Check the extra pixels - should be zero
	 * (assume the caller takes care of this).
	 */

	nxpix = (x1 - x0 + 1);	/* number of real pixels per row */
	
	newport_dma(rcbuf + sb, x0, y0, x1+1, y1+1, pixsize * inc, 
			(flags | DMAFROMGFX) | STRIDEDMA, 
			(nxpix + 1) * bytesperpixel);


	kk = k = sb/pixsize;		/* skip the skipped pixels */
	stridepix = (dx - nxpix) * inc;

	switch (depth) {
	case 0 :
	case 1 :
		for (i = 0; i < dy; i++) {
			for (j = 0; j < nxpix; j++, k+= inc, kk += inc) 
				if (rcbuf[kk] != cbuf[k])  {
				msg_printf (VRB,"got %x expect %x at %x %x\n",
						rcbuf[kk], cbuf[k], i, j);
				errors++;
				}
			if (rcbuf[kk] != 0)  {
				msg_printf (VRB,"got %x expect 0 at %x %x\n",
					rcbuf[kk], i, j);
				errors++;
			}
			k += stridepix;
			kk += inc;
		}
		break;
	case 2 :
		rsbuf = (short *)rcbuf;
		for (i = 0; i < dy; i++) {
			for (j = 0; j < nxpix; j++, k+= inc, kk += inc)
				if (rsbuf[kk] != sbuf[k]) {
				msg_printf (VRB,"got %x expect %x at %x %x\n", 
						rsbuf[kk], sbuf[k], i, j);
					errors++;
				}
			if (rsbuf[kk] != 0)  {
				msg_printf (VRB,"got %x expect 0 at %x %x\n",
					rsbuf[kk], i, j);
				errors++;
			}
			k += stridepix;
			kk += inc;
		}
		break;
	case 3 :
		rlbuf = (int *)rcbuf;
		for (i = 0; i < dy; i++) {
			for (j = 0; j < nxpix; j++, k+= inc, kk += inc)
				if(rlbuf[kk] != lbuf[k]) {
				msg_printf (VRB,"got %x expect %x at %x %x\n", 
						rlbuf[kk], lbuf[k], i, j);
					errors++;
				}
			if (rlbuf[kk] != 0)  {
				msg_printf (VRB,
				"got %x expect 0 at %x %x (%x,%d)\n",
					rlbuf[kk], i, j, (rlbuf+kk),kk);
				errors++;
			}
			k += stridepix;
			kk += inc;
		}
		break;
	}

	free (cbuf);
	free (rcbuf);

	if (errors) {
		msg_printf (SUM, "%d errors in %s ", errors, argv[0]);
		msg_printf (SUM, "dm1 %x sb %d stride %d dx %d\n", 
				dm1, sb, stride, nxpix);
		return -1;
	}
	else
                msg_printf(INFO, "All of the DMA tests have passed.\n");
	return 0;
}

	
int
newport_dma(char *buf, int x0, int y0, int x1, int y1, 
	int pixsize, int flags, int bytesperrow)
{
	int nbytes;
	int nrows;
	int maxrowspershot;
	int rv, dy;
	int xlenbytes = (x1 - x0 + 1) * pixsize;
	maxrowspershot = (256*1024)/bytesperrow;
	nrows = y1 - y0 + 1;
	dma.flags = 0;

	REX3WAIT(REX);
	REX->set.drawmode1 = dm1;
	if (flags & DMAFROMGFX) {		/* gfx to host */
		if (flags & STRIDEDMA)
			REX->set.drawmode0 = DM0_STOPONX|DM0_READ|DM0_BLOCK;
		else
			REX->set.drawmode0 = DM0_STOPONXY|DM0_READ|DM0_BLOCK;
	}
	else {
		REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK | DM0_COLORHOST;
		dma.flags = NG1_WRITE;
	}
	if (flags & STRIDEDMA) {
		dma.flags |= NG1_STRIDE;
		dma.pmstride = bytesperrow ;	/* stride is in bytes */
	}
	else
		dma.pmstride = 0 ;	/* stride is in bytes */

	dma.xzoom = dma.yzoom = 1;

	while (nrows > 0) {
		if (nrows > maxrowspershot)
			dy = maxrowspershot;
		else
			dy = nrows;
		nbytes = bytesperrow * dy;

        	REX->set.xystarti = x0 << 16 | y0;
        	REX->set.xyendi = x1 << 16 | (y0 + dy - 1);
		REX->set.bresoctinc1 = 0;	/* XXX increasing y only */
		dma.buf = buf;
		dma.gfxaddr = (void *)(&REX->go.hostrw0); 
		buf += nbytes;

		if (flags & (STRIDEDMA | DMAFROMGFX)) {
			dma.xlen = xlenbytes ;
			dma.ylen = dy;
			rv = vdma (&dma);
			if (rv)
				msg_printf (ERR, "pixel dma error\n");
		}
		else {
			while (nbytes >= 64*1024) {
				dma.xlen = 32768;
				dma.ylen = nbytes / 32768;
				rv = vdma (&dma);
				dma.buf += 32768 * dma.ylen;
				nbytes %= 32768;
				if (rv)
					msg_printf (ERR, "pixel dma error\n");
			}
			if (nbytes) {
				dma.xlen = nbytes;
				dma.ylen = 1;
				rv = vdma (&dma);
				if (rv)
					msg_printf (ERR, "pixel dma error\n");
			}
		}
		y0 += dy;
		nrows -= dy;
	}
	return rv;
}
#endif	/* ! PROM */
