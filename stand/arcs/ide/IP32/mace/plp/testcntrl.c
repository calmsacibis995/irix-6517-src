#include "datapath.h"
#include "errnum.h"
#include "piodma.h"
#include "msglib.h"
#include <sys/IP32.h>
#include <uif.h>
#include <IP32_status.h>

/* Verify that DMA Enable, DMA Reset and DMA Direction bits in the    */
/* parallel port DMA control are accessible                           */

extern code_t plp_error_code;


int TestBitsInControlRegister()
{
    int reg;
    char buffer[100];
    unsigned xerror = 0;

    plp_error_code.test = 2;

    /* Ensure PLP DMA Channel is reset */
    RESET_PLP_DMA_CONTROL;

    if ((reg = PLP_DMA_CONTROL) != PLP_DMA_RESET) {
	ErrorMsg(PLP_DMA_RESET_BIT_STUCK_AT_ZERO,
		 "PLP DMA Reset bit stuck at zero");
	xerror++;
    }

    plp_error_code.test = 3;
    /* Negate reset to permit other bits other bit in control register */
    /* to be modified.                                                 */
    CLEAR_PLP_DMA_CONTROL(PLP_DMA_RESET);
    if ((reg = PLP_DMA_CONTROL) != 0X0) {
	ErrorMsg(PLP_DMA_RESET_BIT_STUCK_AT_ONE,
		 "PLP DMA Reset bit stuck at one");
	xerror++;
    }

    plp_error_code.test = 4;
    SET_PLP_DMA_CONTROL(PLP_DMA_DIRECTION);
    if ((reg = PLP_DMA_CONTROL) != PLP_DMA_DIRECTION) {
	ErrorMsg(PLP_DMA_DIRECTION_BIT_STUCK_AT_ZERO,
		 "PLP DMA Direction bit stuck at zero");
	xerror++;
    }

    plp_error_code.test = 5;
    CLEAR_PLP_DMA_CONTROL(PLP_DMA_DIRECTION);
    if ((reg = PLP_DMA_CONTROL) & PLP_DMA_DIRECTION != 0X0) {
	ErrorMsg(PLP_DMA_DIRECTION_BIT_STUCK_AT_ONE,
		 "PLP DMA Direction bit stuck at one");
	xerror++;
    }

    plp_error_code.test = 6;
    SET_PLP_DMA_CONTROL(PLP_DMA_ENABLE);
    if ((reg = PLP_DMA_CONTROL) != PLP_DMA_ENABLE) {
	ErrorMsg(PLP_DMA_ENABLE_BIT_STUCK_AT_ZERO,
		 "PLP DMA Enable stuck at zero");
	xerror++;
    }

    plp_error_code.test = 7;
    CLEAR_PLP_DMA_CONTROL(PLP_DMA_ENABLE);
    if ((reg = PLP_DMA_CONTROL) != 0X0) {
	ErrorMsg(PLP_DMA_ENABLE_BIT_STUCK_AT_ONE,
		 "PLP DMA Enable stuck at one");
	xerror++;
    }

    /* Ensure PLP DMA Channel is reset */
    RESET_PLP_DMA_CONTROL;
}
