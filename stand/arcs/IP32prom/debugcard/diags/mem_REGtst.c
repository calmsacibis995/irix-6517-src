/*
 * ===================================================
 * $Date:
 * $Revision:
 * $Author:
 * ===================================================
 *  test name:      mem_REGtst.c
 *  descriptions:   memory controller register read/write test
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>


int
Walking_0 (uint64_t* addr, int bits)
{
  volatile uint64_t *testaddr = addr;
  volatile uint64_t expected_data, observed_data;
  int               i;

  expected_data = 0x1;
  for (i = 0; i < bits; i++) {
    *testaddr = ~expected_data;
    observed_data = *testaddr;
    if ((observed_data & expected_data) != 0x0) {
      printf ("\n"
              "Error: Unexpected walking 0 write,read test\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              testaddr, ~expected_data, observed_data);
      return 0;
    }
    expected_data = expected_data << 1;
  }
  return 1;
}
   
      
int
Walking_1 (uint64_t* addr, int bits)
{
  volatile uint64_t *testaddr = addr;
  volatile uint64_t expected_data, observed_data;
  int               i;

  expected_data = 0x1;
  for (i = 0; i < bits; i++) {
    *testaddr = expected_data;
    observed_data = *testaddr;
    if (observed_data != expected_data) {
      printf ("\n"
              "Error: Unexpected walking 1 write,read test\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              testaddr, expected_data, observed_data);
      return 0;
    }
    expected_data = expected_data << 1;
  }
  return 1;
}
   
      
    
    
int
mem_REGtst (void)
{
  
  volatile  mem_reg memptr = (mem_reg) MEMcntlBASE;
  uint64_t  expected_data, observed_data;
  int32_t   i ; 

  /*
   * [01] Walking 1 through ecc repl register.
   */
  if (!(Walking_1(&memptr->ecc_repl, 8))) return 0;
  
  /*
   * [02] Walking 0 through ecc repl register.
   */
  if (!(Walking_0(&memptr->ecc_repl, 8))) return 0;
  
  /*
   * [03] Pattern tests.
   */
  memptr->ecc_repl = expected_data = (uint64_t)((uint32_t)0x5a);
  if ((observed_data = memptr->ecc_repl) != expected_data) {
    printf ("\n"
            "Error: Unexpected walking 1 write,read test\n"
            "              Address = 0x%0x\n"
            "        Expected data = 0x%0lx\n"
            "        Observed data = 0x%0lx\n",
            &memptr->ecc_repl, expected_data, observed_data);
    return 0;
  }
  
  memptr->ecc_repl = expected_data = (uint64_t)((uint32_t)0xa5);
  if ((observed_data = memptr->ecc_repl) != expected_data) {
    printf ("\n"
            "Error: Unexpected walking 1 write,read test\n"
            "              Address = 0x%0x\n"
            "        Expected data = 0x%0lx\n"
            "        Observed data = 0x%0lx\n",
            &memptr->ecc_repl, expected_data, observed_data);
    return 0;
  }

  expected_data = (uint64_t)((uint32_t)0x9);
  for (i = 0; i < 1; i++) {
    memptr->ecc_repl = expected_data;
    if ((observed_data = memptr->ecc_repl) != expected_data) {
      printf ("\n"
              "Error: Unexpected walking 1 write,read test\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              &memptr->ecc_repl, expected_data, observed_data);
      return 0;
    }
    expected_data = expected_data <<4;
  }
  
  expected_data = (uint64_t)((uint32_t)0x6);
  for (i = 0; i < 1; i++) {
    memptr->ecc_repl = expected_data;
    if ((observed_data = memptr->ecc_repl) != expected_data) {
      printf ("\n"
              "Error: Unexpected walking 1 write,read test\n"
              "              Address = 0x%0x\n"
              "        Expected data = 0x%0lx\n"
              "        Observed data = 0x%0lx\n",
              &memptr->ecc_repl, expected_data, observed_data);
      return 0;
    }
    expected_data = expected_data <<4;
  }
  
  
  /*
   * [02] We're done.
   */
  return 1;
  
} 
    

/*
 * =================================================
 *
 * $Log: mem_REGtst.c,v $
 * Revision 1.1  1997/08/18 20:40:44  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:50:44  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/10/04  20:33:16  kuang
 * Ported to run with crime ECC
 *
 * Revision 1.1  1996/04/04  23:37:34  kuang
 * Ported from petty_crime design verification test suite
 *
 * =================================================
 */

/* END OF FILE */
