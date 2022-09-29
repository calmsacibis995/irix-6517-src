/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Open a temporary file and cache it with io_cache. Delete it on close */

#include "mysys_priv.h"

	/* Open a cacheed tempfile by IO_CACHE */
	/* Should be used when no seeks are done (only reinit_io_buff) */
	/* Return 0 if file is opened ok */

int open_cacheed_file(IO_CACHE *cache, const char* dir, const char *prefix,
		      uint cache_size)
{
  File file;
  DBUG_ENTER("open_cacheed_file");

  cache->buffer=0;				/* Mark that not open */
  if (!(cache->file_name=my_tempnam(dir,prefix,MYF(MY_WME))))
    DBUG_RETURN(1);
  if ((file=my_create(cache->file_name,0,
		      (int) (O_RDWR | O_BINARY | O_TRUNC | O_TEMPORARY |
			     O_SHORT_LIVED),
		      MYF(MY_WME))) >= 0)
  {
#if O_TEMPORARY == 0 && !defined(CANT_DELETE_OPEN_FILES)
    VOID(my_delete(cache->file_name,MYF(MY_WME | ME_NOINPUT)));
#endif
    if (!init_io_cache(cache,file,cache_size,WRITE_CACHE,0L,0,
		      MYF(MY_WAIT_IF_FULL | MY_WME | MY_NABP)))
    {
      DBUG_RETURN(0);
    }
    VOID(my_close(file,MYF(0)));
#ifdef CANT_DELETE_OPEN_FILES
    VOID(my_delete(cache->file_name,MYF(0)));
#endif
    (*free)(cache->file_name);			/* my_tempnam uses malloc() */
  }
  DBUG_RETURN(1);
}


void close_cacheed_file(IO_CACHE *cache)
{
  DBUG_ENTER("close_cacheed_file");

  if (my_b_inited(cache))
  {
    VOID(end_io_cache(cache));
    VOID(my_close(cache->file,MYF(MY_WME)));
#ifdef CANT_DELETE_OPEN_FILES
    VOID(my_delete(cache->file_name,MYF(MY_WME | ME_NOINPUT)));
#endif
    (*free)(cache->file_name);			/* my_tempnam uses malloc() */
  }
  DBUG_VOID_RETURN;
}
