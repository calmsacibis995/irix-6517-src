#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/mman.h>
#include <sys/IP32.h>
#include <IP32_status.h>
#include "uif.h"
#include "piodma.h"
#include "pio.h"
#include "intr.h"
#include "execlib.h"
#include "plptests.h"


extern unsigned char testBuffer[_PAGESZ*2];
extern code_t plp_error_code;


void WriteTestData(unsigned char c)
{

    int timeout;
    long long tmp;

    /* Set the direction bit in the parallel port's        */
    /* control register for mem -> port transfers          */

    SET_PLP_CONTROL(0xc0);
    SET_PLP_DMA_CONTROL((long long)0);
    msg_printf(DBG, "WriteTestData: plp_dma_control 0x%x==0x%llx\n", 
				PLP_DMA_CONTROL_ADDR, PLP_DMA_CONTROL);

    /* Permit DMA to the parallel port by setting the DMA  */
    /* ENABLE and SERVICE INTR bits in the extended        */
    /* configuration register.                             */

    /* Setting the parallel port mode to test mode starts  */
    /* data transfer                                       */

    msg_printf(DBG, "WriteTestData: before plp_ext_cntrl 0x%x==0x%llx\n", 
				PLP_EXT_CNTRL_ADDR, PLP_EXT_CNTRL);
    SET_PLP_EXT_CNTRL(PLP_BI_DIR_MODE);
    msg_printf(DBG, "WriteTestData: after plp_ext_cntrl 0x%x==0x%llx\n", 
				PLP_EXT_CNTRL_ADDR, PLP_EXT_CNTRL);
    SET_PLP_DMA_CONTROL((long long)PLP_DMA_RESET);
    msg_printf(DBG, "WriteTestData: plp_dma_control 0x%x==0x%llx\n", 
				PLP_DMA_CONTROL_ADDR, PLP_DMA_CONTROL);

    SET_PLP_CONTROL(0xcc); /* Select and release reset */
    msg_printf(DBG,"Write to Tester, data = %x.\n",c);

	SET_PLP_DATA(c);
	timeout = 100;
	tmp = (volatile)PLP_STATUS;
	msg_printf(DBG,"status = %llx\n",tmp);
	while(0 != timeout && (!(tmp & PLP_BUSY))){
		timeout--;
		msg_printf(DBG,"busy status = %llx\n",tmp);
		us_delay(1000);
		tmp = (volatile)PLP_STATUS;
	}
	if(!timeout)
		msg_printf(DBG,"timeout1\n");
	tmp = PLP_CONTROL;
	tmp |= PLP_STB;
	SET_PLP_CONTROL(tmp);
	us_delay(1000);
	tmp = PLP_CONTROL;
	tmp &= ~PLP_STB;
	SET_PLP_CONTROL(tmp);
	tmp = PLP_STATUS;
	msg_printf(DBG,"status = %llx\n",tmp);
	us_delay(1000);
	timeout = 100;
	while (0 != timeout && !(tmp & PLP_ACK)){
		timeout--;
		us_delay(100);
		tmp = PLP_STATUS;
	}
	if(!timeout)
		msg_printf(DBG,"timeout2\n");
	msg_printf(DBG,"status = %llx\n",tmp);
}

unsigned char
ReadTestData()
{
    unsigned char c;
    long long tmp;
    int timeout;


    /* Set the direction bit in the parallel port's        */
    /* control register for port -> mem transfers.         */

    SET_PLP_CONTROL(PLP_DIR | 0xca);

    msg_printf(DBG,"Tester data read.\n");
    us_delay(100);
    SET_PLP_CONTROL(PLP_DIR | 0xce); /* release reset */
	tmp = PLP_STATUS;
	msg_printf(DBG,"status = %llx\n",tmp);
	timeout = 100;
	while(0 != timeout && ((tmp & PLP_BUSY))){
		timeout--;
		msg_printf(DBG,"busy status = %llx\n",tmp);
		us_delay(1000);
		tmp = PLP_STATUS;
	}
	if(!timeout)
		msg_printf(DBG,"timeout3\n");
	tmp = PLP_STATUS;
	msg_printf(DBG,"status = %llx\n",tmp);
	c=PLP_DATA;
	tmp = PLP_CONTROL;
	tmp &= ~PLP_STB;
	SET_PLP_CONTROL(tmp);
    return(c);
}

int ParallelPortExternalLoopbackTest(int length)
{
    unsigned char c;
    int i, context, error, timeout;

    StartTest("Memory --> Parallel Port Loopback via DMA", 0, 0);
    /* After Power-On the ISA and parallel port DMA channel    */
    /* are held reset.  Negate the reset on the ISA bus and    */
    /* parallel port DMA channel.                              */

    SET_ISA_BASE_AND_RESET(ISA_BASE_AND_RESET & ~ISA_RESET);

    CLEAR_PLP_DMA_CONTROL(PLP_DMA_RESET);

    msg_printf(VRB,"ISA Bus and Parallel Port reset negated.\n");

#if 1
    /* Verify that no parallel port interrupt is pending.  */
    if (GetInterruptStatus() & PLP_INTR_MASK) {
	if (ISA_INTR_STATUS & 
	    (PLP_A_DMA_DONE | PLP_B_DMA_DONE)) {
	    plp_error_code.w1 = GetInterruptStatus();
	    ErrorMsg(0x0101,"Unexpected parallel port interrupt");
	    return(1);
	}
    }
#endif
    error = 0;
    SET_PLP_CONTROL(0xc0);
    for (i = 0; i < 256; i++) {
   	WriteTestData(i);
    	c = ReadTestData();

	if (i != (int)c){
	    plp_error_code.w1 = c;
	    plp_error_code.w2 = i;
	    ErrorMsg(0x0102, "Parallel Port loopback test failed.");
	    msg_printf(DBG,"expected %x, actual %x\n",i,c);
	    error++;
        }
    }
    return(error);
}

plp_external_loopback()
{
    int result;
    int length;

    plp_error_code.test = 8;
    length = 80;
    if (length < 1 || length > 8192) {
        msg_printf(ERR,"Supplied length is out of range.\n");
        return(1);
    }

    RESET_PLP_DMA_CONTROL;

    /* Call test here */
    result= ParallelPortExternalLoopbackTest(length);
    if (result) {
        sum_error("Parallel port loopback test");
    } else {
        okydoky();
    }
    return(result);
}

