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
 *	Filename:  mgv_ext_vin.c
 *	Purpose:  
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.10 $
 *
 */

#include "mgv_diag.h"

void
mgv_write_ind (  int argc, char **argv )
{
 int offset, value;
 argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'o':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &offset);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &offset);
                }
            break ;
            case 'v':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &value);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &value);
                }
            break ;

            default :
                msg_printf(SUM, " usage: -o offset -v value \n") ;
                return;
        }
        argc--; argv++;
    }

	GAL_IND_W1(mgvbase, offset, value);
}

void
mgv_read_ind (  int argc, char **argv )
{
 int offset, value;
 argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'o':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &offset);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &offset);
                }
            break ;
            default :
                msg_printf(SUM, " usage: -o offset \n") ;
                return;
        }
        argc--; argv++;
    }

	GAL_IND_R1(mgvbase, offset, value);
	msg_printf(SUM, " mgv: offset %x value %x \n", offset, value);
}

/**************************************************************************
*
*	Function: _mgv_ext_vin_gonogo
*
*	Description: see description for _mgv_vin_L0.c above
*
*	Return: 
*
**************************************************************************/
 
int _mgv_ext_vin_gonogo( unsigned char ch_sel, unsigned char tv_std, unsigned char format )
{
	unsigned char pxl_val, reg_val, pattern = 0, match = FALSE, timer = 0,tmp,skip;
	unsigned char* send_field_p;
	unsigned long bufSize;
	unsigned int status;
	extern int xsize[], ysize[];
	unsigned char ext_vid_gonogo[] = 
			{235, 0xAA, 0x55, 16, 0xFF }; 	

	/* set tv standard */
	GAL_IND_W1(mgvbase, GAL_TV_STD, tv_std);

	/* set vin Ctl to normal autophase, truncate both CHs to 8 bits */
	GAL_IND_W1(mgvbase, GAL_VIN_CTRL, 0x30);

	/* set ref black to opposite VGI1 channel ie: if 1 blk->B,2..blk->A */
	reg_val = (ch_sel == ONE) ? 0x76 : 0x75;
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);

	/* if CH1 set vin#1 to main memory through VGI1A */
	/* if CH2 set vin#2 to main memory through VGI1B */
	reg_val = (ch_sel==ONE) ?  0x15 : 0x36;
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, reg_val);

	/* Vidin 1 to vout 1, vidin 2 to vout 2 */
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x18);
	GAL_IND_W1(mgvbase, GAL_MUX_ADDR, 0x37);

	/* genlock reference source */
	GAL_IND_W1(mgvbase, 16, 0x01);


	bufSize = ((xsize[tv_std]*2*(sizeof(char)))*(ysize[tv_std]/2));
	send_field_p = (unsigned char*)get_chunk(bufSize);
	send_field_p = _mgv_PageAlign( send_field_p );

	if(send_field_p == NULL)
	{
		msg_printf(ERR,"\nCould not allocate memory for field array");
		return(-1);
	}

	msg_printf(JUST_DOIT,"\n\nCONFIRM THE TSG422 VIDEO SOURCE is CONNECTED \
						AND SET TO THE FOLLOWING:\
						\n\t\tCHANNEL: %d\
						\n\t\tDISPLAY: FLAT FIELD", ch_sel);

	_mgv_delay_dots( 2, 1 );

	status = _mgv_check_vin_status( ch_sel, tv_std );
	if(status)
		return(0);

	/* load parameters into mgvbase for DMA operation */
	tmp = (ch_sel == ONE) ? VGI1_CHANNEL_1 : VGI1_CHANNEL_2;
	_mgv_setup_VGI1DMA(tmp, tv_std, format, 2,
		send_field_p, VGI1_DMA_WRITE, 0);


msg_printf(DBG,"mgvbase->pVGIDMABuf: %x send_field_p: %x",
					mgvbase->pVGIDMABuf,send_field_p );
	for(pattern = 0; ext_vid_gonogo[ pattern ] != 0xFF ; pattern++)
	{
		timer = 0;
		match = FALSE;
		skip = FALSE;
		msg_printf(JUST_DOIT,"\n**READING VIDEO INPUT FIELD\n\n");
		msg_printf(SUM, "Press <Space> when done\n");
		do
		{
			/* do VidIn to VGI1 DMA */
    			status = _mgv_VGI1DMA();
			if(status)
				return(status);

			pxl_val = *( send_field_p+(xsize[tv_std]*ysize[tv_std]/4*sizeof(char) ) );	
			if( pxl_val == ext_vid_gonogo[ pattern ] )
				match = TRUE;
			else 
			{
				msg_printf(SUM,"\rADJUST FLAT FIELD: %s    VINPUT:%4d  GOAL:%4d  TIMER:%3d",
				(pxl_val > ext_vid_gonogo[pattern]) ? "LOWER ":"HIGHER",
				pxl_val,
				ext_vid_gonogo[pattern],
				60-timer);
				delay( 400 );
				if ((pause(1, " ", " ")) != 2) skip = TRUE;
			}

			timer++;

		}while( (match == FALSE) && (timer < 60)  && (skip == FALSE) );

		if( timer < 60 )
		{
			msg_printf(JUST_DOIT,"\n***VIDEO INPUT FIELD MATCHES VALUE %d",
					ext_vid_gonogo[pattern] );
			if( ext_vid_gonogo[pattern+1] != 0xFF )
				msg_printf(JUST_DOIT,"\nNEXT VALUE TO MATCH IS %d",
					ext_vid_gonogo[pattern+1] );

			_mgv_delay_dots( 2, 1 );
		}
		else
		{
			msg_printf(SUM,"\nTIMED OUT IN VIDEO INPUT MATCHING ATTEMPT\n");
			return(0);
		}
	}  /* for pattern */

		msg_printf(SUM,"\n\nEXTERNAL VIDEO INPUT TEST FOR CHANNEL %d COMPLETE", ch_sel);

	return( 0 );
}

/***************************************************************************
*
*	Function: mgv_ext_vin
*
*	Description: see top header description
*
****************************************************************************/

int mgv_ext_vin( int argc, char *argv[] )
{
int    		status	= 0;
unsigned char 	chan	= 0;
unsigned char  channel	= CHANNEL_DEFAULT;
unsigned char 	format 	= MGV_VGI1_FMT_YUV422_8;
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
			status = _mgv_ext_vin_gonogo( ONE, std, format);
			if(status)
				return(status);
			delay( mseconds );
			status = _mgv_ext_vin_gonogo( TWO, std, format);
			if(status)
				return(status);
			delay( mseconds );
		}	
	}
	else
	{
		status = _mgv_ext_vin_gonogo( channel, std, format);
	}
	
	msg_printf(SUM,"\n\nEND OF EXTERNAL VIDEO INPUT TEST.\n");

	return(status);

}

