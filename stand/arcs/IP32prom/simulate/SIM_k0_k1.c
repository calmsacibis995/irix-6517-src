#include "SIM.h"

/*
 * MEMBLOCK is a large block of memory to which references to k0 and k1
 * memory are redirected.
 */
char SIMblock[MEMBLOCK_SIZE+4096];
char* SIMmemblock;		/* Pointer to pseudo ram */


void SIM_k0_k1(Instr_t inst, Saddr_t eadr, Ctx_t* sc, int ignore){
  /* For k0, k1 references, just redirect to the memory block */
  loadstore(inst, sc,SIMmemblock+(eadr & MEMBLOCK_MASK));
}
