#include <sys/types.h>
#include <sys/IP20.h>
#include <sys/IP20nvram.h>
#include <sys/sbd.h>

#include "IP20util.h"

/* IP20 standalone utility program to unset the NVRAM protect register */
main()
{
	volatile u_char *cpu_auxctl = (u_char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	u_char health_led_off = *cpu_auxctl & CONSOLE_LED;

	*cpu_auxctl &= ~NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0);
	nvram_cs_off();

	*cpu_auxctl |= NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_PREN, 0);
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_PRCLEAR, 0xff);
	nvram_cs_off();
	if (nvram_hold())
		printf("Error -- cannot clear NVRAM protect register\n");

	*cpu_auxctl &= ~NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WDS, 0);
	nvram_cs_off();

	*cpu_auxctl = (*cpu_auxctl & ~CONSOLE_LED) | health_led_off;
}
