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
 *	Filename:  mgv_vin_L1.c
 *	Purpose:  to perform write/read testing of the Video Input 
 *		  registers.  This will consist of writing and
 *		  confirming (reading) each of six sets of data
 *		  (all 1s, all 0s, alt10, alt01, walk1s, walk 0s)
 *		  to each register.
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
extern int test_gal15_rw1(void);
extern void report_results( int );





int do_vin_L1( void )
{
	int status=0;
    int test_req = TEST_A1 | TEST_A0 | TEST_10 | TEST_01 | TEST_W1 | TEST_W0;


	/* Register indirect addresses to be tested. Note 0 to end sequence */	
	int	vo_L1_reg_ut[] = { GAL_VIN_CTRL, -1 };
	
	status = load_tests( vo_L1_reg_ut, test_req ) ;

	if( !status )
		test_gal15_rw1( );
	
	return(status);
}


int mgv_vin_L1(int argc, char *argv[] )
{
int status=0;
	msg_printf(JUST_DOIT,"VIN LEVEL 1 REGISTER BIT PATTERN TEST\n");
	report_results( RESET_TSTBUF );
	status = do_vin_L1( );
	if( !status )
		report_results( DSPY_TSTBUF );

	return(status);
}


