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
 *	Filename:  vid_diags.c
 *	Purpose:  to test the video output path by sending a series of test
 *		  pattern frames from the CC1 frame buffer to the video frame
 *		  buffers and video out (a display monitor)
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
 *      $Revision: 1.1 $
 *
 */

#include "mgv_diag.h"


/*
**  External Variables and Function Declarations
*/

extern char send_field_odd[];
extern char send_field_even[];


void vid_diags( int argc, char *argv[] )
{
int    temp				= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
int location_seconds	= SEC_PER_LOCATION_DEFAULT;
int motion_seconds		= SEC_PER_MOTION_DEFAULT;
int vid_diag_ptrn		= VID_DIAG_PTRN_DEFAULT;

	/* check for valid input parameters */
	if (argc > 1 )
	{
		/* if valid channel, change default */
		temp = atoi( argv[CHAN_ARG] );
		if( (temp == 1) || (temp == 2) )
			channel = (unsigned char)temp;

		/* if valid number of seconds, change default */
		temp  = atoi( argv[LOC_SEC_ARG] );
		if( (temp > 0 ) && (temp < MAX_SEC_PER_PATTERN) )
			location_seconds = temp;

		/* if valid number of seconds, change default */
		temp  = atoi( argv[MOT_SEC_ARG] );
		if( (temp > 0 ) && (temp < MAX_SEC_PER_MOTION) )
			motion_seconds = temp;

		/* if valid vid diag pattern, change default */
		temp  = atoi( argv[VID_ARG] );
		if( (temp > 0 ) && (temp < NUM_VID_PATTERNS) )
			vid_diag_ptrn = temp;

	}

	vout_diag_display( channel, seconds, motion_seconds, vid_diag_ptrn);

}


/**************************************************************************
*
*	Function: vout_diag_display
*
*	Description: see description for vidout_L0.c above
*
*	Inputs: channel select (1 (default) or 2)
*		seconds displayed for each location
*		seconds displayed for each pattern or motion
*
*	Return: None. Visual inspection of display monitor for pass/fail.
*
**************************************************************************/

 
void vout_diag_display( unsigned char ch_sel, int sec_per_location, int mot_sec)
{
int i;
unsigned char image_blk, line_width = 2, first_time = TRUE;
unsigned char ptrn_ctr	= 0;
ulong motion_time		= 0;
ulong location_time		= 0;

extern make_vid_array(unsigned int*, unsigned char, /* video buffer, #bits */
					unsigned char, unsigned char, 	/* Y, U */
					unsigned char, unsigned char,	/* V, A */
					int, int, int , int);		/* h len, y len, h off, y off */

	/* set Crosspoint sw for CC1 to Video Out #1 */
	GAL_IND_W1(mgvbase, GAL_MUX_DIR, BYTE_ZERO | GAL_IN_CH1_CC1 | GAL_OUT_CH1 );


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
	GAL_IND_W1(mgvbase, GAL_FLORA_LO, BYTE_ZERO );		/* ref black = 0    */
	GAL_IND_W1(mgvbase, GAL_FLORA_HI, BYTE_ZERO );     /* ref black = 0    */
	GAL_IND_W1(mgvbase, GAL_ADC_MODE, GL_CH0 );        /* VCXO conn Ch. 0  */


	for(image_blk = 0; image_blk < IMG_BLK_NUM; image_blk++)
	{
		x_loc = y_loc = 0;
		first_time = TRUE;
		location_time = TICKS_PER_SECOND * sec_per_location; 

		while( location_time-- )
		{
			switch( vid_pattern )
			{
				case VID_WALK1:
					/* make background */
					make_vid_array(send_field_odd, TEN, 
									WALK1, WALK1, WALK1, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0], 
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
					make_vid_array(send_field_even, TEN, 
									WALK1, WALK1, WALK1, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
					break;		

				case VID_WALK0:
					/* make background */
					make_vid_array(send_field_odd, TEN, 
									WALK0, WALK0, WALK0, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
					make_vid_array(send_field_even, TEN, 
									WALK0, WALK0, WALK0, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
					break;		

				case VID_VLINE:
					/* make background */
					if( first_time == TRUE )
					{
						make_vid_array(send_field_odd, TEN, 
									WHITE, WHITE, WHITE, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
						make_vid_array(send_field_even, TEN, 
									WHITE, WHITE, WHITE, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
						first_time = FALSE;
					}

						/* make vertical line at y_loc */	
						make_vid_array(send_field_odd, TEN, 
									BLACK, BLACK, BLACK, 0, 
									tv_std_length[tv_std][1], 
									line_width,
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
						make_vid_array(send_field_even, TEN, 
									BLACK, BLACK, BLACK, 0, 
									tv_std_length[tv_std][1], 
									line_width,
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] + y_loc++);
					break;
				case VID_HLINE:
					/* make background */
					if( first_time == TRUE )
					{
						make_vid_array(send_field_odd, TEN, 
									WHITE, WHITE, WHITE, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
						make_vid_array(send_field_even, TEN, 
									WHITE, WHITE, WHITE, 0, 
									tv_std_length[tv_std][1], 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1],
									tv_std_loc[tv_std][image_blk][0] );
						first_time = FALSE;
					}
				
					/* make horizontal line at x_loc */	
						make_vid_array(send_field_odd, TEN, 
									BLACK, BLACK, BLACK, 0, 
									line_width, 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1] + x_loc++,
									tv_std_loc[tv_std][image_blk][0] );
						make_vid_array(send_field_even, TEN, 
									BLACK, BLACK, BLACK, 0, 
									line_width, 
									tv_std_length[tv_std][0],
									tv_std_loc[tv_std][image_blk][1] + x_loc++,
									tv_std_loc[tv_std][image_blk][0] );
					break;
				case VID_BOX:
				case VID_XBOX:
				default:
					break;
			}

		motion_time = TICKS_PER_SEC * mot_sec;

		/* Repeat pattern display until motion_time expires */
		while( motion_time-- )
		{
			write_CC1_field(	
				tv_std_loc[tv_std][image_blk][1], 
				tv_std_length[tv_std][1],
				tv_std_loc[tv_std][image_blk][0], 
				tv_std_length[tv_std][0],
				ODD,  
				send_field_odd );
			write_CC1_field(	
				tv_std_loc[tv_std][image_blk][1], 
				tv_std_length[tv_std][1],
				tv_std_loc[tv_std][image_blk][0], 
				tv_std_length[tv_std][0],
				EVEN,  
				send_field_even);
		}
	}
	}    
}
