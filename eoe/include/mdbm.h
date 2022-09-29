/*
 * mdbm - ndbm work-alike hashed database library based on sdbm which is
 * based on Per-Aake Larson's Dynamic Hashing algorithms.
 * BIT 18 (1978).
 *
 * sdbm Copyright (c) 1991 by Ozan S. Yigit (oz@nexus.yorku.ca)
 *
 * Modifications that:
 *	. Allow 64 bit sized databases,
 * 	. used mapped files & allow multi reader/writer access,
 * 	. move directory into file, and
 *	. use network byte order for db data structures.
 * are:
 *	mdbm Copyright (c) 1995, 1996 by Larry McVoy, lm@sgi.com.
 *	mdbm Copyright (c) 1996 by John Schimmel, jes@sgi.com.
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
 */
#ifndef __MDBM_H_
#define __MDBM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#if 1
#include <netinet/in.h>
#else
#define	htons(a)	(a)	/* for now */
#define	ntohs(a)	(a)	/* for now */
#define	htonl(a)	(a)	/* for now */
#define	ntohl(a)	(a)	/* for now */
#endif
#define	htonll(a)	(a)	/* for now */
#define	ntohll(a)	(a)	/* for now */

/*
 * File format.
 *
 * The database is divided up into m_psize "pages".  The pages do not have
 * to be the same size as the system operating page size, but it is a good
 * idea to make one a multiple of the other.  The database won't create
 * pages smaller that 256 bytes.  Datums must fit on a single page so you
 * probably don't want to change this.  We default to 4K page size if no
 * blocksize is specified at create time.  The page size can't be bigger
 * than 64K because we use 16 bit offsets in the pages.  
 * 
 * Page types.  The first page is special.  It contains the database header,
 * as well as the first set of directory bits.  The directory bits occupy 
 * 8 bytes 
 *
 * The point of putting the directory in the first page was to provide for
 * a very small memory footprint for small databases; this allows this code
 * to be used as a generic hashing package.
 *
 * After the database reaches a size where the directory will no longer fit
 * in the 8 bytes of the first page, the data & directory are grown by
 * doubling its previous size.  The directory is copied to the end of data
 * area.  The default page get you to to a terabyte (2^40) database before
 * copying becomes a problem.
 *
 * Page format:
 *      +--------+----------+----------+
 *      |  key   | data     | key      |
 *      +--------+--+-------+----------+
 *      | data      |  - - - ----->    |  
 *      +-----------+------------------+
 *      |        F R E E A R E A       |
 *      +-----------+---------+--------+
 *      |  <------  | datoff  | keyoff | 
 *      +--------+--+-----+---+--------+
 *      | datoff | keyoff | n | genlock|
 *      +--------+--------+---+--------+       
 *
 * The genlock is a 32 bit per page generation count and lock.  The low
 * order bit is treated as a write lock.
 *
 * We hide our own data in a page by making keyoff == 0.  We do not allow
 * 0 length keys as a result.
 * Suppose we have a 1024 byte page, the header is 16 bytes, the directory
 * is 8 bytes. ( i.e total is 16 + 8 = 24 bytes )
 *
 * Page 0 looks like
 *      +--------+-----+----+----------+
 *      | 13 bytes header (never moves)|
 *      |    8 bytes  for directory    |
 *      +--------+-----+----+----------+
 *      |        F R E E A R E A       |
 *      +--------+----+---+---+--------+
 *      |        | 24 | 0 | 2 | genlock|
 *      +--------+----+---+---+--------+       
 *                     ^^^
 *                     this signifies a zero length key */


#define	_SPLTMAX	10	/* maximum allowed splits */
    				/* for a single insertion */
#define	_INTERNAL	(64*1024-1)

/*
 * Types. 
 */
#ifndef	uchar
# define uchar	unsigned char
#endif
#ifndef int16
# define int16	short
#endif
#ifndef uint16
# define uint16	unsigned short
#endif
#ifndef int32
# define int32	int
#endif
#ifndef uint32
# define uint32	unsigned int
#endif
#ifndef int64
# ifdef _LONGLONG
#  define int64	long long
# else
#  define int64 __long_long
# endif
#endif
#ifndef uint64
# ifdef _LONGLONG
#  define uint64	unsigned long long
# else
#  define uint64	unsigned __long_long
# endif
#endif

/*
 * This is the header for the database.  Keep it small.
 * The values are in network byte order.
 */
typedef struct {
	uint32		mh_magic;	 /* header magic number */
	uint16_t	mh_version;	 /* mdbm version number */
	uint8_t		mh_npgshift;	 /* # of pages is 1<< this*/
	uint8_t		mh_pshift;	 /* page size is 1<< this*/
	uint8_t		mh_max_npgshift; /* for bounded size db */
	uint8_t		mh_dbflags;	 /* flags */
	uint8_t		mh_dbflags2;	 /* more flags */
	uint8_t		mh_dataformat;	 /* user settable db format id */
	uint32		mh_lockspace;	 /* a place to stick a lock */
	int8_t		mh_minp_level;	 /* first level a page apear */
	uint8_t		mh_hashfunc;	 /* see values below */
} MDBM_hdr;		/* MDBM version 1 header */

#ifndef	_DATUM_DEFINED
typedef	struct {
	char	*dptr;
	int	dsize;
} datum;
#define _DATUM_DEFINED
#endif
typedef struct {
	datum key;
	datum val;
} kvpair;

typedef struct mdbm_genlock {
	uint32		genlock;	/* current value of the lock */
	int		pageno;		/* page number of the address */
	uint32		flags;		/* flags about this lock */
	char		*page;		/* address of page */
	struct mdbm_genlock *prev;	/* previous held genlock */
} mdbm_genlock_t;

/*
 * In memory handle structure.  Fields are duplicated from the 
 * header so that we don't have to byte swap on each access.
 */
struct mdbm {
	char	       *m_db;		/* database itself */
	char	       *m_dir;		/* directory bits */
	int		m_fd;		/* database file descriptor */
	char	       *m_file;		/* file name */
	int 		m_level;	/* current level */
	int		m_amask;	/* alignment mask */
	uint32		m_max_free;	/* max free area per page*/
	int 		m_toffset;	/* offset to top of page */
	uint64		m_curbit;	/* XXX - current bit number */
	uint64		m_last_hash;	/* last hash value */
	uint64		m_pageno;	/* last page fetched */
	uint8_t		m_npgshift;	/* # of pages is 1<< this*/
	uint8_t		m_pshift;	/* page size is 1<<this */
	int		m_kino;		/* last key fetched from m_page */
	int		m_next;		/* for getnext */
	uint16_t	m_flags;	/* status/error flags, see below */
	int 		(*m_shake)(struct mdbm *, datum, datum, void *);
					/* "shake" function */
	uint64		(*m_hash)(unsigned char *, int);
					/* hash function */
	mdbm_genlock_t	*locks;		/* Internal page locks */
};

typedef struct mdbm MDBM;

#define	_MDBM_RDONLY	(1<<0)	/* data base open read-only */
#define	_MDBM_IOERR	(1<<1)	/* data base I/O error */
#define _MDBM_LOCKING	(1<<2)	/* lock on read/write */
#define _MDBM_SYNC	(1<<3)	/* msync all writes */
#define _MDBM_REPAIR	(1<<4)	/* Doing the repairs on the database */
#define _MDBM_MEMCHECK	(1<<5)	/* Require on the fly memory checking */

#define	_Mdbm_rdonly(db)	((db)->m_flags & _MDBM_RDONLY)
#define _Mdbm_sync(db)		((db)->m_flags & _MDBM_SYNC)
#define _Mdbm_repair(db)	((db)->m_flags & _MDBM_REPAIR)
#define _Mdbm_memcheck(db)	((db)->m_flags & _MDBM_MEMCHECK)
#define	_Exhash(db, item)	((db)->m_hash((uchar *)item.dptr, item.dsize))

/*
** mdbm flags for the mh_dbflags field
*/
#define _MDBM_ALGN16    0x1     
#define _MDBM_ALGN32    0x3	/* Implies ALGN16 */
#define _MDBM_ALGN64    0x7     /* Implies ALGN32 */
#define _MDBM_PERFECT   0x8     /* perfectly balanced tree */
#define _MDBM_INVALID	0x10	/* a writer has marked the database invalid */
#define _MDBM_CHAIN	0x20	/* Do bucket chaining */
#define _MDBM_MEMCHECK_REQUIRED 0x40	/* require memcheck for all accesses */

/*
** mdbm flags for the mh_dbflags2 field
** (none yet)
*/

#define DB_HDR(db)	((MDBM_hdr *)((db)->m_db))
#define _Mdbm_perfect(db)       (DB_HDR(db)->mh_dbflags & _MDBM_PERFECT)
#define _Mdbm_alnmask(db)	((db)->m_amask)
#define _Mdbm_invalid(db)	(DB_HDR(db)->mh_dbflags & _MDBM_INVALID)
#define _Mdbm_chaining(db)	(DB_HDR(db)->mh_dbflags & _MDBM_CHAIN)

#define mdbm_dataformat(db)	(DB_HDR(db)->mh_dataformat)

#define MDBM_PAGE_SIZE(db)	(1<<(db)->m_pshift)
#define MDBM_CHAIN_SIZE(db)	((MDBM_PAGE_SIZE(db)) / 4)
#define MDBM_NUMBER_PAGES(db)	(1<<(db)->m_npgshift)

/*
 * flags to mdbm_store, mdbm_pre_split, etc.
 *					
 * Flags understood by mdbm_open have to be compatable with open flags
 * defined in <sys/fcntl.h>
 */
#define	MDBM_INSERT		0x0	/* mdbm_store */
#define	MDBM_REPLACE		0x1	/* mdbm_store */
#define MDBM_CHAIN_NULL		0x2	/* mdbm_chain_store */
#define	MDBM_ALLOC_SPACE 	0x80000	/* mdbm_pre_split, mdbm_open */	
#define MDBM_REPAIR		0x100000 /* mdbm_open */
#define MDBM_MEMCHECK		0x200000 /* mdbm_open */

/*
** Hash functions
*/
#define MDBM_HASH_CRC32		0	/* table based 32bit crc */
#define MDBM_HASH_EJB		1	/* from hsearch */
#define MDBM_HASH_PHONG		2	/* congruential hash */
#define MDBM_HASH_OZ		3	/* from sdbm */
#define MDBM_HASH_TOREK		4	/* from Berkeley db */
#define MDBM_HASH_FNV		5	/* very good for small keys */
#define MDBM_HASH_LAST		5

/*
 * Page size range supported.  You don't want to make it smaller and
 * you can't make it bigger - the offsets in the page are 16 bits.
 */
#define	MDBM_MINPAGE	7	/* 2^7 = 128 */
#define	MDBM_MAXPAGE	16	/* 2^16 = (64*1024) = 65536 */
#define	MDBM_PAGESIZ	12	/* 2^12 = 4096 */

/*
 * Magic number which is also a version number.   If the database format
 * changes then we need another magic number.
 */
/* #define _MDBM_MAGIC	0x01023962 */	/* obsolete version 0 mdbm file */
#define _MDBM_MAGIC	0x6d64626d	/* hex for "mdbm" */
#define _MDBM_VERSION	0x1

/*
 * Bitfields used by mdbm_check and mdbm_repair
 */
#define MDBM_CHECK_REPAIR	(1<<0)	/* Passed to repair, cause a check
					   to happen and the results will
					   be repaired */

#define MDBM_CHECK_ERR		(1<<1)	/* An error occured in processing.
					   errno will be set */

/* Unrepariable Errors */
#define MDBM_CHECK_EXIST	(1<<8)	/* Check for the existance of the
					   mdbm file */

#define MDBM_CHECK_MAGIC	(1<<16)	/* Make sure that the magic number
					   is OK */
#define MDBM_CHECK_VERSION	(1<<17)	/* Make sure that we have a
					   suppored MDBM version number */
#define MDBM_CHECK_HASH		(1<<18)	/* Check to see if the hash is
					   valid (In a database */
#define MDBM_CHECK_SIZE		(1<<19)	/* Make sure the database size
					   matches the database pagesize
					   and number of pages */
#define MDBM_CHECK_INO		(1<<27)	/* Check to see if all a entries
					   refer to valid storage */

/* Errors Repairable in the current MDBM file */
#define MDBM_CHECK_INVALID	(1<<24)	/* Check the invalid bit */
#define MDBM_CHECK_LOCKSPACE	(1<<25)	/* Check the global ABI lockspace */
#define MDBM_CHECK_GENLOCK	(1<<26)	/* Check all page locks */

/* Internally use set of flags */
#define MDBM_CHECK_CONTROL	(MDBM_CHECK_REPAIR | \
				 MDBM_CHECK_ERR)

/* Common sets of flags */
#define MDBM_CHECK_INPLACE	(MDBM_CHECK_INVALID | \
				 MDBM_CHECK_LOCKSPACE | \
				 MDBM_CHECK_GENLOCK)
#define MDBM_CHECK_NEWFILE	0
#define MDBM_CHECK_REPAIRABLE	(MDBM_CHECK_INPLACE | \
				 MDBM_CHECK_NEWFILE)
#define MDBM_CHECK_REP_NEVER	(MDBM_CHECK_EXIST | \
				 MDBM_CHECK_MAGIC | \
				 MDBM_CHECK_VERSION | \
				 MDBM_CHECK_HASH | \
				 MDBM_CHECK_SIZE)
#define MDBM_CHECK_REP_TRY	(MDBM_CHECK_INO)
#define MDBM_CHECK_UNREPAIRABLE (MDBM_CHECK_REP_NEVER | \
				 MDBM_CHECK_REP_TRY)

/* Commonly used sets of MDBM_CHECK macros */

#define MDBM_CHECK_ALL		(MDBM_CHECK_REPAIRABLE | \
				 MDBM_CHECK_UNREPAIRABLE)
#define MDBM_CHECK_REPAIR_ALL	(MDBM_CHECK_ALL | \
				 MDBM_CHECK_REPAIR)


/*
 * mdbm interface in libc
 */
extern	MDBM *	mdbm_open(char *, int, mode_t, int);
extern	void	mdbm_close(MDBM *);
extern	datum	mdbm_fetch(MDBM *, kvpair);
extern	int	mdbm_delete(MDBM *, datum);
extern	int	mdbm_store(MDBM *, datum, datum, int);
extern	kvpair	mdbm_first(MDBM *, kvpair);
extern	datum	mdbm_firstkey(MDBM *, datum);
extern	kvpair	mdbm_next(MDBM *, kvpair);
extern	datum	mdbm_nextkey(MDBM *, datum);
extern	void	mdbm_sync(MDBM *);
extern	void	mdbm_close_fd(MDBM *);
extern	int	mdbm_invalidate(MDBM *);
extern	int	mdbm_sethash(MDBM *, int);

/*
 * mdbm interface in libmdbm
 */
extern	void	mdbm_elem_per_page(MDBM *, int *);
extern	void	mdbm_bytes_per_page(MDBM *, int *);
extern	int	mdbm_lock(MDBM *);
extern	int	mdbm_unlock(MDBM *);
extern  int     mdbm_pre_split(MDBM *, uint64, int);
extern 	int 	mdbm_set_alignment(MDBM *, int);
extern  int     mdbm_limit_size(MDBM *, uint64, int (*func)(struct mdbm *, datum, datum, void *));
extern	int	mdbm_set_dataformat(MDBM *, uint8_t);

/* 
** for info on optional symbols, see /usr/include/optional_sym.h>
#pragma optional mdbm_set_dataformat
*/

/*
 * mdbm_chain interface in libmdbm
 */
extern	int	mdbm_set_chain(MDBM *);
extern	datum	mdbm_chain_fetch(MDBM *, kvpair);
extern	kvpair	mdbm_chain_first(MDBM *, kvpair);
extern	kvpair	mdbm_chain_next(MDBM *, kvpair);
extern	int	mdbm_chain_store(MDBM *, datum, datum, int);
extern	int	mdbm_chain_delete(MDBM *, datum);
extern	datum	mdbm_chainP_fetch(MDBM *, kvpair);
extern	kvpair	mdbm_chainP_first(MDBM *, kvpair);
extern	kvpair	mdbm_chainP_next(MDBM *, kvpair);
extern	int	mdbm_chainP_store(MDBM *, datum, datum, int);
extern	int	mdbm_chainP_delete(MDBM *, datum);

/*
#pragma optional mdbm_chainP_fetch
#pragma optional mdbm_chainP_first
#pragma optional mdbm_chainP_next
#pragma optional mdbm_chainP_store
#pragma optional mdbm_chainP_delete
*/

/*
 * mdbm_atomic interface in libmdbm
 */
extern	datum	mdbm_test_and_set(MDBM *, kvpair, datum);
extern	int	mdbm_compare_and_swap(MDBM *, datum, datum, datum);
extern	int	mdbm_compare_and_delete(MDBM *, datum, datum);
extern	int	mdbm_atomic_begin(MDBM *, datum, mdbm_genlock_t *);
extern	int	mdbm_atomic_end(MDBM *);

/*
#pragma optional mdbm_test_and_set
#pragma optional mdbm_compare_and_swap
#pragma optional mdbm_compare_and_delete
#pragma optional mdbm_atomic_begin
#pragma optional mdbm_atomic_end
*/

/*
 * mdbm_check interface in libmdbm
 */
extern	uint32	mdbm_check(char *, uint32);
extern	uint32	mdbm_repair(char *, uint32);

/*
#pragma optional mdbm_check
#pragma optional mdbm_repair
*/

#ifndef	NULL
# define	NULL	0L
#endif

#ifdef __cplusplus
}
#endif
#endif /* __MDBM_H_ */
