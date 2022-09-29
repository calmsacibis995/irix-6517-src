/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	netgraph XDR functions
 *
 *	$Revision: 1.2 $
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
#include <sys/time.h>
#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <stdio.h>
#include <errno.h>

#include "histogram.h"

#define NETGRAPH_MAGIC	    0xA4157021
#define NETGRAPH_HIST_VERSION    2

/* Opens a plain (non-xdr) file for writing.
 * Returns 1 if ok, else 0.
 */
bool_t
open_plain_outfile(char* fileName, FILE** fpp)
{
    *fpp = fopen(fileName, "a");
    if (*fpp == 0)
	return 0;

    return 1;
} /* open_plain_outfile */


/* Closes a plain (non-xdr) file.
 * Returns 1 if ok, else 0.
 */
void
close_plain_file(FILE* fp)
{
    fclose(fp);
} /* close_plain_file */


/* Opens file for recording netgraph history via xdr.
 * Returns 1 if ok, else 0.
 */
bool_t
open_xdr_outfile(char* filename, FILE** fpp, XDR* xdrp, int* highestNump,
    char** optionsp)
{

    int ok = 1;
    unsigned int magic, version;

    *fpp = fopen(filename, "w");
    if (*fpp == 0)
	return 0;
    if (flock(fileno(*fpp), LOCK_EX | LOCK_NB) < 0) {
	*fpp = 0;
	return 0;
    }

    xdrstdio_create(xdrp, *fpp, XDR_ENCODE);

    magic = NETGRAPH_MAGIC;
    version = NETGRAPH_HIST_VERSION;

    ok   = xdr_u_int(xdrp, &magic)
	&& xdr_u_int(xdrp, &version)
	&& xdr_int(xdrp, highestNump)
	&& xdr_string(xdrp, optionsp, 80);

    return ok;

} /* open_xdr_outfile */


/* Opens file for reading netgraph history via xdr.
 * Returns 0 if not ok,  else version num (1 or 2)
 */
int
open_xdr_infile(char* filename, FILE** fpp, XDR* xdrp,
    int* lengthP, int* highestNump, char** optionsp)
{
    int ok = 1;
    unsigned int magic, version;

    *fpp = fopen(filename, "r");
    if (*fpp == 0 || flock(fileno(*fpp), LOCK_SH) < 0)
	return 0;
    fseek(*fpp, 0, SEEK_END);
    *lengthP = ftell(*fpp);
    rewind(*fpp);

    xdrstdio_create(xdrp, *fpp, XDR_DECODE);

    ok  =  xdr_u_int(xdrp, &magic)
	&& ((magic == NETGRAPH_MAGIC)
	&& xdr_u_int(xdrp, &version)
	&& xdr_int(xdrp, highestNump)
	&& xdr_string(xdrp, optionsp, 80));

    if (ok) {
	return version;
    }
    else return 0;

} /* open_xdr_infile */



/* Write or read time value to/from history file.
 * Returns 1 if ok, else 0.
 */
bool_t
do_xdr_time(XDR* xdrp, struct timeval* timestampP)
{
    int ok;


    ok  = xdr_long(xdrp, &(timestampP->tv_sec));
    ok &= xdr_long(xdrp, &(timestampP->tv_usec));

    return ok;
}


/* Write or read one data element to/from history file.
 * Returns 1 if ok, else 0.
 */
bool_t
do_xdr_data(XDR* xdrp, float* valuep)
{
    int ok;

    ok = xdr_float(xdrp, valuep);

    return ok;
} /* do_xdr_data */


/* Write or read strip filter expression, graph type, baseRate,
 * color, avgColor, & graph style
 * to/from history file.
 * Returns 1 if ok, else 0.
 *  NOTE: version 2 has baseRate & avgColor; version 1 has neither.
 */
bool_t
do_xdr_stripInfo(XDR* xdrp, int version, char** stringp,
	int* typep, int* baseRatep, int* colorp, int* avgColorp,
	int* stylep,
	int* alarmSetp, int* alarmBellp,
	float* alarmLop, float* alarmHip)
{
    int retVal;
    
    if (version == 0)
	version = NETGRAPH_HIST_VERSION;
	
    if (version == 1) {
	retVal = (xdr_string(xdrp, stringp, 80)
	    &&  xdr_int(xdrp, typep)
	    &&  xdr_int(xdrp, colorp)
	    &&  xdr_int(xdrp, stylep)
	    &&  xdr_bool(xdrp, alarmSetp)
	    &&  xdr_bool(xdrp, alarmBellp)
	    &&  xdr_float(xdrp, alarmLop)
	    &&  xdr_float(xdrp, alarmHip));
	
    } else {
    
	retVal = (xdr_string(xdrp, stringp, 80)
	    &&  xdr_int(xdrp, typep)
	    &&  xdr_int(xdrp, baseRatep)
	    &&  xdr_int(xdrp, colorp)
	    &&  xdr_int(xdrp, avgColorp)
	    &&  xdr_int(xdrp, stylep)
	    &&  xdr_bool(xdrp, alarmSetp)
	    &&  xdr_bool(xdrp, alarmBellp)
	    &&  xdr_float(xdrp, alarmLop)
	    &&  xdr_float(xdrp, alarmHip));
    }
    
    return retVal;
    
} /* do_xdr_stripInfo */


void
close_xdr_file(FILE* fp, XDR* xdrp)
{
    xdr_destroy(xdrp);

    (void) flock(fileno(fp), LOCK_UN);
    fclose(fp);
    
} /* close_xdr_file */


void
print_xdr_pos(XDR* xdrp, char* label)
{
    printf(".... xdr position : %u (%s)\n", xdr_getpos(xdrp), label);
}

unsigned int
get_xdr_pos(XDR* xdrp)
{
    return(xdr_getpos(xdrp));
}


bool_t
test_xdr_read(char* fileName, FILE** fpp, XDR* xdrp)
{
    int ok, stripNum;
    int length;
    int highNum;
    int type, baseRate, color, avgColor, style;
    int alarmSet, alarmBell;
    float alarmLo, alarmHi;
    struct timeval timestamp;
    long clockSec;
    struct tm* timeStruct;
    char timeString[10];

    float data;
    unsigned int magic, version;
    char filterExpr[80];
    char* filterExprP;
    char optionStr[80];
    char* optionStrP;

    /*** these are needed for testing only ***/
    static char* test_graphTypes[] = { "packets", "bytes", "%packets", "%bytes",
                "%ether", "%fddi", "%tokenring", "%npackets", "%nbytes", 0 };

    static char* test_graphStyles[] = { "bar", "line", 0};

    filterExprP = filterExpr;
    optionStrP = optionStr;

    timestamp.tv_sec = 0.0;
    timestamp.tv_usec = 0.0;

    version = open_xdr_infile(fileName, fpp, xdrp, &length,
	    &highNum, &optionStrP);
    if (version == 0) {
	printf("open failed\n");
	return 0;
    } else {
	printf("opened %s, version %d netgraph history file\n",
	    fileName, version);
    }
    

    /* get filter expression, bin #, graph type, baseRate,
     * color, avgColor,  style for each strip */
    for (stripNum = 1; stripNum <= highNum; stripNum++) {
	if (!do_xdr_stripInfo(xdrp, version,
		&filterExprP, &type, &baseRate, 
		&color, &avgColor, &style,
		&alarmSet, &alarmBell, &alarmLo, &alarmHi))
	    return 0;
	printf("strip #%d: %s (%s) base %d, colors %d %d, <%s>\n", stripNum, filterExpr,
	    test_graphTypes[type], baseRate, color, avgColor, test_graphStyles[style]);
	printf("alarmSet: %d, alarmBell: %d, alarmLo: %f, alarmHi: %f\n",
	    alarmSet, alarmBell, alarmLo, alarmHi);
    }

    ok = do_xdr_time(xdrp, &timestamp);
    while (ok) {
	clockSec = timestamp.tv_sec;
	timeStruct = localtime(&clockSec);
	ascftime(timeString, "%H:%M:%S", timeStruct);
	printf("%s\n", timeString);
	for (stripNum = 1; stripNum <= highNum; stripNum++) {
	    if (!do_xdr_data(xdrp, &data)) 
		break;
	    printf("%i: \t%f\n", stripNum, data);
	}
	ok = do_xdr_time(xdrp, &timestamp);
    }

    return ok;
} /* test_xdr_read */


