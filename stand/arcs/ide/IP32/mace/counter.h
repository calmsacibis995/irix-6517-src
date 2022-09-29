#ident	"IP22diags/time/counter.h:  $Revision: 1.1 $"

/* 
 * timer.h - defines for the HPC's read-only counter used here to 
 * 	time the RTC and timer functions
 */

/* counter is 24 bits @ 30 MHz
 */
#define COUNTER_ADDR	0x1fb80194	/* word wide, read only */
#define COUNTER_FREQ	30		/* 30 cycles count 1 microsecond */

#define US		(COUNTER_FREQ)  /* 1 us */
#define MS		(1000*US)	/* 1 ms */
#define TENMS		(10*MS)		/* 10 ms */
#define SEC		(1000*MS)	/* 1 sec */

#define	SAMPLE_NUM	1		/* 1/2 seconds */
#define	SAMPLE_DENOM	2
#define	SAMPLING_PERIOD	(SAMPLE_NUM*SEC/SAMPLE_DENOM)	/* 1/2 sec sampling */
#define COUNTER_SLOP	(10*US)		/* 10 us of slop for wrapping */

