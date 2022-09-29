#ifndef __NDBM_H__
#define __NDBM_H__
#ident "$Revision: 1.22 $"
/*
*
* Copyright 1992-1997 Silicon Graphics, Inc.
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
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ndbm.h	5.1 (Berkeley) 5/30/85
 */

#include <sys/types.h>
#include <sgidefs.h>

/*
 * Hashed key data base library.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define _PBLKSIZ 1024
#define _DBLKSIZ 4096

typedef struct {
	int	dbm_dirf;		/* open directory file */
	int	dbm_pagf;		/* open page file */
	int	dbm_flags;		/* flags, see below */
	long	dbm_maxbno;		/* last ``bit'' in dir file */
	long	dbm_bitno;		/* current bit number */
	long	dbm_hmask;		/* hash mask */
	long	dbm_blkptr;		/* current block for dbm_nextkey */
	int	dbm_keyptr;		/* current key for dbm_nextkey */
	long	dbm_blkno;		/* current page to read/write */
	long	dbm_pagbno;		/* current page in pagbuf */
	char	dbm_pagbuf[_PBLKSIZ];	/* page file block buffer */
	long	dbm_dirbno;		/* current block in dirbuf */
	char	dbm_dirbuf[_DBLKSIZ];	/* directory file block buffer */
} DBM;

#if _LFAPI
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
typedef struct {
	int	dbm_dirf;		/* open directory file */
	int	dbm_pagf;		/* open page file */
	int	dbm_flags;		/* flags, see below */
/* LINTED long long */
__int64_t	dbm_maxbno;		/* last ``bit'' in dir file */
/* LINTED long long */
__int64_t	dbm_bitno;		/* current bit number */
/* LINTED long long */
__int64_t	dbm_hmask;		/* hash mask */
/* LINTED long long */
__int64_t	dbm_blkptr;		/* current block for dbm_nextkey */
	int	dbm_keyptr;		/* current key for dbm_nextkey */
/* LINTED long long */
__int64_t	dbm_blkno;		/* current page to read/write */
/* LINTED long long */
__int64_t	dbm_pagbno;		/* current page in pagbuf */
	char	dbm_pagbuf[_PBLKSIZ];	/* page file block buffer */
/* LINTED long long */
__int64_t	dbm_dirbno;		/* current block in dirbuf */
	char	dbm_dirbuf[_DBLKSIZ];	/* directory file block buffer */
} DBM64;
#else /* (_MIPS_SIM != _MIPS_SIM_ABI64) */
typedef DBM DBM64;
#endif /* (_MIPS_SIM != _MIPS_SIM_ABI64) */
#endif /* _SGIAPI */

#define _DBM_RDONLY	0x1	/* data base open read-only */
#define _DBM_IOERR	0x2	/* data base I/O error */

#if _SGIAPI

/*
 * These values limit the size of the hashing mask used by DBM
 * so that we can actually store the blocks to which we hash.
 * The constraint is that _DBM_MAXHASH * _PBLKSIZ must be less
 * than the maximum file size.
 */
#define	DBM_MAXHASH64	0x1fffffffffffffLL
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
#define	DBM_MAXHASH	0x1fffffL
#else
#define	DBM_MAXHASH	DBM_MAXHASH64
#endif

#define PBLKSIZ	_PBLKSIZ
#define DBLKSIZ	_DBLKSIZ
#define dbm_rdonly(db)	((db)->dbm_flags & _DBM_RDONLY)

/* for flock(2) and fstat(2) */
#define dbm_dirfno(db)	((db)->dbm_dirf)
#define dbm_pagfno(db)	((db)->dbm_pagf)

#endif	/* _SGIAPI */

typedef struct {
	void	*dptr;
	size_t	dsize;
} datum;

/*
 * flags to dbm_store()
 */
#define DBM_INSERT	0
#define DBM_REPLACE	1

DBM	*dbm_open(const char *, int, mode_t);
void	dbm_close(DBM *);
int	dbm_error(DBM *);
int	dbm_clearerr(DBM *);
#define dbm_error(__db)	((__db)->dbm_flags & _DBM_IOERR)
	/* use this one at your own risk! */
#define dbm_clearerr(__db)	((__db)->dbm_flags &= ~_DBM_IOERR)

#if _LFAPI
DBM64	*dbm_open64(const char *, int, mode_t);
void	dbm_close64(DBM64 *);
int	dbm_clearerr64(DBM64 *);
int	dbm_error64(DBM64 *);
#define dbm_error64(__db)	((__db)->dbm_flags & _DBM_IOERR)
	/* use this one at your own risk! */
#define dbm_clearerr64(__db)	((__db)->dbm_flags &= ~_DBM_IOERR)
#endif /* _LFAPI */

/*
 * NOTE: Application software should NOT program to the _XXX interfaces.
 */
datum	_dbm_fetch(DBM *, datum);
datum	_dbm_firstkey(DBM *);
datum	_dbm_nextkey(DBM *);
int	_dbm_delete(DBM *, datum);
int	_dbm_store(DBM *, datum, datum, int);

#if _SGIAPI
long	_dbm_forder(DBM *, datum);
datum	_dbm_fetch64(DBM64 *, datum);
datum	_dbm_firstkey64(DBM64 *);
datum	_dbm_nextkey64(DBM64 *);
/* LINTED long long */
#if !((_MIPS_SIM == _MIPS_SIM_ABI64) && defined(_DBM_INTERNAL))
__int64_t	_dbm_forder64(DBM64 *, datum);
#endif      
int	_dbm_delete64(DBM64 *, datum);
int	_dbm_store64(DBM64 *, datum, datum, int);
#endif /* _SGIAPI */

#ifndef _DBM_INTERNAL
#if (_MIPS_SIM != _MIPS_SIM_ABI64)

datum	dbm_fetch(DBM *, datum);
datum	dbm_firstkey(DBM *);
datum	dbm_nextkey(DBM *);
int	dbm_delete(DBM *, datum);
int	dbm_store(DBM *, datum, datum, int);

#if _SGIAPI
long	dbm_forder(DBM *, datum);
datum	dbm_fetch64(DBM64 *, datum);
datum	dbm_firstkey64(DBM64 *);
datum	dbm_nextkey64(DBM64 *);
/* LINTED long long */
__int64_t	dbm_forder64(DBM64 *, datum);
int	dbm_delete64(DBM64 *, datum);
int	dbm_store64(DBM64 *, datum, datum, int);
#endif /* _SGIAPI */

#else /* (_MIPS_SIM != _MIPS_SIM_ABI64) */

/*REFERENCED*/
static datum
dbm_fetch(DBM *db, datum key)
{
	return _dbm_fetch(db, key);
}

/*REFERENCED*/
static datum
dbm_firstkey(DBM *db)
{
	return _dbm_firstkey(db);
}

/*REFERENCED*/
static datum
dbm_nextkey(DBM *db)
{
	return _dbm_nextkey(db);
}

/*REFERENCED*/
static int
dbm_delete(DBM *db, datum key)
{
	return _dbm_delete(db, key);
}

/*REFERENCED*/
static int
dbm_store(DBM *db, datum key, datum dat, int replace)
{
	return _dbm_store(db, key, dat, replace);
}

/*REFERENCED*/
#if _SGIAPI
static long
dbm_forder(DBM *db, datum key)
{
	return _dbm_forder(db, key);
}
	
/*REFERENCED*/
static datum
dbm_fetch64(DBM64 *db, datum key)
{
	return _dbm_fetch64(db, key);
}

/*REFERENCED*/
static datum
dbm_firstkey64(DBM64 *db)
{
	return _dbm_firstkey64(db);
}

/*REFERENCED*/
static datum
dbm_nextkey64(DBM64 *db)
{
	return _dbm_nextkey64(db);
}

/*REFERENCED*/
/* LINTED long long */
static __int64_t
dbm_forder64(DBM64 *db, datum key)
{
	return _dbm_forder64(db, key);
}

/*REFERENCED*/
static int
dbm_delete64(DBM64 *db, datum key)
{
	return _dbm_delete64(db, key);
}

/*REFERENCED*/
static int
dbm_store64(DBM64 *db, datum key, datum dat, int replace)
{
	return _dbm_store64(db, key, dat, replace);
}

#endif /* _SGIAPI */
#endif /* (_MIPS_SIM != _MIPS_SIM_ABI64) */
#endif /* _DBM_INTERNAL */

#ifdef __cplusplus
}
#endif
#endif /* !__NDBM_H__ */
