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

#define SELECT_ACL	1
#define INSERT_ACL	2
#define UPDATE_ACL	4
#define DELETE_ACL	8
#define CREATE_ACL	16
#define DROP_ACL	32
#define RELOAD_ACL	64
#define SHUTDOWN_ACL	128
#define PROCESS_ACL	256
#define FILE_ACL	512
#define GRANT_ACL	1024	/* GRANT ACL */
#define REFERENCES_ACL	2048
#define INDEX_ACL	4096
#define ALTER_ACL	8192
#define DB_ACLS		(UPDATE_ACL | SELECT_ACL | INSERT_ACL | \
			 DELETE_ACL | CREATE_ACL | DROP_ACL | GRANT_ACL | \
			 REFERENCES_ACL | INDEX_ACL | ALTER_ACL)
#define TABLE_ACLS	(SELECT_ACL | INSERT_ACL | UPDATE_ACL | \
			 DELETE_ACL | CREATE_ACL | DROP_ACL | GRANT_ACL | \
			 REFERENCES_ACL | INDEX_ACL | ALTER_ACL)
#define COL_ACLS	(SELECT_ACL | INSERT_ACL | UPDATE_ACL | REFERENCES_ACL)
#define GLOBAL_ACLS	(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL |\
			 CREATE_ACL | DROP_ACL |  RELOAD_ACL | SHUTDOWN_ACL |\
			 PROCESS_ACL | FILE_ACL | GRANT_ACL | REFERENCES_ACL |\
			 INDEX_ACL | ALTER_ACL)
#define NO_ACCESS	32768

/* defines to change the above bits to how things are stored in tables */

#define fix_rights_for_db(A) (((A) & 63) | (((A) & ~63) << 4))
#define get_rights_for_db(A) (((A) & 63) | (((A) & ~63) >> 4))
#define fix_rights_for_table(A) (((A) & 63) | (((A) & ~63) << 4))
#define get_rights_for_table(A) (((A) & 63) | (((A) & ~63) >> 4))
#define fix_rights_for_column(A) (((A) & COL_ACLS) | ((A & ~COL_ACLS) << 7))
#define get_rights_for_column(A) (((A) & COL_ACLS) | ((A & ~COL_ACLS) >> 7))

/* prototypes */

int  acl_init(bool dont_read_acl_tables);
void acl_reload(void);
void acl_free(bool end=0);
uint acl_get(const char *host, const char *ip, const char *bin_ip,
	     const char *user, const char *db);
uint acl_getroot(const char *host, const char *ip, const char *user,
		 const char *password,const char *scramble,char **priv_user,
		 bool old_ver);
bool acl_check_host(const char *host, const char *ip);
bool change_password(THD *thd, const char *host, const char *user,
		     char *password);
int mysql_grant(THD *thd, const char *db, List <LEX_USER> &user_list,
		uint rights, bool revoke);
int mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
		      List <LEX_COLUMN> &column_list, uint rights,
		      bool revoke);
int  grant_init(void);
void grant_free(void);
void grant_reload(void);
bool check_grant(THD *thd, uint want_access, TABLE_LIST *tables, 
		 uint show_command=0);
bool check_grant_column (THD *thd,TABLE *table, const char *name,uint length,
			 uint show_command=0);
bool check_grant_all_columns(THD *thd, uint want_access, TABLE *table);
bool check_grant_db(THD *thd,const char *db);

