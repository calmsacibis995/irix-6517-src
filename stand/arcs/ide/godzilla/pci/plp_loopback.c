/*#define MFG_DBGPROM 0*/
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/mman.h>
#include "uif.h"
#include "pio.h"
#include "d_godzilla.h"
#include "d_ioregs.h"
#include "libsk.h"
#include "libsc.h"
#include "d_frus.h"
#include "d_prototypes.h"

void InitParallalPortWrite(void)
{
	
    /* Set the direction bit in the parallel port's        */
    /* control register for mem -> port transfers          */
    PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, 0xC0);
	 us_delay(100);
    /*Clear DMA buffers*/
    PIO_REG_WR_32(IOC3REG_PPCR, 0xFFFFFFFF, 0x4000000);
 	 /*Set up interrupts*/
	 PIO_REG_WR_32(IOC3REG_SIO_IR, 0xFFFFFFFF, 0x003C0000);
	 PIO_REG_WR_32(IOC3REG_SIO_IEC, 0xFFFFFFFF, 0x003C0000);
	 /*Disable DMA, Reset*/
    PIO_REG_WR_32(IOC3REG_PPCR, 0xFFFFFFFF, 0x0000000);
    /*Set PP into bi-directional mode*/
    PIO_REG_WR_8(DU_SIO_PP_ECR, 0xFF,PLP_BI_DIR_MODE);
	 msg_printf(DBG, "ECR DIR = %x\n", PLP_BI_DIR_MODE);
    /* Select and release reset, forward DIR*/
    PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, 0xCC);
	 msg_printf(DBG, "DCR DIR = %x\n", 0xCC);
   PIO_REG_WR_8(DU_SIO_PP_DATA, 0xFF, 0); /*data init state*/
}

void WriteTestData(unsigned char myChar)
{

    int timeout;
    register unsigned char tmp;
	 short count;

	InitParallalPortWrite();
   msg_printf(DBG,"Write to Tester, data = %x.\n",myChar);
   PIO_REG_WR_8(DU_SIO_PP_DATA, 0xFF, myChar);
	timeout = 100;
   PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);
	msg_printf(DBG,"status = %llx\n",tmp);
	while(0 != timeout && (!(tmp & PLP_BUSY))){
		timeout--;
		msg_printf(DBG,"busy status = %llx\n",tmp);
      us_delay(1000);  	
		PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);
	}
	if(!timeout)
		msg_printf(DBG,"timeout1\n");

	/*drive strobe low for device to push date to the plp lines*/
  	PIO_REG_RD_8(DU_SIO_PP_DCR, 0xFF, tmp);
	tmp |= PLP_STB;
  	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, tmp);
	us_delay(400);
  	PIO_REG_RD_8(DU_SIO_PP_DCR, 0xFF, tmp);
	tmp &= ~PLP_STB;
  	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, tmp);
   PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);
	msg_printf(DBG,"status = %llx\n",tmp);
	us_delay(1000);
	timeout = 100;
	while (0 != timeout && !(tmp & PLP_ACK)){
		timeout--;
		us_delay(100);
        	PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);
	}
	if(!timeout)
		msg_printf(DBG,"timeout2\n");
	msg_printf(DBG,"status = %llx\n",tmp);
	
	PIO_REG_RD_8(DU_SIO_PP_DATA, 0xFF, myChar);
   PIO_REG_WR_8(DU_SIO_PP_DATA, 0xFF, 0); /*data init state*/
	PIO_REG_RD_8(DU_SIO_PP_DATA, 0xFF, myChar);
}

unsigned char ReadTestData(void)
{
    unsigned char c;
    register unsigned char tmp;
    int timeout;


    /* Set the direction bit in the parallel port's        */
    /* control register for port -> mem transfers.         */
	 msg_printf(DBG, "Starting Read\n");
    PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF,PLP_DIR | 0xca);
    us_delay(100);
    
	 /*issue init*/
	 PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF,PLP_DIR | 0xce);
	 
	PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);	
	msg_printf(DBG,"status = %llx\n",tmp);
	timeout = 100;
	 while(0 != timeout && ((tmp & PLP_BUSY))){
		timeout--;
		msg_printf(DBG,"busy status = %llx\n",tmp);
		us_delay(1000);
		PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);	
	 }
	
	if(!timeout)
		msg_printf(DBG,"timeout3\n");
	PIO_REG_RD_8(DU_SIO_PP_DSR, 0xFF, tmp);	
	msg_printf(DBG,"status = %llx\n",tmp);
	/*strobe for joe*/
  	PIO_REG_RD_8(DU_SIO_PP_DCR, 0xFF, tmp);
	tmp |= PLP_STB;
  	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, tmp);
	PIO_REG_RD_8(DU_SIO_PP_DATA, 0xFF, c);
  	
  	PIO_REG_RD_8(DU_SIO_PP_DCR, 0xFF, tmp);
	tmp &= ~PLP_STB;
  	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, tmp);
	
	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, tmp);
	msg_printf(DBG, "data read = %x\n", c);
	PIO_REG_RD_8(DU_SIO_PP_DCR, 0xFF, tmp);	
	tmp &= ~PLP_STB;
	
	PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF,tmp);
    return(c);
}

int ParallelPortExternalLoopbackTest(int length)
{
    unsigned char c;
   int i, context, error;

	msg_printf(INFO, "Starting Parallel Port External Loopback Test\n");

    error = 0;
    PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF,0xc0);
    for (i = 0x00; i < 0xff; i++) { /*0..ff*/
    	WriteTestData(i);
    	c = ReadTestData();

	  	if (i != (int)c) {
	    	error++;
	    	msg_printf(DBG,"expected %x, actual %x\n",i,c);
      }
   }


    return(error);
}

plp_external_loopback()
{
    int result;
    int length;

    length = 80;
    if (length < 1 || length > 8192) {
        msg_printf(ERR,"Supplied length is out of range.\n");
        return(1);
    }

    /* Call test here */
    result= ParallelPortExternalLoopbackTest(length);
	 
	 if (result) 
	    	msg_printf(INFO, "Ensure that Parallel Port loopback card is connected\n"); 
	
#ifdef NOTNOW 
	 REPORT_PASS_OR_FAIL(result, "PCI Parallel Port Loopback", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0007], result );
}




