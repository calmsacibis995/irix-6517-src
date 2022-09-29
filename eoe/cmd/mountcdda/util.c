/*
 *	util.c
 *
 *	Description:
 *		Utilities for mountcd program
 *
 *	History:
 *      rogerc      01/17/91    Created
 */

#include <stdlib.h>
#include <syslog.h>
#include "util.h"

void *
safe_malloc(unsigned size)
{
    void *p;

    p = malloc(size);
    if (p == NULL) {
        syslog(LOG_ERR, "Out of memory");
        exit(1);
    }
    return p;
}
