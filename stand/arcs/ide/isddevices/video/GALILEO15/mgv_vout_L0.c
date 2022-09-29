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
 *	Filename:  mgv_vout_L0.c
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
 *      $Revision: 1.7 $
 *
 */

#include "mgv_diag.h"


/**************************************************************************
*
*	Function: vout_gonogo
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
 
int vout_gonogo( unsigned char ch_sel, unsigned char tv_std )
{
int i;
unsigned char tmp, ab_sys, cc_sys, image_blk;
unsigned long buf_size;
unsigned int  num_lines;
unsigned char mode_offset = 12;
unsigned char num_patterns = 4;
unsigned char *send_field;
extern int xsize[], ysize[];

/* Put as many working patterns in this array as you want displayed on the screen.  However ALWAYS end the array with 0xFF!! */
vout_patterns_t vid_gonogo[] = { ALT01_PTRN, ALT10_PTRN,
				 WALK0_PTRN, WALK1_PTRN, 0xFF };


#if 0
	/* reset Galileo board */
	msg_printf(JUST_DOIT,"\nGALILEO 1.5 BOARD BEING RESET- PLEASE WAIT\n");
	_mgv_reset(1);
	delay( 3000 );
	msg_printf(JUST_DOIT,"\nRESET COMPLETE\n");
#endif

	/* set Crosspoint sw for REFBLACK -> both CC1 inputs (pixels, alpha/pixels) */
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x70);
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x71);

	/* set Genlock reference to be loop-thru */
	GAL_IND_W1(mgvbase, GAL_REF_CTRL, 1);

	/* set default for VCXO free run frequency high and low bytes */
	GAL_IND_W1(mgvbase, GAL_VCXO_LO, 0x00);
	GAL_IND_W1(mgvbase, GAL_VCXO_HI, 0x08);

	/* set default for black timing */
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_LO, 0x00);
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_HI, 0x02);

	if(ch_sel == ONE)
	{
		/* Set Ch1 Vid Out to pass-through */
		GAL_IND_W1(mgvbase, GAL_CH1_CTRL, 0x01 );

		/* Set Ch2 Vid Out to blank */
		GAL_IND_W1(mgvbase, GAL_CH2_CTRL, 0x02 );

		/* set xp from CC1 output (pixels) to vid out #1 */
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x88);
	}
	else
	{
		/* Set Ch1 Vid Out to blank */
		GAL_IND_W1(mgvbase, GAL_CH1_CTRL, 0x02 );

		/* Set Ch2 Vid Out to pass-through */
		GAL_IND_W1(mgvbase, GAL_CH2_CTRL, 0x01 );

		/* Set CC1 output control to pass Ch2 chroma */
		GAL_IND_W1(mgvbase, GAL_CC1_IO_CTRL6, 0x80);

		/* set xp from CC1 output (pxls/alpha) to vout #2 */
		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x97);
	}

	/* grChC=vidin, exp. port 0= fbout */
	CC_W1(mgvbase, CC_INDIR1, 55 );
	CC_W1(mgvbase, CC_INDIR2, 0x30 );

	/* DONE IN MGV_INIT--fbin=vidin, vidout=fbout */
	CC_W1(mgvbase, CC_INDIR1, 56 );
	CC_W1(mgvbase, CC_INDIR2, 0x30 );

	/* fbin from CPU, fbout to vid */
	CC_W1(mgvbase, CC_INDIR1, 48 );
	CC_W1(mgvbase, CC_INDIR2, 0x02 );

	/* enable expansion port 0 & video input port 0, wait until both locked */
	if( _mgv_CC1PollReg( 38, 0xA0, 0xAF, 10, &tmp) )
		return(0x1);


	/* set AB1, CC1, & GAL15 System Control direct registers */
	cc_sys = 0x04;			/* exp. port 0 = output */
	AB_R1(mgvbase, AB_SYSCON, ab_sys);


	switch( tv_std )
	{
		case SQ_525:
			cc_sys &= 0xFC;  /* set to NTSC, SQ mode */
			ab_sys |= 0x01;
			mode_offset = 12;
			break;
		case CCIR_625:
			cc_sys |= 0x03;	 /* set to PAL, CCIR601 mode */
			ab_sys &= 0xFE;
			mode_offset = 19;
			break;
		case SQ_625:
			cc_sys |= 0x01;	 /* set to PAL mode */
			cc_sys &= 0xFD;  /* set to SQ mode */
			ab_sys &= 0xFE;
			mode_offset = 19;
			break;
		case  CCIR_525:
		default:		 /* CCIR_525 mode */
			cc_sys &= 0xFE;	 /* set to NTSC mode */
			cc_sys |= 0x02;  /* set to CCIR601 mode */
			ab_sys |= 0x01;
			mode_offset = 12;
			break;
	}

	CC_W1(mgvbase, CC_SYSCON, cc_sys);
	AB_W1(mgvbase, AB_SYSCON, ab_sys);

	/* set tv std */
	GAL_IND_W1(mgvbase, GAL_TV_STD, tv_std); 


	/* commence video double buffering on next vertical */
	CC_W1(mgvbase, CC_SYSCON, (0x40 | cc_sys) );


/* Determine the number of patterns, and divide the vertical screen length/pattern accordingly */

	num_patterns = i = 0;
	while( vid_gonogo[ i++ ] != 0xFF)
		++num_patterns;

/* allocate memory space for a video field array, 8bitYUV 4:2:2, non-Alpha */
num_lines = ysize[tv_std]/(num_patterns*2);
buf_size = (xsize[tv_std]*2*sizeof(char)) * num_lines;
send_field = (unsigned char*)get_chunk( buf_size );
msg_printf(DBG,"\nPATTERN BUFFER SIZE: %d", buf_size);


	for(image_blk = 0; image_blk < num_patterns; image_blk++)
	{
	msg_printf(DBG,"\nimage blk:%d #patterns:%d ", image_blk,num_patterns);

		_mgv_make_vid_array((unsigned int *)send_field, EIGHTBIT, 
				test_values[ vid_gonogo[image_blk] ], /* Y */
				test_values[ vid_gonogo[image_blk] ], /* U */
				test_values[ vid_gonogo[image_blk] ], /* V */
				0,				      /* A */	
				xsize[tv_std],
				num_lines*2,
				0,
				0 );

msg_printf(DBG,"\nPATTERN VALUES(every 1000 bytes)");
for(i=0; i < (xsize[tv_std]*2*sizeof(char) * num_lines);i+=1000)
	msg_printf(DBG,"%5d:%2x ",i,*(send_field+i));


	/* Output the vid data array to the CC1 frame buffer */
	
		for(i=0; i < 2;i++)
		{
		msg_printf(JUST_DOIT,"\nTV Std: %s\tCC1 Field: %s\tPattern: %s",
			asc_tv_std_p[ tv_std ],
			i ? "ODD":"EVEN",
			asc_vid_pat_p[ vid_gonogo[ image_blk ] ] );


		msg_printf(DBG,"\nCC1 %s FIELD PARAMETERS:yoff= %d ylen= %d",
			i ? "ODD":"EVEN",
			(ysize[tv_std]*image_blk/(num_patterns*2) )+mode_offset, 
			ysize[tv_std]/num_patterns);

		write_CC1_field(	
			0,
			xsize[tv_std]*2,
			(ysize[tv_std]*image_blk/(num_patterns*2) )+mode_offset,
			num_lines,
			i ? ODD_FIELD:EVEN_FIELD,  
			send_field);
		}

	}	  /* for image block   */

	return(0);
}

/***************************************************************************
*
*	Function: mgv_vout_L0
*
*	Description: see top header description
*
****************************************************************************/

int mgv_vout_L0( int argc, char *argv[] )
{
unsigned char chan	= 0;
unsigned char status 	= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
int  sec		= 0;
long mseconds		= MSEC_PER_PATTERN_DEFAULT;
tv_std_t std		= CCIR_525;
tv_std_t tmp_std	= CCIR_525;

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

	msg_printf(JUST_DOIT,"\n\tCC1 FRAME BUFFER 8-BIT VIDEO OUTPUT PATTERN TEST FOR CHANNEL %d\n",channel);

	/* Do the Video Out GONOGO test */
	if( std == ALL_STD )
	{
		for( std = 0; std < 4; std++)
		{
			status = vout_gonogo( ONE, std ); 
			if (status)
				return(status);
			delay( mseconds );
			status = vout_gonogo( TWO, std );
			if (status)
				return(status);
			delay( mseconds );
		}	
	}
	else
		status = vout_gonogo( channel, std );
	
	msg_printf(SUM,"\n\nEND OF CC1 8-BIT VIDEO OUTPUT PATTERN TEST FOR CHANNEL %d.\
			\nEXAMINE ALL DISPLAYED PATTERNS.\
			\nCOMPARE WITH EXPECTED STANDARDS.\n\n",channel);
	return(status);

}

