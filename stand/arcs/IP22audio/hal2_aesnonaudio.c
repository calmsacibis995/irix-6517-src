/*
 *  hal2_aesnonaudio.c 
 *
 *      gather AES nonaudio bytes (channel status and user status) from
 *      the AES RX part.
 */

#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes);
void PrintDescriptors(scdescr_t *desc);
void SetupHAL2(void);
void InitiateDMA(scdescr_t *desc_list, int direction, int channel);
void WaitForTwoDMAs(int, int);
void WaitForOneDMA(int which);

#define INFINITE 1

#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);

int sintab[48000];

int outbuf[NSAMPS*2];	/* the *2 is because it's stereo */
int inbuf[NSAMPS*2];	/* the *2 is because it's stereo */

scdescr_t  aes_output_descriptors[MAX_DESCRIPTORS];
scdescr_t  aes_input_descriptors[MAX_DESCRIPTORS];
scdescr_t  dac_output_descriptors[MAX_DESCRIPTORS];

char channel_status_bytes1[24];
char channel_status_bytes2[24];
char *prev, *curr, *temp;       /* previous and current chan stat bytes */

main()
{
    hal2_aesnonaudio();
}

void
hal2_aesnonaudio()
{
    hal2_unreset();
    
    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    

    init_aesrx();
    init_aestx();              /* init the tx just for good measure */
    
    printf("build AES input dma descriptors\n");
    BuildDescriptors(aes_input_descriptors, inbuf, NSAMPS*sizeof(long)*2);

    printf("Set Up HAL2\n");
    SetupHAL2();
    
    printf("Initiate AES input DMA\n");
    InitiateDMA(aes_input_descriptors, 1, 1);

    printf("gathering AES non-audio input data\n");
    gather_nonaudio();
}

void
BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes)
{
    int nremaining, bytes_till_page_boundary, bytes_this_descriptor;
    char *p;
    scdescr_t *desc;

    desc=list;
    nremaining=nbytes;
    p=(char *)startaddr;
    while(nremaining) {
	desc->cbp = K1_TO_PHYS(p);

        bytes_till_page_boundary = 4096-( (int)p & 0xfff);
        bytes_this_descriptor = MIN(bytes_till_page_boundary,nremaining);
        desc->bcnt = bytes_this_descriptor;

	desc->eox=0;
	desc->pad1=0;
	desc->xie=0;
	desc->pad0=0;

        desc->nbp = K1_TO_PHYS(desc+1);
        desc->word_pad = 0;
        nremaining -= bytes_this_descriptor;
	p+= bytes_this_descriptor;
	desc++;
    }
    desc--;
#ifdef INFINITE
    desc->nbp = K1_TO_PHYS(list);
    desc->eox = 0;
#else
    desc->nbp = 0;
    desc->eox = 1;
#endif
}

void
PrintDescriptors(scdescr_t *desc)
{
    while(1) {
        printf(
	"0x%8x: eox 0x%x pad1 0x%x xie 0x%x pad0 0x%4x bc %d\n",
	 desc,
         desc->eox,
         desc->pad1,
         desc->xie,
         desc->pad0,
         desc->bcnt);
         if(desc->eox)break;
	printf("Next address: 0x%8x, buffer address 0x%8x\n",
            desc->nbp, desc->cbp);
	desc++;
    }
}

/*
 * InitiateDMA 
 *
 *    initiates DMA on a particular channel and in a particular 
 *    direction (0 = frommips, 1 = tomips).
 */
void
InitiateDMA(scdescr_t *desc_list, int direction, int channel)
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t* ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(channel));
    volatile unsigned long *dpptr=
	(unsigned long *)PHYS_TO_K1(HPC3_PBUS_DP(channel));

    ctrlval.pad0=0;
    ctrlval.fifo_end=4*channel+3;
    ctrlval.pad1=0;
    ctrlval.fifo_beg=4*channel;
    ctrlval.highwater=11;
    ctrlval.pad2=0;
    ctrlval.real_time=1;
    ctrlval.ch_act_ld=1;
    ctrlval.ch_act=1;
    ctrlval.flush=0;
    ctrlval.receive=direction;
    ctrlval.little=0;
    ctrlval.pad3=0;

    *dpptr=K1_TO_PHYS(desc_list);
    *ctrlptr=ctrlval;
}

void
SetupHAL2(void)
{
    volatile unsigned long *idr0p=(unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *idr1p=(unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned long *idr2p=(unsigned long *)PHYS_TO_K1(HAL2_IDR2);
    volatile unsigned long *idr3p=(unsigned long *)PHYS_TO_K1(HAL2_IDR3);
    volatile unsigned long *iarp=(unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isrp=(unsigned long *)PHYS_TO_K1(HAL2_ISR);
    int inc, mod, modctrl;

    /* Set up the ISR */
    *isrp = 0;
    us_delay(50);
    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;

    /* Clear DMA ENABLE */

    *idr0p = 0x0;  /* Disable all the devices */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* Set up DMA Endian */

    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* Set up DMA DRIVE */

    *idr0p = 2;  /* activate dma channel 1 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /*
     * Here we set up the AES receiver and transmitter to the
     * extent possible. Note that this is just the DMA mapping
     * etc. We must do more work to actually get the RX and TX
     * up and running, viz. we must unreset the chips and send
     * some control values to the registers.
     */
     
    /* Set up AES RX */
    
    *idr0p = 0x1;        /* input DMA channel = 1 as specified above */
    *iarp  = HAL2_AESRX_CTRL_W;
    SPIN;         
             
    /* Set up DMA ENABLE */

    *idr0p = 0x2;  /* Enable AES RX*/
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

void
gather_nonaudio()
{
    unsigned long *aesrx_sr1 = (unsigned long *) PHYS_TO_K1(AESRX_SR1);
    unsigned long *aesrx_cs0 = (unsigned long *) PHYS_TO_K1(AESRX_CS0);
    unsigned long readback;
    int i;
    
    prev = channel_status_bytes1;
    curr = channel_status_bytes2;
    
    while (1) {        /* gather non-audio forever */
	/*
	 * read the status register in the AES RX and mask off
	 * the flags we're not interested in.
	 */
	readback = 0x3 & (*aesrx_sr1 >> 1);
	
	/*
	 * determine which portion of the AES block we're in.
	 * this location affects which part of the channel status
	 * and user status buffers are valid, and
	 * read the appropriate part of the buffer.
	 */
	switch (readback) {
            case 2:    /* flag 2 high, flag 1 low */
	        prev[4]  = *(aesrx_cs0 + 4);
	        prev[5]  = *(aesrx_cs0 + 5);
	        prev[6]  = *(aesrx_cs0 + 6);
	        prev[7]  = *(aesrx_cs0 + 7);
	        prev[8]  = *(aesrx_cs0 + 8);
	        prev[9]  = *(aesrx_cs0 + 9);
	        prev[10] = *(aesrx_cs0 + 10);
	        prev[11] = *(aesrx_cs0 + 11);
	        prev[12] = *(aesrx_cs0 + 12);
	        prev[13] = *(aesrx_cs0 + 13);
	        prev[14] = *(aesrx_cs0 + 14);
	        prev[15] = *(aesrx_cs0 + 15);
	        prev[16] = *(aesrx_cs0 + 16);
	        prev[17] = *(aesrx_cs0 + 17);
	        prev[18] = *(aesrx_cs0 + 18);
	        prev[19] = *(aesrx_cs0 + 19);
	        prev[20] = *(aesrx_cs0 + 20);
	        prev[21] = *(aesrx_cs0 + 21);
	        prev[22] = *(aesrx_cs0 + 22);
	        prev[23] = *(aesrx_cs0 + 23);
	        break;
            case 0:    /* flag 2 low, flag 1 low */
	        curr[0]  = *(aesrx_cs0);
		curr[1]  = *(aesrx_cs0 + 1);
		curr[2]  = *(aesrx_cs0 + 2);
		curr[3]  = *(aesrx_cs0 + 3);
		break;
	    case 1:    /* flag 2 low, flag 1 high */
	        
		curr[0]  = *(aesrx_cs0);
		curr[1]  = *(aesrx_cs0 + 1);
		curr[2]  = *(aesrx_cs0 + 2);
		curr[3]  = *(aesrx_cs0 + 3);
	    
		curr[4]   = *(aesrx_cs0 + 4);
	        curr[5]   = *(aesrx_cs0 + 5);
	        curr[6]   = *(aesrx_cs0 + 6);
	        curr[7]   = *(aesrx_cs0 + 7);
	        curr[8]   = *(aesrx_cs0 + 8);
	        curr[9]   = *(aesrx_cs0 + 9);
	        curr[10]  = *(aesrx_cs0 + 10);
	        curr[11]  = *(aesrx_cs0 + 11);
	        curr[12]  = *(aesrx_cs0 + 12);
	        curr[13]  = *(aesrx_cs0 + 13);
	        curr[14]  = *(aesrx_cs0 + 14);
	        curr[15]  = *(aesrx_cs0 + 15);
		
		/*
		 * print out the previous buffer....
		 */
		for (i = 0; i < 24; i++) {
		    printf("%x ", prev[i]);
		}
		putchar('\n');
	        /*
		 * this time around, we swap the idea of the 
		 * previous and current byte buffer.
		 */
		temp = prev;
		prev = curr;
		curr = temp;
	        break;
	} 
	/*
	 * wait around for a millisecond....just like the driver
	 * will.
	 */
	us_delay(1000);    /* 1000 usec = 1 msec */
    }
}
