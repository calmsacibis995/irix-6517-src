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
#ident "$Id: stat.c,v 1.4 1997/10/12 08:35:35 jwag Exp $"

#include "sgidefs.h"

#ifdef __STDC__
#ifndef _LIBC_ABI
	#pragma weak stat = _stat
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak stat64 = _stat64
#endif
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

#ifndef _LIBC_ABI
int
stat(const char *p, struct stat *sb)
{
	return(_xstat(_STAT_VER, p, sb));
}
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
int
stat64(const char *p, struct stat64 *sb)
{
	return(_xstat(_STAT_VER, p, (struct stat *)sb));
}
#endif
