/*
 * frush.c-
 *
 * This file is not part of the fru analysis library.  frush.c is used
 * to build a stand-alone (that it, not part of icrash) version of the
 * FRU analyzer.  Just "make frush".  It runs on decompressed crash dumps,
 * but it only needs the first few kiolbytes so you can
 *
 *	uncompvm -s 1 vmcore.N.comp
 *
 * and run frush on the resulting file.  To build a version fo frush that
 * works on Irix 5.2 or 5.3, be sure to set the environment variable
 * OS_VERSION appropriately and clobber the library.
 *
 */

#include <stdio.h>

#define _KERNEL  1
#include <sys/types.h>
#undef _KERNEL

#include <sys/EVEREST/everest.h>

#include <sys/EVEREST/evconfig.h>       /* Whole evconfig structure */
#include <sys/EVEREST/dang.h>           /* DANG chip */
#include <sys/EVEREST/fchip.h>          /* F chip */
#include <sys/EVEREST/io4.h>            /* IA chip */
#include <sys/EVEREST/mc3.h>            /* MA chip */
#include <sys/EVEREST/s1chip.h>         /* S1 chip */
#include <sys/EVEREST/vmecc.h>          /* VMECC chip */
#include <sys/EVEREST/gda.h>        	/* for everror_ext determination   */

#include "evfru.h"

extern void show_hardware_state(everror_t *, evcfginfo_t *);

int debug = 0;

main(int argc, char **argv)
{

    FILE *in;
    everror_t errbuf;
    evcfginfo_t ecbuf;
    everror_ext_t errbuf1;
    gda_t gdbuf;

    if ((argc < 2) || (argc > 3)) {
	fprintf(stderr, "Usage: frush [-d] filename\n");
	exit(1);
    }

    if (argc ==3)
	debug = 1;

    in = fopen(argv[argc - 1], "rb");

    if (!in) {
	fprintf(stderr, "Can't open %s\n", argv[argc - 1]);
	exit(1);
    }

    if (fseek(in, 0x2800, SEEK_SET)) {
	fprintf(stderr, "Can't seek to everror.\n");
	exit(1);
    }

    if (!fread(&errbuf, sizeof(everror_t), 1, in)) {
	fprintf(stderr, "Can't read everror.\n");
	exit(1);
    }

    if (fseek(in, 0x2000, SEEK_SET)) {
	fprintf(stderr, "Can't seek to evconfig.\n");
	exit(1);
    }

    if (!fread(&ecbuf, sizeof(evcfginfo_t), 1, in)) {
	fprintf(stderr, "Can't read evconfig.\n");
	exit(1);
    }
    if (fseek(in, 0x400, SEEK_SET)) {
	fprintf(stderr, "Can't seek to everror.\n");
	exit(1);
    }
    if (!fread(&gdbuf, sizeof(gda_t), 1, in)) {
	fprintf(stderr, "Can't read evconfig.\n");
	exit(1);
    }

    if (gdbuf.everror_vers >= 3) {
	  if (fseek(in, 0xc00, SEEK_SET)) {
		fprintf(stderr, "Can't seek to everror.\n");
		exit(1);
	  }
	  if (!fread(&errbuf1, sizeof(everror_ext_t), 1, in)) {
		fprintf(stderr, "Can't read evconfig.\n");
		exit(1);
	  }
    }

    show_hardware_state(&errbuf, &ecbuf);

    icrash_fruan(&errbuf, &errbuf1, &ecbuf, 0, stdout);

}

