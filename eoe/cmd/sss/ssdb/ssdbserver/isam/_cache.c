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

/* Functions for read record cacheing with nisam */
/* Used instead of my_b_read() to allow for no-cacheed seeks */

#include "isamdef.h"

#define READING_NEXT	1
#define READING_HEADER	2

	/* Copy block from cache if it`s in it. If re_read_if_possibly is */
	/* set read to cache (if after current file-position) else read to */
	/* buff								  */

int _ni_read_cache(IO_CACHE *info, byte *buff, ulong pos, uint length, int flag)
{
  uint read_length,in_buff_length;
  ulong offset;
  char *in_buff_pos;

  if (pos < info->pos_in_file)
  {
    read_length= (uint) min((ulong) length,(ulong) (info->pos_in_file-pos));
    info->seek_not_done=1;
    VOID(my_seek(info->file,pos,MY_SEEK_SET,MYF(0)));
    if (my_read(info->file,buff,read_length,MYF(MY_NABP)))
      return 1;
    if (!(length-=read_length))
      return 0;
    pos+=read_length;
    buff+=read_length;
  }
  if ((offset=pos - info->pos_in_file) <
      (ulong) (info->rc_end - info->rc_request_pos))
  {
    in_buff_pos=info->rc_request_pos+(uint) offset;
    in_buff_length= min(length,(uint) (info->rc_end-in_buff_pos));
    memcpy(buff,info->rc_request_pos+(uint) offset,(size_t) in_buff_length);
    if (!(length-=in_buff_length))
      return 0;
    pos+=in_buff_length;
    buff+=in_buff_length;
  }
  else
    in_buff_length=0;
  if (flag & READING_NEXT)
  {
    if (pos != ((info)->pos_in_file +
		(uint) ((info)->rc_end - (info)->rc_request_pos)))
    {
      info->pos_in_file=pos;				/* Force start here */
      info->rc_pos=info->rc_end=info->rc_request_pos;	/* Everything used */
      info->seek_not_done=1;
    }
    else
      info->rc_pos=info->rc_end;			/* All block used */
    if (!(*info->read_function)(info,buff,length))
      return 0;
    if (!(flag & READING_HEADER) || info->error == -1 ||
	(uint) info->error+in_buff_length < 3)
      return 1;
    bzero(buff+info->error,BLOCK_INFO_HEADER_LENGTH - in_buff_length -
	  (uint) info->error);
    return 0;
  }
  info->seek_not_done=1;
  VOID(my_seek(info->file,pos,MY_SEEK_SET,MYF(0)));
  if ((read_length=my_read(info->file,buff,length,MYF(0))) == length)
    return 0;
  if (!(flag & READING_HEADER) || (int) read_length == -1 ||
      read_length+in_buff_length < 3)
    return 1;
  bzero(buff+read_length,BLOCK_INFO_HEADER_LENGTH - in_buff_length -
	read_length);
  return 0;
} /* _ni_read_cache */
