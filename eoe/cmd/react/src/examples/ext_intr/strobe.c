/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Auxiliary Program for Frame Scheduler Test with External Interrupts
 * Generator of External Interrupts
 *
 * Systems other than Origin must feed the external interrupt output back
 * into external interrupt input in order to run this program.
 */

#include <stdio.h>
#include <sys/ei.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/sysmp.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

void print_warning(void);

main(int argc, char *argv[])
{
	int fd;
	int i;

	fd = open("/dev/ei", O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	if (ioctl(fd, EIIOCENABLELB) < 0)
		print_warning();

	i = 0;
	while (1) {
		i++;
		if (ioctl(fd, EIIOCSTROBE, 1 ) < 0) {
			perror("STROBE: ERROR...");
		}
		sleep(1);
	}
}

void
print_warning(void)  
{
	printf("WARNING: External interrupt loopback is not supported for\n");
	printf("         this platform. The external interrupt output must\n");
	printf("         be fed back into the external interrupt input, in\n");
	printf("         order for strobing to succeed.\n"); 
}


