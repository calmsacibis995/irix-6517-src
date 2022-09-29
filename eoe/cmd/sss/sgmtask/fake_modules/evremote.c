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
    int          class;
    int          type;
    char         data[30];
    time_t       curr_time = time(0);
    char         *args[3];

    if ( argc <= 3 ) {
	fprintf(stderr, "Usage: %s <hostname> <classcode> <typecode>\n", argv[0]);
	exit(1);
    }

    memset(data, 0, 30);
    args[0] = argv[1];
    args[1] = 0;
    class = atoi(argv[2]);
    type  = atoi(argv[3]);
    nobytes = snprintf(data, 30, "DeliverRemote:%d-%d", class, type);
    printf ("Running ssDeliverEventRemote with %d, %d, 1, %d, 1, %d, %s\n",
	    class, type, curr_time, nobytes, data);
    err = dsmpgExecEvent(class, type,  1, 1, (void *) data, 
			       nobytes+1, NULL, args);

    if ( err ) pSSSErr(err);

    fprintf(stderr, "Operation completed successfully\n");
    exit(0);


}
