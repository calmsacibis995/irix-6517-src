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

/* create and drop of databases */

#include "mysql_priv.h"
#include "sql_acl.h"
#include <my_dir.h>
#include <m_ctype.h>
#ifdef __WIN32__
#include <direct.h>
#endif


static bool check_name(const char *db)
{
#if defined(USE_MB) && defined(USE_MB_IDENT)
    if (!isalpha(*db) && *db != '_' && *db != '$' && !ismbhead(*db))
	return 1;
    while (*db) {
	int l = 0;
	if (!isvar(*db) && *db != '$' && (l = ismbchar(db, db+MBMAXLEN)) == 0)
	    return 1;
	db += l? l: 1;
    }
    return 0;
#else
  if (!isalpha(*db) && *db != '_' && *db != '$')
    return 1;					// Can't start with num
  for (db++ ; *db ; db++)
  {
    if (!isvar(*db) && *db != '$')
      return 1;
  }
  return 0;
#endif
}


void mysql_create_db(THD *thd,char *db)
{
  char	 path[FN_REFLEN];
  MY_DIR *dirp;
  DBUG_ENTER("mysql_create_db");

  if (!stripp_sp(db) || strlen(db) > NAME_LEN || check_name(db))
  {
    net_printf(&thd->net,ER_WRONG_DB_NAME, db);
    DBUG_VOID_RETURN;
  }
  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));

  /* Check directory */
  (void)sprintf(path,"%s/%s", mysql_data_home, db);
  unpack_dirname(path,path);			// Convert if not unix
  if ((dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    my_dirend(dirp);
    net_printf(&thd->net,ER_DB_CREATE_EXISTS,db);
    goto exit;
  }

  /* Create the directory */
  strend(path)[-1]=0;				// Remove last '/' from path
  if (my_mkdir(path,0700,MYF(0)) < 0)
  {
    net_printf(&thd->net,ER_CANT_CREATE_DB,db,my_errno);
    goto exit;
  }
  send_ok(&thd->net,1L);

exit:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_VOID_RETURN;
}

my_string del_exts[]={".frm",".ISM",".ISD",".ISM",".HSH",".DAT",".MRG",".PSM",
		      NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts",del_exts};


void mysql_rm_db(THD *thd,char *db,bool if_exists)
{
  uint index;
  char	path[FN_REFLEN],filePath[FN_REFLEN];
  MY_DIR *dirp;
  FILEINFO *file;
  ulong deleted=0;
  DBUG_ENTER("mysql_rm_db");

  if (!stripp_sp(db))
  {
    send_error(&thd->net,ER_NO_DB_ERROR);
    DBUG_VOID_RETURN;
  }

  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));
  VOID(pthread_mutex_lock(&LOCK_open));

  /* See if the directory exists */
  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  if (!(dirp = my_dir(path,MYF(MY_WME | MY_DONT_SORT))))
  {
    if (!if_exists)
      net_printf(&thd->net,ER_DB_DROP_EXISTS,db);
    else
      send_ok(&thd->net,0);
    goto exit;
  }

  remove_db_from_cache(db);

  /* remove all files with known extensions */

  for (index=2 ;
       index < (uint) dirp->number_off_files && !thd->killed ;
       index++)
  {
    file=dirp->dir_entry+index;
    if (find_type(fn_ext(file->name),&deletable_extentions,1) <= 0)
      continue;
    sprintf(filePath,"%s/%s",path,file->name);
    if (my_delete(filePath,MYF(MY_WME)))
    {
      net_printf(&thd->net,ER_DB_DROP_DELETE,filePath,my_error);
      my_dirend(dirp);
      goto exit;
    }
    deleted++;
  }

  my_dirend(dirp);
  if (thd->killed)
    send_error(&thd->net,ER_SERVER_SHUTDOWN);
  else
  {
    if (rmdir(path) < 0)
      net_printf(&thd->net,ER_DB_DROP_RMDIR, path,errno);
    else
      send_ok(&thd->net,deleted);
  }
exit:
  VOID(pthread_mutex_unlock(&LOCK_open));
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_VOID_RETURN;
}


bool mysql_change_db(THD *thd,const char *name)
{
  int length;
  char *dbname=my_strdup((char*) name,MYF(MY_WME));
  char	path[FN_REFLEN];
  uint db_access;
  DBUG_ENTER("mysql_change_db");

  if (!dbname || !(length=stripp_sp(dbname)))
  {
    x_free(dbname);				/* purecov: inspected */
    send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (length > NAME_LEN)
  {
    net_printf(&thd->net,ER_WRONG_DB_NAME, dbname);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("general",("Use database: %s", dbname));
  if (test_all_bits(thd->master_access,DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(thd->host,thd->ip,(char*) &thd->remote.sin_addr,
			thd->priv_user,dbname) |
		thd->master_access);
  if (!(db_access & DB_ACLS) && (!grant_option || check_grant_db(thd,name)))
  {
    net_printf(&thd->net,ER_DBACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->host ? thd->host : thd->ip ? thd->ip : "unknown",
	       dbname);
    mysql_log.write(COM_INIT_DB,ER(ER_DBACCESS_DENIED_ERROR),
		    thd->priv_user,
		    thd->host ? thd->host : thd->ip ? thd->ip : "unknown",
		    dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }

  (void)sprintf(path,"%s/%s",mysql_data_home,dbname);
  if (access(path,F_OK))
  {
    net_printf(&thd->net,ER_BAD_DB_ERROR,dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }
  send_ok(&thd->net);
  x_free(thd->db);
  thd->db=dbname;
  thd->db_access=db_access;
  DBUG_RETURN(0);
}
