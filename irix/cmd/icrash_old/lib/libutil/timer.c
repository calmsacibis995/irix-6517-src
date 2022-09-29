#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "icrash.h"
#include "extern.h"

/* Global variables
 */
struct timeval StartTime, EndTime;

/*
 * Start_Timer()
 */
void
Start_Timer()
{
	gettimeofday(&StartTime, NULL);
}

/* 
 * Stop_Timer()
 */
void
Stop_Timer()
{
	gettimeofday(&EndTime, NULL);
}

/*
 * Print_Timer()
 */
void
Print_Timer(FILE *ofp)
{
	double seconds;
	int microseconds;
	struct timeval Elapsed;

	timerdiff(&EndTime, &StartTime, &Elapsed);
	microseconds = Elapsed.tv_usec + (Elapsed.tv_sec * 1000000);
	seconds = (double)microseconds / 1000000;
	fprintf(ofp, "Elapsed Time: %d.%06d seconds\n",
		Elapsed.tv_sec, Elapsed.tv_usec);
}
