
#ifndef UNIXPERF_H
#define UNIXPERF_H

/*** headers ***/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

/*** types ***/

typedef unsigned char Bool;
#define FALSE 0
#define TRUE 1

typedef float Version;
#define V1 1.1

typedef Bool (*InitProc)(Version);
typedef unsigned int (*Proc)(void);
typedef void (*CleanupProc)(void);

#define CHOOSE_NOT 0x0		/* Test not selected */
#define CHOOSE_ALL 0x1		/* Test selected by -all */
#define CHOOSE_SELF 0x2		/* Test selected individually */

#define TF_NONE 0x0
#define TF_NONSTANDARD 0x1
#define TF_NOWARMUP 0x2
#define TF_NOMULTI 0x4

#define U_MP_SINGLE 0x0
#define U_MP_MULTI 0x1
#define U_MP_MUSTRUN 0x2

#define MAXCPU 128

typedef struct _Test {
    char *name;
    char *description;
    InitProc initProc;
    Proc proc;
    CleanupProc passCleanup;
    CleanupProc finalCleanup;
    unsigned int warmupCount;
    int flags;
    Version version;
    int doTest;
} Test, TestPtr;

typedef struct _Alias {
    char *name;
    char **list;
} Alias, AliasPtr;

typedef struct _FloatMatrix {
    float matrix[4][4];
} FloatMatrix, *FloatMatrixPtr;

typedef struct _DoubleMatrix {
    double matrix[4][4];
} DoubleMatrix, *DoubleMatrixPtr;

/*** macros ***/

#define MEMERROR(ptr) if(ptr == NULL) MemError(__FILE__, __LINE__)
#define SYSERROR(rc, msg) if(rc == -1) SysError(msg, __FILE__, __LINE__)
#define DOSYSERROR(msg) SysError(msg, __FILE__, __LINE__)
#ifdef DEBUG
#define debugSYSERROR(rc, msg) if(rc == -1) SysError(msg, __FILE__, __LINE__)
#else
#define debugSYSERROR(rc, msg) {}
#endif

/*** global variables ***/

extern pid_t pid;
extern int rc;
extern int fd;
extern char *ptr;
extern struct timeval StartTime;
extern int TestSeconds;
extern int TestRepeats;
extern Version TestVersion;
extern int NumberOfTests;
extern int NumberOfAliases;
extern Test tests[];
extern Alias aliases[];
extern int TestProblemOccured;
extern char * TmpDir;
extern int MpFlags;
extern int MpNum;
extern uint64_t MpMustrunMask[MAXCPU/64];
extern pid_t *MpChildren;

/*** functions ***/

/* unixperf.c */
extern void Cleanup(void);

/* utils.c */
extern void MemError(char *file, unsigned int line);
extern void SysError(char *msg, char *file, unsigned int line);
extern void StartStopwatch(void);
extern double StopStopwatch(void);
extern double RoundTo3Digits(double d);
extern void ReportTimes(double usecs, unsigned int n, char *str, Bool average);
extern void RemoveAnyTmpFiles(void);

extern void MicroSecondPause(int);

/* describe.c */
extern void DescribeSystem(void);

/* mp.c */
extern int MpBeginTest();
extern void MpEndTest(int nproblems);
extern int MpParseProcStr(uint64_t mask[], const char *str);

#endif /* UNIXPERF_H */

