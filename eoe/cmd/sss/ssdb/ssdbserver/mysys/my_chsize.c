/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"
#include "m_string.h"

	/* Change size of file.  Truncate file if shorter,	*/
	/* else expand with zero.				*/

int my_chsize(File fd, my_off_t newlength, myf MyFlags)
{
  DBUG_ENTER("my_chsize");
  DBUG_PRINT("my",("fd: %d  length: %lu  MyFlags: %d",fd,(ulong) newlength,
		   MyFlags));

#ifdef HAVE_CHSIZE
  if (chsize(fd,(off_t) newlength))
  {
    DBUG_PRINT("error",("errno: %d",errno));
    my_errno=errno;
    if (MyFlags & MY_WME)
      my_error(EE_CANT_CHSIZE,MYF(ME_BELL+ME_WAITTANG),errno);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
#else
  /* if file is shorter, expand with null, else fill unused part with null */
  {
    my_off_t oldsize;
    char buff[IO_SIZE];

    bzero(buff,IO_SIZE);
    oldsize = my_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME+MY_FAE));

#ifdef HAVE_FTRUNCATE
    if (oldsize > newlength)
    {
      if (ftruncate(fd, (off_t) newlength))
      {
	my_errno=errno;
	DBUG_PRINT("error",("errno: %d",errno));
	if (MyFlags & MY_WME)
	  my_error(EE_CANT_CHSIZE, MYF(ME_BELL+ME_WAITTANG), errno);
	DBUG_RETURN(1);
      }
      DBUG_RETURN(0);
    }
#else
    if (oldsize > newlength)
    {				/* Fill diff with null */
      VOID(my_seek(fd, newlength, MY_SEEK_SET, MYF(MY_WME+MY_FAE)));
      swap(long, newlength, oldsize);
    }
#endif
    while (newlength-oldsize > IO_SIZE)
    {
      if (my_write(fd,(byte*) buff,IO_SIZE,MYF(MY_NABP)))
	goto err;
      oldsize+= IO_SIZE;
    }
    if (my_write(fd,(byte*) buff,(uint) (newlength-oldsize),MYF(MY_NABP)))
      goto err;
    DBUG_RETURN(0);
  err:
    if (MyFlags & MY_WME)
      my_error(EE_CANT_CHSIZE,MYF(ME_BELL+ME_WAITTANG),my_errno);
    DBUG_PRINT("error",("errno: %d",my_errno));
    DBUG_RETURN(1);
  }
#endif
}				/* my_chsize */
