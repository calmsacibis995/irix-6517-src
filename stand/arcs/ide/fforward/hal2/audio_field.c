#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/hal2.h"
#include "uif.h"
#include "pbus.h"

static void hal2_idrwalk_buildpattern(int , unsigned short *);
static int hal2_idrwalk_testpattern(unsigned short *, unsigned short *);
static void hal2_idrwalk_shiftpattern(unsigned short *);

static void
hal2_unreset(void)
{
    volatile __uint32_t *isrptr=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);

    /* Set up the ISR */

    *isrptr = HAL2_ISR_CODEC_RESET_N | HAL2_ISR_GLOBAL_RESET_N;
}

static int
hal2_isrstcd(void)
{
    volatile __uint32_t *isrptr=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    for (i = 0; i < 500; i++) {
	/*
	 * put the HAL2 in Indigo mode.
	 */
	*isrptr = *isrptr & ~(HAL2_ISR_CODEC_MODE);
	
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & (HAL2_ISR_CODEC_MODE | HAL2_ISR_TSTATUS)) {
	    nbad++;
	}
	if (isr1 & (HAL2_ISR_CODEC_MODE | HAL2_ISR_TSTATUS)) {
	    nbad++;
	}
	if (isr2 & (HAL2_ISR_CODEC_MODE | HAL2_ISR_TSTATUS)) {
	    nbad++;
	}
	if (isr3 & (HAL2_ISR_CODEC_MODE | HAL2_ISR_TSTATUS)) {
	    nbad++;
	}

	/*
	 * put the HAL2 in Quad mode.
	 */
	*isrptr = *isrptr | HAL2_ISR_CODEC_MODE;
		
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if ((isr0 & HAL2_ISR_CODEC_MODE) == 0) {
	    nbad++;
	}
	if ((isr1 & HAL2_ISR_CODEC_MODE) == 0) {
	    nbad++;
	}
	if ((isr2 & HAL2_ISR_CODEC_MODE) == 0) {
	    nbad++;
	}
	if ((isr3 & HAL2_ISR_CODEC_MODE) == 0) {
	    nbad++;
	}
    }
    return(nbad);
}

static int
hal2_isrcreset(void)
{
    volatile __uint32_t *isrptr=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    for (i = 0; i < 500; i++) {
	/*
	 * unreset the HAL2
	 */
	*isrptr = *isrptr & ~(HAL2_ISR_CODEC_RESET_N);
	
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & HAL2_ISR_CODEC_RESET_N) {
	    nbad++;
	}
	if (isr1 & HAL2_ISR_CODEC_RESET_N) {
	    nbad++;
	}
	if (isr2 & HAL2_ISR_CODEC_RESET_N) {
	    nbad++;
	}
	if (isr3 & HAL2_ISR_CODEC_RESET_N) {
	    nbad++;
	}

	/*
	 * put the HAL2 in codec/synth reset mode.
	 */
	*isrptr = *isrptr | HAL2_ISR_CODEC_RESET_N;
		
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if ((isr0 & HAL2_ISR_CODEC_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr1 & HAL2_ISR_CODEC_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr2 & HAL2_ISR_CODEC_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr3 & HAL2_ISR_CODEC_RESET_N) == 0) {
	    nbad++;
	}
    }
    return(nbad);
}

static int
hal2_isrgreset(void)
{
    volatile __uint32_t *isrptr=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    for (i = 0; i < 500; i++) {
	/*
	 * unreset the HAL2
	 */
	*isrptr = *isrptr & ~(HAL2_ISR_GLOBAL_RESET_N);
	
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr1 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr2 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}
	if (isr3 & HAL2_ISR_GLOBAL_RESET_N) {
	    nbad++;
	}

	/*
	 * put the HAL2 in global reset mode.
	 */
	*isrptr = *isrptr | HAL2_ISR_GLOBAL_RESET_N;
		
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if ((isr0 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr1 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr2 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
	if ((isr3 & HAL2_ISR_GLOBAL_RESET_N) == 0) {
	    nbad++;
	}
    }
    return(nbad);
}

static int
hal2_isrutime(void)
{
    volatile __uint32_t *isrptr=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    int i;
    unsigned short isr0, isr1, isr2, isr3;
    int nbad = 0;
    
    /*
     * read the ISR 2 thousand times; verify that it does
     * not change, and that all the reads are correct. With
     * the HAL quiescent, the busy bit should be 0 ("utime
     * unarmed").
     */
    for (i = 0; i < 500; i++) {
	/*
	 * we do four back-to-back reads to really whomp 
	 * on the HPC3/HAL2 PIO.
	 */
	isr0 = *isrptr;
	isr1 = *isrptr;
	isr2 = *isrptr;
	isr3 = *isrptr;
	if (isr0 & HAL2_ISR_USTATUS) {
	    nbad++;
	}
	if (isr1 & HAL2_ISR_USTATUS) {
	    nbad++;
	}
	if (isr2 & HAL2_ISR_USTATUS) {
	    nbad++;
	}
	if (isr3 & HAL2_ISR_USTATUS) {
	    nbad++;
	}
    }
    return(nbad);
}

static int
hal2_idrwalk(void)
{
    volatile __uint32_t *idr0ptr=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
    volatile __uint32_t *idr1ptr=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
    volatile __uint32_t *idr2ptr=(__uint32_t *)PHYS_TO_K1(HAL2_IDR2);
    volatile __uint32_t *idr3ptr=(__uint32_t *)PHYS_TO_K1(HAL2_IDR3);
    unsigned short pattern[4],readback[4];
    int s,i,nones;
    
    hal2_unreset();
    for(nones=1;nones<64;nones++) {
        hal2_idrwalk_buildpattern(nones,pattern);
        for(i=0;i<10240;i++) {

	    /* Write out a pattern */

	    *idr0ptr = pattern[0];
	    *idr1ptr = pattern[1];
	    *idr2ptr = pattern[2];
	    *idr3ptr = pattern[3];

	    /* Read it back */

            readback[0] = *idr0ptr;
            readback[1] = *idr1ptr;
            readback[2] = *idr2ptr;
            readback[3] = *idr3ptr;

	    /* Compare it */

	    s=hal2_idrwalk_testpattern(pattern,readback);
	    if(s!=0)return(1);

	    /* Shift pattern */
	    hal2_idrwalk_shiftpattern(pattern);
        }
    }
  return(0);
}

static
void
hal2_idrwalk_buildpattern(int nones, unsigned short *pattern)
{
    int i,word,bit;
    unsigned short bitpat[16]={
	0x1, 0x2, 0x4, 0x8,
	0x10, 0x20, 0x40, 0x80,
	0x100, 0x200, 0x400, 0x800,
	0x1000, 0x2000, 0x4000, 0x8000
    };
    pattern[0]=0;
    pattern[1]=0;
    pattern[2]=0;
    pattern[3]=0;
    for(i=0;i<nones;i++) {
	word=i>>4;
	bit=i&15;
	pattern[word] |= bitpat[bit];
    }
}

static int
hal2_idrwalk_testpattern(unsigned short *pattern, unsigned short *readback)
{
    int i;
    for(i=0;i<4;i++) {
	if(pattern[i] != readback[i]) {
	    return(1);
	}
    }
    return(0);
}

static void
hal2_idrwalk_shiftpattern(unsigned short *pattern)
{
    int msbA, msbB;
    
    /*
     * rotate the pattern left one bit.
     */
    msbB = (pattern[3] & 0x8000) >> 15;
    msbA = (pattern[0] & 0x8000) >> 15;
    pattern[0] = (pattern[0] << 1) | msbB;
    msbB = (pattern[1] & 0x8000) >> 15;
    pattern[1] = (pattern[1] << 1) | msbA;
    msbA = (pattern[2] & 0x8000) >> 15;
    pattern[2] = (pattern[2] << 1) | msbB;
    pattern[3] = (pattern[3] << 1) | msbA;
}

/* main function call for audio field diags */
/* returns zero on success, -1 on failure.  */
audiofield()
{
  msg_printf(3,"Audio board/Hal2 chip test entered...\n");
  if (hal2_rev()) {
    msg_printf(2,"Hal2 revision register test failed!\n");
    return(-1);
  }
  if (hal2_isrstcd() || 
      hal2_isrcreset() || 
      hal2_isrgreset() || 
      hal2_isrutime()) {
    msg_printf(2,"Hal2 status register test failed!\n");
    return(-1);
  }
  if (hal2_idrwalk()) {
    msg_printf(2,"Hal2 idr_walk test failed!\n");
    return(-1);
  }
  return(0);
}

