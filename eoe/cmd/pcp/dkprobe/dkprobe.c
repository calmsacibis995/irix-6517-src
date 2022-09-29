/*
 * dkprobe - probe all configured disks
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: dkprobe.c,v 2.7 1997/10/07 02:33:14 markgw Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <invent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "pmapi.h"
#include "impl.h"

#define DISKDIR	"/dev/rdsk"

int
main(int argc, char **argv)
{
    int		c;
    char	*p;
    int		errflag = 0;
    int		verbose = 0;
    inventory_t	*hinv;
    int		fd;
    int		unit;
    int		lun;
    int		remove;
    char	name[16];
    extern char	*optarg;
    extern int	optind;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "V?")) != EOF) {
	switch (c) {

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options\n\
  -V              verbose/diagnostic output\n\n",
		pmProgname);
	exit(1);
    }

    if (chdir(DISKDIR) < 0) {
	fprintf(stderr, "dkprobe: chdir(%s): %s\n", DISKDIR, strerror(errno));
	fprintf(stderr, "dkprobe: no disks found, ignore if system is diskless\n");
	exit(1);
    }

    while ((hinv = getinvent()) != NULL) {
	if (verbose > 1) printf("class: %d type: %d ctlr: %d unit: %d state: %d\n",
		hinv->inv_class, hinv->inv_type, hinv->inv_controller,
		hinv->inv_unit, hinv->inv_state);
	name[0] = '\0';
	remove = 0;

	if (hinv->inv_class == INV_DISK) {
	    switch (hinv->inv_type) {
		case INV_SCSIDRIVE:
		    unit = (int)hinv->inv_unit;
		    lun = hinv->inv_state;
		    if (lun == 0)
			sprintf(name, "dks%dd%dvh", hinv->inv_controller, unit);
		    else
			sprintf(name, "dks%dd%dl%dvh", hinv->inv_controller, unit, lun);
		    break;

		case INV_VSCSIDRIVE:
		    sprintf(name, "jag%dd%dvh", hinv->inv_controller, hinv->inv_unit);
		    break;

		case INV_SCSIRAID:
		    sprintf(name, "rad%dd%dvh", hinv->inv_controller, hinv->inv_unit);
		    break;
	    }
	}
	else if (hinv->inv_class == INV_SCSI) {
	    if (hinv->inv_type == INV_SCSIFLOPPY) {
		/* CDROM in the most common case */
		sprintf(name, "dks%dd%dvh", hinv->inv_controller, hinv->inv_unit);
		remove = 1;
	    }
	}

	if (name[0]) {
	    if (verbose) printf("%s", name);
	    if ((fd = open(name, O_RDONLY)) < 0) {
		if (errno == EIO && remove)
			/* removeable devices, this is really not ready */
			;
		else
		    if (verbose) printf(" %s", strerror(errno));
	    }
	    else {
		close(fd);
	    }
	    if (verbose) putchar('\n');
	}
    }

    exit(0);
    /*NOTREACHED*/
}
