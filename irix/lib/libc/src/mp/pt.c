/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements pthread functionality that needs to reside in libc.
 *
 * Typically this is data/ routines that need to interact with the C
 * runtime (syscalls).
 */


#include "synonyms.h"
#include "mplib.h"
#include <errno.h>
#include <pthread.h>


int
__pttokt(pthread_t pt, tid_t *t)
{
	MTLIB_RETURN( (MTCTL_KTID_HACK, pt, t) );
	return (ENOSYS);
}


int
__ptuncncl(void)
{
	MTLIB_RETURN( (MTCTL_UNCNCL_HACK) );
	return (ENOSYS);
}



unsigned int	__vp_active _INITBSS;		/* Number of active vp's */
