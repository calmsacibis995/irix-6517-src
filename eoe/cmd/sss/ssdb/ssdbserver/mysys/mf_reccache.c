/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  This functions is to read a bunch of records in one read from a (locked?)
  database for quick access when reading in postion order.
  Uses asyncronic io if database is locked.
  init_record_cache() is to init cache_handler.
  read_cache_record() reads a record to buffert.
  end_record_cache() frees cache-memory.
  This is obsolite; Its only used with M-ISAM and P-ISAM.
 */

#include "mysys_priv.h"
#include <m_string.h>
#include "my_nosys.h"

#ifdef HAVE_AIOWAIT
#undef HAVE_AIOWAIT			/* Don't get new bugs in old code */
#endif

	/* if cachesize == 0 then use default cachesize (from s-file) */
	/* returns 0 if we have enough memory */

int init_record_cache(RECORD_CACHE *info, uint cachesize, File file, uint reclength, enum cache_type type, pbool use_async_io)
{
  uint count;
  DBUG_ENTER("init_record_cache");

  info->file=file;
  if (cachesize == 0)
    cachesize= my_default_record_cache_size;
  for (;;)
  {
    if ((count=((cachesize-1)/reclength & (uint) ~1)) <= 1)
    {
      info->rc_length=0;			/* Set for test if cacheing */
      info->rc_record_pos=MY_FILEPOS_ERROR;
      DBUG_RETURN(2);				/* No nead for cacheing */
    }
    info->rc_length=count*reclength;
    if ((info->rc_buff= (byte*) my_malloc(info->rc_length+1,MYF(0))) != 0)
      break;					/* Enough memory found */
    cachesize= (uint) ((long) cachesize*3/4);	/* Try with less memory */
  }
  info->rc_record_pos=0L;
  info->rc_seek=1;
  info->rc_end= (type == READ_CACHE ? info->rc_buff :
		 info->rc_buff+info->rc_length);
  info->rc_pos=info->rc_end;
  info->read_length=info->rc_length;
  info->reclength=reclength;
  info->end_of_file=MY_FILEPOS_ERROR;		/* May be changed by user */
  info->type=type;
  info->error=info->inited=0;
#ifdef HAVE_AIOWAIT
  if ((info->use_async_io=(int) use_async_io))
  {
    info->rc_request_pos=info->rc_buff;
    info->read_length/=2;
    info->rc_buff2=info->rc_buff+info->read_length;
  }
  info->inited=info->aio_result.pending=0;
#endif
  DBUG_RETURN(0);
} /* init_record_cache */


#ifdef HAVE_AIOWAIT
static void my_aiowait(result)
my_aio_result *result;
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
	tmp->aio_errno=errno;
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

	/* Returns 0 if record read */

int read_cache_record(register RECORD_CACHE *info, byte *to)

						/* Read record here */
{
  uint length;
  long max_length;

  for ( ;; )					/* Instead of tail-recursion */
  {
#ifdef MSDOS					/* MSDOS and segments !!! */
    if ((ulong) info->rc_pos+(ulong) info->reclength <= (ulong) info->rc_end)
#else
    if (info->rc_pos+info->reclength <= info->rc_end)
#endif
    {						/* Record in cache */
      memcpy(to,info->rc_pos,(size_t) info->reclength);
      info->rc_pos+=info->reclength;
      info->rc_record_pos+=info->reclength;
      return 0;
    }
    if (info->rc_pos != info->rc_end)
    {
      info->error=0;				/* Not actual error */
      return 2;					/* End of file, chars left */
    }
#ifdef HAVE_AIOWAIT
    if (info->inited)
    {				/* wait for read block */
      info->inited=0;
      my_aiowait(&info->aio_result);
      if (info->aio_result.aio_errno)
      {
	my_errno=info->aio_result.aio_errno;
	return(info->aio_result.aio_errno);
      }
      if (! (length=info->aio_result.aio_return) || length == (uint) -1)
      {
	info->error=(int) length;
	return(1);
      }
      info->rc_pos=info->rc_request_pos;
      info->rc_end=info->rc_pos+length;
    }
    else
#endif
    {
      max_length=(long) (info->end_of_file - info->rc_record_pos);
      if (max_length > (long) info->read_length)
	max_length=(long) info->read_length;
      if (info->rc_seek)
      {						/* File touched, do seek */
	VOID(my_seek(info->file,info->rc_record_pos,MY_SEEK_SET,MYF(0)));
	info->rc_seek=0;
      }
      if ((length=my_read(info->file,info->rc_buff,(uint) max_length,
			   MYF(0))) == 0 ||
	  length == (uint) -1)
      {
	info->error= (int) length;			/* Got error */
	return 1;
      }
      info->rc_pos=info->rc_buff;
      info->rc_end=info->rc_buff+length;
#ifdef HAVE_AIOWAIT
      if (! info->use_async_io)
	continue;
#endif
    }

	/* Read next block with asyncronic io */
#ifdef HAVE_AIOWAIT
    max_length=info->end_of_file - info->rc_record_pos - length;
    if (max_length > (long) info->read_length)
      max_length=(long) info->read_length;
    if (info->rc_request_pos != info->rc_buff)
      info->rc_request_pos=info->rc_buff;
    else
      info->rc_request_pos=info->rc_buff2;
    info->aio_result.aio_errno=AIO_INPROGRESS;
    if (!aioread(info->file,info->rc_request_pos,max_length,
		 info->rc_record_pos+length,MY_SEEK_SET,
		 &info->aio_result.result))
    {							/* Skipp async io */
      info->inited=info->use_async_io=0;
      info->read_length=info->rc_length;		/* Use hole buffer */
    }
    else
      info->inited=info->aio_result.pending=1;
#endif
  }
} /* read_cache_record */


int end_record_cache(RECORD_CACHE *info)
{
  int error=0;
  DBUG_ENTER("end_record_cache");
  if (info->rc_buff)
  {
    if (info->type == WRITE_CACHE)
      error=flush_write_cache(info);
#ifdef HAVE_AIOWAIT
    else
      my_aiowait(&info->aio_result);
#endif
    my_free((gptr) info->rc_buff,MYF(MY_WME));
    info->rc_buff=NullS;
    info->rc_length=0;
  }
  DBUG_RETURN(error);
} /* end_record_cache */


	/* Returns != 0 if error on write */

int write_cache_record(register RECORD_CACHE *info, my_off_t filepos,
		       const byte *record, uint length)
{
  uint rest_length;
  if (!info->rc_length)			/* Write with no cache */
    return test(my_write(info->file,record,length,
			 MYF(MY_NABP | MY_WAIT_IF_FULL)));

#ifdef MSDOS				/* MSDOS and segments !!! */
  if ((ulong) info->rc_pos+(ulong) length > (ulong) info->rc_end)
#else
  if (info->rc_pos+length > info->rc_end)
#endif
  {
    rest_length=(uint) (info->rc_end - info->rc_pos);

    memcpy(info->rc_pos,record,(size_t) rest_length);
    record+=rest_length;
    length-=rest_length;
    info->rc_pos+=rest_length;
    info->rc_record_pos+=rest_length;
    if (flush_write_cache(info))
      return 1;
    info->inited=1;
    info->rc_pos=info->rc_buff;
    info->rc_record_pos=filepos;
  }
  if (length > info->rc_length)
  {					/* Probably using blobs */
    return test(my_write(info->file,record,length,
			 MYF(MY_NABP | MY_WAIT_IF_FULL)));
  }
  memcpy(info->rc_pos,record,(size_t) length);
  info->rc_pos+=length;
  info->rc_record_pos+=length;
  return 0;
} /* write_cache_record */


	/* Flush write cache */

int flush_write_cache(RECORD_CACHE *info)
{
  uint length;
  if (info->inited)
  {
    length=(uint) (info->rc_pos - info->rc_buff);
    if (info->rc_seek)
    {						/* File touched, do seek */
      VOID(my_seek(info->file,info->rc_record_pos-length,MY_SEEK_SET,MYF(0)));
      info->rc_seek=0;
    }
    info->inited=0;
    info->rc_pos=info->rc_end;
    return test(my_write(info->file,info->rc_buff,length,
			 MYF(MY_NABP | MY_WAIT_IF_FULL)));
  }
  else
    info->rc_seek=1;		/* We are here again after flush - seek may */
				/* be neaded */
  return 0;
} /* flush_write_cache */
