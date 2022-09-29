/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.4 $"

#ifdef __STDC__
	#pragma weak sprofil = _sprofil
#endif
#include "synonyms.h"

#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/profil.h>

int
sprofil(struct prof *profp, 
	int	count, 
	struct timeval *tvp, 
	unsigned int flags)
{
	return((int)syssgi(SGI_SPROFIL, profp, count, tvp, flags));
}
