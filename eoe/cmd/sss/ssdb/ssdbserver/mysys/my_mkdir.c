/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __WIN32__
#include <direct.h>
#endif

int my_mkdir(const char *dir, int Flags, myf MyFlags)
{
  DBUG_ENTER("my_dir");
  DBUG_PRINT("enter",("dir: %s",dir));

#ifdef __WIN32__
  if (mkdir(dir))
#else
  if (mkdir((char*) dir, Flags))
#endif
  {
    my_errno=errno;
    DBUG_PRINT("error",("error %d when creating direcory %s",my_errno,dir));
    if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
      my_error(EE_CANT_MKDIR,  MYF(ME_BELL+ME_WAITTANG), dir, my_errno);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}
