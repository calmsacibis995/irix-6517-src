
/* unixperf - an x11perf-style Unix performance benchmark */

/* globals.c declares various global variables used within unixperf. */

#include "unixperf.h"

#include <sys/time.h>

/* random global variables used by tests */
int rc;
pid_t pid;
int fd;
char *ptr;

struct timeval StartTime;

/* test parameters */
int TestSeconds = 5;
int TestRepeats = 5;
Version TestVersion = 1;
int TestProblemOccured;
char *TmpDir = "/usr/tmp";
int MpFlags = U_MP_SINGLE;
char *MpMustrun = NULL;
uint64_t MpMustrunMask[MAXCPU/64];
pid_t *MpChildren = NULL;
int MpNum = 0;
