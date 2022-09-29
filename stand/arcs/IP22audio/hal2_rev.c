#include "sys/sbd.h"
#include "sys/hal2.h"
main()
{
    hal2_rev();
}
hal2_rev()
{
    volatile unsigned  long *revptr=(unsigned long *)PHYS_TO_K1(HAL2_REV);
    unsigned short r, r0, r1, r2, r3;
    int i, uninstalled_count, installed_count, inconsistent_count;

    printf("HAL2 REV register test entered\n");
    hal2_configure_pbus_pio();

    /* Read the rev register one million times and be sure that each time is
       the same */

    r= *revptr;
    for(i=0;i<250000;i++) {
        r0= *revptr;	/* make some of the reads be back to back */
        r1= *revptr;
        r2= *revptr;
        r3= *revptr;
	if(r0 != r || r1 != r || r2 != r || r3 != r) {
	    printf("rev register readback failed:\n");
	    printf("r=0x%x, r0=0x%x r1=0x%x, r2=0x%x, r3=0x%x interation %d\n",
		   r,r0,r1,r2,r3,i);
	}
	if(i%25000 == 24999)printf("%d%% ",(i*100+150)/250000);
    }
    printf("\nHAL2 REV register test done, REV register contains 0x%x\n",r);
}
