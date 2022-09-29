/*
 *  hal2_loopwalk.c -
 * 
 *     walk thru all DMA channel and do loopthru of audio data.
 *     this will test out all the DMA channels on the HAL2 for
 *     input and output capability.
 */
#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void hal2_loopwalk();
void BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes);
void PrintDescriptors(scdescr_t *desc);
void SetupHAL2(void);
void SetupDMAChannels(int, int);
void InitiateDMA(scdescr_t *desc_list, int direction, int channel);
void WaitForTwoDMAs(int, int);

#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);


int soundbuf[NSAMPS*2];	/* the *2 is because it's stereo */
scdescr_t  input_descriptors[MAX_DESCRIPTORS];
scdescr_t  output_descriptors[MAX_DESCRIPTORS];

main()
{
    hal2_unreset();
    hal2_loopwalk();
}

void
hal2_loopwalk()
{
    volatile int i, j, numchannels = 8;
    printf("HAL2 audio loopthru test entered\n");
    
    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    

    printf("build input dma descriptors\n");
    BuildDescriptors(input_descriptors, soundbuf,NSAMPS*sizeof(long)*2);
    printf("build input dma descriptors\n");
    BuildDescriptors(output_descriptors, soundbuf,NSAMPS*sizeof(long)*2);
    
    printf("Set Up HAL2\n");
    SetupHAL2();

    for (i = 0; i < numchannels; i++) {
        j = i + 1;
	if (j == numchannels) j = 0;

        printf("input DMA channel = %d output DMA channel = %d\n", 
	    i, j);
	
	SetupDMAChannels(i, j);
	InitiateDMA(input_descriptors, 1, i);
	InitiateDMA(output_descriptors, 0, i+1);
	
	printf("Wait for DMA\n");
	WaitForTwoDMAs(i, j);
    }
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
    desc->nbp = 0;
    desc->eox = 1;
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
 * InitiateDMA - initiates a transmit DMA on channel 0 of the descriptor
 * 		 chain starting at desc_list.
 */
void
InitiateDMA(scdescr_t *desc_list, int direction, int channel)
/*0=frommips, 1=tomips*/
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t* ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(channel));
    volatile unsigned long *dpptr=
	(unsigned long *)PHYS_TO_K1(HPC3_PBUS_DP(channel));

    ctrlval.pad0=0;
    ctrlval.fifo_end=4*direction+3;
    ctrlval.pad1=0;
    ctrlval.fifo_beg=4*direction;
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

    /* Set up DMA Endian */
    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* set up part of CODEC A control */
    *idr0p = (0<<10)+(0x00<<7)+(0x00<<2);	/* unmute, A/D gains == 0 */
    *idr1p = (0x7<<7)	/* Left D/A output atten */
	    +(0x7<<2)   /* Right D/A output atten */
	    +(0<<1)	/* Digital output port data bit 1 */
	    +(0);	/* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;
    
    /* set up part of CODEC B control */
    *idr0p = (0<<10)+(0<<8)+(0x0<<4)+(0x0);
    /* unmute, left & right from mic, A/D gains == ff */

    *idr1p = (0<<7)	/* Left D/A output atten */
	    +(0<<2)    /* Right D/A output atten */
	    +(0<<1)	/* Digital output port data bit 1 */
	    +(0);	/* Digital output port data bit 0 */
    *iarp = HAL2_CODECB_CTRL2_W;
    SPIN;

    /* Set up BRES CLOCK 1 */
    *idr0p = 0;		/* 48khz master */
    *iarp = HAL2_BRES1_CTRL1_W;
    SPIN;
    inc=1;
    mod=1;
    modctrl = 0xffff & (inc-mod-1);
    *idr0p = inc;
    *idr1p = modctrl;
    *iarp = HAL2_BRES1_CTRL2_W;
    SPIN;
}

void
SetupDMAChannels(int input, int output)
{
    volatile unsigned long *idr0p=(unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned long *iarp=(unsigned long *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned long *isrp=(unsigned long *)PHYS_TO_K1(HAL2_ISR);
    
    /* Clear DMA ENABLE */
    *idr0p = 0x0;  /* Disable all the devices */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* Set up DMA DRIVE */
    *idr0p = (1<<input) + (1<<output);  /* Activate only port 0 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /* Set up CODEC A */
    *idr0p = output	/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(2<<8);	/* Stereo Mode */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;

    /* Set up CODEC B */
    *idr0p = input	/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(2<<8);	/* Stereo Mode */
    *iarp = HAL2_CODECB_CTRL1_W;
    SPIN;

    /* Set up DMA ENABLE */
    *idr0p = 0x18;  /* Enable CODECs A and B */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

void
WaitForTwoDMAs(int input, int output)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(input));
    volatile pbus_control_read_t* ctrl1ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(output));
    pbus_control_read_t ctrl0val,ctrl1val;
    int ntimes;
    int maxtimes=3000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	ctrl1val = *ctrl1ptr;
	if(!ctrl0val.ch_act && !ctrl1val.ch_act)break;
	switch(ntimes&3) {
	case 0:
	    printf("/%c",8);
	    break;
	case 1:
	    printf("-%c",8);
	    break;
	case 2:
	    printf("\\%c",8);
	    break;
	case 3:
	    printf("-%c",8);
	    break;
	}
	us_delay(10000);
    }
    if(ntimes==maxtimes) {
	printf("Dma did not complete after %d*.01 seconds\n",ntimes);
    } else {
	printf("Dma complete after %d*.01 seconds\n",ntimes);
    }
}

void
WaitForOneDMA(int which)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(which));
    pbus_control_read_t ctrl0val;
    int ntimes;
    int maxtimes=3000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	if(!ctrl0val.ch_act)break;
	us_delay(10000);
    }
    if(ntimes==maxtimes) {
	printf("Dma %d did not complete after %d*.01 seconds\n",which,ntimes);
    } else {
	printf("Dma %d complete after %d*.01 seconds\n",which,ntimes);
    }
}
