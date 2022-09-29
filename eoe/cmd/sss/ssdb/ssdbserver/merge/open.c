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

/* open a MERGE-database */

#include "mrgdef.h"
#include <stddef.h>
#include <errno.h>
#ifdef VMS
#include "static.c"
#endif

/*	open a MERGE-database.

	if handle_locking is 0 then exit with error if some database is locked
	if handle_locking is 1 then wait if database is locked
*/


MRG_INFO *mrg_open(name,mode,handle_locking)
const char *name;
int mode;
int handle_locking;
{
  int save_errno,i,errpos;
  uint files,dirname_length;
  ulonglong file_offset;
  char name_buff[FN_REFLEN*2],buff[FN_REFLEN],*end;
  MRG_INFO info,*m_info;
  FILE *file;
  N_INFO *isam,*last_isam;
  DBUG_ENTER("mrg_open");

  LINT_INIT(last_isam);
  isam=0;
  errpos=files=0;
  bzero((gptr) &info,sizeof(info));
  if (!(file=my_fopen(fn_format(name_buff,name,"",MRG_NAME_EXT,4),
		      O_RDONLY | O_SHARE,MYF(0))))
    goto err;
  errpos=1;
  dirname_length=dirname_part(name_buff,name);
  info.reclength=0;
  while (fgets(buff,FN_REFLEN-1,file))
  {
    if ((end=strend(buff))[-1] == '\n')
      end[-1]='\0';
    if (buff[0])		/* Skipp empty lines */
    {
      last_isam=isam;
      VOID(strmov(name_buff+dirname_length,buff));
      VOID(cleanup_dirname(buff,name_buff));
      if (!(isam=ni_open(buff,mode,test(handle_locking))))
	goto err;
      files++;
    }
    last_isam=isam;
    if (info.reclength && info.reclength != isam->s->base.reclength)
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      goto err;
    }
    info.reclength=isam->s->base.reclength;
  }
  if (!(m_info= (MRG_INFO*) my_malloc(sizeof(MRG_INFO)+files*sizeof(MRG_TABLE),
				      MYF(MY_WME))))
    goto err;
  memcpy(m_info,&info,sizeof(info));
  m_info->open_tables=(MRG_TABLE *) (m_info+1);
  m_info->tables=files;

  for (i=files ; i-- > 0 ; )
  {
    m_info->open_tables[i].table=isam;
    m_info->options|=isam->s->base.options;
    m_info->records+=isam->s->state.records;
    m_info->del+=isam->s->state.del;
    m_info->data_file_length=isam->s->state.data_file_length;
    if (i)
      isam=(N_INFO*) (isam->open_list.next->data);
  }
  /* Fix fileinfo for easyer debugging (actually set by rrnd) */
  file_offset=0;
  for (i=0 ; (uint) i < files ; i++)
  {
    m_info->open_tables[i].file_offset=(my_off_t) file_offset;
    file_offset+=m_info->open_tables[i].table->s->state.data_file_length;
  }
  if (sizeof(my_off_t) == 4 && file_offset > (ulonglong) (ulong) ~0L)
  {
    my_errno=HA_ERR_RECORD_FILE_FULL;
    my_free((char*) m_info,MYF(0));
    goto err;
  }

  m_info->end_table=m_info->open_tables+files;
  m_info->last_used_table=m_info->open_tables;

  VOID(my_fclose(file,MYF(0)));
  m_info->open_list.data=(void*) m_info;
  pthread_mutex_lock(&THR_LOCK_open);
  mrg_open_list=list_add(mrg_open_list,&m_info->open_list);
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno;
  switch (errpos) {
  case 1:
    VOID(my_fclose(file,MYF(0)));
    for (i=files ; i-- > 0 ; )
    {
      isam=last_isam;
      if (i)
	last_isam=(N_INFO*) (isam->open_list.next->data);
      ni_close(isam);
    }
  }
  my_errno=save_errno;
  DBUG_RETURN (NULL);
}
