
/* unixperf - an x11perf-style Unix performance benchmark */

/* rpc_service_simp.c implements the server side SunRPCs */

#include <rpc/rpc.h>

#define negate_1  server_negate_1
#define main      server_main

#define _RPCGEN_SVC
#include "rpc_service.h"

#include "rpc_service_svc.c"

int *
negate_1(int *value, struct svc_req *req)
{
  static int neg;

  neg = -(*value);
  return &neg;
}
