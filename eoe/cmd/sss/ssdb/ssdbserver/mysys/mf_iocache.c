/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Cashing of files with only does (sequential) read or writes of fixed-
  length records. A read isn't allowed to go over file-length. A read is ok
  if it ends at file-length and next read can try to read after file-length
  (and get a EOF-error).
  Possibly use of asyncronic io.
  macros for read and writes for faster io.
  Used instead of FILE when reading or writing hole files.
  This shall make mf_rec_cache obsolite.
  One can change info->pos_in_file to a higer value to skipp bytes in file if
  also info->rc_pos is set to info->rc_end.
*/

#include "mysys_priv.h"
#include <m_string.h>
#ifdef HAVE_AIOWAIT
#include "mysys_err.h"
#include <errno.h>
static void my_aiowait(my_aio_result *result);
#endif


	/* if cachesize == 0 then use default cachesize (from s-file) */
	/* returns 0 if we have enough memory */

int init_io_cache(IO_CACHE *info, File file, uint cachesize,
		  enum cache_type type, my_off_t seek_offset,
		  pbool use_async_io, myf cache_myflags)
{
  uint min_cache;
  DBUG_ENTER("init_io_cache");

  info->file=file;
  if (!cachesize)
    if (! (cachesize= my_default_record_cache_size))
      DBUG_RETURN(1);				/* No cache requested */
  min_cache=use_async_io ? IO_SIZE*4 : IO_SIZE*2;
  if (type == READ_CACHE)
  {						/* Assume file isn't growing */
    my_off_t file_pos,end_of_file;
    file_pos=my_tell(file,MYF(0));
    end_of_file=my_seek(file,0L,MY_SEEK_END,MYF(0));
    if (end_of_file < seek_offset)
      end_of_file=seek_offset;
    VOID(my_seek(file,file_pos,MY_SEEK_SET,MYF(0)));
    if ((my_off_t) cachesize > end_of_file-seek_offset+IO_SIZE*2-1)
    {
      cachesize=(uint) (end_of_file-seek_offset)+IO_SIZE*2-1;
      use_async_io=0;				/* No nead to use async */
    }
  }

  for (;;)
  {
    cachesize=(uint) ((ulong) (cachesize + min_cache-1) &
		      (ulong) ~(min_cache-1));
    if (cachesize < min_cache)
      cachesize = min_cache;
    if ((info->buffer=
	 (byte*) my_malloc(cachesize,
			   MYF((cache_myflags & ~ MY_WME) |
			       (cachesize == min_cache ? MY_WME : 0)))) != 0)
      break;					/* Enough memory found */
    if (cachesize == min_cache)
      DBUG_RETURN(2);				/* Can't alloc cache */
    cachesize= (uint) ((long) cachesize*3/4);	/* Try with less memory */
  }
  info->pos_in_file=seek_offset;
  info->read_length=info->buffer_length=cachesize;
  info->seek_not_done=1;			/* Seek not done */
  info->myflags=cache_myflags & ~(MY_NABP | MY_FNABP);
  info->rc_request_pos=info->rc_pos=info->buffer;

  if (type == READ_CACHE)
  {
    info->rc_end=info->buffer;			/* Nothing in cache */
  }
  else
  {
    info->rc_end=info->buffer+info->buffer_length- (seek_offset & (IO_SIZE-1));
  }
  info->end_of_file=MY_FILEPOS_ERROR;		/* May be changed by user */
  info->type=type;
  info->error=0;
  info->read_function=_my_b_read;
#ifdef HAVE_AIOWAIT
  if (use_async_io && ! my_disable_async_io)
  {
    DBUG_PRINT("info",("Using async io"));
    info->read_length/=2;
    info->read_function=_my_b_async_read;
  }
  info->inited=info->aio_result.pending=0;
#endif
  DBUG_RETURN(0);
}						/* init_io_cache */


	/* Wait until current request is ready */

#ifdef HAVE_AIOWAIT
static void my_aiowait(my_aio_result *result)
{
  if (result->pending)
  {
    struct aio_result_t *tmp;
    for (;;)
    {
      if ((int) (tmp=aiowait((struct timeval *) 0)) == -1)
      {
	if (errno == EINTR)
	  continue;
	DBUG_PRINT("error",("No aio request, error: %d",errno));
	result->pending=0;			/* Assume everythings is ok */
	break;
      }
      ((my_aio_result*) tmp)->pending=0;
      if ((my_aio_result*) tmp == result)
	break;
    }
  }
  return;
}
#endif

	/* Use this to reset cache to start or other type */
	/* Some simple optimizing is done when reinit in current buffer */

void reinit_io_cache(IO_CACHE *info, enum cache_type type, my_off_t seek_offset,
		     pbool use_async_io)
{
  DBUG_ENTER("reinit_io_cache");

#ifdef BREAK_SQL
  if (info->type == type && seek_offset >= info->pos_in_file &&
      seek_offset <= info->pos_in_file +
      (uint) (info->rc_end-info->rc_request_pos))
  {						/* use current buffer */
    info->rc_pos=info->rc_request_pos+(seek_offset-info->pos_in_file);
    DBUG_VOID_RETURN;
  }
#endif
  info->pos_in_file=seek_offset;
  info->seek_not_done=1;			/* Seek not done */
  info->rc_request_pos=info->rc_pos=info->buffer;
  if (type == READ_CACHE)
  {
    info->rc_end=info->buffer;			/* Nothing in cache */
  }
  else
  {
    info->rc_end=info->buffer+info->buffer_length- (seek_offset & (IO_SIZE-1));
    info->end_of_file=MY_FILEPOS_ERROR;		/* May be changed by user */
  }
  info->type=type;
  info->error=0;
  info->read_function=_my_b_read;
#ifdef HAVE_AIOWAIT
  my_aiowait(&info->aio_result);		/* Wait for outstanding req */
  if (use_async_io && ! my_disable_async_io &&
      ((ulong) info->buffer_length < (ulong) (info->end_of_file - seek_offset)))
  {
    info->read_length=info->buffer_length/2;
    info->read_function=_my_b_async_read;
  }
  info->inited=0;
#endif
  DBUG_VOID_RETURN;
} /* init_io_cache */



	/* Read buffered. Returns 1 if can't read requested characters */
	/* Returns 0 if record read */

int _my_b_read(register IO_CACHE *info, byte *Buffer, uint Count)

						/* Read record here */

{
  uint length,diff_length,left_length;
  my_off_t max_length, pos_in_file;

  memcpy(Buffer,info->rc_pos,
	 (size_t) (left_length=(uint) (info->rc_end-info->rc_pos)));
  Buffer+=left_length;
  Count-=left_length;
  pos_in_file=info->pos_in_file+(uint) (info->rc_end - info->buffer);
  if (info->seek_not_done)
  {					/* File touched, do seek */
    VOID(my_seek(info->file,pos_in_file,MY_SEEK_SET,MYF(0)));
    info->seek_not_done=0;
  }
  diff_length=(uint) (pos_in_file & (IO_SIZE-1));
  if (Count >= (uint) (IO_SIZE+(IO_SIZE-diff_length)))
  {					/* Fill first intern buffer */
    uint read_length;
    if (info->end_of_file == pos_in_file)
    {					/* End of file */
      info->error=(int) left_length;
      return 1;
    }
    length=(Count & (uint) ~(IO_SIZE-1))-diff_length;
    if ((read_length=my_read(info->file,Buffer,(uint) length,info->myflags))
	!= (uint) length)
    {
      info->error= read_length == (uint) -1 ? -1 :
	(int) (read_length+left_length);
      return 1;
    }
    Count-=length;
    Buffer+=length;
    pos_in_file+=length;
    left_length+=length;
    diff_length=0;
  }
  max_length=info->end_of_file - pos_in_file;
  if (max_length > info->read_length-diff_length)
    max_length=info->read_length-diff_length;
  if (!(length=(int) max_length) ||
      (length=my_read(info->file,info->buffer,(uint) max_length,info->myflags))
      < Count || length == (uint) -1)
  {					/* Got error */
    if (length > 0)
      memcpy(Buffer,info->buffer,(size_t) length);
    info->error= length == (uint) -1 ? -1 : (int) (length+left_length);
    return 1;
  }
  info->rc_pos=info->buffer+Count;
  info->rc_end=info->buffer+length;
  info->pos_in_file=pos_in_file;
  memcpy(Buffer,info->buffer,(size_t) Count);
  return 0;
}


#ifdef HAVE_AIOWAIT

int _my_b_async_read(register IO_CACHE *info, byte *Buffer, uint Count)

						/* Read record here */

{
  uint length,read_length,diff_length,left_length,use_length,org_Count;
  my_off_t max_length;
  my_off_t next_pos_in_file;
  byte *read_buffer;

  memcpy(Buffer,info->rc_pos,
	 (size_t) (left_length=(uint) (info->rc_end-info->rc_pos)));
  Buffer+=left_length;
  org_Count=Count;
  Count-=left_length;

  if (info->inited)
  {						/* wait for read block */
    info->inited=0;				/* No more block to read */
    my_aiowait(&info->aio_result);		/* Wait for outstanding req */
    if (info->aio_result.result.aio_errno)
    {
      if (info->myflags & MY_WME)
	my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(info->file),
		 info->aio_result.result.aio_errno);
      my_errno=info->aio_result.result.aio_errno;
      info->error= -1;
      return(1);
    }
    if (! (read_length = (uint) info->aio_result.result.aio_return) ||
	read_length == (uint) -1)
    {
      my_errno=0;				/* For testing */
      info->error= (read_length == (uint) -1 ? -1 :
		    (int) (read_length+left_length));
      return(1);
    }
    info->pos_in_file+=(uint) (info->rc_end - info->rc_request_pos);

    if (info->rc_request_pos != info->buffer)
      info->rc_request_pos=info->buffer;
    else
      info->rc_request_pos=info->buffer+info->read_length;
    info->rc_pos=info->rc_request_pos;
    next_pos_in_file=info->aio_read_pos+read_length;

	/* Check if pos_in_file is changed
	   (_ni_read_cache may have skipped some bytes) */

    if (info->aio_read_pos < info->pos_in_file)
    {						/* Fix if skipped bytes */
      if (info->aio_read_pos + read_length < info->pos_in_file)
      {
	read_length=0;				/* Skipp block */
	next_pos_in_file=info->pos_in_file;
      }
      else
      {
	my_off_t offset= (info->pos_in_file - info->aio_read_pos);
	info->pos_in_file=info->aio_read_pos; /* Whe are here */
	info->rc_pos=info->rc_request_pos+offset;
	read_length-=offset;			/* Bytes left from rc_pos */
      }
    }
#ifndef DBUG_OFF
    if (info->aio_read_pos > info->pos_in_file)
    {
      my_errno=EINVAL;
      return(info->read_length= -1);
    }
#endif
	/* Copy found bytes to buffer */
    length=min(Count,read_length);
    memcpy(Buffer,info->rc_pos,(size_t) length);
    Buffer+=length;
    Count-=length;
    left_length+=length;
    info->rc_end=info->rc_pos+read_length;
    info->rc_pos+=length;
  }
  else
    next_pos_in_file=(info->pos_in_file+ (uint)
		      (info->rc_end - info->rc_request_pos));

	/* If reading large blocks, or first read or read with skipp */
  if (Count)
  {
    VOID(my_seek(info->file,next_pos_in_file,MY_SEEK_SET,MYF(0)));
    read_length=IO_SIZE*2- (uint) (next_pos_in_file & (IO_SIZE-1));
    if (Count < read_length)
    {					/* Small block, read to cache */
      if ((read_length=my_read(info->file,info->rc_request_pos,
			       read_length, info->myflags)) == (uint) -1)
	return info->error= -1;
      use_length=min(Count,read_length);
      memcpy(Buffer,info->rc_request_pos,(size_t) use_length);
      info->rc_pos=info->rc_request_pos+Count;
      info->rc_end=info->rc_request_pos+read_length;
      info->pos_in_file=next_pos_in_file;	/* Start of block in cache */
      next_pos_in_file+=read_length;

      if (Count != use_length)
      {					/* Didn't find hole block */
	if (info->myflags & (MY_WME | MY_FAE | MY_FNABP) && Count != org_Count)
	  my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		   my_filename(info->file),my_errno);
	info->error=(int) (read_length+left_length);
	return 1;
      }
    }
    else
    {						/* Big block, don't cache it */
      if ((read_length=my_read(info->file,Buffer,(uint) Count,info->myflags))
	  != Count)
      {
	info->error= read_length == (uint)  -1 ? -1 : read_length+left_length;
	return 1;
      }
      info->rc_pos=info->rc_end=info->rc_request_pos;
      info->pos_in_file=(next_pos_in_file+=Count);
    }
  }

	/* Read next block with asyncronic io */
  max_length=info->end_of_file - next_pos_in_file;
  diff_length=(next_pos_in_file & (IO_SIZE-1));

  if (max_length > (my_off_t) info->read_length - diff_length)
    max_length= (my_off_t) info->read_length - diff_length;
  if (info->rc_request_pos != info->buffer)
    read_buffer=info->buffer;
  else
    read_buffer=info->buffer+info->read_length;
  info->aio_read_pos=next_pos_in_file;
  if (max_length)
  {
    info->aio_result.result.aio_errno=AIO_INPROGRESS;	/* Marker for test */
    DBUG_PRINT("aioread",("filepos: %ld  length: %ld",
			  (ulong) next_pos_in_file,(ulong) max_length));
    if (aioread(info->file,read_buffer,(int) max_length,
		(my_off_t) next_pos_in_file,MY_SEEK_SET,
		&info->aio_result.result))
    {						/* Skipp async io */
      my_errno=errno;
      DBUG_PRINT("error",("got error: %d, aio_result: %d from aioread, async skipped",
			  errno, info->aio_result.result.aio_errno));
      if (info->rc_request_pos != info->buffer)
      {
	bmove(info->buffer,info->rc_request_pos,
	      (uint) (info->rc_end - info->rc_pos));
	info->rc_request_pos=info->buffer;
	info->rc_pos-=info->read_length;
	info->rc_end-=info->read_length;
      }
      info->read_length=info->buffer_length;	/* Use hole buffer */
      info->read_function=_my_b_read;		/* Use normal IO_READ next */
    }
    else
      info->inited=info->aio_result.pending=1;
  }
  return 0;					/* Block read, async in use */
} /* _my_b_async_read */
#endif


/* Read one byte when buffer is empty */

int _my_b_get(IO_CACHE *info)
{
  char buff;
  if ((*(info)->read_function)(info,&buff,1))
    return my_b_EOF;
  return (int) (uchar) buff;
}

	/* Returns != 0 if error on write */

int _my_b_write(register IO_CACHE *info, const byte *Buffer, uint Count)
{
  uint rest_length,length;

  rest_length=(uint) (info->rc_end - info->rc_pos);
  memcpy(info->rc_pos,Buffer,(size_t) rest_length);
  Buffer+=rest_length;
  Count-=rest_length;
  info->rc_pos+=rest_length;
  if (flush_io_cache(info))
    return 1;
  if (Count >= IO_SIZE)
  {					/* Fill first intern buffer */
    length=Count & (uint) ~(IO_SIZE-1);
    if (info->seek_not_done)
    {					/* File touched, do seek */
      VOID(my_seek(info->file,info->pos_in_file,MY_SEEK_SET,MYF(0)));
      info->seek_not_done=0;
    }
    if (my_write(info->file,Buffer,(uint) length,info->myflags | MY_NABP))
      return info->error= -1;
    Count-=length;
    Buffer+=length;
    info->pos_in_file+=length;
  }
  memcpy(info->rc_pos,Buffer,(size_t) Count);
  info->rc_pos+=Count;
  return 0;
}


	/* Flush write cache */

int flush_io_cache(IO_CACHE *info)
{
  uint length;
  DBUG_ENTER("flush_io_cache");

  if (info->type == WRITE_CACHE)
  {
    if (info->rc_pos != info->buffer)
    {
      length=(uint) (info->rc_pos - info->buffer);
      if (info->seek_not_done)
      {					/* File touched, do seek */
	VOID(my_seek(info->file,info->pos_in_file,MY_SEEK_SET,MYF(0)));
	info->seek_not_done=0;
      }
      info->rc_pos=info->buffer;
      info->pos_in_file+=length;
      info->rc_end=(info->buffer+info->buffer_length-
		    (info->pos_in_file & (IO_SIZE-1)));
      if (my_write(info->file,info->buffer,length,info->myflags | MY_NABP))
	DBUG_RETURN((info->error= -1));
      DBUG_RETURN(0);
    }
  }
#ifdef HAVE_AIOWAIT
  else
  {
    my_aiowait(&info->aio_result);	/* Wait for outstanding req */
    info->inited=0;
  }
#endif
  info->seek_not_done=1;		/* After flush - seek may be needed */
  DBUG_RETURN(0);
}


int end_io_cache(IO_CACHE *info)
{
  int error=0;
  DBUG_ENTER("end_io_cache");
  if (info->buffer)
  {
    error=flush_io_cache(info);
    my_free((gptr) info->buffer,MYF(MY_WME));
    info->buffer=info->rc_pos=NullS;
  }
  DBUG_RETURN(error);
} /* end_io_cache */
