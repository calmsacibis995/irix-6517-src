/* CLOCK JITTER

   The clock will artificially jitter within the range:
   [(1 - epsilon) * work, (1 + epsilon) * work]

   Each processor receives a random shuffle of jitter samples, taken
   uniformly from the jitter range, s.t. the average workload will be
   the actual requested parallel workload.
*/

/* CLOCK_JITTER_NTICKS - number of clock samples to for jitter */
/* CLOCK_JITTER_MASK - mask for clock jitter bits, s.t.
	    n % CLOCK_JITTER_NTICKS == n & CLOCK_JITTER_MASK */
#define CLOCK_JITTER_MASK	0x1f
#define CLOCK_JITTER_NTICKS	(CLOCK_JITTER_MASK + 1)

/* CLOCK_JITTER_EPSILON - maximum jitter of the clock (1 +/- epsilon) */
#define CLOCK_JITTER_EPSILON 0.25

/* TIMING

   The workload timer can be one of several different flavors.
*/

/* USE_NOP_COUNTER - use a calibrated instruction stream (e.g. nops) */
#define USE_NOP_COUNTER
/* #define CALIBRATE_WITH_TIMES */
#define CALIBRATE_WITH_GETTIMEOFDAY

/* USE_MAPPED_COUNTER - use the SGI cycle counter (somewhat expensive) */
/* #define USE_MAPPED_COUNTER */

/* USE_COP0_COUNTER - use the cycle counter on coprocessor 0
   CAN'T BE DONE FROM USER MODE */
/* #define USE_COP0_COUNTER */
