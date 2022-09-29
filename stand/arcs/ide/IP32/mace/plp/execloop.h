#if !defined(__EXECLOOP_H__)
#define __EXECLOOP_H__

#include <stdlib.h>
#include <string.h>

static int   zzresult;

#define ExecuteTest(test)                                              \
       zzresult = test;
#endif 
