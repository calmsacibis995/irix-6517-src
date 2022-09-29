#ident	"$Revision: 1.8 $"

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
   EEPROM address test
*/
int 
eeprom_address(void)
{
   SIGNALHANDLER			prev_handler;
   unsigned short 			address;
   unsigned short 			address_data;
   unsigned short			address_mask;
   int					complement;
   extern int				number_of_eeprom_register;
   unsigned short 			register_read;


#if IP22
   if (!is_fullhouse()) {
	return (0);
   }
#endif

   msg_printf (VRB, "Non-volatile ram (EEPROM) address test\n");

   size_eeprom ();
   address_mask = number_of_eeprom_register - 1;
   if ((protect_register = read_protect_register ()) == BAD_EEPROM) {
      sum_error ("non-volatile ram (EEPROM) address");
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
      write address, address complement, to EEPROM registers, then read
      back and verify
   */
   for (complement = 0; complement <= 1; complement++) {
      busy (1);
      for (address = 0; address < protect_register; address++) {
	 address_data = complement ? (~address & address_mask) : address;
         if (eeprom_write (address, address_data)) {
            sum_error ("non-volatile ram (EEPROM) address");
            Signal (SIGINT, prev_handler);
	    return (++error_count);
	 }   /* if */
      }   /* for */

      busy (1);
      for (address = 0; address < protect_register; address++) {
	 address_data = complement ? (~address & address_mask) : address;
         register_read = eeprom_read (address);
         if (register_read != address_data) {
	    error_count++;
	    msg_printf (ERR,
	       "EEPROM register: 0x%x, Expected: 0x%04x, Actual: 0x%04x\n",
	       address, address_data, register_read);
         }   /* if */
      }   /* for */
   }   /* for */

   /* 
      restore content of the EEPROM 
   */
   busy (1);
   for (address = 0; address < protect_register; address++)
      if (eeprom_write (address, eeprom_register[address])) {
         sum_error ("non-volatile ram (EEPROM) address");
         Signal (SIGINT, prev_handler);
	 return (++error_count);
      }   /* if */

   busy (0);

   if (error_count)
      sum_error ("non_volatile ram (EEPROM) address");
   else
      okydoky ();

   Signal (SIGINT, prev_handler);

   return (error_count);
}   /* eeprom_address */

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
