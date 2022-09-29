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
 *  VC2 Diagnostics.
 *	SRAM Diagnostics :- 
 *	 	Full Memory Pattern Test.
 *	       	Walking Ones & Zeros :- Address Bus Test.
 *		Address Uniqueness Test.
 *		Walking Ones & Zeros :- Data Bus Test.
 *	CURSOR Tests :-
 *		Blackout Display Test.
 *		Cursor Update Test.
 *		VC2 Reset & Check.
 *		Cursor Mode Test.
 *
 *  $Revision: 1.22 $
 */


#include "ide_msg.h"
#include <libsc.h>
#include <uif.h>

#include <math.h>
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mco_diag.h"
#include "parser.h"

extern mgras_hw *mgbase;
extern __uint32_t XMAX;
extern __uint32_t YMAX;

/* Forward and external references */
extern int _mco_7162PLLInit( __uint32_t mcoTimingSelect);
extern int _mco_Dac7162setpll(uchar_t device, uchar_t pllr, uchar_t pllv, uchar_t pllctl, int pllenable);

extern int _mco_SetVOF(__uint32_t TimingSelect, int flag);
extern __uint32_t _mco_Probe(void);
#ifdef IP30
extern __uint32_t _sr_mco_Probe(void);
extern int mco_probe7162(uchar_t);
extern void Mgras_FC_Disable(void);
#endif	/* IP30 */
extern void mco_set_dcbctrl(mgras_hw *base, int crs);
extern void sr_mco_set_dcbctrl(mgras_hw *base, int crs);
extern int _mco_init7162(uchar_t device, ramDAC7162Def *dacvals);
extern int _mco_init473(uchar_t device, ramDAC473Def *dacvals);
extern void mco_7162SetAddrCmd(mgras_hw *, uchar_t, __uint32_t, uchar_t);
extern __uint32_t _mco_setpllICS(int);
extern int MgrasSetTiming(mgras_hw *base, mgras_info *info, int ucode_loaded, struct mgras_timing_info *tinfo, mgras_data *bdata); 
extern void MgrasInitVC3(register mgras_hw *base);
extern void MgrasInitCMAP(register mgras_hw *base);
extern void MgrasInitXMAP(register mgras_hw *base,mgras_info *info);
extern void MgrasInitCCR(mgras_hw *base, mgras_info *info);
extern void MgrasInitRSS(register mgras_hw *base, mgras_info *info);
extern void MgrasBlankScreen(mgras_hw *base,int blank);
extern void MgrasRecordTiming(mgras_hw *, struct mgras_timing_info *);

#ifdef IP30
void mgrasConfigureXMAP(mgras_hw *, mgras_info *);
#endif	/* IP30 */

#define         VC2_VT_FRAME_TBL_PTR                    8
#define         VC2_VT_LINE_SEQ_PTR                     9
#define         VC2_VT_LINES_RUN                        0xa
#define         VC2_CURS_TBL_PTR                        0xc

/* 
 *	Forward References 
 */

__uint32_t mgras_Reset(void);

void mco_StartTiming(void);

void mco_LoadVC2SRAM( __uint32_t addr,  ushort_t *data,  __uint32_t length,  __uint32_t DataWidth);

void mco_GetVC2SRAM( __uint32_t addr,  ushort_t *data,  __uint32_t length,  ushort_t *expected,  __uint32_t DataWidth);
__uint32_t  mco_vc2_Reset(void);
int  mco_VC2probe(void);
void mco_VC2DisableDsply(void);
void mco_VC2EnableDsply(void);

void mco_VC2CursorEnable(void);
void mco_VC2CursorDisable(void);
__uint32_t  mco_VC2CursorPositionTest();

__uint32_t  _mco_VC2CursorMode( __uint32_t mode);
__uint32_t  mco_VC2CursorMode(int argc, char **argv);
void mco_VC2ClearCursor(void);

__uint32_t mco_VC2Addrs64(void) ;
__uint32_t mco_VC2AddrsBusTest(void);
__uint32_t mco_VC2DataBusTest(void) ;
__uint32_t mco_VC2AddrsUniqTest(void) ;
__uint32_t mco_VC2PatrnTest(void) ;
int mco_VC2Init(int argc, char **argv);
void mco_StopTiming(void);
void mco_StarTiming(void);

int _mco_VC2Init(__uint32_t TimingSelect);
int _mco_initVC2(ramVC2Def *vc2vals);
extern int _mgras_VC3Init(__uint32_t TimingSelect);
extern int _mgras_SetupXMAP(__uint32_t TimingSelect);

extern struct mgras_timing_info mgras_1280_1024_60_1RSS;
extern struct mcoConfigDef mco_info_MINI_640x480_60;
extern ushort_t MINI_640x480_60_ltab[];
extern ushort_t MINI_640x480_60_ftab[];
extern struct mgras_timing_info mgras_1288_961_60_MCO_VGA_MIN_2RSS;
extern struct mgras_timing_info mgras_1288_961_60_MCO_VGA_MIN_1RSS;

extern struct mcoConfigDef mco_info_MINI_800x600_60;
extern ushort_t MINI_800x600_60_ltab[];
extern ushort_t MINI_800x600_60_ftab[];
extern struct mgras_timing_info mgras_1608_1201_60_MCO_SVGA_MIN_2RSS;
extern struct mgras_timing_info mgras_1608_1201_60_MCO_SVGA_MIN_1RSS;

#ifdef IP30
extern struct mcoConfigDef sr_mco_info_four_test_800x600_60;
#else	/* IP30 */
extern struct mcoConfigDef mco_info_four_test_800x600_60;
#endif	/* IP30 */
extern ushort_t four_test_800x600_60_ltab[];
extern ushort_t four_test_800x600_60_ftab[];
extern struct mgras_timing_info mgras_1600_1200_60_MCO_SVGA_4SCRNTST_2RSS;
extern struct mgras_timing_info mgras_1600_1200_60_MCO_SVGA_4SCRNTST_1RSS;
extern struct mgras_timing_info mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_2RSS;
extern struct mgras_timing_info mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_1RSS;

#define	VC2_RAM_MASK	0x7ffe		/* Last 16 Bit Entry in the VC2 Sram */

#define	DATA16		0
#define DATA64		1

extern struct mgras_info *mginfo;
extern struct mgras_timing_info *mg_tinfo;
unsigned short mco_dac_7162lut[256][3];
unsigned short mco_dac_473lut[256][3];

__uint32_t	vc2Timing = 0;

#ifdef IP30
void mgrasConfigureXMAP(mgras_hw *base, mgras_info *info)
{
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, 0x1);
    mgras_xmapSetConfigByte(base, 0x48); /* turn on autoinc bit */
    mgras_xmapSetAddr(base, MGRAS_XMAP_CONFIG_ADDR);
    mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs));
}
#endif	/* IP30 */

/****************************************************************************
 *
 *	MCO VC2 timing tables
 *
 ****************************************************************************/

/****************************************************************************/

void
_report_MCO_VOFinfo(mcoConfigDef *mcoVOFdata) 
{

    msg_printf(VRB,"mco_reportVOF: Altera filename is %s\n", 
	    mcoVOFdata->AlteraName);
    msg_printf(VRB,"mco_reportVOF: MGRAS Timing Select = 0x%x\n", 
    mcoVOFdata->MgrasTimingSelect);

    msg_printf(VRB,"mco_reportVOF: MCO 7162A ID = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->id);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Base Addr = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->addr);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Mode Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->mode);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cmd1 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->cmd1);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cmd2 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->cmd2);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cmd3 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->cmd3);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cmd4 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->cmd4);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cmd5 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->cmd5);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A PLL R Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->pllr);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A PLL V Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->pllv);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A PLL Cntl Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->pllctl);
    msg_printf(VRB,"mco_reportVOF: MCO 7162A Cursor Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162A->curs);

#ifndef IP30
    msg_printf(VRB,"mco_reportVOF: MCO 7162B ID = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->id);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Base Addr = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->addr);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Mode Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->mode);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cmd1 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->cmd1);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cmd2 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->cmd2);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cmd3 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->cmd3);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cmd4 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->cmd4);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cmd5 Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->cmd5);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B PLL R Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->pllr);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B PLL V Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->pllv);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B PLL Cntl Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->pllctl);
    msg_printf(VRB,"mco_reportVOF: MCO 7162B Cursor Reg = 0x%x\n",
    mcoVOFdata->mcoDAC7162B->curs);
#endif	/* IP30 */

    msg_printf(VRB,"mco_reportVOF: MCO 473A Addr Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473A->addr);
    msg_printf(VRB,"mco_reportVOF: MCO 473A Cmd Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473A->cmd);


    msg_printf(VRB,"mco_reportVOF: MCO 473B Addr Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473B->addr);
    msg_printf(VRB,"mco_reportVOF: MCO 473B Cmd Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473B->cmd);


#ifdef IP30
    msg_printf(VRB,"mco_reportVOF: MCO 473C Addr Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473C->addr);
    msg_printf(VRB,"mco_reportVOF: MCO 473C Cmd Reg = 0x%x\n", 
    mcoVOFdata->mcoDAC473C->cmd);
#endif	/* IP30 */


    msg_printf(VRB,"mco_reportVOF: MGRAS VC2 flag = 0x%x\n", 
    mcoVOFdata->mcoVC2->flag);
    msg_printf(VRB,"mco_reportVOF: MGRAS VOF WidthxHeight = %d x %d\n", 
    mcoVOFdata->mcoVC2->w, mcoVOFdata->mcoVC2->h);
    msg_printf(VRB,"mco_reportVOF: MGRAS VC2 Frame Table size = %d (0x%x)\n", 
    mcoVOFdata->mcoVC2->ftab_len, mcoVOFdata->mcoVC2->ftab_len);
    msg_printf(VRB,"mco_reportVOF: MGRAS VC2 Frame Table Addr = 0x%x (%d)\n", 
    mcoVOFdata->mcoVC2->ftab_addr, mcoVOFdata->mcoVC2->ftab_addr);
    msg_printf(VRB,"mco_reportVOF: MGRAS VC2 Line Table size = %d (0x%x)\n", 
    mcoVOFdata->mcoVC2->ltab_len, mcoVOFdata->mcoVC2->ltab_len);
    msg_printf(VRB,"mco_reportVOF: MGRAS VC2 Config Reg = 0x%x\n", 
    mcoVOFdata->mcoVC2->config);

    msg_printf(VRB,"mco_reportVOF: MCO Cntrl1 Reg = 0x%x\n", 
    mcoVOFdata->cntrl1);
    msg_printf(VRB,"mco_reportVOF: MCO Cntrl2 Reg = 0x%x\n", 
    mcoVOFdata->cntrl2);
    msg_printf(VRB,"mco_reportVOF: MCO Cntrl3 Reg = 0x%x\n", 
    mcoVOFdata->cntrl3);
}

int 
_mco_SetVOF(__uint32_t mcoTimingSelect, int flag)
{
    __uint32_t errors = 0;
    char ucodefilename[80];
    int i;
    __uint32_t vc3dc = 0;
#ifdef IP30
    int pixclk;
    char *pserver;
#endif /* IP30 */

    mcoConfigDef *mcoVOFdata;

    /* Initialize the DAC LUTs ... */

    for (i=0; i<256; i++) {
	mco_dac_7162lut[i][0] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][1] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][2] = ((i << 2) | 0x03);

	mco_dac_473lut[i][0] = i;
	mco_dac_473lut[i][1] = i;
	mco_dac_473lut[i][2] = i;
    }

    /* Initialize mcoVOFdata structure */
    switch (mcoTimingSelect) {
	case _MINI_800x600_60:
	    mcoVOFdata = (mcoConfigDef *)(&mco_info_MINI_800x600_60);

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(VRB,"mco_setVOF: Altera filename is %s\n", 
		ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */

	    break;
	case	_MINI_640x480_60:
	    mcoVOFdata = &mco_info_MINI_640x480_60;

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(VRB,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	case	_four_test_800x600_60:
#ifdef IP30
	    mcoVOFdata = &sr_mco_info_four_test_800x600_60;
#else	/* IP30 */
	    mcoVOFdata = &mco_info_four_test_800x600_60;
#endif 	/* IP30 */

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(VRB,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	default:
	    msg_printf(ERR,"_mco_SetVOF :: Unknown Timing Token Selected..%d\n",
	    mcoTimingSelect);
	    return (1);
    }

    /* Load Impact VC3 Timing Table */

    /* Set Mgras video timing pointer */
    switch (mginfo->NumREs) {
	case 1:
	    switch (mcoVOFdata->MgrasTimingSelect) {
		case _1608_1201_60_MCO_SVGA_MIN:
		    msg_printf(VRB, "mco_SetVOF: Setting MGRAS VOF to 1608x1201 @ 60Hz SVGA (Minify)\n");
		    mg_tinfo = &mgras_1608_1201_60_MCO_SVGA_MIN_1RSS;
		    break;	
		case _1288_961_60_MCO_VGA_MIN:
		    msg_printf(VRB, "mco_SetVOF: Setting MGRAS VOF to 1288x961 @ 60Hz VGA (Minify)\n");
		    mg_tinfo = &mgras_1288_961_60_MCO_VGA_MIN_1RSS;
		    break;	
		case _1600_1200_60_MCO_SVGA_4SCRNTST:
		    msg_printf(VRB, "mco_SetVOF: Setting MGRAS VOF to 1600x1200 @ 60Hz SVGA (4 Scrn Test for 1RSS)\n");
#ifdef IP30
		    mg_tinfo = &mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_1RSS;
#else	/* IP30 */
		    mg_tinfo = &mgras_1600_1200_60_MCO_SVGA_4SCRNTST_1RSS;
#endif	/* IP30 */
		    break;	
		default:
		    mg_tinfo = &mgras_1280_1024_60_1RSS;
		    break;
	    }
	    break;
	case 2:
	    switch (mcoVOFdata->MgrasTimingSelect) {
		case _1608_1201_60_MCO_SVGA_MIN:
		    msg_printf(DBG,"mco_SetVOF: Setting MGRAS VOF to 1608x1201 @ 60Hz SVGA (Minify)\n");
		    mg_tinfo = &mgras_1608_1201_60_MCO_SVGA_MIN_2RSS;
		    break;	
		case _1288_961_60_MCO_VGA_MIN:
		    msg_printf(DBG,"mco_SetVOF: Setting MGRAS VOF to 1288x961 @ 60Hz VGA (Minify)\n");
		    mg_tinfo = &mgras_1288_961_60_MCO_VGA_MIN_2RSS;
		    break;	
		case _1600_1200_60_MCO_SVGA_4SCRNTST:
		    msg_printf(DBG,"mco_SetVOF: Setting MGRAS VOF to 1600x1200 @ 60Hz SVGA (4 Scrn Test for 2RSS)\n");
#ifdef IP30
		    mg_tinfo = &mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_2RSS;
#else	/* IP30 */
		    mg_tinfo = &mgras_1600_1200_60_MCO_SVGA_4SCRNTST_2RSS;
#endif	/* IP30 */
		    break;	
		default:
		    mg_tinfo = &mgras_1280_1024_60_1RSS;
		    break;
	    }
	    break;
    }

    MgrasRecordTiming(mgbase, mg_tinfo);
#ifdef IP30
    Mgras_FC_Disable();
#endif 	/* IP30 */
    MgrasReset(mgbase);
#ifdef IP30
    MgrasInitDCBctrl(mgbase);
    mgrasConfigureXMAP(mgbase,mginfo);
#endif 	/* IP30 */
    MgrasInitHQ(mgbase, 0);

    /* Initialize Mgras Raster Backend (see MgrasInitRasterBackend() in libsk */
    MgrasInitDac(mgbase);
    if ((errors = MgrasSyncREPPs(mgbase, mginfo)) != 0) {
	msg_printf(VRB,"ERROR: Couldn't sync Impact REs and PPs\n");
	return errors;
    }
    MgrasSyncPPRDRAMs(mgbase, mginfo);
    MgrasBlankScreen(mgbase, 1);

    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
    MgrasInitVC3(mgbase);
    MgrasInitXMAP(mgbase,mginfo);
    MgrasInitCCR(mgbase,mginfo);
    MgrasInitRSS(mgbase,mginfo);
    MgrasInitCMAP(mgbase);
    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
#ifdef NOT_FOR_SRMCO
#endif /* NOT_FOR_SRMCO */

#ifdef IP30
    /* Blank MGRAS */
    msg_printf(VRB,"Blanking MGRAS\n");
    MgrasBlankScreen(mgbase, 1);
#endif /* IP30 */

    /* Set Mgras XMAX and YMAX based on MGRAS VOF */
    XMAX = mg_tinfo->w;
    YMAX = mg_tinfo->h;

    /* Load MCO FPGA file */
#ifdef IP30
    errors += _sr_mco_initfpga(ucodefilename);
#else	/* IP30 */
    errors += _mco_initfpga(ucodefilename);
#endif	/* IP30 */
    if (errors) {
	msg_printf(VRB,"ERROR: Couldn't load fpga file %s\n", mcoVOFdata->AlteraName);
	return errors;
    }

    /* Start a clock for the VC2 */
    msg_printf(DBG,"_mco_SetVOF: Starting VC2 clock\n");
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 5);
#ifndef IP30
#endif	/* Not IP30 */

#ifdef IP30
    /* Initialize External PLL Clock for the DACs */
    pixclk = mcoVOFdata->mcoDAC7162A->freq;
    msg_printf(DBG,"mco_SetVOF: pixclk = %d\n", pixclk);
    if (_mco_setpllICS(pixclk)) {
	msg_printf(ERR, "Couldn't set 1572 External PLL clock\n");
    }
#endif	/* IP30 */

    /* Initialize the DACs */
#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, MCO_INDEX);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, MCO_INDEX);
#endif	/* IP30 */
    errors +=_mco_init7162(MCO_ADV7162A, mcoVOFdata->mcoDAC7162A);
#ifndef IP30
    errors +=_mco_init7162(MCO_ADV7162B, mcoVOFdata->mcoDAC7162B);
#endif	/* IP30 */

    errors +=_mco_init473(MCO_ADV473A, mcoVOFdata->mcoDAC473A);
    errors +=_mco_init473(MCO_ADV473B, mcoVOFdata->mcoDAC473B);
#ifdef IP30
    errors +=_mco_init473(MCO_ADV473C, mcoVOFdata->mcoDAC473C);
#endif	/* IP30 */

    /* Set the control registers in the DCB FPGA */
    MCO_SETINDEX(mgbase, MCO_CONTROL1, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl1);

    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl2);

    MCO_SETINDEX(mgbase, MCO_CONTROL3, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl3);

    /* Initialize the VC2 for the selected format */
    errors += _mco_initVC2(mcoVOFdata->mcoVC2);

    /* Unblank MGRAS */
    msg_printf(VRB,"Unblanking MGRAS\n");
    MgrasBlankScreen(mgbase, 0);

    if (flag & (VC3_GENLOCK | VC3_GENSYNC)) {
	mgras_vc3GetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
	vc3dc |= VC3_GENSYNC;
	vc3dc &= ~VC3_GENLOCK;
	msg_printf(VRB,"mco_setvof: Framelock set (vc3 DCReg = 0x%x)\n", vc3dc);
	mgras_vc3SetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
#ifdef IP30
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_SEL_GENSYNC_BIT);
#endif 	/* IP30 */
    }
    return(errors);
}

#define MCO_Format_Count	3

static struct VOFtable mcoVOFlist[] = {
	{"mini_800x600_60",	_MINI_800x600_60},
	{"mini_640x480_60",	_MINI_640x480_60},
	{"4test_800x600_60",	_four_test_800x600_60},
	{"",	0},
};

int 
mco_SetVOF(int argc, char **argv)
{
	 __uint32_t TimingToken;
	int VOFfound = 0;
	int i, j;
	int flag = 0;
	int mcovofcount = sizeof(mcoVOFlist)/sizeof(mcoVOFlist[0]);
	char VOFname[80];
	char tmpstr[80], badstr[80];

	for(i=1; i<argc; i++) {
	    sprintf(tmpstr,"%s", argv[i]);
	    msg_printf(DBG,"argv[%d] = %s\n", i, tmpstr);
	    if (tmpstr[0] == '-') {
		switch (tmpstr[1]) {
		    case 'g':	/* Set GENSYNC and GENLOCK flags */
			flag = VC3_GENSYNC | VC3_GENLOCK;
			break;
		    default:
			break;
		}
	    }
	    for (j=0; j < mcovofcount; j++) {
		msg_printf(DBG,"XXX: mcoVOFlist[%d].name = %s\ttmpstr = %s\n",
		j, mcoVOFlist[j].name, tmpstr);
		if(strcmp(mcoVOFlist[j].name, tmpstr) == 0) {
		    sprintf(VOFname,"%s", tmpstr);
		    TimingToken = mcoVOFlist[j].TimingToken;
		    VOFfound++;
		    break;
		}
	    }
	    if (!VOFfound) {
		sprintf(badstr,"%s", tmpstr);
	    }
	}

	if (!VOFfound) {
	    msg_printf(SUM,"\"%s\" is not a valid MCO format\n", badstr);

	    msg_printf(VRB,"Supported MCO formats are:\n");
	    for ( i=0; (mcoVOFlist[i].TimingToken != 0); i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFlist[i].name);
	    }
	    return (1);
	}
	else {
	    msg_printf(VRB,"MCO format: %s\t(0x%x)\n",
	    VOFname, TimingToken);
	    return (_mco_SetVOF(TimingToken, flag));
	}
}

int 
_mgras_setgenlock(int enable)
{
    int source = MGRAS_GENLOCK_SOURCE_INTERNAL;
    __uint32_t vc3dc = 0;

    vc3dc = VC3_VIDEO_ENAB | VC3_DID_ENAB1 | VC3_DID_ENAB2 | VC3_BLACKOUT;

#ifdef NOTYET
    mgras_vc3GetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
#endif 	/* NOTYET */

    if (enable) {
	vc3dc |= VC3_GENSYNC;
    }
    else {
	vc3dc &= ~VC3_GENSYNC;
    }

    if (source) {
	vc3dc |= VC3_GENLOCK;
    }
    else {
	vc3dc &= ~VC3_GENLOCK;
    }

    msg_printf(VRB,"mco_setgenlock: Framelock set (vc3 DCReg = 0x%x)\n", vc3dc);
    mgras_vc3SetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
#ifdef IP30
    if (enable) {
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_SEL_GENSYNC_BIT);
    }
    else {
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
    }
#endif 	/* IP30 */
    return(0);
}

int 
mgras_setgenlock(int argc, char **argv)
{
    char tmpstr[80];
    int i;

    if (argc < 2 ) {	/* Not enough args */
	msg_printf(VRB, "Turning MGRAS genlock ON\n");
	i = 1;
    }
    else {
	sprintf(tmpstr,"%s", argv[1]);
	msg_printf(DBG,"argv[1] = %s\n", tmpstr);
	if (!strcasecmp(tmpstr, "on") ) {
	    msg_printf(VRB, "%s: Turning MGRAS genlock ON\n", argv[0]);
	    i = 1;
	}
	else if (!strcasecmp(tmpstr, "off")) {
	    msg_printf(VRB, "%s: Turning MGRAS genlock OFF\n", argv[0]);
	    i = 0;
	}
	else {
	    msg_printf(VRB, "Error: Invalid argument to %s\n", argv[0]);
	    msg_printf(VRB, "Usage: %s off \t(to turn MGRAS Genlock OFF)\n", argv[0]);
	    msg_printf(VRB, "       %s on \t(to turn MGRAS Genlock ON)\n", argv[0]);
	    return(1);
	}
    }

    _mgras_setgenlock(i);

    return (0);
}

/*
 * _mco_LoadVOF initializes the specified VOF on the MCO board.
 * NOTE:  This routine assumes the appropriate MGRAS/GAMERA VOF has been
 * loaded already!!!!
 */
int
_mco_LoadVOF(__uint32_t mcoTimingSelect)
{
    __uint32_t errors = 0;
    char ucodefilename[80];
    int i;
#ifdef IP30
    int pixclk;
    char *pserver;
#endif /* IP30 */

    mcoConfigDef *mcoVOFdata;

    /* Initialize the DAC LUTs ... */

    for (i=0; i<256; i++) {
	mco_dac_7162lut[i][0] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][1] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][2] = ((i << 2) | 0x03);

	mco_dac_473lut[i][0] = i;
	mco_dac_473lut[i][1] = i;
	mco_dac_473lut[i][2] = i;
    }

    /* Initialize mcoVOFdata structure */
    switch (mcoTimingSelect) {
	case _MINI_800x600_60:
	    mcoVOFdata = (mcoConfigDef *)(&mco_info_MINI_800x600_60);

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
		ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */

	    break;
	case	_MINI_640x480_60:
	    mcoVOFdata = &mco_info_MINI_640x480_60;

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	case	_four_test_800x600_60:
#ifdef IP30
	    mcoVOFdata = &sr_mco_info_four_test_800x600_60;
#else	/* IP30 */
	    mcoVOFdata = &mco_info_four_test_800x600_60;
#endif 	/* IP30 */

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    ucodefilename[0] = '\0';
	    strcat(ucodefilename, "bootp()");
#ifdef IP30
	    pserver = getenv("mfgserver");
	    if (pserver) {
		msg_printf(DBG,"mfgserver = \"%s\"\n", pserver);
		strcat(ucodefilename, pserver);
		strcat(ucodefilename, ":");
	    }
	    else {
		msg_printf(DBG,"mfgserver was not set\n", pserver);
	    }
#endif	/* IP30 */
	    strcat(ucodefilename, mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	default:
	    msg_printf(ERR,"_mco_LoadVOF :: Unknown Timing Token Selected..%d\n",
	    mcoTimingSelect);
	    return (1);
    }

#ifdef IP30
    /* Blank MGRAS */
    msg_printf(VRB,"Blanking MGRAS\n");
    MgrasBlankScreen(mgbase, 1);
#endif	/* IP30 */

    /* Load MCO FPGA file */
    msg_printf(VRB,"mco_loadvof: Loading fpga file %s\n", ucodefilename);
#ifdef IP30
    errors = _sr_mco_initfpga(ucodefilename);
#else	/* IP30 */
    errors = _mco_initfpga(ucodefilename);
#endif	/* IP30 */
    if (errors) {
	msg_printf(VRB,"ERROR: Couldn't load fpga file %s\n", mcoVOFdata->AlteraName);
	return errors;
    }
    msg_printf(VRB,"mco_loadvof: Done loading fpga file %s\n", ucodefilename);

#ifndef IP30
    /* Start a clock for the VC2 */
    msg_printf(VRB,"_mco_LoadVOF: Starting VC2 clock\n");
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 5);
#endif	/* notIP30 */

#ifdef IP30
    /* Initialize External PLL Clock for the DACs */
    pixclk = mcoVOFdata->mcoDAC7162A->freq;
    msg_printf(DBG,"mco_LoadVOF: pixclk = %d\n", pixclk);
    if (_mco_setpllICS(pixclk)) {
	msg_printf(ERR, "Couldn't set 1572 External PLL clock\n");
    }
#endif	/* IP30 */

    /* Initialize the DACs */
    msg_printf(VRB,"mco_loadvof: Initialize the DACs\n");
#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, MCO_INDEX);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, MCO_INDEX);
#endif	/* IP30 */

    errors += _mco_init7162(MCO_ADV7162A, mcoVOFdata->mcoDAC7162A);
#ifndef IP30
    errors += _mco_init7162(MCO_ADV7162B, mcoVOFdata->mcoDAC7162B);
#endif	/* IP30 */

    errors += _mco_init473(MCO_ADV473A, mcoVOFdata->mcoDAC473A);
    errors += _mco_init473(MCO_ADV473B, mcoVOFdata->mcoDAC473B);
#ifdef IP30
    errors += _mco_init473(MCO_ADV473C, mcoVOFdata->mcoDAC473C);
#endif	/* IP30 */

    /* Set the control registers in the DCB FPGA */
    MCO_SETINDEX(mgbase, MCO_CONTROL1, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl1);

    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl2);

    MCO_SETINDEX(mgbase, MCO_CONTROL3, 0);
    MCO_IO_WRITE(mgbase, mcoVOFdata->cntrl3);

    /* Initialize the VC2 for the selected format */
    errors += _mco_initVC2(mcoVOFdata->mcoVC2);

    /* Unblank MGRAS */
    msg_printf(VRB,"Unblanking MGRAS\n");
    MgrasBlankScreen(mgbase, 0);

    return(errors);
}

int 
mco_LoadVOF(int argc, char **argv)
{
	 __uint32_t TimingToken;
	int VOFfound = 0;
	int i, j;
	int mcovofcount = sizeof(mcoVOFlist)/sizeof(mcoVOFlist[0]);
	char VOFname[80];
	char tmpstr[80], badstr[80];

	for(i=1; i<argc; i++) {
	    sprintf(tmpstr,"%s", argv[i]);
	    msg_printf(DBG,"argv[%d] = %s\n", i, tmpstr);
	    if (tmpstr[0] == '-') {
		switch (tmpstr[1]) {
		    case 'g':	/* Set GENSYNC and GENLOCK flags */
#ifdef NOTYET
			flag = VC3_GENSYNC | VC3_GENLOCK;
#endif
			break;
		    default:
			break;
		}
	    }
	    for (j=0; j < mcovofcount; j++) {
		msg_printf(DBG,"XXX: mcoVOFlist[%d].name = %s\ttmpstr = %s\n",
		j, mcoVOFlist[j].name, tmpstr);
		if(strcmp(mcoVOFlist[j].name, tmpstr) == 0) {
		    sprintf(VOFname,"%s", tmpstr);
		    TimingToken = mcoVOFlist[j].TimingToken;
		    VOFfound++;
		    break;
		}
	    }
	    if (!VOFfound) {
		sprintf(badstr,"%s", tmpstr);
	    }
	}

	if (!VOFfound) {
	    msg_printf(SUM,"\"%s\" is not a valid MCO format\n", badstr);

	    msg_printf(VRB,"Supported MCO formats are:\n");
	    for ( i=0; (mcoVOFlist[i].TimingToken != 0); i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFlist[i].name);
	    }
	    return (1);
	}
	else {
	    msg_printf(VRB,"MCO format: %s\t(0x%x)\n",
	    VOFname, TimingToken);
	    return (_mco_LoadVOF(TimingToken));
	}
}

_mco_Load7162(__uint32_t mcoTimingSelect, uchar_t device)
{
    __uint32_t errors = 0;
    char ucodefilename[80];
    int i;

    mcoConfigDef *mcoVOFdata;

    /* Initialize the DAC LUTs ... */

    for (i=0; i<256; i++) {
	mco_dac_7162lut[i][0] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][1] = ((i << 2) | 0x03);
	mco_dac_7162lut[i][2] = ((i << 2) | 0x03);
    }

    /* Initialize mcoVOFdata structure */
    switch (mcoTimingSelect) {
	case _MINI_800x600_60:
	    mcoVOFdata = (mcoConfigDef *)(&mco_info_MINI_800x600_60);

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    sprintf(ucodefilename,"bootp()%s", mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
		ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */

	    break;
	case	_MINI_640x480_60:
	    mcoVOFdata = &mco_info_MINI_640x480_60;

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    sprintf(ucodefilename,"bootp()%s", mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	case	_four_test_800x600_60:
#ifdef IP30
	    mcoVOFdata = &sr_mco_info_four_test_800x600_60;
#else	/* IP30 */
	    mcoVOFdata = &mco_info_four_test_800x600_60;
#endif 	/* IP30 */

	    /* Initialize the Altera FPGA filename */
#ifdef NO_NETWORK
	    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/%s", mcoVOFdata->AlteraName);
#else
	    sprintf(ucodefilename,"bootp()%s", mcoVOFdata->AlteraName);
#endif	/* NO_NETWORK */
	    msg_printf(DBG,"mco_setVOF: Altera filename is %s\n", 
	    ucodefilename);
#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	default:
	    msg_printf(ERR,"_mco_Load7162 :: Unknown Timing Token Selected..%d\n",
	    mcoTimingSelect);
	    return (1);
    }

    /* Initialize the DACs */
#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, MCO_INDEX);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, MCO_INDEX);
#endif	/* IP30 */
    switch (device) {
	case MCO_ADV7162A:
	    errors += _mco_init7162(MCO_ADV7162A, mcoVOFdata->mcoDAC7162A);
	    break;
	case MCO_ADV7162B:
	    errors += _mco_init7162(MCO_ADV7162B, mcoVOFdata->mcoDAC7162B);
	    break;
    }

    return(errors);
}

int 
mco_Load7162(int argc, char **argv)
{
	 __uint32_t TimingToken;
	int VOFfound = 0;
	int i, j;
	int mcovofcount = sizeof(mcoVOFlist)/sizeof(mcoVOFlist[0]);
	char VOFname[80];
	char tmpstr[80], badstr[80];
	uchar_t device;

	if (argc != 3) {
	    msg_printf(SUM, "usage %s VOFformat Device (where Device is A or B)\n",
	    argv[0]);

	    msg_printf(VRB,"Supported MCO formats are:\n");
	    for ( i=0; (mcoVOFlist[i].TimingToken != 0); i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFlist[i].name);
	    }
	    return -1;
	}

	for(i=1; i<argc; i++) {
	    sprintf(tmpstr,"%s", argv[i]);
	    msg_printf(DBG,"argv[%d] = %s\n", i, tmpstr);

	    switch (tmpstr[0]) {
		case 'A':
		    device = MCO_ADV7162A;
		    break;
		case 'B':
		    device = MCO_ADV7162B;
		    break;
	    }

	    for (j=0; j < mcovofcount; j++) {
		msg_printf(DBG,"XXX: mcoVOFlist[%d].name = %s\ttmpstr = %s\n",
		j, mcoVOFlist[j].name, tmpstr);
		if(strcmp(mcoVOFlist[j].name, tmpstr) == 0) {
		    sprintf(VOFname,"%s", tmpstr);
		    TimingToken = mcoVOFlist[j].TimingToken;
		    VOFfound++;
		    break;
		}
	    }
	    if (!VOFfound) {
		sprintf(badstr,"%s", tmpstr);
	    }
	}

	if (!VOFfound) {
	    msg_printf(SUM,"\"%s\" is not a valid MCO format\n", badstr);

	    msg_printf(VRB,"Supported MCO formats are:\n");
	    for ( i=0; (mcoVOFlist[i].TimingToken != 0); i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFlist[i].name);
	    }
	    return (1);
	}
	else {
	    msg_printf(VRB,"mco_7162dacload: reset 7162 with MCO format: %s\t(0x%x)\n",
	    VOFname, TimingToken);
	    return (_mco_Load7162(TimingToken, device));
	}
}

/*
 * _mgras_LoadVOF initializes the specified VOF on the MGRAS/GAMERA board.
 * Use _mco_LoadVOF() to load the appropriate MCO VOF
 */

_mgras_LoadVOF(__uint32_t mcoTimingSelect, int flag)
{
    __uint32_t errors = 0;
    __uint32_t vc3dc = 0;

    mcoConfigDef *mcoVOFdata;

    /* Initialize mcoVOFdata structure */
    switch (mcoTimingSelect) {
	case _MINI_800x600_60:
	    mcoVOFdata = (mcoConfigDef *)(&mco_info_MINI_800x600_60);

#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */

	    break;
	case	_MINI_640x480_60:
	    mcoVOFdata = &mco_info_MINI_640x480_60;

#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	case	_four_test_800x600_60:
#ifdef IP30
	    mcoVOFdata = &sr_mco_info_four_test_800x600_60;
#else	/* IP30 */
	    mcoVOFdata = &mco_info_four_test_800x600_60;
#endif 	/* IP30 */

#ifdef NOTYET
	    _report_MCO_VOFinfo(mcoVOFdata);
#endif /* NOTYET */
	    break;
	default:
	    msg_printf(ERR,"_mgras_LoadVOF :: Unknown Timing Token Selected..%d\n",
	    mcoTimingSelect);
	    return (1);
    }

    /* Load Impact VC3 Timing Table */

    /* Set Mgras video timing pointer */
    switch (mginfo->NumREs) {
	case 1:
	    switch (mcoVOFdata->MgrasTimingSelect) {
		case _1608_1201_60_MCO_SVGA_MIN:
		    msg_printf(VRB, "mgras_LoadVOF: Setting MGRAS VOF to 1608x1201 @ 60Hz SVGA (Minify)\n");
		    mg_tinfo = &mgras_1608_1201_60_MCO_SVGA_MIN_1RSS;
		    break;	
		case _1288_961_60_MCO_VGA_MIN:
		    msg_printf(VRB, "mgras_LoadVOF: Setting MGRAS VOF to 1288x961 @ 60Hz VGA (Minify)\n");
		    mg_tinfo = &mgras_1288_961_60_MCO_VGA_MIN_1RSS;
		    break;	
		case _1600_1200_60_MCO_SVGA_4SCRNTST:
		    msg_printf(VRB, "mgras_LoadVOF: Setting MGRAS VOF to 1600x1200 @ 60Hz SVGA (4 Scrn Test for 1RSS)\n");
#ifdef IP30
		    mg_tinfo = &mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_1RSS;
#else	/* IP30 */
		    mg_tinfo = &mgras_1600_1200_60_MCO_SVGA_4SCRNTST_1RSS;
#endif	/* IP30 */
		    break;	
		default:
		    mg_tinfo = &mgras_1280_1024_60_1RSS;
		    break;
	    }
	    break;
	case 2:
	    switch (mcoVOFdata->MgrasTimingSelect) {
		case _1608_1201_60_MCO_SVGA_MIN:
		    msg_printf(DBG,"mgras_LoadVOF: Setting MGRAS VOF to 1608x1201 @ 60Hz SVGA (Minify)\n");
		    mg_tinfo = &mgras_1608_1201_60_MCO_SVGA_MIN_2RSS;
		    break;	
		case _1288_961_60_MCO_VGA_MIN:
		    msg_printf(DBG,"mgras_LoadVOF: Setting MGRAS VOF to 1288x961 @ 60Hz VGA (Minify)\n");
		    mg_tinfo = &mgras_1288_961_60_MCO_VGA_MIN_2RSS;
		    break;	
		case _1600_1200_60_MCO_SVGA_4SCRNTST:
		    msg_printf(DBG,"mgras_LoadVOF: Setting MGRAS VOF to 1600x1200 @ 60Hz SVGA (4 Scrn Test for 2RSS)\n");
#ifdef IP30
		    mg_tinfo = &mgras_1600_1200_60_SRMCO_SVGA_4SCRNTST_2RSS;
#else	/* IP30 */
		    mg_tinfo = &mgras_1600_1200_60_MCO_SVGA_4SCRNTST_2RSS;
#endif	/* IP30 */
		    break;	
		default:
		    mg_tinfo = &mgras_1280_1024_60_1RSS;
		    break;
	    }
	    break;
    }

    MgrasRecordTiming(mgbase, mg_tinfo);
#ifdef IP30
    Mgras_FC_Disable();
#endif 	/* IP30 */
    MgrasReset(mgbase);
#ifdef IP30
    MgrasInitDCBctrl(mgbase);
    mgrasConfigureXMAP(mgbase,mginfo);
#endif 	/* IP30 */
    MgrasInitHQ(mgbase, 0);

    /* Initialize Mgras Raster Backend (see MgrasInitRasterBackend() in libsk */
    MgrasInitDac(mgbase);
    if ((errors = MgrasSyncREPPs(mgbase, mginfo)) != 0) {
	msg_printf(VRB,"ERROR: Couldn't sync Impact REs and PPs\n");
	return errors;
    }
    MgrasSyncPPRDRAMs(mgbase, mginfo);
    MgrasBlankScreen(mgbase, 1);

    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
    MgrasInitVC3(mgbase);
    MgrasInitXMAP(mgbase,mginfo);
    MgrasInitCCR(mgbase,mginfo);
    MgrasInitRSS(mgbase,mginfo);
    MgrasInitCMAP(mgbase);
    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
    MgrasBlankScreen(mgbase, 0);

    /* Set Mgras XMAX and YMAX based on MGRAS VOF */
    XMAX = mg_tinfo->w;
    YMAX = mg_tinfo->h;

    if (flag & (VC3_GENLOCK | VC3_GENSYNC)) {
	mgras_vc3GetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
	vc3dc |= VC3_GENSYNC;
	vc3dc &= ~VC3_GENLOCK;
	msg_printf(VRB,"mgras_loadvof: Framelock set (vc3 DCReg = 0x%x)\n", vc3dc);
	mgras_vc3SetReg(mgbase, VC3_DISPLAY_CONTROL, vc3dc);
#ifdef IP30
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_SEL_GENSYNC_BIT);
#endif 	/* IP30 */
    }
    return(errors);
}

int 
mgras_LoadVOF(int argc, char **argv)
{
	 __uint32_t TimingToken;
	int VOFfound = 0;
	int i, j;
	int flag = 0;
	int mcovofcount = sizeof(mcoVOFlist)/sizeof(mcoVOFlist[0]);
	char VOFname[80];
	char tmpstr[80], badstr[80];

	for(i=1; i<argc; i++) {
	    sprintf(tmpstr,"%s", argv[i]);
	    msg_printf(DBG,"argv[%d] = %s\n", i, tmpstr);
	    if (tmpstr[0] == '-') {
		switch (tmpstr[1]) {
		    case 'g':	/* Set GENSYNC and GENLOCK flags */
			flag = VC3_GENSYNC | VC3_GENLOCK;
			break;
		    default:
			break;
		}
	    }
	    for (j=0; j < mcovofcount; j++) {
		msg_printf(DBG,"XXX: mcoVOFlist[%d].name = %s\ttmpstr = %s\n",
		j, mcoVOFlist[j].name, tmpstr);
		if(strcmp(mcoVOFlist[j].name, tmpstr) == 0) {
		    sprintf(VOFname,"%s", tmpstr);
		    TimingToken = mcoVOFlist[j].TimingToken;
		    VOFfound++;
		    break;
		}
	    }
	    if (!VOFfound) {
		sprintf(badstr,"%s", tmpstr);
	    }
	}

	if (!VOFfound) {
	    msg_printf(SUM,"\"%s\" is not a valid MCO format\n", badstr);

	    msg_printf(VRB,"Supported MCO formats are:\n");
	    for ( i=0; (mcoVOFlist[i].TimingToken != 0); i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFlist[i].name);
	    }
	    return (1);
	}
	else {
	    msg_printf(VRB,"MCO format: %s\t(0x%x)\n",
	    VOFname, TimingToken);
	    return (_mgras_LoadVOF(TimingToken, flag));
	}
}

void 
mco_MaxPLL_NoTiming(void)
{
    /* We want to have the FASTEST CLOCK OUT OF THE DAC  and No Timing Tests 
     * running for the VC2 Tests. This will stress the system also 
     */

    /* Set DCBCTRL to appropriate value */
    mco_set_dcbctrl(mgbase, 2);

    /* Turn on PLL Reference clock */
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 1);
    mco_set_dcbctrl(mgbase, 2);

    /* 4:1 mux */
    mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_CMD3_ADDR, MCO_ADV7162_CMD3_INIT_FS);

    /* Set 7162 PLL registers */
    mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_R_ADDR,   0xf);
    mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_V_ADDR,   0xb);
    mco_7162SetAddrCmd(mgbase, MCO_ADV7162A, MCO_ADV7162_PLL_CTL_ADDR, 0xc1);

}

void 
mco_LoadVC2SRAM( __uint32_t addr,  ushort_t *data,  __uint32_t length,  __uint32_t DataWidth)
{
        __uint32_t i, Count;

	
	(Count = DataWidth ? length/4 : length) ;

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 7);

	msg_printf(DBG,"LoadVC2SRAM: Setting VC2 Ram Addr to 0x%x\n", addr);
        MCO_VC2_SETREG(mgbase,VC2_RAM_ADDR, addr);
        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
        for (i=0 ;i<Count; i++) {
		msg_printf(DBG,"mco_LoadVC2SRAM: Writing 0x%x to VC2 SRAM\n",
		data[i]);
		if (!DataWidth) {
                	MCO_VC2_RAM_WRITE(mgbase,data[i]);
		} 

                if ((i & 0x3f) == 0x3f)
                {
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
        }
}


void 
mco_GetVC2SRAM( __uint32_t addr,  ushort_t *data,  __uint32_t length,  ushort_t *expected,  __uint32_t DataWidth)
{
        __uint32_t i, Count;
	/*REFERENCED*/
	__uint32_t Exp;
	ushort_t  *RdPtr;
	/*REFERENCED (not really though) */
	__uint64_t	*RdPtr64;

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 4);

	(Count = DataWidth ? length/4 : length) ;
	if (!DataWidth) {
		RdPtr = data;
	} else {
		RdPtr64 = (__uint64_t *)data;
	}
        MCO_VC2_SETREG(mgbase,VC2_RAM_ADDR, addr);

        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	Exp = *expected;
        for (i=0;i < Count;i++)
        {
		if (!DataWidth) {
                	MCO_VC2_RAM_READ(mgbase, RdPtr[i]);
		} 
                if ((i & 0x3f) == 0x3f)
                {
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
        }
}

/* If the DCB FPGA is programmed, then this will check if the
   VC2 is out there.  If the DCB FPGA is not programmed, this
   will probably hang or crash or something... 
*/

int
mco_VC2probe(void)
{
    ushort_t i, j;
    int errs = 0;

    /*Use this to get a clock if the 7162 is not programmed. */
    msg_printf(DBG,"mco_VC2probe: Starting VC2 clock\n");
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 5);

    /* reset VC2 */
    msg_printf(DBG,"mco_VC2probe: Reset VC2\n");
    mco_set_dcbctrl(mgbase, 4);
    MCO_VC2_SETREG(mgbase, VC2_CONFIG, 0);

    MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

    msg_printf(DBG,"mco_VC2probe: Test VC2 Video Entry Ptr Register\n");
    for (i=0; i<0xffff; i++) {
	MCO_VC2_SETREG(mgbase, VC2_VIDEO_ENTRY_PTR, i);
	MCO_VC2_GETREG(mgbase, VC2_VIDEO_ENTRY_PTR, j);
	msg_printf(DBG, "mco_VC2probe: Wrote 0x%x, Read 0x%x\n", i, j);
	if (i != j) {
	    msg_printf(ERR, "ERR: mco_VC2probe: Probe VC2 failed: Wrote %d, Read %d.\n", i, j);
	    errs++;
	    return(errs);
	}
	if (i & 0x3) {
	    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	}

    }
    msg_printf(VRB,"mco_VC2probe: Probe Completed\n");
    return(errs);
}

__uint32_t
mco_VC2Reset(void)
{
	__uint32_t Bit ;
	__uint32_t state = 0xdead;

	/*
 	 * Board Version Register 0 and Board Config reg 0 are the SAME.
 	 * The following statments will reset VC2 and release the reset on VC2
	 */
	mco_MaxPLL_NoTiming();

	/* Reset the VC2 (necessary whenever we change the pixel clock) */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Reset the VC2\n");
#endif /* NOTYET */
	mco_set_dcbctrl(mgbase, 4);
	MCO_VC2_SETREG(mgbase,VC2_CONFIG, 0);

	/* Now enable the VC2 */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Enable the VC2\n");
#endif /* NOTYET */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

	MCO_VC2_GETREG(mgbase,VC2_CONFIG, state);
	msg_printf(DBG, "=======   VC2 Config Info ====\n ");
	for(Bit=0; Bit < 7; Bit++, state >>=1) {
	    switch(Bit) {
		case 0:
		    if (state & 0x1) {
			msg_printf(DBG, "VC2 - Normal Operation :  %d\t" , state & 0x1);
		    } else {
			msg_printf(DBG, "VC2 - SoftReset:  %d\t" , state & 0x1);
		    }
		    break;
		case 1:
		    if (state & 0x1) {
			msg_printf(DBG, "Fast Mode, Monitor Pixel >= 85Mhz:  %d\n" ,state & 0x1);
		    } else {
			msg_printf(DBG, "Slow Mode, Monitor Pixel < 85Mhz:  %d\n" ,state & 0x1);
		    }
		    break;
		case 2:
		    if (state & 0x1) {
			msg_printf(ERR, "\nCursor Error Occurred:   %d  \n" ,state & 0x1);
		    } else {
			msg_printf(DBG, "Cursor Flag :  %d  \n" ,state & 0x1);
		    }
		    break;
		case 3:
		    if (state & 0x1) {
			msg_printf(ERR, "\nDID1 Fifo Underflow :  %d  \n" ,state & 0x1);
		    } else {
			msg_printf(DBG, "DID1 Fifo Underflow Flag Cleared : %d\n" ,state & 0x1);
		    }
		    break;
		case 4:
		    if (state & 0x1) {
			msg_printf(ERR, "\nDID2 Fifo Underflow :  %d  \n" ,state & 0x1);
		    } else {
			msg_printf(DBG, "DID2 Fifo Underflow Flag Cleared : %d\n" ,state & 0x1);
		    }
		    break;
		case 5:
		    if (state & 0x1) {
			msg_printf(ERR, "\nVTG Fifo Underflow :  %d  \n" ,state & 0x1);
		    } else {
			msg_printf(DBG, "VTG Fifo Underflow Flag Cleared : %d\t" ,state & 0x1);
		    }
		    break;
		case 6:
		    if (state & 0x1) {
			msg_printf(ERR, "\nVC2 Revision  Wrong :  %d should be 0\n" ,state & 0x3); 
		    } else {
			msg_printf(DBG, "VC2 Revision 	:  %d\n" ,state & 0x3); 
		    }
		    break;
		}
	}
	msg_printf(DBG, "=======   VC2 Config Info ====\n ");
	
	return 0;
}

#define		DID_TABLE_1	1

__uint32_t 
_mco_Verify_Sram_Load(__uint32_t SramAddr,  ushort_t *Exp, __uint32_t length)
{
        ushort_t addr;
	ushort_t index    = 0;
        __uint32_t errors   = 0;
        ushort_t mask     = 0xffff;
        ushort_t rcv      = 0xdead;

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 7);

	msg_printf(DBG, "_mco_Verify_Sram_Load\n");
	for(index=0, addr = SramAddr; index<length ; index++, addr++) {
		mco_GetVC2SRAM(addr,  &rcv,  1, Exp, DATA16);
		COMPARE("MCO VC2 SRAM Load Verify", addr, Exp[index], rcv, mask, errors);
	}
	msg_printf(DBG, "_mco_Verify_Sram_Load  Done\n");
	return (errors);
}

void
_vc2_dump_regs(void)
{
	ushort_t _rcv1 = 0xbeef;
	ushort_t _rcv2 = 0xbeef;
	ushort_t _rcv3 = 0xbeef;
	ushort_t _rcv4 = 0xbeef;

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 7);

	MCO_VC2_GETREG(mgbase,	VC2_VIDEO_ENTRY_PTR, 	_rcv1);
	MCO_VC2_GETREG(mgbase,	VC2_CURS_ENTRY_PTR, 	_rcv2);
	msg_printf(DBG, "VC2_VIDEO_ENTRY_PTR:  0x%x    VC2_CURS_ENTRY_PTR:0x%x \n", _rcv1, _rcv2);

	MCO_VC2_GETREG(mgbase,	VC2_CURS_X_LOC, _rcv3);
	MCO_VC2_GETREG(mgbase,	VC2_CURS_Y_LOC, _rcv4);
	msg_printf(DBG, "VC2_CURS_X_LOC:  0x%x    VC2_CURS_Y_LOC: 0x%x \n", _rcv3, _rcv4);

	MCO_VC2_GETREG(mgbase, VC2_DID_ENTRY_PTR, _rcv1);
	msg_printf(DBG, "VC2_DID_ENTRY_PTR: 0x%x\n", _rcv1);

	MCO_VC2_GETREG(mgbase, VC2_SCAN_LENGTH, _rcv3);
	MCO_VC2_GETREG(mgbase, VC2_RAM_ADDR,    _rcv4);
	msg_printf(DBG, "VC2_SCAN_LENGTH: 0x%x    VC2_RAM_ADDR: 0x%x \n",  _rcv3, _rcv4);


	MCO_VC2_GETREG(mgbase, VC2_VT_FRAME_TBL_PTR, _rcv1);
	MCO_VC2_GETREG(mgbase, VC2_VT_LINE_SEQ_PTR,  _rcv2);
	msg_printf(DBG, "VC2_VT_FRAME_TBL_PTR: 0x%x    VC2_VT_LINE_SEQ_PTR: 0x%x \n", _rcv1, _rcv2);

	MCO_VC2_GETREG(mgbase, VC2_VT_LINES_RUN,     _rcv3);
	MCO_VC2_GETREG(mgbase, VC2_VERTICAL_LINE_CTR, _rcv4);
	msg_printf(DBG, "VC2_VT_LINES_RUN: 0x%x    VC2_VERTICAL_LINE_CTR: 0x%x \n", _rcv3, _rcv4);

	MCO_VC2_GETREG(mgbase, VC2_CURS_TBL_PTR, _rcv1);
	MCO_VC2_GETREG(mgbase, VC2_WORKING_CURS_Y, _rcv2);
	msg_printf(DBG, "VC2_CURS_TBL_PTR: 0x%x    VC2_WORKING_CURS_Y: 0x%x \n", _rcv1, _rcv2);

	MCO_VC2_GETREG(mgbase, VC2_CURRENT_CURS_X_LOC, _rcv3);
	MCO_VC2_GETREG(mgbase, VC2_DID_FRAME_PTR, _rcv4);
	msg_printf(DBG, "VC2_CURRENT_CURS_X_LOC:    0x%x VC2_DID_FRAME_PTR: 0x%x \n", _rcv3, _rcv4);

	MCO_VC2_GETREG(mgbase, VC2_DID_LINE_TABLE_PTR, _rcv1);
	msg_printf(DBG, "VC2_DID_LINE_TABLE_PTR: 0x%x\n", _rcv1);

	MCO_VC2_GETREG(mgbase, VC2_DC_CONTROL, _rcv2);
	msg_printf(DBG, "VC2_DC_CONTROL: 0x%x\n", _rcv2);

	MCO_VC2_GETREG(mgbase, VC2_CONFIG, _rcv2);
	msg_printf(DBG, "VC2_CONFIG: 0x%x\n", _rcv2);

}

#define DIBPOINTER_REG                  0x0
#define DIBTOPSCAN_REG                  0x4
#define DIBSKIP_REG                     0x8
#define DIBCTL0_REG                     0xC
#define DIBCTL1_REG                     0x10
#define DIBCTL2_REG                     0x14
#define DIBCTL3_REG                     0x18
#define RETRY_REG                       0x1C
#define REFCTL_REG                      0x20

#define REF_RAC_CTL                     0x0
#define RE_RAC_DATA                     0x4

int
_mco_initVC2(ramVC2Def *vc2vals)
{

	int errors = 0;

#ifdef NOTYET
    msg_printf(VRB,"mco_initVC2: MGRAS VC2 flag = 0x%x\n", 
    vc2vals->flag);
    msg_printf(VRB,"mco_initVC2: MGRAS VOF WidthxHeight = %d x %d\n", 
    vc2vals->w, vc2vals->h);
    msg_printf(VRB,"mco_initVC2: MGRAS VC2 Frame Table size = %d (0x%x)\n", 
    vc2vals->ftab_len, vc2vals->ftab_len);
    msg_printf(VRB,"mco_initVC2: MGRAS VC2 Frame Table Addr = 0x%x (%d)\n", 
    vc2vals->ftab_addr, vc2vals->ftab_addr);
    msg_printf(VRB,"mco_initVC2: MGRAS VC2 Line Table size = %d (0x%x)\n", 
    vc2vals->ltab_len, vc2vals->ltab_len);
    msg_printf(VRB,"mco_initVC2: MGRAS VC2 Config Reg = 0x%x\n", 
    vc2vals->config);
#endif /* NOTYET */

	/* Reset the VC2 (necessary whenever we change the pixel clock) */
	mco_set_dcbctrl(mgbase, 4);
	MCO_VC2_SETREG(mgbase,VC2_CONFIG, 0);
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, vc2vals->config);

	/* Load Timing Tables  here */
	mco_LoadVC2SRAM(VC2_VID_LINE_TAB_ADDR, vc2vals->ltab, vc2vals->ltab_len, DATA16);
	msg_printf(DBG, "_mco_initVC2 :: VC2 Timing Loaded\n");
 	errors = _mco_Verify_Sram_Load(VC2_VID_LINE_TAB_ADDR, vc2vals->ltab, vc2vals->ltab_len);

	MCO_VC2_SETREG(mgbase, VC2_VIDEO_ENTRY_PTR, vc2vals->ftab_addr);

	mco_LoadVC2SRAM(VC2_VID_FRAME_TAB_ADDR, vc2vals->ftab, vc2vals->ftab_len, DATA16);
 	errors += _mco_Verify_Sram_Load(VC2_VID_FRAME_TAB_ADDR, vc2vals->ftab, vc2vals->ftab_len);

	MCO_VC2_SETREG(mgbase, VC2_SCAN_LENGTH, (vc2vals->w) << 5);
	MCO_VC2_SETREG(mgbase, VC2_DC_CONTROL, vc2vals->flag);

	msg_printf(DBG, "_mco_initVC2 :: Initialize Completed\n");

	return (errors);
}

void mco_StartTiming(void) 
{
	__uint32_t dcr ;

	msg_printf(DBG, "Starting  Video Timing \n");
	dcr = (VC2_ENA_DISPLAY | VC2_ENA_VIDEO | VC2_ENA_DIDS);

	MCO_VC2_SETREG(mgbase,VC2_DC_CONTROL, dcr);

#ifdef VERBOSE
	_vc2_dump_regs(mgbase);
#endif

	msg_printf(VRB, "Started  Video Timing \n");
}

void 
mco_StopTiming(void) 
{
	__uint32_t dcr = 0 ;

	MCO_VC2_GETREG(mgbase,VC2_DC_CONTROL, dcr);
	msg_printf(DBG, "Stopping Video Timing \n");
	dcr &= ~(VC2_ENA_DISPLAY | VC2_ENA_VIDEO | VC2_ENA_DIDS);

	MCO_VC2_SETREG(mgbase,VC2_DC_CONTROL, dcr);

#ifdef VERBOSE
	_vc2_dump_regs(mgbase);
#endif

	msg_printf(VRB, "Stopped  Video Timing \n");
}

/*ARGSUSED*/
int
mco_VC2Init(int argc, char **argv)
{
	__uint32_t TimingToken;

	atohex(argv[1], (int *)&TimingToken);
#ifdef NOTYET
	return (_mco_VC2Init(TimingToken));
#endif /* NOTYET */
	msg_printf(VRB,"mco_vc2init is not available\n");
	return (0);
}

/*************************************************************************/
__uint32_t 
mco_VC2AddrsBusTest(void) 
{
	ushort_t index	= 0;
	__uint32_t errors  	= 0;
	ushort_t length = 1;
	ushort_t addr	= 0;
	ushort_t mask 	= 0xffff;
	ushort_t exp    = 0xdead;
	ushort_t rcv	= 0xbeef;


	mco_MaxPLL_NoTiming();

	mco_set_dcbctrl(mgbase, 4);

	/* reset VC2 */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, 0);
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);


#ifdef NOTYET
	msg_printf(VRB, "VC2 SRAM Address Bus: Walking Ones & Zeros Test\n");
#endif /* NOTYET */
	for(index=0; index<sizeof(walk_1_0)/sizeof(ushort_t); index++) {
		rcv = 0xdead;
		addr = walk_1_0[index] & VC2_RAM_MASK ;
		exp = ~walk_1_0[index];
		mco_LoadVC2SRAM(addr, &exp,  length, DATA16);

		mco_set_dcbctrl(mgbase, 2);
		CORRUPT_DCB_BUS(mgbase);

		mco_set_dcbctrl(mgbase, 4);
		mco_GetVC2SRAM(addr,  &rcv,  length, &exp, DATA16);
		COMPARE("VC2 SRAM Addr Bus Test", addr, exp, rcv, mask, errors);
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_VC2_ADDR_BUS_TEST]), errors);
}


__uint32_t 
mco_VC2DataBusTest(void) 
{
	ushort_t index  = 0;
	ushort_t addr   = 0;
	__uint32_t errors = 0;
	ushort_t exp    = 0xdead;
	ushort_t rcv    = 0xdead;

	mco_MaxPLL_NoTiming();

	/* Reset the VC2 (necessary whenever we change the pixel clock) */
	mco_set_dcbctrl(mgbase, 4);
	MCO_VC2_SETREG(mgbase,VC2_CONFIG, 0);

	/* Now enable the VC2 */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Enable the VC2\n");
#endif /* NOTYET */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

	mco_set_dcbctrl(mgbase, 4);
	for(index=0; index< sizeof(walk_1_0)/sizeof(ushort_t); index++) {
		rcv = 0xdead;
		exp = walk_1_0[index];
		mco_LoadVC2SRAM(addr, &exp, 1, DATA16);
		msg_printf(DBG, "mco_VC2DataBusTest: Loaded the data 0x%x\n", exp);

		mco_set_dcbctrl(mgbase, 2);
		CORRUPT_DCB_BUS(mgbase);

		mco_set_dcbctrl(mgbase, 4);
		mco_GetVC2SRAM(addr, &rcv, 1, &exp, DATA16);
		COMPARE ("VC2 SRAM Data Bus Test", addr, exp, rcv, 0xffff, errors);
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_VC2_DATA_BUS_TEST]) ,errors);
}

__uint32_t 
mco_VC2AddrsUniqTest(void) 
{
	ushort_t addr   	= 0;
	__uint32_t errors 	= 0;
	ushort_t length	= 1;
	ushort_t mask 	= 0xffff;
	ushort_t data 	= 0;
	ushort_t rcv  	= 0xdead;
	ushort_t exp  	= 0xbeef;

	mco_MaxPLL_NoTiming();

	/* Reset the VC2 (necessary whenever we change the pixel clock) */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Reset the VC2\n");
#endif /* NOTYET */
	mco_set_dcbctrl(mgbase, 4);
	MCO_VC2_SETREG(mgbase,VC2_CONFIG, 0);

	/* Now enable the VC2 */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Enable the VC2\n");
#endif /* NOTYET */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

	mco_set_dcbctrl(mgbase, 4);
	for (addr=0,data=0; addr < VC2_RAMSIZE; addr++, data++) {
		mco_LoadVC2SRAM(addr, &data, length, DATA16);
	}

	rcv = 0xbeef;
        for (addr=0, exp=addr; addr < VC2_RAMSIZE; addr++, exp++) {
		mco_GetVC2SRAM(addr, &rcv, length, &exp, DATA16);
		COMPARE("VC2 SRAM Address Uniqueness Test", addr, exp, rcv, mask, errors);
        }

	for (addr=0,data=~addr; addr < VC2_RAMSIZE; addr++, data =~addr) {
		mco_LoadVC2SRAM(addr, &data, length, DATA16);
	}
	
	rcv = 0xbeef;
        for (addr=0, exp=~addr; addr < VC2_RAMSIZE; addr++, exp = ~addr) {
		mco_GetVC2SRAM(addr, &rcv, length, &exp, DATA16);
		COMPARE("VC2 SRAM Address Uniqueness Test", addr, exp, rcv, mask, errors);
        }

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_VC2_ADDR_UNIQ_TEST]), errors);
}

__uint32_t 
mco_VC2PatrnTest(void) 
{
	ushort_t addr  	= 0;
	ushort_t loop	= 0;
	__uint32_t errors 	= 0;
	ushort_t length	= 1;
	ushort_t mask	= 0xffff;
	ushort_t rcv   	= 0xbeef;
	ushort_t *patrn1, *patrn2, *patrn3, *patrn4, *patrn5, *patrn6;

	mco_MaxPLL_NoTiming();


	/* Reset the VC2 (necessary whenever we change the pixel clock) */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Reset the VC2\n");
#endif /* NOTYET */
	mco_set_dcbctrl(mgbase, 4);
	MCO_VC2_SETREG(mgbase,VC2_CONFIG, 0);

	/* Now enable the VC2 */
#ifdef NOTYET
	msg_printf(VRB,"VC2Reset: Enable the VC2\n");
#endif /* NOTYET */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);

	mco_set_dcbctrl(mgbase, 4);
	for(loop=0; loop < 6; loop++) {
	    addr = 0;
	    patrn1 = (ushort_t *)(mco_patrn + loop + 0);
	    patrn2 = (ushort_t *)(mco_patrn + loop + 1);
	    patrn3 = (ushort_t *)(mco_patrn + loop + 2);
	    patrn4 = (ushort_t *)(mco_patrn + loop + 3);
	    patrn5 = (ushort_t *)(mco_patrn + loop + 4);
	    patrn6 = (ushort_t *)(mco_patrn + loop + 5);
	    msg_printf(DBG, "Filling SRAM sequentially with %x %x %x %x %x %x\n", *patrn1, *patrn2, *patrn3, *patrn4, *patrn5, *patrn6);

	    length = 1;
	    for(addr=0; addr < VC2_RAMSIZE/6; ) {
		mco_LoadVC2SRAM(addr++, patrn1, 1, DATA16);
		mco_LoadVC2SRAM(addr++, patrn2, 1, DATA16);
		mco_LoadVC2SRAM(addr++, patrn3, 1, DATA16);
		mco_LoadVC2SRAM(addr++, patrn4, 1, DATA16);
		mco_LoadVC2SRAM(addr++, patrn5, 1, DATA16);
		mco_LoadVC2SRAM(addr++, patrn6, 1, DATA16);
            }
            mco_LoadVC2SRAM(addr++, patrn1, 1, DATA16);
            mco_LoadVC2SRAM(addr++, patrn2, 1, DATA16);


	    rcv = 0xbeef;
            for (length=1, addr=0; addr < VC2_RAMSIZE/6; ) {
		mco_GetVC2SRAM(addr, &rcv, length, patrn1, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn1, rcv, mask, errors);

		addr++;
		mco_GetVC2SRAM(addr, &rcv, length, patrn2, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn2, rcv, mask, errors);

		addr++;
		mco_GetVC2SRAM(addr, &rcv, length, patrn3, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn3, rcv, mask, errors);

		addr++;
		mco_GetVC2SRAM(addr, &rcv, length, patrn4, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn4, rcv, mask, errors);

		addr++;
		mco_GetVC2SRAM(addr, &rcv, length, patrn5, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn5, rcv, mask, errors);

		addr++;
		mco_GetVC2SRAM(addr, &rcv, length, patrn6, DATA16);
		COMPARE ("VC2 SRAM Pattern Test", addr, *patrn6, rcv, mask, errors);
		addr++;
            }

	    mco_GetVC2SRAM(addr, &rcv, length, patrn1, DATA16);
	    COMPARE ("VC2 SRAM Pattern Test", addr, *patrn1, rcv, mask, errors);

	    addr++;
	    mco_GetVC2SRAM(addr, &rcv, 1, patrn2, DATA16);
	    COMPARE ("VC2 SRAM Pattern Test", addr, *patrn2, rcv, mask, errors);
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_VC2_PATRN_TEST]), errors);
}

/***************************************************************************/
/*                                                                         */
/*      mco_PokeVC2Ram() - Write VC2 SRAM  Location                      */
/*                                                                         */
/*      This is a tool to allow Writing to various VC2 SRAM Locations      */
/*      from the IDE command line.                                         */
/*                                                                         */
/***************************************************************************/
int 
mco_PokeVC2Ram(int argc, char **argv)
{
	int  loopcount;
	__uint32_t  Addr;
	ushort_t Data;
	ushort_t  length;
	struct range range;
	__uint32_t bad_arg = 1;
	char	**argv_save = argv;

	loopcount = 1;

	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'a':
		    /* Address Range */
		    if (argv[0][2]=='\0') {
			--bad_arg;
			if (!(parse_range(&argv[1][0], sizeof(ushort_t), &range))) {
			    msg_printf(SUM, "Syntax error in address range\n");
			    bad_arg++;
			}
			msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
			argc--; argv++;
		    } else {
			--bad_arg;
			if (!(parse_range(&argv[0][2], sizeof(ushort_t), &range))) {
			    msg_printf(SUM, "Syntax error in address range\n");
			    bad_arg++;
			}
			msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
		    }

		    break;

		case 'd':
		    /* Data to be Written */
		    if (argv[0][2]=='\0') {	/* Skip White Space */
					atohex(&argv[1][0], (int *)&Data);
					argc--; argv++;
		    } else {
					atohex(&argv[0][2], (int *)&Data);
		    }
		    msg_printf(SUM, "Data = %x\n", Data);
		    break;
		case 'l':
		    if (argv[0][2]=='\0') { /* has white space */
			atohex(&argv[1][0], (int *)&loopcount);
			argc--; argv++;
		    }
		    else { /* no white space */
			atohex(&argv[0][2], (int *)&loopcount);
		    }
		    msg_printf(SUM, "loopcount = %d\n", loopcount);
		    if (loopcount < 0) {
			msg_printf(SUM, "Error. Loop must be > 0\n");
			bad_arg++;
		    }
		    break;
		default: 
		    bad_arg++; 
		    msg_printf(SUM, "**** Shouldn't be here ****\n");
		    break;
	    }
	    argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "argc = %d\n", argc);
		msg_printf(SUM, "\n\n[-l loopcount] -a Addrs Range  -d Data\n");
		return -1;
	}
	
	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 7);

	Addr = (__uint32_t)range.ra_base;
	length = (ushort_t)range.ra_count * 2;
	for(; loopcount > 0; --loopcount) {
	    msg_printf(SUM, "%s Loop %d Loading VC2 Ram \n" ,
	    argv_save[0], loopcount);
	    mco_LoadVC2SRAM(Addr, &Data, length, DATA16);
	}

	return 0;

}

/***************************************************************************/
/*                                                                         */
/*      mco_PeekVC2Ram() - Read VC2 RAM Location			   */
/*	mco_peekvc2ram -a start:end  [-l loopcnt]			   */
/*	mco_peekvc2ram -a start#count  [-l loopcnt]			   */
/*	mco_peekvc2ram -a start  [-l loopcnt]				   */
/*                                                                         */
/*      This is a tool to allow Reading of various VC2 SRAM Locations      */
/*      from the IDE command line.                                         */
/*                                                                         */
/***************************************************************************/
int 
mco_PeekVC2Ram(int argc, char **argv)
{
	int  loopcount;
	__uint32_t  Addr;
	__uint32_t  length, index;
	ushort_t *Data, *Data_save, exp;
	struct range range;
	__uint32_t bad_arg = 1;
#if 0
	char    **argv_save = argv;
#endif

	loopcount = 1;

	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'a':
		    /* Address Range */
		    if (argv[0][2]=='\0') {
			--bad_arg;
			if (!(parse_range(&argv[1][0], sizeof(ushort_t), &range))) {
			    msg_printf(SUM, "Syntax error in address range\n");
			    bad_arg++;
			}
			msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));	
			argc--; argv++;
		    }
		    else {
			--bad_arg;
			if (!(parse_range(&argv[0][2], sizeof(ushort_t), &range))) {
			    msg_printf(SUM, "Syntax error in address range\n");
			    bad_arg++;
			}
			msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));	
		    }
		    break;

		case 'l':
		    if (argv[0][2]=='\0') { /* has white space */
			atohex(&argv[1][0], (int *)&loopcount);
			argc--; argv++;
		    }
		    else { /* no white space */
			atohex(&argv[0][2], (int *)&loopcount);
		    }
		    if (loopcount < 0) {
			msg_printf(SUM, "Error. Loop must be > 0\n");
			bad_arg++;
		    }
		    break;
		default: 
		    bad_arg++; 
		    break;

	    }
	    argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
	    msg_printf(SUM, "argc = %d\n", argc);
	    msg_printf(SUM, "[-l loopcount] -a Addrs Range \n");
	    return -1;
	}
	
	Addr = (__uint32_t)range.ra_base;
	length = (ushort_t)range.ra_count * 2;

        if ((Data = ( ushort_t *) malloc(length * sizeof(ushort_t))) == NULL) {
	  /* msg_printf(SUM, "%s :  Cannot malloc buffer\n", argv_save[0]); */
	    msg_printf(SUM, "Cannot malloc buffer\n");
	    return -1;
        }

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 7);

	Data_save = Data ;
	for(; loopcount > 0; --loopcount) {
	    Data = Data_save;
	    mco_GetVC2SRAM(Addr, Data, length, &exp, DATA16);

	    Data = Data_save;
	    for(index=0; index < length; index++) {
		msg_printf(SUM, ":  loop %d Addr= 0x%x Data= 0x%x\n" ,
		loopcount, (Addr + index), *(Data + index));
	    }

	}

	return 0;

}

