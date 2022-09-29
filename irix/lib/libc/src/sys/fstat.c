/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: fstat.c,v 1.5 1997/10/12 08:35:08 jwag Exp $"

#include "sgidefs.h"

#ifdef __STDC__
#ifndef _LIBC_ABI
	#pragma weak fstat = _fstat
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak fstat64 = _fstat64
#endif
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

#ifndef _LIBC_ABI
int
fstat(int p, struct stat *sb)
{
	return(_fxstat(_STAT_VER, p, sb));
}
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
int
fstat64(int p, struct stat64 *sb)
{
	return(_fxstat(_STAT_VER, p, (struct stat *)sb));
}
#endif
