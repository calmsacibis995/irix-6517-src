/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	snoooping stuff for NetTop
 *
 *	$Revision: 1.2 $
 *	$Date: 1992/09/15 01:39:07 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>

#include "addrlist.h"
#include "histogram.h"
#include "snoopstream.h"
#include "expr.h"
#include "protoid.h"

#include "constants.h"
#include "messages.h"


/* initialize histogram service */
int
hist_init(SnoopStream* ssp, h_info* hip,
	int rows, int cols, int interval) {

    struct timeval tv;
    char* interface;


    tv.tv_sec  = 10;
    tv.tv_usec =  0;
    if (!ss_open(ssp, hip->node, NULL, &tv))
	return(-NOSNOOPDMSG);

    if (hip->interface[0])
	interface = hip->interface;
    else
	interface = NULL;
	
    if (!ss_subscribe(ssp, SS_HISTOGRAM, hip->interface, rows, cols, interval))
	return(-SSSUBSCRIBEERRMSG);

    if (!ss_setsnooplen(ssp, 128))
	return(-SETSNOOPLENMSG);

    return 0;
}


/* read histogram data */
int
hist_read(SnoopStream *ssp, struct histogram* histp) {
    return(ss_read(ssp, (xdrproc_t) xdr_histogram, histp));

}



/* initialize addrlist service */
int
al_init(SnoopStream* ssp, h_info* hip,
	int entries, int interval) {

    struct timeval tv;

    tv.tv_sec  = 10;
    tv.tv_usec =  0;
    if (!ss_open(ssp, hip->node, NULL, &tv))
	return(-NOSNOOPDMSG);

    if (!ss_subscribe(ssp, SS_ADDRLIST, hip->interface, entries, 0, interval))
	return(-SSSUBSCRIBEERRMSG);

    if (!ss_setsnooplen(ssp, 128))
	return(-SETSNOOPLENMSG);

    return 0;
}


/* read addrlist data */
int
al_read(SnoopStream *ssp, struct alpacket* alp) {
    return(ss_read(ssp, (xdrproc_t) xdr_alpacket, alp));

}
