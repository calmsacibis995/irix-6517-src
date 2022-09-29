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

/*
  locking of isam-tables.
  reads info from a isam-table. Must be first request before doing any furter
  calls to any isamfunktion.  Is used to allow many process use the same
  isamdatabase.
  */

#include "isamdef.h"
#ifdef	__WIN32__
#include <errno.h>
#endif

	/* lock table by F_UNLCK, F_RDLCK or F_WRLCK */

int ni_lock_database(N_INFO *info, int lock_type)
{
  int error;
  uint count;
  ISAM_SHARE *share;
  uint flag;
  DBUG_ENTER("ni_lock_database");

  flag=error=0;
#ifndef NO_LOCKING
  share=info->s;
  if (share->base.options & HA_OPTION_READ_ONLY_DATA ||
      info->lock_type == lock_type)
    DBUG_RETURN(0);
  pthread_mutex_lock(&share->intern_lock);
  switch (lock_type) {
  case F_UNLCK:
    if (info->lock_type == F_RDLCK)
      count= --share->r_locks;
    else
      count= --share->w_locks;
    if (info->lock_type == F_WRLCK && !share->w_locks &&
	flush_key_blocks(share->kfile,0))
      error=my_errno;
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
      if (end_io_cache(&info->rec_cache))
	error=my_errno;

    if (!count)
    {
      if (share->changed && !share->w_locks)
      {
	share->state.process= share->last_process=share->this_process;
	share->state.loop=    info->last_loop=	    ++info->this_loop;
	share->state.uniq=    info->last_uniq=	    info->this_uniq;
	if (my_pwrite(share->kfile,(char*) &share->state.header,
		      share->state_length,0L,MYF(MY_NABP)))
	  error=my_errno;
	share->changed=0;
#ifdef __WIN32__
	if (nisam_flush)
	{
	  _commit(share->kfile);
	  _commit(info->dfile);
	}
#endif
      }
      if (share->r_locks)
      {					/* Only read locks left */
	flag=1;
	if (my_lock(share->kfile,F_RDLCK,0L,F_TO_EOF,
		    MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	  error=my_errno;
      }
      else if (!share->w_locks)
      {					/* No more locks */
	flag=1;
	if (my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		    MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	  error=my_errno;
      }
    }
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
    info->lock_type= F_UNLCK;
    break;
  case F_RDLCK:
    if (info->lock_type == F_WRLCK)
    {						/* Change RW to READONLY */
      if (share->w_locks == 1)
      {
	flag=1;
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		    MYF(MY_SEEK_NOT_DONE)))
	{
	  error=my_errno;
	  break;
	}
      }
      share->w_locks--;
      share->r_locks++;
      info->lock_type=lock_type;
      break;
    }
    if (!share->r_locks && !share->w_locks)
    {
      flag=1;
#ifdef HAVE_FCNTL
      if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,info->lock_wait))
      {
	error=my_errno;
	break;
      }
      if (my_pread(share->kfile,
		   (char*) &share->state.header,share->state_length,0L,
		   MYF(MY_NABP)))
      {
	error=my_errno;
	VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE)));
	my_errno=error;
	break;
      }
#else
      VOID(my_seek(share->kfile,0L,MY_SEEK_SET,MYF(0)));
      if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,info->lock_wait))
      {
	error=my_errno;
	break;
      }
      if (my_read(share->kfile,
		  (char*) &share->state.header,share->state_length,
		  MYF(MY_NABP)))
      {
	error=my_errno;
	VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,info->lock_wait));
	my_errno=error;
	break;
      }
#endif
    }
    VOID(_ni_test_if_changed(info));
    share->r_locks++;
    info->lock_type=lock_type;
    break;
  case F_WRLCK:
    if (info->lock_type == F_RDLCK)
    {						/* Change RW to READONLY */
      if (share->r_locks == 1)
      {
	flag=1;
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		    MYF(info->lock_wait | MY_SEEK_NOT_DONE)))
	{
	  error=my_errno;
	  break;
	}
	share->r_locks--;
	share->w_locks++;
	info->lock_type=lock_type;
	break;
      }
    }
    if (!(share->base.options & HA_OPTION_READ_ONLY_DATA) && !share->w_locks)
    {
      flag=1;
      VOID(my_seek(share->kfile,0L,MY_SEEK_SET,MYF(0)));
      if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,info->lock_wait))
      {
	error=my_errno;
	break;
      }
      if (!share->r_locks)
      {
	if (my_read(share->kfile,
		    (char*) &share->state.header,share->state_length,
		    MYF(MY_NABP)))
	{
	  error=my_errno;
	  VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,info->lock_wait));
	  my_errno=error;
	  break;
	}
      }
    }
    VOID(_ni_test_if_changed(info));
    info->lock_type=lock_type;
    share->w_locks++;
    break;
  default:
    break;				/* Impossible */
  }
  pthread_mutex_unlock(&share->intern_lock);
#if defined(FULL_LOG) || defined(_lint)
  lock_type|=(int) (flag << 8);		/* Set bit to set if real lock */
  nisam_log_command(LOG_LOCK,info,(byte*) &lock_type,sizeof(lock_type),
		    error);
#endif
#endif
  DBUG_RETURN(error);
} /* ni_lock_database */


	/* Is used before access to database is granted */

int _ni_readinfo(register N_INFO *info, int lock_type, int check_keybuffer)
{
  ISAM_SHARE *share;
  DBUG_ENTER("_ni_readinfo");

  share=info->s;
  if (info->lock_type == F_UNLCK)
  {
    if (!share->r_locks && !share->w_locks)
    {
#ifndef HAVE_FCNTL
      VOID(my_seek(share->kfile,0L,MY_SEEK_SET,MYF(0)));
#endif
#ifndef NO_LOCKING
#ifdef UNSAFE_LOCKING
      if ((info->tmp_lock_type=lock_type) != F_RDLCK)
#endif
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,info->lock_wait))
	  DBUG_RETURN(1);
#endif
#ifdef HAVE_FCNTL
      if (my_pread(share->kfile,
		  (char*) &share->state.header,share->state_length,0L,
		  MYF(MY_NABP)))
#else
      if (my_read(share->kfile,
		  (char*) &share->state.header,share->state_length,
		  MYF(MY_NABP)))
#endif
      {
#ifndef NO_LOCKING
	int error=my_errno;
	VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		     MYF(MY_SEEK_NOT_DONE)));
	my_errno=error;
#endif
	DBUG_RETURN(1);
      }
    }
    if (check_keybuffer)
      VOID(_ni_test_if_changed(info));
  }
  else if (lock_type == F_WRLCK && info->lock_type == F_RDLCK)
  {
    my_errno=EACCES;				/* Not allowed to change */
    DBUG_RETURN(-1);				/* when have read_lock() */
  }
  DBUG_RETURN(0);
} /* _ni_readinfo */


	/* Every isam-function that uppdates the isam-database must! end */
	/* with this request */
	/* ARGSUSED */

int _ni_writeinfo(register N_INFO *info, pbool update_keyfile)
{
  int error,olderror;
  ISAM_SHARE *share;
  DBUG_ENTER("_ni_writeinfo");

  error=0;
  share=info->s;
  if (share->r_locks == 0 && share->w_locks == 0)
  {
    olderror=my_errno;			/* Remember last error */
    if (update_keyfile)
    {					/* Two threads can't be here */
      share->state.process= share->last_process=   share->this_process;
      share->state.loop=    info->last_loop=	    ++info->this_loop;
      share->state.uniq=    info->last_uniq=	   info->this_uniq;
      if ((error=my_pwrite(share->kfile,(char*) &share->state.header,
			   share->state_length,0L,MYF(MY_NABP)) != 0))
	olderror=my_errno;
#ifdef __WIN32__
      if (nisam_flush)
      {
	_commit(share->kfile);
	_commit(info->dfile);
      }
#endif
    }
#ifndef NO_LOCKING
#ifdef UNSAFE_LOCKING
    if (info->tmp_lock_type != F_RDLCK)
#endif
    {
      if (my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		  MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	DBUG_RETURN(1);
    }
#endif
    my_errno=olderror;
  }
  else if (update_keyfile)
    share->changed= 1;			/* Mark keyfile changed */
  DBUG_RETURN(error);
} /* _ni_writeinfo */


	/* Test if someone has changed the database */
	/* (Should be called after readinfo) */

int _ni_test_if_changed(register N_INFO *info)
{
#ifndef NO_LOCKING
  {
    ISAM_SHARE *share=info->s;
    if (share->state.process != share->last_process ||
	share->state.loop    != info->last_loop ||
	share->state.uniq    != info->last_uniq)
    {						/* Keyfile has changed */
      if (share->state.process != share->this_process)
	VOID(flush_key_blocks(share->kfile,1));
      share->last_process=share->state.process;
      info->last_loop=	share->state.loop;
      info->last_uniq=	share->state.uniq;
      info->update|=	HA_STATE_WRITTEN;	/* Must use file on next */
      info->data_changed= 1;			/* For ni_is_changed */
      return 1;
    }
  }
#endif
  return (!(info->update & HA_STATE_AKTIV) ||
	  (info->update & (HA_STATE_WRITTEN | HA_STATE_DELETED |
			   HA_STATE_KEY_CHANGED)));
} /* _ni_test_if_changed */
