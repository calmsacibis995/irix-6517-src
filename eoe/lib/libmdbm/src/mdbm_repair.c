#include "common.h"

/*
** mdbm_check an mdbm_repair
** 
** These functions use a bitmask of possible problems to denote problems
** with a MDBM file
** 
** mdbm_check takes this bitmask, and returns a bitmask of conditions that
** are in need of repair
**
**
** MDBM_CHECK and repair bitmask rational
**
** Bits	0-7	control bits
** Bits 8-15	Unrepairable errors
** Bits 16-23	Repairable by creation of a new file
** Bits 24-31	Repairable in current database
*/

static int
_mdbm_check_genlock(MDBM *db, uint32 flags) 
{
	int i;
	int max_page = 1<<db->m_npgshift;
	char *page;
	uint32 gl, *pgl;
	
	debug(DEBUG_REPAIR|DEBUG_ENTRY, 
	      ("mdbm_check_genlock repair=%u\n", flags));
	for (i=0; i < max_page; i++) {
		page = PAG_ADDR(db, i);
		gl = PAGE_GENLOCK(page);
		if (gl & 1) {
			if (flags & MDBM_CHECK_REPAIR) {
				debug(DEBUG_REPAIR|DEBUG_INFO, 
				      ("repair genlock on page %d\n", i));
				gl = (gl+2) & ~1;
				pgl = (uint32 *)&PAGE_GENLOCK(page);
				*pgl = gl;
			} else {
				debug(DEBUG_REPAIR|DEBUG_INFO, 
				      ("locked genlock on page %d\n", i));
				return 1;
			}
		}
	}
	return 0;
}

static int
_mdbm_check_ino(MDBM *db, uint32 flags)
{
	kvpair kv;
	int pagesize, count=0;
	char *key, *value;

	debug(DEBUG_REPAIR|DEBUG_ENTRY, 
	      ("mdbm_check_ino repair=%u\n", flags));
	pagesize = MDBM_PAGE_SIZE(db);

	key = alloca(pagesize);
	value = alloca(pagesize);

	db->m_flags |= _MDBM_MEMCHECK;

	for (kv.key.dptr = key, kv.key.dsize = pagesize,
		     kv.val.dptr = value, kv.val.dsize = pagesize,
		     kv = mdbm_first(db,kv);
	     kv.key.dsize != 0;
	     kv.key.dptr = key, kv.key.dsize = pagesize,
		     kv.val.dptr = value, kv.val.dsize = pagesize,
		     kv = mdbm_next(db,kv)) {
		count++;
	}

	db->m_flags &= ~_MDBM_MEMCHECK;

	if (errno) {
		debug(DEBUG_REPAIR|DEBUG_INFO, 
		      ("bad ino info found for key %d\n", count));
		if (flags & MDBM_CHECK_REPAIR) {
			return -1;
		}
		return 1;
	}
	
	return 0;
}

uint32
mdbm_check(char *file, uint32 flags)
{
	MDBM *db;

	debug(DEBUG_REPAIR|DEBUG_ENTRY, 
	      ("mdbm_check flags=0x%x\n", flags));

	/* Bits 0 - 7 */
	flags &= ~MDBM_CHECK_CONTROL;

	/* Bit 8 */
	if ((db = mdbm_open(file, O_RDONLY|MDBM_REPAIR, 0444, 0)) == NULL) {
		debug(DEBUG_REPAIR|DEBUG_ERROR, 
		      ("mdbm_check open failed: %s\n", strerror(errno)));
		return MDBM_CHECK_ERR;
	} else {
		flags &= ~MDBM_CHECK_EXIST;
	}


	/* Bit 16 */
	if (flags & MDBM_CHECK_MAGIC) {
		if (DB_HDR(db)->mh_magic == _MDBM_MAGIC) {
			flags &= ~MDBM_CHECK_MAGIC;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check magic failed\n"));
		}
	}

	
	/* Bit 17 */
	if (flags & MDBM_CHECK_VERSION) {
		if (DB_HDR(db)->mh_version == _MDBM_VERSION) {
			flags &= ~MDBM_CHECK_VERSION;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check version failed\n"));
		}
	}

	/* Bit 18 */
	if (flags & MDBM_CHECK_HASH) {
		if ((DB_HDR(db)->mh_hashfunc <= MDBM_HASH_LAST)) {
			flags &= ~MDBM_CHECK_HASH;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check hash failed\n"));
		}
	}
	
	/* Bit 19 */
	if (flags & MDBM_CHECK_SIZE) {
		struct stat sbuf;

#define FSIZE(fd) ((fstat((fd), &sbuf) == -1) ? 0 : ((uint64)sbuf.st_size))
		if (DB_SIZE(DB_HDR(db)->mh_npgshift, DB_HDR(db)->mh_pshift) ==
		    FSIZE(db->m_fd)) {
			flags &= ~MDBM_CHECK_SIZE;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check size failed\n"));
		}
	}
	
	/* Bit 24 */
	if (flags & MDBM_CHECK_INVALID) {
		if (!_Mdbm_invalid(db)) {
			flags &= ~MDBM_CHECK_INVALID;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check invalid failed\n"));
		}
	}

	/* Bit 25 */
	if (flags & MDBM_CHECK_LOCKSPACE) {
		if (! (DB_HDR(db)->mh_lockspace)) {
			flags &= ~MDBM_CHECK_LOCKSPACE;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check lockspace failed\n"));
		}
	}

	/* Bit 26 */
	if (flags & MDBM_CHECK_GENLOCK) {
		if (! _mdbm_check_genlock(db, 0)) {
			flags &= ~MDBM_CHECK_GENLOCK;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check genlock failed\n"));
		}
	}

	/* Bit 27 */
	if (flags & MDBM_CHECK_INO) {
		if (! _mdbm_check_ino(db, 0)) {
			flags &= ~MDBM_CHECK_INO;
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_check ino failed\n"));
		}
	}


	mdbm_close(db);
	return flags;
}
#pragma optional mdbm_check


uint
mdbm_repair(char *file, uint32 flags)
{
	MDBM *db;

	debug(DEBUG_REPAIR|DEBUG_ENTRY, 
	      ("mdbm_repair flags=0x%x\n", flags));

	/* Bit 0 */
	if (flags & MDBM_CHECK_REPAIR) {
		flags = mdbm_check(file, flags);
	}

	/* Bits 1 - 7 */
	flags &= ~MDBM_CHECK_CONTROL;

	/* UNREPAIRABLE errors */
	if (flags & MDBM_CHECK_REP_NEVER) {
		return (flags & MDBM_CHECK_REP_NEVER);
	}

	if ((db = mdbm_open(file, O_RDWR|MDBM_REPAIR, 0644, 0)) == NULL) {
		return MDBM_CHECK_ERR;
	}

	/* 
	** repairable errors 
	*/

	/* Bit 24 */
	if (flags & MDBM_CHECK_INVALID) {
		DB_HDR(db)->mh_dbflags &= ~_MDBM_INVALID;
		flags &= ~MDBM_CHECK_INVALID;
		debug(DEBUG_REPAIR|DEBUG_INFO, 
			      ("mdbm_repair invalid\n"));
	}

	/* Bit 25 */
	if (flags & MDBM_CHECK_LOCKSPACE) {
		DB_HDR(db)->mh_lockspace = 0;
		flags &= ~MDBM_CHECK_LOCKSPACE;
		debug(DEBUG_REPAIR|DEBUG_INFO, 
			      ("mdbm_repair lockspace\n"));
	}	

	/* Bit 26 */
	if (flags & MDBM_CHECK_GENLOCK) {
		if (! _mdbm_check_genlock(db, MDBM_CHECK_REPAIR)) {
			flags &= ~MDBM_CHECK_GENLOCK;
			debug(DEBUG_REPAIR|DEBUG_INFO, 
			      ("mdbm_repair genlock\n"));
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_repair genlock failed\n"));
		}
	}

	/* Bit 27 */
	if (flags & MDBM_CHECK_INO) {
		if (! _mdbm_check_ino(db, MDBM_CHECK_REPAIR)) {
			flags &= ~MDBM_CHECK_INO;
			debug(DEBUG_REPAIR|DEBUG_INFO, 
			      ("mdbm_repair ino\n"));
		} else {
			debug(DEBUG_REPAIR|DEBUG_ERROR, 
			      ("mdbm_repair ino failed\n"));
		}
	}

	
	mdbm_close(db);
	return flags;
}
#pragma optional mdbm_repair
