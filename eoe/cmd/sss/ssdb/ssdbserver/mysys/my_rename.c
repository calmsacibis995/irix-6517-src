/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#define USES_TYPES
#include "mysys_priv.h"
#include <my_dir.h>
#include "mysys_err.h"

	/* On unix rename deletes to file if it exists */

int my_rename(const char *from, const char *to, myf MyFlags)
{
  int error = 0;
  DBUG_ENTER("my_rename");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

#if defined(HAVE_FILE_VERSIONS)
  {				/* Check that there isn't a old file */
    int save_errno;
    MY_STAT my_stat_result;
    save_errno=my_errno;
    if (my_stat(to,&my_stat_result,MYF(0)))
    {
      my_errno=EEXIST;
      error= -1;
      if (MyFlags & MY_FAE+MY_WME)
	my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
      DBUG_RETURN(error);
    }
    my_errno=save_errno;
  }
#endif
#if defined(HAVE_RENAME)
  if (rename(from,to))
#else
  if (link(from, to) || unlink(from))
#endif
  {
    my_errno=errno;
    error = -1;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
  }
  DBUG_RETURN(error);
} /* my_rename */
