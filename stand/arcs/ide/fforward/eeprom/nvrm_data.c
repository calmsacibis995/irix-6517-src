#ident	"fforward/eeprom/nvrm_data.c:  $Revision: 1.8 $"

#include <sys/cpu.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include "saioctl.h"
#include "setjmp.h"
#include "eeprom.h"
#include "uif.h"
#include "libsc.h"

#define	NUMBER_OF_PATTERN	(sizeof(test_pattern) / sizeof(unsigned short))


static int catch_intr ();
static int error_count;
static unsigned short protect_register;
static unsigned short eeprom_register[MAX_EEPROM_SIZE];
static jmp_buf catch;

/*
   EEPROM data test
*/
int
eeprom_data(void)
{
   SIGNALHANDLER			prev_handler;
   unsigned short 			address;
   int 					pattern;
   unsigned short 			register_read;
   static unsigned short 		test_pattern[] = {
      					   0xffff,
      					   0xff00,
      					   0xf0f0,
      					   0xcccc,
      					   0xaaaa,
					   0x0000
   					};

#if IP22
   if (!is_fullhouse()) {
	return (0);
   }
#endif

   msg_printf (VRB, "Non-volatile ram (EEPROM) data test\n");

   size_eeprom ();
   if ((protect_register = read_protect_register ()) == BAD_EEPROM) {
      sum_error ("non-volatile ram (EEPROM) data");
      return (++error_count);
   }   /* if */

   /*
      save content of the EEPROM
   */
   for (address = 0; address < protect_register; address++)
      eeprom_register[address] = eeprom_read (address);

   /*
      exit on control-C
   */
   prev_handler = Signal (SIGINT, (void (*)()) catch_intr);
   if (setjmp (catch)) {
      Signal (SIGINT, prev_handler);
      bzero (catch, sizeof(jmp_buf));
      return (error_count);
   }   /* if */

   busy (0);

   /* 
      write different patterns to EEPROM registers, then read back and verify
   */
   for (pattern = 0; pattern < NUMBER_OF_PATTERN; pattern++) {
      busy (1);
      for (address = 0; address < protect_register; address++) {
	 if (eeprom_write (address, test_pattern[pattern])) {
	    sum_error ("non-volatile ram (EEPROM) data");
            Signal (SIGINT, prev_handler);
	    return (++error_count);
	 }   /* if */

	 register_read = eeprom_read (address);
	 if (register_read != test_pattern[pattern]) {
	    error_count++;
	    msg_printf (ERR,
	       "EEPROM register: 0x%x, Expected: 0x%04x, Actual: 0x%04x\n",
	       address, test_pattern[pattern], register_read);
	 }   /* if */
      }   /* for */
   }   /* for */

   /*
      restore content of the EEPROM
   */
   busy (1);
   for (address = 0; address < protect_register; address++)
      if (eeprom_write (address, eeprom_register[address])) {
	 sum_error ("non-volatile ram (EEPROM) data");
         Signal (SIGINT, prev_handler);
	 return (++error_count);
      }   /* if */

   busy (0);

   if (error_count)
      sum_error ("non-volatile ram (EEPROM) data");
   else
      okydoky ();

   Signal (SIGINT, prev_handler);

   return (error_count);
}   /* eeprom_data */

static int
catch_intr ()
{
	unsigned short address;

	busy (0);
	for (address = 0; address < protect_register; address++)
		if (eeprom_write (address, eeprom_register[address])) {
			sum_error ("non-volatile ram (EEPROM) data");
			error_count++;
		}
	longjmp (catch, 1);

	/*NOTREACHED*/
}
