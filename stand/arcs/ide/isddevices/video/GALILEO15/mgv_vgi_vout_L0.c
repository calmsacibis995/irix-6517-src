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
 *	Filename:  mgv_vgi_vout_L0.c
 *	Purpose:  to test the video output path by sending a series of test
 *		  pattern frames from a VGI1 DMA transfer to the video frame
 *		  buffers and video out (a display monitor)
 * 		
 *		  Each pattern will be a full frozen frame, and will be
 *		  displayed for N seconds. The routine concludes when the
 *		  last pattern is displayed on the screen.
 *
 *		  The operator is expected to visually inspect the patterns
 * 		  at each screen location for errors. The 'seconds' term should
 *		  be set for a long enough period for visual inspection.
 *
 *		  There is no automatic determination of pass/fail.
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.6 $
 *
 */

#include "mgv_diag.h"


/*************************************************************************
*
*	Function: _mgv_SetupSendDMA()
*
*
**************************************************************************/
int _mgv_SetupSendDMA(	unsigned char ch_sel, unsigned char tv_std,
			unsigned char format, unsigned int* send_field,
			unsigned int freeze)
{
unsigned char reg_val;

	reg_val = (ch_sel == ONE) ? VGI1_CHANNEL_1 : VGI1_CHANNEL_2; 
	_mgv_setup_VGI1DMA(reg_val, tv_std, format, 2, 
	 	(unsigned char*)send_field, VGI1_DMA_READ, 
		freeze ? freeze:FREEZE_DISABLE);

	msg_printf(DBG,"\n0:%2x 1:%2x 1000:%2x 1001:%2x",
		*send_field,*(send_field+1),
		*(send_field+1000), *(send_field+1001) );
msg_printf(DBG,"DMA ");
	if( _mgv_VGI1DMA() )
	{
		_mgv_VGI1DMADispErr();
		return(-1);
	}
msg_printf(DBG,"DMA_done ");
	return(0);
}

/**************************************************************************
*
*	Function: _mgv_vgi_vout_gonogo
*
*	Description: see description for vout_L0.c above
*
*	Inputs: channel select (1 (default) or 2)
*		seconds displayed for each pattern (5 default)
*		tv standard (CCIR525 (default), CCIR625, SQ525, SQ625)
*
*	Return: None. Visual inspection of display monitor for pass/fail.
*
**************************************************************************/
 
int _mgv_vgi_vout_gonogo( unsigned char ch_sel, unsigned char tv_std, unsigned char format )
{
unsigned long i, buf_size;
unsigned char pattern, reg_val;
unsigned int *send_field;
extern int xsize[], ysize[];

/* Put as many working patterns in this array as you want displayed on the screen.  However ALWAYS end the array with 0xFF!! */
vout_patterns_t vid_gonogo[] = {ALT01_PTRN, ALT10_PTRN,
				WALK1_PTRN, WALK0_PTRN, 0xFF };

#if 0
	/* reset Galileo board */
	msg_printf(JUST_DOIT,"\nGALILEO 1.5 BOARD BEING RESET- PLEASE WAIT\n");
	_mgv_reset(1);
	delay( 3000 );
	msg_printf(JUST_DOIT,"\nRESET COMPLETE\n");
#endif

    	/* set default timing */
	GAL_IND_W1 (mgvbase, GAL_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER));
	GAL_IND_W1 (mgvbase, GAL_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER));

	/* setup VBAR according to inchan and outchan */
		reg_val = GAL_OUT_CH1 | GAL_VGI1_CH1_OUT_SHIFT;
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);
		reg_val = GAL_OUT_CH2 | GAL_VGI1_CH2_OUT_SHIFT;
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);

		reg_val = GAL_OUT_CH1_GIO | GAL_BLACK_REF_IN_SHIFT;
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);
		reg_val = GAL_OUT_CH2_GIO | GAL_BLACK_REF_IN_SHIFT;
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);



/* 	allocate memory space for a video field array(word-wide), 
	10bitYUV 4:2:2, non-Alpha  
	ie: 720pxls*2words/pxl*2bytes/word * 486 lines/frame / 2 fields/frame 
*/
	buf_size = ( xsize[tv_std]*2*(sizeof(char)) * ysize[tv_std]/2 );
	send_field = (unsigned int*)get_chunk( buf_size + mgvbase->pageSize);
	if(send_field == NULL)
	{
		msg_printf(ERR,"\nCould not allocate memory for field array");
		return(-1);
	}
	send_field = (unsigned int*)_mgv_PageAlign((unsigned char*)send_field );

	/* Now display each pattern again for one full frame */	
	for(pattern = 0; vid_gonogo[pattern] != 0xFF; pattern++)
	{
	msg_printf(DBG,"pattern:%d ",pattern);
		/* create a 10-bit video field array */
		_mgv_make_vid_array(send_field, TENBIT, 
				test_values[ vid_gonogo[pattern] ], /* Y */
				test_values[ vid_gonogo[pattern] ], /* U */
				test_values[ vid_gonogo[pattern] ], /* V */
				0,				    /* A */	
				xsize[tv_std],
				ysize[tv_std]/2,
				0,
				0 );

msg_printf(DBG,"\nFirst 100 words of video pattern array\n" );
for(i = 0; i < 100; i++)
	msg_printf(DBG," %d:%4x ",i, *(send_field+i) );


		msg_printf(JUST_DOIT,"\nExamine %s pattern on Video Out Ch. %d monitor", asc_bit_pat_p[vid_gonogo[pattern]], ch_sel);

		if( _mgv_SetupSendDMA(ch_sel,tv_std,format,send_field,300) )
			return(-1);

		
	}	  /* for image block   */

	return (0);
}

/***************************************************************************
*
*	Function: mgv_vgi_vout_L0
*
*	Description: see top header description
*
****************************************************************************/

int mgv_vgi_vout_L0( int argc, char *argv[] )
{
unsigned char chan	= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
int  sec		= 0;
long mseconds		= MSEC_PER_PATTERN_DEFAULT;
tv_std_t std		= CCIR_525;
tv_std_t tmp_std	= CCIR_525;
unsigned char fmt	= MGV_VGI1_FMT_YUV422_8;
unsigned char status 	= 0;

	/* check for valid input parameters */
	if (argc > 1 )
	{
msg_printf(DBG,"Entering argc\n");

		/* if valid channel, change default */
		chan = atoi( argv[CHAN_ARG] );
		if( (chan == 1) || (chan == 2) )
			channel = chan;

		/* if tv_std other than CCIR525, change default */
		tmp_std = atoi( argv[STD_ARG] );
		if( (tmp_std > 0) && (tmp_std < 5) )
			std = tmp_std;

		/* if valid number of seconds, change default */
		sec  = atoi( argv[SEC_ARG] );
		if( (sec > 0 ) && (sec < MAX_SEC_PER_PATTERN) )
			mseconds = sec*1000;

	}

	msg_printf(SUM,"\n\tVGI1 10-BIT %s VIDEO OUT PATTERN TEST FOR CHANNEL %d\
			\nEXAMINE ALL DISPLAYED PATTERNS.\
			\nCOMPARE WITH EXPECTED STANDARDS.\n\n",
			asc_tv_std_p[ std ], channel);


	/* Do the Video Out GONOGO test */
	if( std == ALL_STD )
	{
		for( std = 0; std < 4; std++)
		{
			status = _mgv_vgi_vout_gonogo( ONE, std, fmt );
			if( status )
				return( status );
			delay( mseconds );
			status = _mgv_vgi_vout_gonogo( TWO, std, fmt );
			if( status )
				return( status );
			delay( mseconds );
		}	
	}
	else
		status = _mgv_vgi_vout_gonogo( channel, std, fmt );

	msg_printf(SUM,"\n\nEND OF VGI1 10-BIT %s VIDEO OUTPUT PATTERN TEST FOR CHANNEL %d.\n", asc_tv_std_p[ std ], channel);

	return( status );
}

