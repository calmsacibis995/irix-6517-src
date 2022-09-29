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
 *	Filename:  vidout_L2.c
 *	Purpose:  see Test plan for now.
 *		  
 *		  
 * 		
 *		  Each pattern will be 1/16 of a full frame, and will be
 *		  displayed for N seconds at each of the 16 (4x4) screen
 *		  locations. The next pattern is then displayed at each
 *		  location. The routine concludes when the last pattern is
 *		  displayed at the last location. 
 *
 *		  The operator is expected to visually inspect the patterns
 * 		  at each screen location for errors. The 'seconds' term should
 *		  be set for a long enough period for visual inspection.
 *
 *		  There is no automatic determination of pass/fail.
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.2 $
 *
 */

#include "mgv_diag.h"
#define GAL_OUT_CH1             8
#define GAL_IN_CH1_CC1          (8 << 4)


/*
**  External Variables and Function Declarations
*/

extern char send_field_odd[];


/**************************************************************************
*
*	Function: vout_testptrn
*
*	Description: see description for vidout_L2.c above
*
*	Inputs: channel select (1 (default) or 2)
*		seconds displayed for each pattern (5 default)
*		tv standard (CCIR525 (default), CCIR625, SQ525, SQ625)
*
*	Return: None. Visual inspection of display monitor for pass/fail.
*
**************************************************************************/

 
void vout_testptrn( unsigned char ch_sel, int usec_per_pattern, unsigned char tv_std )
{
unsigned char image_blk;
unsigned char ptrn_ctr	= 0;
ulong ptrn_time		= 0;

	/* set Crosspoint sw for CC1 to Video Out #1 */
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, BYTE_ZERO | GAL_IN_CH1_CC1 | GAL_OUT_CH1 );


	/* set Video Output Control register  */
	if(ch_sel == ONE)
	{
		GAL_IND_W1(mgvbase, GAL_CH1_CTRL, BYTE_ZERO );  	/* sync with ref */
		GAL_IND_W1(mgvbase, GAL_CH1_HPHASE_LO, BYTE_ZERO );/* hphase1L = 0 */
		GAL_IND_W1(mgvbase, GAL_CH1_HPHASE_HI, BYTE_ZERO );/* hphase1H = 0 */
		GAL_IND_W1(mgvbase, GAL_CH2_CTRL, BYTE_ZERO | VO_BLANK_PICT );/*blnk Ch2*/
		GAL_IND_W1(mgvbase, GAL_CH2_HPHASE_LO, BYTE_ZERO );/* hphase2L = 0 */
		GAL_IND_W1(mgvbase, GAL_CH2_HPHASE_HI, BYTE_ZERO );/* hphase2H = 0 */
	}
	else
	{
		GAL_IND_W1(mgvbase, GAL_CH2_CTRL, BYTE_ZERO );  	/* sync with ref */
		GAL_IND_W1(mgvbase, GAL_CH2_HPHASE_LO, BYTE_ZERO );/* hphase1L = 0 */
		GAL_IND_W1(mgvbase, GAL_CH2_HPHASE_HI, BYTE_ZERO );/* hphase1H = 0 */
		GAL_IND_W1(mgvbase, GAL_CH1_CTRL, BYTE_ZERO | VO_BLANK_PICT );/*blnk Ch1*/
		GAL_IND_W1(mgvbase, GAL_CH1_HPHASE_LO, BYTE_ZERO );/* hphase2L = 0 */
		GAL_IND_W1(mgvbase, GAL_CH1_HPHASE_HI, BYTE_ZERO );/* hphase2H = 0 */
	}


	/* set Gen Lock Control register - free run selected  */
	GAL_IND_W1(mgvbase, GAL_REF_CTRL, BYTE_ZERO | GL_REFSRC_FRERUN);
	
	/* set remainder of Gen Lock Control register */

	GAL_IND_W1(mgvbase, GAL_VCXO_HI, BYTE_ZERO );		/* VCXO freerun = 0 */
	GAL_IND_W1(mgvbase, GAL_VCXO_LO, BYTE_ZERO );      /* VCXO freerun = 0 */
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_LO, BYTE_ZERO );		/* ref black = 0    */
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_HI, BYTE_ZERO );     /* ref black = 0    */
	GAL_IND_W1(mgvbase, GAL_ADC_MODE, GL_CH0 );        /* VCXO conn Ch. 0  */



	/* Setup data transfer from CC1 frame buffer to video out */

	while( vo_L0_ptrns[ptrn_ctr] != NULL )
	{
		/* Generate a field of YUV422 10-bit patterned data for the test */
		test_allstds((unsigned long *)send_field_odd, 422, tv_std, 
						TENBIT, vo_L0_ptrns[ptrn_ctr++]);
	
		/* Output the pattern at each of the 16 screen locations for TICKS_PER_SEC * seconds */
			/* Send the data to the CC1 ch. 1 or 2  input */
			for(image_blk = 0; image_blk < 4/*IMG_BLK_NUM*/; image_blk++)
			{
				ptrn_time = TICKS_PER_SEC * usec_per_pattern;

				/* Repeat pattern display until ptrn_time expires */
				while( ptrn_time-- )
				{
					write_CC1_field(	
						tv_std_loc[tv_std][image_blk][1], 
						tv_std_length[tv_std][1],
						tv_std_loc[tv_std][image_blk][0], 
						tv_std_length[tv_std][0],
						ODD,  
						(unsigned char *)send_field_odd );
				}
			}
	}    
}




/*****************************************************************************/


void vout_L2( int argc, char *argv[] )
{
unsigned char chan		= 0;
int    usec			= 0;
unsigned char  channel		= CHANNEL_DEFAULT;
int useconds			= MSEC_PER_PATTERN_DEFAULT;
tv_std_t std			= CCIR_525;

	/* check for valid input parameters */
	if (argc > 1 )
	{
		/* if valid channel, change default */
		chan = atoi( argv[CHAN_ARG] );
		if( (chan == 1) || (chan == 2) )
			channel = chan;

		/* if valid number of seconds, change default */
		usec  = atoi( argv[SEC_ARG] );
		if( (usec > 0 ) && (usec < MAX_SEC_PER_PATTERN) )
			useconds = usec;

		/* if tv_std other than CCIR525, change default */
		if(argv[STD_ARG] == "CCIR625")
				std = CCIR_625;
		else if(argv[STD_ARG] == "SQ525") 
				std = SQ_525;
		else if(argv[STD_ARG] == "SQ625")
				std = SQ_625;
	}

	/* Do the Video Out GONOGO test */
	vout_testptrn( channel, useconds, std );

}

