#include <stdio.h>

#include "datapath.h"
#include "msglib.h"
#include "errnum.h"
#include "piodma.h"
#include <sys/IP32.h>

#include "uif.h"

int RegisterContentsAfterReset()

{
    long long reg;
    char buffer[100];
    unsigned xerror = 0;

    RESET_PLP_DMA_CONTROL;

    reg = PLP_DMA_CONTROL;
    msg_printf(DBG, "RCAR: control reg==0x%llx\n", reg);
    if ((reg & 0x1F) != 0x4) {
	msg_printf(ERR,
	  "PLP Control reg. contains unexpected value %llx after reset.\n",
	   reg);
	ErrorMsg(CONTROL_BAD_AFTER_RESET, buffer);
	xerror++;
    }
    reg = PLP_DMA_DIAG;
    msg_printf(DBG, "RCAR: diagnostic reg==0x%llx\n", reg);
    if ((reg & 0x3FFF) != 0) {
	msg_printf(ERR,
	  "PLP Diagnostic reg. contains unexpected value %llx after reset.\n",
	   reg);
	ErrorMsg(DIAGNOSTIC_BAD_AFTER_RESET, buffer);
	xerror++;
    }
    return(xerror);
}
