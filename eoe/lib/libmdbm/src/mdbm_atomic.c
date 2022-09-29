/*
** mdbm_atomic.c
**
** mdbm atomic operations
*/

#include "common.h"

/*
** mdbm_atomic_begin
** mdbm_atomic_end
*/

int
mdbm_atomic_begin(MDBM *db, datum key, mdbm_genlock_t *nl)
{
	char *page, *pg0;
	mdbm_genlock_t *genlock;
	uint32 gl0;
	
	debug(DEBUG_ATOMIC|DEBUG_ENTRY,
	       ("mdbm_atomic_begin (%x, %s)\n", db, key.dptr));
	if (_mdbm_write_access_check(db)) {
		return -1;
	}

mdbm_atomic_genlock:
	pg0 = PAG_ADDR(db,0);
	gl0 = _mdbm_genlock_get(db, pg0, db->locks);
	if (!gl0) {
		return -1;
	}

	bzero(nl, sizeof(mdbm_genlock_t));
	page = PAG_ADDR(db,GETPAGE(db,_Exhash(db,key)));
	if (!(genlock=_mdbm_genlock_lock(db, page, gl0, key, db->locks, nl))) {
		if (errno == EBUSY) {
			goto mdbm_atomic_genlock;
		}
		return -1;
	}
	db->locks = genlock;
	return 0;
}
#pragma optional mdbm_atomic_begin

int
mdbm_atomic_end(MDBM *db)
{
	int i;

	debug(DEBUG_ATOMIC|DEBUG_ENTRY,("mdbm_atomic_end(%d)\n",db));
	i = _mdbm_genlock_unlock_frame(db, db->locks, NULL);
	db->locks = NULL;
	return i;

}
#pragma optional mdbm_atomic_end

/*
** mdbm_test_and_set
*/

datum
mdbm_test_and_set(MDBM *db, kvpair kv, datum rval)
{
	mdbm_genlock_t nl, *orig_locks;
	kvpair tkv;
	int i;
	
	orig_locks = db->locks;
	debug(DEBUG_ATOMIC|DEBUG_ENTRY,("mdbm_atomic_test_and_set:\n"));
	if (mdbm_atomic_begin(db, kv.key, &nl) == -1) {
		return _mdbm_nulldatum;
	}

	tkv.key = kv.key;
	tkv.val = rval;

	rval = mdbm_chainP_fetch(db, tkv);
	if (!rval.dptr) {
		i = errno;
		mdbm_atomic_end(db);
		setoserror(i);

		kv.val.dptr = NULL;
		kv.val.dsize = rval.dsize;
		return kv.val;
	}
	if (mdbm_chainP_store(db, kv.key, kv.val, MDBM_REPLACE) < 0) {
		rval.dsize = 0;
	}
	
	_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
	db->locks = orig_locks;
	return rval;
}
#pragma optional mdbm_test_and_set

/*
** mdbm_compare_and_swap
*/

#define MDBM_COMPARE_OP_SWAP	1
#define MDBM_COMPARE_OP_DELETE	2
static int _mdbm_compare_and_op(MDBM *db, datum key, datum oldval, datum newval, int op)
{
	mdbm_genlock_t nl, *orig_locks;
	kvpair kv;
	char *buf;
	int pagesize,i;

	orig_locks = db->locks;
	debug(DEBUG_ATOMIC|DEBUG_ENTRY,("mdbm_atomic_compare_and_op: %d\n", op));

	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return -1;
	}
	pagesize = MDBM_PAGE_SIZE(db);

	if ((mdbm_atomic_begin(db, key, &nl)) == -1) {
		return -1;
	}
	
	buf = alloca(pagesize);
	bzero(buf,pagesize);
	kv.key = key;
	kv.val.dptr = buf;
	kv.val.dsize = pagesize;

      mdbm_compare_retry_fetch:
	kv.val = mdbm_chainP_fetch(db, kv);
	if (!kv.val.dptr) {
		i = errno;
	      debug(DEBUG_ATOMIC|DEBUG_INFO,
		     ("mdbm_atomic_compare_and_op: fetch failed\n"));
		_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
		db->locks = orig_locks;
		setoserror(i);
		return -1;
	}
	if (kv.val.dsize > pagesize) {
		debug(DEBUG_ATOMIC|DEBUG_INFO,
		     ("mdbm_atomic_compare_and_op: refetching with size %d\n",
		      kv.val.dsize));
		pagesize = kv.val.dsize;
		buf = alloca(pagesize);
		bzero(buf,pagesize);
		kv.val.dptr = buf;
		goto mdbm_compare_retry_fetch;
	}
	if (kv.val.dsize != oldval.dsize ||
	    memcmp(kv.val.dptr, oldval.dptr, kv.val.dsize)) {
		debug(DEBUG_ATOMIC|DEBUG_INFO,
	      ("mdbm_atomic_compare_and_op: compare failed %d(%s) %d(%s)\n", 
	       kv.val.dsize, kv.val.dptr, oldval.dsize, oldval.dptr));

		setoserror(EBUSY);
		_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
		db->locks = orig_locks;
		return -1;
	}
	
	if (op == MDBM_COMPARE_OP_SWAP ) {
		debug(DEBUG_ATOMIC|DEBUG_GUTS,
		      ("mdbm_atomic_compare_and_swap: store %s\n", 
		       newval.dptr));
		i = mdbm_chainP_store(db, key, newval, MDBM_REPLACE);
	} else { /* MDBM_COMPARE_OP_DELETE */
		debug(DEBUG_ATOMIC|DEBUG_GUTS,
		      ("mdbm_atomic_compare_and_swap: store %s\n", 
		       newval.dptr));
		i = mdbm_chainP_delete(db, key);
	}
	_mdbm_genlock_unlock_frame(db, db->locks, orig_locks);
	db->locks = orig_locks;
	return i;
}


int
mdbm_compare_and_swap(MDBM *db, datum key, datum oldval, datum newval)
{
	return _mdbm_compare_and_op(db, key, oldval, newval, 
				    MDBM_COMPARE_OP_SWAP);

}
#pragma optional mdbm_compare_and_swap


int 
mdbm_compare_and_delete(MDBM *db, datum key, datum value)
{
	return _mdbm_compare_and_op(db, key, value, _mdbm_nulldatum, 
				    MDBM_COMPARE_OP_DELETE);
}
#pragma optional mdbm_compare_and_delete

