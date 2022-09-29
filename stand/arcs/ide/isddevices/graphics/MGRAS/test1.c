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
 *  $Revision: 1.4 $
 */

#include "sys/types.h"
#include <math.h>

#ifndef _STANDALONE
#include "stdio.h"
#endif

void 	ComputeCrc(__uint32_t testPattern);
int 	ResetDacCrc() ;



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
			Red.channel    = ((*(Patrn + clocks)) >> 16) & 0xff ;
			Green.channel  = ((*(Patrn + clocks)) >> 8) & 0xff ;
			Blue.channel   = (*(Patrn + clocks)) & 0xff ;

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
	fprintf(stdout, "\t\t\t\t\t/* Red = 0x%x	Green = 0x%x	Blue = 0x%x	*/ \n" , ((*Patrn) >> 16) & 0xff, ((*Patrn) >> 8) & 0xff, ((*Patrn)) & 0xff);
	fprintf(stdout, "\t\t\t\t\t/* Red = 0x%x	Green = 0x%x	Blue = 0x%x	*/ \n" , ((*(Patrn+1)) >> 16) & 0xff, ((*(Patrn+1)) >> 8) & 0xff, ((*(Patrn+1))) & 0xff);
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

main ()
{
	__uint32_t Patrn, shift;
	__uint32_t loop, index;

	In = &Input;
	Prev = &Previous;
	ExpSign = &Signature;

	fprintf(stdout, "GetComputeSig(__uint32_t WhichCmap, __uint32_t testPattern)\n");
	fprintf(stdout,"{\n");

	
	fprintf(stdout, "\tswitch (WhichCmap) {\n");
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
	fprintf(stdout, "\t\tcase CmapAll:\n");
	fprintf(stdout, "\t\t\t/* =============    Program both Cmap0 & Cmap1  =============   */ \n");
	fprintf(stdout, "\t\t\tswitch (testPattern) {\n");

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

	fprintf(stdout, "\t\t\t\tcase DFB_COLORBARS:\n");
	DFB1_ColorBar_Sig();

	fprintf(stdout, "\t\t\t};\n");
	fprintf(stdout, "\t\tbreak;\n");
	fprintf(stdout, "\t}\n");
	fprintf(stdout,"}\n");

}

