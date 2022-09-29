/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.
   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* Definitions for parameters to do with handler-rutins */

#if defined(HAVE_mit_thread) && !defined(NO_HASH)
#define NO_HASH
#endif

	/* Bits in bas_flag to show what database kan do */

#define HA_READ_NEXT		1	/* Read next record with same key */
#define HA_READ_PREV		2	/* Read prev. record with same key */
#define HA_READ_ORDER		4	/* Read through record-keys in order */
#define HA_READ_RND_SAME	8	/* Read RND-record to KEY-record
					   (To uppdate with RND-read)	   */
#define HA_KEYPOS_TO_RNDPOS	16	/* ha_info gives pos to record */
#define HA_LASTKEY_ORDER	32	/* Next record gives next record
					  according last record read (even
					  if database is updated after read) */
#define HA_REC_NOT_IN_SEQ	64	/* ha_info don't return recnumber;
					   It returns a position to ha_r_rnd */
#define HA_BINARY_KEYS		128	/* Keys must be exact */
#define HA_RSAME_NO_INDEX	256	/* RSAME can't restore index */
#define HA_WRONG_ASCII_ORDER	512	/* Can't use sorting through key */
#define HA_HAVE_KEYREAD_ONLY	1024	/* Can read only keys (no record) */
#define HA_READ_NOT_EXACT_KEY	2048	/* Can read record after/befor. key */
#define HA_NO_INDEX		4096	/* No index neaded for next/prev */
#define HA_LONGLONG_KEYS	8192	/* Can have longlong as key */
#define HA_KEYREAD_WRONG_STR	16384	/* keyread returns converted strings */
#define HA_NULL_KEY		32768	/* One can have keys with NULL */

	/* Parameters for ha_open() (in register form->filestat) */
	/* HA_GET_INFO does a implicit HA_ABORT_IF_LOCKED */

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a ha_info() after open */
#define HA_READ_ONLY		16	/* File opened as readonly */
#define HA_TRY_READ_ONLY	32	/* Try readonly if can't */
					/* open with read and write */
#define HA_WAIT_IF_LOCKED	64	/* Wait if locked on open */
#define HA_ABORT_IF_LOCKED	128	/* skipp if locked on open.*/
#define HA_BLOCK_LOCK		256	/* unlock when reading some records */
#define HA_DELETE_DUPP		1024	/* Delete dupp on write */

	/* Error on write with is recoverable  (Key exist) */

#define HA_WRITE_SKIPP 121		/* Dupplicate key on write */
#define HA_READ_CHECK 123		/* Uppdate with is recoverable */
#define HA_CANT_DO_THAT 131		/* Databasehandler can't do it */

/* row outside file */
#define HA_POS_ERROR	(~ (ha_rows) 0)

	/* Some extern variabels used with handlers */

extern char NEAR bas_ext[][2][FN_EXTLEN];
extern uint NEAR ha_maxrecordlength[],NEAR ha_max_keys[],
	    NEAR ha_max_key_parts[],NEAR ha_option_flag[],
            NEAR ha_max_key_length[];

	/* Prototypes for handler functions */

extern int ha_panic(enum ha_panic_function flag);
extern int ha_open(TABLE *form,char *name,int mode,int test_if_locked);
extern int ha_close(TABLE *form);
extern int ha_write(TABLE *form,byte *buf);
extern int ha_update(TABLE *form,byte *old,byte *new_record);
extern int ha_delete(TABLE *form,byte *buf);
extern int ha_rkey(TABLE *form,byte *buf,int inx,byte *key,
		       uint key_len,enum ha_rkey_function flag);
extern int ha_rnext(TABLE *form,byte *buf,int inx);
extern int ha_rprev(TABLE *form,byte *buf,int inx);
extern int ha_rfirst(TABLE *form,byte *buf,int inx);
extern int ha_rlast(TABLE *form,byte *buf,int inx);
extern int ha_rsame(TABLE *form,byte *buf,int inx);
extern int ha_rsame_with_pos(TABLE *form,byte *buf,int inx,byte *pos);
extern int ha_r_rnd(TABLE *form,byte *buf,byte *pos);
extern int ha_readfirst(TABLE *form,byte *buf);
extern void ha_info(TABLE *form,int flag);
extern int ha_extra(TABLE *form,enum ha_extra_function function);
extern void ha_reset(TABLE *form);
extern int ha_lock(TABLE *form,int lock_type);
extern void ha_key_cache(void);
extern void ha_error(TABLE *form,int error,int errflag);
extern int ha_keyerror(TABLE *form,int error);
extern enum db_type ha_checktype(enum db_type database_type);
extern int ha_frename(enum db_type base,char *from,char *to);
extern int ha_frename (enum db_type base, const char * from, const char * to);
extern int ha_fdelete(enum db_type base,char *name);
