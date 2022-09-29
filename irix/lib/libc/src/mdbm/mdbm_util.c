/*
** mdbm_util.c
**
**  mdbm routines needed by both core libc routines, 
**  and libmdbm support routines.
*/

#include "common.h"
#include <stdlib.h>
#include <sys/param.h>

const datum	_mdbm_nulldatum = {NULL, 0};
const kvpair	_mdbm_nullkv = {NULL, 0};

/*
** _mdbm_genlock_find
** Check to see if I am holding the lock on a given page
*/
/*ARGSUSED*/
mdbm_genlock_t *
_mdbm_genlock_find(MDBM *db, char *page, mdbm_genlock_t *locks)
{
	debug(DEBUG_GL_FIND|DEBUG_ENTRY,
	       ("looking for genlock for 0x%x\n", page));
	for (;locks;locks = locks->prev) {
		debug(DEBUG_GL_FIND|DEBUG_GUTS,
		       ("genlock %x: page %d/0x%x = %d\n", 
			locks, locks->pageno, locks->page, locks->genlock));
		if (locks->page == page) {
			if (locks->flags & MDBM_LOCK_LOCKED) {
				debug(DEBUG_GL_FIND|DEBUG_INFO,
				       ("genlock found locked\n"));
				return(locks);
			} else {
				debug(DEBUG_GL_FIND|DEBUG_INFO,
				       ("genlock found unlocked\n"));
				return(0);
			}
		}
	}
	debug(DEBUG_GL_FIND|DEBUG_INFO,("genlock not found\n"));
	return(0);
}

/*
** genlock_get
** return the generation lock for a page if the page is unlocked.
** spin until it is unlocked.
*/
/*ARGSUSED*/
uint32
_mdbm_genlock_get(MDBM *db, char *page, mdbm_genlock_t *locks)
{
	uint32 genlock;
	unsigned int i = 1;

	debug(DEBUG_GL_FIND|DEBUG_ENTRY,("getting lock for %d/0x%x\n", 
					 PNO(db,page), page));
	genlock = PAGE_GENLOCK(page);
	if (!_mdbm_genlock_find(db,page, locks)) {
		while (genlock & 1) {
			debug(DEBUG_GL_FIND|DEBUG_INFO,
			       ("page 0x%x locked: 0x%x\n", page, genlock));
			if (i > 32) {
				debug(DEBUG_GL_FIND|DEBUG_ERROR,
				   ("Giving up getting unlocked page %x %d\n", 
				    page,PNO(db,page)));
				setoserror(EDEADLK);
				return 0;
			}
			usleep(1000 * i++);
			genlock = PAGE_GENLOCK(page);
		}
	}
	setoserror(0);
	return genlock;
}

/*
** _mdbm_genlock_unlock_frame
** Unlock all locks that are held by this frame
*/
int
_mdbm_genlock_unlock_frame(MDBM *db, mdbm_genlock_t *tail, 
			   mdbm_genlock_t *head)
{
	int ret = 0;
	debug(DEBUG_GL|DEBUG_ENTRY,("unlocking frame\n"));
	while (tail && tail != head) {
		if (!_mdbm_genlock_unlock(db, tail->page, tail)) {
			ret ++;
		}
		tail = tail->prev;
	}
	return (ret);
}

/*
** genlock_unlock
** remove the lock on page.   Should only be used by the holder of
** a locked gained by genlock_lock.  Ensures page will be unlocked, even if 
** was previously unlocked.
*/
/*ARGSUSED*/
int
_mdbm_genlock_unlock(MDBM *db, char *page, mdbm_genlock_t *locks) 
{
	uint32 genlock;
	char *pgl;
	mdbm_genlock_t *lock;
	uint32 glp;

	debug(DEBUG_GL|DEBUG_ENTRY,("unlocking %d/0x%x\n",PNO(db,page),page));

	if (!(lock = _mdbm_genlock_find(db, page, locks))) {
		debug(DEBUG_GL|DEBUG_ERROR,("failed to find genlock\n"));
		setoserror(ENOLCK);
		return (0);
	}
	glp = lock->genlock;

	genlock = PAGE_GENLOCK(page);
	
	/* increment by two and unset the low bit.  if it's 0, set it to 2 */
	genlock = (genlock+2) & ~1;
	if (!genlock) genlock=2;

	pgl=(char *)&PAGE_GENLOCK(page);
	if (! compare_and_swap(pgl, glp, genlock)) {
		debug(DEBUG_GL|DEBUG_INFO,("lock of %x %d was stolen! %x -> %x\n", 
		       page, PNO(db,page), glp, *pgl));
		setoserror(EDEADLK);
		return(0);
	}
	lock->flags &= ~MDBM_LOCK_LOCKED;

	return(genlock);
}


static int
_mdbm_genlock_qsort(const void *a, const void *b) 
{
	mdbm_genlock_t *ag, *bg;

	ag = (mdbm_genlock_t *)a;
	bg = (mdbm_genlock_t *)b;

	if (ag->pageno < bg->pageno) {
		return -1;
	} else if (ag->pageno > bg->pageno) {
		return 1;
	} else {
		return 0;
	}
}

/*
** mdbm_genlock_livelock
** attempt to lock a page that may be in livelock contention
*/
static mdbm_genlock_t *
_mdbm_genlock_livelock(MDBM *db, char *page, uint32 gl0, mdbm_genlock_t *locks,
		       mdbm_genlock_t *newlock, int higherHeld) 
{
	int i, higherCount=0, failcount=0;
	mdbm_genlock_t *mg, *mg2, **higherHeldPages;

	debug(DEBUG_GL|DEBUG_ENTRY,
	       ("attepmting livelock for page 0x%x\n", page));

	/* 
	** allocate enough space to hold the array mdbm_genlocks * that
	** we are forced to unlock
	*/
	higherHeldPages = (mdbm_genlock_t **)
		alloca((unsigned int)sizeof(mdbm_genlock_t *) * higherHeld);
	bzero(higherHeldPages,sizeof(mdbm_genlock_t *) * higherHeld);

	/*
	** unlock all pages that have a higher page number than the one that
	** we want to lock, then sort them by page number.
	*/
	for (mg = locks ; mg ; mg = mg->prev) {
		if (mg->page > page && mg->flags & MDBM_LOCK_LOCKED) {
			higherHeldPages[higherCount++]=mg;
			_mdbm_genlock_unlock(db, mg->page, mg);
		}
	}
	qsort(higherHeldPages, higherHeld, sizeof(mdbm_genlock_t *), 
	      _mdbm_genlock_qsort);

	/*
	** lock the page that caused all of this hubbub
	*/
	if (!(mg2 = _mdbm_genlock_lock(db, page, gl0, _mdbm_nulldatum, 
				       locks, newlock))) {
		failcount++;
	}
	/*
	** and the rest of them
	*/
	for(i=0; i < higherHeld; i++) {
		if (!_mdbm_genlock_lock(db, higherHeldPages[i]->page,gl0, 
					_mdbm_nulldatum, locks, mg)) {
			failcount++;
			setoserror(EDEADLOCK);
			debug(DEBUG_GL|DEBUG_ERROR,
			       ("Failed to relock page 0x%x\n",
				higherHeldPages[i]->page));
		}
	}

	if (failcount) {
		if (mg2) {
			_mdbm_genlock_unlock(db, mg2->page, mg2);
		}
		return NULL;
	} else {
		return locks;
	}
}

/*
** mdbm_genlock_lock
** aquire a lock on page.
** makes sure that the lock on page 0 (gl0) has not changed.  Ensures that 
** page will be locked.
*/

mdbm_genlock_t *
_mdbm_genlock_lock(MDBM *db, char *page, uint32 gl0, datum key, 
		   mdbm_genlock_t *locks, mdbm_genlock_t *newlock) 
{
	uint32 genlock, glp;
	char *pg0;
	int higherHeld = 0, i = 1;
	mdbm_genlock_t *mg;

	debug(DEBUG_GL|DEBUG_ENTRY,("locking %d/0x%x\n",PNO(db,page),page));

	for (mg=locks; mg ; mg = mg->prev) {
		/* do I already have this lock? */
		if (mg->page == page && mg->flags & MDBM_LOCK_LOCKED) {
			return (locks);
		}
		if (mg->page > page && mg->flags & MDBM_LOCK_LOCKED) {
			higherHeld++;
		}
	}

	genlock = glp = _mdbm_genlock_get(db,page, locks);
	if ((! genlock) && (oserror() == EDEADLK)) {
		return NULL;
	}
	genlock = (genlock+1) | 1;
	while (! compare_and_swap(&PAGE_GENLOCK(page),glp, genlock)) {
		if ((! genlock) && (i > 16) && (oserror() == EDEADLK))  {
			if (higherHeld) {
				return (_mdbm_genlock_livelock(db, page, gl0, 
							       locks, newlock,
							       higherHeld));
			}
			return NULL;
		}
		debug(DEBUG_GL|DEBUG_INFO,
		       ("lock of %x(%d) [%d] changed %x -> %x\n",
			page, PNO(db,page), i, glp, genlock));
		sginap(i++);
		genlock = glp = _mdbm_genlock_get(db,page, locks);
		genlock = (genlock+1) | 1;
	}
	
	/* 
	** update the new lock structure
	*/
	newlock->genlock = genlock;
	newlock->page = page;
	newlock->pageno = PNO(db,page);
	newlock->flags |= MDBM_LOCK_LOCKED;
	/* 
	** If the lock alread has a prev pointer, then we want to return
	** the tail of the locks as our return value, not the new lock
	** which is alread in the chain
	*/
	if (! newlock->prev) {
		newlock->prev = locks;
		mg = newlock;
	} else {
		mg = locks;
	}

	/* 
	** If we are locking page 0, then we just bumped the genlock up
	** Thus compare to the right thing.  This has the extra benefit
	** of barfing if setting the genlock caused other than a +1 to happen
	*/
	pg0 = PAG_ADDR(db,0);
 	if (pg0 == page) {
		gl0++;
	}

	if ((gl0 != PAGE_GENLOCK(pg0)) &&
	    (!key.dptr || 
	     (page != PAG_ADDR(db,GETPAGE(db,_Exhash(db,key)))))) {
		debug(DEBUG_GL|DEBUG_INFO,
		       ("lock on page 0 changed, backoff [page %x]: %x -> %x\n"
			, page, gl0, PAGE_GENLOCK(pg0))); 
		_mdbm_genlock_unlock(db, page, mg);
		setoserror(EBUSY);
		mg=NULL;
	}

	return(mg);
}


/*
** Check to see if the datum that XTRACT finds resides on page
*/
int
_mdbm_xtract_bad (MDBM *db, char *page, datum item) {
	register char *top = TOP(db,page);
	register char *bottom = page - (2 * NUM(page));
	
	if ((bottom < top) ||
	    (item.dptr < top) ||
	    (item.dsize < 0) ||
	    (item.dptr + item.dsize > bottom)) {
		setoserror(EBADF);
		return -1;
	}
	return(0);
}

/*
**
*/
int
_mdbm_page_bad (MDBM *db, char *page) {
	register char *top;
	register char *bottom;
	register int n;
	
	if ((! page) || (! db)) {
		setoserror(EBADF);
		return -1;
	}

	if ((page < db->m_db) || 
	    (page > db->m_db + DB_SIZE(db->m_npgshift ,db->m_pshift))) {
		setoserror(EBADF);
		return -1;
	}

	top = TOP(db,page);
	n = NUM(page);
	bottom = page - (2 * n);
	
	if ((bottom <= top) || (ntohs(INO(page,n)) > (bottom - top))) {
		setoserror(EBADF);
		return -1;
	}
	return(0);
}	


/*
 * search for the key in the page.
 * return offset index in the range 0 < i < n.
 * return 0 if not found.
 * WARNING! only safe to use if pag is genlocked
 */
int
_mdbm_seepair(MDBM *db, char *pag, datum key)
{
	register int	 i, n;
	uint16	 koff = 0, doff;
	datum item;

	n = -ntohs(NUM(pag));
	for (i = -1; i > n; i -= 2) {
		XTRACT(db, item, i, pag, koff, doff);
		if (key.dsize == item.dsize) {
			if (_Mdbm_memcheck(db) && 
			    _mdbm_xtract_bad(db,pag,item)) {
				return 0;
			} 
			if (!memcmp(item.dptr, key.dptr, item.dsize)) {
				return i;
			}
		}
		koff = ntohs(_INO(pag, i-1));
	}
	
	setoserror(0);
	return 0;
}

int 
_mdbm_write_access_check(MDBM *db)
{
	if (MDBM_BAD(db)) {
		setoserror(EINVAL);
		return 1;
	}
	if (_Mdbm_rdonly(db)) {
		setoserror(EPERM);
		return 1;
	}
	if (MAP_OBSOLETE(db) && !_mdbm_remap(db,NULL)) {
		/* remap will set errno */
		return 1;
	}
	return 0;
}

int
_mdbm_write_header_check(MDBM *db)
{
	int number_pages, i, elements=0;

	if (_mdbm_write_access_check(db)){
		return 1;
	}
     
	/* 
	** The header takes 2 elements.
	**
	** If there are more than 2 elements in the database then we have
	** user inserted keys.   
	*/
	number_pages = MDBM_NUMBER_PAGES(db);
	for (i=0; i < number_pages ; i++) {
		elements += NUM(PAG_ADDR(db,i));
		if (elements > 2) {
			setoserror(EEXIST);
			return 1;
		}
	}
	return 0;
}
		

int
_mdbm_resize(MDBM *db, uint64 size, mdbm_genlock_t *locks)
{
#ifdef DEBUG
	char *old = db->m_db;
	uint32 pages = db->m_npgshift;
#endif
	if (db->m_fd < 0) {
		setoserror(EINVAL);
		return (0);
	}

	debug(DEBUG_UTIL|DEBUG_ENTRY,
	       ("resizing DB 0x%x to %u bytes\n", db, size));

#ifdef TRACE_MMAP
	_mdbm_trace_munmap(db->m_db, DB_SIZE(db->m_npgshift, db->m_pshift));
#endif
	if (db->m_db != (char *)MAP_FAILED) {
		(void) munmap(db->m_db, DB_SIZE(db->m_npgshift, db->m_pshift));
	}
        if ((db->m_db = mmap(0, (size_t) size, PROT, MFLAGS, db->m_fd, 0)) == 
	    (char *) MAP_FAILED) {
	     debug(DEBUG_ERROR, 
		    ("can't remap database (errno = %d)\n", oserror()));
	        return(0);
        }
#ifdef TRACE_MMAP
	_mdbm_trace_mmap(db->m_db, size);
#endif

	debug(DEBUG_UTIL|DEBUG_DETAIL,
	       ("remaping 0x%x (%d pages) -> 0x%x (%d pages)\n", 
		old, 1<< pages, db->m_db, 1<< db->m_npgshift));

	/* relocate held locks */
	for (; locks ; locks = locks->prev) {
#ifdef DEBUG
		char *old = locks->page;
#endif
		locks->page = PAG_ADDR(db, locks->pageno);
		debug(DEBUG_UTIL|DEBUG_GUTS,
		      ("fixing genlock 0x%x %d/0x%x -> 0x%x",
		       locks, locks->pageno, old, locks->page));
	}

	/* caller must update m_dir, m_npgshift & m_pshift themself */
	return (1);
}

static void
_mdbm_abort_remap(MDBM *db, size_t size)
{
	debug(DEBUG_ERROR, ("Aborting remap on 0x%x m_db=0x%s size=%d\n",
			    db, db->m_db,size));
	if (size && (db->m_db !=  (char *)MAP_FAILED)) {
#ifdef TRACE_MMAP
		_mdbm_trace_munmap(db->m_db, size);
#endif
		munmap(db->m_db, size);
	}
	db->m_db = (char *)MAP_FAILED;
	db->m_dir = 0;
	db->m_npgshift = 0;
	db->m_pshift = 0;
	db->m_toffset = 0;
	db->m_amask = 0;
	db->m_max_free = 0;
}
	

/*
 * Remap the database - called at open and when size has changed.
 * This needs to work for both readonly & read/write databases.
 * Note: this is always called to grow the mappings, never to shrink them.
 */
int
_mdbm_remap(MDBM *db, mdbm_genlock_t *locks)
{
	struct	stat	sbuf;
	uint64  size, dbsize;
	int remapcount=0;

	if (db->m_fd < 0) {
		setoserror(EINVAL);
		return (0);
	}

mdbm_remap_during_remap:

#define FSIZE(fd) ((fstat((fd), &sbuf) == -1) ? 0 : ((uint64)sbuf.st_size))

	size = FSIZE(db->m_fd);

	if (size < MDBM_MINPAGE) {
		debug(DEBUG_ERROR,("bad size %u\n", (uint32)size));
		setoserror(EINVAL);
		return (0);
	}

	if (!_mdbm_resize(db, size, locks)) {
		_mdbm_abort_remap(db, 0);
		return 0;
	}

	db->m_npgshift = DB_HDR(db)->mh_npgshift;
	db->m_pshift = DB_HDR(db)->mh_pshift;
	db->m_toffset = (1<<db->m_pshift) - PAGE_TOP_SIZE;
	db->m_amask = (DB_HDR(db)->mh_dbflags & 0x07);
	db->m_max_free = (uint32) (PAGE_SIZE(db) - PAGE_TOP_SIZE);
	db->m_dir = GET_DIR(db->m_npgshift, db->m_pshift);

	dbsize = DB_SIZE(DB_HDR(db)->mh_npgshift, DB_HDR(db)->mh_pshift);
	if (! dbsize ) {
		setoserror(EBADF);
		debug(DEBUG_ERROR,("zero length mdbm.  Unmapping\n"));
		_mdbm_abort_remap(db, size);
		return (0);
	}

	if (_Mdbm_repair(db)) {
		return(1);
	}
		
	if (DB_HDR(db)->mh_magic != htonl(_MDBM_MAGIC)) {
		setoserror(EBADFD);
		debug(DEBUG_ERROR,("bad header\n"));
		_mdbm_abort_remap(db, size);
		return (0);
	}

	if (DB_HDR(db)->mh_version != htons(_MDBM_VERSION)) {
		setoserror(EBADF);
		debug(DEBUG_ERROR,("bad file version\n"));
		_mdbm_abort_remap(db, size);
		return (0);
	}

	if (_Mdbm_invalid(db)) {
		setoserror(ESTALE);
		debug(DEBUG_ERROR,("invalidated file\n"));
		_mdbm_abort_remap(db,size);
		return (0);
	}

	if (size != dbsize) {
		debug(DEBUG_ERROR,
		       ("File size does not match header: %llu, %llu\n",
			size, dbsize));
		_mdbm_abort_remap(db, size);
		if (remapcount++ > 3) {
			setoserror(EINVAL);
			return(0);
		} else {
			goto mdbm_remap_during_remap;
		}
	}

	debug(DEBUG_UTIL|DEBUG_GUTS,
	       ("remapped DB size=%s dsize=%s db=%x dir=%x\n", 
		p64(size), p64(DIR_SIZE(db->m_npgshift)), 
		db->m_db, db->m_dir));

	return (1);
}

/*
 * All important binary tree traversal.
 */
uint64
_mdbm_getpage(MDBM *db, uint64 hash)
{
	register char *dir;
	register uint64 max_dbit, dbit = 0, h = hash;
	register int    hbit = 0;

	/* 
	** deal with presplit databases
	*/
	if (DB_HDR(db)->mh_minp_level != 0) {
		if (DB_HDR(db)->mh_minp_level != -1) {
			max_dbit = MASK(DB_HDR(db)->mh_minp_level) - 2;
			while (dbit <= max_dbit) {
				dbit = (dbit << 1) + 
					((h & (1 << hbit++)) ? 2 : 1);
			}
		} else {
			max_dbit = MAX_NPG(db) - 2;
			dir = db->m_dir;

			while (dbit <= max_dbit && GETDBIT(dir, dbit)) {
				dbit = (dbit << 1) + 
					((h & (1 << hbit++)) ? 2 : 1);
			}
		}
	}

	db->m_curbit = dbit;
	db->m_level = hbit;
	return (h & ((1 << hbit) - 1));
}


int
_mdbm_sync_page(MDBM *db, char *page) 
{
	return msync(TOP(db,page),MDBM_PAGE_SIZE(db), MS_SYNC);
}


#ifdef DEBUG
char *
p64(unsigned long long num) 
{
	static	char	buf[20][8];
	static	int	nextbuf = 0;
	char	*s = buf[nextbuf++];
	char	*tag = "\0KMGTPS";
	double	amt = num;
	int	i;
	char	*t;
	
	for (i = 0; i < 7 && amt >= 512; i++) {
		amt /= 1024;
	}
	sprintf(s, "%.2f", amt);
	for (t = s; t[1]; t++);
	while (*t == '0') *t-- = 0;
	if (*t == '.') t--;
	t[1] = tag[i]; 
	t[2] = 0;
	if (nextbuf == 7) {
		nextbuf = 0;
	}
	return (s);
}
#endif

#ifdef TRACE_MMAP
typedef struct mmap_trace {
	void *			addr;
	size_t			len;
	struct mmap_trace *	next;
} mmap_trace_t;
mmap_trace_t * _mdbm_mmap_trace = NULL;

void
_mdbm_trace_mmap(void *addr, size_t len) 
{
	mmap_trace_t *mt1;

	debug(DEBUG_DETAIL|DEBUG_UTIL,("mdbm_trace_mmap(0x%x,%u)\n",addr,len));

	if (addr == MAP_FAILED || len == 0) {
#ifndef DEBUG
		abort();
#endif
		return;
	}

	mt1 = malloc(sizeof(mmap_trace_t));

	mt1->addr = addr;
	mt1->len = len;
	mt1->next = _mdbm_mmap_trace;
	_mdbm_mmap_trace = mt1;

	return;
}

void
_mdbm_trace_munmap(void *addr, size_t len)
{	
	mmap_trace_t *mt1, *mt2;

	debug(DEBUG_DETAIL|DEBUG_UTIL,
	      ("mdbm_trace_munmap(0x%x,%u)\n",addr,len));

	if (addr == MAP_FAILED || len == 0) {
		return;
	}

	mt1 = _mdbm_mmap_trace;
	mt2 = NULL;
	while (mt1) {
		if ((mt1->addr == addr) && (mt1->len == len)) {
			if (mt2) {
				mt2->next = mt1->next;
			} else {
				_mdbm_mmap_trace = mt1->next;
			}
			free(mt1);
			return;
		}
		mt2 = mt1;
		mt1 = mt1->next;
	}
	debug(DEBUG_ERROR,
	      ("FAILED mdbm_trace_munmap(0x%x,%u)\n",addr,len));
#ifndef DEBUG
	abort();
#endif
}
#endif /* TRACE_MMAP */
