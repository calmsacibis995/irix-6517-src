/*
 * Program to get/set mode pages on drives attached to a RAID controller.
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
 * Mode Sense Command - opcode 0x1a
 * Read mode pages from the device.
 */
struct mode_sense {
	u_char	opcode;
	u_char	rsvd1;
	u_char	pagecontrol:2, pagecode:6;
	u_char	rsvd2;
	u_char	alloclen;
	u_char	rsvd3;
};

/*
 * Mode Select Command - opcode 0x15
 * Write new mode pages to the device.
 */
struct mode_select {
	u_char	opcode;
	u_char	rsvd1:3, pageflag:1, rsvd2:3, savepage:1;
	u_char	rsvd3[2];
	u_char	paramlen;
	u_char	rsvd4;
};


usage(prog)
{
	printf("Usage: %s [-v] -d drivenum -p pagenum -l pagelen -[gs] drivename\n", prog);
	exit(2);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	struct usraid_passthru pt_args;
	int drivenum, pagenum, pagelen;
	int getflag, setflag, ch, fd, i;
	char buffer[4096];

	drivenum = pagenum = pagelen = getflag = setflag = 0;
	while ((ch = getopt(argc, argv, "d:p:l:gs")) != EOF) {
		switch (ch) {
		case 'd': drivenum = atoi(optarg);	break;
		case 'p': pagenum = atoi(optarg);	break;
		case 'l': pagelen = atoi(optarg);	break;
		case 'g': getflag = 1;			break;
		case 's': setflag = 1;			break;
		default:  usage(argv[0]);		break;
		}
	}
	if ((!setflag && !getflag) || (setflag && getflag) ||
	    (drivenum < 0) || (drivenum > 4) ||
	    (pagelen <= 0) || (pagelen >= 4096) ||
	    (pagenum < 0))
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

	if (getflag == 1) {
		struct mode_sense	sense_cmd;

		/*
		 * Fill out a standard SCSI mode sense command.
		 */
		bzero(&sense_cmd, sizeof(sense_cmd));
		sense_cmd.opcode = 0x1a;
		sense_cmd.pagecontrol = 0;
		sense_cmd.pagecode = pagenum;
		sense_cmd.alloclen = pagelen + 12;

		/*
		 * Fill in the ioctl control parameters.
		 */
		pt_args.drive = drivenum;
		pt_args.todisk = 0;
		pt_args.fromdisk = 1;
		pt_args.xfercount = pagelen + 12;
		pt_args.cdblen = sizeof(sense_cmd);
		bcopy(&sense_cmd, pt_args.cdb, sizeof(sense_cmd));
		pt_args.databuf = buffer;

		/*
		 * Execute the mode sense command.
		 */
		if (ioctl(fd, USRAID_PASSTHRU, &pt_args) < 0) {
			perror("couldn't perform mode sense command");
			exit(1);
		}

		/*
		 * Print the results.
		 */
		for (i = 0; i < 12; i++) {
			printf("0x%02x ", buffer[i]);
		}
		printf("\n");
		for (i = 0; i < pagelen; i++) {
			if ((i != 0) && ((i % 8) == 0))
				printf("\n");
			printf("0x%02x ", buffer[i+12]);
		}
		printf("\n");
	} else {
		struct mode_select	select_cmd;

		/*
		 * Collect the new page contents from stdin.
		 */
		for (i = 0; i < pagelen+12; i++) {
			if (scanf("%x", &ch) != 1) {
				fprintf(stderr, "error: couldn't read byte number %d\n", i);
				exit(1);
			}
			buffer[i] = ch;
		}

		/*
		 * mode select doesn't accept direct mode sense info
		 */
		buffer[0] = 0;
		buffer[12] &= 0x7f;

		/*
		 * Fill out a standard SCSI mode select command.
		 */
		bzero(&select_cmd, sizeof(select_cmd));
		select_cmd.opcode = 0x15;
		select_cmd.savepage = 1;
		select_cmd.paramlen = pagelen + 12;

		/*
		 * Fill in the ioctl control parameters.
		 */
		pt_args.drive = drivenum;
		pt_args.todisk = 1;
		pt_args.fromdisk = 0;
		pt_args.xfercount = pagelen + 12;
		pt_args.cdblen = sizeof(select_cmd);
		bcopy(&select_cmd, pt_args.cdb, sizeof(select_cmd));
		pt_args.databuf = buffer;

		/*
		 * Execute the mode select command.
		 */
		if (ioctl(fd, USRAID_PASSTHRU, &pt_args) < 0) {
			perror("couldn't perform mode select command");
			exit(1);
		}
	}

	exit(0);
}
