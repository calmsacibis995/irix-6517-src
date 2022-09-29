#include <sys/types.h>
#include <sys/IP32.h>
#include <stdlib.h>
#include "intr.h"
#include "execlib.h"
#include "plptests.h"

unsigned long zzHwSync;

void EnableInterrupt(unsigned long mask)
{
    SET_ISA_INTR_ENABLE(ISA_INTR_ENABLE | mask);
    zzHwSync = ISA_INTR_ENABLE;
}

void DisableInterrupt(unsigned long mask)
{
    SET_ISA_INTR_ENABLE(ISA_INTR_ENABLE & ~mask);
    zzHwSync = ISA_INTR_ENABLE;
}

boolean_t InterruptOccurred(unsigned long mask, unsigned long usecs)
{
    unsigned long long currentTime = UST;
    unsigned long long ticks = (usecs * 1000) / NS_PER_TICK;
    unsigned long long elapsedTime;
    
    do {
	delay(3000);
 	elapsedTime = UST - currentTime;
	if ((GetInterruptStatus() & PLP_INTR_MASK) &
            (ISA_INTR_STATUS & mask))
	    return TRUE;
    } while (elapsedTime < ticks);
    return FALSE;
}
