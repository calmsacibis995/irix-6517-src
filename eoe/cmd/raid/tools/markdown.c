/*
 * Program to arbitrarily mark drives attached to a RAID controller up or down.
 */

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/usraid_admin.h>

usage(prog)
{
	printf("Usage: %s [-d drivenum] -[gb] drivename\n", prog);
	exit(2);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	struct usraid_units_down downs;
	int drivenum, markgood, markbad;
	int cmd, ch, fd, i;

	drivenum = markgood = markbad = 0;
	while ((ch = getopt(argc, argv, "d:gb")) != EOF) {
		switch (ch) {
		case 'd': drivenum = atoi(optarg);	break;
		case 'g': markgood = 1;			break;
		case 'b': markbad = 1;			break;
		default:  usage(argv[0]);		break;
		}
	}
	if ((!markgood && !markbad) || (markgood && markbad) ||
	    (drivenum < 0) || (drivenum > 4))
		usage(argv[0]);
	if (optind != (argc-1))
		usage(argv[0]);

	if ((getuid() != 0) || (geteuid() != 0)) {
		fprintf(stderr,"Only root can run this program\n");
		exit(-1);
	}

	if (*argv[optind] != '/') {
		if (chdir("/dev/rdsk")) {
			perror("couldn't cd to /dev/rdsk");
		}
	}
	if ((fd = open(argv[optind], 0)) == -1) {
		fprintf(stderr, "inquire: couldn't open %s\n", argv[optind]);
		perror(argv[optind]);
		exit(1);
	}

	/*
	 * Fill out the control parameters.
	 */
	bzero(&downs, sizeof(downs));
	downs.drive[drivenum] = 1;
	if (markgood == 1)
		cmd = USRAID_CLR_DOWN;
	else
		cmd = USRAID_SET_DOWN;

	/*
	 * Execute the operation.
	 */
	if (ioctl(fd, cmd, &downs) < 0) {
		perror("couldn't perform operation");
		exit(1);
	}

	exit(0);
}
