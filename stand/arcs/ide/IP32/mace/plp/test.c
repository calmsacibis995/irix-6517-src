#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/mman.h>
#include "piodma.h"
#include "pio.h"
#include "intr.h"
#include "execlib.h"

#include "uif.h"
#include "plptests.h"
/*typedef enum { FALSE = 0, TRUE = 1} boolean_t;*/
unsigned char testBuffer[_PAGESZ*3];

int ParallelPortFillTest(int length)
{
    unsigned char *testPattern, *ptr;
    int i, bytes, context, lengthA, lengthB, timeout;
    char buffer[100];

    bytes = _PAGESZ*2;
    /* Allocate a page of memory */
    ptr = testPattern = (unsigned char *)
	(((((unsigned long)testBuffer>>PNUMSHFT)<<PNUMSHFT))+_PAGESZ);
    msg_printf(DBG, "ParallelPortFillTest: ptr==0x%x, length==0x%x (0x%x)\n",
			ptr, length, bytes);

    lengthA = lengthB = length / 2;

    lengthA = lengthA + (length - (lengthA * 2));

    timeout = lengthA / 2;

    if (timeout < 50) 
	timeout = 50;

    /* Fill the newly allocated page with a testPattern */
    for (i = 0; i < bytes/2; i++)
	*ptr++ = (unsigned char) i;

    ptr = testPattern = (unsigned char *)
	((((((unsigned long)testBuffer>>PNUMSHFT)<<PNUMSHFT))+_PAGESZ)
								+(bytes/2));
    msg_printf(DBG, "ParallelPortFillTest: ptr==0x%x, length==0x%x (0x%x)\n",
			ptr, length, bytes);
    for (i = lengthA; i < ((bytes/2) + lengthA); i++)
	*ptr++ = (unsigned char) (i & 0xff);

    /* After Power-On the ISA and parallel port DMA channel    */
    /* are held reset.  Negate the reset on the ISA bus and    */
    /* parallel port DMA channel.                              */

    SET_ISA_BASE_AND_RESET(ISA_BASE_AND_RESET & ~ISA_RESET);

    CLEAR_PLP_DMA_CONTROL(PLP_DMA_RESET);

    InfoMsg("ISA Bus and Parallel Port reset negated.");

    msg_printf(DBG,"DMA started, lengthA = %d, lengthB = %d.",
            lengthA, lengthB);

    InfoMsg(buffer);
    
    SET_PLP_EXT_CNTRL((long long)0x78);

    /* Initialize DMA context registers */
    INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_A_ADDR, 
                         &testPattern[0],lengthA, FALSE);

    if (lengthB > 0) {
	INIT_PLP_DMA_CONTEXT(PLP_DMA_CNTXT_B_ADDR, 
		&testPattern[bytes/2],lengthB, TRUE);
    }
    /* Set the direction bit in the DMA CONTROL register   */
    /* to select FILL(parallel port to memory) operations  */
    /* and enable DMA transfer.                            */

    SET_PLP_DMA_CONTROL(PLP_DMA_ENABLE);
  
    InfoMsg("Parallel Port DMA contexts initialized.");

    EnableInterrupt(PLP_PORT_INTR | PLP_A_DMA_DONE | 
		    PLP_B_DMA_DONE);
#ifdef NOT_BROKEN
	/* Verify that no parallel port interrupt is pending.  */
    if (GetInterruptStatus() & PLP_INTR_MASK) {
	if (ISA_INTR_STATUS & 
	    (PLP_A_DMA_DONE | PLP_B_DMA_DONE)) {
	    ErrorMsg(1,"Unexpected parallel port interrupt");
	}
    }

    /* Set the direction bit in the parallel port's        */
    /* control register.                                   */

    SET_PLP_CONTROL(PLP_CONTROL | PLP_DIR);

    /* Permit DMA to the parallel port by setting the DMA  */
    /* ENABLE and SERVICE INTR bits in the extended        */
    /* configuration register.                             */

    /* TO DO: The TI documentation claims in the latest    */
    /* revision that the SERVICE INTR bit should be        */
    /* cleared, not set to enable DMA                      */

    /* Setting the parallel port mode to either Bi-D or    */
    /* ECP actually starts the DMA.                        */

    SET_PLP_EXT_CNTRL(PLP_EXT_CNTRL | 
	(PLP_ECP_MODE | PLP_DMA_EN | PLP_SERV_INTR));
#endif

    /* Wait until Context A is consumed */

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_A_VALID) == 0),timeout,
	"Context A still in use, after timeout elapsed.",
	"Context A consumed."); 

    /* Wait until Context B is consumed */

    Await(
	((PLP_DMA_CONTROL & PLP_DMA_CONTEXT_B_VALID) == 0),timeout,
	"Context B still in use, after timeout elapsed.",
	"Context B consumed."); 

    Delay(timeout);
    InfoMsg("Delay Completed.");
    EndTest("ParallelPortFillTest");
}

int plp_filltest(void)
{

    int result;
    int length;


    length = 1024;
    if (length < 1 || length > 8192) {
	length = 1024;
	msg_printf(INFO, "Supplied length is out of range., Using==0x%x\n",
				length);
    }

    StartTest("Memory --> Parallel Port DMA", 9, 0);

    RESET_PLP_DMA_CONTROL;

    /* Call your test here */
    result= ParallelPortFillTest(length);
    if (result) {
    	sum_error("Parallel port fill");
    } else {
    	okydoky();
    }
    return(result);
}
