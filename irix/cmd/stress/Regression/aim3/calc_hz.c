/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) calc_hz.c:3.2 5/30/92 20:18:31" };
#endif

long ticks;
#ifdef SYS5
	long	t1,t2,ct1,ct2;
	struct tms tb1,tb2;
#else
#include <sys/types.h>
#include <sys/timeb.h>
	struct timeb tb1,tb2;
#endif

main()
{

#ifdef SYS5
	time(&t2);
	for (;;) {
		time(&t1);
		if (t1>t2) break;
	}

	ct1 = times(&tb1);
	for (;;) {
		time(&t2);
		if (t2>t1) break;
	}
	ct2 = times(&tb2);
	ticks = (ct2-ct1)/10;
#else
	ftime(&tb1);
	do {
		ftime(&tb2);
	} while (tb1.millitm == tb2.millitm);
	ticks = (1000/(tb2.millitm-tb1.millitm))/10;
#endif
	ticks = (ticks+5)/10 * 10;
	printf("The computed HZ value is %d\n",ticks);
	exit(ticks);
}	
/*
long tb[4],times(),t1,ticks;
main()
{
	t1 = times(tb);
	sleep(10);
	ticks = (times(tb) - t1) / 10;
	ticks = (ticks + 5)/10 * 10;
	exit(ticks);
}
*/
