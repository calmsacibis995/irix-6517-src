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

/* Structs that defines the TABLE */

class Item;				/* Needed by ORDER */
class GRANT_TABLE;

enum db_type { DB_TYPE_DIAB_ISAM=1,DB_TYPE_HASH,DB_TYPE_MISAM,DB_TYPE_PISAM,
	       DB_TYPE_RMS_ISAM, DB_TYPE_HEAP, DB_TYPE_ISAM,
	       DB_TYPE_MRG_ISAM, DB_TYPE_MYISAM};

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */

  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
} ORDER;

typedef struct st_grant_info
{
  GRANT_TABLE *grant_table;
  uint version;
  uint privilege;
  uint want_privilege;
} GRANT_INFO;

/* Table cache entry struct */

typedef struct st_table {
  byte* file;				/* datbase file-descriptor */
  File dfile;				/* Key-file-descriptor */
  Field **field;			/* Pointer to fields */
  HASH  name_hash;			/* hash of field names */
  byte *record[3];			/* Pointer to records */
  uint fields;				/* field count */
  uint reclength;			/* Recordlength */
  uint rec_buff_length;
  uint keys,key_parts,max_key_length;
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  KEY  *key_info;			/* data of keys in database */
  TYPELIB keynames;			/* Pointers to keynames */
  KEYFILE_INFO keyfile_info;
  ha_rows max_records;			/* create information */
  ha_rows reloc;			/* create information */
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
  enum db_type db_type;			/* type of database */
  uint db_capabilities;
  uint db_create_options;		/* Create options from database */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint db_stat;				/* mode of file as in handler.h */
  uint status;				/* Used by postfix.. */
  uint system;				/* Set if system record */
  uint time_stamp;			/* Set to offset+1 of record */
  uint next_number_index;
  int locked;				/* Type of lock on database */
  my_bool copy_blobs;			/* copy_blobs when storing */
  my_bool null_row;			/* All columns are null */
  my_bool maybe_null,outer_join;	/* Used with OUTER JOIN */
  my_bool distinct,tmp_table,const_table;
  my_bool key_read;
  Field *next_number_field,		/* Set if next_number is activated */
	*found_next_number_field,	/* Set on open */
	*timestamp_field;
  my_string info;			/* Comment about table */
  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;

  char		*table_cache_key;
  char		*table_name,*real_name;
  uint		key_length;		/* Length of key */
  uint		tablenr,used_fields;
  table_map	map;
  ulong		version;
  uchar		*null_flags;
  IO_CACHE	*io_cache;			/* If sorted trough file*/
  byte		*record_pointers;		/* If sorted in memory */
  ha_rows	found_records;			/* How many records in sort */
  ORDER		*group;
  key_map	quick_keys,used_keys;
  ha_rows	quick_rows[MAX_KEY];

  THD		*in_use;			/* Which thread uses this */
  struct st_table *next,*prev;
} TABLE;


typedef struct st_table_list {
  struct	st_table_list *next;
  char		*db,*name,*real_name;
  thr_lock_type	lock_type;
  bool		straight;			/* optimize with prev table */
  TABLE		*table;
  Item		*on_expr;			/* Used with outer join */
  struct st_table_list *natural_join;		/* natural join on this table*/
  GRANT_INFO	grant;
} TABLE_LIST;
