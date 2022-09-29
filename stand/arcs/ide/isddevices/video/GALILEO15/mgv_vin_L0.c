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
 *	Filename:  vin_L0.c
 *	Purpose:  
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.3 $
 *
 */

#include "mgv_diag.h"


/**************************************************************************
*
*	Function: vin_gonogo
*
*	Description: see description for vin_L0.c above
*
*	Return: 
*
**************************************************************************/
 
int vin_gonogo( unsigned char ch_sel, unsigned char tv_std, unsigned char format )
{
	unsigned char reg_val;
	unsigned int *read_10input_field, *gold_10line;
	unsigned char *read_8input_field, *gold_8line;
	int i;
	extern int xsize[], ysize[];

	/* set tv standard */
	GAL_IND_W1(mgvbase, GAL_TV_STD, tv_std);
	/* set vin Ctl to normal autophase, pass all 10 bits */
	GAL_IND_W1(mgvbase, GAL_VIN_CTRL, 0x0);

	/* set vin#1 to main memory through VGI1A */
	/* or  vin#2 to main memory through VGI1B */
	reg_val = (ch_sel==ONE) ?  0x15 : 0x36;
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);

	msg_printf(JUST_DOIT,"\nSET VIDEO INPUT SOURCE TO THE FOLLOWING:\
							\n\t1. CONNECTED TO CHANNEL %s\
							\n\t2. VIDEO MODE: %s\
							\n\t3. GENLOCK ACTIVE",
							(ch_sel==ONE) ? "ONE":"TWO",
							asc_tv_std_p[ tv_std ]);


	/* check for video input status */
	for(i = 0; i < 10; i++)
	{
		if(ch_sel==ONE)
		{
			GAL_IND_R1(mgvbase, GAL_VIN_CH1_STATUS, reg_val);
		}
		else	
			GAL_IND_R1(mgvbase, GAL_VIN_CH2_STATUS, reg_val);

		if( (reg_val & 0x10) == 0)
			break;
		delay( 1000 );
		msg_printf(JUST_DOIT,".");
	}


	/* display failed result messages */
	if(reg_val & 0x10) 
	{
		msg_printf(JUST_DOIT,"\nCONNECT THE VIDEO SOURCE TO CHANNEL%s\
					AND REPEAT THIS TEST\n", (ch_sel==ONE) ? "ONE":"TWO"); 
		return(-1);
	}
	else if( (reg_val & 0x03) != tv_std ) 
	{
		msg_printf(JUST_DOIT,"\nSET THE VIDEO STANDARD TO %s\
							AND REPEAT THIS TEST", asc_tv_std_p[ tv_std ]); 
		return(-1);
	}
	else if(reg_val & 0x10) 
	{
		msg_printf(JUST_DOIT,"\nAUTOPHASE NOT LOCKED, CHECK GENLOCK STATUS, \
							AND REPEAT THIS TEST\n");
		return(-1);
	}
	else if(reg_val & 0x20) 
	{
		msg_printf(JUST_DOIT,"\nCHANNEL %s INPUT OUT OF RANGE. CHECK SETTINGS \
						AND REPEAT THIS TEST\n",(ch_sel==ONE) ? "ONE":"TWO");
		return(-1);
	}


	/* 	READ THE VIDEO INPUT SOURCE INTO VGI1, 
		REQUEST SOURCE CAPTURE BUFFER FROM VGI1,
		COMPARE WITH GOLD STANDARD FOR FORMAT 
	*/

	msg_printf(JUST_DOIT,"\n\nPERFORMING VIDEO INPUT GONOGO TEST");

	switch( format )
	{
#if 0
	case MGV_VGI1_FMT_YUV422_10:
		/* allocate space for one field = xsize*2*ysize/2 */
		read_10input_field 	= malloc( xsize[ tv_std ] * 3 * ysize[ tv_std ]/2);

		/* allocate space to generate one word-wide gold std. line */
		gold_10line 		= malloc( xsize[ tv_std ] * 3 );
		
		/* create the gold standard line */
		make_vid_array(gold_10line, 10, 128, 128, 128, 0, 
							xsize[ tv_std ] * 3, 1, 0, 0);

		/* capture one field */
		_mgv_VGI1DMAWrite(ch_sel, tv_std, MGV_VGI1_FMT_YUV422_10, 
											ONE, read_10input_field);

        /* compare first(?) line of field with gold line */
		mgv_compare_word_buffers(	xsize[ tv_std ] * 3, 
									gold_10line, 
									read_10input_field);

		free(gold_10line);
		free(read_10input_field);
		break;

    case MGV_VGI1_FMT_AUYV4444_10:
		read_10input_field 	= malloc( xsize[ tv_std ] * 4 * ysize[ tv_std ]/2);
		gold_10line 		= malloc( xsize[ tv_std ] * 4 );
		make_vid_array(gold_10line, TENBIT, 128, 128, 128, 0, 
									xsize[ tv_std ] * 4, 1, 0, 0);
		_mgv_VGI1DMAWrite(ch_sel, tv_std, MGV_VGI1_FMT_AUYV4444_10,
									ONE, read_10input_field);

		_mgv_compare_word_buffers(  xsize[ tv_std ] * 4,
									xsize[ tv_std ] * 4,
									gold_10line,
									read_10input_field, 0, 0);
		free(gold_10line);
		free(read_10input_field);
		break;
#endif

	case MGV_VGI1_FMT_YUV422_8:
		read_8input_field = malloc( xsize[ tv_std ] * 2 * ysize[ tv_std ]/2);
		gold_8line 		= malloc( xsize[ tv_std ] * 2 );
		make_vid_array((unsigned int*)gold_8line, 8, 128, 128, 128, 0, 
									xsize[ tv_std ] * 2, 1, 0, 0);
		_mgv_VGI1DMAWrite(ch_sel, tv_std, MGV_VGI1_FMT_YUV422_8,
									ONE, read_8input_field);
		_mgv_compare_byte_buffers(  xsize[ tv_std ] * 2,
									xsize[ tv_std ] * 2,
									gold_8line,
									read_8input_field, 0, 0);
        free(gold_8line);
		free(read_8input_field);
		break;

#if 0
	case MGV_VGI1_FMT_AUYV4444_8:
		read_8input_field 	= malloc( xsize[ tv_std ] * 3 * ysize[ tv_std ]/2);
		gold_8line 			= malloc( xsize[ tv_std ] * 3 );
		make_vid_array((unsigned int*)gold_8line, 8, 128, 128, 128, 0, 
									xsize[ tv_std ] * 3, 1, 0, 0);
		_mgv_VGI1DMAWrite(ch_sel, tv_std, MGV_VGI1_FMT_AUYV4444_8,
									ONE, read_8input_field);
		_mgv_compare_byte_buffers(  xsize[ tv_std ] * 3,
									xsize[ tv_std ] * 3,
									gold_8line,
									read_8input_field, 0, 0);
		free(gold_8line);
		free(read_8input_field);
		break;
#endif

	default:
		return(-1);

	}
	return( 0 );
}

/***************************************************************************
*
*	Function: vin_L0
*
*	Description: see top header description
*
****************************************************************************/

int vin_L0( int argc, char *argv[] )
{
int    	status		= 0;
unsigned char chan	= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
unsigned char 	fmt = MGV_VGI1_FMT_YUV422_8;
int  		sec		= 0;
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
	}

	/* Do the Video In GONOGO test */
	if( std == ALL_STD )
	{
		for( std = 0; std < 4; std++)
		{
			status = vin_gonogo( ONE, std, fmt );
			if(status)
				return(-1);
			delay( mseconds );
			status = vin_gonogo( TWO, std, fmt );
			if(status)
				return(-1);
			delay( mseconds );
		}	
	}
	else
	{
		status = vin_gonogo( channel, std, fmt );
		if(status)
			return(-1);
		delay( mseconds );
	}
	
	msg_printf(SUM,"\n\nEND OF VIDEO INPUT GONOGO TEST.\n");

}

