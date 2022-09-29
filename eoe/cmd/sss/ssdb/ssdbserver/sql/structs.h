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

/* The old structures from unireg */

struct st_table;
class Field;

typedef struct st_date_format {		/* How to print date */
  uint pos[6];				/* Positions to YY.MM.DD HH:MM:SS */
} DATE_FORMAT;


union un_ref {
  uint32 lastpos;
  byte refpos[MAX_REFLENGTH];		/* When long is not enough */
};

typedef struct st_keyfile_info {	/* used with ha_info() */
  union un_ref ref;			/* Position for last used record */
  uint ref_length;			/* Length of ref (4 or 6) */
  uint block_size;			/* index block size */
  File filenr;				/* (uniq) filenr for table */
  ha_rows records;			/* Records i datafilen */
  ha_rows deleted;			/* Deleted records */
  ulonglong data_file_length;		/* Length off data file */
  int errkey,sortkey;			/* Last errorkey and sorted by */
  long	create_time;			/* When table was created */
  long	check_time;
  long	update_time;
  ulong mean_rec_length;		/* physical reclength */
  ulong *rec_per_key;
} KEYFILE_INFO;


typedef struct st_key_part_info {	/* Info about a key part */
  Field *field;
  uint	length;				/* Length of key_part */
  uint16 offset;			/* offset in record (from 0) */
  uint16 key_type;
  uint16 fieldnr;			/* F{ltnummer i standarformul{r */
  uint8  key_part_flag;			/* 0 or HA_REVERSE_SORT */
} KEY_PART_INFO ;


typedef struct st_key {
  uint	key_length;			/* Tot length of key */
  uint	dupp_key;			/* != 0 om dupp. f}r f|rekomma */
  uint	key_parts;			/* How many key_parts */
  uint	usable_key_parts;		/* Should normally be = key_parts */
  KEY_PART_INFO *key_part;
  char	*name;				/* Name of key */
} KEY;


enum reginfo_type { RI_NORMAL,RI_SYSTEM,RI_REPEAT,RI_REF,RI_CONST,RI_PROG };

typedef struct st_ref_field {		/* Used as indexfield to key */
  char *ptr;
  uint length;
  Field *field;
} REF_FIELD;


typedef struct st_reginfo {		/* Extra info about reg */
  REF_FIELD ref_field[MAX_REF_PARTS];	/* Fields used as key */
  uint	ref_fields;
  table_map reg_used;			/* a bit set for all ref_regs */
  int	ref_key,user_key;		/* Keynr to use */
  uint	ref_length,key_length,user_keylength; /* Length of key used */
  byte	*key_buff;			/* Last used key */
  enum thr_lock_type lock_type;		/* How database is used */
} REGINFO;


struct st_read_record;				/* For referense later */
class SQL_SELECT;

typedef struct st_read_record {			/* Parameter to read_record */
  struct st_table *form;			/* Head-form */
  struct st_table **forms;			/* head and ref forms */
  int (*read_record)(struct st_read_record *);
  SQL_SELECT *select;
  uint cache_records;
  uint ref_length,struct_length,reclength,rec_cache_size,error_offset;
  uint index;
  byte *ref_pos;				/* pointer to form->refpos */
  byte *record;
  byte	*cache,*cache_pos,*cache_end,*read_positions;
  IO_CACHE *io_cache;
} READ_RECORD;

typedef struct st_time {
  uint year,month,day,hour,minute,second;
  bool neg;
} TIME;

typedef struct {
  long year,month,day,hour,minute,second;
  bool neg;
} INTERVAL;


enum SHOW_TYPE { SHOW_LONG,SHOW_CHAR,SHOW_INT,SHOW_CHAR_PTR,SHOW_BOOL,
		 SHOW_MY_BOOL,SHOW_OPENTABLES,SHOW_STARTTIME,SHOW_QUESTION,
		 SHOW_LONG_CONST};
struct show_var_st {
  char *name,*value;
  SHOW_TYPE type;
};

typedef struct lex_string {
  char *str;
  uint length;
} LEX_STRING;

typedef struct	st_lex_user {
  LEX_STRING user, host, password;
} LEX_USER;

	/* Bits in form->update */
#define REG_MAKE_DUPP		1	/* Make a copy of record when read */
#define REG_NEW_RECORD		2	/* Write a new record if not found */
#define REG_UPDATE		4	/* Uppdate record */
#define REG_DELETE		8	/* Delete found record */
#define REG_PROG		16	/* User is updateing database */
#define REG_CLEAR_AFTER_WRITE	32
#define REG_MAY_BE_UPDATED	64
#define REG_AUTO_UPDATE		64	/* Used in D-forms for scroll-tables */
#define REG_OVERWRITE		128
#define REG_SKIPP_DUPP		256

	/* Bits in form->status */
#define STATUS_NO_RECORD	(1+2)	/* Record isn't usably */
#define STATUS_GARBAGE		1
#define STATUS_NOT_FOUND	2	/* No record in database when neaded */
#define STATUS_NO_PARENT	4	/* Parent record wasn't found */
#define STATUS_NOT_READ		8	/* Record isn't read */
#define STATUS_UPDATED		16	/* Record is updated by formula */
