#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include <sys/types.h>

int main (int argc, char **argv)
{
    sgmtask_t    task = {0, 0, 0};
    __uint32_t   err = 0;

    if ( argc <= 2 ) {
	fprintf(stderr, "Usage: %s <hostname> <data string>\n", argv[0]);
	exit(1);
    }

    task.datalen = strlen(argv[2]);
    task.dataptr = strdup(argv[2]);

    err = SGMIrpcclient(argv[1], 2, &task);

    if ( err ) {
	/*syslog(LOG_INFO, "Error returned by sgmrpcclient is %ld\n", err);*/
	fprintf(stderr, "Error returned by sgmrpcclient is %ld\n", err);
	exit(1);
    }

    fprintf(stderr, "Operation completed successfully\n");
    exit(0);


}
