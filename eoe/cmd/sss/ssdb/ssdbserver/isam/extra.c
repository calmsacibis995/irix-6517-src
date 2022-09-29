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

/* Extra functions we want to do with a database */
/* - Set flags for quicker databasehandler */
/* - Set databasehandler to normal */
/* - Reset recordpointers as after open database */

#include "isamdef.h"
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef	__WIN32__
#include <errno.h>
#endif

	/* set extra flags for database */

int ni_extra(N_INFO *info, enum ha_extra_function function)
{
  int error=0;
  DBUG_ENTER("ni_extra");

  switch (function) {
  case HA_EXTRA_RESET:
    info->lastinx= 0;			/* Use first index as def */
    info->int_pos=info->lastpos= NI_POS_ERROR;
    info->update= (info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
      HA_STATE_PREV_FOUND;
					/* Next/prev gives first/last */
    if (info->opt_flag & READ_CACHE_USED)
    {
      VOID(flush_io_cache(&info->rec_cache));
      reinit_io_cache(&info->rec_cache,READ_CACHE,0,
		     (pbool) (info->lock_type != F_UNLCK));
    }
    break;
  case HA_EXTRA_CACHE:
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK && (info->options & HA_OPTION_PACK_RECORD))
    {
      error=1;			/* Not possibly if not locked */
      my_errno=EACCES;
      break;
    }
#endif
#if defined(HAVE_MMAP) && defined(HAVE_MADVICE)
    if ((info->options & HA_OPTION_COMPRESS_RECORD))
    {
      pthread_mutex_lock(&info->s->intern_lock);
      if (info->s->file_map || (info->s->file_map=_ni_memmap_file(info)))
      {
	/* We don't nead MADV_SEQUENTIAL if small file */
	madvise(info->s->file_map,info->s->state.data_file_length,
		info->s->state.data_file_length <= RECORD_CACHE_SIZE*16 ?
		MADV_RANDOM : MADV_SEQUENTIAL);
	pthread_mutex_unlock(&info->s->intern_lock);
	break;
      }
      pthread_mutex_unlock(&info->s->intern_lock);
    }
#endif
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      info->opt_flag&= ~WRITE_CACHE_USED;
      if ((error=end_io_cache(&info->rec_cache)))
	break;
    }
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | MEMMAP_USED)))
    {
      if (!(init_io_cache(&info->rec_cache,info->dfile,
			 (uint) min(info->s->state.data_file_length+1,
				    my_default_record_cache_size),
			 READ_CACHE,0L,(pbool) (info->lock_type != F_UNLCK),
			 MYF(MY_WAIT_IF_FULL))))
	info->opt_flag|=READ_CACHE_USED;
      /* info->rec_cache.end_of_file=info->s->state.data_file_length; */
    }
    break;
  case HA_EXTRA_REINIT_CACHE:
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache,READ_CACHE,info->nextpos,
		     (pbool) (info->lock_type != F_UNLCK));
      /* info->rec_cache.end_of_file=info->s->state.data_file_length; */
    }
    break;
  case HA_EXTRA_WRITE_CACHE:
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK)
    {
      error=1;			/* Not possibly if not locked */
      break;
    }
#endif
    if (!(info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED)))
      if (!(init_io_cache(&info->rec_cache,info->dfile,0,
			 WRITE_CACHE,info->s->state.data_file_length,
			 (pbool) (info->lock_type != F_UNLCK),
			 MYF(MY_WAIT_IF_FULL))))
	info->opt_flag|=WRITE_CACHE_USED;
    break;
  case HA_EXTRA_NO_CACHE:
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
#if defined(HAVE_MMAP) && defined(HAVE_MADVICE)
    if (info->opt_flag & MEMMAP_USED)
      madvise(info->s->file_map,info->s->state.data_file_length,MADV_RANDOM);
#endif
    break;
  case HA_EXTRA_FLUSH_CACHE:
    if (info->opt_flag & WRITE_CACHE_USED)
      error=flush_io_cache(&info->rec_cache);
    break;
  case HA_EXTRA_NO_READCHECK:
    info->opt_flag&= ~READ_CHECK_USED;	/* No readcheck */
    break;
  case HA_EXTRA_READCHECK:
    info->opt_flag|= READ_CHECK_USED;
    break;
  case HA_EXTRA_KEYREAD:			/* Read only keys to record */
  case HA_EXTRA_REMEMBER_POS:
    info->opt_flag |= REMEMBER_OLD_POS;
    bmove((byte*) info->lastkey+info->s->base.max_key_length*2,
	  (byte*) info->lastkey,info->s->base.max_key_length);
    info->save_update=	info->update;
    info->save_lastinx= info->lastinx;
    info->save_lastpos= info->lastpos;
    if (function == HA_EXTRA_REMEMBER_POS)
      break;
    /* fall through */
  case HA_EXTRA_KEYREAD_CHANGE_POS:
    info->opt_flag |= KEY_READ_USED;
    info->read_record=_ni_read_key_record;
    break;
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_RESTORE_POS:
    if (info->opt_flag & REMEMBER_OLD_POS)
    {
      bmove((byte*) info->lastkey,
	    (byte*) info->lastkey+info->s->base.max_key_length*2,
	    info->s->base.max_key_length);
      info->update=	info->save_update | HA_STATE_WRITTEN;
      info->lastinx=	info->save_lastinx;
      info->lastpos=	info->save_lastpos;
    }
    info->read_record=	info->s->read_record;
    info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
    break;
  case HA_EXTRA_NO_USER_CHANGE: /* Database is somehow locked agains changes */
    info->lock_type= F_EXTRA_LCK; /* Simulate as locked */
    break;
  case HA_EXTRA_WAIT_LOCK:
    info->lock_wait=0;
    break;
  case HA_EXTRA_NO_WAIT_LOCK:
    info->lock_wait=MY_DONT_WAIT;
    break;
  case HA_EXTRA_NO_KEYS:
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK)
    {
      error=1;					/* Not possibly if not lock */
      break;
    }
#endif
    info->s->state.keys=0;
    info->s->state.key_file_length=info->s->base.keystart;
    info->s->changed=1;				/* Update on close */
    break;
  case HA_EXTRA_FORCE_REOPEN:
    pthread_mutex_lock(&THR_LOCK_isam);
    info->s->last_version= 0L;			/* Impossible version */
#ifdef __WIN32__
    /* Close the isam and data files as Win32 can't drop an open table */
    if (flush_key_blocks(info->s->kfile,1))
      error=my_errno;
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
    if (info->lock_type != F_UNLCK && ! info->was_locked)
    {
      info->was_locked=info->lock_type;
      if (ni_lock_database(info,F_UNLCK))
	error=my_errno;
    }
    if (info->s->kfile >= 0 && my_close(info->s->kfile,MYF(0)))
      error=my_errno;
    {
      LIST *list_element ;
      for (list_element=nisam_open_list ;
	   list_element ;
	   list_element=list_element->next)
      {
	N_INFO *tmpinfo=(N_INFO*) list_element->data;
	if (tmpinfo->s == info->s)
	{
	  if (tmpinfo->dfile >= 0 && my_close(tmpinfo->dfile,MYF(0)))
	    error = my_errno;
	  tmpinfo->dfile=-1;
	}
      }
    }
    info->s->kfile=-1;				/* Files aren't open anymore */
#endif
    pthread_mutex_unlock(&THR_LOCK_isam);
    break;
  case HA_EXTRA_NORMAL:				/* Theese isn't in use */
  case HA_EXTRA_QUICK:
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }
  nisam_log_command(LOG_EXTRA,info,(byte*) &function,sizeof(function),error);
  DBUG_RETURN(error);
} /* ni_extra */
