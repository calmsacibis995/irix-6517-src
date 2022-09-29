/*
 * mkdisc - Make a WORM disc using a Philips CDD521/10 WORM drive.
 */

#include <stdio.h>
#include <getopt.h>
#include "mkdisc.h"

char *progname;

void
usage(void)
{
    fprintf(stderr,
     "usage: %s [-v] [-DRWV] [-r <retries>] -d <device> -f <file>\n", progname);
    fprintf(stderr,
     "usage: %s [-v] [-DRWV] [-r <retries>] -d <device> -s <size>\n", progname);
}

main(int argc, char *argv[])
{
    char *dev = 0, *image = 0, cmd = ' ';
    int opt, verbose = 0, max_retries = 10, data_size = 0, debug = 0;

    progname = argv[0];
    while ((opt = getopt(argc, argv, "d:f:r:s:vRWVDTC")) != EOF) {
	switch (opt) {
	case 'd':
	    dev = optarg;
	    break;
	case 'f':
	    image = optarg;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'D':
	    verbose = 2;
	    break;
	case 'R':
	    cmd = 'r';
	    break;
	case 'W':
	    cmd = 'w';
	    break;
	case 'V':
	    cmd = 'v';
	    break;
	case 'T':
	    cmd = 't';
	    data_size++;
	    break;
	case 'C':
	    cmd = 'd';
	    data_size++;
	    break;
	case 'r':
	    max_retries = atoi(optarg);
	    break;
	case 's':
	    data_size = atoi(optarg);
	    break;
	default:
	    usage();
	    exit(1);
	}
    }
    if (!dev || (!image && !data_size)) usage();
    else exit(mkdisc(cmd, dev, image, max_retries, data_size, verbose));
    exit(-1);
}
