#include <stdio.h>

#include "datapath.h"
#include "msglib.h"
#include "errnum.h"
#include <sys/IP32.h>
#include "piodma.h"

int WalkOneThroughContextRegisterA()
{
    int result;
    char buffer[100];

    result = PartialWalkingOne(PLP_DMA_CNTXT_A_ADDR,
                                64,PLP_DMA_CNTXT_VALID_BITS);
    if (result > 0) {
	sprintf(buffer,
	    "Bit %d of Context Register A is stuck at a one",
	    result);
	ErrorMsg(CONTEXT_A_STUCK_AT_ONE, buffer);
    }
    return(result);
}


