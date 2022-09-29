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

/* classes to use when handling where clause */

#include "procedure.h"

typedef struct keyuse_t {
  TABLE *table;
  Field *field;				/* Field to compare keypart */
  Item		*val;				/* or value if no field */
  uint		key,keypart;
} KEYUSE;


/*
** CACHE_FIELD and JOIN_CACHE is used on full join to cache records in outer
** table
*/


typedef struct st_cache_field {
  char *str;
  uint length,blob_length;
  Field_blob *blob_field;
  bool strip;
} CACHE_FIELD;


typedef struct st_join_cache {
  uchar *buff,*pos,*end;
  uint records,record_nr,ptr_record,fields,length,blobs;
  CACHE_FIELD *field,**blob_ptr;
  SQL_SELECT *select;
} JOIN_CACHE;


/*
** The structs which holds the join connections and join states
*/

enum join_type { JT_UNKNOWN,JT_SYSTEM,JT_CONST,JT_EQ_REF,JT_REF,JT_MAYBE_REF,
		 JT_ALL, JT_RANGE, JT_NEXT};

struct st_join;

typedef struct st_join_table {
  TABLE 	*table;
  int		(*read_first_record)(struct st_join_table *tab);
  int		(*next_select)(struct st_join *,struct st_join_table *,bool);
  bool		cached_eq_ref_table,eq_ref_table;
  READ_RECORD	read_record;
  uint		keys;				/* all keys with can be used */
  key_map	const_keys;			/* Keys with constant part */
  ha_rows	records,found_records,read_time;
  table_map	needed_reg,dependent,key_dependent;
  uint		use_quick,index;
  uint		status;				// Save status for cache
  enum join_type type;
  JOIN_CACHE	cache;
  KEYUSE	*keyuse;			/* pointer to first used key */
  SQL_SELECT	*select;
  QUICK_SELECT	*quick;
  Item		*on_expr;
  uint		used_fields,used_fieldlength,used_blobs;
  char		*info;
} JOIN_TAB;


typedef struct st_position {			/* Used in find_best */
  JOIN_TAB *table;
  KEYUSE *key;
  double records_read;
} POSITION;


typedef struct st_join {
  JOIN_TAB *join_tab,**best_ref,**map2table;
  TABLE    **table,*sort_by_table;
  uint	   tables,const_tables;
  uint	   copy_field_count,field_count,sum_func_count,func_count;
  uint	   send_group_parts,group_parts,group_length;
  table_map const_bits;
  ha_rows  send_records,end_write_records;
  bool	   sort_and_group,first_record,quick_group,full_join,group;
  POSITION positions[MAX_TABLES+1],best_positions[MAX_TABLES+1];
  table_map index[MAX_TABLES+1];
  double   best_read;
  List<Item> *fields;
  List<Item> copy_funcs;
  List<Item_buff> group_fields;
  TABLE    *tmp_table;
  THD	   *thd;
  Copy_field *copy_field;
  Item_result_field **funcs;
  Item_sum  **sum_funcs;
  byte	    *group_buff;
  Procedure *procedure;
  Item	    *having;
  uint	    select_options;
  select_result *result;
  MYSQL_LOCK *lock;
} JOIN;


typedef struct st_select_check {
  uint const_ref,reg_ref;
} SELECT_CHECK;

extern my_string join_type_str[];
void TEST_join(JOIN *join);

/* Extern functions in sql_select.cc */
bool cmp_buffer_with_ref(byte *buffer,REF_FIELD *fields,uint length);
bool store_val_in_field(Field *field,Item *val);

/* functions from opt_sum.cc */
int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds);
