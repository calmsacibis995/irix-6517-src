#include <sys/types.h>
#include <sys/IP20.h>
#include <sys/sbd.h>

#include "IP20util.h"

static volatile u_char *cpu_auxctl = (u_char *)PHYS_TO_K1(CPU_AUX_CONTROL);

#define	BITS_IN_COMMAND	11


void nvram_cs_on(void);
void nvram_cs_off(void);

/*
 * clock in the nvram command and the register number.  For the
 * national semiconductor NVRAM chip the op code is 3 bits and
 * the address is 8 bits. 
 */
void
nvram_cmd(u_int cmd, u_int reg)
{
	u_short ser_cmd;
	int i;

	ser_cmd = cmd | (reg << (16 - BITS_IN_COMMAND));
	for (i = 0; i < BITS_IN_COMMAND; i++) {
		if (ser_cmd & 0x8000)
			*cpu_auxctl |= CPU_TO_SER;
		else
			*cpu_auxctl &= ~CPU_TO_SER;
		*cpu_auxctl &= ~SERCLK;
		*cpu_auxctl |= SERCLK;
		ser_cmd <<= 1;
	}
	*cpu_auxctl &= ~CPU_TO_SER;	/* see timing diagram */
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
	while (!(*cpu_auxctl & SER_TO_CPU) && timeout--)
		us_delay(NVDELAY_TIME);

	if (!(*cpu_auxctl & SER_TO_CPU))
		error = -1;
	else
		error = 0;
	nvram_cs_off();

	return error;
}



/* enable the serial memory by setting the console chip select */
void
nvram_cs_on(void)
{
	*cpu_auxctl &= NVRAM_PRE;	/* see timing diagram */
	*cpu_auxctl |= CONSOLE_CS;
	*cpu_auxctl |= SERCLK;
}



/* turn off the chip select */
void
nvram_cs_off(void)
{
	*cpu_auxctl &= ~SERCLK;
	*cpu_auxctl &= ~CONSOLE_CS;
	*cpu_auxctl |= SERCLK;
}
