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

/* Function with list databases, tables or fields */

#include "mysql_priv.h"
#include "sql_select.h"				// For select_describe
#include "sql_acl.h"
#include <my_dir.h>

static int mysql_find_files(THD *thd,const char *db,const char *path,
			    const char *wild, bool dir);

/****************************************************************************
** Send list of databases
** A database is a directory in the mysql_data_home directory
****************************************************************************/


int
mysqld_show_dbs(THD *thd,const char *wild)
{
  Item_string *field=new Item_string("",0);
  List<Item> field_list;
  char *end;
  DBUG_ENTER("mysqld_show_dbs");

  field->name=(char*) sql_alloc(20+ (wild ? strlen(wild)+4: 0));
  field->max_length=NAME_LEN;
  end=strmov(field->name,"Database");
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field_list.push_back(field);

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_find_files(thd,NullS,mysql_data_home,wild,1));
}

/***************************************************************************
** List all tables in a database
** A table is a .frm file in the current databasedir
***************************************************************************/

int mysqld_show_tables(THD *thd,const char *db,const char *wild)
{
  Item_string *field=new Item_string("",0);
  List<Item> field_list;
  char path[FN_LEN],*end;
  DBUG_ENTER("mysqld_show_tables");

  field->name=(char*) sql_alloc(20+strlen(db)+(wild ? strlen(wild)+4:0));
  end=strxmov(field->name,"Tables in ",db,NullS);
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field->max_length=NAME_LEN;
  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  (void) unpack_filename(path,path);
  field_list.push_back(field);
  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_find_files(thd,db,path,wild,0));
}


static int
mysql_find_files(THD *thd,const char *db,const char *path,
		 const char *wild,bool dir)
{
  uint i;
  char *ext;
  MY_DIR *dirp;
  FILEINFO *file;
  uint access=thd->col_access;
  TABLE_LIST table_list;
  DBUG_ENTER("mysql_find_files");

  bzero((char*) &table_list,sizeof(table_list));

  if (!(dirp = my_dir(path,MYF(MY_WME | (dir ? MY_WANT_STAT : 0)))))
    DBUG_RETURN(-1);

  for (i=0 ; i < (uint) dirp->number_off_files	; i++)
  {
    file=dirp->dir_entry+i;
    if (dir)
    {						/* Return databases */
      if (file->name[0] == '.' || !MY_S_ISDIR(file->mystat.st_mode) ||
	  (wild && wild[0] && wild_compare(file->name,wild)))
	continue;
    }
    else
    {
	// Return only .frm files which isn't temp files.
      if (strcmp(ext=fn_ext(file->name),reg_ext) ||
	  strchr(file->name,'-'))		// Mysql temp table
	continue;
      *ext=0;
      if (wild && wild[0] && wild_compare(file->name,wild))
	continue;
    }
    /* Don't show tables where we don't have any privileges */
    if (db && !(access & TABLE_ACLS))
    {
      table_list.db= (char*) db;
      table_list.real_name=file->name;
      table_list.grant.privilege=access;
      if (check_grant(thd,TABLE_ACLS,&table_list,1))
	continue;
    }
    thd->packet.length(0);
    net_store_data(&thd->packet,file->name);
    if (my_net_write(&thd->net,(char*) thd->packet.ptr(),thd->packet.length()))
      break;
  }
  send_eof(&thd->net);
  my_dirend(dirp);
  DBUG_RETURN(0);
}


int
mysqld_show_fields(THD *thd, TABLE_LIST *table_list,const char *wild)
{
  TABLE *table;
  char tmp[MAX_FIELD_WIDTH];
  uint access=thd->col_access;
  DBUG_ENTER("mysqld_show_fields");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
		      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_RETURN(1);
  }
  ha_info(table,2);			// sync record count

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Field",NAME_LEN));
  field_list.push_back(new Item_empty_string("Type",40));
  field_list.push_back(new Item_empty_string("Null",1));
  field_list.push_back(new Item_empty_string("Key",3));
  field_list.push_back(new Item_empty_string("Default",NAME_LEN));
  field_list.push_back(new Item_empty_string("Extra",20));

	// Send first number of fields and records
  {
    char *pos;
    pos=net_store_length(tmp,field_list.elements);
    pos=net_store_length(pos,(ulonglong) table->keyfile_info.records);
    (void) my_net_write(&thd->net,tmp,(uint) (pos-tmp));
  }

  if (send_fields(thd,field_list,0))
    DBUG_RETURN(1);
  restore_record(table,2);	// Get empty record

  Field **ptr,*field;
  for (ptr=table->field; (field= *ptr) ; ptr++)
  {
    if (!wild || !wild[0] || !wild_case_compare(field->field_name,wild))
    {
#ifdef NOT_USED
      if (access & TABLE_ACLS ||
	  ! check_grant_column(thd,table,field->field_name,
			       strlen(field->field_name),1))
#endif
      {
	byte *pos;
	uint flags=field->flags;
	String *packet= &thd->packet,type(tmp,sizeof(tmp));

	packet->length(0);
	net_store_data(packet,field->field_name);
	field->sql_type(type);
	net_store_data(packet,type.ptr(),type.length());

	pos=(byte*) ((flags & NOT_NULL_FLAG) &&
		     field->type() != FIELD_TYPE_TIMESTAMP ?
		     "" : "YES");
	net_store_data(packet,(const char*) pos);
	pos=(byte*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
		     (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
		     (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
	net_store_data(packet,(char*) pos);

	if (!(flags & BLOB_FLAG) && field->type() != FIELD_TYPE_TIMESTAMP &&
	    !field->is_null())
	{						// Not null by default
	  type.set(tmp,sizeof(tmp));
	  field->val_str(&type,&type);
	  net_store_data(packet,type.ptr(),type.length());
	}
	else if (field->maybe_null() ||
		 field->type() == FIELD_TYPE_TIMESTAMP ||
		 (flags & BLOB_FLAG))
	  net_store_null(packet);			// Null as default
	else
	  net_store_data(packet,tmp,0);

	char *end=tmp;
	if (field->unireg_check == Field::NEXT_NUMBER)
	  end=strmov(tmp,"auto_increment");
	net_store_data(packet,tmp,(uint) (end-tmp));
	if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
	  DBUG_RETURN(1);
      }
    }
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}


int
mysqld_show_keys(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  DBUG_ENTER("mysqld_show_keys");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
		      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_RETURN(1);
  }

  List<Item> field_list;
  Item *item;
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  field_list.push_back(new Item_int("Non_unique",0,1));
  field_list.push_back(new Item_empty_string("Key_name",NAME_LEN));
  field_list.push_back(new Item_int("Seq_in_index",0,2));
  field_list.push_back(new Item_empty_string("Column_name",NAME_LEN));
  field_list.push_back(item=new Item_empty_string("Collation",1));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Cardinality",0,11));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Sub_part",0,3));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  TABLE *form= table;
  KEY *key_info=form->key_info;
  ha_info(form,2);
  for (uint i=0 ; i < form->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    char buff[10],*end;
    String *packet= &thd->packet;
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      packet->length(0);
      net_store_data(packet,table->table_name);
      net_store_data(packet,(key_info->dupp_key ? "1" :"0"), 1);
      net_store_data(packet,key_info->name);
      end=int2str((long) (j+1),(char*) buff,10);
      net_store_data(packet,buff,(uint) (end-buff));
      net_store_data(packet,key_part->field ? key_part->field->field_name :
		     "?unknown field?");
      if (ha_option_flag[form->db_type] & HA_READ_ORDER)
	net_store_data(packet,(key_part->key_part_flag == 0 ? "A" : "D"), 1);
      else
	net_store_null(packet); /* purecov: inspected */
      if (form->db_type == DB_TYPE_ISAM && form->keyfile_info.rec_per_key[i])
      {
	ulong records=(form->keyfile_info.records /
		       form->keyfile_info.rec_per_key[i]);
	end=int2str((long) records, buff, 10);
	net_store_data(packet,buff,(uint) (end-buff));
      }
      else
	net_store_null(packet);
      if (!key_part->field ||
	  key_part->length !=
	  table->field[key_part->fieldnr-1]->pack_length())
      {
	end=int2str((long) key_part->length, buff,10); /* purecov: inspected */
	net_store_data(packet,buff,(uint) (end-buff)); /* purecov: inspected */
      }
      else
	net_store_null(packet);
      if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
	DBUG_RETURN(1); /* purecov: inspected */
    }
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}


/****************************************************************************
** Return only fields for API mysql_list_fields
** Use "show table wildcard" in mysql instead of this
****************************************************************************/

void
mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild)
{
  TABLE *table;
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter",("table: %s",table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_VOID_RETURN;
  }
  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || !wild_case_compare(field->field_name,wild))
      field_list.push_back(new Item_field(field));
  }
  restore_record(table,2);		// Get empty record
  if (send_fields(thd,field_list,2))
    DBUG_VOID_RETURN;
  VOID(net_flush(&thd->net));
  DBUG_VOID_RETURN;
}


/****************************************************************************
** Return info about all processes
** returns for each thread: thread id, user, host, db, command, info
****************************************************************************/

class thread_info :public Sql_alloc, public ilink {
public:
  ulong thread_id,start_time;
  uint	 command;
  char	*user,*host,*db,*query,*proc_info,*state_info;
};

#ifdef __GNUC__
template class I_List<thread_info>;
template class I_List_iterator<THD>;
#endif


void mysqld_list_processes(THD *thd,const char *user)
{
  Item *field;
  List<Item> field_list;
  I_List<thread_info> thread_infos;
  DBUG_ENTER("mysql_list_processes");

  field_list.push_back(new Item_int("Id",0,7));
  field_list.push_back(new Item_empty_string("User",16));
  field_list.push_back(new Item_empty_string("Host",64));
  field_list.push_back(field=new Item_empty_string("db",NAME_LEN));
  field->maybe_null=1;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(new Item_empty_string("Time",7));
  field_list.push_back(field=new Item_empty_string("State",20));
  field->maybe_null=1;
  field_list.push_back(field=new Item_empty_string("Info",100));
  field->maybe_null=1;
  if (send_fields(thd,field_list,1))
    DBUG_VOID_RETURN;

  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  if (!thd->killed)
  {
    I_List_iterator<THD> it(threads);
    THD *tmp;
    while ((tmp=it++))
    {
      if (tmp->net.fd >= 0 && (!user || !strcmp(tmp->user,user)))
      {
	thread_info *thd_info=new thread_info;

	thd_info->thread_id=tmp->thread_id;
	thd_info->user=sql_strdup(tmp->user ? tmp->user : "unknown user");
	thd_info->host=sql_strdup(tmp->host ? tmp->host : tmp->ip ? tmp->ip : "unknown host");
	if ((thd_info->db=tmp->db))		// Safe test
	  thd_info->db=sql_strdup(thd_info->db);
	thd_info->command=(int) tmp->command;
	if (tmp->mysys_var)
	  pthread_mutex_lock(&tmp->mysys_var->mutex);
	thd_info->proc_info= (char*) (tmp->killed ? "Killed" : 0);
	thd_info->state_info= (char*) (tmp->proc_info ? tmp->proc_info :
				       tmp->locked ? "Locked" :
				       tmp->mysys_var &&
				       tmp->mysys_var->current_cond ?
				       "Waiting for cond" : NullS);
	if (tmp->mysys_var)
	  pthread_mutex_unlock(&tmp->mysys_var->mutex);

#ifndef DONT_USE_THR_ALARM
	if (pthread_kill(tmp->real_id,0))
	  tmp->proc_info="*** DEAD ***";	// This shouldn't happen
#endif
	thd_info->start_time= tmp->start_time;
	thd_info->query=0;
	if (tmp->query)
	{
	  uint length=strlen(tmp->query);
	  if (length > 100)
	    length=100;
	  thd_info->query=(char*) sql_memdup(tmp->query,length+1);
	  thd_info->query[length]=0;
	}
	thread_infos.append(thd_info);
      }
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thread_info *thd_info;
  String *packet= &thd->packet;
  while ((thd_info=thread_infos.get()))
  {
    char buff[20],*end;
    packet->length(0);
    end=int2str((long) thd_info->thread_id, buff,10);
    net_store_data(packet,buff,(uint) (end-buff));
    net_store_data(packet,thd_info->user);
    net_store_data(packet,thd_info->host);
    if (thd_info->db)
      net_store_data(packet,thd_info->db);
    else
      net_store_null(packet);
    if (thd_info->proc_info)
      net_store_data(packet,thd_info->proc_info);
    else
      net_store_data(packet,command_name[thd_info->command]);
    if (thd_info->start_time)
      net_store_data(packet,(ulong) (time((time_t*) 0) - thd_info->start_time));
    else
      net_store_null(packet);
    if (thd_info->state_info)
      net_store_data(packet,thd_info->state_info);
    else
      net_store_null(packet);
    if (thd_info->query)
      net_store_data(packet,thd_info->query);
    else
      net_store_null(packet);
    if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
      break; /* purecov: inspected */
  }
  send_eof(&thd->net);
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Status functions
*****************************************************************************/


int mysqld_show(THD *thd, const char *wild, show_var_st *variables)
{
  uint i;
  char buff[1024];
  String packet2(buff,sizeof(buff));
  List<Item> field_list;
  DBUG_ENTER("mysqld_show");
  field_list.push_back(new Item_empty_string("Variable_name",30));
  field_list.push_back(new Item_empty_string("Value",256));
  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1); /* purecov: inspected */

  for (i=0; variables[i].name; i++)
  {
    if (!(wild && wild[0] && wild_compare(variables[i].name,wild)))
    {
      packet2.length(0);
      net_store_data(&packet2,variables[i].name);
      switch (variables[i].type){
      case SHOW_LONG:
      case SHOW_LONG_CONST:
	net_store_data(&packet2,*(ulong*) variables[i].value);
	break;
      case SHOW_BOOL:
	net_store_data(&packet2,(ulong) *(bool*) variables[i].value ?
		       "ON" : "OFF");
	break;
      case SHOW_MY_BOOL:
	net_store_data(&packet2,(ulong) *(my_bool*) variables[i].value ?
		       "ON" : "OFF");
	break;
      case SHOW_INT:
	net_store_data(&packet2,(ulong) *(int*) variables[i].value);
	break;
      case SHOW_CHAR:
	net_store_data(&packet2,variables[i].value);
	break;
      case SHOW_STARTTIME:
	net_store_data(&packet2,(long) (thd->query_start() - start_time));
	break;
      case SHOW_QUESTION:
	net_store_data(&packet2,(long) thd->query_id);
	break;
      case SHOW_OPENTABLES:
	net_store_data(&packet2,(long) cached_tables());
	break;
      case SHOW_CHAR_PTR:
	{
	  char *value= *(char**) variables[i].value;
	  net_store_data(&packet2,value ? value : "");
	  break;
	}
      }
      if (my_net_write(&thd->net, (char*) packet2.ptr(),packet2.length()))
	DBUG_RETURN(1); /* purecov: inspected */
    }
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}
