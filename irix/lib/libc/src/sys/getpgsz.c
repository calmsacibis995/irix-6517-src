/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: getpgsz.c,v 3.5 1994/10/20 19:49:22 jwag Exp $"

#ifdef __STDC__
	#pragma weak getpagesize = _getpagesize
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/sysmp.h>

int
getpagesize(void)
{
	return((int)sysmp(MP_PGSIZE));
}
