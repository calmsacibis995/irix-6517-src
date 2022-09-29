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

/* Uppdaterare nuvarande record i en pisam-databas */

#include "isamdef.h"
#ifdef	__WIN32__
#include <errno.h>
#endif

	/* Updaterar senaste l{sta record i databasen */

int ni_update(register N_INFO *info, const byte *oldrec, const byte *newrec)
{
  int flag,key_changed,save_errno;
  reg3 ulong pos;
  uint i,length;
  uchar old_key[N_MAX_KEY_BUFF],*new_key;
  DBUG_ENTER("ni_update");

  if (!(info->update & HA_STATE_AKTIV))
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    DBUG_RETURN(-1);
  }
  if (info->s->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
  pos=info->lastpos;
#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_WRLCK,1)) DBUG_RETURN(-1);
#endif
  if ((*info->s->compare_record)(info,oldrec))
  {
    save_errno=my_errno;
    goto err_end;			/* Record has changed */
  }

	/* Flyttar de element i isamfilen som m}ste flyttas */

  new_key=info->lastkey+info->s->base.max_key_length;
  key_changed=HA_STATE_KEY_CHANGED;	/* We changed current database */
					/* Remove key that didn't change */
  for (i=0 ; i < info->s->state.keys ; i++)
  {
    length=_ni_make_key(info,i,new_key,newrec,pos);
    if (length != _ni_make_key(info,i,old_key,oldrec,pos) ||
	memcmp((byte*) old_key,(byte*) new_key,length))
    {
      if ((int) i == info->lastinx)
	key_changed|=HA_STATE_WRITTEN;		/* Mark that keyfile changed */
      if (_ni_ck_delete(info,i,old_key)) goto err;
      if (_ni_ck_write(info,i,new_key)) goto err;
    }
  }

  if ((*info->s->update_record)(info,pos,newrec))
    goto err;

  info->update= HA_STATE_CHANGED | HA_STATE_AKTIV | key_changed;
  nisam_log_record(LOG_UPDATE,info,newrec,info->lastpos,0);
  VOID(_ni_writeinfo(info,key_changed));
  allow_break();				/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  DBUG_PRINT("error",("key: %d  errno: %d",i,my_errno));
  save_errno=my_errno;
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    flag=0;
    do
    {
      length=_ni_make_key(info,i,new_key,newrec,pos);
      if (length != _ni_make_key(info,i,old_key,oldrec,pos) ||
	  memcmp((byte*) old_key,(byte*) new_key,length))
      {
	if ((flag++ && _ni_ck_delete(info,i,new_key)) ||
	    _ni_ck_write(info,i,old_key))
	  break;
      }
    } while (i-- != 0);
  }
  info->update= HA_STATE_CHANGED | HA_STATE_AKTIV | key_changed;
 err_end:
  nisam_log_record(LOG_UPDATE,info,newrec,info->lastpos,my_errno);
  VOID(_ni_writeinfo(info,1));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
    my_errno=HA_ERR_CRASHED;
  else
    my_errno=save_errno;
  DBUG_RETURN(-1);
} /* ni_update */
