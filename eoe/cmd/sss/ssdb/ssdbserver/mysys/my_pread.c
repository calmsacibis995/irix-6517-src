/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>

	/* Read a chunk of bytes from a file  */

uint my_pread(File Filedes, byte *Buffer, uint Count, my_off_t offset,
	      myf MyFlags)
{
  uint readbytes;
  DBUG_ENTER("my_pread");
  DBUG_PRINT("my",("Fd: %d  Seek: %lu  Buffer: %lx  Count: %u  MyFlags: %d",
		   Filedes, (ulong) offset, Buffer, Count, MyFlags));

  for (;;)
  {
#ifndef __WIN32__
    errno=0;					/* Linux doesn't reset this */
#endif
#ifndef HAVE_PREAD
    readbytes= (uint) -1;
    if (lseek(Filedes, offset, MY_SEEK_SET) == -1L ||
	(readbytes = (uint) read(Filedes, Buffer, Count)) != Count)
#else
      if ((readbytes = (uint) pread(Filedes, Buffer, Count, offset)) != Count)
#endif
      {
	my_errno=errno;
	DBUG_PRINT("warning",("Read only %ld bytes off %ld from %d, errno: %d",
			      readbytes,Count,Filedes,my_errno));
#ifdef THREAD
	if (readbytes == 0 && errno == EINTR)
	  continue;				/* Interrupted */
#endif
	if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
	{
	  if ((int) readbytes == -1)
	    my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		     my_filename(Filedes),my_errno);
	  else if (MyFlags & (MY_NABP | MY_FNABP))
	    my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		     my_filename(Filedes),my_errno);
	}
	if ((int) readbytes == -1 || (MyFlags & (MY_FNABP | MY_NABP)))
	  DBUG_RETURN(MY_FILE_ERROR);	/* Return with error */
      }
    if (MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN(0);			/* Ok vid l{sning */
    DBUG_RETURN(readbytes); /* purecov: inspected */
  }
} /* my_pread */


	/* Write a chunk of bytes to a file */

uint my_pwrite(int Filedes, const byte *Buffer, uint Count, my_off_t offset,
	       myf MyFlags)
{
  uint writenbytes,errors;
  ulong written;
  DBUG_ENTER("my_pwrite");
  DBUG_PRINT("my",("Fd: %d  Seek: %lu  Buffer: %lx  Count: %d  MyFlags: %d",
		   Filedes, (ulong) offset,Buffer, Count, MyFlags));
  errors=0; written=0L;

  for (;;)
  {
#ifndef HAVE_PREAD
    writenbytes= (uint) -1;
    if (lseek(Filedes, offset, MY_SEEK_SET) != -1L &&
	(writenbytes = (uint) write(Filedes, Buffer, Count)) == Count)
#else
    if ((writenbytes = (uint) pwrite(Filedes, Buffer, Count,offset)) == Count)
#endif
      break;
    if ((int) writenbytes != -1)
    {					/* Safegueard */
      written+=writenbytes;
      Buffer+=writenbytes;
      Count-=writenbytes;
      offset+=writenbytes;
    }
    my_errno=errno;
    DBUG_PRINT("error",("Write only %d bytes",writenbytes));
#ifndef NO_BACKGROUND
#ifdef THREAD
    if (my_thread_var->abort)
      MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
#endif
    if (my_errno == ENOSPC && (MyFlags & MY_WAIT_IF_FULL))
    {
      if (!(errors++ % MY_WAIT_GIVE_USER_A_MESSAGE))
	my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
		 my_filename(Filedes));
      VOID(sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC));
      continue;
    }
    if (writenbytes == 0 && my_errno == EINTR ||
	writenbytes > 0 && (uint) writenbytes != (uint) -1)
      continue;					/* Retry */
#endif
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),my_errno);
      }
      DBUG_RETURN(MY_FILE_ERROR);		/* Error on read */
    }
    else
      break;					/* Return bytes written */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Want only errors */
  DBUG_RETURN(writenbytes+written); /* purecov: inspected */
} /* my_write */
