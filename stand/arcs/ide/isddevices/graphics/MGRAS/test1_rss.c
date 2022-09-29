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
 *  DAC CRC Diags.
 *
 *  $Revision: 1.2 $
 */

#include <sys/types.h>
#include <math.h>
#include "./mgras_color_z.h"
#include "./mgras_z.h"
#include "./mgras_tetr.h"

#ifndef _STANDALONE
#include "stdio.h"
#endif

void 	ComputeCrc(__uint32_t testPattern);
int 	ResetDacCrc() ;

/* stuff for PP_draw */
__uint32_t points_array[24] = {
                0xff8080ff, 0x0, 0x0, 0x0, 0x0, 0x0,
                0x0, 0x0, 0x7e33ff7f, 0x33ff80, 0x0, 0x0,
                0x0, 0x0, 0x7f33ff80, 0x33ff7f, 0x0, 0x0,
                0x0, 0x0, 0x0, 0x0, 0xffff3f7f, 0x0};

__uint32_t  tex_poly[64] = {
        0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff,
0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee,
0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33};

__uint32_t logicop_array[48] = {
        0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc,
        0x0, 0x0, 0xcccccc, 0xcccccc, 0x0, 0x0, 0xcccccc, 0xcccccc, 0x333333, 0x333333, 0xffffff, 0xffffff, 0x333333, 0x333333, 0xffffff, 0xffffff,
        0x0, 0x88cccc, 0x440000,0xcccccc, 0x2200, 0x88eecc, 0x442200, 0xcceecc,
0x331133, 0xbbddff, 0x771133, 0xffddff, 0x333333, 0xbbffff, 0x773333, 0xffffff};

__uint32_t dither_array[32] = {
        0x0, 0xd00000d, 0x27000027, 0x40000040, 0x5a00005a, 0x73000073, 0x8d00008d, 0xa60000a6, 0xc00000c0, 0xd90000d9, 0xf30000f3, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff, 0x0,
0x0, 0xc00000c, 0x26000026, 0x40000040, 0x59000059, 0x73000073, 0x8c00008c, 0xa60000a6, 0xbf0000bf, 0xd90000d9, 0xf30000f3, 0xff0000ff, 0xff0000ff, 0xff0000ff,
0xff0000ff, 0x0};

__uint32_t telod_32[32] = {
0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333,
0x33333333, 0x0, 0x0};

__uint32_t telod_128[128] = {
0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955,
0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33,
0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff,
0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33,
0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x5555dd77};

__uint32_t telod_128_4tram[128] = {
0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955,
0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33,
0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff,
0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33,
0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5555d56e};


/************************************************************************/
/*									*/
/*	Signature(32:0)	= Previous(32:0) Xor Input(32:0) ;		*/
/*									*/
/*	The real Equation is ::					        */
/*									*/
/*		Signature(32:0) = Not(Previous(32:0)) Xnor Input(32:0)	*/
/*									*/
/*	which is the same as 						*/
/*		Signature(32:0) = Previous(32:0) Xor Input(32:0)	*/
/*									*/
/*	Input(32) 	= 	0;					*/
/*	Input(31) 	= 	0;					*/
/*	Input(30:21) 	= 	Red(9:0);				*/
/*	Input(20:11) 	= 	Green(9:0);				*/
/*	Input(10:1) 	= 	Blue(9:0);				*/
/*	Input(0)	=	Signature(19);				*/
/*									*/
/*	Previous(32:1) 	= 	Signature(31:0);			*/
/*	Previous(0) 	= 	Signature(32);				*/
/*									*/
/************************************************************************/
typedef struct _GammaSize{
	unsigned channel : 10 ;
}GammaSize;

typedef struct _input{
	unsigned Bit0 	  : 1  ;
	unsigned blue	  : 10 ;
	unsigned green    : 10 ;
	unsigned red      : 10 ;
	unsigned Bit31	  : 1  ;
	unsigned Bit32	  : 1  ;
}input;


input	Input,       *In ;
input	Previous,    *Prev ;
input 	Signature,   *ExpSign ;
input	HWSignature, *RcvSign ;


GammaSize	Red  ;
GammaSize	Blue ;
GammaSize	Green;

GammaSize	RedGamma[256] ;
GammaSize	GrnGamma[256] ;
GammaSize	BluGamma[256] ;

/*****************************************************************************/

#define 	MGRAS_DAC_RED_SIG	0x10
#define 	MGRAS_DAC_GRN_SIG	0x11
#define 	MGRAS_DAC_BLU_SIG	0x12
#define 	MGRAS_DAC_MISC_SIG	0x13

/*****************************************************************************/
#define	CLOCKS	(1280 * 1024)

void
ComputeCrc(__uint32_t testPattern)
{
	GammaSize	ExpRed ;
	GammaSize	ExpBlu ;
	GammaSize	ExpGrn ;
	__uint32_t	clocks;

	Red.channel    = (testPattern >> 20) & 0x3ff ;
	Green.channel  = (testPattern >> 10) & 0x3ff ;
	Blue.channel   = testPattern & 0x3ff ;

	In->Bit32 = 0 ;
	In->Bit31 = 0 ;
        In->red   = Red.channel ;
        In->green = Green.channel ;
        In->blue  = Blue.channel ;
       	In->Bit0  = 0;

	Prev->Bit32 = ExpSign->Bit32 = 0 ;
	Prev->Bit31 = ExpSign->Bit31 = 0 ;
        Prev->red   = ExpSign->red = 0 ;
        Prev->green = ExpSign->green = 0 ;
        Prev->blue  = ExpSign->blue = 0 ;
       	Prev->Bit0  = ExpSign->Bit0 = 0;

	for(clocks= 0;  clocks < CLOCKS ; clocks++ ) {

		Prev->Bit32 = ExpSign->Bit31 ;
		Prev->Bit31 = (ExpSign->red >> 9) & 0x1; ;
        	Prev->red   = ((ExpSign->green >> 9) & 0x1) | ((ExpSign->red << 1) & 0x3fe) ;
        	Prev->green = ((ExpSign->blue >> 9) & 0x1) | ((ExpSign->green << 1) & 0x3fe)  ;
        	Prev->blue  = (ExpSign->Bit0  & 0x1) | ((ExpSign->blue << 1) & 0x3fe)  ;
       		Prev->Bit0  = ExpSign->Bit32;

       		ExpSign->Bit0  = In->Bit0 ^ Prev->Bit0;
        	ExpSign->blue  = In->blue ^ Prev->blue;
        	ExpSign->green = In->green ^ Prev->green;
        	ExpSign->red   = In->red ^ Prev->red;
		ExpSign->Bit31 = In->Bit31 ^ Prev->Bit31;
		ExpSign->Bit32 = In->Bit32 ^ Prev->Bit32; 

		In->Bit31 = 0 ;
		In->Bit32 = 0 ;
		In->Bit0 = (ExpSign->green >> 8) & 0x1;

	}

        ExpRed.channel  = ExpSign->red;
        ExpGrn.channel  = ExpSign->green;
	ExpBlu.channel  = ExpSign->blue;

	fprintf(stdout, "\t\tGoldcase	0x%x	:\n" ,testPattern);
	fprintf(stdout, "\t\t\t/* Red = 0x%x\tGreen=0x%x\tBlue=0x%x */\n" ,(testPattern >> 22) & 0xff, (testPattern >> 12) & 0xff, (testPattern >> 2) & 0xff);
	fprintf(stdout, "\t\t\tExpSign->red = 0x%x ; \t ExpSign->green = 0x%x ; \t ExpSign->blue = 0x%x ;\n" , ExpRed.channel, ExpGrn.channel, ExpBlu.channel) ;
	fprintf(stdout, "\t\tbreak;\n");
	fflush(stdout);
}

void
DFB1_ComputeCrc(__uint32_t *Patrn)
{
	GammaSize	ExpRed ;
	GammaSize	ExpBlu ;
	GammaSize	ExpGrn ;
	__uint32_t	clocks;

	In->Bit32 = 0 ;
	In->Bit31 = 0 ;
       	In->Bit0  = 0;

	Prev->Bit32 = ExpSign->Bit32 = 0 ;
	Prev->Bit31 = ExpSign->Bit31 = 0 ;
        Prev->red   = ExpSign->red = 0 ;
        Prev->green = ExpSign->green = 0 ;
        Prev->blue  = ExpSign->blue = 0 ;
       	Prev->Bit0  = ExpSign->Bit0 = 0;

	for(clocks= 0;  clocks < CLOCKS ; clocks++ ) {
#if 0
			Red.channel    = ((*(Patrn + clocks)) >> 16) & 0xff ;
			Green.channel  = ((*(Patrn + clocks)) >> 8) & 0xff ;
			Blue.channel   = (*(Patrn + clocks)) & 0xff ;
#endif
			Blue.channel    = ((*(Patrn + clocks)) >> 16) & 0xff ;
			Green.channel  = ((*(Patrn + clocks)) >> 8) & 0xff ;
			Red.channel   = (*(Patrn + clocks)) & 0xff ;

			Red.channel <<=2; 	Red.channel |= 0x3; 
			Green.channel <<=2; 	Green.channel |= 0x3; 
			Blue.channel <<=2; 	Blue.channel |= 0x3; 

        		In->red   = Red.channel ;
        		In->green = Green.channel ;
        		In->blue  = Blue.channel ;


		Prev->Bit32 = ExpSign->Bit31 ;
		Prev->Bit31 = (ExpSign->red >> 9) & 0x1; ;
        	Prev->red   = ((ExpSign->green >> 9) & 0x1) | ((ExpSign->red << 1) & 0x3fe) ;
        	Prev->green = ((ExpSign->blue >> 9) & 0x1) | ((ExpSign->green << 1) & 0x3fe)  ;
        	Prev->blue  = (ExpSign->Bit0  & 0x1) | ((ExpSign->blue << 1) & 0x3fe)  ;
       		Prev->Bit0  = ExpSign->Bit32;

       		ExpSign->Bit0  = In->Bit0 ^ Prev->Bit0;
        	ExpSign->blue  = In->blue ^ Prev->blue;
        	ExpSign->green = In->green ^ Prev->green;
        	ExpSign->red   = In->red ^ Prev->red;
		ExpSign->Bit31 = In->Bit31 ^ Prev->Bit31;
		ExpSign->Bit32 = In->Bit32 ^ Prev->Bit32; 

		In->Bit31 = 0 ;
		In->Bit32 = 0 ;
		In->Bit0 = (ExpSign->green >> 8) & 0x1;

	}

        ExpRed.channel  = ExpSign->red;
        ExpGrn.channel  = ExpSign->green;
	ExpBlu.channel  = ExpSign->blue;

	fprintf(stdout, "\t\t\t\tcase	0x%x	:\n" ,((*(Patrn)) | (*(Patrn+1))));
	fprintf(stdout, "\t\t\t\t\t/* Blue = 0x%x	Green = 0x%x	Red = 0x%x	*/ \n" , ((*Patrn) >> 16) & 0xff, ((*Patrn) >> 8) & 0xff, ((*Patrn)) & 0xff);
	fprintf(stdout, "\t\t\t\t\t/* Blue = 0x%x	Green = 0x%x	Red = 0x%x	*/ \n" , ((*(Patrn+1)) >> 16) & 0xff, ((*(Patrn+1)) >> 8) & 0xff, ((*(Patrn+1))) & 0xff);
	fprintf(stdout, "\t\t\t\t\tExpSign->red = 0x%x ; \t ExpSign->green = 0x%x ; \t ExpSign->blue = 0x%x ;\n" , ExpRed.channel, ExpGrn.channel, ExpBlu.channel) ;
	fprintf(stdout, "\t\t\t\tbreak;\n");
	fflush(stdout);
}

DFB1_ColorBar_Sig() 
{
	__uint32_t loop, index, count;
	__uint32_t PixRGB[CLOCKS];

	index=0; count = 0;
	for(index=0, loop=0; loop < 8192; loop++) {
		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0xFFFFFF;

		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0x000000;
			
		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0xFF0000;
			
		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0x00FF00;
			
		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0x0000FF;

		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0x55AA55;
			
		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0x5555AA;

		for(count=0; count < 20; count++, index++) 
			PixRGB[index] = 0xAA5555;
	} 

	DFB1_ComputeCrc(PixRGB);
}


DFB1_Pat1_Pat2(__uint32_t Patrn1, __uint32_t Patrn2) 
{
	__uint32_t index;
	__uint32_t PixRGB[CLOCKS];

	for(index=0; index < CLOCKS; index+=2) {
			PixRGB[index]   = Patrn1;
			PixRGB[index+1] = Patrn2;
	} 

	DFB1_ComputeCrc(PixRGB);
}

#define         Cmap0           0x1
#define         Cmap1           0x2
#define         CmapAll         0x3


DFB1_Linear(__uint32_t WhicCmap)
{
	__uint32_t index;
	__uint32_t PixRGB[CLOCKS];

	switch(WhicCmap) {
		case Cmap0:
			for(index=0; index < CLOCKS; index+=2) {
				PixRGB[index]   = index;
				PixRGB[index+1]   = 0x0;
			} 
		break;
		case Cmap1:
			for(index=0; index < CLOCKS; index+=2) {
				PixRGB[index]   = 0x0;
				PixRGB[index+1]   = index;
			} 
		break;
		case CmapAll:
			for(index=0; index < CLOCKS; index++) {
				PixRGB[index]   = index;
			} 
		break;

	}

	DFB1_ComputeCrc(PixRGB);
}

do_block(__uint32_t *data)
{
	__uint32_t x, y, numRows, numCols;

	numCols = 24; numRows = 16;
	for (y = 0; y < numRows; y++) {
                for (x = 0; x < numCols; x+=2) {
                   if (y % 2) { /* odd rows */
                        *(data + x + numCols*y) = 0xff7f7f7f;
                        *(data + x + 1 + numCols*y) = 0xff808080;
                   }
                   else { /* even rows */
                        *(data + x + numCols*y) = 0xff808080;
                        *(data + x + 1 + numCols*y) = 0xff7f7f7f;
                   }
                }
        }
}

do_notexpoly(__uint32_t *data)
{
	__uint32_t i;

	for (i = 0; i < 64; i++) {
                *(data + i) = 0xffffffff;
        }
}

PP_draw(__uint32_t rows, __uint32_t cols, __uint32_t *data)
{
	__uint32_t PixRGB[CLOCKS];
	__uint32_t x, y;

	/* data is abgr, so remove the top byte since it is alpha data */
	for (y = 0; y < rows; y++) {
		/* Real data */
		for (x = 0; x < cols; x++) {
			PixRGB[x + 1280*y] = (*data & 0xffffff);
			data++;
		}
		/* fill the rest of this line with 0 */
		for (x = cols; x < 1280; x++)
			PixRGB[x + 1280*y] = 0x0;
	}
	for (y = rows; y < 1024; y++) 
		for (x = 0; x < 1280; x++)
			PixRGB[x + 1280*y] = 0x0;

	DFB1_ComputeCrc(PixRGB);
}

main ()
{
	__uint32_t Patrn, shift;
	__uint32_t loop, index;
	__uint32_t block_array[384];
	__uint32_t notex_poly[128];

	In = &Input;
	Prev = &Previous;
	ExpSign = &Signature;

	fprintf(stdout, "GetComputeSig(__uint32_t WhichCmap, __uint32_t testPattern)\n");
	fprintf(stdout,"{\n");

	
	fprintf(stdout, "\tswitch (WhichCmap) {\n");
#if 0
	fprintf(stdout, "\t\tcase Cmap0:\n");
	fprintf(stdout, "\t\t\t/* =============    Program Cmap0 Only =============   */ \n");
	fprintf(stdout, "\t\t\tswitch (testPattern) {\n");

	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		for (index=0; index < 8; index++) {
			Patrn = (1<<index) ; 
			Patrn <<= shift;
			DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);
		}
	}

	for (index=0; index < 8; index++) {
		Patrn = 1<<index ; 
		Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);
	}

	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		Patrn = 0xff ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

		Patrn = 0x55 ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

		Patrn = 0xAA  ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

	}

	Patrn = 0x55 ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

	Patrn = 0xAA ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

	Patrn = 0xffffff;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

	Patrn = 0x00;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), 0);

	fprintf(stdout, "\t\t\t\tcase Linear0:\n");
	DFB1_Linear(Cmap0);

	fprintf(stdout, "\t\t\t};\n");

	fprintf(stdout, "\t\tbreak;\n");
	fprintf(stdout, "\t\tcase Cmap1:\n");
	fprintf(stdout, "\t\t\t/* =============    Program Cmap1 Only =============   */ \n");
	fprintf(stdout, "\t\t\tswitch (testPattern) {\n");

	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		for (index=0; index < 8; index++) {
			Patrn = (1<<index) ; 
			Patrn <<= shift;
			DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));
		}
	}

	for (index=0; index < 8; index++) {
		Patrn = 1<<index ; 
		Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
		DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));
	}

	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		Patrn = 0xff ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

		Patrn = 0x55 ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

		Patrn = 0xAA  ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

	}

	Patrn = 0x55 ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

	Patrn = 0xAA ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

	Patrn = 0xffffff;
	DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

	Patrn = 0x00;
	DFB1_Pat1_Pat2(0x0, (Patrn & 0xffffff));

	fprintf(stdout, "\t\t\t\tcase Linear1:\n");
	DFB1_Linear(Cmap1);

	fprintf(stdout, "\t\t\t};\n");
	fprintf(stdout, "\t\tbreak;\n");
#endif
	fprintf(stdout, "\t\tcase CmapAll:\n");
	fprintf(stdout, "\t\t\t/* =============    Program both Cmap0 & Cmap1  =============   */ \n");
	fprintf(stdout, "\t\t\tswitch (testPattern) {\n");

#if 0
	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		for (index=0; index < 8; index++) {
			Patrn = (1<<index) ; 
			Patrn <<= shift;
			DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));
		}
	}

	for (index=0; index < 8; index++) {
		Patrn = 1<<index ; 
		Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));
	}

#endif
#if 0
	for(loop = 0, shift = 0; loop < 3; loop++, shift +=8) {
		Patrn = 0xff ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

		Patrn = 0x55 ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

		Patrn = 0xAA  ;
		Patrn <<= shift;
		DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

	}

	Patrn = 0x55 ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

	Patrn = 0xAA ;
	Patrn = (Patrn << 16) | (Patrn << 8) | Patrn ;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

	Patrn = 0xffffff;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

	Patrn = 0x00;
	DFB1_Pat1_Pat2((Patrn & 0xffffff), (Patrn & 0xffffff));

	fprintf(stdout, "\t\t\t\tcase LinearAll:\n");
	DFB1_Linear(CmapAll);
#endif
#if 0
	fprintf(stdout, "\t\t\t\tcase DFB_COLORBARS:\n");
	DFB1_ColorBar_Sig();
#endif

#if 1
	fprintf(stdout, "\t\t\t/* ====== Color Triangle Test ====== */ \n");	
	fprintf(stdout, "\t\t\t\t/* ====== Red Triangle ====== */ \n");	
	PP_draw(16, 24, red_tri);
	fprintf(stdout, "\t\t\t\t/* ====== Green Triangle ====== */ \n");	
	PP_draw(16, 24, green_tri);
	fprintf(stdout, "\t\t\t\t/* ====== Blue Triangle ====== */ \n");	
	PP_draw(16, 24, blue_tri);
	fprintf(stdout, "\t\t\t\t/* ====== Alpha Triangle ====== */ \n");	
	PP_draw(16, 24, alpha_tri);

	fprintf(stdout, "\t\t\t/* ====== Z-Buffer Test ====== */ \n");	
	fprintf(stdout, "\t\t\t\t/* ====== s1t0 ====== */ \n");	
	PP_draw(16, 16, s1t0);
	fprintf(stdout, "\t\t\t\t/* ====== s1t1 ====== */ \n");	
	PP_draw(16, 16, s1t0);
	fprintf(stdout, "\t\t\t\t/* ====== s1t4 ====== */ \n");	
	PP_draw(16, 16, s1t4);
	fprintf(stdout, "\t\t\t\t/* ====== s1t5 ====== */ \n");	
	PP_draw(16, 16, s1t5);
	fprintf(stdout, "\t\t\t\t/* ====== s2t1 ====== */ \n");	
	PP_draw(16, 16, s2t1);
	fprintf(stdout, "\t\t\t\t/* ====== s2t2 ====== */ \n");	
	PP_draw(16, 16, s2t2);

	fprintf(stdout, "\t\t\t/* ====== Points Test ====== */ \n");	
	PP_draw(4, 6, points_array);

	fprintf(stdout, "\t\t\t/* ====== Line Test ====== */ \n");	
	PP_draw(17, 14, lines_array);

	fprintf(stdout, "\t\t\t/* ====== Stippled Triangle ====== */ \n");	
	PP_draw(16, 24, stip_tri);

	fprintf(stdout, "\t\t\t/* ====== Block Test ====== */ \n");	
	do_block(block_array);
	PP_draw(16, 24, block_array);

	fprintf(stdout, "\t\t\t/* ====== Character Test ====== */ \n");	
	PP_draw(16, 40, char_array);

	fprintf(stdout, "\t\t\t/* ====== Tex Poly Test ====== */ \n");	
	PP_draw(1, 64, tex_poly);

	fprintf(stdout, "\t\t\t/* ====== No Tex Poly Test ====== */ \n");	
	do_notexpoly(notex_poly);
	PP_draw(1, 64, notex_poly);

	fprintf(stdout, "\t\t\t/* ====== Logic Op Test ====== */ \n");	
	PP_draw(3, 16, logicop_array);

	fprintf(stdout, "\t\t\t/* ====== TE LOD Test - 32pixels ====== */ \n");	
	PP_draw(1, 32, telod_32);

	fprintf(stdout, "\t\t\t/* ====== TE LOD Test - 128 Pixels====== */ \n");	
	PP_draw(1, 128, telod_128);

	fprintf(stdout, "\t\t\t/* ====== Load/Resample Test ====== */ \n");	
	PP_draw(4, 64, ldrsmpl_array);

	fprintf(stdout, "\t\t\t/* ====== TE LOD Test - 4tram, 128 pixels ====== */ \n");	
	PP_draw(1, 64, telod_128_4tram);

#endif
	fprintf(stdout, "\t\t\t};\n");
	fprintf(stdout, "\t\tbreak;\n");
	fprintf(stdout, "\t}\n");
	fprintf(stdout,"}\n");

}

