#include "sys/sbd.h"
#include "sys/hal2.h"
main()
{
    hal2_configure_pbus_pio();
    hal2_idrwalk();
}
hal2_idrwalk()
{
    volatile unsigned  long *idr0ptr=(unsigned long *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned  long *idr1ptr=(unsigned long *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned  long *idr2ptr=(unsigned long *)PHYS_TO_K1(HAL2_IDR2);
    volatile unsigned  long *idr3ptr=(unsigned long *)PHYS_TO_K1(HAL2_IDR3);
    volatile unsigned  long *isrptr=(unsigned long *)PHYS_TO_K1(HAL2_ISR);
    unsigned short pattern[4],readback[4];
    int s,i,nones;
    printf("HAL2 IDR walking 1's and zeros test entered\n");
    
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
	    if(s!=0)goto done;

	    /* Shift pattern */
	    hal2_idrwalk_shiftpattern(pattern);
        }
    }
done:
    printf("HAL2 IDR walking 1's and zeros test done\n");
}

hal2_idrwalk_buildpattern(nones,pattern)
int nones;
unsigned short *pattern;
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

hal2_idrwalk_testpattern(pattern,readback)
unsigned short *pattern, *readback;
{
    int i;
    for(i=0;i<4;i++) {
	if(pattern[i] != readback[i]) {
	    printf("    readback failed\n");
	    printf("    expected idr0=0x%x, idr1=0x%x, idr2=0x%x, idr3=0x%x\n",
		   pattern[0],pattern[1],pattern[2],pattern[3]);
	    printf("    read     idr0=0x%x, idr1=0x%x, idr2=0x%x, idr3=0x%x\n",
		   readback[0],readback[1],readback[2],readback[3]);
	    return(1);
	}
    }
    return(0);
}

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
