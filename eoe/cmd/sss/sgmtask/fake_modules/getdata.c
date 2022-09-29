#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include <sys/types.h>

int main (int argc, char **argv)
{
    __uint32_t   err = 0;
    int          nobytes = 0;
    char         data[1024];
    sgmtaskrd_t  *rdata = NULL;

    if ( argc <= 2 ) {
	fprintf(stderr, "Usage: %s <hostname> <sqlstring>\n", argv[0]);
	exit(1);
    }

    memset(data, 0, 1024);
    nobytes = snprintf(data, 1024, "%s",argv[2]);
    printf ("Running getdata with %s\n", data);
    err = SGMIGetData(argv[1], data, &rdata);

    if ( err ) pSSSErr(err);

    fprintf(stdout, "%s", (char *) rdata->dataptr);
    fprintf(stderr, "Operation completed successfully\n");
    exit(0);


}
