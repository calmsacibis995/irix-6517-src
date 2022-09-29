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

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include "uif.h"
#include "libsc.h"
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "iset.diag"
#include "ide_msg.h"



/* HQ3 REIF registers */

hq3reg hq3_reif_regs[] = {
{"SCAN_WIDTH",SCAN_WIDTH,0xfffff},
{"LINE_COUNT",LINE_COUNT,0xffff0000},
{"RE_DMA_MODES",RE_DMA_MODES,0x1c700},
/*
{"RE_DMA_CNTRL",RE_DMA_CNTRL,0x5},
*/
{"RE_DMA_TYPE",RE_DMA_TYPE,0xf},
/* {"REBUS_SYNC",REBUS_SYNC,0x1} */
};

/* HQ3 HAG(DMA) single word registers */

hq3reg hq3_hag_regs[] = {
{"IM_PG_LIST1",IM_PG_LIST1,0xffffffff},
/* Can't do test on these
{"IM_PG_LIST2",IM_PG_LIST2,0xffffffff},
{"IM_PG_LIST3",IM_PG_LIST3,0xffffffff},
{"IM_PG_LIST4",IM_PG_LIST4,0xffffffff},
*/
{"IM_PG_WIDTH",IM_PG_WIDTH,0xfffff},
{"IM_ROW_OFFSET",IM_ROW_OFFSET,0xfffff},
{"IM_ROW_START_ADDR",IM_ROW_START_ADDR,0xfffffff},
{"IM_LINE_CNT",IM_LINE_CNT,0xffff},
{"IM_FIRST_SP_WIDTH",IM_FIRST_SP_WIDTH,0xfffff},
{"IM_LAST_SP_WIDTH",IM_LAST_SP_WIDTH,0xfffff},
{"IM_Y_DEC_FACTOR",IM_Y_DEC_FACTOR,0x1ffff},
{"DMA_PG_SIZE",DMA_PG_SIZE,0x7},
{"DMA_PG_NO_RANGE_IM1",DMA_PG_NO_RANGE_IM1,0xf},
{"DMA_PG_NO_RANGE_IM2",DMA_PG_NO_RANGE_IM2,0xf},
{"DMA_PG_NO_RANGE_IM3",DMA_PG_NO_RANGE_IM3,0xf},
{"DMA_PG_NO_RANGE_GL",DMA_PG_NO_RANGE_GL,0xf},
{"DMA_PG_NO_RANGE_DL",DMA_PG_NO_RANGE_DL,0xf},
{"MAX_IM1_TBL_PTR_OFFSET",MAX_IM1_TBL_PTR_OFFSET,0x1ffffff},
{"MAX_IM2_TBL_PTR_OFFSET",MAX_IM2_TBL_PTR_OFFSET,0x1ffffff},
{"MAX_IM3_TBL_PTR_OFFSET",MAX_IM3_TBL_PTR_OFFSET,0x1ffffff},
{"MAX_GL_TBL_PTR_OFFSET",MAX_GL_TBL_PTR_OFFSET,0x1ffffff},
{"MAX_DL_TBL_PTR_OFFSET",MAX_DL_TBL_PTR_OFFSET,0x1ffffff},
/* {"HAG_STATE_FLAGS",HAG_STATE_FLAGS,0xffffffff},  */
{"HAG_STATE_REMAINDER",HAG_STATE_REMAINDER,0x3ffff}
};

/* HQ3 HAG(DMA) double word registers */

hq3reg hq3_hag_dwregs[] = {
{"IM_TABLE_BASE_ADDR1",IM_TABLE_BASE_ADDR1, 	0xffffffff},
{"IM_TABLE_BASE_ADDR2",IM_TABLE_BASE_ADDR2, 	0xffffffff},
{"IM_TABLE_BASE_ADDR3",IM_TABLE_BASE_ADDR3, 	0xffffffff},
{"GL_TABLE_BASE_ADDR",GL_TABLE_BASE_ADDR,    	0xffffffff},
{"DL_TABLE_BASE_ADDR",DL_TABLE_BASE_ADDR,    	0xffffffff},
};

__uint32_t hq3ucode_addr_pat[] = {
	0x4, 0x8,
	0x10, 0x20, 0x40, 0x80,
	0x100, 0x200, 0x400, 0x800,
	0x1000, 0x2000, 0x4000,
	0x3ffc, 0x3ffc, 0x3ffc,
	0x3fec, 0x3fdc, 0x3fbc, 0x3f7c,
	0x3efc, 0x3dfc, 0x3bfc, 0x37fc,
	0x2ffc, 0x1ffc
};

/* HQ3 GIO Bus test */

__uint32_t
mgras_HQ3dctctrl_test(void) {
	static char *testname = "HQ DCB Ctrl Test";
	int errors = 0;
	__uint32_t index = 0;

	msg_printf(VRB, "%s\n",testname);

	for(index=0; index < sizeof(walk_1_0_32)/sizeof(walk_1_0_32[0]); index++) {
		HQ3_DCBCTRLREG_TST("dcbctrl_0",		mgbase->dcbctrl_0, 		walk_1_0_32[index] );
#if HQ4
		HQ3_DCBCTRLREG_TST("dcbctrl_pcd",	mgbase->dcbctrl_pcd,		walk_1_0_32[index] );
#else
		HQ3_DCBCTRLREG_TST("dcbctrl_1",		mgbase->dcbctrl_1,		walk_1_0_32[index] );
#endif
		HQ3_DCBCTRLREG_TST("dcbctrl_2",		mgbase->dcbctrl_2,		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_cmapall",	mgbase->dcbctrl_cmapall, 	walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_cmap0",	mgbase->dcbctrl_cmap0,		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_cmap1",	mgbase->dcbctrl_cmap1, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_dac",	mgbase->dcbctrl_dac, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_pp1",	mgbase->dcbctrl_pp1, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_vc3",	mgbase->dcbctrl_vc3, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_bdvers",	mgbase->dcbctrl_bdvers, 	walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_ab1",	mgbase->dcbctrl_ab1, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_11",	mgbase->dcbctrl_11, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_cc1",	mgbase->dcbctrl_cc1, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_gal",	mgbase->dcbctrl_gal, 		walk_1_0_32[index] );
		HQ3_DCBCTRLREG_TST("dcbctrl_tmi",	mgbase->dcbctrl_tmi, 		walk_1_0_32[index] );
#if HQ4
		HQ3_DCBCTRLREG_TST("dcbctrl_ddc",	mgbase->dcbctrl_ddc,		walk_1_0_32[index] );
#else
		HQ3_DCBCTRLREG_TST("dcbctrl_15",	mgbase->dcbctrl_15,		walk_1_0_32[index] );
#endif
	}

	if (!errors) { 
		 msg_printf(SUM, "     %s passed.\n" , testname);
	} else {
		 msg_printf(ERR, "**** %s FAILED.\n" , testname);
	}

	return (errors);
}

/***************************************************************************
*   Routine: mgras_HQGIObus
*
*   Purpose: Host to HQ3 GIO Bus test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: First check default settings.
*          Then toggle dcb_be/hq_runs_tlb bit  and see if read back OK or not
*
****************************************************************************/

__uint32_t
mgras_HQGIObus(void)
{
	__uint32_t gio_initstate, expected, actual;
	__uint32_t errors = 0;
#if HQ4
	char *name = "MGRAS HQ XIO Bus Test";
#else
	char *name = "MGRAS HQ GIO Bus Test";
#endif

	msg_printf(VRB,"%s\n",name);

	/* Twiddle dcb_be bit */
	HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_initstate);

        expected = gio_initstate & ~dcb_be;
  	HQ3_PIO_WR(GIO_CONFIG_ADDR, expected, GIO_CONFIG_MASK);
	HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, actual);
	COMPARE(name, GIO_CONFIG_ADDR, expected, actual, GIO_CONFIG_MASK, errors);

	/* Twiddle hq_runs_tlb bit */
        expected = expected & ~hq_runs_tlb;
  	HQ3_PIO_WR(GIO_CONFIG_ADDR, expected, GIO_CONFIG_MASK);
	HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, actual);
	COMPARE(name, GIO_CONFIG_ADDR, expected, actual, GIO_CONFIG_MASK, errors);

	/* restore initial state */

	HQ3_PIO_WR(GIO_CONFIG_ADDR, gio_initstate, GIO_CONFIG_MASK);

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_GIOBUS_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_HQ3bus
*
*   Purpose: Host to HQ3 Bus test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Do a walking 1 and 0 on FLAG_ENAB_SET register
*
****************************************************************************/

__uint32_t
mgras_HQbus(void)
{
	__uint32_t i, errors = 0;
	__uint32_t expected, actual;
	__uint32_t initstate;

#if !HQ4
	msg_printf(VRB,"MGRAS HQ Internal Bus Test\n");

	/* Save original interrupt settings */
	HQ3_PIO_RD(INTR_ENAB_SET_ADDR, 0xffffffff, initstate);
	/* Disable interrupt */

	HQ3_PIO_WR(INTR_ENAB_CLR_ADDR,0,0xffffffff);

	/* clear all flags */

	HQ3_PIO_WR(FLAG_ENAB_CLR_ADDR, HQ3_FLAGS_MASK, 0xffffffff);


	
	/* Walking 1 */

	for (i = 0, expected = 1; i < 32; i++, expected <<=1)
	{
  	   HQ3_PIO_WR(FLAG_ENAB_SET_ADDR, expected, HQ3_FLAGS_MASK);
	   
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	   HQ3_PIO_RD(FLAG_ENAB_SET_ADDR, HQ3_FLAGS_MASK, actual);
	   msg_printf(DBG,"expected = 0x%x, actual = 0x%x\n",expected,actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_ENAB_SET_ADDR, expected, actual, HQ3_FLAGS_MASK, errors); 

	   HQ3_PIO_WR(FLAG_ENAB_CLR_ADDR, expected, HQ3_FLAGS_MASK);

	}

	/* Walking 0 */

	for (i = 0, expected = 0xfffffffe; i < 32; i++, expected = ((expected <<=1) | 1))
	{
  	   HQ3_PIO_WR(FLAG_ENAB_SET_ADDR, expected, HQ3_FLAGS_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	   HQ3_PIO_RD(FLAG_ENAB_SET_ADDR, HQ3_FLAGS_MASK, actual);
	   msg_printf(DBG,"expected = 0x%x, actual = 0x%x\n",expected,actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_ENAB_SET_ADDR, expected, actual, HQ3_FLAGS_MASK, errors); 

	   HQ3_PIO_WR(FLAG_ENAB_CLR_ADDR, expected, HQ3_FLAGS_MASK);
	}

	/* Restore interrupt settings */

	HQ3_PIO_WR(INTR_ENAB_SET_ADDR, initstate, HQ3_FLAGS_MASK);

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_HQBUS_TEST]), errors);
#endif /* !HQ4 */
	return(errors);
}

/***************************************************************************
*   Routine: mgras_hq3_reg_test
*
*   Purpose: Host to HQ3 Registers test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Test registers acess and SET/CLEAR functions
*          Following registers was tested: HQ3_CONFIG, FLAG_SET_PRIV/FLAG_CLR_PRIV,
*          FLAG_SET/FLAG_CLR
*
****************************************************************************/

mgras_hq3_reg_test()
{
	__uint32_t i, errors = 0;
	__uint32_t flag_save1,flag_save2,actual,expected, initstate;

	msg_printf(VRB,"MGRAS HQ3 FLAG_SET/FLAG_CLR registers Test\n");
/* Test HQ3_CONFIG */

	/* Save original configuration */

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, flag_save1); 

	/* Walking 1 */

	for (i = 0, expected = 1; i < 15; i++, expected <<= 1)
	{
	   HQ3_PIO_WR(HQ3_CONFIG_ADDR, expected, HQ3_CONFIG_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);
	   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, actual);
	   COMPARE("HQ3 - HQ3_CONFIG Reg.  test", HQ3_CONFIG_ADDR, expected, actual, HQ3_CONFIG_MASK, errors);
	}
	/* Walking 0 */

	for (i = 0, expected = 0x7ffe; i < 15; i++, expected = ((expected <<= 1) | 1) & HQ3_CONFIG_MASK )
	{
	   HQ3_PIO_WR(HQ3_CONFIG_ADDR, expected, HQ3_CONFIG_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);
	   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, actual);
	   COMPARE("HQ3 - HQ3_CONFIG Reg.  test", HQ3_CONFIG_ADDR, expected, actual, HQ3_CONFIG_MASK, errors);
	}
	
	/* Restore original configuration */

	HQ3_PIO_WR(HQ3_CONFIG_ADDR, flag_save1, HQ3_CONFIG_MASK);

	/* Save original interrupt settings */

	HQ3_PIO_RD(INTR_ENAB_SET_ADDR, HQ3_FLAGS_MASK, initstate);
	/* Disable interrupt */

	HQ3_PIO_WR(INTR_ENAB_CLR_ADDR, 0, HQ3_FLAGS_MASK);


/* FLAG_SET_PRIV/FLAG_CLR_PRIV test */

	/* Save original FLAG_SET_PRIV flags */

	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_save1);

        /* clear all flags */

	HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, HQ3_FLAGS_MASK, HQ3_FLAGS_MASK);

	/* Walking 1 */

	for (i = 0, expected = 1; i < 25; i++, expected = ((expected <<=1) & HQ3_FLAGS_MASK))
	{
	   HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, expected, HQ3_FLAGS_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, HQ3_FLAGS_MASK);

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_SET_PRIV_ADDR, expected, actual, HQ3_FLAGS_MASK, errors);

           HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, expected, HQ3_FLAGS_MASK);

	}

	/* Walking 0 */

	for (i = 0, expected = HQ3_FLAGS_MASK -1; i < 25; i++, expected = ((expected <<=1) | 1) & HQ3_FLAGS_MASK)
	{
	   HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, expected, HQ3_FLAGS_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, HQ3_FLAGS_MASK);

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_SET_PRIV_ADDR, expected, actual, HQ3_FLAGS_MASK, errors);

           HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, expected, HQ3_FLAGS_MASK);

	}

	/* Restore old flags */

	HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR,0, HQ3_FLAGS_MASK);
	HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, flag_save1, HQ3_FLAGS_MASK);

/* FLAG_SET/FLAG_CLR */		
	

	/* Save original FLAG_SET flags */

#ifndef MGRAS_DIAG_SIM 
	HQ3_PIO_RD(FLAG_SET_ADDR, HQ3_FLAGS_MASK, flag_save1);
	
	/* Save original FLAG_ENAB_SET flags */

	HQ3_PIO_RD(FLAG_ENAB_SET_ADDR, HQ3_FLAGS_MASK, flag_save2);
#endif
	/* Enable all flags to be acessible by user */

	HQ3_PIO_WR(FLAG_ENAB_SET_ADDR, HQ3_FLAGS_MASK, HQ3_FLAGS_MASK);

        /* clear all flags */

	HQ3_PIO_WR(FLAG_CLR_ADDR, HQ3_FLAGS_MASK, HQ3_FLAGS_MASK);

	/* Walking 1 */

	for (i = 0, expected = 1; i < 25; i++, expected = (expected <<=1) & HQ3_FLAGS_MASK)
	{
	   HQ3_PIO_WR(FLAG_SET_ADDR, expected, HQ3_FLAGS_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, HQ3_FLAGS_MASK);

	   HQ3_PIO_RD(FLAG_SET_ADDR, HQ3_FLAGS_MASK, actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_SET_ADDR, expected, actual, HQ3_FLAGS_MASK, errors);

           HQ3_PIO_WR(FLAG_CLR_ADDR, expected, HQ3_FLAGS_MASK);

	}

	/* Walking 0 */

	for (i = 0, expected = HQ3_FLAGS_MASK -1; i < 25; i++, expected = ((expected <<=1) | 1) & HQ3_FLAGS_MASK)
	{
	   HQ3_PIO_WR(FLAG_SET_ADDR, expected, HQ3_FLAGS_MASK);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, HQ3_FLAGS_MASK);

	   HQ3_PIO_RD(FLAG_SET_ADDR, HQ3_FLAGS_MASK, actual);
	   COMPARE("HQ3- HQ3bus test", FLAG_SET_ADDR, expected, actual, HQ3_FLAGS_MASK, errors);

           HQ3_PIO_WR(FLAG_CLR_ADDR, expected, HQ3_FLAGS_MASK);
	}

	/* Restore old flags */

	HQ3_PIO_WR(FLAG_CLR_ADDR, 0, HQ3_FLAGS_MASK);
	HQ3_PIO_WR(FLAG_SET_ADDR, flag_save1, HQ3_FLAGS_MASK);

	HQ3_PIO_WR(FLAG_ENAB_CLR_ADDR, 0, HQ3_FLAGS_MASK);
	HQ3_PIO_WR(FLAG_ENAB_SET_ADDR, flag_save2, HQ3_FLAGS_MASK);

	/* Restore interrupt enable settings */
	HQ3_PIO_WR(FLAG_ENAB_SET_ADDR, initstate, HQ3_INTR_MASK);

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_REGISTERS_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_RSS_DataBus
*
*   Purpose: HQ3 RSS Raster Subsystem mapping data bus test 
*
*   History: 3/13/95 : Original Version.
*
*   Notes: Write a walking 1 and 0 to GLINE_DX/GLINE_DY double word register 
*   and verify 
*
*   Double writes are supported, but only single reg reads are supported. Each
*   reg is 26 bits, but expected_lo and expected_hi are 32bit words, so it
*   has the following translation:
*
*	expected_lo(25:0) 	==> GLINE_DY(25:0)
*	expected_lo(31:26) 	==> GLINE_DX(5:0)
*	expected_hi(19:0)	==> GLINE_DX(25:6)
*
*   So the code tests GLINE_DY(25:0), then GLINE_DX(5:0), then GLINE_DX(25:6).
*
****************************************************************************/

__uint32_t
mgras_RSS_DataBus(void)
{
	__uint32_t i, j, errors = 0, tot_errs = 0;
	__uint32_t actual, actual_hi, expected_hi, expected_lo, actual_lo;

	for (j = 0; j < numRE4s; j++) {
	msg_printf(VRB,"Testing RSS-%d\n", j);

	errors = 0;

	   WRITE_RSS_REG(0, CONFIG, j, RSS_DATA_MASK);

	/* Walking 1 */

	/* check data-1, return if fail */
	for (i = 0, expected_hi = 0, expected_lo = 0x1; i < 26; i++, expected_lo = (expected_lo <<=1) & RSS_DATA_MASK) 
	{
	   WRITE_RSS_REG_DBL(0,(GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, RE_TOGGLECNTX, 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DY , actual_lo, RSS_DATA_MASK);
	   actual_lo = actual;

	   if (expected_lo != actual_lo)
	   {
	   	msg_printf(ERR,"REBus(25:0) exp=0x%x, actual=0x%x, diff=0x%x\n",expected_lo,actual_lo, expected_lo^actual_lo);
		errors++;
		REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_RSS_DATABUS_TEST]), errors);
	 	/*return (-1);*/
	   }
	}

	/* check bits (5:0) on data 0 */
	for (i = 26, expected_lo = (0x1 << 26), expected_hi = 0; i < 32; i++, expected_lo = (expected_lo <<=1) & RSS_DATA_MASK) 
	{
	   WRITE_RSS_REG_DBL(0,(GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, RE_TOGGLECNTX, 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DX , actual_hi, RSS_DATA_MASK);
	   actual_hi = actual;

	   if (expected_lo != (actual_hi << 26))
	   {
	   	msg_printf(ERR,"REBus(51:26) exp=0x%x, actual=0x%x, diff=0x%x\n",(expected_lo>>26),actual_hi, (expected_lo>>26)^actual_hi);
		errors++;

	   }
	}

	/* check bits (25:6) of data 0 */
	for (i = 0, expected_lo = 0, expected_hi = 0x1; i < 20; i++, expected_hi = (expected_hi <<=1) & RSS_DATA_MASK)
	{
	   WRITE_RSS_REG_DBL(0, (GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, RE_TOGGLECNTX, 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DX , actual_hi, RSS_DATA_MASK);
	   actual = actual >> 6;

	   if (expected_hi != actual)
	   {
		errors++;
	   	msg_printf(ERR,"REBus(51:26) exp=0x%x, actual=0x%x, diff=0x%x\n"
		,expected_hi<<6,actual<<6, (expected_hi<<6)^(actual<<6));
	   }
	}

	/* Walking 0 */

	/* check data-1, return if fail */
	for (i = 0, expected_hi = 0, expected_lo = 0x3fffffe; i < 26; i++, expected_lo = ((expected_lo <<=1) | 1) & RSS_DATA_MASK)
	{
	   WRITE_RSS_REG_DBL(0,(GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, (RE_TOGGLECNTX), 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DY , actual_lo, RSS_DATA_MASK);
	   actual_lo = actual;

	   if ((expected_lo & 0x3ffffff) != actual_lo)
	   {
		errors++;
	   	msg_printf(ERR,"REBus(25:0) exp=0x%x, actual=0x%x, diff=0x%x\n",(expected_lo & 0x3ffffff),actual_lo, (expected_lo & 0x3ffffff) ^actual_lo);
		REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_RSS_DATABUS_TEST]), errors);
		/*return (-1);*/
	   }
	}

	/* check bits (5:0) in data-0 */
	for (i = 26, expected_lo = 0xfbffffff, expected_hi = 0xfffff; i < 32; i++, expected_lo = ((expected_lo <<=1) | 1) & RSS_DATA_MASK)
	{
	   WRITE_RSS_REG_DBL(0,(GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, (RE_TOGGLECNTX), 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DX , actual_hi, RSS_DATA_MASK);
	   actual_hi = actual;

	   if ((expected_lo >> 26) != (actual_hi & 0x3f))
	   {
		errors++;
	   	msg_printf(ERR,"REBus(51:26) exp=0x%x, actual=0x%x, diff=0x%x\n",0x3ffffc0 | (expected_lo>>26),actual_hi, (0x3ffffc0 | (expected_lo>>26))^actual_hi);
	   }
	}

	/* check bits (25:6) in data-0 */
	for (i = 0, expected_lo = 0xffffffff, expected_hi = 0x3fffffe; i < 20; i++, expected_hi = ((expected_hi <<=1) | 1) & RSS_DATA_MASK)
	{
	   WRITE_RSS_REG_DBL(0, (GLINE_DX + 0x200), expected_hi, expected_lo, RSS_DATA_MASK);
	   WRITE_RSS_REG(0, RE_TOGGLECNTX, 0xdeadbeef, RSS_DATA_MASK);
	   READ_RSS_REG(0, GLINE_DX , actual_hi, RSS_DATA_MASK);
	   if ((expected_hi & 0xfffff) != (actual >> 6))
	   {
		errors++;
	   	msg_printf(ERR,"REBus(51:26) exp=0x%x, actual=0x%x, diff=0x%x\n"		,((expected_hi<<6) | 0x3f) & 0x3ffffff, actual, 
		(((expected_hi<<6) | 0x3f) & 0x3ffffff)^actual);
	   }
	}

	/* reset to default */
	WRITE_RSS_REG(0, CONFIG, CONFIG_RE4_ALL, RSS_DATA_MASK);

	REPORT_PASS_OR_FAIL_NR((&HQ3_err[HQ3_RSS_DATABUS_TEST]), errors);

	tot_errs += errors;
	
	} /* end of looping through RE4s */

	RETURN_STATUS(tot_errs);
}

#if HQ4_TLB_DIAG_READ_BUG_FIXED
__uint32_t mgras_tlb_pattern[8] = { \
	0x1111, 0x2222, 0x3333, 0x4444, \
	0x5555, 0x6666, 0x7777, 0x8888
};
#endif

/***************************************************************************
*   Routine: mgras_tlb_test
*
*   Purpose: HQ3 TLB Read/Write test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write into a TLB entry and read verify 
*
****************************************************************************/
__uint32_t
mgras_tlb_test(void)
{
    __uint32_t i, j, tlb_size, tlb_index;
    __uint32_t expected, actual, gio_config;
    __uint32_t errors = 0;
    __uint32_t pat, index, bank, tlb_test_val, tlb_offset;

#if HQ4
    msg_printf(VRB, "HQ TLB Test\n");

    /* Set TLB_TEST(bit 14) in GIO_CONFIG register */
    HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
    HQ3_PIO_WR(GIO_CONFIG_ADDR, (gio_config | TLB_TEST_BIT) , GIO_CONFIG_MASK);

    tlb_size = TLB_SIZE;
    tlb_index = tlb_size >> 1;

    /* check if the valid bits are toggled right */
    for (i = 0; i < tlb_size; i++) {
    	/* Invalidate all entries */
    	HQ3_PIO_WR(TLB_VALIDS_ADDR, 0x0, 0xffffffff);

	expected = 1 << i;

	/* put TLB entry */
	mgbase->tlb[i].hi = i;

	/* read the TLB valid register */
	HQ3_PIO_RD(TLB_VALIDS_ADDR, 0xffff, actual);

	/* test the bit for this TLB entry is set */
	COMPARE("HQ3 TLB Vaild Bit test", i, expected , actual,0xffff, errors);
    }

    for (j = 0; j < tlb_size; j++) {
	mgbase->tlb[j].hi = mgbase->tlb[j].lo = 0x0;
	mgbase->tlb[j].padhi = mgbase->tlb[j].padlo = 0x0;
    }

    /* Invalidate all entries */
    HQ3_PIO_WR(TLB_VALIDS_ADDR, 0x0, 0xffffffff);

#if HQ4_TLB_DIAG_READ_BUG_FIXED
    /* read and write all possible entries in the TLB RAM */
    for (bank=0, tlb_offset=TLB_ADDR; bank < 2; bank++, tlb_offset+=0x80) {
	msg_printf(DBG, "Testing the physical address...\n");
	tlb_offset = TLB_ADDR;
	msg_printf(DBG, "tlb_offset 0x%x\n", tlb_offset);

	for(index = 0; index < tlb_index; index++) {
		/* write the address */
		HQ3_PIO_WR((tlb_offset+8), mgras_tlb_pattern[index], ~0x0);
		tlb_offset += 16;
	}
	for (index = 0; index < tlb_index; index++) {
    		/* Set TLB_TEST(bit 14) in GIO_CONFIG register */
    		HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
    		HQ3_PIO_WR(GIO_CONFIG_ADDR, (gio_config | TLB_TEST_BIT) , 
				GIO_CONFIG_MASK);

		/* read back physical address and verify */
		/* set TLB_TEST_REGISTER first */
		tlb_test_val = (bank << 4)|(HQ4_PHYS_ADDR_DATA << 3)|index;
		HQ3_PIO_WR(TLB_TEST_ADDR, tlb_test_val, TLB_TEST_WR_MASK);
		HQ3_PIO_RD(TLB_TEST_ADDR, TLB_PHYS_MASK, actual);
		COMPARE("HQ4 TLB PHYSICAL ADDR Test", index, 
		   mgras_tlb_pattern[index], actual, TLB_PHYS_MASK, errors);

		msg_printf(DBG, "pat 0x%x; index 0x%x; bank 0x%x; i 0x%x; tlb_test_val 0x%x\n",
			pat, index, bank, i, tlb_test_val);
		msg_printf(DBG, "tlb_offset 0x%x; tlb_valids_offset 0x%x tlb_test_addr_off 0x%x\n",
			tlb_offset, TLB_VALIDS_ADDR, TLB_TEST_ADDR);
	}
	msg_printf(DBG, "Testing the physical address...Done\n");

	msg_printf(DBG, "Testing the tag...\n");

	tlb_offset = TLB_ADDR;
	msg_printf(DBG, "tlb_offset 0x%x\n", tlb_offset);

	for(index = 0; index < tlb_index; index++) {
		/* write the tag */
		HQ3_PIO_WR((tlb_offset+0), mgras_tlb_pattern[index], ~0x0);
		tlb_offset += 16;
	}
	for(index = 0; index < tlb_index; index++) {
    		/* Set TLB_TEST(bit 14) in GIO_CONFIG register */
    		HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
    		HQ3_PIO_WR(GIO_CONFIG_ADDR, (gio_config | TLB_TEST_BIT) , 
			GIO_CONFIG_MASK);

		/* read back tag and verify */
		/* set TLB_TEST_REGISTER first */
		tlb_test_val = (bank << 4)|(HQ4_TAG_DATA << 3)|index;
		HQ3_PIO_WR(TLB_TEST_ADDR, tlb_test_val, TLB_TEST_WR_MASK);
		HQ3_PIO_RD(TLB_TEST_ADDR, TLB_TAG_MASK, actual);
		COMPARE("HQ4 TLB Tag Test", index, 
			mgras_tlb_pattern[index], actual, TLB_TAG_MASK, errors);
		msg_printf(DBG, "pat 0x%x; index 0x%x; bank 0x%x; i 0x%x; tlb_test_val 0x%x\n",
			pat, index, bank, i, tlb_test_val);
		msg_printf(DBG, "tlb_offset 0x%x; tlb_valids_offset 0x%x tlb_test_addr_off 0x%x\n",
			tlb_offset, TLB_VALIDS_ADDR, TLB_TEST_ADDR);
	}
	msg_printf(DBG, "Testing the tag...Done\n");
    }
#endif /* HQ4_TLB_DIAG_READ_BUG_FIXED */

#else /* HQ3 */
	msg_printf(VRB,"HQ3 TLB Valid Bit Test\n");
	/* Invalidate all entries */
	HQ3_PIO_WR(TLB_VALIDS_ADDR, 0x0, 0xffffffff);

	for (i = 0; i < TLB_SIZE ; i++)
	{
	   expected = 1 << i;
	   HQ3_PIO_WR(TLB_ADDR + (i << 3), i, TLB_MASK);
	   HQ3_PIO_RD(TLB_VALIDS_ADDR, 0xffff, actual);
	   msg_printf(DBG,"TLB_VALIDS read = 0x%x i = 0x%x\n",actual,i); 
	   COMPARE("HQ3 TLB Vaild Bit test", i, expected , actual,0xffff, errors);

	   /* Invalidate  it */
	   HQ3_PIO_WR(TLB_VALIDS_ADDR,(0 << i) , TLB_MASK);
	}

	msg_printf(DBG,"MGRAS HQ3 TLB Write/Read/Verify Test\n");

        for (j = 0, expected = 0x1; j < 20; j++, expected <<= 1)
	{

	   /* Write Part */

	   /* TLB 0 first */
	   for (i = 0; i < TLB_SIZE/2 ; i++)
	   {

	      HQ3_PIO_WR(((__uint32_t) TLB_ADDR | (i << 3)), expected, TLB_MASK);
	      HQ3_PIO_WR(((__uint32_t) TLB_ADDR | (i << 3) + 4) , expected, TLB_MASK);
	   }
	   /* TLB 1 */
	   for (i = 0; i < TLB_SIZE/2 ; i++)
	   {

              HQ3_PIO_WR(((__uint32_t) (TLB_ADDR | 0x40 | (i << 3))), expected, TLB_MASK);
	      HQ3_PIO_WR(((__uint32_t) (TLB_ADDR | 0x40 | (i << 3) + 4)) , expected, 0xffffffff);

	   }
	   /* Read Back and Verify */


	   /* Write TLB Addr into TLB_TEST register, the read it to get the data */

	   /* Set TLB_TEST(bit 14) in GIO_CONFIG register */
	   HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
	   HQ3_PIO_WR(GIO_CONFIG_ADDR , gio_config | TLB_TEST_BIT , GIO_CONFIG_MASK);

	   for (i = 0; i < TLB_SIZE/2 ; i++)
	   {

	       HQ3_PIO_WR(TLB_TEST_ADDR,  (0x8 | i) , TLB_MASK);
	       HQ3_PIO_RD(TLB_TEST_ADDR, TLB_MASK, actual);
	       COMPARE("HQ3 TLB0 LOW ENTERY test", i, expected, actual, TLB_MASK & 0x7ffff, errors);

	       HQ3_PIO_WR(TLB_TEST_ADDR , i , TLB_MASK);
	       HQ3_PIO_RD(TLB_TEST_ADDR, TLB_MASK, actual);
	       COMPARE("HQ3 TLB0 HIGH ENTRY test", i, expected, actual, TLB_MASK, errors);


	   }
	   for (i = 0; i < TLB_SIZE/2 ; i++)
	   {
	       HQ3_PIO_WR(TLB_TEST_ADDR, (0x18 | i) , TLB_MASK);
	       HQ3_PIO_RD(TLB_TEST_ADDR, TLB_MASK, actual);
	       COMPARE("HQ3 TLB1 LOW ENTERY test", i, expected, actual, TLB_MASK & 0x7ffff, errors);

	       HQ3_PIO_WR(TLB_TEST_ADDR, (0x10 | i) , TLB_MASK);
	       HQ3_PIO_RD(TLB_TEST_ADDR, TLB_MASK, actual);
	       COMPARE("HQ3 TLB1 HIGH ENTRY test", i, expected, actual, TLB_MASK, errors);
	   }


	}

#endif /* HQ4 */

	/* Reset TLB_TEST(bit 14) in GIO_CONFIG register */
	HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
	HQ3_PIO_WR(GIO_CONFIG_ADDR , gio_config & ~TLB_TEST_BIT , GIO_CONFIG_MASK);

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_TLB_TEST]), errors);
}

/***************************************************************************
*   Routine: mgras_hq3_reif_wdReg
*
*   Purpose: HQ3 REIF_CTX single registers write/read test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write through Hq3 command fifo and verify via PIO read acess 
*
****************************************************************************/

/* HQ3 REIF_CTX  register read/write  */
mgras_hq3_reif_wdReg(__uint32_t reg_offset, __uint32_t data, __uint32_t mask)
{
	__uint32_t errors = 0;
	__uint32_t reif_cmd, actual;


	/* construct REIF command */
	reif_cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, reg_offset | REIF_CTX_CMD_BASE, 4);
	/* Write the command and data out through CFIFO */
	msg_printf(DBG,"reg_offset = 0x%x reif_cmd = 0x%x data = 0x%x\n",reg_offset,reif_cmd,data);
	HQ3_PIO_WR(CFIFO1_ADDR, reif_cmd, 0xffffffff);
	HQ3_PIO_WR(CFIFO1_ADDR, data, 0xffffffff);

	/* mgras_wrHQ3word(reif_cmd);
	mgras_wrHQ3word(data); */

	/* Wait a while here */

	time_delay(10); /* or what */
	/* Read the data back through PIO */
	HQ3_PIO_RD((reg_offset << 2 ) + REIF_CTX_BASE , mask, actual);
	COMPARE("HQ3 REIF register test", reg_offset, data, actual, mask, errors);
	return errors;

}
/***************************************************************************
*   Routine: mgras_hq3_reif_reg_test
*
*   Purpose: HQ3 REIF_CTX registers test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Read/Write HQ3 REIF_CTX  registers(Write into cfifo and read bach through
*          EIF_CTX PIO acess.
*
****************************************************************************/

__uint32_t
mgras_hq3_reif_reg_test(void)
{
	__uint32_t errors = 0;
	__uint32_t i, reg_index, pattern;

	msg_printf(VRB,"HQ reif_ctx Register test\n");

	/* Pattern test */

	for (reg_index = 0; reg_index < HQ3_REIF_REGS_SIZE ; reg_index++) 
	{
	   /* patterns test */
	   for (i = 0; i < 6 ; ++i)
		errors += mgras_hq3_reif_wdReg(hq3_reif_regs[reg_index].addr_offset, patrn[i], hq3_reif_regs[reg_index].mask); 
	   /* Walking 1 */
	   for (i = 0, pattern = 1; i < 32;i++, pattern <<= 1)
		errors += mgras_hq3_reif_wdReg(hq3_reif_regs[reg_index].addr_offset, patrn[i], hq3_reif_regs[reg_index].mask); 
           /* Walking 0 */
	   for (i = 0, pattern = 0xfffffffe; i < 32;i++, (pattern <<= 1) | 1)
		errors += mgras_hq3_reif_wdReg(hq3_reif_regs[reg_index].addr_offset, patrn[i], hq3_reif_regs[reg_index].mask); 
	}
	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_REIF_CTX_TEST]), errors);
}

/****** HQ3 HAG_CTX registers test *************/

/***************************************************************************
*   Routine: mgras_hq3_hag_wdReg
*
*   Purpose: HQ3 HAG_CTX single word register test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write thru CMD fifo, verify via PIO acess 
*
****************************************************************************/
/* HQ3 HAG_CTX  register read/write  */
mgras_hq3_hag_wdReg(__uint32_t reg_offset, __uint32_t data, __uint32_t mask)
{
	__uint32_t errors = 0;
	__uint32_t hag_cmd, actual;

	/* construct HAG command */
	hag_cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, reg_offset + HAG_CTX_CMD_BASE, 4);
	msg_printf(DBG,"hag_offset = 0x%x hag_cmd = 0x%x data = 0x%x\n",reg_offset,hag_cmd,data);
	/* Write the command and data out through CFIFO */

	HQ3_PIO_WR(CFIFO1_ADDR, hag_cmd, 0xffffffff);
	HQ3_PIO_WR(CFIFO1_ADDR, data, 0xffffffff);

	/* Wait a while here */

	time_delay(10);  /* or what */
	/* Read the data back through PIO */
	HQ3_PIO_RD((reg_offset << 2) + HAG_CTX_BASE , mask, actual);
	COMPARE("HQ HAG register test", reg_offset, data, actual, mask, errors);
	return errors;

}
/***************************************************************************
*   Routine: mgras_hq3_hag_wddReg
*
*   Purpose: HQ3 HAG_CTX double word register test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write thru CMD fifo, verify via PIO acess 
*
****************************************************************************/
/* HQ3 HAG_CTX  double word register read/write  */
mgras_hq3_hag_wddReg(__uint32_t reg_offset, __uint32_t data_h,__uint32_t data_l, __uint32_t mask)
{
	__uint32_t errors = 0;
	__uint32_t hag_cmd;
	__uint32_t  actual_h, actual_l;

	/* construct HAG command */
	hag_cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, reg_offset | HAG_CTX_CMD_BASE, 8);
	/* Write the command and data out through CFIFO */
	/* mgras_wrHQ3word(hag_cmd);
	mgras_wrHQ3word(data); */
	HQ3_PIO_WR(CFIFO1_ADDR, hag_cmd, 0xffffffff);
	HQ3_PIO_WR(CFIFO1_ADDR, data_h, 0xff);
	HQ3_PIO_WR(CFIFO1_ADDR, data_l, 0xffffffff);

	/* Wait a while here */

	time_delay(10); /* or what */
	/* Read the data back through PIO */
	HQ3_PIO_RD((reg_offset << 2) + HAG_CTX_BASE , 0xff, actual_h);
	HQ3_PIO_RD((((reg_offset+1) << 2) | HAG_CTX_BASE) , mask, actual_l);
	COMPARE("HQ3 HAG register test", reg_offset, data_h, actual_h, 0xff, errors);
	COMPARE("HQ HAG register test", reg_offset+1, data_l, actual_l, mask, errors);
	return errors;

}
/* HQ3 HAG_CTX registers test */
/***************************************************************************
*   Routine: mgras_hq3_hag_reg_test
*
*   Purpose: HQ3 HAG_CTX registers test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Read/Write HQ3 HAG_CTX  registers 
*
****************************************************************************/

__uint32_t
mgras_hq3_hag_reg_test(void)
{
	__uint32_t errors = 0;
	__uint32_t i, reg_index, pattern;

	msg_printf(VRB,"MGRAS HQ Hag_ctx Register test\n");

	/* Pattern test */
	

	/* for single word commands */

	/* Testing register 0x800 to 0x80a */
	for (reg_index = 0; reg_index < HQ3_HAG_REGS_SIZE ; reg_index++) 
	{
	   /* patterns test */

	   for (i = 1; i < 7 ; ++i)
{
		errors += mgras_hq3_hag_wdReg(hq3_hag_regs[reg_index].addr_offset, patrn[i], hq3_hag_regs[reg_index].mask);  
}

	   /* Walking 1 */
	   for (i = 0, pattern = 1; i < 32;i++, pattern = (pattern <<= 1) & hq3_hag_regs[reg_index].mask)
		errors += mgras_hq3_hag_wdReg(hq3_hag_regs[reg_index].addr_offset, pattern, hq3_hag_regs[reg_index].mask); 

           /* Walking 0 */

	   for (i = 0, pattern = 0xfffffffe & hq3_hag_regs[reg_index].mask ; i < 31;i++, pattern = ((pattern <<= 1) | 1) & hq3_hag_regs[reg_index].mask)
		errors += mgras_hq3_hag_wdReg(hq3_hag_regs[reg_index].addr_offset, pattern, hq3_hag_regs[reg_index].mask); 

	}
	/* Testing register 0x820 to 0x828i( those involved double word cmds */

	for (reg_index = 0; reg_index <  HQ3_HAG_DWREGS_SIZE ; reg_index++)
	{
	   /* patterns test */

	   for (i = 0; i < 7 ; ++i)

		errors += mgras_hq3_hag_wddReg(hq3_hag_dwregs[reg_index].addr_offset, patrn[i], patrn[i], hq3_hag_dwregs[reg_index].mask); 

	   /* Walking 1 */
	   for (i = 0, pattern = 1; i < 40;i++, pattern = (pattern <<= 1))
		errors += mgras_hq3_hag_wddReg(hq3_hag_dwregs[reg_index].addr_offset, pattern,pattern,  hq3_hag_dwregs[reg_index].mask); 

           /* Walking 0 */

	   for (i = 0, pattern = 0xfffffffe; i < 40;i++, pattern = ((pattern <<= 1) | 1))
		errors += mgras_hq3_hag_wddReg(hq3_hag_dwregs[reg_index].addr_offset, pattern, pattern, hq3_hag_dwregs[reg_index].mask); 
	}

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_HAG_CTX_TEST]), errors);

}



/**** HQ3 Ucode memory test *****/

/***************************************************************************
*   Routine: mgras_hq3ucode_mem_pat
*
*   Purpose: HQ3 internal Ucode memory pattern test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write memory pattern into Ucode RAM and read back verify
*
****************************************************************************/

mgras_hq3ucode_mem_pat(__uint32_t offset, __uint32_t  size)
{
	__uint32_t patrn_index, actual, errors = 0;
	__uint32_t  addr;
	for (patrn_index = 0; patrn_index < 8; patrn_index++ ) 
	{
	   /* Write pattern to all ucode RAM */

           for (addr = 0; addr < size; addr = addr + 4) 
	   {
	      HQ3_PIO_WR(addr+offset, patrn[patrn_index], HQ3_UCODE_MASK);
	   /* Read back and verify */
	
	      HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	      HQ3_PIO_RD(addr+offset, HQ3_UCODE_MASK, actual);
	      COMPARE("HQ UCODE pattern test", addr, patrn[patrn_index], actual, HQ3_UCODE_MASK, errors);
	   } 
	}
	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_UCODE_PATTERN_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_hq3ucode_walking_bit
*
*   Purpose: HQ3 internal Ucode memory walking bits test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write walking 1 and 0 patterns to ucode ram and verify
*
****************************************************************************/
mgras_hq3ucode_walking_bit(__uint32_t offset, __uint32_t  size)
{
	__uint32_t patrn_index, expected,actual, errors = 0;
	__uint32_t  addr;

	msg_printf(DBG,"MGRAS HQ Ucode Walking Bit test\n");

	for (patrn_index = 0; patrn_index < 32; patrn_index++ ) 
	{
	   /* Write walking bits to all ucode RAM */

           for (addr = 0,expected = walk_1_0[patrn_index]; addr < size; addr = addr + 4) 
	      HQ3_PIO_WR(addr+offset, expected, HQ3_UCODE_MASK);

	      HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);
	   
	   /* Read back and verify */

	   for (addr = 0,expected = walk_1_0[patrn_index]; addr < size; addr = addr + 4)
	   {
	      HQ3_PIO_RD(addr+offset, HQ3_UCODE_MASK, actual);
	      COMPARE("HQ UCODE walkingbit Test", addr, expected, actual, HQ3_UCODE_MASK, errors);
	   } 
	}
	return errors;
}

/***************************************************************************
*   Routine: mgras_hq3ucode_DataBus
*
*   Purpose: HQ3 internal Ucode memory data bus test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write walking 1 to first locaton of the Ucode Ram to make
*          sure data bus is OK
*
****************************************************************************/
		
mgras_hq3ucode_DataBus(__uint32_t offset)
{
	__uint32_t i, expected,actual, errors = 0;
	__uint32_t  addr = 0x0;

	msg_printf(DBG,"MGRAS HQ Ucode Data Bus test\n");

	for (i = 0, expected = 1; i < 24; i++, expected <<= 1 ) 
	{
	   /* Write pattern to the first ucode RAM location */

	      HQ3_PIO_WR((__uint32_t)(offset+addr), expected, HQ3_UCODE_MASK);

	      HQ3_PIO_WR(TLB_VALIDS_ADDR, ~expected, 0xffffffff);
	   
	   /* Read back and verify */

	      HQ3_PIO_RD((__uint32_t)(offset+addr),HQ3_UCODE_MASK, actual);
	      COMPARE("HQ UCODE DataBus test", (__uint32_t)(offset+addr), expected, actual, HQ3_UCODE_MASK, errors);
	} 
	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_UCODE_DATA_BUS_TEST]), errors); 
}

/***************************************************************************
*   Routine: mgras_hq3ucode_AddrBus
*
*   Purpose: HQ3 internal Ucode memory address bus test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write walking 1 and 0 address pattern to different Ucode RAM
*          location to make sure address line does not get stuck
*
****************************************************************************/

mgras_hq3ucode_AddrBus(__uint32_t offset)
{
	__uint32_t addr_index, expected,actual, errors = 0;
	__uint32_t  addr;

	msg_printf(DBG,"MGRAS HQ Ucode Addr Bus test\n");

	for (addr_index = 0; addr_index < HQ3UCODE_PAT_SIZE ; addr_index++ ) 
	{
	   /* Write walking 0 and 1 to address lines */
	      expected = (~ hq3ucode_addr_pat[addr_index]) &HQ3_UCODE_MASK;

	      HQ3_PIO_WR(((__uint32_t) hq3ucode_addr_pat[addr_index]+ offset), expected, HQ3_UCODE_MASK);

	}

	   /* Read back and verify */

	for (addr_index = 0; addr_index < HQ3UCODE_PAT_SIZE ; addr_index++ ) 
	{
	      expected = (~ hq3ucode_addr_pat[addr_index]) &HQ3_UCODE_MASK;
	      HQ3_PIO_RD(((__uint32_t) hq3ucode_addr_pat[addr_index]+ offset),HQ3_UCODE_MASK, actual);
	      COMPARE("HQ UCODE AddrBus test", addr, expected, actual, HQ3_UCODE_MASK, errors);
	} 
	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_UCODE_ADDR_BUS_TEST]), errors);
}
		
/***************************************************************************
*   Routine: mgras_hq3ucode_AddrUniq
*
*   Purpose: HQ3 internal Ucode memory address uniqueness test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Write address as data to all Ucode RAM and verify
*
****************************************************************************/

mgras_hq3ucode_AddrUniq(__uint32_t offset, __uint32_t size)
{
	__uint32_t  actual, errors = 0;
	__uint32_t  addr;

	msg_printf(DBG,"MGRAS HQ Ucode Addrunique test\n");

	/* Write address as data to the all ucode RAM location */
	for (addr = 0; addr < size  ; addr = addr + 4 ) 
	      HQ3_PIO_WR((addr+offset), addr, HQ3_UCODE_MASK);

	   
	   /* Read back and verify */

	for (addr = 0; addr < size  ; addr = addr + 4 ) 
	{
	      HQ3_PIO_RD((addr + offset), HQ3_UCODE_MASK, actual);
	      COMPARE("HQ UCODE AddrUniq test", addr, addr, actual, HQ3_UCODE_MASK, errors);
	}
	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_UCODE_ADDRUNIQ_TEST]), errors); 
}
		
	/*  HQ3 ucode RAM test */

/***************************************************************************
*   Routine: mgras_ucode_test
*
*   Purpose: HQ3 internal Ucode memory test 
*
*   History: 10/13/94 : Original Version.
*
*   Notes: Invoke all the subtests 
*
****************************************************************************/
__uint32_t
mgras_ucode_test(void)
{
	__uint32_t hq_config;
	__uint32_t 	errors = 0;

	msg_printf(VRB,"MGRAS HQ Ucode RAM test\n");

	/* stall ucode first */ 
#ifndef MGRAS_BRINGUP
	/* mgbase->hq_config |= HQ3_UCODE_STALL; */
#endif
	HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK,  hq_config);
	HQ3_PIO_WR(HQ3_CONFIG_ADDR,hq_config | HQ3_UCODE_STALL , HQ3_CONFIG_MASK);

	errors += mgras_hq3ucode_DataBus(HQUC_ADDR);  
	errors += mgras_hq3ucode_AddrBus(HQUC_ADDR);  
	errors += mgras_hq3ucode_AddrUniq(HQUC_ADDR,HQUC_SIZE);  
	errors += mgras_hq3ucode_mem_pat(HQUC_ADDR,HQUC_SIZE);  
	errors += mgras_hq3ucode_walking_bit(HQUC_ADDR,HQUC_SIZE);  

	/* unstall ucode first */ 
#ifndef MGRAS_BRINGUP
	/* base->hq_config &= ~HQ3_UCODE_STALL; */
#endif

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK,  hq_config);
	HQ3_PIO_WR(HQ3_CONFIG_ADDR,hq_config & ~HQ3_UCODE_STALL , HQ3_CONFIG_MASK);
	return errors;
}

int
mgras_Hq3RegWr(int argc, char** argv)
{
	__uint32_t regaddr, data;
	if (argc != 3) {
		msg_printf(SUM, "usage: %s RegAddrOffset data \n" , argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&regaddr);
	atohex(argv[2], (int*)&data);
	msg_printf(VRB, "regaddr = 0x%x data = 0x%x\n", regaddr, data);
	HQ3_PIO_WR(regaddr, data , 0xfffffff);
	return 0;
}

int
mgras_Hq3RegRd(int argc, char** argv)
{
	__uint32_t regaddr, data, mask;
	if (argc != 3) {
		msg_printf(SUM, "usage: %s RegAddrOffset Mask\n" , argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&regaddr);
	atohex(argv[2], (int*)&mask);
	msg_printf(VRB, "regaddr = 0x%x mask = 0x%x\n", regaddr, mask);
	HQ3_PIO_RD(regaddr, mask, data);

	msg_printf(VRB,"Reading Hq3 reg 0x%x = 0x%x\n",regaddr,data);
	return 0;
}

#ifdef MFG_USED
int
mgras_Hq3RegVf(int argc, char** argv)
{
	__uint32_t regaddr, data, rd_data, mask;
	if (argc != 4) {
		msg_printf(SUM, "usage: %s RegAddrOffset Data Mask\n" , argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&regaddr);
	atohex(argv[2], (int*)&data);
	atohex(argv[3], (int*)&mask);
	HQ3_PIO_WR(regaddr,data , mask);
	HQ3_PIO_RD(regaddr, mask, rd_data);

	if (data != rd_data) {
		msg_printf(ERR,"Hq3 reg. 0x%x Write/Verify error: Data written = 0x%x Data read = 0x%x\n",regaddr,data, rd_data);
		return 1;
	}
	return 0;
}

int
mgras_Hq3RegTest(int argc, char** argv)
{
	__uint32_t regaddr, mask;
	__uint32_t expected, i, actual;
	__uint32_t  errors = 0;

	if (argc != 3) {
		msg_printf(SUM, "usage: %s RegAddrOffset Mask\n" , argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&regaddr);
	atohex(argv[2], (int*)&mask);

	/* Walking 1 */

	for (i = 0, expected = 1; i < 32; i++, expected <<=1)
	{
  	   HQ3_PIO_WR(regaddr, expected, mask);
	   
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	   HQ3_PIO_RD(regaddr, mask, actual);
	   msg_printf(DBG,"expected = 0x%x actual = 0x%x\n",expected,actual);
	   COMPARE("HQ - HQ bus test", regaddr, expected, actual, mask, errors); 

	   HQ3_PIO_WR(regaddr, expected, mask);

	}

	/* Walking 0 */

	for (i = 0, expected = 0xfffffffe; i < 32; i++, expected = ((expected <<=1) | 1))
	{
  	   HQ3_PIO_WR(regaddr, expected, mask);
	   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0xdeadbeef, 0xffffffff);

	   HQ3_PIO_RD(regaddr, mask, actual);
	   msg_printf(DBG,"expected = 0x%x actual = 0x%x\n",expected,actual);
	   COMPARE("HQ3- HQ bus test", regaddr, expected, actual, mask, errors); 

	   HQ3_PIO_WR(regaddr,expected,mask);
	}
	return errors;
}

int
mgras_Hq3FifoWr(int argc, char** argv)
{
	__uint32_t data;
	if (argc != 2) {
		msg_printf(SUM, "usage: %s data\n" , argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&data);
	HQ3_PIO_WR(CFIFO1_ADDR, data, 0xffffffff);

	return 0;
}

int
mgras_Hq3FifoWr64(int argc, char** argv)
{
	__uint32_t data, cmd;
	if (argc != 3) {
		msg_printf(SUM, "usage: %s cmd data\n" , argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&cmd);
	atohex(argv[2], (int*)&data);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, 0xffffffff);
	HQ3_PIO_WR(CFIFO1_ADDR, data, 0xffffffff);

	return 0;
}
#endif /* MFG_USED */

int
mgras_hq3_init_check(void)
{
	__uint32_t id, config, gio_config, errors = 0;
	__uint32_t init_config = NUMGES | UCODE_STALL | DIAG_GIO_CP;
	__uint32_t  init_gio_config = GIO_BIGENDIAN_BIT | DCB_BIGENDIAN | EXT_PIPEREG | TLB_UPDATE | HQFIFO_HW | MST_W_BC_DAT | MST_GNT_AS | MST_R_DAT_AS | MST_W_DAT_AS | MST_FREE_REQ;
#ifdef DEBUG			/* assume this is correct in production */
	unsigned int bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) <<
				mgras_baseindex(mgbase);
	extern unsigned int _gio64_arb;

	if ((_gio64_arb & bits) != bits) {
		msg_printf(SUM,"GIO64_ARB not set correctly!(Check GE)\n");
	}
#endif

#if HQ4
#else

	/* Check GIO_ID first */
	HQ3_PIO_RD(GIO_ID_ADDR, 0xffffffff, id);
#ifdef DEBUG
	msg_printf(DBG,"HQ3 GIO_ID read actual = 0x%x expected = 0x%x\n",id, GIO_ID); 
#endif
	if (id != GIO_ID)
	{
		msg_printf(ERR,"HQ GIO_ID read error: actual = 0x%x, expected = 0x%x\n",id, GIO_ID); 
		errors++;
	}
#endif
	/* Read config register */

#ifdef DEBUG
	msg_printf(DBG,"Read config register\n");
	msg_printf(DBG ,"HQ _PIO_RD(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n" ,HQ3_CONFIG_ADDR, 0xffffffff,  READ_MODE, config, 0, HQ_MOD);
#endif

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, 0xffffffff, config);

#ifdef DEBUG
	msg_printf(DBG,"HQ config reg read : actual = 0x%x, expected = 0x%x\n",config, init_config); 
#endif
	if (config != init_config)
	{
		msg_printf(DBG,"HQ config register read error: actual = 0x%x, expected = 0x%x\n",config, init_config); 
		errors++;
	}

	msg_printf(DBG, "Read gio_config register\n");


#ifdef DEBUG
	msg_printf(DBG, "HQ _PIO_RD(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n" ,GIO_CONFIG_ADDR, 0xffffffff,  READ_MODE, gio_config, 0, HQ_MOD);
#endif

	HQ3_PIO_RD(GIO_CONFIG_ADDR, 0xffffffff,  gio_config);

#ifdef DEBUG
	msg_printf(DBG,"HQ gio_config reg read : actual = 0x%x, expected = 0x%x\n",gio_config, init_gio_config); 
#endif
	if (gio_config != init_gio_config)
	{
		msg_printf(DBG,"HQ gio_config regisater read error: actual = 0x%x, expected = 0x%x\n",gio_config, init_gio_config); 
		errors++;
	}

	mgras_HQ3dctctrl_test();

	return errors;
}

int
mgras_hq3_test(int argc, char **argv)
{
	int errors= 0;
	__uint32_t testno;
	__uint32_t id;

#ifndef HQ4
#ifdef DEBUG		/* assume this is correct for production */
	unsigned int bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) <<
			mgras_baseindex(mgbase);
	extern unsigned int _gio64_arb;

	/* Check GIO BUS init */
	if ((_gio64_arb & bits) != bits) {
		msg_printf(ERR,"GIO64_ARB not set correctly!\n");
		return 1;
	}
#endif

	HQ3_PIO_RD(GIO_ID_ADDR, 0xffffffff,  id);
#ifdef DEBUG
	msg_printf(DBG,"HQ GIO_ID read : actual = 0x%x, expected = 0x%x\n",id, GIO_ID); 
#endif
	if (id != GIO_ID)
	{
		msg_printf(DBG,"HQ GIO_ID read error: actual = 0x%x, expected = 0x%x\n",id, GIO_ID); 
		errors++;
	}
#endif

	_mg_set_ZeroGE();
#if 0
	/* Configure 0 GE */
	msg_printf(DBG,"Set to 0 GE\n");
	HQ3_PIO_RD(HQ3_CONFIG_ADDR, 0xffffffff, hq_config);
	hq_config &= ~0x1;  /* 0 GE */
	HQ3_PIO_WR(HQ3_CONFIG_ADDR, hq_config, HQ3_CONFIG_MASK);
	msg_printf(DBG,"Set to 0 GE Done\n");
#endif


  if (argc > 3) { 
    msg_printf(SUM, "usage: %s testno\n", argv[0]); 
#if HQ4
    msg_printf(SUM, " testno = 1 - HQ XIO bus test\n");
#else
    msg_printf(SUM, " testno = 1 - HQ GIObus test\n");
#endif
    msg_printf(SUM, " testno = 2 - HQ bus test\n");
    msg_printf(SUM, " testno = 3 - HQ DCB Control Registers  test\n");
    msg_printf(SUM, " testno = 4 - HQ RSS_DataBus test\n");
    msg_printf(SUM, " testno = 5 - HQ TLB test\n");
    msg_printf(SUM, " testno = 6 - HQ REIF Registers test\n");
    msg_printf(SUM, " testno = 7 - HQ HAG Registers test\n");
    msg_printf(SUM, " testno = 8 - HQ Ucode memory test\n");
    return -1;
  }  
  else
    if (argc == 1) {

        errors += mgras_HQGIObus(); 
        errors += mgras_HQbus(); 
    /*  errors += mgras_hq3_reg_test(); */
        errors += mgras_HQ3dctctrl_test(); 
    /*  errors += mgras_RSS_DataBus(); */
        errors += mgras_tlb_test();
        errors += mgras_hq3_reif_reg_test(); 
        errors += mgras_hq3_hag_reg_test(); 
        errors += mgras_ucode_test(); 

	/* each test reports status when VRB */

	return errors; 
    }
    else {
        testno = atoi(*(++argv));

	switch(testno) {

	/* HQGIObus test */
        case 1:
		if (mgras_HQGIObus() > 0) {
		   return -1;
		}
		break;
	/* HQbus test */
        case 2:
		if (mgras_HQbus() > 0) {
		   return -1;
		}
		break;
	/* DCB Control regisers test */
	case 3:
		if (mgras_HQ3dctctrl_test() > 0) {
		   return -1;
		}
		break;

	/* HQ3 RSS Data Bus test */
	case 4:
		if (mgras_RSS_DataBus() != 0) {
		   return -1;
		}
		break;
  	/* HQ3 HQ3 TLB test */
	case 5:
		if (mgras_tlb_test() > 0) {
		   return -1;
		}
		break;
	/* HQ3 REIF Registers test */
	case 6:
		if (mgras_hq3_reif_reg_test() > 0) {
		   return -1;
		}
		break;
	/* HQ3 HAG Registers test */ 
	case 7:
		if (mgras_hq3_hag_reg_test() > 0) {
		   return -1;
		}	
		break;

	/* HQ3 Ucode memory test */ 
	case 8:
		if (mgras_ucode_test() > 0) {
		   return -1;
		}
		break;

	default: 
		msg_printf(ERR,"Test not defined\n");
		return -1;
	}
    }
   	return errors; 
}

__int32_t
mgras_hq3_ucode_dnload(void)
{
	__int32_t errors = 0;
	errors = mgrasDownloadHQ3Ucode(mgbase, &iset_table[0], (sizeof(iset_table)/sizeof(iset_table[0])), 1);

	return (errors);
}

#ifdef MFG_USED
void
mgras_hq3_reg_dump(void)
{
	__uint32_t rd_value;

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"HQ_CONFIG(0x%x) = 0x%x\n",HQ3_CONFIG_ADDR,rd_value);

	HQ3_PIO_RD(HQPC_ADDR, 0xffffffff,  rd_value);
	msg_printf(VRB,"HQPC(0x%x) = 0x%x\n",HQPC_ADDR,rd_value);

	HQ3_PIO_RD(CP_DATA_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"CP_DATA(0x%x) = 0x%x\n",CP_DATA_ADDR,rd_value);

	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"FLAG_SET_PRIV(0x%x) = 0x%x\n",FLAG_SET_PRIV_ADDR,rd_value);

	HQ3_PIO_RD(FLAG_ENAB_SET_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"FLAG_ENAB_SET_PRIV(0x%x) = 0x%x\n",FLAG_ENAB_SET_ADDR,rd_value);

	HQ3_PIO_RD(INTR_ENAB_SET_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"INTR_ENAB_SET_PRIV(0x%x) = 0x%x\n",INTR_ENAB_SET_ADDR,rd_value);

	HQ3_PIO_RD(GIO_CONFIG_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"GIO_CONFIG(0x%x) = 0x%x\n",GIO_CONFIG_ADDR,rd_value);

#if !HQ4
	HQ3_PIO_RD(GIO_STATUS_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"GIO_STATUS(0x%x) = 0x%x\n",GIO_STATUS_ADDR,rd_value);

	HQ3_PIO_RD(STATUS_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"STATUS(0x%x) = 0x%x\n",STATUS_ADDR,rd_value);
#endif

	HQ3_PIO_RD(FIFO_STATUS_ADDR, 0xffffffff, rd_value);
	msg_printf(VRB,"FIFO_STATUS(0x%x) = 0x%x\n",FIFO_STATUS_ADDR,rd_value);
}

#if HQ4
void mgras_hq4peek (int argc, char **argv)
{

    __uint32_t offset, rcv, mask = ~0x0;
    __uint32_t bad_arg = 0;

    msg_printf(DBG, "Entering mgras_hq4peek...\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &offset);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &offset);
                    }
                    break;
                default: 
		    bad_arg++;
                    break;
	}
	argc--; argv++;
    }

    if ( bad_arg || argc ) {
	msg_printf(SUM,
	   " Usage: mg_hq4peek -o <reg offset>  \n");
	return;
    }	   

    msg_printf(DBG, "mgbase 0x%x; offset 0x%x\n", mgbase, offset);

    HQ3_PIO_RD(offset, mask, rcv);

    msg_printf(SUM, "reg offset 0x%x value 0x%x \n", offset, rcv);

    msg_printf(DBG, "Leaving mgras_hq4peek...\n");
}

void mgras_hq4poke(int argc, char **argv)
{
    __uint32_t offset, val, mask = ~0x0;
    __uint32_t bad_arg = 0;

    msg_printf(DBG, "Entering mgras_hq4poke...\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &offset);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &offset);
                    }
                    break;
                case 'v':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &val);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &val);
                    }
                    break;
                default: 
		    bad_arg++;
                    break;
	}
	argc--; argv++;
    }

    if ( bad_arg || argc ) {
	msg_printf(SUM,
	   " Usage: mg_hq4poke -o <reg offset> -v <value> \n");
	return;
    }	   

    msg_printf(DBG, "mgbase 0x%x; offset 0x%x\n", mgbase, offset);

    HQ3_PIO_WR(offset, val, mask);

    msg_printf(DBG, "Leaving mgras_hq4poke...\n");
}


void mgras_xbowpeek (int argc, char **argv)
{

    __uint32_t offset, rcv, mask = ~0x0, port;
    __uint32_t bad_arg = 0;
    xbow_t     *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);

    msg_printf(DBG, "Entering mgras_xbowpeek...\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'p':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &port);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &port);
                    }
                    break;
                default: 
		    bad_arg++;
                    break;
	}
	argc--; argv++;
    }

    if ( bad_arg || argc ) {
	msg_printf(SUM,
	   " Usage: mg_xbowpeek -p <port#>  \n");
	return;
    }	   

    msg_printf(SUM, " link_control 0x%x \n", xbow_wid->xb_link(port).link_control);
    msg_printf(SUM, " link_status 0x%x \n", xbow_wid->xb_link(port).link_status);
    msg_printf(SUM, " link_arb_upper 0x%x \n", xbow_wid->xb_link(port).link_arb_upper);
    msg_printf(SUM, " link_arb_lower 0x%x \n", xbow_wid->xb_link(port).link_arb_lower);
    msg_printf(SUM, " link_aux_status 0x%x \n", xbow_wid->xb_link(port).link_aux_status);


    msg_printf(DBG, "Leaving mgras_xbowpeek...\n");
}

void mgras_xbowpoke(int argc, char **argv)
{
    __uint32_t offset, val, mask = ~0x0;
    __uint32_t bad_arg = 0;
    uchar_t     *xbow_wid = (uchar_t *)K1_MAIN_WIDGET(XBOW_ID);

    msg_printf(DBG, "Entering mgras_xbowpoke...\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &offset);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &offset);
                    }
                    break;
                case 'v':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &val);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &val);
                    }
                    break;
                default: 
		    bad_arg++;
                    break;
	}
	argc--; argv++;
    }

    if ( bad_arg || argc ) {
	msg_printf(SUM,
	   " Usage: mg_xbowpoke -o <reg offset> -v <value> \n");
	return;
    }	   

    msg_printf(DBG, "mgbase 0x%x; offset 0x%x\n", mgbase, offset);

    *(xbow_wid + offset) = val ; 

    msg_printf(DBG, "Leaving mgras_xbowpoke...\n");
}
#if 0
mgras_testreset (int argc, char **argv)
{

	    __uint32_t port;
    atobu(argv[1], &port);
    msg_printf(DBG, " port number is 0x%x \n", port);
    _mgras_hq4linkreset ((xbowreg_t) port);

}
#endif
#endif /* HQ4 */




#endif /* MFG_USED */
