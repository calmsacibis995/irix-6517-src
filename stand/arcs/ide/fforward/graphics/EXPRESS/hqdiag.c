/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/fforward/graphics/EXPRESS/RCS/hqdiag.c,v 1.12 1995/02/13 19:53:20 jeffs Exp $ */
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "ide_msg.h"
#include <sys/gr2hw.h>
#include <sys/param.h>	/* For DELAY() */
#include "diag_glob.h"
#include "diagcmds.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

int gr2_reg_test(void);
int gr2_act_attr_test(void);
int gr2_attr_test(void);
int gr2_attr_test_broadcast(void);
int gr2_rel_vctr_test(void);
int gr2_s4f(void);
int gr2_s4i(void);
int gr2_c4f(void);
int gr2_c4i(void);
int gr2_c3f(void);
int gr2_c3i(void);
int gr2_s3f(void);
int gr2_s3i(void);
int gr2_s2f(void);
int gr2_s2i(void);
int gr2_cpack(void);
int gr2_cindex(void);
int gr2_cflt(void);
int int_to_float(int);
int float_to_int(int);
extern int power(int, int);
int normalized_to_int(int, int);
void Clear_GERam(int, int);
void show_ram0_hex(int);
int check_value(int, int, int);

#define FLOAT_ONE 	 0x3f800000 /* IEEE float point one */
#define NEAR_ONE	 0x3f7f0000 /* IEEE float point 255/256 */ 
#define ALMOST_ONE	 0x3f7ff000 /* IEEE float point 4095/4096 */ 
#define NUM_RANGE	 0x01000000 /*  power(2,24) */ 
#define INCR_NUM	 0x00101010  /* power(2,20)+ */ 
#define UC_FLAG_ONE	 0xffff /* HQ ucode flag one value */
extern struct gr2_hw *base;

static int numge;
/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

/*ARGSUSED*/
int
gr2_hq2test(int argc, char *argv[])
{
  int errors;

  errors =0;
  /* Find out how many GEs installed */
  numge = Gr2ProbeGEs(base);

  if (gr2_reg_test() > 0) {
    msg_printf(ERR,"HQ2 Registers Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 Registers test PASSED...\n");

  if (gr2_attr_test() > 0) {
    msg_printf(ERR,"HQ2 attribute Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 attribute test PASSED...\n");

  if (gr2_attr_test_broadcast() > 0) {
    msg_printf(ERR,"HQ2 attribute Test - broadcast mode Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 attribute test- broadcast mode  PASSED...\n");

  if (gr2_act_attr_test() > 0) {
    msg_printf(ERR,"HQ2 active attribute Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 active attribute test PASSED...\n");

  if (gr2_rel_vctr_test() > 0) {
    msg_printf(ERR,"HQ2 Relative vctr Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 Relative vctr test PASSED...\n");

  if (gr2_s4f() > 0) {
    msg_printf(ERR,"HQ2 S4f Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S4f test PASSED...\n");

  if (gr2_s4i() > 0) {
    msg_printf(ERR,"HQ2 S4i Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S4i test PASSED...\n");

  if (gr2_s3f() > 0) {
    msg_printf(ERR,"HQ2 S3f Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S3f test PASSED...\n");

  if (gr2_s3i() > 0) {
    msg_printf(ERR,"HQ2 S3i Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S3i test PASSED...\n");

  if (gr2_s2f() > 0) {
    msg_printf(ERR,"HQ2 S2f Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S2f test PASSED...\n");

  if (gr2_s2i() > 0) {
    msg_printf(ERR,"HQ2 S2i Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 S2i test PASSED...\n");

  if (gr2_c4i() > 0) {
    msg_printf(ERR,"HQ2 C4i Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 C4i test PASSED...\n");

  if (gr2_c4f() > 0) {
    msg_printf(ERR,"HQ2 C4f Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 C4f test PASSED...\n");

  if (gr2_c3i() > 0) {
    msg_printf(ERR,"HQ2 C3i Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 C3i test PASSED...\n");

  if (gr2_c3f() > 0) {
    msg_printf(ERR,"HQ2 C3f Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 C3f test PASSED...\n");

  if (gr2_cpack() > 0) {
    msg_printf(ERR,"HQ2 Cpack Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 Cpack test PASSED...\n");

  if (gr2_cindex() > 0) {
    msg_printf(ERR,"HQ2 Cindex(color) Test Failed\n");
    errors++; 
  }
  else msg_printf(VRB,"Hq2 Cindex(color) test PASSED...\n");

  if (gr2_cflt() > 0) {
    msg_printf(ERR,"HQ2 Cflt(colorf) Test Failed\n");
    errors++;
  }
  else msg_printf(VRB,"Hq2 Cflt(colorf) test PASSED...\n");

 if (errors == 0) msg_printf(VRB,"Hq2 Tests PASSED....\n");
 else msg_printf(VRB,"Hq2 Tests failed...\n");
 return errors;
}
/***************************************************************************
*   Routine: gr2_numge_test
*
*   Purpose: To test the numge register and Ge counter			
*
*   History:
*	    1/30/92	    Keiming Yen 	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_reg_test(void)
{
	int val,err=0,i;

	/* clear the share RAM results area */
	for (i=0; i<numge; i++)
	{
		Clear_GERam(i,128);
	}

	msg_printf(DBG, "HQ2 test: Registers test...\n");

	/* send command */

	base->fifo[DIAG_GECTR] = 0x55aa; /* DREG = 0x55aa */

	/* wait for diag ucode to finish */
	time_delay(1000);

	for (i=0; i< numge; i++)
	{
		/* read numge */   
		val = base->ge[i].ram0[0];
		if (val != numge) {
			msg_printf(ERR,"HQ2: Wrong NUMGE, expected = %d, actual = %d\n",numge,val);
			err++;
		}
		/* read current ge number */
		val = base->ge[i].ram0[1];
		if (val != i) {
			msg_printf(ERR,"HQ2: Wrong gectr, expected = %d, actual = %d\n",i,val);
			err++;
		}
		/* read DREG value */
		val = base->ge[i].ram0[2];
		if (val != 0x55aa) {
			msg_printf(ERR,"HQ2: Wrong DREG, expected = %d, actual = %d\n",0x55aa,val);
			err++;
		}
		/* read Ucode ONE value */
		val = base->ge[i].ram0[3];
		if (val != UC_FLAG_ONE) {
			msg_printf(ERR,"HQ2: Wrong ONE Reg., expected = %d, actual = %d\n",UC_FLAG_ONE,val);
			err++;
		}
		/* read UC ZERO reg. */
		val = base->ge[i].ram0[4];
		if (val != 0) {
			msg_printf(ERR,"HQ2: Wrong ZERO Reg., expected = %d, actual = %d\n",0,val);
			err++;
		}
		
	}

    	if(err)
    	{
		msg_printf(ERR, "HQ2: Registers test failed RAM0 dump following\n\n");
		show_ram0_hex(32);

        	msg_printf(VRB, "Test failed......\n");
    	}

    return(err);
}
/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_attr_test(void)
{
    unsigned int index;
    register int i;
    int	val;
    int	err;


    /* clear the shared RAM results area */

    for(i=0; i<numge; i++) 
    {
	Clear_GERam(i, 128);
    }

    /* send command */

    msg_printf(DBG, "HQ2 test: Attribute Test...\n");

    err = 0;
    val = 0;
    base->fifo[DIAG_ATTR] = 0;
    for(i=0; i<numge; i++) 
    {
    	msg_printf(DBG, "Attribute Test: Sending command to GE7%#x....\n",i);
    	base->fifo[DIAG_ATTR] = 0x11111111;
    	base->fifo[DIAG_ATTR] = 0x22222222;
    	base->fifo[DIAG_ATTR] = 0x33333333;
    	base->fifo[DIAG_ATTR] = 0x44444444;
    	base->fifo[DIAG_ATTR] = 0x55555555;
       /* Wait for diag ucode to finish */

    	time_delay(1000);
   	 msg_printf(DBG, "Attribute store Test : Checking results....\n");

	for(index = 0; index <= 15; index += 3)
	{
	    val = 0;

	    val = base->ge[i].ram0[index];

	    if (val != UC_FLAG_ONE) 
	    {
		msg_printf(DBG, "Attribute test failed for GE7 #%d\n", i);
		msg_printf(DBG, "Expected value 0x%x, actual = %#x index = %#x\n",UC_FLAG_ONE,val,index);
		err++;
	    }
	}
	for(index = 1; index <= 15; index += 3)
	{
	    val = 1;

	    val = base->ge[i].ram0[index];

	    if (val != 0) 
	    {
		msg_printf(ERR, "Attribute test failed for GE7 #%d\n", i);
		msg_printf(ERR, "Expected value 0, actual = %#x index = %#x\n",val,index);
		err++;
	    }
	}
	    err += check_value(i, 2, 0x11111111);
	    err += check_value(i, 5, 0x22222222);
	    err += check_value(i, 8, 0x33333333);
	    err += check_value(i, 11, 0x44444444);
	    err += check_value(i, 14, 0x55555555);

    }

    if(err)
    {
	msg_printf(ERR, "Attribute test failed RAM0 dump following\n\n");
	show_ram0_hex(32);

        msg_printf(VRB, "Test failed......\n");
    }

    return(err);
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_attr_test_broadcast(void)
{
    register	i;
    int	val;
    int	err;
    unsigned int index;

    /* clear the shared RAM results area */

    for(i=0; i<numge; i++) 
    {
	Clear_GERam(i, 128);
    }

    /* send command */

    msg_printf(DBG, "HQ2 - Attribute Test Broadcast mode:....\n");

    base->fifo[DIAG_ATTR_ALL] = 0;
    base->fifo[DIAG_ATTR_ALL] = 0x11111111;
    base->fifo[DIAG_ATTR_ALL] = 0x22222222;
    base->fifo[DIAG_ATTR_ALL] = 0x33333333;
    base->fifo[DIAG_ATTR_ALL] = 0x44444444;
    base->fifo[DIAG_ATTR_ALL] = 0x55555555;

    /* Wait for diag ucode to finish */

    time_delay(1500);

    err = 0;
    val = 0;


    for(i=0; i<numge; i++) 
    {

	for(index = 0; index <= 15; index += 3)
	{
	    val = 0;

	    val = base->ge[i].ram0[index];

	    if (val != UC_FLAG_ONE) 
	    {
		msg_printf(ERR, "Attribute test failed for GE7 #%d\n", i);
		msg_printf(ERR, "Expected value 0x%x, actual = 0x%x\n",UC_FLAG_ONE,val);
		err++;
	    }
	}

	for(index = 1; index <= 15; index += 3)
	{
	    val = 0;

	    val = base->ge[i].ram0[index];

	    if (val != 0) 
	    {
		msg_printf(ERR, "Attribute test failed for GE7 #%d\n", i);
		msg_printf(ERR, "Expected value 0x%x, actual = 0x%x\n",0,val);
		err++;
	    }
	}

	    err = 0;
	    check_value(i, 2, 0x11111111);
	    check_value(i, 5, 0x22222222);
	    check_value(i, 8, 0x33333333);
	    check_value(i, 11, 0x44444444);
	    check_value(i, 14, 0x55555555);


    }

    if(err)
    {
	msg_printf(ERR, "Attribute test failed RAM0 dump following\n\n");
	show_ram0_hex(32);

        msg_printf(VRB, "Attribute Test failed......\n");
    }

    return(err);
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_act_attr_test(void)
{
    unsigned int newval;
    register int i;
    int	index;
    int err;


    msg_printf(DBG, "HQ2 - Active Attribute Test....\n");


    err = 0;

    for(i=0; i < numge; i++) 
    {
    /* clear the shared RAM results area */
	Clear_GERam(i, 128);
   

    /* send command */

   	 msg_printf(DBG, "Active Attribute Test: Sending command to GE7....\n");
    	base->fifo[DIAG_ACTATTR] = 0;

    	/* Wait for diag ucode to finish */

    	time_delay(1500);
    

	for(index = 0; index < 16; index++)
    	{
	    
		if( (newval = base->ge[i].ram0[(int)index]) != UC_FLAG_ONE)
		{ 
	    		msg_printf(DBG, "Expected value 0xffff, actual = %x\n", newval);
	    		err++;    
		}

    	}  
    } 
    if(err)
    {
	msg_printf(ERR, "Active Attribute test failed RAM0 dump following\n\n");
/*	show_ram0_hex(16); */
	msg_printf(VRB,"active attribute Test failed.........\n");
    }

    return(err);
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_rel_vctr_test(void)
{
    register	i;
    int	err;
    unsigned int newval;

    err = 0;
    /* clear the shared RAM results area */

    for(i=0; i < numge; i++) 
    {
	Clear_GERam(i, 128);
    }

    msg_printf(DBG, "HQ2 - Relative Vctr Test....\n");
    /* send command */

    msg_printf(DBG, "Relative Vctr Test: Sending command to GE7....\n");

    base->fifo[DIAG_RELVCTR] = 0;
 
    time_delay(1000);


    for(i=0; i < numge; i++) 
    {
	if( (newval = base->ge[i].ram0[2]) != UC_FLAG_ONE)
	{ 
	    msg_printf(DBG, "Expected value 0xffff, actual = %x\n", newval);
	    err++;    
	}
	if( (newval = base->ge[i].ram0[10]) != UC_FLAG_ONE)
	{ 
	    msg_printf(DBG, "Expected value 0xffff, actual = %x\n",newval);
	    err++;    
	}
	if( (newval = base->ge[i].ram0[18]) != UC_FLAG_ONE)
	{ 
	    msg_printf(DBG, "Expected value 0xffff, actual = %x\n",newval);
	    err++;    
	}
	if( (newval = base->ge[i].ram0[26]) != UC_FLAG_ONE)
	{ 
	    msg_printf(DBG, "Expected value 0xffff, actual = %x\n",newval);
	    err++;    
	}
    }
   
    if(err)
    {
	msg_printf(ERR, "Relative Vctr test failed.....\n\n");
	show_ram0_hex(32);
    }  

    return(err);
}
/***************************************************************************
*   Routine: gr2_s4f (v4f)  
*
*   Purpose:
*	    NOOP			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_s4f(void)
{

    int i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "S4F: Sending command to GE7....\n");

      for (i = -NUM_RANGE ;i < NUM_RANGE;i = i + INCR_NUM)  {

      	base->fifo[DIAG_S4F ] = int_to_float(i);
      	base->fifo[DIAG_S4F ] = int_to_float(i+1);
      	base->fifo[DIAG_S4F ] = int_to_float(i+2);
      	base->fifo[DIAG_S4F ] = int_to_float(i+3);
   	time_delay(500);
	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != int_to_float(i)) {
	  err++;
	  msg_printf(DBG,"x= %#x, converted x value= %#x\n",int_to_float(i),base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != int_to_float(i+1)) {
	  err++;
	  msg_printf(DBG,"y= %#x, converted y value= %#x\n",int_to_float(i+1),base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != int_to_float(i+2)) {
	  err++;
	  msg_printf(DBG,"z= %#x, converted z value= %#x\n",int_to_float(i+2),base->ge[0].ram0[2]);
        }
	if (base->ge[0].ram0[3] != int_to_float(i+3)) {
	  err++;
	  msg_printf(DBG,"w= %#x, converted w value= %#x\n",int_to_float(i+3),base->ge[0].ram0[3]);
	}
      } 
 return(err);    
}
/***************************************************************************
*   Routine: gr2_s4i  
*
*   Purpose:
*	    Convert to float...			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_s4i(void)
{

    int i, err;


    err =0;
    /* send command */

    msg_printf(DBG, "S4I: Sending command to GE7....\n");

      for ( i = -NUM_RANGE; i < NUM_RANGE; i = i + INCR_NUM)  {

      	base->fifo[DIAG_S4I ] = i;
      	base->fifo[DIAG_S4I ] = i+1;
      	base->fifo[DIAG_S4I ] = i+2;
      	base->fifo[DIAG_S4I ] = i+3;
   	time_delay(500);

	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != i) {
	  err++;
	  msg_printf(DBG,"x= %#x, converted x value= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != i+1) {
	  err++;
	  msg_printf(DBG,"y= %#x, converted y value= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != i+2) {
	  err++;
	  msg_printf(DBG,"z= %#x, converted z value= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != i+3) {
	  err++;
	  msg_printf(DBG,"w= %#x, converted w value= %#x\n",i+3,base->ge[0].ram0[3]);
	}
      }
	return (err);
}
/***************************************************************************
*   Routine:  	gr2_s3f (v3f) 
*
*   Purpose:
*	    Stuff W			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_s3f(void)
{

    int  i, err;


    err = 0;
    /* send command */

    msg_printf(DBG, "S3F: Sending command to GE7....\n");

      for ( i = -NUM_RANGE; i < NUM_RANGE; i = i + INCR_NUM)  { 

      	base->fifo[DIAG_S3I | DIAG_CONVV3 ] = int_to_float(i);
      	base->fifo[DIAG_S3I | DIAG_CONVV3 ] = int_to_float(i+1);
      	base->fifo[DIAG_S3I | DIAG_CONVV3 ] = int_to_float(i+2);
   	time_delay(500);

	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != int_to_float(i)) {
	  err++;
	  msg_printf(DBG,"x= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
        }
	if (base->ge[0].ram0[1] != int_to_float(i+1)) {
	  err++;
	  msg_printf(DBG,"y= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != int_to_float(i+2)) {
	  err++;
	  msg_printf(DBG,"y= %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
        }
	if (base->ge[0].ram0[3] != FLOAT_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff w= %#x\n",base->ge[0].ram0[3]);
	}
      }	 
   return(err);   
    
}
/***************************************************************************
*   Routine: gr2_s3i (v3i)
*
*   Purpose:
*	    Convert and Stuff W			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen         
*   Dependencies:                               
*
*   Notes:
*
****************************************************************************/
int
gr2_s3i(void)
{

    int i, err;


    err =0;
    /* send command */

    msg_printf(DBG, "S3I: Sending command to GE7....\n");

      for ( i = -NUM_RANGE; i < NUM_RANGE; i = i + INCR_NUM)  {
	base->ge[0].ram0[3] = 0xaa; /* make it nozero */
      	base->fifo[DIAG_S3I | DIAG_CONVV3 | DIAG_ITOF] = i;
      	base->fifo[DIAG_S3I | DIAG_CONVV3 | DIAG_ITOF] = i+1;
      	base->fifo[DIAG_S3I | DIAG_CONVV3 | DIAG_ITOF] = i+2;
   	time_delay(500);
	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (float_to_int(base->ge[0].ram0[0]) != i) {
	  err++;
	  msg_printf(DBG,"x= %#x, converted number= %#x\n",i,base->ge[0].ram0[0]);
        }
	
	if (float_to_int(base->ge[0].ram0[1]) != i+1) {
	  err++;
	  msg_printf(DBG,"y= %#x, converted number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (float_to_int(base->ge[0].ram0[2]) != i+2) {
	  err++;
	  msg_printf(DBG,"y= %#x, converted number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != FLOAT_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff w= %#x\n",base->ge[0].ram0[3]);
	}
       }
  return(err);
}
/***************************************************************************
*   Routine: gr2_s2i (v2i)
*
*   Purpose:
*	    convert and stuff Z and W			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_s2i(void)
{

    int i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "S2I: Sending command to GE7....\n");

      for ( i = -NUM_RANGE; i < NUM_RANGE; i = i + INCR_NUM)  {  
	base->ge[0].ram0[2] = 0xaa; /* make it nozero */
      	base->fifo[DIAG_S2I | DIAG_CONVV2 | DIAG_ITOF] = i;
      	base->fifo[DIAG_S2I | DIAG_CONVV2 | DIAG_ITOF] = i+1;
   	time_delay(500);

	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);


	if (float_to_int(base->ge[0].ram0[0]) != i) {
	  err++;
	  msg_printf(DBG,"x= %#x, converted number= %#x\n",i,float_to_int(base->ge[0].ram0[0]));
	}
	if (float_to_int(base->ge[0].ram0[1]) != i+1) {
	  err++;
	  msg_printf(DBG,"y= %#x, converted number= %#x\n",i+1,float_to_int(base->ge[0].ram0[1]));
	}
	if (float_to_int(base->ge[0].ram0[2]) != 0) {
	  err++;
	  msg_printf(DBG,"Stuff z = %#x\n",base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != FLOAT_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff w= %#x\n",base->ge[0].ram0[3]);
	}
      }	 
   return(err); 
}
/***************************************************************************
*   Routine: gr2_s2f (v2f)
*
*   Purpose:
*	    stuff Z and W			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_s2f(void)
{

    int i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "S2F: Sending command to GE7....\n");


      for (i=0;i <260;i=i+2)  {

	base->ge[0].ram0[2] = 0xaa; /* make it nozero */
      	base->fifo[DIAG_S2F | DIAG_CONVV2 ] = int_to_float(i);
      	base->fifo[DIAG_S2F | DIAG_CONVV2 ] = int_to_float(i+1);
   	time_delay(500);

	msg_printf(DBG,"x= %#x, y = %#x, z= %#x, w = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != int_to_float(i)) {
	  err++;
	  msg_printf(DBG,"x= %#x,  read number= %#x\n",int_to_float(i),base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != int_to_float(i+1)) {
	  err++;
	  msg_printf(DBG,"y= %#x, read number= %#x\n",int_to_float(i+1),base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != 0) {
	  err++;
	  msg_printf(DBG,"Stuff z = %#x\n",base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != FLOAT_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff w= %#x\n",base->ge[0].ram0[3]);
	}
      }	 
  return(err);
}
/***************************************************************************
*   Routine: gr2_c4i (c4i)
*
*   Purpose: Convert and Normalize {clamp 255/256 }
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_c4i(void)
{

    int i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "C4I: Sending command to GE7....\n");

 
      for (i=0;i <255;i=i+4)  {

    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+1;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+2;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+3;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, a = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (normalized_to_int(base->ge[0].ram0[0],8) != i) {
	  err++;
	  msg_printf(DBG,"r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (normalized_to_int(base->ge[0].ram0[1],8) != i+1) {
	  err++;
	  msg_printf(DBG,"g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (normalized_to_int(base->ge[0].ram0[2],8) != i+2) {
	  err++;
	  msg_printf(DBG,"b= %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (normalized_to_int(base->ge[0].ram0[3],8) != i+3) {
	  err++;
	  msg_printf(DBG,"a= %#x, read number= %#x\n",i+3,base->ge[0].ram0[3]);
	}
       }
   /* Check clampping */ 
    
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+1;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+2;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 | DIAG_ITOF] = i+3;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, a = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0]!= NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed:r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1]!= NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed:g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed:b= %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed:a= %#x, read number= %#x\n",i+3,base->ge[0].ram0[3]);
	}
  return(err);
}

/***************************************************************************
*   Routine: gr2_c4f (c4f)
*
*   Purpose: Clamp 255/256			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

int
gr2_c4f(void)
{

    int  i,err;

    err = 0;

    /* send command */

    msg_printf(DBG, "C4F: Sending command to GE7....\n");

    
      for (i=0x3b800000;i < 0x3f800000;i=i+0x00080000)  {

    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+1;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+2;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+3;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, a = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != i) {
	  err++;
	  msg_printf(DBG,"r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != i+1) {
	  err++;
	  msg_printf(DBG,"g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != i+2) {
	  err++;
	  msg_printf(DBG,"b = %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != i+3) {
	  err++;
	  msg_printf(DBG,"z = %#x, read number= %#x\n",i+3,base->ge[0].ram0[3]);
	}
      } 
      /* check clamppin */

	i = 0x3f800000; /* make it great than 255/256 */
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+0x00800000;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+0x00800000;
    	base->fifo[DIAG_C4I| DIAG_CONVC4 ] = i+0x00800000;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, a = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (base->ge[0].ram0[0] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed:r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed:g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	  if (base->ge[0].ram0[2] != NEAR_ONE) {
	  err++;
	msg_printf(DBG,"Clampping failed:b = %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clampping failed: alpha = %#x, read number= %#x\n",i+3,base->ge[0].ram0[3]);
	}
   return(err);
}

/***************************************************************************
*   Routine: gr2_c3i (c3i)
*
*   Purpose: Conver Normalize & Stuff alpha			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_c3i(void)
{

    int  i, err;

    err = 0;


    /* send command */

    msg_printf(DBG, "C3I: Sending command to GE7....\n");

      for (i=0;i <255;i=i+3)  {
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+1;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+2;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, alpha = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);


	if (normalized_to_int(base->ge[0].ram0[0],8) != i) {
	  err++;
	  msg_printf(DBG,"r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (normalized_to_int(base->ge[0].ram0[1],8) != i+1) {
	  err++;
	  msg_printf(DBG,"g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (normalized_to_int(base->ge[0].ram0[2],8) != i+2) {
	  err++;
	  msg_printf(DBG,"b = %#x read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff alpha= %#x\n",base->ge[0].ram0[3]);
	}
      } 
      /* check clampping */

    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+1;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+2;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, alpha = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);


	if (base->ge[0].ram0[0] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed: r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed: g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed:b = %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Stuffing failed:Stuff alpha= %#x\n",base->ge[0].ram0[3]);
	}
    return(err);
}
/***************************************************************************
*   Routine: gr2_c3f (c3f)
*
*   Purpose:i Stuff alpha and clamp
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_c3f(void)
{

    int  i, err;


    err = 0;
    /* send command */

    msg_printf(DBG, "C3F: Sending command to GE7....\n");
	/* 0 < i < 1 */
      for (i=0x3b800000;i < 0x3f800000;i=i+0x00080000)  {
	Clear_GERam(0,4);
    	base->fifo[DIAG_C3I| DIAG_CONVC3 ] = i;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 ] = i+1;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 ] = i+2;
	
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, alpha = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);
	if (base->ge[0].ram0[0] != i) {
	  err++;
	  msg_printf(DBG,"r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != i+1) {
	  err++;
	  msg_printf(DBG,"g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != i+2) {
	  err++;
	  msg_printf(DBG,"b = %#x read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Stuff alpha= %#x\n",base->ge[0].ram0[3]);
	}
      }
      /* check clampping */
	
	i = 0x3f800000; /* make it > 2555/256 */
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+1;
    	base->fifo[DIAG_C3I| DIAG_CONVC3 | DIAG_ITOF] = i+2;
   	time_delay(500);

	msg_printf(DBG,"r= %#x, g = %#x, b= %#x, alpha = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);


	if (base->ge[0].ram0[0] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed: r= %#x,  read number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (base->ge[0].ram0[1] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed: g= %#x, read number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (base->ge[0].ram0[2] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Clamping failed:b = %#x, read number= %#x\n",i+2,base->ge[0].ram0[2]);
	}
	if (base->ge[0].ram0[3] != NEAR_ONE) {
	  err++;
	  msg_printf(DBG,"Stuffing failed:Stuff alpha= %#x\n",base->ge[0].ram0[3]);
	}
    return(err);
}

/***************************************************************************
*   Routine: gr2_cpack
*
*   Purpose: Convert and Normalize			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_cpack(void)
{

    int i,err;

    err = 0;
    /* send command */

    msg_printf(DBG, "CPACK: Sending command to GE7....\n");

      for (i=0;i <255;i=i+4)  {
      	base->fifo[DIAG_CPACK | DIAG_CONVCP | DIAG_ITOF] = (i<<24) | ((i+1)<<16) | ((i+2)<<8) | i+3;
   	time_delay(500);

	msg_printf(DBG,"alpha= %#x, b = %#x, g= %#x, r = %#x\n",base->ge[0].ram0[0],base->ge[0].ram0[1],base->ge[0].ram0[2],base->ge[0].ram0[3]);

	if (normalized_to_int(base->ge[0].ram0[0],8) != i+3) {
	  err++;
	  msg_printf(DBG,"red value= %#x, converted number= %#x\n",i,base->ge[0].ram0[0]);
	}
	if (normalized_to_int(base->ge[0].ram0[1],8) != i+2) {
	  err++;
	  msg_printf(DBG,"green value= %#x, converted number= %#x\n",i+1,base->ge[0].ram0[1]);
	}
	if (normalized_to_int(base->ge[0].ram0[2],8) != i+1) {
	  err++;
	  msg_printf(DBG,"blue value= %#x, converted number= %#x\n",i+2,base->ge[0].ram0[1]);
	}
	if (normalized_to_int(base->ge[0].ram0[3],8) != i) {
	  err++;
	  msg_printf(DBG,"alpha value= %#x, converted number= %#x\n",i+3,base->ge[0].ram0[1]);
	}
      }	 
  return(err);
}

/***************************************************************************
*   Routine: gr2_cindex (color)
*
*   Purpose: Conver and normalize (clamp 4095/4096)			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_cindex(void)
{

    int  i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "CINDEX: Sending command to GE7....\n");

    for (i=0; i < 4095; i=i+128) {

      base->fifo[DIAG_CINDEX | DIAG_CONVC1 | DIAG_ITOF] = i;
    
      time_delay(500);
       

      msg_printf(DBG,"Color index = %#x, Converted index= %#x\n",i,base->ge[0].ram0[0]);
      if (normalized_to_int(base->ge[0].ram0[0],12) != i) {
        err++;
        msg_printf(DBG,"Color index = %#x, Converted index = %#x\n",i,base->ge[0].ram0[0]);
      }
    }
      /* check clamp */

      base->fifo[DIAG_CINDEX | DIAG_CONVC1 | DIAG_ITOF] = i;
    
      time_delay(500);
       

      msg_printf(DBG,"Color index = %#x, Converted index= %#x\n",i,base->ge[0].ram0[0]);
      if (base->ge[0].ram0[0] != ALMOST_ONE) {
        err++;
        msg_printf(DBG,"Color index = %#x, Converted index = %#x\n",i,base->ge[0].ram0[0]);
      }
   return(err); 
}

/***************************************************************************
*   Routine: gr2_cflt (colorf)
*
*   Purpose: Normalize (clamp 4095/4096)			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*	    5/22/91	    Keiming Yen		
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
int
gr2_cflt(void)
{

    int  i, err;

    err = 0;
    /* send command */

    msg_printf(DBG, "CFLT: Sending command to GE7....\n");


    for (i=0x3f800000; i < 0x43800000; i=i+0x00800000) {

      base->fifo[DIAG_CFLT | DIAG_CONVC1] = i;
    
      time_delay(500);

        msg_printf(DBG,"Color index = %#x, Normalized index = %#x\n",float_to_int(i),normalized_to_int(base->ge[0].ram0[0],12));
      if (normalized_to_int(base->ge[0].ram0[0],12) != float_to_int(i)) {
	err++;
        msg_printf(DBG,"Color index = %#x, Normalized index = %#x\n",float_to_int(i),normalized_to_int(base->ge[0].ram0[0],12));
      }
    }
   return(err); 
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/
void
Clear_GERam(int genum, int cnt)
{
    register	i;

    for(i=0; i<cnt;i++) 
	base->ge[genum].ram0[i] = 0;
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

void
time_delay(unsigned int ticks)
{
    DELAY(ticks);
}

/***************************************************************************
*   Routine:
*
*   Purpose:			
*
*   History:
*	    4/15/91	    Eric Manghise	Original Draft
*
*   Dependencies:  
*
*   Notes:
*
****************************************************************************/

void
show_ram0_hex(int size)
{
    int cnt, i, cell;


    for(i = 0; i < numge; i++)
    {
       for(cnt = 0,cell =0; cnt < size/8; cnt++)
	{
	    printf("ge %d Addr: %x     %x %x %x %x %x %x %x %x\n", i, cell, 
		    base->ge[i].ram0[cell], base->ge[i].ram0[cell+1],
		    base->ge[i].ram0[cell+2], base->ge[i].ram0[cell+3],
		    base->ge[i].ram0[cell+4], base->ge[i].ram0[cell+5],
		    base->ge[i].ram0[cell+6], base->ge[i].ram0[cell+7]);
	    cell += 8;
	}

    }
}

int
check_value(int genum, int index, int value)
{
 
	int newval;
   
	if( (newval = base->ge[genum].ram0[index]) != value)
	{ 
	    msg_printf(DBG, "Expected value %x, actual = %x\n",value, newval);
	    return(1);    
	} /* else */ return (0);
}

/***************************************************************************
*   Routine: int_to_float
*
*   Purpose: Convert a int number to IEEE floating poing number  
*
*   History:
*	    5/22/91	    Keiming Yen		Original Draft
*
*   Dependencies:  
*
*   Notes:
*
**************************************************************************/
int
int_to_float(int num)
{
	unsigned int exp,mant;
	int sign,i;

	if (num == 0) return 0;
 	if (num < 0) { 
	  sign = 0x80000000; 
          num = ~num + 1; /* 2's compliment */ 
	} else sign = 0;
	for (exp=127,mant=num,i=0; mant !=0 ;  mant = mant>> 1, ++i)
	exp++;
	mant = num;
	return(sign | exp <<23 | (mant << (33-i)) >> 33-i);
}


/***************************************************************************
*   Routine: float_to_int
*
*   Purpose: Convert a IEEE floating poing number back to integer 
*
*   History:
*	    5/22/91	    Keiming Yen		Original Draft
*
*   Dependencies:  
*
*   Notes:
*
**************************************************************************/
int float_to_int(int num)
{
int sign, mant, exp, result;
	if (num == 0) return 0;


	mant = num & 0x7fffff;
	exp = (num >> 23) & 0xff;
	sign = num >> 31;
	result = power(2,(exp-127));
	for (mant= mant <<9,exp--;mant !=0;mant = mant <<1,exp--) 
	  if (mant & 0x80000000 ) result += power(2,exp-127);
	if (sign == 0) return(result);
	 else return (-result);
}

int
power(int x, int n)
{
	int p;

	for(p=1;n > 0;--n)
		p = p * x;

	return(p);
}

/***************************************************************************
*   Routine: normalized_to_int
*
*   Purpose: Convert a 8-bit normalized number back to integer 
*
*   History:
*	    5/22/91	    Keiming Yen		Original Draft
*
*   Dependencies:  
*
*   Notes:
*
**************************************************************************/
int normalized_to_int(int num, int norm_bits)
{
	int sign, mant, exp, result;

	if (num == 0) return 0;

	mant = num & 0x7fffff;
	exp = ((num >> 23) & 0xff) + norm_bits;
	sign = num >> 31;
	result = power(2,(exp-127));
	for (mant= mant <<9,exp--;mant !=0;mant = mant <<1,exp--) 
	  if (mant & 0x80000000 ) result += power(2,exp-127);
	return(power(-1,sign)*result);
}

