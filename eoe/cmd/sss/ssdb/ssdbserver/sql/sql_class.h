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

/* Classes in mysql */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_IGNORE };
enum timestamp_type { TIMESTAMP_NONE, TIMESTAMP_DATE, TIMESTAMP_FULL };

class MYSQL_LOG {
 public:
  enum type { CLOSED, NORMAL, NEW };
  FILE *file;
  time_t last_time,query_start;
  ulonglong last_insert_id;
  type log_type;
  char *name;
  char time_buff[20],db[NAME_LEN+1];

public:
  MYSQL_LOG() :file(0),last_time(0),query_start(0),last_insert_id(0),name(0),
    log_type(CLOSED) {}
  void open(const my_string log_name,type log_type);
  void new_file();
  void write(enum enum_server_command command,const my_string format,...);
  void write(const char *query);
  void flush();
  void close();
};

/* character conversion tables */

class CONVERT
{
  const uchar *from_map,*to_map;
  void convert_array(const uchar *mapping,uchar *buff,uint length);
public:
  const my_string name;
  CONVERT(my_string name_par,uchar *from_par,uchar *to_par)
    :name(name_par),from_map(from_par),to_map(to_par) {}
  friend CONVERT *get_convert_set(const char *name_ptr);
  inline void convert(char *a,uint length)
  {
    convert_array(from_map, (uchar*) a,length);
  }
  bool store(String *, const char *,uint);
};

typedef struct st_copy_info {
  ha_rows records;
  ha_rows deleted;
  ha_rows copied;
  enum enum_duplicates handle_duplicates;
  int escape_char;
} COPY_INFO;


class key_part_spec :public Sql_alloc {
public:
  char *field_name;
  uint length;
  key_part_spec(char *name,uint len=0) :field_name(name), length(len) {}
};


class Alter_drop :public Sql_alloc {
public:
  enum drop_type {KEY, COLUMN };
  char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,char *par_name)
    :name(par_name), type(par_type) {}
};


class Alter_column :public Sql_alloc {
public:
  char *name;
  Item *def;
  Alter_column(char *par_name,Item *literal)
    :name(par_name), def(literal) {}
};


class Key :public Sql_alloc {
public:
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE };
  enum Keytype type;
  List<key_part_spec> columns;
  char *Name;

  Key(enum Keytype type_par,char *name,List<key_part_spec> &cols)
    :type(type_par), columns(cols),Name(name) {}
  ~Key() {}
  char *name() { return Name; }
};


typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;


class LEX_COLUMN : public Sql_alloc
{
public:
  String column;
  uint rights;
  LEX_COLUMN (const String& x,const  uint& y ): column (x),rights (y) {}
};

#include "sql_lex.h"				/* Must be here */

/****************************************************************************
** every connection is handle by a thread with a THD
****************************************************************************/

extern uint thd_startup_options;

class THD :public ilink {
  ulonglong  last_insert_id;
public:
  NET	  net;
  String  packet;				/* Room for 1 row */
  LEX	  lex;
  char	  *host,*user,*priv_user,*db,*ip,*proc_info;
  struct  sockaddr_in local,remote;
  uint	  client_capabilities,max_packet_length;
  uint	  master_access,db_access;
  TABLE   *open_tables;
  MYSQL_LOCK *lock,*locked_tables;
  MEM_ROOT alloc;
  ULL	  *ull;
  struct st_my_thread_var *mysys_var;
  enum enum_server_command command;
  char	  *query,*where,*thread_stack;
  char	  scramble[9];
  time_t  start_time;

  uint	current_tablenr,tmp_table,cond_count,options,
	col_access;
  ulong query_id,version;
  ha_rows select_limit,offset_limit,default_select_limit,cuted_fields;
  List<Item> item_list,field_list,value_list;
  create_field *last_field;
  Item	     *free_list;
  SQL_LIST   order_list,table_list,group_list,proc_list;
  CONVERT    *convert_set;
  Field      *dupp_field;
  bool	     set_query_id,locked,count_cuted_fields,some_tables_deleted;
  bool	     no_errors, allow_sum_func, password, fatal_error;
  bool	     query_start_used,last_insert_id_used,insert_id_used,user_time;
  bool	     volatile killed,bootstrap;
  struct rand_struct rand;
  pthread_t  real_id;
  ulong thread_id;
  long dbug_thread_id;
  ulonglong next_insert_id;
#ifndef __WIN32__
  sigset_t signals,block_signals;
#else
  HANDLE handle_thread ;
#endif

  THD();
  ~THD();
  inline time_t query_start() { query_start_used=1; return start_time; }
  inline void	set_time()    { if (!user_time) time(&start_time); }
  inline void	set_time(time_t t) { start_time=t; user_time=1; }
  inline void	insert_id(ulonglong id)
  { last_insert_id=id; insert_id_used=1; }
  inline ulonglong insert_id(void)
  {
    last_insert_id_used=1; return last_insert_id;
  }

};


class sql_exchange :public Sql_alloc
{
public:
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  uint skip_lines;
  sql_exchange(char *name);
  ~sql_exchange() {}
};

/*
** This is used to get result from a select
*/

class select_result :public Sql_alloc {
protected:
  THD *thd;
public:
  select_result();
  virtual ~select_result() {};
  virtual int prepare(List<Item> &list __attribute__((unused))) { return 0; }
  virtual bool send_fields(List<Item> &list,uint flag)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual void send_error(uint errcode,char *err)=0;
  virtual void send_eof()=0;
};


class select_send :public select_result {
public:
  select_send() {}
  bool send_fields(List<Item> &list,uint flag);
  bool send_data(List<Item> &items);
  void send_error(uint errcode,char *err);
  void send_eof();
};


class select_export :public select_result {
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  uint field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  bool fixed_row_size;
public:
  select_export(sql_exchange *ex) :exchange(ex),file(-1),row_count(0L) {}
  ~select_export();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list __attribute__((unused)),
		   uint flag __attribute__((unused))) { return 0; }
  bool send_data(List<Item> &items);
  void send_error(uint errcode,char *err);
  void send_eof();
};


class select_insert :public select_result {
  TABLE *table;
  List<Item> *fields;
  uint save_time_stamp;
  COPY_INFO info;
public:
  select_insert(TABLE *table_par,List<Item> *fields_par,enum_duplicates dup)
    :table(table_par),fields(fields_par),save_time_stamp(table_par->time_stamp)
    {
      bzero((char*) &info,sizeof(info)); info.handle_duplicates=dup;
    }
  ~select_insert();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list,uint flag) { return 0; }
  bool send_data(List<Item> &items);
  void send_error(uint errcode,char *err);
  void send_eof();
};


/* Structs used when sorting */

typedef struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  uint	 length;			/* Length of sort field */
  my_bool reverse;			/* if descending sort */
  Item_result result_type;		/* Type of item */
} SORT_FIELD;


typedef struct st_sort_buffer {
  uint index;					/* 0 or 1 */
  uint sort_orders;
  uint change_pos;				/* If sort-fields changed */
  char **buff;
  SORT_FIELD *sortorder;
} SORT_BUFFER;


/* Structure for db & table in sql_yacc */

class Table_ident :public Sql_alloc {
 public:
  LEX_STRING db;
  LEX_STRING table;
  inline Table_ident(LEX_STRING db_arg,LEX_STRING table_arg,bool force)
    :table(table_arg)
  {
    if (!force && (current_thd->client_capabilities & CLIENT_NO_SCHEMA))
      db.str=0;
    else
      db= db_arg;
  }
  inline Table_ident(LEX_STRING table_arg) :table(table_arg) {db.str=0;}
  inline void change_db(char *db_name)
  { db.str= db_name; db.length=strlen(db_name); }
};
