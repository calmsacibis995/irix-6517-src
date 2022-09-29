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
 *  Patterns Test
 *  $Revision: 1.7 $
 */

#include <sys/param.h>
#include "sys/ng1hw.h" 
#include "ide_msg.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern Rex3chip *REX;
extern int dm1;

#define TIME 1024

int ng1bars(int argc, char **argv)
{
	int x, i;
	int width;

	if(! ng1checkboard())
		return -1;

	if (argc == 1)
		width = 128;
	else
		atob(argv[1], &width);

        REX3WAIT(REX);
	ng1_planes(DM1_RGBPLANES); 
	ng1_drawdepth(DM1_DRAWDEPTH8);
	ng1_setvisual(0);

	for (x = 0, i = 0; x < NG1_XSIZE; x += width, i++) {
		ng1_color(i % 256);
		ng1_block(x, 0, x+width-1, NG1_YSIZE-1, 0, 0);
	}
}


/*
 * Write a series of patterns to the framebuffer to test REX3.
 */
int ng1patterns(void)
{
	int x, y;
	float gray;
	float color[4][3];
	int i;

	if(! ng1checkboard())
		return -1;

	REX3WAIT(REX);
	ng1_planes(DM1_RGBPLANES); 
	ng1_drawdepth(DM1_DRAWDEPTH24);
	ng1_setvisual(5);

	printf("Black Screen\n");
	ng1_rgbcolor(0, 0, 0);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);

	DELAY (200);

	/* Gray screen */
	printf("Gray Screen\n");
	ng1_rgbcolor(128, 128, 128);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	DELAY (200);

	/* White screen */
	printf("White Screen\n");
	ng1_rgbcolor(255, 255, 255);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	DELAY (200);

	/* Black with white block */
	printf("Black with white block\n");
	ng1_rgbcolor(0, 0, 0);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 255, 255);
	ng1_block(NG1_XSIZE/4, NG1_YSIZE/4, 3*NG1_XSIZE/4, 3*NG1_YSIZE/4, 0, 0);
	DELAY (200);

	/* White with black block */
	printf("White with black block\n");
	ng1_rgbcolor(255, 255, 255);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(0, 0, 0);
	ng1_block(NG1_XSIZE/4, NG1_YSIZE/4, 3*NG1_XSIZE/4, 3*NG1_YSIZE/4, 0, 0);
	DELAY (200);

	/* Black with white grid pattern */
	printf("Black with white grid pattern\n");
	ng1_rgbcolor(0, 0, 0);
	ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 255, 255);
	for (x = 0; x <= NG1_XSIZE-1; x += 32)
		ng1_line(x, 0, x, NG1_YSIZE-1, 0);
	ng1_line(NG1_XSIZE-1, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0);
	for (y = 0; y <= NG1_YSIZE-1; y += 32)
		ng1_line(0, y, NG1_XSIZE-1, y, 0);
	ng1_line(0, NG1_YSIZE-1, NG1_XSIZE-1, NG1_YSIZE-1, 0 );
	DELAY (200);

#if 0
	/* Gray scale */
	printf("Gray scale\n");
	for (x = 0, gray = 0.0; x < NG1_XSIZE; x += 32, gray += 255.0/31.0) {
		ng1_rgbcolor((int)gray, (int)gray, (int)gray);
		ng1_block(x, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	}
	for (x = 0, gray = 1.0; x < NG1_XSIZE; x += 32, gray -= 255.0/31.0) {
		ng1_rgbcolor((int)gray, (int)gray, (int)gray);
		ng1_block(x, 384, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);
	}
	DELAY (200);
#endif
	/* Color bars */
	printf("Color bars\n");
	ng1_rgbcolor(0, 0, 0);
	ng1_block(0, 0, 1*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 0, 0);
	ng1_block(1*NG1_XSIZE/8, 0, 2*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(0, 255, 0);
	ng1_block(2*NG1_XSIZE/8, 0, 3*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 255, 0);
	ng1_block(3*NG1_XSIZE/8, 0, 4*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(0, 0, 255);
	ng1_block(4*NG1_XSIZE/8, 0, 5*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 0, 255);
	ng1_block(5*NG1_XSIZE/8, 0, 6*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(0, 255, 255);
	ng1_block(6*NG1_XSIZE/8, 0, 7*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	ng1_rgbcolor(255, 255, 255);
	ng1_block(7*NG1_XSIZE/8, 0, 8*NG1_XSIZE/8-1, NG1_YSIZE-1, 0, 0);
	DELAY (200);

	/* Color spans */
	printf("Color spans\n");
	color[0][0] = 1.0;    
	color[0][1] = 1.0;    
	color[0][2] = 1.0;
	color[1][0] = 1.0;    
	color[1][1] = 1.0;    
	color[1][2] = 1.0;

	color[2][0] = 0.0;    
	color[2][1] = 0.0;    
	color[2][2] = 0.0;
	color[3][0] = 0.0;    
	color[3][1] = 0.0;    
	color[3][2] = 0.0;
	ng1_polygon(0*NG1_XSIZE/8, 0, 1*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 1.0;    
	color[2][1] = 0.0;    
	color[2][2] = 0.0;
	color[3][0] = 1.0;    
	color[3][1] = 0.0;    
	color[3][2] = 0.0;
	ng1_polygon(1*NG1_XSIZE/8, 0, 2*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 0.0;    
	color[2][1] = 1.0;    
	color[2][2] = 0.0;
	color[3][0] = 0.0;    
	color[3][1] = 1.0;    
	color[3][2] = 0.0;
	ng1_polygon(2*NG1_XSIZE/8, 0, 3*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 1.0;    
	color[2][1] = 1.0;    
	color[2][2] = 0.0;
	color[3][0] = 1.0;    
	color[3][1] = 1.0;    
	color[3][2] = 0.0;
	ng1_polygon(3*NG1_XSIZE/8, 0, 4*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 0.0;    
	color[2][1] = 0.0;    
	color[2][2] = 1.0;
	color[3][0] = 0.0;    
	color[3][1] = 0.0;    
	color[3][2] = 1.0;
	ng1_polygon(4*NG1_XSIZE/8, 0, 5*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 1.0;    
	color[2][1] = 0.0;    
	color[2][2] = 1.0;
	color[3][0] = 1.0;    
	color[3][1] = 0.0;    
	color[3][2] = 1.0;
	ng1_polygon(5*NG1_XSIZE/8, 0, 6*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 0.0;    
	color[2][1] = 1.0;    
	color[2][2] = 1.0;
	color[3][0] = 0.0;    
	color[3][1] = 1.0;    
	color[3][2] = 1.0;
	ng1_polygon(6*NG1_XSIZE/8, 0, 7*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);
	color[2][0] = 1.0;    
	color[2][1] = 1.0;    
	color[2][2] = 1.0;
	color[3][0] = 1.0;    
	color[3][1] = 1.0;    
	color[3][2] = 1.0;
	ng1_polygon(7*NG1_XSIZE/8, 0, 8*NG1_XSIZE/8-1, NG1_YSIZE/2-1, color);

	color[2][0] = 0.0;    
	color[2][1] = 0.0;    
	color[2][2] = 0.0;
	color[3][0] = 0.0;    
	color[3][1] = 0.0;    
	color[3][2] = 0.0;

	color[0][0] = 0.0;    
	color[0][1] = 0.0;    
	color[0][2] = 0.0;
	color[1][0] = 0.0;    
	color[1][1] = 0.0;    
	color[1][2] = 0.0;
	ng1_polygon(0*NG1_XSIZE/8, 0, 1*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 1.0;    
	color[0][1] = 0.0;    
	color[0][2] = 0.0;
	color[1][0] = 1.0;    
	color[1][1] = 0.0;    
	color[1][2] = 0.0;
	ng1_polygon(1*NG1_XSIZE/8, 0, 2*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 0.0;    
	color[0][1] = 1.0;    
	color[0][2] = 0.0;
	color[1][0] = 0.0;    
	color[1][1] = 1.0;    
	color[1][2] = 0.0;
	ng1_polygon(2*NG1_XSIZE/8, 0, 3*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 1.0;    
	color[0][1] = 1.0;    
	color[0][2] = 0.0;
	color[1][0] = 1.0;    
	color[1][1] = 1.0;    
	color[1][2] = 0.0;
	ng1_polygon(3*NG1_XSIZE/8, 0, 4*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 0.0;    
	color[0][1] = 0.0;    
	color[0][2] = 1.0;
	color[1][0] = 0.0;    
	color[1][1] = 0.0;    
	color[1][2] = 1.0;
	ng1_polygon(4*NG1_XSIZE/8, 0, 5*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 1.0;    
	color[0][1] = 0.0;    
	color[0][2] = 1.0;
	color[1][0] = 1.0;    
	color[1][1] = 0.0;    
	color[1][2] = 1.0;
	ng1_polygon(5*NG1_XSIZE/8, 0, 6*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 0.0;    
	color[0][1] = 1.0;    
	color[0][2] = 1.0;
	color[1][0] = 0.0;    
	color[1][1] = 1.0;    
	color[1][2] = 1.0;
	ng1_polygon(6*NG1_XSIZE/8, 0, 7*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	color[0][0] = 1.0;    
	color[0][1] = 1.0;    
	color[0][2] = 1.0;
	color[1][0] = 1.0;    
	color[1][1] = 1.0;    
	color[1][2] = 1.0;
	ng1_polygon(7*NG1_XSIZE/8, 0, 8*NG1_XSIZE/8-1, NG1_YSIZE-1, color);
	DELAY (200);

	/* Shaded polygon */
	color[0][0] = 1.0;    
	color[0][1] = 0.0;    
	color[0][2] = 0.0;
	color[1][0] = 0.0;    
	color[1][1] = 1.0;    
	color[1][2] = 0.0;
	color[2][0] = 0.0;    
	color[2][1] = 0.0;    
	color[2][2] = 1.0;
	color[3][0] = 1.0;    
	color[3][1] = 1.0;    
	color[3][2] = 1.0;
	ng1_polygon(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, color);
	DELAY (200);
}
