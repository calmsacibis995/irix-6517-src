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

/* This file should be included when using nisam_funktions */
/* Author: Michael Widenius */

#ifndef _nisam_h
#define _nisam_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif
	/* defines used by nisam-funktions */

#define N_MAXKEY	16		/* Max allowed keys */
#define N_MAXKEY_SEG	16		/* Max segments for key */
#define N_MAX_KEY_LENGTH 256		/* May be increased up to 500 */
#define N_MAX_KEY_BUFF	 (N_MAX_KEY_LENGTH+N_MAXKEY_SEG+sizeof(double)-1)
#define N_MAX_POSSIBLE_KEY_BUFF 500+9

#define N_NAME_IEXT	".ISM"
#define N_NAME_DEXT	".ISD"
#define NI_POS_ERROR	(~ (ulong) 0)


	/* Param to/from ni_info */

typedef struct st_n_isaminfo		/* Struct from h_info */
{
  ulong records;			/* Records in database */
  ulong deleted;			/* Deleted records in database */
  ulong recpos;			/* Pos for last used record */
  ulong newrecpos;			/* Pos if we write new record */
  ulong data_file_length;		/* Length of data file */
  uint	reclength;			/* Recordlength */
  uint	mean_reclength;			/* Mean recordlength (if packed) */
  uint	keys;				/* How many keys used */
  uint	options;			/* HA_OPTIONS_... used */
  int	errkey,				/* With key was dupplicated on err */
	sortkey;			/* clustered by this key */
  File	filenr;				/* (uniq) filenr for datafile */
  time_t create_time;			/* When table was created */
  time_t isamchk_time;
  time_t update_time;
  ulong *rec_per_key;			/* for sql optimizing */
} N_ISAMINFO;


	/* Info saved on file for each info-part */

#ifdef __WATCOMC__
#pragma pack(2)
#define uint uint16			/* Same format as in MSDOS */
#endif

#ifdef __ZTC__
#pragma ZTC align 2
#define uint uint16			/* Same format as in MSDOS */
#endif

typedef struct st_n_save_keyseg		/* Key-portion */
{
  uint8  type;				/* Typ av nyckel (f|r sort) */
  uint8  flag;				/* HA_DIFF_LENGTH */
  uint16 start;				/* Start of key in record */
  uint16 length;			/* Keylength */
} N_SAVE_KEYSEG;

typedef struct st_n_save_keydef /* Key definition with create & info */
{
  uint8 flag;				/* NOSAME, PACK_USED */
  uint8 keysegs;			/* Number of key-segment */
  uint16 block_length;			/* Length of keyblock (auto) */
  uint16 keylength;			/* Tot length of keyparts (auto) */
  uint16 minlength;			/* min length of (packed) key (auto) */
  uint16 maxlength;			/* max length of (packed) key (auto) */
} N_SAVE_KEYDEF;

typedef struct st_n_save_recinfo	/* Info of record */
{
  int16  type;				/* en_fieldtype */
  uint16 length;			/* length of field */
} N_SAVE_RECINFO;


#ifdef __ZTC__
#pragma ZTC align
#undef uint
#endif

#ifdef __WATCOMC__
#pragma pack()
#undef uint
#endif


struct st_isam_info;			/* For referense */

#ifndef ISAM_LIBRARY
typedef struct st_isam_info N_INFO;
#endif

typedef struct st_n_keyseg		/* Key-portion */
{
  N_SAVE_KEYSEG base;
} N_KEYSEG;


typedef struct st_n_keydef		/* Key definition with open & info */
{
  N_SAVE_KEYDEF base;
  N_KEYSEG seg[N_MAXKEY_SEG+1];
  int (*bin_search)(struct st_isam_info *info,struct st_n_keydef *keyinfo,
		    uchar *page,uchar *key,
		    uint key_len,uint comp_flag,uchar * *ret_pos,
		    uchar *buff);
  uint (*get_key)(struct st_n_keydef *keyinfo,uint nod_flag,uchar * *page,
		  uchar *key);
} N_KEYDEF;


typedef struct st_decode_tree		/* Decode huff-table */
{
  uint16 *table;
  uint	 quick_table_bits;
  byte	 *intervalls;
} DECODE_TREE;


struct st_bit_buff;

typedef struct st_n_recinfo		/* Info of record */
{
  N_SAVE_RECINFO base;
#ifndef NOT_PACKED_DATABASES
  void (*unpack)(struct st_n_recinfo *rec,struct st_bit_buff *buff,
		 uchar *start,uchar *end);
  enum en_fieldtype base_type;
  uint space_length_bits,pack_type;
  DECODE_TREE *huff_tree;
#endif
} N_RECINFO;


extern my_string nisam_log_filename;		/* Name of logfile */
extern uint nisam_block_size;
extern my_bool nisam_flush;

	/* Prototypes for nisam-functions */

extern int ni_close(struct st_isam_info *file);
extern int ni_delete(struct st_isam_info *file,const char *buff);
extern struct st_isam_info *ni_open(const char *name,int mode,
				    int wait_if_locked);
extern int ni_panic(enum ha_panic_function function);
extern int ni_rfirst(struct st_isam_info *file,byte *buf,int inx);
extern int ni_rkey(struct st_isam_info *file,byte *buf,int inx,
		   const byte *key,
		   uint key_len, enum ha_rkey_function search_flag);
extern int ni_rlast(struct st_isam_info *file,byte *buf,int inx);
extern int ni_rnext(struct st_isam_info *file,byte *buf,int inx);
extern int ni_rprev(struct st_isam_info *file,byte *buf,int inx);
extern int ni_rrnd(struct st_isam_info *file,byte *buf,ulong pos);
extern int ni_rsame(struct st_isam_info *file,byte *record,int inx);
extern int ni_rsame_with_pos(struct st_isam_info *file,byte *record,
			     int inx,ulong pos);
extern int ni_update(struct st_isam_info *file,const byte *old,
		     const byte *new_record);
extern int ni_write(struct st_isam_info *file,const byte *buff);
extern int ni_info(struct st_isam_info *file,N_ISAMINFO *x,int flag);
extern int ni_lock_database(struct st_isam_info *file,int lock_type);
extern int ni_create(const char *name,uint keys,N_KEYDEF *keyinfo,
		     N_RECINFO *recinfo,ulong records,
		     ulong reloc,uint flags,uint options,
		     ulong data_file_length);
extern int ni_extra(struct st_isam_info *file,
		    enum ha_extra_function function);
extern ulong ni_records_in_range(struct st_isam_info *info,int inx,
				 const byte *start_key,uint start_key_len,
				 enum ha_rkey_function start_search_flag,
				 const byte *end_key,uint end_key_len,
				 enum ha_rkey_function end_search_flag);
extern int ni_log(int activate_log);
extern int ni_is_changed(struct st_isam_info *info);
extern uint _calc_blob_length(uint length , const byte *pos);

#ifdef	__cplusplus
}
#endif
#endif
