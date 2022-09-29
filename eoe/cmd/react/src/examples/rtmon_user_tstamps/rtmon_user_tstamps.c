#include <stdio.h>
#include <stdlib.h>
#include <sys/rtmon.h>
#include <sys/sysmp.h>


/* a sample program that alternates between processors 1, 2, and 3
   and logs a user timestamp every TIC */

main()
{
    int proc;
    int count;

    proc = 1;
    count = 0;

    /* start off running on processor 1 */
    sysmp(MP_MUSTRUN, proc);

    while (1) {
	/* pause for a CLK_TCK */
	sginap(1);
	/* log a user timestamp:
	     we will use processor as the identifier of the event we log,
	         as the man page indicates this will appear in the
		 event stream as 4000X  where X is 1, 2, and 3 in turn
	     for demonstration purposes we'll log two event qualifiers,
	         17 and 11 and indicate that by setting numb_quals to 
		 be 2 */
	rtmon_log_user_tstamp(proc, 2, 17, 11, 0);
	
	/* move to a different processor every 100 events we log */
	count++;
	if ((count % 100) == 0) {
	    proc++;
	    if (proc == 3) proc = 1;
	    sysmp(MP_MUSTRUN, proc);
	}
    }
}
