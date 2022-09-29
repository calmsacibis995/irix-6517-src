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
 *	Filename:  mgv_read_CC1.c
 *
 *
 *
 *
 *  RETURNS: None
 *		  
 *      $Revision: 1.2 $
 *
 */

#include "mgv_diag.h"

/***************************************************************************
*
*	Function: mgv_read_CC1.c
*
*	Description: to dump the  requested CC1 field data
*
*
*	Return: 0 sucessful
*	 	non-zero  memory allocation failed
*
****************************************************************************/

int mgv_read_CC1( int argc, char *argv[] )
{
int x, y, tmp;
unsigned char field= EVEN;
unsigned char start_line = 12, stop_line= 243;
unsigned char tv_std = CCIR_525;
unsigned char *read_field_p;
extern xsize[], ysize[];

	/* check for valid input parameters */
	if (argc > 1 )
	{
msg_printf(DBG,"Entering argc\n");

		/* if valid field, change default */
		tmp = atoi( argv[ 1 ] );
		if( (tmp == ODD) || (tmp == EVEN) )
			field = tmp;
		if( argc > 2 )
		{
			/* if start line, change default */
			tmp = atoi( argv[ 2 ] );
			if( (tmp >= 0) && (tmp < 244) )
			{
				start_line = tmp;

				if( argc > 3 )
				{
					/* if stop line, change default */
					tmp = atoi( argv[ 3 ] );
					if( tmp < start_line )
					{
						msg_printf(SUM,"\nPlease enter a stop line value greater than or equal to the start line value\n");
						return( 1 );
					}
					if( (tmp >= start_line) && (tmp < 244) )
						stop_line = tmp;
				}
				/* if no stop_line entered, 
						just display start line */
				else
					stop_line = start_line;
			}
		}
	}
	else
	{
		msg_printf(SUM,"\nMGV_RDCC:");
		msg_printf(SUM,"\n\tARG 1: FIELD - EVEN (=1), ODD (=0)");
		msg_printf(SUM,"\n\tARG 2: START LINE (0-243) (default: 12)");
		msg_printf(SUM,"\n\tARG 3: STOP  LINE (0-243) (default: 243(if entered) or 'start line #'(if not entered))"); 
		msg_printf(SUM,"\n");
		return( 0 );
	}	

msg_printf(JUST_DOIT,"\n\tCC1 FRAME BUFFER DUMP - %s FIELD",field ? "EVEN":"ODD");

	read_field_p = (unsigned char*) get_chunk( xsize[tv_std]*2 * ysize[tv_std]/2 );
	if( read_field_p == NULL )
		return( -1 );

	rd_CC1_field(
       			0,
                        xsize[tv_std]*2, /* #of bytes, not pixels */
			start_line,	 /* fixed for CCIR for now */
                        (stop_line - start_line+1),
                        field,
                        read_field_p);

for( y= start_line; y < (stop_line+1); y++ )
{
	msg_printf(SUM,"\n\n**FRAME BUFFER LINE%3d:\n ", y);

	for( x = 0; x < xsize[tv_std]*2; x++)
	{
		msg_printf(SUM,"%4d:%2x ",x,*read_field_p);
		read_field_p++;
	}
}
		
	msg_printf(SUM,"\n\nEND OF READ OF CC1 %s FIELD - LINES %d THROUGH %d\n", field? "EVEN":"ODD", start_line, stop_line);

	return(0);
}

