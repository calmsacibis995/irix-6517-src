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
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#endif

	/* Append a file to another */

int my_append(const char *from, const char *to, myf MyFlags)


				/* Dont set MY_FNABP or MY_NABP bits on
					   when calling this funktion */
{
  uint Count;
  File from_file,to_file;
  char buff[IO_SIZE];
  DBUG_ENTER("my_append");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

  from_file=to_file= -1;

  if ((from_file=my_open(from,O_RDONLY,MyFlags)) >= 0)
  {
    if ((to_file=my_open(to,O_APPEND | O_WRONLY,MyFlags)) >= 0)
    {
      while ((Count=my_read(from_file,buff,IO_SIZE,MyFlags)) != 0)
	if (Count == (uint) -1 ||
	    my_write(to_file,buff,Count,MYF(MyFlags | MY_NABP)))
	  goto err;
      if (my_close(from_file,MyFlags) | my_close(to_file,MyFlags))
	DBUG_RETURN(-1);				/* Error on close */
      DBUG_RETURN(0);
    }
  }
err:
  if (from_file >= 0) VOID(my_close(from_file,MyFlags));
  if (to_file >= 0)   VOID(my_close(to_file,MyFlags));
  DBUG_RETURN(-1);
}
