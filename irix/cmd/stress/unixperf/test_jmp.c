
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_jmp.c tests the speed of setjmp/longjmp functionality. */

#include "unixperf.h"
#include "test_jmp.h"

#include <setjmp.h>

static jmp_buf buf;

static void
work(void)
{
  longjmp(buf, 1);
}

unsigned int
DoSetjmp(void)
{
  int i;

  for(i=1000;i>0;i--) {
    if (setjmp(buf) == 0)
      work();
    if (setjmp(buf) == 0)
      work();
    if (setjmp(buf) == 0)
      work();
    if (setjmp(buf) == 0)
      work();
  }
  return 4000;
}

static sigjmp_buf sigbuf;

static void
sigwork(void)
{
  siglongjmp(sigbuf, 1);
}

unsigned int
DoSigsetjmp(void)
{
  int i;

  for(i=1000;i>0;i--) {
    if (sigsetjmp(sigbuf, 1) == 0)
      sigwork();
    if (sigsetjmp(sigbuf, 1) == 0)
      sigwork();
    if (sigsetjmp(sigbuf, 1) == 0)
      sigwork();
    if (sigsetjmp(sigbuf, 1) == 0)
      sigwork();
  }
  return 4000;
}
