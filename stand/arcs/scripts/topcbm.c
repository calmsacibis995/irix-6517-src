/*
 *	topcbm - convert a .rgb image prom color bitmap source
 */

#include <strings.h>
#include <image.h>
#include "assert.h"

short rbuf[1024]; 
short gbuf[1024]; 
short bbuf[1024]; 

unsigned short line[1024];

struct cdata {
	int r,g,b;
	int x1,y1,x2,y2;
	int ref;
} cdata[128];

void checkcolor(int x, int y);

main(int argc, char **argv)
{
	char imagename[256];
	register IMAGE *image;
	register int x, xsize;
	register int y, ysize;
	register int zsize;
	long machclr[5];
	int i;
	char *p;
	
	if (argc < 2) {
		fprintf(stderr, "usage: %s inimage.rgb\n", argv[0]);
		exit(1);
	} 
	if ((image = iopen(argv[1], "r")) == NULL ) {
		fprintf(stderr, "readimg: can't open input file %s\n", argv[1]);
		exit(1);
	}

	strcpy(imagename,argv[1]);
	p = strstr(imagename,".rgb");
	if (p) *p = '\0';

	xsize = image->xsize;
	ysize = image->ysize;
	zsize = image->zsize;

	printf("/* prom color bitmap: %s */\n",imagename);

	for (y = 0; y < ysize; y++) {

		getrow(image, rbuf, y, 0);
		getrow(image, gbuf, y, 1);
		getrow(image, bbuf, y, 2);

		for (x = 0; x < xsize; x++) {
			checkcolor(x,y);
		}
	}

	/* print bitmaps for each color */
	for (i=0; cdata[i].ref; i++) {
		int r = cdata[i].r;
		int g = cdata[i].g;
		int b = cdata[i].b;
		int clr, j, tx, nsh, bit, idx;
		char *cl;

		switch (clr=(r<<16|g<<8|b)) {
		case 0:
			cl = "BLACK";
			break;
		case 0xff0000:
			cl = "RED";
			break;
		case 0x00ff00:
			cl = "GREEN";
			break;
		case 0xffff00:
			cl = "YELLOW";
			break;
		case 0x0000ff:
			cl = "BLUE";
			break;
		case 0xff00ff:
			cl = "MAGENTA";
			break;
		case 0x00ffff:
			cl = "CYAN";
			break;
		case 0xffffff:
			cl = "WHITE";
			break;
		case 0x555555:				/* DarkGray -- 85 */
			cl = "BLACK2";
			break;
		case 0xc67171:
			cl = "RED2";
			break;
		case 0x71c671:				/* 113 198 113 */
			cl = "GREEN2";
			break;
		case 0x8e8e38:				/* 142 142 56 */
			cl = "YELLOW2";
			break;
		case 0x7171c6:				/* 113 113 198 */
			cl = "BLUE2";
			break;
		case 0x883388:				/* 136 51 136 uigrp */
		case 0x8e388e:				/* 142 56 142 */
			cl = "MAGENTA2";
			break;
		case 0x388383:				/* 56 131 131 */
			cl = "CYAN2";
			break;
		case 0xaaaaaa:
			cl = "WHITE2";			/* LightGray -- 170 */
			break;
		case 0xdddddd:				/* 221 uigrp */
		case 0xd5d5d5:				/* 213 */
			cl = "VeryLightGray";
			break;
		case 0x888888:				/* 136 uigrp */
		case 0x808080:				/* 128 */
			cl = "MediumGray";
			break;
		case 0x222222:				/* 34 uigrp */
		case 0x2a2a2a:				/* 42 */
			cl = "VeryDarkGray";
			break;
		case 0x606070:				/* 96 96 112 */
			cl = "TP_IDX";
			break;
		case 0x389279:				/* Indigo2 */
		case 0x013057:				/* Indy */
		case 0x082f4a:				/* SR */
			cl = "MACHCLR0";
			machclr[0] = clr;
			break;
		case 0x097057:				/* Indigo2 */
		case 0x236fb4:				/* Indy */
		case 0x008b9b:				/* SR */
			cl = "MACHCLR1";
			machclr[1] = clr;
			break;
		case 0x3fffbf:				/* Indigo2 */
		case 0x094575:				/* Indy */
		case 0x574763:				/* SR */
			cl = "MACHCLR2";
			machclr[2] = clr;
			break;
		case 0x3f917f:				/* Indigo2 */
			cl = "MACHCLR3";
			machclr[3] = clr;
			break;
		default:
			cl = "XXX";
			break;
		}
		printf("#define %s_%d_color	%s	/* %d %d %d */\n",
			imagename,i,cl,r,g,b);
		printf("unsigned short %s_%d_bits[] = {\n",imagename,i);
		for (y=cdata[i].y1; y <= cdata[i].y2; y++) {
			getrow(image,rbuf,y,0);
			getrow(image,gbuf,y,1);
			getrow(image,bbuf,y,2);

			bzero(line,sizeof(line));
			for (x=cdata[i].x1; x <= cdata[i].x2; x++) {
				if ( (r == rbuf[x]) &&
				     (g == gbuf[x]) &&
				     (b == bbuf[x]) ) {
					tx = x-cdata[i].x1;
					idx = tx / 16;
					bit = tx % 16;
					line[idx] |= (1<<(15-bit));
				}
			}
			bit = (cdata[i].x2-cdata[i].x1+1);
			nsh = bit/16 + ((bit%16) != 0);
			for (j=0; j < nsh; j++)	
				printf("0x%.4x, ",line[j]);
			printf("\n");
		}
		printf("};\n");
	}

	/* dump node table */
	printf("\nstruct pcbm_node %s_nodes[] = {\n",imagename);
	for (i=0; cdata[i].ref; i++) {
		printf("\t{%s_%d_color,%d,%d,%d,%d,%s_%d_bits},\n",
			imagename,i,
			cdata[i].x1,cdata[i].y1,
			cdata[i].x2-cdata[i].x1+1,
			cdata[i].y2-cdata[i].y1+1,
			imagename,i);
	}
	printf("\t{0,0,0,0,0,0}\n};\n");

	/* dump the main table */
	printf("struct pcbm %s = {\n\t%d,\n\t%d,\n\t0,\n\t0,\n\t%s_nodes\n};\n",
		imagename,
		xsize,
		ysize,
		imagename);

	/* dump mach dependent colors */
	printf("\n");
	for (i=0; i<5;i++) {
		if (machclr[i])
			printf("#define MACHCLR%d_CLR\t0x%x\n",i,machclr[i]);
	}
}

void
checkcolor(int x, int y)
{
	int i;

	for (i=0; cdata[i].ref; i++) {
		if ( (cdata[i].r == rbuf[x]) &&
		     (cdata[i].g == gbuf[x]) &&
		     (cdata[i].b == bbuf[x]) ) {
			cdata[i].ref++;
			/* update bounding box */
			if (x < cdata[i].x1)
				cdata[i].x1 = x;
			else if (x > cdata[i].x2)
				cdata[i].x2 = x;
			if (y < cdata[i].y1)
				cdata[i].y1 = y;
			else if (y > cdata[i].y2)
				cdata[i].y2 = y;
			return;
		}
	}
	cdata[i].ref = 1;
	cdata[i].r = rbuf[x];
	cdata[i].g = gbuf[x];
	cdata[i].b = bbuf[x];
	cdata[i].x1 = cdata[i].x2 = x;
	cdata[i].y1 = cdata[i].y2 = y;
}
