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

/* Ger tillbaka en struct med information om isam-filen */

#include "isamdef.h"
#ifdef	__WIN32__
#include <sys/stat.h>
#endif

	/* If flag == 1 one only gets pos of last record */
	/* if flag == 2 one get current info (no sync from database */

int ni_info(N_INFO *info, register N_ISAMINFO *x, int flag)
{
  struct stat state;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("ni_info");

  x->recpos  = info->lastpos;
  if (flag != 1)
  {
#ifndef NO_LOCKING
    if (!flag)
    {
      pthread_mutex_lock(&share->intern_lock);
      VOID(_ni_readinfo(info,F_RDLCK,0));
      VOID(_ni_writeinfo(info,0));
      pthread_mutex_unlock(&share->intern_lock);
    }
#endif
    x->records	 = share->state.records;
    x->deleted	 = share->state.del;
    x->keys	 = share->state.keys;
    x->reclength = share->base.reclength;
    x->mean_reclength= share->state.records ?
      (share->state.data_file_length-share->state.empty)/share->state.records :
      share->min_pack_length;
    x->data_file_length=share->state.data_file_length;
    x->filenr	 = info->dfile;
    x->errkey	 = info->errkey;
    x->options	 = share->base.options;
    x->create_time=share->base.create_time;
    x->isamchk_time=share->base.isamchk_time;
    x->rec_per_key=share->base.rec_per_key;
    if (!fstat(info->dfile,&state))
      x->update_time=state.st_mtime;
    else
      x->update_time=0;
    x->sortkey= -1;				/* No clustering */
  }
  DBUG_RETURN(0);
} /* ni_info */
