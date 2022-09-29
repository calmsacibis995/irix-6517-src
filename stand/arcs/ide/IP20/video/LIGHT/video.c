/*
 *	video.c -- Starter video diagnostics main routine
 */
#include <sys/types.h>
#include <ide_msg.h>
#include <libsc.h>
#include <uif.h>
#include "sv.h"

static char mbuf[120];

int lg1_video(int argc, char *argv[])
{
    int errcount, numloops;
    int i, fifotest, initialize, clutest;

    errcount = 0;
    numloops = 1;
    fifotest = 1;
    initialize = 1;
    clutest = 1;
    while (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0') {
	switch (argv[1][1]) {
	    case 'c':		/* clut test only */
		clutest = 1;
		fifotest = 0;
		initialize = 0;
		break;
	    case 'f':		/* fifo test only */
		clutest = 0;
		fifotest = 1;
		initialize = 0;
		break;
	    case 'i':		/* initialize only */
		clutest = 0;
		fifotest = 0;
		initialize = 1;
		break;
	    case 'l':		/* loop count */
		numloops = atoi(&argv[1][2]);
		break;
	    default:
		errcount++;
		break;
	}
	argc--;
	argv++;
    }
    if (argc > 1 || errcount > 0) {
	sprintf(mbuf, "Usage: lg1_video -c -f -i -lnn \n");
	msg_printf(ERR,mbuf); 
	goto end_prog;
    }
    for (i = 0; i < numloops; i++) {
	if (initialize)
	    if (initsv() < 0)
		goto end_prog;
	if (fifotest)
	    if (vfifo1test() < 0)
		goto end_prog;
	if (clutest)
	    if (voclut_test() < 0)
		goto end_prog;
    }
    return 1;
end_prog:
    return -1;
}
