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
 *  VRAM Diagnostics
 *  $Revision: 1.5 $
 */

#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <uif.h>
#include <libsc.h>

#define RexWait while ((REX->p1.set.configmode) & CHIPBUSY)

extern int f_planes;
extern int f_xywin;
extern int f_rgb;
extern Rexchip *REX;

/*
 * print_vram()
 *
 *	Read a 64 by 20 block of VRAM, mask off the high nibble
 *	and print to the terminal.
 */
int print_vram(void)
{
    int x, y;
    int x0, y0;
    unsigned int c;
    int savex, savey;
    static unsigned char ar[] = { '.', '1', '2', '3', '4', '5', '6', '7',
				  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    printf("xywin = %x  planes = %x\n", f_xywin, f_planes);
    x0 = (f_xywin >> 16) - 0x800;
    y0 = (f_xywin & 0xffff) - 0x800;
    savex = REX->set.xstart.word;
    savey = REX->set.ystart.word;
    REX->set.xendi = 0x7ff;
    REX->set.yendi = 0x7ff;
    for (y = 19; y >= 0; y--) {
	if ((y+y0) % 5 == 0)
	    printf("%4d+", y+y0);
	else
	    printf("    |");
	REX->set.ystarti = y;
	for (x = 0; x < 64; x += 4) {
	    REX->set.xstarti = x;
	    REX->go.command = QUADMODE | REX_LDPIXEL;
	    REX->set.command = 0;
	    c = REX->set.rwaux1;
	    printf("%c%c%c%c", ar[(c>>24)&0xf], ar[(c>>16)&0xf],
		   ar[(c>>8)&0xf], ar[c&0xf]);
	}
	printf("\n");
    }
    printf("    *");
    for (x = 0; x < 64; x++)
	if ((x+x0) % 5 == 0)
	    printf("+");
	else
	    printf("-");
    printf("\n");
    printf("     ");
    x = 0;
    while ((x+x0) % 5 != 0) {
	printf(" ");
	x++;
    }
    for (;x < 64; x += 5)
	printf("%-5d", x+x0);
    printf("\n");
    REX->set.xstart.word = savex;
    REX->set.ystart.word = savey;
    REX->go.command = REX_NOP;
}


int print_vram1(void)
{
    int x, y;
    int x0, y0;
    unsigned int c;
    volatile int tmp;
    int savex, savey;
    static unsigned char ar[] = { '.', '1', '2', '3', '4', '5', '6', '7',
				  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    printf("xywin = %x  planes = %x\n", f_xywin, f_planes);
    x0 = (f_xywin >> 16) - 0x800;
    y0 = (f_xywin & 0xffff) - 0x800;
    savex = REX->set.xstart.word;
    savey = REX->set.ystart.word;
    REX->set.xendi = 0x7ff;
    REX->set.yendi = 0x7ff;
    for (y = 19; y >= 0; y--) {
	if ((y+y0) % 5 == 0)
	    printf("%4d+", y+y0);
	else
	    printf("    |");
	REX->set.ystarti = y;
	for (x = 0; x < 64; x++) {
	    REX->set.xstarti = x;
	    REX->set.command = REX_LDPIXEL;
	    tmp = REX->go.rwaux1;
	    c = REX->set.rwaux1;
	    printf("%c", ar[(c>>24)&0xf]);
	}
	printf("\n");
    }
    printf("    *");
    for (x = 0; x < 64; x++)
	if ((x+x0) % 5 == 0)
	    printf("+");
	else
	    printf("-");
    printf("\n");
    printf("     ");
    x = 0;
    while ((x+x0) % 5 != 0) {
	printf(" ");
	x++;
    }
    for (;x < 64; x += 5)
	printf("%-5d", x+x0);
    printf("\n");
    REX->set.xstart.word = savex;
    REX->set.ystart.word = savey;
    REX->go.command = REX_NOP;
}

int test_vram(int argc, char **argv)
{
    int x, y;
    int i, j, k;
    int color;
    int r, g, b;
    int quad;
    int got;
    int totalerrors = 0;
    int rgb_tmp;
    int yres, ysize;
    static int high[] = { 1, 3, 5, 8 };
    static int low[] = { 0, 2, 4, 7 };
 
    if (argc > 1)
	yres = 816;
    else
	yres = 768;
    ysize = yres/16;

    msg_printf(VRB, "Starting VRAM tests...\n");
    rgb_tmp = f_rgb;
    f_rgb = RGBMODECMD;
    for (i = 0; i < 4; i++) {
	color = 0x55*i;
	msg_printf(VRB, "Filling VRAM with %#02x...\n", color);
	r = 0x24 * (color >> 0x5);
	g = 0x24 * (color % 0x8);
	b = 0x55 * ((color % 0x20) >> 0x3);
	_rgbcolor(r, g, b);
	_block(0, 0, 1023, yres-1);
	RexWait;
	for (x = 0; x < 1024; x += 4)
	    for (y = 0; y < yres; y++) {
		REX->set.xstarti = x;
		REX->set.ystarti = y;
		REX->set.command = REX_LO_SRC | QUADMODE | f_rgb | REX_LDPIXEL;
		quad = REX->go.rwaux1;
		quad = REX->set.rwaux1;
		for (k = 0; k < 4; k++) {
		    got = (quad >> 8*k) & 0xff;
		    if (got != color) {
			if ((got & 0xf0) != (color & 0xf0)) {
			    msg_printf(ERR, "Error reading from VRAM U2%d, returned: %#02x, expected: %#02x\n", high[(x+k)%4], got, color);
			    totalerrors++;
			}
			if ((got & 0x0f) != (color & 0x0f)) {
			    msg_printf(ERR, "Error reading from VRAM U2%d, returned: %#02x, expected: %#02x\n", low[(x+k)%4], got, color);
			    totalerrors++;
			}
		    }
		}
	    }
    }
    for (x = 0; x < 1024; x += 64) {
	for (y = 0; y < yres; y += ysize) {
	    color = x/64 + y/ysize*16;
	    r = 0x24 * (color >> 0x5);
	    g = 0x24 * (color % 0x8);
	    b = 0x55 * ((color % 0x20) >> 0x3);
	    _rgbcolor(r, g, b);
	    _block(x, y, x+63, y+ysize-1);
	    RexWait;
	    for (i = 0; i < 64; i += 4)
		for (j = 0; j < ysize; j++) {
		    REX->set.xstarti = x+i;
		    REX->set.ystarti = y+j;
		    REX->set.command = REX_LO_SRC | QUADMODE | f_rgb | REX_LDPIXEL;
		    quad = REX->go.rwaux1;
		    quad = REX->set.rwaux1;
		    for (k = 0; k < 4; k++) {
			got = (quad >> 8*k) & 0xff;
			if (got != color) {
			    if ((got & 0xf0) != (color & 0xf0)) {
				msg_printf(ERR, "Error reading from VRAM U2%d, returned: %#02x, expected: %#02x\n", high[(x+i+k)%4], got, color);
				totalerrors++;
			    }
			    if ((got & 0x0f) != (color & 0x0f)) {
				msg_printf(ERR, "Error reading from VRAM U2%d, returned: %#02x, expected: %#02x\n", low[(x+i+k)%4], got, color);
				totalerrors++;
			    }
			}
		    }
		}
	}
    }
    f_rgb = rgb_tmp;
    if (totalerrors) {
	msg_printf(SUM,"Total errors in VRAM test: %d\n", totalerrors);
	return -1;
    } else {
	okydoky();
	return 0;
    }
}


