/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.

   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

#ifdef __EMX__
#include "../mysys/my_lock.c"
#else

#include <global.h>
#include <my_sys.h>
#include <mysys_err.h>
#include <my_pthread.h>
#include <thr_alarm.h>
#include <errno.h>

#ifdef HAVE_FCNTL
static struct flock lock;		/* Must be static for sun-sparc */
#endif

	/* Lock a part of a file */

int my_lock(File fd,int locktype,my_off_t start,my_off_t length,myf MyFlags)
{
  thr_alarm_t alarmed;
  uint wait_for_alarm;
  DBUG_ENTER("my_lock");
  DBUG_PRINT("my",("Fd: %d  Op: %d  start: %ld  Length: %ld  MyFlags: %d",
		   fd,locktype,(ulong) start,(ulong) length,MyFlags));
  if (my_disable_locking)
    DBUG_RETURN(0); /* purecov: inspected */
  lock.l_type=(short) locktype;
  lock.l_whence=0L;
  lock.l_start=(long) start;
  lock.l_len=(long) length;
  wait_for_alarm=(MyFlags & MY_DONT_WAIT ? MY_HOW_OFTEN_TO_ALARM :
		  (uint) 12*60*60);
  if (fcntl(fd,F_SETLK,&lock) != -1)	/* Check if we can lock */
    DBUG_RETURN(0);			/* Ok, file locked */
  DBUG_PRINT("info",("Was locked, trying with alarm"));
  if (!thr_alarm(&alarmed,wait_for_alarm))
  {
    int value;
    while ((value=fcntl(fd,F_SETLKW,&lock)) && !thr_got_alarm(alarmed) &&
	   errno == EINTR) ;
    thr_end_alarm(&alarmed);
    if (value != -1)
      DBUG_RETURN(0);
  }
  else
  {
    errno=EINTR;
  }
  if (errno == EINTR || errno == EACCES)
    my_errno=EAGAIN;			/* Easier to check for this */
  else
    my_errno=errno;

  if (MyFlags & MY_WME)
  {
    if (locktype == F_UNLCK)
      my_error(EE_CANTUNLOCK,MYF(ME_BELL+ME_WAITTANG),errno);
    else
      my_error(EE_CANTLOCK,MYF(ME_BELL+ME_WAITTANG),errno);
  }
  DBUG_PRINT("error",("errno: %d",errno));
  DBUG_RETURN(-1);
}
#endif
