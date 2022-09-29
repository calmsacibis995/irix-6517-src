/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
#ifdef sgi
#ident "$Revision: 1.11 $"
#endif
#include <sgidefs.h>
#ifdef __STDC__
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
	#pragma weak dbmclose = _dbmclose
	#pragma weak dbminit  = _dbminit
	#pragma weak delete   = _delete
	#pragma weak fetch    = _fetch
	#pragma weak firstkey = _firstkey
	#pragma weak forder   = _forder
	#pragma weak nextkey  = _nextkey
	#pragma weak store    = _store
#else
	/*
	 * For XPG4 compliance, the datum structure was changed
	 * to use a size_t in place of an int for the dsize field.
	 * This breaks binary compatibility with old 64 bit apps.
	 * In order to keep those old apps working, we map the affected
	 * routines to new 'compat' routines.  New callers are mapped
	 * in dbm.h directly to the _XXX routines, so they do not
	 * need weak symbols.
	 */
	#pragma weak dbmclose = _dbmclose
	#pragma weak dbminit  = _dbminit
	#pragma weak delete   = _delete_compat
	#pragma weak fetch    = _fetch_compat
	#pragma weak firstkey = _firstkey_compat
	#pragma weak forder   = _forder_compat
	#pragma weak nextkey  = _nextkey_compat
	#pragma weak store    = _store_compat

	#pragma weak dbmclose64 = _dbmclose
	#pragma weak dbminit64  = _dbminit
	#pragma weak delete64   = _delete_compat
	#pragma weak fetch64    = _fetch_compat
	#pragma weak firstkey64 = _firstkey_compat
	#pragma weak forder64   = _forder_compat
	#pragma weak nextkey64  = _nextkey_compat
	#pragma weak store64    = _store_compat

	/*
	 * These are for use by callers of the dbm_XXX64 routines
	 * from within libc and by the static routines in ndbm.h.
	 * Synonyms.h or the static functions will have remapped
	 * them to _dbm_XXX64, and here we map them to the non 64
	 * versions.
	 */
	#pragma weak _dbmclose64 = _dbmclose
	#pragma weak _dbminit64  = _dbminit
	#pragma weak _delete64   = _delete
	#pragma weak _fetch64    = _fetch
	#pragma weak _firstkey64 = _firstkey
	#pragma weak _forder64   = _forder
	#pragma weak _nextkey64  = _nextkey
	#pragma weak _store64    = _store
#endif
#endif

#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)dbm.c	5.3 (Berkeley) 85/08/15";
#endif

#include	<stdio.h>
#include	<rpcsvc/ypupdate_prot.h>	/* for prototyping */
#define	_DBM_INTERNAL			/* keep out static functions */
#include	"dbm.h"
#include	"gen_extern.h"

#define	NODB	((DBM *)0)

static DBM *cur_db = NODB;

static char *no_db = "dbm: no open database\n";

extern int _finddatum(char buf[], datum item);

int
dbminit(const char *file)
{
	if (cur_db != NODB)
		dbm_close(cur_db);

	cur_db = dbm_open(file, 2, 0);
	if (cur_db == NODB) {
		cur_db = dbm_open(file, 0, 0);
		if (cur_db == NODB)
			return (-1);
	}
	return (0);
}

void
dbmclose(void)
{
	if (cur_db == NODB)
		return;
	
	dbm_close(cur_db);
	cur_db = NODB;
}

long
_forder(datum key)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (0L);
	}
	return (_dbm_forder(cur_db, key));
}

datum
_fetch(datum key)
{
	datum item;

	if (cur_db == NODB) {
		printf(no_db);
		item.dptr = 0;
		return (item);
	}
	return (_dbm_fetch(cur_db, key));
}

int
_delete(datum key)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (-1);
	}
	if (dbm_rdonly(cur_db))
		return (-1);
	return (_dbm_delete(cur_db, key));
}

int
_store(datum key, datum dat)
{
	if (cur_db == NODB) {
		printf(no_db);
		return (-1);
	}
	if (dbm_rdonly(cur_db))
		return (-1);

	return (_dbm_store(cur_db, key, dat, DBM_REPLACE));
}

datum
_firstkey(void)
{
	datum item;

	if (cur_db == NODB) {
		printf(no_db);
		item.dptr = 0;
		return (item);
	}
	return (_dbm_firstkey(cur_db));
}

datum
_nextkey(datum key)
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
	item = _dbm_fetch(cur_db, key);
	if (item.dptr == 0)
		return (item);
	/*
	 * There is actually no interaction between dbm_fetch
	 * and dbm_nextkey.  We must supply the interaction
	 * here by setting the fields used to implement
	 * dbm_nextkey.
	 */
	cur_db->dbm_blkptr = cur_db->dbm_pagbno;
	cur_db->dbm_keyptr = _finddatum(cur_db->dbm_pagbuf, key) + 2;
	return (_dbm_nextkey(cur_db));
#else
	/*
	 * This is the implementation as it came from
	 * Berkeley.  This is not really compatible
	 * with the old libdbm, because the key is
	 * actually ignored by dbm_nextkey.
	 */
	return (_dbm_nextkey(cur_db, key));
#endif
}


#if (_MIPS_SIM == _MIPS_SIM_ABI64)

/*
 * For XPG4 compliance in 6.2 we changed the datum structure to use
 * a size_t in place of the int for the dsize field.  This would have
 * broken binary compatibility for 64 bit apps.  In order to keep old
 * 64 bit applications using these interfaces working, the routines
 * below are used to translate between the old datum structures and the
 * new ones.  Check out the weak symbol mangling at the top of this file
 * to see how we manage the symbol remapping to get here.
 */
typedef struct {
	void	*dptr;
	int	dsize;
} odatum;



int
delete_compat(odatum okey)
{
	datum	key;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	return _delete(key);
}

int
store_compat(odatum okey, odatum odat)
{
	datum	key;
	datum	dat;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	dat.dptr = odat.dptr;
	dat.dsize = odat.dsize;
	return _store(key, dat);
}

long
forder_compat(odatum okey)
{
	datum	key;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	return _forder(key);
}


odatum
fetch_compat(odatum okey)
{
	datum	key;
	datum	item;
	odatum	oitem;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	item = _fetch(key);
	oitem.dptr = item.dptr;
	oitem.dsize = (int)item.dsize;
	return oitem;
}

odatum
firstkey_compat(void)
{	
	datum	item;
	odatum	oitem;

	item = _firstkey();
	oitem.dptr = item.dptr;
	oitem.dsize = (int)item.dsize;
	return oitem;
}

odatum
nextkey_compat(odatum okey)
{
	datum	key;
	datum	item;
	odatum	oitem;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	item = _nextkey(key);
	oitem.dptr = item.dptr;
	oitem.dsize = (int)item.dsize;
	return oitem;
}

#endif /* (_MIPS_SIM == _MIPS_SIM_ABI64) */
