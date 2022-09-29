#include <assert.h>
#include "sim.h"
#include "simSvr.h"

static SimAddrMatch_t* freeAddrSlot(void);
static SimCop0Match_t* freeCop0Slot(void);

/********************
 * simRegisterService
 */
int
simRegisterService( void (*service)(void)){
  int i = simServiceFree;
  simService[simServiceFree++] = service;
  assert(simServiceFree<SIM_SERVICE_SIZE);
  return i;
}

/**********************
 * simRegisterAddrMatch
 */
void
simRegisterAddrMatch(__uint64_t    mask,
		     __uint64_t    target,
		     int           matchCode,
		     int           targetCode,
		     SimAddrMap_t* map){
  SimAddrMatch_t* am = freeAddrSlot();
  am->mask = mask;
  am->target = target;
  am->matchCode = matchCode;
  am->targetCode = targetCode;
  am->mapping = map;
}


/**************
 * freeAddrSlot
 */
static SimAddrMatch_t*
freeAddrSlot(void){
  SimAddrMatch_t* am;
  int i;
  for(am = &simControl->addrTbl[1], i= 0;
      i<ADDR_TBL_SIZE;
      i++,am++)
    if (am->matchCode==SIM_nomatch)
      return am;
  return 0;
}


/**********************
 * simRegisterCop0Match
 */
void
simRegisterCop0Match(int mask, int target, int matchCode, int targetCode){
  SimCop0Match_t* cm = freeCop0Slot();
  
  cm->mask = mask;
  cm->target = target;
  cm->matchCode = matchCode;
  cm->targetCode = targetCode;
}

/**************
 * freeCop0Slot
 */
static SimCop0Match_t*
freeCop0Slot(void){
  SimCop0Match_t* cm;
  int i;
  for(cm = &simControl->cop0Tbl[1], i= 0;
      i<COP0_TBL_SIZE;
      i++,cm++)
    if (cm->matchCode==SIM_nomatch)
      return cm;
  return 0;
}
