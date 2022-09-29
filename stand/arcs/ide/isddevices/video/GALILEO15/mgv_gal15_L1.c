/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *	Filename:  mgv_reg_L1.c
 *	Purpose:  to perform write/read testing of all indirect G1.5 
 *		  registers.  This will consist of writing
 *		  and confirming (reading) each of six sets of data
 *		  (all 1s, all 0s, alt10, alt01, walk1s, walk 0s) to each
 *		  register.
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.5 $
 *
 */

#include "mgv_diag.h"

/*
**  External Variables and Function Declarations
*/
extern int  load_tests( int*, int );
extern int test_gal15_rw1(void );
extern void report_results( int );

/*****************************************************************************
*
*		Function: do_reg_L1( void )
*
*		Description: A test request bit pattern is set, and an array filled
*					with all registers to be tested.  The test requests and 
*					registers are loaded into the batch_test[] test buffer 
*					and the tests are performed.  Reporting of results is
*					NOT done in this routine.
*
*		Return:  0 	if registers and test requests loaded successfully
*				 -1 if above had errors in loading
*
******************************************************************************/
int do_reg_L1( void )
{
	int status;
	/* Bit patterns for batch test requests */
	int test_req = TEST_A1 | TEST_A0 | TEST_10 | TEST_01 | TEST_W1 | TEST_W0;

	/* Register indirect addresses to be tested. Note 0 to end sequence */	
	int		vo_L1_reg_ut[] = { 
						GAL_TV_STD,			GAL_VIN_CTRL,
						GAL_CC1_IO_CTRL5,	GAL_CC1_IO_CTRL6,
						GAL_CH1_CTRL,		GAL_CH2_CTRL,
						GAL_VCXO_LO,		GAL_VCXO_HI,
						GAL_REF_CTRL,		GAL_TRIG_CTRL,
						GAL_ADC_MODE,
						-1 };
	
	/*  load registers and test requests */
	status = load_tests( vo_L1_reg_ut, test_req ) ;

	/* test all registers stored in batch_test[] test matrix */
	if( !status )
	{
		test_gal15_rw1( );
		return( 0 );
	}
	else  return(-1);
}


/****************************************************************************
*
*		Function: mgv_reg_L1( int, char* )
*
*		Description: see description at top of file
*
*****************************************************************************/

int mgv_reg_L1( )
{
	int status;
	msg_printf(JUST_DOIT, "\nLEVEL 1 BIT PATTERN TEST on GAL1.5 INDIRECT R/W REGISTERS");
	msg_printf(JUST_DOIT, "\n\t-ALLONES, ALLZEROES, ALT10, ALT01, WALK1, WALK0-");
	report_results( RESET_TSTBUF );
	status = do_reg_L1( );
	if( !status )
	{
		report_results( DSPY_TSTBUF );
		msg_printf(JUST_DOIT,"\n");
	}	
	else
		msg_printf(ERR, "Error in loading reg_L1 test\n");

	return(status);
}

