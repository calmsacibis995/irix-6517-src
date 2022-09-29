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
 *  VC2 Internal Register Tests 
 *  $Revision: 1.9 $
 *
 *	mco_VC2InternalRegTest();
 *	mco_PokeVC2();
 *	mco_PeekVC2();
 */

#include <libsc.h>
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

extern mgras_hw *mgbase;
extern void mco_set_dcbctrl(mgras_hw *base, int crs);
extern void mco_MaxPLL_NoTiming(void);

/*	Defines for VC2 Registers not found in mgrashw.h */

#define 	VC2_VT_FRAME_TBL_PTR			8
#define 	VC2_VT_LINE_SEQ_PTR			9
#define 	VC2_VT_LINES_RUN			0xa
#define 	VC2_CURS_TBL_PTR			0xc


__uint32_t 
mco_VC2RegTest(char *RegStr,  ushort_t reg,  __uint32_t patrn) 
{
	ushort_t rcv = 0xbeef;

	MCO_VC2_SETREG(mgbase, reg, patrn);
	MCO_VC2_GETREG(mgbase, reg, rcv);
	msg_printf(DBG, "VC2 %s Test reg %d, exp: %x, rcv: %x\n" ,
						  RegStr,reg, patrn, rcv);
	if (rcv != patrn) {
		msg_printf(ERR, "MCO VC2 %s Test -ERROR- reg %d, exp: %x, rcv: %x\n",
		RegStr,reg, patrn, rcv);
		return -1;
	} 
	return 0;
}

/***************************************************************************/
/*									   */
/* 	mco_VC2InternalRegTest :- VC2 Internal Register Tests		   */
/*									   */
/***************************************************************************/
__uint32_t 
mco_VC2InternalRegTest(void)
{
	__uint32_t  index  = 0;
	__uint32_t  errors = 0;
	ushort_t  *DataPtr;

	mco_MaxPLL_NoTiming();

	mco_set_dcbctrl(mgbase, 4);

	/* reset VC2 */
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, 0);
	MCO_VC2_SETREG(mgbase, VC2_CONFIG, VC2_RESET);



	DataPtr = walk_1_0;
	for(index=0; index <sizeof(mco_walk_1_0)/sizeof(mco_walk_1_0[0]); index++) {
	    errors += mco_VC2RegTest("VC2_VIDEO_ENTRY_PTR", VC2_VIDEO_ENTRY_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_CURS_ENTRY_PTR", VC2_CURS_ENTRY_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_CURS_X_LOC", 	VC2_CURS_X_LOC,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_CURS_Y_LOC", 	VC2_CURS_Y_LOC,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_DID_ENTRY_PTR", VC2_DID_ENTRY_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_SCAN_LENGTH", VC2_SCAN_LENGTH,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_RAM_ADDR", VC2_RAM_ADDR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_VT_FRAME_TBL_PTR",VC2_VT_FRAME_TBL_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_VT_LINESEQ_PTR", VC2_VT_LINESEQ_PTR, 
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_VT_LINES_IN_RUN", VC2_VT_LINES_IN_RUN,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_VERTICAL_LINE_CTR", VC2_VERTICAL_LINE_CTR, 
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_CURS_TABLE_PTR", VC2_CURS_TABLE_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_WORKING_CURS_Y", VC2_WORKING_CURS_Y,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_CURRENT_CURS_X_LOC", VC2_CURRENT_CURS_X_LOC,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_DID_FRAME_PTR", VC2_DID_FRAME_PTR,
	    *(DataPtr+index));
	    errors += mco_VC2RegTest("VC2_DID_LINE_TABLE_PTR", VC2_DID_LINE_TABLE_PTR,
	    *(DataPtr+index));
#if DO_NOT_TOUCH
	    errors += mco_VC2RegTest("VC2_DC_CONTROL",  VC2_DC_CONTROL,
	    *(DataPtr+index));
#endif
	}
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_VC2_REG_TEST]) ,errors);
}

/***************************************************************************/
/*									   */
/* 	mco_PokeVC2() - Write VC2 register				   */
/*									   */
/*      This is a tool to allow writing of arbitrary values to the various */
/*      VC2 registers from the IDE command line.			   */
/*									   */
/*	mco_pokevc2 [-l LOOPCOUNT] -r VC2Register -d DATA		   */
/*									   */
/***************************************************************************/
int
mco_PokeVC2(int argc, char **argv)
{
	__uint32_t Vc2Reg; 
	__uint32_t Data;
	int loopcount;
	__uint32_t bad_arg = 0;
	char cmdname[80];
	
	Data = 0xdeadbeef;

	loopcount = 1;
	sprintf(cmdname,"%s",argv[0]);
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch(argv[0][1]) {
		case 'r' :
			/* Register Select */	
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int *)&Vc2Reg);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int *)&Vc2Reg);
			}
			break;
		case 'd':
			/* Data to be Written */
			if (argv[0][2]=='\0') { /* Skip White Space */
			    atob(&argv[1][0], (int *)&Data);
			    argc--; argv++;
			} else {
			    atob(&argv[0][2], (int *)&Data);
			}
			break;
case 'l':
			if (argv[0][2]=='\0') { /* has white space */
			    atob(&argv[1][0], (int *)&loopcount);
			    argc--; argv++;
			}
			else { /* no white space */
			    atob(&argv[0][2], (int *)&loopcount);
			}
			if (loopcount < 0) {
			    msg_printf(SUM, "Error. Loop must be > or = to 0\n");
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
            msg_printf(SUM, "%s [-l loopcount] -r VC2Register  -d Data\n", cmdname);
	    return -1;
        }

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 4);

	for(; loopcount > 0;loopcount--) {
	    msg_printf(SUM, "%s: loopcount %d Writing Reg %d Data 0x%x\n" ,cmdname, loopcount, Vc2Reg, Data);
	    MCO_VC2_SETREG(mgbase, Vc2Reg, Data);
	}


	return 0 ;
}

/***************************************************************************/
/*									   */
/* 	mco_PeekVC2() - Read VC2 register				   */
/*									   */
/*      This is a tool to allow Reading of various VC2 registers from the  */
/*	IDE command line.			   			   */
/*									   */
/*	mco_peekvc2 [-l LOOPCOUNT] -r VC2Register			   */
/*									   */
/***************************************************************************/
__uint32_t 
mco_PeekVC2(int argc, char **argv)
{
	__uint32_t Vc2Reg; 
	ushort_t Data;
	int loopcount;
	__uint32_t bad_arg = 0;
	
	Data = 0xdead;

	loopcount = 1;
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch(argv[0][1]) {
		case 'r' :
			/* Register Select */	
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int *)&Vc2Reg);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int *)&Vc2Reg);
			}
			break;
		case 'l':
			if (argv[0][2]=='\0') { /* has white space */
			    atob(&argv[1][0], (int *)&loopcount);
			    argc--; argv++;
			}
			else { /* no white space */
			    atob(&argv[0][2], (int *)&loopcount);
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
	    msg_printf(SUM, "mco_peekvc2 [-l loopcount] -r VC2Register  \n" );
	    return -1;
        }

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, 4);

	for(; loopcount > 0;loopcount--) {
	    MCO_VC2_GETREG(mgbase, Vc2Reg, Data);
	    msg_printf(SUM, "mco_peekvc2: loopcount %d Read Reg %d Data 0x%x\n",
	    loopcount, Vc2Reg, Data);
	}

	return((__uint32_t)Data);
}
