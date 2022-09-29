#include <stdio.h>

#include "datapath.h"
#include "msglib.h"
#include "errnum.h"
#include "piodma.h"

int WalkZeroThroughContextRegisterA()
{
    int result;
    char buffer[100];
    result = PartialWalkingZero(PLP_DMA_CNTXT_A_ADDR,
                                64,PLP_DMA_CNTXT_VALID_BITS);
    if (result > 0) {
	sprintf(buffer,
	    "Bit %d of Context Register A is stuck at a one",
	    result);
	ErrorMsg(CONTEXT_B_STUCK_AT_ZERO, buffer);
    }
    return(result);
}

