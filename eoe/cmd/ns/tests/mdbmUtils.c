#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <mdbm.h>

void mdbm_dump(MDBM *db)
{
        kvpair  kv;
        char *key , *value;
        int pagesize;

        pagesize = MDBM_PAGE_SIZE(db);

        key = (char *)alloca(pagesize);
        value = (char *)alloca(pagesize);

        for (kv.key.dptr = key, kv.key.dsize = pagesize,
                     kv.val.dptr = value, kv.val.dsize = pagesize,
                     kv = mdbm_first(db, kv);
             kv.key.dsize != 0;
             kv.key.dptr = key, kv.key.dsize = pagesize,
                     kv.val.dptr = value, kv.val.dsize = pagesize,
                     kv = mdbm_next(db, kv)) {
		kv.key.dptr[kv.key.dsize] = '\0';
/*		kv.val.dptr[kv.val.dsize] = '\0'; */
                printf("Key-VAL:%s:%s\n", kv.key.dptr, kv.val.dptr+8);
	}
}

