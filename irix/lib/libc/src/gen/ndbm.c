/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
#include <sgidefs.h>
#if defined(__STDC__)
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
	#pragma weak dbm_open     = _dbm_open
	#pragma weak dbm_close    = _dbm_close
	#pragma weak dbm_forder   = _dbm_forder
	#pragma weak dbm_fetch    = _dbm_fetch
	#pragma weak dbm_delete   = _dbm_delete
	#pragma weak dbm_store    = _dbm_store
	#pragma weak dbm_firstkey = _dbm_firstkey
	#pragma weak dbm_nextkey  = _dbm_nextkey
	#pragma weak dbm_clearerr = _dbm_clearerr
	#pragma weak dbm_error    = _dbm_error
#else
	/*
	 * For XPG4 compliance, the datum structure was changed
	 * to use a size_t in place of an int for the dsize field.
	 * This breaks binary compatibility with old 64 bit apps.
	 * In order to keep those old apps working, we map the affected
	 * routines to new 'compat' routines.  New callers are mapped
	 * in ndbm.h directly to the _dbm_XXX routines, so they do not
	 * need weak symbols.
	 */
	#pragma weak dbm_open     = _dbm_open
	#pragma weak dbm_close    = _dbm_close
	#pragma weak dbm_forder   = _dbm_forder_compat
	#pragma weak dbm_fetch    = _dbm_fetch_compat
	#pragma weak dbm_delete   = _dbm_delete_compat
	#pragma weak dbm_store    = _dbm_store_compat
	#pragma weak dbm_firstkey = _dbm_firstkey_compat
	#pragma weak dbm_nextkey  = _dbm_nextkey_compat
	#pragma weak dbm_clearerr = _dbm_clearerr
	#pragma weak dbm_error    = _dbm_error

	#pragma weak dbm_open64     = _dbm_open
	#pragma weak dbm_close64    = _dbm_close
	#pragma weak dbm_forder64   = _dbm_forder_compat
	#pragma weak dbm_fetch64    = _dbm_fetch_compat
	#pragma weak dbm_delete64   = _dbm_delete_compat
	#pragma weak dbm_store64    = _dbm_store_compat
	#pragma weak dbm_firstkey64 = _dbm_firstkey_compat
	#pragma weak dbm_nextkey64  = _dbm_nextkey_compat
	#pragma weak dbm_clearerr64 = _dbm_clearerr
	#pragma weak dbm_error64    = _dbm_error

	/*
	 * These are for use by callers of the dbm_XXX64 routines
	 * from within libc and by the static routines in ndbm.h.
	 * Synonyms.h or the static functions will have remapped
	 * them to _dbm_XXX64, and here we map them to the non 64
	 * versions.
	 */
	#pragma weak _dbm_open64     = _dbm_open
	#pragma weak _dbm_close64    = _dbm_close
	#pragma weak _dbm_forder64   = _dbm_forder
	#pragma weak _dbm_fetch64    = _dbm_fetch
	#pragma weak _dbm_delete64   = _dbm_delete
	#pragma weak _dbm_store64    = _dbm_store
	#pragma weak _dbm_firstkey64 = _dbm_firstkey
	#pragma weak _dbm_nextkey64  = _dbm_nextkey
	#pragma weak _dbm_clearerr64 = _dbm_clearerr
	#pragma weak _dbm_error64    = _dbm_error
#endif
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)ndbm.c	5.2 (Berkeley) 6/19/85";
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <limits.h>
#include <stdio.h>
#include <bstring.h>		/* for prototyping */
#include <stdlib.h>		/* for prototyping */
#include <string.h>		/* for prototyping */
#include <fcntl.h>		/* for prototyping */
#include <unistd.h>		/* for prototyping */
#include <errno.h>
#define	_DBM_INTERNAL		/* keep out static functions */
#include <ndbm.h>
#include "gen_extern.h"

#define BYTESIZ 8
#undef setbit

/*
 * Forward References
 */
static	int	additem(char buf[], datum item, datum item1);
static	int	delitem(char buf[], int n);
static  long	dcalchash(datum item);
static	void	dbm_access(DBM *db, long hash);
static	int	getbit(DBM *db);
static  datum	makdatum(char buf[], int i);
static	void	setbit(DBM *db);

int		_finddatum(char buf[], datum item);

DBM *
dbm_open(const char *file, int flags, mode_t mode)
{
	struct stat64 statb;
	register DBM *db;

	if(!file)
		return ((DBM *)0);

	if ((db = (DBM *)malloc(sizeof *db)) == 0) {
		_setoserror(ENOMEM);
		return ((DBM *)0);
	}
	db->dbm_flags = (flags & 03) == O_RDONLY ? _DBM_RDONLY : 0;
	if ((flags & 03) == O_WRONLY)
		flags = (flags & ~03) | O_RDWR;
	strcpy(db->dbm_pagbuf, file);
	strcat(db->dbm_pagbuf, ".pag");
	db->dbm_pagf = open(db->dbm_pagbuf, flags, mode);
	if (db->dbm_pagf < 0)
		goto bad;

	/*
	 * Added to stay out of trouble with large databases on xfs
	 */
	fstat64(db->dbm_pagf, &statb);
	if (statb.st_size >= LONG_MAX){
		fprintf(stderr,"Data base too large. Use dbm64 routines");
		goto bad1;
	}
	strcpy(db->dbm_pagbuf, file);
	strcat(db->dbm_pagbuf, ".dir");
	db->dbm_dirf = open(db->dbm_pagbuf, flags, mode);
	if (db->dbm_dirf < 0)
		goto bad1;
	fstat64(db->dbm_dirf, &statb);
	/*
	 * Added to stay out of trouble with large databases on xfs
	 */
	if((long long)statb.st_size*BYTESIZ-1 >= (long long)LONG_MAX){
		fprintf(stderr,"Data base too large. Use dbm64 routines");
		goto bad2;
	}
	db->dbm_maxbno = (long)(statb.st_size*BYTESIZ-1);
	db->dbm_pagbno = db->dbm_dirbno = -1;
	return (db);
bad2:
	(void) close(db->dbm_dirf);
bad1:
	(void) close(db->dbm_pagf);
bad:
	free((char *)db);
	return ((DBM *)0);
}

void
dbm_close(DBM *db)
{

	(void) close(db->dbm_dirf);
	(void) close(db->dbm_pagf);
	free((char *)db);
}

int
_dbm_clearerr(DBM *db)
{
	db->dbm_flags &= ~_DBM_IOERR;
	return(0);
}

int
_dbm_error(DBM *db)
{
	int ret = (db->dbm_flags & _DBM_IOERR);
	return(ret);
}

long
_dbm_forder(register DBM *db, datum key)
{
	long hash;

	hash = dcalchash(key);
	for (db->dbm_hmask=0;; db->dbm_hmask=(db->dbm_hmask<<1)+1) {
		db->dbm_blkno = hash & db->dbm_hmask;
		db->dbm_bitno = db->dbm_blkno + db->dbm_hmask;
		if (getbit(db) == 0)
			break;
	}
	return (db->dbm_blkno);
}

datum
_dbm_fetch(register DBM *db, datum key)
{
	register int i;
	datum item;

	if (dbm_error(db))
		goto err;
	/*
	 * dbm_access() will fetch the page where the item described
	 * by the given key should be.  If finddatum() can find it
	 * in the page then return it, but otherwise return a NULL
	 * item.
	 */
	dbm_access(db, dcalchash(key));
	if ((i = _finddatum(db->dbm_pagbuf, key)) >= 0) {
		item = makdatum(db->dbm_pagbuf, i+1);
		if (item.dptr != NULL)
			return (item);
	}
err:
	item.dptr = NULL;
	item.dsize = 0;
	return (item);
}

int
_dbm_delete(register DBM *db, datum key)
{
	register int i;

	if (dbm_error(db))
		return (-1);
	if (dbm_rdonly(db)) {
		_setoserror(EPERM);
		return (-1);
	}
	/*
	 * dbm_access() will fetch the page where the item described
	 * by the given key should be.  If finddatum() can find it
	 * in the page then delete it and write out the new version
	 * of the page.  Otherwise return an error.
	 */
	dbm_access(db, dcalchash(key));
	if ((i = _finddatum(db->dbm_pagbuf, key)) < 0)
		return (-1);
	if (!delitem(db->dbm_pagbuf, i))
		goto err;
	db->dbm_pagbno = db->dbm_blkno;
	(void) lseek(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
	err:
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	return (0);
}

int
_dbm_store(register DBM *db, datum key, datum dat, int replace)
{
	register int i;
	off_t seekval;
	datum item, item1;
	char ovfbuf[_PBLKSIZ];

	if (dbm_error(db))
		return (-1);
	if (dbm_rdonly(db)) {
		_setoserror(EPERM);
		return (-1);
	}
loop:
	/*
	 * Get dbm_pagbuf set to contain the data of the block where
	 * this entry would live if it was already there.
	 */
	dbm_access(db, dcalchash(key));
	/*
	 * Look for the entry in the buffer.  If it is there, then
	 * either fail or try to delete the old copy so that we can
	 * add a new one.
	 */
	if ((i = _finddatum(db->dbm_pagbuf, key)) >= 0) {
		if (!replace) {
			return (1);
		}
		if (!delitem(db->dbm_pagbuf, i)) {
			db->dbm_flags |= _DBM_IOERR;
			return (-1);
		}
	}
	/*
	 * If the entry won't fit in the block, then try to split
	 * the block and try again.
	 */
	if (!additem(db->dbm_pagbuf, key, dat))
		goto split;
	db->dbm_pagbno = db->dbm_blkno;
	seekval = lseek(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (seekval == (off_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	return (0);

split:
	/*
	 * If the entry couldn't fit in a block all by itself, then
	 * just fail it.
	 */
	if (key.dsize+dat.dsize+3*(int)(sizeof(short)) >= _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		_setoserror(EINVAL);
		return (-1);
	}
	/*
	 * We limit how far we'll allow the hash mask to grow so
	 * that we don't try to write beyond our maximum file size
	 * when performing a block split.
	 */
	if (db->dbm_hmask >= DBM_MAXHASH) {
		db->dbm_flags |= _DBM_IOERR;
		_setoserror(EFBIG);
		return (-1);
	}
	bzero(ovfbuf, _PBLKSIZ);
	for (i=0;;) {
		item = makdatum(db->dbm_pagbuf, i);
		if (item.dptr == NULL)
			break;
		/*
		 * Only move the entries for which the new hash
		 * mask bit is significant.  The others stay behind
		 * since they still hash to the same block even
		 * with the new mask.
		 */
		if (dcalchash(item) & (db->dbm_hmask+1)) {
			item1 = makdatum(db->dbm_pagbuf, i+1);
			if (item1.dptr == NULL) {
				fprintf(stderr, "ndbm: split not paired\n");
				db->dbm_flags |= _DBM_IOERR;
				break;
			}
			if (!additem(ovfbuf, item, item1) ||
			    !delitem(db->dbm_pagbuf, i)) {
				db->dbm_flags |= _DBM_IOERR;
				return (-1);
			}
			continue;
		}
		i += 2;
	}
	/*
	 * First write the new, further out block.  If we can't,
	 * then don't write out either so that we don't lose the
	 * data in the block we're splitting.
	 */
	seekval = lseek(db->dbm_pagf,
			(db->dbm_blkno+db->dbm_hmask+1)*_PBLKSIZ, L_SET);
	if (seekval == (off_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, ovfbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	db->dbm_pagbno = db->dbm_blkno;
	seekval = lseek(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (seekval == (off_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}

	/*
	 * Mark the new block as allocated in the .dir bitmap.  This
	 * indicates to future dbm_access() calls that there is data
	 * out here.
	 */
	setbit(db);
	goto loop;
}

datum
_dbm_firstkey(DBM *db)
{

	db->dbm_blkptr = 0L;
	db->dbm_keyptr = 0;
	return (_dbm_nextkey(db));
}

datum
_dbm_nextkey(register DBM *db)
{
	struct stat64 statb;
	datum item;

	if (dbm_error(db) || fstat64(db->dbm_pagf, &statb) < 0)
		goto err;
	statb.st_size /= _PBLKSIZ;
	for (;;) {
		if (db->dbm_blkptr != db->dbm_pagbno) {
			db->dbm_pagbno = db->dbm_blkptr;
			(void) lseek(db->dbm_pagf, db->dbm_blkptr*_PBLKSIZ, L_SET);
			if (read(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ)
				bzero(db->dbm_pagbuf, _PBLKSIZ);
#ifdef DEBUG
			else if (chkblk(db->dbm_pagbuf) < 0)
				db->dbm_flags |= _DBM_IOERR;
#endif
		}
		if (((short *)db->dbm_pagbuf)[0] != 0) {
			item = makdatum(db->dbm_pagbuf, db->dbm_keyptr);
			if (item.dptr != NULL) {
				db->dbm_keyptr += 2;
				return (item);
			}
			db->dbm_keyptr = 0;
		}
		if (++db->dbm_blkptr >= statb.st_size)
			break;
	}
err:
	item.dptr = NULL;
	item.dsize = 0;
	return (item);
}

static void
dbm_access(register DBM *db, long hash)
{
	off_t	seekval;

	/*
	 * Follow the path of allocated blocks that match our
	 * hash value until we see that the next block has not
	 * yet been allocated.  At each step we incorporate the
	 * next bit of hash value. As dbm_store() adds new blocks and
	 * increases the size to which the hash mask can grow before
	 * getbit() returns 0, we move forward all the entries in the
	 * current "node" (block) for which the new bit in the hash
	 * mask is set.  This makes for a very funky data structure
	 * in the .pag file that takes advantage of the fact that the
	 * number of addressable blocks doubles each time we add a bit
	 * to the hash mask.
	 *
	 * We limit the maximum size of the hash mask in dbm_store()
	 * so that we keep within the addressable block range.
	 */
	for (db->dbm_hmask=0;; db->dbm_hmask=(db->dbm_hmask<<1)+1) {
		db->dbm_blkno = hash & db->dbm_hmask;
		db->dbm_bitno = db->dbm_blkno + db->dbm_hmask;
		if (getbit(db) == 0)
			break;
	}
	/*
	 * Just fetch the page where the item with the given hask
	 * value should be.
	 */
	if (db->dbm_blkno != db->dbm_pagbno) {
		db->dbm_pagbno = db->dbm_blkno;
		seekval = lseek(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
		if (seekval == (off_t)-1) {
			bzero(db->dbm_pagbuf, _PBLKSIZ);
			return;
		}
		if (read(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ)
			bzero(db->dbm_pagbuf, _PBLKSIZ);
#ifdef DEBUG
		else if (chkblk(db->dbm_pagbuf) < 0)
			db->dbm_flags |= _DBM_IOERR;
#endif
	}
}

static int
getbit(register DBM *db)
{
	long bn;
	register long b, i, n;
	
	/*
	 * The .dir file is just a plain, linear bitmap of the
	 * blocks allocated in the .pag file.  This routine
	 * returns whether the block number indicated by dbm_bitno
	 * is allocated or not.
	 */
	if (db->dbm_bitno > db->dbm_maxbno)
		return (0);
	n = db->dbm_bitno % BYTESIZ;
	bn = db->dbm_bitno / BYTESIZ;
	i = bn % _DBLKSIZ;
	b = bn / _DBLKSIZ;
	if (b != db->dbm_dirbno) {
		db->dbm_dirbno = b;
		(void) lseek(db->dbm_dirf, b*_DBLKSIZ, L_SET);
		if (read(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
			bzero(db->dbm_dirbuf, _DBLKSIZ);
	}
	return (db->dbm_dirbuf[i] & (1<<n));
}

static void
setbit(register DBM *db)
{
	long bn;
	register long i, n, b;

	/*
	 * The .dir file is just a plain, linear bitmap of the
	 * blocks allocated in the .pag file.  This routine
	 * sets  the block number indicated by dbm_bitno to be
	 * allocated.
	 */
	if (db->dbm_bitno > db->dbm_maxbno)
		db->dbm_maxbno = db->dbm_bitno;
	n = db->dbm_bitno % BYTESIZ;
	bn = db->dbm_bitno / BYTESIZ;
	i = bn % _DBLKSIZ;
	b = bn / _DBLKSIZ;
	if (b != db->dbm_dirbno) {
		db->dbm_dirbno = b;
		(void) lseek(db->dbm_dirf, b*_DBLKSIZ, L_SET);
		if (read(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
			bzero(db->dbm_dirbuf, _DBLKSIZ);
	}
	db->dbm_dirbuf[i] |= 1<<n;
	db->dbm_dirbno = b;
	(void) lseek(db->dbm_dirf, b*_DBLKSIZ, L_SET);
	if (write(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
		db->dbm_flags |= _DBM_IOERR;
}

static datum
makdatum(char buf[_PBLKSIZ], int n)
{
	register short *sp;
	register int t;
	datum item;

	/*
	 * This actually makes a datum for the item pointed to
	 * by entry n+1 in the sp array.  I suppose that makes
	 * it a virtual array that starts at index 1.
	 *
	 * The sp[0] value is the number of entries in the sp
	 * array (starting with 1).  The other sp[] entries
	 * point to the start of their data.
	 * The data areas for the items are actually stored in
	 * reverse order and are kept packed up against the
	 * end of the page.  For example, the data for entry 1
	 * (sp[1]) is at the very end of the page. The size
	 * of the data can be calculated by subtracting the
	 * the offset of the entry you want, sp[n+1], from the
	 * start of the previous entry sp[n] with a special case
	 * for the first entry.  Remember that the data for the
	 * entries is in reverse order from the entries in the
	 * sp[] array.
	 */
	sp = (short *)buf;
	if ((short)n >= sp[0]) {
		item.dptr = NULL;
		item.dsize = 0;
		return (item);
	}
	t = _PBLKSIZ;
	if (n > 0)
		t = sp[n];
	item.dptr = buf+sp[n+1];
	item.dsize = t - sp[n+1];
	return (item);
}

int
_finddatum(char buf[_PBLKSIZ], datum item)
{
	register short *sp;
	register int i, n, j;

	sp = (short *)buf;
	n = _PBLKSIZ;
	for (i=0, j=sp[0]; i<j; i+=2, n = sp[i]) {
		n -= sp[i+1];
		if (n != item.dsize)
			continue;
		if (n == 0 || bcmp(&buf[sp[i+1]], item.dptr, n) == 0) {
			return (i);
		}
	}
	return (-1);
}

static const int hitab[16]
/* ken's
{
	055,043,036,054,063,014,004,005,
	010,064,077,000,035,027,025,071,
};
*/
 = {    61, 57, 53, 49, 45, 41, 37, 33,
	29, 25, 21, 17, 13,  9,  5,  1,
};
static const long hltab[64]
 = {
	06100151277L,06106161736L,06452611562L,05001724107L,
	02614772546L,04120731531L,04665262210L,07347467531L,
	06735253126L,06042345173L,03072226605L,01464164730L,
	03247435524L,07652510057L,01546775256L,05714532133L,
	06173260402L,07517101630L,02431460343L,01743245566L,
	00261675137L,02433103631L,03421772437L,04447707466L,
	04435620103L,03757017115L,03641531772L,06767633246L,
	02673230344L,00260612216L,04133454451L,00615531516L,
	06137717526L,02574116560L,02304023373L,07061702261L,
	05153031405L,05322056705L,07401116734L,06552375715L,
	06165233473L,05311063631L,01212221723L,01052267235L,
	06000615237L,01075222665L,06330216006L,04402355630L,
	01451177262L,02000133436L,06025467062L,07121076461L,
	03123433522L,01010635225L,01716177066L,05161746527L,
	01736635071L,06243505026L,03637211610L,01756474365L,
	04723077174L,03642763134L,05750130273L,03655541561L,
};

#ifndef	sgi
static long
hashinc(register DBM *db, long hash)
{
	long bit;

	hash &= db->dbm_hmask;
	bit = db->dbm_hmask+1;
	for (;;) {
		bit >>= 1;
		if (bit == 0)
			return (0L);
		if ((hash & bit) == 0)
			return (hash | bit);
		hash &= ~bit;
	}
}
#endif

static long
dcalchash(datum item)
{
	register int s, c, j;
	register char *cp;
	register long hashl;
	register int hashi;

	hashl = 0;
	hashi = 0;
	for (cp = item.dptr, s=(int)item.dsize; --s >= 0; ) {
		c = *cp++;
		for (j=0; j<BYTESIZ; j+=4) {
			hashi += hitab[c&017];
			hashl += hltab[hashi&63];
			c >>= 4;
		}
	}
	return (hashl);
}

/*
 * Delete pairs of items (n & n+1).
 * This routine treats the entries like an array indexed starting
 * at 0 (as opposed to makedatum()).  It deletes the pair of
 * entries, ie the key and data entries.
 *
 * If the entries are the last ones in the array, then the number
 * of entries in the array is simply decremented and we just forget
 * about them.  If the entries to be deleted are not at the end,
 * then they are removed from the array, the entries after them in
 * the array are slid down to cover the deleted ones, and the data
 * for the entries after them is slid forward over top of them.
 */
static int
delitem(char buf[_PBLKSIZ], int n)
{
	register short *sp, *sp1;
	register int i1, i2;

	sp = (short *)buf;
	i2 = sp[0];
	/*
	 * If the index is greater than or equal to the number
	 * of entries or we've specified an odd entry, fail.
	 */
	if (n >= i2 || (n & 1))
		return (0);
	/*
	 * If it is the last pair of entries that we want to delete,
	 * then just decrement the number of entries and forget about
	 * them.
	 */
	if (n == i2-2) {
		sp[0] -= 2;
		return (1);
	}
	/*
	 * We have to actually delete the entries from the beginning
	 * or middle of the array.  First we move the data.
	 *
	 * First we set i1 to be the offset of the byte after the
	 * data we're deleting.  If we're deleting the first entry,
	 * then this is at the end of the page.  Otherwise it is
	 * the beginning of the entry before us (in the sp array).
	 * Since deleting entries n and n+1 really means deleting
	 * entries sp[n+1] and sp[n+2], sp[n] is the beginning of the
	 * entry before us.
	 */
	i1 = _PBLKSIZ;
	if (n > 0)
		i1 = sp[n];
	/*
	 * Now that i1 contains the offset of the end of the data
	 * we're deleting, subtract out the offset of the beginning
	 * of the data to get its length.  We're deleting sp[n+1] and
	 * sp[n+2], so sp[n+2] is the beginning of the data.
	 */
	i1 -= sp[n+2];
	if (i1 > 0) {
		/*
		 * Now slide the data for the entries after those
		 * we're deleting in the sp array over top of the
		 * data we're deleting.  i2 is just the value of
		 * sp[0], so sp[i2] is the offset of the last entry
		 * in the sp array, ie the first byte of data in the
		 * page.  We slide it forward by i1 bytes, the length
		 * of the data we're deleting.  The number of bytes
		 * we move is the offset of the first byte of data
		 * we're deleting minus the first byte of data in
		 * the page.  That's where sp[n+2] (the start of the
		 * data we're deleting) - i2 (once we've assigned
		 * i2 = sp[i2]) comes from.
		 */
		i2 = sp[i2];
		bcopy(&buf[i2], &buf[i2 + i1], sp[n+2] - i2);
	}
	/*
	 * Decrement the number of entries in the sp array and
	 * slide down the entries above those we deleted from
	 * the array.  For each one we slide down, increment its
	 * value by the number of bytes we slid its data forward,
	 * ie the length of the deleted entries i1.
	 */
	sp[0] -= 2;
	for (sp1 = sp + sp[0], sp += n+1; sp <= sp1; sp++)
		sp[0] = (short)(sp[2] + i1);
	return (1);
}

/*
 * Add pairs of items (item & item1).
 * Entries are always added in pairs.  The sp array at the beginning
 * of the page points to the start of the data at that index, with
 * the exception that the sp[0] entry describes the number of entries
 * in the array starting at 1.  The data for the entries is kept
 * packed in reverse order from the end of the page.  That is, the
 * data for entry sp[1] is packed against the end of the page, the
 * data for entry sp[2] ends at the start of entry sp[1], and so on.
 * The new entries are added at the end of the sp array and their
 * data is packed before the existing entries in the page.
 */
static int
additem(char buf[_PBLKSIZ], datum item, datum item1)
{
	register short *sp;
	register int i1, i2;

	sp = (short *)buf;
	/*
	 * First set i1 to be the offset of the first used data
	 * byte in the page (if it is empty this is just past the
	 * end of the page) and i2 to be the number of entries
	 * in the page.  If the page is not empty the first used
	 * data byte will be pointed to by the last entry in the
	 * sp array since the data is in reverse order from the array.
	 */
	i1 = _PBLKSIZ;
	i2 = sp[0];
	if (i2 > 0)
		i1 = sp[i2];
	/*
	 * Now check to see if there is room in the page for the
	 * new items.  First we subtract out the length of the
	 * items to be added from the remaining space, and then
	 * we take into account the size of the sp array at the
	 * beginning of the page.  If it won't fit we return 0.
	 */
	i1 -= item.dsize + item1.dsize;
#if defined(__sgi)
	if (i1 <= (i2+3) * (int)(sizeof(short)))
#else
	if (i1 <= (i2+3) * sizeof(short))
#endif
		return (0);
	/*
	 * The new items will fit, so bump the count of items kept
	 * in sp[0] and copy in the data for the new items.  The
	 * sp array entry for each new item is set to point at the
	 * beginning of the data for that entry.
	 */
	sp[0] += 2;
	sp[++i2] = (short)(i1 + item1.dsize);
	bcopy(item.dptr, &buf[i1 + item1.dsize], (int)item.dsize);
	sp[++i2] = (short)i1;
	bcopy(item1.dptr, &buf[i1], (int)item1.dsize);
	return (1);
}

#ifdef DEBUG
static
chkblk(char buf[_PBLKSIZ])
{
	register short *sp;
	register t, i;

	sp = (short *)buf;
	t = _PBLKSIZ;
	for (i=0; i<sp[0]; i++) {
		if (sp[i+1] > t)
			return (-1);
		t = sp[i+1];
	}
	if (t < (sp[0]+1)*sizeof(short))
		return (-1);
	return (0);
}

dumpdbm(char *buf)
{
	register short *sp;
	register t, i;

	sp = (short *)buf;
	t = _PBLKSIZ;
	for (i=0; i<sp[0]; i++) {
		if (sp[i+1] > t)
			return (-1);
		t = sp[i+1];
		printf("[%s]", &buf[t]);
	}
	if (t < (sp[0]+1)*sizeof(short))
		return (-1);
	printf("\n");
	return (0);
}
#endif

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

long
dbm_forder_compat(register DBM *db, odatum okey)
{
	datum	key;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	return _dbm_forder(db, key);
}


odatum
dbm_fetch_compat(register DBM *db, odatum okey)
{
	datum	key;
	datum	item;
	odatum	oitem;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	item = _dbm_fetch(db, key);
	oitem.dptr = item.dptr;
	oitem.dsize = (int)item.dsize;
	return oitem;
}

int
dbm_delete_compat(register DBM *db, odatum okey)
{
	datum	key;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	return _dbm_delete(db, key);
}

int
dbm_store_compat(register DBM *db, odatum okey, odatum odat, int replace)
{
	datum	key;
	datum	dat;

	key.dptr = okey.dptr;
	key.dsize = okey.dsize;
	dat.dptr = odat.dptr;
	dat.dsize = odat.dsize;	
	return _dbm_store(db, key, dat, replace);
}


odatum
dbm_firstkey_compat(DBM *db)
{
	datum	key;
	odatum	okey;

	key = _dbm_firstkey(db);
	okey.dptr = key.dptr;
	okey.dsize = (int)key.dsize;
	return okey;
}


odatum
dbm_nextkey_compat(register DBM *db)
{
	datum	key;
	odatum	okey;

	key = _dbm_nextkey(db);
	okey.dptr = key.dptr;
	okey.dsize = (int)key.dsize;
	return okey;
}

#endif /* (_MIPS_SIM == _MIPS_SIM_ABI64) */
