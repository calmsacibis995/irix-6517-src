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

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql_version.h"
#include <hash.h>
#include <signal.h>
#include <thr_lock.h>
#include <my_base.h>				/* Needed by field.h */

extern "C" {					/* Bug in libc 2.0 */
#ifndef __WIN32__
#ifdef MSDOS
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif
#endif /* MSDOS */
#endif /* __WIN32 */
}

typedef ulong table_map;		/* Used for table bits in join */
typedef ulong key_map;			/* Used for finding keys */

#include "mysql_com.h"
#include "unireg.h"

void init_sql_alloc(MEM_ROOT *root,uint block_size);
gptr sql_alloc(unsigned size);
gptr sql_calloc(unsigned size);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str,uint len);
gptr sql_memdup(const void * ptr,unsigned size);
void sql_element_free(void *ptr);

#define x_free(A)	{ my_free((gptr) (A),MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR)); }
#define safeFree(x)	{ if(x) { my_free((gptr) x,MYF(0)); x = NULL; } }
#define PREV_BITS(A)	((table_map) (((table_map) 1 << (A)) -1))
#define all_bits_set(A,B) ((A) & (B) != (B))

#ifndef LL
#ifdef HAVE_LONG_LONG
#define LL(A) A ## LL
#else
#define LL(A) A ## L
#endif
#endif

/***************************************************************************
  Configuration parameters
****************************************************************************/

#define MAX_BLOB_WIDTH	8192			/* Default width for blob */
#define CONNECT_TIMEOUT 5			// Do not wait long for connect
#define MAX_ACCEPT_RETRY 10			// Test accept this many times
#define MAX_CONNECT_ERRORS 10			// errors before disabling host
#define MAX_FIELDS_BEFORE_HASH 32
#define ACL_CACHE_SIZE 256
#define HOST_CACHE_SIZE 128

// The following is used to decide if MySQL should use table scanning
// instead of reading with keys.  The number says how many evaluation of the
// WHERE clause is comparable to reading one extra row from a table.
#define TIME_FOR_COMPARE 10	// 10 compares == one read

#ifdef	__WIN32__
#define INTERRUPT_PRIOR -2
#define CONNECT_PRIOR	-1
#define WAIT_PRIOR	0
#define QUERY_PRIOR	2
#else
#define INTERRUPT_PRIOR 10
#define CONNECT_PRIOR	9
#define WAIT_PRIOR	8
#define QUERY_PRIOR	6
#endif /* __WIN92__ */

struct st_table;
class THD;

#define TEST_PRINT_CACHED_TABLES 1
#define TEST_NO_KEY_GROUP	 2
#define TEST_MIT_THREAD		4
#define TEST_BLOCKING		8
#define TEST_KEEP_TMP_TABLES	16
#define TEST_NO_THREADS		32	/* For debugging under Linux */
#define TEST_READCHECK		64	/* Force use of readcheck */
#define TEST_NO_EXTRA		128
#define	TEST_KILL_ON_DEBUG	256	/* Kill server */

/* options for select set by the yacc parser */
#define SELECT_DISTINCT		1
#define SELECT_STRAIGHT_JOIN	2
#define SELECT_DESCRIBE		4
#define SELECT_SMALL_RESULT	8
#define SELECT_HIGH_PRIORITY	64		/* Intern */
#define SELECT_USE_CACHE	256		/* Intern */

#define OPTION_BIG_TABLES	512		/* for SQL OPTION */
#define OPTION_BIG_SELECTS	1024		/* for SQL OPTION */
#define OPTION_LOG_OFF		2048
#define OPTION_UPDATE_LOG	4096		/* update log flag */
#define OPTION_LOW_PRIORITY_UPDATES 8192
#define OPTION_WARNINGS		16384

/* Struct to handle simple linked lists */

typedef struct st_sql_list {
  uint elements;
  byte *first;
  byte **next;
} SQL_LIST;


uint nr_of_decimals(const char *str);		/* Neaded by sql_string.h */

extern pthread_key(THD*, THR_THD);
inline THD *_current_thd(void) __attribute__ ((const))
{
  return my_pthread_getspecific_ptr(THD*,THR_THD);
}
#define current_thd _current_thd()

#include "sql_string.h"
#include "sql_list.h"
#include "sql_map.h"
#include "table.h"
#include "field.h"				/* Field definitions */
#include "sql_udf.h"
#include "item.h"
#include "sql_class.h"
#include "handler.h"
#include "opt_range.h"

void mysql_create_db(THD *thd, char *db);
int mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists);
bool mysql_change_db(THD *thd,const char *name);
void mysql_parse(THD *thd,char *inBuf,uint length);
pthread_handler_decl(handle_one_connection,arg);
int handle_bootstrap(THD *thd);
sig_handler end_thread(int sig);
void mysql_execute_command(void);
bool do_command(THD *thd);
bool check_stack_overrun(THD *thd,char *dummy);
bool reload_acl_and_cache(uint options);
void mysql_rm_db(THD *thd,char *db,bool if_exists);
void table_cache_init(void);
void table_cache_free(void);
uint cached_tables(void);
void close_connection(NET *net,uint errcode=0,bool lock=1);
bool check_access(THD *thd,uint access,const char *db=0,uint *save_priv=0,
		  bool no_grant=0);

/* net_pkg.c */
void send_error(NET *net,uint sql_errno=0, char *err=0);
void net_printf(NET *net,uint sql_errno, ...);
void send_ok(NET *net,ha_rows affected_rows=0L,ulonglong id=0L,
	     const char *info=0);
void send_eof(NET *net,bool no_flush=0);
char *net_store_length(char *packet,ulonglong length);
char *net_store_data(char *to,const char *from);
char *net_store_data(char *to,long from);
bool net_store_null(String *packet);
bool net_store_data(String *packet,ulong from);
bool net_store_data(String *packet,const char *from);
bool net_store_data(String *packet,const char *from,uint length);

int mysql_select(THD *thd,TABLE_LIST *tables,List<Item> &list,COND *conds,
		 ORDER *order, ORDER *group,Item *having,ORDER *proc_param,
		 uint select_type,select_result *result);
int mysql_create_table(THD *thd,const char *db, const char *table_name,
		       List<create_field> &fields, List<Key> &keys,
		       bool tmp_table);
int mysql_alter_table(THD *thd, char *new_db, char *new_name,
		      TABLE_LIST *table_list,
		      List<create_field> &fields,
		      List<Key> &keys,List<Alter_drop> &drop_list,
		      List<Alter_column> &alter_list,
		      bool drop_primary,enum enum_duplicates handle_duplicates);
int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys);
int mysql_drop_index(THD *thd, TABLE_LIST *table_list,
		     List<Alter_drop> &drop_list);
int mysql_update(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &values,COND *conds,
		 bool low_priority);
int mysql_insert(THD *thd,TABLE_LIST *table,List<Item> &fields,
		 List<List_item> &values, enum_duplicates flag,
		 bool low_priority);
int mysql_delete(THD *thd,TABLE_LIST *table,COND *conds,ha_rows rows,
		 bool low_priority);
TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type update);
TABLE *open_table(THD *thd,const char *db,const char *table,const char *alias,
		  bool *refresh);
bool reopen_table(THD *thd,TABLE *table,bool locked=0);
bool reopen_tables(THD *thd,bool get_locks,bool in_refresh);
void close_old_data_files(THD *thd, TABLE *table, bool abort_locks);
bool close_data_tables(THD *thd,const char *db, const char *table_name);
bool wait_for_tables(THD *thd);
bool drop_locked_tables(THD *thd,const char *db, const char *table_name);
void abort_locked_tables(THD *thd,const char *db, const char *table_name);
Field *find_field_in_tables(THD *thd,Item_field *item,TABLE_LIST *tables);
Field *find_field_in_table(THD *thd,TABLE *table,const char *name,uint length,
			   bool check_grant);

/* sql_list.c */
int mysqld_show_dbs(THD *thd,const char *wild);
int mysqld_show_tables(THD *thd,const char *db,const char *wild);
int mysqld_show_fields(THD *thd,TABLE_LIST *table, const char *wild);
int mysqld_show_keys(THD *thd, TABLE_LIST *table);
void mysqld_list_fields(THD *thd,TABLE_LIST *table, const char *wild);
void mysqld_list_processes(THD *thd,const char *user);
int mysqld_show_status(THD *thd);
int mysqld_show_variables(THD *thd,const char *wild);
int mysqld_show(THD *thd, const char *wild, show_var_st *variables);

/* sql_base.cc */
void set_item_name(Item *item,char *pos,uint length);
bool add_field_to_list(char *field_name, enum enum_field_types type,
		       char *length, char *decimal,
		       uint type_modifier, Item *default_value,char *change,
		       TYPELIB *interval);
void store_position_for_column(my_string name);
bool add_to_list(SQL_LIST &list,Item *group,bool asc=0);
TABLE_LIST *add_table_to_list(Table_ident *table,LEX_STRING *alias,
			      thr_lock_type flags=TL_UNLOCK);
void add_left_join_on(TABLE_LIST *a,TABLE_LIST *b,Item *expr);
void add_left_join_natural(TABLE_LIST *a,TABLE_LIST *b);
bool add_proc_to_list(Item *item);
TABLE *unlink_open_table(THD *thd,TABLE *list,TABLE *find);

SQL_SELECT *make_select(TABLE **table,uint table_count,uint head,
			uint const_tables, COND *conds, int *error);
Item ** find_item_in_list(Item *item,List<Item> &items);
int setup_fields(THD *thd,TABLE_LIST *tables,List<Item> &item,
		 bool set_query_id,List<Item> *sum_func_list);
int setup_conds(THD *thd,TABLE_LIST *tables,COND *conds);
bool wait_for_refresh(THD *thd);
int open_tables(THD *thd,TABLE_LIST *tables);
int open_and_lock_tables(THD *thd,TABLE_LIST *tables);
int lock_tables(THD *thd,TABLE_LIST *tables);
TABLE *open_tmp_table(THD *thd,const char *db,const char *table_name);
bool send_fields(THD *thd,List<Item> &item,uint send_field_count);
void free_io_cache(TABLE *entry);
void intern_close_table(TABLE *entry);
void close_thread_tables(THD *thd);
void remove_db_from_cache(const my_string db);
bool remove_table_from_cache(const char *db,const char *table);
bool close_cached_tables(bool wait_for_refresh);
void copy_field_from_tmp_record(Field *field,int offset);
int fill_record(List<Item> &fields,List<Item> &values);
int fill_record(TABLE *table,List<Item> &values);

/* sql_calc.cc */
bool eval_const_cond(COND *cond);

/* sql_load.cc */
int mysql_load(THD *thd,sql_exchange *ex, TABLE_LIST *table_list,
	       List<Item> &fields, enum enum_duplicates handle_duplicates,
	       bool local_file);
int write_record(THD *thd,TABLE *table,COPY_INFO *info);
/* sql_test.cc */
#ifndef DBUG_OFF
void print_where(COND *cond,char *info);
void print_cached_tables(void);
void TEST_filesort(TABLE **form,SORT_FIELD *sortorder,uint s_length,
		   ha_rows special);
#endif
void mysql_print_status(void);
/* key.cc */
int find_ref_key(TABLE *form,Field *field);
void key_copy(byte *key,TABLE *form,uint index,uint key_length);
void key_restore(TABLE *form,byte *key,uint index,uint key_length);
int key_cmp(TABLE *form,byte *key,uint index,uint key_length);
void key_unpack(String *to,TABLE *form,uint index);
void init_errmessage(void);

void sql_perror(const my_string message);
extern "C" void sql_print_error(const my_string format,...);
extern "C" void hash_password(ulong *result, const char *password);

extern char mysql_data_home[2],server_version[40],max_sort_char;
extern my_string mysql_unix_port,mysql_tmpdir,first_keyword;
extern ulong refresh_version,query_id,opened_tables,created_tmp_tables,
	     aborted_threads,aborted_connects;
extern uint test_flags,thread_count,select_errors,mysql_port,
	    thd_startup_options;
extern time_t start_time;
extern char *command_name[];
extern I_List<THD> threads;
extern MYSQL_LOG mysql_log,mysql_update_log;
extern pthread_key(MEM_ROOT*,THR_MALLOC);
extern pthread_key(NET*, THR_NET);
extern pthread_mutex_t LOCK_mysql_create_db,LOCK_Acl,LOCK_open,
       LOCK_thread_count,LOCK_mapped_file,LOCK_user_locks, LOCK_status,
       LOCK_grant, LOCK_log;
extern pthread_cond_t COND_refresh;
extern bool opt_endinfo;
extern ulong ha_read_count, ha_write_count, ha_delete_count, ha_update_count,
             ha_read_key_count, ha_read_first_count, ha_read_next_count,
	     ha_read_rnd_count;

extern char f_fyllchar;
extern uchar *days_in_month;
extern DATE_FORMAT dayord;
extern double log_10[32];
extern uint current_pid,protocol_version;
extern ulong keybuff_size,sortbuff_size,max_item_sort_length,table_cache_size,
	     max_join_size,join_buff_size,tmp_table_size,thread_stack,
	     max_connections,max_connect_errors,long_query_time,
	     long_query_count,net_wait_timeout,what_to_log;
extern ulong specialflag;
extern bool volatile abort_loop, grant_option;
extern bool low_priority_updates;
extern char language[LIBLEN],reg_ext[FN_EXTLEN],blob_newline;
extern char **errmesg;				/* Error messages */
extern byte last_ref[MAX_REFLENGTH];		/* Index ref of keys */
extern String empty_string;
extern struct show_var_st init_vars[];
extern struct show_var_st status_vars[];
#ifndef __WIN32__
extern pthread_t signal_thread;
#endif

MYSQL_LOCK *mysql_lock_tables(THD *thd,TABLE **table,uint count);
void mysql_unlock_tables(MYSQL_LOCK *sql_lock);
void mysql_unlock_read_tables(MYSQL_LOCK *sql_lock);
void mysql_unlock_some_tables(TABLE **table,uint count);
void mysql_lock_remove(MYSQL_LOCK *locked,TABLE *table);
void mysql_lock_abort(TABLE *table);
MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b);

/* old unireg functions */

void unireg_init(ulong options);
void unireg_end(int signal);
int rea_create_table(my_string file_name,enum db_type database_type,
		     uint table_options, ha_rows records, ha_rows reloc,
		     List<create_field> &create_field,
		     uint key_count,KEY *key_info);
int format_number(uint inputflag,uint max_length,my_string pos,uint length,
		  my_string *errpos);
int cre_database(char *name);
int openfrm(const char *name,const char *alias,uint filestat,uint prgflag,
	    TABLE *outparam);
int closefrm(TABLE *table);
void free_blobs(TABLE *table);
int set_zone(int nr,int min_zone,int max_zone);
ulong convert_period_to_month(ulong period);
ulong convert_month_to_period(ulong month);
long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);
void get_date_from_daynr(long daynr,uint *year, uint *month,
			 uint *day);
void init_time(void);
long my_gmt_sec(TIME *);
time_t str_to_timestamp(const char *str,uint length);
ulong str_to_date(const char *str,uint length);
bool str_to_time(const char *str,uint length,TIME *l_time);
longlong str_to_datetime(const char *str,uint length);
timestamp_type str_to_TIME(const char *str, uint length, TIME *l_time);

int test_if_number(char *str,int *res,bool allow_wildcards);
void change_byte(byte *,uint,char,char);
void unireg_abort(int exit_code);
void init_read_record(READ_RECORD *info, TABLE *reg_form, SQL_SELECT *select,
		      bool use_record_cache=1);
void end_read_record(READ_RECORD *info);
ha_rows filesort(TABLE **form,struct st_sort_field *sortorder, uint s_length,
		 SQL_SELECT *select, ha_rows special,ha_rows max_records);
void change_double_for_sort(double nr,byte *to);
int get_quick_record(SQL_SELECT *select);
int calc_weekday(long daynr,bool sunday_first_day_of_week);
void find_date(char *pos,uint *vek,uint flag);
TYPELIB *convert_strings_to_array_type(my_string *typelibs, my_string *end);
TYPELIB *typelib(List<String> &strings);
void clean_up(void);
ulong get_form_pos(File file, uchar *head,
		   my_string outname, TYPELIB *save_names);
ulong make_new_entry(File file,uchar *fileinfo,TYPELIB *formnames,
		     char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res,const char *pos);
int create_frm(char *name,uint reclength,uchar *fileinfo,
	       enum db_type database,uint options,
	       ha_rows records,ha_rows reloc,uint keys);
int rename_file_ext(const char * from,const char * to,const char * ext);
int fix_frm_ref(const char * name);
char *get_field(MEM_ROOT *mem,TABLE *table,uint fieldnr);
int wild_case_compare(const char *str,const char *wildstr);
/* from hostname.cc */
struct in_addr;
my_string ip_to_hostname(struct in_addr *in,uint *errors);
void inc_host_errors(struct in_addr *in);
void reset_host_errors(struct in_addr *in);
bool hostname_cache_init();
void hostname_cache_free();
void hostname_cache_refresh(void);
bool get_interval_info(const char *str,uint length,uint count,
		       long *values);
/* Some inline functions for more speed */

inline bool add_item_to_list(Item *item)
{
  return current_thd->item_list.push_back(item);
}
inline bool add_value_to_list(Item *value)
{
  return current_thd->value_list.push_back(value);
}
inline bool add_order_to_list(Item *item,bool asc)
{
  return add_to_list(current_thd->order_list,item,asc);
}
inline bool add_group_to_list(Item *item,bool asc)
{
  return add_to_list(current_thd->group_list,item,asc);
}
