/*
** mdbm_support.c
**
**    mdbm support operations
**
*/

#include <abi_mutex.h>

#include "common.h"

#if 0
void
mdbm_dump(MDBM *db)
{
	kvpair	kv;
	char *key , *value;
	int pagesize;

	pagesize = MDBM_PAGE_SIZE(db);

	key = alloca(pagesize);
	value = alloca(pagesize);

	for (kv.key.dptr = key, kv.key.dsize = pagesize,
		     kv.val.dptr = value, kv.val.dsize = pagesize,
		     kv = mdbm_first(db,kv);
	     kv.key.dsize != 0;
	     kv.key.dptr = key, kv.key.dsize = pagesize,
		     kv.val.dptr = value, kv.val.dsize = pagesize,
		     kv = mdbm_next(db,kv)) {
		printf("%s:%s\n", pk(kv.key), pv(kv.val));
	}
}
#endif

void
mdbm_elem_per_page(MDBM *db, int *inoperpage)
{
	int i;
	int max_page =  1 << db->m_npgshift;

	for (i=0 ; i < max_page ; i++) {
		inoperpage[i] = NUM(PAG_ADDR(db,i));
	}
	return;
}

void
mdbm_bytes_per_page(MDBM *db, int *bytesperpage)
{
	int i,num;
	int max_page =  1 << db->m_npgshift;
	char *pag;

	for (i=0 ; i < max_page ; i++) {
		pag = PAG_ADDR(db,i);
		num = NUM(pag);
		bytesperpage[i] = num?INO(pag, num):0;
	}
	return;
}

int
mdbm_pre_split(MDBM *db, uint64 n, int flags)
{
	register int i;
	uint64 dbsize, m, q;
	int synced = 0;

	/* This can return a EEXIST, which says that there is already 
	   data in the database and this is not allowed.  This can be 
	   considered a "soft" error for programs that blindly
	   open(CREATE)/pre_split/...   */
	
	if (_mdbm_write_header_check(db)) {
		return -1;
	}

	if (n > 0xff) {
		debug(DEBUG_SUPPORT|DEBUG_ERROR,
 ("mdbm_pre_split: requested size (%d) bigger than max supported size (%d).\n",
 (int)n, 0xff));
		setoserror(EINVAL);
		return -1;
	}

	/* if used, this function must be called immediately */
	/* after dbm_open() 				     */
	m=1<<n;
        if (n > DB_HDR(db)->mh_max_npgshift) {
                n = DB_HDR(db)->mh_max_npgshift;
        }

        dbsize = DB_SIZE(n, db->m_pshift);

	/* extend the file and remap */
	if (RE_SIZE(db, dbsize, db->locks)) {
		(db)->m_flags |= _MDBM_IOERR;
		debug(DEBUG_IOERR, ("ioerr at %s %d\n", __FILE__, __LINE__));
                return (-1);
        }

	/* update dir[] to pre-split the tree */
	DB_HDR(db)->mh_minp_level = DB_HDR(db)->mh_npgshift = db->m_npgshift=n;
        db->m_dir = GET_DIR(n, db->m_pshift);
	q  = --m / BITS_PER_BYTE;
	i  = m % BITS_PER_BYTE;
	memset(db->m_dir, 0xff, q);
	while (i > 0) {
                db->m_dir[q] |= (1 << --i);
	}

	/* tag this as a "perfectly balanced tree" */
	DB_HDR(db)->mh_dbflags |= _MDBM_PERFECT;

	/*
	** Attempt to allocate disk blocks for to fill out the space requested
	** by the ftruncate.   If possible, return an error if we cant get
	** the requested blocks.
	*/

	if (flags & MDBM_ALLOC_SPACE) {
		int start;
		int len;

		start = MDBM_PAGE_SIZE(db);
		len = ((1 << db->m_npgshift) -1) * MDBM_PAGE_SIZE(db);

#ifdef F_RESVSP64
		if (db->m_fd) {
			flock64_t fsize = {0};
			fsize.l_whence = 0;
			fsize.l_start = start;
			fsize.l_len = len ;
			if (fcntl (db->m_fd, F_RESVSP64, &fsize) != 0) {
				/*
				** EINVAL implies that what we are asking
				** is invalid.  This is always the case if
				** we are not on a fstype that supports
				** the F_RESVSP fcntl
				*/
				if (oserror() != EINVAL) {
					return (-1);
				}
			}
		}
#endif
		memset(db->m_db + start, 0, len);
#ifdef F_RESVSP64
		/*
		** Paranoia ... sync the whole database to disk
		** with blocks zeroed
		*/
		msync(db->m_db + start,  len, MS_SYNC);
		synced=1;
#endif
	}
	if (_Mdbm_sync(db) && ! synced) {
		mdbm_sync(db);
	}


	return(0);
}

int
mdbm_limit_size(MDBM *db, uint64 max_page, int (*shake)(MDBM *, datum, datum, void *))
{
	uchar i;

	if (!shake) {
               debug(DEBUG_ERROR,("mdbm_limit_size: must supply shake function\n"));
	         setoserror(EINVAL);
		 return -1;
	}
	db->m_shake = shake;

	if (max_page > 0xff) {
                debug(DEBUG_ERROR,("mdbm_limit_size: requested size bigger than max supported size.\n"));
                setoserror(EINVAL);
                return -1;
        } else {
                i = (uchar)max_page;
        }

	if (db->m_npgshift > i) {
		debug(DEBUG_ERROR,("mdbm_limit_size: database is already bigger then requested size limit\n"));
		setoserror(EINVAL);
		return -1;
	}

	/* EEXIST is a soft error ... we need to set db->m_shake */
	/* Allow for databases to be "grown" */
	if (_mdbm_write_header_check(db) && (errno != EEXIST)) {
		return -1;
	}

	/* Dont bother writing the same size into the databasse */
	if (i > db->m_npgshift) {
		DB_HDR(db)->mh_max_npgshift = (uchar) i;

		if (_Mdbm_sync(db)) {
			_mdbm_sync_page(db,PAG_ADDR(db,0));
		}
	}
	return (0);
}

int
mdbm_set_alignment(MDBM *db, int amask)
{
	if (_mdbm_write_header_check(db)) {
		return -1;
	}

	/* 0x07 is 2^3, which is the max supported value of alignment */
	if (DB_HDR(db)->mh_dbflags & 0x07) {
		db->m_amask = DB_HDR(db)->mh_dbflags & 0x07;
		setoserror(EEXIST);
		return -1;
	}

	db->m_amask = (amask & 0x07);
	DB_HDR(db)->mh_dbflags |= (amask & 0x07);

	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,PAG_ADDR(db,0));
	}


	return 0;
}

#ifdef MDBM_COMPRESS

#define MERGE_PAGE(db, left, right)	split_page(db, right, left, _merge, 0)

/* ARGSUSED */
static int
_merge(struct mdbm * db, datum key, datum val, void *param)
{
	return 1;
}
static int
__is_dir(MDBM *db, uint64 dbit)
{
	if (dbit < (MAX_NPG(db) - 1) && GETDBIT(db->m_dir, dbit))
		return 1;
	else
		return 0;
}

static uchar
find_min_level(MDBM *db, uint64 dbit, int level)
{
        uchar min_level,r_level;
#define LEFT_CHLD  ((dbit<<1)+1)
#define RIGHT_CHLD ((dbit<<1)+2)

	if (!__is_dir(db, LEFT_CHLD) || !__is_dir(db, RIGHT_CHLD))
		return  level +1;

        min_level = find_min_level(db, LEFT_CHLD, level+1);
        r_level   = find_min_level(db, RIGHT_CHLD, level+1);
        if (r_level < min_level) min_level = r_level;
	return min_level;
}

static uchar
__min_level(MDBM *db)
{
	if (!__is_dir(db, 0))
		return 0;
	return find_min_level(db, 0LL, 0);
}

/*
 *  recursive tree merge function
 */
static uchar
merge_all_branch(MDBM *db, uint64 dbit, uint64 pageno, int level)
{
	uchar max_level;
#define LEFT_CHLD  ((dbit<<1)+1)
#define RIGHT_CHLD ((dbit<<1)+2)

	max_level = level + 1;
	if (__is_dir(db, LEFT_CHLD))
		max_level = merge_all_branch(db, LEFT_CHLD, pageno, level+1);
	if(__is_dir(db, RIGHT_CHLD)) {
		uchar r_level;
		r_level = merge_all_branch(db, RIGHT_CHLD, (pageno | (1LL<<level)), level+1);
		if (r_level > max_level) max_level = r_level;
	}
	if (!__is_dir(db, LEFT_CHLD) && !__is_dir(db, RIGHT_CHLD)) {
		char *lpag = PAG_ADDR(db, pageno), *rpag = PAG_ADDR(db, (pageno | (1LL<<level)));

		if ((ALGNED(db, OFF(lpag)) + OFF(rpag) + ((NUM(lpag) + NUM(rpag) + 1) * INO_SIZE)) > PAGE_SIZE(db))
			goto done;
		if (NUM(rpag))
			MERGE_PAGE(db, lpag, rpag);
		UNSETDBIT(db->m_dir, dbit);
		DB_HDR(db)->mh_dbflags &= ~_MDBM_PERFECT;

		return level;
	}
done:
	return max_level;
}

/*
** XXX
** This function has not been made to use the new mdbm_tuncate logic,
** and can do a (shrink) f_truncate..
** Nor does it honor the generation lock protocol.
*/
void
mdbm_compress_tree(MDBM *db)
{
	uchar new_npgshift;
	uint64 new_dbsize;

        if (!__is_dir(db, 0)) return;

	new_npgshift = merge_all_branch(db, 0, 0, 0);
	/* done the recursive tree merge, now fix dir[] & mmap size */
	if (new_npgshift < db->m_npgshift) {
		new_dbsize = DB_SIZE(new_npgshift, db->m_pshift);
		if (db->m_npgshift > (MIN_DSHIFT - DSHIFT_PER_PAGE)){
			memmove(GET_DIR(new_npgshift, db->m_pshift), db->m_dir, (size_t) DIR_SIZE(new_npgshift));
		}
		if (RE_SIZE(db, new_dbsize, db->locks)) {
#if 0
                	debug(DEBUG_ERROR,("Can't remap db up to %s (errno=%d)\n", p64((uint64)new_dbsize), errno));
#endif
			(db)->m_flags |= _MDBM_IOERR;
			debug(DEBUG_IOERR, ("ioerr at %s %d\n", __FILE__, 
					     __LINE__));
                	return;
        	}
		DB_HDR(db)->mh_npgshift = db->m_npgshift = new_npgshift;
		db->m_dir = GET_DIR(db->m_npgshift, db->m_pshift);
	}
	DB_HDR(db)->mh_minp_level =  __min_level(db);
}

#endif /* MDBM_COMPRESS */


/*
** mdbm_lock
** mdbm_unlock
**
** global spinlocks should not be used for most applications.
*/

int
mdbm_lock(MDBM *db)
{
	debug(DEBUG_SUPPORT|DEBUG_ENTRY,("mdbm_lock\n"));
	if (! (db->m_flags & _MDBM_RDONLY)) {
		spin_lock((abilock_t *)&(DB_HDR(db)->mh_lockspace));
	}
	return 1;
}

int
mdbm_unlock(MDBM *db)
{
	debug(DEBUG_SUPPORT|DEBUG_ENTRY,("mdbm_unlock\n"));
	if (! (db->m_flags & _MDBM_RDONLY)) {
		release_lock((abilock_t *)&(DB_HDR(db)->mh_lockspace));
	}
	return 1;
}


/*
** mdbm_setdataformat
*/

int
mdbm_set_dataformat(MDBM *db, uint8_t dataformat)
{
	if  (_mdbm_write_access_check(db)) {
		return -1;
	}

	mdbm_dataformat(db) = dataformat;

	if (_Mdbm_sync(db)) {
		_mdbm_sync_page(db,PAG_ADDR(db,0));
	}


	return 0;
}
#pragma optional mdbm_set_dataformat


