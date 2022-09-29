/*	program to print the drive type, so I can determine which script to feed to
	fx to set the drive parameters the way we want them for the Eclipse, just
	prior to reformating.
	Dave Olson, 7/88
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/scsi.h>

main(cnt, args)
char **args;
{
	static char inqname[SCSI_DEVICE_NAME_SIZE+1];
	int fd;
	int verbose = 0;

	security();
	if(cnt == 3 && strcmp(args[1], "-v") == 0)
		verbose++, args++, cnt--;
	if(cnt != 2)
		usage(args[0]);
	
	if(*args[1] != '/')
		if(chdir("/dev/rdsk")) {
			perror("couldn't cd to /dev/rdsk");
			exit(1);
		}

	if((fd = open(args[1], 0)) == -1) {
		fprintf(stderr, "inquire: couldn't open %s\n",args[1]);
		/*perror(*args);*/
		exit(1);
	}

	if(ioctl(fd, DIOCDRIVETYPE, inqname) == -1) {
		perror("couldn't determine drive type");
		exit(1);
	}
	if(verbose)
		printf("%s:\t%s\n", args[1], inqname);
	else
		printf("%s\n", inqname);
	exit(0);
}

usage(prog)
{
	printf("Usage: %s [-v] drivename\n", prog);
	exit(2);
}

security()
{
	if ((getuid() != 0) || (geteuid() != 0))
	{
		fprintf(stderr,"Only root can run this program\n");
		exit(-1);
	}
}
