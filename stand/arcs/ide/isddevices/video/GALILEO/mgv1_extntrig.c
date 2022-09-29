#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "ide_msg.h"
#include "uif.h"

#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

extern  mgras_hw *mgbase;

#define HPC3_EXT_IO_ADDR_P0              0x1fbd9900      /* 7,0x00 */


mgv1_ev1_extn_trig(int argc, char *argv[])
{
	int errCnt = 0;
	int timeout = 0;
	uint addr;
#define STATUS_TIMEOUT 50000
	
	while (!((addr = *(volatile uint *)PHYS_TO_K1(HPC3_EXT_IO_ADDR_P0)) & 
			0x1)) {
		if (timeout >= STATUS_TIMEOUT)
			break;
		timeout++;
		DELAY(1);
	}
	if (timeout < STATUS_TIMEOUT) {
		msg_printf(VRB,"Got the external trigger\n");
	}
	else {
		msg_printf(ERR,"Never got the external trigger, status = 0x%x\n",addr);
		errCnt++;
	}

	if (errCnt)
		sum_error("External Trigger");
	else
		okydoky();

	return(errCnt);
}
