/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Quicker interface to read & write. Used with my_nosys.h */

#include "mysys_priv.h"
#include "my_nosys.h"


uint my_quick_read(File Filedes,byte *Buffer,uint Count,myf MyFlags)
{
  uint readbytes;

  if ((readbytes = (uint) read(Filedes, Buffer, Count)) != Count)
  {
    my_errno=errno;
    return readbytes;
  }
  return (MyFlags & (MY_NABP | MY_FNABP)) ? 0 : readbytes;
}


uint my_quick_write(File Filedes,const byte *Buffer,uint Count)
{
  if ((uint) write(Filedes,Buffer,Count) != Count)
  {
    my_errno=errno;
    return (uint) -1;
  }
  return 0;
}

ulong my_quick_seek(File fd,ulong pos,int whence)
{
  off_t newpos=lseek(fd, (off_t) pos, whence);
  if ((off_t) newpos == (off_t) -1)
  {
    my_errno=errno;
  }
  return (ulong) newpos;
}
