#include <assert.h>
#include "sim.h"

#define IMPOSSIBLE 0

void
lwl(__int64_t*,void*);
void
lwr(__int64_t*,void*);
void
swl(__int64_t*,void*);
void
swr(__int64_t*,void*);

/************
 * simSsStore	Returns true if inst is a store (sw, sb,...) or a mtc0,...
 */
int
simIsStore(Inst_t inst){  switch(inst.i_format.opcode){
  case sb_op:
  case sh_op:
  case sw_op:
  case swl_op:
  case swr_op:
  case sc_op:
  case sd_op:
  case sdl_op:
  case sdr_op:
  case scd_op:
    return 1;
  case cop0_op:
    return inst.r_format.rs > 3;
  }
  return 0;
}


/**************
 * simMatchAddr
 *-------------
 * The Addr match table is simply a linear vector of entries and we just
 * search the list.  A hash list of a sorted list would be better but the
 * "masking" part of the search complicates the design of faster searches.
 *
 * Note that the first entry is reserved.
 */
int
simMatchAddr(Saddr_t addr, int load){
  SimAddrMatch_t* table = simControl->addrTbl+1;
  Saddr_t         t     = TO_PHYSICAL(addr);
  int             i;

  if (load)
    t |= ADDR_MATCH_STORE;

  for (i=1;table->matchCode!=SIM_nomatch;i++,table++)
    if ((t & table->mask) == table->target){
      simControl->matchIndex = i;
      simControl->matchCode = table->matchCode;
      return i;
    }
  return 0;
}


/**************
 * simMatchCop0
 *-------------
 * The cop0 match table is simply a linear vector of entries and we just
 * search the list.  
 */
int
simMatchCop0(int reg, int load){
  SimCop0Match_t* table = simControl->cop0Tbl+1;
  int             i;


  if (load)
    reg |= COP0_MATCH_STORE;
  for (i=1;table->matchCode!=SIM_nomatch;i++,table++)
    if ((reg & table->mask)==table->target){
      simControl->matchIndex = i;
      simControl->matchCode = table->matchCode;
      return i;
    }
  return 0;
}


/**************
 * simLoadStore	Simulate load/store
 */
void
simLoadStore(Inst_t		inst,
	     SigContext_t*	sc,
	     Saddr_t 		paddr,
	     int              index){
  SimAddrMatch_t*   sam = &simControl->addrTbl[index];
  SimAddrMap_t*     map = sam->mapping; 
  __int64_t*        srcdst = (__int64_t*)&sc->sc_regs[inst.i_format.rt];
  void*             addr;

  /*
   * Test for ignore.  Used to filter out operations that fail silently.
   */
  if (map->ignoreIfZero && *map->ignoreIfZero==0)
    return;

  /*
   * Map the platform physical address to the associated simulator
   * resource address.
   */
  addr = map->base + ((paddr & map->mask)>>map->shift) * map->stride;


  /* Dispatch and process according to the Instr_t type */
  switch(inst.i_format.opcode){
  case lb_op:
    *srcdst = *((char*)addr);
    break;
  case lbu_op:
    *srcdst = *((unsigned char*)addr);
    break;
  case lh_op:
    *srcdst = *((short*)addr);
    break;
  case lhu_op:
    *srcdst = *((unsigned short*)addr);
    break;
  case lw_op:
    *srcdst = *((long*)addr);
    break;
  case lwl_op:
    lwl(srcdst,addr);
    break;
  case lwr_op:
    lwr(srcdst,addr);
    break;
  case ll_op:
    assert(ll_op==0);
    break;
  case ld_op:
   *srcdst = *((long long*)(addr));
    break;
  case ldl_op:
    assert(ldl_op==0);
    break;
  case ldr_op:
    assert(ldr_op==0);
    break;
  case lld_op:
    assert(lld_op==0);
    break;
  case sb_op:
    *((unsigned char*)addr) = *srcdst;
    break;
  case sh_op:
    *((unsigned short*)addr) = *srcdst;
    break;
  case sw_op:
    *((unsigned long*)addr) = *srcdst;
    break;
  case swl_op:
    swl(srcdst,addr);
    break;
  case swr_op:
    swr(srcdst,addr);
    break;
  case sc_op:
    assert(sc_op==0);
    break;
  case sd_op:
    *((long long*)addr) = *srcdst;
    break;
  case sdl_op:
    assert(sdl_op==0);
    break;
  case sdr_op:
    assert(sdr_op==0);
    break;
  case scd_op:
    assert(scd_op==0);
  default:
    assert(IMPOSSIBLE);
  }
}


#ifdef _MIPSEB
void
lwl(__int64_t* srcdst, void* addr){
  int o,j;
  assert(sizeof(__uint64_t)==8);
  for (j=0,o=(long)addr&3;j<4-o;j++)
    *((char*)srcdst+j+4) = *((char*)addr+o+j);
  if (((signed char*)srcdst)[4]<0)
    *(long*)srcdst = -1;
  else
    *(long*)srcdst = 0;
}

void
lwr(__int64_t* srcdst, void* addr){
  int o,j;
  assert(sizeof(__uint64_t)==8);
  for (j=0,o=(long)addr&3;j>=o;j++)
    *((char*)srcdst+7-o+j) = *((char*)addr+j);
  if (o==3)
    if (*(char*)addr<0)
      *(long*)srcdst = -1;
    else
      *(long*)srcdst = 0;
}

void
swl(__int64_t* srcdst, void* addr){
  int o,j;
  assert(sizeof(__uint64_t)==8);
  for (j=0,o=(long)addr&3;j<4-o;j++)
    *((char*)addr+o+j) = *((char*)srcdst+j+4);
}

void
swr(__int64_t* srcdst, void* addr){
  int o,j;
  assert(sizeof(__uint64_t)==8);
  for (j=0,o=(long)addr&3;j>=o;j++)
    *((char*)addr+j) = *((char*)srcdst+7-o+j);
}
#endif

