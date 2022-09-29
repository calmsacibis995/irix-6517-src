#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <string.h>
#include <sys/cpu.h>
#include <unistd.h>

/*
 * For now, we implement a very simple crime model where crime is simply
 * an array of read/write RAM.  This allows the register read/writes to
 * succeed but there are no side effects.
 */

typedef struct{
  long long    regs[16];	/* Crime registers */
  long         memCtl;		/* Memory control register */
  SimAddrMap_t regMap;		/* Register map */
  SimAddrMap_t memCtlMap;	/* Mem ctrl map */
} Crime_t;

static Crime_t* crime;

#define UNIMPLEMENTED 0


/**************
 * simCrimeInit	Connect Crime "simulation" to server
 */
void
simCrimeInit(void){

  /* Allocate storage area for crime registers */
  crime = (Crime_t*)usmalloc(sizeof(Crime_t),arena);
  assert(crime);
  memset(crime,0,sizeof(Crime_t));

  /* Set up map */
  crime->regMap.base = (char*)crime->regs;
  crime->regMap.stride = 1;
  crime->regMap.mask = 0x7f;
  crime->regMap.shift = 0;
  crime->regMap.ignoreIfZero = 0;

  crime->memCtlMap.base = (char*)&crime->memCtl;
  crime->memCtlMap.stride = 1;
  crime->memCtlMap.mask = 0;
  crime->memCtlMap.shift = 0;
  crime->memCtlMap.ignoreIfZero = 0;

  /* All accesses are SIM_loadStore */
  simRegisterAddrMatch(~0 & (~ADDR_MATCH_STORE),
		       TO_PHYSICAL(CRM_MEM_CONTROL),
		       SIM_loadStore,
		       0,
		       &crime->memCtlMap);
  simRegisterAddrMatch(~0x7F & (~ADDR_MATCH_STORE),
		       TO_PHYSICAL(CRM_BASEADDR),
		       SIM_loadStore,
		       0,
		       &crime->regMap);
}
