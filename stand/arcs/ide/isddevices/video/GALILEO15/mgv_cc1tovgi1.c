 /*************************************************************************
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
 *	Filename:  mgv_cc1tovgi1.c 
 *	Purpose:  to test the video output path by sending a series of test
 *		  pattern frames from the CC1 frame buffer to the video frame
 *		  buffers.
 * 		
 *		  Each pattern will be a full field, and will be displayed for
 *		  N seconds. The next pattern is then displayed. The routine
 *		  concludes when the last pattern is displayed.
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

/**************************************************************************
*
*	Function: _mgv_setup_cc1_out()
*
*
*	Return: None
*
***************************************************************************/
void _mgv_setup_cc1_out(unsigned char tv_std, 
		unsigned char* mode_y_offset,
		unsigned char* cc_sys)
{
unsigned char ab_sys, tmp;
int status;

	/* grChC=vidin, exp. port 0= fbout */
	CC_W1(mgvbase, CC_INDIR1, 55 );
	CC_W1(mgvbase, CC_INDIR2, 0x30 );

	/* DONE IN MGV_INIT--fbin=vidin, vidout=fbout */
	CC_W1(mgvbase, CC_INDIR1, 56 );
	CC_W1(mgvbase, CC_INDIR2, 0x30 );

	/* fbin from CPU, fbout to vid */
	CC_W1(mgvbase, CC_INDIR1, 48 );
	CC_W1(mgvbase, CC_INDIR2, 0x02 );

	/* enable expansion port 0 & video input port 0, 
	wait until both locked */
	status = _mgv_CC1PollReg( 38, 0xA0, 0xFF, 10, &tmp); 
	if(status)
		msg_printf(SUM,"\nCC1_Reg38:%2x ",tmp);


	/* set AB1, CC1, & GAL15 System Control direct registers */
	*cc_sys = 0x04;			/* exp. port 0 = output */
	AB_R1(mgvbase, AB_SYSCON, ab_sys);

	switch( tv_std )
	{
		case SQ_525:
			*cc_sys &= 0xFC;  /* set to NTSC, SQ mode */
			ab_sys |= 0x01;
			*mode_y_offset = 12;
			break;
		case CCIR_625:
			*cc_sys |= 0x03; /* set to PAL, CCIR601 mode */
			ab_sys &= 0xFE;
			*mode_y_offset = 19;
			break;
		case SQ_625:
			*cc_sys |= 0x01; /* set to PAL mode */
			*cc_sys &= 0xFD; /* set to SQ mode */
			ab_sys &= 0xFE;
			*mode_y_offset = 19;
			break;
		case  CCIR_525:
		default:		 /* CCIR_525 mode */
			*cc_sys &= 0xFE; /* set to NTSC mode */
			*cc_sys |= 0x02; /* set to CCIR601 mode */
			ab_sys |= 0x01;
			*mode_y_offset = 12;
			break;
	}

	CC_W1(mgvbase, CC_SYSCON, *cc_sys);
	AB_W1(mgvbase, AB_SYSCON, ab_sys);

}


/**************************************************************************
*
*	Function: _mgv_cc1tovgi1loop
*
*	Description:
*
*	Inputs:
*
*	Return: None. 
*
**************************************************************************/
 
int _mgv_cc1tovgi1loop( unsigned char ch_sel, unsigned char tv_std, unsigned format )
{
int i;
unsigned char tmp, image_blk, cc_sys;
unsigned long buf_size, num_lines;
unsigned char mode_offset = 12;
unsigned char num_patterns = 4;
unsigned char *vid_data_p, *send_field_out_p, *read_field_in_p;
extern int xsize[], ysize[];

/* Put as many working patterns in this array as you want displayed on the screen.  However ALWAYS end the array with 0xFF!! */
vout_patterns_t vid_gonogo[] = { WALK1_PTRN, WALK0_PTRN, ALT01_PTRN, ALT10_PTRN, 0xFF };


#if 0
	/* reset Galileo board */
	msg_printf(JUST_DOIT,"\nGALILEO 1.5 BOARD BEING RESET- PLEASE WAIT\n");
	_mgv_reset(1);
	delay( 3000 );
	msg_printf(JUST_DOIT,"\nRESET COMPLETE\n");
#endif
        /* set tv standard */
        GAL_IND_W1(mgvbase, GAL_TV_STD, tv_std);

        /* set vin Ctl to autophase on, truncate both CHs to 8 bits */
        GAL_IND_W1(mgvbase, GAL_VIN_CTRL, 0x30);

	/* set Crosspoint sw for REFBLACK -> both CC1 inputs (pixels, alpha/pixels) */
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x70);
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x71);

	/* set Genlock reference to be passthru */
	GAL_IND_W1(mgvbase, GAL_REF_CTRL, 1);

	/* set default for VCXO free run frequency high and low bytes */
	GAL_IND_W1(mgvbase, GAL_VCXO_LO, 0x00);
	GAL_IND_W1(mgvbase, GAL_VCXO_HI, 0x08);

	/* set default for black timing */
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_LO, 0x00);
	GAL_IND_W1(mgvbase, GAL_REF_OFFSET_HI, 0x02);

	/* Set CC1 output control to pass Ch2 chroma */
	GAL_IND_W1(mgvbase, GAL_CC1_IO_CTRL6, 0x80);


	/* CHANNEL BASED OPTIONS */
	
	/*Set active channel to pass-through,pass chroma,non-freeze,non-blank*/
        tmp = (ch_sel == ONE) ? GAL_CH1_CTRL : GAL_CH2_CTRL;
	GAL_IND_W1(mgvbase, tmp, 0x01 );

	/* Set inactive channel to blank */
        tmp = (ch_sel == ONE) ? GAL_CH2_CTRL : GAL_CH1_CTRL;
	GAL_IND_W1(mgvbase, tmp, 0x02 );

	/* set xp from CC1 output (pixels) to vid out #1 */
        tmp = (ch_sel == ONE) ? 0x98 : 0x97;
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, tmp);

        /* if CH1 set vin#1 to main memory through VGI1A */
        /* if CH2 set vin#2 to main memory through VGI1B */
        tmp = (ch_sel==ONE) ?  0x15 : 0x36;
        GAL_IND_W1(mgvbase, GAL_MUX_ADDR, tmp);

	/* set ref black to opposite VGI1 channel ie: if 1 blk->B,2..blk->A */
        tmp = (ch_sel == ONE) ? 0x76 : 0x75;
        GAL_IND_W1(mgvbase, GAL_MUX_ADDR, tmp);





	_mgv_setup_cc1_out(tv_std, &mode_offset, &cc_sys);


/* Determine the number of patterns, and divide the vertical screen length/pattern accordingly */

	num_patterns = i = 0;
	while( vid_gonogo[ i++ ] != 0xFF)
		++num_patterns;

/* allocate memory space for one video field array for video out (8bitYUV 4:2:2, non-Alpha), and one video field array read in */
num_lines = ysize[tv_std];
buf_size = (xsize[tv_std]*2*sizeof(char)) * num_lines;
vid_data_p = (unsigned char*)get_chunk( buf_size + mgvbase->pageSize );
if( vid_data_p == NULL )
{
	msg_printf(ERR,"\nCould not allocate memory for video pattern arrays.");
	return(-1);
}
/* first half of allocated mem. space is for field output array */
send_field_out_p = vid_data_p;

/* last half is for frame input array; VGI1DMA op requires page alignment */
read_field_in_p  = vid_data_p + (buf_size/2);
read_field_in_p = _mgv_PageAlign( read_field_in_p );

msg_printf(DBG,"\nPATTERN BUFFER SIZE: %d\nvid_data_p:%x\
    send_field_out_p:%x    read_field_in_p:%x",
	buf_size, vid_data_p, send_field_out_p, read_field_in_p);




	/* commence video double buffering on next vertical */
	CC_W1(mgvbase, CC_SYSCON, (0x40 | cc_sys) );

	for(image_blk = 0; image_blk < num_patterns; image_blk++)
	{
	msg_printf(DBG,"\nimage blk:%d #patterns:%d ", image_blk,num_patterns);

		_mgv_make_vid_array((unsigned int *)send_field_out_p, EIGHTBIT, 
				test_values[ vid_gonogo[image_blk] ], /* Y */
				test_values[ vid_gonogo[image_blk] ], /* U */
				test_values[ vid_gonogo[image_blk] ], /* V */
				0,				      /* A */	
				xsize[tv_std],
				ysize[tv_std],
				0,
				0 );

	/* Output the vid data array to the CC1 frame buffer */

	msg_printf(JUST_DOIT,"\n\nWriting pattern to CC1 and video output channel %d", ch_sel);

		/* write fields twice to avoid CC1 byte48 data corruption */
		for(i=0; i < 4;i++)
		{
		msg_printf(JUST_DOIT,"\nTV Std: %s\tCC1 Field: %s\tPattern: %s",
			asc_tv_std_p[ tv_std ],
			(i%2) ? "ODD":"EVEN",
			asc_vid_pat_p[ vid_gonogo[ image_blk ] ] );

		write_CC1_field(	
			0,
			xsize[tv_std]*2, /* #of bytes, not pixels */
			mode_offset,
			ysize[tv_std]/2, /* num_lines is 2 fields worth now */
			(i%2) ? ODD_FIELD:EVEN_FIELD,  
			send_field_out_p);
		}

		/* One frame is displayed, 
			ready to test loopback read of one field */

		/* check video input status (ie: confirm cable is connected) */
        	if( _mgv_check_vin_status( ch_sel, tv_std ) )
                	return( -1 );

        	/* set vin Ctl to autophase off to do VGI1 DMA */
        	GAL_IND_R1(mgvbase, GAL_VIN_CTRL, tmp);
        	GAL_IND_W1(mgvbase, GAL_VIN_CTRL, (tmp | 0x03) );

        	/* load parameters into mgvbase for DMA operation */
		/* pointer is for upper half of vid_data memory space */
		msg_printf(DBG,"\nsetting up parameters for DMA");
        	tmp = (ch_sel == ONE) ? VGI1_CHANNEL_1 : VGI1_CHANNEL_2;
        	_mgv_setup_VGI1DMA(tmp, tv_std, format, 1,
                		read_field_in_p, VGI1_DMA_WRITE, 0);

		/* do the vidtomem DMA */
		msg_printf(JUST_DOIT,"\nReading the loopback video output at video input channel %d", ch_sel);
		msg_printf(DBG,"\ndoing vidtomem DMA");
		if( _mgv_VGI1DMA() )
		{
			_mgv_VGI1DMADispErr();
			return(-1);
		}

		/* compare the field sent out, with the field read in */

#if 0
		rd_CC1_field(	
			0,
			xsize[tv_std]*2, /* #of bytes, not pixels */
			mode_offset,
			ysize[tv_std]/2, /* num_lines is 2 fields worth now */
			EVEN_FIELD,  
			send_field_out_p);
#endif

#if 0
msg_printf(SUM,"\n");
for(i=0;i < 100;i++)
	msg_printf(SUM," %d:%2x/%2x ", i, *(send_field_out_p+i),  *((unsigned char*)(read_field_in_p+i)) );
#endif

		if (_mgv_compare_byte_buffers(ysize[tv_std]/2, 
					ysize[tv_std]/2,
					send_field_out_p,
					read_field_in_p,
					0, 0))
			return (-1);


	}	  /* for image block   */

	return (0);
}

/***************************************************************************
*
*	Function: mgv_cc1tovgi1.c
*
*	Description: see top header description
*
****************************************************************************/

int mgv_cc1tovgi1( int argc, char *argv[] )
{
unsigned char chan	= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
unsigned char   format  = MGV_VGI1_FMT_YUV422_8;
int  sec		= 0;
int status;	
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

	msg_printf(JUST_DOIT,"\n\tCC1 TO VGI1 8-BIT LOOPBACK TEST FOR CHANNEL %d\n",channel);

	/* Do the Loopback test */
	if( std == ALL_STD )
	{
		for( std = 0; std < 4; std++)
		{
			status = _mgv_cc1tovgi1loop( ONE, std, format );
			if(status)
				return(-1);
			delay( mseconds );
			status = _mgv_cc1tovgi1loop( TWO, std, format );
			if(status)
				return(-1);
			delay( mseconds );
		}	
	}
	else
		status = _mgv_cc1tovgi1loop(channel, std, format );

	msg_printf(SUM,"\n\nEND OF CC1 8-BIT LOOPBACK TEST FOR CHANNEL %d.\
			\n\n",channel);

	return(status);
	
}

