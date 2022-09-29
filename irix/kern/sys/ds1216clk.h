/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.6 $"

/*
** various bit defines for the SMARTWATCH
*/
#define	RTC_12H		0x80	/* register 3: 12 hour mode		     */
#define	RTC_RUN		0x20	/* register 4: turn on oscillator	     */
#define	RTC_RESET	0x10	/* register 4: abort data xfer if reset line */
				/*  goes low				     */

#define	RTC_READ	0	/* clock specific function: read	*/
#define	RTC_WRITE	1	/* clock specific function: write	*/

#define	RTC_PATTERNS	8

/*
** pattern recognition bytes
*/
char	nvrampat[] = {
	0xc5,	0x3a,	0xa3,	0x5c,	0xc5,	0x3a,	0xa3,	0x5c
};
