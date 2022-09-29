#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "sys/ng1hw.h"
#include "regio.h"
#include "ide_msg.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern struct gr2_hw  *base ;


#define         RED             0x0
#define         GREEN           0x8
#define         BLUE            0x10


#define DISPLAY_BUS_WIDE 8

/*ARGSUSED*/
int
ev1_disp_bus(int argc, char *argv[])
{
	int channel, index, exp, rcv, errCnt;
	char rgb[10];
	void freeze_all_ABs(void);

	msg_printf (VRB, "Display control bus AB1 internal reg test .....\n");
	errCnt = 0;
	freeze_all_ABs ();

	/* AB1 internal Reg used */
	for(index=0; index<5;index++) {
		exp = (1<< index);

		AB_W1 (base, sysctl, exp);

		/* Wait,read back and verify */
		DELAY(2);
		rcv = AB_R1 (base, sysctl);
		msg_printf(DBG,"ev1_disp_bus:: exp=0x%x  rcv=0x%x\n", exp, rcv);
		if (exp!= rcv){
			msg_printf(ERR,"disp_bus: (AB1 Internal Reg Test) Walking One Test, ExpVal= 0x%x RcvVal= 0x%x\n", exp, rcv);
			++errCnt;

		}
	}

	if (errCnt == 0) {
		msg_printf(VRB,"disp_bus test (AB1 Internal) Reg Test Passed\n");
	} else {
		msg_printf(ERR,"disp_bus test (AB1 Internal) Reg Test FAILED\n");
	}

	/* Walking One test across the display bus (8 bits wide ) */
	channel = RED;
	sprintf(rgb, "RED CHANNEL"); 
	while (channel < 24) {
		/* Select the desired AB1 channel, also diagnostic mode  */
		AB_W1 (base, sysctl, channel);
		msg_printf(VRB,"%s disp_bus test\n" , rgb);
		for(index=0; index<DISPLAY_BUS_WIDE;index++) {
			exp = (1<< index);
	
			AB_W1 (base, high_addr, 0 );
			AB_W1 (base, low_addr, 0 );
			AB_W1 (base, dRam, exp);
	
			/* Wait, Read Back and verify */
			DELAY(1);
			AB_W1 (base, high_addr, 0 );
			AB_W1 (base, low_addr, 0 );
			rcv = AB_R1 (base, dRam) ;
	
			msg_printf(DBG,"%s disp_bus:: exp=0x%x  rcv=0x%x\n", rgb, exp, rcv);
			if (exp!= rcv){
				msg_printf(ERR,"%s disp_bus: Walking One Test, ExpVal= 0x%x RcvVal= 0x%x\n", rgb, exp, rcv);
				++errCnt;

			}
		}
		channel += 0x8;
		if (channel == GREEN)
			sprintf(rgb, "GREEN CHANNEL"); 
		else if (channel == BLUE)
			sprintf(rgb, "BLUE CHANNEL"); 
	}

	errCnt = 0;

	msg_printf(VRB,"disp_bus test CC1 Internal Reg Test. \n");
	for (index=0; index<80; index++) {
		exp = index;
		CC_W1 (base, indrct_addreg, exp);
		rcv = CC_R1 (base, indrct_addreg) ;
		msg_printf(DBG,"%s disp_bus:: exp=0x%x  rcv=0x%x\n", "CC1", exp, rcv);
		if (exp != rcv) {
			msg_printf(ERR,"%s disp_bus:: exp=0x%x  rcv=0x%x\n", "CC1", exp, rcv);
			errCnt++;
		}
	}

        if (errCnt == 0) {
                msg_printf(VRB,"disp_bus test (CC1 Internal) Reg Test Passed\n");        } else {
                msg_printf(ERR,"disp_bus test (CC1 Internal) Reg Test FAILED\n");        }


	if (errCnt)
		sum_error("ev1 display bus");
	else
		okydoky();

	return(errCnt);
}

