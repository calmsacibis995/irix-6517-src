/*
 * mdbm - ndbm work-alike hashed database library based on sdbm which is
 * based on Per-Aake Larson's Dynamic Hashing algorithms.
 * BIT 18 (1978).
 *
 * sdbm Copyright (c) 1991 by Ozan S. Yigit
 *
 * Modifications to use memory mapped files to enable multiple concurrent
 * users:
 *      Copyright (c) 1996 by Larry McVoy, lm@sgi.com.
 *      Copyright (c) 1996 by John Schimmel, jes@sgi.com.
 *      Copyright (c) 1996 by Andrew Chang, awc@sgi.com.
 *	Copyright (c) 1997 by Robert G. Mende Jr., mende@sgi.com.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 *			oz@nexus.yorku.ca
 *
 *			Ozan S. Yigit
 *			Dept. of Computing and Communication Svcs.
 *			Steacie Science Library. T103
 *			York University
 *			4700 Keele Street, North York
 *			Ontario M3J 1P3
 *			Canada
 */

#ifdef MDBM_LIBC
#ifdef __STDC__
	#pragma weak mdbm_close = _mdbm_close
	#pragma weak mdbm_delete = _mdbm_delete
	#pragma weak mdbm_fetch = _mdbm_fetch
	#pragma weak mdbm_first = _mdbm_first
	#pragma weak mdbm_firstkey = _mdbm_firstkey
	#pragma weak mdbm_next = _mdbm_next
	#pragma weak mdbm_nextkey = _mdbm_nextkey
	#pragma weak mdbm_open = _mdbm_open
	#pragma weak mdbm_store = _mdbm_store
	#pragma weak mdbm_sethash = _mdbm_sethash
	#pragma weak mdbm_close_fd = _mdbm_close_fd
	#pragma weak mdbm_invalidate = _mdbm_invalidate
	#pragma weak mdbm_sync = _mdbm_sync
#endif

#include "synonyms.h"
#endif /* MDBM_LIBC */

#include "common.h"
#include <abi_mutex.h>
#include <sys/param.h>

static kvpair	_mdbm_getnext(MDBM *, kvpair);
static char	*makeroom(MDBM *, uint64, int, mdbm_genlock_t *);
static void	putpair(MDBM *, char *, datum, datum);
static int	_split(struct mdbm *, datum, datum, void *);
static int	_delete(struct mdbm *, datum, datum, void *);
static int	addpage(MDBM *db);
static int	split_page(MDBM *, char *, char *, int (*func)(struct mdbm *, datum, datum, void *), void *);

/* 
** mdbm_open
*/

MDBM    *
mdbm_open(char *file, int flags, mode_t mode, int psize)
{
	MDBM	*db;
	MDBM	*odb=(MDBM *)0;
	char	*file2;
	int	flags2;

	debug(DEBUG_ENTRY,("mdbm_open(%s, 0%o, %o, %d)\n", 
		file, flags, mode, psize));

	if (!file || !*file) {
		setoserror(EINVAL);
		return (MDBM *)0;
	}
	if (!(db = (MDBM *)calloc(1, sizeof (MDBM)))) {
		setoserror(ENOMEM);
		return (MDBM *)0;
	}
	db->m_kino = -1;  /* just in case */
	db->m_db = (char *)MAP_FAILED;

	/*
	** Deal wtih O_SYNC
	*/
	if (flags & O_SYNC) {
		db->m_flags |= _MDBM_SYNC;
		flags &= ~O_SYNC;
	}
		
	/*
	** Deal wtih MDBM_REPAIR
	*/
	if (flags & MDBM_REPAIR) {
		db->m_flags |= _MDBM_REPAIR;
		flags &= ~MDBM_REPAIR;
	}
		
	/*
	** Deal wtih MDBM_MEMCHECK
	*/
	if (flags & MDBM_MEMCHECK) {
		db->m_flags |= _MDBM_MEMCHECK;
		flags &= ~MDBM_MEMCHECK;
	}
		
	/*
	 * adjust user flags so that WRONLY becomes RDWR,
	 * as required by this package. Also set our internal
	 * flag for RDONLY if needed.
	 */
	if (flags & O_WRONLY) {
		flags = (flags & ~O_WRONLY) | O_RDWR;
	} else if ((flags & (O_RDWR|O_WRONLY|O_RDONLY)) == O_RDONLY) {
		db->m_flags |= _MDBM_RDONLY;
		flags &= ~(O_CREAT|O_TRUNC);
	}
	
	/*
	** Flags:
	**			exists			!exists
	** O_CREAT		open			create, open
	** O_TRUNC		inval, open (new)	fail
	** O_CREAT|O_TRUNC	inval, open (new)	create, open
	*/

	file2 = file;		
	if (flags & (O_CREAT |O_TRUNC)) {
		if ((access(file, F_OK|EFF_ONLY_OK) == 0)) {
			if (!(flags & O_TRUNC)) {
				flags &= ~O_CREAT;
			} else {
				file2 = alloca((unsigned)strlen(file) + 12);
				strcpy(file2, file);
				strcat(file2,".tmp.XXXXXX");
				file2 = mktemp(file2);
				if ((odb = mdbm_open(file, O_RDWR, 0644, 0)) 
				    == NULL) {
					if (oserror() == ESTALE || 
					    oserror() == EBADF ||
					    oserror() == EBADFD) {
 		      debug(DEBUG_INFO,("Overwriteing invalid mdbm file\n"));
		      /* 
		      ** This case will cause a mdbm file to be created without
		      ** an implicit call to mdbm_invalidate().  This is OK 
		      ** since anything that is accessing it will fail.
		      */
						unlink(file);
						file2 = file;
					} else {
						debug(DEBUG_ERROR,
				     ("failed to mdbm_open orig mdbm file\n"));
						free(db);
						return (MDBM *)0;
					} 
				}
				flags |= O_CREAT;
			}
		} else {
			if (!(flags & O_CREAT)) {
				free(db);
				setoserror(EINVAL);
				return (MDBM *)0;
			}
		}
	}

	/* 
	** remove flags we dont want passed to open 
	*/
	flags2 = flags & ~(O_TRUNC|MDBM_ALLOC_SPACE);

	if ((db->m_fd = open(file2, flags2, mode)) == -1) {
		free(db);
		return (MDBM *)0;
	}
	
	/*
	** create a new mdbm file or open an existing one.
	** Make sure that the O_CREAT flag is cleared if we are not creating
	** a new database!
	*/
	if (flags & O_CREAT) {
		uint64	dbsize;
		char *pg0; /* page zero */
		mdbm_genlock_t gl;

		if (!psize) {
			psize = MDBM_PAGESIZ; /* go with default size */
		} else if (psize < MDBM_MINPAGE) {
			psize = MDBM_MINPAGE;
		} else if (psize > MDBM_MAXPAGE) {
			psize = MDBM_MAXPAGE;
		}
		db->m_pshift = psize;
		db->m_toffset = (1<<psize) - PAGE_TOP_SIZE;
		db->m_max_free = (uint32) (PAGE_SIZE(db) - PAGE_TOP_SIZE);
		dbsize = DATA_SIZE(0, db->m_pshift);

		if (ftruncate(db->m_fd, (off_t) dbsize) == -1 ||
		    ((db->m_db = (char *) mmap(0, (size_t) dbsize, PROT,
					       MFLAGS, db->m_fd, 0))
		     == (char *) MAP_FAILED)) {
			(void)close(db->m_fd);
			free((char *) db);
			return (MDBM *) NULL;
		}
#ifdef TRACE_MMAP
		_mdbm_trace_mmap(db->m_db, dbsize);
#endif
#ifdef F_RESVSP64
		if (flags & MDBM_ALLOC_SPACE) {
			flock64_t fsize = {0};

			fsize.l_len = (long)dbsize;
			if (fcntl (db->m_fd, F_RESVSP64, &fsize) != 0) {
				/* 
				** EINVAL implies that what we are asking
				** is invalid.  This is always the case if
				** we are not on a fstype that supports the
				** F_RESVSP fcntl
				*/
				if (oserror() == EINVAL) {
					setoserror(ENOEXIST);
				} else {
					(void)close(db->m_fd);
					free((char *) db);
					return (MDBM *) NULL;
				}
			}
			memset(db->m_db, 0, (size_t)dbsize);
			msync(db->m_db, (size_t)dbsize, MS_SYNC);
		}
#endif 
		pg0 = PAG_ADDR(db, 0); /* hand craft 1st 2 entry in page 0 */
		PAGE_GENLOCK(pg0) = htonl(1);
		NUM(pg0) = htons(2); /* 2 entry for header */
		INO(pg0, 1) = 0; /* null key for header */
		INO(pg0, 2) = htons(sizeof(MDBM_hdr) + MIN_DSIZE); 
		memset(DB_HDR(db), 0, sizeof(MDBM_hdr) + MIN_DSIZE);
		db->m_dir = TOP(db, pg0) + MIN_DIR_OFFSET(db);
		DB_HDR(db)->mh_magic = htonl(_MDBM_MAGIC);
		DB_HDR(db)->mh_version = htons(_MDBM_VERSION);
		DB_HDR(db)->mh_max_npgshift = (uchar) 0xff;
		DB_HDR(db)->mh_pshift = db->m_pshift;
		DB_HDR(db)->mh_minp_level = -1;
		mdbm_sethash(db, _mdbm_hash_id);
		if (flags & _MDBM_MEMCHECK) {
			DB_HDR(db)->mh_dbflags |= _MDBM_MEMCHECK_REQUIRED;
		}
		init_lock((abilock_t *)&(DB_HDR(db)->mh_lockspace));
		debug(DEBUG_INFO,
		       ("after open: db=%x dir=%x dsize=%d psize=%u\n",
			db->m_db, db->m_dir, 
			DIR_SIZE(db->m_npgshift), 1<<db->m_pshift));
		gl.genlock = 1;
		gl.pageno = 0;
		gl.page = pg0;
		gl.flags = MDBM_LOCK_LOCKED;
		gl.prev = NULL;
		_mdbm_genlock_unlock(db,pg0,&gl);
		/*
		** We are overwriting a existing mdbm file, which we need
		** to cleanly remove
		*/
		if (odb) {
			rename(file2, file);
			mdbm_invalidate(odb);
			mdbm_close(odb);
		}
		return (db);
	} else if (_mdbm_remap(db,NULL)) {
		/*
		** Set the default functions.
		*/
		switch(DB_HDR(db)->mh_hashfunc) {
		case 0:
			db->m_hash = _mdbm_hash0;
			break;
		case 1:
			db->m_hash = _mdbm_hash1;
			break;
		case 2:
			db->m_hash = _mdbm_hash2;
			break;
		case 3:
			db->m_hash = _mdbm_hash3;
			break;
		case 4:
			db->m_hash = _mdbm_hash4;
			break;
		case 5:
			db->m_hash = _mdbm_hash5;
			break;
		default:
			db->m_hash = _mdbm_hash;
		}

		if (DB_HDR(db)->mh_dbflags & _MDBM_MEMCHECK_REQUIRED) {
			db->m_flags |= _MDBM_MEMCHECK;
		}
		return (db);
	}

	/*
	** If open failed, make sure we close,unmap,free everything.
	*/
	if (db) {
		mdbm_close(db);
	}
	return (MDBM *) NULL;
}

void
mdbm_close(MDBM *db)
{
	debug(DEBUG_ENTRY,("mdbm_close\n"));
	if (db == NULL) {
		setoserror(EINVAL);
	} else {
		if (db->m_db != (char *)MAP_FAILED) {
			size_t size;

			size = (size_t) DB_SIZE(db->m_npgshift, db->m_pshift);
			if (size) {
				mdbm_sync(db);
#ifdef TRACE_MMAP
				_mdbm_trace_munmap(db->m_db, size);
#endif
				(void) munmap(db->m_db, size);

				/* only the paranoid survive */
				db->m_db = (char *)MAP_FAILED;
			}
		}
		if (db->m_fd >= 0) {
			(void) close(db->m_fd);
		}
		free((char *) db);
	}
}


/* 
** mdbm_fetch
**   And associated functions
*/

datum
mdbm_fetch(MDBM *db, kvpair kv) 
{
	uint64  hash;
	char	*page, *pg0;
	uint16	koff, doff;
	uint32 gl0, glp;
	register int i,n;
	datum itemk, itemv;

	debug(DEBUG_ENTRY,("mdbm_fetch\n"));
	if (MDBM_BAD(db) || KEY_BAD(kv.key)) {
		setoserror(EINVAL);
		return _mdbm_nulldatum;
	}

mdbm_fetch_genlock:
	if (MAP_OBSOLETE(db) && !_mdbm_remap(db,NULL)) {
		return _mdbm_nulldatum;
	}
	pg0 = PAG_ADDR(db,0);
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,pg0)) {
		return _mdbm_nulldatum;
	}

	gl0 = _mdbm_genlock_get(db,pg0,db->locks);
	if (! gl0) {
		return _mdbm_nulldatum;
	}

	/*
	 * Hash to a page.
	 */
	hash = _Exhash(db, kv.key);
	n = GETPAGE(db, hash);
	/* 
	 *  detect changes in the directory 
	 */
	if (n >= MAX_NPG(db)) {
		if (! _mdbm_remap(db,NULL)) {
			return _mdbm_nulldatum;
		}
		goto mdbm_fetch_genlock;
	}

	page = PAG_ADDR(db, n);
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,page)) {
		return _mdbm_nulldatum;
	}
	
	glp = _mdbm_genlock_get(db,page,db->locks);
	if (!glp) {
		return _mdbm_nulldatum;
	}
	if ( gl0 != PAGE_GENLOCK(pg0)) {
		goto mdbm_fetch_genlock;
	}

	if (!(n = -ntohs(NUM(page)))) {
		setoserror(ENOEXIST);
		return _mdbm_nulldatum;
	}

	/* code to match performance of single entry cache in other package */
	if ((hash == db->m_last_hash) && (NUM(page) >= -(db->m_kino))) { 
		koff = ((i = db->m_kino) == -1)? 0 : ntohs(_INO(page, i+1));
		XTRACT(db, itemk, i, page, koff, doff);
		if (_Mdbm_memcheck(db) && _mdbm_xtract_bad(db,page,itemk)) {
			return _mdbm_nulldatum;
		}
		if (glp != PAGE_GENLOCK(page)) {
			goto mdbm_fetch_genlock;
		}
		if ((kv.key.dsize == itemk.dsize) && 
		    !memcmp(kv.key.dptr, itemk.dptr, itemk.dsize)) {
			goto found;
		}
	}

	/*
	 * Look through page for given key.   ... This is _mdbm_seepair
	 */
	if (glp != PAGE_GENLOCK(page)) {
		goto mdbm_fetch_genlock;
	}
	koff=0;
	for (i = -1; i > n; i -= 2) {
		XTRACT(db, itemk, i, page, koff, doff);
		if (glp != PAGE_GENLOCK(page)) 
			goto mdbm_fetch_genlock;
		if (kv.key.dsize == itemk.dsize) {
			if (_Mdbm_memcheck(db) && 
			    _mdbm_xtract_bad(db,page,itemk)) {
				return _mdbm_nulldatum;
			}
			if (!memcmp(itemk.dptr, kv.key.dptr, itemk.dsize)) {
				if (glp != PAGE_GENLOCK(page)) {
					goto mdbm_fetch_genlock;
				}
				db->m_last_hash = hash;
				db->m_kino = i;
			found:
				doff = ntohs(_INO(page,i));
				doff = ALGNED(db,doff);
				XTRACT(db, itemv, i-1, page, doff, koff);
				if (_Mdbm_memcheck(db) && 
				    _mdbm_xtract_bad(db,page,itemv)) {
					return _mdbm_nulldatum;
				}
				db->m_pageno = PNO(db,page);
				db->m_next = i - 2;
				
				if (kv.val.dsize) {
					if (kv.val.dsize >= itemv.dsize) {
						memcpy(kv.val.dptr, itemv.dptr,
						       itemv.dsize);
						itemv.dptr = kv.val.dptr;

					      if (kv.val.dsize > itemv.dsize) {
						itemv.dptr[itemv.dsize] = '\0';
					      }
					} else {
						itemv.dptr = NULL;
					}
				}
				
				if (glp != PAGE_GENLOCK(page)) {
					goto mdbm_fetch_genlock;
				}
				debug(DEBUG_CORE|DEBUG_GUTS,
				      ("mdbm_fetch(%s)[page%d%d]=%s\n", 
				       kv.key.dptr, db->m_pageno, db->m_next,
				       kv.val.dptr));
				return itemv;
			}
		}
		koff = ntohs(_INO(page, i-1));
	}

	setoserror(ENOEXIST);
	return _mdbm_nulldatum;
}



/* 
** mdbm_delete 
**   And associated functions
*/

int 
mdbm_delete(MDBM *db, datum key) 
{
	char	*page, *pg0;
	uint32 gl0;
	int n;
	mdbm_genlock_t *genlock, *nl, *orig_locks;

	debug(DEBUG_CORE|DEBUG_ENTRY, ("mdbm_delete(%s)\n", key.dptr));

	if (MDBM_BAD(db) || KEY_BAD(key)) {
		setoserror(EINVAL);
		return -1;
	}
	orig_locks = db->locks;

	if  (_mdbm_write_access_check(db)) {
		return -1;
	}

mdbm_delete_genlock:
	pg0 = PAG_ADDR(db,0);
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,pg0)) {
		return -1;
	}

	gl0 = _mdbm_genlock_get(db,pg0,db->locks);
	if (!gl0) {
		return -1;
	}

	n = GETPAGE(db, _Exhash(db, key));
	/* 
	 *  detect changes in the directory 
	 */
	if (n >= MAX_NPG(db)) {
		if (! _mdbm_remap(db, db->locks)) {
			return -1;
		}
		goto mdbm_delete_genlock;
	}

		
	page = PAG_ADDR(db, n);
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,page)) {
		return -1;
	}

	nl = alloca(sizeof(mdbm_genlock_t));
	bzero(nl, sizeof(mdbm_genlock_t));
	if (! (genlock=_mdbm_genlock_lock(db, page, gl0, key, db->locks, nl))){
		if (errno == EBUSY) {
			goto mdbm_delete_genlock;
		}
		return -1;
	}
	db->locks = genlock;

	/*
	** The right thing happens here if seepair detects DB corruption
	** Be very careful if you add anything below this point
	*/
	if (NUM(page) && _mdbm_seepair(db,page,key)) {
		DELPAIR(db, page, key);
	}


	_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
	db->locks = orig_locks;

	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,page);
	}

	return 0;
}


/* 
** mdbm_store
**   And associated functions
*/

int
mdbm_store(MDBM *db, datum key, datum val, int flags)
{
	register uint32	need;
	register char	*page, *npage;
	char *pg0;
	uint32 gl0;
	uint64	hash;
	int pageno, i;
	uint16 koff, doff;
	datum itemv;
	mdbm_genlock_t *nl, *genlock = NULL, *orig_locks;

	debug(DEBUG_ENTRY,("mdbm_store:\n"));
	
	if (MDBM_BAD(db) || KEY_BAD(key)) {
		setoserror(EINVAL);
		return -1;
	}
	orig_locks = db->locks;

	need = (uint32) (key.dsize + val.dsize + (2 * INO_SIZE) + 
			 _Mdbm_alnmask(db));

mdbm_store_genlock:
	if  (_mdbm_write_access_check(db)) {
		setoserror(EINVAL);
		return -1;
	}

	pg0 = PAG_ADDR(db, 0);
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,pg0)) {
		setoserror(EINVAL);
		return -1;
	}

	if (!(gl0=_mdbm_genlock_get(db,pg0,db->locks))) {
		return -1;
	}

	hash = _Exhash(db, key);
	pageno = GETPAGE(db, hash);
	/* 
	 *  detect changes in the directory 
	 */
	if (pageno >= MAX_NPG(db)) {
		if (! _mdbm_remap(db,db->locks)) {
			return -1;
		}
		goto mdbm_store_genlock;
	}
	
	page = npage = PAG_ADDR(db,pageno);

	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,page)) {
		return -1;
	}
	
	
	nl = alloca(sizeof(mdbm_genlock_t));
	bzero(nl, sizeof(mdbm_genlock_t));
	if (! (genlock=_mdbm_genlock_lock(db, page, gl0, key, db->locks, nl))){
		if (errno == EBUSY) {
			goto mdbm_store_genlock;
		}
		return -1;
	}
	db->locks=genlock;

	debug(DEBUG_ENTRY,("store: need %d, have %d ... \n", 
	       need, FREE_AREA(db,page)));

	/*
	** Replacing an existing key:
	**
	** If the length of the key that we are replacing is the same
	** length as the current key.   If so, just slap the new one in
	** place, and return.   If not the same length, delete the key/data
	** pair then do a normal insert.
	*/
	if (NUM(page) && (i=_mdbm_seepair(db, page, key))) {
		if (!i && oserror() == EBADF) {
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
			return -1;
		}
		if (flags == MDBM_INSERT) {
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
			setoserror(EEXIST); 
			return -1;
		}
		
		koff = ntohs(_INO(page,i));
		XTRACT(db, itemv, i - 1, page, koff, doff);
		if (_Mdbm_memcheck(db) && _mdbm_xtract_bad(db, page, itemv)) {
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
			return -1;
		}
		if (itemv.dsize == val.dsize) {
			register char *t = TOP(db, page);

			(void)memcpy(t + koff, val.dptr, val.dsize);

			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
			return 0;
		}
		DELPAIR(db, page, key);
		need = (uint32) (key.dsize + val.dsize + (2 * INO_SIZE) + 
				 _Mdbm_alnmask(db));
	}
	/*
	 * if we do not have enough room, we have to split.
	 */
	if (need  > FREE_AREA(db, page)) {
		if (need > MAX_FREE(db)) {
			debug(DEBUG_ERROR,("mdbm_store: too big\n"));
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
			setoserror(ENOSPC);
			return -1;
		}

		/*
		** To split a page, we have to lock page 0.
		** _mdbm_genlock_lock's will deal wtih deadlock and livelock
		** situations.
		*/
		nl = alloca(sizeof(mdbm_genlock_t));
		bzero(nl, sizeof(mdbm_genlock_t));
		if (!(genlock = _mdbm_genlock_lock(db, pg0, gl0, 
						   _mdbm_nulldatum, db->locks, 
						   nl))){
			/* 
			** we want to preserve the errno of the lock failure 
			** in case it is EDEADLOCK (locks may have been broken.
			*/
			i = errno;
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			setoserror(i);
			db->locks = orig_locks;
			return -1;
		}
		db->locks=genlock;

		/*
		** make more room in the database.
		*/
		nl = alloca(sizeof(mdbm_genlock_t));
		bzero(nl, sizeof(mdbm_genlock_t));

		if ((npage = makeroom(db, hash, need, nl)) == 
		    (char *) -1LL) {
			int tmperr = errno;

			debug(DEBUG_ERROR,("mdbm_store: no room\n"));
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);

			/* above call may have modified errno */
			setoserror(tmperr);
			db->locks = orig_locks;
			(db)->m_flags |= _MDBM_IOERR;
			debug(DEBUG_IOERR, ("ioerr at %s %d\n", 
					     __FILE__, __LINE__));
			return -1;
		}
	}

	/*
	 * we have enough room or split is successful. insert the key.
	 */
	putpair(db, npage, key, val);

	/*
	** cleanup and exit
	*/
	_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
	db->locks = orig_locks;

	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,npage);
	}

	return 0;
}

static void
putpair(MDBM *db, char *pag, datum key, datum val)
{
	register int	n, off;
	register char 	*t;

#if DEBUG >= 2
	int l;
	char buf1[12], buf2[12];

	l = key.dsize > 11?11:key.dsize;
	memcpy(buf1, key.dptr, l);
	buf1[l]='\0';

	l = val.dsize > 11?11:val.dsize;
	memcpy(buf2, val.dptr, l);
	buf2[l]='\0';

	debug(DEBUG_CORE|DEBUG_GUTS,("putpair: page %d %s[%d] = %s[%d]\n", 
		PNO(db,pag),buf1,key.dsize, buf2, val.dsize));
#endif

	n = NUM(pag);
	off = OFF(pag); /* enter the key first */
	t = TOP(db,pag);

	(void) memcpy(t + off , key.dptr, key.dsize);
	off += key.dsize;
	INO(pag, n + 1) = htons(off);
	off = ALGNED(db,off);
	(void) memcpy(t + off , val.dptr, val.dsize);
	off += val.dsize;
	INO(pag, n + 2) = htons(off);
	NUM(pag) = htons(n + 2); /* adjust item count */
}


/*
** makeroom - make room by splitting the overfull page
** this routine will attempt to make room for _SPLTMAX times before
** giving up.
** returns the page for the key or (char*)-1
**
** This function requires that page 0, and the page that hash refers to are
** genlocked.  It will create new pages on the fly, and if they are not the 
** (new) destination of the hashed key unlock them.   The destination page
** will be locked when it is returned.   
**
** If the destination page is a new page created by makeroom, we have to
** copy the mdbm_genlock_t alloced in our frame into the new mdbm_genlock_t nl
** from our parent's frame.  
*/
static char*
makeroom(MDBM *db, uint64 hash, int need, mdbm_genlock_t *nl) 
{
	uint64	dbit = 0, hbit = 0, urgency, npageno;
	char	*page, *npage, *dest;
	mdbm_genlock_t gl, *opagegl;
	int	smax = _SPLTMAX;

	if (_Mdbm_perfect(db)) {
		register uint64 max_pgno;

		/* reset the _MDBM_PERFECT flag */
		DB_HDR(db)->mh_dbflags &= ~_MDBM_PERFECT;
		max_pgno = MAX_NPG(db)-1;
	        while (dbit < max_pgno)
                	dbit = 2 * dbit + ((hash & (1 << hbit++)) ? 2 : 1);
	} else {
		dbit = db->m_curbit;
		hbit = db->m_level;
	}

	if (!(opagegl = 
	      _mdbm_genlock_find(db, PAG_ADDR(db, (hash & MASK(hbit))),
				 db->locks))) {
		setoserror (ENOLCK);
		return (char *)-1LL;
	}
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,opagegl->page)) {
		return (char *)-1LL;
	}
	do {
		/* page number of the new page */
		npageno = (hash & MASK(hbit)) | (1LL<<hbit);
		debug(DEBUG_INFO,("creating new page %llu\n", npageno));

		if (npageno  >=  MAX_NPG(db)) {
			if (db->m_npgshift >= DB_HDR(db)->mh_max_npgshift)
				goto do_shake;

			if (!addpage(db)) 
				return ((char*)-1LL);	
		}
	       	page = PAG_ADDR(db, (hash & MASK(hbit))); 
	       	npage = PAG_ADDR(db, npageno); 
		if (_Mdbm_memcheck(db) && 
		    (_mdbm_page_bad(db,page) || _mdbm_page_bad(db,npage))) {
			return (char *)-1LL;
		}
		NUM(npage) = 0;
		
		PAGE_GENLOCK(npage) = htonl(1);
		
	 	/* split the current page*/
		if (split_page(db, page, npage, _split, (void *) hbit)) {
			return (char *)-1LL;
		}
		SETDBIT(db->m_dir, dbit);

		/* 
		** We need to unlock any page that is not:
		**   the original page
		**   the page the key hashed onto. 
		**     (the next itteration or the caller will unlock this)
		*/
		   
		if (hash>>hbit & 1LL) {		/* hashes to npage */
			dest = npage;
			if (page != opagegl->page) {
				gl.genlock = 1;
				gl.pageno = PNO(db,page);
				gl.page = page;
				gl.flags = MDBM_LOCK_LOCKED;
				gl.prev = NULL;
				_mdbm_genlock_unlock(db, page, &gl);

				if (_Mdbm_sync(db)) {
					_mdbm_sync_page(db,page);
				}
			}
		} else {			/* hashes to page */
			dest=page;
			gl.genlock = 1;
			gl.pageno = npageno;
			gl.page = npage;
			gl.flags = MDBM_LOCK_LOCKED;
			gl.prev = NULL;
			_mdbm_genlock_unlock(db, npage, &gl);

			if (_Mdbm_sync(db)) {
				_mdbm_sync_page(db,npage);
			}
		}
		if (FREE_AREA(db, dest) >= need) {
			/* 
			** If the destination page is not the original
			** page, fillout the supplied mdbm_genlock_t and make
			** the parent locks point to it.  Otherwise dont
			** mess with it.
			*/
			if (opagegl->page != dest) {
				nl->genlock = 1;
				nl->pageno = PNO(db, dest);
				nl->page = dest;
				nl->flags = MDBM_LOCK_LOCKED;
				nl->prev = db->locks;
				db->locks = nl;
			}
			return dest;
		}

		dbit = 2 * dbit + ((hash & (1<<hbit++)) ? 2 : 1);
	} while (--smax);

	/*
	 * if we are here, this is real bad news. After _SPLTMAX splits,
	 * we still cannot fit the key. say goodnight.
	 */
	debug(DEBUG_ERROR,("mdbm: cannot insert after _SPLTMAX attempts.\n"));
	setoserror(ENOSPC);
	return (char*)-1LL;

do_shake:
	page = PAG_ADDR(db, (hash & MASK(hbit))); 
	if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,page)) {
		return (char *)-1LL;
	}
	if (! db->m_shake) {
		setoserror(ENOSPC);
		return (char *)-1LL;
	}
	for (urgency =0; urgency< 3; urgency++)  {
		if (split_page(db, page, (char *) 0, db->m_shake, 
			       (void *) urgency)) {
			return (char *)-1LL;
		}
		if (FREE_AREA(db, page) >= need) {
			return page;
		}
	}

	setoserror(ENOSPC);
	return (char*)-1LL;
}

/*
 * Add space for a page to the database.
 * Make sure that the directory can address the new page; grow if it can't.
 * Only called for writeable databases.
 * interesting problem - what to do when I/O error?
 */
static int
addpage(MDBM *db)
{
	uint64	new_dbsize;
	char	*newdir;
	uchar   new_npgshift;

	/*
	 * Have to resize, _mdbm_remap, and copy the directory.
	 * Bump up the data & dir sizes by step.
	 */
	new_npgshift = db->m_npgshift + 1; /* double # of page */
	new_dbsize = DB_SIZE(new_npgshift, db->m_pshift);

	if (RE_SIZE(db, new_dbsize, db->locks)) {
#if 0
		debug(DEBUG_ERROR, 
		       ("Can't _mdbm_remap db up to %s (errno=%d)\n", 
			p64((uint64)new_dbsize), errno));
#endif
		(db)->m_flags |= _MDBM_IOERR;
		debug(DEBUG_IOERR, ("ioerr at %s %d\n", __FILE__, __LINE__));
		return (0);
	}

	/* Just in case db->m_dir changed after RE_SIZE() */
	db->m_dir = GET_DIR(db->m_npgshift, db->m_pshift);
	if (new_npgshift > (MIN_DSHIFT - DSHIFT_PER_PAGE)) {
		newdir = db->m_db + DATA_SIZE(new_npgshift, db->m_pshift);
		debug(DEBUG_GUTS,("DIR COPY %s\n", 
				   p64(DIR_SIZE(db->m_npgshift))));

		/* should realy take 64 bit argument, but ok for now */
		memmove(newdir, db->m_dir, (size_t) DIR_SIZE(db->m_npgshift));
		db->m_dir = newdir;
	} 
	DB_HDR(db)->mh_npgshift = db->m_npgshift = new_npgshift;
	return (1);
}


/* 
** mdbm_first/next
**   And associated functions
*/

kvpair
mdbm_first(MDBM *db, kvpair kvin)
{
	debug(DEBUG_ENTRY,("mdbm_first\n"));

	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return _mdbm_nullkv;
	}

	/*
	 * start at page 0
	 */
	db->m_pageno = 0;
	db->m_next = -1;

	if (MAP_OBSOLETE(db) && !_mdbm_remap(db,NULL))  {
		return _mdbm_nullkv;
	}
	
	return _mdbm_getnext(db, kvin);
}

datum
mdbm_firstkey(MDBM *db, datum key)
{
	kvpair kv;

	kv.key.dsize = key.dsize;
	kv.key.dptr = key.dptr;
	kv.val.dsize = 0;
	kv.val.dptr = NULL;
	
	kv = mdbm_first(db, kv);
	return kv.key;
}

kvpair
mdbm_next(MDBM *db, kvpair kvin)
{
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return _mdbm_nullkv;
	}

	if (MAP_OBSOLETE(db)) {
		if (!_mdbm_remap(db,NULL)) {
			return _mdbm_nullkv;
		}

		/* a file changing size in the middle is a reset. */
		db->m_pageno = 0;
		db->m_next = -1;
	}

	return _mdbm_getnext(db, kvin);
}

datum
mdbm_nextkey(MDBM *db, datum key)
{
	kvpair kv;

	kv.key.dsize = key.dsize;
	kv.key.dptr = key.dptr;
	kv.val.dsize = 0;
	kv.val.dptr = NULL;
	
	kv = mdbm_next(db, kv);
	debug(DEBUG_INFO,("next returns: dsize - %d, val - %s\n", 
	       kv.key.dsize, kv.key.dptr));
	return kv.key;
}

/*
 * getnext - get the next key in the page, and if done with
 * the page, try the next page in sequence
 */
static kvpair
_mdbm_getnext(MDBM *db, kvpair kvin)
{
	int n;
	uint16	koff, doff;
	char	*page;
	kvpair kv;
	uint32 glp;
	int i;
	datum itemk, itemv;
	uint32 maxpage;

	maxpage=MAX_NPG(db) - 1;

	while (db->m_pageno <= maxpage) {
		debug(DEBUG_CORE|DEBUG_DETAIL,
		      ("mdbm_getnext page %d%d (maxpage %d)\n", 
		       db->m_pageno, db->m_next, maxpage));
	getnext_genlock:
		page = PAG_ADDR(db, db->m_pageno);
		if (_Mdbm_memcheck(db) && _mdbm_page_bad(db,page)) {
			return _mdbm_nullkv;
		}
		glp = _mdbm_genlock_get(db,page,db->locks);
		/* 
		** There is no genlock set (so glp will be 0) on an empty page.
		*/
		n = -ntohs(NUM(page));
		if (!n) {
			db->m_pageno++;
			continue;
		}
		if (! glp) {
			return _mdbm_nullkv;
		}
		if (glp != PAGE_GENLOCK(page)) {
			goto getnext_genlock;
		}
		
		debug(DEBUG_CORE|DEBUG_GUTS,
		      ("getnext page=%u max=%u n=%u next=%u\n", 
		       (uint32)db->m_pageno, MAX_NPG(db)-1, n, db->m_next));
next_entry:
		if (n <  db->m_next) {
			i = db->m_next;
			koff = (i == -1) ? 0 : ntohs(_INO(page, i+1));
			XTRACT(db, itemk, i, page, koff, doff);
			if (_Mdbm_memcheck(db) && 
			    _mdbm_xtract_bad(db,page,itemk)) {
				return _mdbm_nullkv;
			}
			if (glp != PAGE_GENLOCK(page)) {
				goto getnext_genlock;
			}
			if (!itemk.dsize) {
				db->m_next -= 2;
				goto next_entry; /* ignore header */
			}
			kv.key=itemk;
			doff = ntohs(_INO(page, i));
                	doff = ALGNED(db,doff);
                	XTRACT(db, itemv, i-1, page, doff, koff);
			if (_Mdbm_memcheck(db) && 
			    _mdbm_xtract_bad(db,page,itemv)) {
				return _mdbm_nullkv;
			}
			if (glp != PAGE_GENLOCK(page)) {
				goto getnext_genlock;
			}
			db->m_next -= 2;
			kv.val = itemv;
			if (kvin.key.dsize) {
				if (kvin.key.dsize >= kv.key.dsize) {
					memcpy(kvin.key.dptr, kv.key.dptr, 
					       kv.key.dsize);
					kv.key.dptr=kvin.key.dptr;
					if (kvin.key.dsize > kv.key.dsize) {
						kv.key.dptr[kv.key.dsize]='\0';
					}
				} else {
					kv.key.dptr=NULL;
				}
			}				
			if (kvin.val.dsize) {
				if (kvin.val.dsize >= kv.val.dsize) {
					memcpy(kvin.val.dptr, kv.val.dptr, 
					       kv.val.dsize);
					kv.val.dptr=kvin.val.dptr;
					if (kvin.val.dsize > kv.val.dsize) {
						kv.val.dptr[kv.val.dsize]='\0';
					}
				} else {
					kv.val.dptr=NULL;
				}
			}				
			if (glp != PAGE_GENLOCK(page)) {
				goto getnext_genlock;
			}
			return (kv);
		}

		/*
		 * we either run out, or there is nothing on this page..
		 * try the next one... 
		 */
		(db->m_pageno)++;
		db->m_next = -1;
	}
	setoserror(0);
	return _mdbm_nullkv;
}


/*
** split and friends
*/

/* ARGSUSED */
static int
_split(struct mdbm  *db, datum key, datum val, void *param)
{
	return ((_Exhash(db, key) >> (int64)param) & 1LL);
}

/* ARGSUSED */
static int
_delete(struct mdbm  *db, datum key, datum val, void *param)
{
#define TARGET ((datum *) param) /* this will break if sizeof(int) < sizeof(pointer) */
	if ((TARGET->dsize == key.dsize) && 
	    !memcmp(TARGET->dptr, key.dptr, key.dsize)) {
		TARGET->dsize = -1; /* so we return 0 on later call */
		return 2;
	} else  
		return 0;
}

static void
move_rest(MDBM *db, char *pag, int i, int n) 
{
	int moved = n - i - 1;
	int num = NUM(pag);

	/*
	 * found the key. if it is the last entry
	 * [i.e. i == n - 1] we just adjust the entry count.
	 * hard case: move all data down onto the deleted pair,
	 * shift offsets onto deleted offsets, and adjust them.
	 * [note: 0 < i < n]
	 */

	if (moved) {
		char *src = pag - (db)->m_toffset + ntohs(INO(pag,i + 1));
		char *dst = pag - (db)->m_toffset + ntohs(INO(pag,num));
		int zoo = (int)(src - dst);
		int move = (int)((pag - (db)->m_toffset + ntohs(INO(pag,n))) 
				 - src);
		int removed = i + 1 - num;
#define DUFF
#ifdef DUFF
#define	MOVB 	*dst++ = *src++

		if (move > 0) {
			int loop = (move + 8 - 1) >> 3;

			switch (move & (8 - 1)) {
			case 0:
				do {
					MOVB;
			case 7:
					MOVB;
			case 6:
					MOVB;
			case 5:
					MOVB;
			case 4:
					MOVB;
			case 3:
					MOVB;
			case 2:
					MOVB;
			case 1:
					MOVB;
				} while (--loop);
			}
		}
#else
#ifdef MEMMOVE
		memmove(dst, src, move);
#else
		while (move--) {
			*dst++ = *src++;
		}
#endif
#endif
		/*
		 * adjust offset index up
		 */
		
		while (i < n - 1) {
			INO(pag,i) = htons(ntohs(INO(pag,i + removed)) - zoo);
			i++;
		}
	}
	NUM(pag) = htons(num + moved);
}

/*
 * Take all the items in pag and split them between pag & new.
 * If "new" is null, then we just "shake/delete" out "unwanted" entry 
 * This is also used as a "merge" function for mdbm_compress_tree()
 */
static int
split_page(MDBM *db, char *pag, char *new, 
	   int (*move_it)(MDBM *, datum, datum, void *), void *param)
{
	register int	i, j, n;
	int	s;
	datum itemk, itemv;
#define NO_HOLE 0xffff
	uint16	doff, koff, fh = NO_HOLE; /* location of first hole */

	n = ntohs(NUM(pag)); /* caller must insure n != 0 */

	if (pag == PAG_ADDR(db,0)) {
		s = - 3; /* 1 for n, 2 for the header inode entries*/
		koff = ntohs(INO(pag, 2));
	} else {
		s = - 1 ; 
		koff = 0 ;
	}
			
	for (i = s; i > -n; i -= 2) {
		XTRACT(db, itemk, i, pag, koff, doff);
		if (_Mdbm_memcheck(db) && _mdbm_xtract_bad(db,pag,itemk)) {
			return -1;
		}
		doff = ALGNED(db,doff);
		XTRACT(db, itemv, i-1, pag, doff, koff);
		if (_Mdbm_memcheck(db) && _mdbm_xtract_bad(db,pag,itemv)) {
			return -1;
		}

		j=(*move_it)(db,itemk,itemv, param);
		if (j) {
			if (new) {
				putpair(db, new, itemk, itemv);
			}
			/* 
			** (*move_it) has told us there are no more entries
			** after this to move.   
			*/
			if (j > 1 && ! db->m_amask) {
				NUM(pag) = htons(-(i+1));
				move_rest(db, pag, -i, n);
				break;
			}

			if (fh == NO_HOLE) {
				fh = NUM(pag) = htons(-(i+1));
			}
		} else {
			if (fh != NO_HOLE) {
				putpair(db, pag, itemk, itemv);
			}
		}
	}
#ifdef DEBUG
	if (new) {
		debug(DEBUG_CORE|DEBUG_GUTS,
		      ("split([%d]%u -> [%d]%u [%d]%u)\n", 
		       n,PNO(db,pag),ntohs(*((uint16*)pag)),PNO(db,pag),
		       ntohs(*((uint16 *)new)),PNO(db,new)));
	} else {
		debug(DEBUG_CORE|DEBUG_GUTS,("split(delete) -> [%d]%u\n", 
		       PNO(db,pag), ntohs(*((uint16 *)pag))));
	}
#endif

	return 0;
}


/*
** Misc functions 
*/

void
mdbm_sync(MDBM *db)
{
	debug(DEBUG_ENTRY,("mdbm_sync\n"));
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
	} else {
		if (db->m_db != (char *)MAP_FAILED) {
			if (!_Mdbm_rdonly(db)) {
#ifdef linux
				fsync(db->m_fd);
#else
				msync(db->m_db, 
				      (size_t)DB_SIZE(db->m_npgshift,
						      db->m_pshift), MS_ASYNC);
#endif
			}
		}
	}
}

void
mdbm_close_fd(MDBM *db)
{
#ifdef linux
	setoserror(EINVAL);
#else
	if (db->m_fd >= 0) {
		close(db->m_fd);
		db->m_fd = -1;
	}
#endif
}

/*
**  invalidate a database.  mdbm file now can be unlinked safely.
*/
int
mdbm_invalidate(MDBM *db)
{

	if (_mdbm_write_access_check(db)) {
		return -1;
	}

	/* mark this db as invalid */
	DB_HDR(db)->mh_dbflags |= _MDBM_INVALID;

	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,PAG_ADDR(db,0));
	}

	return 0;
}

int
mdbm_sethash(MDBM *db, int number)
{
	debug(DEBUG_ENTRY,("mdbm_sethash to %d\n", number));

	if (_mdbm_write_header_check(db)) {
		return -1;
	}

	switch(number) {
	    case 0:
		db->m_hash = _mdbm_hash0;
		break;
	    case 1:
		db->m_hash = _mdbm_hash1;
		break;
	    case 2:
		db->m_hash = _mdbm_hash2;
		break;
	    case 3:
		db->m_hash = _mdbm_hash3;
		break;
	    case 4:
		db->m_hash = _mdbm_hash4;
		break;
	    case 5:
		db->m_hash = _mdbm_hash5;
		break;
	    default:
		setoserror(EINVAL);    
		return -1;
	}
	DB_HDR(db)->mh_hashfunc = number;
	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,PAG_ADDR(db,0));
	}

	return 0;
}
