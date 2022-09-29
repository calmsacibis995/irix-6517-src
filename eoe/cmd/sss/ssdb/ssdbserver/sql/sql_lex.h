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

/* YACC and LEX Definitions */

/* These may not be declared yet */
class Table_ident;
class sql_exchange;
class LEX_COLUMN;

// The following hack is neaded because mysql_yacc.cc does not define
// YYSTYPE before including this file

#ifdef MYSQL_YACC
#define LEX_YYSTYPE void *
#else
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#endif

enum enum_sql_command {
  SQLCOM_SELECT,SQLCOM_CREATE_TABLE,SQLCOM_CREATE_INDEX,SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE,SQLCOM_INSERT,SQLCOM_INSERT_SELECT,SQLCOM_DELETE,
  SQLCOM_DROP_TABLE,SQLCOM_DROP_INDEX,SQLCOM_SHOW_DATABASES,
  SQLCOM_SHOW_TABLES,SQLCOM_SHOW_FIELDS,SQLCOM_SHOW_KEYS,
  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT, SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB,
  SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT, SQLCOM_SHOW_VARIABLES,
  SQLCOM_SHOW_STATUS, SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_SHOW_PROCESSLIST,SQLCOM_REVOKE,SQLCOM_OPTIMIZE,
  SQLCOM_FLUSH, SQLCOM_KILL
};

enum lex_states { STATE_START, STATE_CHAR, STATE_IDENT,
		  STATE_IDENT_SEP,
		  STATE_IDENT_START,
		  STATE_FOUND_IDENT,
		  STATE_SIGNED_NUMBER,
		  STATE_REAL,
		  STATE_CMP_OP,
		  STATE_STRING,
		  STATE_COMMENT,
		  STATE_END,
		  STATE_OPERATOR_OR_IDENT,
		  STATE_NUMBER_IDENT,
		  STATE_INT_OR_REAL,
		  STATE_REAL_OR_POINT,
		  STATE_BOOL,
		  STATE_EOL,
		  STATE_ESCAPE,
		  STATE_LONG_COMMENT,
		  STATE_END_LONG_COMMENT,
		  STATE_COLON,
		  STATE_USER_END,
		  STATE_HOSTNAME
};

typedef List<Item> List_item;

/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex {
  uint	 yylineno,yytoklen;			/* Simulate lex */
  LEX_YYSTYPE yylval;
  uchar *ptr,*tok_start,*tok_end,*end_of_query;
  ha_rows select_limit,offset_limit;
  bool	 create_refs,drop_primary,drop_if_exists,low_priority,local_file,
	 in_comment,ignore_space;
  enum_sql_command sql_command;
  enum lex_states next_state;
  uint options,in_sum_expr,grant,grant_tot_col,which_columns,global_grant;
  char *length,*dec,*change,*name,*password;
  String *wild;
  sql_exchange *exchange;
  List<List_item>     expr_list;
  List<List_item>     many_values;
  List<key_part_spec> col_list;
  List<Alter_drop>    drop_list;
  List<Alter_column>  alter_list;
  List<String>	      interval_list;
  List<st_lex_user>   users_list;
  List<LEX_COLUMN>  columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      *insert_list;

  TYPELIB	      *interval;
  Item *where,*having,*default_value;
  enum enum_duplicates duplicates;
  enum db_type db_type;
  ulong avg_length,thread_id,type;
  ulonglong max_rows;
  CONVERT *convert_set;
  char *db,*db1,*table1,*db2,*table2;		/* For outer join using .. */
  gptr yacc_yyss,yacc_yyvs;
  THD *thd;
  udf_func udf;
} LEX;


void lex_init(void);
void lex_free(void);
LEX *lex_start(THD *thd, uchar *buf,uint length);
void lex_end(LEX *lex);

extern pthread_key(LEX*,THR_LEX);

#define current_lex (&current_thd->lex)
