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

#include "libsk.h"
#include "sys/types.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "iset.diag"
#include "ide_msg.h"
#include "mgras_hq3.h"
#include "mgras_hq3_cp.h"
#include "mgras_dma.h"
#include "cp_ge_sim_tokens.h"
#include "ge11symtab.h"

/* HQ3 CP test */

/*
{"cp_branch3_1",ISET_BRANCH3_1,32,branch3_1_matrix},
{"cp_branch3_2",ISET_BRANCH3_2,32,branch3_2_matrix},
{"cp_branch3_3",ISET_BRANCH3_3,32,branch3_3_matrix},
{"cp_branch3_4",ISET_BRANCH3_4,32,branch3_4_matrix},
{"cp_branch3_5",ISET_BRANCH3_5,32,branch3_5_matrix},
{"cp_branch3_6",ISET_BRANCH3_6,32,branch3_6_matrix},
{"cp_branch3_6",ISET_BRANCH3_7,32,branch3_7_matrix},
{"cp_mix1",ISET_MIX1,32,mix1_matrix},
{"cp_mix2",ISET_MIX2,32,mix2_matrix},
{"cp_imove2",ISET_IMOVE2,32,imove2_matrix},
{"cp_dmove12_1",ISET_DMOVE12_1,32,dmove12_1_matrix},
{"cp_dmove12_3",ISET_DMOVE12_3,32,dmove12_3_matrix},
{"cp_dmove12_4",ISET_DMOVE12_4,32,dmove12_4_matrix},
*/
test_matrix  dmove12_1_matrix[] = {
{3,0xfffffffe,0xfffffffe,1},{5,0xfffffffd,0x3fd,1},
{7,0xfffffffb,0xfffffffb,1},{9,0xfffffff7,0xfffffff7,1},
{11,0xffffffef,0xffef,1},{13,0xffffffdf,0x1fdf,1},
{15,0xffffffbf,0x1fbf,1},{17,0xffffff7f,0xf7f,1},
{19,0xfffffeff,0x2ff,1},{21,0xfffffdff,0x1dff,1},
{23,0xfffffbff,0x1bff,1},{25,0xfffff7ff,0x17ff,1},
{27,0xffffefff,0x0fff,1},{29,0xffffdfff,0xffdfff,1},
{31,0xffffbf03,0x3,1},{33,0x100000,0x100000,1},
{34,0xfffffffa,0,0},{35,0xfffffff5,0,0},
{36,0xffffffeb,0,0},{38,0xffffffd7,0x1fd7,1},
{40,0xbaadcafe,0x1feb,1},{42,0xbaadcafe,0x1ff5,1},
{44,0xbaadcafe,0x1ffa,1},{46,0xbaadcafe,0,1},
{48,0x24,0x24,1},{50,0xaaaaaaaa,0xaaaaaa00,1},
{52,0x55555555,0x15555555,1},{54,0x55555555,0x55555500,1},
{56,0xaaaaaaaa,0x2aaaaaaa,1},{62,0,1,1},
{66,0,2,1},{70,0,3,1},
{74,0,4,1},{78,0,5,1},
{82,0,6,1},{86,0,7,1},
{90,0,8,1},{94,0,9,1},
{98,0,10,1},{102,0,11,1},
{106,0,12,1},{110,0,13,1},
{114,0,14,1},{118,0,15,1},
{122,0,16,1},{126,0,17,1},
{130,0,18,1},{134,0,19,1},
{138,0,20,1},{142,0,21,1},
{146,0,22,1},{150,0,23,1},
{154,0,24,1},{158,0,25,1},
{162,0,26,1},{166,0,27,1},
{170,0,28,1},{174,0,29,1},
{178,0,30,1},{182,0,31,1},
{186,0,32,1},
{187,0,32,1}
};

test_matrix  dmove12_2_matrix[] = {
{4,0,33,1},{8,0,34,1},
{12,0,35,1},{16,0,36,1},
{20,0,37,1},{24,0,38,1},
{28,0,39,1},{32,0,40,1},
{36,0,41,1},{40,0,42,1},
{44,0,43,1},{48,0,44,1},
{52,0,45,1},{56,0,46,1},
{60,0,47,1},{64,0,48,1},
{68,0,49,1},{72,0,50,1},
{76,0,51,1},{80,0,52,1},
{84,0,53,1},{88,0,54,1},
{92,0,55,1},{96,0,56,1},
{100,0,57,1},{104,0,58,1},
{108,0,59,1},{112,0,60,1},
{116,0,61,1},{120,0,62,1},
{124,0,63,1},{128,0,64,1},
{129,0,64,0},
};
test_matrix  dmove12_3_matrix[] = {
{4,0,65,1},{8,0,66,1},
{12,0,67,1},{16,0,68,1},
{20,0,69,1},{24,0,70,1},
{28,0,71,1},{32,0,72,1},
{36,0,73,1},{40,0,74,1},
{44,0,75,1},{48,0,76,1},
{52,0,77,1},{56,0,78,1},
{60,0,79,1},{64,0,80,1},
{68,0,81,1},{72,0,82,1},
{76,0,83,1},{80,0,84,1},
{84,0,85,1},{88,0,86,1},
{92,0,87,1},{96,0,88,1},
{100,0,89,1},{104,0,90,1},
{108,0,91,1},{112,0,92,1},
{116,0,93,1},{120,0,0xaaaa005e,1},
{124,0,0xbbbb005f,1},{128,0,0xcccc0060,1},
{129,0,0xcccc0060,1},
};
test_matrix  imove1_matrix[] = {
{3,0,3,1},{6,0,6,1},
{9,0,0xe,1},{12,0,0x18,1},
{15,0,0x30,1},{18,0,0x60,1},
{21,0,0xe0,1},
};

test_matrix  imove2_matrix[] = {
{4,0,0xf0f1,1},{7,0,0xffff,1},
{10,0,0xf0e,1},{13,0,0,1},
{15,0,0xf2f30000,1},{17,0,0xffff0000,1},
{19,0,0x0d0c0000,1},{21,0,0,1},
{23,0,0xf4f5,1},
{25,0,0xffff,1},{27,0,0xb0a,1},
{29,0,0,1},{32,0xf6f70000,1},
{35,0,0xffff0000,1},{38,0,0x9080000,1},
{41,0,0,1},
{44,0,0xf8f9,1},{47,0,0xffff,1},
{50,0,0x706,1},{53,0,0,1},
{56,0,0xf6f70000,1},{59,0,0xffff0000,1},
{62,0,0x9080000,1},{65,0,0,1},
{68,0,0xf8f9,1},{71,0,0xffff,1},
{74,0,0x706,1},{77,0,0,1},
{78,0,0,1},
};

test_matrix  mix1_matrix[] = {
{1,1,1,0},{6,0,0,1},
{8,0,1,1},{10,0,1,1},
{12,0,1,1},{15,0,1,1},
{17,0,0,1},{19,1,1,1},
{21,1,1,1},{23,0,0,1},
{25,0,0,1},{27,0,1,1},
{29,0,1,1},{32,0,1,1},
{34,0,1,1},{36,0,0,1},
{38,0,1,1},{41,0,0,1},
{43,0,1,1},{45,0,0,1},
{47,0,1,1},{50,0,1,1},
{52,0,0,1},{54,0,0,1},
{56,0,1,1},{60,0,0,1},
{62,0,0,1},{64,0,0,1},
{66,0,1,1},{70,0,1,1},
{72,0,1,1},{74,0,1,1},
{76,0,0,1},{80,0,0,1},
{82,0,1,1},{84,0,1,1},
{86,0,0,1},{90,0,1,1},
{92,0,0,1},{94,0,1,1},
{96,0,0,1},{101,0,0,1},
{103,0,0,1},{105,0,1,1},
{107,0,0,1},{112,0,1,1},
{114,0,1,1},{116,0,0,1},
{118,0,0,1},{123,0,0,1},
{125,0,1,1},{127,0,0,1},
{129,0,0,1},{134,0,1,1},
{136,0,0,1},{138,0,0,1},
{140,0,0,1},{145,0,0,1},
{147,0,0,1},{149,0,0,1},
{151,0,0,1},
{151,0,0,0},
};

test_matrix  mix2_matrix[] = {
{1,0x3fc,0,0},{4,0,0x3fd,1},
{7,0,0x3fe,1},{10,0,0x3ff,1},
{13,0,0,1},{16,0,1,1},
{19,0,2,1},{22,0,3,1},
{25,0,4,1},{28,0,0,1},
{29,0,0,0},
};


hq3_cp_test cp_tests[] = {
{"cp_dmove12_1",ISET_DMOVE12_1,0xf8,1,dmove12_1_matrix}, 
{"cp_dmove12_2",ISET_DMOVE12_2,0x80,0,dmove12_2_matrix},  
{"cp_dmove12_3",ISET_DMOVE12_3,0x80,0,dmove12_3_matrix},
{"cp_imove1",ISET_IMOVE1,28,0,imove1_matrix},
{"cp_imove2",ISET_IMOVE2,28*4,0,imove2_matrix},
{"cp_mix1",ISET_MIX1,61*4,0,mix1_matrix},
{"cp_mix2",ISET_MIX2,10*4,0,mix2_matrix}, 
};


/***************************************************************************
*   Routine: mgras_HQCP_com 
*
*   Purpose: Host to CP communication routine 
*            Send the test vectors to CP, read back the test
*            result(if applicable) and verify against a predefined result
*
*   History: 10/13/94 : Original Version.
*
*   Notes:
*
****************************************************************************/

__uint32_t
mgras_HQCP_com(hq3_cp_test cp_test)
{
   __uint32_t i, errors = 0; 
   __uint32_t  test_entry, current_pc, current_gpr, time_out, value, config;

   /* Unstall HQ CP first */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK); 

   msg_printf(VRB,"HQ_CP test - %s:\n",cp_test.name);

   /* Send the command token and bytecount first */

   value = cp_test.token << 8 | cp_test.bytecount;
   HQ3_PIO_WR(CFIFO1_ADDR,value, HQ3_CFIFO_MASK); 

   /* Figure out the test entry point int HQCP ucode */
   /* Offset from HQUC_BASE_ADDR is token*2*(each token jump table takes
      2 words and each word takes 4 bytes */

   value = HQUC_ADDR+2*4*(cp_test.token);

	flush_cache();

   HQ3_PIO_RD((HQUC_ADDR+2*4*(cp_test.token)), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */

   /* Then start the test loop */
   for (i = 0; i < (cp_test.bytecount/4) ; i++)
   {
       /* Send the data down to FIFO */
       value = cp_test.cp[i].data_out;
       msg_printf(DBG,"cp_test.cp[i].data_out = %x i = %x\n",value,i);
       if (cp_test.send_data_first == 1)
       HQ3_PIO_WR(CFIFO1_ADDR,value, HQ3_CFIFO_MASK);

      /* check if we hit the test point, which is the test_entry pointer plus
	   the offset */

      /* Need to add a time-out exit here to avoid infinitive looping*/

      POLL_HQCP_PC(cp_test.cp[i].pc_offset);
#if 0
      do {
         HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);
	 msg_printf(DBG, "polling for data\n");
      } while ((current_pc - test_entry) != cp_test.cp[i].pc_offset);
#endif
      /* check if data verification flag is set or not? */
      if (cp_test.cp[i].verif_flag == 1)
      {
	  /* flag is set, read data in CP GPR(which is CP_DATA) and verify it 
	     against known result */

	 msg_printf(DBG, "check data\n");
          HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
	  COMPARE("HQ3 CP test", CP_DATA_ADDR, cp_test.cp[i].expected, current_gpr, 0xffffffff, errors);
	  /*
	  if (cp_test.cp[i].expected != current_gpr)
		{
		msg_printf(VRB,"**** Data compare failed\n");
		msg_printf(DBG,"current_gpr = %x\nexpected_gpr = %x\n",
			current_gpr, cp_test.cp[i].expected);
		    errors++;
		}
	  */
       }
       if (cp_test.send_data_first == 0)
       HQ3_PIO_WR(CFIFO1_ADDR, value, HQ3_CFIFO_MASK);
   }      
   msg_printf(VRB,"HQ_CP test - %s: passed\n",cp_test.name);
   return errors;

}

/***************************************************************************
*   Routine: mgras_HQ_CP_test 
*
*   Purpose: Main program to invoke  CP tests 
*
*   History: 10/13/94 : Original Version.
*
*   Notes:
*
****************************************************************************/
/* Main program for CP tests*/
__uint32_t
mgras_HQ_CP_test(void)
{
   __uint32_t i, errors = 0;
   mgrasDownloadHQ3Ucode(mgbase, &iset_table[0], (sizeof(iset_table)/sizeof(iset_table[0])), 1);
   for (i = 0; i < sizeof(cp_tests)/sizeof(hq3_cp_test); i++) 
   {
      errors += mgras_HQCP_com(cp_tests[i]); 
   }
   REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CP_TEST]), errors);
}
/* mgras_HQCP_GE_Rebus_HQ_test */

/***************************************************************************
*   Routine: mgras_HQCP_GE_Rebus_HQ_test 
*
*   Purpose: Test the data path from HQCP -> GE -> REbus -> HQ 
*            Send test token and data to GE through CP_PASS_THROUGH_GE CP token
*
*   History: 10/26/94 : Original Version.
*
*   Notes: Send 32 data pattern, read back and verify 
*
****************************************************************************/

__uint32_t
mgras_HQCP_GE_Rebus_HQ_test(void)
{
   __uint32_t i, actual, errors = 0; 
   __uint32_t  current_status;
   
   msg_printf(DBG,"HQCP_GEbus_test\n");
 
  /* Send token */
   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (35*4) ) , HQ3_CFIFO_MASK);

   /* Then, send GE command token and data  */


   HQ3_PIO_WR(CFIFO1_ADDR, 34 , HQ3_CFIFO_MASK);
   HQ3_PIO_WR(CFIFO1_ADDR, hq_ge_bus , HQ3_CFIFO_MASK);
   
   /* Number of data for GE to process */
   HQ3_PIO_WR(CFIFO1_ADDR, 32 , HQ3_CFIFO_MASK);

   /* Send a walking 1 pattern */
   for (i = 0; i < 32; i++) {
      HQ3_PIO_WR(CFIFO1_ADDR, 1<<i, HQ3_CFIFO_MASK);
}

   /* Waiting for data coming back from Rebus or time-out and flag error */

   /* The sequence is like this:
      1. The GE_READ bit in FLAG register(readable from FLAG_SET) will 
	 be set when GE dump data into REbus  
      2. Host can pick up the data from REBus by reading data from 
	 REBUS_READ_H and REBUS_READ_L. When REBUS_READ_L is read, the GE_READ
	 will be set again if data is still pending from GE.
      3. Repeat Step 1 and Step 2 till data run out 
   */
      /* Wait till GE_READ flag is set */
msg_printf(DBG,"Waiting Read_flag to be set\n");
       for (i = 0; i < 32; i++)
       {

	 POLL_GE_READ_FLAG;

	 HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
msg_printf(DBG," REBUS_READ_L= %x\n",actual);
	}
	return errors;
}


/* mgras_HQCP_GE_Rebus_RE_test */

/***************************************************************************
*   Routine: mgras_HQCP_GE_Rebus_RE_test 
*
*   Purpose: Test the data path from HQCP -> GE -> REbus -> RE 
*            Send test token to GE through CP_PASS_THROUGH_GE CP token
*
*   History: 12/02/94 : Original Version.
*
*   Notes: Send walking bits pattern, read back and verify 
*
****************************************************************************/
#ifdef MFG_USED
__uint32_t
mgras_HQCP_GE_Rebus_RE_test(void)
{
   __uint32_t i, actual, errors = 0; 
   __uint32_t  current_status;
   
   msg_printf(DBG,"HQCP_GE_Rebus_RE_test\n");

#if 0
   /* Do the Globalal initialization on GE first */
   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (5*4 )) , HQ3_CFIFO_MASK);

   HQ3_PIO_WR(CFIFO1_ADDR, 4 , HQ3_CFIFO_MASK);
   HQ3_PIO_WR(CFIFO1_ADDR, mge_GlobalInitialize , HQ3_CFIFO_MASK);
   HQ3_PIO_WR(CFIFO1_ADDR, 2 , HQ3_CFIFO_MASK);
   HQ3_PIO_WR(CFIFO1_ADDR, 3 , HQ3_CFIFO_MASK);
   HQ3_PIO_WR(CFIFO1_ADDR, 4 , HQ3_CFIFO_MASK);
#endif
   for (i=0; i < 32; i++)
   {

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*4 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 3 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, hq_ge_re_bus , HQ3_CFIFO_MASK);
   

	/* Number of data for GE to process */
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x1 , HQ3_CFIFO_MASK);

	/* Walking bits */
    	HQ3_PIO_WR(CFIFO1_ADDR, 0x1<<i, HQ3_CFIFO_MASK);


   /* Waiting for data coming back from Rebus or time-out and flag error */
	
	 POLL_GE_READ_FLAG;
	 HQ3_PIO_RD(REBUS_READL_ADDR, HQ3_CPDATA_MASK, actual);
        /* COMPARE("HQCP->GE->Rebus->HQ test", 0, actual, expected,~0); */
	 if (actual != (0x1 << i)) 
	 {
		errors++;
		msg_printf(VRB,"HQ3 read back failed\n",actual);
	 }
	 /* Read back XFRCOUNTERS and verify */
	 HQ3_PIO_RD((RSS_BASE + RSS_XFRCOUNTERS), ~0x0, actual); 
        /* COMPARE("HQCP->GE->Rebus->HQ test", 0, actual, expected,~0); */
	 if (actual != (1 << i)) 
	 {
		errors++;
		msg_printf(VRB,"RE4 read back failed\n",actual);
	 }

     }
     return errors;
 
}
#endif /* MFG_USED */

#if HQ4
#define CFIFO_HIGH_WATER_BIT_SHIFT 24
#else
#define CFIFO_HIGH_WATER_BIT_SHIFT 22
#endif

/***************************************************************************
*   Routine: mgras_cfifo
*
*   Purpose: HQ3 cfifo test 
*
*   History: 2/15/95 : Original Version.
*
*   Notes: By combinating setting watermark levels and pumping data into
*	   cfifo, we check if high/low watermarks are set/reset appropriate
*	   or not.
*
***************************************************************************/
__uint32_t
mgras_cfifo_test(void)
{
	__uint32_t errors = 0;
	__uint32_t actual, expected, config, interrupt_config;
	__uint32_t i, flag_read;
	__uint32_t test_entry, current_gpr, current_pc;
	__uint32_t j;

        msg_printf(DBG,"CFIFO Test\n");

	mgbase->cfifo_hw          = 40;


	/* Download the Ucode first */
	mgrasDownloadHQ3Ucode(mgbase, &iset_table[0], (sizeof(iset_table)/sizeof(iset_table[0])), 1);

        HQ3_PIO_RD((HQUC_ADDR+2*4*(FIFO_DRAIN_ONE)), HQ3_UCODE_MASK, test_entry);
        test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */
        /* Then unstall HQ3 cp ucode */

        HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
        config &= (~HQ3_UCODE_STALL);
        HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

	/* Save the original interrupt settings */
	HQ3_PIO_RD(INTR_ENAB_SET_ADDR, ~0x0, interrupt_config);
        msg_printf(DBG,"INTR_ENAB_SET reading is 0x%x\n",interrupt_config);

	/* Turn off all interrupts */
	HQ3_PIO_WR(INTR_ENAB_CLR_ADDR, 0x0, 0xffffffff);

	/* Set FIFO_DELAY to minimum delay */
        HQ3_PIO_WR(CFIFO_DELAY_ADDR, 0, 0xffffffff);
       
	/* Check if FIFO is empty or not? */
	do {
        	HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
	} while ( (actual & 0x7f) != 0);

	/* Set high and low water marks */
	for ( i =0; i < 7; i++) {
	   HQ3_PIO_WR(CFIFO_HW_ADDR, 1 << i, 0xffffffff);
	   /* Read CFIFO High Water Mark from FIFO_STATUS */
	   HQ3_PIO_RD(FIFO_STATUS_ADDR, ~0x0, actual);
	   if (((actual >> CFIFO_HIGH_WATER_BIT_SHIFT) & 0x7f) != (1 << i)  ) {
	       msg_printf(ERR,"FIFO_STATUS High Water read back failed\n",(actual >> 24) & 0x7f);
	       errors++;
	   }
	}
	/* Initialize with 40 and 10 */

	HQ3_PIO_WR(CFIFO_HW_ADDR, 0x28, 0xffffffff);
	HQ3_PIO_WR(CFIFO_LW_ADDR, 0x0a, 0xffffffff);

	


        /* Send command token and data not exceed low water mark */

	HQ3_PIO_WR(CFIFO1_ADDR,((FIFO_DRAIN_ONE << 8) | 36) , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, 8, HQ3_CFIFO_MASK);

        for (i =0; i < 8; i++) {
	   HQ3_PIO_WR(CFIFO1_ADDR, i, HQ3_CFIFO_MASK);
        }


	/* Check water mark status */

	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
#ifdef DEBUG
        msg_printf(DBG,"status in level 1 = %x\n",actual);
#endif
	if ((actual  & CFIFO_LW_BIT) != CFIFO_LW_BIT  ) {
	       msg_printf(ERR,"FIFO_STATUS1 Low Water Mark not set\n");
	       errors++;
	}
	if ((actual  & CFIFO_HW_BIT) != 0 ) {
	       msg_printf(ERR,"FIFO_STATUS1 High Water Mark not clear\n");
	       errors++;
	}

	/* Clear bit 10-11 of FLAGS reg.( CP_FLAAG[1:0]) */

	HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, 0xc00, HQ3_FLAGS_MASK);


	/* First wait until CP_FLAG0 cleared up by CP ucode */	
	do {
	    HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
            HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);
	} while ((flag_read & CP_FLAG0) == CP_FLAG0);



	/* Now start to send signal to drain fifo one at a time */
	for (i = 0; i < 8; i++)
	{

	   /* Set HOST_CP_BIT0 Flag */

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	   HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, ((flag_read | HOST_CP_BIT0) & ~ CP_FLAG0) , HQ3_CFIFO_MASK);

	   /* wait utill CP_FLAG0 is set */

	   /* Read the data back */
	   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
	   j = 1;
	   do {
	       HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	       j++;
	      } while (((flag_read & CP_FLAG0) != CP_FLAG0) && j < 300 );
	   if (j == 300) 
	   {
		msg_printf(ERR,"CFIFO_TEST1 timeout\nflag_read = 0x%x\n",
			flag_read);
		errors++;
	   }

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);

	   /* Clear CP_FLAG0  and HOST_CP_BIT0 */
	   HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR,  (CP_FLAG0 | HOST_CP_BIT0) , HQ3_CFIFO_MASK);

	}	
        /* Send command with N (10<N<40) so that fifo level will be in between
	   Low Water mark and High water mark*/

	HQ3_PIO_WR(CFIFO1_ADDR,((FIFO_DRAIN_ONE << 8) | 4*(32+1)) , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, 32, HQ3_CFIFO_MASK);

        for (i =0; i < 32; i++) {
	   HQ3_PIO_WR(CFIFO1_ADDR, 1 << i, HQ3_CFIFO_MASK);
        }
	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	/* Check water mark status */
	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
#ifdef DEBUG
        msg_printf(DBG,"status in level 2 = %x\n",actual);
#endif
	if ((actual  & CFIFO_LW_BIT) != 0  ) {
	       msg_printf(ERR,"FIFO_STATUS2 Low Water Mark not clear\n");
	       errors++;
	}
	if ((actual  & CFIFO_HW_BIT) != 0 ) {
	       msg_printf(ERR,"FIFO_STATUS2 High Water Mark not clear\n");
	       errors++;
	}

	/* Now start to send signal to drain fifo one at a time */
	for (i = 0; i < 32; i++)
	{
	   /* Set HOST_CP_BIT0 Flag */

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	   HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, ((flag_read | HOST_CP_BIT0) & ~ CP_FLAG0) , HQ3_CFIFO_MASK);
	   /* wait utill CP_FLAG0 is set */

	   /* Read the data back */
	   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
	   COMPARE("HQ3 CFIFO_test", CP_DATA_ADDR,1 << i , current_gpr, 0xffffffff, errors);
	   HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);
	   j = 1;
	   do {
	       HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	       j++;
	      } while (((flag_read & CP_FLAG0) != CP_FLAG0) && j < 300 );
	   if (j == 300) 
	   {
		msg_printf(ERR,"HQ3 CFIFO_Test2 timeout\nflag_read =0x%x\n",
			flag_read);
		errors++;
	   }

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);

	   /* Clear CP_FLAG0  and HOST_CP_BIT0 */
	   HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR,  (CP_FLAG0 | HOST_CP_BIT0) , HQ3_CFIFO_MASK);

	}	

#define	CFIFO_HIGH_WATER_MARK	50

        /* Send command with N(N>40) so that fifo level will be in 
	   High water mark area */

	HQ3_PIO_WR(CFIFO1_ADDR,((FIFO_DRAIN_ONE << 8) | 4*(CFIFO_HIGH_WATER_MARK+1)) , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, CFIFO_HIGH_WATER_MARK, HQ3_CFIFO_MASK);

        for (i =0, expected = 0xfffffffe; i < CFIFO_HIGH_WATER_MARK; i++, expected = ((expected <<=1) | 1)) {
	   HQ3_PIO_WR(CFIFO1_ADDR, expected, HQ3_CFIFO_MASK);
        }
	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);

	/* Check water mark status */

	HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
#ifdef DEBUG
        msg_printf(DBG,"status in level 3 = %x\n",actual);
#endif
	if ((actual  & CFIFO_LW_BIT) != 0  ) {
	       msg_printf(ERR,"FIFO_STATUS3 Low Water Mark not clear\n");
	       errors++;
	}
	if ((actual  & CFIFO_HW_BIT) != CFIFO_HW_BIT ) {
	       msg_printf(ERR,"FIFO_STATUS3 High Water Mark not set\n");
	       errors++;
	}

	/* Now start to send signal to drain fifo one at a time */
	for (i = 0, expected = 0xfffffffe; i < CFIFO_HIGH_WATER_MARK; i++, expected = ((expected <<=1) | 1))
	{
	   /* Set HOST_CP_BIT0 Flag */

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	   HQ3_PIO_WR(FLAG_SET_PRIV_ADDR, ((flag_read | HOST_CP_BIT0) & ~ CP_FLAG0) , HQ3_CFIFO_MASK);

	   /* Read the data back */

	   HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
	   COMPARE("HQ3 CFIFO_test", CP_DATA_ADDR, expected, current_gpr, 0xffffffff, errors);
	   HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);
	   j = 1;
	   do {
	       HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);
	       j++;
	      } while (((flag_read & CP_FLAG0) != CP_FLAG0) && j < 300 );
	   if (j == 300) 
	   {
		msg_printf(ERR,"HQ3 CFIFO Test3 timeout\nflag_read =0x%x\n",
			flag_read);
		errors++;
	   }

	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_CONFIG_MASK, flag_read);

	   /* Clear CP_FLAG0  and HOST_CP_BIT0 */
	   HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR,  (CP_FLAG0 | HOST_CP_BIT0) , HQ3_CFIFO_MASK);

	}	
	/* Restore original interrupt settings */
	HQ3_PIO_WR(INTR_ENAB_SET_ADDR, interrupt_config, 0xffffffff);

#if HQ4
	mgbase->cfifo_hw          = 80;
#endif

	REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CFIFO_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_conv_test32
*
*   Purpose: HQ3 32-bit conversion test 
*
*   History: 2/15/95 : Original Version.
*
*   Notes: Issue all 32-bit conversion commands with predefined data to CP
*	   through CFIFO. and verify the result through CP and PIO acess.
*	   
*
***************************************************************************/
__uint32_t
mgras_conv_test32(void) 
{
   __uint32_t i, errors = 0 ;
   __uint32_t current_gpr, current_pc, test_entry, config;
   __uint32_t time_out ;
   struct hq3_conv_32bits {
	   __uint32_t cmd;
	   __uint32_t in_patterns[4];
	   __uint32_t out_patterns[4];
	   };
   struct hq3_conv_32bits patterns[] = {
	0x28,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0x3f7fffff,0x3b810182,0x3f7ffefd,
	0x38,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0xb0000000,0x3c010182,0xb8010200,
	0x20,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0x4f7fffff,0x4b810182,0x4f7ffefd,
	0x30,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0xbf800000,0x4b810182,0xc7810200,
	0x34,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0xbb800000,0x3f800000,0xc3810200,
	0x3c,0,0xffffffff,0x01020304,0xfffefdfc,0x0,0xffffffff,0x01020304,0xfffefdfc,
   };

   msg_printf(DBG,"HQ3 32-bit conversion test\n");

   /* Stall HQ3 cp ucode First to get the entry point */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config |= HQ3_UCODE_STALL;
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

   HQ3_PIO_RD((HQUC_ADDR+2*4*(CP_CONV_CHECK32)), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */

   /* Then unstall HQ3 cp ucode */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);


   for(i = 0; i < sizeof(patterns)/sizeof(struct hq3_conv_32bits); i++) {
       	HQ3_PIO_WR(CFIFO1_ADDR,(patterns[i].cmd <<23) | CP_CONV_CHECK32 << 8 | 0x10, HQ3_CFIFO_MASK);

   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns[0], HQ3_CFIFO_MASK);

	/* Polling HQCP PC to reach test point, will return error if time-out */

	POLL_HQCP_PC(2);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
        msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[0]) {
		errors++;
		msg_printf(VRB,"HQ3 32-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[0],current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns[1] , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(3);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
        msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[1]) {
		errors++;
		msg_printf(ERR,"HQ3 32-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[1],current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns[2] , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(4);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
   	msg_printf(DBG,"Readback 3 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[2]) {
		errors++;
		msg_printf(ERR,"HQ3 32-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[2],current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns[3] , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(5);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
   	msg_printf(DBG,"Readback 4 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[3]) {
		errors++;
		msg_printf(ERR,"HQ3 32-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[3],current_gpr);
	}
   }
   REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CONV_32BITS_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_conv_test8
*
*   Purpose: HQ3 8-bit conversion test 
*
*   History: 2/15/95 : Original Version.
*
*   Notes: Issue all 8-bit conversion commands with predefined data to CP
*	   through CFIFO. and verify the result through CP and PIO acess.
*	   
*
***************************************************************************/
__uint32_t
mgras_conv_test8(void) 
{
   __uint32_t i, time_out, errors =0;
   __uint32_t current_gpr, current_pc, test_entry, config;
   struct hq3_conv_8bits {
	   __uint32_t cmd;
	   __uint32_t in_patterns;
	   __uint32_t out_patterns[4];
	   };
   struct hq3_conv_8bits patterns[] = {				     /* Conv CMD   Type     Range             Scale      */
	0x29,0x01020304,0x3b808080,0x3c008080,0x3c40c000,0x3c808080, /*  101001     ub   0 to 2**8-1          1/(2**8-1) */ 
	0x29,0xfffefdfc,0x3f7fffff,0x3f7efefe,0x3f7dfd00,0x3f7cfcfc,
	0x39,0xfffefdfc,0xbc010204,0xbc810204,0xbcc18300,0xbd010204, /*  111001      b   -2**7 to 2**7-1      1/(2**7-1) */
	0x39,0x01020304,0x3c010204,0x3c810204,0x3cc18300,0x3d010204,
	0x21,0x01020304,0x3f800000,0x40000000,0x40400000,0x40800000, /*  100001     ub   0 to 2**8-1          1          */
	0x21,0xfffefdfc,0x437f0000,0x437e0000,0x437d0000,0x437c0000, 
	0x31,0x01020304,0x3f800000,0x40000000,0x40400000,0x40800000, /*  110001      b   -2**7 to 2**7-1      1          */
	0x31,0xfffefdfc,0xbf800000,0xc0000000,0xc0400000,0xc0800000,
   };

   msg_printf(DBG,"HQ3 8-bit conversion test\n");

   /* Stall HQ3 cp ucode First to get the entry point */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config |= HQ3_UCODE_STALL;
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

   HQ3_PIO_RD((HQUC_ADDR+2*4*(CP_CONV_CHECK8)), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */

   /* Then unstall HQ3 cp ucode */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);



   for (i = 0; i < sizeof(patterns)/sizeof(struct hq3_conv_8bits); i++) {

        /* First drop the data */
       	HQ3_PIO_WR(CFIFO1_ADDR,(patterns[i].cmd<<23) | CP_CONV_WR8 << 8 | 0x4, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns , HQ3_CFIFO_MASK);

	/* Read back and verify */
       	HQ3_PIO_WR(CFIFO1_ADDR,CP_CONV_CHECK8 << 8 | 0xc, HQ3_CFIFO_MASK);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
	if (current_gpr != patterns[i].out_patterns[0]) {
		errors++;
		msg_printf(ERR,"HQ3 8-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[0],current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	/* Polling HQCP PC to reach test point, will return error if time-out */
	POLL_HQCP_PC(3);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
	if (current_gpr != patterns[i].out_patterns[1]) {
		errors++;
		msg_printf(ERR,"HQ3 8-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[1],current_gpr);
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(5);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
   	msg_printf(DBG,"Readback 3 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[2]) {
		errors++;
		msg_printf(ERR,"HQ3 8-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[2],current_gpr);
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(7);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 4 = 0x%x\n",current_gpr);
	if (current_gpr != patterns[i].out_patterns[3]) {
		errors++;
		msg_printf(ERR,"HQ3 8-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[3],current_gpr);
	}
   }
   REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CONV_8BITS_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_conv_test16
*
*   Purpose: HQ3 16-bit conversion test 
*
*   History: 2/15/95 : Original Version.
*
*   Notes: Issue all 16-bit conversion commands with predefined data to CP
*	   through CFIFO. and verify the result through CP and PIO acess.
*	   
*
***************************************************************************/
__uint32_t
mgras_conv_test16(void) 
{
   __uint32_t i, errors =0;
   __uint32_t current_gpr, current_pc, test_entry, config, time_out;
   struct hq3_conv_16bits {
	   __uint32_t cmd;
	   __uint32_t in_patterns;
	   __uint32_t out_patterns[2];
	   };
   struct hq3_conv_16bits patterns[] = {			
	0x2a,0x01020304,0x3b810081,0x3c4100c1, 
	0x2a,0xfffefdfc,0x3f7ffeff,0x3f7dfcfd, 
	0x3a,0x01020304,0x3c010102,0x3cc10182, 
	0x3a,0xfffefdfc,0xb8800100,0xbc810102,
	0x22,0x01020304,0x43810000,0x44410000,
	0x22,0xfffefdfc,0x477ffe00,0x477dfc00,
	0x32,0x01020304,0x43810000,0x44410000,
	0x32,0xfffefdfc,0xc0000000,0xc4010000,
	0x36,0x01020304,0x3f800000,0x3f800000,
	0x36,0xfffefdfc,0xbc000000,0xc0010000,

   };

   msg_printf(DBG,"HQ3 16-bit conversion test\n");

   /* Stall HQ3 cp ucode First to get the entry point */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config |= HQ3_UCODE_STALL;
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

   HQ3_PIO_RD((HQUC_ADDR+2*4*(CP_CONV_CHECK16)), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */

   /* Then unstall HQ3 cp ucode */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

   for (i = 0; i < sizeof(patterns)/sizeof(struct hq3_conv_16bits); i++) {

        /* First drop the data */
       	HQ3_PIO_WR(CFIFO1_ADDR,(patterns[i].cmd<<23) | CP_CONV_WR16 << 8 | 0x4, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, patterns[i].in_patterns , HQ3_CFIFO_MASK);

	/* Read back and verify */
       	HQ3_PIO_WR(CFIFO1_ADDR,CP_CONV_CHECK16 << 8 | 0x4, HQ3_CFIFO_MASK);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
#ifdef DEBUG
   	msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
#endif
	if (current_gpr != patterns[i].out_patterns[0]) {
		errors++;
		msg_printf(ERR,"HQ3 16-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[0],current_gpr);
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);
	
	/* Polling HQCP PC to reach test point or time-out exit */

	POLL_HQCP_PC(3);


   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
	if (current_gpr != patterns[i].out_patterns[1]) {
		errors++;
		msg_printf(ERR,"HQ3 16-bit conversion failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		patterns[i].cmd,patterns[i].out_patterns[1],current_gpr);
	}

   }
   REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CONV_16BITS_TEST]), errors);
}
/***************************************************************************
*   Routine: mgras_stuff_test
*
*   Purpose: HQ3 stuff functionality test 
*
*   History: 2/15/95 : Original Version.
*
*   Notes: Test Vertex2 convert into vertex4, Vertex3 convert into vertex4,
*	   and TexCord1  convert into TexCord4 commands with data 
*	   through CFIFO. and verify the result through CP and PIO acess.
*	   
*
***************************************************************************/
__uint32_t
mgras_stuff_test(void) 
{
   __uint32_t errors =0;
   __uint32_t current_gpr, current_pc, test_entry, config, time_out;
 
   msg_printf(DBG,"HQ3 Converter Stuff test\n");

   /* Stall HQ3 cp ucode First to get the entry point */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config |= HQ3_UCODE_STALL;
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);

   /* Use the same token as b-bit converter */
   HQ3_PIO_RD((HQUC_ADDR+2*4*(CP_CONV_CHECK8)), HQ3_UCODE_MASK, test_entry);
   test_entry = test_entry  & 0x1fff ;  /* only keep the branch address part */

   /* Then unstall HQ3 cp ucode */

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, config);
   config &= (~HQ3_UCODE_STALL);
   HQ3_PIO_WR(HQ3_CONFIG_ADDR,config, HQ3_CONFIG_MASK);


  /* Vertex2 convert into vertex4 */

        /* First drop the data */
   	msg_printf(DBG,"Readback cmd = 0x%x\n",0x40000000 | CP_CONV_WR8 << 8 | 0x4*2);
       	HQ3_PIO_WR(CFIFO1_ADDR,0x40000000 | CP_CONV_WR8 << 8 | 0x4*2, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x12345678, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0xfedcba98, HQ3_CFIFO_MASK);

	/* Read back and verify */
       	HQ3_PIO_WR(CFIFO1_ADDR,CP_CONV_CHECK8 << 8 | 0xc, HQ3_CFIFO_MASK);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
	if (current_gpr != 0x12345678) {
		errors++;
		msg_printf(DBG,"HQ3 Stuff test Vertex2 failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x10,0x12345678,current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	/* Polling HQCP PC to reach test point or time-out exit */

	POLL_HQCP_PC(3);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
	if (current_gpr != 0xfedcba98) {
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x10,0xfedcba98,current_gpr);
		errors++;
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(5);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 3 = 0x%x\n",current_gpr);
	if (current_gpr != 0) {
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x10,0,current_gpr);
		errors++;
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(7);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 4 = 0x%x\n",current_gpr);
	if (current_gpr != 0x3f800000) {
		errors++;
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x10,0x3f800000,current_gpr);
	}

  /* Vertex3 convert into vertex4 */

        /* First drop the data */
   	msg_printf(DBG,"Readback cmd = 0x%x\n",0x20000000 | CP_CONV_WR8 << 8 | 0x4*2);
       	HQ3_PIO_WR(CFIFO1_ADDR,0x20000000 | CP_CONV_WR8 << 8 | 0x4*3, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x12345678, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0xfedcba98, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x12345678, HQ3_CFIFO_MASK);

	/* Read back and verify */
       	HQ3_PIO_WR(CFIFO1_ADDR,CP_CONV_CHECK8 << 8 | 0xc, HQ3_CFIFO_MASK);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
	if (current_gpr != 0x12345678) {
		errors++;
		msg_printf(ERR,"HQ3 Stuff - Vertex3 test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x01,0x12345678,current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(3);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
	if (current_gpr != 0xfedcba98) {
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x01,0xfedcba98,current_gpr);
		errors++;
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(5);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 3 = 0x%x\n",current_gpr);
	if (current_gpr != 0x12345600) {
		msg_printf(ERR,"HQ3 Stuff test - Vertex3 failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x01,0x12345600,current_gpr);
		errors++;
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(7);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 4 = 0x%x\n",current_gpr);
	if (current_gpr != 0x3f800000) {
		errors++;
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x01,0x3f800000,current_gpr);
	}
  /* TexCord1  convert into TexCord4 */

        /* First drop the data */
   	msg_printf(DBG,"Readback cmd = 0x%x\n",0x60000000 | CP_CONV_WR8 << 8 | 0x4*2);
       	HQ3_PIO_WR(CFIFO1_ADDR,0x60000000 | CP_CONV_WR8 << 8 | 0x4, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x12345678, HQ3_CFIFO_MASK);

	/* Read back and verify */
       	HQ3_PIO_WR(CFIFO1_ADDR,CP_CONV_CHECK8 << 8 | 0xc, HQ3_CFIFO_MASK);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 1 = 0x%x\n",current_gpr);
	if (current_gpr != 0x12345678) {
		errors++;
		msg_printf(ERR,"HQ3 Stuff test- TexCord1 failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x11,0x12345678,current_gpr);
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(3);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 2 = 0x%x\n",current_gpr);
	if (current_gpr != 0x0) {
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x11,0x0,current_gpr);
		errors++;
	}

   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(5);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 3 = 0x%x\n",current_gpr);
	if (current_gpr != 0x0) {
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x11,0,current_gpr);
		errors++;
	}
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x0 , HQ3_CFIFO_MASK);

	POLL_HQCP_PC(7);

   	HQ3_PIO_RD(CP_DATA_ADDR, HQ3_CPDATA_MASK, current_gpr);
   	msg_printf(DBG,"Readback 4 = 0x%x\n",current_gpr);
	if (current_gpr != 0x3f800000) {
		errors++;
		msg_printf(ERR,"HQ3 Stuff test failed: cmd = 0x%x, expected = 0x%x, actual = 0x%x\n",
		0x11,0x3f800000,current_gpr);
	}
        REPORT_PASS_OR_FAIL((&HQ3_err[HQ3_CONV_STUFF_TEST]), errors);
}

__uint32_t
mgras_hq3_converter(void)
{
	__uint32_t errors =0;

	mgrasDownloadHQ3Ucode(mgbase, &iset_table[0], (sizeof(iset_table)/sizeof(iset_table[0])), 1); 
	errors += mgras_conv_test32();
	errors += mgras_conv_test16();
	errors += mgras_conv_test8();
	errors += mgras_stuff_test();
	if ( errors != 0)
		msg_printf(ERR,"Mgras HQ3 Converter Test failed\n");
	else
		msg_printf(SUM,"Mgras HQ3 Converter Test passed\n");

	return errors;
}
