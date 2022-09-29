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

#include "sys/types.h"
#include <libsk.h>
#include "uif.h"
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "ge_inst.h"
#include "mgras_hq3.h"
#include "mgras_hq3_cp.h"
#include "ge11symtab.h"
#include "cp_ge_sim_tokens.h"
#include "ide_msg.h"

/***************************************************************************
*   Routine: mgras_ge11_cram 
*
*   Purpose: Test the  cram inside GE11
*
*   History: 12/22/94 : Original Version.
*
*   Notes: Send address unique and ckecker board patterns, read back and verify 
*
****************************************************************************/

__uint32_t
mgras_ge11_cram(void)
{
  	__uint32_t i, pattern, expected, actual, failed_addr;
	int errors = 0;

   	msg_printf(VRB,"GE11 Cram Addruniq Test\n");
	GE11_ROUTE_GE_2_HQ();

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, cram_Addruniq , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

	/* Test passed */
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Cram Addruniq Test passed\n");
	}
	/* Test failed */
	else if (actual == GE_DIAG_FAILED)
	     {

		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 

	        msg_printf(ERR,"**** GE11 Cram Addruniq Test failed\n");
		msg_printf(VRB,"**** CRAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
		errors++;
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_CRAM_TEST]), errors);
	     }

/**************************************************************************/

   msg_printf(VRB,"GE11 Cram Data Pattern Test\n");
   for (i = 0; i < 6;  i++)
   {
	DELAY(100000);
	pattern = patrn[i];
  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*3 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 2 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, cram_mempat , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, pattern , HQ3_CFIFO_MASK);


   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	 actual = GE11_Diag_read(~0);

        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Cram Data Pattern Test 0x%x passed\n",pattern);
	}	
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 
		msg_printf(ERR,"**** GE11 Cram Data Pattern Test failed\n");
/*
		msg_printf(VRB,"**** CRAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
*/
		errors++;
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_CRAM_TEST]), errors);
	     }
   }

   GE11_ROUTE_GE_2_RE();
   msg_printf(VRB,"GE11_cram_data_pattern_test completed\n");
   REPORT_PASS_OR_FAIL((&GE11_err[GE11_CRAM_TEST]), errors);
 	        
}	

/***************************************************************************
*   Routine: mgras_ge11_wram 
*
*   Purpose: Test the  wram inside GE11
*
*   History: 12/22/94 : Original Version.
*
*   Notes: Send address unique and ckecker board patterns, read back and verify 
*
****************************************************************************/

__uint32_t
mgras_ge11_wram(void)
{
   	__uint32_t i, pattern, expected, actual, failed_addr, errors = 0; 

   	msg_printf(VRB,"GE11 Wram Addruniq Test\n");
	GE11_ROUTE_GE_2_HQ();


  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, wram_Addruniq , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

	/* Test passed */
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"       GE11 Wram Addruniq Test passed\n");
	}	

	/* Test failed */
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 

		msg_printf(ERR,"**** GE11 Wram Addruniq Test failed\n");
		msg_printf(VRB,"**** WRAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);

		errors++;
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_WRAM_TEST]), errors);
	     }

/**************************************************************************/
    
   msg_printf(VRB,"GE11 Wram Data Pattern Test\n");

   for (i = 0; i < 6;  i++) 
   {
	DELAY(100000);
	pattern = patrn[i];
  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*3 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 2 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, wram_mempat , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, pattern , HQ3_CFIFO_MASK);


   	/* Waiting for data coming back from Rebus or time-out and flag error */
	actual = GE11_Diag_read(~0);	

        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Wram Data Pattern Test 0x%x passed\n",pattern);
	}	
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 

		msg_printf(ERR,"**** GE11 Wram Data Pattern Test failed\n");
		msg_printf(VRB,"**** WRAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
		errors++;
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_WRAM_TEST]), errors);
	     }
   }
   GE11_ROUTE_GE_2_RE(); 	        
   msg_printf(VRB,"GE11 Wram Data Pattern Test completed\n");
   REPORT_PASS_OR_FAIL((&GE11_err[GE11_WRAM_TEST]), errors);
}	

/***************************************************************************
*   Routine: mgras_ge11_eram 
*
*   Purpose: Test the  eram inside GE11
*
*   History: 12/22/94 : Original Version.
*
*   Notes: Send address unique and ckecker board patterns, read back and verify 
*
****************************************************************************/

__uint32_t
mgras_ge11_eram(void)
{
   	__uint32_t i, expected, pattern, actual, failed_addr, errors = 0; 

   	msg_printf(VRB,"GE11 Eram Databus Test\n");
	GE11_ROUTE_GE_2_HQ();
  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, eram_Databus , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

	/* Test passed */
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Eram Databus Test passed\n");
	}
	/* Test failed */
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 
		errors++;
		msg_printf(ERR,"**** GE11 Eram Databus Test failed\n");
		msg_printf(ERR,"**** ERAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);

	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_ERAM_TEST]), errors);
	     }

/**********************************************************************/

   	msg_printf(VRB,"GE11 Eram Addrbus Test\n");

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, eram_Addrbus , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

	/* Test passed */
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Eram Addrbus Test passed\n");
	}	

	/* Test failed */
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 
		errors++;
		msg_printf(ERR,"**** GE11 Eram Addrbus Test failed\n");
		msg_printf(ERR,"**** ERAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_ERAM_TEST]), errors);
	     }
/**************************************************************************/
   	msg_printf(VRB,"GE11 Eram Addruniq Test\n");

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, eram_Addruniq , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

	/* Test passed */
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"    GE11 Eram Addruniq Test passed\n");
	}
	/* Test failed */
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 

		errors++;
		msg_printf(ERR,"**** GE11 Eram Addruniq Test failed\n");
		msg_printf(ERR,"**** ERAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_ERAM_TEST]), errors);
	     }
/**************************************************************************/
   msg_printf(VRB,"GE11 Eram Data Pattern Test\n");
   for (i = 0; i < 6;  i++)
   {
	pattern = patrn[i];

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*3 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 2 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, eram_mempat , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, pattern , HQ3_CFIFO_MASK);

   	/* Waiting for data coming back from Rebus or time-out and flag error */

	actual = GE11_Diag_read(~0);
	
        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"     GE11 Eram Data Pattern Test 0x%x passed\n",pattern);
	}	
	else if (actual == GE_DIAG_FAILED)
	     {
		expected = GE11_Diag_read(~0);
          
	     	actual = GE11_Diag_read(~0);

	      	failed_addr = GE11_Diag_read(~0); 
		errors++;
		msg_printf(ERR,"**** GE11 Eram Data Pattern Test failed\n");
		msg_printf(ERR,"**** ERAM addr = 0x%x, expected = 0x%x, actual = 0x%x\n", failed_addr, expected, actual);
	     }
	     else
	     {
		errors++;
		msg_printf(ERR,"Time-out\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_ERAM_TEST]), errors);
	     }
   }
   GE11_ROUTE_GE_2_RE(); 	        
   msg_printf(VRB,"GE11 Eram Test completed\n");
   REPORT_PASS_OR_FAIL((&GE11_err[GE11_ERAM_TEST]), errors);
}	
#if 0
mgras_ge11_areg()
{
   	__uint32_t i, expected, actual, failed_addr, errors = 0; 

  	 msg_printf(VRB,"GE11_areg_test\n");

   	GE11_ROUTE_GE_2_HQ();

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*3 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 2 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, areg, HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x5a5a5a5a, HQ3_CFIFO_MASK);


   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

        if (actual == GE_DIAG_PASSED) {
	    msg_printf(VRB,"GE11_areg_test passed\n"); 
	}	
	else if (actual == GE_DIAG_FAILED)
	{
	      errors++;
              msg_printf(ERR,"GE11_areg_test failed\n"); 
	GE11_ROUTE_GE_2_RE();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_AREG_TEST]), errors);
}	
#endif

__uint32_t
mgras_ge11_dreg(void)
{
   	__uint32_t actual, errors = 0; 

   	msg_printf(VRB,"GE11 Dreg Test\n");

	GE11_ROUTE_GE_2_HQ();

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, dreg, HQ3_CFIFO_MASK);


   	/* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);
	
	if (actual == GE_DIAG_PASSED)
		msg_printf(VRB,"    GE11 Dreg Test passed\n");

	else
	{
		errors++;
		msg_printf(ERR,"**** GE11 Dreg Test failed\n");
 	}        
	GE11_ROUTE_GE_2_RE();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_DREG_TEST]), errors);
}	

#ifdef MFG_USED
__uint32_t
mgras_ge11_areg1(void)
{
  	__uint32_t i, actual;
	int errors = 0;

   	msg_printf(VRB,"GE11_areg1_test\n");
   	msg_printf(DBG,"ucode start at 0x%x\n",areg1);



	GE11_ROUTE_GE_2_HQ();

  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*30 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 29 , HQ3_CFIFO_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, areg1, HQ3_CFIFO_MASK);
	for (i = 0; i < 28; i++)
   	   HQ3_PIO_WR(CFIFO1_ADDR, i, HQ3_CFIFO_MASK);



   /* Waiting for data coming back from Rebus or time-out and flag error */
	
	for (i =0; i < 28; i++) {
		actual = GE11_Diag_read(~0);
	        msg_printf(DBG,"actual = 0x%x\n",actual);
	}

	actual = GE11_Diag_read(~0);
        if (actual == GE_DIAG_PASSED)  {  
	    msg_printf(VRB,"GE11_areg1_test passed\n");
	}
	else if (actual == GE_DIAG_FAILED)
	{
		errors++;
		msg_printf(ERR,"GE11_areg1_test failed\n");
	}

	GE11_ROUTE_GE_2_RE();

	REPORT_PASS_OR_FAIL((&GE11_err[GE11_AREG_TEST]), errors);
} 	        
#endif /* MFG_USED */

#define 	AALU3_DATA_MASK	0x3ffff
	__uint32_t aalu3_op[][3] = {
		0x3ffff, 0, 0x3ffff,
		0x3fffe, -0x3fffe, 0,
		0x3ffff, 0x3ffff, 0x3fffe, 
		-0x1, 0, -0x1,
		0x1ffff, 0x1ffff, -0x2,
        	0x3ffff/2, 0x3ffff/2, 0x3fffe,
	};

__uint32_t
mgras_ge11_aalu3(void)
{
   	__uint32_t i, actual, errors = 0; 

   	msg_printf(DBG,"GE11_aalu3_test\n");

	GE11_ROUTE_GE_2_HQ();

   	for (i = 0; i < sizeof(aalu3_op)/(4*3); i++)
   	{
  	   /* Send HQCP pass through */
   	   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*4 )) , HQ3_CFIFO_MASK);

   	   /* Then, send GE command token and data  */

   	   HQ3_PIO_WR(CFIFO1_ADDR, 3 , AALU3_DATA_MASK);
   	   HQ3_PIO_WR(CFIFO1_ADDR, aalu3, AALU3_DATA_MASK);
   	   HQ3_PIO_WR(CFIFO1_ADDR, aalu3_op[i][0], AALU3_DATA_MASK);
   	   HQ3_PIO_WR(CFIFO1_ADDR, aalu3_op[i][1], AALU3_DATA_MASK);



          /* Waiting for data coming back from Rebus or time-out and flag error */
	

	  actual = GE11_Diag_read(~0);
msg_printf(DBG,"actual = 0x%x, expected = 0x%x\n",actual & AALU3_DATA_MASK,(aalu3_op[i][2] & AALU3_DATA_MASK));
          if ((actual & AALU3_DATA_MASK) != (aalu3_op[i][2] & AALU3_DATA_MASK))
	  {
		errors++;
       		msg_printf(ERR,"GE11_aalu3_test failed\n");

       		msg_printf(ERR,"opr1= 0x%x, opr2 = 0x%x, expected = 0x%x, actual = 0x%x\n",aalu3_op[i][0],aalu3_op[i][1],aalu3_op[i][2],actual);

          }
   	}
	GE11_ROUTE_GE_2_RE();
   	msg_printf(VRB,"GE11_aalu3_test completed\n");
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_AALU3_TEST]), errors);
}
#define 	ALU_DATA_MASK	0xffffffff
__uint32_t iadd_op[][3] = {
	0xffffffff, 0, 0xffffffff,
	0xfffffffe, -0xfffffffe, 0,
	0xffffffff, 0xffffffff, 0xfffffffe, 
	-0x1, 0, -0x1,
        0x7fffffff, 0x7fffffff, 0x7fffffff,
};

__uint32_t
mgras_ge11_iadd(void)
{
   __uint32_t i, actual, errors = 0; 

   msg_printf(VRB,"GE11 Iadd Test\n");

   GE11_ROUTE_GE_2_HQ();
   for (i = 0; i < sizeof(iadd_op)/(4*3); i++)
   {
  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*4 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 3 , ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, iadd, ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, iadd_op[i][0], ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, iadd_op[i][1], ALU_DATA_MASK);



   /* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

msg_printf(DBG,"actual = 0x%x, expected = 0x%x\n",actual & ALU_DATA_MASK,(iadd_op[i][2] & ALU_DATA_MASK));
        if ((actual & ALU_DATA_MASK) != (iadd_op[i][2] & ALU_DATA_MASK))
	{
		errors++;
       		msg_printf(ERR,"**** GE11 Alu Test failed\n");
/*
       		msg_printf(VRB,"**** opr1= 0x%x, opr2 = 0x%x, expected = 0x%x, actual = 0x%x\n",iadd_op[i][0],iadd_op[i][1],iadd_op[i][2],actual);
*/
         }
   }
   if (errors == 0)
   	msg_printf(VRB,"     GE11 Alu Test passed\n");

   GE11_ROUTE_GE_2_RE();
   REPORT_PASS_OR_FAIL((&GE11_err[GE11_ALU_TEST]), errors);
}

#ifdef MFG_USED
static __uint32_t imul_op[][3] = {
	0x123, 0x123, 0x14ac9,
	0xfffffffe, 0xfffffffe, 0xfffffffe,
	0xfffffffe, 0xffffffff, 0xfffffffe, 
	-0x1, 1, -0x1,
        0x7fffffff, 0x7fffffff, 0x1,
};

__uint32_t
mgras_ge11_imul(void)
{
   __uint32_t i, actual, errors = 0; 

   msg_printf(VRB,"GE11_imul_test\n");
   GE11_ROUTE_GE_2_HQ();

   for (i = 0; i < sizeof(imul_op)/(4*3); i++)
   {
  	/* Send HQCP pass through */
   	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*4 )) , HQ3_CFIFO_MASK);

   	/* Then, send GE command token and data  */

   	HQ3_PIO_WR(CFIFO1_ADDR, 3 , ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, imul, ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, imul_op[i][0], ALU_DATA_MASK);
   	HQ3_PIO_WR(CFIFO1_ADDR, imul_op[i][1], ALU_DATA_MASK);



   /* Waiting for data coming back from Rebus or time-out and flag error */
	
	actual = GE11_Diag_read(~0);

         if ((actual & ALU_DATA_MASK) != (imul_op[i][2] & ALU_DATA_MASK))
	 {
		errors++;
       		msg_printf(ERR,"GE11_imul_test failed\n");
/*
       		msg_printf(VRB,"opr1= 0x%x, opr2 = 0x%x, expected = 0x%x, actual = 0x%x\n",imul_op[i][0],imul_op[i][1],imul_op[i][2],actual);
*/
         }
   }
   msg_printf(VRB,"GE11_imul_test completed\n");
   GE11_ROUTE_GE_2_RE();
   REPORT_PASS_OR_FAIL((&GE11_err[GE11_MUL_TEST]), errors);
}
#endif /* MFG_USED */

__uint32_t
mgras_ge11_inst(void)
{
	__uint32_t errors = 0;
	__uint32_t rcv_h, rcv_l;
	__uint32_t  i;
	msg_printf(DBG,"GE11 Instruction test\n");
msg_printf(DBG,"cmd = 0x%x\n",imedt);

	GE11_ROUTE_GE_2_HQ();
	/* Send token through HQCP pass through */

	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

	/* Then, send GE command token without data  */
	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, imedt, HQ3_CFIFO_MASK);

	/* Waiting for data coming back from Rebus or time-out and flag error */
	for (i = 0; i < _NUM_TOKENS_RCVD; i++)
	{
	     rcv_h = 0xdead; rcv_l = 0xbaad;
	     GE11_Diag_read_Dbl(~0, &rcv_h, &rcv_l);
#if 0
	     msg_printf(DBG, "Hexp 0x%x Lexp 0x%x Hrcv 0x%x Lrcv 0x%x\n" ,expected_token[i], expected_token[i+1], rcv_h, rcv_l);	
	     if ((expected_token[i] != rcv_h) || (expected_token[i+1] != rcv_l) ) {							
	     msg_printf(ERR, "Error Hexp 0x%x Lexp 0x%x Hrcv 0x%x Lrcv 0x%x\n" ,expected_token[i], expected_token[i+1], rcv_h, rcv_l);	
	     }
#endif
	     if (expected_token[i] != rcv_l ) 
	     {
		errors++;	
		msg_printf(ERR, "i= %d Lexp 0x%x  Lrcv 0x%x \n" ,i,expected_token[i], rcv_l);
	     }	

	}
	GE11_ROUTE_GE_2_RE();
	if (errors == 0)
		msg_printf(VRB, "mgras_GE_INST test passed\n");
	else
		msg_printf(ERR, "mgras_GE_INST test failed\n");
		
   	REPORT_PASS_OR_FAIL((&GE11_err[GE11_INST_TEST]), errors);
		
}
#if 0
mgras_ge11_fib()
{
	__uint32_t expected, actual, errors = 0;
	__uint32_t rcv_l;
	__uint32_t  i;
	msg_printf(VRB,"GE11 fibonacci test\n");
msg_printf(1,"cmd = 0x%x\n",ge_fib_stress);

	GE11_ROUTE_GE_2_HQ();
	/* Send token through HQCP pass through */

	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

	/* Then, send GE command token without data  */
	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, ge_fib_stress, HQ3_CFIFO_MASK);

	/* Waiting for data coming back from Rebus or time-out and flag error */
	for (i = 0; i < 100 ; i++)
	{
	     rcv_l = 0xbaad;
	     rcv_l = GE11_Diag_read(~0);

msg_printf(1,"i = %d, rec'd data = 0x%x\n",rcv_l); 
	}
	GE11_ROUTE_GE_2_RE();
	msg_printf(VRB, "mgras_Fib_Stress test completed\n");
}
#endif

#ifdef MFG_USED
__uint32_t
mgras_ge11_mul_stress(void)
{
	__uint32_t errors = 0;
	__uint32_t rcv_l;
	__uint32_t  i;
	msg_printf(VRB,"GE11 Multiplication test\n");

	GE11_ROUTE_GE_2_HQ();
	/* Send token through HQCP pass through */

	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (4*2 )) , HQ3_CFIFO_MASK);

	/* Then, send GE command token without data  */
	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, ge_mul, HQ3_CFIFO_MASK);

	/* Waiting for data coming back from Rebus or time-out and flag error */
	for (i = 0; i < 100 ; i++)
	{
	     rcv_l = 0xbaad;
	     rcv_l = GE11_Diag_read(~0);

	}
	GE11_ROUTE_GE_2_RE();
	msg_printf(VRB, "mgras_mul test completed\n");

	return(errors);
}		
#endif /* MFG_USED */

__uint32_t
mgras_nextge(void)
{
	__uint32_t errors = 0;

	msg_printf(VRB,"Next GE %d\n",switch_ge);

	GE11_ROUTE_GE_2_HQ(); 
	/* Send token through HQCP pass through */

	HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_NEXTGE << 8 | (4*2 )) , HQ3_CFIFO_MASK);

	/* Then, send GE command token without data  */
	HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, switch_ge, HQ3_CFIFO_MASK);
/*
	actual = GE11_Diag_read(~0);
msg_printf(1,"rec'd data = 0x%x\n",actual); 
*/
	GE11_ROUTE_GE_2_RE(); 
	msg_printf(VRB, "Next GE done\n");

	return errors;
}

#ifdef MFG_USED
__uint32_t
mgras_rebus_read(void)
{
	__uint32_t actual, errors = 0;

	GE11_ROUTE_GE_2_HQ();
	actual = GE11_Diag_read(~0);
	msg_printf(1,"rec'd data = 0x%x\n",actual);
	GE11_ROUTE_GE_2_RE();

	return(errors);
}

__uint32_t
mgras_ge_power(void)
{
	__uint32_t errors = 0;

	msg_printf(VRB,"Ge Power test\n");
	msg_printf(1,"cmd = 0x%x\n",ge_power);


	HQ3_PIO_WR(CFIFO1_ADDR, (CP_GE_POWER << 8 | (4*3 )) , HQ3_CFIFO_MASK);

	/* Then, send GE command token without data  */
	HQ3_PIO_WR(CFIFO1_ADDR, 0xffffffff , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, 0x00000000 , HQ3_CFIFO_MASK);
	HQ3_PIO_WR(CFIFO1_ADDR, ge_power, HQ3_CFIFO_MASK);
	msg_printf(VRB, "End of GE Power Test\n");

	return errors;
}
#endif /* MFG_USED */
