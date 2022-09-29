/*
 * mkdisc - Make a WORM disc using a Sony CDW-E1 encoding unit
 */

#include <stdio.h>
#include <getopt.h>
#include "mkdisc.h"

char *progname;

void
usage(void)
{
    fprintf(stderr,
     "usage: %s [-v] [-a] [-p] [-c <cuesheet> ] [-r <retries> ] [-d <device>] -f <file>\n", progname);
    fprintf(stderr,
     "usage: %s [-v] [-a] [-p] [-c <cuesheet> ] [-r <retries> ] [-d <device>] -s <size>\n", progname);
    fprintf(stderr, "-v = verbose, -p = pretend to write (CDW900E only)\n");
    fprintf(stderr, "device defaults to /dev/cdr\n");
    fprintf(stderr, "-a = audio (program doesnt attempt to pad data)\n");
}

err_exit(char *s)
{
fprintf(stderr, "Error: %s\n",s);
usage();
exit(1);
}


main(int argc, char *argv[])
{
    char *dev = "/dev/cdr", *image = 0;
    char *cuesheet_file=NULL;
    int opt, verbose = 0, max_retries = 10, data_size = 0, pretend_flag=0;
    int audio_flag=0;

    progname = argv[0];
    while ((opt = getopt(argc, argv, "avpd:f:r:s:c:")) != EOF) {
	switch (opt) {
	case 'd':
	    dev = optarg;
	    break;
	case 'f':
	    image = optarg;
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'r':
	    max_retries = atoi(optarg);
	    break;
	case 's':
	    data_size = atoi(optarg);
	    break;
	case 'c':
	    cuesheet_file = optarg;
	    break;
	case 'p':
	    pretend_flag=1;
	    break;
	case 'a':
	    audio_flag=1;
	    break;
	default:
	    usage();
	    exit(1);
	}
    }

    if (!dev || (!image && !data_size)) {
	usage();
	exit (1);
    }
    
    exit(mkdisc(dev, image, max_retries, data_size, verbose, pretend_flag, cuesheet_file, audio_flag));
}
