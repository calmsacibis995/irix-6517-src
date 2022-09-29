/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#define USES_TYPES
#include "global.h"

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
#include "mysys_priv.h"
#include <sys/times.h>
#endif

long my_clock(void)
{
#if !defined(MSDOS) && !defined(__WIN32__)
  struct tms tmsbuf;
  VOID(times(&tmsbuf));
  return (tmsbuf.tms_utime + tmsbuf.tms_stime);
#else
  return clock();
#endif
}
