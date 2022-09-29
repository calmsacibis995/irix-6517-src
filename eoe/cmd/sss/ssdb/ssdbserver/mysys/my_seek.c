/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"

	/* Seek to position in file */
	/*ARGSUSED*/

my_off_t my_seek(File fd, my_off_t pos, int whence, myf MyFlags)
{
  reg1 my_off_t newpos;
  DBUG_ENTER("my_seek");
  DBUG_PRINT("my",("Fd: %d  Pos: %lu  Whence: %d  MyFlags: %d",
		   fd, (ulong) pos, whence, MyFlags));
  newpos=lseek(fd, pos, whence);
  if (newpos == MY_FILEPOS_ERROR)
  {
    my_errno=errno;
    DBUG_PRINT("error",("lseek: %lu, errno: %d",newpos,errno));
    DBUG_RETURN(MY_FILEPOS_ERROR);
  }
  DBUG_RETURN(newpos);
} /* my_seek */


	/* Tell current position of file */
	/* ARGSUSED */

my_off_t my_tell(File fd, myf MyFlags)
{
  my_off_t pos;
  DBUG_ENTER("my_tell");
  DBUG_PRINT("my",("Fd: %d  MyFlags: %d",fd, MyFlags));
#ifdef HAVE_TELL
  pos=tell(fd);
#else
  pos=lseek(fd, 0L, MY_SEEK_CUR);
#endif
  if (pos == (my_off_t) -1)
    my_errno=errno;
  DBUG_PRINT("exit",("pos: %lu",pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_tell */
