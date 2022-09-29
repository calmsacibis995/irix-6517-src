#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <stddef.h>
#include <strings.h>
#include <bstring.h>
#include <alloca.h>
#include <mdbm.h>
#include "tune.h"

/*
 * useful macros - XXX - useful to dbu.c as well, make them accessable
 */
     /*
     ** Check to see if the db looks ok
     */
#define MDBM_BAD(db) ((db) == NULL || DB_HDR(db)->mh_magic != htonl(_MDBM_MAGIC) || DB_HDR(db)->mh_version != htons(_MDBM_VERSION))
#define KEY_BAD(x) ((x).dptr == NULL || (x).dsize <= 0)

     /* 
     ** max # of page 
     */
#define MAX_NPG(db)    (1LL<<((db)->m_npgshift))
#define HASHALL(db)	(MAX_NPG(db) -1)
#define GETPAGE(db, h)	((_Mdbm_perfect(db) ? (HASHALL(db) & (h)) : _mdbm_getpage(db, (h))))
     
     /*
     ** set, unset, or check a bit in the hash directory
     */
#define SETDBIT(dir, dbit)      {dir[(dbit) >> 3] |= ('\01' << ((dbit) & 0x07));}
#define UNSETDBIT(dir, dbit)      {dir[(dbit) >> 3] &= (~('\01' << ((dbit) & 0x07)));}
#define GETDBIT(dir, dbit)	(dir[(dbit) >> 3 ] & ('\01' << ((dbit) & 0x07)))

     /* 
     ** return a char * pointer to the number of inodes in a page
     */
#define	PAG_ADDR(db, off)	((db)->m_db + ((off+1) << db->m_pshift) - PAGE_TOP_SIZE)

     /*
     ** return the page number of a page.   p must be a value recieved from
     ** PAG_ADDR
     */
#define	PNO(db, p)		(uint32)(((p) - (db)->m_db - PAGE_SIZE(db) + PAGE_TOP_SIZE) >> (db)->m_pshift)

     /*
     ** common mmap settings
     */
#define	MFLAGS			MAP_SHARED
#define PROT			((db)->m_flags&_MDBM_RDONLY ? \
				    PROT_READ : PROT_READ|PROT_WRITE)

     /*
     ** location of the directory in small databases is right after the header
     */
#define MIN_DIR_OFFSET(db)	(sizeof(MDBM_hdr))

     /*
     ** Size of a page given a power of 2 size
     */
#define PAGE_SIZE(db)    	(1L<<db->m_pshift)

#define MIN_DSHIFT		2  /* minumum dir size ==  1<< this*/
#define MIN_DSIZE		(1<<MIN_DSHIFT)
#define DSHIFT_PER_PAGE (-3)	/* 8 page per byte */
#define DSHIFT(npgshift)	 (((npgshift) > (MIN_DSHIFT - DSHIFT_PER_PAGE)) ?  ((npgshift) + DSHIFT_PER_PAGE) : MIN_DSHIFT)

     /*
     ** Size of a database
     */
#define DIR_SIZE(npgshift)	(1LL<<DSHIFT(npgshift))
#define DATA_SIZE(npgshift, pshift)	(1LL<<((npgshift) + (pshift)))
#define DB_SIZE(npgshift, pshift)	((npgshift <= (MIN_DSHIFT - DSHIFT_PER_PAGE)) ? DATA_SIZE(npgshift, pshift): DATA_SIZE(npgshift, pshift) + DIR_SIZE(npgshift))

#define GET_DIR(npgshift, pshift)     ((npgshift) <= (MIN_DSHIFT - DSHIFT_PER_PAGE)) ? \
                	 ((db)->m_db + MIN_DIR_OFFSET(db)) : \
			 ((db)->m_db + DATA_SIZE(npgshift, pshift))

     /*
     ** Checked if the size of the database has changed
     */
#define M_SIZE_PTR(db) 	 ((uint16 *) &(db->m_npgshift))
#define MH_SIZE_PTR(db)  ((uint16 *) &(DB_HDR(db)->mh_npgshift))
#define SIZE_CHANGED(db)   (*M_SIZE_PTR(db) != *MH_SIZE_PTR(db))

     /*
     ** Check to see if we need to remap/reopen the file
     */
#define MAP_OBSOLETE(db)   (_Mdbm_invalid(db) || SIZE_CHANGED(db))

     /*
     ** Generate a mask to make hash address level
     */
#define MASK(level)	 ((1LL<<(level)) -1)

#define RE_SIZE(db, newsize, locks) ((ftruncate(db->m_fd, (off_t) newsize) == -1) || !_mdbm_resize(db, newsize, locks))


     /* 
     ** return a uint16 * pointer to inode n in page pag
     ** use INO if n is positive
     ** use _INO if n is already a negative count
     */
#define INO_SIZE (sizeof (uint16))
#define INO(pag, n) (((uint16 *) (pag))[-(n)])
#define _INO(pag, n) (((uint16 *) (pag))[n])

     /*
     ** return a uint16 * pointer to n of page pag (the ino count)
     */
#define NUM(pag) (ntohs(_INO(pag,0)))

     /* 
     ** return a uint32 * pointer to the generation lock for page pag
     */
#define GENLOCK_SIZE (sizeof (uint32))
#define PAGE_TOP_SIZE (int)(INO_SIZE + GENLOCK_SIZE)
#define PAGE_GENLOCK(pag) (((uint32 *)((pag) + PAGE_TOP_SIZE - GENLOCK_SIZE))[0])

     /* 
     ** return byte 0 of pg
     */
#define TOP(db, pg) (pg - (db)->m_toffset)

#define XTRACT(db, item,i,pag,off1,off2) \
{(item).dptr = TOP(db,pag) + (off1); (off2) = _INO(pag,i); \
	 (item).dsize = (off2) - (off1);}

#define DELPAIR(db, pag, key) { datum target; target = key; split_page(db, pag, 0, _delete, (void *) (&target)); }

/*  unused
#define INO_TOTAL(pag) ((NUM(pag) + 1) * INO_SIZE)
*/

#define ALGNED(db,off) ((_Mdbm_alnmask(db) & (off)) ? (~_Mdbm_alnmask(db) & (off)) + _Mdbm_alnmask(db) + 1 : off)

     /*
     ** How much space is available on a page?
     */
#define MAX_FREE(db) ((db)->m_max_free)
#define OFF(pag) (ntohs(INO(pag,NUM(pag))))
#define FREE_AREA(db, pag) (MAX_FREE(db) - (NUM(pag) * INO_SIZE) - OFF(pag))

     /*
     ** Support for compare_and_swap atomic compiler intrinsic.  
     */
#if (!defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700)) /* !Mongoose -- 710  */
int __compare_and_swap(int32 *, int32, int32);
#endif
#define compare_and_swap(a,b,c) (__compare_and_swap((int32 *)(a),(int32)(b),(int32)(c)))


/*
 *  Flags used in mdbm_genlock.flags
 */
#define MDBM_LOCK_LOCKED 	(1<<0)
#define MDBM_LOCK_MALLOC 	(1<<1)

/*
** Non public functions defined in hash.c
*/
#define _mdbm_hash_id 5
#define _mdbm_hash _mdbm_hash5
extern  uint64  _mdbm_hash0(unsigned char *, int);
extern  uint64  _mdbm_hash1(unsigned char *, int);
extern  uint64  _mdbm_hash2(unsigned char *, int);
extern  uint64  _mdbm_hash3(unsigned char *, int);
extern  uint64  _mdbm_hash4(unsigned char *, int);
extern  uint64  _mdbm_hash5(unsigned char *, int);

/*
** Non public functions defined in mdbm_util.c
*/
extern mdbm_genlock_t *_mdbm_genlock_find(MDBM *, char *, mdbm_genlock_t *);
extern mdbm_genlock_t *_mdbm_genlock_lock(MDBM *, char *, uint32, datum, 
					  mdbm_genlock_t *, mdbm_genlock_t *);
extern  uint32	_mdbm_genlock_get(MDBM *,char *, mdbm_genlock_t *);
extern	int	_mdbm_genlock_unlock(MDBM *,char *, mdbm_genlock_t *);
extern	int	_mdbm_genlock_unlock_frame(MDBM *, mdbm_genlock_t *,
					   mdbm_genlock_t *);
extern  int	_mdbm_xtract_bad(MDBM *, char *, datum);
extern	int	_mdbm_page_bad(MDBM *, char *);
extern	int	_mdbm_seepair(MDBM *, char *, datum);
extern	int	_mdbm_sync_page(MDBM *, char *);
extern	int	_mdbm_write_access_check(MDBM *);
extern	int	_mdbm_write_header_check(MDBM *);
extern	int	_mdbm_remap(MDBM *, mdbm_genlock_t *);
extern	int	_mdbm_resize(MDBM *, uint64, mdbm_genlock_t *);
extern	uint64	_mdbm_getpage(MDBM *, uint64);
#ifdef DEBUG
extern char	*p64(uint64);
#endif
#ifdef TRACE_MMAP
extern	void	_mdbm_trace_mmap(void *, size_t);
extern	void	_mdbm_trace_munmap(void *, size_t);
#endif

/* 
** symbols defined in mdbm_util.c
*/

extern	const datum	_mdbm_nulldatum;
extern	const kvpair	_mdbm_nullkv;
