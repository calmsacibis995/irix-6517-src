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

/* L{ser och skriver nyckelblock */

#include "isamdef.h"
#ifdef	__WIN32__
#include <errno.h>
#endif

	/* Fetch a key-page in memory */

uchar *_ni_fetch_keypage(register N_INFO *info, N_KEYDEF *keyinfo,
			 my_off_t page, uchar *buff, int return_buffer)
{
  uchar *tmp;
  tmp=(uchar*) key_cache_read(info->s->kfile,page,(byte*) buff,
			     (uint) keyinfo->base.block_length,
			     (uint) keyinfo->base.block_length,
			     return_buffer);
  if (tmp == info->buff)
  {
    info->update|=HA_STATE_BUFF_SAVED;
    info->int_pos=page;
  }
  else
  {
    info->update&= ~HA_STATE_BUFF_SAVED;
    if (tmp)
      info->int_pos=page;
    else
    {
      info->int_pos=NI_POS_ERROR;
      DBUG_PRINT("error",("Got errno: %d from key_cache_read",my_errno));
      my_errno=HA_ERR_CRASHED;
    }
  }
  return tmp;
} /* _ni_fetch_keypage */


	/* Write a key-page on disk */

int _ni_write_keypage(register N_INFO *info, register N_KEYDEF *keyinfo,
		      my_off_t page, uchar *buff)
{
  reg3 uint length;
#ifndef QQ					/* Safety check */
  if (page < info->s->base.keystart ||
      page+keyinfo->base.block_length > info->s->state.key_file_length ||
      page & (nisam_block_size-1))
  {
    DBUG_PRINT("error",("Trying to write outside key region: %lu",
			(long) page));
    my_errno=EINVAL;
    return(-1);
  }
  DBUG_PRINT("page",("write page at: %lu",(long) page,buff));
  DBUG_DUMP("buff",(byte*) buff,getint(buff));
#endif

  if ((length=keyinfo->base.block_length) > IO_SIZE*2 &&
       info->s->state.key_file_length != page+length)
    length= ((getint(buff)+IO_SIZE-1) & (uint) ~(IO_SIZE-1));
#ifdef HAVE_purify
  {
    length=getint(buff);
    bzero((byte*) buff+length,keyinfo->base.block_length-length);
    length=keyinfo->base.block_length;
  }
#endif
  return (key_cache_write(info->s->kfile,page,(byte*) buff,length,
			 (uint) keyinfo->base.block_length,
			 (int) (info->lock_type != F_UNLCK)));
} /* ni_write_keypage */


	/* Remove page from disk */

int _ni_dispose(register N_INFO *info, N_KEYDEF *keyinfo, my_off_t pos)
{
  uint keynr= (uint) (keyinfo - info->s->keyinfo);
  ulong old_link;				/* ulong is ok here */
  DBUG_ENTER("_ni_dispose");

  old_link=info->s->state.key_del[keynr];
  info->s->state.key_del[keynr]=pos;
  DBUG_RETURN(key_cache_write(info->s->kfile,pos,(byte*) &old_link,
			      sizeof(long),
			      (uint) keyinfo->base.block_length,
			      (int) (info->lock_type != F_UNLCK)));
} /* _ni_dispose */


	/* Make new page on disk */

ulong _ni_new(register N_INFO *info, N_KEYDEF *keyinfo)
{
  uint keynr= (uint) (keyinfo - info->s->keyinfo);
  my_off_t pos;
  DBUG_ENTER("_ni_new");

  if ((pos=info->s->state.key_del[keynr]) == NI_POS_ERROR)
  {
    if (info->s->state.key_file_length >= info->s->base.max_key_file_length)
    {
      my_errno=HA_ERR_INDEX_FILE_FULL;
      DBUG_RETURN(NI_POS_ERROR);
    }
    pos=info->s->state.key_file_length;
    info->s->state.key_file_length+= keyinfo->base.block_length;
  }
  else
  {
    if (!key_cache_read(info->s->kfile,pos,
			(byte*) &info->s->state.key_del[keynr],
			(uint) sizeof(long),
			(uint) keyinfo->base.block_length,0))
      pos= NI_POS_ERROR;
  }
  DBUG_PRINT("exit",("Pos: %d",pos));
  DBUG_RETURN(pos);
} /* _ni_new */
