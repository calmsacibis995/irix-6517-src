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
#ident "$Id: lstat.c,v 1.4 1997/10/12 08:35:28 jwag Exp $"

#include "sgidefs.h"

#ifdef __STDC__
#ifndef _LIBC_ABI
	#pragma weak lstat = _lstat
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak lstat64 = _lstat64
#endif
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

#ifndef _LIBC_ABI
int
lstat(const char *p, struct stat *sb)
{
	return(_lxstat(_STAT_VER, p, sb));
}
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
int
lstat64(const char *p, struct stat64 *sb)
{
	return(_lxstat(_STAT_VER, p, (struct stat *)sb));
}
#endif
