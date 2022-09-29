
#include "unixperf.h"

/* test_vm.c */

extern Bool InitDevZero(Version version);
extern unsigned int DoDevZeroMap(void);
extern unsigned int DoDevZeroWalk(void);
extern void CleanupDevZero(void);
extern Bool InitMmapMegabyteFile(Version version);
extern unsigned int DoMmapMegabyteFile(void);
extern void CleanupMmapMegabyteFile(void);

