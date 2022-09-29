#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <unistd.h>
#include <errno.h>
#include <bstring.h>
#include <string.h>
#include <sys/time.h>

SimControl_t*   simControl;
usptr_t*        arena;
int             done;

static int      recvFd,sendFd;


/* Client/server sync routines */
static void send(void);
static int  recv(void);


/* List of server modules to be initialized */
void (*simlist[])(void) = SIM_LIST;

/* Table os service dispath routines */
void (*simService[50])(void);
int simServiceFree;


/******
 * main		Initialize the server
 */
int 
main(int argc, char** argv){
  int i;
  int lastCode;

  if (argc<2)
    exit(0);
  
  printf(" SIM server [%s]\n", argv[1]);
  
  usconfig(CONF_ATTACHADDR,SIM_ARENA_ADDR);
  arena = usinit(argv[1]);
  assert(arena);
  
  /* Locate the SimControl structure in the communication area. */
  simControl = (SimControl_t*)usgetinfo(arena);
  
  
  /* Allocate and init the match tables */
  simControl->addrTbl =
    (SimAddrMatch_t*)usmalloc(ADDR_TBL_SIZE * sizeof(SimAddrMatch_t),arena);
  memset(simControl->addrTbl,0,ADDR_TBL_SIZE * sizeof(SimAddrMatch_t));
  simControl->cop0Tbl = 
    (SimCop0Match_t*)usmalloc(COP0_TBL_SIZE * sizeof(SimCop0Match_t),arena);
  memset(simControl->cop0Tbl,0,COP0_TBL_SIZE * sizeof(SimCop0Match_t));
  
  /* Allocate and init the cop0 registers */
  simControl->cop0Regs =
    (long long*)usmalloc(32*sizeof(long long),arena);
  memset(simControl->cop0Regs,0,32*sizeof(long long));

  /* Assign a file descriptor to the sendSema (client send, server recv) */
  recvFd = usopenpollsema(simControl->sendSema,0777);
  sendFd = usopenpollsema(simControl->recvSema,0777);

  assert(recvFd>=0);

  /* Attach the simulation modules */
  for (i=0;simlist[i];i++)
    (*simlist[i])();

  /* Let the client proceed */
  simControl->refs++;
  send();

  /*
   * Main loop
   *
   * Wait for received "post" from client and then dispatch to
   * service routine.
   */
  while(!done){
    SimAddrMatch_t* am;
    SimCop0Match_t* cm;

    switch(lastCode = recv()){
    case SIM_abort:
      goto exit;
    case SIM_fwd_loadStore:
    case SIM_loadStore_fwd:
      am = &simControl->addrTbl[simControl->matchIndex];
      (*simService[am->targetCode])();
      break;
    case SIM_fwd_Cop0:
      cm = &simControl->cop0Tbl[simControl->matchIndex];
      (*simService[cm->targetCode])();
      break;
    }
    send();
  }
  
  /*
   * Clean up and exit
   */
 exit:
  simControl->refs--;
  usdetach(arena);
  exit(0);
}



/******
 * recv		Wait post from client
 */
static int
recv(void){
  int    i;
  fd_set r;
  static struct timeval timeout = {10,0};
  i = uspsema(simControl->sendSema);
  while(i==0){
    FD_ZERO(&r);
    FD_SET(recvFd,&r);
    i = select(recvFd+1,&r,0,0,&timeout);

    /* Filter out EINTR errors */
    if (i<0 && errno == EINTR)
      i = 0;

    if (i==0)
      /*
       * Timeout or EINTR.
       * Check that the client is still there.
       * If not, return SIM_abort, this assures us the server will exit.
       *
       * Note: We assume EINTR's are relatively rare (once a second?).
       */
      if (kill(simControl->clientPid,0))
	return SIM_abort;

    /* Terminate if any error */
    if (i<0)
      return SIM_abort;
  }
  return simControl->matchCode;
}


/******
 * send		Post to client
 */
static
void send(void){
  usvsema(simControl->recvSema);
}

