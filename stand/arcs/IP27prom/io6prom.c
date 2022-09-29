#include <sys/types.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/arch.h>
#include <sys/SN/agent.h>
#include "ip27prom.h"

/*
 * This is a false IO6 prom that just blinks LEDs.
 * We can make sure the IP27 segment loader properly decompresses
 * it into memory and transfers control to it.
 *
 * The stack pointer is left alone so we are relying on it still
 * being in the IP27 stack area.
 */

void delay(__uint64_t loops);

void main(void)
{
	while(1) {
		if (SN00)
			SD(LOCAL_HUB(MD_UREG1_0), 48);
		else
			SD(LOCAL_HUB(MD_LED0), 48);

		delay(0x1000000);

		if (SN00)
			SD(LOCAL_HUB(MD_UREG1_0), 0);
		else
			SD(LOCAL_HUB(MD_LED0), 0);

		delay(0x1000000);
	}
}
