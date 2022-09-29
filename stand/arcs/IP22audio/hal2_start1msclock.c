#include "sys/sbd.h"
#include "sys/i8254clock.h"
#include "sys/IP22.h"
#include "sys/param.h"

/*
   Counter 1 is clocked at 1Mhz.  It's output clocks both counter 1 and
   counter 2.
*/

#define COUNTER2_INPUT_FREQ	1000000				    /*hz*/
#define COUNTER2_OUTPUT_FREQ	5000				    /*hz*/
#define COUNTER2_COUNTS	(COUNTER2_INPUT_FREQ/COUNTER2_OUTPUT_FREQ)  /*counts*/

#define COUNTER1_INPUT_FREQ	COUNTER2_OUTPUT_FREQ 		    /*hz*/
#define COUNTER1_OUTPUT_FREQ	1000 				    /*hz*/
#define COUNTER1_COUNTS	(COUNTER1_INPUT_FREQ/COUNTER1_OUTPUT_FREQ) /*counts*/

#define COUNTER0_INPUT_FREQ	COUNTER2_OUTPUT_FREQ 		    /*hz*/
#define COUNTER0_OUTPUT_FREQ	100 				    /*hz*/
#define COUNTER0_COUNTS	(COUNTER0_INPUT_FREQ/COUNTER0_OUTPUT_FREQ)  /*counts*/

main()
{
    int sr;
    sr=getsr();
    sr |= SR_IBIT6;
    setsr(sr);
    hal2_start1msclock();
}
hal2_start1msclock()
{
    char line[100];
    volatile struct pt_clock *pt = (struct pt_clock *)PT_CLOCK_ADDR;
    int cause,cause2;
    printf("\nStarting the 1ms clock on INT2\n");

printf("counter 2 %d counter 1 %d counter 0 %d\n", COUNTER2_COUNTS, COUNTER1_COUNTS, COUNTER0_COUNTS);
    /* Counter 0 is the scheduling counter. */

    pt->pt_control = PTCW_SC(0)|PTCW_16B|PTCW_MODE(MODE_RG);
    pt->pt_counter0 = COUNTER0_COUNTS;
    pt->pt_counter0 = COUNTER0_COUNTS>>8;

    /* Counter 1 is the scheduling counter. */

    pt->pt_control = PTCW_SC(1)|PTCW_16B|PTCW_MODE(MODE_RG);
    pt->pt_counter1 = COUNTER1_COUNTS;
    pt->pt_counter1 = COUNTER1_COUNTS>>8;

    /* Counter 2 is the master counter.  Set it to 1Mhz */

    pt->pt_control = PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_RG);
    pt->pt_counter2 = COUNTER2_COUNTS;
    pt->pt_counter2 = COUNTER2_COUNTS>>8;

#ifdef LATER
    while(1) {
            *(volatile char *)TIMER_ACK_ADDR = ACK_TIMER1;
	cause=getcause();
	if(cause&0x2000) {
            *(volatile char *)TIMER_ACK_ADDR = ACK_TIMER1;
	    cause2=getcause();
	}
    }
#endif
    printf("\n1ms clock on INT2 started\n");
}
