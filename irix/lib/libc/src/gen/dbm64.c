/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
#ifdef sgi
#ident "$Revision: 1.3 $"
#endif

#ifdef __STDC__
	#pragma weak dbmclose64 = _dbmclose64
	#pragma weak dbminit64  = _dbminit64
	#pragma weak delete64   = _delete64
	#pragma weak fetch64    = _fetch64
	#pragma weak firstkey64 = _firstkey64
	#pragma weak forder64   = _forder64
	#pragma weak nextkey64  = _nextkey64
	#pragma weak store64    = _store64
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)dbm.c	5.3 (Berkeley) 85/08/15";
#endif

#include	<stdio.h>
#include	<rpcsvc/ypupdate_prot.h>	/* for prototyping */
#include	"dbm.h"
#include	"gen_extern.h"

#define	NODB	((DBM64 *)0)

static DBM64 *cur_db = NODB;

static char *no_db = "dbm: no open database\n";

extern int	_finddatum64(char buf[], datum item);

int
dbminit64(const char *file)
{
	if (cur_db != NODB)
		dbm_close64(cur_db);

	cur_db = dbm_open64(file, 2, 0);
	if (cur_db == NODB) {
		cur_db = dbm_open64(file, 0, 0);
		if (cur_db == NODB)
			return (-1);
	}
	return (0);
}

void
dbmclose64(void)
{
	if (cur_db == NODB)
		return;
	
	dbm_close64(cur_db);
	cur_db = NODB;
}

long long
_forder64(datum key)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (0LL);
	}
	return (_dbm_forder64(cur_db, key));
}

datum
_fetch64(datum key)
{
	datum item;

	if (cur_db == NODB) {
		printf(no_db);
		item.dptr = 0;
		return (item);
	}
	return (_dbm_fetch64(cur_db, key));
}

int
_delete64(datum key)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (-1);
	}
	if (dbm_rdonly(cur_db))
		return (-1);
	return (_dbm_delete64(cur_db, key));
}

int
_store64(datum key, datum dat)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (-1);
	}
	if (dbm_rdonly(cur_db))
		return (-1);

	return (_dbm_store64(cur_db, key, dat, DBM_REPLACE));
}

datum
_firstkey64(void)
{
	datum item;

	if (cur_db == NODB) {
		printf(no_db);
		item.dptr = 0;
		return (item);
	}
	return (_dbm_firstkey64(cur_db));
}

datum
_nextkey64(datum key)
{
	datum item;

	if (cur_db == NODB) {
		printf(no_db);
		item.dptr = 0;
		return (item);
	}
#ifdef sgi
	/*
	 * Position to the given key
	 */
	item = _dbm_fetch64(cur_db, key);
	if (item.dptr == 0)
		return (item);
	/*
	 * There is actually no interaction between dbm_fetch
	 * and dbm_nextkey.  We must supply the interaction
	 * here by setting the fields used to implement
	 * dbm_nextkey.
	 */
	cur_db->dbm_blkptr = cur_db->dbm_pagbno;
	cur_db->dbm_keyptr = _finddatum64(cur_db->dbm_pagbuf, key) + 2;
	return (_dbm_nextkey64(cur_db));
#else
	/*
	 * This is the implementation as it came from
	 * Berkeley.  This is not really compatible
	 * with the old libdbm, because the key is
	 * actually ignored by dbm_nextkey.
	 */
	return (_dbm_nextkey64(cur_db, key));
#endif
}
