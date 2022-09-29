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
 *  DMA Diagnostics.
 *  	o Host <-> RE transfer test
 *  	o Host <-> GE transfer test
 *  	o RE <-> RE transfer test
 *
 *  $Revision: 1.39 $
 */


#include <sys/types.h>
#include <sys/sbd.h>
#include <libsk.h>
#include "uif.h"
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "iset.diag"
#include "ide_msg.h"
#include "mgras_hq3.h"
#include "mgras_hq3_cp.h"
#include "mgras_dma.h"
#include "ge11symtab.h"

#ifndef MGRAS_BRINGUP
#define	DMA_DEBUG	0
#endif /* MGRAS_BRINGUP */

/*
 * The following constant makes sure that the tests
 * use the general purpose scratch buffer rather than
 * a dedicated buffer for storing translation table entries
 */
#define trans_table mgras_scratch_buf


#if 0
#define   K1_TO_PHYS(x)   ((__paddr_t)(x)&0x1FFFFFFF)      /* kseg1 to physical */
#endif


__uint32_t		dlist0[DLIST0_SIZE];
__uint32_t		dlist1[DLIST1_SIZE];

__uint32_t
_mgras_host_hq_dl_dma(__uint32_t nWords, __uint32_t *data, __uint32_t cmd_addr)
{
   __uint32_t		i, cmd, gio_config, t1, t2;
   __uint32_t		status = 0, remainBits, data_lo, data_hi;
   __uint32_t		data_hi_tbl_base, gtlbMode;
   __uint64_t		mask64 = ~0x0, phys0, physPage0, phys1, physPage1;
#ifdef DEBUG		/* now assume GIO64_ARB is correct for production */
   unsigned int bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) << 
			mgras_baseindex(mgbase);
   extern __uint32_t	 _gio64_arb;

   if ((_gio64_arb & bits) != bits) {
	msg_printf(ERR,"GIO64_ARB not set correctly!\n");
	return 1;
   }
#endif


   FormatterMode(0x201);

#if !HQ4
   /* Setup the translation table */
   trans_table[0] = (K1_TO_PHYS(&(dlist0[0]))) >> 12; /* XXX page size = 2^12 */
   trans_table[1] = (K1_TO_PHYS(&(dlist1[0]))) >> 12; /* XXX page size = 2^12 */
#else
   phys0 = (K1_TO_PHYS(&(dlist0[0]))); 
   physPage0 = phys0 >> 12;
   phys1 = (K1_TO_PHYS(&(dlist1[0])));
   physPage1 = phys1 >> 12;
   trans_table[0] = physPage0;
   trans_table[1] = physPage1;

   /*
    * The translation table can hold a maximum of 22 bits;
    * The remaining 14 bits must be kept in graphics dma mode B register
    */
   remainBits = HQ4_DMAB_ADDX(physPage0 >> 22);

   msg_printf(DBG, "dlist0 0x%x; dlist1 0x%x\n", &(dlist0[0]), &(dlist1[0]));
   msg_printf(DBG, "phys0 0x%x; physPage0 0x%x\n", phys0, physPage0);
   msg_printf(DBG, "phys1 0x%x; physPage1 0x%x\n", phys1, physPage1);
   msg_printf(DBG, "remainBits = 0x%x\n", remainBits);

   HQ3_PIO_WR(GRAPHICS_DMA_MODE_B, remainBits, ~0x0);
   HQ3_PIO_WR(GRAPHICS_DMA_MODE_A, (HEART_ID << 16), ~0x0);
#endif /* !HQ4 */

	/* store the 14 bits in the graphics dma mode 8 register */

   /* Invalidate TLB entries */
   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0x0, ~0x0);

#if HQ_CAN_NOT_SEARCH_TLB_ENTRIES
   tlb_entry[0] = (DL_POOL << 16);		/* key for dlist0 */
   tlb_entry[1] = trans_table[0];		/* phy page for dlist0 */
   tlb_entry[2] = (1 | (DL_POOL << 16));	/* key for dlist1 */
   tlb_entry[3] = trans_table[1];		/* phy page for dlist1 */

   /* Put valid bits in there */
   mgbase->tlb[14] = tlb_entry[2];
   mgbase->tlb[15] = tlb_entry[3];
   mgbase->tlb[30] = tlb_entry[0];
   mgbase->tlb[31] = tlb_entry[1];

   msg_printf(DBG, "tlb addr 0x%x data 0x%x\n", &(mgbase->tlb[14]), tlb_entry[2]);
   msg_printf(DBG, "tlb addr 0x%x data 0x%x\n", &(mgbase->tlb[15]), tlb_entry[3]);
   msg_printf(DBG, "tlb addr 0x%x data 0x%x\n", &(mgbase->tlb[30]), tlb_entry[0]);
   msg_printf(DBG, "tlb addr 0x%x data 0x%x\n", &(mgbase->tlb[31]), tlb_entry[1]);
#endif

   t1 = (nWords + 3);
   t2 = (nWords * 4);
   /* Fill in display lists - compiler complains if we statically initialize */
   dlist0[0] = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, (HAG_CTX_CMD_BASE + DL_JUMP_NO_WAIT), 8);
   dlist0[1] = ((__paddr_t)dlist1) & ((1 << 13) - 1); /* 2 * page size mask */
   dlist0[2] = t1;         		/* Length of dl1 */

   dlist1[0] = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, (HAG_CTX_CMD_BASE + DL_RETURN), 4);
   dlist1[1] = 0;           /* No wait */
   dlist1[2] = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, cmd_addr, t2);  
   for (i = 0; i < nWords; i++, data++) {
	dlist1[i+3] = *data;
   }

   /* Tell the Gfx to handle TLB misses */
   HQ3_PIO_RD(GIO_CONFIG_ADDR, ~0x0, gio_config);
	us_delay(10000);
   gio_config |= HQ_GIOCFG_HQ_UPDATES_TLB_BIT;
   HQ3_PIO_WR(GIO_CONFIG_ADDR, gio_config, ~0x0);

   /* Clear the hag state flags */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + HAG_STATE_FLAGS), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
	flush_cache();

#if HQ4
   /* Set up the table base address */
   phys0 = (K1_TO_PHYS(&(trans_table[0])));
   data_lo = (__uint32_t)(phys0); /* the lower 32 bits */
   data_hi = (__uint32_t)((__uint64_t)phys0 >> 32);
   data_hi_tbl_base = data_hi & 0xff; /* the upper 8 bits */

   msg_printf(DBG, "Setting up the table base and tlb mode registers...\n");
   msg_printf(DBG, "phys0 0x%x; data_lo 0x%x; data_hi_tbl_base 0x%x\n",
	phys0, data_lo, data_hi_tbl_base);
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DL_TABLE_BASE_ADDR), 8);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR64(CFIFO1_ADDR, data_hi_tbl_base, data_lo, mask64);

   /* store the remaining 8 bits and the compat mode in graphics TLB mode */
   gtlbMode = ((data_hi >> 8) & 0xff) << 24;
   gtlbMode |= (1 << 22);
   gtlbMode |= (HEART_ID << 16);
   msg_printf(DBG, "data_hi 0x%x; gtlbMode 0x%x\n", data_hi, gtlbMode);
   HQ3_PIO_WR(GRAPHICS_TLB_MODE, gtlbMode, ~0x0);
	flush_cache();
#else
   /* Set up the table base address */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DL_TABLE_BASE_ADDR), 8);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, (K1_TO_PHYS(&(trans_table[0]))), ~0x0);
#endif /* HQ4 */
   
   /* Set up the page size */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DMA_PG_SIZE), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
   
   /* Set up the page range */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DMA_PG_NO_RANGE_DL), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x2, ~0x0);
   
   /* Set up the page range */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + MAX_DL_TBL_PTR_OFFSET), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x2, ~0x0);
   
   /* IDE might keep the data in caches, HQ can not DMA from caches */
   flush_cache();

   /* Kick off the DL DMA */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DL_JUMP_NO_WAIT), 8);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, (__uint32_t)(((__paddr_t)dlist0) & ((1 << 13) - 1)), 
	      ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, DLIST0_SIZE, ~0x0);
   flush_cache();

   /* Poll till dma is finished */
   HQ3_POLL_DMA(0x0, (HQ_BUSY_HAG_DMA_BIT | HQ_BUSY_MSTR_DMA_BIT), BUSY_DMA_ADDR, status);

   if (status == DMA_TIMED_OUT) return(status);

   return (0);
}

/***************************************************************************
*   Routine: mgras_host_hqdma
*
*   Purpose: Host to HQ3 DMA test 
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Creates a display list that sets up one of the HQ3 registers
*	   DMA the display list to HQ3
*	   Read back (using PIO) the value of the register and verify the DMA operation
*
***************************************************************************/
__uint32_t
mgras_host_hqdma(void)
{
   __uint32_t		errors = 0;
   __uint32_t		cmd, y_dec_factor, y_dec_factor_saved, status;
   __uint32_t		initial_value = (0x55555555 & Y_DEC_FACTOR_MASK);
   __uint32_t		new_value = (0xaaaaaaaa & Y_DEC_FACTOR_MASK);

   msg_printf(DBG, "Start mgras_host_hqdma\n");

#if HQ4
   mgbase->flag_clr_priv = ~0;
   mgbase->flag_enab_clr = ~0;
   mgbase->flag_intr_enab_clr = ~0;
   mgbase->stat_intr_enab_clr = ~0;
#endif

   /* Save the register */
   HQ3_PIO_RD((HAG_CTX_BASE + (IM_Y_DEC_FACTOR << 2)), ~0x0, y_dec_factor_saved);

   /* Set the register with some initial value */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (HAG_CTX_CMD_BASE + IM_Y_DEC_FACTOR), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, initial_value, ~0x0);

	flush_cache();

   if ((status = _mgras_host_hq_dl_dma(1, &new_value, (HAG_CTX_CMD_BASE + IM_Y_DEC_FACTOR))) == 0) {

   	/* Read back the value of the register by using PIO */
   	HQ3_PIO_RD((HAG_CTX_BASE + (IM_Y_DEC_FACTOR << 2)), ~0x0, 
		y_dec_factor);
   	if (y_dec_factor == new_value) {

#if DMA_DEBUG
   	    fprintf(stderr, "y_dec_factor w/ DMA = %x\n", y_dec_factor);
#endif

   	    /* Restore the register with the old value */
   	    cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (HAG_CTX_CMD_BASE + IM_Y_DEC_FACTOR), 4);
   	    HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   	    HQ3_PIO_WR(CFIFO1_ADDR, y_dec_factor_saved, ~0x0);

#if DMA_DEBUG
   	    HQ3_PIO_RD((HAG_CTX_BASE + (IM_Y_DEC_FACTOR << 2)), ~0x0,
	    	y_dec_factor);
   	    fprintf(stderr, "y_dec_factor & NO DMA= %x\n", y_dec_factor);
#endif

	    REPORT_PASS_OR_FAIL((&HQ3_err[HOST_HQ_DMA_TEST]), 0);

   	} 
   }

   if (status == DMA_TIMED_OUT) {
	msg_printf(ERR, "mgras_host_hqdma Test Error: DMA TIME OUT!!!\n");
	errors++;
   }
   REPORT_PASS_OR_FAIL((&HQ3_err[HOST_HQ_DMA_TEST]), 1);
}

/*
 * Host <-> HQ3/CP DMA test
 */
/***************************************************************************
*   Routine: mgras_host_hqdma
*
*   Purpose: Host <-> HQ3/CP DMA test   
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Creates a display list that sets up one of the HQ3 registers
*	   DMA the display list to HQ3
*	   Read back (using CP) the value of the register and verify the DMA operation
*
***************************************************************************/
__uint32_t
mgras_host_hq_cp_dma(void)
{
   __uint32_t 		config;
   __uint32_t		errors = 0;
   __uint32_t		current_pc, current_gpr, test_entry;
   __uint32_t		data[5] = { 0x00112233, 0x44556677, 0x8899AABB,
				    0xCCDDEEFF, 0x5A5A5A5A };

   msg_printf(DBG, "Start mgras_host_hq_cp_dma\n");

   /* Download the Ucode first */
   mgrasDownloadHQ3Ucode(mgbase, &iset_table[0], (sizeof(iset_table)/sizeof(iset_table[0])), 1);

#if 1
   if (_mgras_host_hq_dl_dma(5, data, DFIFO_WR)) {
	msg_printf(ERR, "DMA failed\n");
	errors++;
   }
#endif

   /* Use CFIFO to send command to CP to read back DMA data */

   /* Find the CP ucode test entry first */
   HQ3_PIO_RD((HQUC_ADDR+2*4*DFIFO_RD), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */
   msg_printf(DBG, "test_entry=0x%x\n", test_entry);

   /* Then unstall HQ3 cp ucode */
   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);


   HQ3_PIO_WR(CFIFO1_ADDR, (DFIFO_RD << 8 | 16 | (1 << 22)) , HQ3_CFIFO_MASK);
      do {
	     HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	 } while ((current_pc - test_entry) != 4);

   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   if (current_gpr != data[0]) {
      errors++;
      msg_printf(DBG,"HOST_CP_DMA Read failed. expected = 0x%x, actual = 0x%x\n",data[0],current_gpr);
   }

   HQ3_PIO_WR(CFIFO1_ADDR, (1 << 22) , HQ3_CFIFO_MASK);
      do {
	     HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	 } while ((current_pc - test_entry) != 8);

   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   if (current_gpr != data[1]) {
      errors++;
      msg_printf(DBG,"HOST_CP_DMA Read failed. expected = 0x%x, actual = 0x%x\n",data[1],current_gpr);
   }

   HQ3_PIO_WR(CFIFO1_ADDR, (1 << 22) , HQ3_CFIFO_MASK);
      do {
	       HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	 } while ((current_pc - test_entry) != 12);

   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   if (current_gpr != data[2]) {
      errors++;
      msg_printf(DBG,"HOST_CP_DMA Read failed. expected = 0x%x, actual = 0x%x\n",data[2],current_gpr);
   }

   HQ3_PIO_WR(CFIFO1_ADDR, (1 << 22) , HQ3_CFIFO_MASK);
      do {
	       HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	 } while ((current_pc - test_entry) != 16);

   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   if (current_gpr != data[3]){
      errors++;
      msg_printf(DBG,"HOST_CP_DMA Read failed. expected = 0x%x, actual = 0x%x\n",data[3],current_gpr);
   }
   HQ3_PIO_WR(CFIFO1_ADDR, (1 << 22) , HQ3_CFIFO_MASK);
      do {
	       HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	 } while ((current_pc - test_entry) != 20);

   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   if (current_gpr != data[4]) {
      errors++;
      msg_printf(DBG,"HOST_CP_DMA Read failed. expected = 0x%x, actual = 0x%x\n",data[4],current_gpr);
   }
   msg_printf(DBG, "End mgras_host_hq_cp_dma");

   REPORT_PASS_OR_FAIL((&HQ3_err[HOST_HQ_CP_DMA_TEST]), errors);

}

/***************************************************************************
*   Routine: mgras_host_hq_cp_ge_dma
*
*   Purpose: Host -> HQ3/CP -> GE -> Host DMA path test   
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Creates a display list that sets up one of the HQ3 registers
*	   DMA the display list to GE via HQCP 
*	   Read back (using PIO) the value of the register and verify the DMA operation
*
***************************************************************************/
__uint32_t
mgras_host_hq_cp_ge_dma(void)
{
   __uint32_t		errors = 0;
   __uint32_t		actual;
   __uint32_t		data[6] = { 5, hq_ge_bus, 3, 0x8899AABB,
				    0xCCDDEEFF, 0x5A5A5A5A};

   msg_printf(VRB, "Mgras host_cp_ge DMA test\n");

   GE11_ROUTE_GE_2_HQ();

   if (_mgras_host_hq_dl_dma(6, data, CP_PASS_THROUGH_GE_FIFO)) {
	errors++;
   }

   /* Use GE Pass througp to send DMA data back to Host */

   actual = GE11_Diag_read(~0);
   if (actual != 0x8899AABB) {
	errors++;
	msg_printf(ERR,"mgras_host_hq_cp_ge_dma test failed. expected = 0x%x, actual = 0x%x\n",0x8899AABB, actual);
   }

   actual = GE11_Diag_read(~0);
   if (actual != 0xCCDDEEFF) {
	errors++;
	msg_printf(ERR,"mgras_host_hq_cp_ge_dma test failed. expected = 0x%x, actual = 0x%x\n",0xCCDDEEFF, actual);
   }

   actual = GE11_Diag_read(~0);
   if (actual != 0x5A5A5A5A) {
	errors++;
	msg_printf(ERR,"mgras_host_hq_cp_ge_dma test failed. expected = 0x%x, actual = 0x%x\n",0x5A5A5A5A, actual);
   }

   msg_printf(VRB, "Mgras host_cp_ge DMA test completed\n");

   GE11_ROUTE_GE_2_RE();

   REPORT_PASS_OR_FAIL((&GE11_err[GE11_DMA_TEST]), errors);

}

#ifndef IP28
/*
 * Host <-> HQ3/CP-> GE-> -> RE DMA test
 */
/***************************************************************************
*   Routine: mgras_host_hq_cp_ge_re_dma
*
*   Purpose: Host -> HQ3/CP -> GE -> RE -> Host DMA path test   
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Creates a display list that sets up one of the HQ3 registers
*	   DMA the display list to RE via HQCP and GE 
*	   Read back (using PIO) the value of the register and verify the DMA operation
*
***************************************************************************/
#ifdef MFG_USED
__uint32_t
mgras_host_hq_cp_ge_re_dma(void)
{
   __uint32_t		errors = 0;
   __uint32_t		current_status, actual;
   __uint32_t		data[10] = { 10, hq_ge_re_bus, 1, 0x8899aabb,
				       hq_ge_re_bus, 1, 0xCCDDEEFF,
				       hq_ge_re_bus, 1, 0x5a5a5a5a,
				    };

   msg_printf(DBG, "Start mgras_host_hq_cp_ge_re_dma\n");

   if (_mgras_host_hq_dl_dma(10, data, CP_PASS_THROUGH_GE_FIFO)) {
	errors++;
   }

   /* Use GE Pass througp to send DMA data back to Host */

   POLL_GE_READ_FLAG;
   HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
   if (actual != 0x8899aabb) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test0 failed. expected = 0x%x, actual = 0x%x\n",0x8899aabb, actual);
   }
   HQ3_PIO_RD((RSS_BASE + RSS_XFRCOUNTERS), ~0x0, actual);
   /* fprintf(2,"rss read back is 0x%x\n",actual); */
   if (actual != 0x8899aabb) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test1 failed. expected = 0x%x, actual = 0x%x\n",0x8899aabb, actual);
   }

   POLL_GE_READ_FLAG;
   HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
   if (actual != 0xCCDDEEFF) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test2 failed. expected = 0x%x, actual = 0x%x\n",0xCCDDEEFF, actual);
   }
   HQ3_PIO_RD((RSS_BASE + RSS_XFRCOUNTERS), ~0x0, actual);
   if (actual != 0xCCDDEEFF) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test3 failed. expected = 0x%x, actual = 0x%x\n",0xCCDDEEFF, actual);
   }

   POLL_GE_READ_FLAG;
   HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
   if (actual != 0x5a5a5a5a) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test4 failed. expected = 0x%x, actual = 0x%x\n",0x5a5a5a5a, actual);
   }
   HQ3_PIO_RD((RSS_BASE + RSS_XFRCOUNTERS), ~0x0, actual);
   if (actual != 0x5a5a5a5a) { 
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test5 failed. expected = 0x%x, actual = 0x%x\n",0x5a5a5a5a, actual);
   }
   msg_printf(DBG, "End mgras_host_hq_cp_ge_re_dma");

   return(errors);
}
#endif /* MFG_USED */
#endif /* ifndef IP28 */

#ifndef IP28
/*
 * Host <-> HQ3/CP-> GE-> RE - DMA test
 */
/***************************************************************************
*   Routine: mgras_host_hq_cp_ge_re_dma_data 
*
*   Purpose: Host -> HQ3/CP -> GE -> Host DMA path test   
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Creates a display list that sets up one of the HQ3 registers
*	   DMA the display list to GE via HQCP 
*	   Read back (using PIO) the value of the register and verify the DMA operation
*
*	   Not working yet, can't verify data written into CHAR_H and CHAR_L in re  
***************************************************************************/
#ifdef MFG_USED
__uint32_t
mgras_host_hq_cp_ge_re_dma_data(void)
{
   __uint32_t		errors = 0;
   __uint32_t		current_status, actual;
   __uint32_t		data[5] = { 5, hq_ge_re_data, 1, 0x8899aabb, 0x5a5a5a5a,
				    };

   msg_printf(DBG, "Start mgras_host_hq_cp_ge_re_dma\n");

   msg_printf(DBG, "Start dma\n");


   if (_mgras_host_hq_dl_dma(5, data, CP_PASS_THROUGH_GE_FIFO)) {
	errors++;
   }
   msg_printf(DBG, "Finish dma\n");

   /* Use GE Pass througp to send DMA data back to Host */

   POLL_GE_READ_FLAG;
   HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
   if (actual != 0xcaffe) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test0 failed. expected = 0x%x, actual = 0x%x\n",0x8899aabb, actual);
   }
   msg_printf(DBG,"Read from RSS\n");
/*
   WRITE_RSS_REG(0, RE_TOGGLECNTX, ~0x0,~0x0);
   WRITE_RSS_REG(0, CHAR_H, 0xdeadbeef, ~0x0);
*/
   READ_RSS_REG(0,CHAR_H, ~0x0,~0x0);
   /* fprintf(stderr,"rss read back is 0x%x\n",actual); */
   if (actual != 0x8899aabb) {
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test1 failed. expected = 0x%x, actual = 0x%x\n",0x8899aabb, actual);
   }

   READ_RSS_REG(0,CHAR_L, ~0x0,~0x0);
   if (actual != 0x5a5a5a5a) { 
	errors++;
	msg_printf(DBG,"mgras_host_hq_cp_ge_dma test5 failed. expected = 0x%x, actual = 0x%x\n",0x5a5a5a5a, actual);
   }
   msg_printf(DBG, "End mgras_host_hq_cp_ge_re_dma_data");

   return(errors);
}
#endif /* MFG_USED */
#endif /* ifndef IP28 */

/***************************************************************************
*   Routine: mgras_dfifo
*
*   Purpose: HQ3 dfifo test 
*
*   History: 2/06/95 : Original Version.
*
*   Notes: Not working yet 
*
***************************************************************************/
__uint32_t
mgras_dfifo_test(void)
{
	__uint32_t actual, config, command;
	__uint32_t i, errors = 0;
	RSS_DMA_Params       *rssDmaPar = &mgras_RssDmaParams;

        _mgras_dma_re_setup(rssDmaPar, DMA_PIO, DMA_WRITE, 0);
#if KEIMING_KNOWS_THIS
	if (_mgras_host_hq_dl_dma(11, data1, CP_DELAY10)) {
		errors++;
	   }
#endif
        msg_printf(DBG, "Finish dma\n");

        msg_printf(VRB,"DFIFO Test\n");

	/* Turn off all interrupts */
	HQ3_PIO_WR(INTR_ENAB_CLR_ADDR, ~0x0, 0xffffffff);
	/* Set FIFO_DELAY to minimum delay */
        HQ3_PIO_WR(DFIFO_DELAY_ADDR, 0, 0xffffffff);
       
	/* Check if DFIFO is empty or not? */
	do {
        	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
	} while ( ((actual >> 15) & 0x7f) != 0);



	/* Initialize with 0x4 and  0x18 */

	HQ3_PIO_WR(DFIFO_HW_ADDR, 0x10, 0xffffffff);
	HQ3_PIO_WR(DFIFO_LW_ADDR, 0x2, 0xffffffff);

        /* Unstall HQ CP first */
        HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
        config &= (~HQ3_UCODE_STALL);
        HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK); 
	
	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"fifo_status = %x\n",actual); */

        /* Send command tokens but not exceed low water mark */
        _mgras_dma_re_setup(rssDmaPar, DMA_PIO, DMA_WRITE, 0);
#if KEIMING_KNOWS_THIS
	if (_mgras_host_hq_dl_dma(11, data1, CP_DELAY10)) {
		errors++;
	   }
#endif
	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"status = %x\n",actual); */
	if ((actual  & DFIFO_LW_BIT) != DFIFO_LW_BIT  ) {
	       msg_printf(VRB,"FIFO_STATUS Low Water not set\n");
	       return(-1);
	}
	if ((actual  & DFIFO_HW_BIT) != 0 ) {
	       msg_printf(VRB,"FIFO_STATUS High Water not clear\n");
	       return(-1);
	}

	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"fifo_status = %x\n",actual); */

        /* Send more commands so that fifo level will be in between
	   Low Water mark and High water mark*/

        for (i =0; i < 5; i++) {
           _mgras_dma_re_setup(rssDmaPar, DMA_PIO, DMA_WRITE, 0);
#if KEIMING_KNOWS_THIS
	   if (_mgras_host_hq_dl_dma(11, data1, CP_DELAY10)) {
		errors++;
	   }
#endif
        }

	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"status = %x\n",actual); */
	if ((actual  & DFIFO_LW_BIT) != 0  ) {
	       msg_printf(VRB,"FIFO_STATUS Low Water not clear\n");
	       return(-1);
	}
	if ((actual  & DFIFO_HW_BIT) != 0 ) {
	       msg_printf(VRB,"FIFO_STATUS High Water not clear\n");
	       return(-1);
	}

	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"fifo_status = %x\n",actual); */

	/* Fifo level above high water mark */
        for (i =0; i < 20; i++) {
#if KEIMING_KNOWS_THIS
           command = CP_DELAY10 << 8 | 4*11;
#endif
	   HQ3_PIO_WR(CFIFO1_ADDR, command, HQ3_CFIFO_MASK);
	   HQ3_PIO_WR(CFIFO1_ADDR, 0x7c0, HQ3_CFIFO_MASK);
        }

	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"status = %x\n",actual); */
	if ((actual & DFIFO_LW_BIT) != 0 ) {
	       msg_printf(VRB,"FIFO_STATUS Low Water not clear\n");
	       return(-1);
	}
        /* fprintf(stderr,"status = %x\n",actual); */
	if ((actual  & DFIFO_HW_BIT) != DFIFO_HW_BIT  ) {
	       msg_printf(VRB,"FIFO_STATUS High Water not set\n");
	       return(-1);
	}
	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
        /* fprintf(stderr,"fifo_status = %x\n",actual); */

        msg_printf(VRB,"end of DFIFO Test\n");

	return(errors);
}
