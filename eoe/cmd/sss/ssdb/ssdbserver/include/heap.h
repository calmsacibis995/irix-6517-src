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

/* This file should be included when using heap_database_funktions */
/* Author: Michael Widenius */

#ifndef _heap_h
#define _heap_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif

	/* defines used by heap-funktions */

#define HP_MAX_LEVELS	4		/* 128^5 records is enough */
#define HP_PTRS_IN_NOD	128

	/* struct used with heap_funktions */

typedef struct st_heapinfo		/* Struct from heap_info */
{
  ulong records;			/* Records in database */
  ulong deleted;			/* Deleted records in database */
  ulong current_record;			/* Pos for last used record */
  uint reclength;			/* Length of one record */
  int errkey;
} HEAPINFO;


	/* Structs used by heap-database-handler */

typedef struct st_heap_ptrs
{
  byte *blocks[HP_PTRS_IN_NOD];		/* pointers to HP_PTRS or records */
} HP_PTRS;

struct st_level_info
{
  uint free_ptrs_in_block,records_under_level;
  HP_PTRS *last_blocks;			/* pointers to HP_PTRS or records */
};

typedef struct st_heap_block		/* The data is saved in blocks */
{
  HP_PTRS *root;
  struct st_level_info level_info[HP_MAX_LEVELS+1];
  uint levels;
  uint records_in_block;		/* Records in a heap-block */
  uint recbuffer;			/* Length of one saved record */
  ulong last_allocated;			/* Blocks allocated, used by keys */
} HP_BLOCK;

typedef struct st_hp_keyseg		/* Key-portion */
{
  uint start;				/* Start of key in record (from 0) */
  uint length;				/* Keylength */
  uint type;
} HP_KEYSEG;

typedef struct st_hp_keydef		/* Key definition with open */
{
  uint flag;				/* NOSAME */
  uint keysegs;				/* Number of key-segment */
  uint length;				/* Length of key (automatic) */
  HP_KEYSEG *seg;
  HP_BLOCK block;			/* Where keys are saved */
} HP_KEYDEF;

typedef struct st_heap_info
{
  HP_BLOCK block;
  HP_KEYDEF  *keydef;
  ulong min_records,max_records;	/* Params to open */
  ulong current_record;
  uint records;				/* records */
  uint blength;
  uint deleted;				/* Deleted records in database */
  uint reclength;			/* Length of one record */
  uint update,opt_flag,changed;
  uint keys;
  int  mode;				/* Mode of file (READONLY..) */
  int lastinx,errkey;
  byte *current_ptr,
       *del_link,			/* Link to next block with del. rec */
       *lastkey;			/* Last used key with rkey */
  LIST open_list;
  my_string name;			/* Name of "memory-file" */
} HP_INFO;

	/* Prototypes for heap-functions */

extern HP_INFO* heap_open(const char *name,int mode,uint keys,
			  HP_KEYDEF *keydef,uint reclength,
			  ulong max_records,ulong min_reloc);
extern int heap_close(HP_INFO *info);
extern int heap_write(HP_INFO *info,const byte *buff);
extern int heap_update(HP_INFO *info,const byte *old,const byte *newdata);
extern int heap_rrnd(HP_INFO *info,byte *buf,ulong pos);
extern int heap_delete(HP_INFO *info,const byte *buff);
extern int heap_info(HP_INFO *info,HEAPINFO *x,int flag);
extern int heap_create(const char *name);
extern int heap_extra(HP_INFO *info,enum ha_extra_function function);
extern int heap_rename(const char *old_name,const char *new_name);
extern int heap_panic(enum ha_panic_function flag);
extern int heap_rsame(HP_INFO *info,byte *record,int inx);
extern int heap_rfirst(HP_INFO *info,byte *record,int inx);
extern int heap_rnext(HP_INFO *info,byte *record,int inx);
extern int heap_rlast(HP_INFO *info,byte *record,int inx);
extern int heap_rprev(HP_INFO *info,byte *record,int inx);
extern void heap_clear(HP_INFO *info);
extern int heap_rkey(HP_INFO *info,byte *record,int inx,const byte *key);
extern gptr heap_find(HP_INFO *info,int inx,const byte *key);
extern int heap_check_heap(HP_INFO *info);

#define heap_delete_all(a) heap_create(a)

#ifdef	__cplusplus
}
#endif
#endif
