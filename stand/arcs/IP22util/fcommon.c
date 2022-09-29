#include <sys/types.h>
#include <sys/IP22.h>
#include <sys/sbd.h>

static volatile unsigned char *ser_wr_reg = 
			    (unsigned char *) PHYS_TO_K1( CPU_AUX_CONTROL );
static volatile unsigned char *ser_rd_reg = 
			    (unsigned char *) PHYS_TO_K1( CPU_AUX_CONTROL );
#define SERDATA_OUT	CPU_TO_SER	/* out of cpu to nvram */
#define SERDATA_IN	SER_TO_CPU	/* into cpu from nvram */

#define	RESET_AUXSERCLK	*cpu_auxctl &= ~SERCLK
#define	SET_AUXSERCLK	*cpu_auxctl |= SERCLK
#define	RESET_PRE	*cpu_auxctl &= ~NVRAM_PRE
#define	SET_PRE		*cpu_auxctl |= NVRAM_PRE

void nvram_cs_on(void);
void nvram_cs_off(void);
/*
   find out number of address bits in the nvram. since this routine will be 
   called from unix, any reference to prom BSS MUST be avoided 
*/

void us_delay(uint);

int
size_nvram ()
{
   int					bit_in_command;
   register int				i;
   register volatile unsigned char 	*cpu_auxctl = (unsigned char *)
					   PHYS_TO_K1(CPU_AUX_CONTROL);
   unsigned char 			health_led_off = 
					   *cpu_auxctl & LED_GREEN;

   RESET_PRE;
   nvram_cs_on ();

   /*
      shift in read command. assume 3 bits opcode, 6 bits address 
   */
   for (i = 0; i < 9; i++) {
      if ((SER_READ << i) & 0x80000000)
	 *ser_wr_reg |= SERDATA_OUT;
      else
	 *ser_wr_reg &= ~SERDATA_OUT;
      RESET_AUXSERCLK;
      SET_AUXSERCLK;
   }   /* for */

   if (*ser_rd_reg & SERDATA_IN) {
      bit_in_command = 11;
      *ser_wr_reg &= ~SERDATA_OUT;
      RESET_AUXSERCLK;
      SET_AUXSERCLK;
      RESET_AUXSERCLK;
      SET_AUXSERCLK;
   }
   else
      bit_in_command = 9;

   /*
      shift out garbage data
   */
   for (i = 0; i < 16; i++) {
      RESET_AUXSERCLK;
      SET_AUXSERCLK;
   }   /* for */
      
   nvram_cs_off ();
   *cpu_auxctl = (*cpu_auxctl & ~LED_GREEN) | health_led_off;

#ifndef LATER
   return(11);
#else /* !LATER */
   return (bit_in_command);
#endif /* !LATER */
}   /* size_nvram */

/*
 * clock in the nvram command and the register number.  For the
 * natl semi conducter nv ram chip the op code is 3 bits and
 * the address is 6/8 bits. 
 */
void
nvram_cmd(unsigned int cmd, unsigned int reg, register int bit_in_command)
{
	ushort ser_cmd;
	int i;
   	volatile unsigned char *cpu_auxctl =
				(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);


	ser_cmd = cmd | (reg << (16 - bit_in_command));	/* left justified */
	for (i = 0; i < bit_in_command; i++) {
		if (ser_cmd & 0x8000)	/* if high order bit set */
			*ser_wr_reg |= SERDATA_OUT;
		else
			*ser_wr_reg &= ~SERDATA_OUT;
		RESET_AUXSERCLK;
		SET_AUXSERCLK;
		ser_cmd <<= 1;
	}
	*ser_wr_reg &= ~SERDATA_OUT;	/* see data sheet timing diagram */
}

/*
 * after write/erase commands, we must wait for the command to complete
 * write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */

int
nvram_hold(void)
{
	int error;
	int timeout = NVDELAY_LIMIT;

	nvram_cs_on();
	while (!(*ser_rd_reg & SERDATA_IN) && timeout--)
		us_delay(NVDELAY_TIME);

	if (!(*ser_rd_reg & SERDATA_IN))
		error = -1;
	else
		error = 0;
	nvram_cs_off();

	return (error);
}



/*
 * the version of nvram_cs_on() and nvram_cs_off() we have here is slightly
 * different than the one in stand/saio/ml/IP12.c.
 * THEY ARE NOT INTERCHANGEABLE
 */ 

/*
 * enable the serial memory by setting the console chip select
 */
void
nvram_cs_on()
{
	volatile unsigned char *cpu_auxctl =
		(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char cs_on;
	int delaylen = 100;

	cs_on = *cpu_auxctl & ~(SERCLK | CONSOLE_CS);
	*cpu_auxctl &= ~CPU_TO_SER;	/* see data sheet timing diagram */
	RESET_AUXSERCLK;
	*cpu_auxctl = cs_on;
	while (--delaylen)
		;
	*cpu_auxctl = cs_on | CONSOLE_CS;
	SET_AUXSERCLK;
}



/*
 * turn off the chip select
 */
void
nvram_cs_off()
{
	volatile unsigned char *cpu_auxctl =
		(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char cs_off;

	cs_off = *cpu_auxctl & ~(SERCLK | CONSOLE_CS);
	RESET_AUXSERCLK;
	*cpu_auxctl = cs_off;
	SET_AUXSERCLK;
}
