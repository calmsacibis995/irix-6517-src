/*********************************************************************
 *                                                                   *
 *  Title :  RadUtils   					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  03.18.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  								     *
 *      Utility routines for RAD bringup tests.                      *
 *                                                                   *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/
#include "ide_msg.h"
#include "uif.h"
#include "libsk.h"

#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"
#include "sys/PCI/ioc3.h"
#include "sys/PCI/bridge.h"


#include "d_rad.h"
#include "d_rad_util.h"
#include "pause.h"

/* 
 * RadSetup - Initialize RAD for operation.  There are five basic things
 * that are done for setup: 
 * 
 * 1. Check for proper ID register
 * 2. Reset RAD by writing all 1's to the status register
 * 3. Write RAD's memory base register
 * 4. Write something large to the latency register
 * 5. Setup PCI holdoff to one shot
 * 6. Initialize the DMA Descriptor RAM (needed for RadSetupDMA)
 */ 

static uint_t RadDMALoAdr[RAD_TOTAL_DMA];
static uint_t RadDMAHiAdr[RAD_TOTAL_DMA];
static uint_t RadDMACtrlAdr[RAD_TOTAL_DMA];

int RadSetup(void)
{
    uint_t      readback;    
    int         i;
    uint_t      baseAddr;
    uint_t      *valueptr;

    u_int mask,flags;

    /* Verify RAD ID register */

    readback = RAD_CFG_READ(RAD_CFG_ID);
    msg_printf(DBG,"Rad Device ID: %x\n",readback);

    if(readback != 0x000510A9) {
	msg_printf(SUM,"Rad ID register did not match pattern\n");
	msg_printf(SUM,"readback = %x \n",readback);
	return -1;
    }

    /* reset RAD by writing to the status register */
    RAD_CFG_WRITE(RAD_CFG_STATUS,0x6);
    
    /* Insert RAD's proper memory base register */
    /*RAD_CFG_WRITE(RAD_CFG_MEMORY_BASE,PCI_MEM_BASE);*/
    
    /* Initialialize the latency register to 1's PCI section 6.2.4*/   
    /*RAD_CFG_WRITE(RAD_CFG_LATENCY,0x00001000);*/
    RAD_CFG_WRITE(RAD_CFG_LATENCY,~0x0);
   
    /* Setup PCI Holdoff to one shot */
    RAD_WRITE(RAD_PCI_HOLDOFF,0x08000010);

    /* Setup PCI Arbitration */
    RAD_WRITE(RAD_PCI_ARB_CONTROL,0xfac688);
   
    /* Initialize to zero all the descriptor registers */

    for(i=0; i<RAD_NUMBER_DESCRIPTORS; i++) {
	RAD_WRITE(RAD_PCI_LOADR_D0 + 4*i,0x0);
    }

    /*
     * Set reset and misc registers to initiate shadows
     */
    
    RAD_WRITE(RAD_RESET,0xFFFFFFFE);  
    RAD_WRITE(RAD_MISC_CONTROL,0x580); 

    /* 
     * Set working register addresses into arrays
     */
    baseAddr = 	RAD_PCI_LOADR_ADAT_RX;
    for(i=0; i<RAD_TOTAL_DMA; i++) {
	RadDMALoAdr[i] = baseAddr;   baseAddr += 4;
	RadDMACtrlAdr[i] = baseAddr; baseAddr += 4;
    }	

    baseAddr = RAD_PCI_HIADR_ADAT_RX;
    for(i=0; i<RAD_TOTAL_DMA; i++) {
	RadDMAHiAdr[i] = baseAddr; 
	baseAddr += 4;
    }

    return 0;
}

int
RadSetupDMA(int dmaType, int* baseAddr, int cacheLines, int loopback)
{ 
  uint_t value = 0,
         descriptorValue=0; 
  uint_t readback = 0;
  uint_t descriptorAddr = RAD_PCI_LOADR_D0;
  int i;

  /*
   * Set the base Address in main memory for the status DMA
   */
   
    
  msg_printf(DBG,"RadSetupDMA: Setup of base adress %x\n",
	     (uint_t)Phys_to_PCI(K1_TO_PHYS(baseAddr)));
  RAD_WRITE(RadDMALoAdr[dmaType],(uint_t)Phys_to_PCI(K1_TO_PHYS(baseAddr)));
  RAD_WRITE(RadDMAHiAdr[dmaType],0);
  
  msg_printf(DBG,"RadSetupDMA: Low address %x %x\n",RadDMALoAdr[dmaType],
	     RAD_READ(RadDMALoAdr[dmaType]));
  msg_printf(DBG,"RadSetupDMA: High address %x %x\n",RadDMAHiAdr[dmaType],
	     RAD_READ(RadDMAHiAdr[dmaType]));
   
   /*
    * Set the status control register according to the information 
    * passed in. If loopback is set that means that it will run in
    * an infinite loop.
    */

    SETBITS(value,0x1FFFFFFF,-cacheLines,7); /* bits 31:7 -> dma line cnt */
    if(loopback) {
	/* 
	 * Look for the next RAD descriptor available, the search will
	 * assume that the PCI Address descriptors are set to zero, when
	 * RadSetup was called.
	 */
	for(i=0; i<RAD_NUMBER_DESCRIPTORS; i++) {
	    readback = RAD_READ(descriptorAddr);
	    if(!readback) /* we found a winner, break from the loop. */
		break;
	    descriptorAddr += 12;
	}
	if(i == RAD_NUMBER_DESCRIPTORS) {
	    msg_printf(SUM,"Error: Unavailable DMA descriptor RAM\n");
	    return -1;
	}
	
	/* set the chosen descriptor on the control register, and indicate
	 * that this is not the end of descriptor.
	 */
	SETBITS(value,0xF,i,3);  /*bits 6:3 -> next descriptor ptr=i    */
	SETBITS(value,0x1,0,2);  /* bit 2-> end of descriptor, 1 = true */
       
	/* Proceed to initialize the chosen descriptor the following way:
	 * PCILoAdr_Di = baseAddr
	 * PCIHiAdr_Di = 0
	 * Control_Di: MinusLC  = -cachelines 
	 *             NxtPtr   = i (Point back to itself)
	 *             EndofDesc = 0 (to go on infinite loop)
	 */	    

	/* Low Address */
	RAD_WRITE(descriptorAddr,
		  (uint_t)Phys_to_PCI(K1_TO_PHYS((__uint64_t)baseAddr)));  
	/* Hi Address  */
	RAD_WRITE(descriptorAddr+4,0);   
	/* Control Address */
	SETBITS(descriptorValue,0x1FFFFFF,-cacheLines,7);
	SETBITS(descriptorValue,0xF,i,3);
	SETBITS(descriptorValue,0x1,0,2);
	RAD_WRITE(descriptorAddr+8,descriptorValue);

	msg_printf(DBG,"RadSetupDMA: descriptor LoAdr: %x value: %x\n",
		   descriptorAddr,RAD_READ(descriptorAddr));
	msg_printf(SUM,"RadSetupDMA: descriptor HiAdr: %x value: %x\n",
		   descriptorAddr+4,RAD_READ(descriptorAddr+4));
	msg_printf(SUM,"RadSetupDMA: descriptor Control: %x value: %x\n",
		   descriptorAddr+8,RAD_READ(descriptorAddr+8));
    }
    else {
	/*
	 * Setup for a one shot dma transfer.
	 */
	SETBITS(value,0xF,0,3); /*bits 6:3 -> next descriptor ptr=i    */
	SETBITS(value,0x1,1,2); /* bit 2-> end of descriptor, 1 = true */
    }

  

    RAD_WRITE(RadDMACtrlAdr[dmaType],value);
    msg_printf(DBG,"RadSetupDMA:Setup control register: %x, value = %x\n",
	       RadDMACtrlAdr[dmaType],value);
    return 0;
}

int
RadStartDMA(dmaType type, uint_t* miscControl)
{
    uint_t readback;

    msg_printf(DBG,"RAdStartDma: RAD ATOD: %d\n",type);

    switch(type) {
    case RAD_ATOD:
	/* 
	 * Misc settings
	 */
 	RAD_WRITE(RAD_VOLUME_CONTROL,0xC0C0C0C0); 
 	SETBITS(*miscControl,0x1,0,7); /*  bit 7: VIn Mute Reset = 1 */
   	RAD_WRITE(RAD_MISC_CONTROL,*miscControl);   
  	RAD_WRITE(RAD_MISC_CONTROL,0);  
	/*
	 * Start DMA
	 */
	readback = RAD_READ(RAD_ATOD_CONTROL);
	SETBITS(readback,0x1,1,0);  /* bit 0: Start DMA = 1       */
	RAD_WRITE(RAD_ATOD_CONTROL,readback);
	msg_printf(SUM,"Started ATOD DMA: %x\n",readback);
	break;

    case RAD_DTOA:
	/* 
	 * Misc settings
	 */
	SETBITS(*miscControl,0x1,0,8); /* bit 8: Vout Mute Reset = 1 */
  	RAD_WRITE(RAD_MISC_CONTROL,*miscControl);  
	/*
	 * Start DMA
	 */
	readback = RAD_READ(RAD_DTOA_CONTROL);
	SETBITS(readback,0x1,1,25); /* bit 25: LineMuxSelect = 1  */
	SETBITS(readback,0x1,1,0);  /* bit 0: Start DMA = 1       */
	RAD_WRITE(RAD_DTOA_CONTROL,readback);
	msg_printf(SUM,"Started ATOD DMA: %x\n",readback);
	break;

    case RAD_AES_TX:
	/* 
	 * Miscelaneous settings 
	 */
 	SETBITS(*miscControl,0x1,0,9);  /* bit 9:  CopOutSel = 0  */
  	RAD_WRITE(RAD_MISC_CONTROL,*miscControl);  

	readback = RAD_READ(RAD_AES_TX_CONTROL);
	SETBITS(readback,0x1,1,0);  /* bit 0: Start DMA = 1       */
	RAD_WRITE(RAD_AES_TX_CONTROL,readback);
 	msg_printf(SUM,"Started AES Tx DMA: %x\n", 
 		   RAD_READ(RAD_AES_TX_CONTROL)); 
	break;
    default:
	msg_printf(SUM,"Unsuported DMA type.\n");
	return -1;

    }
    msg_printf(DBG,"RadStartDMA: Control: %x miscControl: %x DMA: %d\n",
	       readback,*miscControl,type);
    return 0;
}
    
int
RadStopDMA(dmaType type) 
{
    uint_t readback;

    switch(type) {
    case RAD_ATOD:
	readback = RAD_READ(RAD_ATOD_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_ATOD_CONTROL,readback);
	msg_printf(DBG,"Stopped ATOD DMA\n");
	break;

    case RAD_DTOA:
	readback = RAD_READ(RAD_DTOA_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_DTOA_CONTROL,readback);
	msg_printf(DBG,"Stopped DTOA DMA\n");
	break;

    case RAD_AES_TX:
	readback = RAD_READ(RAD_AES_TX_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_AES_TX_CONTROL,readback);
	msg_printf(DBG,"Stopped AES Tx DMA\n");
	break;

    case RAD_AES_RX:

	readback = RAD_READ(RAD_AES_RX_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_AES_RX_CONTROL,readback);
	msg_printf(DBG,"Stopped AES Rx DMA\n");
	break;

    case RAD_ADAT_TX:
	readback = RAD_READ(RAD_ADAT_TX_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_ADAT_TX_CONTROL,readback);
	msg_printf(DBG,"Stopped ADAT Tx DMA\n");
	break;
    case RAD_ADAT_RX:

	readback = RAD_READ(RAD_ADAT_RX_CONTROL);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_ADAT_RX_CONTROL,readback);
	msg_printf(DBG,"Stopped ADAT Rx DMA\n");
	break;

    case RAD_STATUS:

	readback = RAD_READ(RAD_STATUS_TIMER);
	SETBITS(readback,0x1,0,0);
	RAD_WRITE(RAD_STATUS_TIMER,readback);
	msg_printf(DBG,"Stopped Status DMA\n");
	break;

    default:
	msg_printf(SUM,"Unsopported DMA Type: exiting.\n");
	return -1;
    }
    msg_printf(DBG,"RadStopDMA: Control Register: %x dma: %d\n",
	       readback,type);
    return 0;

}

void 
RadStartStatusDMA(uint_t timer, uint_t retrigger)
{
    uint_t uitmp=0;

    SETBITS(uitmp,0x7FFF,timer,8);	/* bits 17:2 - timer value */
    SETBITS(uitmp,0x1,retrigger,1);	/* bit 1 - retrigger, 1 = enable */
    SETBITS(uitmp,0x1,1,0);		/* bit 0 - status DMA, 1 = enable */
    RAD_WRITE(RAD_STATUS_TIMER,uitmp);

    msg_printf(DBG,"RadStartStatusDMA: Control Register: %x \n",uitmp);
}

void
RadWait(uint_t milliseconds)
{
    unsigned long tickhz, thistick, futuretick, delay;
    
    tickhz = (unsigned long)cpu_get_freq(0);
    delay = tickhz * milliseconds/1000;

    thistick = r4k_getticker();
    futuretick = thistick + delay;
    
    if(futuretick < thistick) /* overflow */
	while(thistick > futuretick)
	    thistick = r4k_getticker();
    else
	while(thistick < futuretick) 
	    thistick = r4k_getticker();

}

/* 
 * Returns relative time in microseconds
 */

unsigned long
RadGetRelativeTime(void)
{

    static int	lasttime;	/* last returned value */
    static unsigned cummdelta;	/* cummulative fraction of a sec. */
    static unsigned lasttick;	/* last value of count register */
    static unsigned tickhz;	/* ticker speed in Hz */
    unsigned	thistick;	/* current value of count register */

    if ( ! lasttick ) {
	lasttick = r4k_getticker();
	tickhz = cpu_get_freq(0);
	return 0;
    }

    thistick = r4k_getticker();

    if ( thistick < lasttick )

	cummdelta += (0xffffffff - lasttick) + thistick;
    else
	cummdelta += thistick - lasttick;
    
    lasttick = thistick;

    return(cummdelta*1000000/tickhz);
}

/*
 * RadSetupCG: This method does very simple setup for clock 
 * generation.  For a more complex and thorough testing of the 
 * clock generators see RadCGTest.
 */

int
RadSetupCG(uint_t cg, uint_t sf, RadReference ref, uint_t* reset)
{

    uint_t readback;

    static const uint_t radMuxSel[4] = {
	RAD_FREQ_SYNTH0_MUX_SEL,
	RAD_FREQ_SYNTH1_MUX_SEL,
	RAD_FREQ_SYNTH2_MUX_SEL,
	RAD_FREQ_SYNTH3_MUX_SEL,
    }	;
    
    /*
     * Parameter check
     */

    if(cg > 3) {
	msg_printf(SUM,"Invalid selection of clock generator\n");
	return -1;
    }

    if(sf != 48000 && sf != 44100) {
	msg_printf(SUM,"Unsupported frequency at the moment.\n");
	return -1;
    }

    /*
     * I,R and M setting for the clock genetor, also input clock
     */
    RAD_WRITE(radMuxSel[cg],ref);    
    RAD_WRITE(RAD_CLOCKGEN_REM,0x0000ffff);
    RAD_WRITE(RAD_CLOCKGEN_ICTL,0x40000403);

    msg_printf(DBG,"clock: %x Rad Mux: %x val: %x\n",cg,radMuxSel[cg],ref);
    /* 
     * Take interfaces out of reset
     */

    SETBITS(*reset,0x1,0,13);    /* bit 13 -> PIDivRst, 0 = reset */
    RAD_WRITE(RAD_RESET,*reset);

    SETBITS(*reset,0x1,0,12);    /* bit 12 -> ClkGenDivRst, 0 = reset */
    SETBITS(*reset,0x1,0,cg+8); /* Synth, 0 = reset */
    RAD_WRITE(RAD_RESET,*reset);

    msg_printf(DBG,"RadSetClockGen: Rad Reset:%x\n",*reset);
    return 0;
}

int
RadSetClockIn(dmaType type,uint_t clock) 
{
    uint_t readback;
    uint_t uitmp=0;

    switch(type) {

    case RAD_ATOD:
	readback = RAD_READ(RAD_ATOD_CONTROL);
	SETBITS(readback,0x7,clock,29) ;  /* bits 29: 31 SelClkIn  */
 	SETBITS(readback,0x1,1,25); /* bit 25: LineMuxSelect = 1  */ 
 	SETBITS(readback,0x1,1,24); /* bit 24: Output Enable = 1  */ 
	RAD_WRITE(RAD_ATOD_CONTROL,readback);
	msg_printf(DBG,"RadSetClockIn: RAD ATOD Control : %x\n",readback);
	break;
    case RAD_AES_TX:
	readback = RAD_READ(RAD_AES_TX_CONTROL);
	SETBITS(readback,0x7,clock,29) ;  /* bits 29: 31 SelClkIn  */
	RAD_WRITE(RAD_AES_TX_CONTROL,readback);
	break;
    case RAD_DTOA:
	SETBITS(uitmp,0x1,1,25);          /* bit 25: LD_CONV       */
	SETBITS(uitmp,0xf,1,0);           /* bits 4:0 SelRange     */
	RAD_WRITE(RAD_CLOCKGEN_ICTL,uitmp);
	msg_printf(DBG,"RadSetClockIn: RAD Integer and Ctl: %x\n",
		   uitmp);

	readback = RAD_READ(RAD_DTOA_CONTROL);
	SETBITS(readback,0x7,clock,29) ;  /* bits 29: 31 SelClkIn  */
	RAD_WRITE(RAD_DTOA_CONTROL,readback);
	break;
    default:
	msg_printf(SUM,"Unsopported DMA type \n.");
	return -1;
    }
    msg_printf(DBG,"RadSetClockIn: Control register: %x dmaType: %d\n",
	       readback,type);
    return 0;
	
}

int
RadUnresetInterfaces(dmaType type, uint_t* reset)
{
    uint_t readback;

    switch(type) {
    case RAD_ATOD:
	/*
	 * first external then internal
	 */
	SETBITS(*reset,0x1,0,15); /* bit 15: ExtAtoDRst      */
	RAD_WRITE(RAD_RESET,*reset);
	msg_printf(DBG,"RAD Reset: %x\n",*reset);
	SETBITS(*reset,0x1,0,4);  /* bit 4: IntAtoDRst       */
	RAD_WRITE(RAD_RESET,*reset);
	msg_printf(DBG,"RAD Reset: %x\n",*reset);
	break;

    case RAD_DTOA:
	/*
	 * first internal then external
	 */
	SETBITS(*reset,0x1,0,1); /* bit 1: IntDtoAReset      */
	RAD_WRITE(RAD_RESET,*reset);
	SETBITS(*reset,0x1,0,14);  /* bit 14: IntDtoAreset   */
	RAD_WRITE(RAD_RESET,*reset);
	break;

    case RAD_AES_TX:
	SETBITS(*reset,0x1,0,2); /* bit 2: AESTxRst */
	RAD_WRITE(RAD_RESET,*reset);
	break;
    default:
	msg_printf(SUM,"Unsuported DMA type.\n");
	return -1;

    }
    msg_printf(DBG,"RadUnresetInterfaces: RAD_RESET: %x dmaType: %d\n",
	       *reset,type);
    return 0;
}


int 
RadSetStatusMask(dmaType type, uint_t *mask) 
{
    uint_t readback;

    switch(type) {
    case RAD_ATOD:
	SETBITS(*mask,0x1,1,3);
	break;
    case RAD_DTOA:
	SETBITS(*mask,0x1,1,0);
	break;

    case RAD_AES_TX:
	SETBITS(*mask,0x1,1,1);
	break;

    case RAD_AES_RX:
	SETBITS(*mask,0x1,1,5);
	break;

    case RAD_ADAT_TX:
	SETBITS(*mask,0x1,1,2);
	break;

    case RAD_ADAT_RX:
	SETBITS(*mask,0x1,1,7);
	break;

    default:
	msg_printf(SUM,"Unsopported DMA Type: exiting.\n");
	return -1;
    }
    msg_printf(DBG,"RadStatusMask: Control Register: %x \n",*mask);
    return 0;

}

int
IsOctaneLx()
{
    /* get the pointer to device space */
    ioc3_mem_t *ioc3_mem = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;

    /* Check the generic PIO pin 2 in the IOC3 */
    /* A value '0' -> Octane-Lx & '1' -> Octane-Classic */
    if (ioc3_mem->gppr_2 == 0) {
        msg_printf(SUM, "This is an Octane-Lx\n");
        return 1;
    }
    else {
        msg_printf(DBG, "This is an Octane-Classic\n");
        return 0;
    }
}

