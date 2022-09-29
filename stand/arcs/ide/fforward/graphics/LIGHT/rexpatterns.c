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
 *  $Revision: 1.5 $
 */

#include <sys/param.h>
#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <libsc.h>
#include <libsk.h>

#define TIME 1024

int bars(int argc, char **argv)
{
    int x, i;
    int width;

    if (argc == 1)
	width = 128;
    else
	atob(argv[1], &width);

    for (x = 0, i = 0; x < 1024; x += width, i++) {
	_color(i % 256);
	_block(x, 0, x+width-1, 767);
    }
}


/*
 * Write a series of patterns to the framebuffer to test REX.
 */
int rexpatterns(void)
{
    long x, y;
    float gray;
    float color[4][3];

    /* Black screen */
    _rgbcolor(0, 0, 0);
    _block(0, 0, 1023, 767);
    DELAY(TIME);
    
    /* Gray screen */
    _rgbcolor(128, 128, 128);
    _block(0, 0, 1023, 767);
    DELAY(TIME);
    
    /* White screen */
    _rgbcolor(255, 255, 255);
    _block(0, 0, 1023, 767);
    DELAY(TIME);
    
    /* Black with white block */    
    _rgbcolor(0, 0, 0);
    _block(0, 0, 1023, 767);
    _rgbcolor(255, 255, 255);
    _block(256, 192, 767, 575);
    DELAY(TIME);
    
    /* White with black block */
    _rgbcolor(255, 255, 255);
    _block(0, 0, 1023, 767);
    _rgbcolor(0, 0, 0);
    _block(256, 192, 767, 575);
    DELAY(TIME);
    
    /* Black with white grid pattern */
    _rgbcolor(0, 0, 0);
    _block(0, 0, 1023, 767);
    _rgbcolor(255, 255, 255);
    for (x = 0; x <= 1023; x += 32)
	_line(x, 0, x, 767);
    _line(1023, 0, 1023, 767);
    for (y = 0; y <= 767; y += 32)
	_line(0, y, 1023, y);
    _line(0, 767, 1023, 767);
    DELAY(TIME);
    
    /* Gray scale */
    for (x = 0, gray = 0.0; x < 1024; x += 32, gray += 255.0/31.0) {
	_rgbcolor((int)gray, (int)gray, (int)gray);
	_block(x, 0, 1023, 767);
    }
    for (x = 0, gray = 1.0; x < 1024; x += 32, gray -= 255.0/31.0) {
	_rgbcolor((int)gray, (int)gray, (int)gray);
	_block(x, 384, 1023, 767);
    }    
    DELAY(TIME);
    
    /* Color bars */
    _rgbcolor(0, 0, 0);
    _block(0, 0, 127, 767);
    _rgbcolor(255, 0, 0);
    _block(128, 0, 255, 767);
    _rgbcolor(0, 255, 0);
    _block(256, 0, 383, 767);
    _rgbcolor(255, 255, 0);
    _block(384, 0, 511, 767);
    _rgbcolor(0, 0, 255);
    _block(512, 0, 639, 767);
    _rgbcolor(255, 0, 255);
    _block(640, 0, 767, 767);
    _rgbcolor(0, 255, 255);
    _block(768, 0, 895, 767);
    _rgbcolor(255, 255, 255);
    _block(896, 0, 1023, 767);
    DELAY(TIME);

    /* Color spans */
    color[0][0] = 1.0;    color[0][1] = 1.0;    color[0][2] = 1.0;
    color[1][0] = 1.0;    color[1][1] = 1.0;    color[1][2] = 1.0;

    color[2][0] = 0.0;    color[2][1] = 0.0;    color[2][2] = 0.0;
    color[3][0] = 0.0;    color[3][1] = 0.0;    color[3][2] = 0.0;
    _polygon(0, 0, 127, 383, color);
    color[2][0] = 1.0;    color[2][1] = 0.0;    color[2][2] = 0.0;
    color[3][0] = 1.0;    color[3][1] = 0.0;    color[3][2] = 0.0;
    _polygon(128, 0, 255, 383, color);
    color[2][0] = 0.0;    color[2][1] = 1.0;    color[2][2] = 0.0;
    color[3][0] = 0.0;    color[3][1] = 1.0;    color[3][2] = 0.0;
    _polygon(256, 0, 383, 383, color);
    color[2][0] = 1.0;    color[2][1] = 1.0;    color[2][2] = 0.0;
    color[3][0] = 1.0;    color[3][1] = 1.0;    color[3][2] = 0.0;
    _polygon(384, 0, 511, 383, color);
    color[2][0] = 0.0;    color[2][1] = 0.0;    color[2][2] = 1.0;
    color[3][0] = 0.0;    color[3][1] = 0.0;    color[3][2] = 1.0;
    _polygon(512, 0, 639, 383, color);
    color[2][0] = 1.0;    color[2][1] = 0.0;    color[2][2] = 1.0;
    color[3][0] = 1.0;    color[3][1] = 0.0;    color[3][2] = 1.0;
    _polygon(640, 0, 767, 383, color);
    color[2][0] = 0.0;    color[2][1] = 1.0;    color[2][2] = 1.0;
    color[3][0] = 0.0;    color[3][1] = 1.0;    color[3][2] = 1.0;
    _polygon(768, 0, 895, 383, color);
    color[2][0] = 1.0;    color[2][1] = 1.0;    color[2][2] = 1.0;
    color[3][0] = 1.0;    color[3][1] = 1.0;    color[3][2] = 1.0;
    _polygon(896, 0, 1023, 383, color);

    color[2][0] = 0.0;    color[2][1] = 0.0;    color[2][2] = 0.0;
    color[3][0] = 0.0;    color[3][1] = 0.0;    color[3][2] = 0.0;

    color[0][0] = 0.0;    color[0][1] = 0.0;    color[0][2] = 0.0;
    color[1][0] = 0.0;    color[1][1] = 0.0;    color[1][2] = 0.0;
    _polygon(0, 384, 127, 767, color);
    color[0][0] = 1.0;    color[0][1] = 0.0;    color[0][2] = 0.0;
    color[1][0] = 1.0;    color[1][1] = 0.0;    color[1][2] = 0.0;
    _polygon(128, 384, 255, 767, color);
    color[0][0] = 0.0;    color[0][1] = 1.0;    color[0][2] = 0.0;
    color[1][0] = 0.0;    color[1][1] = 1.0;    color[1][2] = 0.0;
    _polygon(256, 384, 383, 767, color);
    color[0][0] = 1.0;    color[0][1] = 1.0;    color[0][2] = 0.0;
    color[1][0] = 1.0;    color[1][1] = 1.0;    color[1][2] = 0.0;
    _polygon(384, 384, 511, 767, color);
    color[0][0] = 0.0;    color[0][1] = 0.0;    color[0][2] = 1.0;
    color[1][0] = 0.0;    color[1][1] = 0.0;    color[1][2] = 1.0;
    _polygon(512, 384, 639, 767, color);
    color[0][0] = 1.0;    color[0][1] = 0.0;    color[0][2] = 1.0;
    color[1][0] = 1.0;    color[1][1] = 0.0;    color[1][2] = 1.0;
    _polygon(640, 384, 767, 767, color);
    color[0][0] = 0.0;    color[0][1] = 1.0;    color[0][2] = 1.0;
    color[1][0] = 0.0;    color[1][1] = 1.0;    color[1][2] = 1.0;
    _polygon(768, 384, 895, 767, color);
    color[0][0] = 1.0;    color[0][1] = 1.0;    color[0][2] = 1.0;
    color[1][0] = 1.0;    color[1][1] = 1.0;    color[1][2] = 1.0;
    _polygon(896, 384, 1023, 767, color);
    DELAY(TIME);

    /* Shaded polygon */
    color[0][0] = 1.0;    color[0][1] = 0.0;    color[0][2] = 0.0;
    color[1][0] = 0.0;    color[1][1] = 1.0;    color[1][2] = 0.0;
    color[2][0] = 0.0;    color[2][1] = 0.0;    color[2][2] = 1.0;
    color[3][0] = 1.0;    color[3][1] = 1.0;    color[3][2] = 1.0;
    _polygon(0, 0, 1023, 767, color);
    DELAY(TIME);
}


