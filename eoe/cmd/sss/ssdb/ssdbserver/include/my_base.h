/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* This file includes constants used with all databases */
/* Author: Michael Widenius */

#ifndef _my_base_h
#define _my_base_h

#ifndef stdin				/* Included first in handler */
#define USES_TYPES			/* my_dir with sys/types is included */
#define CHSIZE_USED
#include <global.h>
#include <my_dir.h>			/* This includes types */
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#ifdef MSDOS
#include <share.h>			/* Neaded for sopen() */
#endif
#if !defined(USE_MY_FUNC) && !defined(THREAD)
#include <my_nosys.h>			/* For faster code, after test */
#endif	/* USE_MY_FUNC */
#endif	/* stdin */
#include <list.h>

	/* The following is parameter to ha_open() how to handle locks */

#define HA_OPEN_ABORT_IF_LOCKED		0
#define HA_OPEN_WAIT_IF_LOCKED		1
#define HA_OPEN_IGNORE_IF_LOCKED	2

	/* The following is parameter to ha_rkey() how to use key */

enum ha_rkey_function {
  HA_READ_KEY_EXACT,			/* Find first record else error */
  HA_READ_KEY_OR_NEXT,			/* Record or next record */
  HA_READ_KEY_OR_PREV,			/* Record or previous */
  HA_READ_AFTER_KEY,			/* Find next rec. after key-record */
  HA_READ_BEFORE_KEY			/* Find next rec. before key-record */
};

	/* The following is parameter to ha_extra() */

enum ha_extra_function {
  HA_EXTRA_NORMAL=0,			/* Optimize for space (def) */
  HA_EXTRA_QUICK=1,			/* Optimize for speed */
  HA_EXTRA_RESET=2,			/* Reset database to after open */
  HA_EXTRA_CACHE=3,			/* Cash record in HA_rrnd() */
  HA_EXTRA_NO_CACHE=4,			/* End cacheing of records (def) */
  HA_EXTRA_NO_READCHECK=5,		/* No readcheck on update */
  HA_EXTRA_READCHECK=6,			/* Use readcheck (def) */
  HA_EXTRA_KEYREAD=7,			/* Read only key to database */
  HA_EXTRA_NO_KEYREAD=8,		/* Normal read of records (def) */
  HA_EXTRA_NO_USER_CHANGE=9,		/* No user is allowed to write */
  HA_EXTRA_KEY_CACHE=10,
  HA_EXTRA_NO_KEY_CACHE=11,
  HA_EXTRA_WAIT_LOCK=12,		/* Wait until file is avalably (def) */
  HA_EXTRA_NO_WAIT_LOCK=13,		/* If file is locked, return quickly */
  HA_EXTRA_WRITE_CACHE=14,		/* Use write cache in ha_write() */
  HA_EXTRA_FLUSH_CACHE=15,		/* flush write_record_cache */
  HA_EXTRA_NO_KEYS=16,			/* Remove all update of keys */
  HA_EXTRA_KEYREAD_CHANGE_POS=17,	/* Keyread, but change pos */
					/* xxxxchk -r must be used */
  HA_EXTRA_REMEMBER_POS=18,		/* Remember pos for next/prev */
  HA_EXTRA_RESTORE_POS=19,
  HA_EXTRA_REINIT_CACHE=20,		/* init cache from current record */
  HA_EXTRA_FORCE_REOPEN=21		/* Datafile have changed on disk */
};

	/* The following is parameter to ha_panic() */

enum ha_panic_function {
  HA_PANIC_CLOSE,			/* Close all databases */
  HA_PANIC_WRITE,			/* Unlock and write status */
  HA_PANIC_READ				/* Lock and read keyinfo */
};

	/* The following is parameter to ha_create(); keytypes */

enum ha_base_keytype {
  HA_KEYTYPE_END=0,
  HA_KEYTYPE_TEXT=1,			/* Key is sorted as letters */
  HA_KEYTYPE_BINARY=2,			/* Key is sorted as unsigned chars */
  HA_KEYTYPE_SHORT_INT=3,
  HA_KEYTYPE_LONG_INT=4,
  HA_KEYTYPE_FLOAT=5,
  HA_KEYTYPE_DOUBLE=6,
  HA_KEYTYPE_NUM=7,			/* Not packed num with pre-space */
  HA_KEYTYPE_USHORT_INT=8,
  HA_KEYTYPE_ULONG_INT=9,
#ifdef HAVE_LONG_LONG
  HA_KEYTYPE_LONGLONG=10,
  HA_KEYTYPE_ULONGLONG=11,
#endif
  HA_KEYTYPE_INT24=12,
  HA_KEYTYPE_UINT24=13,
  HA_KEYTYPE_INT8=14
};

#define HA_MAX_KEYTYPE	15		/* Must be log2 */

	/* These flags kan be ored to key-flag */

#define HA_NOSAME		 1	/* Set if not dupplicated records */
#define HA_PACK_KEY		 2	/* Pack key to previous key */

	/* Automatic bits in key-flag */

#define HA_SPACE_PACK_USED	 4	/* Test for if SPACE_PACK used */
#define HA_SORT_ALLOWS_SAME	 128	/* Intern bit when sorting records */

	/* These flags can be order to key-seg-flag */

#define HA_SPACE_PACK		 1	/* Pack space in key-seg */
/* #define HA_PACK_KEY		 2 */	/* Pack key to previous key */
#define HA_REVERSE_SORT		 128	/* Sort key in reverse order */

	/* optionbits for database */
#define HA_OPTION_PACK_RECORD		1
#define HA_OPTION_PACK_KEYS		2
#define HA_OPTION_COMPRESS_RECORD	4
#define HA_OPTION_TEMP_COMPRESS_RECORD	((uint) 16384)	/* set by isamchk */
#define HA_OPTION_READ_ONLY_DATA	((uint) 32768)	/* Set by isamchk */

	/* Bits in flag to ni_create() */

#define HA_DONT_TOUCH_DATA	1	/* Don't empty datafile (isamchk) */

	/* Errorcodes given by functions */

#define HA_ERR_KEY_NOT_FOUND	120	/* Didn't find key on read or update */
#define HA_ERR_FOUND_DUPP_KEY	121	/* Dupplicate key on write */
#define HA_ERR_RECORD_CHANGED	123	/* Uppdate with is recoverable */
#define HA_ERR_WRONG_INDEX	124	/* Wrong index given to function */
#define HA_ERR_CRASHED		126	/* Indexfile is crashed */
#define HA_ERR_WRONG_IN_RECORD	127	/* Record-file is crashed */
#define HA_ERR_OUT_OF_MEM	128	/* Record-file is crashed */
#define HA_ERR_WRONG_COMMAND	131	/* Command not supported */
#define HA_ERR_OLD_FILE		132	/* old databasfile */
#define HA_ERR_NO_ACTIVE_RECORD 133	/* No record read in update() */
#define HA_ERR_RECORD_DELETED	134	/* Intern error-code */
#define HA_ERR_RECORD_FILE_FULL 135	/* No more room in file */
#define HA_ERR_INDEX_FILE_FULL	136	/* No more room in file */
#define HA_ERR_END_OF_FILE	137	/* end in next/prev/first/last */
#define HA_ERR_UNSUPPORTED	138	/* unsupported extension used */

	/* Other constants */

#define HA_NAMELEN 64			/* Max length of saved filename */

	/* Intern constants in databases */

	/* bits in _search */
#define SEARCH_FIND	1
#define SEARCH_NO_FIND	2
#define SEARCH_SAME	4
#define SEARCH_BIGGER	8
#define SEARCH_SMALLER	16
#define SEARCH_SAVE_BUFF	32

	/* bits in opt_flag */
#define QUICK_USED	1
#define READ_CACHE_USED	2
#define READ_CHECK_USED 4
#define KEY_READ_USED	8
#define WRITE_CACHE_USED 16

	/* bits in update */
#define HA_STATE_CHANGED	1	/* Database has changed */
#define HA_STATE_AKTIV		2	/* Has a current record */
#define HA_STATE_WRITTEN	4	/* Record is written */
#define HA_STATE_DELETED	8
#define HA_STATE_NEXT_FOUND	16	/* Next found record (record before) */
#define HA_STATE_PREV_FOUND	32	/* Prev found record (record after) */
#define HA_STATE_NO_KEY		64	/* Last read didn't find record */
#define HA_STATE_KEY_CHANGED	128
#define HA_STATE_WRITE_AT_END	256	/* set in _ps_find_writepos */
#define HA_STATE_BUFF_SAVED	512	/* If current keybuff is info->buff */

enum en_fieldtype {
  FIELD_LAST=-1,FIELD_NORMAL,FIELD_SKIPP_ENDSPACE,FIELD_SKIPP_PRESPACE,
  FIELD_SKIPP_ZERO,FIELD_BLOB,FIELD_CONSTANT,FIELD_INTERVALL,FIELD_ZERO
};

/* For number of records */
#ifdef BIG_TABLES
typedef my_off_t	ha_rows;
#else
typedef ulong		ha_rows;	
#endif

#endif /* _my_base_h */
