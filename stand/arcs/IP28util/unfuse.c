#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include "libsc.h"
#include "nvram.h"

#define	RESET_PRE	*cpu_auxctl &= ~NVRAM_PRE
#define	SET_PRE		*cpu_auxctl |= NVRAM_PRE

/*
 * IP26 standalone utility program to unset the NVRAM protect register
 */
main(int argc, char **argv)
{
	int 				bits_in_cmd = size_nvram ();
	volatile unsigned char		*cpu_auxctl = (unsigned char *)
						PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char 			health_led_off = 
						*cpu_auxctl & LED_GREEN;

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0, bits_in_cmd);
	nvram_cs_off();

	SET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_PREN, 0, bits_in_cmd);
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_PRCLEAR, 0xff, bits_in_cmd);
	nvram_cs_off();
	nvram_hold();

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WDS, 0, bits_in_cmd);
	nvram_cs_off();

	*cpu_auxctl = (*cpu_auxctl & ~LED_GREEN) | health_led_off;
	EnterInteractiveMode(); /* Go back to prom monitor mode */

	return (0);
}
