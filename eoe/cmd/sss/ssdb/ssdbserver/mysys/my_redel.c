/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#define USES_TYPES				/* sys/types is included */
#include "mysys_priv.h"
#include <my_dir.h>
#include "mysys_err.h"
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

	/* Rename with copy stat form old file */
	/* Copy stats from old file to new file, deletes orginal and */
	/* changes new file name to old file name */

int my_redel(const char *org_name, const char *tmp_name, myf MyFlags)
{
  DBUG_ENTER("my_redel");
  DBUG_PRINT("my",("org_name: '%s' tmp_name: '%s'  MyFlags: %d",
		   org_name,tmp_name,MyFlags));

  if (my_copystat(org_name,tmp_name,MyFlags) < 0 ||
      my_delete(org_name,MyFlags) ||
      my_rename(tmp_name,org_name,MyFlags))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
} /* my_redel */


	/* Copy stat from one file to another */
	/* Return -1 if can't get stat, 1 if wrong type of file */

int my_copystat(const char *from, const char *to, int MyFlags)
{
  struct stat statbuf;

  if (stat((char*) from, &statbuf))
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_STAT, MYF(ME_BELL+ME_WAITTANG),from,errno);
    return -1;				/* Can't get stat on input file */
  }
  if ((statbuf.st_mode & S_IFMT) != S_IFREG)
    return 1;
  VOID(chmod(to, statbuf.st_mode & 07777));		/* Copy modes */

#if !defined(MSDOS) && !defined(__WIN32__) && !defined(__EMX__)
  if (statbuf.st_nlink > 1 && MyFlags & MY_LINK_WARNING)
  {
    if (MyFlags & MY_LINK_WARNING)
      my_error(EE_LINK_WARNING,MYF(ME_BELL+ME_WAITTANG),from,statbuf.st_nlink);
  }
  VOID(chown(to, statbuf.st_uid, statbuf.st_gid));	/* Copy ownership */
#endif /* MSDOS */

#ifndef VMS
#ifndef __ZTC__
  if (MyFlags & MY_COPYTIME)
  {
    struct utimbuf timep;
    timep.actime  = statbuf.st_atime;
    timep.modtime = statbuf.st_mtime;
    VOID(utime((char*) to, &timep));/* Update last accessed and modified times */
  }
#else
  if (MyFlags & MY_COPYTIME)
  {
    time_t time[2];
    time[0]= statbuf.st_atime;
    time[1]= statbuf.st_mtime;
    VOID(utime((char*) to, time));/* Update last accessed and modified times */
  }
#endif
#endif
  return 0;
} /* my_copystat */
