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


unsigned char *testPattern;
extern unsigned char testBuffer[_PAGESZ*2];
extern code_t plp_error_code;


void WriteTestFifo()
{
    /* Set the direction bit in the parallel port's        */
    /* control register for mem -> port transfers          */

    SET_PLP_CONTROL(0);
    SET_PLP_DMA_CONTROL((long long)0);
    msg_printf(DBG, "WriteTestFifo: plp_dma_control 0x%x==0x%llx\n", 
				PLP_DMA_CONTROL_ADDR, PLP_DMA_CONTROL);

    /* Permit DMA to the parallel port by setting the DMA  */
    /* ENABLE and SERVICE INTR bits in the extended        */
    /* configuration register.                             */

    /* Setting the parallel port mode to test mode starts  */
    /* data transfer                                       */

    msg_printf(DBG, "WriteTestFifo: before plp_ext_cntrl 0x%x==0x%llx\n", 
				PLP_EXT_CNTRL_ADDR, PLP_EXT_CNTRL);
    SET_PLP_EXT_CNTRL(PLP_TEST_MODE | PLP_DMA_EN );
    msg_printf(DBG, "WriteTestFifo: after plp_ext_cntrl 0x%x==0x%llx\n", 
				PLP_EXT_CNTRL_ADDR, PLP_EXT_CNTRL);
    SET_PLP_DMA_CONTROL((long long)PLP_DMA_ENABLE);
    msg_printf(DBG, "WriteTestFifo: plp_dma_control 0x%x==0x%llx\n", 
				PLP_DMA_CONTROL_ADDR, PLP_DMA_CONTROL);

    /* Initialize DMA context registers */
    msg_printf(DBG,"W_REG64 0x%x==0x%llx\n", PLP_DMA_CNTXT_A_ADDR,
		PLP_DMA_PHYS_ADDR(&testPattern[0], 8, FALSE));
    INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_A_ADDR, 
                         &testPattern[0], 8,FALSE);

    msg_printf(DBG,"W_REG64 0x%x==0x%llx\n", PLP_DMA_CNTXT_B_ADDR,
		PLP_DMA_PHYS_ADDR(&testPattern[8], 8, TRUE));
    INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_B_ADDR, 
	                 &testPattern[8], 8, TRUE);
 
    InfoMsg("Mem -> Test Fifo DMA started.\n");
    /* Wait until Context A is consumed */

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_A_VALID) == 0), 25,
	"Context A still in use, after 25 usecs elapsed.",
	"Context A consumed."); 
    msg_printf(DBG, "WriteTestFifo: plp_dma_control==0x%llx\n",
		PLP_DMA_CONTROL);

    /* Wait until Context B is consumed */

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_B_VALID) == 0), 25,
	"Context B still in use, after 25 usces elapsed.",
	"Context B consumed."); 
    msg_printf(DBG, "WriteTestFifo: plp_dma_control==0x%llx\n",
		PLP_DMA_CONTROL);
}

void ReadTestFifo()
{
    /* Set the direction bit in the parallel port's        */
    /* control register for port -> mem transfers.         */

    SET_PLP_CONTROL(PLP_DIR);
    SET_PLP_DMA_CONTROL((long long)PLP_DMA_DIRECTION | PLP_DMA_ENABLE);

    /* Permit DMA from the parallel port by setting the    */
    /* DMA ENABLE and SERVICE INTR bits in the extended    */
    /* configuration register.                             */

    /* Setting the parallel port mode to test mode starts  */
    /* data transfer                                       */


    /* Initialize DMA context registers */
    INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_A_ADDR, 
                         &testPattern[0x40], 8, FALSE);

    INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_B_ADDR, 
		         &testPattern[0x80], 8, TRUE);
    SET_PLP_EXT_CNTRL(PLP_TEST_MODE | PLP_DMA_EN);
    InfoMsg("Test Fifo -> Mem DMA started.\n");

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_A_VALID) == 0), 0x4000,
	"Context A still in use, after 25 usecs elapsed.",
	"Context A consumed."); 

    /* Wait until Context B is consumed */

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_B_VALID) == 0), 0x4000,
	"Context B still in use, after 25 usces elapsed.",
	"Context B consumed."); 
   msg_printf(DBG,"End of read\n");

}

int ParallelPortLoopbackTest(int length)
{
    unsigned char *ptr;
    int i, bytes, context, lengthA, lengthB, timeout;

    bytes = _PAGESZ;
    /* Allocate a page of memory */
    ptr = testPattern = (unsigned char *)
	(((((unsigned long)testBuffer>>PNUMSHFT)<<PNUMSHFT))+_PAGESZ);

    /* Fill the first 16 bytes of the newly allocated page     */
    /* with a testPattern */
    msg_printf(DBG,"ParallelPortLoopbackTest:ptr==0x%x(0x%x),lem==0x%x\n",
			ptr, testBuffer, length);

    for (i = 0; i < 16; i++)
	*ptr++ = (unsigned char) (i + 1);

    /* Zero the rest of the page */
    ptr = &testPattern[16];
    msg_printf(DBG,"ParallelPortLoopbackTest:zero ptr==0x%x(0x%x),lem==0x%x\n",
			ptr, testPattern, length-16);
    for (i = 16; i < bytes; i++)
	*ptr++ = 0;

    StartTest("Memory --> Parallel Port Loopback via DMA", 0, 0);
    /* After Power-On the ISA and parallel port DMA channel    */
    /* are held reset.  Negate the reset on the ISA bus and    */
    /* parallel port DMA channel.                              */

    SET_ISA_BASE_AND_RESET(ISA_BASE_AND_RESET & ~ISA_RESET);

    CLEAR_PLP_DMA_CONTROL(PLP_DMA_RESET);

    InfoMsg("ISA Bus and Parallel Port reset negated.\n");

#if 1
    /* Verify that no parallel port interrupt is pending.  */
    if (GetInterruptStatus() & PLP_INTR_MASK) {
	if (ISA_INTR_STATUS & 
	    (PLP_A_DMA_DONE | PLP_B_DMA_DONE)) {
	    ErrorMsg(0x0101,"Unexpected parallel port interrupt");
	}
    }
#endif

    WriteTestFifo();
    ReadTestFifo();

    for (i = 0; i < 8; i++) {
	if (testPattern[i] != testPattern[0x40+i]) {
	    ErrorMsg(0x0102, "Parallel Port loopback test failed.");
	    msg_printf(DBG,"expected %x, actual %x\n",testPattern[i],testPattern[0x40+i]);
        }
    }

    for (i = 0; i < 8; i++) {
	if (testPattern[8+i] != testPattern[0x80+i]) {
	    ErrorMsg(0x0103, "Parallel Port loopback test failed.");
	    msg_printf(DBG,"expected %x, actual %x\n",testPattern[i],testPattern[0x80+i]);
        }
    }

    InfoMsg("Parallel Port loopback test succeeded.\n");
    EndTest("Memory --> Parallel Port Loopback via DMA");
}

plp_loopback()
{

    int result;
    int length;

    plp_error_code.test = 1;
    length = 16;
    if (length < 1 || length > 8192) {
	printf("Supplied length is out of range.\n");
	return(1);
    }

    RESET_PLP_DMA_CONTROL;

    /* Call test here */
    result= ParallelPortLoopbackTest(length);
    if (result) {
    	sum_error("Parallel port loopback test");
    } else {
    	okydoky();
    }
    return(result);
}


