
#include "unixperf.h"

/* test_misc.c */

extern Bool NullInitProc(Version version);
extern void NullProc(void);

/* Really in dhry_1.c (with assistance from dhry_2.c) */
extern Bool InitDhrystone(Version version);
extern unsigned int DoDhrystone(void);
extern void CleanupDhrystone(void);

extern unsigned int DoGetPid(void);
extern unsigned int DoEzSysCalls(void);
extern unsigned int DoGetTimeOfDay(void);
extern unsigned int DoForkAndWait(void);

extern unsigned int DoSleep1(void);

extern Bool InitSelect0(Version version);
extern Bool InitSelect16R(Version version);
extern Bool InitSelect32R(Version version);
extern Bool InitSelect16W(Version version);
extern Bool InitSelect32W(Version version);
extern Bool InitSelect64RW(Version version);
extern unsigned int DoSelect(void);
extern void CleanupSelect(void);

