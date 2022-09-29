/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	do snoopstream stuff
 *
 *	$Revision: 1.2 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include "histogram.h"
#include "snoopstream.h"
#include "expr.h"
#include "protoid.h"

#include "constants.h"
#include "messages.h"

/**** NEW ONE
hist_init(SnoopStream* ssp, struct hostent* hp, char* interface, int interval)
*****/

int
hist_init(SnoopStream* ssp, h_info* hip, int interval)
{
    struct sockaddr_in srv_addr;
    struct timeval tv;
    char* interface;

/*    printf("hist_init hip->node:(%s), hip->interface:(%s)\n", 
	hip?hip->node:"<hip is null>", hip->interface);
*/  

    tv.tv_sec  = 10;
    tv.tv_usec =  0;
    if (!ss_open(ssp, hip->node, NULL, &tv))
	return(-NOSNOOPDMSG);

    if (hip->interface[0])
	interface = hip->interface;
    else
	interface = NULL;
	
    if (!ss_subscribe(ssp, SS_HISTOGRAM, hip->interface, 0, 0, interval))
	return(-SSSUBSCRIBEERRMSG);

    if (!ss_setsnooplen(ssp, 128))
	return(-SETSNOOPLENMSG);

/*    printf("hist_init is returning with hp->h_name:(%s)\n", hp->h_name);
*/
    return 0;
}

int
hist_read(SnoopStream *ssp, struct histogram* histp)
{
    return(ss_read(ssp, (xdrproc_t) xdr_histogram, histp));

}
