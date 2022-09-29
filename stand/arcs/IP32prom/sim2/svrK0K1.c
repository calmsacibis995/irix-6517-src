#include <assert.h>
#include "sim.h"
#include "simSvr.h"

#define K0K1Size (1024*1024*8)
#define K0K1Mask (K0K1Size - 1)

typedef struct{
  char         ram[K0K1Size];
  SimAddrMap_t map;
} K0K1_t;

static K0K1_t* ram;

/*************
 * simK0K1Init
 */
void
simK0K1Init(void){
  
  ram  = (K0K1_t*)usmemalign(4096,sizeof(K0K1_t),arena);
  assert(ram);

  /* Initialize address map structure */
  ram->map.base = ram->ram;
  ram->map.stride= 1;
  ram->map.mask  = K0K1Mask;
  ram->map.shift = 0;
  ram->map.ignoreIfZero = 0;

  /* Establish a read/write server for K1 memory */
  simRegisterAddrMatch(~K0K1Mask & (~ADDR_MATCH_STORE),
		       0,
		       SIM_loadStore,
		       0,
		       &ram->map);
  /* Establish a read/write server for K0 memory */
  simRegisterAddrMatch(~K0K1Mask & (~ADDR_MATCH_STORE),
		       0,
		       SIM_loadStore,
		       0,
		       &ram->map);
}
