#ifndef __EVENT_STRINGS_H__
#define __EVENT_STRINGS_H__
/*
 * evTypeStrings.h -- static strings equivalent to the const eventID 
 *                    and enums ins event.h
 * $Revision: 1.4 $
 * 
 * This file must be updated when new eventIDs, object types, rate bases or
 * alarm leveles are added in event.h 
 */
 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
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

 static char * eventTypeStrings[] = {
     "UNKNOWN", 
     "STARTUP", 
     "SHUTDOWN", 
     "START SNOOP", 
     "STOP SNOOP", 
     "EVENT 5", 
     "EVENT 6", 
     "EVENT 7", 
     "EVENT 8", 
     "EVENT 9", 
     "NEW NODE", 
     "NEW NET", 
     "CONVERSATION START", 
     "CONVERSATION STOP", 
     "NEW PROTOCOL", 
     "NEW TOPN NODES", 
     "EVENT 16", 
     "EVENT 17", 
     "EVENT 18", 
     "EVENT 19", 
     "EVENT 20", 
     "EVENT 21", 
     "EVENT 22", 
     "EVENT 23", 
     "EVENT 24", 
     "EVENT 25", 
     "EVENT 26", 
     "EVENT 27", 
     "EVENT 28", 
     "EVENT 29", 
     "RATE HI ALARM", 
     "RATE HI CLEARED", 
     "RATE LO ALARM", 
     "RATE LO CLEARED", 
     "EVENT 34", 
     "EVENT 35", 
     "EVENT 36", 
     "EVENT 37", 
     "EVENT 38", 
     "EVENT 39", 
     "EVENT 40", 
     "EVENT 41", 
     "EVENT 42", 
     "EVENT 43", 
     "EVENT 44", 
     "EVENT 45", 
     "EVENT 46", 
     "EVENT 47", 
     "EVENT 48", 
     "EVENT 49", 
     "APPLICATION REGISTER", 
     "EVENT 51", 
     "EVENT 52", 
     "EVENT 53", 
     "EVENT 54", 
     "EVENT 55", 
     "EVENT 56", 
     "EVENT 57", 
     "EVENT 58", 
     "EVENT 59", 
     "LOGGING BEGIN", 
     "LOGGING END", 
     "EVENT 62", 
     "EVENT 63", 
     "EVENT 64", 
     "EVENT 65", 
     "EVENT 66", 
     "EVENT 67", 
     "EVENT 68", 
     "EVENT 69", 
     "NETLOOK HILIGHT"
 };

static char *alarmStrings[] = {
    "INFO", 
    "MINOR", 
    "MAJOR", 
    "CRITICAL", 
    "CLEAR"
};

static char *objectStrings[] = {
    "UNKNOWN", 
    "NODE", 
    "NETWORK", 
    "CONVERSATION", 
    "PROTOCOL"
};

static char *rateStrings[] = {
    "bytes", 
    "packets", 
    "percent utilization.", 
    "percent of all bytes.", 
    "percent of all packets.", 
    "percent of", 
    "percent of"
};
#endif
