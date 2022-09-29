
#include "unixperf.h"

/* test_ipc.c */

extern Bool InitPipePing1(Version version);
extern Bool InitPipePing1K(Version version);
extern unsigned int DoPipePing(void);
extern void CleanupPipePing(void);

extern Bool InitSpawningServerTCP(Version version);
extern Bool InitStandaloneServerTCP(Version version);
extern unsigned int DoEchoClientTCP(void);
extern void CleanupServerTCP(void);

