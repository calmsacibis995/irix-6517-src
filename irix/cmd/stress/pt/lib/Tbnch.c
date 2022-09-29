
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include <Tst.h>
#include <Bnch.h>

#include <time.h>

#if defined(mips)

#include <sys/syssgi.h>


static long long wrap_val;

#undef BnchDelta
void
BnchDelta(struct timespec *t, struct timespec *tv0, struct timespec *tv1)
{ 
	long long	tt0 = (long long)tv0->tv_sec * NSEC_PER_SEC
				+ tv0->tv_nsec;
	long long	tt1 = (long long)tv1->tv_sec * NSEC_PER_SEC
				+ tv1->tv_nsec;
	struct timespec	res;

	if (tt1 < tt0) {
		if (!wrap_val) {
			clock_getres(CLOCK_SGI_CYCLE, &res);
			if (syssgi(SGI_CYCLECNTR_SIZE) == 32) {
				wrap_val = 0x100000000uL * res.tv_nsec;
			} else {
				printf("64 bit clock wrap !!!\n");
			}
		}
		tt1 += wrap_val;
	}
	tt0 = tt1 - tt0;
	t->tv_sec = tt0 / NSEC_PER_SEC;
	t->tv_nsec = tt0 - (tt0 / NSEC_PER_SEC) * NSEC_PER_SEC;
}

#endif /* mips */
