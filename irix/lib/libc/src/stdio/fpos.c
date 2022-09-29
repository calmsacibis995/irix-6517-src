/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fpos.c	1.2"
/*LINTLIBRARY*/

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
#if (_MIPS_SIM == _MIPS_SIM_NABI32 || _MIPS_SIM == _MIPS_SIM_ABI64)
	#pragma weak fgetpos64 = fgetpos
	#pragma weak fsetpos64 = fsetpos
	#pragma weak _fgetpos64 = fgetpos
	#pragma weak _fsetpos64 = fsetpos
#endif
#endif
#include "synonyms.h"
#include <stdio.h>

int
fgetpos(stream, pos)
FILE *stream;
fpos_t *pos;
{
	/* Use _ABIN32 here, since sgidefs is not included */
#if _MIPS_SIM == _ABIN32
	if ((*pos = ftell64(stream)) == -1L)
#else
	if ((*pos = ftell(stream)) == -1L)
#endif
		return(-1);
	return(0);
}

int
fsetpos(stream, pos)
FILE *stream;
const fpos_t *pos;
{
#if _MIPS_SIM == _ABIN32
	if (fseek64(stream, *pos, SEEK_SET) != 0)
#else
	if (fseek(stream, *pos, SEEK_SET) != 0)
#endif
		return(-1);
	return(0);
}
