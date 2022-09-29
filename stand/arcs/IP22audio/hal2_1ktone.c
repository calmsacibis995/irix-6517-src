#define INFINITE
void hal2_1ktone();
void MakeTable(int *tab);
void MakeTune(int *tunetab, int *sintab, int step, int nsamps);
void PrintTune(int *tunetab,int nsamps);
void BuildDescriptors(void);
void PrintDescriptors(void);
void SetupHAL2(void);
void InitiateDMA(void);
void WaitForDMA(void);

#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/IP22.h"

#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);

int sintab[48000];
int tunetab[NSAMPS*2];	/* the *2 is because it's stereo */
scdescr_t  desc_list[MAX_DESCRIPTORS];

main()
{
    volatile long *p6 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6));
    volatile long *p5 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(5));
    volatile long *p4 = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(4));
    *p6=0x141b;
    *p5=0x40e07;
    *p4=0x0e07;
    hal2_1ktone();
}

void
hal2_1ktone()
{
    printf("HAL2 1k tone test entered\n");
    hal2_configure_pbus_pio();
    printf("pbus pio configured\n");
    hal2_configure_pbus_dma();    
    printf("pbus dma configured\n");
    MakeTable(sintab);
    printf("sin table configured\n");
    MakeTune(tunetab,sintab,STEP,NSAMPS);
    printf("tune table configured\n");
/*
    PrintTune(tunetab,26);
*/
    printf("build dma descriptors\n");
    BuildDescriptors();
/*
    PrintDescriptors();
*/
    printf("Set Up HAL2\n");
    SetupHAL2();
    printf("Initiate DMA\n");
    InitiateDMA();
    printf("Wait for DMA\n");
    WaitForDMA();
    printf("HAL2 1k tone test done\n");
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
        /*Use a taylor series for cos x to compute values in the second octant*/
        *p2--= 65535-(t2>>1) + ( 30*t4 -t6)/720;
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

#define NORMAL_TUNE
#if defined NORMAL_TUNE
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
#elif defined ALTERNATING
void
MakeTune(int *tunetab, int *sintab, int step, int nsamps)
{
    int i,j,s;
    j=0;
    for(i=0;i<nsamps;i++) {
	*tunetab=0xffff;
	tunetab++;
	*tunetab=0xffff;
	tunetab++;
        j+=step;
	if(j>=48000)j-=48000;
    }
}
#else
void
MakeTune(int *tunetab, int *sintab, int step, int nsamps)
{
    int i,j,k,s,t;
    j=0;
    k=0;
    for(i=0;i<nsamps;i++) {
	s=(sintab[j]>>8)&0xff;
	t=(sintab[k]>>8)&0xff;

#ifdef LATER
	*tunetab=(s<<24)+(s<<16)+(t<<8)+t;
	tunetab++;
	*tunetab=(s<<24)+(s<<16)+(t<<8)+t;
	tunetab++;
#endif
	*tunetab=(s<<16)+s;
	tunetab++;
	*tunetab=(t<<16)+t;
	tunetab++;

        j+=step;
	if(j>=48000)j-=48000;
        k+=step+step;
	if(k>=48000)k-=48000;
    }
}
#endif
void
PrintTune(int *tunetab,int nsamps)
{
    int i;
    for(i=0;i<nsamps;i+=2) {
	printf("%d	%d %d	%d	%d %d\n",
		i,tunetab[0],tunetab[1],
		1+i,tunetab[2],tunetab[3]);
	tunetab+=4;
    }
}
void
BuildDescriptors()
{
    int nbytes, nremaining, bytes_till_page_boundary, bytes_this_descriptor;
    char *p;
    scdescr_t *desc;

    desc=desc_list;
    nbytes=NSAMPS*sizeof(long)*2;
    nremaining=nbytes;
    p=(char *)tunetab;
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
    desc->nbp = K1_TO_PHYS(desc_list);
    desc->eox = 0;
#else
    desc->nbp = 0;
    desc->eox = 1;
#endif

}

void
PrintDescriptors()
{
    scdescr_t *desc;
    desc=desc_list;
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
** InitiateDMA - initiates a transmit DMA on channel 0 of the descriptor
** 		 chain starting at desc_list.
*/
void
InitiateDMA()
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t* ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(0));
    volatile unsigned long *dpptr=(unsigned long *)PHYS_TO_K1(HPC3_PBUS_DP(0));

    ctrlval.pad0=0;
    ctrlval.fifo_end=47;
    ctrlval.pad1=0;
    ctrlval.fifo_beg=0;
    ctrlval.highwater=11;
    ctrlval.pad2=0;
    ctrlval.real_time=1;
    ctrlval.ch_act_ld=1;
    ctrlval.ch_act=1;
    ctrlval.flush=0;
    ctrlval.receive=0;
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

    /* Set the relay (for Indigo-type audio) */
    *idr0p = 0x1;
    *iarp  = HAL2_RELAY_CONTROL_W;
    SPIN;


    /* Set up DMA DRIVE */

    *idr0p = 1;  /* Activate only port 0 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /* Set up CODEC A */

    *idr0p = 0		/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(2<<8);	/* Stereo Mode */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;
    *idr0p = (0<<10)+(0x00<<7)+(0x00<<2);	/* unmute, A/D gains == 0 */
    *idr1p = (0x0<<7)	/* Left D/A output atten */
	    +(0x0<<2)    /* Right D/A output atten */
	    +(0<<1)	/* Digital output port data bit 1 */
	    +(0);	/* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
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

    *idr0p = 0x8;  /* Enable only DAC A*/
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

void
WaitForDMA(void)
{
    volatile unsigned  long *revptr=(unsigned long *)PHYS_TO_K1(HAL2_REV);
    volatile unsigned  long *idr0ptr=(unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    /*volatile unsigned  char *ser1ptr=(unsigned char *)PHYS_TO_K1(0xbfbd9807);*/
    volatile unsigned  char *ser1ptr=(unsigned char *)PHYS_TO_K1(0xbfbd9837);
    long rev;
    volatile pbus_control_read_t* ctrlptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(0));
    pbus_control_read_t ctrlval;
    int ntimes;
    int maxtimes=3000;

#ifdef INFINITE
    for(ntimes=0;;ntimes++) {
#else
    for(ntimes=0;ntimes<maxtimes;ntimes++) {
#endif
	ctrlval = *ctrlptr;
	if(!ctrlval.ch_act)break;
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
