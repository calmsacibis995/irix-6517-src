
#include "unixperf.h"

/* test_fileio.c */

extern Bool InitSetupDir(Version version);
extern Bool InitSetupDirLong(Version version);
extern unsigned int DoCreateAndUnlink(void);
extern unsigned int DoMkdirAndRmdir(void);
extern unsigned int DoStat(void);
extern unsigned int DoStatLong(void);
extern void CleanupSetupDir(void);
extern void CleanupSetupDirLong(void);

extern Bool InitSetupFiles(Version version);
extern unsigned int DoOpenAndClose(void);
extern unsigned int DoChmod(void);
extern unsigned int DoStatInDir(void);
extern void CleanupSetupFiles(void);

extern Bool InitWriteLog(Version version);
extern unsigned int DoWriteLog(void);
extern void CleanupWriteLogPass(void);
extern void CleanupWriteLog(void);

extern Bool InitDbmLookup(Version version);
extern unsigned int DoDbmLookup(void);
extern void CleanupDbmLookup(void);

