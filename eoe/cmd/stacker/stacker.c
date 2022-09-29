/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: stacker.c,v 1.3 1994/11/29 00:37:39 curtis Exp $"

/* 
 * stacker.c - stacker robotics control program.
 */

#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dslib.h>
#include <string.h>
#include <errno.h>
#include "control.h"

extern int debug;		/* control debuging printfs in this prog */
extern int dsdebug;		/* control debuging printfs in /dev/scsi lib */
void usage(char *name);

int
main(int argc, char *argv[])
{
	int config, unload, load, drive, to, from;
	int c, cmdcnt;
	char *devname;

	cmdcnt = debug = 0;
	config = unload = load = drive = to = from = 0;
	drive = 1;
	while ((c = getopt(argc, argv, "Dcd:l:u:t:f:")) != -1) {
		switch (c) {
		case 'D':
			debug++;
			break;
		case 'c':
			config = 1;
			if (cmdcnt)
				usage(argv[0]);
			cmdcnt++;
			break;
		case 'u':
			unload = atoi(optarg);
			if (cmdcnt && (drive == 0))
				usage(argv[0]);
			cmdcnt++;
			break;
		case 'l':
			load = atoi(optarg);
			if (cmdcnt && (drive == 0))
				usage(argv[0]);
			cmdcnt++;
			break;
		case 'd':
			drive = atoi(optarg);
			if (cmdcnt && (load == 0) && (unload == 0))
				usage(argv[0]);
			cmdcnt++;
			break;
		case 't':
			to = atoi(optarg);
			if (cmdcnt && (from == 0))
				usage(argv[0]);
			cmdcnt++;
			break;
		case 'f':
			from = atoi(optarg);
			if (cmdcnt && (to == 0))
				usage(argv[0]);
			cmdcnt++;
			break;
		default:
			usage(argv[0]);
		}
	}
	if ((to || from) && (load || unload))
		usage(argv[0]);
	if ((cmdcnt == 0) || (cmdcnt > 2))
		usage(argv[0]);
	if (optind == argc-1) {
		devname = argv[optind];
	} else if ((devname = getenv("STACKER_DEV")) == NULL) {
		fprintf(stderr, "No device specified\n");
		usage(argv[0]);
	}

	if (debug) {
		printf("device is %s\n", devname);
		if (debug > 1)
			dsdebug = 1;
	}

	if (setup(devname)) {
		exit(1);
	}

	devpositions();
	if (config) {
		printconfig(devname);
	} else if (load) {
		movemedia(load, TYPE_SLOT, drive, TYPE_DRIVE);
	} else if (unload) {
		movemedia(drive, TYPE_DRIVE, unload, TYPE_SLOT);
	} else if (to && from) {
		movemedia(from, TYPE_SLOT, to, TYPE_SLOT);
	} else {
		usage(argv[0]);
	}

	cleanup();

	return(0);
}

void
usage(char *progname)
{
	fprintf(stderr, "usage:\t%s [-D[D]...] -c [devname]\n", progname);
	fprintf(stderr,
	    "\t%s [-D[D]...] -l binnumber [-d drivenumber] [devname]\n",
	    progname);
	fprintf(stderr,
	    "\t%s [-D[D]...] -u binnumber [-d drivenumber] [devname]\n",
	    progname);
	fprintf(stderr,
	    "\t%s [-D[D]...] -f binnumber -t binnumber [devname]\n",
	    progname);
	fprintf(stderr, "Devname is optional if STACKER_DEV is set.\n");
	exit(1);
}
