#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "ide_msg.h"
#include "uif.h"


extern struct gr2_hw  *base ;

#define DISPLAY_BUS_WIDE 8

ev1_disp_bus(int argc, char *argv[])
{
	int index, exp, rcv, errCnt;

	errCnt = 0;
	/* Walking One test across the display bus (8 bits wide ) */
	for(index=0; index<DISPLAY_BUS_WIDE;index++) {
#ifdef OLD
		exp = (1<< index) & 0x1F;
#endif
		exp = (1<< index);

		base->ab1.high_addr= 0 ;
		base->ab1.low_addr= 0 ;
		base->ab1.dRam = exp;

		/* Wait, Read Back and verify */
		DELAY(1);
		base->ab1.high_addr= 0 ;
		base->ab1.low_addr= 0 ;
		rcv = base->ab1.dRam ;

#ifdef OLD
		base->ab1.sysctl = exp;

		/* Wait,read back and verify */
		DELAY(1);
		rcv = base->ab1.sysctl;
#endif
		msg_printf(DBG,"ev1_disp_bus:: exp=0x%x  rcv=0x%x\n", exp, rcv);
		if (exp!= rcv){
			msg_printf(ERR,"disp_bus: Walking One Test, ExpVal= 0x%x RcvVal= 0x%x\n", exp, rcv);
			++errCnt;

		}
	}

	if (errCnt)
		sum_error("ev1 display bus");
	else
		okydoky();

	return(errCnt);
}

