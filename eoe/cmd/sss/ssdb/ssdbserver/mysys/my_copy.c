/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#define USES_TYPES		/* sys/types is included */
#include "mysys_priv.h"
#include <sys/stat.h>
#include <m_string.h>
#if defined(HAVE_SYS_UTIME_H)
#include <sys/utime.h>
#elif defined(HAVE_UTIME_H)
#include <utime.h>
#elif !defined(HPUX)
#include <time.h>
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#endif


	/*
	  Ordinary ownership and accesstimes are copied from 'from-file'
	  if MyFlags & MY_HOLD_ORIGINAL_MODES is set and to-file exists then
	  the modes of to-file isn't changed
	  Dont set MY_FNABP or MY_NABP bits on when calling this function !
	  */

int my_copy(const char *from, const char *to, myf MyFlags)
{
  uint Count;
  int new_file_stat;
  File from_file,to_file;
  char buff[IO_SIZE];
  struct stat stat_buff,new_stat_buff;
  DBUG_ENTER("my_copy");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

  from_file=to_file= -1;
  new_file_stat=0;
  if (MyFlags & MY_HOLD_ORIGINAL_MODES)		/* Copy stat if possible */
    new_file_stat=stat((char*) to, &new_stat_buff);

  if ((from_file=my_open(from,O_RDONLY,MyFlags)) >= 0)
  {
    if (stat(from,&stat_buff))
    {
      my_errno=errno;
      goto err;
    }
    if (MyFlags & MY_HOLD_ORIGINAL_MODES && !new_file_stat)
      memcpy((byte*) &stat_buff,(byte*) &new_stat_buff,sizeof(stat_buff));
    if ((to_file=  my_create(to,(int) stat_buff.st_mode,
			     O_WRONLY | O_TRUNC | O_BINARY,
			     MyFlags)) < 0)
      goto err;

    while ((Count=my_read(from_file,buff,IO_SIZE,MyFlags)) != 0)
	if (Count == (uint) -1 ||
	    my_write(to_file,buff,Count,MYF(MyFlags | MY_NABP)))
	goto err;

    if (my_close(from_file,MyFlags) | my_close(to_file,MyFlags))
      DBUG_RETURN(-1);				/* Error on close */

    /* Copy modes if possible */

    if (MyFlags & MY_HOLD_ORIGINAL_MODES && new_file_stat)
	DBUG_RETURN(0);			/* File copyed but not stat */
    VOID(chmod(to, stat_buff.st_mode & 07777)); /* Copy modes */
#if !defined(MSDOS) && !defined(__WIN32__) && !defined(__EMX__)
    VOID(chown(to, stat_buff.st_uid,stat_buff.st_gid)); /* Copy ownership */
#endif
#if !defined(VMS) && !defined(__ZTC__)
    if (MyFlags & MY_COPYTIME)
    {
      struct utimbuf timep;
      timep.actime  = stat_buff.st_atime;
      timep.modtime = stat_buff.st_mtime;
      VOID(utime((char*) to, &timep)); /* last accessed and modified times */
    }
#endif
    DBUG_RETURN(0);
  }

err:
  if (from_file >= 0) VOID(my_close(from_file,MyFlags));
  if (to_file >= 0)   VOID(my_close(to_file,MyFlags));
  DBUG_RETURN(-1);
} /* my_copy */
