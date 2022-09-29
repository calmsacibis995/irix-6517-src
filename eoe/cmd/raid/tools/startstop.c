/*
 * Program to start or stop a RAID controller.
 */

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <dslib.h>

/*
 * Start Unit Command - opcode 0x1b
 * Spin up or spin down the device.
 */
struct usr_startunit {
	u_char	opcode;
	u_char	rsvd1:7, immediate:1;
	u_char	rsvd2[2];
	u_char	rsvd3:7, startflag:1;
	u_char	rsvd4;
};


usage(prog)
{
	printf("Usage: %s -[ud] /dev/scsi/drivename\n", prog);
	exit(2);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int upflag, downflag, ch, error;
	unsigned long lba;
	struct dsreq *dsp;

	upflag = downflag = 0;
	while ((ch = getopt(argc, argv, "ud")) != EOF) {
		switch (ch) {
		case 'u': upflag = 1;			break;
		case 'd': downflag = 1;			break;
		default:  usage(argv[0]);		break;
		}
	}
	if ((!upflag && !downflag) || (upflag && downflag))
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
	dsp = dsopen(argv[optind], 0);

	/* 
	 * Fill in control stuff to send the command
	 */
	fillg0cmd(dsp, CMDBUF(dsp), 0x1b, 1, 0, 0, upflag, 0);
	filldsreq(dsp, 0, 0, DSRQ_SENSE);
	TIME(dsp) = 60 * 1000;	/* 20 seconds */

	/*
	 * Send the command.
	 */
	if (error = doscsireq(getfd(dsp), dsp))
		printf("err = %d\n", error);

	dsclose(dsp);
	exit(0);
}
