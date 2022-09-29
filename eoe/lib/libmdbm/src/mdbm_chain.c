/*
** mdbm_chain.c
**
** mdbm bucket chaining routines
*/

#include "common.h"

#define MDBM_NUMBER_CHAINS(db, size) (size?((size + (MDBM_CHAIN_SIZE(db)) - 1 ) /( MDBM_CHAIN_SIZE(db)) - 1):0)

/*
** mdbm_set_chain
*/
int
mdbm_set_chain(MDBM *db) 
{
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return -1;
	}

	if (_Mdbm_rdonly(db)) {
	    debug(DEBUG_ERROR,
		   ("can not set chaining, db is open for read only\n"));
                setoserror(EINVAL);
                return -1;
	}

	DB_HDR(db)->mh_dbflags |= _MDBM_CHAIN;
	return 0;
}

/*
** the mdbm_chainP_ wrappers 
*/
int
mdbm_chainP_delete(MDBM *db, datum key)
{
	if (_Mdbm_chaining(db)) {
		return mdbm_chain_delete(db, key);
	} else {
		return mdbm_delete(db,key);
	}
}
#pragma optional mdbm_chainP_delete

datum
mdbm_chainP_fetch(MDBM *db, kvpair kv) 
{
	if (_Mdbm_chaining(db)) {
		return mdbm_chain_fetch(db, kv);
	} else {
		return mdbm_fetch(db, kv);
	}
}	
#pragma optional mdbm_chainP_fetch

kvpair
mdbm_chainP_first(MDBM *db, kvpair kv)
{
	if (_Mdbm_chaining(db)) {
		return mdbm_chain_first(db, kv);
	} else {
		return mdbm_first(db, kv);
	}
}
#pragma optional mdbm_chainP_first

kvpair
mdbm_chainP_next(MDBM *db, kvpair kv)
{
	if (_Mdbm_chaining(db)) {
		return mdbm_chain_next(db, kv);
	} else {
		return mdbm_next(db, kv);
	}
}
#pragma optional mdbm_chainP_next

int
mdbm_chainP_store(MDBM *db, datum key, datum val, int flags)
{

	if (_Mdbm_chaining(db)) {
		return mdbm_chain_store(db, key, val, flags);
	} else {
		return mdbm_store(db, key, val, flags);
	}
}
#pragma optional mdbm_chainP_store

/*
** mdbm_chain_fetch
*/
datum	
mdbm_chain_fetch(MDBM *db, kvpair kv)
{
	kvpair kvt;
	uint32_t size, i, *j;
	uint32 pageno, next;
	int  number_chains, chain_size;

	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return _mdbm_nulldatum;
	}

	if (! _Mdbm_chaining(db)) {
		debug(DEBUG_ERROR,("MDBM file not set to do chaining\n"));
		setoserror(EINVAL);
                return _mdbm_nulldatum;
	}

	kvt.key = kv.key;
	kvt.val.dsize = MDBM_PAGE_SIZE(db);
	kvt.val.dptr = alloca(kvt.val.dsize);
	
	kvt.val = mdbm_fetch(db, kvt);
	if (!kvt.val.dsize) {
		debug(DEBUG_CHAIN|DEBUG_INFO,
		      ("mdbm_chain_fetch(%s) no such key\n", kv.key.dptr));
		return _mdbm_nulldatum;
	}

	memcpy(&size, kvt.val.dptr, sizeof(size));
	size = htonl(size);
	debug(DEBUG_CHAIN|DEBUG_INFO,
	      ("mdbm_chain_fetch(%s) found %d byte value\n", 
	       kv.key.dptr, size));

	pageno = db->m_pageno;
	next = db->m_next;
	if (!kv.val.dsize || size > kv.val.dsize) {
		kv.val.dsize = size;
		kv.val.dptr = NULL;
	} else {
		number_chains = MDBM_NUMBER_CHAINS(db,size);
		chain_size = MDBM_CHAIN_SIZE(db);

		memmove(kv.val.dptr, kvt.val.dptr + sizeof(size), 
			kvt.val.dsize - sizeof(size));
		kv.val.dsize = kvt.val.dsize - (int)sizeof(size);
		kvt.key.dsize = kv.key.dsize + 2 + (int)sizeof(size);
		kvt.key.dptr = alloca(kvt.key.dsize);
				
		kvt.key.dptr[0] = '\0';
		memcpy(kvt.key.dptr + 1, kv.key.dptr , kv.key.dsize);
		kvt.key.dptr[kv.key.dsize + 1] = '\0';
		for (i=1; i <= number_chains ; i++) {
			j = &i;
			memcpy(kvt.key.dptr + kv.key.dsize + 2, j, 
			       sizeof(uint32_t));
			kvt.val.dptr = kv.val.dptr + ( i * chain_size);
			if (((i + 1) * chain_size) <= size) {
				kvt.val.dsize = chain_size;
			} else {
				kvt.val.dsize = size - (i * chain_size);
			}
			
			kvt.val =  mdbm_fetch(db, kvt);
			if (! kvt.val.dsize) {
				debug(DEBUG_CHAIN|DEBUG_ERROR,
				       ("Chain fetch %d failed\n",i));
				kvt.val.dptr = NULL;
				break;
			}
			kv.val.dsize += kvt.val.dsize;
		}
		if (kv.val.dsize > size) {
			kv.val.dptr[size]='\0';
		}
	}

	db->m_pageno = pageno ;
	db->m_next = next;
	return kv.val;
}

/*
** mdbm_chain_first
*/
kvpair	
mdbm_chain_first(MDBM *db, kvpair kv) 
{
	db->m_pageno = 0;
	db->m_next = -1;
	
	return (mdbm_chain_next(db, kv));
}

/*
** mdbm_chain_last
*/
kvpair	
mdbm_chain_next(MDBM *db, kvpair kv) 
{
	int pageno, next;
	kvpair kvt;
	
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return _mdbm_nullkv;
	}

	if (! _Mdbm_chaining(db)) {
		debug(DEBUG_ERROR,("MDBM file not set to do chaining\n"));
		setoserror(EINVAL);
                return _mdbm_nullkv;
	}

	/*
	** Skip entries that start with \0
	** save pageno, next ; Call mdbm_chain_fetch with NULL kv.val.dptr
	** reset pageno, next
	*/

	kv.key.dptr[0] = '\0';
	kvt.key.dptr = kv.key.dptr;
	kvt.key.dsize = kv.key.dsize;
	
	while (kvt.key.dsize && (*kvt.key.dptr == '\0')) {
		kvt.key.dptr = kv.key.dptr;
		kvt.key.dsize = kv.key.dsize;
		kvt.val.dptr = kv.val.dptr;
		kvt.val.dsize = kv.val.dsize;
		kvt = mdbm_next(db, kvt);
	}
	pageno = db->m_pageno;
	next = db->m_next;
	
	if (! kvt.val.dsize) {
		return _mdbm_nullkv;
	}

	kv.key.dsize = kvt.key.dsize;
	memcpy(kv.key.dptr,kvt.key.dptr,kvt.key.dsize);

	kvt.val.dptr = kv.val.dptr;
	kvt.val.dsize = kv.val.dsize;
	kv.val = mdbm_chain_fetch(db,kvt);

	db->m_pageno = pageno;
	db->m_next = next;
	
	return kv;
}

/*
** mdbm_chain_store
*/
int
mdbm_chain_store(MDBM *db, datum key, datum val, int flags) 
{
	int ret, number_chains, chain_size;
	uint32_t size, *size2, i, *j;
	char *keyp, *valp;
	datum key2, val2;
	mdbm_genlock_t nl, *orig_locks;
	
	
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return -1;
	}

	if (! _Mdbm_chaining(db)) {
		debug(DEBUG_ERROR,("MDBM file not set to do chaining\n"));
		setoserror(EINVAL);
                return -1;
	}
	if (_Mdbm_rdonly(db)) {
		debug(DEBUG_ERROR,("db is open for read only\n"));
		setoserror(EINVAL);
                return -1;
	}

	if (*key.dptr == '\0' && ! (flags & MDBM_CHAIN_NULL)) {
		debug(DEBUG_CHAIN|DEBUG_ERROR,("key starts with NULL\n"));
		setoserror(EINVAL);
		return -1;
	}

	if (flags & MDBM_REPLACE) {
		orig_locks = db->locks;
		if (mdbm_atomic_begin(db,key,&nl) < 0) {
			return -1;
		}
		mdbm_chain_delete(db, key);
	}

	size = htonll(val.dsize);
	number_chains = MDBM_NUMBER_CHAINS(db,size);
	chain_size = MDBM_CHAIN_SIZE(db);
	
	val2.dsize = (number_chains?chain_size:val.dsize) + (int)sizeof(size);
	valp = alloca(val2.dsize);
	val2.dptr = valp;

	key2.dsize = key.dsize + 2 + (int)sizeof(size);
	keyp = alloca(key2.dsize);
	key2.dptr = keyp;

	size2 = (uint32_t *)valp;
	*size2 = htonl(size);
	if (val.dsize) {
		memcpy(valp + sizeof(size), val.dptr, 
		       val.dsize>chain_size?chain_size:val.dsize);
	}

	ret = mdbm_store(db, key, val2, MDBM_INSERT);
	if (ret < 0) {
		debug(DEBUG_CHAIN|DEBUG_ERROR,("Chain store 0 failed\n"));
		if (flags & MDBM_REPLACE) {
			_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
			db->locks = orig_locks;
		}
                return ret;
	}

	key2.dptr[0] = '\0';
	memcpy(key2.dptr + 1, key.dptr , key.dsize);
	key2.dptr[key.dsize + 1] = '\0';
	for (i=1; i <= number_chains ; i++) {
		j = &i;
		memcpy(key2.dptr + key.dsize + 2, j, sizeof(uint32_t));

		val2.dptr = val.dptr + ( i * chain_size);
		if (((i + 1) * chain_size) <= val.dsize) {
			val2.dsize = chain_size;
		} else {
			val2.dsize = val.dsize - (i * chain_size);
		}
		
		ret = mdbm_store(db, key2, val2, MDBM_INSERT);
		if (ret < 0) {
			debug(DEBUG_CHAIN|DEBUG_ERROR,
			       ("Chain store %d failed\n",i));
			mdbm_chain_delete(db, key);
			if (flags & MDBM_REPLACE) {
				_mdbm_genlock_unlock_frame(db, db->locks, 
							   orig_locks);
				db->locks = orig_locks;
			}
			return ret;
		}
	}
	if (flags & MDBM_REPLACE) {
		_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
		db->locks = orig_locks;
	}

	return 0;
	
}

/*
** mdbm_chain_delete
*/
int
mdbm_chain_delete(MDBM *db, datum key) 
{
	int number_chains;
	uint32_t i, *j, size;
	kvpair kv;

	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return -1;
	}

	if (! _Mdbm_chaining(db)) {
		debug(DEBUG_ERROR,("MDBM file not set to do chaining\n"));
		setoserror(EINVAL);
                return -1;
	}
	if (_Mdbm_rdonly(db)) {
		debug(DEBUG_ERROR,("db is open for read only\n"));
		setoserror(EINVAL);
                return -1;
	}

	kv.key = key;
	kv.val.dptr = NULL;
	kv.val.dsize = 0;
	
	kv.val = mdbm_chain_fetch(db, kv);
	if (! kv.val.dsize) {
		return 0;
	}
	
	kv.val.dsize = MDBM_PAGE_SIZE(db);
	kv.val.dptr = alloca(kv.val.dsize);

	kv.val = mdbm_fetch(db, kv);
	if (! kv.val.dsize) {
		return 0;
	}

	memcpy(&size, kv.val.dptr, sizeof(size));
	size = htonl(size);

	number_chains = MDBM_NUMBER_CHAINS(db,size);

	mdbm_delete(db, key);
	
	kv.key.dsize = key.dsize + 2 + (int)sizeof(size);
	kv.key.dptr = alloca(kv.key.dsize);
	kv.key.dptr[0] = '\0';
	memcpy(kv.key.dptr + 1, key.dptr , key.dsize);
	kv.key.dptr[key.dsize + 1] = '\0';
	
	for (i=1; i <= number_chains ; i++) {
		j = &i;
		memcpy(kv.key.dptr + key.dsize + 2, j, sizeof(uint32_t));

		mdbm_delete(db, kv.key);
	}
	return 0;
}
