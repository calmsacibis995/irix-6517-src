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
 *  $Revision: 1.43 $
 */
#include <math.h>
#include <libsc.h>
#include "uif.h"
#include "sys/mgrashw.h"
#include "ide_msg.h"
#include "mgras_diag.h"

/*
 * The following constant makes sure that the tests
 * use the general purpose scratch buffer rather than
 * a dedicated buffer for storing cmap values
 */
#define	CmapBuf	mgras_scratch_buf

/************************************************************************/
/*									*/
/*	Signature(32:0)	= Previous(32:0) Xor Present(32:0) ;		*/
/*									*/
/*	The real Equation is ::					        */
/*									*/
/*		Signature(32:0) = Not(Previous(32:0)) Xnor Present(32:0)	*/
/*									*/
/*	which is the same as 						*/
/*		Signature(32:0) = Previous(32:0) Xor Present(32:0)	*/
/*									*/
/*	Present(32) 	= 	0;					*/
/*	Present(31) 	= 	0;					*/
/*	Present(30:21) 	= 	Red(9:0);				*/
/*	Present(20:11) 	= 	Green(9:0);				*/
/*	Present(10:1) 	= 	Blue(9:0);				*/
/*	Present(0)	=	Signature(19);				*/
/*									*/
/*	Previous(32:1) 	= 	Signature(31:0);			*/
/*	Previous(0) 	= 	Signature(32);				*/
/*									*/
/************************************************************************/

input	Present,       *In ;
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
void
_PrintGammaTable(void)
{
	 ushort_t index;
	GammaSize RedRcv[256], GreenRcv[256], BlueRcv[256];

	bzero(RedRcv,   sizeof(RedRcv) );
        bzero(BlueRcv,  sizeof(BlueRcv) );
        bzero(GreenRcv, sizeof(GreenRcv) );

	mgras_ReadDac(0x0,  RedRcv, GreenRcv, BlueRcv, 256);

	for(index=0; index<256; index++) {
		msg_printf(6, "i = %x R %x G %x B %x\n" ,index, RedRcv[index].channel>>2, GreenRcv[index].channel>>2, BlueRcv[index].channel>>2);
	}
}

void
GammaTableLoad(ushort_t GammaRamp)
{
	GammaSize 	RGBval ;

	 ushort_t index ;

	mgras_dacSetAddr(mgbase, 0);
	switch (GammaRamp) {
		case LINEAR_RAMP:
			for(index=0; index<256; index++) {
				RGBval.channel  = ((index << 2) | 0x3 ) & 0x3ff ;
				RedGamma[index] = RGBval ;
				GrnGamma[index] = RGBval ;
				BluGamma[index] = RGBval ;
			}
			break;
		case RAMP_FLAT_0:
			for(index=0; index<256; index++) {
				RGBval.channel  = ((0) | 0x3) & 0x3ff ;
				RedGamma[index] = RGBval ;
				GrnGamma[index] = RGBval ;
				BluGamma[index] = RGBval ;
			}
			break;
		case RAMP_FLAT_5:
			for(index=0; index<256; index++) {
				RGBval.channel  = ( ushort_t) ((0x55 << 2) | 0x3) & 0x3ff;
				RedGamma[index] = RGBval ;
				GrnGamma[index] = RGBval ;
				BluGamma[index] = RGBval ;
			}
			break;
		case RAMP_FLAT_F:
			for(index=0; index<256; index++) {
				RGBval.channel  = 0xFFF  ;
				RedGamma[index] = RGBval ;
				GrnGamma[index] = RGBval ;
				BluGamma[index] = RGBval ;
			}
			break;
		case RAMP_FLAT_A:
			for(index=0; index<256; index++) {
				RGBval.channel  = ( ushort_t) ((0xAA << 2) | 0x3) & 0x3ff ;
				RedGamma[index] = RGBval ;
				GrnGamma[index] = RGBval ;
				BluGamma[index] = RGBval ;
			}
			break;
	}
	mgras_WriteDac(0,RedGamma, GrnGamma, BluGamma, 256, 0x3ff) ;
	_PrintGammaTable();
	msg_printf(DBG, "Gamma Table Loaded\n");
}

int
mg_gammaprog(int argc, char **argv)
{
         __uint32_t RampType = 0x0;

        if (argc == 1)
                RampType = 0x0;
        else
                atohex(argv[1], (int *)&RampType);

        if (RampType > RAMP_TYPES) {
                msg_printf(SUM, "mg_gammaprog RampType\n");
                msg_printf(SUM, "0x0 :LINEAR_RAMP\n");
                msg_printf(SUM, "0x1 : RAMP_FLAT_0\n");
                msg_printf(SUM, "0x2 : RAMP_FLAT_5\n");
                msg_printf(SUM, "0x3 : RAMP_FLAT_A\n");
                msg_printf(SUM, "0x4 : RAMP_FLAT_F\n");

                return -1;
        }

        GammaTableLoad(RampType);
        return(0);
}


void
mgras_CmapLd( __uint32_t WhichCmap,  __uint32_t StAddr, char *data,  __uint32_t length)
{
         __uint32_t i, addr;

        addr = MGRAS_8BITCMAP_BASE + StAddr;
        /* First Drain the BFIFO - DCB FIFO*/
        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
        for (i = 0; i < length; i++, addr++, data+=4) {

        /* Setting the address twice every time we check cmapFIFOWAIT    */
        /* is a s/w work around for the cmap problem. The address        */
        /* is lost ramdomly when the bus is switched from write to       */
        /* read mode or vice versa.                                      */


                /* Every 16 colors, check cmap FIFO.
                 * Need to give 2 dummy writes after the read.
                 */
                if ((i == 0) || ((i & 0x1F) == 0x1F)) {
			mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */
                        /* cmapFIFOWAIT calls BFIFOWAIT */
                        mgras_cmapFIFOWAIT_which(WhichCmap, mgbase);
			mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

                        mgras_cmapSetAddr_which(WhichCmap, mgbase, addr);
                        mgras_cmapSetAddr_which(WhichCmap, mgbase, addr);

                }

#if 0
                if ((i & 0xF) == 0xF)
                {
                        /* Wait before next stream of 4*16 writes */
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
#endif

                mgras_cmapSetRGB_which(mgbase, WhichCmap, *(data+1), *(data+2), *(data+3) );
                msg_printf(DBG, "CmapLd addr %x R %x G %x B %x\n", addr,*(data+1), *(data+2), *(data+3) );
        }
	mgras_cmapToggleCblank(mgbase, 0);	/* Enable cblank */
        /* cmapFIFOWAIT calls BFIFOWAIT */
        mgras_cmapFIFOWAIT_which(WhichCmap, mgbase);
}




void
LoadCmap(__uint32_t WhichCmap, __uint32_t Patrn)
{
         __uint32_t index ;
         __uint32_t value;

       	value = Patrn & 0xffffff;

        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++) {
        	CmapBuf[index] = value ;
        }

        mgras_CmapLd(WhichCmap, 0x0, (char*)CmapBuf, MGRAS_CMAP_NCOLMAPENT);
        msg_printf(DBG, "CmapLoad Patrn=0x%x\n", value);
}



void
LoadGammaTable( __uint32_t Patrn)
{
	GammaSize 	Rval ;
	GammaSize 	Gval ;
	GammaSize 	Bval ;
	 ushort_t index ;

	mgras_dacSetAddr(mgbase, 0);
	Bval.channel  = Patrn & 0x3ff ;
	Gval.channel  = (Patrn >> 10) & 0x3ff;
	Rval.channel  = (Patrn >> 20) & 0x3ff;
        msg_printf(DBG, "LoadGammaTable R = 0x%x G = %x B = %x \n", Rval.channel, Gval.channel, Bval.channel);

	for(index=0; index<256; index++) {
		RedGamma[index] = Rval ;
		GrnGamma[index] = Gval ;
		BluGamma[index] = Bval ;
	}
	mgras_WriteDac(0,RedGamma, GrnGamma, BluGamma, 256, 0x3ff) ;
	_PrintGammaTable();
	msg_printf(DBG, "Gamma Table Loaded Patrn %x\n" ,Patrn);
}
/*****************************************************************************/

__uint32_t
CrcCompare(void)
{
	GammaSize	ExpRed ;
	GammaSize	ExpBlu ;
	GammaSize	ExpGrn ;
 	__uint32_t 	errors ;

	errors = 0;
	ExpRed.channel  = ExpSign->red;
	ExpGrn.channel  = ExpSign->green;
	ExpBlu.channel  = ExpSign->blue;

	msg_printf(DBG, "  Red Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpRed.channel, RcvSign->red) ;
	if (RcvSign->red != ExpRed.channel) {
		msg_printf(DBG, "ERROR ==> Red Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpRed.channel, RcvSign->red) ;
		++errors ;
	}

	msg_printf(DBG, "Green Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpGrn.channel, RcvSign->green) ;
	if (RcvSign->green != ExpGrn.channel) {
		msg_printf(DBG, "ERROR ==> Green Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpGrn.channel, RcvSign->green) ;
		++errors ;
	}

	msg_printf(DBG, "Blue Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpBlu.channel, RcvSign->blue) ;
	if (RcvSign->blue != ExpBlu.channel) {
		msg_printf(DBG, "ERROR ==> Blue Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" ,ExpBlu.channel, RcvSign->blue) ;
		++errors ;
	}

	return (errors) ;
}

void
GetSignature(void)
{
	 ushort_t ControlReg4 ;
	 ushort_t RcvRed, RcvGrn, RcvBlu ;

	RcvRed = 0xbeef ; RcvBlu = 0xbeef ; RcvGrn = 0xbeef ;

	/* Reset signature reg and acquire signature ...*/
	WAIT_FOR_VBLANK() ;
	ControlReg4 = 0xA0;	/* Bits 5 & 7 = 1, Toggle Bit 6 to reset analyzer */
	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_CMD4_ADDR, ControlReg4) ;

	ControlReg4 = 0xE0;	
	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_CMD4_ADDR, ControlReg4) ;

	WAIT_FOR_NOT_VBLANK() ;
	

	WAIT_FOR_VBLANK() ;
	ControlReg4 = 0xC0;	/* stop sgi clk */
	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_CMD4_ADDR, ControlReg4) ;

	mgras_dacGetAddr16Cmd16(mgbase,MGRAS_DAC_RED_SIG, RcvRed) ;
	mgras_dacGetAddr16Cmd16(mgbase,MGRAS_DAC_GRN_SIG, RcvGrn) ;
	mgras_dacGetAddr16Cmd16(mgbase,MGRAS_DAC_BLU_SIG, RcvBlu) ;

        RcvSign->red   = RcvRed & 0x3ff ;
        RcvSign->green = RcvGrn & 0x3ff ;
        RcvSign->blue  = RcvBlu & 0x3ff ;
	msg_printf(DBG, "GetSignature CRC's Recv (1) R 0x%x G 0x%x B 0x%x\n",
		RcvRed, RcvGrn, RcvBlu);
}


void
ComputeCrc( __uint32_t testPattern)
{
         __uint32_t    rCmap, gCmap, bCmap;


	rCmap = ((CmapBuf[testPattern] & 0xff0000) >> 16);
	gCmap = ((CmapBuf[testPattern] & 0xff00) >> 8);
	bCmap = ((CmapBuf[testPattern] & 0xff));

	rCmap = (testPattern >> 16) & 0xff;
	gCmap = (testPattern >> 8) & 0xff;
	bCmap = testPattern & 0xff;

	msg_printf(DBG, "ComputeCrc testPattern= %x rCmap= %x gCmap= %x bCmap= %x\n", testPattern, rCmap, bCmap, gCmap);

	Red   = RedGamma[rCmap];
        Green = GrnGamma[gCmap];
        Blue  = BluGamma[bCmap];
	msg_printf(DBG, "rGamma= %x gGamma= %x bGamma= %x\n", Red.channel, Green.channel, Blue.channel);


        In->red   = Red.channel ;
        In->green = Green.channel ;
        In->blue  = Blue.channel ;
	In->Bit0 = (ExpSign->green >> 8) & 0x1;

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

	msg_printf(DBG, "Expected CRC's :- testPattern 0x%x Red 0x%x Green 0x%x Blue 0x%x\n" , testPattern, ExpSign->red, ExpSign->green, ExpSign->blue);
}

void
ResetDacCrc(void) 
{
	RcvSign->Bit32 = In->Bit32 = Prev->Bit32 = ExpSign->Bit32 = 0 ;
        RcvSign->Bit31 = In->Bit31 = Prev->Bit31 = ExpSign->Bit31 = 0 ;
        RcvSign->red   = In->red   = Prev->red   = ExpSign->red   = 0 ;
        RcvSign->green = In->green = Prev->green = ExpSign->green = 0 ;
        RcvSign->blue  = In->blue  = Prev->blue  = ExpSign->blue  = 0 ;
        RcvSign->Bit0  = In->Bit0  = Prev->Bit0  = ExpSign->Bit0  = 0 ;
}


void
LoadDiagScanReg( ushort_t RGBmode,  __uint32_t RGBpatrn) 
{
	__uint32_t  TopScan;
	__uint32_t  Patrn, Byte0, Byte1, Byte2;

	Byte0 =  RGBpatrn & 0xff;
	Byte1 = (RGBpatrn >> 8) & 0xff;
	Byte2 = (RGBpatrn >> 16) & 0xff;

	Patrn = (Byte0 << 16) | (Byte1 << 8) | Byte2  ;

	msg_printf(DBG, "LoadDiagScanReg :- RGBmode 0x%x RGBPatrn 0x%x\n" , RGBmode, RGBpatrn);


	/*********************************************************/
	/*							 */
	/* 	Select Pixel Type 	   		   	 */
	/* DIB_TOPSCAN[11] = 0 Select Color Index	   	 */
	/* DIB_TOPSCAN[11] = 1 Select RGB		   	 */
	/*							 */
	/* 	Select Graphics/Video	   		   	 */
	/* DIB_TOPSCAN[12] = 0 Selects Video 		   	 */
	/* DIB_TOPSCAN[12] = 1 Selects Graphics 	   	 */
	/*							 */
	/* 	Select Pixel Path 	   		   	 */
	/* DIB_TOPSCAN[13] = 1	Diag scan register to Pixel Path */
	/* DIB_TOPSCAN[13] = 0	Normal Pixel on Pixel Path 	 */
	/*							 */
	/*********************************************************/


#if 0
	TopScan = (1 << 13);
#endif

	if (RGBmode) 
		TopScan = 0x00003800;
	else
		TopScan = 0x00003000;

	mgras_xmapSetAddr(mgbase, 0x4);
	WAIT_FOR_VBLANK();
	mgras_xmapSetDIBdata(mgbase, TopScan);

	/***********************************************************/
	/* 	Load the Diag Scan Register with a Pattern 	   */
	/* Main_Buf_Select[23:0]   = Color[23:0] 		   */
	/* Main_Buf_Select[31:24]  = Alpha[7:0] 		   */
	/***********************************************************/
	mgras_xmapSetAddr(mgbase,MGRAS_XMAP_MAINBUF_ADDR);
	WAIT_FOR_VBLANK();
	mgras_xmapSetBufSelect(mgbase, Patrn);
}

#ifdef MFG_USED
int
mgras_DCB_PixelPath(int argc, char **argv)
{
	 __uint32_t error = 0;
	 __uint32_t RGBMode;
	 __uint32_t Cramp ;		/* Cmap Table Ramp Select */
	 __uint32_t Gramp ;		/* Gamma Table Ramp Select */
	 __uint32_t   DcbRgbPatrn ; 	/* DCBpatrn Select */
	 __uint32_t InputError ;
         __uint32_t    clocks;
	 __uint32_t re4_id, mgras_pp1_id;
	 __uint32_t CmapId, otherCmapId;
	 __uint32_t spFlag = 0x0, restrict = 0x0, specialCmapVal, index;

	In   = &Present ;
	Prev = &Previous ;
	ExpSign = &Signature ;
	RcvSign = &HWSignature ;
	
	Cramp = 0xdead;
	Gramp = 0xbaad;
	error = 0 ;
	InputError = 0 ;
	DcbRgbPatrn = -1 ;
	RGBMode = 1;


	CmapId = 2;
	re4_id = 0; /* Default */
        /* Select Second byte of the config reg :- for AutoInc */
        mgras_xmapSetAddr(mgbase, 0x1) ;        /* Do NOT REMOVE THIS */ 
        mgras_xmapSetConfig(mgbase, 0xff000000); /* Hack for Auto Inc */

	msg_printf(DBG, "DCB == PixelPath..\n");
	MGRAS_GFX_CHECK() ;
	argc--; argv++; /* Skip Test Name */
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'c' :
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&Cramp);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&Cramp);
                                }

				if (Cramp > RAMP_TYPES) 
					++InputError;

				break;

			case 'm' :
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&RGBMode);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&RGBMode);
                                }

				break;

			case 'g' :
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&Gramp);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&Gramp);
                                }

				if (Gramp > RAMP_TYPES)
					++InputError;

				break;

			case 'p' :
				/* DacRgbPatrn */
				msg_printf(VRB, "DacRgbPatrn\n");
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&DcbRgbPatrn);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&DcbRgbPatrn);
                                }
				break;

			case 's' :
				/* SpecialCmapVal */
				msg_printf(VRB, "SpecialCmapVal\n");
				spFlag = 0x1;  /* to recognize this case */
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&specialCmapVal);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&specialCmapVal);
                                }
				break;
			case 'r' :
				/* restricted switch */
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&restrict);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&restrict);
                                }
				break;

			case 'l' :
				/* Which Cmap ID */
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&CmapId);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&CmapId);
                                }
				if ((CmapId != Cmap0) && (CmapId !=  Cmap1)) {
				    ++InputError;
				}
				break;
			default :
				msg_printf(VRB, "== >>>>>> Default ??? ===== \n");
				++InputError;
				break;
		}
		argc--; argv++;
	}

	if (spFlag) {
#if 0
	   if (specialCmapVal > MGRAS_CMAP_NCOLMAPENT) {
		++InputError;
	   } else {
		DcbRgbPatrn = specialCmapVal;
	   }
#else
		DcbRgbPatrn = specialCmapVal;
#endif
	}

	if (DcbRgbPatrn == -1)
		++InputError;

	if (InputError) {
		msg_printf(SUM, "[-c RAMP_SELECT] [-g RAMP_SELECT] [-m RGBMode] -pDcbRgbPattern\
		-r -l [Which Cmap] -s specialCmapVal\n");
		msg_printf(SUM, "-p RGBPattern\n");
		msg_printf(SUM, "-g RAMP_TYPE\n");
		msg_printf(SUM, "-c RAMP_TYPE\n");
		msg_printf(SUM, "   0 :Linear Ramp, 1:Entire LUT's = 0x0 \n");
		msg_printf(SUM, "   2: Entire LUT's = 0x5\n");
		msg_printf(SUM, "-m RGBMode : 0 -> CI Mode, 1 -> RGB Mode \n");
		msg_printf(SUM, "-r Restrict ; used with -l & -s switches \n");
		msg_printf(SUM, "-l Which Cmap [0|1] \n");
		msg_printf(SUM, "In the restricted case the following RGB patterns are supported\n\
		0xff0000, 0x550000, 0xaa0000, 0x555555, 0xaaaaaa, 0xffffff\n");
		msg_printf(SUM, "-s Special CMAP Value (< 0x2000) (Linear Gamma Always)\n");
		return(InputError) ;
	}

	if (restrict) {
	     Gramp = 0;
	     RGBMode = 1;
	     if ( (specialCmapVal != 0xff0000) && (specialCmapVal != 0x550000) &&
	          (specialCmapVal != 0xaa0000) && (specialCmapVal != 0x555555) &&
	          (specialCmapVal != 0xaaaaaa) && (specialCmapVal != 0xffffff) ) {
		msg_printf(SUM, "Incorrect Pattern used in the restricted mode\n\
		Using 0x555555 for the test\n");
		specialCmapVal = DcbRgbPatrn = 0x555555;
	     }
	     if (CmapId == Cmap0) {
		otherCmapId = Cmap1;
	     } else {
		otherCmapId = Cmap0;
	     }
	} else {
	     mgras_pp1_id = MGRAS_XMAP_SELECT_PP1(MGRAS_XMAP_SELECT_BROADCAST);
       	     mgras_xmapSetPP1Select(mgbase, mgras_pp1_id); 
	     msg_printf(DBG, " mgras_xmapSetPP1Select(mgbase, 0x%x) \n" ,mgras_pp1_id); 
	}

	ResetDacCrc();
	/*
	 * XXX: Special case of the test where the GAMMA table is loaded
	 *	with LINEAR_RAMP and CMAP is loaded with the user given 
	 *	flat value...
	 */
	if (spFlag) {
		GammaTableLoad(LINEAR_RAMP);
		msg_printf(DBG, "Entering Special CmapLoad function \
			SpecialCmapVal = 0x%x\n", specialCmapVal);
		for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++) {
			CmapBuf[index] = specialCmapVal;
		}
	     	mgras_pp1_id = MGRAS_XMAP_SELECT_PP1((re4_id << 1) | CmapId);
       		mgras_xmapSetPP1Select(mgbase, mgras_pp1_id); 
		mgras_CmapLd(CmapId, 0x0, (char*)CmapBuf, MGRAS_CMAP_NCOLMAPENT);
		for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++) {
			CmapBuf[index] = 0x0;
		}
	     	mgras_pp1_id = MGRAS_XMAP_SELECT_PP1((re4_id << 1) | otherCmapId);
       		mgras_xmapSetPP1Select(mgbase, mgras_pp1_id); 
		mgras_CmapLd(otherCmapId, 0x0, (char*)CmapBuf, MGRAS_CMAP_NCOLMAPENT);
		msg_printf(DBG, "CmapLoad - Loaded......\n");
	} else {
		GammaTableLoad(Gramp);
		CmapLoad(Cramp);
	}

	LoadDiagScanReg(RGBMode, DcbRgbPatrn);
	ResetDacCrc();
	GetSignature();
	if (restrict) {
		GetComputeSig(CmapId, specialCmapVal);
	} else {
		msg_printf(VRB, "Please be patient... Computing Signature takes a little time...\n");
		for(clocks= 0;  clocks < CLOCKS ; clocks++ ) {
			ComputeCrc(DcbRgbPatrn);
		}
	}
	msg_printf(DBG, "Expected CRC's :- testPattern 0x%x Red 0x%x Green 0x%x Blue 0x%x\n" , DcbRgbPatrn, ExpSign->red, ExpSign->green, ExpSign->blue);
	error = CrcCompare();
	if (error) {
		msg_printf(ERR, "Error - verifying the CRC Mechanism\n");
	} else {
		msg_printf(SUM, "CRC Test Passed\n");
	}

	msg_printf(DBG, "Leaving DCB_PixelPath\n");
	return(error);
}
#endif

void
GetComputeSig(__uint32_t WhichCmap, __uint32_t testPattern)
{
	switch (WhichCmap) {
		case Cmap0:
			/* =============    Program Cmap0 Only =============   */ 
			switch (testPattern) {
				case	0x1	:
					/* Red = 0x0	Green = 0x0	Blue = 0x1	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3c0 ; 	 ExpSign->green = 0xa8 ; 	 ExpSign->blue = 0x10a ;
				break;
				case	0x2	:
					/* Red = 0x0	Green = 0x0	Blue = 0x2	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2b7 ; 	 ExpSign->green = 0x255 ; 	 ExpSign->blue = 0x384 ;
				break;
				case	0x4	:
					/* Red = 0x0	Green = 0x0	Blue = 0x4	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x58 ; 	 ExpSign->green = 0x3ae ; 	 ExpSign->blue = 0x299 ;
				break;
				case	0x8	:
					/* Red = 0x0	Green = 0x0	Blue = 0x8	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x186 ; 	 ExpSign->green = 0x58 ; 	 ExpSign->blue = 0xa2 ;
				break;
				case	0x10	:
					/* Red = 0x0	Green = 0x0	Blue = 0x10	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x23b ; 	 ExpSign->green = 0x3b5 ; 	 ExpSign->blue = 0xd5 ;
				break;
				case	0x20	:
					/* Red = 0x0	Green = 0x0	Blue = 0x20	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x140 ; 	 ExpSign->green = 0x6f ; 	 ExpSign->blue = 0x3a ;
				break;
				case	0x40	:
					/* Red = 0x0	Green = 0x0	Blue = 0x40	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3b7 ; 	 ExpSign->green = 0x3db ; 	 ExpSign->blue = 0x1e4 ;
				break;
				case	0x80	:
					/* Red = 0x0	Green = 0x0	Blue = 0x80	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x258 ; 	 ExpSign->green = 0xb3 ; 	 ExpSign->blue = 0x259 ;
				break;
				case	0x100	:
					/* Red = 0x0	Green = 0x1	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x146 ; 	 ExpSign->green = 0x86 ; 	 ExpSign->blue = 0x23e ;
				break;
				case	0x200	:
					/* Red = 0x0	Green = 0x2	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3bb ; 	 ExpSign->green = 0x208 ; 	 ExpSign->blue = 0x1ed ;
				break;
				case	0x400	:
					/* Red = 0x0	Green = 0x4	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x240 ; 	 ExpSign->green = 0x315 ; 	 ExpSign->blue = 0x24b ;
				break;
				case	0x800	:
					/* Red = 0x0	Green = 0x8	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1b6 ; 	 ExpSign->green = 0x12e ; 	 ExpSign->blue = 0x106 ;
				break;
				case	0x1000	:
					/* Red = 0x0	Green = 0x10	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x25b ; 	 ExpSign->green = 0x159 ; 	 ExpSign->blue = 0x39c ;
				break;
				case	0x2000	:
					/* Red = 0x0	Green = 0x20	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x181 ; 	 ExpSign->green = 0x1b6 ; 	 ExpSign->blue = 0x2a9 ;
				break;
				case	0x4000	:
					/* Red = 0x0	Green = 0x40	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x235 ; 	 ExpSign->green = 0x68 ; 	 ExpSign->blue = 0xc3 ;
				break;
				case	0x8000	:
					/* Red = 0x0	Green = 0x80	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x247 ; 	 ExpSign->green = 0x29f ; 	 ExpSign->blue = 0x259 ;
				break;
				case	0x10000	:
					/* Red = 0x1	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1ba ; 	 ExpSign->green = 0x1e6 ; 	 ExpSign->blue = 0x23a ;
				break;
				case	0x20000	:
					/* Red = 0x2	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x243 ; 	 ExpSign->green = 0xc8 ; 	 ExpSign->blue = 0x1e5 ;
				break;
				case	0x40000	:
					/* Red = 0x4	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1b1 ; 	 ExpSign->green = 0x295 ; 	 ExpSign->blue = 0x25a ;
				break;
				case	0x80000	:
					/* Red = 0x8	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x254 ; 	 ExpSign->green = 0x22e ; 	 ExpSign->blue = 0x124 ;
				break;
				case	0x100000	:
					/* Red = 0x10	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x19e ; 	 ExpSign->green = 0x359 ; 	 ExpSign->blue = 0x3d9 ;
				break;
				case	0x200000	:
					/* Red = 0x20	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x20a ; 	 ExpSign->green = 0x1b6 ; 	 ExpSign->blue = 0x222 ;
				break;
				case	0x400000	:
					/* Red = 0x40	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x123 ; 	 ExpSign->green = 0x68 ; 	 ExpSign->blue = 0x1d4 ;
				break;
				case	0x800000	:
					/* Red = 0x80	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x371 ; 	 ExpSign->green = 0x3d5 ; 	 ExpSign->blue = 0x239 ;
				break;
				case	0x10101	:
					/* Red = 0x1	Green = 0x1	Blue = 0x1	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x33c ; 	 ExpSign->green = 0x1c8 ; 	 ExpSign->blue = 0x10e ;
				break;
				case	0x20202	:
					/* Red = 0x2	Green = 0x2	Blue = 0x2	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x34f ; 	 ExpSign->green = 0x95 ; 	 ExpSign->blue = 0x38c ;
				break;
				case	0x40404	:
					/* Red = 0x4	Green = 0x4	Blue = 0x4	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3a9 ; 	 ExpSign->green = 0x22e ; 	 ExpSign->blue = 0x288 ;
				break;
				case	0x80808	:
					/* Red = 0x8	Green = 0x8	Blue = 0x8	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x264 ; 	 ExpSign->green = 0x358 ; 	 ExpSign->blue = 0x80 ;
				break;
				case	0x101010	:
					/* Red = 0x10	Green = 0x10	Blue = 0x10	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1fe ; 	 ExpSign->green = 0x1b5 ; 	 ExpSign->blue = 0x90 ;
				break;
				case	0x202020	:
					/* Red = 0x20	Green = 0x20	Blue = 0x20	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2cb ; 	 ExpSign->green = 0x6f ; 	 ExpSign->blue = 0xb1 ;
				break;
				case	0x404040	:
					/* Red = 0x40	Green = 0x40	Blue = 0x40	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xa1 ; 	 ExpSign->green = 0x3db ; 	 ExpSign->blue = 0xf3 ;
				break;
				case	0x808080	:
					/* Red = 0x80	Green = 0x80	Blue = 0x80	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x36e ; 	 ExpSign->green = 0x1f9 ; 	 ExpSign->blue = 0x239 ;
				break;
				case	0xff	:
					/* Red = 0x0	Green = 0x0	Blue = 0xff	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x12f ; 	 ExpSign->green = 0x345 ; 	 ExpSign->blue = 0x97 ;
				break;
				case	0x55	:
					/* Red = 0x0	Green = 0x0	Blue = 0x55	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x106 ; 	 ExpSign->green = 0x194 ; 	 ExpSign->blue = 0x1d2 ;
				break;
				case	0xaa	:
					/* Red = 0x0	Green = 0x0	Blue = 0xaa	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x33b ; 	 ExpSign->green = 0x2d ; 	 ExpSign->blue = 0x235 ;
				break;
				case	0xff00	:
					/* Red = 0x0	Green = 0xff	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1b1 ; 	 ExpSign->green = 0x51 ; 	 ExpSign->blue = 0x41 ;
				break;
				case	0x5500	:
					/* Red = 0x0	Green = 0x55	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x7a ; 	 ExpSign->green = 0x5e ; 	 ExpSign->blue = 0x5a ;
				break;
				case	0xaa00	:
					/* Red = 0x0	Green = 0xaa	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2d9 ; 	 ExpSign->green = 0x2f3 ; 	 ExpSign->blue = 0x36b ;
				break;
				case	0xff0000	:
					/* Red = 0xff	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2c8 ; 	 ExpSign->green = 0x23b ; 	 ExpSign->blue = 0x1c7 ;
				break;
				case	0x550000	:
					/* Red = 0x55	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3a4 ; 	 ExpSign->green = 0x2be ; 	 ExpSign->blue = 0x11d ;
				break;
				case	0xaa0000	:
					/* Red = 0xaa	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x27e ; 	 ExpSign->green = 0x279 ; 	 ExpSign->blue = 0x3aa ;
				break;
				case	0x555555	:
					/* Red = 0x55	Green = 0x55	Blue = 0x55	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2d8 ; 	 ExpSign->green = 0x374 ; 	 ExpSign->blue = 0x95 ;
				break;
				case	0xaaaaaa	:
					/* Red = 0xaa	Green = 0xaa	Blue = 0xaa	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x39c ; 	 ExpSign->green = 0xa7 ; 	 ExpSign->blue = 0x2f4 ;
				break;
				case	0xffffff	:
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x256 ; 	 ExpSign->green = 0x12f ; 	 ExpSign->blue = 0x111 ;
				break;
				case	0x0	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x312 ; 	 ExpSign->green = 0x2fc ; 	 ExpSign->blue = 0x370 ;
				break;
				case Linear0:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x11d ; 	 ExpSign->green = 0x2dc ; 	 ExpSign->blue = 0x227 ;
				break;
			};
		break;
		case Cmap1:
			/* =============    Program Cmap1 Only =============   */ 
			switch (testPattern) {
				case	0x1	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x1	*/ 
					ExpSign->red = 0x37b ; 	 ExpSign->green = 0x3d6 ; 	 ExpSign->blue = 0x24d ;
				break;
				case	0x2	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x2	*/ 
					ExpSign->red = 0x3c0 ; 	 ExpSign->green = 0xa8 ; 	 ExpSign->blue = 0x10a ;
				break;
				case	0x4	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x4	*/ 
					ExpSign->red = 0x2b7 ; 	 ExpSign->green = 0x255 ; 	 ExpSign->blue = 0x384 ;
				break;
				case	0x8	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x8	*/ 
					ExpSign->red = 0x58 ; 	 ExpSign->green = 0x3ae ; 	 ExpSign->blue = 0x299 ;
				break;
				case	0x10	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x10	*/ 
					ExpSign->red = 0x186 ; 	 ExpSign->green = 0x58 ; 	 ExpSign->blue = 0xa2 ;
				break;
				case	0x20	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x20	*/ 
					ExpSign->red = 0x23b ; 	 ExpSign->green = 0x3b5 ; 	 ExpSign->blue = 0xd5 ;
				break;
				case	0x40	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x40	*/ 
					ExpSign->red = 0x140 ; 	 ExpSign->green = 0x6f ; 	 ExpSign->blue = 0x3a ;
				break;
				case	0x80	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x80	*/ 
					ExpSign->red = 0x3b7 ; 	 ExpSign->green = 0x3db ; 	 ExpSign->blue = 0x1e4 ;
				break;
				case	0x100	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x1	Blue = 0x0	*/ 
					ExpSign->red = 0x238 ; 	 ExpSign->green = 0x3c1 ; 	 ExpSign->blue = 0x3d7 ;
				break;
				case	0x200	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x2	Blue = 0x0	*/ 
					ExpSign->red = 0x146 ; 	 ExpSign->green = 0x86 ; 	 ExpSign->blue = 0x23e ;
				break;
				case	0x400	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x4	Blue = 0x0	*/ 
					ExpSign->red = 0x3bb ; 	 ExpSign->green = 0x208 ; 	 ExpSign->blue = 0x1ed ;
				break;
				case	0x800	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x8	Blue = 0x0	*/ 
					ExpSign->red = 0x240 ; 	 ExpSign->green = 0x315 ; 	 ExpSign->blue = 0x24b ;
				break;
				case	0x1000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x10	Blue = 0x0	*/ 
					ExpSign->red = 0x1b6 ; 	 ExpSign->green = 0x12e ; 	 ExpSign->blue = 0x106 ;
				break;
				case	0x2000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x20	Blue = 0x0	*/ 
					ExpSign->red = 0x25b ; 	 ExpSign->green = 0x159 ; 	 ExpSign->blue = 0x39c ;
				break;
				case	0x4000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x40	Blue = 0x0	*/ 
					ExpSign->red = 0x181 ; 	 ExpSign->green = 0x1b6 ; 	 ExpSign->blue = 0x2a9 ;
				break;
				case	0x8000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x80	Blue = 0x0	*/ 
					ExpSign->red = 0x3b8 ; 	 ExpSign->green = 0xcd ; 	 ExpSign->blue = 0x1e4 ;
				break;
				case	0x10000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x1	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x246 ; 	 ExpSign->green = 0x371 ; 	 ExpSign->blue = 0x3d5 ;
				break;
				case	0x20000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x2	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1ba ; 	 ExpSign->green = 0x1e6 ; 	 ExpSign->blue = 0x23a ;
				break;
				case	0x40000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x4	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x243 ; 	 ExpSign->green = 0xc8 ; 	 ExpSign->blue = 0x1e5 ;
				break;
				case	0x80000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x8	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x1b1 ; 	 ExpSign->green = 0x295 ; 	 ExpSign->blue = 0x25a ;
				break;
				case	0x100000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x10	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x254 ; 	 ExpSign->green = 0x22e ; 	 ExpSign->blue = 0x124 ;
				break;
				case	0x200000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x20	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x19e ; 	 ExpSign->green = 0x359 ; 	 ExpSign->blue = 0x3d9 ;
				break;
				case	0x400000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x40	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x20a ; 	 ExpSign->green = 0x1b6 ; 	 ExpSign->blue = 0x222 ;
				break;
				case	0x800000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x80	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x123 ; 	 ExpSign->green = 0x68 ; 	 ExpSign->blue = 0x1d4 ;
				break;
				case	0x10101	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x1	Green = 0x1	Blue = 0x1	*/ 
					ExpSign->red = 0x305 ; 	 ExpSign->green = 0x366 ; 	 ExpSign->blue = 0x24f ;
				break;
				case	0x20202	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x2	Green = 0x2	Blue = 0x2	*/ 
					ExpSign->red = 0x33c ; 	 ExpSign->green = 0x1c8 ; 	 ExpSign->blue = 0x10e ;
				break;
				case	0x40404	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x4	Green = 0x4	Blue = 0x4	*/ 
					ExpSign->red = 0x34f ; 	 ExpSign->green = 0x95 ; 	 ExpSign->blue = 0x38c ;
				break;
				case	0x80808	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x8	Green = 0x8	Blue = 0x8	*/ 
					ExpSign->red = 0x3a9 ; 	 ExpSign->green = 0x22e ; 	 ExpSign->blue = 0x288 ;
				break;
				case	0x101010	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x10	Green = 0x10	Blue = 0x10	*/ 
					ExpSign->red = 0x264 ; 	 ExpSign->green = 0x358 ; 	 ExpSign->blue = 0x80 ;
				break;
				case	0x202020	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x20	Green = 0x20	Blue = 0x20	*/ 
					ExpSign->red = 0x1fe ; 	 ExpSign->green = 0x1b5 ; 	 ExpSign->blue = 0x90 ;
				break;
				case	0x404040	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x40	Green = 0x40	Blue = 0x40	*/ 
					ExpSign->red = 0x2cb ; 	 ExpSign->green = 0x6f ; 	 ExpSign->blue = 0xb1 ;
				break;
				case	0x808080	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x80	Green = 0x80	Blue = 0x80	*/ 
					ExpSign->red = 0x12c ; 	 ExpSign->green = 0x37e ; 	 ExpSign->blue = 0x1d4 ;
				break;
				case	0xff	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0xff	*/ 
					ExpSign->red = 0xc ; 	 ExpSign->green = 0x20 ; 	 ExpSign->blue = 0x83 ;
				break;
				case	0x55	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x55	*/ 
					ExpSign->red = 0x218 ; 	 ExpSign->green = 0x348 ; 	 ExpSign->blue = 0x221 ;
				break;
				case	0xaa	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0xaa	*/ 
					ExpSign->red = 0x106 ; 	 ExpSign->green = 0x194 ; 	 ExpSign->blue = 0x1d2 ;
				break;
				case	0xff00	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0xff	Blue = 0x0	*/ 
					ExpSign->red = 0x43 ; 	 ExpSign->green = 0x1aa ; 	 ExpSign->blue = 0xe8 ;
				break;
				case	0x5500	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x55	Blue = 0x0	*/ 
					ExpSign->red = 0x2a6 ; 	 ExpSign->green = 0x3ad ; 	 ExpSign->blue = 0x2e5 ;
				break;
				case	0xaa00	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0xaa	Blue = 0x0	*/ 
					ExpSign->red = 0x1f7 ; 	 ExpSign->green = 0xfb ; 	 ExpSign->blue = 0x17d ;
				break;
				case	0xff0000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xff	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3ff ; 	 ExpSign->green = 0x29f ; 	 ExpSign->blue = 0x2b ;
				break;
				case	0x550000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x55	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x349 ; 	 ExpSign->green = 0x2dd ; 	 ExpSign->blue = 0x246 ;
				break;
				case	0xaa0000	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xaa	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3a4 ; 	 ExpSign->green = 0x2be ; 	 ExpSign->blue = 0x11d ;
				break;
				case	0x555555	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x55	Green = 0x55	Blue = 0x55	*/ 
					ExpSign->red = 0x3f7 ; 	 ExpSign->green = 0x238 ; 	 ExpSign->blue = 0x282 ;
				break;
				case	0xaaaaaa	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xaa	Green = 0xaa	Blue = 0xaa	*/ 
					ExpSign->red = 0x355 ; 	 ExpSign->green = 0x3d1 ; 	 ExpSign->blue = 0x1b2 ;
				break;
				case	0xffffff	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					ExpSign->red = 0x3b0 ; 	 ExpSign->green = 0x315 ; 	 ExpSign->blue = 0x40 ;
				break;
				case	0x0	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x312 ; 	 ExpSign->green = 0x2fc ; 	 ExpSign->blue = 0x370 ;
				break;
				case Linear1:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x215 ; 	 ExpSign->green = 0xec ; 	 ExpSign->blue = 0x3db ;
				break;
			};
		break;
		case CmapAll:
			/* =============    Program both Cmap0 & Cmap1  =============   */ 
			switch (testPattern) {
				case	0x1	:
					/* Red = 0x0	Green = 0x0	Blue = 0x1	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x1	*/ 
					ExpSign->red = 0x3a9 ; 	 ExpSign->green = 0x182 ; 	 ExpSign->blue = 0x37 ;
				break;
				case	0x2	:
					/* Red = 0x0	Green = 0x0	Blue = 0x2	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x2	*/ 
					ExpSign->red = 0x265 ; 	 ExpSign->green = 0x1 ; 	 ExpSign->blue = 0x1fe ;
				break;
				case	0x4	:
					/* Red = 0x0	Green = 0x0	Blue = 0x4	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x4	*/ 
					ExpSign->red = 0x1fd ; 	 ExpSign->green = 0x307 ; 	 ExpSign->blue = 0x26d ;
				break;
				case	0x8	:
					/* Red = 0x0	Green = 0x0	Blue = 0x8	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x8	*/ 
					ExpSign->red = 0x2cc ; 	 ExpSign->green = 0x10a ; 	 ExpSign->blue = 0x14b ;
				break;
				case	0x10	:
					/* Red = 0x0	Green = 0x0	Blue = 0x10	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x10	*/ 
					ExpSign->red = 0xaf ; 	 ExpSign->green = 0x111 ; 	 ExpSign->blue = 0x307 ;
				break;
				case	0x20	:
					/* Red = 0x0	Green = 0x0	Blue = 0x20	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x20	*/ 
					ExpSign->red = 0x69 ; 	 ExpSign->green = 0x126 ; 	 ExpSign->blue = 0x39f ;
				break;
				case	0x40	:
					/* Red = 0x0	Green = 0x0	Blue = 0x40	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x40	*/ 
					ExpSign->red = 0x1e5 ; 	 ExpSign->green = 0x148 ; 	 ExpSign->blue = 0x2ae ;
				break;
				case	0x80	:
					/* Red = 0x0	Green = 0x0	Blue = 0x80	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x80	*/ 
					ExpSign->red = 0x2fd ; 	 ExpSign->green = 0x194 ; 	 ExpSign->blue = 0xcd ;
				break;
				case	0x100	:
					/* Red = 0x0	Green = 0x1	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x1	Blue = 0x0	*/ 
					ExpSign->red = 0x6c ; 	 ExpSign->green = 0x1bb ; 	 ExpSign->blue = 0x299 ;
				break;
				case	0x200	:
					/* Red = 0x0	Green = 0x2	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x2	Blue = 0x0	*/ 
					ExpSign->red = 0x1ef ; 	 ExpSign->green = 0x72 ; 	 ExpSign->blue = 0xa3 ;
				break;
				case	0x400	:
					/* Red = 0x0	Green = 0x4	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x4	Blue = 0x0	*/ 
					ExpSign->red = 0x2e9 ; 	 ExpSign->green = 0x3e1 ; 	 ExpSign->blue = 0xd6 ;
				break;
				case	0x800	:
					/* Red = 0x0	Green = 0x8	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x8	Blue = 0x0	*/ 
					ExpSign->red = 0xe4 ; 	 ExpSign->green = 0xc7 ; 	 ExpSign->blue = 0x3d ;
				break;
				case	0x1000	:
					/* Red = 0x0	Green = 0x10	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x10	Blue = 0x0	*/ 
					ExpSign->red = 0xff ; 	 ExpSign->green = 0x28b ; 	 ExpSign->blue = 0x1ea ;
				break;
				case	0x2000	:
					/* Red = 0x0	Green = 0x20	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x20	Blue = 0x0	*/ 
					ExpSign->red = 0xc8 ; 	 ExpSign->green = 0x213 ; 	 ExpSign->blue = 0x245 ;
				break;
				case	0x4000	:
					/* Red = 0x0	Green = 0x40	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x40	Blue = 0x0	*/ 
					ExpSign->red = 0xa6 ; 	 ExpSign->green = 0x322 ; 	 ExpSign->blue = 0x11a ;
				break;
				case	0x8000	:
					/* Red = 0x0	Green = 0x80	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x80	Blue = 0x0	*/ 
					ExpSign->red = 0x2ed ; 	 ExpSign->green = 0xae ; 	 ExpSign->blue = 0xcd ;
				break;
				case	0x10000	:
					/* Red = 0x1	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x1	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xee ; 	 ExpSign->green = 0x6b ; 	 ExpSign->blue = 0x29f ;
				break;
				case	0x20000	:
					/* Red = 0x2	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x2	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xeb ; 	 ExpSign->green = 0x3d2 ; 	 ExpSign->blue = 0xaf ;
				break;
				case	0x40000	:
					/* Red = 0x4	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x4	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xe0 ; 	 ExpSign->green = 0xa1 ; 	 ExpSign->blue = 0xcf ;
				break;
				case	0x80000	:
					/* Red = 0x8	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x8	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xf7 ; 	 ExpSign->green = 0x247 ; 	 ExpSign->blue = 0xe ;
				break;
				case	0x100000	:
					/* Red = 0x10	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x10	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0xd8 ; 	 ExpSign->green = 0x38b ; 	 ExpSign->blue = 0x18d ;
				break;
				case	0x200000	:
					/* Red = 0x20	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x20	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x86 ; 	 ExpSign->green = 0x13 ; 	 ExpSign->blue = 0x28b ;
				break;
				case	0x400000	:
					/* Red = 0x40	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x40	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3b ; 	 ExpSign->green = 0x322 ; 	 ExpSign->blue = 0x86 ;
				break;
				case	0x800000	:
					/* Red = 0x80	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x80	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x140 ; 	 ExpSign->green = 0x141 ; 	 ExpSign->blue = 0x9d ;
				break;
				case	0x10101	:
					/* Red = 0x1	Green = 0x1	Blue = 0x1	*/ 
					/* Red = 0x1	Green = 0x1	Blue = 0x1	*/ 
					ExpSign->red = 0x32b ; 	 ExpSign->green = 0x52 ; 	 ExpSign->blue = 0x31 ;
				break;
				case	0x20202	:
					/* Red = 0x2	Green = 0x2	Blue = 0x2	*/ 
					/* Red = 0x2	Green = 0x2	Blue = 0x2	*/ 
					ExpSign->red = 0x361 ; 	 ExpSign->green = 0x3a1 ; 	 ExpSign->blue = 0x1f2 ;
				break;
				case	0x40404	:
					/* Red = 0x4	Green = 0x4	Blue = 0x4	*/ 
					/* Red = 0x4	Green = 0x4	Blue = 0x4	*/ 
					ExpSign->red = 0x3f4 ; 	 ExpSign->green = 0x47 ; 	 ExpSign->blue = 0x274 ;
				break;
				case	0x80808	:
					/* Red = 0x8	Green = 0x8	Blue = 0x8	*/ 
					/* Red = 0x8	Green = 0x8	Blue = 0x8	*/ 
					ExpSign->red = 0x2df ; 	 ExpSign->green = 0x38a ; 	 ExpSign->blue = 0x178 ;
				break;
				case	0x101010	:
					/* Red = 0x10	Green = 0x10	Blue = 0x10	*/ 
					/* Red = 0x10	Green = 0x10	Blue = 0x10	*/ 
					ExpSign->red = 0x88 ; 	 ExpSign->green = 0x11 ; 	 ExpSign->blue = 0x360 ;
				break;
				case	0x202020	:
					/* Red = 0x20	Green = 0x20	Blue = 0x20	*/ 
					/* Red = 0x20	Green = 0x20	Blue = 0x20	*/ 
					ExpSign->red = 0x27 ; 	 ExpSign->green = 0x326 ; 	 ExpSign->blue = 0x351 ;
				break;
				case	0x404040	:
					/* Red = 0x40	Green = 0x40	Blue = 0x40	*/ 
					/* Red = 0x40	Green = 0x40	Blue = 0x40	*/ 
					ExpSign->red = 0x178 ; 	 ExpSign->green = 0x148 ; 	 ExpSign->blue = 0x332 ;
				break;
				case	0x808080	:
					/* Red = 0x80	Green = 0x80	Blue = 0x80	*/ 
					/* Red = 0x80	Green = 0x80	Blue = 0x80	*/ 
					ExpSign->red = 0x150 ; 	 ExpSign->green = 0x7b ; 	 ExpSign->blue = 0x9d ;
				break;
				case	0xff	:
					/* Red = 0x0	Green = 0x0	Blue = 0xff	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0xff	*/ 
					ExpSign->red = 0x231 ; 	 ExpSign->green = 0x199 ; 	 ExpSign->blue = 0x364 ;
				break;
				case	0x55	:
					/* Red = 0x0	Green = 0x0	Blue = 0x55	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x55	*/ 
					ExpSign->red = 0xc ; 	 ExpSign->green = 0x20 ; 	 ExpSign->blue = 0x83 ;
				break;
				case	0xaa	:
					/* Red = 0x0	Green = 0x0	Blue = 0xaa	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0xaa	*/ 
					ExpSign->red = 0x12f ; 	 ExpSign->green = 0x345 ; 	 ExpSign->blue = 0x97 ;
				break;
				case	0xff00	:
					/* Red = 0x0	Green = 0xff	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0xff	Blue = 0x0	*/ 
					ExpSign->red = 0x2e0 ; 	 ExpSign->green = 0x307 ; 	 ExpSign->blue = 0x3d9 ;
				break;
				case	0x5500	:
					/* Red = 0x0	Green = 0x55	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x55	Blue = 0x0	*/ 
					ExpSign->red = 0x1ce ; 	 ExpSign->green = 0x10f ; 	 ExpSign->blue = 0x1cf ;
				break;
				case	0xaa00	:
					/* Red = 0x0	Green = 0xaa	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0xaa	Blue = 0x0	*/ 
					ExpSign->red = 0x3c ; 	 ExpSign->green = 0xf4 ; 	 ExpSign->blue = 0x166 ;
				break;
				case	0xff0000	:
					/* Red = 0xff	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xff	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x225 ; 	 ExpSign->green = 0x258 ; 	 ExpSign->blue = 0x29c ;
				break;
				case	0x550000	:
					/* Red = 0x55	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x55	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x3ff ; 	 ExpSign->green = 0x29f ; 	 ExpSign->blue = 0x2b ;
				break;
				case	0xaa0000	:
					/* Red = 0xaa	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0xaa	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x2c8 ; 	 ExpSign->green = 0x23b ; 	 ExpSign->blue = 0x1c7 ;
				break;
				case	0x555555	:
					/* Red = 0x55	Green = 0x55	Blue = 0x55	*/ 
					/* Red = 0x55	Green = 0x55	Blue = 0x55	*/ 
					ExpSign->red = 0x23d ; 	 ExpSign->green = 0x3b0 ; 	 ExpSign->blue = 0x167 ;
				break;
				case	0xaaaaaa	:
					/* Red = 0xaa	Green = 0xaa	Blue = 0xaa	*/ 
					/* Red = 0xaa	Green = 0xaa	Blue = 0xaa	*/ 
					ExpSign->red = 0x3db ; 	 ExpSign->green = 0x18a ; 	 ExpSign->blue = 0x36 ;
				break;
				case	0xffffff	:
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					ExpSign->red = 0x2f4 ; 	 ExpSign->green = 0xc6 ; 	 ExpSign->blue = 0x221 ;
				break;
				case	0x0	:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					ExpSign->red = 0x312 ; 	 ExpSign->green = 0x2fc ; 	 ExpSign->blue = 0x370 ;
				break;
				case LinearAll:
					/* Red = 0x0	Green = 0x0	Blue = 0x0	*/ 
					/* Red = 0x0	Green = 0x0	Blue = 0x1	*/ 
					ExpSign->red = 0x73 ; 	 ExpSign->green = 0x1e6 ; 	 ExpSign->blue = 0x3b1 ;
				break;
				case DFB_COLORBARS:
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					/* Red = 0xff	Green = 0xff	Blue = 0xff	*/ 
					ExpSign->red = 0x2af ; 	 ExpSign->green = 0x14c ; 	 ExpSign->blue = 0x75 ;
				break;
			};
		break;
	}
}


/*ARGSUSED*/
int
mgras_DCB_WalkOnePixelPath(int argc, char **argv)
{
	 __uint32_t RGBMode  ;		/* RGB/CI mode Select*/
	 __uint32_t error, test_error = 0;
         __uint32_t loop, index;
	 __uint32_t CmapId;
	 __uint32_t CmapPatrn;
	 __uint32_t DcbRgbPatrn; 	/* DCBpatrn Select */
	 __uint32_t cmapshift, dacshift;
	 __uint32_t re4_id, mgras_pp1_id;

	In   = &Present;
	Prev = &Previous;
	ExpSign = &Signature;
	RcvSign = &HWSignature;
	
	MGRAS_GFX_CHECK();
	RGBMode = 1;

       	mgras_xmapSetPP1Select(mgbase,MGRAS_XMAP_WRITEALLPP1);

        /* Select Second byte of the config reg :- for AutoInc */
        mgras_xmapSetAddr(mgbase, 0x1) ;        /* Do NOT REMOVE THIS */ 
        mgras_xmapSetConfig(mgbase, 0xff000000); /* Hack for Auto Inc */

	error = 0;

	re4_id = 0; /* Default */
	GammaTableLoad(LINEAR_RAMP);
	for(CmapId=Cmap0; CmapId <= CmapAll; CmapId++) {
#if 0
	for(CmapId=CmapAll; CmapId >= Cmap0; CmapId--) X 
#endif

                switch (CmapId) {
                        case Cmap0   :
				mgras_pp1_id = MGRAS_XMAP_SELECT_PP1((re4_id << 1) | Cmap0);
#if 0
        			CmapLoad(LINEAR_RAMP);
#endif
                                break;

                        case Cmap1   :
				mgras_pp1_id = MGRAS_XMAP_SELECT_PP1((re4_id << 1) | Cmap1);
#if 0
        			CmapLoad(LINEAR_RAMP);
#endif
                                break;

                        case CmapAll :
                                mgras_pp1_id = MGRAS_XMAP_SELECT_BROADCAST;
#if 0
        			CmapLoad(LINEAR_RAMP);
#endif
                                break;
                        default :
                                msg_printf(ERR, "BAD CMAPID %x\n" ,CmapId);
#if 0
        			CmapLoad(LINEAR_RAMP);
#endif
				return (error);
                }
        	CmapLoad(LINEAR_RAMP);
		mgras_xmapSetPP1Select(mgbase, mgras_pp1_id);
		msg_printf(DBG, "xmapSetPP1Select(mgbase, 0x%x)\n" ,mgras_pp1_id);

        	for(loop = 0, dacshift = 0, cmapshift=0; loop < 3; loop++, dacshift +=10, cmapshift +=8)
		{
                	for (index=2; index < 10; index++) {
				CmapPatrn = 1<< (index-2);
				CmapPatrn <<= cmapshift;
                        	DcbRgbPatrn = (1<<index) & 0x3ff ;
				DcbRgbPatrn <<= dacshift;
				DcbRgbPatrn |= ((0x3 << 20) |(0x3 << 10) | 0x3) ;
				msg_printf(DBG, "DcbRgbPatrn %x\n",DcbRgbPatrn);
				LoadDiagScanReg(RGBMode, CmapPatrn) ;
				ResetDacCrc() ;
				GetSignature() ;
				GetComputeSig(CmapId, CmapPatrn);
				error += CrcCompare() ;
				CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
				if (error) {
					test_error += error;
					error = 0;
				}
                	}
        	}

        	for (index=2; index < 10; index++) {
			CmapPatrn = 1<< (index-2);
			CmapPatrn = (CmapPatrn << 16) | (CmapPatrn << 8) | CmapPatrn ;
                	DcbRgbPatrn = (1<<index)|0x3 ;
                	DcbRgbPatrn = (DcbRgbPatrn << 20) | (DcbRgbPatrn << 10) | DcbRgbPatrn ;
			msg_printf(DBG, "DcbRgbPatrn %x\n" ,DcbRgbPatrn);
			LoadDiagScanReg(RGBMode, CmapPatrn) ;
			ResetDacCrc() ;
			GetSignature() ;
			GetComputeSig(CmapId, CmapPatrn);
			error += CrcCompare() ;
			CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
			if (error) {
				test_error += error;
				error = 0;
			}
        	}

		CmapPatrn = 0x000000;
        	DcbRgbPatrn  = 0x00000000;
		msg_printf(DBG, "DcbRgbPatrn %x\n" ,DcbRgbPatrn);
		LoadDiagScanReg(RGBMode, CmapPatrn) ;
		ResetDacCrc() ;
		GetSignature() ;
		GetComputeSig(CmapId, CmapPatrn);
		error += CrcCompare() ;
		CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
		if (error) {
			test_error += error;
			error = 0;
		}
	
		CmapPatrn = 0x555555;
        	DcbRgbPatrn = ((0x55 << 2) | 0x3) & 0x3ff;
		DcbRgbPatrn = ((DcbRgbPatrn) | (DcbRgbPatrn << 10) | (DcbRgbPatrn << 20));
		msg_printf(DBG, "DcbRgbPatrn %x\n" ,DcbRgbPatrn);
		LoadDiagScanReg(RGBMode, CmapPatrn) ;
		ResetDacCrc() ;
		GetSignature() ;
		GetComputeSig(CmapId, CmapPatrn);
		error += CrcCompare() ;
		CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
		if (error) {
			test_error += error;
			error = 0;
		}
		error = 0;
	
		CmapPatrn = 0xaaaaaa;
        	DcbRgbPatrn = ((0xAA << 2) | 0x3) & 0x3ff;
		DcbRgbPatrn = ((DcbRgbPatrn) | (DcbRgbPatrn << 10) | (DcbRgbPatrn << 20));
		msg_printf(DBG, "DcbRgbPatrn %x\n" ,DcbRgbPatrn);
		LoadDiagScanReg(RGBMode, CmapPatrn) ;
		ResetDacCrc() ;
		GetSignature() ;
		GetComputeSig(CmapId, CmapPatrn);
		error += CrcCompare() ;
		CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
		if (error) {
			test_error += error;
			error = 0;
		}
	
		GammaTableLoad(LINEAR_RAMP);
		CmapPatrn = 0xffffff;
       		DcbRgbPatrn = ((0xff << 2) | 0x3 ) & 0x3ff;
       		DcbRgbPatrn = (DcbRgbPatrn << 20) | (DcbRgbPatrn << 10) | DcbRgbPatrn ;
		msg_printf(DBG, "DcbRgbPatrn %x\n" ,DcbRgbPatrn);
		LoadDiagScanReg(RGBMode, CmapPatrn) ;
		ResetDacCrc() ;
		GetSignature() ;
		GetComputeSig(CmapId, CmapPatrn);
		error += CrcCompare() ;
		CONTINUITY_CHECK((&BackEnd[PIXEL_PATH_PATH_TEST]), CmapId, DcbRgbPatrn, error);
		if (error) {
			test_error += error;
			error = 0;
		}

	}
	REPORT_PASS_OR_FAIL((&BackEnd[PIXEL_PATH_PATH_TEST]), test_error);
}
