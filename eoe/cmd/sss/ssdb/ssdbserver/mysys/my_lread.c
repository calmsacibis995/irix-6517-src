/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"

	/* Read a chunk of bytes from a file  */

uint32 my_lread(int Filedes, byte *Buffer, uint32 Count, myf MyFlags)
				/* File descriptor */
				/* Buffer must be at least count bytes */
				/* Max number of bytes returnd */
				/* Flags on what to do on error */
{
  uint32 readbytes;
  DBUG_ENTER("my_lread");
  DBUG_PRINT("my",("Fd: %d  Buffer: %ld  Count: %ld  MyFlags: %d",
		   Filedes, Buffer, Count, MyFlags));

  /* Temp hack to get count to int32 while read wants int */
  if ((readbytes = (uint32) read(Filedes, Buffer, (size_t) Count)) != Count)
  {
    my_errno=errno;
    if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
    {
      if (readbytes == MY_FILE_ERROR)
	my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),errno);
      else
      if (MyFlags & (MY_NABP | MY_FNABP))
	my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),errno);
    }
    if (readbytes == MY_FILE_ERROR || MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN((uint32) -1);		/* Return med felkod */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Ok vid l{sning */
  DBUG_RETURN(readbytes);
} /* my_lread */
