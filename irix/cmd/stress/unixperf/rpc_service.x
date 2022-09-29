
/* unixperf - an x11perf-style Unix performance benchmark */

/* rpc_service.x is a simple SunRPC service for benchmarking */

program RPC_SERVICE_PROG {
  version RPC_SERVICE_VERS {
    int NEGATE(int) = 1;
#if 0
    int STRLEN(string) = 2;
    string REPEAT100(string) = 3;
#endif
  } = 1;
} = 0x36666666;

