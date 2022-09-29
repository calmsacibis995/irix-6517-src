#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <sys/sbd.h>
#include <time.h>

#define UNIMPLEMENTED

static unsigned long base;

/**********
 * c0_count	Simulate read/write access to C0_COUNT
 *---------
 *
 * Serviced by SIM_fwd_cop0 so the client reads C0_COUNT after
 * we set it.
 */
static void
c0_count(void){
  unsigned long now = clock();
  simControl->cop0Regs[C0_COUNT] = (now - base)*50;
}


/*************
 * simCop0Init
 */
void simCop0Init(){
  int targetCode = simRegisterService(c0_count);
  simControl->cop0Regs[C0_SR] = SR_BEV;
  simControl->cop0Regs[C0_PRID] = C0_IMP_TRITON<<C0_IMPSHIFT;

  base = clock();
  /* Register a read service for C0_COUNT */
  simRegisterCop0Match(~0,
		       C0_COUNT,
		       SIM_fwd_Cop0,
		       targetCode);

  /* Register a read/write service for all cop0 registers */
  simRegisterCop0Match(0,0,SIM_cop0,0);
}
