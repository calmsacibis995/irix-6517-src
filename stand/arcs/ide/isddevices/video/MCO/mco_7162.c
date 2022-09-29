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
 *  DAC- ADV 7162  Diagnostics.
 *
 *  $Revision: 1.18 $
 */

#include <sys/types.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include <math.h>
#include "sys/mgrashw.h"
#ifdef NOTYET
#include "sys/vc2.h"
#endif /* NOTYET */
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif
#include "mco_diag.h"


typedef struct _GammaSize{
         __uint32_t channel : 10 ;
}GammaSize;

/* XXX From mgras_diag.h */
typedef struct _DacInternal_Reg_Info {
		char *str;
		ushort_t RegName;
                ushort_t mask;
} _DacInternal_Reg_Info ;

extern mgras_hw *mgbase;
#ifdef IP30
extern void sr_mco_set_dcbctrl(mgras_hw *, int crs);
#endif	/* IP30 */
extern void mco_set_dcbctrl(mgras_hw *, int crs);

static GammaSize	Red[256],    Green[256],    Blue[256];
static GammaSize	RedRcv[256], GreenRcv[256], BlueRcv[256];

static _DacInternal_Reg_Info _MCODAC_Internal_Reg [] = {
	{"MCO_ADV7162_ID_ADDR",		MCO_ADV7162_ID_ADDR, 		0xff},
	{"MCO_ADV7162_REV_ADDR",	MCO_ADV7162_REV_ADDR,		0xff},
	{"MCO_ADV7162_PIXMASK_ADDR",	MCO_ADV7162_PIXMASK_ADDR,	0xff},
	{"MCO_ADV7162_CMD1_ADDR",	MCO_ADV7162_CMD1_ADDR,		0xff},
	{"MCO_ADV7162_CMD2_ADDR",	MCO_ADV7162_CMD2_ADDR,		0xff},
	{"MCO_ADV7162_CMD3_ADDR",	MCO_ADV7162_CMD3_ADDR,		0xff},
	{"MCO_ADV7162_CMD4_ADDR",	MCO_ADV7162_CMD4_ADDR,		0xff},
};

static ushort_t dac_patrn[] = {
        0x35a, 0x23c, 0x1f0, 0x3a5, 
	0x2c3, 0x10f, 0x0ff,
        0x35a, 0x23c, 0x1f0, 0x3a5, 
	0x2c3, 0x10f, 0x0ff
};

static ushort_t dac_walking_patrn[] = {
        0x001,   0x002,   0x004,   0x008,
        0x010,   0x020,   0x040,   0x080,
	0x100,	 0x200,	 
        0x0FE,   0x0FD,   0x0FB,   0x0F7,
        0x0EF,   0x0DF,   0x0BF,   0x07F,
	0x2FF,	 0x1FF

};


/*
 *	Forward References 
 */
void mco_7162SetRGB(mgras_hw *,__uint32_t,__uint32_t,__uint32_t);
void mco_7162GetRGB(mgras_hw *,__uint32_t *,__uint32_t *,__uint32_t *);
void mco_7162SetCtrl(mgras_hw *base, uchar_t device, uchar_t data);
void mco_7162GetCtrl(mgras_hw *base, uchar_t device, uchar_t *Rcv);
void mco_7162SetModeReg(mgras_hw *base,uchar_t device,uchar_t data);
void mco_7162GetModeReg(mgras_hw *base,uchar_t device,ushort_t *Rcv);
void mco_7162GetAddrReg(mgras_hw *base, uchar_t device, ushort_t *Rcv);
void mco_WriteDac7162( uchar_t device, __uint32_t addr,GammaSize *red, GammaSize *green, 
	  GammaSize *blue,  __uint32_t length,  ushort_t mask);

void mco_ReadDac7162( uchar_t device, __uint32_t addr, GammaSize *red, GammaSize *green, GammaSize *blue, __uint32_t length);

__uint32_t mco_Dac7162Compare(char *TestName, uchar_t device, GammaSize *exp,  GammaSize *rcv, __uint32_t length,  ushort_t mask);

__uint32_t mco_7162ClrPaletteAddrUniqTest(uchar_t);
__uint32_t mco_7162ClrPalettePatrnTest(uchar_t);
__uint32_t mco_7162ClrPaletteWalkBitTest(uchar_t);
__uint32_t _mco_Dac7162AddrRegTest(uchar_t);
__uint32_t _mco_Dac7162ModeRegTest(uchar_t);
__uint32_t _mco_Dac7162CtrlRegTest(uchar_t);

__uint32_t mco_Dac7162PatrnLoad( ushort_t PatrnType);
__uint32_t _mco_Dac7162PLLInit( __uint32_t mcoTimingSelect);
__uint32_t _mco_Dac7162Reset(uchar_t);
int _mco_send7162LUT(uchar_t, unsigned short pixels[256][3]);


/*******************************************************************
 * Miscellaneous 7162 Access Functions (originally macros)
 */

/* 7162 Address Register MUST be set before using this function! */
void 
mco_7162SetRGB(mgras_hw *base,__uint32_t r,__uint32_t g,__uint32_t b) {
	MCO_IO_WRITE(base, ((r & 0x03fc) >> 2));
	MCO_IO_WRITE(base, (r & 0x3)); 	
	MCO_IO_WRITE(base, ((g & 0x03fc) >> 2));
	MCO_IO_WRITE(base, (g & 0x3));
	MCO_IO_WRITE(base, ((b & 0x03fc) >> 2));
	MCO_IO_WRITE(base, (b & 0x3));
}

/* 7162 Address Register MUST be set before using this function! */
void 
mco_7162GetRGB(mgras_hw *base,__uint32_t *r,__uint32_t *g,__uint32_t *b) {
	unsigned char _low, _high;

	MCO_IO_READ(base, _high);
	MCO_IO_READ(base, _low);
	*r = ((_high & 0xff) << 2) | (_low & 0x3);
	MCO_IO_READ(base, _high);
	MCO_IO_READ(base, _low);
	*g = ((_high & 0xff) << 2) | (_low & 0x3);
	MCO_IO_READ(base, _high);
	MCO_IO_READ(base, _low);
	*b = ((_high & 0xff) << 2) | (_low & 0x3);
}

/* 7162 Address Register MUST be set before using this function! */
void 
mco_7162SetCtrl(mgras_hw *base, uchar_t device, uchar_t data) {
	MCO_SETINDEX(base, device, MCO_ADV7162_CTRL);
	MCO_IO_WRITE(base, data);
}

/* 7162 Address Register MUST be set before using this function! */
void
mco_7162GetCtrl(mgras_hw *base, uchar_t device, uchar_t *Rcv) {
	MCO_SETINDEX(base, device, MCO_ADV7162_CTRL);
	MCO_IO_READ(base, *Rcv);
}

void
mco_7162SetModeReg(mgras_hw *base,uchar_t device,uchar_t data) {
	MCO_ADV7162_SETADDR(base, device, MCO_ADV7162_MODE);
	MCO_IO_WRITE(base,(data & 0xff));
}

void
mco_7162GetModeReg(mgras_hw *base,uchar_t device,ushort_t *Rcv) {
	MCO_ADV7162_SETADDR(base, device, MCO_ADV7162_MODE);
	MCO_IO_READ(base, *Rcv);
}

void
mco_7162SetAddrReg(mgras_hw *base, uchar_t device, __uint32_t addr) {
	MCO_ADV7162_SETADDR(base, device, MCO_ADV7162_ADDR);
	MCO_IO_WRITE(base,(addr & 0xff));
	MCO_IO_WRITE(base,((addr >> 8) & 0x3));
}

void
mco_7162GetAddrReg(mgras_hw *base, uchar_t device, ushort_t *Rcv) {
	uchar_t _lobyte;
	uchar_t _hibyte;

	MCO_ADV7162_SETADDR(base, device, MCO_ADV7162_ADDR);
	MCO_IO_READ(base, _lobyte);
	MCO_IO_READ(base, _hibyte);
	*Rcv = ((_hibyte & 0x3) << 8) | (_lobyte & 0xff);
}

void
mco_7162SetAddrCmd(mgras_hw *base, 
		   uchar_t device, 
		   __uint32_t addr, 
		   uchar_t data)
{
	mco_7162SetAddrReg(base, device, addr);
	MCO_SETINDEX(base, device, MCO_ADV7162_CTRL);
	MCO_IO_WRITE(base, data);
}

void
mco_7162GetAddrCmd(mgras_hw *base,
		   uchar_t device, 
		   __uint32_t addr, 
		   ushort_t *Rcv)
{
	mco_7162SetAddrReg(base, device, addr);
	MCO_SETINDEX(base, device, MCO_ADV7162_CTRL);
	MCO_IO_READ(base, *Rcv);
}

/* 7162 Address Register MUST be set before using this function! */
void
mco_7162GetSig(mgras_hw *base, uchar_t device, __uint32_t addr,ushort_t *Rcv) {
	unsigned char _low, _high;

	mco_7162SetAddrReg(base, device, addr);
	MCO_SETINDEX(base, device, MCO_ADV7162_CTRL);
	MCO_IO_READ(base, _high);
	MCO_IO_READ(base, _low);
	*Rcv = ((_high & 0xff) << 2) | (_low & 0x3);
}

/**********************************************************************/

/********************************************************************* 
 * mco_init7162 is very similar to mco_probe7162() except that   *
 * the calling function provides the initialization values. 
 *********************************************************************/

int 
_mco_init7162(uchar_t device, ramDAC7162Def *dacvals)
{

    uchar_t id;
    uchar_t mode, cmd2, cmd3, cmd4, cmd5, pllr, pllv, pllctl;
    int errs = 0;
    char dacname[10];

    switch(device) {
	case MCO_ADV7162A:
	    sprintf(dacname,"7162A");
	    break;
#ifndef IP30
	case MCO_ADV7162B:
	    sprintf(dacname,"7162B");
	    break;
#endif	/* IP30 */
    }

    /* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_set_dcbctrl(mgbase, 2);

    /* The initialization procedure given here is based on Appendix 4 *
     * of the 7162 data sheet.  It is VERY IMPORTANT that the first   *
     * accesses to the 7162 treat it as though the address register   *
     * is 8 bits -- Thus, do not use setaddr7162 for the              *
     * initialization of CMD1 as setaddr7162 does 16 bit writes...    */

#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: MCO 7162 ID = 0x%x\n",
    dacvals->id);
    msg_printf(VRB,"mco_init7162: MCO 7162 Base Addr = 0x%x\n",
    dacvals->addr);
    msg_printf(VRB,"mco_init7162: MCO 7162 Mode Reg = 0x%x\n",
    dacvals->mode);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cmd1 Reg = 0x%x\n",
    dacvals->cmd1);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cmd2 Reg = 0x%x\n",
    dacvals->cmd2);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cmd3 Reg = 0x%x\n",
    dacvals->cmd3);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cmd4 Reg = 0x%x\n",
    dacvals->cmd4);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cmd5 Reg = 0x%x\n",
    dacvals->cmd5);
    msg_printf(VRB,"mco_init7162: MCO 7162 PLL R Reg = 0x%x\n",
    dacvals->pllr);
    msg_printf(VRB,"mco_init7162: MCO 7162 PLL V Reg = 0x%x\n",
    dacvals->pllv);
    msg_printf(VRB,"mco_init7162: MCO 7162 PLL Cntl Reg = 0x%x\n",
    dacvals->pllctl);
    msg_printf(VRB,"mco_init7162: MCO 7162 Cursor Reg = 0x%x\n",
    dacvals->curs);
    for (i=0; i<256; i++) {
	msg_printf(VRB,"mco_init7162: DAC 7162 LUT[%d][0-2] = ", i);
	for (j=0; j<3; j++) {
	    msg_printf(VRB,"(%3d),\t", *(dacvals->lut+i+j));
	}
	msg_printf(VRB,"\n");
    }
#endif /* NOTYET */


    /* Reset the DAC */
    msg_printf(VRB, "Resetting the %s DAC\n", dacname);

    /* Set MCO Index Register to point to 7162 MR1 */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_MODE);
#ifdef NOTYET
    msg_printf(DBG, "Set MCO Index Register to point to 7162 MR1\n");
#endif /* NOTYET */
 
    /* PS0, PS1 = 0; No Calib; Normal Op.; 8-bit MPU Bus; 10-bit DAC; RESET */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT & ~MCO_ADV7162_MODE_RESET_BIT);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, dacvals->mode);

    /* This test is not robust because capacitance on the DCB lines could     *
     * cause the test to succeed... The ID test (next) is conclusive, though. */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, mode);
    if ((mode & 0xff) != dacvals->mode) {
	msg_printf(VRB, "MR1 Write Failed. (Rd 0x%x != Exp 0x%x)\n", mode, dacvals->mode);
	errs++;
    }

    /* Select ADDRESS Port */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_ADDR);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_CMD1_ADDR);

    /* Select CMD Port */
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_CTRL);
    MCO_IO_WRITE(mgbase, (dacvals->cmd1 & 0xFE));

#ifdef NOTYET
    msg_printf(DBG, "Reading the %s DAC ID\n", dacname);
#endif /* NOTYET */

    /* Check to see if the ID register is accessible... */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrReg(mgbase, device, MCO_ADV7162_ID_ADDR);	/* Select ID */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_CTRL);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, id);

    if (id != DAC7162_ID) {
	msg_printf(VRB, "Invalid 7162 ID: %d != %d\n", id, DAC7162_ID);
	errs++;
    }

    /* Now initialize the command registers. */

    /* Set color mode (cmd2) ... */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD2_ADDR, dacvals->cmd2);
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd2 Reg = 0x%x\n", dacvals->cmd2);
#endif /* NOTYET */
    /* Check that Cmd2 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd2);
    if (cmd2 != dacvals->cmd2) {
	msg_printf(VRB, "CMD2 Write Failed. (Rd 0x%x != Exp 0x%x)\n", cmd2, dacvals->cmd2);
	errs++;
    }

    /* Mux configuration (cmd3) ... */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD3_ADDR, dacvals->cmd3);
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd3 Reg = 0x%x\n", dacvals->cmd3);
#endif /* NOTYET */
    /* Check that Cmd3 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd3);
    if (cmd3 != dacvals->cmd3) {
	msg_printf(VRB, "CMD3 Write Failed. (Rd 0x%x != Exp 0x%x)\n", cmd3, dacvals->cmd3);
	errs++;
    }

    
    /* cmd4 ?? */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, dacvals->cmd4);
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd4 Reg = 0x%x\n", dacvals->cmd4);
#endif /* NOTYET */
    /* Check that Cmd4 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd4);
    if (cmd4 != dacvals->cmd4) {
	msg_printf(VRB, "CMD4 Write Failed. (Rd 0x%x != Exp 0x%x)\n", cmd4, dacvals->cmd4);
	errs++;
    }


    /* Internal PLL */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_R_ADDR, dacvals->pllr);
    /* Check that PLLR register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllr);
    if ((pllr & 0x7f) != dacvals->pllr) {
	msg_printf(VRB, "PLLR Write Failed. (Rd 0x%x != Exp 0x%x)\n", pllr, dacvals->pllr);
	errs++;
    }

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_V_ADDR, dacvals->pllv);
    /* Check that pllv register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllv);
    if ((pllv & 0x7f) != dacvals->pllv) {
	msg_printf(VRB, "PLLV Write Failed. (Rd 0x%x != Exp 0x%x)\n", pllv, dacvals->pllv);
	errs++;
    }

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_CTL_ADDR, dacvals->pllctl);
    /* Check that pllctl register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllctl);
    if (pllctl != dacvals->pllctl) {
	msg_printf(VRB, "PLLCTL Write Failed. (Rd 0x%x != Exp 0x%x)\n", pllctl, dacvals->pllctl);
	errs++;
    }

#ifdef IP30
    /* Set up 7162 to use the External PLL */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD5_ADDR, (dacvals->cmd5 & 0));
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd5 Reg = 0x%x\n", (dacvals->cmd5 & 0));
#endif /* NOTYET */
    /* Check that Cmd5 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd5);
    if (cmd5 != (dacvals->cmd5 & 0x0)) {
	msg_printf(VRB, "CMD5 Write Failed. (Rd 0x%x != Exp 0x%x)\n", 
		cmd5, (dacvals->cmd5 & 0x0));
	errs++;
    }
#else
    /* Enable the Internal PLL */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD5_ADDR, dacvals->cmd5);
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd5 Reg = 0x%x\n", dacvals->cmd5);
#endif /* NOTYET */
    /* Check that Cmd5 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd5);
    if (cmd5 != dacvals->cmd5) {
	msg_printf(VRB, "CMD5 Write Failed. (Rd 0x%x != Exp 0x%x)\n", cmd5, dacvals->cmd5);
	errs++;
    }
#endif /* IP30 */

    /* Cursors... */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CURS_ADDR, dacvals->curs);
    
    /* Set up Gamma Tables */
    _mco_send7162LUT(device, dacvals->lut);	/* XXX fix the lut type here! */

#ifdef NOTYET
#ifdef IP30
    /* UnBlank the DACs */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_ADDR);

    b = (MCO_ADV7162_PIXMASK_ADDR & 0x00ff);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, b);                       /* Send the ADDRESS lb  */

    b = ((MCO_ADV7162_PIXMASK_ADDR & 0x0300) >> 8);
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, b);                       /* Send the ADDRESS hb  */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_CMD);

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, 0xFF);

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_WRITE(mgbase, 0xFF);
#endif	/* IP30 */
#endif	/* NOTYET */
    msg_printf(VRB, "Initialization of %s DAC completed\n", dacname);
    return(errs);
}

/* Probe to see if the 7162 is out there.  This function destroys the   *
 * current state of the 7162.                                           */

int 
mco_probe7162(uchar_t device) 
{

    int id;
    int i, t;
    GammaSize red[256];
    GammaSize green[256]; 
    GammaSize blue[256];


    switch (device) {
	case MCO_ADV7162A:
	    msg_printf(DBG, "mco_probe7162: Probing MCO 7162A...\n");
	    break;
#ifndef IP30
	case MCO_ADV7162B:
	    msg_printf(DBG, "mco_probe7162: Probing MCO 7162B...\n");
	    break;
#endif	/* IP30 */
    }
    /* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
    mco_set_dcbctrl(mgbase, MCO_INDEX);

    /* The initialization procedure given here is based on Appendix 4 *
     * of the 7162 data sheet.  It is VERY IMPORTANT that the first   *
     * accesses to the 7162 treat it as though the address register   *
     * is 8 bits -- Thus, do not use setaddr7162 for the              *
     * initialization of CMD1 as setaddr7162 does 16 bit writes...    */

    /* Reset the DAC */

    /* Set MCO Index Register to point to 7162 MR1 */
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_MODE);

    /* PS0, PS1 = 0; No Calib; Normal Op.; 8-bit MPU Bus; 10-bit DAC; RESET */
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT);		/* MR1 */
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT & ~MCO_ADV7162_MODE_RESET_BIT);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT);		/* MR1 */

    /* This test is not robust because capacitance on the DCB lines could 
     * cause the test to succeed... The ID test (next) is conclusive, though.
     */

    MCO_IO_READ(mgbase, t);
    if (t != MCO_ADV7162_MODE_INIT) {
	msg_printf(VRB, "MR1 Write Failed. (0x%x != 0x%x)\n", t, MCO_ADV7162_MODE_INIT);
	return(-1);
    }

    /* Select ADDRESS Port */
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_ADDR);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_CMD1_ADDR);               /* */

    /* Select CMD Port */
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_CTRL);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_CMD1_INIT);               /* */

    /* Check to see if the ID register is accessible... */
    mco_7162SetAddrReg(mgbase, device, MCO_ADV7162_ID_ADDR);	/* Select ID */

    MCO_SETINDEX(mgbase, device, MCO_ADV7162_CTRL);
    MCO_IO_READ(mgbase, id);

    if (id != DAC7162_ID) {
	msg_printf(VRB, "Invalid 7162 ID: %d != %d\n", id, DAC7162_ID);
	return(-2);
    }


    /* Set for 24 bit true color */
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD2_ADDR, MCO_ADV7162_CMD2_INIT);

    /* 4:1 mux */
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD3_ADDR, MCO_ADV7162_CMD3_INIT);

    /* Disable signature, set sync recognition for red and blue */
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, MCO_ADV7162_CMD4_INIT);

    /* Disable Cursors... */
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CURS_ADDR, MCO_ADV7162_CURS_DISABLE);

    if (device == MCO_ADV7162A) {
	/* Enable the Internal PLL */
	mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD5_ADDR, MCO_ADV7162_CMD5_INIT);

    }
    /* Set up Gamma Tables to a ramp for now */

    for (i=0; i<256; i++) {
	red[i].channel = ((i << 2) | 0x02);
	green[i].channel = red[i].channel;
	blue[i].channel = red[i].channel;
    }

    mco_WriteDac7162(device, 0x00, red, green, blue, 0x100, 0x3ff);

    if (device == MCO_ADV7162A) {
	/* Set 7162 PLL registers */
#ifdef NOTYET
	msg_printf(VRB,"Probe7162: Writing 0x0F to PLL R Reg\n");
#endif /* NOTYET */
	mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_R_ADDR,   0xf);
#ifdef NOTYET
	msg_printf(VRB,"Probe7162: Writing 0x0B to PLL V Reg\n");
#endif /* NOTYET */
	mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_V_ADDR,   0xb);
#ifdef NOTYET
	msg_printf(VRB,"Probe7162: Writing 0xC1 to PLL Command Reg\n");
#endif /* NOTYET */
	mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_CTL_ADDR, 0xc1);
    }

    return(0);
}

int
mco_Dac7162Probe(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;
	char devicename[40];

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

#ifdef IP30
	sprintf(devicename, "A");
	device = MCO_ADV7162A;
#endif	/* IP30 */

	errors = mco_probe7162(device); 
	if (errors) {
		msg_printf(ERR,"MCO 7162%c Probe Failed\n", devicename);
	}
	else {
		msg_printf(VRB,"MCO 7162%c Probe Completed\n", devicename);
	}
	return (errors);
}

/************************************************************************
 * _mco_send7162LUT will send out a 7162 LUT and verify that it was 
 * written correctly. *
 * This function is currently limited in that it will set red,
 * green, and blue values to the same thing.
 ************************************************************************/

int 
_mco_send7162LUT(uchar_t device, /* Specifies MCO_ADV7162A or MCO_ADV7162B */
	    unsigned short pixels[256][3]) 
{
    int i;
    unsigned short tmp1, tmp2, retval;

#ifdef NOTYET
    for (i=0; i<256; i++) {
	msg_printf(VRB,"mco_send7162: R = 0x%x, G = 0x%x, B = 0x%x\n",
	(pixels[i][0] & 0x3fc), (pixels[i][1] & 0x3fc), (pixels[i][2] & 0x3fc)); 
    }
#endif /* NOTYET */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrReg(mgbase, device, 0x00);

    /* Set up Gamma Tables */
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_LUT);

    for (i = 0; i < 256 ; i++) {                /* Now send the LUT */
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	MCO_IO_WRITE(mgbase, (pixels[i][0] & 0x03fc) >> 2 ); /* Red */
	MCO_IO_WRITE(mgbase, pixels[i][0] & 0x0003);
	MCO_IO_WRITE(mgbase, (pixels[i][1] & 0x03fc) >> 2 ); /* Green */
	MCO_IO_WRITE(mgbase, pixels[i][1] & 0x00ff);
	MCO_IO_WRITE(mgbase, (pixels[i][2] & 0x03fc) >> 2 ); /* Blue */
	MCO_IO_WRITE(mgbase, pixels[i][2] & 0x0003);
    }

    /* Read back the Gamma Tables to a be sure. */

    /* Set the 7162 to access the LUT */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrReg(mgbase, device, 0x00);  

    MCO_SETINDEX(mgbase, device, MCO_ADV7162_LUT);

    for (i = 0; i < 256 ; i++) {
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	MCO_IO_READ(mgbase, tmp1);     /* First check the RED data */
	MCO_IO_READ(mgbase, tmp2);
	retval = ((tmp1 & 0xff) << 2) + (tmp2 & 0x03);

	if (retval != pixels[i][0]) {
	    msg_printf(ERR, "LUT Red Verify Failed. (Addr %d, Wrote %d, Read = %d)\n", 
		 i, pixels[i][0], retval);
	    return(-6);
	}
         
	MCO_IO_READ(mgbase, tmp1);     /* Now check the GREEN data */
	MCO_IO_READ(mgbase, tmp2);
	retval = ((tmp1 & 0xff) << 2) + (tmp2 & 0x03);

	if (retval != pixels[i][1]) {
	    msg_printf(ERR, "LUT Green Verify Failed. (Addr %d, Wrote %d, Read = %d)\n", 
		 i, pixels[i][1], retval);
	    return(-6);
	}

	MCO_IO_READ(mgbase, tmp1);     /* Now check the BLUE data */
	MCO_IO_READ(mgbase, tmp2);
	retval = ((tmp1 & 0xff) << 2) + (tmp2 & 0x03);

	if (retval != pixels[i][2]) {
	    msg_printf(ERR, "LUT Blue Verify Failed. (Addr %d, Wrote %d, Read = %d)\n", 
		 i, pixels[i][2], retval);
	    return(-6);
	}
    }

    return(0);
}

void
mco_WriteDac7162(uchar_t device, /* Specifies MCO_ADV7162A or MCO_ADV7162B */
		__uint32_t addr, 	/* Color Palette or Overlay Address to use */
		GammaSize *red,
		GammaSize *green, 
	 	GammaSize *blue,
		__uint32_t length, 	/* # of RGB values to write */
		ushort_t mask)		/* RGB data mask */
{
	ushort_t i;
	ushort_t saddr = addr;

	mco_7162SetAddrReg(mgbase, device, saddr);

	/* Set MCO Index Register to point to 7162 LUT */
	MCO_SETINDEX(mgbase, device, MCO_ADV7162_LUT);

	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	for (i = 0; i < length ; i++) {
	    mco_7162SetRGB(mgbase, (red[i].channel & mask), (green[i].channel & mask), (blue[i].channel &mask) );
#ifdef NOTYET
	    msg_printf(DBG, "Writing i = 0x%x, r= 0x%x g = 0x%x b = 0x%x\n" ,
	    i, (red[i].channel & mask), (green[i].channel & mask), (blue[i].channel &mask) );
#endif /* NOTYET */
	    if (i & 0x3) {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    }
	}
}

void
mco_ReadDac7162( uchar_t device, /* Specifies MCO_ADV7162A or MCO_ADV7162B */
		__uint32_t addr, 
		GammaSize *red, 
		GammaSize *green, 
		GammaSize *blue,  
		__uint32_t length)	/* # of RGB values to write */
{
	ushort_t i;
	ushort_t saddr = addr;
	unsigned char _low, _high;		\

	mco_7162SetAddrReg(mgbase, device, saddr);

	/* Set MCO Index Register to point to 7162 LUT */
	MCO_SETINDEX(mgbase, device, MCO_ADV7162_LUT);

	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	for(i=0; i< length; i++) {
	    MCO_IO_READ(mgbase, _high);
	    MCO_IO_READ(mgbase, _low);
	    red[i].channel = ((_high & 0xff) << 2) | (_low & 0x3);
	    MCO_IO_READ(mgbase, _high);
	    MCO_IO_READ(mgbase, _low);
	    green[i].channel = ((_high & 0xff) << 2) | (_low & 0x3);
	    MCO_IO_READ(mgbase, _high);
	    MCO_IO_READ(mgbase, _low);
	    blue[i].channel = ((_high & 0xff) << 2) | (_low & 0x3);
#ifdef NOTYET
	    msg_printf(DBG, "Read i = %x, r= %x g = %x b = %x\n",
	    red[i].channel, green[i].channel, blue[i].channel);
#endif /* NOTYET */
	    if (i & 0x3)
	    {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    }
	}
}

/*ARGSUSED2*/
__uint32_t
mco_Dac7162Compare(char *TestName, 
		uchar_t device, 
		GammaSize *exp,  
		GammaSize *rcv,  
	 	__uint32_t length,  
		ushort_t mask)
{
	 ushort_t i;
	 ushort_t err = 0;

	for(i=0; i<length; i++) {
	    if (exp[i].channel != rcv[i].channel ) {
		++err;
		msg_printf(ERR, "%s i=%d, exp= 0x%x, rcv=0x%x \n",
		TestName,i,exp[i].channel, rcv[i].channel );
	    }
#ifdef NOTYET
	    msg_printf(DBG, "%s i=%d, exp= 0x%x, rcv=0x%x \n",
	    TestName, i, exp[i].channel, rcv[i].channel );
#endif /* NOTYET */
	}
	return (err);
}

__uint32_t 
mco_7162ClrPaletteAddrUniqTest(uchar_t device) 
{
	ushort_t i;
	__uint32_t errors = 0;
	
	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "Color Palette Address Uniquness Test\n");
#endif /* NOTYET */

	/* First Check the lower 8 Bits */
	for (i = 0; i < 256 ; i++){
		Red[i].channel = i; Green[i].channel = i; Blue[i].channel = i;
	}
	
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );

#ifdef NOTYET
	msg_printf(DBG, "Next: Check the Lower Bits (7:0)\n");
#endif /* NOTYET */
	mco_WriteDac7162(device, 0x00, Red,    Green,    Blue,    0xff, 0x3ff);
	mco_ReadDac7162(device, 0x00,  RedRcv, GreenRcv, BlueRcv, 0xff);

	errors  = mco_Dac7162Compare("Red Palette Addr Uniq Test", device, Red,   RedRcv,   0xff, 0x3ff);
	errors += mco_Dac7162Compare("Grn Palette Addr Uniq Test", device, Green, GreenRcv, 0xff, 0x3ff);
	errors += mco_Dac7162Compare("Blu Palette Addr Uniq Test", device, Blue,  BlueRcv,  0xff, 0x3ff);

#ifdef NOTYET
	msg_printf(DBG, "Next: Check the Upper Bits (9:8)\n");
#endif /* NOTYET */
	for (i = 0; i < 256 ; i++){
		Red[i].channel = ((i << 8) | 0x55) ; Green[i].channel = ((i << 8) | 0xaa); Blue[i].channel = ((i << 8) | 0xcc);
	}
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );
	mco_WriteDac7162(device, 0x00, Red,    Green,    Blue,    0x4, 0x3ff);
	mco_ReadDac7162(device, 0x00,  RedRcv, GreenRcv, BlueRcv, 0x4);
	errors  = mco_Dac7162Compare("Red Palette Addr Uniq Test", device, Red,   RedRcv,   0x4, 0x3ff);
	errors += mco_Dac7162Compare("Grn Palette Addr Uniq Test", device, Green, GreenRcv, 0x4, 0x3ff);
	errors += mco_Dac7162Compare("Blu Palette Addr Uniq Test", device, Blue,  BlueRcv,  0x4, 0x3ff);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_CLRPAL_ADDRUNIQ_TEST]), errors);
	
}

int
mco_Dac7162ClrPaletteAddrUniqTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

#ifdef IP30
	device =  MCO_ADV7162A;
#endif	/* IP30 */
	
	errors = mco_7162ClrPaletteAddrUniqTest(device); 
	return (errors);
}

__uint32_t 
mco_7162ClrPaletteWalkBitTest(uchar_t device) 
{
	 ushort_t i;
	 __uint32_t errors = 0;
	 __uint32_t length = 256;

#ifdef NOTYET
	MGRAS_GFX_CHECK();
#endif /* NOTYET */

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "Color Palette Walking Bit: DataBus Test\n");
#endif /* NOTYET */
	for (i = 0; i < length ; i++){
		Red[i].channel = Green[i].channel = Blue[i].channel = dac_walking_patrn[i%20];
	}

	bzero(RedRcv,   sizeof(RedRcv) );
	  bzero(BlueRcv,  sizeof(BlueRcv) );
	  bzero(GreenRcv, sizeof(GreenRcv) );

	mco_WriteDac7162(device, 0x00, Red,   Green,   Blue,    length, 0x3ff);
	mco_ReadDac7162(device, 0x00,  RedRcv, GreenRcv, BlueRcv, length);

	errors  = mco_Dac7162Compare("Red Palette Data WalkBit Test", device, Red,   RedRcv,   length,	0x3ff);
	errors += mco_Dac7162Compare("Grn Palette Data WalkBit Test", device, Green, GreenRcv, length,	0x3ff);
	errors += mco_Dac7162Compare("Blu Palette Data WalkBit Test", device, Blue,  BlueRcv,  length,	0x3ff);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_CLRPAL_WALK_BIT_TEST]), errors);
}

int
mco_Dac7162ClrPaletteWalkBitTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

#ifdef IP30
	device =  MCO_ADV7162A;
#endif	/* IP30 */
	
	errors = mco_7162ClrPaletteWalkBitTest(device); 
	return (errors);
}

__uint32_t 
mco_7162ClrPalettePatrnTest(uchar_t device) 
{
	 ushort_t i;
	 ushort_t loop;
	 __uint32_t errors;
	 ushort_t *PatrnPtr;

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "Color Pallette Pattern Test\n");
#endif /* NOTYET */
	for(loop=0; loop<7; loop++) {
		PatrnPtr = dac_patrn + loop;
#ifdef NOTYET
		msg_printf(DBG, "Loop= %d, Filling GAMMA RAM with:\n", loop);
		msg_printf(DBG, "%x %x %x %x %x %x %x \n" ,
			*(PatrnPtr),    *(PatrnPtr+1),  *(PatrnPtr+2),
			*(PatrnPtr+3),  *(PatrnPtr+4),  *(PatrnPtr+5), *(PatrnPtr+6) );
#endif /* NOTYET */

		for (i = 0; i < 256 ; i++){
			Red[i].channel = Green[i].channel = Blue[i].channel = *(PatrnPtr + (i%7) ) ;
		}

		bzero(RedRcv,   sizeof(RedRcv) );
		bzero(BlueRcv,  sizeof(BlueRcv) );
		bzero(GreenRcv, sizeof(GreenRcv) );
	
		mco_WriteDac7162(device, 0x00, Red, Green, Blue, 0xff, 0x3ff);
		mco_ReadDac7162(device, 0x00,  RedRcv, GreenRcv, BlueRcv, 0xff);
	
		errors  = mco_Dac7162Compare("Red Palette Patrn Test", device, Red,   RedRcv,   0xff,	0x3ff);
		errors += mco_Dac7162Compare("Grn Palette Patrn Test", device, Green, GreenRcv, 0xff,	0x3ff);
		errors += mco_Dac7162Compare("Blu Palette Patrn Test", device, Blue,  BlueRcv,  0xff,	0x3ff);
	}

	
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_CLRPAL_PATRN_TEST]), errors);
}

int
mco_Dac7162ClrPalettePatrnTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

#ifdef IP30
	device =  MCO_ADV7162A;
#endif	/* IP30 */
	
	errors = mco_7162ClrPalettePatrnTest(device); 
	return (errors);
}


__uint32_t 
_mco_Dac7162InternalRegisterTest(char *str, 
				uchar_t device, 
				__uint32_t RegId,  
				__uint32_t BitMask)
{
	 __uint32_t errors = 0;
	 ushort_t i, rcv;
	 ushort_t orig;

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	/* Save the current value before the test */
	mco_7162GetAddrCmd(mgbase, device, RegId, &orig);
	
	for(i=0; i < sizeof(dac_walking_patrn); i++) {
		mco_7162SetAddrCmd(mgbase, device, RegId, (dac_walking_patrn[i]& 0xff) );
		rcv = 0xbeef;
		CORRUPT_DCB_BUS(mgbase);
		mco_7162GetAddrCmd(mgbase, device, RegId, &rcv);
		COMPARE(str, RegId, (dac_walking_patrn[i] & 0xff), rcv, BitMask,errors);
	}

	/* Restore the original value before leaving */
	mco_7162SetAddrCmd(mgbase, device, RegId, orig);

	errors += _mco_Dac7162Reset(device);

	return (errors);
}

__uint32_t 
_mco_Dac7162CtrlRegTest(uchar_t device) 
{
	 ushort_t i;
	 ushort_t rcv;
	 ushort_t exp;
	 __uint32_t errors = 0;
	
	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "DAC Control Register Test\n");
#endif /* NOTYET */
	for(i=0; i<(sizeof(_MCODAC_Internal_Reg)/sizeof(_MCODAC_Internal_Reg[0])); i++) {
	    msg_printf(DBG, "Testing Internal Register Test %s 0x%x\n" ,_MCODAC_Internal_Reg[i].str, _MCODAC_Internal_Reg[i].RegName);
	    switch (_MCODAC_Internal_Reg[i].RegName) {
		case MCO_ADV7162_ID_ADDR:
			rcv = 0xdead;
			exp = DAC7162_ID;
			CORRUPT_DCB_BUS(mgbase);
			mco_7162GetAddrCmd(mgbase, device, _MCODAC_Internal_Reg[i].RegName, &rcv);
			COMPARE(_MCODAC_Internal_Reg[i].str, _MCODAC_Internal_Reg[i].RegName, exp, rcv, 0xff,errors);
			break;
		case MCO_ADV7162_REV_ADDR:
			rcv = 0xbeef;
			exp = DAC7162_REVISION;
			CORRUPT_DCB_BUS(mgbase);
			mco_7162GetAddrCmd(mgbase, device, _MCODAC_Internal_Reg[i].RegName, &rcv);
			if (exp != rcv) {
			    if (exp == DAC7162_REVISION_OLD)  {
				msg_printf(VRB, "WARNING -- OLDER DAC in Use!\n");
				exp = DAC7162_REVISION_OLD;
			    }
			}
			COMPARE(_MCODAC_Internal_Reg[i].str, _MCODAC_Internal_Reg[i].RegName, exp, rcv, 0xff,errors);
			break;

		case MCO_ADV7162_PIXMASK_ADDR:
#ifdef YOU_SHOULD_NOT_CHECK_THIS_REGISTER
		case MCO_ADV7162_CMD1_ADDR:
		case MCO_ADV7162_CMD2_ADDR:
		case MCO_ADV7162_CMD3_ADDR:
		case MCO_ADV7162_CMD4_ADDR:
#endif
			errors += _mco_Dac7162InternalRegisterTest(_MCODAC_Internal_Reg[i].str, device, _MCODAC_Internal_Reg[i].RegName, _MCODAC_Internal_Reg[i].mask);
			break;

	    }
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_CNTRL_REG_TEST]), errors);
}

int
mco_Dac7162CtrlRegTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

#ifdef IP30
	device =  MCO_ADV7162A;
#endif	/* IP30 */
	
	errors = _mco_Dac7162CtrlRegTest(device);
	return (errors);
}

__uint32_t 
_mco_Dac7162ModeRegTest(uchar_t device) 
{
	 ushort_t i;
	 ushort_t orig;
	 ushort_t rcv;
	 __uint32_t errors = 0;

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "DAC Mode Register Test\n");
#endif /* NOTYET */
	mco_7162GetModeReg(mgbase, device, &orig );	/* Get the mode register */
	for(i=0; i < sizeof(dac_walking_patrn); i++) {
		mco_7162SetModeReg(mgbase, device, (dac_walking_patrn[i] & 0xff) );
		CORRUPT_DCB_BUS(mgbase);
		mco_7162GetModeReg(mgbase, device, &rcv );
		COMPARE("Dac Mode Reg Test" ,0, dac_walking_patrn[i], rcv, 0xff,errors);
	}
	mco_7162SetModeReg(mgbase, device, orig);		/* Set the original value */
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_MODE_REG_TEST]), errors);
}

int
mco_Dac7162ModeRegTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

#ifdef IP30
	device =  MCO_ADV7162A;
#endif	/* IP30 */
	
	errors = _mco_Dac7162ModeRegTest(device);
	return (errors);
}

__uint32_t 
_mco_Dac7162AddrRegTest(uchar_t device) 
{
	 ushort_t i 	= 0;
	 __uint32_t errors 	= 0;
	 ushort_t rcv 	= 0xbeef;

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

#ifdef NOTYET
	msg_printf(VRB, "DAC Address Register Test\n");
#endif /* NOTYET */
	for(i=0; i < sizeof(dac_patrn); i++) {
		mco_7162SetAddrReg(mgbase, device, dac_patrn[i]);
		mco_7162GetAddrReg(mgbase, device, &rcv);
		COMPARE("Dac Addr Reg Test (8-Bit)" ,0,dac_patrn[i], rcv,0xff,errors);
	}

	for(i=0; i < sizeof(dac_patrn); i++) {
		mco_7162SetAddrReg(mgbase, device, dac_patrn[i]);
		mco_7162GetAddrReg(mgbase, device, &rcv);
		COMPARE("Dac Addr Reg Test (16-Bit)" ,0,dac_patrn[i], rcv,0xff,errors);
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_7162_ADDR_REG_TEST]), errors);
}

int
mco_Dac7162AddrRegTest(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV7162A;
	char devicename[40];

#ifndef IP30
	if (argc != 2) {
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
		return -1;
	}
#endif	/* IP30 */

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}
	
#ifdef IP30
	device = MCO_ADV7162A;
#endif	/* IP30 */

	errors = _mco_Dac7162AddrRegTest(device);
	return (errors);
}

int
mco_Peek7162ClrPalette(int argc, char **argv)
{
	__uint32_t addr 	= 0x00;
	__uint32_t length	= 0x1;
	uchar_t device = MCO_ADV7162A;
	GammaSize _Red, _Green, _Blue;
	char devicename[20];

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	if (argc != 3) {
#ifndef IP30
	    msg_printf(SUM, "usage: %s Device Addr (where Device is A or B)\n",
	    argv[0]);
#else
	    msg_printf(SUM, "usage: %s A Addr\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	/* Get Address to read */
	atohex(argv[2], (int *)&addr);

	mco_ReadDac7162(device, addr, &_Red, &_Green, &_Blue, length);
	msg_printf(VRB, "Reading Dac7162%s Palette Addr 0x%x returns  Red 0x%x Green 0x%x Blue 0x%x\n",
	devicename, addr, _Red.channel, _Green.channel, _Blue.channel);

	return 0;
}

int
mco_Poke7162ClrPalette(int argc, char **argv)
{
	__uint32_t addr 	= 0x00;
	__uint32_t RedVal 	= 0xff;
	__uint32_t BluVal	= 0xff;
	__uint32_t GrnVal	= 0xff;
	__uint32_t length	= 0x1;
	__uint32_t mask 	= 0x3ff;
	uchar_t device = MCO_ADV7162A;
	GammaSize _Red, _Green, _Blue;
	char devicename[20];

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	if (argc != 6) {
#ifndef IP30
	    msg_printf(SUM, "usage: %s Device Addr RedVal GreenVal BlueVal\n" , argv[0]);
	    msg_printf(SUM, "(where Device is A or B)\n");
#else
	    msg_printf(SUM, "usage: %s A Addr RedVal GreenVal BlueVal\n" , argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	atohex(argv[2], (int *)&addr);
	atohex(argv[3], (int *)&RedVal);
	atohex(argv[4], (int *)&GrnVal);
	atohex(argv[5], (int *)&BluVal);

	_Red.channel = RedVal; _Green.channel = GrnVal; _Blue.channel = BluVal;
	mco_WriteDac7162(device, addr, &_Red, &_Green, &_Blue, length, mask);
	msg_printf(VRB, "Writing Dac7162%s Palette Addr 0x%x with Red 0x%x Green 0x%x Blue 0x%x\n",
	devicename, addr, RedVal, GrnVal, BluVal);

	return 0;
}

int
mco_Poke7162Ctrl(int argc, char **argv)
{
	__uint32_t value = 0xdeadbeef;
	ushort_t addr;
	uchar_t device = MCO_ADV7162A;
	char devicename[20];

	if (argc != 3) {
#ifndef IP30
		msg_printf(SUM, "usage: %s Device AddrValue (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage: %s A AddrValue\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	atohex(argv[2], (int *)&value);
	mco_7162GetAddrReg(mgbase, device, &addr);
	msg_printf(VRB, "Writing Dac7162%s Ctrl Reg 0x%x with Value 0x%x\n",
	devicename, addr, value);
	mco_7162SetCtrl(mgbase, device, value);
	return 0;
}

int
mco_Peek7162Ctrl(int argc, char **argv)
{
	ushort_t addr = 0xdead;
	uchar_t Readdata;
	uchar_t device;
	char devicename[20];

	if (argc != 2) {
#ifndef IP30
		msg_printf(SUM, "usage: %s Device (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage: %s A\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	mco_7162GetAddrReg(mgbase, device, &addr);
	msg_printf(VRB, "%s: Dac7162%s Addr Reg = 0x%x\n", 
	argv[0], devicename, addr);
	mco_7162GetCtrl(mgbase, device, &Readdata);
	msg_printf(VRB, "%s: Dac7162%s Ctrl Reg Adrress 0x%x = 0x%x\n", 
	argv[0], devicename, addr, Readdata);

	return 0;
}

int
mco_Poke7162Addr(int argc, char **argv)
{
	 __uint32_t AddrValue = 0xdeadbeef;
	 uchar_t device = MCO_ADV7162A;
	char devicename[20];

	if (argc != 3) {
#ifndef IP30
		msg_printf(SUM, "usage: %s Device AddrValue (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage: %s A AddrValue\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	atohex(argv[2], (int *)&AddrValue);
	mco_7162SetAddrReg(mgbase, device, AddrValue); 
	msg_printf(VRB, "%s: Wrote Dac7162%s Addr Reg with 0x%x\n", 
	argv[0], devicename, AddrValue);
	return 0;
}

int
mco_Peek7162Addr(int argc, char **argv)
{
	 ushort_t rcv = 0xbeef;
	 uchar_t device = MCO_ADV7162A;
	char devicename[20];

	if (argc != 2) {
#ifndef IP30
		msg_printf(SUM, "usage: %s Device (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage: %s A\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162A;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	mco_7162GetAddrReg(mgbase, device, &rcv);
	msg_printf(VRB, "%s: Dac7162%s Addr Reg returns 0x%x\n", 
	argv[0], devicename, rcv);
	return 0;
}

#ifdef NOTYET
int
mco_7162PatrnLoad( ushort_t PatrnType)
{
	/* Load Various Patterns to Test Out the DAC more extensively */
	msg_printf(VRB, "7162PatrnLoad(): Not implemented yet\n");
}
#endif /* NOTYET */


int
_mco_7162PLLInit( __uint32_t mcoTimingSelect)
{
    uchar_t device = MCO_ADV7162A;  /* PLL only works on 7162 channel A */

#ifdef NOTYET
      mgbase->bdvers.bdvers0 = 0x0;
#endif /* NOTYET */
    switch(mcoTimingSelect) {
    /* PLL Initialize */
	case    _MINI_640x480_60 :
	    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_R_ADDR,   0x7);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_V_ADDR,   0x7);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_CTL_ADDR, 0x61);
	    	break;
	case    _MINI_800x600_60 :
	    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_R_ADDR,   0xb);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_V_ADDR,   0x8);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_CTL_ADDR, 0x11);
	     	break;
	default:
	    msg_printf(VRB, "_mco_Dac7162PLLInit, mcoTimingSelect = _MINI_800x600_60\n");
	    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_R_ADDR,   0xb);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_V_ADDR,   0x8);
	    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_CTL_ADDR, 0x11);
	    break;
    }
    us_delay(30);

    /* reset VC2 */
    mco_set_dcbctrl(mgbase, 4);
    MCO_VC2_SETREG(mgbase, VC2_CONFIG, 0);

    MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

    msg_printf(VRB, "_mco_Dac7162PLLInit Completed.\n");

    return 0;
}

__uint32_t
_mco_Dac7162Reset(uchar_t device)
{
    ushort_t rcv, IDReg, Dac7162Rev;
    ushort_t DisableCurs, PixMask, cmd1, cmd2, cmd3, cmd4;
    __uint32_t errors = 0;
    __uint32_t OldDac = 0;

    /* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
    mco_set_dcbctrl(mgbase, 2);

    /* Set MCO Index Register to point to 7162 MR1 */
    /* PS0, PS1 = 0; No Calib; Normal Op.; 8-bit MPU Bus; 10-bit DAC; RESET */
    /* DAC Reset = 1 */
    mco_7162SetModeReg(mgbase, device, MCO_ADV7162_MODE_INIT);
    /* Assert -  DAC Reset */
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT & ~MCO_ADV7162_MODE_RESET_BIT); 
    /* Release - DAC Reset & set Dac MPU Port to 8 Bit */
    MCO_IO_WRITE(mgbase, MCO_ADV7162_MODE_INIT);

    /* Addr-Lo  ONLY - DO NOT WRITE TO THE UPPER Address byte YET! */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_SETINDEX(mgbase, device, MCO_ADV7162_ADDR);
    MCO_IO_WRITE(mgbase, MCO_ADV7162_CMD1_ADDR);

    /* Enables Access to Addr Hi, Single Write only */
    mco_7162SetCtrl(mgbase, device, 0x89); 
    
    /* Must be set on for PLL  - at power */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD5_ADDR, 0x40); 

    rcv = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrReg(mgbase, device, 0); 	/* Addr. Inter. reg = 0 */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrReg(mgbase, device, &rcv);
    if ((rcv & 0xff) != 0) {
    	++errors;
    	msg_printf(ERR, "mco_Dac7162Reset Addr Reg 0 Exp = 0x0 Rev= 0x%x\n",rcv&0xff);
    }

    /* DAC7162_ID */
    IDReg = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_ID_ADDR, &IDReg);

    if ((IDReg & 0xff) != DAC7162_ID)  {
    	if ((IDReg & 0xff) != DAC7162_ID_OLD) {
    	    ++errors;
    	    msg_printf(ERR, "mco_Dac7162Reset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MCO_ADV7162_ID_ADDR, DAC7162_ID, (IDReg & 0xff));
    	}
    }

    /* DAC7162_REV */
    Dac7162Rev = 0x0;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_REV_ADDR, &Dac7162Rev);
    if (OldDac) {
    	if ((Dac7162Rev & 0xff) != DAC7162_REVISION_OLD) {
    	    ++errors;
    	    msg_printf(ERR, "mco_Dac7162Reset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MCO_ADV7162_REV_ADDR, DAC7162_REVISION, (Dac7162Rev & 0xff));
    	}
    } else {
    	if ((Dac7162Rev & 0xff) != DAC7162_REVISION) {
    	    ++errors;
    	    msg_printf(ERR, "mco_Dac7162Reset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MCO_ADV7162_REV_ADDR, DAC7162_REVISION, (Dac7162Rev & 0xff));
    	}
    }

    cmd1 = 0x89;
    cmd2 = 0xe4;
    cmd3 = 0xc0;
    cmd4 = 0x6;
    PixMask = 0xff;
    DisableCurs = 0x1;

    /* RGB */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_PIXMASK_ADDR, 0xff); 

    /* Bypass Mode */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_CMD1_ADDR, cmd1);    

    /* 7.5IRE? */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_CMD2_ADDR, cmd2);  

    /* 4:1 pix. mux. */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_CMD3_ADDR, cmd3);  

    /* Sync set-up */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_CMD4_ADDR, cmd4);  

    /* Disable Cursor */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase,device, MCO_ADV7162_CURS_ADDR, DisableCurs);


    /* Read Back & verify to see if we have initialized the dac properly */
    rcv = 0xdead;
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_PIXMASK_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_PIXMASK_ADDR", 0, PixMask, rcv, 0xff, errors);

    rcv = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_CMD1_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_CMD1_ADDR", 0, cmd1, rcv, 0xff, errors);

    rcv = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_CMD2_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_CMD2_ADDR", 0, cmd2, rcv, 0xff, errors);

    rcv = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_CMD3_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_CMD3_ADDR", 0, cmd3, rcv, 0xff, errors);

    rcv = 0xdead;
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase, device, MCO_ADV7162_CMD4_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_CMD4_ADDR", 0, cmd4, rcv, 0xff, errors);

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162GetAddrCmd(mgbase,device, MCO_ADV7162_CURS_ADDR, &rcv);
    COMPARE("DAC RESET :- MCO_ADV7162_CURS_ADDR", 0, DisableCurs, rcv, 0xff, errors);

#ifdef DOES_NOT_WORK
    GammaTableLoad(LINEAR_RAMP);
#endif /* DOES_NOT_WORK */

    return (errors);
}

int
mco_Dac7162Reset(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV7162A;

	if (argc != 2) {
#ifndef IP30
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage %s A\n", argv[0]);
	
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	errors = _mco_Dac7162Reset(device);
	return (errors);
}

__uint32_t
_mco_Dac7162setpll(uchar_t device, uchar_t pllr, uchar_t pllv, uchar_t pllctl, int pllenable)
{
    ushort_t cmd5, cmd5_rd;
    __uint32_t errors = 0;
#ifdef NOTYET
    uchar_t pllr_rd, pllv_rd;
#endif
    uchar_t pllctl_rd, pllctl_wr;

    cmd5 = 0x40;	/* Internal PLL */
#ifdef NOTYET
    pllr_rd = 0;
    pllv_rd = 0;
#endif /* NOTYET */
    pllctl_rd = 0;
    pllctl_wr = pllctl;

    msg_printf(VRB,"mco_7162setpll: pllr=0x%x, pllv=0x%x, pllctl=0x%x, enable=%d\n",
    pllr, pllv, pllctl, pllenable);

    /* Internal PLL */
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_R_ADDR, pllr);
    /* Check that PLLR register was set correctly */
#ifdef NOTYET
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllr_rd);
    if ((pllr_rd & 0x7f) != pllr) {
	msg_printf(VRB, "PLLR Write Failed. (Rd 0x%x != Exp 0x%x)\n", pllr_rd, pllr);
	errors++;
    }
#endif /* NOTYET */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_V_ADDR, pllv);
    /* Check that pllv register was set correctly */
#ifdef NOTYET
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllv_rd);
    if ((pllv_rd & 0x7f) != pllv) {
	msg_printf(VRB, "PLLV Write Failed. (Rd 0x%x != Exp 0x%x)\n", pllv_rd, pllv);
	errors++;
    }
#endif /* NOTYET */

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    if (pllenable) {
	/* Enable PLL */
	pllctl_wr = pllctl | 0x1;
    }
    else {
	/* Disable PLL */
	pllctl_wr = pllctl & 0xFE;
    }
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_PLL_CTL_ADDR, pllctl_wr);
    /* Check that pllctl register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, pllctl_rd);
    if (pllctl_rd != pllctl_wr) {
	msg_printf(VRB, "PLLCTL Write Failed. (Rd 0x%x != Exp 0x%x)\n", 
	pllctl_rd, pllctl_wr);
	errors++;
    }

    /* Enable the Internal PLL */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    mco_7162SetAddrCmd(mgbase, device, MCO_ADV7162_CMD5_ADDR, cmd5);
#ifdef NOTYET
    msg_printf(VRB,"mco_init7162: Dac7162 Cmd5 Reg = 0x%x\n", cmd5);
#endif /* NOTYET */
    /* Check that Cmd5 register was set correctly */
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    MCO_IO_READ(mgbase, cmd5_rd);
    if (cmd5_rd != cmd5) {
	msg_printf(VRB, "CMD5 Write Failed. (Rd 0x%x != Exp 0x%x)\n", cmd5_rd, cmd5);
	errors++;
    }

    return (errors);
}

int
mco_Dac7162SetPLL(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV7162A;
	uchar_t pllr, pllv, pllctl;
	uchar_t enable = 1;

	pllr = 0;
	pllv = 0;
	pllctl = 0x0;

	if (argc != 5) {
#ifndef IP30
	    msg_printf(SUM, "usage %s Device PLLR PLLV PLLCTL\n", argv[0]);
	    msg_printf(SUM, "(where Device is A or B)\n");
#else
	    msg_printf(SUM, "usage %s A PLLR PLLV PLLCTL\n", argv[0]);
#endif	/* IP30 */
	
	    msg_printf(SUM, "(where PLLR, PLLV, PLLCTL are 7162 PLL register values)\n");
	    return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		break;
	    default:
		device =  MCO_ADV7162A;
		break;
	}
	pllr = atoi(argv[2]);
	pllv = atoi(argv[3]);
	pllctl = atoi(argv[4]);

	if (!pllr || !pllv) {
	    /* Disable pll */
	    enable = 0;
	}

	msg_printf(VRB, "mco_7162setpll: pllr = 0x%x, pllv = 0x%x, pllctl = 0x%x, enable = %d\n",
	pllr, pllv, pllctl, enable);

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	errors = _mco_Dac7162setpll(device, pllr, pllv, pllctl, enable);
	return (errors);
}



int
mco_Poke7162Mode(int argc, char **argv)
{
	 __uint32_t DacMode = 0xdeadbeef;
	 uchar_t device = MCO_ADV7162A;  /* default to channel A */
	char devicename[20];

	if (argc != 3) {
#ifndef IP30
		msg_printf(SUM, "usage %s Device Value (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage %s A Value\n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}

	atohex(argv[2], (int *)&DacMode);

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "Writing Dac7162%s Mode Register with 0x%x\n",
	devicename, DacMode);
	mco_7162SetModeReg(mgbase, device, DacMode);

	return 0;
}

int
mco_Peek7162Mode(int argc, char **argv)
{
	ushort_t DacMode = 0xdead;
	uchar_t device = MCO_ADV7162A;
	char devicename[20];

	if (argc != 2) {
#ifndef IP30
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
		argv[0]);
#else
		msg_printf(SUM, "usage %s A \n", argv[0]);
#endif	/* IP30 */
	
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	    case 'B':
		device =  MCO_ADV7162B;
		sprintf(devicename, "B");
		break;
	    default:
		device =  MCO_ADV7162A;
		sprintf(devicename, "A");
		break;
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	mco_7162GetModeReg(mgbase, device, &DacMode);
	msg_printf(VRB, "Reading Dac7162%s Mode Register returns 0x%x\n",
	devicename, DacMode);

	return 0;
}
