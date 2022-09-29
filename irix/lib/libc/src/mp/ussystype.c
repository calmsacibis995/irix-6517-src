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
#ident "$Revision: 1.12 $ $Author: ack $"

#include "synonyms.h"
#include <unistd.h>
#include <stdlib.h>
#include <ulocks.h>
#include "us.h"

/*
 * Determine system type - at this point just whether MP or UP
 */
int
_getsystype(void)
{
	int nprocs;

	nprocs = (int)sysconf(_SC_NPROC_CONF);

#ifndef NDEBUG
	{ char *syst;
	int ctype;
	if ((syst = getenv("USSYSTYPE")) != NULL) {
		ctype = atoi(syst);
		switch (ctype) {
		case US_UP:
		case US_MP:
			return ctype;
		default:
			if (_uerror)
				_uprint(0, "usinit:ERROR:Bad USSYSTYPE value");
		}
	}
	}
#endif

	if (nprocs <= 1)
		return US_UP;
	return US_MP;
}
