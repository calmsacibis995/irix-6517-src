#include "sys/sbd.h"
#include "sys/hal2.h"
#include "sys/hpc3.h"

void hal2_loopthru();
void BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes);
void PrintDescriptors(scdescr_t *desc);
void SetupHAL2(void);
void InitiateDMA(scdescr_t *desc_list, int direction /*0=frommips, 1=tomips*/);
void WaitForTwoDMAS(void);
void WaitForOneDMA(int which);

#define STEP 1000    	/* Generates a 1Khz tone */
#define NSAMPS 96000 	/* 2 seconds of tune */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX_DESCRIPTORS 1000
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);

int soundbuf[NSAMPS*2];	/* the *2 is because it's stereo */
int buf0[10000];
int buf1[10000];
scdescr_t  input_descriptors[MAX_DESCRIPTORS];
scdescr_t  output_descriptors[MAX_DESCRIPTORS];

main()
{
    volatile long *p = (volatile long *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6));
    *p=0x2143b;
    hal2_loopthru();
}

void
hal2_loopthru()
{
volatile int i;
    printf("HAL2 audio loopthru test entered\n");
    hal2_configure_pbus_pio();
    hal2_configure_pbus_dma();    

    printf("build input dma descriptors\n");
    BuildDescriptors(input_descriptors, soundbuf,NSAMPS*sizeof(long)*2);
/*
    PrintDescriptors(input_descriptors);
*/
    printf("build output dma descriptors\n");
    BuildDescriptors(output_descriptors, soundbuf,NSAMPS*sizeof(long)*2);
/*
    PrintDescriptors(output_descriptors);
*/

    printf("Set Up HAL2\n");
    SetupHAL2();

    printf("Initiate input DMA\n");
    InitiateDMA(input_descriptors, 1);

#ifdef NEVER
    printf("Wait for input DMA\n");
    WaitForOneDMA(1);
    for(i=0;i<1000;i++) {
	printf("%x ",soundbuf[i]);
	if(i%8==7)printf("\n");
    }
#endif

    for(i=0;i<10000;i++);

    printf("Initiate output DMA\n");
    InitiateDMA(output_descriptors, 0);
    printf("Wait for output DMA\n");
/*
    WaitForOneDMA(0);
*/
    WaitForTwoDMAs(0);

done:
    printf("HAL2 audio loopthru test done\n");
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
** InitiateDMA - initiates a transmit DMA on channel 0 of the descriptor
** 		 chain starting at desc_list.
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

    /* Clear DMA ENABLE */

    *idr0p = 0x0;  /* Disable all the devices */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* Set up DMA Endian */

    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* Set up DMA DRIVE */

    *idr0p = 3;  /* Activate only ports 0 and 1 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /* Set up CODEC A */

    *idr0p = 0		/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(2<<8);	/* Stereo Mode */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;
    *idr0p = (0<<10)+(0x00<<7)+(0x00<<2);	/* unmute, A/D gains == 0 */
    *idr1p = (0x4<<7)	/* Left D/A output atten */
	    +(0x4<<2)    /* Right D/A output atten */
	    +(0<<1)	/* Digital output port data bit 1 */
	    +(0);	/* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;

    /* Set up CODEC B */

    *idr0p = 1		/* Physical DMA channel */
	    +(1<<3)	/* Bres Clock ID */
	    +(2<<8);	/* Stereo Mode */
    *iarp = HAL2_CODECB_CTRL1_W;
    SPIN;

    *idr0p = (0<<10)+(0<<8)+(0x0<<4)+(0x0);
    /* unmute, left & right from mic, A/D gains == 00 */

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
    mod=4;
    modctrl = 0xffff & (inc-mod-1);
    *idr0p = inc;
    *idr1p = modctrl;
    *iarp = HAL2_BRES1_CTRL2_W;
    SPIN;

    /* Set up DMA ENABLE */

    *idr0p = 0x8+0x10;  /* Enable only DAC A and ADC B*/
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
    volatile unsigned long * bp0ptr=
	(unsigned long*)PHYS_TO_K1(HPC3_PBUS_BP(0));
    volatile unsigned long * bp1ptr=
	(unsigned long*)PHYS_TO_K1(HPC3_PBUS_BP(1));
    int i,ntimes;
    int maxtimes=3000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	ctrl1val = *ctrl1ptr;
	if(!ctrl0val.ch_act && !ctrl1val.ch_act)break;
	buf0[ntimes] = *bp0ptr;
	buf1[ntimes] = *bp1ptr;
	us_delay(10000);
    }
    if(ntimes==maxtimes) {
	printf("Dma did not complete after %d*.01 seconds\n",ntimes);
    } else {
	printf("Dma complete after %d*.01 seconds\n",ntimes);
    }
    for (i = 0; i < ntimes; i++) {
	printf("0x%x 0x%x (%d)\n",buf0[i],buf1[i],buf1[i]-buf0[i]);
    }
    printf("soundbuf is %d bytes at 0x%x\n",
	2 * sizeof(int) * NSAMPS,soundbuf);
}

void
WaitForOneDMA(int which)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(which));
    volatile unsigned long * bp0ptr=
	(unsigned long*)PHYS_TO_K1(HPC3_PBUS_BP(0));
    volatile unsigned long * bp1ptr=
	(unsigned long*)PHYS_TO_K1(HPC3_PBUS_BP(1));
    pbus_control_read_t ctrl0val;
    int ntimes;
    int maxtimes=10000;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
	ctrl0val = *ctrl0ptr;
	if(!ctrl0val.ch_act)break;
	buf0[ntimes] = *bp0ptr;
	buf1[ntimes] = *bp1ptr;
	us_delay(10000);
    }
    if(ntimes==maxtimes) {
	printf("Dma %d did not complete after %d*.01 seconds\n",which,ntimes);
    } else {
	printf("Dma %d complete after %d*.01 seconds\n",which,ntimes);
    }
}
