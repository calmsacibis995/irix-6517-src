
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_rpc.c tests the speed of SunRPC calls. */

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <rpc/rpc.h>

#include "unixperf.h"
#include "test_rpc.h"
#define _RPCGEN_CLNT
#include "rpc_service.h"

#define RMACHINE "localhost"
CLIENT *handle = NULL; 

/* Implement the fastest no-op SunRPC with no use of XDR. */
void
nullproc_1(void *argp, CLIENT *clnt)
{
  static struct timeval TIMEOUT = { 25, 0 };

  if (clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void, 0, (xdrproc_t)xdr_void, 0, TIMEOUT) != RPC_SUCCESS) {
    TestProblemOccured = 1;
  }
}

Bool
startRpcService(void)
{
  CLIENT *handle;
  int i;

  pid = fork();
  SYSERROR(pid, "fork");
  if (pid == 0) {
    server_main();
  }
  /* Once we have started the service, try to connect to the service.  This
     could take a couple tries; particularly on an MP machine. */
  for (i=0; i<100; i++) {
    handle = clnt_create(RMACHINE, RPC_SERVICE_PROG, RPC_SERVICE_VERS, "tcp");
    if (handle) {
      clnt_destroy(handle);
      return TRUE;
    }
    /* We don't want to simply spin trying to create the client handle;
       take a breather.  This could help the service have the cycles it
       needs to initialize. */
    MicroSecondPause(1000);
  }
  printf("Could not connect to SunRPC server after 100 tries.\n");
  rc = kill(pid, SIGKILL);
  SYSERROR(rc, "kill");
  return FALSE;
}

Bool
InitSunRPC(Version version)
{
  return startRpcService();
}

unsigned int
DoCreateDestroyTcpRpcHnd(void)
{
  CLIENT *handle;
  int i;

  for(i=5;i>0;i--) {
    handle = clnt_create(RMACHINE, RPC_SERVICE_PROG, RPC_SERVICE_VERS, "tcp");
    if (handle) {
      clnt_destroy(handle);
    } else {
      TestProblemOccured = 1;
    }
  }
  return 5;
}

unsigned int
DoCreateDestroyUdpRpcHnd(void)
{
  CLIENT *handle;
  int i;

  for(i=5;i>0;i--) {
    handle = clnt_create(RMACHINE, RPC_SERVICE_PROG, RPC_SERVICE_VERS, "udp");
    if (handle) {
      clnt_destroy(handle);
    } else {
      TestProblemOccured = 1;
    }
  }
  return 5;
}

Bool
InitTcpSunRPC(Version version)
{
  if(startRpcService() == FALSE) return FALSE;

  handle = clnt_create(RMACHINE, RPC_SERVICE_PROG, RPC_SERVICE_VERS, "tcp");
  if (handle == 0) {
    clnt_pcreateerror("clnt_create");
    printf("Could not setup SunRPC client handle.\n");
    return FALSE;
  }
  return TRUE;
}

Bool
InitUdpSunRPC(Version version)
{
  if(startRpcService() == FALSE) return FALSE;

  handle = clnt_create(RMACHINE, RPC_SERVICE_PROG, RPC_SERVICE_VERS, "udp");
  if (handle == 0) {
    clnt_pcreateerror("clnt_create");
    printf("Could not setup SunRPC client handle.\n");
    return FALSE;
  }
  return TRUE;
}

unsigned int
DoTinySunRPC(void)
{
  int value, i;

  value = 30;
  for(i=100;i>0;i--) {
    value = *negate_1(&value, handle);
    value = *negate_1(&value, handle);
    value = *negate_1(&value, handle);
    value = *negate_1(&value, handle);
  }
  return 400;
}

unsigned int
DoNullSunRPC(void)
{
  int i;

  for(i=100;i>0;i--) {
    nullproc_1(0, handle);
    nullproc_1(0, handle);
    nullproc_1(0, handle);
    nullproc_1(0, handle);
  }
  return 400;
}

void
CleanupSunRPC(void)
{
  int rc;

  if (handle) clnt_destroy(handle);
  handle = NULL;
  rc = kill(pid, SIGKILL);
  SYSERROR(rc, "kill");
}
