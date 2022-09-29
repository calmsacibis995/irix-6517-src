/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef __STDC__
	#pragma weak dbm_open64     = _dbm_open64
	#pragma weak dbm_close64    = _dbm_close64
	#pragma weak dbm_forder64   = _dbm_forder64
	#pragma weak dbm_fetch64    = _dbm_fetch64
	#pragma weak dbm_delete64   = _dbm_delete64
	#pragma weak dbm_store64    = _dbm_store64
	#pragma weak dbm_firstkey64 = _dbm_firstkey64
	#pragma weak dbm_error64    = _dbm_error64
	#pragma weak dbm_clearerr64 = _dbm_clearerr64
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
#include <ndbm.h>
#include "gen_extern.h"

#define BYTESIZ 8
#undef setbit

/*
 * Forward References
 */
static	int	additem(char buf[], datum item, datum item1);
static	int	delitem(char buf[], int n);
static  long long	dcalchash(datum item);
static	void	dbm_access(DBM64 *db, long long hash);
static	int	getbit(DBM64 *db);
static  datum	makdatum(char buf[], int i);
static	void	setbit(DBM64 *db);

int		_finddatum64(char buf[], datum item);
DBM64 *
dbm_open64(const char *file, int flags, mode_t mode)
{
	struct stat64 statb;
	register DBM64 *db;

	if(!file)
		return ((DBM64 *)0);

	if ((db = (DBM64 *)malloc(sizeof *db)) == 0) {
		_setoserror(ENOMEM);
		return ((DBM64 *)0);
	}
	db->dbm_flags = (flags & 03) == O_RDONLY ? _DBM_RDONLY : 0;
	if ((flags & 03) == O_WRONLY)
		flags = (flags & ~03) | O_RDWR;
	strcpy(db->dbm_pagbuf, file);
	strcat(db->dbm_pagbuf, ".pag");
	db->dbm_pagf = open(db->dbm_pagbuf, flags, mode);
	if (db->dbm_pagf < 0)
		goto bad;
	strcpy(db->dbm_pagbuf, file);
	strcat(db->dbm_pagbuf, ".dir");
	db->dbm_dirf = open(db->dbm_pagbuf, flags, mode);
	if (db->dbm_dirf < 0)
		goto bad1;
	fstat64(db->dbm_dirf, &statb);
	db->dbm_maxbno = statb.st_size*BYTESIZ-1;
	db->dbm_pagbno = db->dbm_dirbno = -1;
	return (db);
bad1:
	(void) close(db->dbm_pagf);
bad:
	free((char *)db);
	return ((DBM64 *)0);
}

void
dbm_close64(DBM64 *db)
{

	(void) close(db->dbm_dirf);
	(void) close(db->dbm_pagf);
	free((char *)db);
}

int
_dbm_clearerr64(DBM64 *db)
{
	db->dbm_flags &= ~_DBM_IOERR;
	return(0);
}

int
_dbm_error64(DBM64 *db)
{
	int ret = (db->dbm_flags & _DBM_IOERR);
	return(ret);
}

long long
_dbm_forder64(register DBM64 *db, datum key)
{
	long long hash;

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
_dbm_fetch64(register DBM64 *db, datum key)
{
	register int i;
	datum item;

	if (dbm_error64(db))
		goto err;
	dbm_access(db, dcalchash(key));
	if ((i = _finddatum64(db->dbm_pagbuf, key)) >= 0) {
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
_dbm_delete64(register DBM64 *db, datum key)
{
	register int i;

	if (dbm_error64(db))
		return (-1);
	if (dbm_rdonly(db)) {
		_setoserror(EPERM);
		return (-1);
	}
	dbm_access(db, dcalchash(key));
	if ((i = _finddatum64(db->dbm_pagbuf, key)) < 0)
		return (-1);
	if (!delitem(db->dbm_pagbuf, i))
		goto err;
	db->dbm_pagbno = db->dbm_blkno;
	(void) lseek64(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
	err:
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	return (0);
}

int
_dbm_store64(register DBM64 *db, datum key, datum dat, int replace)
{
	register int i;
	off64_t seekval;
	datum item, item1;
	char ovfbuf[_PBLKSIZ];

	if (dbm_error64(db))
		return (-1);
	if (dbm_rdonly(db)) {
		_setoserror(EPERM);
		return (-1);
	}
loop:
	dbm_access(db, dcalchash(key));
	if ((i = _finddatum64(db->dbm_pagbuf, key)) >= 0) {
		if (!replace)
			return (1);
		if (!delitem(db->dbm_pagbuf, i)) {
			db->dbm_flags |= _DBM_IOERR;
			return (-1);
		}
	}
	if (!additem(db->dbm_pagbuf, key, dat))
		goto split;
	db->dbm_pagbno = db->dbm_blkno;
	seekval = lseek64(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (seekval == (off64_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	return (0);

split:
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
	if (db->dbm_hmask >= DBM_MAXHASH64) {
		db->dbm_flags |= _DBM_IOERR;
		_setoserror(EFBIG);
		return (-1);
	}
	bzero(ovfbuf, _PBLKSIZ);
	for (i=0;;) {
		item = makdatum(db->dbm_pagbuf, i);
		if (item.dptr == NULL)
			break;
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
	seekval = lseek64(db->dbm_pagf,
			  (db->dbm_blkno+db->dbm_hmask+1)*_PBLKSIZ, L_SET);
	if (seekval == (off64_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, ovfbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	db->dbm_pagbno = db->dbm_blkno;
	seekval = lseek64(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
	if (seekval == (off64_t)-1) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}
	if (write(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ) {
		db->dbm_flags |= _DBM_IOERR;
		return (-1);
	}

	setbit(db);
	goto loop;
}

datum
_dbm_firstkey64(DBM64 *db)
{

	db->dbm_blkptr = 0L;
	db->dbm_keyptr = 0;
	return (_dbm_nextkey64(db));
}

datum
_dbm_nextkey64(register DBM64 *db)
{
	struct stat64 statb;
	datum item;

	if (dbm_error64(db) || fstat64(db->dbm_pagf, &statb) < 0)
		goto err;
	statb.st_size /= _PBLKSIZ;
	for (;;) {
		if (db->dbm_blkptr != db->dbm_pagbno) {
			db->dbm_pagbno = db->dbm_blkptr;
			(void) lseek64(db->dbm_pagf, db->dbm_blkptr*_PBLKSIZ, L_SET);
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
dbm_access(register DBM64 *db, long long hash)
{

	for (db->dbm_hmask=0;; db->dbm_hmask=(db->dbm_hmask<<1)+1) {
		db->dbm_blkno = hash & db->dbm_hmask;
		db->dbm_bitno = db->dbm_blkno + db->dbm_hmask;
		if (getbit(db) == 0)
			break;
	}
	if (db->dbm_blkno != db->dbm_pagbno) {
		db->dbm_pagbno = db->dbm_blkno;
		(void) lseek64(db->dbm_pagf, db->dbm_blkno*_PBLKSIZ, L_SET);
		if (read(db->dbm_pagf, db->dbm_pagbuf, _PBLKSIZ) != _PBLKSIZ)
			bzero(db->dbm_pagbuf, _PBLKSIZ);
#ifdef DEBUG
		else if (chkblk(db->dbm_pagbuf) < 0)
			db->dbm_flags |= _DBM_IOERR;
#endif
	}
}

static int
getbit(register DBM64 *db)
{
	long long bn, b;
	register int i, n;
	

	if (db->dbm_bitno > db->dbm_maxbno)
		return (0);
	n = db->dbm_bitno % BYTESIZ;
	bn = db->dbm_bitno / BYTESIZ;
	i = bn % _DBLKSIZ;
	b = bn / _DBLKSIZ;
	if (b != db->dbm_dirbno) {
		db->dbm_dirbno = b;
		(void) lseek64(db->dbm_dirf, (long)b*_DBLKSIZ, L_SET);
		if (read(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
			bzero(db->dbm_dirbuf, _DBLKSIZ);
	}
	return (db->dbm_dirbuf[i] & (1<<n));
}

static void
setbit(register DBM64 *db)
{
	long long bn, b;
	register int i, n;

	if (db->dbm_bitno > db->dbm_maxbno)
		db->dbm_maxbno = db->dbm_bitno;
	n = db->dbm_bitno % BYTESIZ;
	bn = db->dbm_bitno / BYTESIZ;
	i = bn % _DBLKSIZ;
	b = bn / _DBLKSIZ;
	if (b != db->dbm_dirbno) {
		db->dbm_dirbno = b;
		(void) lseek64(db->dbm_dirf, (long)b*_DBLKSIZ, L_SET);
		if (read(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
			bzero(db->dbm_dirbuf, _DBLKSIZ);
	}
	db->dbm_dirbuf[i] |= 1<<n;
	db->dbm_dirbno = b;
	(void) lseek64(db->dbm_dirf, (long)b*_DBLKSIZ, L_SET);
	if (write(db->dbm_dirf, db->dbm_dirbuf, _DBLKSIZ) != _DBLKSIZ)
		db->dbm_flags |= _DBM_IOERR;
}

static datum
makdatum(char buf[_PBLKSIZ], int n)
{
	register short *sp;
	register int t;
	datum item;

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
_finddatum64(char buf[_PBLKSIZ], datum item)
{
	register short *sp;
	register int i, n, j;

	sp = (short *)buf;
	n = _PBLKSIZ;
	for (i=0, j=sp[0]; i<j; i+=2, n = sp[i]) {
		n -= sp[i+1];
		if (n != item.dsize)
			continue;
		if (n == 0 || bcmp(&buf[sp[i+1]], item.dptr, n) == 0)
			return (i);
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
static const long long hlltab[64]
 = {
	06100151277LL,06106161736LL,06452611562LL,05001724107LL,
	02614772546LL,04120731531LL,04665262210LL,07347467531LL,
	06735253126LL,06042345173LL,03072226605LL,01464164730LL,
	03247435524LL,07652510057LL,01546775256LL,05714532133LL,
	06173260402LL,07517101630LL,02431460343LL,01743245566LL,
	00261675137LL,02433103631LL,03421772437LL,04447707466LL,
	04435620103LL,03757017115LL,03641531772LL,06767633246LL,
	02673230344LL,00260612216LL,04133454451LL,00615531516LL,
	06137717526LL,02574116560LL,02304023373LL,07061702261LL,
	05153031405LL,05322056705LL,07401116734LL,06552375715LL,
	06165233473LL,05311063631LL,01212221723LL,01052267235LL,
	06000615237LL,01075222665LL,06330216006LL,04402355630LL,
	01451177262LL,02000133436LL,06025467062LL,07121076461LL,
	03123433522LL,01010635225LL,01716177066LL,05161746527LL,
	01736635071LL,06243505026LL,03637211610LL,01756474365LL,
	04723077174LL,03642763134LL,05750130273LL,03655541561LL,
};

#ifndef	sgi
static long long
hashinc(register DBM64 *db, long long hash)
{
	long long bit;

	hash &= db->dbm_hmask;
	bit = db->dbm_hmask+1;
	for (;;) {
		bit >>= 1;
		if (bit == 0)
			return (0LL);
		if ((hash & bit) == 0)
			return (hash | bit);
		hash &= ~bit;
	}
}
#endif

static long long
dcalchash(datum item)
{
	register int s, c, j;
	register char *cp;
	register long long hashl;
	register int hashi;

	hashl = 0;
	hashi = 0;
	for (cp = item.dptr, s=item.dsize; --s >= 0; ) {
		c = *cp++;
		for (j=0; j<BYTESIZ; j+=4) {
			hashi += hitab[c&017];
			hashl += hlltab[hashi&63];
			c >>= 4;
		}
	}
	return (hashl);
}

/*
 * Delete pairs of items (n & n+1).
 */
static int
delitem(char buf[_PBLKSIZ], int n)
{
	register short *sp, *sp1;
	register int i1, i2;

	sp = (short *)buf;
	i2 = sp[0];
	if (n >= i2 || (n & 1))
		return (0);
	if (n == i2-2) {
		sp[0] -= 2;
		return (1);
	}
	i1 = _PBLKSIZ;
	if (n > 0)
		i1 = sp[n];
	i1 -= sp[n+2];
	if (i1 > 0) {
		i2 = sp[i2];
		bcopy(&buf[i2], &buf[i2 + i1], sp[n+2] - i2);
	}
	sp[0] -= 2;
	for (sp1 = sp + sp[0], sp += n+1; sp <= sp1; sp++)
		sp[0] = (short)(sp[2] + i1);
	return (1);
}

/*
 * Add pairs of items (item & item1).
 */
static int
additem(char buf[_PBLKSIZ], datum item, datum item1)
{
	register short *sp;
	register int i1, i2;

	sp = (short *)buf;
	i1 = _PBLKSIZ;
	i2 = sp[0];
	if (i2 > 0)
		i1 = sp[i2];
	i1 -= item.dsize + item1.dsize;
#if defined(__sgi)
	if (i1 <= (i2+3) * (int)(sizeof(short)))
#else
	if (i1 <= (i2+3) * sizeof(short))
#endif
		return (0);
	sp[0] += 2;
	sp[++i2] = (short)(i1 + item1.dsize);
	bcopy(item.dptr, &buf[i1 + item1.dsize], item.dsize);
	sp[++i2] = (short)i1;
	bcopy(item1.dptr, &buf[i1], item1.dsize);
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
