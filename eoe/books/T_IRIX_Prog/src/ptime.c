/*
|| Program to exercise POSIX clock_gettime() and clock_getres() functions.
||
||	ptime [-r -c -R -C -F]
||		-r	display CLOCK_REALTIME value
||		-R	display CLOCK_REALTIME resolution
||		-c	display CLOCK_SGI_CYCLE value
||		-C	display CLOCK_SGI_CYCLE resolution
||		-F	display CLOCK_SGI_FAST resolution (cannot get time from this)
|| Default is display everything (-rRcC).
*/
#include <time.h>
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <stdio.h>
void showtime(const timespec_t tm, const char *caption)
{
	printf("%s: sec %ld, ns %ld [%g sec]\n",
		caption, tm.tv_sec, tm.tv_nsec,
		((double)tm.tv_sec) + ((double)tm.tv_nsec / 1e9));
}
main(int argc, char **argv)
{
	int opta = 1;
	int optr = 0;
	int optR = 0;
	int optc = 0;
	int optC = 0;
	int optF = 0;
	timespec_t sample, res;
	int c;
	while ( -1 != (c = getopt(argc,argv,"arRcCF")) )
	{
		switch (c)
		{
		case 'a': opta=1; break;
		case 'r': optr=1; opta=0; break;
		case 'R': optR=1; opta=0; break;
		case 'c': optc=1; opta=0; break;
		case 'C': optC=1; opta=0; break;
		case 'F': optF=1; opta=0; break;
		default: return -1;
		}
	}
	if (opta || optr)
	{
		if (!clock_gettime(CLOCK_REALTIME,&sample))
			showtime(sample,"CLOCK_REALTIME value");
		else
			perror("clock_gettime(CLOCK_REALTIME)");
	}
	if (opta || optR)
	{
		if (!clock_getres(CLOCK_REALTIME,&res))
			showtime(res,"CLOCK_REALTIME units");
		else
			perror("clock_getres(CLOCK_REALTIME)");
	}
	if (opta || optc)
	{
		if (!clock_gettime(CLOCK_SGI_CYCLE,&sample))
			showtime(sample,"CLOCK_SGI_CYCLE value");
		else
			perror("clock_gettime(CLOCK_SGI_CYCLE)");
	}
	if (opta || optC)
	{
		if (!clock_getres(CLOCK_SGI_CYCLE,&res))
			showtime(res,"CLOCK_SGI_CYCLE units");
		else
			perror("clock_getres(CLOCK_SGI_CYCLE)");
	}
	if (opta || optF)
	{
		if (!clock_getres(CLOCK_SGI_FAST,&res))
			showtime(res,"CLOCK_SGI_FAST units");
		else
			perror("clock_getres(CLOCK_SGI_FAST)");
	}
}
