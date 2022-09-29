#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <string.h>
#include <sys/cpu.h>
#include <time.h>
#include <unistd.h>


typedef struct{
  long long    regs[10];
  SimAddrMap_t map;
  long long    baseUST;
} UST_t;

static UST_t* ust;

#define UNIMPLEMENTED_UST 0


/***************
 * simUSTService	Service forwarded UST requests
 */
void
simUSTService(void){
  SimAddrMatch_t* am = &simControl->addrTbl[simControl->matchIndex];
  SimAddrMap_t*   map = am->mapping;
  int             regNum = (simControl->paddr & map->mask)>>map->shift;
  long long       t = time(0);

  ust->regs[0] = ((t - ust->baseUST) * 1041666666LL) & 0xffffffffLL; 

  switch(regNum){
    /*
     * We assume a reference to 0 is a dword reference and a reference to
     * 1 is word reference.  We could check this but we aren't.
     */
  case 0:
  case 1:
    break;
  default:
    assert(UNIMPLEMENTED_UST);
  }
}


/************
 * simUSTInit	Connect RTC simulation to server
 */
void
simUSTInit(void){
  int targetCode = simRegisterService(simUSTService);

  /* Allocate storage area for UST registers */
  ust = (UST_t*)usmalloc(sizeof(UST_t),arena);
  assert(ust);
  memset(ust,0,sizeof(UST_t));
  
  /*
   * Fill in the map
   */
  ust->map.base  = (char*)ust->regs;
  ust->map.stride= 4;
  ust->map.mask  = 0x3c;
  ust->map.shift = 2;
  
  /* register client mediated write access*/
  simRegisterAddrMatch(~0xFF,
		       MACE_UST | ADDR_MATCH_STORE,
		       SIM_loadStore,
		       targetCode,
		       &ust->map);
  
  /* Register server side read processing for entire ust */
  simRegisterAddrMatch(~0xFF & (~ADDR_MATCH_STORE),
		       MACE_UST,
		       SIM_fwd_loadStore,
		       targetCode,
		       &ust->map);

  /*
   * Record a "starting time".  We set it back 20 seconds so that
   * UST will have a value by the time we first read it.
   */
  ust->baseUST = time(0) - 20;
}
