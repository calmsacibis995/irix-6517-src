/*
 *    hal2_timestamp_quad.c - 
 *    
 *        test the timestamping mechanism on the hal2 chip.
 * 
 *    tidbits of wisdom:
 * 
 *    1. timestamp is 64 bits = 2 audio samples
 *    2. frame of data is order as follows: audio L, audio R, 
 *       timestamp (for stereo of course).
 *    3. timestamps come in 2 flavors:
 *       a. second/microsecond
 *       b. second/tenths of microseconds/microseconds
 * 
 *    thus, we have for one frame (of stereo data):
 *    
 *         32 bits      32 bits     64 bits
 *            L            R       TIMESTAMP
 * 
 *    for quad things look just a tad different:
 * 
 *         l (front)   r (front)   l (back)   r (back)  timestamp
 * 
 *        framesize = 4 bytes * 4 + 8 bytes = 24 bytes = 6 int's
 */
 
#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void hal2_timestamp_quad();
void BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes);
void PrintDescriptors(scdescr_t *desc);
void SetupHAL2(void);
void InitiateDMA(scdescr_t *desc_list, int direction /*0=frommips, 1=tomips*/);
void WaitForTwoDMAS(void);
void WaitForOneDMA(int which);
void ExamineTimestamps(int *soundbuf, int nframes, int mode);

#define TIMESTAMP_MODE 1   /* 0 = sec/usec, 1 = sec/usec/tenths of usec */
#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);


#ifdef NOSTAMPS
int soundbuf[NSAMPS*4];	/* the *4 is because it's quad */
#else
int soundbuf[NSAMPS*6];	/* the *6 is because it's stereo w/stamps */
#endif
scdescr_t  input_descriptors[MAX_DESCRIPTORS];
scdescr_t  output_descriptors[MAX_DESCRIPTORS];
static    int timestampmode = TIMESTAMP_MODE;

main()
{
    hal2_unreset();
    hal2_timestamp_quad();
}

void
hal2_timestamp_quad()
{
    char line[256];
    volatile int i;
    
    printf("HAL2 audio timestamp test entered\n");
    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    

    printf("Enter the timestamp mode (0=sec/usec;1=sec/usec/0.1*usec):\n");
    gets(line); 
    timestampmode=atoi(line);

    printf("build input dma descriptors\n");
#ifdef NOSTAMPS
    BuildDescriptors(input_descriptors, soundbuf,NSAMPS*sizeof(long)*4);
#else
    BuildDescriptors(input_descriptors, soundbuf,NSAMPS*sizeof(long)*6);
#endif
    /*
    PrintDescriptors(input_descriptors);
    */
    printf("Set Up HAL2\n");
    SetupHAL2();

    printf("Initiate input DMA\n");
#ifdef NOSTAMPS
    for(i=0;i<NSAMPS*4;i++) { 
#else
    for(i=0;i<NSAMPS*6;i++) { 
#endif
        soundbuf[i]=0;
    }
    InitiateDMA(input_descriptors, 1);

    printf("Wait for input DMA\n");
    WaitForOneDMA(1);
    /*
    for(i=0;i<10000;i++) {
	printf("%x ",soundbuf[i]);
	if(i%8==7)printf("\n");
    }
    */
#ifndef NOSTAMPS
    ExamineTimestamps(soundbuf, NSAMPS, timestampmode);
#else
    for (i = 0; i < NSAMPS*4; i+=4) {
       if (i % 1000 == 0) printf("%x %x %x %x\n", soundbuf[i],
          soundbuf[i+1], soundbuf[i+2], soundbuf[i+3]);
    }
#endif
    printf("HAL2 audio timestamp test done\n");
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

        bytes_till_page_boundary = 4096-((int)p & 0xfff);
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
InitiateDMA(scdescr_t *desc_list, int direction /*0=frommips, 1=tomips*/)
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t* ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(direction));
    volatile unsigned long *dpptr=
	(unsigned long *)PHYS_TO_K1(HPC3_PBUS_DP(direction));

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
#ifndef NOSTAMPS
    us_delay(50);
    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N|
            HAL2_ISR_CODEC_MODE;   /* unreset and put in quad mode */
    if (*isrp != HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N|HAL2_ISR_CODEC_MODE) printf("bad isr value\n");
#endif

    /* Clear DMA ENABLE */

    *idr0p = 0x0;  /* Disable all the devices */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* Set up DMA Endian */

    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* Set up DMA DRIVE */

    *idr0p = 2;  /* Activate only port 1 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /* 
     * Set up CODEC A 
     * 
     * Need to set up codec A even though we are doing input
     * only. For one thing we need to indicate that the DMA 
     * channel is going to be quad. Unfortunately, the only
     * way to do that is to make the channel attached to CODEC
     * A a quad channel. Then the B channel is forced to be
     * quad by the HAL2 (I think).
     * 
     * Now, don't know whether we must actually activate this
     * DMA channel or not for all this to work. Also, don't 
     * know whether we need to drive this DMA channel (see
     * assignment of DMA_DRIVE above).
     */

    *idr0p = 0		/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(3<<8);	/* Quad Mode */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;
    *idr0p = (0<<10)+(0x0<<8)+(0x00<<0);       /* unmute, A/D gains == 0 */
    *idr1p = (0x03<<7)  /* Left D/A output atten */
            +(0x03<<2)  /* Right D/A output atten */
            +(0<<1)     /* Digital output port data bit 1 */
            +(0);       /* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;

    /* 
     * Set up CODEC B
     *
     * This is the guy we're interested in!
     */

    *idr0p = 1		/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
#ifndef NOSTAMPS
	    +(1<<5)     /* Time stamping */
#endif
	    +(timestampmode<<6);
    *iarp = HAL2_CODECB_CTRL1_W;
    SPIN;

    *idr0p = (0<<10)+(0x3<<8)+(0x0<<4)+(0x0);
    /* unmute, left & right from mic, A/D gains == 00 */

    *idr1p = (0<<7)	/* Left D/A output atten */
	    +(0<<2)     /* Right D/A output atten */
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

    /* Set up DMA ENABLE */

    /*
     * for now do not enable the DAC (codec A). don't know
     * whether this will influence the success or failure of
     * the program.
     */
    *idr0p = 0x8 + 0x10;  /* Enable only ADC B*/
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

void
WaitForTwoDMAs(void)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(0));
    volatile pbus_control_read_t* ctrl1ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(1));
    pbus_control_read_t ctrl0val,ctrl1val;
    int ntimes;
    int maxtimes=3000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	ctrl1val = *ctrl1ptr;
	if(!ctrl0val.ch_act && !ctrl1val.ch_act)break;
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

    for (ntimes = 0; ntimes < maxtimes; ntimes++) {
	ctrl0val = *ctrl0ptr;
	if(!ctrl0val.ch_act) break;
	us_delay(10000);
    }
    
    if (ntimes == maxtimes) {
	printf("Dma %d did not complete after %d*.01 seconds\n",which,ntimes);
    } 
    else {
	printf("Dma %d complete after %d*.01 seconds\n",which,ntimes);
    }
}

void
ExamineTimestamps(int *buf, int nframes, int mode)
/*
 * Now make this work with 6 int's per frame.
 */
{
    volatile i;
    int delta_usec; 
    long delta_tenthusec;
    static int prevTimestampLSW = 0, prevTimestampMSW = 0;
    int timestampLSW, timestampMSW;

    switch (mode) {

    case 0:      /* seconds/microseconds mode */
    
	for (i = 0; i < nframes; i++) {
	    timestampMSW = buf[4];      /* upper 32 bits of stamp */
	    timestampLSW = buf[5];      /* lower 32 bits of stamp */
	    buf += 6;                   /* next frame, please */
	    
	    delta_usec = (timestampMSW - prevTimestampMSW) * 1000000 +
		(timestampLSW - prevTimestampLSW);
	    if (delta_usec < 0) {
		printf("time is going backwards: frame=%d %d\n", i, delta_usec);
	    }
	    if (i % 1000 == 0) {
                printf("lf:%x rf:%x lb:%x rb:%x tsm:%x tsl:%x ",
                    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
		printf("ts(msw)=%d ts(lsw)=%d ", buf[4], buf[5]);
		printf("delta_usec=%d\n", delta_usec);
	    }
#ifdef LATER
	    /* check on things every 1000 frames */
	    if (i % 1000 == 0) {
		printf("%d: MSW = %d LSW = %d\n", i, 
		    timestampMSW, timestampLSW);
	    }
#endif
	    prevTimestampMSW = timestampMSW;
	    prevTimestampLSW = timestampLSW;
	}
        break;

    case 1:      /* seconds/tenths of microseconds/microseconds */
        /*
         *  bit allocation within the words:
         *
         *    MSW = seconds
         *    LSW[0-19] = microseconds
         *    LSW[20-23] = tenths of microseconds
         */
        for (i = 0; i < nframes; i++) {
            timestampMSW = buf[4];      /* upper 32 bits of stamp */
	    timestampLSW = buf[5];      /* lower 32 bits of stamp */
	    buf += 6;                   /* next frame, please */
	
            delta_tenthusec = (timestampMSW - prevTimestampMSW) * 10000000 +
              ((0x7ffff & timestampLSW) - (0x7ffff & prevTimestampLSW)) * 10 +
              (0xf & (timestampLSW >> 20) - 0xf & (prevTimestampLSW >> 20));
            if (delta_tenthusec < 0) {
                printf("time is going backwards:%d %d\n", i, delta_tenthusec);
            }
            if (i % 10000 == 0) {
                printf("ts(msw)=%d ts(lsw)=%d ", buf[4], buf[5]);
                printf("delta_tenthusec=%d\n", delta_tenthusec);
        }
#ifdef LATER
        /* check on things every 1000 frames */
        if (i % 1000 == 0) {
            printf("%d: MSW = %d LSW = %d\n", i,
                timestampMSW, timestampLSW);
        }
#endif
            prevTimestampMSW = timestampMSW;
            prevTimestampLSW = timestampLSW;
        }
        break;
    }
}
