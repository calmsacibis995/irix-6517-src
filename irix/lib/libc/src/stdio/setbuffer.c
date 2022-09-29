/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Routines to mimic 4.3BSD's setbuffer() and setlinebuf().
 */
#ident "$Revision: 1.5 $"
#ifdef __STDC__
	#pragma weak setbuffer = _setbuffer
	#pragma weak setlinebuf = _setlinebuf
#endif
#include "synonyms.h"
#include "mplib.h"
#include "shlib.h"
#include <stdio.h>

int
setbuffer(FILE *iop, char *buf, int size)
{
	if (buf == NULL) {
		return(setvbuf(iop, NULL, _IONBF, 0));
	} else {
		return(setvbuf(iop, buf, _IOFBF, (size_t)size));
	}
}

int
setlinebuf(FILE *iop)
{
	LOCKDECLINIT(l, LOCKFILE(iop));
	fflush(iop);
	setvbuf(iop, NULL, _IOLBF, BUFSIZ);
	UNLOCKFILE(iop, l);
	return (0);
}
