
#include "sys/types.h"
#include "sys/time.h"
#include "stdio.h"
#include "bstring.h"
#include <unistd.h>

#include "fam.h"
/* 

FILE famtest.c - simple fam test program
                 monitors the directory passed in for the first argument

		 ex.  famtest /usr/people/guest

*/

void processDirEvents(FAMEvent* fe);

void
main(int argc, char **argv)
{
    /* Create fam test */
    FAMConnection fc;
    FAMRequest fr;
    FAMEvent fe;
    fd_set readfds;
    char *testData = "TESTING: ";

    FAMOpen(&fc);
    
    if (argc < 2) {
	printf("usage: famtest dirname\n");
	exit(1);
    }
    if (FAMMonitorDirectory(&fc, argv[1], &fr, testData) < 0)
    {	fprintf(stderr, "FAMMonitorDirectory error\n");
	exit(1);
    }

    FD_ZERO(&readfds);
    while (1) {
        FD_SET(fc.fd, &readfds);
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
            break;
        for (; FAMPending(&fc); ) {
	    FAMNextEvent(&fc, &fe);
	    processDirEvents(&fe);
	}
    }
/*
    FD_ZERO(&readfds);
    while (1) {
        FD_SET(fc.fd, &readfds);
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
            break;
        for (; FAMNextEvent(&fc, &fe); ) processDirEvents(&fe);
    }
*/
}

void
processDirEvents(FAMEvent* fe)
{
    if (fe->userdata) printf("%s  ", fe->userdata);
    switch (fe->code) {
	case FAMExists:
	    printf("%s Exists\n", fe->filename);
	    break;
	case FAMEndExist:
	    printf("%s EndExist\n", fe->filename);
	    break;
	case FAMChanged:
	    printf("%s Changed\n", fe->filename);
	    break;
	case FAMDeleted:
	    printf("%s was deleted\n", fe->filename);
	    break;
	case FAMStartExecuting:
	    printf("%s started executing\n", fe->filename);
	    break;
	case FAMStopExecuting:
	    printf("%s stopped executing\n", fe->filename);
	    break;
	case FAMCreated:
	    printf("%s was created\n", fe->filename);
	    break;
	case FAMMoved:
	    printf("%s was moved\n", fe->filename);
	    break;
    }
}

