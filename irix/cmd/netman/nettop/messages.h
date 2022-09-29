#ifndef __messages_h
#define __messages_h

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	error and warning messagess for NetTop
 *
 *	$Revision: 1.5 $
 *	$Date: 1992/10/13 23:54:12 $
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

#define ILLEGALINTFCMSG	     0
#define BADINTFCMSG	     1
#define NOSNOOPDMSG	     2
#define GETHOSTNAMEMSG	     3
#define SSREADERRMSG	     4
#define SSSUBSCRIBEERRMSG    5
#define SETSNOOPLENMSG	     6
#define NODELFILTERMSG       7
#define NOSTARTSNOOPMSG	     8
#define NOSTOPSNOOPMSG	     9
#define NOSETINTVLMSG	     10
#define SSUNSUBSCRIBEERRMSG  11
#define NOMULTICHOICE        12
#define NOINSTANCE           13
#define NOGLPARENT           14
#define SSREADBINSSG	     15

static char *Message[] = {
    "Data Station name not recognized... message in ::post",
    "Bad Interface\n",
    "Could not connect to snoopd... message in ::post",
    "Could not get name of node",
    "Error in snoop read",
    "Could not connect to snoopd... message in ::post",

    "Error in setting snoop length",
    "Could not delete snoop filter",
    "Could not start snooping",
    "Could not stop snooping",
    "Could not set snooping interval",

    "Error in snoop unsubscribe",
    "No type multichoice",
    "No instance name",
    "Internal Error - No GL parent",
    "Error in snoop read - wrong number of bins", 
};



#endif /* __messages_h_ */
