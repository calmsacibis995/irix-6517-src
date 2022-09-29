#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <mdbm.h>
#include <alloca.h>

typedef MDBM* MDBM_File;
#define mdbm_TIEHASH(dbtype,filename,flags,mode,pagesize) mdbm_open(filename,flags,mode,pagesize)


MODULE = MDBM_File	PACKAGE = MDBM_File	PREFIX = mdbm_

MDBM_File
mdbm_TIEHASH(dbtype, filename, flags, mode, pagesize = 0)
	char *		dbtype
	char *		filename
	int		flags
	int		mode
	int		pagesize

void
mdbm_DESTROY(db)
	MDBM_File	db
    CODE:
	mdbm_close(db);

int
mdbm_FETCH(db, key)
	MDBM_File	db
	datum		key
    CODE:
    {
	kvpair kv;
	int pagesize;

	kv.key = key;
	pagesize = MDBM_PAGE_SIZE(db);
	if (_Mdbm_chaining(db)) {
		kv.val.dsize = 0;
		kv.val = mdbm_chain_fetch(db, kv);

		if (kv.val.dsize) {
			kv.val.dptr = alloca(kv.val.dsize);
			kv.val = mdbm_chain_fetch(db, kv);
		}
	} else {
		kv.val.dptr = alloca(pagesize);
		kv.val.dsize = pagesize;

		kv.val = mdbm_fetch(db, kv);
	}
	ST(0) = sv_newmortal();
	if (kv.val.dptr) {
		sv_setpvn(ST(0), kv.val.dptr, kv.val.dsize);
	}
    }

int
mdbm_STORE(db, key, value, flags = MDBM_REPLACE)
	MDBM_File	db
	datum		key
	datum		value
	int		flags
    CODE:
    {
	if (_Mdbm_chaining(db)) {
		RETVAL = mdbm_chain_store(db, key, value, flags);
	} else {
		RETVAL = mdbm_store(db, key, value, flags);
	}
    }
    OUTPUT:
	RETVAL
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to mdbm file");
	    croak("mdbm store returned %d, errno %d, key \"%s\"",
			RETVAL,errno,key.dptr);
	}

int
mdbm_DELETE(db, key)
	MDBM_File	db
	datum		key
    CODE:
    {
	if (_Mdbm_chaining(db)) {
		RETVAL = mdbm_chain_delete(db, key);
	} else {
		RETVAL = mdbm_delete(db, key);
	}
    }
    OUTPUT:
	RETVAL

int
mdbm_FIRSTKEY(db)
	MDBM_File	db
    CODE:
    {
	kvpair kv;
	int pagesize;

	pagesize = MDBM_PAGE_SIZE(db);
	kv.key.dptr = alloca(pagesize);
	kv.key.dsize = pagesize;
	kv.val.dptr = 0;
	kv.val.dsize = 0;

	if (_Mdbm_chaining(db)) {
		kv = mdbm_chain_first(db, kv);
	} else {
		kv = mdbm_first(db, kv);
	}
	ST(0) = sv_newmortal();
	if (kv.key.dptr) {
		sv_setpvn(ST(0), kv.key.dptr, kv.key.dsize);
	}
    }

int
mdbm_NEXTKEY(db, key)
	MDBM_File	db
	datum		key
    CODE:
    {
	kvpair kv;
	int pagesize;

	pagesize = MDBM_PAGE_SIZE(db);
	kv.key.dptr = alloca(pagesize);
	kv.key.dsize = pagesize;
	kv.val.dptr = 0;
	kv.val.dsize = 0;

	if (_Mdbm_chaining(db)) {
		kv = mdbm_chain_next(db, kv);
	} else {
		kv = mdbm_next(db, kv);
	}
	ST(0) = sv_newmortal();
	if (kv.key.dptr) {
		sv_setpvn(ST(0), kv.key.dptr, kv.key.dsize);
	}
    }

int
mdbm_chain(db)
	MDBM_File       db
    CODE:
	mdbm_set_chain(db);

int
mdbm_fd(db)
	MDBM_File       db
    CODE:
	RETVAL = db->m_fd;
    OUTPUT:
	RETVAL

int
mdbm_invalidate(db)
	MDBM_File	db
    CODE:
	RETVAL = mdbm_invalidate(db);
    OUTPUT:
	RETVAL

void
mdbm_sethash(db, number)
	MDBM_File	db
	int		number
    CODE:
	mdbm_sethash(db, number);

void
mdbm_sync(db)
	MDBM_File       db
    CODE:
	mdbm_sync(db);

