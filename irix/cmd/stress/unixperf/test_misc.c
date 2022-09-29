
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_misc.c tests the speed of various OS facilities. */

#include "unixperf.h"
#include "test_misc.h"

#include <sys/time.h>
#include <sys/wait.h>
#include <bstring.h>

Bool
NullInitProc(Version version)
{
   return TRUE;
}

void
NullProc(void)
{
}

unsigned int
DoGetTimeOfDay(void)
{
    int i;
    struct timeval t;

    for(i=100;i>0;i--) {
#ifdef SVR4
        gettimeofday(&t);
        gettimeofday(&t);
        gettimeofday(&t);
        gettimeofday(&t);
#else
        struct timezone foo;
        gettimeofday(&t, &foo);
        gettimeofday(&t, &foo);
        gettimeofday(&t, &foo);
        gettimeofday(&t, &foo);
#endif
    }
    return 400;
}

unsigned int
DoForkAndWait(void)
{
    int i;

    for(i=50;i>0;i--) {
	pid = fork();
	SYSERROR(pid, "fork");
	if(pid == 0) {
	    /* child */
	    _exit(0);
	} else {
	    /* parent */
	    int waitstat;

	    wait(&waitstat);
	}
    }
    return 50;
}

unsigned int
DoGetPid(void)
{
    int i;

    for(i=500;i>0;i--) {
       pid = getpid();
    }
    return 500;
}

unsigned int
DoEzSysCalls(void)
{
    int i;

    for(i=500;i>0;i--) {
       close(dup(0));
       getpid();
       getuid();
       umask(022);
    }
    return 500;
}

unsigned int
DoSleep1(void)
{
    sleep(1);
    return 1;
}

#define NUM_PIPES 64

static int stashedFds;
static int stashedMaxFd;
static int pipe_set[NUM_PIPES][2];
static fd_set readset;
static fd_set writeset;
static fd_set exceptset;
static struct timeval notimeout = { 0, 0 };

Bool
InitSelectFDs(Version version, int fds, Bool forRead, Bool forWrite)
{
    int i;
    char c = 'a';

    stashedFds = fds;
    stashedMaxFd = 0;
    FD_ZERO(&readset);
    FD_ZERO(&writeset);
    FD_ZERO(&exceptset);
    for(i=0;i<fds;i++) {
	rc = pipe(&pipe_set[i][0]);
	SYSERROR(rc, "pipe");
	if(forRead) {
	    FD_SET(pipe_set[i][0], &readset);
	    if(pipe_set[i][0] > stashedMaxFd) stashedMaxFd = pipe_set[i][0];
	    /* write a character to make read side readable */
	    write(pipe_set[i][1], &c, 1);
        }
	if(forWrite) {
	    FD_SET(pipe_set[i][1], &writeset);
	    if(pipe_set[i][1] > stashedMaxFd) stashedMaxFd = pipe_set[i][1];
        }
    }
    stashedMaxFd++;
    return TRUE;
}

Bool
InitSelect0(Version version)
{
    return InitSelectFDs(version, 0, FALSE, FALSE);
}

Bool
InitSelect16R(Version version)
{
    return InitSelectFDs(version, 16, TRUE, FALSE);
}

Bool
InitSelect32R(Version version)
{
    return InitSelectFDs(version, 32, TRUE, FALSE);
}

Bool
InitSelect16W(Version version)
{
    return InitSelectFDs(version, 16, FALSE, TRUE);
}

Bool
InitSelect32W(Version version)
{
    return InitSelectFDs(version, 16, FALSE, TRUE);
}

Bool
InitSelect64RW(Version version)
{
    return InitSelectFDs(version, 64, TRUE, TRUE);
}

unsigned int
DoSelect(void)
{
   int i;

   for(i=100;i>0;i--) {
       rc = select(stashedMaxFd, &readset, &writeset, &exceptset, &notimeout);
       debugSYSERROR(rc, "select");
   }
   return 100;
}

void
CleanupSelect(void)
{
    int i;

    for(i=0;i<stashedFds;i++) {
	rc = close(pipe_set[i][0]);
	SYSERROR(rc, "close");
	rc = close(pipe_set[i][1]);
	SYSERROR(rc, "close");
    }
}
