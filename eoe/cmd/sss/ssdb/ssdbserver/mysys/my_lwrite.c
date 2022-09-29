/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"

	/* Write a chunk of bytes to a file */

uint32 my_lwrite(int Filedes, const byte *Buffer, uint32 Count, myf MyFlags)
{
  uint32 writenbytes;
  DBUG_ENTER("my_lwrite");
  DBUG_PRINT("my",("Fd: %d  Buffer: %lx  Count: %ld  MyFlags: %d",
		   Filedes, Buffer, Count, MyFlags));

  /* Temp hack to get count to int32 while write wants int */
  if ((writenbytes = (uint32) write(Filedes, Buffer, (size_t) Count)) != Count)
  {
    my_errno=errno;
    if (writenbytes == (uint32) -1 || MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),errno);
      }
      DBUG_RETURN((uint32) -1);		/* Return med felkod */
    }
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Ok vid l{sning */
  DBUG_RETURN(writenbytes);
} /* my_lwrite */
