#include "stdio.h"
#include "sys/types.h"
#include "sys/mman.h"
#include "TL16550.h"



unsigned long mace_rb_base_addr;
unsigned int longPattern[2] = {0x55555555, 0xAAAAAAAA} ;
unsigned int *pRingBuff;

SERIAL_DMA_RING_REG_DESCRIPTOR *pSer1DmaReg = (SERIAL_DMA_RING_REG_DESCRIPTOR *) SERIAL_A_DMA_REG_ADDRS;

SERIAL_DMA_RING_REG_DESCRIPTOR *pSer2DmaReg = (SERIAL_DMA_RING_REG_DESCRIPTOR *) SERIAL_B_DMA_REG_ADDRS;


unsigned int getRingAddress();
void testDmaRingBuffs();
void testDmaTransmit();
void testDmaReceive();
void testAceRegs();
void setAceRegs();
void fillRingBuff();
void testRingBuffs();
void setIntrReg();




/**********************************************
 * sioDMATests
 * This module contains a series of functions
 * which tests the DMA ring buffers
 *
 */



void
setAceRegs(pSerIntfReg)
unsigned char *pSerIntfReg;
{

    unsigned char *pRegRBR_THR,*pRegIER,*pRegIIR_FCR,*pRegLCR,*pRegMCR,
		  *pRegLSR,*pRegMSR,*pRegSCR,*pRegDLL,*pRegDLM;


    /*
     * Enable the 16550 fifos and CTS
     */

    pRegRBR_THR = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegIER =  pSerIntfReg + ( (REG_IER * REG_INT_VIEW_OFFSET) + 7);
    pRegIIR_FCR = pSerIntfReg + ( (REG_FCR * REG_INT_VIEW_OFFSET) + 7);
    pRegLCR = pSerIntfReg +  (REG_LCR * REG_INT_VIEW_OFFSET) + 7;
    pRegMCR = pSerIntfReg +  (REG_MCR * REG_INT_VIEW_OFFSET) + 7;
    pRegLSR = pSerIntfReg +  (REG_LSR * REG_INT_VIEW_OFFSET) + 7;
    pRegMSR = pSerIntfReg +  (REG_MSR * REG_INT_VIEW_OFFSET) + 7;
    pRegSCR = pSerIntfReg +  (REG_SCR * REG_INT_VIEW_OFFSET) + 7;
    pRegDLL = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegDLM = pSerIntfReg + (REG_IER * REG_INT_VIEW_OFFSET) + 7;


    *pRegLCR = UART_LCR_DLAB;

    printf("\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);

    *pRegDLL = 0xC;    /* speed 9600 */
    *pRegDLM = 0;

    printf("DLL = %x, DLM = %x\n",*pRegDLL,*pRegDLM);
    *pRegLCR = 0;      /*dlab=0*/

    *pRegLCR = UART_LCR_WLS0 | UART_LCR_WLS1;

    *pRegIIR_FCR = UART_FCR_FIFO_ENABLE | UART_FCR_RCV_TRIG_MSB;

    *pRegIER = UART_IER_ERDAI | UART_IER_ETHREI;


    printf("\nRBR reg: %x, Val: %x", pRegRBR_THR, *pRegRBR_THR);
    printf("\nIER reg: %x, Val: %x", pRegIER, *pRegIER);
    printf("\nIIR reg: %x, Val: %x", pRegIIR_FCR, *pRegIIR_FCR);
    printf("\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);
    printf("\nMCR reg: %x, Val: %x", pRegMCR, *pRegMCR);
    printf("\nLSR reg: %x, Val: %x", pRegLSR, *pRegLSR);
    printf("\nMSR reg: %x, Val: %x", pRegMSR, *pRegMSR);
    printf("\nSCR reg: %x, Val: %x", pRegSCR, *pRegSCR);


    return;

}



void
testAceRegs(pSerIntfReg)
unsigned char *pSerIntfReg;
{

    unsigned char *pRegRBR_THR,*pRegIER,*pRegIIR_FCR,*pRegLCR,*pRegMCR,
		  *pRegLSR,*pRegMSR,*pRegSCR,*pRegDLL,*pRegDLM,reg_data;


    /*
     * Enable the 16550 fifos and CTS
     */

    pRegRBR_THR = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegIER =  pSerIntfReg + ( (REG_IER * REG_INT_VIEW_OFFSET) + 7);
    pRegIIR_FCR = pSerIntfReg + ( (REG_FCR * REG_INT_VIEW_OFFSET) + 7);
    pRegLCR = pSerIntfReg +  (REG_LCR * REG_INT_VIEW_OFFSET) + 7;
    pRegMCR = pSerIntfReg +  (REG_MCR * REG_INT_VIEW_OFFSET) + 7;
    pRegLSR = pSerIntfReg +  (REG_LSR * REG_INT_VIEW_OFFSET) + 7;
    pRegMSR = pSerIntfReg +  (REG_MSR * REG_INT_VIEW_OFFSET) + 7;
    pRegSCR = pSerIntfReg +  (REG_SCR * REG_INT_VIEW_OFFSET) + 7;
    pRegDLL = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegDLM = pSerIntfReg + (REG_IER * REG_INT_VIEW_OFFSET) + 7;

    printf("\nRBR reg: %x, Val: %x", pRegRBR_THR, *pRegRBR_THR);
    printf("\nIER reg: %x, Val: %x", pRegIER, *pRegIER);
    printf("\nIIR reg: %x, Val: %x", pRegIIR_FCR, *pRegIIR_FCR);
    printf("\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);
    printf("\nMCR reg: %x, Val: %x", pRegMCR, *pRegMCR);
    printf("\nLSR reg: %x, Val: %x", pRegLSR, *pRegLSR);
    printf("\nMSR reg: %x, Val: %x", pRegMSR, *pRegMSR);
    printf("\nSCR reg: %x, Val: %x", pRegSCR, *pRegSCR);

    *pRegSCR = 0xaa;
    if(reg_data=*pRegSCR != 0xaa)
	printf("ERROR: SCR reg = %x, expected = 0xaa\n",reg_data);
    
    *pRegSCR = 0x55;
    if(reg_data=*pRegSCR != 0x55)
	printf("ERROR: SCR reg = %x, expected = 0x55\n",reg_data);
    
    *pRegIER = 0x04;
    *pRegIIR_FCR = 0x04;
    *pRegLCR = 0xff;
    *pRegMCR = 0x1f;

    *pRegDLL = 0x55;    /* speed 9600 */
    *pRegDLM = 0xAA;

    printf("\nDLL reg: %x, Val: %x", pRegDLL, *pRegDLL);
    printf("\nDLM reg: %x, Val: %x", pRegDLM, *pRegDLM);
    printf("\nIIR reg: %x, Val: %x", pRegIIR_FCR, *pRegIIR_FCR);
    printf("\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);
    printf("\nMCR reg: %x, Val: %x", pRegMCR, *pRegMCR);
    printf("\nLSR reg: %x, Val: %x", pRegLSR, *pRegLSR);
    printf("\nMSR reg: %x, Val: %x", pRegMSR, *pRegMSR);
    printf("\nSCR reg: %x, Val: %x", pRegSCR, *pRegSCR);


    *pRegLCR = 0;      /*dlab=0*/
    printf("\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);
    printf("\nDLL reg: %x, Val: %x", pRegDLL, *pRegDLL);
    printf("\nDLM reg: %x, Val: %x", pRegDLM, *pRegDLM);

    return;

}


/*
 *Transmit N bytes of data, byte pattern decrements from fe to 0,
 *waits for interrupt from crime and checks value of depth register,
 *pads invalid bytes with ff.
 */
void
TransmitData(ByteCount, pSerDmaReg)
int ByteCount;
SERIAL_DMA_RING_REG_DESCRIPTOR *pSerDmaReg;
{
	unsigned char count, control;
	int i, depth, dl_index;
	int depth_limits[8]={0,0x400,0x800,0xc00,0,0,0xfe0,0xfe0};
	unsigned long *pIntrStatusReg = (unsigned long *)INTR_STATUS_REG;
	unsigned long CRIMEIntStatus, MACEIntStatus;

	depth = pSerDmaReg->txDepth;
	dl_index = (pSerDmaReg->txCont & 0xe0) >> 4;

	for (i = 0; i < 0x20; i++)
	{
	    /* Wait for interrupt */
	    if (CRIMEIntStatus = GetInterruptStatus()) 
		{
    		    MACEIntStatus = *pIntrStatusReg;
		    break;
		}
	    else sleep(1);
	}

	if (i == 0x20)
	   printf("\nERROR:No Interrupt\n");
	printf("\n@Addr: %x IntrStatus: %x",pIntrStatusReg, 
	   (unsigned int)MACEIntStatus); 
	printf("Crime int status = %x\n",CRIMEIntStatus);

/********** need loops to fill buffer, wait for int, check depth ***********/
}

/*
 *This function fills the ring buffer
 */

void
fillRingBuff(pRingBase, ringId, pattern)
unsigned int  *pRingBase;
unsigned int ringId;
unsigned int pattern;
{
    unsigned int ringAddr, ringBase, controlBytes, i;
    long *pRingAddr, *pRingAddrBase, ringVal;
    unsigned char contrChar = 0;



    /*
     *zeroout the ring buff
     */
    for (i = 0; i < DMA_RING_SIZE / 4; i++)
	{
	    ringBase = (unsigned int)pRingBase;
	    ringAddr = ringBase | (ringId << 12) | (i << 5);
	    pRingAddr = (long *) ringAddr;
	    *pRingAddr++ = (long)0;
	}

    /*
     * write pattern
     */

    
    ringBase = (unsigned int)pRingBase;
    ringAddr = ringBase | (ringId << 12);
    pRingAddr = (long *) ringAddr;
    printf("\npRingAddr: %x", pRingAddr); 
    contrChar = 0x80;
    controlBytes = (contrChar << 24) | (contrChar << 16) | 
	(contrChar << 8) | contrChar;
    ringVal = pattern;

    for (i = 0; i < 16; i++)
	{
	    printf("\npRingAddr: %x", pRingAddr); 
	    *pRingAddr++ = controlBytes;
	    *pRingAddr++ = ringVal;
	}
    printf("\nControlBytes: %x", (unsigned int)controlBytes);
    controlBytes = ringVal >> 31;
    printf("\nring control bytes: %x", (unsigned int)controlBytes );
    printf("\nIn address: %x pattern: %x", pRingAddr, 
	   (unsigned int)*pRingAddr);


}

/*
 * This function displays the ring buffer
 */

void
testRingBuffs(pRingBase, ringId)
unsigned int  *pRingBase;
unsigned int ringId;
{
    unsigned int ringAddr, ringBase, offset, i;
    unsigned long *pRingAddr;

   printf("base = %x, Id = %x\n",pRingBase,ringId);
    ringBase = (unsigned int)pRingBase;
    ringAddr = ringBase | (ringId << 12);
   printf("ringAddr = %x\n",ringAddr);
    pRingAddr = (unsigned long *) ringAddr;
    printf("\nIn ring address: %x pattern: %x", pRingAddr, *pRingAddr);

	sleep(30);

    return;
}

/*
 *Function to set the interrupt mask register
 */

void
setIntrReg()
{

    unsigned long *pIntrMaskReg = (unsigned long *)INTR_MASK_REG;
    unsigned long *pIntrStatusReg = (unsigned long *)INTR_STATUS_REG;

    unsigned long intrMask = 0;
    unsigned char intrVal = 0;


    intrMask = 0xfff00000;

    *pIntrMaskReg = intrMask;

    printf("\n@Addr: %x interrupt mask: %x",pIntrMaskReg, 
	   (unsigned int)intrMask );


    return;
}

/*
 *This function calculates the location of the DMA ring ptr based on the 
 *parameters
 */

unsigned int
getRingAddress(pRingBase, ringId, offset)
unsigned int  *pRingBase;
unsigned int ringId;
unsigned int offset;
{

    unsigned int ringAddr, ringBase;
    ringBase = (unsigned int)pRingBase;
    ringAddr = ringBase | (ringId << 12) | (offset << 5);
    printf("\nIn func ringAddr: %x", ringAddr); 
    return(ringAddr);

}


int
setup_mace_dma(void)
{
  register int fd;
  void *pa;
  static int pgsize;

  long *mace_rb_base = (long *)ISA_RING_BASE_REG;

  /* Get page aligned Tile for DMA buffs */
  pa = GetTile(count);

  /* setup mace dma address */
  *mace_rb_base = (long)pa;

  mace_rb_base_addr = (unsigned int)pa;
  return 0;
}



