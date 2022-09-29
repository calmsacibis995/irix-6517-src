
/* unixperf - an x11perf-style Unix performance benchmark */

#include "unixperf.h"

/* test_rpc.h */

extern Bool InitSunRPC(Version version);
extern unsigned int DoCreateDestroyTcpRpcHnd(void);
extern unsigned int DoCreateDestroyUdpRpcHnd(void);
extern Bool InitTcpSunRPC(Version version);
extern Bool InitUdpSunRPC(Version version);
extern unsigned int DoTinySunRPC(void);
extern unsigned int DoNullSunRPC(void);
extern void CleanupSunRPC(void);

