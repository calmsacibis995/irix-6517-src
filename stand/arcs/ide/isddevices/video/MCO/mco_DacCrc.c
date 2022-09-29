/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  MCO 7162DAC CRC Diags.
 *
 *  $Revision: 1.6 $
 */

#include <libsc.h>
#include <uif.h>

#include <math.h>
#include "sys/mgrashw.h"
#ifdef NOTYET
#include "sys/vc2.h"
#endif /* NOTYET */
#include "ide_msg.h"
#include "mco_diag.h"
#ifdef IP30
#include <sys/mips_addrspace.h>
#include <sys/RACER/heart.h>
#endif /* IP30 */


extern mgras_hw *mgbase;

extern void mco_set_dcbctrl(mgras_hw *, int);
extern void mco_7162GetSig(mgras_hw *, uchar_t, __uint32_t,ushort *);
extern void mco_7162GetAddrCmd(mgras_hw *, uchar_t, __uint32_t, ushort_t *);
extern void mco_7162SetAddrCmd(mgras_hw *, uchar_t, __uint32_t, uchar_t);


__uint32_t 	mco_CrcCompare(uchar_t, int);
__uint32_t 	mco_Reset7162Crc(uchar_t) ;
__uint32_t 	mco_Get7162Signature(uchar_t);

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
         unsigned Bit0    : 1  ;
         unsigned blue    : 10 ;
         unsigned green    : 10 ;
         unsigned red      : 10 ;
         unsigned Bit31   : 1  ;
         unsigned Bit32   : 1  ;
} input;

input	mco7162AInput,       *mco7162AIn ;
input	mco7162BInput,       *mco7162BIn ;
input	mco7162APrevious,    *mco7162APrev ;
input	mco7162BPrevious,    *mco7162BPrev ;
input 	mco7162ASignature,   *mco7162AExpSign ;
input 	mco7162BSignature,   *mco7162BExpSign ;
input	mco7162AHWSignature, *mco7162ARcvSign ;
input	mco7162BHWSignature, *mco7162BRcvSign ;



GammaSize	mcoRed  ;
GammaSize	mcoBlue ;
GammaSize	mcoGreen;

GammaSize	mcoRedGamma[256] ;
GammaSize	mcoGrnGamma[256] ;
GammaSize	mcoBluGamma[256] ;

/*****************************************************************************/

#define 	MCO_7162DAC_RED_SIG	0x10
#define 	MCO_7162DAC_GRN_SIG	0x11
#define 	MCO_7162DAC_BLU_SIG	0x12
#define 	MCO_7162DAC_MISC_SIG	0x13

/*****************************************************************************/

input mco_7162ACrcprecomp[10] = {
    {	/* Green 50% */
	/* mg0_clear_color -g 50 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x159,		/* unsigned blue    : 10 ;	*/
	0x195,		/* unsigned green    : 10 ;	*/
	0x3B7,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Black */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x3C9,		/* unsigned blue    : 10 ;	*/
	0x3F5,		/* unsigned green    : 10 ;	*/
	0x1DD,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Red */
	/* mg0_clear_color -c 256 -r 255 -g 0 -b 0 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x3A8,		/* unsigned blue    : 10 ;	*/
	0x3B9,		/* unsigned green    : 10 ;	*/
	0x249,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Green */
	/* mg0_clear_color -c 256 -r 0 -g 255 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x83,		/* unsigned blue    : 10 ;	*/
	0x251,		/* unsigned green    : 10 ;	*/
	0x2B1,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Blue */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 255 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x23D,		/* unsigned blue    : 10 ;	*/
	0xF9,		/* unsigned green    : 10 ;	*/
	0x3BD,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid White */
	/* mg0_clear_color -c 256 -r 255 -g 255 -b 255 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x116,		/* unsigned blue    : 10 ;	*/
	0x111,		/* unsigned green    : 10 ;	*/
	0x345,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* R=0xAA, G=0xAA, B=0xAA */
	/* mg0_clear_color -c 256 -r 170 -g 170 -b 170 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x24C,		/* unsigned blue    : 10 ;	*/
	0x333,		/* unsigned green    : 10 ;	*/
	0xBA,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* R=0x55, G=0x55, B=0x55 */
	/* mg0_clear_color -c 256 -r 85 -g 85 -b 85 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x159,		/* unsigned blue    : 10 ;	*/
	0x097,		/* unsigned green    : 10 ;	*/
	0x120,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0xA5,		/* unsigned blue    : 10 ;	*/
	0xA5,		/* unsigned green    : 10 ;	*/
	0xA5,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0xA5,		/* unsigned blue    : 10 ;	*/
	0xA5,		/* unsigned green    : 10 ;	*/
	0xA5,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
};	

input mco_7162BCrcprecomp[10] = {
    {	/* Green 50% */
	/* mg0_clear_color -g 50 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x159,		/* unsigned blue    : 10 ;	*/
	0x195,		/* unsigned green    : 10 ;	*/
	0x3B7,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Black */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x3C9,		/* unsigned blue    : 10 ;	*/
	0x3F5,		/* unsigned green    : 10 ;	*/
	0x1DD,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Red */
	/* mg0_clear_color -c 256 -r 255 -g 0 -b 0 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x3A8,		/* unsigned blue    : 10 ;	*/
	0x3B9,		/* unsigned green    : 10 ;	*/
	0x249,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Green */
	/* mg0_clear_color -c 256 -r 0 -g 255 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x83,		/* unsigned blue    : 10 ;	*/
	0x251,		/* unsigned green    : 10 ;	*/
	0x2B1,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid Blue */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 255 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x23D,		/* unsigned blue    : 10 ;	*/
	0xF9,		/* unsigned green    : 10 ;	*/
	0x3BD,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* Solid White */
	/* mg0_clear_color -c 256 -r 255 -g 255 -b 255 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0x116,		/* unsigned blue    : 10 ;	*/
	0x111,		/* unsigned green    : 10 ;	*/
	0x345,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* R=0xAA, G=0xAA, B=0xAA */
	/* mg0_clear_color -c 256 -r 170 -g 170 -b 170 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x24C,		/* unsigned blue    : 10 ;	*/
	0x333,		/* unsigned green    : 10 ;	*/
	0xBA,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
    {	/* R=0x55, G=0x55, B=0x55 */
	/* mg0_clear_color -c 256 -r 85 -g 85 -b 85 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0x159,		/* unsigned blue    : 10 ;	*/
	0x097,		/* unsigned green    : 10 ;	*/
	0x120,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	1,		/* unsigned Bit0    : 1  ;	*/
	0xA5,		/* unsigned blue    : 10 ;	*/
	0xA5,		/* unsigned green    : 10 ;	*/
	0xA5,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	1,		/* unsigned Bit32   : 1  ;	*/
    },
    {
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	0,		/* unsigned Bit0    : 1  ;	*/
	0xA5,		/* unsigned blue    : 10 ;	*/
	0xA5,		/* unsigned green    : 10 ;	*/
	0xA5,		/* unsigned red      : 10 ;	*/
	0,		/* unsigned Bit31   : 1  ;	*/
	0,		/* unsigned Bit32   : 1  ;	*/
    },
};	

input *mco_four_screen_CRCprecomp(uchar_t device, int screenpat) {
   switch (device) {
	case MCO_ADV7162A:
	   return((input *)&(mco_7162ACrcprecomp[screenpat-1]));
	case MCO_ADV7162B:
	   return((input *)&(mco_7162BCrcprecomp[screenpat-1]));
	default:
	   return((input *)&(mco_7162ACrcprecomp[screenpat-1]));
   }
}

__uint32_t
mco_CrcCompare(uchar_t device, int pattern)
{
	GammaSize	ExpRed ;
	GammaSize	ExpBlu ;
	GammaSize	ExpGrn ;
 	__uint32_t 	errors ;
	input *ExpSign;
	input *RcvSign;
	char Device[2];

	errors = 0;

	switch (device) {
	    case (MCO_ADV7162A):
		sprintf(Device, "A");
		ExpSign = mco7162AExpSign;
		RcvSign = mco7162ARcvSign;
		break;
	    case (MCO_ADV7162B):
		sprintf(Device, "B");
		ExpSign = mco7162BExpSign;
		RcvSign = mco7162BRcvSign;
		break;
	    default:
		sprintf(Device, "A");
		ExpSign = mco7162AExpSign;
		RcvSign = mco7162ARcvSign;
		break;
	}
	msg_printf(DBG, "mco_CrcCompare: device 7162%s, pattern %d\n", 
	Device, pattern);

	ExpRed.channel  = ExpSign->red;
	ExpGrn.channel  = ExpSign->green;
	ExpBlu.channel  = ExpSign->blue;

	msg_printf(DBG, "mco_CrcCompare: 7162%s  Red Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpRed.channel, RcvSign->red) ;
	if (RcvSign->red != ExpRed.channel) {
		msg_printf(ERR, "ERROR ==> 7162%s Red Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpRed.channel, RcvSign->red) ;
		++errors ;
	}

	msg_printf(DBG, "mco_CrcCompare: 7162%s  Green Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpGrn.channel, RcvSign->green) ;
	if (RcvSign->green != ExpGrn.channel) {
		msg_printf(ERR, "ERROR ==> 7162%s Green Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpGrn.channel, RcvSign->green) ;
		++errors ;
	}

	msg_printf(DBG, "mco_CrcCompare:  7162%s Blue Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpBlu.channel, RcvSign->blue) ;
	if (RcvSign->blue != ExpBlu.channel) {
		msg_printf(ERR, "ERROR ==> 7162%s Blue Channel CRC's :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpBlu.channel, RcvSign->blue) ;
		++errors ;
	}

	msg_printf(DBG, "mco_CrcCompare:  7162%s Bit 0 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit0, RcvSign->Bit0) ;
	if (RcvSign->Bit0 != ExpSign->Bit0) {
		msg_printf(ERR, "ERROR ==> 7162%s Bit 0 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit0, RcvSign->Bit0) ;
		++errors ;
	}

	msg_printf(DBG, "mco_CrcCompare:  7162%s Bit 31 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit31, RcvSign->Bit31) ;
	if (RcvSign->Bit31 != ExpSign->Bit31) {
		msg_printf(ERR, "ERROR ==> 7162%s Bit 31 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit31, RcvSign->Bit31) ;
		++errors ;
	}

	msg_printf(DBG, "mco_CrcCompare:  7162%s Bit 32 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit32, RcvSign->Bit32) ;
	if (RcvSign->Bit32 != ExpSign->Bit32) {
		msg_printf(ERR, "ERROR ==> 7162%s Bit 32 CRC :-\t Exp 0x%x\t  Rcv 0x%x\n" , Device, ExpSign->Bit32, RcvSign->Bit32) ;
		++errors ;
	}

	return (errors) ;
}

__uint32_t
mco_Get7162Signature(uchar_t device)
{
    ushort_t ControlReg4 = 0xbeef;
    ushort_t RcvRed, RcvGrn, RcvBlu, RcvMisc ;
    input *RcvSign;
    ushort_t	lncount;	\

    #ifdef NOTYET
    #ifdef IP30
    __uint64_t ticker, delta;

    ticker = 0;
    delta = 0;
    #endif	/* IP30 */
    #endif	/* NOTYET */

    RcvRed = 0xbeef ; RcvBlu = 0xbeef ; RcvGrn = 0xbeef ; RcvMisc = 0xbeef;

    switch (device) {
	case (MCO_ADV7162A):
	    msg_printf(DBG, "mco_Get7162Signature: device = 7162A\n");
	    RcvSign = mco7162ARcvSign;
	    break;
	case (MCO_ADV7162B):
	    msg_printf(DBG, "mco_Get7162Signature: device = 7162B\n");
	    RcvSign = mco7162BRcvSign;
	    break;
	default:
	    msg_printf(DBG, "mco_Get7162Signature: device = 7162A\n");
	    RcvSign = mco7162ARcvSign;
	    break;
    }

    /* Get 7162 DAC ControlReg4 */
    mco_set_dcbctrl(mgbase, MCO_INDEX); /* Set DCBCTRL to access the 7162 DACs */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, &ControlReg4);
    msg_printf(DBG,"mco_Get7162Signature: 7162 DAC CtrlReg4 = 0x%x\n",
    ControlReg4);

    /* Reset signature reg and acquire signature ...*/

    /* First, make sure that the DCB CTRL reg is set up for VC3 accesses... */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mgbase->dcbctrl_vc3 = MGRAS_DCBCTRL_VC3;

    /* Wait until MCO is in vertical retrace */
    msg_printf(DBG,"mco_Get7162Signature: Waiting for VBlank...\n");
    lncount = 0xff;
    do {
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount);
    } while (lncount != REFRESH_START);
    #ifdef NOTYET
    #ifdef IP30
    ticker = HEART_PIU_K1PTR->h_count;
    #endif	/* IP30 */
    #endif	/* NOTYET */

    /* Enable 7162 DAC Signature Analyzer */

    /* Bits 5 & 7 = 1, Toggle Bit 6 to reset analyzer */
    ControlReg4 = 0xA0;	
    mco_set_dcbctrl(mgbase, MCO_INDEX); /* Set DCBCTRL to access the 7162 DACs */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, ControlReg4);

    ControlReg4 = 0xE0;	
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, ControlReg4);

    /* msg_printf(VRB,"mco_Get7162Signature: Waiting for No VBlank...\n"); */
    lncount = 0xff;
    do {
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount);
    } while (lncount < 200);
	
    /* Wait until next vertical retrace */
    /* msg_printf(VRB,"mco_Get7162Signature: Waiting for VBlank...\n"); */
    lncount = 0xff;
    do {				
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount);
    } while (lncount != REFRESH_START);
    #ifdef NOTYET
    #ifdef IP30
    delta = HEART_PIU_K1PTR->h_count - ticker;
    #endif	/* IP30 */
    #endif	/* NOTYET */

    /* Stop the 7162 Signature Analyzer */

    ControlReg4 = 0xC0;
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, ControlReg4);

    /* Read the signatures from the 7162 Signature Registers */
    #ifdef NOTYET
    msg_printf(DBG,"mco_Get7162Signature: Vert Retrace time was %d usecs (%d heart ticks)\n",
    ((delta*2)/25), delta);
    #endif	/* NOTYET */
    msg_printf(DBG,"mco_Get7162Signature: Read 7162 Signature Analyzer\n");
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetSig(mgbase, device, MCO_7162DAC_RED_SIG, &RcvRed);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetSig(mgbase, device, MCO_7162DAC_GRN_SIG, &RcvGrn);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetSig(mgbase, device, MCO_7162DAC_BLU_SIG, &RcvBlu);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetSig(mgbase, device, MCO_7162DAC_MISC_SIG, &RcvMisc);

    RcvSign->red   = RcvRed & 0x3ff ;
    RcvSign->green = RcvGrn & 0x3ff ;
    RcvSign->blue  = RcvBlu & 0x3ff ;
    RcvSign->Bit0  = RcvMisc & 0x1;
    RcvSign->Bit31 = (RcvMisc & 0x2) >> 1;
    RcvSign->Bit32 = (RcvMisc & 0x4) >> 2;

    msg_printf(DBG, "mco_Get7162Signature CRC's Recv (1) R 0x%x G 0x%x B 0x%x Misc 0x%x\n",
    	RcvRed, RcvGrn, RcvBlu, RcvMisc);

#ifndef IP30
    /* Restore the 7162 Signature Analyzer */
    ControlReg4 = 0x0;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, ControlReg4);
#endif	/* IP30 */

    return 0;
}

#ifdef NOTYET
#define	CLOCKS	(1280 * 1024)
#endif /* NOTYET */


#ifdef NOTYET
void
mco_ComputeCrc( __uint32_t testPattern)
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
#endif /* NOTYET */

__uint32_t
mco_Reset7162Crc(uchar_t device) 
{
    switch (device) {
	case MCO_ADV7162A:
	    msg_printf(DBG, "mco_Reset7162Crc: Resetting 7162A CRCs\n");
	    mco7162ARcvSign->Bit32 = mco7162AIn->Bit32 = mco7162APrev->Bit32 = 0 ;
            mco7162ARcvSign->Bit31 = mco7162AIn->Bit31 = mco7162APrev->Bit31 = 0 ;
            mco7162ARcvSign->red   = mco7162AIn->red   = mco7162APrev->red   = 0 ;
            mco7162ARcvSign->green = mco7162AIn->green = mco7162APrev->green = 0 ;
            mco7162ARcvSign->blue  = mco7162AIn->blue  = mco7162APrev->blue  = 0 ;
            mco7162ARcvSign->Bit0  = mco7162AIn->Bit0  = mco7162APrev->Bit0  = 0 ;
	    msg_printf(DBG, "mco_Reset7162Crc: DONE Resetting 7162A CRCs\n");
	    break;
	case MCO_ADV7162B:
	    msg_printf(DBG, "mco_Reset7162Crc: Resetting 7162B CRCs\n");
	    mco7162BRcvSign->Bit32 = mco7162BIn->Bit32 = mco7162BPrev->Bit32 = 0 ;
            mco7162BRcvSign->Bit31 = mco7162BIn->Bit31 = mco7162BPrev->Bit31 = 0 ;
            mco7162BRcvSign->red   = mco7162BIn->red   = mco7162BPrev->red   = 0 ;
            mco7162BRcvSign->green = mco7162BIn->green = mco7162BPrev->green = 0 ;
            mco7162BRcvSign->blue  = mco7162BIn->blue  = mco7162BPrev->blue  = 0 ;
            mco7162BRcvSign->Bit0  = mco7162BIn->Bit0  = mco7162BPrev->Bit0  = 0 ;
	    break;
    }
    return 0;
}


/* mco_DacCrcTest -- Reads CRC Signature from specified 7162 DAC
 *	and compares against "known good" precomputed CRC signatures
 *
 *	Requirements:  This test assumes the MCO VOF is set up already
 * 	    and that the test pattern is written into the MGRAS framebuffer
 *
 *	usage: mco_daccrctest device testpattern#
 *	
 *	where device is one of: A B
 *	and testpattern# is a number from 1 to 6 
 *
 *	Test description:    This test reads the specified 
 *		7162 Dac Signature Registers and compares the checksus 
 *		with the pre-computed DAC checksum values.
 */


int 
mco_DacCrcTest(int argc, char **argv)
{
	uchar_t device = MCO_ADV7162A;
	__uint32_t errors = 0;
	char tmpstr[80];
	int pattern = 0;

	if (argc != 3) {
		msg_printf(SUM, "usage %s Device testpattern# (where Device is A or B)\n",
		argv[0]);
		return -1;
	}

	mco7162AIn   = &mco7162AInput ;
	mco7162BIn   = &mco7162BInput ;

	mco7162APrev = &mco7162APrevious ;
	mco7162BPrev = &mco7162BPrevious ;

	mco7162ARcvSign = &mco7162AHWSignature;
	mco7162BRcvSign = &mco7162BHWSignature;

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(tmpstr, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		sprintf(tmpstr, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(tmpstr, "A");
		break;
	}

	pattern = atoi(argv[2]);

	mco7162AExpSign = &mco_7162ACrcprecomp[pattern-1];
	mco7162BExpSign = &mco_7162BCrcprecomp[pattern-1];

	mco_Reset7162Crc(device) ;
	errors = mco_Get7162Signature(device) ;
	errors += mco_CrcCompare(device, pattern) ;
#ifdef NOTYET
	if (errors) {
	    msg_printf(DBG,"\nMCO 7162%c CRC changed: R 0x%x G 0x%x B 0x%x\n",
	    argv[1][0], RcvSign->red, RcvSign->green, RcvSign->blue);
	    return errors;
	}
	else {
		msg_printf(VRB,"MCO 7162%c CRC Test PASSED\n", argv[1][0]);
	}
#endif /* NOTYET */

	switch (device) {
	    case MCO_ADV7162A:
		REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_DACA_CRC_TEST]), errors);
	    case MCO_ADV7162B:
		REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_DACB_CRC_TEST]), errors);
	}

	return (errors);
}
