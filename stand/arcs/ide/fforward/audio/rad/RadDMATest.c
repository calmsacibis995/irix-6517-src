
/*********************************************************************
 *                                                                   *
 *  Title :  RadDmaTest   					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  04.08.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  								     *
 *      Rad DMA test.  Tests RAD dma functionality. It will support  * 
 *      a variety of options.                                        *
 *                                                                   *
 *     -h  <help> selection
 *     -lt <dma1 dma2> loopthru mode                                 *
 *     -lb <dma1 dma2> loopback mode                                 *
 *     -sp <dma> single port                                         *
 *     -sf <freq> Sample frequency for interface                     *
 *      -s   (Spin mode) default: false                              *
 *      -sin <f sf amp> (frequency, sampling freq. amplitude)        * 
 *           default f = 1 kHz, sf = 48 kHz, amp = full scale        *
 *      -v   (Verbose mode) default: false                           *
 *      -r   Reset chip after being finished                         *
 *      dma options: atod dtoa aestx aesrx adatrx adattx             *
 *                                                                   *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/

#include "ide_msg.h"
#include "uif.h"

#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"
#include <libsc.h>
#include "pause.h"

#include <sys/rad.h>
#include "rad_util.h"

#define RAD_MAX_DMA 4
#define RAD_BUFFER_SIZE 1024  

static int buffer0[RAD_BUFFER_SIZE*2+32];
static int buffer1[RAD_BUFFER_SIZE*2+32];
static int buffer2[RAD_BUFFER_SIZE*2+32];
static int buffer3[RAD_BUFFER_SIZE*2+32];

static int statusbuf[64];
 
enum mode {SINGLE_PORT, LOOPBACK, LOOPTHRU};

typedef  struct {
    int* buffer;
    uint_t taken;
} dmaBuffer;

typedef struct {
    dmaType src;       /* Initiating dma interface */
    dmaType dst;       /* Receiving dma interface  */
    enum mode dmaMode; /* Type of test             */
    int* buffer;       /* Memory buffer for dma    */
    int* bufferdst;    /* If doing LOOPBACK DMA    */
} dma_struct;

struct _dmaTestStruct {

    int spin;
    int verbose;
    int reset;

    uint_t volume;
    uint_t gpio3;
    
    uint_t totaldma;
    dma_struct dmaStruct[RAD_MAX_DMA];

    uint_t freq ;      /* Sine wave parameters     */
    uint_t sampleFreq ;
    uint_t amplitude ;
    uint_t channels;

} dmaTestStruct;

int RadCheckArgs(int, char**);
void RadDMAPrintOptions(void);


int RadDMATest(int argc, char** argv)
{
    int i,j;
    uint_t mask=0;
    int count = 0;
    int status = 0;
    uint_t uitmp = 0;
    uint_t readback;
    uint_t oscRef=0;
    uint_t totalCaches = 0;

    uint_t rad_reset = RAD_RESET_VALUE;
    uint_t miscControl=RAD_MISC_VALUE ;
    dmaBuffer dmaBuf[RAD_MAX_DMA];

    /*
     * Check for command line Options
     */

    if(argc == 1) {
	RadDMAPrintOptions();
	return 0;
    }

    if(RadCheckArgs(argc,argv) == -1) {
	msg_printf(SUM,"rad_dma: Problems reading arguments.\n");
	RadDMAPrintOptions(); 
	return -1;
    }


    /*
     *  Setup RAD for operation.
     */

    status = RadSetup();
    if(status == -1) {
	msg_printf(SUM,"Problems setting up RAD, exiting diagnostic\n");
	return -1;
    }

    /*
     * Setup dma buffers for RAD operation 
     */
    for(i=0; i<RAD_BUFFER_SIZE*2+32; i++)
	buffer0[i] = 0;
    for(i=0; i<RAD_BUFFER_SIZE*2+32; i++)
	buffer1[i] = 0;
    for(i=0; i<RAD_BUFFER_SIZE*2+32; i++)
	buffer2[i] = 0;
    for(i=0; i<RAD_BUFFER_SIZE*2+32; i++)
	buffer3[i] = 0;

    dmaBuf[0].buffer = buffer0;
    dmaBuf[0].taken = 0;

    dmaBuf[1].buffer = buffer1;
    dmaBuf[1].taken = 0;

    dmaBuf[2].buffer = buffer2;
    dmaBuf[2].taken = 0;

    dmaBuf[3].buffer = buffer3;
    dmaBuf[3].taken = 0;

    /*
     * Generate reference input sine wave if needed
     */

    msg_printf(DBG,"Setting up memory buffers for DMA\n");

    for(i=0; i<dmaTestStruct.totaldma; i++) { 
	msg_printf(DBG,"Independent DMA operation: %d\n",i);
	msg_printf(DBG,"Dma mode: %d\n",dmaTestStruct.dmaStruct[i].dmaMode);
	msg_printf(DBG,"Dma src: %d\n",dmaTestStruct.dmaStruct[i].src);
	msg_printf(DBG,"Dma dst: %d\n",dmaTestStruct.dmaStruct[i].dst);

	/*
	 * Set memory for all buffers 
	 */
	j=0;
	while(dmaBuf[j].taken) 
	    j++;

	if(j >= RAD_MAX_DMA) {
	    msg_printf(SUM,"To many dma operations, try less\n");
	    return -1;
	}

	msg_printf(DBG,"Setting up src memory buffer:%d for DMA operation\n",j);

	dmaBuf[j].taken = 1;
	dmaTestStruct.dmaStruct[i].buffer = 
	    (int *)((int)dmaBuf[j].buffer & ~127)+128;
	
	if(dmaTestStruct.dmaStruct[i].dmaMode == LOOPBACK) {

	    j=0;
	    while(dmaBuf[j].taken) 
		j++;

	    if(j >= RAD_MAX_DMA) {
		msg_printf(SUM,"To many dma operations, try less\n");
		return -1;
	    }

	    msg_printf(DBG,"Setting up dst memory buffer:%d for DMA operation\n",j);

	    dmaBuf[j].taken = 1;
	    dmaTestStruct.dmaStruct[i].bufferdst = 
		(int *)((int)dmaBuf[j].buffer & ~127)+128;

	    if(!oscRef) {
		status = RadOscillatorInit(dmaTestStruct.sampleFreq);
		oscRef = 1;
	    }
	    if(status == -1) {
		msg_printf(SUM,"Problem initializing oscillator ref tables.\n");
		return -1;
	    }
	    
	    /*
	     * Hack! Right now to avoid clicking, hardwire the frequency of the
	     * sine wave.  This needs to be fixed for sine waves with approximated
	     * frequencies where their periods fit a single cache line.
	     */

	    if(dmaTestStruct.sampleFreq == 48000) 
		status = RadOscillator(dmaTestStruct.dmaStruct[i].buffer,
				       480*dmaTestStruct.channels,
				       dmaTestStruct.channels,
				       1000,
				       dmaTestStruct.amplitude);
	    else if(dmaTestStruct.sampleFreq == 44100) 
		status = RadOscillator(dmaTestStruct.dmaStruct[i].buffer,
				       352,
				       dmaTestStruct.channels,
				       1002,
				       dmaTestStruct.amplitude);
		
	
	    if(status == -1) {
		msg_printf(SUM,"Problem generating sine wave.\n");
		return -1;
	    }
	} else if(dmaTestStruct.dmaStruct[i].dmaMode == SINGLE_PORT) {
	    if(dmaTestStruct.dmaStruct[i].src == RAD_DTOA ||
	       dmaTestStruct.dmaStruct[i].src == RAD_AES_TX ||
	       dmaTestStruct.dmaStruct[i].src == RAD_ADAT_TX) {
		msg_printf(DBG,"Single Port Setup.  Sample Freq: %d\n",
			   dmaTestStruct.sampleFreq);
		if(!oscRef) {
		    status = RadOscillatorInit(dmaTestStruct.sampleFreq);
		    oscRef = 1;
		}
		if(status == -1) {
		    msg_printf(SUM,"Problem initializing oscillator ref tables.\n");
		    return -1;
		}
		/*
		 * Hack! Right now to avoid clicking, hardwire the frequency of the
		 * sine wave.  This needs to be fixed for sine waves with approximated
		 * frequencies where their periods fit a single cache line.
		 */
		if(dmaTestStruct.sampleFreq == 48000) 
		    status = RadOscillator(dmaTestStruct.dmaStruct[i].buffer,
					   480*dmaTestStruct.channels,
					   dmaTestStruct.channels,
					   1000,
					   dmaTestStruct.amplitude);
		else if(dmaTestStruct.sampleFreq == 44100) 
		    status = RadOscillator(dmaTestStruct.dmaStruct[i].buffer,
					   352,
					   dmaTestStruct.channels,
					   1002,
					   dmaTestStruct.amplitude);

		if(status == -1) {
		    msg_printf(SUM,"Problem generating sine wave.\n");
		    return -1;

		}
	    }
	}
    }

    flushbus();

    /*
     * Set volume and gpio3
     */

    RAD_WRITE(RAD_VOLUME_CONTROL,dmaTestStruct.volume);

    /*
     * Start status dma 
     */
   
    RadSetupDMA(RAD_STATUS,statusbuf,2,1);   
    if(status == -1) {
	msg_printf(SUM,"Problems setting up Status DMA.\n");
	return -1;
    }

   RadStartStatusDMA(128,1);   

    for(i=0; i<dmaTestStruct.totaldma; i++) { 

	msg_printf(DBG,"Independent DMA operation: %d\n",i);
	msg_printf(DBG,"Dma mode: %d\n",dmaTestStruct.dmaStruct[i].dmaMode);
	msg_printf(DBG,"Dma src: %d\n",dmaTestStruct.dmaStruct[i].src);
	msg_printf(DBG,"Dma dst: %d\n",dmaTestStruct.dmaStruct[i].dst);

	/*
	 * Set status mask if not in spin mode
	 */

	if(!dmaTestStruct.spin) {
	    status = RadSetStatusMask(dmaTestStruct.dmaStruct[i].src,&mask);
	    if(status == -1) {
		msg_printf(DBG,"Problems setting up status mask\n");
		return -1;
	    }
	    if(dmaTestStruct.dmaStruct[i].dmaMode != SINGLE_PORT) {
		status = RadSetStatusMask(dmaTestStruct.dmaStruct[i].dst,&mask);
		if(status == -1) {
		    msg_printf(DBG,"Problems setting up status mask\n");
		    return -1;
		}
		
	    }
	}
	/*
	 * Set the clock generator for the interfaces
	 */

	msg_printf(DBG,"Setup Clock Generator 2 at sample freq: %d\n",
		   dmaTestStruct.sampleFreq);

	if(dmaTestStruct.sampleFreq == 48000)
	    status = RadSetupCG(2,48000,RAD_OSC1, &rad_reset);
	else
	    status = RadSetupCG(2,44100,RAD_OSC0, &rad_reset);
	if(status == -1) {
	    msg_printf(SUM,"Problems setting up Clock Generators\n");
	    return -1;
	}	

	msg_printf(DBG,"Setting up control reg to clock gen for src\n");

	status = RadSetClockIn(dmaTestStruct.dmaStruct[i].src,2);

	if(status == -1) {
	    msg_printf(SUM,"Problems setting Clock in ctrl registers\n");
	    return -1;
	}	
	if(dmaTestStruct.dmaStruct[i].dmaMode != SINGLE_PORT) {
	    msg_printf(DBG,"Setting up control reg to clock gen for dst\n");

	    status = RadSetClockIn(dmaTestStruct.dmaStruct[i].src,2);
	    
	    if(status == -1) {
		msg_printf(SUM,"Problems setting Clock in ctrl registers\n");
		return -1;
	    }	

	}

	/*
	 * Get interfaces out of reset
	 */
	
	msg_printf(DBG,"rad_dma: Take interfaces out of reset\n");

	/* 
	 * Take interfaces out of reset. Order is important
	 */

	status = RadUnresetInterfaces(dmaTestStruct.dmaStruct[i].src,
				      &rad_reset);
	    
	if(status == -1) {
	    msg_printf(SUM,"Problems taking interfaces out of reset\n");
	    return -1;
	}	
	if(dmaTestStruct.dmaStruct[i].dmaMode != SINGLE_PORT) {
	    status = RadUnresetInterfaces(dmaTestStruct.dmaStruct[i].dst,
					  &rad_reset);
	    if(status == -1) {
		msg_printf(SUM,"Problems taking interfaces out of reset\n");
		return -1;
	    }	 
	}

	msg_printf(DBG,"rad_dma: Setting up DMA registers\n");
	if(dmaTestStruct.sampleFreq == 48000) 
	    totalCaches = 480*dmaTestStruct.channels/32;
	else
	    totalCaches = 11;

	msg_printf(DBG,"Total Number of cache lines: %d \n",totalCaches);

	status = RadSetupDMA(dmaTestStruct.dmaStruct[i].src,
			     dmaTestStruct.dmaStruct[i].buffer,
			     totalCaches,
			     dmaTestStruct.spin);
	if(status == -1) {
	    msg_printf(SUM,"Problems setting up DMA Registers\n");
	    return -1;
	}	
	if(dmaTestStruct.dmaStruct[i].dmaMode == LOOPTHRU) {
	    status = RadSetupDMA(dmaTestStruct.dmaStruct[i].dst,
				 dmaTestStruct.dmaStruct[i].buffer,
				 totalCaches,
				 dmaTestStruct.spin);
	    if(status == -1) {
		msg_printf(SUM,"Problems setting up DMA Registers\n");
		return -1;
	    }	
	} else if(dmaTestStruct.dmaStruct[i].dmaMode == LOOPBACK) {
	    status = RadSetupDMA(dmaTestStruct.dmaStruct[i].dst,
				 dmaTestStruct.dmaStruct[i].bufferdst,
				 totalCaches,
				 dmaTestStruct.spin);
	    if(status == -1) {
		msg_printf(SUM,"Problems setting up DMA Registers\n");
		return -1;
	    }	
	}
	/*
	 * Start DMA
	 */
	msg_printf(SUM,"rad_dma: Starting dma\n");
	status = RadStartDMA(dmaTestStruct.dmaStruct[i].src,&miscControl);
	if(status == -1) {
	    msg_printf(SUM,"Problems starting up DMA\n");
	    return -1;
	}	
	if(dmaTestStruct.dmaStruct[i].dmaMode != SINGLE_PORT) {
	    msg_printf(SUM,"rad_dma: Starting dma\n");
	    status = RadStartDMA(dmaTestStruct.dmaStruct[i].dst,&miscControl);
	    if(status == -1) {
		msg_printf(SUM,"Problems setting up DMA Registers\n");
		return -1;
	    }	
	}
	
    }
    if(dmaTestStruct.spin) {
	msg_printf(SUM,"Press spacebar to stop DMA\n");
	pause(0," ",NULL);
	for(i=0; i<dmaTestStruct.totaldma; i++) {
	    msg_printf(DBG,"Stopping dma %d\n",
		       dmaTestStruct.dmaStruct[i].src);
	    RadStopDMA(dmaTestStruct.dmaStruct[i].src);

	    if(dmaTestStruct.dmaStruct[i].dmaMode != SINGLE_PORT) {
	    msg_printf(DBG,"Stopping dma %d\n",
		       dmaTestStruct.dmaStruct[i].dst);
		RadStopDMA(dmaTestStruct.dmaStruct[i].dst);
	    }
	}
    } else {  
	do {	    RadWait(100); /* wait for 100 ms */
	} while(statusbuf[29] & mask);
    }

    /*
     * Stop Status dma and exit
     */

    RadStopDMA(RAD_STATUS);
    
    /* 
     * Print some value 
     */
    
    msg_printf(SUM,"RAD Misc Control = %x\n",miscControl);
    msg_printf(SUM,"RAD Reset = %x\n",rad_reset);

    readback = RAD_READ(RAD_ATOD_CONTROL);
    msg_printf(SUM,"RAD ATOD Control = %x\n",readback);

    readback = RAD_READ(RAD_AES_TX_CONTROL);
    msg_printf(SUM,"RAD AES Tx Control Buffer = %x\n",readback);

    readback = RAD_READ(RAD_PCI_LOADR_ATOD);
    msg_printf(SUM,"RAD PCI Low Addr = %x\n",readback);

    readback = RAD_READ(RAD_PCI_HIADR_ATOD);
    msg_printf(SUM,"RAD PCI Hi Addr = %x\n",readback);

    readback = RAD_READ(RAD_PCI_CONTROL_ATOD);
    msg_printf(SUM,"RAD PCI Control ATOD = %x\n",readback);


    return status;

}
int RadCheckArgs(int argc, char** argv) 
{

    int i;
    int j;
    int pattern = 0x0000ff;
    /* 
     * Initialize default parameters 
     */
    
    dmaTestStruct.spin = 0;
    dmaTestStruct.verbose = 0;
    dmaTestStruct.reset = 0;
    dmaTestStruct.volume = 0xC0C0C0C0;
    dmaTestStruct.gpio3 = 18;

    dmaTestStruct.freq = 1000;          /* Sine wave parameters */
    dmaTestStruct.sampleFreq = 44100;
    dmaTestStruct.amplitude = 16;
    dmaTestStruct.channels = 2;

    dmaTestStruct.totaldma = 0;

    for(i=1; i<argc; i++) {
	if(!strcmp("-r",argv[i]))
	    dmaTestStruct.reset = 1;

	if(!strcmp("-s",argv[i]))
	    dmaTestStruct.spin = 1;

	if(!strcmp("-v",argv[i]))
	    dmaTestStruct.verbose = 1;

	if(!strcmp("-gpio3",argv[i]))
	    dmaTestStruct.gpio3 = atoi(argv[++i]);

	if(!strcmp("-vol",argv[i]))
	    dmaTestStruct.volume = atoi(argv[++i]);

	if(!strcmp("-lt",argv[i])) {
	    int current = dmaTestStruct.totaldma++;
	    if(argc < (i+2)) {
		msg_printf(SUM,"Improper number of arguments\n");
		return -1;
	    }

	    dmaTestStruct.dmaStruct[current].dmaMode = LOOPTHRU;

	    i++;
	    if(!strcmp("atod",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ATOD;
	    else if(!strcmp("dtoa",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_DTOA;
	    else if(!strcmp("aestx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_TX;
	    else if(!strcmp("aesrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_RX;
	    else if(!strcmp("adattx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_TX;
	    else if(!strcmp("adatrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_RX;
    
	    i++;
	    if(!strcmp("atod",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ATOD;
	    else if(!strcmp("dtoa",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_DTOA;
	    else if(!strcmp("aestx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_AES_TX;
	    else if(!strcmp("aesrx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_AES_RX;
	    else if(!strcmp("adattx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ADAT_TX;
	    else if(!strcmp("adatrx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ADAT_RX;

	}

	if(!strcmp("-lb",argv[i])) {
	    int current = dmaTestStruct.totaldma++;
	    if(argc < (i+2)) {
		msg_printf(SUM,"Improper number of arguments\n");
		return -1;
	    }
	    dmaTestStruct.dmaStruct[current].dmaMode = LOOPBACK;

	    i++;
	    if(!strcmp("atod",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ATOD;
	    else if(!strcmp("dtoa",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_DTOA;
	    else if(!strcmp("aestx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_TX;
	    else if(!strcmp("aesrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_RX;
	    else if(!strcmp("adattx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_TX;
	    else if(!strcmp("adatrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_RX;
    
	    i++;
	    if(!strcmp("atod",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ATOD;
	    else if(!strcmp("dtoa",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_DTOA;
	    else if(!strcmp("aestx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_AES_TX;
	    else if(!strcmp("aesrx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_AES_RX;
	    else if(!strcmp("adattx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ADAT_TX;
	    else if(!strcmp("adatrx",argv[i]))
		dmaTestStruct.dmaStruct[current].dst = RAD_ADAT_RX;

	}

	if(!strcmp("-sp",argv[i])) {
	    int current = dmaTestStruct.totaldma++;
	    dmaTestStruct.dmaStruct[current].dmaMode = SINGLE_PORT;
	    i++;

	    if(!strcmp("atod",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ATOD;
	    else if(!strcmp("dtoa",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_DTOA;
	    else if(!strcmp("aestx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_TX;
	    else if(!strcmp("aesrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_AES_RX;	
	    else if(!strcmp("adattx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_TX;
	    else if(!strcmp("adatrx",argv[i]))
		dmaTestStruct.dmaStruct[current].src = RAD_ADAT_RX;
	}

	if(!strcmp("-a",argv[i])) 
	    dmaTestStruct.amplitude = atoi(argv[++i]);
	    
	if(!strcmp("-sf",argv[i])) {
	    if(dmaTestStruct.sampleFreq == 44100 ||
	       dmaTestStruct.sampleFreq == 48000) 
		dmaTestStruct.sampleFreq = (uint_t)atoi(argv[++i]);
	    else {
		msg_printf(SUM,"rad_dma:Unsopported frequency, exiting.\n");
		return -1;
	    }
	}
    }
    msg_printf(DBG,"RadCheckArgs: freq=%d sf=%d amp=%d ch=%d\n",
	       dmaTestStruct.freq,dmaTestStruct.sampleFreq,
	       dmaTestStruct.amplitude,dmaTestStruct.channels);
	       
}

   

void
RadDMAPrintOptions() 
{
    msg_printf(SUM,"RAD DMA diagnostio: \n");
    msg_printf(SUM,"\tAvailable  DMA streams for operations\n");
    msg_printf(SUM,"\t\tatod\n");
    msg_printf(SUM,"\t\tdtoa\n");
    msg_printf(SUM,"\t\taesrx\n");
    msg_printf(SUM,"\t\taestx\n");
    msg_printf(SUM,"\t\tadattx\n");
    msg_printf(SUM,"\t\tadatrx\n\n");
    
    msg_printf(SUM,"Options: \n");
    msg_printf(SUM,"\t\t-h (this help screen) \n");
    msg_printf(SUM,"\t\t-lt <dma1 dma2>  loopthru mode \n");
    msg_printf(SUM,"\t\t-sp <dma> \n Single Port test\n");
    msg_printf(SUM,"\t\t-s (Spin mode)\n");
    msg_printf(SUM,"\t\t-v (Verbose mode)\n");
    msg_printf(SUM,"\t\t-gpio3 <1 | 0> (Set GPIO bit 3)\n");
    msg_printf(SUM,"\t\t-vol <volume> (Volume In and Out)\n");
}
