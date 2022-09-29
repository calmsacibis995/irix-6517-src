#ident	"IP12diags/eeprom/nvrm_rdwr.c:  $Revision: 1.6 $"

#include "sys/cpu.h"
#include "sys/sbd.h"
#include "eeprom.h"
#include "uif.h"

#define	BIT_IN_EEPROM_DATA	16
#ifdef IP20
#define HEALTH_LED_VARIABLE	unsigned char health_led_off
#define	GET_HEALTH_LED		health_led_off = *cpu_aux_control & CONSOLE_LED
#define	RESTORE_HEALTH_LED	*cpu_aux_control = (*cpu_aux_control &	\
				   ~CONSOLE_LED) | health_led_off
#else
#define HEALTH_LED_VARIABLE
#define	GET_HEALTH_LED		
#define	RESTORE_HEALTH_LED		
#endif
#define	HOLD_TIME(x)		{					\
				   volatile int	counter;		\
				   for (counter = MAX_CPU_RATE * x;	\
				        counter;			\
				        counter--);			\
				}
#define	MAX_CPU_RATE		20
#define	MSB_IN_EEPROM_DATA	(1 << (BIT_IN_EEPROM_DATA - 1))
#define RESET_CS		*cpu_aux_control &= ~CONSOLE_CS
#define RESET_PRE		*cpu_aux_control &= ~NVRAM_PRE
#define	RESET_SERIAL_CLOCK	HOLD_TIME(5); 				\
				*cpu_aux_control &= ~SERCLK
#define SET_CS			*cpu_aux_control |= CONSOLE_CS
#define SET_PRE			*cpu_aux_control |= NVRAM_PRE
#define	SET_SERIAL_CLOCK	HOLD_TIME(5); 				\
				*cpu_aux_control |= SERCLK
#define	SHIFT_IN_LEADING_0	*cpu_aux_control &= ~CPU_TO_SER;	\
				SET_SERIAL_CLOCK;			\
				RESET_SERIAL_CLOCK
#define	WAIT_FOR_READY		HOLD_TIME(5);				\
				SET_CS;					\
				{					\
				   volatile int	counter;		\
				   for (counter = MAX_CPU_RATE * 10000;	\
				        counter; 			\
				        counter--)			\
				      if (*cpu_aux_control & SER_TO_CPU) { \
					 eeprom_ready = 1;		\
					 break;				\
				      }   /* if */			\
				}					\
				RESET_CS;				\
				HOLD_TIME(5)

static int				bit_in_address;
static int				bit_in_command;
static volatile unsigned char 		*cpu_aux_control = (unsigned char *) 
					   PHYS_TO_K1(CPU_AUX_CONTROL);
int					number_of_eeprom_register;

/*
   following is a set of routines used by the EEPROM diagnostics
   according to the timing diagrams in the EEPROM data sheets, a 0 must be
   shifted into the EEPROM before any COMMAND cycle
   size_eeprom () must be called before other routines to initialize all 
   static and global variables
   all routines should restore the health led to it original state 
*/



/*
   shift data into the EEPROM, data should be left justified
*/
static void 
eeprom_shift_in(unsigned short data, int number_of_bit)
{
   while (--number_of_bit >= 0) {
      if (data & MSB_IN_EEPROM_DATA)
	 *cpu_aux_control |= CPU_TO_SER;
      else
	 *cpu_aux_control &= ~CPU_TO_SER;
      SET_SERIAL_CLOCK;
      RESET_SERIAL_CLOCK;
      data <<= 1;
   }   /* while */
}   /* eeprom_shift_in */



/*
   shift data out of the EEPROM, data is right justified 
*/
static unsigned short 
eeprom_shift_out (int number_of_bit)
{
   unsigned short 			data = 0x0000;


   while (--number_of_bit >= 0) {
      SET_SERIAL_CLOCK;
      if (*cpu_aux_control & SER_TO_CPU)
	 data |= 1 << number_of_bit;
      RESET_SERIAL_CLOCK;
   }   /* while */

   return (data);
}   /* eeprom_shift_out */



/*
   read from a EEPROM register 
*/
unsigned short 
eeprom_read (unsigned short register_address)

{
   unsigned short 			data;
   HEALTH_LED_VARIABLE;

   GET_HEALTH_LED;
   RESET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   eeprom_shift_in (SER_READ | (register_address << (13 - bit_in_address)), 
      bit_in_command);
   data = eeprom_shift_out (BIT_IN_EEPROM_DATA);

   RESET_CS;
   RESTORE_HEALTH_LED;

   return (data);
}   /* eeprom_read */



/*
   enable the EEPROM for writing 
*/
static void 
eeprom_wen (void)

{
   HEALTH_LED_VARIABLE;

   GET_HEALTH_LED;
   RESET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   eeprom_shift_in (SER_WEN, bit_in_command);

   RESET_CS;
   RESTORE_HEALTH_LED;
}   /* eeprom_wen */



/*
   disable the EEPROM for writing 
*/
static void 
eeprom_wds (void)

{
   HEALTH_LED_VARIABLE;

   GET_HEALTH_LED;
   RESET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   eeprom_shift_in (SER_WDS, bit_in_command);

   RESET_CS;
   RESTORE_HEALTH_LED;
}   /* eeprom_wds */



/*
   write to a EEPROM register
*/
int
eeprom_write (unsigned short register_address, unsigned short data)
{
   int					eeprom_ready = 0;
   int					error_count = 0;
   HEALTH_LED_VARIABLE;

   eeprom_wen ();

   GET_HEALTH_LED;
   RESET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   eeprom_shift_in (SER_WRITE | (register_address << (13 - bit_in_address)),
      bit_in_command);
   eeprom_shift_in (data, BIT_IN_EEPROM_DATA);

   RESET_CS;
   WAIT_FOR_READY;
   RESTORE_HEALTH_LED;

   if (!eeprom_ready) {
      msg_printf (ERR,
	 "EEPROM did not complete WRITE cycle, DO pin did not go high\n");
      error_count++;
   }   /* if */

   eeprom_wds ();

   return (error_count);
}   /* eeprom_write */



/*
   read from the EEPROM protect register
*/
static unsigned short 
eeprom_prread (void)

{
   unsigned short 			register_address;
   HEALTH_LED_VARIABLE;


   GET_HEALTH_LED;
   SET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   eeprom_shift_in (SER_PRREAD, bit_in_command);
   register_address = eeprom_shift_out (bit_in_address);
   (void)eeprom_shift_out (BIT_IN_EEPROM_DATA - bit_in_address);

   RESET_CS;
   RESTORE_HEALTH_LED;

   return (register_address & (number_of_eeprom_register - 1));
}   /* eeprom_prread */



/*
   read the EEPROM protect register and find out whether the security feature
   is enabled or not
*/
unsigned short 
read_protect_register (void)

{
   unsigned short			protect_register;
   unsigned short			register_read;


   protect_register = eeprom_prread ();
   register_read = eeprom_read (protect_register);

   if (eeprom_write (protect_register, ~register_read))
      return (BAD_EEPROM);
   if (eeprom_read (protect_register) == register_read)
      return (protect_register);
   else {
      if (eeprom_write (protect_register, register_read))
	 return (BAD_EEPROM);
      else
	 return (number_of_eeprom_register);
   }   /* if */

}   /* read_protect_register */



/*
   size the EEPROM 
*/
void 
size_eeprom (void)

{
   HEALTH_LED_VARIABLE;


   GET_HEALTH_LED;
   RESET_PRE;
   SET_CS;
   SHIFT_IN_LEADING_0;

   /* 
      assume 3 bits opcode, 6 bits address 
   */
   eeprom_shift_in (SER_READ, 9);
   if (*cpu_aux_control & SER_TO_CPU) {
      number_of_eeprom_register = 128;
      bit_in_address = 8;
      eeprom_shift_in (0x0000, 2);
   }
   else {
      number_of_eeprom_register = 64;
      bit_in_address = 6;
   }   /* if */

   bit_in_command = bit_in_address + 3;
   (void)eeprom_shift_out (BIT_IN_EEPROM_DATA);

   RESET_CS;
   RESTORE_HEALTH_LED;
}   /* size_eeprom */
