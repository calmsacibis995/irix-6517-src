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

/* close a isam-database */

#include "isamdef.h"

int ni_close(register N_INFO *info)
{
  int error=0,flag;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("ni_close");
  DBUG_PRINT("enter",("base: %lx  reopen: %u  locks: %u",
		      info,(uint) share->reopen,
		      (uint) (share->w_locks+share->r_locks)));

  pthread_mutex_lock(&THR_LOCK_isam);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

#ifndef NO_LOCKING
  if (info->lock_type != F_UNLCK)
    VOID(ni_lock_database(info,F_UNLCK));
#else
  info->lock_type=F_UNLCK;
  share->w_locks--;
  if (_ni_writeinfo(info,share->changed))
    error=my_errno;
#endif
  pthread_mutex_lock(&share->intern_lock);

  if (share->base.options & HA_OPTION_READ_ONLY_DATA)
    share->r_locks--;
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    if (end_io_cache(&info->rec_cache))
      error=my_errno;
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag= !--share->reopen;
  nisam_open_list=list_delete(nisam_open_list,&info->open_list);
  pthread_mutex_unlock(&share->intern_lock);

  if (flag)
  {
    if (share->kfile >= 0 && flush_key_blocks(share->kfile,(pbool) 1))
      error=my_errno;
    if (share->kfile >= 0 && my_close(share->kfile,MYF(0)))
      error = my_errno;
#ifdef HAVE_MMAP
    if (info->opt_flag & MEMMAP_USED)
      _ni_unmap_file(info);
#endif
    if (share->decode_trees)
    {
      my_free((gptr) share->decode_trees,MYF(0));
      my_free((gptr) share->decode_tables,MYF(0));
    }
#ifdef THREAD
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->intern_lock));
#endif
    my_free((gptr) info->s,MYF(0));
  }
  pthread_mutex_unlock(&THR_LOCK_isam);
  if (info->dfile >= 0 && my_close(info->dfile,MYF(0)))
    error = my_errno;

  nisam_log_command(LOG_CLOSE,info,NULL,0,error);
  my_free((gptr) info->rec_alloc,MYF(0));
  my_free((gptr) info,MYF(0));

  if (error)
  {
    my_errno=error;
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
} /* ni_close */
