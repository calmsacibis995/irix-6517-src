#include <stdio.h>
#include <IP32_status.h>

#include "datapath.h"
#include "errnum.h"
#include "piodma.h"
#include "uif.h"

extern code_t plp_error_code;

plp_walking0()
{
    int result;
    char buffer[100];

    StartTest("Walking a 0 through PLP Dma Context Register A", 8, 0);
    result = PartialWalkingZero(PLP_DMA_CNTXT_A_ADDR,
                                64,PLP_DMA_CNTXT_VALID_BITS);
    if ( result ) {
	msg_printf(ERR,
	    "Bit %d of Context Register A is stuck at a one",
	    result);
	ErrorMsg(CONTEXT_B_STUCK_AT_ZERO, buffer);
    }
    EndTest("Walking a 0 through PLP Dma Context Register A");
    if (result) {
    	sum_error("Parallel port DMA Context Register A");
    } else {
    	okydoky();
    }
}

