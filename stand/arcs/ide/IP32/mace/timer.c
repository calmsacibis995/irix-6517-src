#ident	"IP22/time/timer.c:  $Revision: 1.1 $"

#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"
#include "sys/i8254clock.h"
#include <fault.h>
#include "setjmp.h"
#include "uif.h"

#define	TIMER_0			10		/* timer 0 counter */
#define	TIMER_1			20		/* timer 1 counter */
#define	TIMER_2			1000		/* timer 2 counter */
#define	TIMER_0_FREQUENCY	(TIMER_2_FREQUENCY / TIMER_0)	/* 100 Hz */
#define	TIMER_1_FREQUENCY	(TIMER_2_FREQUENCY / TIMER_1)	/* 50 Hz */
#define	TIMER_2_FREQUENCY	(MASTER_FREQ / TIMER_2)	/* 1000 Hz */

#define SAMPLE_TIME		3	/* seconds */

#define	EXPECTED_INTERRUPT_0	(SAMPLE_TIME*TIMER_0_FREQUENCY)
#define	EXPECTED_INTERRUPT_1	(SAMPLE_TIME*TIMER_1_FREQUENCY)

#define CAUSE_TIMER0		CAUSE_IP5	/* external level 2 */
#define CAUSE_TIMER1		CAUSE_IP6	/* external level 3 */

/* 
 * verify the timer frequency based on the TODC
 */
bool_t timer ()
{
    register unsigned int c0_cause;
    jmp_buf faultbuf;
    int errcount = 0;
    register volatile struct pt_clock *pt = (struct pt_clock *)RT_CLOCK_ADDR;
#ifndef IP32
    register volatile char *timerack = (char *)TIMER_ACK_ADDR;
#endif /* IP32 */
    register int timer_0_interrupt = 0;
    register int timer_1_interrupt = 0;
    long save_SR;

    msg_printf (VRB, "Timer test\n");

    if (is_ioc1 ()) {
	/* printf ("Clock broken; test disabled.\n"); */
	return (errcount);
    }


    /* disable timer interrupts keeping power interrupt enabled */
    set_SR ((save_SR=get_SR()) & ~SR_IMASK | SR_IBIT4);

    /* this should start the timer */
    pt->pt_control = PTCW_SC(2) | PTCW_16B | PTCW_MODE(MODE_RG);
    pt->pt_counter2 = TIMER_2;
    pt->pt_counter2 = TIMER_2 >> 8;

    pt->pt_control = PTCW_SC(0) | PTCW_16B | PTCW_MODE(MODE_RG);
    pt->pt_counter0 = TIMER_0;
    pt->pt_counter0 = TIMER_0 >> 8;

    pt->pt_control = PTCW_SC(1) | PTCW_16B | PTCW_MODE(MODE_RG);
    pt->pt_counter1 = TIMER_1;
    pt->pt_counter1 = TIMER_1 >> 8;

    if (setjmp (faultbuf)) {
	++errcount;
	msg_printf (ERR, "Phantom interrupt during timer test\n");
	show_fault ();
    } 
    else {
	register unsigned int timval, finval;
	register unsigned int curval;

	nofault = faultbuf;

	timval = get_tod();
	while (timval == get_tod())
		;
	finval = (timval+1) + SAMPLE_TIME;

	/* clear any pending timer 0/1 interrupts */
#ifndef IP32
	*timerack = ACK_TIMER0 | ACK_TIMER1;
#endif /* IP32 */

	/* poll the cause register to see how many times the 
	 * timer status bits get set 
	 */
	while ((curval = get_tod()) < finval) { /*
				       * get_tod() will flush wb
				       * which is necessary to make timerack
				       * work
				       */
	    c0_cause = get_CAUSE ();
	    if (c0_cause & CAUSE_TIMER0) {
#ifndef IP32
		*timerack = ACK_TIMER0;
#endif /* IP32 */
		timer_0_interrupt++;
	    }   /* if */
	    if (c0_cause & CAUSE_TIMER1) {
#ifndef IP32
		*timerack = ACK_TIMER1;
#endif /* IP32 */
		timer_1_interrupt++;
	    }   /* if */

	    if (curval != timval) {
		busy(1);
		timval = curval;
	    }
	}   /* while */

	busy(0);
	msg_printf (DBG,
	    "Timer 0 interrupts, Expected: %d, Actual: %d\n",
	    EXPECTED_INTERRUPT_0, timer_0_interrupt);
	msg_printf (DBG,
	    "Timer 1 interrupts, Expected: %d, Actual: %d\n",
	    EXPECTED_INTERRUPT_1, timer_1_interrupt);

	if ((timer_0_interrupt > (EXPECTED_INTERRUPT_0 + 1)) || 
	    (timer_0_interrupt < (EXPECTED_INTERRUPT_0 - 1)) || 
	    (timer_1_interrupt > (EXPECTED_INTERRUPT_1 + 1)) || 
	    (timer_1_interrupt < (EXPECTED_INTERRUPT_1 - 1))) {
	    ++errcount;
	    msg_printf (ERR,
	    "INT2 Interval timers are not consistent with the DP8572 clock.\n");
	}   /* if */
    }   /* if */

    nofault = 0;

    if (errcount == 0)
	okydoky();

    set_SR (save_SR);

    return (errcount);
}   /* timer */
