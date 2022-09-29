/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)dbm.c	8.6 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <ndbm.h>
#include "hash.h"

/*
 *
 * This package provides dbm and ndbm compatible interfaces to DB.
 * First are the DBM routines, which call the NDBM routines, and
 * the NDBM routines, which call the DB routines.
 */
static DBM *__cur_db;

static void no_open_db __P((void));

int
dbminit(file)
	char *file;
{
	if (__cur_db != NULL)
		(void)dbm_close(__cur_db);
	if ((__cur_db = dbm_open(file, O_RDWR, 0)) != NULL)
		return (0);
	if ((__cur_db = dbm_open(file, O_RDONLY, 0)) != NULL)
		return (0);
	return (-1);
}

datum
fetch(key)
	datum key;
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		return (item);
	}
	return (dbm_fetch(__cur_db, key));
}

datum
firstkey()
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		return (item);
	}
	return (dbm_firstkey(__cur_db));
}

datum
nextkey(key)
	datum key;
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		return (item);
	}
	return (dbm_nextkey(__cur_db));
}

int
delete(key)
	datum key;
{
	if (__cur_db == NULL) {
		no_open_db();
		return (-1);
	}
	if (dbm_rdonly(__cur_db))
		return (-1);
	return (dbm_delete(__cur_db, key));
}

int
store(key, dat)
	datum key, dat;
{
	if (__cur_db == NULL) {
		no_open_db();
		return (-1);
	}
	if (dbm_rdonly(__cur_db))
		return (-1);
	return (dbm_store(__cur_db, key, dat, DBM_REPLACE));
}

static void
no_open_db()
{
	(void)fprintf(stderr, "dbm: no open database.\n");
}

/*
 * Returns:
 * 	*DBM on success
 *	 NULL on failure
 */
DBM *
dbm_open(file, flags, mode)
	const char *file;
	int flags, mode;
{
	HASHINFO info;
	char path[MAXPATHLEN];

	info.bsize = 4096;
	info.ffactor = 40;
	info.nelem = 1;
	info.cachesize = 0;
	info.hash = NULL;
	info.lorder = 0;
	(void)strcpy(path, file);
	(void)strcat(path, DBM_SUFFIX);
	return ((DBM *)__hash_open(path, flags, mode, &info, 0));
}

/*
 * Returns:
 *	Nothing.
 */
void
dbm_close(db)
	DBM *db;
{
	(void)(db->close)(db);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_fetch(db, key)
	DBM *db;
	datum key;
{
	datum retval;
	int status;

	status = (db->get)(db, (DBT *)&key, (DBT *)&retval, 0);
	if (status) {
		retval.dptr = NULL;
		retval.dsize = 0;
	}
	return (retval);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_firstkey(db)
	DBM *db;
{
	int status;
	datum retdata, retkey;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_FIRST);
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_nextkey(db)
	DBM *db;
{
	int status;
	datum retdata, retkey;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_NEXT);
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 */
int
dbm_delete(db, key)
	DBM *db;
	datum key;
{
	int status;

	status = (db->del)(db, (DBT *)&key, 0);
	if (status)
		return (-1);
	else
		return (0);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 *	 1 if DBM_INSERT and entry exists
 */
int
dbm_store(db, key, content, flags)
	DBM *db;
	datum key, content;
	int flags;
{
	return ((db->put)(db, (DBT *)&key, (DBT *)&content,
	    (flags == DBM_INSERT) ? R_NOOVERWRITE : 0));
}

int
dbm_error(db)
	DBM *db;
{
	HTAB *hp;

	hp = (HTAB *)db->internal;
	return (hp->errno);
}

int
dbm_clearerr(db)
	DBM *db;
{
	HTAB *hp;

	hp = (HTAB *)db->internal;
	hp->errno = 0;
	return (0);
}

int
dbm_dirfno(db)
	DBM *db;
{
	return(((HTAB *)db->internal)->fp);
}
