/*
 * Program to write over ECC bytes on drives attached to a RAID controller.
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
 * Read Long Command - opcode 0x3e
 * Read data AND ecc bytes from the device.
 */
struct read_long {
	u_char	opcode;
	u_char	lun:3, rsvd1:3, correct:1, reladr:1;
	u_char	lba[4];
	u_char	rsvd2;
	u_char	xferlen[2];
	u_char	rsvd3;
};

/*
 * Write Long Command - opcode 0x3f
 * Write data AND ecc bytes from the device.
 */
struct write_long {
	u_char	opcode;
	u_char	lun:3, rsvd1:4, reladr:1;
	u_char	lba[4];
	u_char	rsvd2;
	u_char	xferlen[2];
	u_char	rsvd3;
};


usage(prog)
{
	printf("Usage: %s -d drivenum -b blocknum -[rw] drivename\n", prog);
	exit(2);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	struct usraid_passthru pt_args;
	int drivenum, blocknum;
	int getflag, setflag, ch, fd, i;
	int xferlength;
	char *buffer;

	drivenum = blocknum = getflag = setflag = 0;
	while ((ch = getopt(argc, argv, "d:b:n:rw")) != EOF) {
		switch (ch) {
		case 'd': drivenum = atoi(optarg);	break;
		case 'b': blocknum = atoi(optarg);	break;
		case 'r': getflag = 1;			break;
		case 'w': setflag = 1;			break;
		default:  usage(argv[0]);		break;
		}
	}
	if ((!setflag && !getflag) || (setflag && getflag) ||
	    (drivenum < 0) || (drivenum > 4) || (blocknum < 0))
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

	xferlength = 512 + 20;	/* 20 bytes of LRC and ECC */
	buffer = (char *)malloc( (unsigned)xferlength );

	if (getflag == 1) {
		struct read_long readlong;

		/*
		 * Fill out a standard SCSI read long command.
		 */
		bzero(&readlong, sizeof(readlong));
		readlong.opcode = 0x3e;
		USR_MSB_SPLIT4(blocknum, readlong.lba);
		USR_MSB_SPLIT2(xferlength, readlong.xferlen);

		/*
		 * Fill in the ioctl control parameters.
		 */
		pt_args.drive = drivenum;
		pt_args.todisk = 0;
		pt_args.fromdisk = 1;
		pt_args.xfercount = xferlength;
		pt_args.cdblen = sizeof(readlong);
		bcopy(&readlong, pt_args.cdb, sizeof(readlong));
		pt_args.databuf = buffer;

		/*
		 * Execute the read long command.
		 */
		if (ioctl(fd, USRAID_PASSTHRU, &pt_args) < 0) {
			perror("couldn't perform read long command");
			exit(1);
		}

		/*
		 * Print the results.
		 */
		for (i = 0; i < xferlength; i++) {
			if ((i != 0) && ((i % 8) == 0))
				printf("\n");
			printf("0x%02x ", buffer[i]);
		}
		printf("\n");
	} else {
		struct write_long writelong;

		/*
		 * Collect the new sector contents from stdin.
		 */
		for (i = 0; i < xferlength; i++) {
			if (scanf("%x", &ch) != 1) {
				fprintf(stderr, "error: couldn't read byte number %d\n", i);
				exit(1);
			}
			buffer[i] = ch;
		}

		/*
		 * Fill out a standard SCSI write long command.
		 */
		bzero(&writelong, sizeof(writelong));
		writelong.opcode = 0x3f;
		USR_MSB_SPLIT4(blocknum, writelong.lba);
		USR_MSB_SPLIT2(xferlength, writelong.xferlen);

		/*
		 * Fill in the ioctl control parameters.
		 */
		pt_args.drive = drivenum;
		pt_args.todisk = 1;
		pt_args.fromdisk = 0;
		pt_args.xfercount = xferlength;
		pt_args.cdblen = sizeof(writelong);
		bcopy(&writelong, pt_args.cdb, sizeof(writelong));
		pt_args.databuf = buffer;

		/*
		 * Execute the write long command.
		 */
		if (ioctl(fd, USRAID_PASSTHRU, &pt_args) < 0) {
			perror("couldn't perform write long command");
			exit(1);
		}
	}

	exit(0);
}
