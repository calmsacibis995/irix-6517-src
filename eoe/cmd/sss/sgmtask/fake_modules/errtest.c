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
    __uint32_t  err = 0;
    __uint32_t b20 = SERVERERR;
    __uint32_t b16 = PARAMERR;
    __uint32_t b0 = INVALIDARGS;
    __uint32_t tmperr = 0;


    err = b20<<24;
    tmperr = tmperr|err;
    err = b16<<16;
    tmperr = tmperr|err;
    err = b0;
    tmperr = tmperr|err;

    err = ERR(b20, b16, b0);
    pSSSErr(err);

    exit(0);


}
