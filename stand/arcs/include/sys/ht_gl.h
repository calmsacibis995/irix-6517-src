/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/**************************************************************************/
/* 
** Standalone version of gl.h 
*/
/**************************************************************************/


#define XMAXSCREEN 1279
#define YMAXSCREEN 1023
#define XMAXMEDIUM 1023
#define YMAXMEDIUM 767

#define BLACK	0
#define RED	1
#define GREEN	2
#define YELLOW	3
#define BLUE	4
#define MAGENTA	5
#define CYAN	6
#define WHITE	7

typedef unsigned short Colorindex;

/*
** global data for character sets
**
*/
#define Byte unsigned char

typedef struct {
    unsigned short offset;
    Byte w, h;
    signed char xoff, yoff;
    short width;
} Fontchar;

typedef struct Bitmap {
    unsigned short *base;
    short xsize, ysize;
    short xorig, yorig;
    short xmove, ymove;
    short sper;
} Bitmap;

extern Fontchar hc_font[];
extern unsigned short hc_raster[];

