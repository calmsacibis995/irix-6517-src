#if TFP

/*
 * div2.c
 *
 * This test program is originated from iain@mti div2.c, I modified
 * it for IP21prom environment. 
 * When FPU test failed in Slave processor, proper diag error code will
 * return to Master processor.
 */


#include <sys/types.h>
#include <math.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h> 
#include <pon.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/diagval_strs.i>

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


void fputest(void);
void prom_flash_leds(void);
void loop_block_div(struct test_block *);
int dtest(double , double , double , double , long long);


void
fputest(void)
{
	long long p[14] = {3,5,7,11,13,17,19,23,29,31,37,41,43,47};
	int i, slot, slice;

	slot =  (load_double((long long *)EV_SPNUM)
				& EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (load_double((long long *)EV_SPNUM)
				& EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	for (i=0; i<14; i++) {
    		if (dtest(1.0, 1.0/p[i], 1.0, 1.0/p[i], p[i]) != 0) {
			EVCFGINFO->ecfg_board[slot].eb_cpuarr[slice].cpu_diagval = EVDIAG_FPU;
			prom_flash_leds();
		}
	}
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
            if (test.inexact_rem)
		fpu_error++;
            if (test.bigrem)
		fpu_error++;
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
#endif
