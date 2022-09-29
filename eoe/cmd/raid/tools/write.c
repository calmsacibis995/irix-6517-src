/*
 * Program to write a sector on a drive attached to a RAID controller.
 */

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/usraid_admin.h>

/*
 * Useful twiddling stuff for SCSI commands and return values.
 */
#define B(v,s) ((uchar_t)((v) >> s))
#define USR_MSB_SPLIT1(v,a)                                                (a)[0]=B(v,0)
#define USR_MSB_SPLIT2(v,a)                                 (a)[0]=B(v,8), (a)[1]=B(v,0)
#define USR_MSB_SPLIT3(v,a)                 (a)[0]=B(v,16), (a)[1]=B(v,8), (a)[2]=B(v,0)
#define USR_MSB_SPLIT4(v,a) (a)[0]=B(v,24), (a)[1]=B(v,16), (a)[2]=B(v,8), (a)[3]=B(v,0)


/*
 * Read Command - opcode 0x28
 * Write Command - opcode 0x2a
 * Read/write the specified number of blocks from/to the specified LBA.
 */
struct rw_cmd {
	u_char	opcode;
	u_char	rsvd1;
	u_char	lba[4];
	u_char	rsvd2;
	u_char	length[2];
	u_char	rsvd3;
};

usage(prog)
{
	printf("Usage: %s -d drivenum -b blocknum -n numblocks -[rw] drivename\n", prog);
	exit(2);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	struct usraid_passthru pt_args;
	int drivenum, blocknum, numblocks;
	int getflag, setflag, ch, fd, i;
	struct rw_cmd cmd;
	int xferlength;
	char *buffer;

	drivenum = blocknum = numblocks = getflag = setflag = 0;
	while ((ch = getopt(argc, argv, "d:b:n:rw")) != EOF) {
		switch (ch) {
		case 'd': drivenum = atoi(optarg);	break;
		case 'b': blocknum = atoi(optarg);	break;
		case 'n': numblocks = atoi(optarg);	break;
		case 'r': getflag = 1;			break;
		case 'w': setflag = 1;			break;
		default:  usage(argv[0]);		break;
		}
	}
	if ((!setflag && !getflag) || (setflag && getflag) ||
	    (drivenum < 0) || (drivenum > 4) || (blocknum < 0) ||
	    (numblocks < 0) || (numblocks >= 128))
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

	xferlength = numblocks * 512;
	buffer = (char *)malloc( (unsigned)xferlength );

	if (getflag == 0) {
		/*
		 * Collect the new sector contents from stdin.
		 */
		for (i = 0; i < xferlength; i++) {
			if (scanf("%x", &ch) != 1) {
				fprintf(stderr, "error: couldn't scanf byte number %d\n", i);
				exit(1);
			}
			buffer[i] = ch;
		}
	}

	/*
	 * Fill out a standard SCSI read or write command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = getflag ? 0x28 : 0x2a;
	USR_MSB_SPLIT4(blocknum, cmd.lba);
	USR_MSB_SPLIT2(numblocks, cmd.length);

	/*
	 * Fill in the ioctl control parameters.
	 */
	bzero(&pt_args, sizeof(pt_args));
	pt_args.drive = drivenum;
	pt_args.todisk = !getflag;
	pt_args.fromdisk = getflag;
	pt_args.xfercount = xferlength;
	pt_args.cdblen = sizeof(cmd);
	bcopy(&cmd, pt_args.cdb, sizeof(cmd));
	pt_args.databuf = buffer;

	/*
	 * Execute the read long command.
	 */
	if (ioctl(fd, USRAID_PASSTHRU, &pt_args) < 0) {
		perror("couldn't perform read long command");
		exit(1);
	}

	if (getflag == 1) {
		/*
		 * Print the results.
		 */
		for (i = 0; i < xferlength; i++) {
			if ((i != 0) && ((i % 8) == 0))
				printf("\n");
			printf("0x%02x ", buffer[i]);
		}
		printf("\n");
	}

	exit(0);
}
