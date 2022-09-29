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
 *  PP1-XMAP Diagnostics.
 *
 *  $Revision: 1.34 $
 */

#include <math.h>
#include <libsc.h>
#include "uif.h"
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "ide_msg.h"
#include "mgras_diag.h"

/**************************************************************************/

#ifndef IP28

int 
_mgras_XmapDibTest(char *str,  __uint32_t addr)
{
	 __uint32_t i 	= 0;
	 __uint32_t errors = 0;
	 __uint32_t rcv 	= 0xdeadbeef; 
	 __uint32_t exp 	= 0xdeadbeef; 
	 __uint32_t mask 	= ~0;

	msg_printf(DBG, "%s\n", str);

#if 0
	mgras_video_toggle(0);		/* Diable the timing */
#endif

	for(i=0; i<sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); i++) {
		mgras_xmapSetAddr(mgbase,addr);
		exp = walk_1_0_32[i] & mask;
		mgras_xmapSetDIBdata(mgbase, exp);

		rcv = 0xdead;
		mgras_xmapSetAddr(mgbase,addr);
		mgras_xmapGetDIBdata(mgbase, rcv);
		COMPARE(str, addr, exp, rcv, mask, errors);
	}

#if 0
	mgras_video_toggle(1);		/* Enable the timing */
#endif
	return (errors);
}
#endif

#define	XMAP_DCB_AUTOINC	(1 << 19)

#ifdef MGRAS_BRINGUP
 __uint32_t	pp1_number;
#endif /* MGRAS_BRINGUP */

int
_mgras_XmapDcbRegTest( __uint32_t WhichPP1) 
{
	 __uint32_t i	= 0;
	 __uint32_t IndexReg	= 0;
	 __uint32_t errors = 0;
	 __uint32_t mask 	= 0xffffffff;
	 __uint32_t rcv	= 0xdeadbeef;
	 __uint32_t exp	= 0xdeadbeef;

#if 0
	mgras_video_toggle(0);		/* Diable the timing */
#endif

	/* 0. PP1 Select Register is Write Only - Cannot Test */
	/* 1. Index Register is Write Only - Cannot Test */
	msg_printf(VRB, "MGRAS_XMAP_DCB Register Test\n");

	mgras_xmapSetPP1Select(mgbase, MGRAS_XMAP_SELECT_PP1(WhichPP1));

#ifdef MGRAS_BRINGUP
	pp1_number = WhichPP1;
#endif /* MGRAS_BRINGUP */

	/*
 	 * Set auto increment in config register
   	 */
	/* Select Second byte of the config reg :- for AutoInc */
	mgras_xmapSetAddr(mgbase, 0x1) ;	/* Do NOT REMOVE THIS */
	mgras_xmapSetConfig(mgbase, 0xff000000); /* Hack for Auto Inc */
#if 0
	mgras_xmapSetConfig(mgbase, 0x48000000); /* Hack for Auto Inc */
#endif

	rcv = 0x2222beef;
	exp = 0x2222beef;
	mgras_xmapSetAddr(mgbase,MGRAS_XMAP_REV_ADDR);
	mgras_xmapGetConfig(mgbase, rcv);
	msg_printf(DBG, "XMAP Revision %x\n" ,rcv);
	if ((rcv != 0x0) && (rcv != 0x1)) {
		msg_printf(ERR, "XMAP Revision exp 0 or 1, rcv 0x%x\n" ,rcv);
		errors++;
	}

	mgras_xmapSetAddr(mgbase,MGRAS_XMAP_FIFOCAP_ADDR);
	rcv = 0x3333beef;
	exp = 0x3333beef;
	mgras_xmapGetConfig(mgbase, rcv);
	msg_printf(DBG, "XMAP FIFO CAPACITY is %x\n" ,rcv);
	if ((rcv & 0xf) != 0x1) {
		msg_printf(ERR, "XMAP FIFO CAPACITY  exp 0%x rcv 0x%x\n" ,0x1, (rcv & 0xf));
		errors++;
	}


	msg_printf(DBG, "XMAP MAIN BUFFER Select Register\n");

	for(i=0; i<sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); i++) {
		mgras_xmapSetAddr(mgbase,MGRAS_XMAP_MAINBUF_ADDR);
		mask = ~0 ;
		exp =  walk_1_0_32[i] & mask;
		mgras_xmapSetBufSelect(mgbase,  exp);

		mgras_xmapSetAddr(mgbase,MGRAS_XMAP_MAINBUF_ADDR);
		rcv= 0x4444dead;
		mgras_xmapGetBufSelect(mgbase, rcv);
		COMPARE("XMAP MAIN BUFFER SELECT REGISTER", 0,  exp, rcv, mask, errors);
	}

	msg_printf(DBG, "XMAP OVERLAYBUF Select Register\n");
	for(i=0; i<sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); i++) {
		mgras_xmapSetAddr(mgbase, MGRAS_XMAP_OVERLAYBUF_ADDR);
		mask = (1 << 8) - 1 ;
#if 0
		mask = ~0;
#endif
		exp = walk_1_0_32[i] & mask;
		mgras_xmapSetBufSelect(mgbase,  exp);

		mgras_xmapSetAddr(mgbase, MGRAS_XMAP_OVERLAYBUF_ADDR);
		rcv= 0x5555dead;
		mgras_xmapGetBufSelect(mgbase, rcv);
		COMPARE("XMAP OVERLAYBUF BUFFER SELECT REGISTER", 0,  exp, rcv, mask, errors);
	}
	
	msg_printf(DBG, "XMAP MAIN MODE Register File\n");
	for(IndexReg=0; IndexReg < 32; IndexReg++ ) {
		for(i=0; i<sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); i++) {
			mgras_xmapSetAddr(mgbase, IndexReg);
			mask = (1 << 23) - 1;
#if 0
			mask = ~0;
#endif
			exp =  walk_1_0_32[i] & mask;
			mgras_xmapSetMainMode(mgbase, 0, exp);

			mgras_xmapSetAddr(mgbase, IndexReg);
			rcv= 0x6666dead;
			mgras_xmapGetMainMode(mgbase, 0, rcv);
			COMPARE("XMAP MAIN MODE Register File" ,IndexReg, exp, rcv, mask, errors);
		}
	}
	
	msg_printf(DBG, "XMAP OVERLAY MODE Register File\n");
	for(IndexReg=0; IndexReg < 8; IndexReg++ ) {
		for(i=0; i<sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); i++) {
			mgras_xmapSetAddr(mgbase, IndexReg);
			mask = (1 << 10) - 1;
#if 0
			mask = ~0;
#endif
			exp = walk_1_0_32[i] & mask;
			mgras_xmapSetOvlMode(mgbase, 0, exp);

			mgras_xmapSetAddr(mgbase, IndexReg);
			rcv= 0x777dead;
			mgras_xmapGetOvlMode(mgbase, 0, rcv);
			COMPARE("XMAP OVERLAY MODE Register File" ,IndexReg, exp, rcv, mask, errors);
		}
	}


	switch (WhichPP1) {
		case 0: REPORT_PASS_OR_FAIL((&BackEnd[XMAP0_DCB_REG_TEST]), 
			errors); 
		case 1: REPORT_PASS_OR_FAIL((&BackEnd[XMAP1_DCB_REG_TEST]), 
			errors);
		case 2: REPORT_PASS_OR_FAIL((&BackEnd[XMAP2_DCB_REG_TEST]), 
			errors); 
		case 3: REPORT_PASS_OR_FAIL((&BackEnd[XMAP3_DCB_REG_TEST]), 
			errors);
		case 4: REPORT_PASS_OR_FAIL((&BackEnd[XMAP4_DCB_REG_TEST]), 
			errors);
		case 5: REPORT_PASS_OR_FAIL((&BackEnd[XMAP5_DCB_REG_TEST]), 
			errors);
		case 6: REPORT_PASS_OR_FAIL((&BackEnd[XMAP6_DCB_REG_TEST]), 
			errors);
		case 7: REPORT_PASS_OR_FAIL((&BackEnd[XMAP7_DCB_REG_TEST]), 
			errors);
	}
	msg_printf(DBG, "End of XMAP Tests.");

#if 0
	mgras_video_toggle(1);		/* Enable the timing */
#endif
	return(errors);
}

int
mgras_XmapDcbRegTest(int argc, char **argv)
{
	int WhichPP1, errors = 0; 

	if (argc == 1) {
		errors += _mgras_XmapDcbRegTest(0);
		errors += _mgras_XmapDcbRegTest(1);
		if (numRE4s == 2) {
			errors += _mgras_XmapDcbRegTest(2);
			errors += _mgras_XmapDcbRegTest(3);
		}
	} else {
		atohex(argv[1], &WhichPP1);
		errors += _mgras_XmapDcbRegTest(WhichPP1);
	}
	return(errors);
}

static PP1_Xmap_Regs Xmap_Regs[] = {
		{"XMAP_PP1_SELECT",		XMAP_PP1_SELECT,		WRITE_ONLY},
		{"XMAP_INDEX",			XMAP_INDEX,			WRITE_ONLY},
		{"XMAP_CONFIG",			XMAP_CONFIG,			WRITE_ONLY},
		{"XMAP_REV",			XMAP_REV,			READ_ONLY},
		{"XMAP_FIFO_CAP",		XMAP_FIFO_CAP,			READ_ONLY},

		{"XMAP_BUF_SELECT",		XMAP_BUF_SELECT,		READ_WRITE},
		{"XMAP_OVL_BUF_SELECT",		XMAP_OVL_BUF_SELECT,		READ_WRITE},
		{"XMAP_MAIN_MODE",		XMAP_MAIN_MODE,			READ_WRITE},
		{"XMAP_OVERLAY_MODE",		XMAP_OVERLAY_MODE,		READ_WRITE},

		{"XMAP_DIB_PTR",		XMAP_DIB_PTR,			WRITE_ONLY},
		{"XMAP_DIB_TOPSCAN",		XMAP_DIB_TOPSCAN,		WRITE_ONLY},
		{"XMAP_DIB_SKIP",		XMAP_DIB_SKIP,			WRITE_ONLY},
		{"XMAP_DIB_CTRL0",		XMAP_DIB_CTRL0,			WRITE_ONLY},
		{"XMAP_DIB_CTRL1",		XMAP_DIB_CTRL1,			WRITE_ONLY},
		{"XMAP_DIB_CTRL2",		XMAP_DIB_CTRL2,			WRITE_ONLY},
		{"XMAP_DIB_CTRL3",		XMAP_DIB_CTRL3,			WRITE_ONLY},

		{"XMAP_RE_RAC_CTRL",		XMAP_RE_RAC_CTRL,		WRITE_ONLY},
		{"XMAP_RE_RAC_DATA",		XMAP_RE_RAC_DATA,		READ_WRITE},
		{"",				0,				0},

};

__uint32_t
GetXmapIndex (char *regname)
{
	__uint32_t index;

	msg_printf(DBG, "GetXmapIndex(regname=%s)\n", regname);
	for(index=0; ;index++) {
		msg_printf(DBG, "Xmap_Regs[index].str=%s\n", Xmap_Regs[index].str);
#if 0
		if (!strcmp(Xmap_Regs[index].str, NULL)) {
			msg_printf(DBG, "Hit the bottom\n");
			return(index);
		}
#endif
		if (!strcmp(Xmap_Regs[index].str, regname)) {
			msg_printf(DBG, "Found the reg\n");
			return(index);
		} 
	}
}

#ifdef MFG_USED
#ifndef IP28
/***************************************************************************/
/*									   */
/* 	mgras_PeekXMAP() - Read XMAP register				   */
/*									   */
/*      This is a tool to allow Reading of various XMAP registers from the  */
/*	IDE command line.			   			   */
/*									   */
/***************************************************************************/
int 
mgras_PeekXMAP(int argc, char **argv)
{
	char XmapReg[32]; 
	 __uint32_t TestToken;
	 __int32_t loopcount;
	__uint32_t bad_arg = 0;
	 __uint32_t rcv        = 0xdeadbeef;
	
	MGRAS_GFX_CHECK();
	loopcount = 1;
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch(argv[0][1]) {
			case 'r' :
				/* Register Select */	
				if (argv[0][2]=='\0') {
					sprintf(XmapReg, "%s", &argv[1][0]);
					argc--; argv++;
				} else {
					sprintf(XmapReg, "%s", &argv[0][2]);
				}
				break;
                        case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atob(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atob(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount < 0) {
                                        msg_printf(SUM, "%s Error. Loop must be > or = to 0\n", argv[0]);
                                        return (0);
                                }
                                break;
                        default:
                                bad_arg++;
                                break;

		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
                msg_printf(SUM, "%s [-l loopcount] -r XmapRegister  \n", argv[0]);
		return -1;
        }

	
	TestToken = GetXmapIndex(XmapReg);
	msg_printf(DBG, "TestToken = 0x%x\n", TestToken);

	if ((Xmap_Regs[TestToken].Mode == 0) || (Xmap_Regs[TestToken].Mode == WRITE_ONLY)) {
			return -1;
	}

#if 0
	for(; loopcount >= 0;loopcount--) {
#endif
		switch(Xmap_Regs[TestToken].reg) {
                	case XMAP_REV :
				mgras_xmapSetAddr(mgbase,MGRAS_XMAP_REV_ADDR);
				mgras_xmapGetConfig(mgbase, rcv);
			break;

                	case XMAP_FIFO_CAP :
				mgras_xmapSetAddr(mgbase,MGRAS_XMAP_FIFOCAP_ADDR);
				mgras_xmapGetConfig(mgbase, rcv);
			break;

                	case XMAP_BUF_SELECT :
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase,MGRAS_XMAP_MAINBUF_ADDR);
				mgras_xmapGetBufSelect(mgbase, rcv);
				mgras_video_toggle(1);		/* Enable the timing */

			break;

                	case XMAP_OVL_BUF_SELECT :
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, MGRAS_XMAP_OVERLAYBUF_ADDR);
				mgras_xmapGetBufSelect(mgbase, rcv);	
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_MAIN_MODE :
				/* Assumed :- User has Programmed Index Reg Properly :- RAW MODE */
				mgras_xmapGetMainMode(mgbase, 0, rcv);
			break;

                	case XMAP_OVERLAY_MODE :
				/* Assumed :- User has Programmed Index Reg Properly :- RAW MODE */
				mgras_xmapGetOvlMode(mgbase, 0, rcv);
			break;

                	case XMAP_RE_RAC_DATA :
				mgras_xmapSetAddr(mgbase, 4);
				mgras_xmapGetRE_RAC(mgbase,rcv);
			break;

		}
		msg_printf(SUM, "%s loopcount %d Read XmapReg %s Rcv 0x%x\n" ,argv[0], loopcount, XmapReg, rcv);
#if 0
	}
#endif

	return 0 ;
}
#endif /* ifndef IP28 */
#endif /* MFG_USED */

/***************************************************************************/
/*									   */
/* 	mgras_PokeXMAP() - Read XMAP register				   */
/*									   */
/*      This is a tool to allow Reading of various XMAP registers from the  */
/*	IDE command line.			   			   */
/*									   */
/***************************************************************************/
int 
mgras_PokeXMAP(int argc, char **argv)
{
	char XmapReg[32]; 
	 __uint32_t TestToken;
	 __int32_t loopcount;
	__uint32_t bad_arg = 0;
	 __uint32_t Data        = 0xdeadbeef;
	 __uint32_t broadcast  = 0;
	
	/* MGRAS_GFX_CHECK(); */
	loopcount = 1;
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch(argv[0][1]) {
			case 'r' :
				/* Register Select */	
				if (argv[0][2]=='\0') {
#ifdef DEBUG
					msg_printf(DBG, "argv[1][0] = %s\n", &(argv[1][0]));
#endif
					/* XXX use strncpy here! */
					sprintf(XmapReg, "%s", &argv[1][0]);
					argc--; argv++;
				} else {
#ifdef DEBUG
					msg_printf(DBG, "argv[0][2] = %s\n", &(argv[0][2]));
#endif
					/* XXX use strncpy here! */
					sprintf(XmapReg, "%s", &argv[0][2]);
				}
				break;
                        case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atob(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atob(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount < 0) {
                                        msg_printf(SUM, "%s Error. Loop must be > or = to 0\n", argv[0]);
                                        return (0);
                                }
                                break;
                        case 'd':
                                /* Data to be Written */
                                if (argv[0][2]=='\0') { /* Skip White Space */
                                        atob(&argv[1][0], (int*)&Data);
                                        argc--; argv++;
                                } else {
                                        atob(&argv[0][2], (int*)&Data);
                                }
                                break;
                        default:
                                bad_arg++;
                                break;

		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
                msg_printf(SUM, "%s [-l loopcnt] -r XmapRegister\n", argv[0]);
		return -1;
        }

	
	TestToken = GetXmapIndex(XmapReg);
#ifdef DEBUG
	msg_printf(DBG, "TestToken = 0x%x\n", TestToken);
#endif

	if ((Xmap_Regs[TestToken].Mode == 0) || (Xmap_Regs[TestToken].Mode == READ_ONLY)) {
			return -1;
	}

#if 0
	for(; loopcount >= 0;loopcount--) {
#endif
		switch(Xmap_Regs[TestToken].reg) {
                	case XMAP_PP1_SELECT:
#ifdef DEBUG
				msg_printf(DBG, "mgras_xmapSetPP1Select(0x%x, 0x%x)\n" ,mgbase, ((Data<<1) | broadcast));
#endif
				mgras_xmapSetPP1Select(mgbase, ((Data<<1) | broadcast));
			break;

                	case XMAP_INDEX:
#ifdef DEBUG
				msg_printf(DBG, "mgras_xmapSetAddr(0x%x 0x%x)\n" ,mgbase, Data);
#endif
				mgras_xmapSetAddr(mgbase, Data);
			break;

                	case XMAP_CONFIG:
				mgras_xmapSetAddr(mgbase,MGRAS_XMAP_CONFIG_ADDR);
				mgras_xmapSetConfig(mgbase, Data);
			break;

                	case XMAP_BUF_SELECT:
				msg_printf(DBG, "XMAP_BUF_SEL\n");
				mgras_xmapSetAddr(mgbase, Data);
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase,MGRAS_XMAP_MAINBUF_ADDR);
				mgras_xmapSetBufSelect(mgbase,  Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_OVL_BUF_SELECT:
				msg_printf(DBG, "XMAP_OVL_BUF_SEL\n");
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, MGRAS_XMAP_OVERLAYBUF_ADDR);
				mgras_xmapSetBufSelect(mgbase,  Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_MAIN_MODE:
				msg_printf(DBG, "XMAP_MAIN_MODE\n");
				mgras_xmapSetMainMode(mgbase, 0, Data);
			break;

                	case XMAP_OVERLAY_MODE:
				mgras_xmapSetOvlMode(mgbase, 0, Data);
			break;


                	case XMAP_DIB_PTR:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 0);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_TOPSCAN:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 4);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_SKIP:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 8);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_CTRL0:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 0xC);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_CTRL1:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 0x10);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_CTRL2:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 0x14);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

                	case XMAP_DIB_CTRL3:
				mgras_video_toggle(0);		/* Diable the timing */
				mgras_xmapSetAddr(mgbase, 0x18);
				mgras_xmapSetDIBdata(mgbase, Data);
				mgras_video_toggle(1);		/* Enable the timing */
			break;

			default :
				msg_printf(SUM, "%s Selected Register %s Cannot be Written to.\n" ,argv[0], XmapReg);
			break;

		}
		msg_printf(DBG, "%s loopcount %d Wrote XmapReg %s Data 0x%x\n" ,argv[0], loopcount, XmapReg, Data);
#if 0
	}
#endif

	return 0 ;
}

__uint32_t
_mgras_disp_ctrl(int ppflag, int ppnum, int reflag, int renum, int opflag, int operation)
{
	__uint32_t select;
	__uint32_t enable = 0x1bb, disable = 0xbb;
	
	if (ppflag || reflag) {
		select = MGRAS_XMAP_SELECT_PP1((renum << 1) | ppnum);
	} else {
		select = MGRAS_XMAP_SELECT_PP1(MGRAS_XMAP_SELECT_BROADCAST);
	}
	mgras_xmapSetPP1Select(mgbase, select);
	mgras_xmapSetAddr(mgbase, 0xc); /* dibctrl0 */

	if (operation) {
		mgras_xmapSetDIBdata(mgbase, enable);
	} else {
		mgras_xmapSetDIBdata(mgbase, disable);
	}

	select = MGRAS_XMAP_SELECT_PP1(MGRAS_XMAP_SELECT_BROADCAST);
	mgras_xmapSetPP1Select(mgbase, select);

	return 0;
}

int 
mgras_disp_ctrl(int argc, char **argv)
{
	__uint32_t bad_arg = 0, ppflag = 0, reflag = 0, opflag = 0;
	__uint32_t ppnum = 0, renum = 0, operation = 0;
	
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch(argv[0][1]) {
                        case 'p':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atobu(&argv[1][0], &ppnum);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atobu(&argv[0][2], &ppnum);
                                }
				ppflag = 1;
                                break;
                        case 'r':
                                if (argv[0][2]=='\0') { /* Skip White Space */
                                        atobu(&argv[1][0], &renum);
                                        argc--; argv++;
                                } else {
                                        atobu(&argv[0][2], &renum);
                                }
				reflag = 1;
                                break;
                        case 'o':
                                if (argv[0][2]=='\0') { /* Skip White Space */
                                        atobu(&argv[1][0], &operation);
                                        argc--; argv++;
                                } else {
                                        atobu(&argv[0][2], &operation);
                                }
				opflag = 1;
                                break;
                        default:
                                bad_arg++;
                                break;

		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc) || (!opflag)) {
                msg_printf(SUM, "%s -o [0|1] [-p ppnum] [-r renum]\n", argv[0]);
		return -1;
        }

	_mgras_disp_ctrl(ppflag, ppnum, reflag, renum, opflag, operation);

	return 0;
}
