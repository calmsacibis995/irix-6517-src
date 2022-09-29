/*
 *   hal2_aestimestamp.c - test timestamps on AES input
 */

#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void hal2_aestimestamp();
void BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes);
void PrintDescriptors(scdescr_t *desc);
void SetupHAL2(void);
void InitiateDMA(scdescr_t *desc_list, int direction, int channel);
void WaitForTwoDMAs(int, int);
void WaitForOneDMA(int which);
void init_aestx();
void init_aesrx();

#define EMULATION  1
#define TIMESTAMPMODE 1

#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);

int sintab[48000];

int outbuf[NSAMPS*2];	/* the *2 is because it's stereo */
int inbuf[NSAMPS*4];	/* the *4 is because it's stereo with timestamps */

scdescr_t  aes_output_descriptors[MAX_DESCRIPTORS];
scdescr_t  aes_input_descriptors[MAX_DESCRIPTORS];
scdescr_t  dac_output_descriptors[MAX_DESCRIPTORS];

main()
{
    hal2_unreset();
    hal2_aestimestamp();
}

int mode;

void
hal2_aestimestamp()
{
    volatile int i;
    char str[256];

    volatile long *p6 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6));
    volatile long *p5 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(5));
    volatile long *p4 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(4));
    *p6=0x141b;
    *p5=0x40e07;
    *p4=0x0e07;

    printf("HAL2 AES loop thru test entered\n");
#ifdef LATER
    printf("Enter 1 for external VCO, 0 for internal: \n");
    gets(str);
    mode = atoi(str);
#endif
    mode = 1;

    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    

    init_aesrx();
    init_aestx();

    MakeTable(sintab);
    MakeTune(outbuf,sintab,STEP,NSAMPS);

    printf("build AES output dma descriptors\n");
    BuildDescriptors(aes_output_descriptors, outbuf,NSAMPS*sizeof(long)*2);

    printf("build AES input dma descriptors\n");
    BuildDescriptors(aes_input_descriptors, inbuf,NSAMPS*sizeof(long)*4);

    printf("build DAC output dma descriptors\n");
    BuildDescriptors(dac_output_descriptors, inbuf,NSAMPS*sizeof(long)*2);

    printf("Set Up HAL2\n");
    SetupHAL2();

    printf("Initiate AES output DMA\n");
    InitiateDMA(aes_output_descriptors, 0, 0);

    for(i=0;i<10000;i++);    /* wait for a moment */

    printf("Initiate AES input DMA\n");
    InitiateDMA(aes_input_descriptors, 1, 1);

    for(i=0;i<10000;i++);    /* wait for a moment */

#ifdef NEVER
    printf("Initiate DAC output DMA\n");
    InitiateDMA(dac_output_descriptors, 0, 2);
#endif

    WaitForTwoDMAs(0, 1);
    ExamineTimestamps(inbuf, NSAMPS, TIMESTAMPMODE);
}

/* Make a 48000 element table containing one cycle of 32767*sin(x) */

void
MakeTable(int *tab)
{
    unsigned int theta,t2,t3,t4,t5,t6,t7;
    int *p1,*p2;
    int i;

    /* Compute the first quadrant */
    p1=tab; p2=p1+12000;
    for(i=0;i<=6000;i++) {
        theta=i*51472;            /* theta=i*.25*pi*65536 */
        theta/=6000;              /* theta=i*.25*pi*65536/6000 */
        t2=(theta*theta)>>16;
        t3=(t2*theta)>>16;
        t4=(t2*t2)>>16;
        t5=(t2*t3)>>16;
        t6=(t3*t3)>>16;
        t7=(t3*t4)>>16;
        /*Use a taylor series for sin x to compute values in the first octant*/
        *p1++=(5040*theta-840*t3+42*t5-t7)/5040;
        /*Use a taylor series for cos x to compute values in the second octant*/        *p2--= 65535-(t2>>1) + ( 30*t4 -t6)/720;
    }

    /* Scale and limit */
    p1=tab; p2=p1+12000;
    while(p1<=p2) {
        t7 = (*p1>>1);
        if(t7>=32767)
            t7=32767;
        *p1=t7;
        p1++;
    }

    /* Reflect the first quadrant into the second quadrant */
    p1=tab; p2=p1+24000;
    while(p1<=p2) {
        *p2 = *p1;
        p1++;
        p2--;
    }
    /* Reflect the first two quadrants int the last two quadrants */
    p1=tab+1; p2=tab+47999;
    while(p1<=p2) {
        *p2 = -(*p1);
        p1++;
        p2--;
    }
}


void
MakeTune(int *tunetab, int *sintab, int step, int nsamps)
{
    int i,j,s;
    j=0;
    for(i=0;i<nsamps;i++) {
        *tunetab=sintab[j]<<8;
        tunetab++;
        *tunetab=sintab[j]<<8;
        tunetab++;
        j+=step;
        if(j>=48000)j-=48000;
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

    *idr0p = 3;  /* activate dma channels 0, 1 and 2 */
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
    
    *idr0p = 0x1 +
             (TIMESTAMPMODE << 6) +
             (0x1 << 5);   /* timestamping on */
    *iarp  = HAL2_AESRX_CTRL_W;
    SPIN;         
             
    /* Set up AES TX */
    
    *idr0p = 0 +       /* output DMA channel = 0 */
             (1<<3) +  /* assign bres clock */
	     (2<<8);   /* stereo mode */
    *iarp  = HAL2_AESTX_CTRL_W;
    SPIN;

    /*
     * Set up Codec A (the output codec)
     *
     *  output on dma channel 2
     *  stereo mode
     *  bres clock 1
     */
    *idr0p = 2          /* Physical DMA channel */
            +(2<<3)     /* Bres Clock ID */
#ifdef LATER
            +(2<<8);    /* Stereo Mode */
#endif
            +(3<<8);    /* fool the output to put out timestamped data */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;
    *idr0p = (0<<10)+(0x00<<7)+(0x00<<2);       /* unmute, A/D gains == 0 */
    *idr1p = (0x4<<7)   /* Left D/A output atten */
            +(0x4<<2)    /* Right D/A output atten */
            +(0<<1)     /* Digital output port data bit 1 */
            +(0);       /* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;

    
    /* 
     * Set up BRES CLOCK 1 
     */

    *idr0p = 0;		
    *iarp = HAL2_BRES1_CTRL1_W;
    SPIN;
    inc=1;
    mod=1;
    modctrl = 0xffff & (inc-mod-1);
    *idr0p = inc;
    *idr1p = modctrl;
    *iarp = HAL2_BRES1_CTRL2_W;
    SPIN;

    /*
     * Set up BRES CLOCK 2
     */

    *idr0p = 2;                  /* master = AES RX */
    *iarp = HAL2_BRES2_CTRL1_W;
    SPIN;
    inc=1;
    mod=1;
    modctrl = 0xffff & (inc-mod-1);
    *idr0p = inc;
    *idr1p = modctrl;
    *iarp = HAL2_BRES2_CTRL2_W;
    SPIN;


    /* Set up DMA ENABLE */

    *idr0p = 0x4 + 0x2;  /* Enable AES TX, AES RX, and DAC */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

void
WaitForThreeDMAs(void)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(0));
    volatile pbus_control_read_t* ctrl1ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(1));
    volatile pbus_control_read_t* ctrl2ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(2));
    pbus_control_read_t ctrl0val,ctrl1val,ctrl2val;
    int ntimes;
    int maxtimes=3000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	ctrl1val = *ctrl1ptr;
	ctrl2val = *ctrl2ptr;
	if(!ctrl0val.ch_act && !ctrl1val.ch_act && !ctrl2val.ch_act)break;
	us_delay(10000);
    }
    if(ntimes==maxtimes) {
	printf("Dma did not complete after %d*.01 seconds\n",ntimes);
    } else {
	printf("Dma complete after %d*.01 seconds\n",ntimes);
    }
}

void
WaitForTwoDMAs(int first, int second)
{
    volatile pbus_control_read_t* ctrl0ptr=
        (pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(first));
    volatile pbus_control_read_t* ctrl1ptr=
        (pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(second));
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

void
ExamineTimestamps(int *buf, int nframes, int mode)
{
    volatile i;
    int delta_usec;
    long delta_tenthusec;
    static int prevTimestampLSW = 0, prevTimestampMSW = 0;
    int timestampLSW, timestampMSW;

    switch (mode) {

    case 0:      /* seconds/microseconds mode */
   
    for (i = 0; i < nframes; i++) {
        timestampMSW = buf[2];      /* upper 32 bits of stamp */
        timestampLSW = buf[3];      /* lower 32 bits of stamp */
        buf += 4;                   /* next frame, please */

        delta_usec = (timestampMSW - prevTimestampMSW) * 1000000 +
            (timestampLSW - prevTimestampLSW);
        if (delta_usec < 0) {
            printf("time is going backwards: frame=%d %d\n", i, delta_usec);
        }
        if (i % 10000 == 0) {
            printf("frame %d: L=%x R=%x ", i, buf[0], buf[1]);
            printf("ts(msw)=%d ts(lsw)=%d ", buf[2], buf[3]);
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
            timestampMSW = buf[2];      /* upper 32 bits of stamp */
            timestampLSW = buf[3];      /* lower 32 bits of stamp */
            buf += 4;                   /* next frame, please */

            delta_tenthusec = (timestampMSW - prevTimestampMSW) * 10000000 +
              ((0x7ffff & timestampLSW) - (0x7ffff & prevTimestampLSW)) * 10 +
              (0xf & (timestampLSW >> 20) - 0xf & (prevTimestampLSW >> 20));
            if (delta_tenthusec < 0) {
                printf("time is going backwards:%d %d\n", i, delta_tenthusec);
            }
            if (i % 10000 == 0) {
                printf("frame %d: L=%x R=%x ", i, buf[0], buf[1]);
                printf("ts(msw)=%d ts(lsw)=%d ", buf[2], buf[3]);
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

