/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#ifdef __STDC__
	#pragma weak sysid = _sysid
#endif
#include "synonyms.h"
#include "stdlib.h"
#include <sys/systeminfo.h>
#include <sys/syssgi.h>
#include <bstring.h>		/* for prototyping */

unsigned
sysid(unsigned char id[16])
{
	char	systemid[ MAXSYSIDSIZE ];

	if (sysinfo(SI_HW_SERIAL, systemid, MAXSYSIDSIZE) == -1)
		return (0);

	if (id)
		bcopy(systemid, id, 16);

	return((unsigned)atol(systemid));
}
