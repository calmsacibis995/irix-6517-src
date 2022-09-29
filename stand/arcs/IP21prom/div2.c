/*
 * div2.c
 *
 * This test program is originated from iain@mti div2.c, I modified
 * it for IP21prom environment. 
 * When FPU test failed in Slave processor, proper diag error code will
 * return to Master processor.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include "div2.h"
#include "prom_externs.h"
#include "ip21prom.h"
#include <sys/EVEREST/evdiag.h>

struct test_block {
    double      num[4];
    double      denom[4];
    double      res[4][4];      /* indexed by [numerator][denominator] */
    double      rem[4][4];      /* indexed by [numerator][denominator] */
    long long   destep;
    long long   des, pts;
    int         inexact_rem;
    int         bigrem;
    double      hulp;
    double      minrem[4];
    double      maxrem[4];
};


int fputest();
int dtest(double , double , double , double , long long);


int fputest() {
	long long p[14] = {3,5,7,11,13,17,19,23,29,31,37,41,43,47};
	int i;
	int fpu_error;
	int errcnt=0;
	
	loprintf("Checking FPU...                                ... ");
	for (i=0; i<14; i++) {
    		fpu_error = dtest(1.0, 1.0/p[i], 1.0, 1.0/p[i], p[i]);
    		if (fpu_error != 0)
			errcnt++;
	};

	if (errcnt == 0)
		loprintf("passed. \n");
	else
		loprintf("failed. \n");
}

int dtest(double nbase, double nstep, double dbase, double destep, long long p) {
    long	ns;
    struct test_block	test;
    double	min[4],max[4];
    int		n, d;
    double	margin, himargin, lomargin;
    double	maxround, minround;
    long long	denstart;
    int 	fpu_error = 0;

    maxround = minround = 0.;
    *(long long*)&test.hulp = 0x3ca0000000000000;
    test.destep = 0x6000000000000000/(long long)p;
    test.pts = p;
    for (ns = 0; ns < p; ns+=4) {
	test.num[0] = nbase +  ns   *nstep;
	test.num[1] = nbase + (ns+1)*nstep;
	test.num[2] = nbase + (ns+2)*nstep;
	test.num[3] = nbase + (ns+3)*nstep;
	denstart = 0x1000000000000000;
	test.denom[0] = *(double *)&denstart;
	denstart += test.destep;
	test.denom[1] = *(double *)&denstart;
	denstart += test.destep;
	test.denom[2] = *(double *)&denstart;
	denstart += test.destep;
	test.denom[3] = *(double *)&denstart;
	test.minrem[0]=test.minrem[1]=test.minrem[2]=test.minrem[3]= 0;
	test.maxrem[0]=test.maxrem[1]=test.maxrem[2]=test.maxrem[3]= 0;
	for (test.des = 0; test.des < p;) {
	    loop_block_div(&test);
            if (test.inexact_rem) {
                loprintf("FPU test error.  Inexact remainder. %d, %d\n", ns, test.des);
		fpu_error++;
		
            }
            if (test.bigrem) {
                loprintf("FPU test error.  Rounding too large, check point %d, %d\n",ns,test.des);
		fpu_error++;
            }
	}
	for (n=0; n<4; n++) {
	    min[n] = test.minrem[n]/(test.hulp*test.num[n]);
	    max[n] = test.maxrem[n]/(test.hulp*test.num[n]);
	    if (min[n] < minround) minround = min[n];
	    if (max[n] > maxround) maxround = max[n];
	}
	return(fpu_error);
    }

}	

