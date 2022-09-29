#ident	"IP22diags/eeprom/nvrm_id.c:  $Revision: 1.5 $"

#include <sys/cpu.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include "saioctl.h"
#include "setjmp.h"
#include "eeprom.h"
#include "uif.h"
#include "libsc.h"

static int catch_intr ();
static int error_count;
static unsigned short protect_register;
static unsigned short eeprom_register[MAX_EEPROM_SIZE];
static jmp_buf catch;

/*
   EEPROM id test
*/
int 
eeprom_id ()

{
   SIGNALHANDLER			prev_handler;
   unsigned short 			address;
   extern int				number_of_eeprom_register;
   unsigned short 			register_read;


   msg_printf (VRB, "Non-volatile ram (EEPROM) security test\n");

   size_eeprom ();
   if ((protect_register = read_protect_register ()) == BAD_EEPROM) {
      sum_error ("non-volatile ram (EEPROM) security");
      return (++error_count);
   }
   else if (protect_register == number_of_eeprom_register) {
      msg_printf (SUM,
	 "\r\t\t\t\t\t\t\t\t- ERROR -\nEEPROM security feature not enabled\n");
      return (++error_count);
   }   /* if */

   /*
      save content of the EEPROM
   */
   for (address = 0; address < number_of_eeprom_register; address++)
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
      write to the protected registers, then read back and make sure none of
      them get modified
   */
   busy (1);
   for (address = protect_register; 
	address < number_of_eeprom_register;
	address++)
      if (eeprom_write (address, ~eeprom_register[address])) {
         sum_error ("non-volatile ram (EEPROM) security");
         Signal (SIGINT, prev_handler);
         return (++error_count);
      }   /* if */

   busy (1);
   for (address = protect_register;
	address < number_of_eeprom_register;
	address++) {
      register_read = eeprom_read (address);
      if (register_read != eeprom_register[address]) {
	 error_count++;
	 msg_printf (ERR,
	    "EEPROM register: 0x%x, Expected: 0x%04x, Actual: 0x%04x\n",
	     address, eeprom_register[address], register_read);
      }   /* if */
   }   /* for */

   /*
      restore content of the EEPROM
   */
   busy (1);
   for (address = 0; address < number_of_eeprom_register; address++)
      if (eeprom_write (address, eeprom_register[address])) {
         sum_error ("non-volatile ram (EEPROM) security");
         Signal (SIGINT, prev_handler);
         return (++error_count);
      }   /* if */

   busy (0);

   if (error_count)
      sum_error ("non-volatile ram (EEPROM) security");
   else
      okydoky ();

   Signal (SIGINT, prev_handler);

   return (error_count);
}   /* eeprom_id */

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
