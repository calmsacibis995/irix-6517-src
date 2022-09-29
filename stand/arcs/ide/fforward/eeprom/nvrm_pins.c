#ident	"fforward/eeprom/nvrm_pins.c:  $Revision: 1.6 $"

#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "eeprom.h"
#include "libsk.h"
#include "uif.h"

#define	REPETITION	5		/* number of times to test signals */


/*
   EEPROM signals test
*/
int 
eeprom_pins(void)
{
   int 					counter;
   volatile unsigned char 		*cpu_aux_cntrl = (unsigned char *) 
					   PHYS_TO_COMPATK1(CPU_AUX_CONTROL);
   int					error_count = 0;
#ifdef IP20
   unsigned char			health_led_off =
					   *cpu_aux_cntrl & CONSOLE_LED;
#endif

   msg_printf (VRB, "Non-volatile ram (EEPROM) signals test\n");

   /* keep the dbg2 chips reset
    */
   *cpu_aux_cntrl &= ~SERCLK;
   *cpu_aux_cntrl |= NVRAM_PRE;

   /*
      test the CS signal (console cs bit in the cpu aux control register)
   */
   for (counter = 0; counter < REPETITION; counter++) {
      *cpu_aux_cntrl |= CONSOLE_CS;
      if ((*cpu_aux_cntrl & CONSOLE_CS) != CONSOLE_CS) {
         error_count++;
         msg_printf (ERR, "Cannot set CS in the cpu aux control register\n");
	 break;
      }   /* if */

      *cpu_aux_cntrl &= ~CONSOLE_CS;
      if ((*cpu_aux_cntrl & CONSOLE_CS) != 0x00) {
         error_count++;
         msg_printf (ERR, "Cannot clear CS in the cpu aux control register\n");
	 break;
      }   /* if */
   }   /* for */

   /*
      test the PRE signal (console led bit in the cpu aux control register) 
   */
   for (counter = 0; counter < REPETITION; counter++) {
      *cpu_aux_cntrl |= NVRAM_PRE;
      if ((*cpu_aux_cntrl & NVRAM_PRE) != NVRAM_PRE) {
         error_count++;
         msg_printf (ERR, "Cannot set PRE in the cpu aux control register\n");
	 break;
      }   /* if */
      DELAY (250000);

      *cpu_aux_cntrl &= ~NVRAM_PRE;
      if ((*cpu_aux_cntrl & NVRAM_PRE) != 0x00) {
         error_count++;
         msg_printf (ERR, "Cannot clear PRE in the cpu aux control register\n");
	 break;
      }   /* if */
      DELAY (250000);
   }   /* for */

#ifdef IP20
   if (!error_count && health_led_off)
      *cpu_aux_cntrl |= CONSOLE_LED;
#endif

   /* keep the dbg2 chips reset
    */
   *cpu_aux_cntrl |= NVRAM_PRE;

   /*
      test the SK signal (serclk in the cpu aux control register) 
   */
   for (counter = 0; counter < REPETITION; counter++) {
      *cpu_aux_cntrl |= SERCLK;
      if ((*cpu_aux_cntrl & SERCLK) != SERCLK) {
         error_count++;
         msg_printf (ERR, "Cannot set SK in the cpu aux control register\n");
	 break;
      }   /* if */

      *cpu_aux_cntrl &= ~SERCLK;
      if ((*cpu_aux_cntrl & SERCLK) != 0x00) {
         error_count++;
         msg_printf (ERR, "Cannot clear SK in the cpu aux control register\n");
	 break;
      }   /* if */
   }   /* for */

   *cpu_aux_cntrl &= ~NVRAM_PRE;
   *cpu_aux_cntrl |= CONSOLE_CS;

   if (error_count)
      sum_error ("non-volatile ram (EEPROM) signals");
   else
      okydoky ();

   return (error_count++);
}   /* eeprom_pins */
