/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	rtc_access.h - routine headers for the RTC/NVRAM		*
 *									*
 ************************************************************************/

#ident "arcs/ide/EVEREST/pbus/rtc_access.h $Revision: 1.3 $"

#define	RTCSIZE	14		/* number of time control registers */

void
rtc_reset(int region, int adap);

uint
ide_get_nvreg(int region, int adap, uint offset);

int
ide_set_nvreg(int region, int adap, uint offset, char byte);

uint
get_rtcreg(int region, int adap, uint offset);

int
set_rtcreg(int region, int adap, uint offset, char byte);

void
get_rtcvals(int region, int adap, char *savearr);

void
set_rtcvals(int region, int adap, char *savearr);
