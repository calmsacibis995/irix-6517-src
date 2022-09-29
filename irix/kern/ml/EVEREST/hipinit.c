
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

/*
 * Early initialization of IO4 HIPPI devices.
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>


/* This is a lot easier than having the drivers search through
 * the IO4 boards for HIPPI adaptors.  We do minimal set-up of
 * HIPPI adaptors here and let configured drivers handle the rest
 * later.  A list of all the HIPPIs found at start-up is kept in
 * io4hip_adaps[].
 */

ioadap_t	io4hip_adaps[ EV_MAX_HIPADAPS ];
int		io4hip_cnt = 0;


/* ARGSUSED */
int
io4hip_init( u_char slot, u_char padap, u_char window, int adap )
{
	ioadap_t	*hip = &io4hip_adaps[ io4hip_cnt ];

	if ( io4hip_cnt >= EV_MAX_HIPADAPS )
		return 0;

	hip->slot	= slot;
	hip->padap	= padap;
	hip->type	= IO4_ADAP_HIPPI;
	hip->swin	= SWIN_BASE(window, padap);
	hip->lwin	= 0;	/* do later */

	io4hip_cnt++;

	return 1;
}

