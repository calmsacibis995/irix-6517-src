#include <libsc.h>
#include <uif.h>
#include <sys/immu.h>
#include "sys/RACER/heart.h"

extern int err_code;

int 
dummy (int i)
{
  return 0;
}

void ct_delay(int delay)
{
  int i;
  for(i=0; i<delay; i++)
    dummy(i);
}

void 
ct_exit(void* not_used) 
{

  msg_printf(ERR, "Cache Thrasher Test Failed\n");

  err_code = 1;
  
}

/*
 * sets up heart trigger
 *
 */
void
setup_heart_trigger(void) {

  heartreg_t heart_mode_reg;

  /* set up register */
  heart_mode_reg = PHYS_TO_K1(HEART_MODE);

#ifdef VERBOSE
  pon_puts("setup_heart_trigger: before setup, mode reg = ");
  pon_puthex64(*(volatile heartreg_t*)heart_mode_reg);
  pon_puts("\r\n");
#endif  

  /* select Trig Register bit 0 in Mode Register */
  *(volatile heartreg_t*)heart_mode_reg = *(volatile heartreg_t*)heart_mode_reg | HM_TRIG_REG_BIT;

#ifdef VERBOSE
  pon_puts("setup_heart_trigger: after setup, mode reg = ");
  pon_puthex64(*(volatile heartreg_t*)heart_mode_reg);
  pon_puts("\r\n");
#endif  

}

/*
 * trigger heart
 *
 */
void 
heart_trigger(int val) {

  heartreg_t heart_trigger_reg = PHYS_TO_K1(HEART_TRIGGER);

#if ORIG
  /* turn trigger bit on */
  *(volatile heartreg_t*) heart_trigger_reg = 0x1;

  /* turn trigger bit off */
  *(volatile heartreg_t*) heart_trigger_reg = 0x0;
#else
  *(volatile heartreg_t*) heart_trigger_reg = val;
#endif
}

void
print_heart_exception_cause_reg(void) {

  volatile heartreg_t *heart_cause_reg =
	(volatile heartreg_t *)PHYS_TO_K1(HEART_CAUSE);

  msg_printf(VRB, "Heart exception cause = 0x%X\n", *heart_cause_reg);

}






