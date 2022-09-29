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

#ifndef _opt_range_h
#define _opt_range_h

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

typedef struct st_key_part {
  uint16 key,part,length;
  Field *field;
} KEY_PART;

class QUICK_RANGE :public Sql_alloc {
 public:
  char *min_key,*max_key;
  uint16 min_length,max_length,flag;
  QUICK_RANGE();				/* Full range */
  QUICK_RANGE(const char *min_key_arg,uint min_length_arg,
	      const char *max_key_arg,uint max_length_arg,
	      uint flag_arg)
    : min_key(sql_memdup(min_key_arg,min_length_arg+1)),
      max_key(sql_memdup(max_key_arg,max_length_arg+1)),
      min_length(min_length_arg),
      max_length(max_length_arg),
      flag(flag_arg)
    {}
};

class QUICK_SELECT {
public:
  bool next;
  TABLE *head;
  uint index;
  List<QUICK_RANGE> ranges;
  List_iterator<QUICK_RANGE> it;
  MEM_ROOT alloc;
  KEY_PART *key_parts;
  ha_rows records,read_time;

  QUICK_SELECT(TABLE *table,uint index_arg,bool no_alloc=0);
  ~QUICK_SELECT();
  void reset(void) { next=0; it.rewind(); }
  int get_next();
  int cmp_next(QUICK_RANGE *range);
};


class SQL_SELECT :public Sql_alloc {
 public:
  QUICK_SELECT *quick;		// If quick-select used
  COND		*cond;		// where condition
  TABLE **tables,*head;
  IO_CACHE file;		// Positions to used records
  ha_rows records;		// Records in use if read from file
  ha_rows read_time;		// Time to read rows
  key_map quick_keys;		// Possible quick keys
  table_map needed_reg;		// Possible quick keys after prev tables.
  table_map const_tables,read_tables;
  bool	free_cond;

  SQL_SELECT();
  ~SQL_SELECT();
  bool check_quick()	{ return test_quick_select(~0L,0,HA_POS_ERROR) < 0; }
  bool skipp_record()	{ return cond ? cond->val_int() == 0 : 0; }
  int test_quick_select(key_map keys,table_map prev_tables,ha_rows limit);
};

#endif
