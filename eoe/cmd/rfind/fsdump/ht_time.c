#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <types.h>

/*
 * Decide if we still have enough time left in our quantum
 * to continue computing fenv values.
 */

int havethetime() {
	extern time_t StartTime;	/* Time in secs. that fsdump began */
	extern int quantum;		/* number of minutes between each scheduled dump */
	extern time_t inodescantime;	/* how many seconds it took to scan inodes */
	extern bool_t isxfs;		/* Is it is xfs filesystem? */

	time_t Now;		/* Current time in seconds */
	time_t secsallowed;	/* how many secs after Start we can still do fenv */
	time_t dumptime;	/* est. number seconds needed to dump out file */

	if (quantum == 0)
		return 1;
	if (time(&Now) == (time_t) -1) {
		(void) fprintf(stderr, "fsdump: time() %s\n", sys_errlist[errno]);
		exit(2);
	}

	/*
	 * We need to leave time to dump the file back out,
	 * and still almost always complete before our quantum
	 * is up.  The best estimate I can find of the time to
	 * dump is the time we spent to read in inodes.
	 * The dump time is almost always between 10% and 200% of
	 * the inode scan time.  A wide range, but better than
	 * the range of absolute times - which have been between
	 * 4 and 500 seconds on one source machine this last week.
	 * Using a percentage of inode scan time should scale, too.
	 */
	if (isxfs) {
		dumptime = inodescantime / 3;
	} else
		dumptime = 2 * inodescantime;
	secsallowed = quantum * 60 - dumptime;
	return Now < StartTime + secsallowed;
}
