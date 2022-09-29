#include "sa_procs.h"
#include "niblet.h"

/* Test array format is a list of process names (as defined in "sa_procs.h")
 * 	followed by a "-1" terminator.  Do not list more that PROCS_PRE_LIST
 *	processes (as defined in niblet.h).
 */

int test_array[][PROCS_PER_TEST + 1] = {
	{INVALID, -1},						/* 0 */
	{MPSLOCK, MPSLOCK, INVALID, -1},			/* 1 */
	{MPROVE, MPSLOCK, MPROVE, MPSLOCK, INVALID, -1},	/* 2 */
	{MPROVE, MPROVE, -1},					/* 3 */
	{BIGINTADD_4, BIGINTADD_4, BIGINTADD_4,
		BIGINTADD_4, INVALID, MPROVE, MPROVE,
		MPROVE, BIGHLOCK, BIGHLOCK,
		MPSLOCK, MPSLOCK, -1},				/* 4 */
	{BIGMEM, BIGMEM, BIGMEM, INVALID, INVALID,
		INVALID, -1},					/* 5 */
	{PRINTTEST, INVALID, PRINTTEST, -1},			/* 6 */
	{BIGINTADD_4, BIGINTADD_4, BIGINTADD_4, BIGINTADD_4,
		INVALID, BIGROVE, BIGROVE, BIGROVE, BIGHLOCK,
		BIGHLOCK, BIGMEM, BIGMEM, BIGMEM, INVALID, -1}	/* 7 */
};

int num_tests = sizeof(test_array) / (sizeof(int) * PROCS_PER_TEST + 1);
int avail_procs = NUM_PROCS;

