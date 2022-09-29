#ifndef __DBM_H__
#define __DBM_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.11 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dbm.h	5.2 (Berkeley) 85/06/26
 */
#include <standards.h>
#ifndef NULL
/*
 * this is lunacy, we no longer use it (and never should have
 * unconditionally defined it), but, this whole file is for
 * backwards compatability - someone may rely on this.
 */
#define	NULL	((char *) 0L)
#endif

#include "ndbm.h"

int		dbminit(const char *);
void		dbmclose(void);

#if _SGIAPI
int		dbminit64(const char *);
void		dbmclose64(void);
#endif


/*
 * NOTE: Application software should NOT program to the _XXX interfaces.
 */
datum	_fetch(datum);
int	_store(datum, datum);
int	_delete(datum);
datum	_firstkey(void);
datum	_nextkey(datum);

#if _SGIAPI
datum	_fetch64(datum);
int	_store64(datum, datum);
int	_delete64(datum);
datum	_firstkey64(void);
datum	_nextkey64(datum);
#endif /* _SGIAPI */

#ifndef _DBM_INTERNAL
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
	
datum	fetch(datum);
int	store(datum, datum);
int	delete(datum);
datum	firstkey(void);
datum	nextkey(datum);

#if _SGIAPI
datum	fetch64(datum);
int	store64(datum, datum);
int	delete64(datum);
datum	firstkey64(void);
datum	nextkey64(datum);
#endif /* _SGIAPI */

#else /* (_MIPS_SIM != _MIPS_SIM_ABI64) */

/*REFERENCED*/
static datum
fetch(datum key)
{
	return _fetch(key);
}

/*REFERENCED*/
static int
store(datum key, datum dat)
{
	return _store(key, dat);
}

/*REFERENCED*/
static int
delete(datum key)
{
	return _delete(key);
}

/*REFERENCED*/
static datum
firstkey(void)
{
	return _firstkey();
}

/*REFERENCED*/
static datum
nextkey(datum key)
{
	return _nextkey(key);
}

#if _SGIAPI

/*REFERENCED*/
static datum
fetch64(datum key)
{
	return _fetch64(key);
}

/*REFERENCED*/
static int
store64(datum key, datum dat)
{
	return _store64(key, dat);
}

/*REFERENCED*/
static int
delete64(datum key)
{
	return _delete64(key);
}

/*REFERENCED*/
static datum
firstkey64(void)
{
	return _firstkey64();
}

/*REFERENCED*/
static datum
nextkey64(datum key)
{
	return _nextkey64(key);
}

#endif /* _SGIAPI */

#endif /* (_MIPS_SIM != _MIPS_SIM_ABI64) */
#endif /* _DBM_INTERNAL */

#ifdef __cplusplus
}
#endif
#endif /* !__DBM_H__ */
