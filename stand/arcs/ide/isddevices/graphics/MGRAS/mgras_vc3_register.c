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
 *  VC3 Internal Register Tests 
 *  $Revision: 1.25 $
 *
 *	mgras_VC3InternalRegTest();
 *	mgras_PokeVC3();
 *	mgras_PeekVC3();
 */
#include "libsc.h"
#include <math.h>
#include <uif.h>
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "ide_msg.h"
#include "mgras_diag.h"

__uint32_t 
mgras_VC3RegTest(char *RegStr,  ushort_t reg,  __uint32_t patrn) 
{
	 ushort_t rcv = 0xbeef;

	mgras_vc3SetReg(mgbase, reg, patrn);
	mgras_vc3GetReg(mgbase, reg, rcv);
	msg_printf(DBG, "VC3 %s Test reg %d exp %x rcv %x\n",
		  RegStr,reg, patrn, rcv);
	if (rcv != patrn) {
		msg_printf(ERR, "VC3 %s Test -ERROR- reg %d exp %x rcv %x\n",
			RegStr,reg, patrn, rcv);
		return -1;
	} 
	return 0;
}

/***************************************************************************/
/*									   */
/* 	mgras_VC3InternalRegTest :- VC3 Internal Register Tests		   */
/*									   */
/***************************************************************************/
__uint32_t 
mgras_VC3InternalRegTest(void)
{
	 __uint32_t  index  = 0;
	 __uint32_t  errors = 0;
	 ushort_t  *DataPtr;

	MGRAS_GFX_CHECK();
	mgras_MaxPLL_NoTiming();
	msg_printf(VRB, "VC3 Internal Register Test\n");

	DataPtr = walk_1_0;
	mgras_vc3SetReg(mgbase,VC3_DISPLAY_CONTROL, 0x0);
	for(index=0; index <sizeof(walk_1_0)/sizeof(walk_1_0[0]); index++) {
		errors += mgras_VC3RegTest("VC3_VID_EP", 	VC3_VID_EP,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_CURS_EP", 	VC3_CURS_EP,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_CURS_X_LOC", 	VC3_CURS_X_LOC,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_CURS_Y_LOC", 	VC3_CURS_Y_LOC,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_EP1", 	VC3_DID_EP1,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_EP2", 	VC3_DID_EP2,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_SCAN_LENGTH", 	VC3_SCAN_LENGTH,*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_RAM_ADDR", VC3_RAM_ADDR,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_VT_FRAME_TBL_PTR",	VC3_VT_FRAME_TBL_PTR,	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_VT_LINE_SEQ_PTR",	VC3_VT_LINE_SEQ_PTR, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_VT_LINES_RUN",		VC3_VT_LINES_RUN, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_VERT_LINE_COUNT",	VC3_VERT_LINE_COUNT, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_CURS_TBL_PTR",		VC3_CURS_TBL_PTR, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_WORKING_CURS_Y",	VC3_WORKING_CURS_Y, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_CUR_CURSOR_X",		VC3_CUR_CURSOR_X, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_UNUSED",		VC3_UNUSED, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_FRAMETAB1",		VC3_DID_FRAMETAB1, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_LINETAB1",		VC3_DID_LINETAB1, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_FRAMETAB2",		VC3_DID_FRAMETAB2, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DID_LINETAB2",		VC3_DID_LINETAB2, 	*(DataPtr+index));
#if DO_NOT_TOUCH
		errors += mgras_VC3RegTest("VC3_CURS_CONTROL", 		VC3_CURS_CONTROL, 	*(DataPtr+index));
		errors += mgras_VC3RegTest("VC3_DISPLAY_CONTROL", 	VC3_DISPLAY_CONTROL, 	*(DataPtr+index));
#endif
	}
	REPORT_PASS_OR_FAIL((&BackEnd[VC3_REG_TEST]) ,errors);
}

#ifdef MFG_USED
/***************************************************************************/
/*									   */
/* 	mgras_PokeVC3() - Write VC3 register				   */
/*									   */
/*      This is a tool to allow writing of arbitrary values to the various */
/*      VC3 registers from the IDE command line.			   */
/*									   */
/***************************************************************************/
int
mgras_PokeVC3(int argc, char **argv)
{
	 __uint32_t Vc3Reg; 
	 __uint32_t Data;
	 __int32_t loopcount;
	__uint32_t bad_arg = 0;
	
	Data = 0xdeadbeef;
	MGRAS_GFX_CHECK();
	loopcount = 1;
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch(argv[0][1]) {
			case 'r' :
				/* Register Select */	
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&Vc3Reg);
					argc--; argv++;
				} else {
					atob(&argv[0][2], (int*)&Vc3Reg);
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
                        case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atob(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atob(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount < 0) {
                                        msg_printf(SUM, "Error Loop must be > or = to 0\n");
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
                msg_printf(SUM, "%s [-l loopcount] -r VC3Register -d Data\n", argv[0]);
		return -1;
        }

	for(; loopcount > 0;loopcount--) {
		msg_printf(SUM, "%s loopcount %d Writing Reg %d Data 0x%x\n" ,argv[0], loopcount, Vc3Reg, Data);
		mgras_vc3SetReg(mgbase, Vc3Reg, Data);
	}


	return 0 ;
}

/***************************************************************************/
/*									   */
/* 	mgras_PeekVC3() - Read VC3 register				   */
/*									   */
/*      This is a tool to allow Reading of various VC3 registers from the  */
/*	IDE command line.			   			   */
/*									   */
/***************************************************************************/
int 
mgras_PeekVC3(int argc, char **argv)
{
	 __uint32_t Vc3Reg; 
	 ushort_t Data;
	 __int32_t loopcount;
	__uint32_t bad_arg = 0;
	
	Data = 0xdead;
	MGRAS_GFX_CHECK();
	loopcount = 1;
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch(argv[0][1]) {
			case 'r' :
				/* Register Select */	
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&Vc3Reg);
					argc--; argv++;
				} else {
					atob(&argv[0][2], (int*)&Vc3Reg);
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
                                        msg_printf(SUM, "%s Error Loop must be >= to 0\n", argv[0]);
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
                msg_printf(SUM, "%s [-l loopcount] -r VC3Register\n", argv[0]);
		return -1;
        }

	for(; loopcount > 0;loopcount--) {
		mgras_vc3GetReg(mgbase, Vc3Reg, Data);
		msg_printf(SUM, "%s loopcount %d Read Reg %d Data 0x%x\n" ,argv[0], loopcount, Vc3Reg, Data);
	}

	return 0 ;
}
#endif /* MFG_USED */
